"""
SPARQL Test Runner

This module provides the core SPARQL test execution functionality for running
W3C SPARQL test suites against Rasqal.

Copyright (C) 2025, David Beckett https://www.dajobe.org/

This package is Free Software and part of Redland http://librdf.org/

It is licensed under the following three licenses as alternatives:
   1. GNU Lesser General Public License (LGPL) V2.1 or any newer version
   2. GNU General Public License (GPL) V2 or any newer version
   3. Apache License, V2.0 or any newer version

You may not use this file except in compliance with at least one of
the above three licenses.

See LICENSE.html or LICENSE.txt at the top of this package for the
complete terms and further detail along with the license texts for
the licenses in COPYING.LIB, COPYING and LICENSE-2.0.txt respectively.
"""

import os
import argparse
import logging
from pathlib import Path
import time
import re
import subprocess
import xml.etree.ElementTree as ET
from typing import List, Dict, Any, Optional, Tuple, Union
from dataclasses import dataclass, field
from enum import Enum

from ..test_types import TestResult, TestType, Namespaces, TestTypeResolver
from ..config import TestConfig
from ..utils import find_tool, run_command, SparqlTestError
from ..manifest import ManifestParser, UtilityNotFoundError
from ..execution import run_roqet_with_format, filter_format_output

# Create namespace instance for backward compatibility
NS = Namespaces()

# Configure logging
logging.basicConfig(level=logging.INFO, format="%(levelname)s: %(message)s")
logger = logging.getLogger(__name__)

CURDIR = Path.cwd()


# --- Custom Exceptions ---
class TestExecutionError(SparqlTestError):
    """Raised when a test fails due to execution issues (e.g., roqet crash)."""

    pass


# --- Helper functions ---
def normalize_blank_nodes(text_output: str) -> str:
    """Normalizes blank node IDs in text output for consistent comparison."""
    return re.sub(r"blank \w+", "blank _", text_output)


ROQET = find_tool("roqet") or "roqet"
TO_NTRIPLES = find_tool("to-ntriples") or "to-ntriples"
DIFF_CMD = os.environ.get("DIFF") or "diff"


# --- Constants and Enums ---
# Global flag for file preservation
_preserve_debug_files = False

# Global to hold variable order across calls if needed
_projected_vars_order: List[str] = []

# Temporary file names (using Path for consistency)
ROQET_OUT = Path("roqet.out")
RESULT_OUT = Path("result.out")
ROQET_TMP = Path("roqet.tmp")
ROQET_ERR = Path("roqet.err")
DIFF_OUT = Path("diff.out")
TO_NTRIPLES_ERR = Path("to_ntriples.err")


def cleanup_temp_files() -> None:
    """Clean up temporary files if not preserving them."""
    if not _preserve_debug_files:
        temp_files = [
            ROQET_OUT,
            RESULT_OUT,
            ROQET_TMP,
            ROQET_ERR,
            DIFF_OUT,
            TO_NTRIPLES_ERR,
        ]
        for temp_file in temp_files:
            if temp_file.exists():
                try:
                    temp_file.unlink()
                except OSError:
                    pass  # Ignore cleanup errors


def finalize_test_result(
    test_result_summary: Dict[str, Any], expect_status: TestResult
):
    """Finalizes the test result based on expected vs actual outcome."""
    actual_result = test_result_summary["result"]

    if expect_status == TestResult.FAILED:
        # Expected to fail
        if actual_result == "failure":
            test_result_summary["is_success"] = True
            test_result_summary["result"] = "success"
        else:
            test_result_summary["is_success"] = False
            test_result_summary["result"] = "failure"
    else:
        # Expected to pass
        if actual_result == "success":
            test_result_summary["is_success"] = True
        else:
            test_result_summary["is_success"] = False


def _execute_roqet(config: TestConfig) -> Dict[str, Any]:
    """Execute roqet with the given test configuration."""
    start_time = time.time()

    # Build roqet command
    cmd = [ROQET, "-i", config.language or "sparql"]

    # Add debug output for analysis
    if config.test_type == TestType.CSV_RESULT_FORMAT_TEST.value:
        cmd.extend(["-r", "csv"])
    else:
        cmd.extend(["-d", "debug"])

    # Add warning level
    cmd.extend(["-W", str(getattr(config, "warning_level", 0))])

    # Add data files
    for data_file in config.data_files:
        cmd.extend(["-D", str(data_file)])

    # Add named data files
    for named_data_file in config.named_data_files:
        cmd.extend(["-G", str(named_data_file)])

    # Don't execute query, just parse if not executing
    if not config.execute:
        cmd.append("-n")

    # Add query file as positional argument
    if config.test_file:
        # Always pass the test file as a file:// URI if it is an absolute path
        test_file_path = str(config.test_file)
        if os.path.isabs(test_file_path):
            test_file_uri = f"file://{test_file_path}"
        else:
            test_file_uri = test_file_path
        cmd.append(test_file_uri)

    # Execute command
    try:
        result = run_command(cmd, CURDIR, f"Error running roqet for test {config.name}")

        elapsed_time = time.time() - start_time

        return {
            "stdout": result.stdout,
            "stderr": result.stderr,
            "returncode": result.returncode,
            "elapsed_time": elapsed_time,
            "query_cmd": " ".join(cmd),
        }
    except Exception as e:
        elapsed_time = time.time() - start_time
        return {
            "stdout": "",
            "stderr": str(e),
            "returncode": -1,
            "elapsed_time": elapsed_time,
            "query_cmd": " ".join(cmd),
        }


def _process_actual_output(roqet_stdout_str: str) -> Dict[str, Any]:
    """
    Parses roqet's debug output to extract result type, variable order, and actual results.
    Also writes a normalized version of the output to ROQET_OUT if debug level >= 2.
    """
    global _projected_vars_order
    _projected_vars_order = []  # Reset for each new parse
    lines = roqet_stdout_str.splitlines()
    result_type = "bindings"
    vars_seen_in_this_roqet_output: Dict[str, int] = {}
    is_sorted_by_query = False
    processed_output_lines: List[str] = []
    roqet_results_count = 0
    actual_boolean_value = None

    for line in lines:
        if match := re.match(r"projected variable names: (.*)", line):
            if not _projected_vars_order:  # Only set once per output stream
                for vname in re.split(r",\s*", match.group(1)):
                    if vname not in vars_seen_in_this_roqet_output:
                        _projected_vars_order.append(vname)
                        vars_seen_in_this_roqet_output[vname] = 1
                if logger.level == logging.DEBUG:
                    logger.debug(
                        f"Set roqet projected vars order to: {_projected_vars_order}"
                    )
        elif match := re.match(r"query verb:\s+(\S+)", line):
            verb = match.group(1).upper()
            if verb in ("CONSTRUCT", "DESCRIBE"):
                result_type = "graph"
            elif verb == "ASK":
                result_type = "boolean"
        elif result_type == "boolean" and line.strip() in ["true", "false"]:
            # Capture boolean result value for ASK queries
            actual_boolean_value = line.strip() == "true"
            processed_output_lines.append(line.strip())
        elif "query order conditions:" in line:
            is_sorted_by_query = True
        elif row_match := re.match(r"^(?:row|result): \[(.*)\]$", line):
            # Apply normalize_blank_nodes here for bindings content
            content = (
                normalize_blank_nodes(row_match.group(1))
                .replace("=INV:", "=")
                .replace("=udt", "=string")
            )
            # Replace =xsdstring(...) with formatted literal
            content = re.sub(
                r"=xsdstring\((.*?)\)",
                r'=string("\1"^^<http://www.w3.org/2001/XMLSchema#string>)',
                content,
            )
            prefix = "row: " if line.startswith("row:") else "result: "
            processed_output_lines.append(f"{prefix}[{content}]")
            roqet_results_count += 1
        elif result_type == "graph" and (line.startswith("_:") or line.startswith("<")):
            processed_output_lines.append(normalize_blank_nodes(line))

    final_output_lines = processed_output_lines
    if result_type == "bindings" and not is_sorted_by_query:
        final_output_lines.sort()  # Sort bindings if not sorted by query
    elif result_type == "graph":
        # For graphs, ensure uniqueness and consistent sorting of triples
        final_output_lines = sorted(list(set(processed_output_lines)))

    # Always write to file for comparison, cleanup later if not preserving
    ROQET_OUT.write_text("\n".join(final_output_lines) + "\n" if final_output_lines else "")

    return {
        "result_type": result_type,
        "roqet_results_count": roqet_results_count,
        "vars_order": _projected_vars_order[:],  # Return a copy
        "is_sorted_by_query": is_sorted_by_query,
        "boolean_value": actual_boolean_value,  # Add boolean value for ASK queries
        "content": "\n".join(final_output_lines) + "\n" if final_output_lines else "",
        "count": len(final_output_lines),
        "format": result_type,
    }


def read_query_results_file(
    result_file_path: Path,
    result_format_hint: str,
    expected_vars_order: List[str],
    sort_output: bool,
) -> Optional[Dict[str, Any]]:
    """
    Reads a SPARQL query results file (e.g., SRX, SRJ, CSV, TSV) using roqet and normalizes its output.
    Returns a dictionary with result count or None on error.
    """
    # Handle boolean results (ASK queries) for CSV/TSV formats
    if result_format_hint in ["csv", "tsv"]:
        # Try to detect if this is a boolean result by checking file content
        try:
            content = result_file_path.read_text().strip()
            # CSV/TSV boolean results contain just "true" or "false"
            if content.lower() in ["true", "false"]:
                # Parse boolean result properly
                boolean_value = content.lower() == "true"
                logger.debug(f"Parsed CSV/TSV boolean result: {boolean_value}")
                return {
                    "type": "boolean",
                    "value": boolean_value,
                    "count": 1,
                    "vars": [],
                    "results": [],
                }
        except Exception as e:
            logger.debug(f"Could not read file content for boolean detection: {e}")

        return parse_csv_tsv_with_roqet(
            result_file_path, result_format_hint, expected_vars_order, sort_output
        )

    abs_result_file_path = result_file_path.resolve()
    cmd = [
        ROQET,
        "-q",
        "-R",
        result_format_hint,
        "-r",
        "simple",
        "-t",
        str(abs_result_file_path),
    ]
    if logger.level == logging.DEBUG:
        logger.debug(f"(read_query_results_file): Running {' '.join(cmd)}")
    try:
        process = run_command(
            cmd,
            CURDIR,
            f"Error reading results file '{abs_result_file_path}' ({result_format_hint})",
        )
        roqet_stderr_content = process.stderr

        if process.returncode != 0 or "Error" in roqet_stderr_content:
            logger.warning(
                f"Reading results file '{abs_result_file_path}' ({result_format_hint}) FAILED."
            )
            if roqet_stderr_content:
                logger.warning(f"  Stderr(roqet):\n{roqet_stderr_content.strip()}")
            return None

        parsed_rows_for_output: List[str] = []
        current_vars_order: List[str] = []
        first_row = True
        order_to_use: List[str] = expected_vars_order if expected_vars_order else []

        for line in process.stdout.splitlines():
            line = line.strip()
            if not line.startswith("row: [") or not line.endswith("]"):
                continue

            content = normalize_blank_nodes(line[len("row: [") : -1])
            row_data: Dict[str, str] = {}
            if content:
                # Split pairs, handling cases where values might contain '='
                pairs = re.split(r",\s*(?=\S+=)", content)
                for pair in pairs:
                    if "=" in pair:
                        var_name, var_val = pair.split("=", 1)
                        row_data[var_name] = var_val
                        if first_row and var_name not in current_vars_order:
                            current_vars_order.append(var_name)
            first_row = False

            # Update order_to_use with dynamically determined current_vars_order if needed
            if not expected_vars_order:
                order_to_use = current_vars_order
            # Ensure all expected variables are present, fill with "NULL" if missing
            formatted_row_parts = [
                f"{var}={row_data.get(var, NS.RS + 'undefined')}"
                for var in order_to_use
            ]
            parsed_rows_for_output.append(f"row: [{', '.join(formatted_row_parts)}]")

        if sort_output:
            parsed_rows_for_output.sort()

        # Always write to file for comparison, cleanup later if not preserving
        RESULT_OUT.write_text("\n".join(parsed_rows_for_output) + "\n" if parsed_rows_for_output else "")

        return {
            "content": (
                "\n".join(parsed_rows_for_output) + "\n"
                if parsed_rows_for_output
                else ""
            ),
            "count": len(parsed_rows_for_output),
            "format": "bindings",
            "vars_order": order_to_use,
        }

    except Exception as e:
        logger.error(f"Error reading results file: {e}")
        return None


def parse_csv_tsv_with_roqet(
    file_path: Path,
    format_type: str,
    expected_vars_order: List[str],
    sort_output: bool,
) -> Optional[Dict[str, Any]]:
    """
    Parse CSV/TSV files using roqet's built-in parsing capabilities.
    Converts CSV/TSV to SRX format for normalized comparison.
    """

    file_path = Path(file_path)

    # Basic existence check only - let roqet handle format validation
    if not file_path.exists():
        logger.error(f"{format_type} file does not exist: {file_path}")
        return None

    # Use roqet to convert CSV/TSV to normalized SRX format for comparison
    try:
        cmd = [ROQET, "-R", format_type, "-t", str(file_path), "-r", "xml"]
        if logger.level == logging.DEBUG:
            logger.debug(f"(parse_csv_tsv_with_roqet): Running {' '.join(cmd)}")

        result = run_command(
            cmd, CURDIR, f"Failed to parse {format_type} file {file_path}"
        )

        if result.returncode != 0:
            logger.error(
                f"roqet failed to parse {format_type} file {file_path}: {result.stderr}"
            )
            return None

        if not result.stdout.strip():
            logger.warning(
                f"roqet produced empty output for {format_type} file {file_path}"
            )
            return {"count": 0, "vars": [], "results": []}

        # Parse the converted SRX output using XML parsing
        return parse_srx_from_roqet_output(
            result.stdout, expected_vars_order, sort_output
        )

    except Exception as e:
        logger.error(f"Error parsing {format_type} file {file_path}: {e}")
        return None


def parse_srx_from_roqet_output(
    srx_content: str, expected_vars_order: List[str], sort_output: bool
) -> Optional[Dict[str, Any]]:
    """
    Parse SRX content from roqet output into normalized format.
    Returns a dictionary compatible with existing test infrastructure.
    """
    try:
        # Parse XML using ElementTree
        root = ET.fromstring(srx_content)

        # Extract namespaces
        ns = {"sparql": "http://www.w3.org/2005/sparql-results#"}

        # Extract variables
        vars_elem = root.find(".//sparql:head", ns)
        variables = []
        if vars_elem is not None:
            for var_elem in vars_elem.findall(".//sparql:variable", ns):
                var_name = var_elem.get("name")
                if var_name:
                    variables.append(var_name)

        # Convert to the format expected by existing test infrastructure
        parsed_rows_for_output = []

        # Extract results
        results_elem = root.find(".//sparql:results", ns)
        if results_elem is not None:
            for result_elem in results_elem.findall(".//sparql:result", ns):
                result_row = {}
                for binding_elem in result_elem.findall(".//sparql:binding", ns):
                    var_name = binding_elem.get("name")
                    if var_name:
                        # Extract value based on type
                        value_elem = binding_elem.find("*")
                        if value_elem is not None:
                            if value_elem.tag.endswith("uri"):
                                result_row[var_name] = f'uri("{value_elem.text}")'
                            elif value_elem.tag.endswith("literal"):
                                # Handle different literal types
                                datatype = value_elem.get("datatype")
                                lang = value_elem.get(
                                    "{http://www.w3.org/XML/1998/namespace}lang"
                                )
                                if datatype:
                                    result_row[var_name] = (
                                        f'literal("{value_elem.text}", datatype={datatype})'
                                    )
                                elif lang:
                                    result_row[var_name] = (
                                        f'literal("{value_elem.text}", lang={lang})'
                                    )
                                else:
                                    result_row[var_name] = (
                                        f'string("{value_elem.text}")'
                                    )
                            elif value_elem.tag.endswith("bnode"):
                                result_row[var_name] = f"blank {value_elem.text}"

                # Use expected_vars_order if available, otherwise use discovered variables
                order_to_use = expected_vars_order if expected_vars_order else variables

                # Format row in the same way as the existing function
                formatted_row_parts = [
                    f"{var}={result_row.get(var, NS.RS + 'undefined')}"
                    for var in order_to_use
                ]
                parsed_rows_for_output.append(
                    f"row: [{', '.join(formatted_row_parts)}]"
                )

        # Sort results if requested
        if sort_output:
            parsed_rows_for_output.sort()

        # Always write to file for comparison, cleanup later if not preserving
        RESULT_OUT.write_text("\n".join(parsed_rows_for_output) + "\n" if parsed_rows_for_output else "")

        return {"count": len(parsed_rows_for_output)}

    except ET.ParseError as e:
        logger.error(f"Error parsing SRX XML: {e}")
        return None
    except Exception as e:
        logger.error(f"Error processing SRX content: {e}")
        return None


def detect_result_format(result_file_path: Path) -> str:
    """
    Detect the format of a result file by examining its suffix.
    Returns the appropriate format string for roqet.
    """
    ext = result_file_path.suffix.lower()
    if ext == ".srx":
        return "xml"
    elif ext == ".srj":
        return "srj"
    elif ext == ".rdf":
        return "rdfxml"
    elif ext in [".csv", ".tsv"]:
        return ext[1:]  # Remove the dot
    elif ext == ".ttl":
        return "turtle"
    elif ext == ".n3":
        return "turtle"  # Turtle parser can handle N3 format
    else:
        return "turtle"  # Default for unknown extensions


def read_rdf_graph_file(result_file_path: Path) -> Optional[str]:
    """
    Reads an RDF graph file and converts it to N-Triples format using to-ntriples.
    Returns the N-Triples content as a string or None on error.
    """
    abs_result_file_path = result_file_path.resolve()
    # Use 'file:///path/to/file' as base URI for parsing local files with to-ntriples
    base_uri = f"file://{abs_result_file_path}"
    cmd = [TO_NTRIPLES, str(abs_result_file_path), base_uri]
    if logger.level == logging.DEBUG:
        logger.debug(f"(read_rdf_graph_file): Running {' '.join(cmd)}")
    try:
        process = run_command(
            cmd, CURDIR, f"Error running '{TO_NTRIPLES}' for {abs_result_file_path}"
        )
        to_ntriples_stderr = process.stderr

        if process.returncode != 0 or (
            "Error -" in to_ntriples_stderr or "error:" in to_ntriples_stderr.lower()
        ):
            logger.warning(
                f"Parsing RDF graph result file '{abs_result_file_path}' FAILED - {TO_NTRIPLES} reported errors or non-zero exit."
            )
            if to_ntriples_stderr:
                logger.warning(
                    f"  Stderr from {TO_NTRIPLES}:\n{to_ntriples_stderr.strip()}"
                )
            else:
                logger.warning(
                    f"  {TO_NTRIPLES} exit code: {process.returncode}, stdout (first 200 chars): {process.stdout[:200] if process.stdout else '(empty)'}"
                )
            return None
        return process.stdout
    except Exception as e:
        logger.error(f"Error running '{TO_NTRIPLES}' for {abs_result_file_path}: {e}")
        return None


def _compare_actual_vs_expected(
    test_result_summary: Dict[str, Any],
    actual_result_info: Dict[str, Any],
    expected_result_file: Optional[Path],
    cardinality_mode: str,
    global_debug_level: int,
):
    """
    Compares the actual roqet output with the expected results.
    Updates test_result_summary with comparison details.
    """
    name = test_result_summary["name"]
    actual_result_type = actual_result_info["result_type"]
    actual_results_count = actual_result_info["roqet_results_count"]
    actual_vars_order = actual_result_info["vars_order"]
    is_sorted_by_query_from_actual = actual_result_info["is_sorted_by_query"]

    expected_results_count = 0

    if not expected_result_file:
        logger.info(f"Test '{name}': OK (roqet succeeded, no result_file to compare)")
        test_result_summary["result"] = "success"
        return  # Early exit if no result file

    # Process expected results file
    if actual_result_type == "graph":
        if global_debug_level > 0:
            logger.debug(
                f"Reading expected RDF graph result file {expected_result_file}"
            )
        expected_ntriples = read_rdf_graph_file(expected_result_file)
        if expected_ntriples is not None:  # Allow empty graph
            sorted_expected_triples = sorted(list(set(expected_ntriples.splitlines())))
            # Always write to file for comparison, cleanup later if not preserving
            RESULT_OUT.write_text("\n".join(sorted_expected_triples) + "\n" if sorted_expected_triples else "")
        else:
            logger.warning(
                f"Test '{name}': FAILED (could not read/parse expected graph result file {expected_result_file} or it's not explicitly empty)"
            )
            test_result_summary["result"] = "failure"
            return
    elif actual_result_type in ["bindings", "boolean"]:
        expected_result_format = detect_result_format(expected_result_file)

        if global_debug_level > 0:
            logger.debug(
                f"Reading expected '{actual_result_type}' result file {expected_result_file} (format: {expected_result_format})"
            )

        should_sort_expected = not is_sorted_by_query_from_actual
        expected_results_info = read_query_results_file(
            expected_result_file,
            expected_result_format,
            actual_vars_order,
            sort_output=should_sort_expected,
        )

        if expected_results_info:
            expected_results_count = expected_results_info.get("count", 0)
        else:
            # Handle cases where result_file exists but is empty or unparsable as empty
            if (
                expected_result_file.exists()
                and expected_result_file.stat().st_size == 0
            ):
                RESULT_OUT.write_text("")
                expected_results_count = 0
            else:
                logger.warning(
                    f"Test '{name}': FAILED (could not read/parse expected results file {expected_result_file} or it's not explicitly empty)"
                )
                test_result_summary["result"] = "failure"
                return
    else:
        logger.error(
            f"Test '{name}': Unknown actual_result_type '{actual_result_type}'"
        )
        test_result_summary["result"] = "failure"
        return

    # If processing expected results didn't fail, proceed to comparison
    comparison_rc = -1
    if actual_result_type == "graph":
        comparison_rc = compare_rdf_graphs(RESULT_OUT, ROQET_OUT, DIFF_OUT)
    elif (
        actual_result_type == "boolean"
        and expected_results_info
        and expected_results_info.get("type") == "boolean"
    ):
        # Handle boolean result comparison directly
        expected_boolean = expected_results_info.get("value", False)
        # Get actual boolean from processed output
        actual_boolean = actual_result_info.get("boolean_value", False)

        if expected_boolean == actual_boolean:
            logger.info(f"Test '{name}': OK (boolean result matches: {actual_boolean})")
            test_result_summary["result"] = "success"
        else:
            logger.warning(
                f"Test '{name}': FAILED (boolean result mismatch: expected {expected_boolean}, got {actual_boolean})"
            )
            test_result_summary["result"] = "failure"
        return
    else:
        # For bindings, compare the processed output files directly
        try:
            # Write actual output to file for comparison
            with open(ROQET_TMP, "w") as f:
                f.write(actual_result_info.get("content", ""))

            # Generate diff
            diff_result = run_command(
                [DIFF_CMD, "-u", str(RESULT_OUT), str(ROQET_TMP)],
                CURDIR,
                "Error generating diff",
            )

            # Write diff output
            with open(DIFF_OUT, "w") as f:
                f.write(diff_result.stdout)
                if diff_result.stderr:
                    f.write(diff_result.stderr)

            comparison_rc = diff_result.returncode
        except Exception as e:
            logger.error(f"Error comparing bindings: {e}")
            comparison_rc = 1

    # Interpret comparison result
    if comparison_rc == 0:
        logger.info(f"Test '{name}': OK (results match)")
        test_result_summary["result"] = "success"
    else:
        logger.warning(f"Test '{name}': FAILED (results do not match)")
        test_result_summary["result"] = "failure"
        test_result_summary["diff"] = (
            DIFF_OUT.read_text() if DIFF_OUT.exists() else "No diff available"
        )


def compare_rdf_graphs(
    file1_path: Path, file2_path: Path, diff_output_path: Path
) -> int:
    """
    Compares two RDF graphs (N-Triples files). Prefers `ntc` or `jena.rdfcompare`
    if available, otherwise falls back to `diff`.
    Returns the exit code of the comparison tool (0 for match, non-zero for diff).
    """
    abs_file1, abs_file2 = str(file1_path.resolve()), str(file2_path.resolve())
    ntc_path, jena_root = os.environ.get("NTC"), os.environ.get("JENAROOT")

    # Try NTC
    if ntc_path:
        cmd_list = [ntc_path, abs_file1, abs_file2]
        logger.debug(f"Comparing graphs using NTC: {' '.join(cmd_list)}")
        try:
            process = run_command(cmd_list, CURDIR, "Error running NTC")
            # Always write diff output for debugging, cleanup later if not preserving
            diff_output_path.write_text(
                process.stdout + process.stderr
            )  # NTC can write output to stdout/stderr
            if process.stderr:
                logger.debug(f"NTC stderr: {process.stderr.strip()}")
            return process.returncode
        except Exception as e:
            logger.warning(f"Error running NTC: {e}. Falling back to diff.")

    # Try Jena rdfcompare
    if jena_root:
        lib_path = Path(jena_root) / "lib"
        classpath_items = [str(p) for p in lib_path.glob("*.jar")]
        if not classpath_items:
            logger.warning(
                f"Jena at {jena_root} but no JARs in lib/. Falling back to diff."
            )
        else:
            cmd_list = [
                "java",
                "-cp",
                os.pathsep.join(classpath_items),
                "jena.rdfcompare",
                abs_file1,
                abs_file2,
                "N-TRIPLE",
                "N-TRIPLE",
            ]
            logger.debug(
                f"Comparing graphs using Jena rdfcompare: {' '.join(cmd_list)}"
            )
            try:
                process = run_command(cmd_list, CURDIR, "Error running Jena rdfcompare")
                # Always write diff output for debugging, cleanup later if not preserving
                diff_output_path.write_text(
                    process.stdout + process.stderr
                )  # Jena writes to stdout/stderr
                if process.stderr:
                    logger.debug(f"Jena rdfcompare stderr: {process.stderr.strip()}")
                if process.returncode != 0:
                    logger.warning(
                        f"Jena rdfcompare command failed (exit {process.returncode}). Falling back to diff."
                    )
                    return process.returncode
                else:
                    return 0 if "graphs are equal" in process.stdout.lower() else 1
            except Exception as e:
                logger.warning(
                    f"Error running Jena rdfcompare: {e}. Falling back to diff."
                )

    # Fallback to system diff
    cmd_list = [DIFF_CMD, "-u", abs_file1, abs_file2]
    logger.debug(f"Comparing graphs using system diff: {' '.join(cmd_list)}")
    try:
        process = run_command(
            cmd_list, CURDIR, f"Error running system diff ('{DIFF_CMD}')"
        )
        # Always write diff output for debugging, cleanup later if not preserving
        diff_output_path.write_text(process.stdout)
        return process.returncode
    except Exception as e:
        logger.error(f"Error running diff command: {e}")
        diff_output_path.write_text(f"Error running diff command: {e}\n")
        return 2  # Indicate a general error


def get_rasqal_version() -> str:
    """Retrieves the roqet (Rasqal) version string."""
    try:
        return run_command(
            [ROQET, "-v"], CURDIR, "Could not get roqet version"
        ).stdout.strip()
    except Exception as e:
        logger.warning(f"Could not get roqet version: {e}")
        return "unknown"


def html_escape(text: Optional[str]) -> str:
    """Escape text for HTML output."""
    if text is None:
        return ""

    return (
        text.replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
        .replace('"', "&quot;")
        .replace("'", "&#39;")
    )


def generate_earl_report(
    earl_file_path: Path, test_results: List[Dict[str, Any]], args: argparse.Namespace
):
    """Generate EARL (Evaluation and Report Language) report."""
    try:
        with open(earl_file_path, "w") as f:
            f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
            f.write(
                '<rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#"\n'
            )
            f.write('         xmlns:earl="http://www.w3.org/ns/earl#"\n')
            f.write('         xmlns:foaf="http://xmlns.com/foaf/0.1/">\n\n')

            # Add test subject (Rasqal)
            f.write('  <foaf:Agent rdf:about="http://librdf.org/rasqal">\n')
            f.write(f"    <foaf:name>Rasqal {get_rasqal_version()}</foaf:name>\n")
            f.write("  </foaf:Agent>\n\n")

            # Add test results
            for test_result in test_results:
                test_name = test_result.get("name", "Unknown")
                test_uri = test_result.get(
                    "uri", f"http://example.org/test/{test_name}"
                )
                is_success = test_result.get("is_success", False)

                f.write(f'  <earl:TestResult rdf:about="{test_uri}">\n')
                f.write(
                    f'    <earl:outcome rdf:resource="http://www.w3.org/ns/earl#{"passed" if is_success else "failed"}"/>\n'
                )
                f.write(f'    <earl:test rdf:resource="{test_uri}"/>\n')
                f.write('    <earl:subject rdf:resource="http://librdf.org/rasqal"/>\n')
                f.write("  </earl:TestResult>\n\n")

            f.write("</rdf:RDF>\n")

        logger.info(f"EARL report written to {earl_file_path}")

    except Exception as e:
        logger.error(f"Error generating EARL report: {e}")


def generate_junit_report(
    junit_file_path: Path,
    test_results: List[Dict[str, Any]],
    args: argparse.Namespace,
    suite_elapsed_time: float,
):
    """Generate JUnit XML report."""
    try:
        with open(junit_file_path, "w") as f:
            f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
            f.write("<testsuites>\n")
            f.write(
                f'  <testsuite name="SPARQL Tests" tests="{len(test_results)}" time="{suite_elapsed_time:.3f}">\n'
            )

            for test_result in test_results:
                test_name = test_result.get("name", "Unknown")
                is_success = test_result.get("is_success", False)
                elapsed_time = test_result.get("elapsed-time", 0)

                f.write(
                    f'    <testcase name="{html_escape(test_name)}" time="{elapsed_time:.3f}">\n'
                )

                if not is_success:
                    f.write("      <failure>\n")
                    detail = test_result.get("detail", "Test failed")
                    f.write(f"        {html_escape(detail)}\n")
                    f.write("      </failure>\n")

                f.write("    </testcase>\n")

            f.write("  </testsuite>\n")
            f.write("</testsuites>\n")

        logger.info(f"JUnit report written to {junit_file_path}")

    except Exception as e:
        logger.error(f"Error generating JUnit report: {e}")


def run_single_test(config: TestConfig, global_debug_level: int) -> Dict[str, Any]:
    """
    Runs a single SPARQL test using roqet and compares results.
    Returns a dictionary summarizing the test outcome.
    """
    test_result_summary: Dict[str, Any] = {
        "name": config.name,
        "uri": config.test_uri,
        "result": "failure",
        "is_success": False,
        "stdout": "",
        "stderr": "",
        "query": "",
        "elapsed-time": 0,
        "roqet-status-code": -1,
        "diff": "",
    }

    if global_debug_level > 0:
        logger.debug(
            f"run_single_test: (URI: {config.test_uri})\n"
            f"  name: {config.name}\n"
            f"  language: {config.language}\n"
            f"  query: {config.test_file}\n"
            f"  data: {'; '.join(map(str, config.data_files))}\n"
            f"  named data: {'; '.join(map(str, config.named_data_files))}\n"
            f"  result file: {config.result_file or 'none'}\n"
            f"  expect: {config.expect.value}\n"
            f"  card mode: {config.cardinality_mode}\n"
            f"  execute: {config.execute}\n"
            f"  test_type: {config.test_type}"
        )

    try:
        # 1. Execute roqet
        roqet_execution_data = _execute_roqet(config)
        test_result_summary["stdout"] = roqet_execution_data["stdout"]
        test_result_summary["stderr"] = roqet_execution_data["stderr"]
        test_result_summary["roqet-status-code"] = roqet_execution_data["returncode"]
        test_result_summary["elapsed-time"] = roqet_execution_data["elapsed_time"]
        test_result_summary["query"] = roqet_execution_data["query_cmd"]

        # Initial outcome based on roqet's exit code
        # For warning tests, only exit code 2 (warning) is considered success
        if config.test_type == TestType.WARNING_TEST.value:
            test_result_summary["result"] = (
                "success" if roqet_execution_data["returncode"] == 2 else "failure"
            )
        else:
            test_result_summary["result"] = (
                "failure" if roqet_execution_data["returncode"] != 0 else "success"
            )

        # Handle non-zero exit codes (but not for warning tests with exit code 2)
        if roqet_execution_data["returncode"] != 0 and not (
            config.test_type == TestType.WARNING_TEST.value
            and roqet_execution_data["returncode"] == 2
        ):
            outcome_msg = f"exited with status {roqet_execution_data['returncode']}"
            if global_debug_level > 0:
                logger.debug(f"roqet for '{config.name}' {outcome_msg}")
            if config.expect == TestResult.FAILED:
                logger.info(
                    f"Test '{config.name}': OK (roqet failed as expected: {outcome_msg})"
                )
            else:
                logger.warning(
                    f"Test '{config.name}': FAILED (roqet command failed: {outcome_msg})"
                )
                if roqet_execution_data["stderr"]:
                    logger.warning(
                        f"  Stderr:\n{roqet_execution_data['stderr'].strip()}"
                    )

            finalize_test_result(test_result_summary, config.expect)
            return test_result_summary

        # Handle warning tests with exit code 0 (should be failure)
        if (
            config.test_type == TestType.WARNING_TEST.value
            and roqet_execution_data["returncode"] == 0
        ):
            logger.warning(
                f"Test '{config.name}': FAILED (warning test got exit code 0, expected 2)"
            )
            finalize_test_result(test_result_summary, config.expect)
            return test_result_summary

        # roqet command succeeded (exit code 0) - for non-warning tests
        if config.expect == TestResult.FAILED or config.expect == TestResult.XFAILED:
            # Test was expected to fail but succeeded - use centralized logic
            final_result, detail = TestTypeResolver.determine_test_result(
                config.expect, TestResult.PASSED
            )
            logger.warning(
                f"Test '{config.name}': FAILED (roqet succeeded, but was expected to fail)"
            )
            test_result_summary["result"] = "failure"
            test_result_summary["detail"] = detail
            finalize_test_result(test_result_summary, final_result)
            return test_result_summary

        # Roqet succeeded and was expected to pass
        if not config.execute:  # Syntax test (positive or negative)
            if config.expect == TestResult.FAILED:
                # Negative syntax test - roqet should have failed
                logger.warning(
                    f"Test '{config.name}': FAILED (negative syntax test, but roqet succeeded)"
                )
                test_result_summary["result"] = "failure"
            else:
                # Positive syntax test - roqet succeeded as expected
                logger.info(f"Test '{config.name}': OK (positive syntax check passed)")
                test_result_summary["result"] = "success"

            finalize_test_result(test_result_summary, config.expect)
            cleanup_temp_files()
            return test_result_summary

        # Evaluation test - need to compare results (skip for warning tests)
        if config.test_type != TestType.WARNING_TEST.value:
            actual_result_info = _process_actual_output(roqet_execution_data["stdout"])

            _compare_actual_vs_expected(
                test_result_summary,
                actual_result_info,
                config.result_file,
                config.cardinality_mode,
                global_debug_level,
            )
        else:
            # For warning tests, just mark as success if we got here (exit code 2 was handled as success)
            logger.info(f"Test '{config.name}': OK (warning test passed)")
            test_result_summary["result"] = "success"

        finalize_test_result(test_result_summary, config.expect)
        cleanup_temp_files()
        return test_result_summary

    except Exception as e:
        logger.error(f"Unexpected error running test '{config.name}': {e}")
        test_result_summary["result"] = "failure"
        test_result_summary["detail"] = f"Unexpected error: {e}"
        finalize_test_result(test_result_summary, config.expect)
        cleanup_temp_files()
        return test_result_summary


class SparqlTestRunner:
    """Main SPARQL test runner for executing W3C SPARQL test suites."""

    def __init__(self):
        self.args = None
        self.global_debug_level = 0

    def setup_argument_parser(self) -> argparse.ArgumentParser:
        """Setup argument parser for SPARQL tests."""
        parser = argparse.ArgumentParser(
            description="Run Rasqal against W3C SPARQL testsuites",
            formatter_class=argparse.RawDescriptionHelpFormatter,
            epilog="""
Examples:
  %(prog)s --manifest-file manifest.ttl /path/to/test/directory
  %(prog)s --manifest-file manifest.n3 --test-case test-01 /path/to/test/directory
            """,
        )

        parser.add_argument(
            "srcdir",
            nargs="?",
            type=Path,
            help="Source directory containing test files",
        )
        parser.add_argument(
            "--srcdir",
            "-s",
            type=Path,
            help="Source directory for test files and manifests (alternative to positional argument)",
        )
        parser.add_argument(
            "--manifest-file",
            "-m",
            type=Path,
            required=True,
            help="Manifest file to use (e.g., manifest.n3, manifest.ttl)",
        )
        parser.add_argument(
            "--test-case",
            help="Run specific test case",
        )
        parser.add_argument(
            "--debug",
            type=int,
            choices=[0, 1, 2],
            default=0,
            help="Debug level (0=quiet, 1=normal, 2=verbose)",
        )
        parser.add_argument(
            "--earl-report",
            type=Path,
            help="Generate EARL report to specified file",
        )
        parser.add_argument(
            "--junit-report",
            type=Path,
            help="Generate JUnit XML report to specified file",
        )
        parser.add_argument(
            "--preserve-files",
            action="store_true",
            help="Preserve temporary files for debugging",
        )
        parser.add_argument(
            "--warnings",
            "-W",
            type=int,
            default=0,
            help="Warning level for roqet (0-3)",
        )

        return parser

    def process_arguments(self, args: argparse.Namespace) -> None:
        """Process arguments and setup runner state."""
        self.args = args
        self.global_debug_level = args.debug

        # Handle srcdir argument - use -s if provided, otherwise use positional
        if args.srcdir:
            self.args.srcdir = args.srcdir
        elif not hasattr(self.args, "srcdir") or not self.args.srcdir:
            # Default to current directory if no srcdir provided
            self.args.srcdir = Path(".")

        # Setup logging
        if args.debug >= 2:
            logging.getLogger().setLevel(logging.DEBUG)
        elif args.debug >= 1:
            logging.getLogger().setLevel(logging.INFO)

        # Set global file preservation flag
        global _preserve_debug_files
        _preserve_debug_files = args.preserve_files

    def discover_and_filter_tests(
        self,
    ) -> Tuple[List[TestConfig], List[TestConfig], ManifestParser]:
        """Discover and filter tests from manifest."""
        if not self.args.manifest_file:
            raise ValueError("--manifest-file is required")

        if not self.args.manifest_file.exists():
            raise FileNotFoundError(
                f"Manifest file not found: {self.args.manifest_file}"
            )

        parser = ManifestParser(self.args.manifest_file)
        test_configs = parser.get_tests(self.args.srcdir)

        # Set warning level on all test configurations
        for test_config in test_configs:
            test_config.warning_level = self.args.warnings

        # Filter tests if specific test case requested
        if self.args.test_case:
            tests_to_run = [t for t in test_configs if t.name == self.args.test_case]
            if not tests_to_run:
                raise ValueError(f"Test case '{self.args.test_case}' not found")
        else:
            tests_to_run = test_configs

        return tests_to_run, test_configs, parser

    def run_all_tests(
        self,
        tests_to_run: List[TestConfig],
        all_tests: List[TestConfig],
    ) -> Tuple[
        List[TestConfig],
        List[TestConfig],
        List[TestConfig],
        List[Dict[str, Any]],
        float,
    ]:
        """Run all tests and return results."""
        start_time = time.time()

        passed_tests = []
        failed_tests = []
        skipped_tests = []
        test_results_data = []

        for test_config in tests_to_run:
            logger.info(f"Running test: {test_config.name}")

            result = run_single_test(test_config, self.global_debug_level)
            test_results_data.append(result)

            if result["is_success"]:
                passed_tests.append(test_config)
            else:
                failed_tests.append(test_config)

        elapsed_time = time.time() - start_time

        return (
            passed_tests,
            failed_tests,
            skipped_tests,
            test_results_data,
            elapsed_time,
        )

    def summarize_and_report(
        self,
        passed_tests: List[TestConfig],
        failed_tests: List[TestConfig],
        skipped_tests: List[TestConfig],
        test_results_data: List[Dict[str, Any]],
        elapsed_time_suite: float,
    ):
        """Summarize results and generate reports."""
        total_tests = len(passed_tests) + len(failed_tests) + len(skipped_tests)

        logger.info(f"Test suite completed in {elapsed_time_suite:.3f} seconds")
        logger.info(f"Total tests: {total_tests}")
        logger.info(f"Passed: {len(passed_tests)}")
        logger.info(f"Failed: {len(failed_tests)}")
        logger.info(f"Skipped: {len(skipped_tests)}")

        # Generate reports if requested
        if self.args.earl_report:
            generate_earl_report(self.args.earl_report, test_results_data, self.args)

        if self.args.junit_report:
            generate_junit_report(
                self.args.junit_report, test_results_data, self.args, elapsed_time_suite
            )

    def main(self) -> int:
        """Main entry point for SPARQL testing."""
        parser = self.setup_argument_parser()
        args = parser.parse_args()
        self.process_arguments(args)

        try:
            # Discover and filter tests
            tests_to_run, all_tests, parser = self.discover_and_filter_tests()

            # Run tests
            (
                passed_tests,
                failed_tests,
                skipped_tests,
                test_results_data,
                elapsed_time,
            ) = self.run_all_tests(tests_to_run, all_tests)

            # Summarize and report
            self.summarize_and_report(
                passed_tests,
                failed_tests,
                skipped_tests,
                test_results_data,
                elapsed_time,
            )

            # Return appropriate exit code based on test results
            # 0 = all tests passed, 1 = some tests failed
            return 0 if not failed_tests else 1

        except Exception as e:
            logger.error(f"Error running SPARQL tests: {e}")
            return 1


def main():
    """Main entry point for SPARQL testing."""
    runner = SparqlTestRunner()
    return runner.main()


if __name__ == "__main__":
    import sys

    sys.exit(main())

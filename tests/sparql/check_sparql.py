#!/usr/bin/env python3
#
# check-sparql - Run Rasqal against W3C SPARQL testsuites
#
# USAGE: check-sparql [options] [TEST]
#
# DEPRECATED: This script is deprecated. Use 'run-sparql-tests' from tests/bin/ instead.
# This script will be removed in a future version.
#
# Copyright (C) 2025, David Beckett http://www.dajobe.org/
#
# This package is Free Software and part of Redland http://librdf.org/
#
# It is licensed under the following three licenses as alternatives:
#   1. GNU Lesser General Public License (LGPL) V2.1 or any newer version
#   2. GNU General Public License (GPL) V2 or any newer version
#   3. Apache License, V2.0 or any newer version
#
# You may not use this file except in compliance with at least one of
# the above three licenses.
#
# See LICENSE.html or LICENSE.txt at the top of this package for the
# complete terms and further detail along with the license texts for
# the licenses in COPYING.LIB, COPYING and LICENSE-2.0.txt respectively.
#
# Requires:
#   roqet (from rasqal) compiled in the parent directory
#   to-ntriples (Rasqal utility) for manifest parsing
#
# Depends on a variety of rasqal internal debug print formats
#

import os
import argparse
import logging
import warnings
from pathlib import Path
import time
import re
import subprocess
import xml.etree.ElementTree as ET
from typing import List, Dict, Any, Optional, Tuple, Union
from dataclasses import dataclass, field
from enum import Enum

import sys
from pathlib import Path

# Issue deprecation warning
warnings.warn(
    "The 'check_sparql.py' script is deprecated. Use 'run-sparql-tests' from tests/bin/ instead. "
    "This script will be removed in a future version.",
    DeprecationWarning,
    stacklevel=1,
)

# Add the parent directory to the Python path to find rasqal_test_util
sys.path.append(str(Path(__file__).resolve().parent.parent))

from rasqal_test_util import (
    Namespaces,
    TestResult,
    TestType,
    TestConfig,
    find_tool,
    run_command,
    ManifestParser,
    SparqlTestError,
    ManifestParsingError,
    UtilityNotFoundError,
)

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

# Temporary file names (using Path for consistency)
ROQET_OUT = Path("roqet.out")
RESULT_OUT = Path("result.out")
ROQET_TMP = Path("roqet.tmp")
ROQET_ERR = Path("roqet.err")
DIFF_OUT = Path("diff.out")
TO_NTRIPLES_ERR = Path("to_ntriples.err")

NS = Namespaces()


# --- Core Test Runner Functions ---


def finalize_test_result(
    test_result_summary: Dict[str, Any], expect_status: TestResult
):
    """
    Sets the final 'is_success' status based on the execution outcome and expected status.
    """
    execution_outcome = test_result_summary.get("result", "failure")
    test_result_summary["is_success"] = (
        execution_outcome == "success" and expect_status == TestResult.PASSED
    ) or (execution_outcome == "failure" and expect_status == TestResult.FAILED)


def _execute_roqet(config: TestConfig) -> Dict[str, Any]:
    """
    Executes the roqet command for a given test configuration.
    Returns a dictionary containing stdout, stderr, and return code.
    """
    roqet_args = [ROQET, "-i", config.language]
    if config.test_type == TestType.CSV_RESULT_FORMAT_TEST.value:
        roqet_args.extend(["-r", "csv"])
    else:
        roqet_args.extend(["-d", "debug"])  # Debug output for analysis
    roqet_args.extend(["-W", str(config.warning_level)])

    # Only use non-None data files
    for df in [d for d in config.data_files if d is not None]:
        roqet_args.extend(["-D", str(df)])
    for ndf in [n for n in config.named_data_files if n is not None]:
        roqet_args.extend(["-G", str(ndf)])

    if not config.execute:
        roqet_args.append("-n")  # Don't execute query, just parse

    # Always pass the test file as a file:// URI if it is an absolute path
    test_file_path = str(config.test_file)
    if os.path.isabs(test_file_path):
        test_file_uri = f"file://{test_file_path}"
    else:
        test_file_uri = test_file_path
    roqet_args.append(test_file_uri)

    # Simplified logging of roqet arguments to avoid backslash/quoting issues
    logger.debug(f"Executing roqet: {' '.join(map(str, roqet_args))}")

    start_time = time.time()
    process = run_command(
        roqet_args, CURDIR, f"Error running roqet for '{config.name}'"
    )
    elapsed_time = time.time() - start_time

    # Save raw outputs to temp files only if preserving files
    if _preserve_debug_files:
        ROQET_TMP.write_text(process.stdout)
        ROQET_ERR.write_text(process.stderr)

    return {
        "stdout": process.stdout,
        "stderr": process.stderr,
        "returncode": process.returncode,
        "elapsed_time": elapsed_time,
        "query_cmd": " ".join(map(str, roqet_args)),  # Use simplified join here as well
    }


_projected_vars_order: List[str] = (
    []
)  # Global to hold variable order across calls if needed


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

    # Write to file only if preserving files
    if _preserve_debug_files:
        ROQET_OUT.write_text("\n".join(final_output_lines) + "\n")

    return {
        "result_type": result_type,
        "roqet_results_count": roqet_results_count,
        "vars_order": _projected_vars_order[:],  # Return a copy
        "is_sorted_by_query": is_sorted_by_query,
        "boolean_value": actual_boolean_value,  # Add boolean value for ASK queries
    }


def find_manifest(srcdir: Path, explicit_manifest: Optional[Path]) -> Path:
    """
    Determines the manifest file path. If explicit_manifest is provided,
    it's used. Otherwise, it searches for manifest files in srcdir.
    """
    if explicit_manifest:
        manifest_path = srcdir / explicit_manifest
        if not manifest_path.is_file():
            raise ManifestParsingError(
                f"Explicit manifest file not found: {manifest_path}"
            )
        return manifest_path
    else:
        # Look for common manifest file names
        for mf_name in [
            "manifest.ttl",
            "manifest.n3",
            "manifest-good.n3",
            "manifest-bad.n3",
        ]:
            manifest_path = srcdir / mf_name
            if manifest_path.is_file():
                return manifest_path
        raise ManifestParsingError(
            f"No manifest file found in {srcdir}. Looked for manifest.ttl, manifest.n3, manifest-good.n3, manifest-bad.n3"
        )


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

            # Use expected_vars_order if available, otherwise dynamically determined current_vars_order
            order_to_use = (
                expected_vars_order if expected_vars_order else current_vars_order
            )
            # Ensure all expected variables are present, fill with "NULL" if missing
            formatted_row_parts = [
                f"{var}={row_data.get(var, NS.RS + 'undefined')}"
                for var in order_to_use
            ]
            parsed_rows_for_output.append(f"row: [{', '.join(formatted_row_parts)}]")

        if sort_output:
            parsed_rows_for_output.sort()

        # Write to file only if preserving files
        if _preserve_debug_files:
            RESULT_OUT.write_text("\n".join(parsed_rows_for_output) + "\n")
        #        # Always write to RESULT_OUT since it's used for comparison
        #        RESULT_OUT.write_text("\n".join(parsed_rows_for_output) + "\n")
        return {"count": len(parsed_rows_for_output)}

    except UtilityNotFoundError as e:
        logger.error(e)
        return None
    except Exception as e:
        logger.error(
            f"Error in read_query_results_file for {abs_result_file_path}: {e}"
        )
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

    except subprocess.TimeoutExpired:
        logger.error(f"Timeout parsing {format_type} file {file_path}")
        return None
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

        # Write to file only if preserving files (consistent with existing behavior)
        if _preserve_debug_files:
            RESULT_OUT.write_text("\n".join(parsed_rows_for_output) + "\n")

        return {"count": len(parsed_rows_for_output)}

    except ET.ParseError as e:
        logger.error(f"Error parsing SRX XML: {e}")
        return None
    except Exception as e:
        logger.error(f"Error processing SRX content: {e}")
        return None


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
    except UtilityNotFoundError as e:
        logger.error(e)
        return None
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
            # Write to file only if preserving files
            if _preserve_debug_files:
                RESULT_OUT.write_text("\n".join(sorted_expected_triples) + "\n")
        else:
            logger.warning(
                f"Test '{name}': FAILED (could not read/parse expected graph result file {expected_result_file} or it's not explicitly empty)"
            )
            test_result_summary["result"] = "failure"
            return
    elif actual_result_type in ["bindings", "boolean"]:
        result_file_ext = expected_result_file.suffix.lower()
        expected_result_format = "turtle"  # Default for unknown extensions
        if result_file_ext == ".srx":
            expected_result_format = "xml"
        elif result_file_ext == ".srj":
            expected_result_format = "srj"
        elif result_file_ext in [".csv", ".tsv"]:
            expected_result_format = result_file_ext[1:]
        elif result_file_ext == ".rdf":
            expected_result_format = "rdfxml"

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
            comparison_rc = 0
            logger.debug(
                f"Boolean comparison successful: expected={expected_boolean}, actual={actual_boolean}"
            )
        else:
            comparison_rc = 1
            logger.debug(
                f"Boolean comparison failed: expected={expected_boolean}, actual={actual_boolean}"
            )
            # Write comparison details for debugging
            if _preserve_debug_files:
                DIFF_OUT.write_text(
                    f"Expected: {expected_boolean}\nActual: {actual_boolean}\n"
                )
    else:
        diff_process = run_command(
            [DIFF_CMD, "-u", str(RESULT_OUT), str(ROQET_OUT)],
            CURDIR,
            f"Error running diff for '{name}'",
        )
        # Write diff output only if preserving files
        if _preserve_debug_files:
            DIFF_OUT.write_text(diff_process.stdout)
        comparison_rc = diff_process.returncode

    if comparison_rc != 0:
        # Special handling for lax cardinality for bindings results
        if not (
            actual_result_type == "bindings"
            and cardinality_mode == "lax"
            and actual_results_count <= expected_results_count
        ):
            msg = f"Test '{name}': FAILED (Results differ)."
            if (
                actual_result_type == "bindings"
                and expected_results_count != actual_results_count
                and cardinality_mode != "lax"
            ):
                msg = f"Test '{name}': FAILED (Expected {expected_results_count} result(s), got {actual_results_count}). Results may also differ."
            logger.warning(msg)
            diff_content = DIFF_OUT.read_text().strip()
            test_result_summary["diff"] = diff_content
            if global_debug_level > 1 or (diff_content and len(diff_content) < 1000):
                logger.warning(f"  Differences:\n{diff_content}")
            else:
                logger.warning(f"  Differences written to {DIFF_OUT}")
            test_result_summary["result"] = "failure"
        else:  # Lax cardinality success
            logger.info(
                f"Test '{name}': OK (Lax cardinality: {actual_results_count} vs {expected_results_count} expected, diff ignored)."
            )
            test_result_summary["result"] = (
                "success"  # Explicitly set to success for lax cardinality match
            )
    else:  # comparison_rc == 0
        logger.info(f"Test '{name}': OK (Results match)")
        test_result_summary["result"] = "success"  # Explicitly set to success


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
            # Write diff output only if preserving files
            if _preserve_debug_files:
                diff_output_path.write_text(
                    process.stdout + process.stderr
                )  # NTC can write output to stdout/stderr
            if process.stderr:
                logger.debug(f"NTC stderr: {process.stderr.strip()}")
            return process.returncode
        except UtilityNotFoundError as e:
            logger.warning(f"{e}. Falling back to diff.")
        except TestExecutionError as e:
            logger.warning(f"{e}. Falling back to diff.")

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
                # Write diff output only if preserving files
                if _preserve_debug_files:
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
            except UtilityNotFoundError as e:
                logger.warning(f"{e}. Falling back to diff.")
            except TestExecutionError as e:
                logger.warning(f"{e}. Falling back to diff.")

    # Fallback to system diff
    cmd_list = [DIFF_CMD, "-u", abs_file1, abs_file2]
    logger.debug(f"Comparing graphs using system diff: {' '.join(cmd_list)}")
    try:
        process = run_command(
            cmd_list, CURDIR, f"Error running system diff ('{DIFF_CMD}')"
        )
        # Write diff output only if preserving files
        if _preserve_debug_files:
            diff_output_path.write_text(process.stdout)
        return process.returncode
    except UtilityNotFoundError as e:
        logger.error(e)
        diff_output_path.write_text(f"Error: {e}\n")
        return 2  # Indicate a utility not found error
    except TestExecutionError as e:
        logger.error(e)
        diff_output_path.write_text(f"Error running diff command: {e}\n")
        return 2  # Indicate a general error


def get_rasqal_version() -> str:
    """Retrieves the roqet (Rasqal) version string."""
    try:
        return run_command(
            [ROQET, "-v"], CURDIR, "Could not get roqet version"
        ).stdout.strip()
    except (UtilityNotFoundError, TestExecutionError) as e:
        logger.warning(f"Could not get roqet version: {e}")
        return "unknown"


RASQAL_VERSION = get_rasqal_version()
RASQAL_URL = "http://librdf.org/rasqal/"


def html_escape(text: Optional[str]) -> str:
    """Escapes special characters for HTML output."""
    return (
        text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
        if text
        else ""
    )


def generate_earl_report(
    earl_file_path: Path, test_results: List[Dict[str, Any]], args: argparse.Namespace
):
    """Generates an EARL (Evaluation and Report Language) report."""
    from datetime import datetime, timezone

    path = earl_file_path
    rasqal_date = datetime.now(timezone.utc).strftime("%Y-%m-%d")
    rasqal_name = f"Rasqal {RASQAL_VERSION}"

    is_new_file = not path.exists() or path.stat().st_size == 0

    with open(path, "a") as f:
        if is_new_file:
            f.write(
                f"""@prefix doap: <http://usefulinc.com/ns/doap#> .
@prefix earl: <http://www.w3.org/ns/earl#> .
@prefix foaf: <http://xmlns.com/foaf/0.1/> .
@prefix xsd: <http://www.w3.org/2001/XMLSchema#> .

_:author a foaf:Person;
     foaf:homepage <http://www.dajobe.org/>;
     foaf:name "Dave Beckett".

<{RASQAL_URL}> a doap:Project;
     doap:name "Rasqal";
     doap:homepage <{RASQAL_URL}>;
     doap:release
       [ a doap:Version;
         doap:created "{rasqal_date}"^^xsd:date ;
         doap:name "{rasqal_name}"].
"""
            )
        for res in test_results:
            outcome = "earl:pass" if res.get("is_success") else "earl:fail"
            if res.get("result") == "skipped":
                outcome = "earl:untested"
            f.write(
                f"""
[] a earl:Assertion;
   earl:assertedBy _:author;
   earl:result [ a earl:TestResult; earl:outcome {outcome} ];
   earl:subject <{RASQAL_URL}>;
   earl:test <{res['uri']}> .
"""
            )
    logger.info(f"EARL report generated at {earl_file_path}")


def generate_junit_report(
    junit_file_path: Path,
    test_results: List[Dict[str, Any]],
    args: argparse.Namespace,
    suite_elapsed_time: float,
):
    """Generates a JUnit XML report summarizing test results."""
    from datetime import datetime, timezone
    import socket

    ts = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S")
    name = f"Rasqal {RASQAL_VERSION}"
    host = socket.gethostname()
    suite_name = args.suite or "rasqal.sparql.testsuite"

    # Filter out skipped tests for counts
    relevant_tests = [tr for tr in test_results if tr.get("result") != "skipped"]
    tests_c = len(relevant_tests)
    failures_c = len([tr for tr in relevant_tests if not tr.get("is_success", False)])

    with open(junit_file_path, "w") as f:
        f.write(
            f"""<?xml version="1.0" encoding="UTF-8"?>
<testsuites>
  <testsuite name="{html_escape(suite_name)}" timestamp="{ts}" hostname="{html_escape(host)}" tests="{tests_c}" failures="{failures_c}" errors="0" time="{suite_elapsed_time:.3f}">
    <properties>
       <property name="author-name" value="Dave Beckett" />
       <property name="author-homepage" value="http://www.dajobe.org/" />
       <property name="project-name" value="Rasqal" />
       <property name="project-uri" value="{RASQAL_URL}" />
       <property name="project-version" value="{html_escape(name)}" />
    </properties>
"""
        )
        for res in test_results:
            if res.get("result") == "skipped":
                f.write(
                    f"""    <testcase name="{html_escape(res['name'])}" classname="{html_escape(res['uri'])}" time="0.000"><skipped/></testcase>
"""
                )
                continue

            etime = f"{res.get('elapsed-time', 0):.3f}"
            f.write(
                f"""    <testcase name="{html_escape(res['name'])}" classname="{html_escape(res['uri'])}" time="{etime}">
"""
            )
            if not res.get("is_success"):
                roq_stat = res.get("roqet-status-code", -1)
                msg = "Test failed."
                if roq_stat != 0 and roq_stat != -1:
                    msg = f"Test failed (roq_stat exited {roq_stat})"

                # Join non-empty parts for failure message
                txt_parts = [
                    f"STDOUT:\n{res['stdout']}" if res.get("stdout") else "",
                    f"STDERR:\n{res['stderr']}" if res.get("stderr") else "",
                    f"DIFF:\n{res['diff']}" if res.get("diff") else "",
                ]
                # Filter out empty strings before joining
                full_message = "\n\n".join(filter(None, txt_parts))

                f.write(
                    f"""      <failure message="{html_escape(msg)}" type="TestFailure">
{html_escape(full_message)}
      </failure>
"""
                )
            f.write(
                f"""    </testcase>
"""
            )
        f.write(
            f"""  <system-out></system-out>
  <system-err></system-err>
  </testsuite>
</testsuites>
"""
        )
    logger.info(f"JUnit report generated at {junit_file_path}")


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
            f"run_single_test: (URI: {config.test_uri})\n  name: {config.name}\n  language: {config.language}\n  query: {config.test_file}\n  data: {'; '.join(map(str, config.data_files))}\n  named data: {'; '.join(map(str, config.named_data_files))}\n  result file: {config.result_file or 'none'}\n  expect: {config.expect.value}\n  card mode: {config.cardinality_mode}\n  execute: {config.execute}\n  test_type: {config.test_type}"
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
        # For warning tests, exit code 2 (warning) is considered success
        if (
            config.test_type == TestType.WARNING_TEST.value
            and roqet_execution_data["returncode"] == 2
        ):
            test_result_summary["result"] = "success"
        else:
            test_result_summary["result"] = (
                "failure" if roqet_execution_data["returncode"] != 0 else "success"
            )

        if roqet_execution_data["returncode"] != 0 and not (
            config.test_type == TestType.WARNING_TEST.value
            and roqet_execution_data["returncode"] == 2
        ):  # roqet command failed (but not for warning tests with exit code 2)
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
            logger.debug(
                f"Before finalize (roqet failed): config.expect={config.expect}, returncode={roqet_execution_data['returncode']}, is_success={test_result_summary['is_success']}"
            )
            finalize_test_result(test_result_summary, config.expect)
            logger.debug(
                f"After finalize (roqet failed): is_success={test_result_summary['is_success']}"
            )
            return test_result_summary

        # roqet command succeeded (exit code 0)
        if (
            config.expect == TestResult.FAILED
        ):  # Roqet succeeded, but was expected to fail
            logger.warning(
                f"Test '{config.name}': FAILED (roqet succeeded, but was expected to fail)"
            )
            logger.debug(
                f"Before finalize (roqet succeeded, expected fail): config.expect={config.expect}, returncode={roqet_execution_data['returncode']}, is_success={test_result_summary['is_success']}"
            )
            finalize_test_result(test_result_summary, config.expect)
            logger.debug(
                f"After finalize (roqet succeeded, expected fail): is_success={test_result_summary['is_success']}"
            )
            return test_result_summary

        # Roqet succeeded and was expected to pass (or it's an eval test needing further checks)
        if config.test_type == TestType.CSV_RESULT_FORMAT_TEST.value:
            if not config.result_file:
                logger.warning(
                    f"Test '{config.name}': FAILED (CSVResultFormatTest but no result_file specified)"
                )
                test_result_summary["result"] = "failure"  # Mark execution as failed
            else:
                diff_process = run_command(
                    [DIFF_CMD, "-u", str(ROQET_TMP), str(config.result_file)],
                    CURDIR,
                    f"Error comparing CSV output for '{config.name}'",
                )
                if diff_process.returncode != 0:
                    logger.warning(
                        f"Test '{config.name}': FAILED (CSV output differs from {config.result_file})"
                    )
                    test_result_summary["diff"] = diff_process.stdout
                    DIFF_OUT.write_text(diff_process.stdout)
                    if global_debug_level > 0 or len(diff_process.stdout) < 1000:
                        logger.warning(f"  Differences:\n{diff_process.stdout.strip()}")
                    else:
                        logger.warning(f"  Differences written to {DIFF_OUT}")
                    test_result_summary["result"] = "failure"  # Diff failed
                else:
                    logger.info(f"Test '{config.name}': OK (CSV output matches)")
                    test_result_summary["result"] = "success"
            finalize_test_result(test_result_summary, config.expect)
            return test_result_summary

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
            return test_result_summary

        rasqal_errors_in_stderr = [
            line
            for line in roqet_execution_data["stderr"].splitlines()
            if "rasqal error -" in line
        ]
        if rasqal_errors_in_stderr:
            logger.warning(
                f"Test '{config.name}': FAILED (roqet succeeded but rasqal reported errors in stderr)"
            )
            for err_line in rasqal_errors_in_stderr:
                logger.warning(f"  {err_line}")
            test_result_summary["errors"] = "\n".join(rasqal_errors_in_stderr)
            test_result_summary["result"] = (
                "failure"  # Error in output means execution failed objective
            )
            finalize_test_result(test_result_summary, config.expect)
            return test_result_summary

        # 2. Process Actual Output (from ROQET_TMP)
        actual_output_info = _process_actual_output(roqet_execution_data["stdout"])

        # 3. Compare Actual vs. Expected Results (skip for warning tests)
        if config.test_type != TestType.WARNING_TEST.value:
            _compare_actual_vs_expected(
                test_result_summary,
                actual_output_info,
                config.result_file,
                config.cardinality_mode,
                global_debug_level,
            )
        else:
            # For warning tests, just mark as success if we got here (exit code 2 was handled as success)
            logger.info(f"Test '{config.name}': OK (warning test passed)")
            test_result_summary["result"] = "success"

    except (UtilityNotFoundError, TestExecutionError) as e:
        logger.error(f"Test '{config.name}': FAILED: {e}")
        test_result_summary["stderr"] = str(e)
        test_result_summary["result"] = "failure"
    except Exception as e:
        logger.error(
            f"An unexpected error occurred while running test '{config.name}': {type(e).__name__}: {e}"
        )
        import traceback

        logger.error(
            traceback.format_exc()
        )  # Print full traceback for unexpected errors
        test_result_summary["stderr"] = f"Unexpected error: {type(e).__name__}: {e}"
        test_result_summary["result"] = "failure"

    finalize_test_result(test_result_summary, config.expect)
    return test_result_summary


# --- Main function decomposition ---


def _parse_arguments() -> argparse.Namespace:
    """Parses command line arguments."""
    parser = argparse.ArgumentParser(description="Run SPARQL tests.")
    parser.add_argument(
        "--debug",
        "-d",
        action="count",
        default=0,
        help="Increase debug verbosity (e.g., -d, -dd)",
    )
    parser.add_argument(
        "--srcdir",
        "-s",
        default=".",
        type=Path,
        help="Source directory for test files and manifests.",
    )
    parser.add_argument(
        "--input",
        "-i",
        default="sparql",
        help="Input language for roqet (e.g., 'sparql', 'sparql11').",
    )
    parser.add_argument(
        "--manifest", "-m", type=Path, help="Explicit path to a test manifest file."
    )
    parser.add_argument(
        "--earl", "-e", type=Path, help="Path to output an EARL report."
    )
    parser.add_argument(
        "--junit", "-j", type=Path, help="Path to output a JUnit XML report."
    )
    parser.add_argument(
        "--suite", "-u", help="Name of the test suite for JUnit report."
    )
    parser.add_argument(
        "--approved",
        "-a",
        action="store_true",
        help="Only run tests explicitly marked as approved.",
    )
    parser.add_argument(
        "--warnings", "-W", type=int, default=0, help="Warning level for roqet (0-3)."
    )
    parser.add_argument(
        "test",
        nargs="?",
        help="Optional: Run a specific test by name or URI substring.",
    )
    args = parser.parse_args()
    return args


def _initialize_logging(debug_level: int, preserve_files: bool = False):
    """Sets the logging level based on debug argument."""
    if debug_level > 0:
        logger.setLevel(logging.DEBUG)
        logging.getLogger().setLevel(logging.DEBUG)  # Also set the root logger level

    # Set a global flag for file preservation
    global _preserve_debug_files
    _preserve_debug_files = preserve_files or debug_level >= 2

    logger.debug(
        f"Arguments parsed: debug={debug_level}, preserve_files={_preserve_debug_files}"
    )


def _discover_and_filter_tests(
    args: argparse.Namespace,
) -> Tuple[List[TestConfig], List[TestConfig], ManifestParser]:
    """Discovers all tests and returns both filtered and unfiltered lists."""
    manifest_file_path = find_manifest(args.srcdir, args.manifest)
    logger.info(f"Using manifest file: {manifest_file_path}")

    try:
        parser = ManifestParser(manifest_file_path)
        all_tests = parser.get_tests(args.srcdir, args.test)
    except (ManifestParsingError, SparqlTestError) as e:
        logger.critical(f"Failed to parse manifest: {e}")
        sys.exit(1)

    if args.test and not all_tests:
        logger.error(f"Test '{args.test}' not found in manifest.")
        sys.exit(1)  # Critical early exit
    if not all_tests:
        logger.info("No tests found or selected to run.")
        sys.exit(0)  # Critical early exit

    # Apply filtering to get tests to run
    tests_to_run = []
    for test_config in all_tests:
        if test_config.should_run_test(approved_only=args.approved):
            tests_to_run.append(test_config)
        else:
            skip_reason = test_config.get_skip_reason(approved_only=args.approved)
            logger.debug(
                f"Test '{test_config.name}' ({test_config.test_uri}) skipped: {skip_reason}"
            )

    return tests_to_run, all_tests, parser


def _run_all_tests(
    tests_to_run: List[TestConfig],
    all_tests: List[TestConfig],
    args: argparse.Namespace,
) -> Tuple[
    List[TestConfig], List[TestConfig], List[TestConfig], List[Dict[str, Any]], float
]:
    """Executes filtered tests and marks others as skipped."""
    passed_tests: List[TestConfig] = []
    failed_tests: List[TestConfig] = []
    skipped_tests: List[TestConfig] = []
    test_results_data: List[Dict[str, Any]] = []

    # Mark tests that weren't run as skipped
    tests_to_run_uris = {test.test_uri for test in tests_to_run}
    for test in all_tests:
        if test.test_uri not in tests_to_run_uris:
            skipped_tests.append(test)
            test_results_data.append(
                {
                    "name": test.name,
                    "uri": test.test_uri,
                    "result": "skipped",
                    "is_success": False,
                    "reason": test.get_skip_reason(approved_only=args.approved),
                }
            )

    # Run the actual tests
    start_time_suite = time.time()
    for test_config in tests_to_run:
        # Set runtime specific config from args
        test_config.language = test_config.language or args.input
        if (
            test_config.language == "sparql"
        ):  # If manifest didn't specify, use global input
            test_config.language = args.input
        test_config.warning_level = args.warnings

        test_result_details = run_single_test(test_config, args.debug)
        test_results_data.append(test_result_details)
        if test_result_details.get("is_success", False):
            passed_tests.append(test_config)
        else:
            failed_tests.append(test_config)

    elapsed_time_suite = time.time() - start_time_suite
    return (
        passed_tests,
        failed_tests,
        skipped_tests,
        test_results_data,
        elapsed_time_suite,
    )


def _summarize_and_report(
    passed_tests: List[TestConfig],
    failed_tests: List[TestConfig],
    skipped_tests: List[TestConfig],
    test_results_data: List[Dict[str, Any]],
    elapsed_time_suite: float,
    args: argparse.Namespace,
):
    """Summarizes test results, cleans up, and generates reports."""
    # Clean up temporary files
    if not _preserve_debug_files:
        for f_path in [
            ROQET_OUT,
            RESULT_OUT,
            ROQET_TMP,
            ROQET_ERR,
            DIFF_OUT,
            TO_NTRIPLES_ERR,
        ]:
            try:
                f_path.unlink(missing_ok=True)
            except OSError as e:
                logger.warning(f"Could not delete temp file {f_path}: {e}")

    # Summarize results
    if failed_tests:
        logger.info(f"{len(failed_tests)} FAILED tests:")
        for t_conf in failed_tests:
            res = next(
                (r for r in test_results_data if r["uri"] == t_conf.test_uri), None
            )
            reason_msg = ""
            if res and res.get("stderr"):
                for line in res["stderr"].splitlines():
                    if "rasqal error" in line or "Error" in line:
                        reason_msg = f" ({line.split('rasqal error -')[-1].split('Error -')[-1].strip()})"
                        break
            logger.info(f"  FAILED: {t_conf.name}{reason_msg}")

    logger.info(
        f"Summary: {len(passed_tests)} tests passed, {len(failed_tests)} tests failed, "
        f"{len(skipped_tests)} tests skipped in {elapsed_time_suite:.2f}s"
    )

    # Generate reports if requested
    if args.earl:
        generate_earl_report(args.earl, test_results_data, args)
    if args.junit:
        generate_junit_report(args.junit, test_results_data, args, elapsed_time_suite)


# --- Main function ---


def main():
    """Main entry point for the SPARQL test runner."""
    args = _parse_arguments()
    _initialize_logging(args.debug, preserve_files=bool(args.test))

    try:
        tests_to_run, all_tests, parser = _discover_and_filter_tests(args)
        (
            passed_tests,
            failed_tests,
            skipped_tests,
            test_results_data,
            elapsed_time_suite,
        ) = _run_all_tests(tests_to_run, all_tests, args)

        _summarize_and_report(
            passed_tests,
            failed_tests,
            skipped_tests,
            test_results_data,
            elapsed_time_suite,
            args,
        )
        sys.exit(len(failed_tests))

    except (
        SparqlTestError,
        ManifestParsingError,
        UtilityNotFoundError,
        TestExecutionError,
    ) as e:
        logger.critical(f"A critical error occurred: {e}")
        sys.exit(1)
    except Exception as e:
        logger.critical(
            f"An unexpected critical error occurred: {type(e).__name__}: {e}",
            exc_info=True,
        )
        sys.exit(1)


if __name__ == "__main__":
    main()

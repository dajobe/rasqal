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
from ..utils import (
    find_tool,
    run_command,
    SparqlTestError,
    compare_files_custom_diff,
    set_preserve_files,
    get_temp_file_path,
    cleanup_temp_files,
)
from ..manifest import ManifestParser, UtilityNotFoundError
from ..execution import run_roqet_with_format, filter_format_output

# Create namespace instance for backward compatibility
NS = Namespaces()

# Configure logging - will be set properly in process_arguments
logger = logging.getLogger(__name__)


CURDIR = Path.cwd()


# --- Custom Exceptions ---
class TestExecutionError(SparqlTestError):
    """Raised when a test fails due to execution issues (e.g., roqet crash)."""

    pass


# --- Helper functions ---
def normalize_blank_nodes(text_output: str) -> str:
    """Normalizes blank node IDs in text output for consistent comparison."""
    # Handle both "blank \w+" and "_:identifier" patterns
    # First normalize "blank \w+" to "blank _"
    text_output = re.sub(r"blank \w+", "blank _", text_output)

    # Handle anonymous blank nodes "[]" in Turtle/N3 format
    counter = 0

    def replace_blank_node(match):
        nonlocal counter
        counter += 1
        return f"_:bnode_{counter}"

    # Replace anonymous blank nodes [] with numbered blank nodes
    text_output = re.sub(r"\[\]", replace_blank_node, text_output)

    # Then normalize named blank nodes "_:identifier" to "_:bnode_<counter>"
    counter = 0  # Reset counter for named blank nodes
    text_output = re.sub(r"_:[\w-]+", replace_blank_node, text_output)
    return text_output


ROQET = find_tool("roqet") or "roqet"
TO_NTRIPLES = find_tool("to-ntriples") or "to-ntriples"
RASQAL_COMPARE = find_tool("rasqal-compare") or "rasqal-compare"
DIFF_CMD = find_tool("diff") or "diff"


# --- Constants and Enums ---
# Global to hold variable order across calls if needed
_projected_vars_order: List[str] = []

# Use shared temp file system from utils


def finalize_test_result(
    test_result_summary: Dict[str, Any], expect_status: TestResult
):
    """Finalizes the test result based on expected vs actual outcome."""
    actual_result = test_result_summary["result"]

    if expect_status == TestResult.FAILED:
        # Expected to fail
        if actual_result == "failed":
            test_result_summary["is_success"] = True
            test_result_summary["result"] = "passed"
        else:
            test_result_summary["is_success"] = False
            test_result_summary["result"] = "failed"
    else:
        # Expected to pass
        if actual_result == "success":
            test_result_summary["is_success"] = True
        else:
            test_result_summary["is_success"] = False


def _handle_construct_query(
    config: TestConfig, additional_args: List[str]
) -> Optional[Dict[str, Any]]:
    """Handle CONSTRUCT queries by converting to N-Triples."""
    # Use turtle format to get the RDF graph
    turtle_stdout, turtle_stderr, turtle_rc = run_roqet_with_format(
        query_file=str(config.test_file),
        data_file=None,
        result_format="turtle",
        roqet_path=ROQET,
        additional_args=additional_args,
    )
    if turtle_rc not in (0, 2):
        logger.warning(f"roqet turtle execution failed rc={turtle_rc}: {turtle_stderr}")
        return None

    return _convert_turtle_to_ntriples(turtle_stdout)


def _convert_turtle_to_ntriples(turtle_stdout: str) -> Optional[Dict[str, Any]]:
    """Convert turtle output to N-Triples for comparison."""
    # Create a temporary file with the turtle content
    import tempfile
    import os

    with tempfile.NamedTemporaryFile(
        mode="w", suffix=".ttl", delete=False
    ) as temp_turtle:
        temp_turtle.write(turtle_stdout)
        temp_turtle_path = temp_turtle.name

    try:
        # Convert to N-Triples using to-ntriples
        ntriples_cmd = [
            TO_NTRIPLES,
            temp_turtle_path,
            f"file://{temp_turtle_path}",
        ]
        ntriples_result = run_command(
            cmd=ntriples_cmd,
            cwd=str(CURDIR),
            error_msg="Error converting turtle to N-Triples",
        )
        returncode, stdout, stderr = ntriples_result

        if returncode == 0:
            return _process_ntriples_success(stdout)
        else:
            return _process_ntriples_fallback(turtle_stdout)
    finally:
        # Clean up temporary file
        try:
            os.unlink(temp_turtle_path)
        except OSError:
            pass


def _process_ntriples_success(ntriples_output: str) -> Dict[str, Any]:
    """Process successful N-Triples conversion."""
    # Normalize blank nodes
    normalized_output = normalize_blank_nodes(ntriples_output)
    # Prepare sorted unique triples (skip directives)
    actual_triples = [
        line
        for line in normalized_output.split("\n")
        if line.strip() and not line.startswith("@")
    ]
    triple_count = len(actual_triples)
    sorted_actual_triples = "\n".join(sorted(set(actual_triples))) + (
        "\n" if actual_triples else ""
    )

    # Write normalized, sorted N-Triples output
    try:
        get_temp_file_path("roqet.tmp").write_text(normalized_output)
        get_temp_file_path("roqet.out").write_text(sorted_actual_triples)
    except Exception:
        pass

    return {
        "result_type": "graph",
        "roqet_results_count": triple_count,
        "vars_order": [],
        "is_sorted_by_query": False,
        "boolean_value": None,
        "content": normalized_output,
        "count": triple_count,
        "format": "ntriples",
    }


def _process_ntriples_fallback(turtle_stdout: str) -> Dict[str, Any]:
    """Process N-Triples conversion fallback using turtle output."""
    normalized_turtle = normalize_blank_nodes(turtle_stdout)
    # Count the actual triples in the result
    actual_triples = [
        line
        for line in normalized_turtle.split("\n")
        if line.strip() and not line.startswith("@")
    ]
    triple_count = len(actual_triples)
    sorted_actual_triples = "\n".join(sorted(set(actual_triples))) + (
        "\n" if actual_triples else ""
    )

    try:
        get_temp_file_path("roqet.tmp").write_text(normalized_turtle)
        get_temp_file_path("roqet.out").write_text(sorted_actual_triples)
    except Exception:
        pass

    return {
        "result_type": "graph",
        "roqet_results_count": triple_count,
        "vars_order": [],
        "is_sorted_by_query": False,
        "boolean_value": None,
        "content": turtle_stdout,
        "count": triple_count,
        "format": "turtle",
    }


def _handle_ask_query(
    config: TestConfig, additional_args: List[str]
) -> Optional[Dict[str, Any]]:
    """Handle ASK queries by extracting boolean result from debug output."""
    # Use debug flag to get the boolean result from debug output
    debug_cmd = [ROQET, "-i", (config.language or "sparql"), "-d", "debug"]
    for df in getattr(config, "data_files", []) or []:
        debug_cmd.extend(["-D", str(df)])
    for ng in getattr(config, "named_data_files", []) or []:
        debug_cmd.extend(["-G", str(ng)])
    debug_cmd.extend(["-W", str(getattr(config, "warning_level", 0))])
    debug_cmd.append(str(config.test_file))

    try:
        debug_result = run_command(
            cmd=debug_cmd,
            cwd=str(CURDIR),
            error_msg="Error running ASK query with debug",
        )
        returncode, stdout, stderr = debug_result
        if returncode not in (0, 2):
            logger.warning(f"roqet debug execution failed rc={returncode}")
            return None

        boolean_value = _parse_boolean_from_debug_output((returncode, stdout, stderr))
        if boolean_value is None:
            logger.warning("Could not parse boolean result from ASK query debug output")
            return None

        # Write to RESULT_OUT for comparison
        try:
            get_temp_file_path("result.out").write_text(str(boolean_value).lower())
        except Exception:
            pass

        return {
            "content": str(boolean_value).lower(),
            "count": 1,
            "result_type": "boolean",
            "type": "boolean",
            "boolean_value": boolean_value,
            "roqet_results_count": 1,
            "vars_order": [],
            "is_sorted_by_query": False,
        }
    except Exception as e:
        logger.warning(f"Error running ASK query with debug: {e}")
        return None


def _parse_boolean_from_debug_output(debug_result) -> Optional[bool]:
    """Parse boolean result from roqet debug output."""
    # Unpack the tuple result
    returncode, stdout, stderr = debug_result

    # Check both stdout and stderr for the boolean result
    for output in [stdout, stderr]:
        for line in output.splitlines():
            line = line.strip()
            if line.startswith("roqet: Query has a boolean result:"):
                # Extract just the boolean value from the end of the line
                result_part = line.split("boolean result:")[1].strip()
                if result_part.lower() == "true":
                    return True
                elif result_part.lower() == "false":
                    return False
    return None


def _handle_select_query(
    config: TestConfig,
    additional_args: List[str],
    expected_vars_order: List[str],
    sort_output: bool,
) -> Optional[Dict[str, Any]]:
    """Handle SELECT queries by executing and parsing results."""
    # For SELECT queries, execute the query and parse the results
    logger.debug(f"Executing SELECT query with simple format")
    simple_stdout, simple_stderr, simple_rc = run_roqet_with_format(
        query_file=str(config.test_file),
        data_file=None,
        result_format="simple",
        roqet_path=ROQET,
        additional_args=additional_args,
    )

    logger.debug(f"roqet simple execution rc={simple_rc}")
    logger.debug(f"roqet simple stdout length: {len(simple_stdout)}")
    logger.debug(f"roqet simple stderr length: {len(simple_stderr)}")

    if simple_rc not in (0, 2):
        logger.warning(f"roqet simple execution failed rc={simple_rc}: {simple_stderr}")
        return None

    return _parse_select_results(simple_stdout, expected_vars_order, sort_output)


def _parse_select_results(
    simple_stdout: str, expected_vars_order: List[str], sort_output: bool
) -> Dict[str, Any]:
    """Parse SELECT query results into normalized format."""
    # Parse simple rows output into normalized format
    parsed_rows_for_output: List[str] = []
    current_vars_order: List[str] = []
    first_row = True
    order_to_use: List[str] = expected_vars_order if expected_vars_order else []

    for line in simple_stdout.splitlines():
        line = line.strip()
        if not line.startswith("row: [") or not line.endswith("]"):
            continue

        content_line = normalize_blank_nodes(line[len("row: [") : -1])
        row_data: Dict[str, str] = {}
        if content_line:
            pairs = re.split(r",\s*(?=\S+=)", content_line)
            for pair in pairs:
                if "=" in pair:
                    var_name, var_val = pair.split("=", 1)
                    row_data[var_name] = var_val
                    if first_row and var_name not in current_vars_order:
                        current_vars_order.append(var_name)
        first_row = False

        if not expected_vars_order:
            order_to_use = current_vars_order

        formatted_row_parts = [
            f"{var}={row_data.get(var, NS.RS + 'undefined')}" for var in order_to_use
        ]
        parsed_rows_for_output.append(f"row: [{', '.join(formatted_row_parts)}]")

    if sort_output:
        parsed_rows_for_output.sort()

    content = "\n".join(parsed_rows_for_output) + "\n" if parsed_rows_for_output else ""

    try:
        roqet_tmp_path = get_temp_file_path("roqet.tmp")
        roqet_out_path = get_temp_file_path("roqet.out")
        roqet_tmp_path.write_text(content)
        roqet_out_path.write_text(content)
        # Debug message moved to level 2 - file paths are now unique when not using --preserve
    except Exception as e:
        logger.error(f"Error writing to output files: {e}")
        pass

    return {
        "result_type": "bindings",
        "roqet_results_count": len(parsed_rows_for_output),
        "vars_order": order_to_use,
        "is_sorted_by_query": False,
        "boolean_value": None,
        "content": content,
        "count": len(parsed_rows_for_output),
        "format": "bindings",
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
    import re  # Import re at the top of the function

    # Handle completely empty files (common in warning tests)
    try:
        content = result_file_path.read_text().strip()
        if not content:
            logger.debug(f"Detected empty result file: {result_file_path}")
            # Write empty result to RESULT_OUT for comparison
            try:
                get_temp_file_path("result.out").write_text("")
            except Exception:
                pass
            return {
                "content": "",
                "count": 0,
                "format": "bindings",
                "vars_order": expected_vars_order,
            }
    except Exception as e:
        logger.debug(f"Could not check if file is empty: {e}")

    # Handle boolean results (ASK queries) for CSV/TSV formats
    if result_format_hint in ["csv", "tsv"]:
        from .format_handlers import handle_csv_tsv_boolean

        boolean_result = handle_csv_tsv_boolean(result_file_path)
        if boolean_result:
            return boolean_result

    # Special handling for empty SRX files
    if result_format_hint == "xml":
        from .format_handlers import handle_empty_srx

        empty_srx_result = handle_empty_srx(result_file_path, expected_vars_order)
        if empty_srx_result:
            return empty_srx_result

    # Special handling for SRJ files (SPARQL Results JSON)
    if result_format_hint == "srj":
        from .format_handlers import handle_srj_file

        srj_result = handle_srj_file(result_file_path, expected_vars_order)
        if srj_result:
            return srj_result

    # General handling for other formats using roqet
    from .roqet_parser import parse_with_roqet

    return parse_with_roqet(
        result_file_path, result_format_hint, expected_vars_order, sort_output
    )


def _process_roqet_output(
    stdout: str, expected_vars_order: List[str], sort_output: bool
) -> Dict[str, Any]:
    """Process the output from roqet command."""
    parsed_rows_for_output: List[str] = []
    current_vars_order: List[str] = []
    first_row = True
    order_to_use: List[str] = expected_vars_order if expected_vars_order else []

    # Handle empty result sets - if no stdout, it means the file was read successfully but contains no results
    if not stdout.strip():
        # This is a valid case for empty result sets
        logger.debug("Empty result set detected")
        # For empty results, we still need to determine variable order from the expected vars
        if expected_vars_order:
            order_to_use = expected_vars_order
        else:
            # If no expected vars, create an empty result
            order_to_use = []
    else:
        # Process non-empty results
        logger.debug(
            f"Processing non-empty results, stdout lines: {len(stdout.splitlines())}"
        )
        from .roqet_parser import RoqetParser

        parser = RoqetParser()
        parsed_rows_for_output, order_to_use = parser._parse_roqet_rows(
            stdout, expected_vars_order
        )

    logger.debug(f"Parsed rows count: {len(parsed_rows_for_output)}")
    logger.debug(f"Order to use: {order_to_use}")

    if sort_output:
        parsed_rows_for_output.sort()
        logger.debug(f"After sorting: {parsed_rows_for_output}")

    # Always write to file for comparison, cleanup later if not preserving
    result_out_path = get_temp_file_path("result.out")
    content_to_write = (
        "\n".join(parsed_rows_for_output) + "\n" if parsed_rows_for_output else ""
    )
    result_out_path.write_text(content_to_write)

    return {
        "content": (
            "\n".join(parsed_rows_for_output) + "\n" if parsed_rows_for_output else ""
        ),
        "count": len(parsed_rows_for_output),
        "format": "bindings",
        "vars_order": order_to_use,
    }


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
            cmd=cmd,
            cwd=str(CURDIR),
            error_msg=f"Failed to parse {format_type} file {file_path}",
        )
        returncode, stdout, stderr = result

        if returncode != 0:
            logger.error(
                f"roqet failed to parse {format_type} file {file_path}: {stderr}"
            )
            return None

        if not stdout.strip():
            logger.warning(
                f"roqet produced empty output for {format_type} file {file_path}"
            )
            return {"count": 0, "vars": [], "results": []}

        # Parse the converted SRX output using XML parsing
        from .srx_parser import parse_srx_from_roqet_output

        return parse_srx_from_roqet_output(
            stdout, expected_vars_order, sort_output, get_temp_file_path
        )

    except Exception as e:
        logger.error(f"Error parsing {format_type} file {file_path}: {e}")
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
            cmd=cmd,
            cwd=str(CURDIR),
            error_msg=f"Error running '{TO_NTRIPLES}' for {abs_result_file_path}",
        )
        returncode, stdout, stderr = process
        to_ntriples_stderr = stderr

        if returncode != 0 or (
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
                    f"  {TO_NTRIPLES} exit code: {returncode}, stdout (first 200 chars): {stdout[:200] if stdout else '(empty)'}"
                )
            return None
        return stdout
    except Exception as e:
        logger.error(f"Error running '{TO_NTRIPLES}' for {abs_result_file_path}: {e}")
        return None


def _compare_actual_vs_expected(
    test_result_summary: Dict[str, Any],
    actual_result_info: Dict[str, Any],
    expected_result_file: Optional[Path],
    cardinality_mode: str,
    global_debug_level: int,
    use_rasqal_compare: bool = False,
):
    """
    Compares the actual roqet output with the expected results.
    Updates test_result_summary with comparison details.
    """
    name = test_result_summary["name"]
    logger.debug(
        f"_compare_actual_vs_expected called for test '{name}' with debug_level={global_debug_level}"
    )
    # Use the new ComparisonManager for result comparison
    from .debug_manager import compare_actual_vs_expected, handle_comparison_result

    # Perform comparison using the new manager
    comparison_success = compare_actual_vs_expected(
        actual_result_info, {}, name, use_rasqal_compare
    )

    # Handle the comparison result
    handle_comparison_result(comparison_success, name)

    # Update test result summary
    if comparison_success:
        test_result_summary["result"] = "passed"
    else:
        test_result_summary["result"] = "failed"


def _perform_comparison(
    actual_result_type: str,
    actual_result_info: Dict[str, Any],
    use_rasqal_compare: bool,
    name: str,
) -> int:
    """Perform the actual comparison based on result type."""
    # Use the new ComparisonManager for all comparison operations
    from .debug_manager import ComparisonManager

    manager = ComparisonManager(use_rasqal_compare)

    # Create a mock expected result info for the comparison
    expected_result_info = {}

    # Perform comparison using the manager
    comparison_success = manager.compare_actual_vs_expected(
        actual_result_info, expected_result_info, name
    )

    return 0 if comparison_success else 1


def _compare_graph_results(use_rasqal_compare: bool, name: str) -> int:
    """Compare graph results using rasqal-compare or internal comparison."""
    # Use the new ComparisonManager for graph comparison
    from .debug_manager import ComparisonManager

    manager = ComparisonManager(use_rasqal_compare)
    return 0 if manager._compare_graph_results(name) else 1


def _compare_srj_results(name: str) -> int:
    """Compare SRJ results using system diff."""
    # Use the new ComparisonManager for SRJ comparison
    from .debug_manager import ComparisonManager

    manager = ComparisonManager(False)  # Use system diff
    return 0 if manager._compare_srj_results(name) else 1


def _compare_boolean_results(actual_result_info: Dict[str, Any], name: str) -> int:
    """Compare boolean results directly."""
    # Use the new ComparisonManager for boolean comparison
    from .debug_manager import ComparisonManager

    manager = ComparisonManager(False)
    return 0 if manager._compare_boolean_results(actual_result_info, name) else 1


def _compare_bindings_results(actual_result_info: Dict[str, Any], name: str) -> int:
    """Compare bindings results using system diff."""
    # Use the new ComparisonManager for bindings comparison
    from .debug_manager import ComparisonManager

    manager = ComparisonManager(False)  # Use system diff
    return 0 if manager._compare_bindings_results(actual_result_info, name) else 1


def _handle_comparison_result(
    comparison_rc: int,
    test_result_summary: Dict[str, Any],
    name: str,
    global_debug_level: int,
    use_rasqal_compare: bool,
):
    """Handle the comparison result and update test summary."""
    if comparison_rc == 0:
        logger.info(f"Test '{name}': OK (results match)")
        test_result_summary["result"] = "passed"
        from .debug_manager import show_success_debug_info

        show_success_debug_info(name, {}, global_debug_level)
    else:
        logger.warning(f"Test '{name}': FAILED (results do not match)")
        test_result_summary["result"] = "failed"
        from .debug_manager import show_failure_debug_info, show_failure_summary

        show_failure_debug_info(name, {}, {}, global_debug_level)
        show_failure_summary(name, 0)


def compare_rdf_graphs(
    file1_path: Path, file2_path: Path, diff_output_path: Path
) -> int:
    """
    Compare two RDF graphs by simple external diff on normalized N-Triples.
    (ntc/jena removed).
    """
    try:
        result = run_command(
            cmd=[DIFF_CMD, "-u", str(file1_path), str(file2_path)],
            cwd=str(CURDIR),
            error_msg="Error running diff",
        )
        returncode, stdout, stderr = result
        diff_output_path.write_text(stdout + (stderr or ""))
        return returncode
    except Exception as e:
        logger.error(f"Error running external diff: {e}")
        diff_output_path.write_text(f"Error running external diff: {e}\n")
        return 2


def get_rasqal_version() -> str:
    """Retrieves the roqet (Rasqal) version string."""
    try:
        result = run_command(
            cmd=[ROQET, "-v"], cwd=str(CURDIR), error_msg="Could not get roqet version"
        )
        returncode, stdout, stderr = result
        return stdout.strip()
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


def run_single_test(
    config: TestConfig, global_debug_level: int, use_rasqal_compare: bool = False
) -> Dict[str, Any]:
    """
    Runs a single SPARQL test using roqet and compares results.
    Returns a dictionary summarizing the test outcome.
    """
    logger.debug(f"run_single_test called with debug_level={global_debug_level}")

    test_result_summary: Dict[str, Any] = {
        "name": config.name,
        "uri": config.test_uri,
        "result": "failed",
        "is_success": False,
        "stdout": "",
        "stderr": "",
        "query": "",
        "elapsed-time": 0,
        "roqet-status-code": -1,
        "diff": "",
    }

    if global_debug_level > 0:
        _log_test_config_details(config)

    try:
        # Execute query using QueryExecutor (replaces all old monolithic functions)
        from .query_executor import QueryExecutor

        query_executor = QueryExecutor()
        query_result = query_executor.execute_query(config)

        # Set basic result information
        test_result_summary["stdout"] = query_result.content
        test_result_summary["stderr"] = ""
        test_result_summary["roqet-status-code"] = 0
        test_result_summary["elapsed-time"] = 0.0
        test_result_summary["query"] = f"roqet query execution for {config.name}"

        # Handle different test types
        if config.test_type == TestType.QUERY_EVALUATION_TEST.value:
            # Use the component-based result comparison
            from .result_comparer import ResultComparer

            comparer = ResultComparer(use_rasqal_compare)

            # Read expected result if available
            expected_content = ""
            if config.result_file and config.result_file.exists():
                expected_content = config.result_file.read_text()

            # Handle case where roqet doesn't include variables that should be unbound
            from .xml_processor import normalize_expected_variables

            expected_content = normalize_expected_variables(
                expected_content, query_result.content
            )

            # Compare results
            comparison_result = comparer.compare_results(
                actual=query_result.content,
                expected=expected_content,
                result_type=query_result.result_type,
                config=config,
            )

            if comparison_result.is_match:
                test_result_summary["result"] = "passed"
                test_result_summary["is_success"] = True
            else:
                test_result_summary["result"] = "failed"
                test_result_summary["is_success"] = False
                test_result_summary["diff"] = comparison_result.diff_output or ""
        elif config.test_type == TestType.WARNING_TEST.value:
            # For warning tests, check if warnings were generated
            # In the new architecture, warnings are handled by QueryExecutor
            warnings_generated = query_result.metadata.get("warnings_generated", False)
            if warnings_generated:
                # Warnings were generated - test passes
                test_result_summary["result"] = "passed"
                test_result_summary["is_success"] = True
                test_result_summary["stderr"] = (
                    "Warning test succeeded (warnings generated)"
                )
            else:
                # No warnings generated - test fails
                test_result_summary["result"] = "failed"
                test_result_summary["is_success"] = False
                test_result_summary["stderr"] = (
                    "Warning test got exit code 0, expected warnings"
                )
        else:
            # For other non-evaluation tests (syntax), success is based on query execution
            test_result_summary["result"] = "passed"
            test_result_summary["is_success"] = True

        return test_result_summary

    except Exception as e:
        import traceback

        logger.error(f"Unexpected error in run_single_test for '{config.name}': {e}")
        traceback.print_exc()
        test_result_summary["result"] = "failed"
        test_result_summary["stderr"] = str(e)
        finalize_test_result(test_result_summary, config.expect)
        return test_result_summary


def _log_test_config_details(config: TestConfig):
    """Log detailed test configuration information."""
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


def _is_warning_test_failure(
    config: TestConfig, roqet_execution_data: Dict[str, Any]
) -> bool:
    """Check if this is a warning test that should have failed."""
    # Warning tests should get exit code 2 when warnings are generated
    # Exit code 0 means no warnings (which is a failure for warning tests)
    return (
        config.test_type == TestType.WARNING_TEST.value
        and roqet_execution_data["returncode"] == 0
    )


def _is_roqet_failure(config: TestConfig, roqet_execution_data: Dict[str, Any]) -> bool:
    """Check if roqet execution failed."""
    # Exit code 2 (warnings only) is considered success for all tests
    # Exit code 0 (success) is also considered success
    # Any other exit code indicates actual failure
    return roqet_execution_data["returncode"] not in [0, 2]


def _is_unexpected_success(
    config: TestConfig, roqet_execution_data: Dict[str, Any]
) -> bool:
    """Check if test was expected to fail but succeeded."""
    return (
        config.expect == TestResult.FAILED or config.expect == TestResult.XFAILED
    ) and roqet_execution_data["returncode"] == 0


def _is_syntax_test(config: TestConfig) -> bool:
    """Check if this is a syntax test that only needs parsing validation."""
    # Check for common syntax test type identifiers
    syntax_test_types = [
        "http://jena.hpl.hp.com/2005/05/test-manifest-extra#TestSyntax",
        "http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#TestSyntax",
        "TestSyntax",
        "syntax",
    ]

    # Check test type
    if config.test_type in syntax_test_types:
        return True

    # Check if test name contains syntax indicators
    if "syntax" in config.name.lower():
        return True

    # Check if there's no result file (syntax tests typically don't have expected results)
    if not config.result_file:
        return True

    return False


def _compare_test_results(
    test_result_summary: Dict[str, Any],
    config: TestConfig,
    global_debug_level: int,
    use_rasqal_compare: bool,
) -> Dict[str, Any]:
    """Compare actual vs expected test results."""
    # Build actual results from roqet output
    actual_result_info = _build_actual_from_srx(
        config,
        expected_vars_order=[],
        sort_output=True,
    )

    if not actual_result_info:
        logger.warning(f"Test '{config.name}': FAILED (could not build actual results)")
        test_result_summary["result"] = "failed"
        finalize_test_result(test_result_summary, config.expect)
        return test_result_summary

    # Compare actual vs expected results
    _compare_actual_vs_expected(
        test_result_summary,
        actual_result_info,
        config.result_file,
        config.cardinality_mode,
        global_debug_level,
        use_rasqal_compare,
    )

    finalize_test_result(test_result_summary, config.expect)
    return test_result_summary


class SparqlTestRunner:
    """Main SPARQL test runner for executing W3C SPARQL test suites."""

    def __init__(self):
        self.args = None
        self.global_debug_level = 0

        # Component classes for modular functionality
        self.query_executor = None
        self.result_processor = None
        self.result_comparer = None
        self.debug_manager = None

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
            "-d",
            "--debug",
            action="count",
            default=0,
            help="Debug level (use multiple -d for more detail: -d=normal, -dd=verbose)",
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
            "--preserve",
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
        parser.add_argument(
            "--use-rasqal-compare",
            action="store_true",
            help="Use rasqal-compare utility for result comparison (experimental)",
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
        if args.debug >= 1:
            logging.getLogger().setLevel(logging.DEBUG)
            logging.basicConfig(
                level=logging.DEBUG, format="%(levelname)s: %(message)s"
            )
        else:
            logging.getLogger().setLevel(logging.INFO)

        # Initialize component classes
        self.setup_components()

        # Set global file preservation flag using shared system
        set_preserve_files(args.preserve)

    def setup_components(self):
        """Initialize all component classes."""
        from ..runners.query_executor import QueryExecutor
        from ..runners.result_processor import ResultProcessor
        from ..runners.result_comparer import ResultComparer
        from ..runners.debug_output_manager import DebugOutputManager

        self.query_executor = QueryExecutor()
        self.result_processor = ResultProcessor()
        self.result_comparer = ResultComparer(self.args.use_rasqal_compare)
        self.debug_manager = DebugOutputManager(self.global_debug_level)

    def run_single_test_with_components(self, config: TestConfig) -> Dict[str, Any]:
        """Run a single test using the new component-based architecture.

        This method provides an alternative execution path using the new
        QueryExecutor, ResultProcessor, and ResultComparer classes.
        It maintains the same interface as run_single_test for compatibility.
        """
        logger.debug(f"run_single_test_with_components called for test: {config.name}")

        test_result_summary: Dict[str, Any] = {
            "name": config.name,
            "uri": config.test_uri,
            "result": "failed",
            "is_success": False,
            "stdout": "",
            "stderr": "",
            "query": "",
            "elapsed-time": 0,
            "roqet-status-code": -1,
            "diff": "",
        }

        # Log test configuration details if debug level is high enough
        if self.global_debug_level > 0:
            logger.debug(
                f"Test config: {config.name}, type: {config.test_type}, files: {config.test_file}"
            )

        try:
            # 1. Execute query using QueryExecutor
            query_result = self.query_executor.execute_query(config)

            # Debug: log query result details
            logger.debug(f"Query result type: {query_result.result_type}")
            logger.debug(f"Query result format: {query_result.format}")
            logger.debug(f"Query result count: {query_result.count}")
            logger.debug(f"Query result content length: {len(query_result.content)}")
            if query_result.content:
                logger.debug(
                    f"Query result content (first 500 chars): {repr(query_result.content[:500])}"
                )
            else:
                logger.debug("Query result content is EMPTY!")

            test_result_summary["stdout"] = query_result.content
            test_result_summary["stderr"] = (
                ""  # QueryExecutor doesn't capture stderr separately
            )
            test_result_summary["roqet-status-code"] = 0  # Assume success for now
            test_result_summary["elapsed-time"] = 0.0
            test_result_summary["query"] = f"roqet query execution for {config.name}"

            # 2. Handle different test types
            if config.test_type in (
                TestType.QUERY_EVALUATION_TEST.value,
                "http://ns.librdf.org/2009/test-manifest#XFailTest",
            ):
                # Use the component-based result comparison for both regular and XFail tests
                from .result_comparer import ResultComparer

                comparer = ResultComparer(self.args.use_rasqal_compare)

                # Read expected result if available
                expected_content = ""
                if config.result_file and config.result_file.exists():
                    expected_content = config.result_file.read_text()
                    logger.debug(f"Expected result file: {config.result_file}")
                    logger.debug(f"Expected content: {expected_content}")
                else:
                    logger.debug(f"No expected result file found for {config.name}")

                logger.debug(f"Actual result content: {query_result.content}")
                logger.debug(f"Query result format: {query_result.format}")
                logger.debug(f"Query result type: {query_result.result_type}")

                # Handle case where roqet doesn't include variables that should be unbound
                # This can happen with certain SPARQL constructs where variables are mentioned
                # but not actually bound by triple patterns
                from .xml_processor import normalize_expected_variables

                expected_content = normalize_expected_variables(
                    expected_content, query_result.content
                )

                # Compare results
                comparison_result = comparer.compare_results(
                    actual=query_result.content,
                    expected=expected_content,
                    result_type=query_result.result_type,
                    config=config,
                )

                # Debug: print comparison result
                logger.debug(
                    f"Comparison result for {config.name}: is_match={comparison_result.is_match}"
                )

                # For XFail tests, we expect them to fail, so mark as XFAILED (success)
                if config.expect == TestResult.XFAILED:
                    if comparison_result.is_match:
                        logger.debug(f"XFail test {config.name} unexpectedly passed!")
                        final_result = TestResult.UXPASSED
                        detail_message = (
                            "Test passed (XFailTest - expected to fail but passed)"
                        )
                    else:
                        logger.debug(f"XFail test {config.name} failed as expected")
                        final_result = TestResult.XFAILED
                        detail_message = "Test failed as expected"
                else:
                    # Regular test - use normal comparison logic
                    actual_status = (
                        TestResult.PASSED
                        if comparison_result.is_match
                        else TestResult.FAILED
                    )
                    final_result, detail_message = (
                        TestTypeResolver.determine_test_result(
                            config.expect, actual_status
                        )
                    )

                # Set test result based on final outcome
                if final_result in (
                    TestResult.PASSED,
                    TestResult.XFAILED,
                    TestResult.UXPASSED,
                ):
                    test_result_summary["result"] = (
                        final_result.value
                    )  # Keep the specific result type
                    test_result_summary["is_success"] = True
                else:
                    test_result_summary["result"] = "failed"
                    test_result_summary["is_success"] = False

                test_result_summary["detail_message"] = detail_message
                if (
                    hasattr(comparison_result, "is_match")
                    and not comparison_result.is_match
                ):
                    test_result_summary["diff"] = comparison_result.diff_output

            elif config.test_type == TestType.WARNING_TEST.value:
                # For warning tests, check if warnings were generated
                # In the new architecture, warnings are handled by QueryExecutor
                warnings_generated = query_result.metadata.get(
                    "warnings_generated", False
                )
                if warnings_generated:
                    # Warnings were generated - test passes
                    test_result_summary["result"] = "passed"
                    test_result_summary["is_success"] = True
                    test_result_summary["stderr"] = (
                        "Warning test succeeded (warnings generated)"
                    )
                else:
                    # No warnings generated - test fails
                    test_result_summary["result"] = "failed"
                    test_result_summary["is_success"] = False
                    test_result_summary["stderr"] = (
                        "Warning test got exit code 0, expected warnings"
                    )
            else:
                # For other non-evaluation tests (syntax), success is based on query execution
                test_result_summary["result"] = "passed"
                test_result_summary["is_success"] = True

            return test_result_summary

        except Exception as e:
            logger.error(f"Error in run_single_test_with_components: {e}")
            test_result_summary["stderr"] = str(e)
            test_result_summary["roqet-status-code"] = -1
        return test_result_summary

    def run_test_suite_with_components(
        self, manifest_file: Path, srcdir: Path
    ) -> List[Dict[str, Any]]:
        """Run a test suite using the new component-based architecture.

        This method provides an alternative to run_test_suite using the new
        component classes while maintaining the same interface.
        """
        logger.info(f"Running test suite with components: {manifest_file}")

        # Parse manifest to get test configurations
        manifest_parser = ManifestParser(manifest_file)

        # Apply test case filter if specified
        unique_test_filter = getattr(self.args, "test_case", None)
        test_configs = manifest_parser.get_tests(srcdir, unique_test_filter)

        results = []
        for config in test_configs:
            logger.info(f"Running test: {config.name}")
            result = self.run_single_test_with_components(config)
            results.append(result)

            # Log result with proper result type
            result_type = result.get("result", "unknown")
            if result_type == "xfailed":
                logger.info(f"✓ XFAILED: {config.name}")
            elif result_type == "uxpassed":
                logger.info(f"⚠ UXPASSED: {config.name}")
            elif result["is_success"]:
                logger.info(f"✓ PASSED: {config.name}")
            else:
                logger.info(f"✗ FAILED: {config.name}")

        return results

    def main(self):
        """Main method to run the SPARQL test runner."""
        parser = self.setup_argument_parser()
        args = parser.parse_args()

        # Handle the case where no manifest file is provided
        if not args.manifest_file:
            parser.error("The following arguments are required: --manifest-file")

        self.process_arguments(args)

        # Run the test suite
        try:
            results = self.run_test_suite_with_components(
                args.manifest_file, args.srcdir
            )

            # Print summary with proper result type counting
            passed = sum(1 for r in results if r.get("result") == "passed")
            xfailed = sum(1 for r in results if r.get("result") == "xfailed")
            uxpassed = sum(1 for r in results if r.get("result") == "uxpassed")
            failed = sum(1 for r in results if r.get("result") == "failed")

            print(f"\nTest Results Summary:")
            print(f"  Total: {len(results)}")
            print(f"  Passed: {passed}")
            print(f"  XFailed: {xfailed}")
            if uxpassed > 0:
                print(f"  Uxpassed: {uxpassed}")
            print(f"  Failed: {failed}")

            if failed > 0:
                print(f"\nFailed tests:")
                for result in results:
                    if result.get("result") == "failed":
                        print(f"  {result['name']}")

            # Clean up temporary files
            cleanup_temp_files()

            # Return 0 (success) if no tests actually failed
            # XFAILED and UXPASSED are considered successful outcomes
            return 0 if failed == 0 else 1

        except Exception as e:
            logger.error(f"Error running tests: {e}")
            import traceback

            traceback.print_exc()
            return 1


def main():
    """Main entry point for SPARQL testing."""
    runner = SparqlTestRunner()
    return runner.main()


if __name__ == "__main__":
    import sys

    sys.exit(main())

"""
Utilities and helper functions for SPARQL Test Framework

This module contains path resolution, logging setup, command execution,
error handling, and other utility functions.

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
import sys
import subprocess
import logging
import shutil
import argparse
import re
from pathlib import Path
from typing import List, Dict, Optional, Tuple, Callable, Any

from .config import TestConfig


# --- Custom Exceptions ---


class SparqlTestError(Exception):
    """Base exception for SPARQL test runner errors."""

    pass


class ManifestParsingError(SparqlTestError):
    """Raised when there's an issue parsing the test manifest."""

    pass


# --- Logging Setup ---


def setup_logging(
    debug: bool = False, level: Optional[int] = None, stream=sys.stderr
) -> logging.Logger:
    """
    Setup logging configuration and return logger.

    Args:
        debug: If True, sets log level to DEBUG
        level: Explicit log level (overrides debug parameter)
        stream: Output stream for logging (defaults to stderr)

    Returns:
        Configured logger instance
    """
    if level is None:
        level = logging.DEBUG if debug else logging.INFO

    logging.basicConfig(level=level, format="%(levelname)s: %(message)s", stream=stream)
    return logging.getLogger(__name__)


# --- Command Line Argument Helpers ---


def add_common_arguments(parser: argparse.ArgumentParser) -> None:
    """
    Add common command-line arguments used across multiple test scripts.

    Args:
        parser: ArgumentParser instance to add arguments to
    """
    parser.add_argument(
        "-d", "--debug", action="store_true", help="Enable debug logging"
    )
    parser.add_argument(
        "--manifest-file", type=Path, help="Path to a manifest file (e.g., manifest.n3)"
    )
    parser.add_argument(
        "--srcdir",
        type=Path,
        help="The source directory (equivalent to $(srcdir) in Makefile.am)",
    )
    parser.add_argument(
        "--builddir",
        type=Path,
        help="The build directory (equivalent to $(top_builddir) in Makefile.am)",
    )


# --- Tool Discovery ---


def find_tool(name: str, builddir: Optional[Path] = None) -> Optional[str]:
    """
    Finds a required tool (like 'roqet' or 'to-ntriples') by searching
    the environment, a relative 'utils' directory, the build directory, and the system PATH.

    Args:
        name: Name of the tool to find
        builddir: Optional build directory to search for the tool

    Returns:
        Path to the tool if found, None otherwise
    """
    logger = logging.getLogger(__name__)

    # 1. Check environment variable
    env_var = name.upper().replace("-", "_")
    env_val = os.environ.get(env_var)
    if env_val and Path(env_val).is_file():
        logger.debug(f"Found {name} via environment variable {env_var}: {env_val}")
        return env_val

    # 2. Search in build directory if provided
    if builddir:
        tool_path = builddir / "utils" / name
        if tool_path.is_file() and os.access(tool_path, os.X_OK):
            logger.debug(f"Found {name} in build directory: {tool_path}")
            return str(tool_path)

    # 3. Search in current directory and common test directories
    current_dir = Path.cwd()
    for search_dir in [current_dir] + list(current_dir.parents):
        # Look in current directory
        tool_path = search_dir / name
        if tool_path.is_file() and os.access(tool_path, os.X_OK):
            logger.debug(f"Found {name} in current directory: {tool_path}")
            return str(tool_path)

        # Look in utils subdirectory
        tool_path = search_dir / "utils" / name
        if tool_path.is_file() and os.access(tool_path, os.X_OK):
            logger.debug(f"Found {name} in utils directory: {tool_path}")
            return str(tool_path)

        # Look in tests/algebra subdirectory (for convert_graph_pattern)
        tool_path = search_dir / "tests" / "algebra" / name
        if tool_path.is_file() and os.access(tool_path, os.X_OK):
            logger.debug(f"Found {name} in tests/algebra directory: {tool_path}")
            return str(tool_path)

        # Look for build directory structure in current directory tree
        # This handles make distcheck where we're in a subdirectory of the build tree
        for level in range(5):  # Search up to 5 levels up
            if level == 0:
                build_search_dir = search_dir
            else:
                if len(search_dir.parents) < level:
                    break
                build_search_dir = search_dir.parents[level - 1]

            tool_path = build_search_dir / "utils" / name
            if tool_path.is_file() and os.access(tool_path, os.X_OK):
                logger.debug(f"Found {name} in build tree utils: {tool_path}")
                return str(tool_path)

            tool_path = build_search_dir / "tests" / "algebra" / name
            if tool_path.is_file() and os.access(tool_path, os.X_OK):
                logger.debug(f"Found {name} in build tree tests/algebra: {tool_path}")
                return str(tool_path)

    # 5. Fallback to system PATH
    tool_path_in_path = shutil.which(name)
    if tool_path_in_path:
        logger.debug(f"Found {name} in system PATH: {tool_path_in_path}")
        return tool_path_in_path

    logger.warning(f"Tool '{name}' could not be found.")
    return None


# --- Command Execution ---


def run_command(
    cmd: List[str], cwd: Path, error_prefix: str
) -> subprocess.CompletedProcess:
    """
    Runs a shell command and returns its CompletedProcess object, handling common errors.

    Args:
        cmd: Command list to execute
        cwd: Working directory for command execution
        error_prefix: Error message prefix for exceptions

    Returns:
        CompletedProcess object

    Raises:
        RuntimeError: If command fails or is not found
    """
    logger = logging.getLogger(__name__)

    try:
        logger.debug(f"Running command in {cwd}: {' '.join(cmd)}")
        process = subprocess.run(
            cmd, capture_output=True, text=True, cwd=cwd, check=False, encoding="utf-8"
        )
        return process
    except FileNotFoundError:
        raise RuntimeError(
            f"{error_prefix}. Command not found: '{cmd[0]}'. Please ensure it is installed and in your PATH."
        )
    except Exception as e:
        logger.error(
            f"An unexpected error occurred while running command '{cmd[0]}': {e}"
        )
        raise RuntimeError(
            f"{error_prefix}. Unexpected error running '{cmd[0]}'."
        ) from e


# --- Format Testing Utilities ---


def run_roqet_with_format(
    query_file: str,
    data_file: Optional[str],
    result_format: str,
    roqet_path: Optional[str] = None,
    timeout: int = 30,
    additional_args: Optional[List[str]] = None,
) -> Tuple[str, str, int]:
    """
    Run roqet with specified output format.

    Args:
        query_file: Path to SPARQL query file
        data_file: Path to data file (optional)
        result_format: Output format (csv, tsv, srj, xml, etc.)
        roqet_path: Path to roqet executable (auto-detected if None)
        timeout: Command timeout in seconds
        additional_args: Additional command line arguments

    Returns:
        Tuple of (stdout, stderr, returncode)
    """
    if roqet_path is None:
        roqet_path = find_tool("roqet") or "roqet"

    cmd = [roqet_path, "-r", result_format, "-W", "0", "-q", query_file]
    if data_file:
        cmd.extend(["-D", data_file])
    if additional_args:
        cmd.extend(additional_args)

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        return result.stdout, result.stderr, result.returncode
    except subprocess.TimeoutExpired:
        return "", f"Command timed out after {timeout} seconds", 124
    except Exception as e:
        return "", f"Error running roqet: {e}", 1


def filter_format_output(output: str, result_format: str) -> Optional[str]:
    """
    Filter format-specific output from roqet output.

    Args:
        output: Raw roqet output
        result_format: Expected format (json/srj will look for JSON, others return as-is)

    Returns:
        Filtered output or None if format not found
    """
    if result_format.lower() in ["json", "srj"]:
        # Filter out debug output - look for JSON start
        json_start = output.find("{")
        if json_start >= 0:
            return output[json_start:]
        return None
    else:
        # For CSV, TSV, XML etc., return output as-is
        return output.strip() if output.strip() else None


def run_manifest_test(
    test_id: str,
    srcdir: Path,
    test_handler_map: Dict[str, Callable[[TestConfig, Path], int]],
    manifest_filename: str,
) -> int:
    """
    Generic manifest-driven test execution.

    Args:
        test_id: Test case identifier from manifest
        srcdir: Source directory containing manifest and test files
        test_handler_map: Map of test_id to handler function
        manifest_filename: Name of manifest file

    Returns:
        0 for success, 1 for failure
    """
    from .manifest import ManifestParser

    try:
        # Parse manifest to get test details
        manifest_file = srcdir / manifest_filename
        if not manifest_file.exists():
            print(f"Manifest file not found: {manifest_file}")
            return 1

        parser = ManifestParser(manifest_file)
        tests = parser.get_tests(srcdir)

        # Find the test case
        test_config = None
        for test in tests:
            if test.name == test_id:
                test_config = test
                break

        if test_config is None:
            print(f"Test case '{test_id}' not found in manifest")
            return 1

        # Change to source directory for relative paths
        original_cwd = os.getcwd()
        try:
            os.chdir(srcdir)

            # Look for handler function
            if test_id in test_handler_map:
                return test_handler_map[test_id](test_config, srcdir)
            else:
                print(f"No handler found for test case: {test_id}")
                return 1

        finally:
            os.chdir(original_cwd)

    except Exception as e:
        print(f"Error running test case {test_id}: {e}")
        return 1


# --- String Processing Utilities ---


def decode_literal(lit_str: str) -> str:
    """
    Decodes an N-Triples literal string, handling common escapes.
    Expected to receive only valid literal strings (quoted, with optional suffix).

    Args:
        lit_str: N-Triples literal string to decode

    Returns:
        Decoded string value
    """
    logger = logging.getLogger(__name__)

    if not lit_str or not lit_str.startswith('"'):
        # If it's not a quoted string, it's either a URI or Blank Node already handled.
        # Or it's a malformed literal. Return as is.
        return lit_str

    # Attempt to extract the value part from within the quotes and unescape it.
    value_part_match = re.match(
        r'^"(?P<value>(?:[^"\\]|\\.)*)"(?:@[\w-]+|\^\^<[^>]+>)?$', lit_str
    )
    if not value_part_match:
        logger.warning(f"Malformed literal string passed to decode_literal: {lit_str}")
        return lit_str

    val = value_part_match.group("value")

    # Unescape common N-Triples sequences:
    # Order matters: replace longer sequences first to avoid partial replacements.
    val = val.replace('\\"', '"')
    val = val.replace("\\n", "\n")
    val = val.replace("\\r", "\r")
    val = val.replace("\\t", "\t")
    val = val.replace("\\\\", "\\")  # Unescape escaped backslashes last
    return val


def escape_turtle_literal(s: str) -> str:
    """
    Escapes a string for use as a triple-quoted Turtle literal.
    Handles backslashes, triple double quotes, and control characters (\n, \r, \t).

    Args:
        s: String to escape

    Returns:
        Escaped string suitable for Turtle triple-quoted literals
    """
    # Escape backslashes first
    s = s.replace("\\", "\\\\")
    # Handle triple quotes by replacing with escaped double quotes
    s = s.replace('"""', '\\"\\"\\"')
    if s.endswith('"'):  # avoid """{escaped ending with "}"""
        s = s[:-1] + '\\"'
    # Escape newlines, carriage returns, and tabs
    s = s.replace("\n", "\\n")
    s = s.replace("\r", "\\r")
    s = s.replace("\t", "\\t")
    return s


# --- Path Resolution ---


class PathResolver:
    """Helper class for resolving URIs to file paths."""

    def __init__(self, srcdir: Path, extractor):
        """
        Initialize path resolver.

        Args:
            srcdir: Source directory for resolving relative paths
            extractor: ManifestTripleExtractor instance for unquoting URIs
        """
        self.srcdir = srcdir
        self.extractor = extractor

    def resolve_path(self, uri_val: Optional[str]) -> Optional[Path]:
        """
        Resolve a URI to a file path.

        Args:
            uri_val: URI string to resolve

        Returns:
            Resolved Path object, or None if uri_val is None/empty
        """
        if not uri_val:
            return None
        ps = self.extractor.unquote(uri_val)
        if ps.startswith("file:///"):
            p = Path(ps[len("file://") :])
        elif ps.startswith("file:/"):
            p = Path(ps[len("file:") :])
        else:
            p = Path(ps)
        return p.resolve() if p.is_absolute() else (self.srcdir / p).resolve()

    def resolve_paths(self, uri_vals: List[str]) -> List[Path]:
        """
        Resolve multiple URIs to file paths, filtering out None values.

        Args:
            uri_vals: List of URI strings to resolve

        Returns:
            List of resolved Path objects
        """
        return [
            p
            for p in [self.resolve_path(uri_val) for uri_val in uri_vals]
            if p is not None
        ]


# --- Manifest Triple Extraction ---


class ManifestTripleExtractor:
    """Helper class for extracting values from RDF triples."""

    def __init__(self, triples_by_subject: Dict[str, List[Dict[str, Any]]]):
        """
        Initialize extractor with triple data.

        Args:
            triples_by_subject: Dictionary mapping subject URIs to lists of triple dictionaries
        """
        self.triples_by_subject = triples_by_subject

    def get_obj_val(self, subj: str, pred: str) -> Optional[str]:
        """
        Get a single object value for a subject-predicate pair.

        Args:
            subj: Subject URI
            pred: Predicate URI

        Returns:
            Object value string, or None if not found
        """
        return next(
            (
                t["o_full"]
                for t in self.triples_by_subject.get(subj, [])
                if t["p"] == pred
            ),
            None,
        )

    def get_obj_vals(self, subj: str, pred: str) -> List[str]:
        """
        Get all object values for a subject-predicate pair.

        Args:
            subj: Subject URI
            pred: Predicate URI

        Returns:
            List of object value strings
        """
        return [
            t["o_full"] for t in self.triples_by_subject.get(subj, []) if t["p"] == pred
        ]

    def unquote(self, value: Optional[str]) -> Optional[str]:
        """
        Unquote a URI or literal value.

        Args:
            value: Value to unquote

        Returns:
            Unquoted value, or None if input was None
        """
        if not value:
            return value

        # Handle URI references
        if value.startswith("<") and value.endswith(">"):
            return value[1:-1]

        # Handle literals
        if value.startswith('"'):
            return decode_literal(value)

        return value

"""
Core utility classes for SPARQL test framework.

This module contains the core utility classes that were previously
defined in the top-level utils.py file.

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

import logging
import os
import re
import subprocess
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple, Callable

# Import TestConfig for type hints
from ..config import TestConfig


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


def generate_custom_diff(
    expected_content: str,
    actual_content: str,
    expected_file: str = "expected",
    actual_file: str = "actual",
) -> str:
    """
    Generate a custom diff-like output without using the system diff command.

    This function creates a diff-like output similar to what roqet -d debug produces,
    showing differences between expected and actual content in a structured format.

    Args:
        expected_content: The expected content as a string
        actual_content: The actual content as a string
        expected_file: Name of the expected file (for display purposes)
        actual_file: Name of the actual file (for display purposes)

    Returns:
        A string containing the diff-like output
    """
    if expected_content == actual_content:
        return "Files are identical"

    expected_lines = expected_content.splitlines()
    actual_lines = actual_content.splitlines()

    diff_output = []
    diff_output.append(f"--- {expected_file}")
    diff_output.append(f"+++ {actual_file}")
    diff_output.append("")

    # Enhanced comparison with context awareness
    max_lines = max(len(expected_lines), len(actual_lines))

    for i in range(max_lines):
        expected_line = expected_lines[i] if i < len(expected_lines) else None
        actual_line = actual_lines[i] if i < len(actual_lines) else None

        if expected_line == actual_line:
            diff_output.append(f" {expected_line}")
        else:
            # Show both lines when they differ
            if expected_line is not None:
                diff_output.append(f"-{expected_line}")
            if actual_line is not None:
                diff_output.append(f"+{actual_line}")

    # Add summary information
    diff_output.append("")
    diff_output.append(
        f"Summary: {len(expected_lines)} expected lines, {len(actual_lines)} actual lines"
    )

    return "\n".join(diff_output)


def compare_files_custom_diff(
    file1_path: Path, file2_path: Path, diff_output_path: Path
) -> int:
    """
    Compare two files using custom diff generation instead of system diff command.

    Args:
        file1_path: Path to first file (expected)
        file2_path: Path to second file (actual)
        diff_output_path: Path to write diff output to

    Returns:
        0 if files are identical, 1 if different, 2 on error
    """
    try:
        # Read file contents
        content1 = file1_path.read_text(encoding="utf-8", errors="replace")
        content2 = file2_path.read_text(encoding="utf-8", errors="replace")

        # Generate custom diff
        diff_content = generate_custom_diff(
            content1, content2, str(file1_path), str(file2_path)
        )

        # Write diff output
        diff_output_path.write_text(diff_content)

        # Return 0 if identical, 1 if different
        return 0 if content1 == content2 else 1

    except Exception as e:
        error_msg = f"Error comparing files: {e}\n"
        diff_output_path.write_text(error_msg)
        return 2


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

    # Build options first, then positional query file last
    cmd = [roqet_path, "-r", result_format, "-W", "0"]
    if additional_args:
        cmd.extend(additional_args)
    if data_file:
        cmd.extend(["-D", data_file])
    cmd.extend(["-q", query_file])

    try:
        # Set RASQAL_DEBUG_LEVEL=0 to suppress debug output during
        # testing.  This eliminates the need for fragile string-based
        # filtering of debug messages.
        env = os.environ.copy()
        env['RASQAL_DEBUG_LEVEL'] = '0'
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, env=env)
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
    fmt = result_format.lower()
    if fmt in ["json", "srj"]:
        # Filter out debug output - look for JSON start
        json_start = output.find("{")
        if json_start >= 0:
            return output[json_start:]
        return None
    elif fmt in ["xml", "srx", "rdfxml"]:
        # Extract XML payload by locating the XML root
        # Prefer the SPARQL Results root if present
        for marker in ["<sparql", "<?xml", "<rdf", "<RDF"]:
            idx = output.find(marker)
            if idx >= 0:
                xml_payload = output[idx:]
                return xml_payload.strip() if xml_payload.strip() else None
        # Fallback: strip and return if it looks like XML
        stripped = output.lstrip()
        if stripped.startswith("<"):
            return stripped
        return None
    else:
        # For CSV, TSV etc., return output as-is
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
    from ..manifest import ManifestParser

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


# --- Shared Temporary File Management ---
# Global flag for file preservation (shared across all runners)
_preserve_debug_files = False

# Global to track temporary file paths when using secure temp files
_temp_file_paths: List[Path] = []
_temp_file_cache: Dict[str, Path] = {}


def set_preserve_files(preserve: bool) -> None:
    """Set the global preserve files flag.

    Args:
        preserve: Whether to preserve temporary files instead of using /tmp
    """
    global _preserve_debug_files
    _preserve_debug_files = preserve


def get_temp_file_path(filename: str, prefix: str = "rasqal_test") -> Path:
    """Get a temporary file path for any type of output file.

    When --preserve is not given, creates secure temporary files in /tmp.
    When --preserve is given, uses local files in current working directory.

    Multiple calls with the same filename return the same path to ensure consistency.

    Args:
        filename: Base filename (e.g., "roqet.out", "plan.ttl")
        prefix: Prefix for temp file naming (default: "rasqal_test")

    Returns:
        Path to the file (temp or local)
    """
    global _temp_file_paths, _temp_file_cache

    if _preserve_debug_files:
        # Use local files for debugging
        return Path.cwd() / filename
    else:
        # Use secure temporary files for parallel execution
        # Cache paths by filename to ensure consistency
        if filename not in _temp_file_cache:
            import tempfile
            import os

            # Use mkstemp for better security - it creates the file atomically
            # and returns a file descriptor that we can use to set permissions
            fd, temp_path_str = tempfile.mkstemp(
                prefix=f"{prefix}_{filename}_",
                suffix="",
                dir="/tmp",
            )

            temp_path = Path(temp_path_str)

            # Set restrictive permissions (owner read/write only)
            # This prevents other users from reading our temp files
            os.chmod(temp_path, 0o600)

            # Close the file descriptor since we'll reopen it as needed
            os.close(fd)

            _temp_file_paths.append(temp_path)
            _temp_file_cache[filename] = temp_path

        return _temp_file_cache[filename]


def cleanup_temp_files() -> None:
    """Clean up temporary files if not preserving them."""
    global _temp_file_paths, _temp_file_cache

    if not _preserve_debug_files:
        # Clean up only the files we created (tracked in _temp_file_paths)
        # This is much safer than globbing and deleting arbitrary files
        for temp_file in _temp_file_paths:
            if temp_file.exists():
                try:
                    temp_file.unlink()
                except OSError:
                    pass  # Ignore cleanup errors
        _temp_file_paths.clear()
        _temp_file_cache.clear()
    else:
        # When preserving files, we don't clean them up
        # This allows users to inspect the files for debugging
        pass

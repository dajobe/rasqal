"""
ResultComparer class for handling result comparison logic.

This module provides the ResultComparer class that handles result comparison
for different result types including graphs, bindings, boolean values, and SRJ.

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
import subprocess
from pathlib import Path
from typing import Optional, Dict, Any

from ..config import TestConfig
from ..utils import run_command, find_tool


class ComparisonResult:
    """Represents the result of a comparison operation."""

    def __init__(
        self,
        is_match: bool,
        diff_output: Optional[str] = None,
        error_message: Optional[str] = None,
        comparison_method: str = "unknown",
    ):
        self.is_match = is_match
        self.diff_output = diff_output
        self.error_message = error_message
        self.comparison_method = comparison_method


class ResultComparer:
    """Handles result comparison logic for different result types."""

    def __init__(self, use_rasqal_compare: bool = False):
        self.use_rasqal_compare = use_rasqal_compare
        self.logger = logging.getLogger(__name__)

        # Initialize comparison tools
        self.rasqal_compare_path = None
        if self.use_rasqal_compare:
            self.rasqal_compare_path = self._ensure_rasqal_compare_available()

    def _ensure_rasqal_compare_available(self) -> str:
        """Ensure rasqal-compare tool is available."""
        if self.rasqal_compare_path:
            return self.rasqal_compare_path

        path = find_tool("rasqal-compare")
        if not path:
            raise RuntimeError("rasqal-compare tool not found but required")

        self.rasqal_compare_path = path
        return path

    def compare_results(
        self, actual: str, expected: str, result_type: str, config: TestConfig
    ) -> ComparisonResult:
        """Compare actual and expected results based on result type."""
        try:
            if result_type == "graph":
                return self.compare_graph_results(actual, expected, config)
            elif result_type == "bindings":
                return self.compare_bindings_results(actual, expected, config)
            elif result_type == "boolean":
                return self.compare_boolean_results(actual, expected)
            elif result_type == "srj":
                return self.compare_srj_results(actual, expected, config)
            else:
                return ComparisonResult(
                    is_match=False,
                    error_message=f"Unknown result type: {result_type}",
                    comparison_method="unknown",
                )
        except Exception as e:
            self.logger.error(f"Error during result comparison: {e}")
            return ComparisonResult(
                is_match=False, error_message=str(e), comparison_method="error"
            )

    def compare_graph_results(
        self, actual: str, expected: str, config: TestConfig
    ) -> ComparisonResult:
        """Compare graph results (N-Triples, Turtle, etc.)."""
        if self.use_rasqal_compare and self.rasqal_compare_path:
            return self._compare_with_rasqal_compare(actual, expected, config)
        else:
            return self._compare_graphs_textually(actual, expected)

    def _compare_with_rasqal_compare(
        self, actual: str, expected: str, config: TestConfig
    ) -> ComparisonResult:
        """Compare graphs using rasqal-compare tool."""
        try:
            # Create temporary files for comparison
            actual_file = config.get_temp_file("actual_graph")
            expected_file = config.get_temp_file("expected_graph")

            # Write content to files
            actual_file.write_text(actual)
            expected_file.write_text(expected)

            # Run rasqal-compare
            cmd = [self.rasqal_compare_path, str(actual_file), str(expected_file)]
            exit_code, stdout, stderr = run_command(cmd)

            if exit_code == 0:
                return ComparisonResult(
                    is_match=True, comparison_method="rasqal-compare"
                )
            else:
                # Generate diff output for debugging
                diff_output = self._generate_diff_output(actual_file, expected_file)
                return ComparisonResult(
                    is_match=False,
                    diff_output=diff_output,
                    comparison_method="rasqal-compare",
                )

        except Exception as e:
            self.logger.warning(
                f"rasqal-compare failed, falling back to textual comparison: {e}"
            )
            return self._compare_graphs_textually(actual, expected)

    def _compare_graphs_textually(self, actual: str, expected: str) -> ComparisonResult:
        """Compare graphs using textual comparison."""
        # Normalize whitespace and sort lines for comparison
        actual_normalized = self._normalize_graph_content(actual)
        expected_normalized = self._normalize_graph_content(expected)

        is_match = actual_normalized == expected_normalized

        if is_match:
            return ComparisonResult(is_match=True, comparison_method="textual")
        else:
            # Generate simple diff output
            diff_output = self._generate_simple_diff(
                actual_normalized, expected_normalized
            )
            return ComparisonResult(
                is_match=False, diff_output=diff_output, comparison_method="textual"
            )

    def _normalize_graph_content(self, content: str) -> str:
        """Normalize graph content for comparison."""
        # Split into lines, strip whitespace, filter empty lines, and sort
        lines = [line.strip() for line in content.splitlines() if line.strip()]
        lines.sort()
        return "\n".join(lines)

    def _generate_simple_diff(self, actual: str, expected: str) -> str:
        """Generate simple diff output for debugging."""
        actual_lines = actual.splitlines()
        expected_lines = expected.splitlines()

        diff_output = []
        diff_output.append("=== ACTUAL ===")
        diff_output.extend(actual_lines)
        diff_output.append("=== EXPECTED ===")
        diff_output.extend(expected_lines)

        return "\n".join(diff_output)

    def compare_bindings_results(
        self, actual: str, expected: str, config: TestConfig
    ) -> ComparisonResult:
        """Compare bindings results (CSV, TSV, etc.)."""
        try:
            # Normalize bindings content
            actual_normalized = self._normalize_bindings_content(actual)
            expected_normalized = self._normalize_bindings_content(expected)

            is_match = actual_normalized == expected_normalized

            if is_match:
                return ComparisonResult(is_match=True, comparison_method="bindings")
            else:
                # Generate diff output
                diff_output = self._generate_bindings_diff(
                    actual_normalized, expected_normalized
                )
                return ComparisonResult(
                    is_match=False,
                    diff_output=diff_output,
                    comparison_method="bindings",
                )

        except Exception as e:
            return ComparisonResult(
                is_match=False,
                error_message=f"Error comparing bindings: {e}",
                comparison_method="bindings",
            )

    def _normalize_bindings_content(self, content: str) -> str:
        """Normalize bindings content for comparison."""
        # Split into lines, strip whitespace, filter empty lines, and sort
        lines = [line.strip() for line in content.splitlines() if line.strip()]
        lines.sort()
        return "\n".join(lines)

    def _generate_bindings_diff(self, actual: str, expected: str) -> str:
        """Generate diff output for bindings comparison."""
        diff_output = []
        diff_output.append("=== ACTUAL BINDINGS ===")
        diff_output.extend(actual.splitlines())
        diff_output.append("=== EXPECTED BINDINGS ===")
        diff_output.extend(expected.splitlines())

        return "\n".join(diff_output)

    def compare_boolean_results(self, actual: bool, expected: bool) -> ComparisonResult:
        """Compare boolean results."""
        is_match = actual == expected

        if is_match:
            return ComparisonResult(is_match=True, comparison_method="boolean")
        else:
            diff_output = f"Expected: {expected}, Got: {actual}"
            return ComparisonResult(
                is_match=False, diff_output=diff_output, comparison_method="boolean"
            )

    def compare_srj_results(
        self, actual: str, expected: str, config: TestConfig
    ) -> ComparisonResult:
        """Compare SRJ results."""
        try:
            # Parse and normalize SRJ content
            actual_normalized = self._normalize_srj_content(actual)
            expected_normalized = self._normalize_srj_content(expected)

            is_match = actual_normalized == expected_normalized

            if is_match:
                return ComparisonResult(is_match=True, comparison_method="srj")
            else:
                # Generate diff output
                diff_output = self._generate_srj_diff(
                    actual_normalized, expected_normalized
                )
                return ComparisonResult(
                    is_match=False, diff_output=diff_output, comparison_method="srj"
                )

        except Exception as e:
            return ComparisonResult(
                is_match=False,
                error_message=f"Error comparing SRJ results: {e}",
                comparison_method="srj",
            )

    def _normalize_srj_content(self, content: str) -> str:
        """Normalize SRJ content for comparison."""
        # For SRJ, we'll do a simple text normalization
        # In a more sophisticated implementation, this could parse JSON and normalize
        lines = [line.strip() for line in content.splitlines() if line.strip()]
        lines.sort()
        return "\n".join(lines)

    def _generate_srj_diff(self, actual: str, expected: str) -> str:
        """Generate diff output for SRJ comparison."""
        diff_output = []
        diff_output.append("=== ACTUAL SRJ ===")
        diff_output.extend(actual.splitlines())
        diff_output.append("=== EXPECTED SRJ ===")
        diff_output.extend(expected.splitlines())

        return "\n".join(diff_output)

    def _generate_diff_output(self, actual_file: Path, expected_file: Path) -> str:
        """Generate diff output using system diff tool."""
        try:
            # Try to use system diff tool
            cmd = ["diff", "-u", str(expected_file), str(actual_file)]
            exit_code, stdout, stderr = run_command(cmd)

            if exit_code == 0:
                return "Files are identical"
            elif exit_code == 1:
                return stdout
            else:
                return f"Diff tool error: {stderr}"

        except Exception as e:
            return f"Could not generate diff: {e}"

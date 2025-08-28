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
                # Check if the content is in SRX format
                if actual.strip().startswith("<?xml") or expected.strip().startswith(
                    "<?xml"
                ):
                    return self.compare_srx_results(actual, expected, config)
                else:
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

    def compare_srx_results(
        self, actual: str, expected: str, config: TestConfig
    ) -> ComparisonResult:
        """Compare SRX (SPARQL Results XML) results."""
        try:
            self.logger.debug("SRX comparison called")
            self.logger.debug(f"Actual SRX (first 200 chars): {actual[:200]}")
            self.logger.debug(f"Expected SRX (first 200 chars): {expected[:200]}")

            # Normalize SRX content by removing formatting differences
            actual_normalized = self._normalize_srx_content(actual)
            expected_normalized = self._normalize_srx_content(expected)

            self.logger.debug(
                f"Actual normalized (first 500 chars): {actual_normalized[:500]}"
            )
            self.logger.debug(
                f"Expected normalized (first 500 chars): {expected_normalized[:500]}"
            )

            is_match = actual_normalized == expected_normalized
            self.logger.debug(f"SRX comparison result: {is_match}")

            if not is_match:
                self.logger.debug(f"Actual normalized length: {len(actual_normalized)}")
                self.logger.debug(
                    f"Expected normalized length: {len(expected_normalized)}"
                )

                # Find first difference
                for i, (a, e) in enumerate(zip(actual_normalized, expected_normalized)):
                    if a != e:
                        self.logger.debug(
                            f"First difference at position {i}: '{a}' vs '{e}'"
                        )
                        self.logger.debug(
                            f"Actual context: {actual_normalized[max(0, i-20):i+20]}"
                        )
                        self.logger.debug(
                            f"Expected context: {expected_normalized[max(0, i-20):i+20]}"
                        )
                        break

            if is_match:
                return ComparisonResult(is_match=True, comparison_method="srx")
            else:
                # Generate diff output
                diff_output = self._generate_srx_diff(
                    actual_normalized, expected_normalized
                )
                return ComparisonResult(
                    is_match=False, diff_output=diff_output, comparison_method="srx"
                )

        except Exception as e:
            return ComparisonResult(
                is_match=False,
                error_message=f"Error comparing SRX results: {e}",
                comparison_method="srx",
            )

    def _normalize_srj_content(self, content: str) -> str:
        """Normalize SRJ content for comparison."""
        # For SRJ, we'll do a simple text normalization
        # In a more sophisticated implementation, this could parse JSON and normalize
        lines = [line.strip() for line in content.splitlines() if line.strip()]
        lines.sort()
        return "\n".join(lines)

    def _normalize_srx_content(self, content: str) -> str:
        """Normalize SRX content for comparison."""
        # For SRX, we'll normalize XML by:
        # 1. Removing unnecessary whitespace between tags
        # 2. Sorting result elements to handle order differences
        # 3. Normalizing attribute order

        import re
        import xml.etree.ElementTree as ET

        try:
            # Parse XML and reformat
            root = ET.fromstring(content)

            # Remove namespace declarations for comparison
            for elem in root.iter():
                if "xmlns" in elem.attrib:
                    del elem.attrib["xmlns"]

            # Sort variables in the header for consistent comparison
            head_elem = root.find(".//{http://www.w3.org/2005/sparql-results#}head")
            if head_elem is not None:
                variable_elements = head_elem.findall(
                    ".//{http://www.w3.org/2005/sparql-results#}variable"
                )
                # Sort by variable name
                variable_elements.sort(key=lambda var: var.get("name", ""))

                # Clear and re-add sorted variables
                head_elem.clear()
                head_elem.extend(variable_elements)

            # Sort result elements by their binding values for consistent comparison
            results_elem = root.find(
                ".//{http://www.w3.org/2005/sparql-results#}results"
            )
            if results_elem is not None:
                result_elements = results_elem.findall(
                    ".//{http://www.w3.org/2005/sparql-results#}result"
                )

                # Sort bindings within each result by variable name
                for result in result_elements:
                    binding_elements = result.findall(
                        ".//{http://www.w3.org/2005/sparql-results#}binding"
                    )
                    # Sort by variable name
                    binding_elements.sort(key=lambda binding: binding.get("name", ""))
                    # Clear and re-add sorted bindings
                    result.clear()
                    result.extend(binding_elements)

                # Sort by concatenating all binding values
                def sort_key(result):
                    binding_values = []
                    for binding in result.findall(
                        ".//{http://www.w3.org/2005/sparql-results#}binding"
                    ):
                        # Get the variable name and value
                        var_name = binding.get("name")
                        value_elem = binding.find(".//*")
                        if value_elem is not None:
                            if value_elem.tag.endswith("literal"):
                                binding_values.append(f"{var_name}:{value_elem.text}")
                            elif value_elem.tag.endswith("uri"):
                                binding_values.append(f"{var_name}:{value_elem.text}")
                            elif value_elem.tag.endswith("bnode"):
                                binding_values.append(f"{var_name}:{value_elem.text}")
                    return "|".join(sorted(binding_values))

                result_elements.sort(key=sort_key)

                # Clear and re-add sorted results
                results_elem.clear()
                results_elem.extend(result_elements)

            # Convert back to string with consistent formatting
            normalized_xml = ET.tostring(root, encoding="unicode", method="xml")

            # Clean up formatting
            normalized_xml = re.sub(
                r">\s*<", "><", normalized_xml
            )  # Remove whitespace between tags
            normalized_xml = re.sub(r"\s+", " ", normalized_xml)  # Normalize whitespace
            normalized_xml = normalized_xml.replace(
                "?>", "?>\n"
            )  # Add newline after XML declaration

            return normalized_xml.strip()

        except Exception as e:
            # If XML parsing fails, fall back to simple text normalization
            self.logger.warning(
                f"Failed to parse SRX XML: {e}, falling back to text normalization"
            )
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

    def _generate_srx_diff(self, actual: str, expected: str) -> str:
        """Generate diff output for SRX comparison."""
        diff_output = []
        diff_output.append("=== ACTUAL SRX ===")
        diff_output.extend(actual.splitlines())
        diff_output.append("=== EXPECTED SRX ===")
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

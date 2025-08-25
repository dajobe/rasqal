"""
ResultProcessor class for handling result normalization and processing.

This module provides the ResultProcessor class that handles result
normalization,  conversion, and preparation for comparison.

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
import re
import subprocess
from pathlib import Path
from typing import List, Optional, Dict, Any

from ..utils import run_command, find_tool


class ProcessedResult:
    """Represents a processed and normalized result."""

    def __init__(
        self,
        normalized_content: str,
        triple_count: int = 0,
        variables: Optional[List[str]] = None,
        is_sorted: bool = False,
        metadata: Optional[Dict[str, Any]] = None,
    ):
        self.normalized_content = normalized_content
        self.triple_count = triple_count
        self.variables = variables or []
        self.is_sorted = is_sorted
        self.metadata = metadata or {}


class ResultProcessor:
    """Handles result normalization, conversion, and preparation for comparison."""

    def __init__(self, temp_file_manager=None):
        self.temp_file_manager = temp_file_manager
        self.logger = logging.getLogger(__name__)
        self.to_ntriples_path = None

    def _ensure_to_ntriples_available(self) -> str:
        """Ensure to-ntriples is available and return its path."""
        if not self.to_ntriples_path:
            self.to_ntriples_path = find_tool("to-ntriples")
            if not self.to_ntriples_path:
                raise RuntimeError("Could not find 'to-ntriples' command")
        return self.to_ntriples_path

    def normalize_blank_nodes(self, content: str) -> str:
        """Normalize blank node IDs in the content."""
        # Simple blank node normalization - replace with consistent IDs
        blank_node_map = {}
        blank_node_counter = [0]  # Use list to allow modification in nested function

        def replace_blank_node(match):
            original_id = match.group(1)
            if original_id not in blank_node_map:
                blank_node_map[original_id] = f"b{blank_node_counter[0]}"
                blank_node_counter[0] += 1
            return f"_:{blank_node_map[original_id]}"

        # Replace blank node patterns
        normalized = re.sub(r"_:([a-zA-Z0-9_]+)", replace_blank_node, content)
        return normalized

    def convert_turtle_to_ntriples(
        self, turtle_content: str, input_format: str = "turtle"
    ) -> str:
        """Convert Turtle content to N-Triples format."""
        if not turtle_content.strip():
            return ""

        # Use to-ntriples command for conversion
        to_ntriples = self._ensure_to_ntriples_available()

        try:
            # Create temporary input file
            if self.temp_file_manager:
                input_file = self.temp_file_manager.get_temp_file_path("input.ttl")
                input_file.write_text(turtle_content)
            else:
                import tempfile

                with tempfile.NamedTemporaryFile(
                    mode="w", suffix=".ttl", delete=False
                ) as f:
                    f.write(turtle_content)
                    input_file = Path(f.name)

            # Run to-ntriples conversion
            cmd = [to_ntriples, "-f", input_format, str(input_file)]
            returncode, stdout, stderr = run_command(cmd)

            if returncode != 0:
                self.logger.warning(f"to-ntriples failed: {stderr}")
                return turtle_content  # Fallback to original content

            return stdout

        except Exception as e:
            self.logger.warning(f"Failed to convert Turtle to N-Triples: {e}")
            return turtle_content  # Fallback to original content
        finally:
            # Clean up temporary file if not using temp file manager
            if not self.temp_file_manager and "input_file" in locals():
                try:
                    input_file.unlink()
                except:
                    pass

    def process_ntriples_result(self, content: str) -> ProcessedResult:
        """Process N-Triples result content."""
        if not content.strip():
            return ProcessedResult("", 0)

        # Normalize blank nodes
        normalized_content = self.normalize_blank_nodes(content)

        # Count triples (non-empty lines)
        lines = [
            line.strip() for line in normalized_content.splitlines() if line.strip()
        ]
        triple_count = len(lines)

        # Sort triples for consistent comparison
        sorted_content = "\n".join(sorted(lines)) + "\n"

        return ProcessedResult(
            normalized_content=sorted_content, triple_count=triple_count, is_sorted=True
        )

    def process_select_results(
        self, content: str, expected_vars: List[str], format: str = "csv"
    ) -> ProcessedResult:
        """Process SELECT query results."""
        if not content.strip():
            return ProcessedResult("", 0, variables=expected_vars)

        if format == "srj":
            return self._process_srj_results(content, expected_vars)
        elif format == "csv":
            return self._process_csv_results(content, expected_vars)
        else:
            # Default processing
            lines = [line.strip() for line in content.splitlines() if line.strip()]
            return ProcessedResult(
                normalized_content=content,
                triple_count=len(lines),
                variables=expected_vars,
            )

    def _process_srj_results(
        self, content: str, expected_vars: List[str]
    ) -> ProcessedResult:
        """Process SRJ (SPARQL Results JSON) results."""
        # Simple SRJ processing - extract bindings count
        binding_lines = [line for line in content.splitlines() if '"bindings"' in line]
        binding_count = len(binding_lines)

        # Normalize content (could be enhanced with proper JSON parsing)
        normalized_content = self.normalize_blank_nodes(content)

        return ProcessedResult(
            normalized_content=normalized_content,
            triple_count=binding_count,
            variables=expected_vars,
        )

    def _process_csv_results(
        self, content: str, expected_vars: List[str]
    ) -> ProcessedResult:
        """Process CSV results."""
        lines = [line.strip() for line in content.splitlines() if line.strip()]

        if len(lines) <= 1:  # Only header or empty
            return ProcessedResult(
                normalized_content=content, triple_count=0, variables=expected_vars
            )

        # Count data rows (excluding header)
        data_count = len(lines) - 1

        return ProcessedResult(
            normalized_content=content, triple_count=data_count, variables=expected_vars
        )

    def process_boolean_result(
        self, content: str, format: str = "srj"
    ) -> ProcessedResult:
        """Process boolean (ASK) query results."""
        if not content.strip():
            return ProcessedResult("", 0)

        # Extract boolean value
        boolean_value = self._extract_boolean_value(content, format)

        # Normalize content
        normalized_content = self.normalize_blank_nodes(content)

        return ProcessedResult(
            normalized_content=normalized_content,
            triple_count=1,
            metadata={"boolean_value": boolean_value},
        )

    def _extract_boolean_value(self, content: str, format: str) -> Optional[bool]:
        """Extract boolean value from result content."""
        if format == "srj":
            if '"boolean": true' in content:
                return True
            elif '"boolean": false' in content:
                return False

        # Try to parse other formats
        content_lower = content.lower().strip()
        if content_lower in ["true", "1", "yes"]:
            return True
        elif content_lower in ["false", "0", "no"]:
            return False

        return None

    def sort_results(self, results: List[str]) -> List[str]:
        """Sort results for consistent comparison."""
        return sorted(results)

    def normalize_for_comparison(
        self, content: str, result_type: str, format: str = "text"
    ) -> str:
        """Normalize content for comparison based on result type and format."""
        if result_type == "graph":
            # Convert to N-Triples if needed and normalize
            if format != "ntriples":
                content = self.convert_turtle_to_ntriples(content, format)
            processed = self.process_ntriples_result(content)
            return processed.normalized_content
        elif result_type == "bindings":
            # Process bindings results
            if format == "srj":
                # Extract variables from content (simplified)
                expected_vars = self._extract_variables_from_srj(content)
                processed = self.process_select_results(content, expected_vars, format)
            else:
                processed = self.process_select_results(content, [], format)
            return processed.normalized_content
        elif result_type == "boolean":
            # Process boolean results
            processed = self.process_boolean_result(content, format)
            return processed.normalized_content
        else:
            # Default normalization
            return self.normalize_blank_nodes(content)

    def _extract_variables_from_srj(self, content: str) -> List[str]:
        """Extract variable names from SRJ content."""
        # Simple regex to find variables in SRJ
        variables = re.findall(r'"vars":\s*\[(.*?)\]', content)
        if variables:
            # Parse the variables array
            vars_str = variables[0]
            var_names = re.findall(r'"([^"]+)"', vars_str)
            return var_names
        return []

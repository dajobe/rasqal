"""
SRJHandler class for specialized SPARQL Results JSON handling.

This module provides the SRJHandler class that handles SRJ format parsing,
normalization, and processing.
"""

import json
import logging
import re
from pathlib import Path
from typing import List, Optional, Dict, Any

from ..config import TestConfig
from ..utils.exceptions import SRJProcessingError


class SRJResult:
    """Represents a processed SRJ result."""

    def __init__(
        self,
        json_content: str,
        normalized_content: str,
        result_type: str,
        variables: Optional[List[str]] = None,
        bindings_count: int = 0,
        boolean_value: Optional[bool] = None,
        metadata: Optional[Dict[str, Any]] = None,
    ):
        self.json_content = json_content
        self.normalized_content = normalized_content
        self.result_type = result_type  # "bindings", "boolean"
        self.variables = variables or []
        self.bindings_count = bindings_count
        self.boolean_value = boolean_value
        self.metadata = metadata or {}


class SRJHandler:
    """Specialized handling for SPARQL Results JSON (SRJ) format."""

    def __init__(self, temp_file_manager=None):
        self.temp_file_manager = temp_file_manager
        self.logger = logging.getLogger(__name__)

    def handle_ask_query_srj(self, config: TestConfig, roqet_output: str) -> SRJResult:
        """Handle ASK query results in SRJ format."""
        try:
            # Parse JSON output
            json_data = json.loads(roqet_output)

            # Extract boolean value
            boolean_value = json_data.get("boolean", None)
            if boolean_value is None:
                raise SRJProcessingError("Missing 'boolean' field in SRJ output")

            # Normalize JSON
            normalized_content = self.normalize_srj_json(json_data)

            return SRJResult(
                json_content=roqet_output,
                normalized_content=normalized_content,
                result_type="boolean",
                boolean_value=boolean_value,
            )

        except json.JSONDecodeError as e:
            raise SRJProcessingError(f"Invalid JSON in SRJ output: {e}")
        except Exception as e:
            raise SRJProcessingError(f"Failed to process ASK query SRJ: {e}")

    def handle_select_query_srj(
        self, config: TestConfig, roqet_output: str
    ) -> SRJResult:
        """Handle SELECT query results in SRJ format."""
        try:
            # Parse JSON output
            json_data = json.loads(roqet_output)

            # Extract variables from head section
            variables = []
            if "head" in json_data and "vars" in json_data["head"]:
                variables = json_data["head"]["vars"]

            # Extract bindings from results section
            bindings = []
            if "results" in json_data and "bindings" in json_data["results"]:
                bindings = json_data["results"]["bindings"]
            bindings_count = len(bindings)

            # Normalize JSON
            normalized_content = self.normalize_srj_json(json_data)

            return SRJResult(
                json_content=roqet_output,
                normalized_content=normalized_content,
                result_type="bindings",
                variables=variables,
                bindings_count=bindings_count,
            )

        except json.JSONDecodeError as e:
            raise SRJProcessingError(f"Invalid JSON in SRJ output: {e}")
        except Exception as e:
            raise SRJProcessingError(f"Failed to process SELECT query SRJ: {e}")

    def normalize_srj_json(self, json_data: Dict) -> str:
        """Normalize SRJ JSON for consistent comparison."""
        try:
            # Create a copy to avoid modifying original
            normalized_data = json_data.copy()

            # Normalize blank node IDs
            normalized_data = self.normalize_blank_node_ids(normalized_data)

            # Sort bindings if present for consistent comparison
            if "bindings" in normalized_data:
                normalized_data["bindings"] = self._sort_bindings(
                    normalized_data["bindings"]
                )

            # Return normalized JSON as string
            return json.dumps(normalized_data, sort_keys=True, separators=(",", ":"))

        except Exception as e:
            self.logger.warning(f"Failed to normalize SRJ JSON: {e}")
            # Fallback to original JSON
            return json.dumps(json_data, sort_keys=True, separators=(",", ":"))

    def extract_variables_from_query(self, query_content: str) -> List[str]:
        """Extract variable names from SPARQL query."""
        # Simple regex to find variables in SELECT clause
        select_match = re.search(
            r"SELECT\s+(.+?)\s+WHERE", query_content, re.IGNORECASE | re.DOTALL
        )
        if not select_match:
            return []

        select_clause = select_match.group(1)
        # Find variables (words starting with ? or $)
        variables = re.findall(r"[?$](\w+)", select_clause)
        return variables

    def normalize_blank_node_ids(self, json_data: Dict) -> Dict:
        """Normalize blank node IDs in JSON data."""
        try:
            # Convert to string for processing
            json_str = json.dumps(json_data)

            # Normalize blank node IDs
            blank_node_map = {}
            blank_node_counter = [
                0
            ]  # Use list to allow modification in nested function

            def replace_blank_node(match):
                original_id = match.group(1)
                if original_id not in blank_node_map:
                    blank_node_map[original_id] = f"b{blank_node_counter[0]}"
                    blank_node_counter[0] += 1
                return f"_:{blank_node_map[original_id]}"

            # Replace blank node patterns
            normalized_str = re.sub(r"_:([a-zA-Z0-9_]+)", replace_blank_node, json_str)

            # Parse back to JSON
            return json.loads(normalized_str)

        except Exception as e:
            self.logger.warning(f"Failed to normalize blank node IDs: {e}")
            return json_data

    def _sort_bindings(self, bindings: List[Dict]) -> List[Dict]:
        """Sort bindings for consistent comparison."""
        try:
            # Sort bindings by variable names and values
            def binding_key(binding):
                # Create a sortable key from binding
                key_parts = []
                for var_name in sorted(binding.keys()):
                    var_value = binding[var_name]
                    if isinstance(var_value, dict):
                        # Handle typed literals and other complex values
                        value_str = json.dumps(var_value, sort_keys=True)
                    else:
                        value_str = str(var_value)
                    key_parts.append(f"{var_name}:{value_str}")
                return "|".join(key_parts)

            return sorted(bindings, key=binding_key)

        except Exception as e:
            self.logger.warning(f"Failed to sort bindings: {e}")
            return bindings

    def validate_srj_structure(self, json_data: Dict) -> bool:
        """Validate that JSON data has proper SRJ structure."""
        try:
            # Check for required fields
            if "head" not in json_data:
                return False

            # Check for either "boolean" or "results" field
            has_boolean = "boolean" in json_data
            has_results = "results" in json_data

            if not (has_boolean or has_results):
                return False

            # If it's a boolean result, check boolean field type
            if has_boolean and not isinstance(json_data["boolean"], bool):
                return False

            # If it's a results result, check structure
            if has_results:
                results = json_data["results"]
                if not isinstance(results, dict) or "bindings" not in results:
                    return False
                if not isinstance(results["bindings"], list):
                    return False

            return True

        except Exception:
            return False

    def extract_metadata(self, json_data: Dict) -> Dict[str, Any]:
        """Extract metadata from SRJ output."""
        metadata = {}

        try:
            # Extract head information
            if "head" in json_data:
                head = json_data["head"]
                if "vars" in head:
                    metadata["variables"] = head["vars"]
                if "link" in head:
                    metadata["links"] = head["link"]

            # Extract result information
            if "results" in json_data:
                results = json_data["results"]
                metadata["bindings_count"] = len(results.get("bindings", []))

                # Check for ordered/distinct information
                if "ordered" in results:
                    metadata["ordered"] = results["ordered"]
                if "distinct" in results:
                    metadata["distinct"] = results["distinct"]

            # Extract boolean value if present
            if "boolean" in json_data:
                metadata["boolean_value"] = json_data["boolean"]

        except Exception as e:
            self.logger.warning(f"Failed to extract metadata: {e}")

        return metadata

    def write_normalized_srj(self, srj_result: SRJResult, output_path: Path) -> None:
        """Write normalized SRJ content to a file."""
        try:
            if self.temp_file_manager:
                # Use temp file manager if available
                self.temp_file_manager.preserve_file(
                    "normalized.srj", srj_result.normalized_content
                )
            else:
                # Write directly to output path
                output_path.write_text(srj_result.normalized_content)

        except Exception as e:
            raise SRJProcessingError(f"Failed to write normalized SRJ: {e}")

    def compare_srj_results(self, actual: SRJResult, expected: SRJResult) -> bool:
        """Compare two SRJ results for equality."""
        try:
            # Compare basic properties
            if actual.result_type != expected.result_type:
                return False

            if actual.result_type == "boolean":
                return actual.boolean_value == expected.boolean_value

            elif actual.result_type == "bindings":
                # Compare variables
                if set(actual.variables) != set(expected.variables):
                    return False

                # Compare bindings count
                if actual.bindings_count != expected.bindings_count:
                    return False

                # Compare normalized content
                return actual.normalized_content == expected.normalized_content

            return False

        except Exception as e:
            self.logger.warning(f"Failed to compare SRJ results: {e}")
            return False

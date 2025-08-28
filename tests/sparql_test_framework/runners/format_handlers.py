"""
Format Handlers Module for SPARQL Test Results

This module handles different file format processing for various result types
(CSV, TSV, SRX, SRJ), extracting complex format handling logic from the main sparql.py file.

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

import json
import logging
import re
from pathlib import Path
from typing import Dict, Any, List, Optional

logger = logging.getLogger(__name__)


class FormatHandlers:
    """Handles different file format processing for various result types."""

    def __init__(self):
        self.logger = logging.getLogger(__name__)

    def handle_csv_tsv_boolean(
        self, result_file_path: Path
    ) -> Optional[Dict[str, Any]]:
        """Handle boolean results in CSV/TSV formats."""
        try:
            content = result_file_path.read_text().strip()
            # CSV/TSV boolean results contain just "true" or "false"
            if content.lower() in ["true", "false"]:
                # Parse boolean result properly
                boolean_value = content.lower() == "true"
                self.logger.debug(f"Parsed CSV/TSV boolean result: {boolean_value}")
                return {
                    "type": "boolean",
                    "value": boolean_value,
                    "count": 1,
                    "vars": [],
                    "results": [],
                }
        except Exception as e:
            self.logger.debug(f"Could not read file content for boolean detection: {e}")
        return None

    def handle_empty_srx(
        self, result_file_path: Path, expected_vars_order: List[str]
    ) -> Optional[Dict[str, Any]]:
        """Handle empty SRX result files."""
        try:
            content = result_file_path.read_text()
            # Check if this is an empty SRX file (contains <results></results> with possible whitespace/newlines)
            if re.search(r"<results>\s*</results>", content):
                self.logger.debug(f"Detected empty SRX result file: {result_file_path}")
                # Extract variable names from the <head> section
                var_matches = re.findall(r'<variable name="([^"]+)"', content)
                if var_matches:
                    order_to_use = var_matches
                else:
                    order_to_use = expected_vars_order
                # Return empty result set
                # Write to RESULT_OUT for comparison
                try:
                    # Import here to avoid circular imports
                    from .sparql import get_temp_file_path

                    get_temp_file_path("result.out").write_text("")
                except Exception:
                    pass
                return {
                    "content": "",
                    "count": 0,
                    "format": "bindings",
                    "vars_order": order_to_use,
                }
        except Exception as e:
            self.logger.debug(f"Could not check SRX file content: {e}")
        return None

    def handle_srj_file(
        self, result_file_path: Path, expected_vars_order: List[str]
    ) -> Optional[Dict[str, Any]]:
        """Handle SRJ (SPARQL Results JSON) files."""
        try:
            content = result_file_path.read_text()
            data = json.loads(content)

            # Check if this is a boolean result (ASK query)
            if "boolean" in data:
                self.logger.debug(
                    f"Detected boolean SRJ result file: {result_file_path}"
                )
                boolean_value = data["boolean"]
                # Write to RESULT_OUT for comparison
                try:
                    # Import here to avoid circular imports
                    from .sparql import get_temp_file_path

                    get_temp_file_path("result.out").write_text(
                        str(boolean_value).lower()
                    )
                except Exception:
                    pass
                return {
                    "content": str(boolean_value).lower(),
                    "count": 1,
                    "result_type": "boolean",
                    "type": "boolean",
                    "value": boolean_value,
                    "vars_order": [],
                }

            # Check if this is an empty bindings result
            if (
                "results" in data
                and "bindings" in data["results"]
                and len(data["results"]["bindings"]) == 0
            ):
                self.logger.debug(
                    f"Detected empty bindings SRJ result file: {result_file_path}"
                )
                # Extract variable names from the head section
                vars_order = data.get("head", {}).get("vars", [])
                if not vars_order:
                    vars_order = expected_vars_order
                # Write to RESULT_OUT for comparison
                try:
                    # Import here to avoid circular imports
                    from .sparql import get_temp_file_path

                    get_temp_file_path("result.out").write_text("")
                except Exception:
                    pass
                return {
                    "content": "",
                    "count": 0,
                    "format": "bindings",
                    "vars_order": vars_order,
                }

            # Handle non-empty bindings result
            if "results" in data and "bindings" in data["results"]:
                bindings = data["results"]["bindings"]
                vars_order = data.get("head", {}).get("vars", [])
                if not vars_order:
                    vars_order = expected_vars_order

                # Convert to simple text format for comparison
                result_lines = []
                for binding in bindings:
                    binding_parts = []
                    for var in vars_order:
                        if var in binding:
                            value = binding[var]
                            if isinstance(value, dict):
                                if value.get("type") == "uri":
                                    binding_parts.append(f"{var}={value['value']}")
                                elif value.get("type") == "literal":
                                    binding_parts.append(f"{var}={value['value']}")
                                elif value.get("type") == "bnode":
                                    binding_parts.append(f"{var}=_:{value['value']}")
                                else:
                                    binding_parts.append(f"{var}={value['value']}")
                            else:
                                binding_parts.append(f"{var}={value}")
                        else:
                            binding_parts.append(f"{var}=unbound")
                    result_lines.append(" ".join(binding_parts))

                result_content = "\n".join(result_lines)
                if result_content:
                    result_content += "\n"

                # Write to RESULT_OUT for comparison
                try:
                    # Import here to avoid circular imports
                    from .sparql import get_temp_file_path

                    get_temp_file_path("result.out").write_text(result_content)
                except Exception:
                    pass

                return {
                    "content": result_content,
                    "count": len(bindings),
                    "format": "bindings",
                    "vars_order": vars_order,
                }

        except Exception as e:
            self.logger.debug(f"Could not process SRJ file: {e}")
        return None


# Convenience functions for backward compatibility
def handle_csv_tsv_boolean(result_file_path: Path) -> Optional[Dict[str, Any]]:
    """Backward compatibility wrapper for FormatHandlers.handle_csv_tsv_boolean."""
    handler = FormatHandlers()
    return handler.handle_csv_tsv_boolean(result_file_path)


def handle_empty_srx(
    result_file_path: Path, expected_vars_order: List[str]
) -> Optional[Dict[str, Any]]:
    """Backward compatibility wrapper for FormatHandlers.handle_empty_srx."""
    handler = FormatHandlers()
    return handler.handle_empty_srx(result_file_path, expected_vars_order)


def handle_srj_file(
    result_file_path: Path, expected_vars_order: List[str]
) -> Optional[Dict[str, Any]]:
    """Backward compatibility wrapper for FormatHandlers.handle_srj_file."""
    handler = FormatHandlers()
    return handler.handle_srj_file(result_file_path, expected_vars_order)

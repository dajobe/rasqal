"""
SRX Parser Module for SPARQL Test Results

This module handles SRX (SPARQL Results XML) format parsing, extracting complex
SRX processing logic from the main sparql.py file.

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
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Dict, Any, List, Optional

logger = logging.getLogger(__name__)

# SPARQL Results namespace
SPARQL_NS = "http://www.w3.org/2005/sparql-results#"
# Redland namespace for undefined values
RS_NS = "http://ns.librdf.org/2005/sparql-results#"


class SRXParser:
    """Handles SRX (SPARQL Results XML) format parsing."""

    def __init__(self):
        self.namespaces = {"sparql": SPARQL_NS}

    def parse_srx_from_roqet_output(
        self,
        srx_content: str,
        expected_vars_order: List[str],
        sort_output: bool,
        get_temp_file_path_func,
    ) -> Optional[Dict[str, Any]]:
        """
        Parse SRX content from roqet output into normalized format.
        Returns a dictionary compatible with existing test infrastructure.
        """
        try:
            # Parse XML using ElementTree
            root = ET.fromstring(srx_content)

            # Extract variables
            variables = self._extract_variables(root)

            # Parse results
            parsed_rows_for_output = self._parse_results(
                root, expected_vars_order, variables
            )

            # Sort results if requested
            if sort_output:
                parsed_rows_for_output.sort()

            # Write to file for comparison
            self._write_result_file(parsed_rows_for_output, get_temp_file_path_func)

            return {"count": len(parsed_rows_for_output)}

        except ET.ParseError as e:
            logger.error(f"Error parsing SRX XML: {e}")
            return None
        except Exception as e:
            logger.error(f"Error processing SRX content: {e}")
            return None

    def _extract_variables(self, root: ET.Element) -> List[str]:
        """Extract variable names from SRX head section."""
        variables = []
        vars_elem = root.find(".//sparql:head", self.namespaces)
        if vars_elem is not None:
            for var_elem in vars_elem.findall(".//sparql:variable", self.namespaces):
                var_name = var_elem.get("name")
                if var_name:
                    variables.append(var_name)
        return variables

    def _parse_results(
        self, root: ET.Element, expected_vars_order: List[str], variables: List[str]
    ) -> List[str]:
        """Parse result rows from SRX content."""
        parsed_rows_for_output = []
        results_elem = root.find(".//sparql:results", self.namespaces)

        if results_elem is not None:
            for result_elem in results_elem.findall(
                ".//sparql:result", self.namespaces
            ):
                result_row = self._parse_result_row(result_elem)

                # Use expected_vars_order if available, otherwise use discovered variables
                order_to_use = expected_vars_order if expected_vars_order else variables

                # Format row in the same way as the existing function
                formatted_row = self._format_result_row(result_row, order_to_use)
                parsed_rows_for_output.append(formatted_row)

        return parsed_rows_for_output

    def _parse_result_row(self, result_elem: ET.Element) -> Dict[str, str]:
        """Parse a single result row from SRX."""
        result_row = {}

        for binding_elem in result_elem.findall(".//sparql:binding", self.namespaces):
            var_name = binding_elem.get("name")
            if var_name:
                value = self._extract_binding_value(binding_elem)
                if value:
                    result_row[var_name] = value

        return result_row

    def _extract_binding_value(self, binding_elem: ET.Element) -> Optional[str]:
        """Extract value from a binding element based on its type."""
        value_elem = binding_elem.find("*")
        if value_elem is None:
            return None

        if value_elem.tag.endswith("uri"):
            return f'uri("{value_elem.text}")'
        elif value_elem.tag.endswith("literal"):
            return self._format_literal_value(value_elem)
        elif value_elem.tag.endswith("bnode"):
            return f"blank {value_elem.text}"

        return None

    def _format_literal_value(self, value_elem: ET.Element) -> str:
        """Format literal values with appropriate type information."""
        datatype = value_elem.get("datatype")
        lang = value_elem.get("{http://www.w3.org/XML/1998/namespace}lang")

        if datatype:
            return f'literal("{value_elem.text}", datatype={datatype})'
        elif lang:
            return f'literal("{value_elem.text}", lang={lang})'
        else:
            return f'string("{value_elem.text}")'

    def _format_result_row(
        self, result_row: Dict[str, str], order_to_use: List[str]
    ) -> str:
        """Format a result row for output."""
        formatted_row_parts = [
            f"{var}={result_row.get(var, RS_NS + 'undefined')}" for var in order_to_use
        ]
        return f"row: [{', '.join(formatted_row_parts)}]"

    def _write_result_file(
        self, parsed_rows_for_output: List[str], get_temp_file_path_func
    ):
        """Write parsed results to output file."""
        try:
            content = (
                "\n".join(parsed_rows_for_output) + "\n"
                if parsed_rows_for_output
                else ""
            )
            get_temp_file_path_func("result.out").write_text(content)
        except Exception as e:
            logger.warning(f"Could not write result file: {e}")


# Convenience function for backward compatibility
def parse_srx_from_roqet_output(
    srx_content: str,
    expected_vars_order: List[str],
    sort_output: bool,
    get_temp_file_path_func,
) -> Optional[Dict[str, Any]]:
    """Backward compatibility wrapper for SRXParser.parse_srx_from_roqet_output."""
    parser = SRXParser()
    return parser.parse_srx_from_roqet_output(
        srx_content, expected_vars_order, sort_output, get_temp_file_path_func
    )

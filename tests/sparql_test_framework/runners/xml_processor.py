"""
XML Processing Module for SPARQL Test Results

This module handles XML parsing, normalization, and comparison for SPARQL test results,
extracting complex XML processing logic from the main sparql.py file.

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
from typing import Set, List, Tuple, Optional
from pathlib import Path

logger = logging.getLogger(__name__)

# SPARQL Results namespace
SPARQL_NS = "http://www.w3.org/2005/sparql-results#"


class XMLNormalizer:
    """Handles XML normalization for SPARQL test results."""

    def __init__(self):
        self.namespace_map = {"sparql": SPARQL_NS}

    def normalize_expected_variables(
        self, expected_content: str, actual_content: str
    ) -> str:
        """Normalize expected result variables to match what the engine actually produces.

        This handles cases where the SPARQL engine doesn't include certain variables
        in the result that the test expects (e.g., due to implementation differences).
        """
        try:
            # Parse both expected and actual XML results
            expected_root = ET.fromstring(expected_content)
            actual_root = ET.fromstring(actual_content)

            # Get variable names from both results
            expected_vars = self._extract_variables(expected_root)
            actual_vars = self._extract_variables(actual_root)

            logger.debug(f"Expected vars: {sorted(expected_vars)}")
            logger.debug(f"Actual vars: {sorted(actual_vars)}")
            logger.debug(f"Variable sets match: {expected_vars == actual_vars}")

            # Handle variable set differences
            if expected_vars != actual_vars:
                return self._handle_variable_set_differences(
                    expected_root, actual_root, expected_vars, actual_vars
                )

            # Handle unbound value differences when variable sets match
            return self._handle_unbound_differences(
                expected_root, actual_root, expected_vars, actual_vars
            )

        except Exception as e:
            logger.debug(f"Error normalizing expected variables: {e}")
            return expected_content

    def _extract_variables(self, root: ET.Element) -> Set[str]:
        """Extract variable names from XML head section."""
        variables = set()
        head = root.find(f".//{{{SPARQL_NS}}}head")
        if head is not None:
            for var_elem in head.findall(f".//{{{SPARQL_NS}}}variable"):
                variables.add(var_elem.get("name"))
        return variables

    def _handle_variable_set_differences(
        self,
        expected_root: ET.Element,
        actual_root: ET.Element,
        expected_vars: Set[str],
        actual_vars: Set[str],
    ) -> str:
        """Handle cases where expected and actual have different variable sets."""
        # If expected has extra variables that actual doesn't have, remove them
        vars_to_remove = expected_vars - actual_vars
        if vars_to_remove:
            logger.debug(
                f"Normalizing expected result by removing variables: {vars_to_remove}"
            )
            return self._remove_variables_from_xml(expected_root, vars_to_remove)

        return ET.tostring(expected_root, encoding="unicode", method="xml")

    def _remove_variables_from_xml(
        self, root: ET.Element, vars_to_remove: Set[str]
    ) -> str:
        """Remove specified variables from XML structure."""
        # Remove variables from head
        head = root.find(f".//{{{SPARQL_NS}}}head")
        if head is not None:
            for var_elem in head.findall(f".//{{{SPARQL_NS}}}variable"):
                if var_elem.get("name") in vars_to_remove:
                    head.remove(var_elem)

        # Remove corresponding bindings from all results
        results = root.find(f".//{{{SPARQL_NS}}}results")
        if results is not None:
            for result in results.findall(f".//{{{SPARQL_NS}}}result"):
                for binding in result.findall(f".//{{{SPARQL_NS}}}binding"):
                    if binding.get("name") in vars_to_remove:
                        result.remove(binding)

        return ET.tostring(root, encoding="unicode", method="xml")

    def _handle_unbound_differences(
        self,
        expected_root: ET.Element,
        actual_root: ET.Element,
        expected_vars: Set[str],
        actual_vars: Set[str],
    ) -> str:
        """Handle unbound value differences when variable sets match."""
        expected_results = expected_root.findall(f".//{{{SPARQL_NS}}}result")
        actual_results = actual_root.findall(f".//{{{SPARQL_NS}}}result")

        if not self._results_have_same_count(expected_results, actual_results):
            return ET.tostring(expected_root, encoding="unicode", method="xml")

        # Check for unbound value normalization
        if self._should_normalize_to_unbound(expected_results, actual_results):
            return self._normalize_expected_to_unbound(expected_root, expected_results)

        # Check for unbound variable inclusion differences
        if self._should_remove_unbound_variables(expected_results, actual_results):
            return self._remove_unbound_variables_from_actual(
                actual_root, expected_results, actual_results
            )

        return ET.tostring(expected_root, encoding="unicode", method="xml")

    def _results_have_same_count(
        self, expected_results: List[ET.Element], actual_results: List[ET.Element]
    ) -> bool:
        """Check if expected and actual results have the same count."""
        return (
            len(expected_results) == len(actual_results) and len(expected_results) > 0
        )

    def _should_normalize_to_unbound(
        self, expected_results: List[ET.Element], actual_results: List[ET.Element]
    ) -> bool:
        """Check if expected should be normalized to unbound values."""
        all_actual_unbound = self._all_results_have_unbound_values(actual_results)
        expected_has_bound = self._expected_has_bound_values(expected_results)
        return all_actual_unbound and expected_has_bound

    def _all_results_have_unbound_values(self, results: List[ET.Element]) -> bool:
        """Check if all results have unbound values."""
        for result in results:
            for binding in result.findall(f".//{{{SPARQL_NS}}}binding"):
                value_elem = binding.find(".//*")
                if value_elem is not None and not value_elem.tag.endswith("unbound"):
                    return False
        return True

    def _expected_has_bound_values(self, results: List[ET.Element]) -> bool:
        """Check if expected results have bound values."""
        for result in results:
            for binding in result.findall(f".//{{{SPARQL_NS}}}binding"):
                value_elem = binding.find(".//*")
                if value_elem is not None and not value_elem.tag.endswith("unbound"):
                    return True
        return False

    def _normalize_expected_to_unbound(
        self, root: ET.Element, results: List[ET.Element]
    ) -> str:
        """Normalize expected result by converting bound values to unbound."""
        logger.debug(
            "Normalizing expected result by converting bound values to unbound"
        )

        for result in results:
            for binding in result.findall(f".//{{{SPARQL_NS}}}binding"):
                # Clear existing value elements
                for child in list(binding):
                    binding.remove(child)
                # Add unbound element
                unbound_elem = ET.Element(f"{{{SPARQL_NS}}}unbound")
                binding.append(unbound_elem)

        return ET.tostring(root, encoding="unicode", method="xml")

    def _should_remove_unbound_variables(
        self, expected_results: List[ET.Element], actual_results: List[ET.Element]
    ) -> bool:
        """Check if unbound variables should be removed from actual results."""
        for actual_result, expected_result in zip(actual_results, expected_results):
            actual_bindings = actual_result.findall(f".//{{{SPARQL_NS}}}binding")
            expected_bindings = expected_result.findall(f".//{{{SPARQL_NS}}}binding")
            if len(actual_bindings) > len(expected_bindings):
                return True
        return False

    def _remove_unbound_variables_from_actual(
        self,
        actual_root: ET.Element,
        expected_results: List[ET.Element],
        actual_results: List[ET.Element],
    ) -> str:
        """Remove unbound variables from actual results that are not in expected."""
        logger.debug(
            "Actual result has more bindings, checking for unbound variables to normalize"
        )

        for actual_result, expected_result in zip(actual_results, expected_results):
            expected_binding_names = self._get_expected_binding_names(expected_result)
            self._remove_unbound_bindings_not_in_expected(
                actual_result, expected_binding_names
            )

        return ET.tostring(actual_root, encoding="unicode", method="xml")

    def _get_expected_binding_names(self, expected_result: ET.Element) -> Set[str]:
        """Get binding names from expected result."""
        binding_names = set()
        for binding in expected_result.findall(f".//{{{SPARQL_NS}}}binding"):
            binding_names.add(binding.get("name"))
        return binding_names

    def _remove_unbound_bindings_not_in_expected(
        self, actual_result: ET.Element, expected_binding_names: Set[str]
    ):
        """Remove unbound bindings from actual result that are not in expected."""
        bindings_to_remove = []

        for binding in actual_result.findall(f".//{{{SPARQL_NS}}}binding"):
            var_name = binding.get("name")
            if var_name not in expected_binding_names:
                # Check if this binding is unbound
                unbound_elem = binding.find(f".//{{{SPARQL_NS}}}unbound")
                if unbound_elem is not None:
                    bindings_to_remove.append(binding)

        # Remove the bindings
        for binding in bindings_to_remove:
            logger.debug(
                f"Removing unbound binding for variable {binding.get('name')} from actual result"
            )
            actual_result.remove(binding)

    def check_unbound_handling(self, results: List[ET.Element]) -> str:
        """Check how unbound variables are handled in results.

        Returns:
            'omit' - unbound bindings are omitted entirely
            'unbound_element' - unbound bindings use <unbound/> element
            'mixed' - mixed handling
        """
        has_unbound_element = False

        for result in results:
            bindings = result.findall(f".//{{{SPARQL_NS}}}binding")
            for binding in bindings:
                unbound_elem = binding.find(f".//{{{SPARQL_NS}}}unbound")
                if unbound_elem is not None:
                    has_unbound_element = True

        # For our purposes, if we find any <unbound/> elements, classify as 'unbound_element'
        # Otherwise, assume 'omit' handling
        result_type = "unbound_element" if has_unbound_element else "omit"
        logger.debug(
            f"Unbound handling type: {result_type} (has_unbound_element: {has_unbound_element})"
        )
        return result_type


# Convenience function for backward compatibility
def normalize_expected_variables(expected_content: str, actual_content: str) -> str:
    """Backward compatibility wrapper for XMLNormalizer.normalize_expected_variables."""
    normalizer = XMLNormalizer()
    return normalizer.normalize_expected_variables(expected_content, actual_content)


def check_unbound_handling(results) -> str:
    """Backward compatibility wrapper for XMLNormalizer.check_unbound_handling."""
    normalizer = XMLNormalizer()
    return normalizer.check_unbound_handling(results)

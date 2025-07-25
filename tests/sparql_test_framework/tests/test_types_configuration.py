#!/usr/bin/env python3
"""
Unit tests for types configuration improvements.

This module contains unit tests for the configuration improvements made to
Namespaces, TestResult, and TestTypeResolver classes in sparql_test_framework.types.

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

import unittest
from test_base import SparqlTestFrameworkTestBase

# Import from new module structure
from sparql_test_framework.test_types import (
    Namespaces,
    TestResult,
    TestType,
    TestTypeResolver,
)


class TestNamespacesConfiguration(SparqlTestFrameworkTestBase):
    """Test the Namespaces class configuration improvements."""

    def test_test_type_mapping_constant(self):
        """Test that _TEST_TYPE_MAPPING constant exists and has expected entries."""
        self.assertIn("PositiveTest", Namespaces._TEST_TYPE_MAPPING)
        self.assertIn("NegativeTest", Namespaces._TEST_TYPE_MAPPING)
        self.assertIn("XFailTest", Namespaces._TEST_TYPE_MAPPING)
        self.assertIn("WarningTest", Namespaces._TEST_TYPE_MAPPING)
        self.assertIn("PositiveSyntaxTest", Namespaces._TEST_TYPE_MAPPING)
        self.assertIn("NegativeSyntaxTest", Namespaces._TEST_TYPE_MAPPING)
        self.assertIn("QueryEvaluationTest", Namespaces._TEST_TYPE_MAPPING)

    def test_prefix_to_namespace_constant(self):
        """Test that _PREFIX_TO_NAMESPACE reverse mapping exists and is correct."""
        # Test that reverse mapping exists
        self.assertIsInstance(Namespaces._PREFIX_TO_NAMESPACE, dict)

        # Test some key mappings
        self.assertEqual(Namespaces._PREFIX_TO_NAMESPACE["t"], Namespaces.T)
        self.assertEqual(Namespaces._PREFIX_TO_NAMESPACE["mf"], Namespaces.MF)
        self.assertEqual(Namespaces._PREFIX_TO_NAMESPACE["ut"], Namespaces.UT)

        # Test that it's the reverse of _NAMESPACE_PREFIXES
        for uri, prefix in Namespaces._NAMESPACE_PREFIXES.items():
            self.assertEqual(Namespaces._PREFIX_TO_NAMESPACE[prefix], uri)

    def test_uri_to_prefixed_name_simple_types(self):
        """Test uri_to_prefixed_name with simple test type names."""
        self.assertEqual(
            Namespaces.uri_to_prefixed_name("PositiveTest"), "t:PositiveTest"
        )
        self.assertEqual(
            Namespaces.uri_to_prefixed_name("NegativeTest"), "t:NegativeTest"
        )
        self.assertEqual(
            Namespaces.uri_to_prefixed_name("PositiveSyntaxTest"),
            "mf:PositiveSyntaxTest",
        )

    def test_uri_to_prefixed_name_full_uris(self):
        """Test uri_to_prefixed_name with full URIs."""
        test_uri = f"{Namespaces.T}PositiveTest"
        self.assertEqual(Namespaces.uri_to_prefixed_name(test_uri), "t:PositiveTest")

        test_uri = f"{Namespaces.MF}QueryEvaluationTest"
        self.assertEqual(
            Namespaces.uri_to_prefixed_name(test_uri), "mf:QueryEvaluationTest"
        )

    def test_uri_to_prefixed_name_edge_cases(self):
        """Test uri_to_prefixed_name with edge cases."""
        # Empty string
        self.assertEqual(Namespaces.uri_to_prefixed_name(""), "")

        # None
        self.assertEqual(Namespaces.uri_to_prefixed_name(None), None)

        # Unknown simple name
        self.assertEqual(Namespaces.uri_to_prefixed_name("UnknownTest"), "UnknownTest")

        # Unknown full URI
        unknown_uri = "http://unknown.org/test#UnknownTest"
        self.assertEqual(Namespaces.uri_to_prefixed_name(unknown_uri), "UnknownTest")

    def test_prefixed_name_to_uri_optimization(self):
        """Test that prefixed_name_to_uri uses the reverse mapping for efficiency."""
        # Test that the method works correctly
        self.assertEqual(
            Namespaces.prefixed_name_to_uri("t:PositiveTest"),
            f"{Namespaces.T}PositiveTest",
        )

        # Test that it handles unknown prefixes gracefully
        self.assertEqual(
            Namespaces.prefixed_name_to_uri("unknown:Test"), "unknown:Test"
        )

    def test_prefixed_name_to_uri_edge_cases(self):
        """Test prefixed_name_to_uri with edge cases."""
        # Empty string
        self.assertEqual(Namespaces.prefixed_name_to_uri(""), "")

        # None
        self.assertEqual(Namespaces.prefixed_name_to_uri(None), None)

        # No prefix
        self.assertEqual(Namespaces.prefixed_name_to_uri("Test"), "Test")

        # Multiple colons (should not happen in practice)
        self.assertEqual(Namespaces.prefixed_name_to_uri("a:b:c"), "a:b:c")


class TestResultConfiguration(SparqlTestFrameworkTestBase):
    """Test the TestResult enum configuration improvements."""

    def test_display_char_all_results(self):
        """Test that all TestResult values have display characters."""
        for result in TestResult:
            self.assertIsInstance(result.display_char(), str)
            self.assertEqual(len(result.display_char()), 1)

    def test_display_name_all_results(self):
        """Test that all TestResult values have display names."""
        for result in TestResult:
            self.assertIsInstance(result.display_name(), str)
            self.assertGreater(len(result.display_name()), 0)


class TestTypeResolverConfiguration(SparqlTestFrameworkTestBase):
    """Test the TestTypeResolver class configuration improvements."""

    def test_default_behavior_constant(self):
        """Test that _DEFAULT_BEHAVIOR constant exists and has correct structure."""
        self.assertIsInstance(TestTypeResolver._DEFAULT_BEHAVIOR, tuple)
        self.assertEqual(len(TestTypeResolver._DEFAULT_BEHAVIOR), 3)

        should_execute, expected_result, language = TestTypeResolver._DEFAULT_BEHAVIOR
        self.assertIsInstance(should_execute, bool)
        self.assertIsInstance(expected_result, TestResult)
        self.assertIsInstance(language, str)

    def test_skipped_test_types_constant(self):
        """Test that _SKIPPED_TEST_TYPES constant exists and contains expected types."""
        self.assertIsInstance(TestTypeResolver._SKIPPED_TEST_TYPES, set)

        # Test that known skipped types are included
        skipped_types = [
            TestType.UPDATE_EVALUATION_TEST.value,
            TestType.PROTOCOL_TEST.value,
        ]

        for test_type in skipped_types:
            self.assertIn(test_type, TestTypeResolver._SKIPPED_TEST_TYPES)

    def test_behavior_pattern_constants_used(self):
        """Test that behavior pattern constants are used in the behavior map."""
        # Test that skipped test types return skipped behavior
        for test_type in TestTypeResolver._SKIPPED_TEST_TYPES:
            result = TestTypeResolver.resolve_test_behavior(test_type)
            self.assertEqual(result[1], TestResult.SKIPPED)

        # Test that default behavior is used for unknown types
        unknown_result = TestTypeResolver.resolve_test_behavior("UnknownTestType")
        self.assertEqual(unknown_result, TestTypeResolver._DEFAULT_BEHAVIOR)

    def test_configuration_constants_consistency(self):
        """Test that configuration constants are consistent with each other."""
        # Test that default behavior is used for unknown types
        unknown_result = TestTypeResolver.resolve_test_behavior("UnknownTestType")
        self.assertEqual(unknown_result, TestTypeResolver._DEFAULT_BEHAVIOR)

        # Test that skipped types are actually skipped
        for test_type in TestTypeResolver._SKIPPED_TEST_TYPES:
            result = TestTypeResolver.resolve_test_behavior(test_type)
            self.assertEqual(result[1], TestResult.SKIPPED)


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3
"""
Unit tests for types configuration improvements.

This module contains unit tests for the configuration improvements made to
Namespaces, TestResult, and TestTypeResolver classes in types.py.

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
import sys
import os

sys.path.insert(0, os.path.dirname(__file__))
from test_base import SparqlTestFrameworkTestBase

# Import from module structure
from sparql_test_framework.test_types import Namespaces, TestResult, TestTypeResolver


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

    def test_prefixed_name_to_uri_optimization(self):
        """Test that prefixed_name_to_uri uses optimized lookup."""
        # Test normal cases
        self.assertEqual(
            Namespaces.prefixed_name_to_uri("t:PositiveTest"),
            f"{Namespaces.T}PositiveTest",
        )
        self.assertEqual(
            Namespaces.prefixed_name_to_uri("mf:QueryEvaluationTest"),
            f"{Namespaces.MF}QueryEvaluationTest",
        )

    def test_prefixed_name_to_uri_edge_cases(self):
        """Test prefixed_name_to_uri with edge cases."""
        # No colon
        self.assertEqual(Namespaces.prefixed_name_to_uri("nocolon"), "nocolon")

        # Empty string
        self.assertEqual(Namespaces.prefixed_name_to_uri(""), "")

        # None
        self.assertEqual(Namespaces.prefixed_name_to_uri(None), None)

        # Unknown prefix
        self.assertEqual(
            Namespaces.prefixed_name_to_uri("unknown:test"), "unknown:test"
        )


class TestResultConfiguration(SparqlTestFrameworkTestBase):
    """Test the TestResult class configuration improvements."""

    def test_display_char_all_results(self):
        """Test display_char returns correct character for all result types."""
        self.assertEqual(TestResult.PASSED.display_char(), ".")
        self.assertEqual(TestResult.FAILED.display_char(), "F")
        self.assertEqual(TestResult.XFAILED.display_char(), "*")
        self.assertEqual(TestResult.UXPASSED.display_char(), "!")
        self.assertEqual(TestResult.SKIPPED.display_char(), "-")

    def test_display_name_all_results(self):
        """Test display_name returns correct names for all result types."""
        self.assertEqual(TestResult.PASSED.display_name(), "passed")
        self.assertEqual(TestResult.FAILED.display_name(), "FAILED")
        self.assertEqual(TestResult.XFAILED.display_name(), "XFAILED")
        self.assertEqual(TestResult.UXPASSED.display_name(), "UXPASSED")
        self.assertEqual(TestResult.SKIPPED.display_name(), "skipped")


class TestTypeResolverConfiguration(SparqlTestFrameworkTestBase):
    """Test the TestTypeResolver class configuration improvements."""

    def test_default_behavior_constant(self):
        """Test that _DEFAULT_BEHAVIOR constant is used consistently."""
        # Test unknown type uses default
        result = TestTypeResolver.resolve_test_behavior("unknown_type")
        self.assertEqual(result, TestTypeResolver._DEFAULT_BEHAVIOR)

        # Test None uses default
        result = TestTypeResolver.resolve_test_behavior(None)
        self.assertEqual(result, TestTypeResolver._DEFAULT_BEHAVIOR)

        # Test empty string uses default
        result = TestTypeResolver.resolve_test_behavior("")
        self.assertEqual(result, TestTypeResolver._DEFAULT_BEHAVIOR)

    def test_skipped_test_types_constant(self):
        """Test that _SKIPPED_TEST_TYPES constant is correct."""
        from sparql_test_framework.test_types import TestType

        # Test that expected types are in the skipped set
        self.assertIn(
            TestType.UPDATE_EVALUATION_TEST.value, TestTypeResolver._SKIPPED_TEST_TYPES
        )
        self.assertIn(
            TestType.PROTOCOL_TEST.value, TestTypeResolver._SKIPPED_TEST_TYPES
        )

        # Test should_skip_test_type uses this constant
        self.assertTrue(
            TestTypeResolver.should_skip_test_type(
                TestType.UPDATE_EVALUATION_TEST.value
            )
        )
        self.assertTrue(
            TestTypeResolver.should_skip_test_type(TestType.PROTOCOL_TEST.value)
        )

    def test_behavior_pattern_constants_used(self):
        """Test that behavior pattern constants are used in mapping."""
        # Test that constants exist
        self.assertIsNotNone(TestTypeResolver._SYNTAX_ONLY_PASS_SPARQL)
        self.assertIsNotNone(TestTypeResolver._SYNTAX_ONLY_PASS_SPARQL11)
        self.assertIsNotNone(TestTypeResolver._EXECUTE_PASS_SPARQL11)
        self.assertIsNotNone(TestTypeResolver._SKIP_TEST)
        self.assertIsNotNone(TestTypeResolver._EXECUTE_XFAIL)

        # Test that they have correct structure
        for pattern in [
            TestTypeResolver._SYNTAX_ONLY_PASS_SPARQL,
            TestTypeResolver._SYNTAX_ONLY_PASS_SPARQL11,
            TestTypeResolver._EXECUTE_PASS_SPARQL11,
            TestTypeResolver._SKIP_TEST,
            TestTypeResolver._EXECUTE_XFAIL,
        ]:
            self.assertIsInstance(pattern, tuple)
            self.assertEqual(len(pattern), 3)
            self.assertIsInstance(pattern[0], bool)  # execute
            self.assertIsInstance(pattern[1], TestResult)  # expect
            self.assertIsInstance(pattern[2], str)  # language

    def test_configuration_constants_consistency(self):
        """Test that all configuration constants are consistent."""
        # Test that skipped types map to _SKIP_TEST behavior
        from sparql_test_framework.test_types import TestType

        for test_type in TestTypeResolver._SKIPPED_TEST_TYPES:
            behavior = TestTypeResolver.TEST_BEHAVIOR_MAP.get(test_type)
            self.assertEqual(behavior, TestTypeResolver._SKIP_TEST)


if __name__ == "__main__":
    unittest.main()

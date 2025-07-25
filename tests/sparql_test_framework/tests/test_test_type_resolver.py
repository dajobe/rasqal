#!/usr/bin/env python3
"""
Unit tests for TestTypeResolver class.

This module contains unit tests for the TestTypeResolver class from
sparql_test_framework.types.

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
from sparql_test_framework import TestTypeResolver, TestResult, TestType


class TestTestTypeResolver(SparqlTestFrameworkTestBase):
    """Test the TestTypeResolver class."""

    def setUp(self):
        """Set up test fixtures."""
        super().setUp()

    def test_resolve_test_behavior_empty_type(self):
        """Test resolve_test_behavior with empty query type."""
        result = TestTypeResolver.resolve_test_behavior("")
        self.assertEqual(result, (True, TestResult.PASSED, "sparql"))

    def test_resolve_test_behavior_none_type(self):
        """Test resolve_test_behavior with None query type."""
        result = TestTypeResolver.resolve_test_behavior(None)
        self.assertEqual(result, (True, TestResult.PASSED, "sparql"))

    def test_resolve_test_behavior_positive_syntax_test(self):
        """Test resolve_test_behavior with positive syntax test."""
        result = TestTypeResolver.resolve_test_behavior(
            TestType.POSITIVE_SYNTAX_TEST.value
        )
        self.assertEqual(result, (False, TestResult.PASSED, "sparql"))

    def test_resolve_test_behavior_positive_syntax_test_11(self):
        """Test resolve_test_behavior with SPARQL 1.1 positive syntax test."""
        result = TestTypeResolver.resolve_test_behavior(
            TestType.POSITIVE_SYNTAX_TEST_11.value
        )
        self.assertEqual(result, (False, TestResult.PASSED, "sparql11"))

    def test_resolve_test_behavior_negative_syntax_test(self):
        """Test resolve_test_behavior with negative syntax test."""
        result = TestTypeResolver.resolve_test_behavior(
            TestType.NEGATIVE_SYNTAX_TEST.value
        )
        self.assertEqual(result, (False, TestResult.FAILED, "sparql"))

    def test_resolve_test_behavior_negative_syntax_test_11(self):
        """Test resolve_test_behavior with SPARQL 1.1 negative syntax test."""
        result = TestTypeResolver.resolve_test_behavior(
            TestType.NEGATIVE_SYNTAX_TEST_11.value
        )
        self.assertEqual(result, (False, TestResult.FAILED, "sparql11"))

    def test_resolve_test_behavior_bad_syntax_test(self):
        """Test resolve_test_behavior with bad syntax test."""
        result = TestTypeResolver.resolve_test_behavior(TestType.BAD_SYNTAX_TEST.value)
        self.assertEqual(result, (False, TestResult.FAILED, "sparql"))

    def test_resolve_test_behavior_test_syntax(self):
        """Test resolve_test_behavior with generic syntax test."""
        result = TestTypeResolver.resolve_test_behavior(TestType.TEST_SYNTAX.value)
        self.assertEqual(result, (False, TestResult.PASSED, "sparql"))

    def test_resolve_test_behavior_update_evaluation_test(self):
        """Test resolve_test_behavior with unsupported update evaluation test."""
        result = TestTypeResolver.resolve_test_behavior(
            TestType.UPDATE_EVALUATION_TEST.value
        )
        self.assertEqual(result, (False, TestResult.SKIPPED, "sparql"))

    def test_resolve_test_behavior_protocol_test(self):
        """Test resolve_test_behavior with unsupported protocol test."""
        result = TestTypeResolver.resolve_test_behavior(TestType.PROTOCOL_TEST.value)
        self.assertEqual(result, (False, TestResult.SKIPPED, "sparql"))

    def test_resolve_test_behavior_query_evaluation_test(self):
        """Test resolve_test_behavior with SPARQL 1.1 query evaluation test."""
        result = TestTypeResolver.resolve_test_behavior(
            TestType.QUERY_EVALUATION_TEST.value
        )
        self.assertEqual(result, (True, TestResult.PASSED, "sparql11"))

    def test_resolve_test_behavior_positive_update_syntax_test_11(self):
        """Test resolve_test_behavior with SPARQL 1.1 positive update syntax test."""
        result = TestTypeResolver.resolve_test_behavior(
            TestType.POSITIVE_UPDATE_SYNTAX_TEST_11.value
        )
        self.assertEqual(result, (False, TestResult.PASSED, "sparql11"))

    def test_resolve_test_behavior_negative_update_syntax_test_11(self):
        """Test resolve_test_behavior with SPARQL 1.1 negative update syntax test."""
        result = TestTypeResolver.resolve_test_behavior(
            TestType.NEGATIVE_UPDATE_SYNTAX_TEST_11.value
        )
        self.assertEqual(result, (False, TestResult.FAILED, "sparql11"))

    def test_resolve_test_behavior_csv_result_format_test(self):
        """Test resolve_test_behavior with CSV result format test."""
        result = TestTypeResolver.resolve_test_behavior(
            TestType.CSV_RESULT_FORMAT_TEST.value
        )
        self.assertEqual(result, (True, TestResult.PASSED, "sparql"))

    def test_resolve_test_behavior_service_test(self):
        """Test resolve_test_behavior with service test."""
        result = TestTypeResolver.resolve_test_behavior(TestType.SERVICE_TEST.value)
        self.assertEqual(result, (True, TestResult.PASSED, "sparql11"))

    def test_resolve_test_behavior_unknown_type(self):
        """Test resolve_test_behavior with unknown test type."""
        result = TestTypeResolver.resolve_test_behavior("UnknownTestType")
        self.assertEqual(result, (True, TestResult.PASSED, "sparql"))

    def test_test_behavior_map_completeness(self):
        """Test that all TestType values have corresponding behavior mappings."""
        # Get all TestType enum values
        test_types = [t.value for t in TestType]

        # Check that all test types have behavior mappings
        for test_type in test_types:
            result = TestTypeResolver.resolve_test_behavior(test_type)
            self.assertIsInstance(result, tuple)
            self.assertEqual(len(result), 3)
            self.assertIsInstance(result[0], bool)  # should_execute
            self.assertIsInstance(result[1], TestResult)  # expected_result
            self.assertIsInstance(result[2], str)  # language

    def test_test_behavior_map_values_structure(self):
        """Test that behavior map values have correct structure."""
        # Test a few specific mappings to ensure structure
        test_cases = [
            (
                TestType.QUERY_EVALUATION_TEST.value,
                (True, TestResult.PASSED, "sparql11"),
            ),
            (TestType.POSITIVE_SYNTAX_TEST.value, (False, TestResult.PASSED, "sparql")),
            (TestType.NEGATIVE_SYNTAX_TEST.value, (False, TestResult.FAILED, "sparql")),
            (
                TestType.UPDATE_EVALUATION_TEST.value,
                (False, TestResult.SKIPPED, "sparql"),
            ),
        ]

        for test_type, expected in test_cases:
            result = TestTypeResolver.resolve_test_behavior(test_type)
            self.assertEqual(result, expected)


if __name__ == "__main__":
    unittest.main()

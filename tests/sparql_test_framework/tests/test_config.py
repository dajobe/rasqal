#!/usr/bin/env python3
"""
Unit tests for TestConfig class.

This module contains unit tests for the TestConfig class from
sparql_test_framework.config.

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
from sparql_test_framework import TestConfig, TestResult, TestType, Namespaces
from pathlib import Path


class TestTestConfig(SparqlTestFrameworkTestBase):
    """Test the TestConfig class."""

    def setUp(self):
        """Set up test fixtures."""
        super().setUp()

    def test_unsupported_test_types_constant(self):
        """Test that UNSUPPORTED_TEST_TYPES constant contains expected types."""
        expected_types = [
            TestType.UPDATE_EVALUATION_TEST.value,
            f"{Namespaces.MF}UpdateEvaluationTest",
            TestType.PROTOCOL_TEST.value,
        ]
        self.assertEqual(TestConfig.UNSUPPORTED_TEST_TYPES, expected_types)

    def test_get_skip_condition_withdrawn_test(self):
        """Test _get_skip_condition returns 'withdrawn' for withdrawn tests."""
        config = self.create_mock_test_config(is_withdrawn=True)
        result = config._get_skip_condition()
        self.assertEqual(result, "withdrawn")

    def test_get_skip_condition_not_approved(self):
        """Test _get_skip_condition returns 'not approved' when approved_only=True and test not approved."""
        config = self.create_mock_test_config(is_approved=False)
        result = config._get_skip_condition(approved_only=True)
        self.assertEqual(result, "not approved")

    def test_get_skip_condition_approved_test_with_approved_only(self):
        """Test _get_skip_condition passes approved test when approved_only=True."""
        config = self.create_mock_test_config(is_approved=True)
        result = config._get_skip_condition(approved_only=True)
        self.assertIsNone(result)

    def test_get_skip_condition_entailment_regime(self):
        """Test _get_skip_condition returns 'has entailment regime' for tests with entailment."""
        config = self.create_mock_test_config(has_entailment_regime=True)
        result = config._get_skip_condition()
        self.assertEqual(result, "has entailment regime")

    def test_get_skip_condition_unsupported_test_type(self):
        """Test _get_skip_condition returns 'unsupported test type' for unsupported types."""
        config = self.create_mock_test_config(
            test_type=TestType.UPDATE_EVALUATION_TEST.value
        )
        result = config._get_skip_condition()
        self.assertEqual(result, "unsupported test type")

    def test_get_skip_condition_protocol_test(self):
        """Test _get_skip_condition returns 'unsupported test type' for protocol tests."""
        config = self.create_mock_test_config(test_type=TestType.PROTOCOL_TEST.value)
        result = config._get_skip_condition()
        self.assertEqual(result, "unsupported test type")

    def test_get_skip_condition_supported_test(self):
        """Test _get_skip_condition returns None for supported tests."""
        config = self.create_mock_test_config(
            test_type=TestType.POSITIVE_SYNTAX_TEST.value,
            is_withdrawn=False,
            is_approved=True,
            has_entailment_regime=False,
        )
        result = config._get_skip_condition()
        self.assertIsNone(result)

    def test_should_run_test_withdrawn(self):
        """Test should_run_test returns False for withdrawn tests."""
        config = self.create_mock_test_config(is_withdrawn=True)
        result = config.should_run_test()
        self.assertFalse(result)

    def test_should_run_test_not_approved_with_approved_only(self):
        """Test should_run_test returns False when approved_only=True and test not approved."""
        config = self.create_mock_test_config(is_approved=False)
        result = config.should_run_test(approved_only=True)
        self.assertFalse(result)

    def test_should_run_test_approved_with_approved_only(self):
        """Test should_run_test returns True when approved_only=True and test is approved."""
        config = self.create_mock_test_config(is_approved=True)
        result = config.should_run_test(approved_only=True)
        self.assertTrue(result)

    def test_should_run_test_entailment_regime(self):
        """Test should_run_test returns False for tests with entailment regime."""
        config = self.create_mock_test_config(has_entailment_regime=True)
        result = config.should_run_test()
        self.assertFalse(result)

    def test_should_run_test_unsupported_type(self):
        """Test should_run_test returns False for unsupported test types."""
        config = self.create_mock_test_config(
            test_type=TestType.UPDATE_EVALUATION_TEST.value
        )
        result = config.should_run_test()
        self.assertFalse(result)

    def test_should_run_test_supported_test(self):
        """Test should_run_test returns True for supported tests."""
        config = self.create_mock_test_config(
            test_type=TestType.POSITIVE_SYNTAX_TEST.value,
            is_withdrawn=False,
            is_approved=True,
            has_entailment_regime=False,
        )
        result = config.should_run_test()
        self.assertTrue(result)

    def test_get_skip_reason_withdrawn(self):
        """Test get_skip_reason returns 'withdrawn' for withdrawn tests."""
        config = self.create_mock_test_config(is_withdrawn=True)
        result = config.get_skip_reason()
        self.assertEqual(result, "withdrawn")

    def test_get_skip_reason_not_approved(self):
        """Test get_skip_reason returns 'not approved' when approved_only=True and test not approved."""
        config = self.create_mock_test_config(is_approved=False)
        result = config.get_skip_reason(approved_only=True)
        self.assertEqual(result, "not approved")

    def test_get_skip_reason_entailment_regime(self):
        """Test get_skip_reason returns 'has entailment regime' for tests with entailment."""
        config = self.create_mock_test_config(has_entailment_regime=True)
        result = config.get_skip_reason()
        self.assertEqual(result, "has entailment regime")

    def test_get_skip_reason_unsupported_type(self):
        """Test get_skip_reason returns 'unsupported test type' for unsupported types."""
        config = self.create_mock_test_config(test_type=TestType.PROTOCOL_TEST.value)
        result = config.get_skip_reason()
        self.assertEqual(result, "unsupported test type")

    def test_get_skip_reason_supported_test(self):
        """Test get_skip_reason returns None for supported tests."""
        config = self.create_mock_test_config(
            test_type=TestType.QUERY_EVALUATION_TEST.value,
            is_withdrawn=False,
            is_approved=True,
            has_entailment_regime=False,
        )
        result = config.get_skip_reason()
        self.assertIsNone(result)

    def test_skip_condition_precedence_withdrawn_first(self):
        """Test that withdrawn status takes precedence over other skip conditions."""
        config = self.create_mock_test_config(
            is_withdrawn=True,
            is_approved=False,
            has_entailment_regime=True,
            test_type=TestType.UPDATE_EVALUATION_TEST.value,
        )
        result = config._get_skip_condition(approved_only=True)
        self.assertEqual(result, "withdrawn")

    def test_skip_condition_precedence_approval_second(self):
        """Test that approval status takes precedence over entailment and type."""
        config = self.create_mock_test_config(
            is_withdrawn=False,
            is_approved=False,
            has_entailment_regime=True,
            test_type=TestType.UPDATE_EVALUATION_TEST.value,
        )
        result = config._get_skip_condition(approved_only=True)
        self.assertEqual(result, "not approved")

    def test_skip_condition_precedence_entailment_third(self):
        """Test that entailment regime takes precedence over unsupported type."""
        config = self.create_mock_test_config(
            is_withdrawn=False,
            is_approved=True,
            has_entailment_regime=True,
            test_type=TestType.UPDATE_EVALUATION_TEST.value,
        )
        result = config._get_skip_condition(approved_only=True)
        self.assertEqual(result, "has entailment regime")

    def test_consistency_should_run_and_get_skip_reason(self):
        """Test that should_run_test and get_skip_reason are consistent."""
        test_cases = [
            {"is_withdrawn": True},
            {"is_approved": False, "approved_only": True},
            {"has_entailment_regime": True},
            {"test_type": TestType.UPDATE_EVALUATION_TEST.value},
            {"test_type": TestType.POSITIVE_SYNTAX_TEST.value, "is_approved": True},
        ]

        for case in test_cases:
            approved_only = case.pop("approved_only", False)
            config = self.create_mock_test_config(**case)

            should_run = config.should_run_test(approved_only)
            skip_reason = config.get_skip_reason(approved_only)

            # They should be consistent: if skip_reason is None, should_run should be True
            self.assertEqual(
                should_run,
                skip_reason is None,
                f"Inconsistency for case {case}: should_run={should_run}, skip_reason={skip_reason}",
            )


if __name__ == "__main__":
    unittest.main()

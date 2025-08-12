#!/usr/bin/env python3
"""
Unit tests for SparqlTestRunner.

This module contains unit tests for the SparqlTestRunner class
from sparql_test_framework.runners.sparql.

Copyright (C) 2025, David Beckett https://www.dajobe.org/

This package is Free Software and part of Redland http://librdf.org/
"""

import unittest
from pathlib import Path
from unittest.mock import patch, MagicMock, call
import argparse
import io

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))
from test_base import SparqlTestFrameworkTestBase
from sparql_test_framework.runners import SparqlTestRunner


class TestSparqlTestRunner(SparqlTestFrameworkTestBase):
    """Test the SparqlTestRunner class."""

    def test_sparql_runner_initialization(self):
        """Test SparqlTestRunner can be initialized."""
        runner = SparqlTestRunner()
        self.assertIsNotNone(runner)

    def test_sparql_runner_can_be_imported(self):
        """Test that SparqlTestRunner can be imported successfully."""
        # This is a basic smoke test to ensure the class is accessible
        self.assertTrue(hasattr(SparqlTestRunner, "__init__"))

    def test_use_rasqal_compare_flag_exists(self):
        """Test that --use-rasqal-compare flag is added to argument parser."""
        runner = SparqlTestRunner()
        parser = runner.setup_argument_parser()

        # Test that the flag exists
        args = parser.parse_args(
            ["--manifest-file", "test.ttl", "--use-rasqal-compare"]
        )
        self.assertTrue(args.use_rasqal_compare)

        # Test that it defaults to False
        args = parser.parse_args(["--manifest-file", "test.ttl"])
        self.assertFalse(args.use_rasqal_compare)

    def test_use_rasqal_compare_flag_help_text(self):
        """Test that --use-rasqal-compare flag appears in help text."""
        runner = SparqlTestRunner()
        parser = runner.setup_argument_parser()

        # Capture help output
        help_output = io.StringIO()
        parser.print_help(file=help_output)
        help_text = help_output.getvalue()

        self.assertIn("--use-rasqal-compare", help_text)
        self.assertIn("Use rasqal-compare utility", help_text)

    def test_use_rasqal_compare_flag_is_optional(self):
        """Test that --use-rasqal-compare flag is optional (no argument required)."""
        runner = SparqlTestRunner()
        parser = runner.setup_argument_parser()

        # Should not raise an error when flag is not provided
        args = parser.parse_args(["--manifest-file", "test.ttl"])
        self.assertFalse(args.use_rasqal_compare)

        # Should work when flag is provided
        args = parser.parse_args(
            ["--manifest-file", "test.ttl", "--use-rasqal-compare"]
        )
        self.assertTrue(args.use_rasqal_compare)


class TestRasqalCompareIntegration(SparqlTestFrameworkTestBase):
    """Test the rasqal-compare integration functionality."""

    def setUp(self):
        """Set up test fixtures."""
        super().setUp()
        self.runner = SparqlTestRunner()

    @patch("sparql_test_framework.runners.sparql.run_command")
    def test_compare_with_rasqal_compare_success(self, mock_run_command):
        """Test compare_with_rasqal_compare function with successful comparison."""
        from sparql_test_framework.runners.sparql import compare_with_rasqal_compare

        # Mock successful comparison (different results)
        mock_result = MagicMock()
        mock_result.returncode = 1
        mock_result.stdout = "Found 1 differences:\n  1: row 0: x='25' vs row 0: x='26'"
        mock_result.stderr = ""
        mock_run_command.return_value = mock_result

        # Test the function
        result = compare_with_rasqal_compare(
            Path("expected.xml"), Path("actual.xml"), Path("diff.out")
        )

        # Verify result
        self.assertEqual(result, 1)

        # Verify command was called correctly
        mock_run_command.assert_called_once()
        call_args = mock_run_command.call_args[0][0]
        self.assertIn("rasqal-compare", call_args[0])
        self.assertIn("-e", call_args)
        self.assertIn("-a", call_args)

    @patch("sparql_test_framework.runners.sparql.run_command")
    def test_compare_with_rasqal_compare_identical(self, mock_run_command):
        """Test compare_with_rasqal_compare function with identical results."""
        from sparql_test_framework.runners.sparql import compare_with_rasqal_compare

        # Mock identical comparison
        mock_result = MagicMock()
        mock_result.returncode = 0
        mock_result.stdout = "Results are identical"
        mock_result.stderr = ""
        mock_run_command.return_value = mock_result

        # Test the function
        result = compare_with_rasqal_compare(
            Path("expected.xml"), Path("actual.xml"), Path("diff.out")
        )

        # Verify result
        self.assertEqual(result, 0)

    @patch("sparql_test_framework.runners.sparql.run_command")
    def test_compare_with_rasqal_compare_unified_format(self, mock_run_command):
        """Test compare_with_rasqal_compare function with unified format."""
        from sparql_test_framework.runners.sparql import compare_with_rasqal_compare

        # Mock unified format output
        mock_result = MagicMock()
        mock_result.returncode = 1
        mock_result.stdout = "--- expected\n+++ actual\n@@ Comparison Results @@\n-row 0: x='25'\n+row 0: x='26'"
        mock_result.stderr = ""
        mock_run_command.return_value = mock_result

        # Test the function with unified format
        result = compare_with_rasqal_compare(
            Path("expected.xml"), Path("actual.xml"), Path("diff.out"), "unified"
        )

        # Verify result
        self.assertEqual(result, 1)

        # Verify unified flag was added
        call_args = mock_run_command.call_args[0][0]
        self.assertIn("-u", call_args)

    @patch("sparql_test_framework.runners.sparql.run_command")
    def test_compare_with_rasqal_compare_debug_format(self, mock_run_command):
        """Test compare_with_rasqal_compare function with debug format."""
        from sparql_test_framework.runners.sparql import compare_with_rasqal_compare

        # Mock debug format output
        mock_result = MagicMock()
        mock_result.returncode = 1
        mock_result.stdout = "comparison result: different\ndifferences count: 1"
        mock_result.stderr = ""
        mock_run_command.return_value = mock_result

        # Test the function with debug format
        result = compare_with_rasqal_compare(
            Path("expected.xml"), Path("actual.xml"), Path("diff.out"), "debug"
        )

        # Verify result
        self.assertEqual(result, 1)

        # Verify debug flag was added
        call_args = mock_run_command.call_args[0][0]
        self.assertIn("-k", call_args)

    @patch("sparql_test_framework.runners.sparql.run_command")
    def test_compare_with_rasqal_compare_error(self, mock_run_command):
        """Test compare_with_rasqal_compare function with error."""
        from sparql_test_framework.runners.sparql import compare_with_rasqal_compare

        # Mock error
        mock_run_command.side_effect = Exception("rasqal-compare not found")

        # Test the function
        result = compare_with_rasqal_compare(
            Path("expected.xml"), Path("actual.xml"), Path("diff.out")
        )

        # Verify error handling
        self.assertEqual(result, 2)

    @patch("sparql_test_framework.runners.sparql.run_command")
    @patch("sparql_test_framework.runners.sparql.DIFF_OUT")
    def test_compare_actual_vs_expected_with_rasqal_compare(
        self, mock_diff_out, mock_run_command
    ):
        """Test _compare_actual_vs_expected function with rasqal-compare enabled."""
        from sparql_test_framework.runners.sparql import _compare_actual_vs_expected

        # Mock system diff result (bindings comparisons always use system diff)
        mock_result = MagicMock()
        mock_result.returncode = 0  # No differences
        mock_result.stdout = ""
        mock_result.stderr = ""
        mock_run_command.return_value = mock_result

        # Mock DIFF_OUT file operations
        mock_diff_out.exists.return_value = False  # No diff file when no differences

        # Create test data
        test_result_summary = {"name": "test", "result": "failure"}
        actual_result_info = {
            "result_type": "bindings",
            "content": "test content",
            "roqet_results_count": 1,
            "vars_order": ["x"],
            "is_sorted_by_query": False,
        }
        expected_result_file = Path("expected.xml")

        # Mock file operations
        with patch("builtins.open", create=True):
            with patch("pathlib.Path.exists", return_value=True):
                with patch("pathlib.Path.stat") as mock_stat:
                    mock_stat.return_value.st_size = 100
                    with patch(
                        "sparql_test_framework.runners.sparql.read_query_results_file"
                    ) as mock_read:
                        mock_read.return_value = {"count": 1, "type": "bindings"}

                        # Test the function with rasqal-compare enabled
                        _compare_actual_vs_expected(
                            test_result_summary,
                            actual_result_info,
                            expected_result_file,
                            "strict",
                            0,
                            use_rasqal_compare=True,
                        )

                        # Verify system diff was called (bindings always use system diff)
                        mock_run_command.assert_called_once()
                        call_args = mock_run_command.call_args[0][0]
                        self.assertIn("diff", call_args[0])

                        # Verify test result was updated to success
                        self.assertEqual(test_result_summary["result"], "success")

    @patch("sparql_test_framework.runners.sparql.run_command")
    @patch("sparql_test_framework.runners.sparql.DIFF_OUT")
    def test_compare_actual_vs_expected_without_rasqal_compare(
        self, mock_diff_out, mock_run_command
    ):
        """Test _compare_actual_vs_expected function without rasqal-compare (uses system diff)."""
        from sparql_test_framework.runners.sparql import _compare_actual_vs_expected

        # Mock system diff result
        mock_result = MagicMock()
        mock_result.returncode = 1
        mock_result.stdout = "diff output"
        mock_result.stderr = ""
        mock_run_command.return_value = mock_result

        # Mock DIFF_OUT file operations
        mock_diff_out.exists.return_value = True
        mock_diff_out.read_text.return_value = "diff content"

        # Create test data
        test_result_summary = {"name": "test", "result": "failure"}
        actual_result_info = {
            "result_type": "bindings",
            "content": "test content",
            "roqet_results_count": 1,
            "vars_order": ["x"],
            "is_sorted_by_query": False,
        }
        expected_result_file = Path("expected.xml")

        # Mock file operations
        with patch("builtins.open", create=True):
            with patch("pathlib.Path.exists", return_value=True):
                with patch("pathlib.Path.stat") as mock_stat:
                    mock_stat.return_value.st_size = 100
                    with patch(
                        "sparql_test_framework.runners.sparql.read_query_results_file"
                    ) as mock_read:
                        mock_read.return_value = {"count": 1, "type": "bindings"}

                        # Test the function without rasqal-compare
                        _compare_actual_vs_expected(
                            test_result_summary,
                            actual_result_info,
                            expected_result_file,
                            "strict",
                            0,
                            use_rasqal_compare=False,
                        )

                        # Verify system diff was called
                        mock_run_command.assert_called_once()
                        call_args = mock_run_command.call_args[0][0]
                        self.assertIn("diff", call_args[0])

    @patch("sparql_test_framework.runners.sparql._compare_actual_vs_expected")
    @patch("sparql_test_framework.runners.sparql._execute_roqet")
    def test_run_single_test_passes_use_rasqal_compare(
        self, mock_execute_roqet, mock_compare
    ):
        """Test that run_single_test passes use_rasqal_compare parameter correctly."""
        from sparql_test_framework.runners.sparql import run_single_test
        from sparql_test_framework import TestConfig, TestResult, TestType

        # Create test config
        config = self.create_mock_test_config(
            name="test",
            expect=TestResult.PASSED,
            test_type=TestType.QUERY_EVALUATION_TEST.value,
            result_file=Path("expected.xml"),
        )

        # Mock roqet execution
        mock_execute_roqet.return_value = {
            "returncode": 0,
            "stdout": "test output",
            "stderr": "",
            "elapsed_time": 1.0,
            "query_cmd": "roqet test.rq",
        }

        # Mock comparison
        mock_compare.return_value = None

        # Test with rasqal-compare enabled
        run_single_test(config, 0, use_rasqal_compare=True)

        # Verify comparison was called with correct parameter
        mock_compare.assert_called_once()
        call_args = mock_compare.call_args[0]
        self.assertTrue(call_args[5])  # use_rasqal_compare parameter

    def test_rasqal_compare_tool_constant(self):
        """Test that RASQAL_COMPARE constant is defined."""
        from sparql_test_framework.runners.sparql import RASQAL_COMPARE

        self.assertIsNotNone(RASQAL_COMPARE)
        self.assertIsInstance(RASQAL_COMPARE, str)

    def test_diff_cmd_tool_constant(self):
        """Test that DIFF_CMD constant is defined."""
        from sparql_test_framework.runners.sparql import DIFF_CMD

        self.assertIsNotNone(DIFF_CMD)
        self.assertIsInstance(DIFF_CMD, str)


if __name__ == "__main__":
    unittest.main()

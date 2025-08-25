#!/usr/bin/env python3
"""
Unit tests for SparqlTestRunner.

This module contains unit tests for the SparqlTestRunner class
from sparql_test_framework.runners.sparql.

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

        # Mock successful comparison
        mock_run_command.return_value = (
            1,
            "Found 1 differences:\n  1: row 0: x='25' vs row 0: x='26'",
            "",
        )

        # Test the function
        result = compare_with_rasqal_compare(
            Path("expected.xml"), Path("actual.xml"), Path("diff.out")
        )

        # Verify result
        self.assertEqual(result, 1)

        # Verify command was called correctly
        mock_run_command.assert_called_once()
        call_args = mock_run_command.call_args
        cmd = call_args.kwargs.get("cmd", [])
        self.assertIn("rasqal-compare", cmd[0])
        self.assertIn("-e", cmd)
        self.assertIn("-a", cmd)

    @patch("sparql_test_framework.runners.sparql.run_command")
    def test_compare_with_rasqal_compare_identical(self, mock_run_command):
        """Test compare_with_rasqal_compare function with identical results."""
        from sparql_test_framework.runners.sparql import compare_with_rasqal_compare

        # Mock identical comparison
        mock_run_command.return_value = (0, "Results are identical", "")

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
        mock_run_command.return_value = (
            1,
            "--- expected\n+++ actual\n@@ Comparison Results @@\n-row 0: x='25'\n+row 0: x='26'",
            "",
        )

        # Test the function with unified format
        result = compare_with_rasqal_compare(
            Path("expected.xml"), Path("actual.xml"), Path("diff.out"), "unified"
        )

        # Verify result
        self.assertEqual(result, 1)

        # Verify unified flag was added
        call_args = mock_run_command.call_args
        cmd = call_args.kwargs.get("cmd", [])
        self.assertIn("-u", cmd)

    @patch("sparql_test_framework.runners.sparql.run_command")
    def test_compare_with_rasqal_compare_debug_format(self, mock_run_command):
        """Test compare_with_rasqal_compare function with debug format."""
        from sparql_test_framework.runners.sparql import compare_with_rasqal_compare

        # Mock debug format output
        mock_run_command.return_value = (
            1,
            "comparison result: different\ndifferences count: 1",
            "",
        )

        # Test the function with debug format
        result = compare_with_rasqal_compare(
            Path("expected.xml"), Path("actual.xml"), Path("diff.out"), "debug"
        )

        # Verify result
        self.assertEqual(result, 1)

        # Verify debug flag was added
        call_args = mock_run_command.call_args
        cmd = call_args.kwargs.get("cmd", [])
        self.assertIn("-k", cmd)

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
    def test_compare_bindings_results_directly(self, mock_run_command):
        """Test _compare_bindings_results function directly."""
        from sparql_test_framework.runners.sparql import _compare_bindings_results

        # Mock system diff result
        mock_run_command.return_value = (1, "diff output", "")

        # Create test data
        actual_result_info = {
            "result_type": "bindings",
            "content": "row: [x=uri<http://example.org/data/y>]\n",
            "roqet_results_count": 1,
            "vars_order": ["x"],
            "is_sorted_by_query": False,
        }

        # Mock the temporary file paths that the comparison logic needs
        with patch(
            "sparql_test_framework.runners.sparql.get_temp_file_path"
        ) as mock_temp_path:
            # Also mock DIFF_CMD to ensure it's what we expect
            with patch("sparql_test_framework.runners.sparql.DIFF_CMD", "diff"):
                # Create actual temporary files that the comparison logic needs
                temp_files = {}

                def temp_path_side_effect(name):
                    if name not in temp_files:
                        temp_files[name] = Path(f"/tmp/{name}")
                        # Create the file with some content
                        if name == "result.out":
                            temp_files[name].write_text(
                                "row: [x=uri<http://example.org/data/x>]\n"
                            )
                        elif name == "roqet.tmp":
                            temp_files[name].write_text(
                                "row: [x=uri<http://example.org/data/y>]\n"
                            )  # Different content to trigger diff
                        elif name == "diff.out":
                            temp_files[name].write_text("")
                    return temp_files[name]

                mock_temp_path.side_effect = temp_path_side_effect

                # Test the function directly
                print(f"DEBUG: About to call _compare_bindings_results")
                print(f"DEBUG: actual_result_info = {actual_result_info}")
                try:
                    result = _compare_bindings_results(actual_result_info, "test")
                    print(f"DEBUG: _compare_bindings_results returned: {result}")
                except Exception as e:
                    print(f"DEBUG: _compare_bindings_results threw exception: {e}")
                    import traceback

                    traceback.print_exc()
                    raise

                # Check if the mock was called
                print(
                    f"DEBUG: mock_run_command.call_count = {mock_run_command.call_count}"
                )
                print(
                    f"DEBUG: mock_run_command.call_args = {mock_run_command.call_args}"
                )
                print(
                    f"DEBUG: mock_run_command.call_args_list = {mock_run_command.call_args_list}"
                )

                # Verify result
                self.assertEqual(result, 1)

                # Verify run_command was called
                mock_run_command.assert_called_once()
                call_args = mock_run_command.call_args
                cmd = call_args.kwargs.get("cmd", [])
                self.assertIn(
                    "diff", cmd[0]
                )  # Check that first command contains "diff"

    @patch("sparql_test_framework.runners.sparql.run_command")
    @patch("sparql_test_framework.runners.sparql.DIFF_OUT")
    def test_compare_actual_vs_expected_without_rasqal_compare(
        self, mock_diff_out, mock_run_command
    ):
        """Test _compare_actual_vs_expected function without rasqal-compare (uses system diff)."""
        from sparql_test_framework.runners.sparql import _compare_actual_vs_expected

        # Mock system diff result
        mock_run_command.return_value = (1, "diff output", "")

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

        # Mock file operations and temporary file paths
        with patch("builtins.open", create=True):
            with patch("pathlib.Path.exists", return_value=True):
                with patch("pathlib.Path.stat") as mock_stat:
                    mock_stat.return_value.st_size = 100
                    with patch(
                        "sparql_test_framework.runners.sparql.read_query_results_file"
                    ) as mock_read:
                        mock_read.return_value = {"count": 1, "type": "bindings"}

                        # Mock the temporary file paths that the comparison logic needs
                        with patch(
                            "sparql_test_framework.runners.sparql.get_temp_file_path"
                        ) as mock_temp_path:
                            # Create actual temporary files that the comparison logic needs
                            temp_files = {}

                            def temp_path_side_effect(name):
                                if name not in temp_files:
                                    temp_files[name] = Path(f"/tmp/{name}")
                                    # Create the file with some content
                                    if name == "result.out":
                                        temp_files[name].write_text(
                                            "row: [x=uri<http://example.org/data/x>]\n"
                                        )
                                    elif name == "roqet.tmp":
                                        temp_files[name].write_text(
                                            "row: [x=uri<http://example.org/data/y>]\n"
                                        )  # Different content to trigger diff
                                    elif name == "diff.out":
                                        temp_files[name].write_text(
                                            "--- result.out\n+++ roqet.tmp\n@@ -1 +1 @@\n-row: [x=uri<http://example.org/data/x>]\n+row: [x=uri<http://example.org/data/y>]\n"
                                        )
                                return temp_files[name]

                            mock_temp_path.side_effect = temp_path_side_effect

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
                            call_args = mock_run_command.call_args
                            cmd = call_args.kwargs.get("cmd", [])
                            self.assertIn("diff", cmd[0])

    @patch("sparql_test_framework.runners.sparql._compare_actual_vs_expected")
    @patch("sparql_test_framework.runners.sparql._build_actual_from_srx")
    @patch("sparql_test_framework.runners.sparql._execute_roqet")
    def test_run_single_test_passes_use_rasqal_compare(
        self, mock_execute_roqet, mock_build_actual, mock_compare
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

        # Mock building actual results
        mock_build_actual.return_value = {
            "result_type": "bindings",
            "roqet_results_count": 1,
            "vars_order": ["x"],
            "is_sorted_by_query": False,
            "boolean_value": None,
            "content": "test content",
            "count": 1,
            "format": "bindings",
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

    @patch("sparql_test_framework.runners.sparql._build_actual_from_srx")
    @patch("sparql_test_framework.runners.sparql._execute_roqet")
    def test_run_single_test_syntax_test_handling(
        self, mock_execute_roqet, mock_build_actual
    ):
        """Test that syntax tests are handled correctly without building actual results."""
        from sparql_test_framework.runners.sparql import run_single_test
        from sparql_test_framework import TestConfig, TestResult

        # Create a syntax test config (similar to the failing tests)
        config = self.create_mock_test_config(
            name="syntax-form-construct05.rq",
            expect=TestResult.PASSED,
            test_type="http://jena.hpl.hp.com/2005/05/test-manifest-extra#TestSyntax",
            result_file=None,  # Syntax tests typically don't have result files
        )

        # Mock successful roqet execution (syntax test passed)
        mock_execute_roqet.return_value = {
            "returncode": 0,
            "stdout": "test output",
            "stderr": "",
            "elapsed_time": 1.0,
            "query_cmd": "roqet test.rq",
        }

        # Test that syntax test is handled correctly
        result = run_single_test(config, 0, use_rasqal_compare=False)

        # Verify the test was marked as successful
        self.assertEqual(result["result"], "success")
        self.assertTrue(result["is_success"])
        self.assertEqual(result["roqet-status-code"], 0)

        # Verify that _build_actual_from_srx was NOT called for syntax tests
        # This is the key assertion that would catch the regression
        mock_build_actual.assert_not_called()

    @patch("sparql_test_framework.runners.sparql._build_actual_from_srx")
    @patch("sparql_test_framework.runners.sparql._execute_roqet")
    def test_run_single_test_evaluation_test_handling(
        self, mock_execute_roqet, mock_build_actual
    ):
        """Test that evaluation tests still go through full result comparison."""
        from sparql_test_framework.runners.sparql import run_single_test
        from sparql_test_framework import TestConfig, TestResult

        # Create an evaluation test config
        config = self.create_mock_test_config(
            name="evaluation-test",
            expect=TestResult.PASSED,
            test_type="http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#QueryEvaluationTest",
            result_file=Path("expected.xml"),
        )

        # Mock successful roqet execution
        mock_execute_roqet.return_value = {
            "returncode": 0,
            "stdout": "test output",
            "stderr": "",
            "elapsed_time": 1.0,
            "query_cmd": "roqet test.rq",
        }

        # Mock building actual results
        mock_build_actual.return_value = {
            "result_type": "bindings",
            "roqet_results_count": 1,
            "vars_order": ["x"],
            "is_sorted_by_query": False,
            "boolean_value": None,
            "content": "test content",
            "count": 1,
            "format": "bindings",
        }

        # Test that evaluation test goes through full comparison
        result = run_single_test(config, 0, use_rasqal_compare=False)

        # Verify that _build_actual_from_srx WAS called for evaluation tests
        mock_build_actual.assert_called_once()

        # Verify the test result structure
        self.assertEqual(result["roqet-status-code"], 0)

    @patch("sparql_test_framework.runners.sparql._build_actual_from_srx")
    @patch("sparql_test_framework.runners.sparql._execute_roqet")
    def test_run_single_test_warning_test_handling(
        self, mock_execute_roqet, mock_build_actual
    ):
        """Test that warning tests are handled correctly with exit code 2."""
        from sparql_test_framework.runners.sparql import run_single_test
        from sparql_test_framework import TestConfig, TestResult

        # Create a warning test config (similar to the failing tests)
        config = self.create_mock_test_config(
            name="warning-test",
            expect=TestResult.PASSED,
            test_type="http://ns.librdf.org/2009/test-manifest#WarningTest",
            result_file=None,  # Warning tests don't need expected results
        )

        # Mock roqet execution with exit code 2 (warnings generated)
        mock_execute_roqet.return_value = {
            "returncode": 2,  # Exit code 2 indicates warnings
            "stdout": "",  # No results (empty bindings)
            "stderr": "Warning - Variable a was used but is not bound",
            "elapsed_time": 1.0,
            "query_cmd": "roqet test.rq -W 100",
        }

        # Mock building actual results (empty bindings for warning tests)
        mock_build_actual.return_value = {
            "result_type": "bindings",
            "roqet_results_count": 0,
            "vars_order": ["a"],
            "is_sorted_by_query": False,
            "boolean_value": None,
            "content": "",
            "count": 0,
            "format": "bindings",
        }

        # Test that warning test is handled correctly
        result = run_single_test(config, 0, use_rasqal_compare=False)

        # Verify the test was marked as successful (exit code 2 is success for warning tests)
        self.assertEqual(result["result"], "success")
        self.assertTrue(result["is_success"])
        self.assertEqual(result["roqet-status-code"], 2)

        # For warning tests with no result file, _build_actual_from_srx is not called
        # because the test succeeds immediately when roqet returns exit code 2
        mock_build_actual.assert_not_called()

    @patch("sparql_test_framework.runners.sparql._build_actual_from_srx")
    @patch("sparql_test_framework.runners.sparql._execute_roqet")
    def test_run_single_test_warning_test_failure_with_exit_code_0(
        self, mock_execute_roqet, mock_build_actual
    ):
        """Test that warning tests fail when they get exit code 0 instead of 2."""
        from sparql_test_framework.runners.sparql import run_single_test
        from sparql_test_framework import TestConfig, TestResult

        # Create a warning test config
        config = self.create_mock_test_config(
            name="warning-test",
            expect=TestResult.PASSED,
            test_type="http://ns.librdf.org/2009/test-manifest#WarningTest",
            result_file=None,  # Warning tests don't need expected results
        )

        # Mock roqet execution with exit code 0 (no warnings - this should fail for warning tests)
        mock_execute_roqet.return_value = {
            "returncode": 0,  # Exit code 0 means no warnings (failure for warning tests)
            "stdout": "test output",
            "stderr": "",
            "elapsed_time": 1.0,
            "query_cmd": "roqet test.rq -W 0",
        }

        # Test that warning test fails when it gets exit code 0
        result = run_single_test(config, 0, use_rasqal_compare=False)

        # Verify the test was marked as failure (exit code 0 is failure for warning tests)
        self.assertEqual(result["result"], "failure")
        self.assertFalse(result["is_success"])
        self.assertEqual(result["roqet-status-code"], 0)

        # Verify that _build_actual_from_srx was NOT called (test failed early)
        mock_build_actual.assert_not_called()


if __name__ == "__main__":
    unittest.main()

"""
Unit tests for comparison and debug classes: ResultComparer and DebugOutputManager.

This module tests the comparison and debug output management classes
that handle result comparison logic and debug output presentation.

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

import os
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, patch, MagicMock

# Add the parent directory to the path for imports
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(__file__))))

from sparql_test_framework.runners.result_comparer import (
    ResultComparer,
    ComparisonResult,
)
from sparql_test_framework.runners.debug_output_manager import DebugOutputManager
from sparql_test_framework.config import TestConfig


class TestComparisonResult(unittest.TestCase):
    """Test ComparisonResult class."""

    def test_comparison_result_creation(self):
        """Test ComparisonResult object creation."""
        result = ComparisonResult(
            is_match=True, diff_output="No differences", comparison_method="textual"
        )

        self.assertTrue(result.is_match)
        self.assertEqual(result.diff_output, "No differences")
        self.assertEqual(result.comparison_method, "textual")
        self.assertIsNone(result.error_message)

    def test_comparison_result_with_error(self):
        """Test ComparisonResult with error message."""
        result = ComparisonResult(
            is_match=False, error_message="Comparison failed", comparison_method="error"
        )

        self.assertFalse(result.is_match)
        self.assertEqual(result.error_message, "Comparison failed")
        self.assertEqual(result.comparison_method, "error")
        self.assertIsNone(result.diff_output)


class TestResultComparer(unittest.TestCase):
    """Test ResultComparer class."""

    def setUp(self):
        """Set up test fixtures."""
        self.comparer = ResultComparer(use_rasqal_compare=False)
        self.mock_config = Mock(spec=TestConfig)

        # Mock temp file creation
        self.temp_dir = tempfile.mkdtemp()
        self.mock_config.get_temp_file = Mock(
            side_effect=lambda name: Path(self.temp_dir) / f"{name}.tmp"
        )

    def tearDown(self):
        """Clean up test fixtures."""
        import shutil

        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_init_without_rasqal_compare(self):
        """Test initialization without rasqal-compare."""
        comparer = ResultComparer(use_rasqal_compare=False)
        self.assertFalse(comparer.use_rasqal_compare)
        self.assertIsNone(comparer.rasqal_compare_path)

    @patch("sparql_test_framework.runners.result_comparer.find_tool")
    def test_init_with_rasqal_compare_success(self, mock_find_tool):
        """Test initialization with rasqal-compare available."""
        mock_find_tool.return_value = "/usr/bin/rasqal-compare"

        comparer = ResultComparer(use_rasqal_compare=True)
        self.assertTrue(comparer.use_rasqal_compare)
        self.assertEqual(comparer.rasqal_compare_path, "/usr/bin/rasqal-compare")

    @patch("sparql_test_framework.runners.result_comparer.find_tool")
    def test_init_with_rasqal_compare_not_found(self, mock_find_tool):
        """Test initialization with rasqal-compare not found."""
        mock_find_tool.return_value = None

        with self.assertRaises(RuntimeError):
            ResultComparer(use_rasqal_compare=True)

    def test_compare_results_unknown_type(self):
        """Test comparison with unknown result type."""
        result = self.comparer.compare_results(
            "actual", "expected", "unknown", self.mock_config
        )

        self.assertFalse(result.is_match)
        self.assertEqual(result.comparison_method, "unknown")
        self.assertIn("Unknown result type", result.error_message)

    def test_compare_boolean_results_match(self):
        """Test boolean comparison with matching values."""
        result = self.comparer.compare_boolean_results(True, True)

        self.assertTrue(result.is_match)
        self.assertEqual(result.comparison_method, "boolean")
        self.assertIsNone(result.diff_output)

    def test_compare_boolean_results_mismatch(self):
        """Test boolean comparison with mismatched values."""
        result = self.comparer.compare_boolean_results(True, False)

        self.assertFalse(result.is_match)
        self.assertEqual(result.comparison_method, "boolean")
        self.assertIn("Expected: False, Got: True", result.diff_output)

    def test_compare_graph_results_textual_match(self):
        """Test graph comparison with matching content."""
        actual = "_:b1 <p> <o> .\n_:b2 <p> <o> ."
        expected = "_:b2 <p> <o> .\n_:b1 <p> <o> ."

        result = self.comparer.compare_graph_results(actual, expected, self.mock_config)

        self.assertTrue(result.is_match)
        self.assertEqual(result.comparison_method, "textual")

    def test_compare_graph_results_textual_mismatch(self):
        """Test graph comparison with mismatched content."""
        actual = "_:b1 <p> <o> .\n_:b2 <p> <o> ."
        expected = "_:b1 <p> <o> .\n_:b3 <p> <o> ."

        result = self.comparer.compare_graph_results(actual, expected, self.mock_config)

        self.assertFalse(result.is_match)
        self.assertEqual(result.comparison_method, "textual")
        self.assertIn("=== ACTUAL ===", result.diff_output)
        self.assertIn("=== EXPECTED ===", result.diff_output)

    def test_compare_bindings_results_match(self):
        """Test bindings comparison with matching content."""
        actual = "s,p,o\n1,2,3\n4,5,6"
        expected = "s,p,o\n4,5,6\n1,2,3"

        result = self.comparer.compare_bindings_results(
            actual, expected, self.mock_config
        )

        self.assertTrue(result.is_match)
        self.assertEqual(result.comparison_method, "bindings")

    def test_compare_bindings_results_mismatch(self):
        """Test bindings comparison with mismatched content."""
        actual = "s,p,o\n1,2,3\n4,5,6"
        expected = "s,p,o\n1,2,3\n7,8,9"

        result = self.comparer.compare_bindings_results(
            actual, expected, self.mock_config
        )

        self.assertFalse(result.is_match)
        self.assertEqual(result.comparison_method, "bindings")
        self.assertIn("=== ACTUAL BINDINGS ===", result.diff_output)
        self.assertIn("=== EXPECTED BINDINGS ===", result.diff_output)

    def test_compare_srj_results_match(self):
        """Test SRJ comparison with matching content."""
        actual = '{"head": {"vars": ["s"]}, "results": {"bindings": [{"s": "1"}]}}'
        expected = '{"head": {"vars": ["s"]}, "results": {"bindings": [{"s": "1"}]}}'

        result = self.comparer.compare_srj_results(actual, expected, self.mock_config)

        self.assertTrue(result.is_match)
        self.assertEqual(result.comparison_method, "srj")

    def test_compare_srj_results_mismatch(self):
        """Test SRJ comparison with mismatched content."""
        actual = '{"head": {"vars": ["s"]}, "results": {"bindings": [{"s": "1"}]}}'
        expected = '{"head": {"vars": ["s"]}, "results": {"bindings": [{"s": "2"}]}}'

        result = self.comparer.compare_srj_results(actual, expected, self.mock_config)

        self.assertFalse(result.is_match)
        self.assertEqual(result.comparison_method, "srj")
        self.assertIn("=== ACTUAL SRJ ===", result.diff_output)
        self.assertIn("=== EXPECTED SRJ ===", result.diff_output)

    def test_normalize_graph_content(self):
        """Test graph content normalization."""
        content = "  _:b1 <p> <o> .  \n\n_:b2 <p> <o> .  "
        normalized = self.comparer._normalize_graph_content(content)

        # Should be sorted and whitespace normalized
        expected = "_:b1 <p> <o> .\n_:b2 <p> <o> ."
        self.assertEqual(normalized, expected)

    def test_normalize_bindings_content(self):
        """Test bindings content normalization."""
        content = "  s,p,o  \n\n  1,2,3  \n  4,5,6  "
        normalized = self.comparer._normalize_bindings_content(content)

        # Should be sorted and whitespace normalized
        expected = "1,2,3\n4,5,6\ns,p,o"
        self.assertEqual(normalized, expected)

    def test_normalize_srj_content(self):
        """Test SRJ content normalization."""
        content = '  {"head": {"vars": ["s"]}}  \n\n  {"results": {"bindings": []}}  '
        normalized = self.comparer._normalize_srj_content(content)

        # Should be sorted and whitespace normalized
        expected = '{"head": {"vars": ["s"]}}\n{"results": {"bindings": []}}'
        self.assertEqual(normalized, expected)


class TestDebugOutputManager(unittest.TestCase):
    """Test DebugOutputManager class."""

    def setUp(self):
        """Set up test fixtures."""
        self.debug_manager = DebugOutputManager(debug_level=2)
        self.mock_config = Mock(spec=TestConfig)
        self.mock_config.query_file = Path("test.rq")
        self.mock_config.data_file = Path("test.ttl")
        self.mock_config.expected_result_file = Path("expected.ttl")
        self.mock_config.result_format = "ntriples"
        self.mock_config.debug_level = 2

    def test_init_with_debug_level(self):
        """Test initialization with debug level."""
        manager = DebugOutputManager(debug_level=3)
        self.assertEqual(manager.debug_level, 3)

    def test_set_debug_level(self):
        """Test setting debug level."""
        self.debug_manager.set_debug_level(1)
        self.assertEqual(self.debug_manager.debug_level, 1)

    def test_show_success_debug_info_level_0(self):
        """Test success debug info with level 0 (no output)."""
        manager = DebugOutputManager(debug_level=0)

        # Should not raise any exceptions
        manager.show_success_debug_info("test_name", "result", "graph")

    def test_show_success_debug_info_level_1(self):
        """Test success debug info with level 1."""
        manager = DebugOutputManager(debug_level=1)

        # Should not raise any exceptions
        manager.show_success_debug_info("test_name", "result", "graph")

    def test_show_success_debug_info_level_2(self):
        """Test success debug info with level 2."""
        manager = DebugOutputManager(debug_level=2)

        # Should not raise any exceptions
        manager.show_success_debug_info("test_name", "result", "graph")

    def test_show_failure_debug_info_level_0(self):
        """Test failure debug info with level 0 (no output)."""
        manager = DebugOutputManager(debug_level=0)

        # Should not raise any exceptions
        manager.show_failure_debug_info("test_name", "actual", "expected", "graph")

    def test_show_failure_debug_info_level_1(self):
        """Test failure debug info with level 1."""
        manager = DebugOutputManager(debug_level=1)

        # Should not raise any exceptions
        manager.show_failure_debug_info("test_name", "actual", "expected", "graph")

    def test_show_failure_debug_info_level_2(self):
        """Test failure debug info with level 2."""
        manager = DebugOutputManager(debug_level=2)

        # Should not raise any exceptions
        manager.show_failure_debug_info("test_name", "actual", "expected", "graph")

    def test_log_test_start_level_0(self):
        """Test test start logging with level 0 (no output)."""
        manager = DebugOutputManager(debug_level=0)

        # Should not raise any exceptions
        manager.log_test_start("test_name", "graph")

    def test_log_test_start_level_1(self):
        """Test test start logging with level 1."""
        manager = DebugOutputManager(debug_level=1)

        # Should not raise any exceptions
        manager.log_test_start("test_name", "graph")

    def test_log_test_end_level_0(self):
        """Test test end logging with level 0 (no output)."""
        manager = DebugOutputManager(debug_level=0)

        # Should not raise any exceptions
        manager.log_test_end("test_name", True)
        manager.log_test_end("test_name", False)

    def test_log_test_end_level_1(self):
        """Test test end logging with level 1."""
        manager = DebugOutputManager(debug_level=1)

        # Should not raise any exceptions
        manager.log_test_end("test_name", True)
        manager.log_test_end("test_name", False, 1.5)

    def test_log_configuration_info_level_1(self):
        """Test configuration logging with level 1 (no output)."""
        manager = DebugOutputManager(debug_level=1)

        # Should not raise any exceptions
        manager.log_configuration_info(self.mock_config)

    def test_log_configuration_info_level_2(self):
        """Test configuration logging with level 2."""
        manager = DebugOutputManager(debug_level=2)

        # Should not raise any exceptions
        manager.log_configuration_info(self.mock_config)

    def test_log_execution_details_level_2(self):
        """Test execution details logging with level 2 (no output)."""
        manager = DebugOutputManager(debug_level=2)

        # Should not raise any exceptions
        manager.log_execution_details(["roqet", "-q", "test.rq"], 0, "output", "error")

    def test_log_execution_details_level_3(self):
        """Test execution details logging with level 3."""
        manager = DebugOutputManager(debug_level=3)

        # Should not raise any exceptions
        manager.log_execution_details(["roqet", "-q", "test.rq"], 0, "output", "error")

    def test_show_summary_level_0(self):
        """Test summary display with level 0 (no output)."""
        manager = DebugOutputManager(debug_level=0)

        # Should not raise any exceptions
        manager.show_summary(10, 8, 2)

    def test_show_summary_level_1(self):
        """Test summary display with level 1."""
        manager = DebugOutputManager(debug_level=1)

        # Should not raise any exceptions
        manager.show_summary(10, 8, 2, 15.5)

    def test_generate_diff_for_debug_file_not_exists(self):
        """Test diff generation when files don't exist."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f:
            f.write("content")
            temp_file = f.name

        try:
            # Test with non-existent file
            diff_output = self.debug_manager.generate_diff_for_debug(
                Path("nonexistent"), Path(temp_file)
            )
            self.assertIn("does not exist", diff_output)

            diff_output = self.debug_manager.generate_diff_for_debug(
                Path(temp_file), Path("nonexistent")
            )
            self.assertIn("does not exist", diff_output)
        finally:
            os.unlink(temp_file)

    def test_generate_diff_for_debug_success(self):
        """Test successful diff generation."""
        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f1:
            f1.write("actual content\nline 2")
            actual_file = f1.name

        with tempfile.NamedTemporaryFile(mode="w", delete=False) as f2:
            f2.write("expected content\nline 2")
            expected_file = f2.name

        try:
            diff_output = self.debug_manager.generate_diff_for_debug(
                Path(actual_file), Path(expected_file)
            )

            self.assertIn("=== FILE DIFF ===", diff_output)
            self.assertIn("Actual file: 2 lines", diff_output)
            self.assertIn("Expected file: 2 lines", diff_output)
            self.assertIn("--- ACTUAL (first few lines) ---", diff_output)
            self.assertIn("--- EXPECTED (first few lines) ---", diff_output)
        finally:
            os.unlink(actual_file)
            os.unlink(expected_file)

    def test_generate_simple_file_diff(self):
        """Test simple file diff generation."""
        actual = "line 1\nline 2\nline 3"
        expected = "line 1\nline 2\nline 4"

        diff_output = self.debug_manager._generate_simple_file_diff(actual, expected)

        self.assertIn("=== FILE DIFF ===", diff_output)
        self.assertIn("Actual file: 3 lines", diff_output)
        self.assertIn("Expected file: 3 lines", diff_output)
        self.assertIn("--- ACTUAL (first few lines) ---", diff_output)
        self.assertIn("--- EXPECTED (first few lines) ---", diff_output)
        self.assertIn("1: line 1", diff_output)
        self.assertIn("2: line 2", diff_output)
        self.assertIn("3: line 3", diff_output)


if __name__ == "__main__":
    unittest.main()

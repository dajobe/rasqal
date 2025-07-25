#!/usr/bin/env python3
"""
Unit tests for TestConfigBuilder class.

This module contains unit tests for the TestConfigBuilder class from
sparql_test_framework.config, focusing on the refactored helper methods.

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
from unittest.mock import patch, MagicMock

from test_base import SparqlTestFrameworkTestBase
from sparql_test_framework.config import TestConfigBuilder, TestResult, TestConfig
from sparql_test_framework.test_types import Namespaces


class TestTestConfigBuilder(SparqlTestFrameworkTestBase):
    """Test the TestConfigBuilder class."""

    def setUp(self):
        """Set up test fixtures."""
        super().setUp()

        # Create mock extractor and path_resolver
        self.mock_extractor = MagicMock()
        self.mock_path_resolver = MagicMock()

        # Create TestConfigBuilder instance
        self.builder = TestConfigBuilder(self.mock_extractor, self.mock_path_resolver)

    def test_extract_metadata_basic(self):
        """Test _extract_metadata extracts basic metadata correctly."""
        entry_uri_full = "<http://example.org/test1>"

        # Mock extractor responses
        self.mock_extractor.get_obj_val.return_value = "Test Name"
        self.mock_extractor.unquote.side_effect = lambda x: x

        result = self.builder._extract_metadata(entry_uri_full)

        # Verify the result
        self.assertEqual(result["name"], "Test Name")
        self.assertEqual(result["test_uri"], entry_uri_full)

        # Verify extractor was called correctly
        self.mock_extractor.get_obj_val.assert_called_once_with(
            entry_uri_full, f"<{Namespaces.MF}name>"
        )
        self.mock_extractor.unquote.assert_called()

    def test_extract_metadata_with_none_values(self):
        """Test _extract_metadata handles None values gracefully."""
        entry_uri_full = "<http://example.org/test1>"

        # Mock extractor to return None
        self.mock_extractor.get_obj_val.return_value = None
        self.mock_extractor.unquote.return_value = ""

        result = self.builder._extract_metadata(entry_uri_full)

        # Verify the result
        self.assertEqual(result["name"], "")
        self.assertEqual(result["test_uri"], "")

    def test_extract_file_lists_basic(self):
        """Test _extract_file_lists extracts file lists correctly."""
        entry_uri_full = "<http://example.org/test1>"
        action_node = "<http://example.org/action1>"

        # Mock extractor responses
        self.mock_extractor.get_obj_vals.side_effect = [
            ["data1.ttl", "data2.ttl"],  # data files
            ["graph1.ttl"],  # graph data files
            ["extra1.txt", "extra2.txt"],  # extra files
        ]

        # Mock path resolver responses
        self.mock_path_resolver.resolve_paths.side_effect = [
            [Path("data1.ttl"), Path("data2.ttl")],
            [Path("graph1.ttl")],
            [Path("extra1.txt"), Path("extra2.txt")],
        ]

        result = self.builder._extract_file_lists(entry_uri_full, action_node)

        # Verify the result
        self.assertEqual(len(result["data_files"]), 2)
        self.assertEqual(len(result["named_data_files"]), 1)
        self.assertEqual(len(result["extra_files"]), 2)

        # Verify extractor was called correctly
        self.mock_extractor.get_obj_vals.assert_called()

    def test_extract_file_lists_empty(self):
        """Test _extract_file_lists handles empty file lists."""
        entry_uri_full = "<http://example.org/test1>"
        action_node = "<http://example.org/action1>"

        # Mock extractor to return empty lists
        self.mock_extractor.get_obj_vals.return_value = []
        self.mock_path_resolver.resolve_paths.return_value = []

        result = self.builder._extract_file_lists(entry_uri_full, action_node)

        # Verify the result
        self.assertEqual(result["data_files"], [])
        self.assertEqual(result["named_data_files"], [])
        self.assertEqual(result["extra_files"], [])

    def test_extract_test_flags_approved(self):
        """Test _extract_test_flags extracts approved test flags correctly."""
        entry_uri_full = "<http://example.org/test1>"
        action_node = "<http://example.org/action1>"

        # Mock extractor to indicate approved test
        self.mock_extractor.get_obj_val.return_value = f"{Namespaces.DAWGT}Approved"
        self.mock_extractor.unquote.side_effect = lambda x: x

        result = self.builder._extract_test_flags(entry_uri_full, action_node)

        # Verify the result
        self.assertTrue(result["is_approved"])
        self.assertFalse(result["is_withdrawn"])

        # Verify extractor was called correctly
        self.mock_extractor.get_obj_val.assert_called()

    def test_extract_test_flags_withdrawn(self):
        """Test _extract_test_flags extracts withdrawn test flags correctly."""
        entry_uri_full = "<http://example.org/test1>"
        action_node = "<http://example.org/action1>"

        # Mock extractor to indicate withdrawn test
        self.mock_extractor.get_obj_val.return_value = f"{Namespaces.DAWGT}Withdrawn"
        self.mock_extractor.unquote.side_effect = lambda x: x

        result = self.builder._extract_test_flags(entry_uri_full, action_node)

        # Verify the result
        self.assertFalse(result["is_approved"])
        self.assertTrue(result["is_withdrawn"])

        # Verify extractor was called correctly
        self.mock_extractor.get_obj_val.assert_called()

    def test_extract_test_flags_sparql11_entailment(self):
        """Test _extract_test_flags detects SPARQL 1.1 entailment regime."""
        entry_uri_full = "<http://example.org/test1>"
        action_node = "<http://example.org/action1>"

        # Mock extractor to indicate SPARQL 1.1 entailment
        self.mock_extractor.get_obj_val.side_effect = [
            None,
            None,
            "some_entailment",
        ]  # not approved, not withdrawn, has entailment
        self.mock_extractor.unquote.side_effect = lambda x: x

        result = self.builder._extract_test_flags(entry_uri_full, action_node)

        # Verify the result
        self.assertFalse(result["is_approved"])
        self.assertFalse(result["is_withdrawn"])
        self.assertTrue(result["has_entailment_regime"])

    def test_build_test_config_integration(self):
        """Test build_test_config integrates all helper methods correctly."""
        entry_uri_full = "<http://example.org/test1>"
        action_node = "<http://example.org/action1>"

        # Mock all the helper methods
        with patch.object(
            self.builder, "_extract_metadata"
        ) as mock_metadata, patch.object(
            self.builder, "_extract_file_lists"
        ) as mock_files, patch.object(
            self.builder, "_extract_test_flags"
        ) as mock_flags:

            # Set up mock return values
            mock_metadata.return_value = {
                "name": "Test Name",
                "test_uri": entry_uri_full,
            }
            mock_files.return_value = {
                "data_files": [Path("data.ttl")],
                "named_data_files": [Path("graph.ttl")],
                "extra_files": [Path("extra.txt")],
            }
            mock_flags.return_value = {
                "is_approved": True,
                "is_withdrawn": False,
                "has_entailment_regime": False,
                "cardinality_mode": "strict",
            }

            # Call the method
            result = self.builder.build_test_config(
                entry_uri_full,
                action_node,
                "<http://example.org/query1>",
                Path("test.rq"),
                True,
                TestResult.PASSED,
                "sparql",
                "query",
            )

            # Verify it's a TestConfig object
            self.assertIsInstance(result, TestConfig)

            # Verify the helper methods were called
            mock_metadata.assert_called_once_with(entry_uri_full)
            mock_files.assert_called_once_with(entry_uri_full, action_node)
            mock_flags.assert_called_once_with(entry_uri_full, action_node)

    def test_build_test_config_with_none_result_file(self):
        """Test build_test_config handles None result file correctly."""
        entry_uri_full = "<http://example.org/test1>"
        action_node = "<http://example.org/action1>"

        # Mock helper methods and path resolver
        with patch.object(
            self.builder, "_extract_metadata"
        ) as mock_metadata, patch.object(
            self.builder, "_extract_file_lists"
        ) as mock_files, patch.object(
            self.builder, "_extract_test_flags"
        ) as mock_flags:

            # Set up mock return values with None result file
            mock_metadata.return_value = {
                "name": "Test Name",
                "test_uri": entry_uri_full,
            }
            mock_files.return_value = {
                "data_files": [Path("data.ttl")],
                "named_data_files": [],
                "extra_files": [],
            }
            mock_flags.return_value = {
                "is_approved": True,
                "is_withdrawn": False,
                "has_entailment_regime": False,
                "cardinality_mode": "strict",
            }

            # Mock path resolver to return None for result file
            self.mock_path_resolver.resolve_path.return_value = None

            # Call the method
            result = self.builder.build_test_config(
                entry_uri_full,
                action_node,
                "<http://example.org/query1>",
                Path("test.rq"),
                True,
                TestResult.PASSED,
                "sparql",
                "query",
            )

            # Verify the result file is None
            self.assertIsNone(result.result_file)


if __name__ == "__main__":
    unittest.main()

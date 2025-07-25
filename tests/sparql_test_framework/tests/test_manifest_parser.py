#!/usr/bin/env python3
"""
Unit tests for ManifestParser class.

This module contains unit tests for the ManifestParser class from
sparql_test_framework.manifest, focusing on the refactored helper methods.

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
from sparql_test_framework import ManifestParser, Namespaces


class TestManifestParserHelpers(SparqlTestFrameworkTestBase):
    """Test the helper methods extracted from ManifestParser.get_tests()."""

    def setUp(self):
        """Set up test fixtures."""
        super().setUp()
        self.mock_manifest_path = Path("test_manifest.ttl")

        # Create a mock ManifestParser instance
        with self.patch_find_tool(), patch.object(ManifestParser, "_parse"):
            self.parser = ManifestParser(self.mock_manifest_path, "mock-to-ntriples")

        # Mock the triples_by_subject to avoid actual parsing
        self.parser.triples_by_subject = {}

    def test_create_helpers(self):
        """Test _create_helpers method creates required helper objects."""
        srcdir = Path("/test/srcdir")

        helpers = self.parser._create_helpers(srcdir)

        # Check that all required helpers are created
        self.assertIn("extractor", helpers)
        self.assertIn("config_builder", helpers)
        self.assertIn("path_resolver", helpers)

        # Check that helpers are properly initialized
        self.assertEqual(
            helpers["extractor"].triples_by_subject, self.parser.triples_by_subject
        )
        self.assertEqual(helpers["path_resolver"].srcdir, srcdir)

    def test_has_nested_manifests_true(self):
        """Test _has_nested_manifests returns True when mf:include is present."""
        manifest_node_uri = "<http://example.org/manifest>"

        # Mock extractor to return non-empty list for mf:include
        mock_extractor = MagicMock()
        mock_extractor.get_obj_vals.return_value = ["_:list1"]

        result = self.parser._has_nested_manifests(manifest_node_uri, mock_extractor)

        self.assertTrue(result)
        mock_extractor.get_obj_vals.assert_called_once_with(
            manifest_node_uri, f"<{Namespaces.MF}include>"
        )

    def test_has_nested_manifests_false(self):
        """Test _has_nested_manifests returns False when no mf:include is present."""
        manifest_node_uri = "<http://example.org/manifest>"

        # Mock extractor to return empty list for mf:include
        mock_extractor = MagicMock()
        mock_extractor.get_obj_vals.return_value = []

        result = self.parser._has_nested_manifests(manifest_node_uri, mock_extractor)

        self.assertFalse(result)
        mock_extractor.get_obj_vals.assert_called_once_with(
            manifest_node_uri, f"<{Namespaces.MF}include>"
        )

    def test_should_include_test_no_filter(self):
        """Test _should_include_test returns True when no filter is specified."""
        config = self.create_mock_test_config(name="test-1")

        result = self.parser._should_include_test(config, None)

        self.assertTrue(result)

    def test_should_include_test_with_matching_filter(self):
        """Test _should_include_test returns True when filter matches test name."""
        config = self.create_mock_test_config(name="test-1")

        result = self.parser._should_include_test(config, "test-1")

        self.assertTrue(result)

    def test_should_include_test_with_non_matching_filter(self):
        """Test _should_include_test returns False when filter doesn't match test name."""
        config = self.create_mock_test_config(name="test-1")

        result = self.parser._should_include_test(config, "test-2")

        self.assertFalse(result)

    def test_matches_filter_exact_match(self):
        """Test _matches_filter returns True for exact match."""
        config = self.create_mock_test_config(
            name="test-1", test_uri="http://example.org/test-1"
        )
        result = self.parser._matches_filter(config, "test-1")
        self.assertTrue(result)

    def test_matches_filter_partial_match(self):
        """Test _matches_filter returns True for partial match."""
        config = self.create_mock_test_config(
            name="test-1", test_uri="http://example.org/test-1"
        )
        result = self.parser._matches_filter(config, "test")
        self.assertTrue(result)

    def test_matches_filter_no_match(self):
        """Test _matches_filter returns False for no match."""
        config = self.create_mock_test_config(
            name="test-1", test_uri="http://example.org/test-1"
        )
        result = self.parser._matches_filter(config, "other")
        self.assertFalse(result)

    def test_process_nested_manifests_empty(self):
        """Test _process_nested_manifests returns empty list when no manifests."""
        manifest_node_uri = "<http://example.org/manifest>"
        srcdir = Path("/test/srcdir")
        helpers = {
            "extractor": MagicMock(),
            "path_resolver": MagicMock(),
            "config_builder": MagicMock(),
        }
        helpers["extractor"].get_obj_vals.return_value = []

        result = self.parser._process_nested_manifests(
            manifest_node_uri, srcdir, None, helpers
        )

        self.assertEqual(result, [])
        helpers["extractor"].get_obj_vals.assert_called_once_with(
            manifest_node_uri, f"<{Namespaces.MF}include>"
        )

    def test_process_direct_entries_empty(self):
        """Test _process_direct_entries returns empty list when no entries."""
        manifest_node_uri = "<http://example.org/manifest>"
        srcdir = Path("/test/srcdir")
        helpers = {
            "extractor": MagicMock(),
            "path_resolver": MagicMock(),
            "config_builder": MagicMock(),
        }
        helpers["extractor"].get_obj_vals.return_value = []
        # Make extractor return None for action node so no config is built
        helpers["extractor"].get_obj_val.return_value = None

        # Add mock triples with mf:entries to the parser
        self.parser.triples_by_subject = {
            manifest_node_uri: [
                {"p": f"<{Namespaces.MF}entries>", "o_full": "_:list1"}
            ],
            "_:list1": [
                {"p": f"<{Namespaces.RDF}first>", "o_full": "_:entry1"},
                {"p": f"<{Namespaces.RDF}rest>", "o_full": f"<{Namespaces.RDF}nil>"},
            ],
        }

        result = self.parser._process_direct_entries(
            manifest_node_uri, srcdir, None, helpers
        )

        self.assertEqual(result, [])
        # The method calls get_obj_val, not get_obj_vals
        helpers["extractor"].get_obj_val.assert_called()


class TestManifestParserIntegration(SparqlTestFrameworkTestBase):
    """Integration tests for ManifestParser.get_tests()."""

    def setUp(self):
        """Set up test fixtures."""
        super().setUp()
        self.mock_manifest_path = Path("test_manifest.ttl")

    def test_get_tests_with_nested_manifests(self):
        """Test get_tests processes nested manifests correctly."""
        # Mock the parser to simulate nested manifests
        with self.patch_find_tool(), patch.object(
            ManifestParser, "_parse"
        ) as mock_parse, patch.object(
            ManifestParser, "_has_nested_manifests", return_value=True
        ), patch.object(
            ManifestParser,
            "_process_nested_manifests",
            return_value=[self.create_mock_test_config()],
        ), patch.object(
            ManifestParser, "_process_direct_entries", return_value=[]
        ):

            parser = ManifestParser(self.mock_manifest_path, "mock-to-ntriples")
            # Add mock triples with mf:include to simulate nested manifests
            parser.triples_by_subject = {
                "<http://example.org/manifest>": [
                    {"p": f"<{Namespaces.MF}include>", "o_full": "_:list1"}
                ]
            }

            result = parser.get_tests(Path("/test/srcdir"), None)

            self.assertEqual(len(result), 1)
            self.assertIsInstance(result[0], type(self.create_mock_test_config()))

    def test_get_tests_with_direct_entries(self):
        """Test get_tests processes direct entries correctly."""
        # Mock the parser to simulate direct entries
        with self.patch_find_tool(), patch.object(
            ManifestParser, "_parse"
        ) as mock_parse, patch.object(
            ManifestParser, "_has_nested_manifests", return_value=False
        ), patch.object(
            ManifestParser, "_process_nested_manifests", return_value=[]
        ), patch.object(
            ManifestParser,
            "_process_direct_entries",
            return_value=[self.create_mock_test_config()],
        ):

            parser = ManifestParser(self.mock_manifest_path, "mock-to-ntriples")
            # Add mock triples with mf:entries to simulate direct entries
            parser.triples_by_subject = {
                "<http://example.org/manifest>": [
                    {"p": f"<{Namespaces.MF}entries>", "o_full": "_:list1"}
                ]
            }

            result = parser.get_tests(Path("/test/srcdir"), None)

            self.assertEqual(len(result), 1)
            self.assertIsInstance(result[0], type(self.create_mock_test_config()))


if __name__ == "__main__":
    unittest.main()

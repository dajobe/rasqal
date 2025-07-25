#!/usr/bin/env python3
"""
Unit tests for utilities module.

This module contains unit tests for the utility functions and classes
in sparql_test_framework.utils.

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

import sys
import os

sys.path.insert(0, os.path.dirname(__file__))
from test_base import SparqlTestFrameworkTestBase
from sparql_test_framework import PathResolver, find_tool, setup_logging


class TestPathResolver(SparqlTestFrameworkTestBase):
    """Test the PathResolver class."""

    def setUp(self):
        """Set up test fixtures."""
        super().setUp()
        # Create a mock extractor
        mock_extractor = MagicMock()
        mock_extractor.unquote_uri.return_value = "test.txt"
        self.resolver = PathResolver(srcdir=Path("/base"), extractor=mock_extractor)

    def test_path_resolver_initialization(self):
        """Test PathResolver initializes correctly."""
        self.assertEqual(self.resolver.srcdir, Path("/base"))
        self.assertIsNotNone(self.resolver.extractor)

    def test_resolve_path_functionality(self):
        """Test resolve_path method can be called."""
        # Basic smoke test - actual implementation would be more detailed
        result = self.resolver.resolve_path("http://example.org/test.txt")
        # Test passes if no exception is raised


class TestUtilityFunctions(SparqlTestFrameworkTestBase):
    """Test utility functions."""

    def test_find_tool_functionality(self):
        """Test find_tool function can be called."""
        # Basic smoke test - actual implementation would be more detailed
        with patch("shutil.which", return_value="/usr/bin/mock-tool"):
            result = find_tool("mock-tool")
            self.assertEqual(result, "/usr/bin/mock-tool")

    def test_setup_logging_functionality(self):
        """Test setup_logging function can be called."""
        # Basic smoke test - actual implementation would test logging configuration
        with patch("logging.basicConfig"):
            setup_logging()
            # Test passes if no exception is raised


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3
"""
Unit tests for rasqal-compare integration.

This module contains unit tests for the --use-rasqal-compare flag integration
in the SPARQL test runner.

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
from pathlib import Path
from unittest.mock import patch, MagicMock
import io

# Add parent directories to path for imports
sys.path.append(str(Path(__file__).parent.parent.parent.parent))
sys.path.append(str(Path(__file__).parent.parent.parent))

from test_base import SparqlTestFrameworkTestBase


class TestRasqalCompareIntegration(SparqlTestFrameworkTestBase):
    """Test the rasqal-compare integration functionality."""

    def setUp(self):
        """Set up test fixtures."""
        super().setUp()
        from sparql_test_framework.runners import sparql
        self.runner = sparql.SparqlTestRunner()

    def test_use_rasqal_compare_flag_parsing(self):
        """Test that the --use-rasqal-compare flag is properly parsed."""
        parser = self.runner.setup_argument_parser()
        test_args = ["--manifest-file", "test.ttl", "--use-rasqal-compare"]
        
        args = parser.parse_args(test_args)
        self.assertTrue(args.use_rasqal_compare)

    def test_use_rasqal_compare_flag_default(self):
        """Test that the flag defaults to False when not specified."""
        parser = self.runner.setup_argument_parser()
        test_args = ["--manifest-file", "test.ttl"]
        
        args = parser.parse_args(test_args)
        self.assertFalse(args.use_rasqal_compare)

    def test_use_rasqal_compare_help_output(self):
        """Test that the help output includes the new flag."""
        parser = self.runner.setup_argument_parser()
        
        # Capture help output
        help_output = io.StringIO()
        parser.print_help(file=help_output)
        help_text = help_output.getvalue()
        
        self.assertIn("--use-rasqal-compare", help_text)

    def test_use_rasqal_compare_flag_description(self):
        """Test that the flag has a proper description in help."""
        parser = self.runner.setup_argument_parser()
        
        # Capture help output
        help_output = io.StringIO()
        parser.print_help(file=help_output)
        help_text = help_output.getvalue()
        
        # Check that the flag appears in the help text with some context
        self.assertIn("--use-rasqal-compare", help_text)
        # The help should mention something about rasqal-compare
        self.assertIn("rasqal-compare", help_text)

    def test_use_rasqal_compare_with_other_flags(self):
        """Test that the flag works correctly with other flags."""
        parser = self.runner.setup_argument_parser()
        test_args = [
            "--manifest-file", "test.ttl",
            "--use-rasqal-compare",
            "--debug", "2",
            "--preserve-files"
        ]
        
        args = parser.parse_args(test_args)
        self.assertTrue(args.use_rasqal_compare)
        self.assertEqual(args.debug, 2)
        self.assertTrue(args.preserve_files)


if __name__ == "__main__":
    unittest.main() 
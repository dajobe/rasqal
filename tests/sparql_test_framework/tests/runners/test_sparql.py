#!/usr/bin/env python3
"""
Unit tests for the SPARQL test runner.

This module tests the SparqlTestRunner class and its functionality.

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

import io
import unittest
from unittest.mock import patch

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


if __name__ == "__main__":
    unittest.main()

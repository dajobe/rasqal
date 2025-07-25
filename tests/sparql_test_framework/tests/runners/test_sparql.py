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
from unittest.mock import patch, MagicMock

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


if __name__ == "__main__":
    unittest.main()

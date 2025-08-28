#!/usr/bin/env python3
"""
Unit tests for data models.

This module contains unit tests for the new data model classes
that will replace the dictionary-based approach.

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
import tempfile
import os
import sys

# Add parent directories to path for imports
sys.path.append(str(Path(__file__).parent.parent.parent.parent))
sys.path.append(str(Path(__file__).parent.parent.parent))

from test_base import SparqlTestFrameworkTestBase
from sparql_test_framework.data import QueryType, QueryResult
from sparql_test_framework.utils import (
    QueryExecutionError,
    ResultProcessingError,
    ComparisonError,
)


class TestDataModels(SparqlTestFrameworkTestBase):
    """Test the data model classes."""

    def test_query_type_creation(self):
        """Test QueryType creation and attributes."""
        qt = QueryType(is_construct=True, is_ask=False, is_select=False)
        self.assertTrue(qt.is_construct)
        self.assertFalse(qt.is_ask)
        self.assertFalse(qt.is_select)

    def test_query_result_creation(self):
        """Test QueryResult creation and attributes."""
        qr = QueryResult(
            content="test content",
            result_type="graph",
            count=5,
            vars_order=["x", "y"],
            boolean_value=None,
            format="turtle",
            metadata={"key": "value"},
        )
        self.assertEqual(qr.content, "test content")
        self.assertEqual(qr.result_type, "graph")
        self.assertEqual(qr.count, 5)
        self.assertEqual(qr.vars_order, ["x", "y"])
        self.assertIsNone(qr.boolean_value)
        self.assertEqual(qr.format, "turtle")
        self.assertEqual(qr.metadata, {"key": "value"})


class TestExceptions(SparqlTestFrameworkTestBase):
    """Test the custom exception classes."""

    def test_query_execution_error(self):
        """Test QueryExecutionError creation."""
        error = QueryExecutionError("Test error", "SELECT", "test_config")
        self.assertEqual(str(error), "Test error")
        self.assertEqual(error.query_type, "SELECT")
        self.assertEqual(error.config_name, "test_config")

    def test_result_processing_error(self):
        """Test ResultProcessingError creation."""
        error = ResultProcessingError("Processing failed", "graph", "turtle")
        self.assertEqual(str(error), "Processing failed")
        self.assertEqual(error.result_type, "graph")
        self.assertEqual(error.format, "turtle")

    def test_comparison_error(self):
        """Test ComparisonError creation."""
        error = ComparisonError("Comparison failed", "diff")
        self.assertEqual(str(error), "Comparison failed")
        self.assertEqual(error.comparison_method, "diff")


if __name__ == "__main__":
    unittest.main()

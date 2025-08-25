#!/usr/bin/env python3
"""
Unit tests for CsvTsvTestRunner.

This module contains unit tests for the CsvTsvTestRunner class
from sparql_test_framework.runners.csv_tsv.

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
from unittest.mock import patch, MagicMock
from pathlib import Path

from sparql_test_framework.tests.test_base import SparqlTestFrameworkTestBase
from sparql_test_framework.runners.csv_tsv import CsvTsvTestRunner


class TestCsvTsvTestRunner(SparqlTestFrameworkTestBase):
    """Test the CsvTsvTestRunner class."""

    def test_csv_tsv_runner_can_be_imported(self):
        """Test that CsvTsvTestRunner can be imported successfully."""
        from sparql_test_framework.runners.csv_tsv import CsvTsvTestRunner

        self.assertIsNotNone(CsvTsvTestRunner)

    def test_csv_tsv_runner_initialization(self):
        """Test CsvTsvTestRunner can be initialized."""
        runner = CsvTsvTestRunner()
        self.assertIsNotNone(runner)

    def test_csv_tsv_runner_inherits_from_format_base(self):
        """Test that CsvTsvTestRunner inherits from FormatTestRunner."""
        from sparql_test_framework.runners.format_base import FormatTestRunner

        runner = CsvTsvTestRunner()
        self.assertIsInstance(runner, FormatTestRunner)

    def test_csv_tsv_runner_has_required_methods(self):
        """Test that CsvTsvTestRunner has required methods."""
        runner = CsvTsvTestRunner()
        self.assertTrue(hasattr(runner, "run_format_test"))
        self.assertTrue(hasattr(runner, "main"))


if __name__ == "__main__":
    unittest.main()

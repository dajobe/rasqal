#!/usr/bin/env python3
"""
Unit tests for AlgebraTestRunner.

This module contains unit tests for the AlgebraTestRunner class
from sparql_test_framework.runners.algebra.

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

from test_base import SparqlTestFrameworkTestBase
from sparql_test_framework.runners.algebra import AlgebraTestRunner


class TestAlgebraTestRunner(SparqlTestFrameworkTestBase):
    """Test the AlgebraTestRunner class."""

    def test_algebra_runner_can_be_imported(self):
        """Test that AlgebraTestRunner can be imported successfully."""
        from sparql_test_framework.runners.algebra import AlgebraTestRunner

        self.assertIsNotNone(AlgebraTestRunner)

    def test_algebra_runner_initialization(self):
        """Test AlgebraTestRunner can be initialized."""
        runner = AlgebraTestRunner()
        self.assertIsNotNone(runner)

    def test_algebra_runner_has_required_methods(self):
        """Test that AlgebraTestRunner has required methods."""
        runner = AlgebraTestRunner()
        self.assertTrue(hasattr(runner, "main"))


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3
"""
Unit tests for TestPackager.

This module contains unit tests for the TestPackager class
from sparql_test_framework.tools.packager.

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
from sparql_test_framework.tools.packager import TestPackager


class TestTestPackager(SparqlTestFrameworkTestBase):
    """Test the TestPackager class."""

    def test_packager_can_be_imported(self):
        """Test that TestPackager can be imported successfully."""
        from sparql_test_framework.tools.packager import TestPackager

        self.assertIsNotNone(TestPackager)

    def test_packager_initialization(self):
        """Test TestPackager can be initialized."""
        packager = TestPackager()
        self.assertIsNotNone(packager)

    def test_packager_has_required_methods(self):
        """Test that TestPackager has required methods."""
        packager = TestPackager()
        self.assertTrue(hasattr(packager, "extract_files_from_manifest"))
        self.assertTrue(hasattr(packager, "package_files"))


if __name__ == "__main__":
    unittest.main()

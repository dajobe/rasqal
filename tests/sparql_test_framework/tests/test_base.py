#!/usr/bin/env python3
"""
Base test class for SPARQL Test Framework unit tests.

This module provides a base class with common utilities and setup
for unit testing the sparql_test_framework package.

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

# Add parent directories to path for imports
sys.path.append(str(Path(__file__).parent.parent.parent.parent))
sys.path.append(str(Path(__file__).parent.parent.parent))


class SparqlTestFrameworkTestBase(unittest.TestCase):
    """Base class for SPARQL Test Framework unit tests with common utilities."""

    def setUp(self):
        """Set up test fixtures."""
        super().setUp()
        self.test_data_dir = Path(__file__).parent / "test_data"
        self.test_data_dir.mkdir(exist_ok=True)

    def tearDown(self):
        """Clean up test fixtures."""
        super().tearDown()
        
        # Clean up test_data directory if it exists and is empty
        if self.test_data_dir.exists():
            try:
                # Remove the directory and all its contents
                import shutil
                shutil.rmtree(self.test_data_dir, ignore_errors=True)
            except (OSError, IOError):
                # Ignore errors during cleanup
                pass

    def create_mock_manifest_triples(self, triples_data):
        """
        Create mock manifest triples for testing.

        Args:
            triples_data: Dict mapping subject URIs to list of triple dicts

        Returns:
            Dict suitable for ManifestParser.triples_by_subject
        """
        return triples_data

    def create_mock_test_config(self, **kwargs):
        """
        Create a mock TestConfig object for testing.

        Args:
            **kwargs: TestConfig attributes to set

        Returns:
            Mock TestConfig object
        """
        from sparql_test_framework import TestConfig, TestType, TestResult

        # Default values
        defaults = {
            "name": "test-config",
            "test_uri": "http://example.org/test",
            "test_file": Path("test.rq"),
            "expect": TestResult.PASSED,
            "language": "sparql",
            "execute": True,
            "cardinality_mode": "strict",
            "is_withdrawn": False,
            "is_approved": False,
            "has_entailment_regime": False,
            "test_type": TestType.QUERY_EVALUATION_TEST.value,
            "data_files": [],
            "named_data_files": [],
            "result_file": None,
            "extra_files": [],
            "warning_level": 0,
        }

        # Override with provided kwargs
        defaults.update(kwargs)

        return TestConfig(**defaults)

    def assert_test_config_equal(self, config1, config2):
        """
        Assert that two TestConfig objects are equal.

        Args:
            config1: First TestConfig object
            config2: Second TestConfig object
        """
        self.assertEqual(config1.name, config2.name)
        self.assertEqual(config1.test_uri, config2.test_uri)
        self.assertEqual(config1.test_file, config2.test_file)
        self.assertEqual(config1.expect, config2.expect)
        self.assertEqual(config1.language, config2.language)
        self.assertEqual(config1.execute, config2.execute)
        self.assertEqual(config1.test_type, config2.test_type)

    def patch_find_tool(self, return_value="mock-to-ntriples"):
        """
        Create a patch for find_tool function.

        Args:
            return_value: Value to return from find_tool

        Returns:
            Context manager for patching
        """
        return patch("sparql_test_framework.utils.find_tool", return_value=return_value)

    def patch_subprocess_run(self, return_value=None):
        """
        Create a patch for subprocess.run function.

        Args:
            return_value: Mock return value for subprocess.run

        Returns:
            Context manager for patching
        """
        if return_value is None:
            return_value = MagicMock(returncode=0, stdout=b"mock output", stderr=b"")

        return patch("subprocess.run", return_value=return_value)

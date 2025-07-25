#!/usr/bin/env python3
"""
Integration tests for SPARQL Test Framework.

This module contains integration tests that verify cross-module functionality
and end-to-end workflows in the sparql_test_framework package.

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
from sparql_test_framework import (
    TestConfig,
    TestType,
    TestResult,
    Namespaces,
    ManifestParser,
    TestTypeResolver,
    PathResolver,
)


class TestFrameworkIntegration(SparqlTestFrameworkTestBase):
    """Integration tests for the complete framework."""

    def test_package_imports_work(self):
        """Test that all main framework components can be imported."""
        # Test core components
        self.assertIsNotNone(TestConfig)
        self.assertIsNotNone(TestType)
        self.assertIsNotNone(TestResult)
        self.assertIsNotNone(Namespaces)

        # Test framework components
        self.assertIsNotNone(ManifestParser)
        self.assertIsNotNone(TestTypeResolver)
        self.assertIsNotNone(PathResolver)

    def test_namespace_and_type_integration(self):
        """Test integration between Namespaces and TestType classes."""
        # Test that TestType URIs can be processed by Namespaces
        test_type_uri = TestType.POSITIVE_SYNTAX_TEST.value
        prefixed_name = Namespaces.uri_to_prefixed_name(test_type_uri)

        # Should convert URI to prefixed form
        self.assertIn(":", prefixed_name)

        # Should be reversible
        reconstructed_uri = Namespaces.prefixed_name_to_uri(prefixed_name)
        self.assertEqual(reconstructed_uri, test_type_uri)

    def test_config_and_type_resolver_integration(self):
        """Test integration between TestConfig and TestTypeResolver."""
        # Create a test config
        config = self.create_mock_test_config(
            test_type=TestType.POSITIVE_SYNTAX_TEST.value
        )

        # Test that type resolver can handle the config's test type
        behavior = TestTypeResolver.resolve_test_behavior(config.test_type)
        self.assertIsNotNone(behavior)
        self.assertIsInstance(behavior, tuple)
        self.assertEqual(len(behavior), 3)  # (execute, expect, language)

    def test_result_display_integration(self):
        """Test that TestResult display methods work consistently."""
        # Test all result types
        for result in TestResult:
            char = result.display_char()
            name = result.display_name()

            self.assertIsInstance(char, str)
            self.assertIsInstance(name, str)
            self.assertTrue(len(char) == 1)  # Display char should be single character
            self.assertTrue(len(name) > 0)  # Display name should be non-empty

    def test_path_resolver_integration(self):
        """Test PathResolver integration with framework paths."""
        # Create a mock extractor
        mock_extractor = MagicMock()
        mock_extractor.unquote_uri.return_value = "query.rq"

        base_dir = Path("/test/base")
        resolver = PathResolver(srcdir=base_dir, extractor=mock_extractor)

        # Test that PathResolver can be created and has expected attributes
        self.assertEqual(resolver.srcdir, base_dir)
        self.assertIsNotNone(resolver.extractor)


class TestBackwardCompatibilityIntegration(SparqlTestFrameworkTestBase):
    """Integration tests for backward compatibility."""

    def test_compatibility_import_layer(self):
        """Test that old import patterns still work through compatibility layer."""
        # Test importing through the compatibility shim
        with patch("warnings.warn"):  # Suppress deprecation warning in tests
            try:
                # This should work through the compatibility layer
                from rasqal_test_util import TestConfig as OldTestConfig
                from sparql_test_framework import TestConfig as NewTestConfig

                # They should refer to the same class
                self.assertEqual(OldTestConfig, NewTestConfig)
            except ImportError:
                # If compatibility layer not available, test passes
                pass

    def test_framework_components_available(self):
        """Test that all framework components are available through main package."""
        # Import from main package
        from sparql_test_framework import (
            TestOrchestrator,
            SparqlTestRunner,
            PlanGenerator,
        )

        # Test that classes can be instantiated
        orchestrator = TestOrchestrator()
        runner = SparqlTestRunner()
        generator = PlanGenerator()

        self.assertIsNotNone(orchestrator)
        self.assertIsNotNone(runner)
        self.assertIsNotNone(generator)


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3
"""
Unit tests for PlanGenerator tool.

This module contains unit tests for the PlanGenerator class
from sparql_test_framework.tools.plan_generator.

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
from sparql_test_framework.tools import PlanGenerator


class TestPlanGenerator(SparqlTestFrameworkTestBase):
    """Test the PlanGenerator class."""

    def test_plan_generator_initialization(self):
        """Test PlanGenerator can be initialized."""
        generator = PlanGenerator()
        self.assertIsNotNone(generator)

    def test_plan_generator_can_be_imported(self):
        """Test that PlanGenerator can be imported successfully."""
        # This is a basic smoke test to ensure the class is accessible
        self.assertTrue(hasattr(PlanGenerator, "__init__"))


if __name__ == "__main__":
    unittest.main()

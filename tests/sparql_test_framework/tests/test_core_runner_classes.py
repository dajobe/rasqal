"""
Unit tests for Phase 2 classes: QueryExecutor, ResultProcessor, and SRJHandler.

This module tests the core component classes that handle query execution,
result processing, and SRJ format handling.

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

import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import Mock, patch, MagicMock

import sys
import os

sys.path.insert(0, os.path.dirname(__file__))
from sparql_test_framework.runners.query_executor import (
    QueryExecutor,
    QueryType,
    QueryResult,
)
from sparql_test_framework.runners.result_processor import (
    ResultProcessor,
    ProcessedResult,
)
from sparql_test_framework.runners.srj_handler import SRJHandler, SRJResult
from sparql_test_framework.config import TestConfig
from sparql_test_framework.utils.exceptions import SRJProcessingError


class TestQueryType(unittest.TestCase):
    """Test QueryType class."""

    def test_construct_query_detection(self):
        """Test CONSTRUCT query detection."""
        query = "CONSTRUCT { ?s ?p ?o } WHERE { ?s ?p ?o }"
        query_type = QueryType(query)
        self.assertTrue(query_type.is_construct)
        self.assertFalse(query_type.is_ask)
        self.assertFalse(query_type.is_select)
        self.assertFalse(query_type.is_update)

    def test_ask_query_detection(self):
        """Test ASK query detection."""
        query = "ASK WHERE { ?s ?p ?o }"
        query_type = QueryType(query)
        self.assertFalse(query_type.is_construct)
        self.assertTrue(query_type.is_ask)
        self.assertFalse(query_type.is_select)
        self.assertFalse(query_type.is_update)

    def test_select_query_detection(self):
        """Test SELECT query detection."""
        query = "SELECT ?s ?p ?o WHERE { ?s ?p ?o }"
        query_type = QueryType(query)
        self.assertFalse(query_type.is_construct)
        self.assertFalse(query_type.is_ask)
        self.assertTrue(query_type.is_select)
        self.assertFalse(query_type.is_update)

    def test_update_query_detection(self):
        """Test UPDATE query detection."""
        query = "INSERT { ?s ?p ?o } WHERE { ?s ?p ?o }"
        query_type = QueryType(query)
        self.assertFalse(query_type.is_construct)
        self.assertFalse(query_type.is_ask)
        self.assertFalse(query_type.is_select)
        self.assertTrue(query_type.is_update)


class TestQueryResult(unittest.TestCase):
    """Test QueryResult class."""

    def test_query_result_creation(self):
        """Test QueryResult creation with all parameters."""
        result = QueryResult(
            content="test content",
            result_type="graph",
            count=5,
            vars_order=["s", "p", "o"],
            boolean_value=None,
            format="ntriples",
            metadata={"key": "value"},
        )

        self.assertEqual(result.content, "test content")
        self.assertEqual(result.result_type, "graph")
        self.assertEqual(result.count, 5)
        self.assertEqual(result.vars_order, ["s", "p", "o"])
        self.assertIsNone(result.boolean_value)
        self.assertEqual(result.format, "ntriples")
        self.assertEqual(result.metadata, {"key": "value"})

    def test_query_result_defaults(self):
        """Test QueryResult creation with default values."""
        result = QueryResult("test content", "bindings")

        self.assertEqual(result.content, "test content")
        self.assertEqual(result.result_type, "bindings")
        self.assertEqual(result.count, 0)
        self.assertEqual(result.vars_order, [])
        self.assertIsNone(result.boolean_value)
        self.assertEqual(result.format, "text")
        self.assertEqual(result.metadata, {})


class TestQueryExecutor(unittest.TestCase):
    """Test QueryExecutor class."""

    def setUp(self):
        """Set up test fixtures."""
        self.executor = QueryExecutor()
        self.mock_config = Mock(spec=TestConfig)
        self.mock_config.query = "SELECT ?s ?p ?o WHERE { ?s ?p ?o }"
        self.mock_config.query_file = Path("test.rq")
        self.mock_config.data_files = [Path("test.ttl")]
        self.mock_config.result_format = "csv"
        self.mock_config.debug_level = 0

    @patch("sparql_test_framework.runners.query_executor.find_tool")
    def test_ensure_roqet_available(self, mock_find_tool):
        """Test roqet availability check."""
        mock_find_tool.return_value = "/usr/bin/roqet"

        path = self.executor._ensure_roqet_available()
        self.assertEqual(path, "/usr/bin/roqet")
        self.assertEqual(self.executor.roqet_path, "/usr/bin/roqet")

    @patch("sparql_test_framework.runners.query_executor.find_tool")
    def test_ensure_roqet_not_found(self, mock_find_tool):
        """Test roqet not found error."""
        mock_find_tool.return_value = None

        with self.assertRaises(RuntimeError):
            self.executor._ensure_roqet_available()

    def test_detect_query_type(self):
        """Test query type detection."""
        query_type = self.executor.detect_query_type(self.mock_config)
        self.assertTrue(query_type.is_select)
        self.assertFalse(query_type.is_construct)
        self.assertFalse(query_type.is_ask)

    @patch("sparql_test_framework.runners.query_executor.find_tool")
    def test_build_roqet_args_select(self, mock_find_tool):
        """Test roqet argument building for SELECT queries."""
        mock_find_tool.return_value = "/usr/bin/roqet"
        query_type = QueryType("SELECT ?s WHERE { ?s ?p ?o }")

        args = self.executor.build_roqet_args(self.mock_config, query_type)

        expected = ["/usr/bin/roqet", "-q", "test.rq", "-D", "test.ttl", "-r", "csv"]
        self.assertEqual(args, expected)

    @patch("sparql_test_framework.runners.query_executor.find_tool")
    def test_build_roqet_args_construct(self, mock_find_tool):
        """Test roqet argument building for CONSTRUCT queries."""
        mock_find_tool.return_value = "/usr/bin/roqet"
        query_type = QueryType("CONSTRUCT { ?s ?p ?o } WHERE { ?s ?p ?o }")

        args = self.executor.build_roqet_args(self.mock_config, query_type)

        expected = [
            "/usr/bin/roqet",
            "-q",
            "test.rq",
            "-D",
            "test.ttl",
            "-r",
            "ntriples",
        ]
        self.assertEqual(args, expected)

    @patch("sparql_test_framework.runners.query_executor.find_tool")
    def test_build_roqet_args_ask(self, mock_find_tool):
        """Test roqet argument building for ASK queries."""
        mock_find_tool.return_value = "/usr/bin/roqet"
        query_type = QueryType("ASK WHERE { ?s ?p ?o }")

        args = self.executor.build_roqet_args(self.mock_config, query_type)

        expected = ["/usr/bin/roqet", "-q", "test.rq", "-D", "test.ttl", "-r", "srj"]
        self.assertEqual(args, expected)

    @patch("sparql_test_framework.runners.query_executor.find_tool")
    @patch("sparql_test_framework.runners.query_executor.run_command")
    def test_execute_select_query(self, mock_run_command, mock_find_tool):
        """Test SELECT query execution."""
        mock_find_tool.return_value = "/usr/bin/roqet"
        mock_run_command.return_value = (0, "s,p,o\n1,2,3\n4,5,6", "")

        result = self.executor.execute_select_query(self.mock_config)

        self.assertEqual(result.result_type, "bindings")
        self.assertEqual(result.count, 2)  # 2 data rows
        self.assertEqual(result.format, "csv")
        self.assertIn("s", result.vars_order)

    def test_extract_variables_from_query(self):
        """Test variable extraction from SELECT queries."""
        query = "SELECT ?s ?p ?o WHERE { ?s ?p ?o }"
        variables = self.executor._extract_variables_from_query(query)
        self.assertEqual(set(variables), {"s", "p", "o"})

    def test_count_bindings_csv(self):
        """Test binding counting for CSV format."""
        csv_output = "s,p,o\n1,2,3\n4,5,6"
        count = self.executor._count_bindings(csv_output, "csv")
        self.assertEqual(count, 2)

    def test_count_bindings_srj(self):
        """Test binding counting for SRJ format."""
        srj_output = '{"head": {"vars": ["s"]}, "results": {"bindings": [{"s": "1"}, {"s": "2"}]}}'
        count = self.executor._count_bindings(srj_output, "srj")
        self.assertEqual(count, 2)


class TestProcessedResult(unittest.TestCase):
    """Test ProcessedResult class."""

    def test_processed_result_creation(self):
        """Test ProcessedResult creation with all parameters."""
        result = ProcessedResult(
            normalized_content="normalized content",
            triple_count=10,
            variables=["s", "p", "o"],
            is_sorted=True,
            metadata={"key": "value"},
        )

        self.assertEqual(result.normalized_content, "normalized content")
        self.assertEqual(result.triple_count, 10)
        self.assertEqual(result.variables, ["s", "p", "o"])
        self.assertTrue(result.is_sorted)
        self.assertEqual(result.metadata, {"key": "value"})

    def test_processed_result_defaults(self):
        """Test ProcessedResult creation with default values."""
        result = ProcessedResult("test content")

        self.assertEqual(result.normalized_content, "test content")
        self.assertEqual(result.triple_count, 0)
        self.assertEqual(result.variables, [])
        self.assertFalse(result.is_sorted)
        self.assertEqual(result.metadata, {})


class TestResultProcessor(unittest.TestCase):
    """Test ResultProcessor class."""

    def setUp(self):
        """Set up test fixtures."""
        self.processor = ResultProcessor()

    def test_normalize_blank_nodes(self):
        """Test blank node normalization."""
        content = "_:b1 <p> <o> .\n_:b2 <p> <o> .\n_:b1 <p> <o> ."
        normalized = self.processor.normalize_blank_nodes(content)

        # Should normalize blank nodes consistently
        self.assertIn("_:b0", normalized)
        self.assertIn("_:b1", normalized)
        self.assertNotIn("_:b2", normalized)  # b1 should be reused

    @patch("sparql_test_framework.runners.result_processor.find_tool")
    @patch("sparql_test_framework.runners.result_processor.run_command")
    def test_convert_turtle_to_ntriples(self, mock_run_command, mock_find_tool):
        """Test Turtle to N-Triples conversion."""
        mock_find_tool.return_value = "/usr/bin/to-ntriples"
        mock_run_command.return_value = (0, "<s> <p> <o> .", "")

        turtle_content = "@prefix : <http://example.org/> .\n:s :p :o ."
        result = self.processor.convert_turtle_to_ntriples(turtle_content)

        self.assertEqual(result, "<s> <p> <o> .")

    def test_process_ntriples_result(self):
        """Test N-Triples result processing."""
        content = "<s1> <p1> <o1> .\n<s2> <p2> <o2> ."
        result = self.processor.process_ntriples_result(content)

        self.assertEqual(result.triple_count, 2)
        self.assertTrue(result.is_sorted)
        self.assertIn("<s1> <p1> <o1> .", result.normalized_content)

    def test_process_csv_results(self):
        """Test CSV result processing."""
        csv_content = "s,p,o\n1,2,3\n4,5,6"
        result = self.processor.process_select_results(
            csv_content, ["s", "p", "o"], "csv"
        )

        self.assertEqual(result.triple_count, 2)
        self.assertEqual(result.variables, ["s", "p", "o"])

    def test_process_boolean_result(self):
        """Test boolean result processing."""
        srj_content = '{"boolean": true}'
        result = self.processor.process_boolean_result(srj_content, "srj")

        self.assertEqual(result.triple_count, 1)
        self.assertEqual(result.metadata["boolean_value"], True)

    def test_extract_boolean_value(self):
        """Test boolean value extraction."""
        # Test SRJ format
        self.assertTrue(
            self.processor._extract_boolean_value('{"boolean": true}', "srj")
        )
        self.assertFalse(
            self.processor._extract_boolean_value('{"boolean": false}', "srj")
        )

        # Test text format
        self.assertTrue(self.processor._extract_boolean_value("true", "text"))
        self.assertFalse(self.processor._extract_boolean_value("false", "text"))


class TestSRJResult(unittest.TestCase):
    """Test SRJResult class."""

    def test_srj_result_creation(self):
        """Test SRJResult creation with all parameters."""
        result = SRJResult(
            json_content='{"key": "value"}',
            normalized_content='{"key":"value"}',
            result_type="bindings",
            variables=["s", "p"],
            bindings_count=5,
            boolean_value=None,
            metadata={"key": "value"},
        )

        self.assertEqual(result.json_content, '{"key": "value"}')
        self.assertEqual(result.normalized_content, '{"key":"value"}')
        self.assertEqual(result.result_type, "bindings")
        self.assertEqual(result.variables, ["s", "p"])
        self.assertEqual(result.bindings_count, 5)
        self.assertIsNone(result.boolean_value)
        self.assertEqual(result.metadata, {"key": "value"})

    def test_srj_result_defaults(self):
        """Test SRJResult creation with default values."""
        result = SRJResult("json", "normalized", "boolean")

        self.assertEqual(result.json_content, "json")
        self.assertEqual(result.normalized_content, "normalized")
        self.assertEqual(result.result_type, "boolean")
        self.assertEqual(result.variables, [])
        self.assertEqual(result.bindings_count, 0)
        self.assertIsNone(result.boolean_value)
        self.assertEqual(result.metadata, {})


class TestSRJHandler(unittest.TestCase):
    """Test SRJHandler class."""

    def setUp(self):
        """Set up test fixtures."""
        self.handler = SRJHandler()
        self.mock_config = Mock(spec=TestConfig)

    def test_handle_ask_query_srj(self):
        """Test ASK query SRJ handling."""
        srj_output = '{"head": {}, "boolean": true}'

        result = self.handler.handle_ask_query_srj(self.mock_config, srj_output)

        self.assertEqual(result.result_type, "boolean")
        self.assertTrue(result.boolean_value)
        self.assertIn("boolean", result.normalized_content)

    def test_handle_select_query_srj(self):
        """Test SELECT query SRJ handling."""
        srj_output = (
            '{"head": {"vars": ["s", "p"]}, "results": {"bindings": [{"s": "1"}]}}'
        )

        result = self.handler.handle_select_query_srj(self.mock_config, srj_output)

        self.assertEqual(result.result_type, "bindings")
        self.assertEqual(result.variables, ["s", "p"])
        self.assertEqual(result.bindings_count, 1)

    def test_handle_invalid_json(self):
        """Test handling of invalid JSON."""
        invalid_json = "invalid json content"

        with self.assertRaises(SRJProcessingError):
            self.handler.handle_ask_query_srj(self.mock_config, invalid_json)

    def test_normalize_srj_json(self):
        """Test SRJ JSON normalization."""
        json_data = {
            "head": {"vars": ["s", "p"]},
            "results": {"bindings": [{"s": "_:b1", "p": "_:b2"}]},
        }

        normalized = self.handler.normalize_srj_json(json_data)

        # Should be valid JSON
        parsed = json.loads(normalized)
        self.assertIn("head", parsed)
        self.assertIn("results", parsed)

    def test_extract_variables_from_query(self):
        """Test variable extraction from SPARQL queries."""
        query = "SELECT ?s ?p ?o WHERE { ?s ?p ?o }"
        variables = self.handler.extract_variables_from_query(query)

        self.assertEqual(set(variables), {"s", "p", "o"})

    def test_validate_srj_structure_valid(self):
        """Test valid SRJ structure validation."""
        valid_srj = {"head": {"vars": ["s"]}, "results": {"bindings": []}}

        self.assertTrue(self.handler.validate_srj_structure(valid_srj))

    def test_validate_srj_structure_invalid(self):
        """Test invalid SRJ structure validation."""
        invalid_srj = {"invalid": "structure"}

        self.assertFalse(self.handler.validate_srj_structure(invalid_srj))

    def test_compare_srj_results_boolean(self):
        """Test SRJ result comparison for boolean results."""
        result1 = SRJResult("json1", "norm1", "boolean", boolean_value=True)
        result2 = SRJResult("json2", "norm1", "boolean", boolean_value=True)

        self.assertTrue(self.handler.compare_srj_results(result1, result2))

    def test_compare_srj_results_bindings(self):
        """Test SRJ result comparison for bindings results."""
        result1 = SRJResult(
            "json1", "norm1", "bindings", variables=["s"], bindings_count=1
        )
        result2 = SRJResult(
            "json2", "norm1", "bindings", variables=["s"], bindings_count=1
        )

        self.assertTrue(self.handler.compare_srj_results(result1, result2))


if __name__ == "__main__":
    unittest.main()

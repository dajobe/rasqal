"""
QueryExecutor class for handling SPARQL query execution.

This module provides the QueryExecutor class that handles query execution
and result processing for different SPARQL query types.

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

import logging
import re
from pathlib import Path
from typing import List, Optional, Dict, Any

from ..config import TestConfig
from ..utils import run_command, find_tool


class QueryType:
    """Represents the type of SPARQL query."""

    def __init__(self, query_content: str):
        self.is_construct = "CONSTRUCT" in query_content.upper()
        self.is_ask = "ASK" in query_content.upper()
        self.is_select = "SELECT" in query_content.upper()
        self.is_describe = "DESCRIBE" in query_content.upper()
        self.is_update = any(
            keyword in query_content.upper()
            for keyword in ["INSERT", "DELETE", "LOAD", "CLEAR", "CREATE", "DROP"]
        )


class QueryResult:
    """Represents the result of a SPARQL query execution."""

    def __init__(
        self,
        content: str,
        result_type: str,
        count: int = 0,
        vars_order: Optional[List[str]] = None,
        boolean_value: Optional[bool] = None,
        format: str = "text",
        metadata: Optional[Dict[str, Any]] = None,
    ):
        self.content = content
        self.result_type = result_type  # "graph", "bindings", "boolean"
        self.count = count
        self.vars_order = vars_order or []
        self.boolean_value = boolean_value
        self.format = format
        self.metadata = metadata or {}


class QueryExecutor:
    """Handles query execution and result processing for different SPARQL query types."""

    def __init__(self):
        self.logger = logging.getLogger(__name__)
        self.roqet_path = None

    def _ensure_roqet_available(self) -> str:
        """Ensure roqet is available and return its path."""
        if not self.roqet_path:
            self.roqet_path = find_tool("roqet")
            if not self.roqet_path:
                raise RuntimeError("Could not find 'roqet' command")
        return self.roqet_path

    def detect_query_type(self, config: TestConfig) -> QueryType:
        """Detect the type of SPARQL query from the query content."""
        try:
            # First try to get query from direct attribute (for tests)
            query_content = getattr(config, "query", None)
            if query_content is None:
                # Fall back to reading from file
                query_content = config.test_file.read_text()
        except (AttributeError, FileNotFoundError):
            # Fallback for test configs that might not have test_file or file issues
            query_content = ""
        return QueryType(query_content)

    def build_roqet_args(
        self, config: TestConfig, query_type: QueryType, result_format: str = None
    ) -> List[str]:
        """Build roqet command line arguments for the given test configuration."""
        args = [self._ensure_roqet_available(), "-q", str(config.test_file)]

        # Check if this is a syntax test by looking at the actual test type
        # Don't guess based on test name - use the manifest test type
        test_type = getattr(config, "test_type", "")
        is_syntax_test = False

        if test_type:
            # Check if this is actually a syntax test type from the manifest
            from ..test_types import SYNTAX_TEST_TYPES

            is_syntax_test = test_type in SYNTAX_TEST_TYPES

        if is_syntax_test:
            # For actual syntax tests, only parse, don't execute
            args.extend(["-n", "-W", "0"])
        else:
            # Add data files for regular tests
            for data_file in config.data_files:
                args.extend(["-D", str(data_file)])

            # Add result format - use provided format or default based on query type
            if result_format:
                args.extend(["-r", result_format])
            else:
                # Fallback to default formats based on query type
                if query_type.is_construct or query_type.is_describe:
                    args.extend(["-r", "turtle"])
                elif query_type.is_select:
                    args.extend(["-r", "csv"])
                elif query_type.is_ask:
                    args.extend(["-r", "srj"])

        return args

    def execute_construct_query(self, config: TestConfig) -> QueryResult:
        """Execute a CONSTRUCT or DESCRIBE query and return the result."""
        query_type = self.detect_query_type(config)
        if not (query_type.is_construct or query_type.is_describe):
            raise ValueError("Query is not a CONSTRUCT or DESCRIBE query")

        args = self.build_roqet_args(config, query_type)
        self.logger.debug(f"Executing CONSTRUCT/DESCRIBE query: {' '.join(args)}")

        returncode, stdout, stderr = run_command(cmd=args, cwd=str(Path.cwd()))

        if returncode not in [0, 2]:
            raise RuntimeError(f"roqet failed with exit code {returncode}: {stderr}")

        # Count triples in result
        triple_count = len([line for line in stdout.splitlines() if line.strip()])

        # Add metadata about warnings and exit code
        metadata = {
            "exit_code": returncode,
            "warnings_generated": returncode == 2,
            "stderr": stderr,
        }

        return QueryResult(
            content=stdout,
            result_type="graph",
            count=triple_count,
            format="ntriples",
            metadata=metadata,
        )

    def execute_ask_query(self, config: TestConfig) -> QueryResult:
        """Execute an ASK query and return the result."""
        query_type = self.detect_query_type(config)
        if not query_type.is_ask:
            raise ValueError("Query is not an ASK query")

        args = self.build_roqet_args(config, query_type)
        self.logger.debug(f"Executing ASK query: {' '.join(args)}")

        returncode, stdout, stderr = run_command(cmd=args, cwd=str(Path.cwd()))

        if returncode not in [0, 2]:
            raise RuntimeError(f"roqet failed with exit code {returncode}: {stderr}")

        # Parse boolean result from SRJ output
        boolean_value = self._parse_ask_result(stdout)

        # Add metadata about warnings and exit code
        metadata = {
            "exit_code": returncode,
            "warnings_generated": returncode == 2,
            "stderr": stderr,
        }

        return QueryResult(
            content=stdout,
            result_type="boolean",
            count=1,
            boolean_value=boolean_value,
            format="srj",
            metadata=metadata,
        )

    def execute_select_query(self, config: TestConfig) -> QueryResult:
        """Execute a SELECT query and return the result."""
        query_type = self.detect_query_type(config)
        if not query_type.is_select:
            raise ValueError("Query is not a SELECT query")

        # Determine output format based on expected result file
        result_format = self._determine_result_format(config)

        args = self.build_roqet_args(config, query_type, result_format)
        self.logger.debug(f"Executing SELECT query: {' '.join(args)}")
        self.logger.debug(f"Working directory: {Path.cwd()}")

        returncode, stdout, stderr = run_command(cmd=args, cwd=str(Path.cwd()))

        self.logger.debug(f"Roqet return code: {returncode}")
        self.logger.debug(f"Roqet stdout length: {len(stdout)}")
        self.logger.debug(f"Roqet stderr length: {len(stderr)}")
        if stdout:
            self.logger.debug(f"Roqet stdout (first 500 chars): {repr(stdout[:500])}")
        if stderr:
            self.logger.debug(f"Roqet stderr (first 500 chars): {repr(stderr[:500])}")

        if returncode not in [0, 2]:
            raise RuntimeError(f"roqet failed with exit code {returncode}: {stderr}")

        # Extract variable order and count bindings
        try:
            # First try to get query from direct attribute (for tests)
            query_content = getattr(config, "query", None)
            if query_content is None:
                # Fall back to reading from file
                query_content = config.test_file.read_text()
        except (AttributeError, FileNotFoundError):
            query_content = ""
        vars_order = self._extract_variables_from_query(query_content)
        binding_count = self._count_bindings(stdout, result_format)

        # Add metadata about warnings and exit code
        metadata = {
            "exit_code": returncode,
            "warnings_generated": returncode == 2,
            "stderr": stderr,
        }

        return QueryResult(
            content=stdout,
            result_type="bindings",
            count=binding_count,
            vars_order=vars_order,
            format=result_format,
            metadata=metadata,
        )

    def _determine_result_format(self, config: TestConfig) -> str:
        """Determine the appropriate result format based on expected result file."""
        if not config.result_file or not config.result_file.exists():
            return "csv"  # Default format

        # Read first few lines to detect format
        try:
            with open(config.result_file, "r") as f:
                first_line = f.readline().strip()

            if first_line.startswith("<?xml") or first_line.startswith("<sparql"):
                return "xml"  # SRX format
            elif first_line.startswith("{") or '"results"' in first_line:
                return "srj"  # SRJ/JSON format
            else:
                return "csv"  # Default to CSV for other formats
        except Exception:
            return "csv"  # Fall back to CSV if detection fails

    def execute_srj_query(self, config: TestConfig) -> QueryResult:
        """Execute a query with SRJ output format."""
        query_type = self.detect_query_type(config)

        if query_type.is_ask:
            return self.execute_ask_query(config)
        elif query_type.is_select:
            return self.execute_select_query(config)
        else:
            raise ValueError("SRJ format only supported for ASK and SELECT queries")

    def execute_query(self, config: TestConfig) -> QueryResult:
        """Execute a query based on its type and return the result."""
        query_type = self.detect_query_type(config)

        if query_type.is_construct or query_type.is_describe:
            return self.execute_construct_query(config)
        elif query_type.is_ask:
            return self.execute_ask_query(config)
        elif query_type.is_select:
            return self.execute_select_query(config)
        else:
            # Default to SELECT for unknown query types
            return self.execute_select_query(config)

    def _parse_ask_result(self, srj_output: str) -> bool:
        """Parse boolean result from SRJ output."""
        # Simple parsing - look for "boolean" field in JSON
        if '"boolean": true' in srj_output:
            return True
        elif '"boolean": false' in srj_output:
            return False
        else:
            self.logger.warning("Could not parse boolean result from SRJ output")
            return False

    def _extract_variables_from_query(self, query_content: str) -> List[str]:
        """Extract variable names from SELECT query."""
        # Simple regex to find variables in SELECT clause
        select_match = re.search(
            r"SELECT\s+(.+?)\s+WHERE", query_content, re.IGNORECASE | re.DOTALL
        )
        if not select_match:
            return []

        select_clause = select_match.group(1)
        # Find variables (words starting with ? or $)
        variables = re.findall(r"[?$](\w+)", select_clause)
        return variables

    def _count_bindings(self, output: str, format: str) -> int:
        """Count the number of bindings in the result."""
        if format == "srj":
            # Count bindings in SRJ output - look for actual binding objects
            try:
                # Try to parse as JSON first
                import json

                data = json.loads(output)
                if "results" in data and "bindings" in data["results"]:
                    return len(data["results"]["bindings"])
            except:
                pass
            # Fallback: count lines with "bindings" in SRJ output
            return len([line for line in output.splitlines() if '"bindings"' in line])
        elif format == "csv":
            # Count non-header lines in CSV output
            lines = output.splitlines()
            if len(lines) <= 1:  # Only header or empty
                return 0
            return len(lines) - 1  # Subtract header
        else:
            # Default: count non-empty lines
            return len([line for line in output.splitlines() if line.strip()])

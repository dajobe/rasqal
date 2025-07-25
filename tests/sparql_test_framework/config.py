"""
Test configuration and suite management for SPARQL Test Framework

This module contains all configuration-related classes including test configuration,
test suite definitions, and configuration builders.

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

from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path
from typing import List, Dict, Any, Optional

from .test_types import TestResult, TestType, Namespaces


class TestExecutionMode(Enum):
    """Different modes for test execution based on what's being tested."""

    LEXER_ONLY = "lexer-only"  # Only lexical analysis
    PARSER_ONLY = "parser-only"  # Syntax parsing only
    FULL_EXECUTION = "full-execution"  # Complete query execution


@dataclass
class TestSuiteConfig:
    """Configuration for a test suite defining its behavior."""

    name: str
    execution_mode: TestExecutionMode
    test_id_predicate: str  # Which predicate to use for test IDs
    supports_negative_tests: bool = True
    supports_execution: bool = True
    comment_template: Optional[str] = (
        None  # Custom template override, None for defaults
    )

    def get_test_id_predicate(self) -> str:
        """Get the RDF predicate to use for extracting test IDs."""
        return self.test_id_predicate


@dataclass
class TestConfig:
    """Represents the configuration for a single SPARQL test."""

    # Unsupported test types that should be skipped
    UNSUPPORTED_TEST_TYPES = [
        TestType.UPDATE_EVALUATION_TEST.value,
        f"{Namespaces.MF}UpdateEvaluationTest",
        TestType.PROTOCOL_TEST.value,
    ]

    name: str
    test_uri: str
    test_file: Path
    expect: TestResult
    language: str = "sparql"
    execute: bool = True
    cardinality_mode: str = "strict"
    is_withdrawn: bool = False
    is_approved: bool = False
    has_entailment_regime: bool = False
    test_type: Optional[str] = None
    data_files: List[Path] = field(default_factory=list)
    named_data_files: List[Path] = field(default_factory=list)
    result_file: Optional[Path] = None
    extra_files: List[Path] = field(default_factory=list)
    warning_level: int = 0

    def _get_skip_condition(self, approved_only: bool = False) -> Optional[str]:
        """
        Check if test should be skipped and return the reason.

        Args:
            approved_only: If True, check approved-only filtering

        Returns:
            Reason string if test should be skipped, None if it should run
        """
        if self.is_withdrawn:
            return "withdrawn"
        if approved_only and not self.is_approved:
            return "not approved"
        if self.has_entailment_regime:
            return "has entailment regime"
        if self.test_type in self.UNSUPPORTED_TEST_TYPES:
            return "unsupported test type"
        return None

    def should_run_test(self, approved_only: bool = False) -> bool:
        """
        Determines if this test should be run based on filtering criteria.

        Args:
            approved_only: If True, only run tests explicitly marked as approved

        Returns:
            True if the test should be run, False if it should be skipped
        """
        skip_reason = self._get_skip_condition(approved_only)
        return skip_reason is None

    def get_skip_reason(self, approved_only: bool = False) -> Optional[str]:
        """
        Gets the reason why this test would be skipped.

        Args:
            approved_only: If True, check approved-only filtering

        Returns:
            Reason string if test would be skipped, None if it would run
        """
        return self._get_skip_condition(approved_only)


class TestConfigBuilder:
    """Helper class for building TestConfig objects from manifest data."""

    def __init__(self, extractor, path_resolver):
        """
        Initialize builder with manifest extractor and path resolver.

        Args:
            extractor: ManifestTripleExtractor instance
            path_resolver: PathResolver instance
        """
        self.extractor = extractor
        self.path_resolver = path_resolver

    def _extract_metadata(self, entry_uri_full: str) -> Dict[str, Any]:
        """Extract basic metadata from the test entry.

        Args:
            entry_uri_full: Full URI of the test entry

        Returns:
            Dictionary containing extracted metadata
        """
        name = self.extractor.unquote(
            self.extractor.get_obj_val(entry_uri_full, f"<{Namespaces.MF}name>")
        )

        return {
            "name": name,
            "test_uri": self.extractor.unquote(entry_uri_full),
        }

    def _extract_file_lists(
        self, entry_uri_full: str, action_node: str
    ) -> Dict[str, List[Path]]:
        """Extract file lists from the test entry and action.

        Args:
            entry_uri_full: Full URI of the test entry
            action_node: URI of the action node

        Returns:
            Dictionary containing extracted file lists
        """
        data_files = self.path_resolver.resolve_paths(
            self.extractor.get_obj_vals(action_node, f"<{Namespaces.QT}data>")
        )
        named_data_files = self.path_resolver.resolve_paths(
            self.extractor.get_obj_vals(action_node, f"<{Namespaces.QT}graphData>")
        )
        extra_files = self.path_resolver.resolve_paths(
            self.extractor.get_obj_vals(entry_uri_full, f"<{Namespaces.T}extraFile>")
        )

        return {
            "data_files": data_files,
            "named_data_files": named_data_files,
            "extra_files": extra_files,
        }

    def _extract_test_flags(
        self, entry_uri_full: str, action_node: str
    ) -> Dict[str, Any]:
        """Extract test flags and approval status.

        Args:
            entry_uri_full: Full URI of the test entry
            action_node: URI of the action node

        Returns:
            Dictionary containing extracted test flags
        """
        # Extract approval status
        approval = self.extractor.unquote(
            self.extractor.get_obj_val(entry_uri_full, f"<{Namespaces.DAWGT}approval>")
        )
        is_withdrawn = approval == f"{Namespaces.DAWGT}Withdrawn"
        is_approved = approval == f"{Namespaces.DAWGT}Approved"

        # Extract cardinality mode
        cardinality = self.extractor.unquote(
            self.extractor.get_obj_val(
                entry_uri_full, f"<{Namespaces.MF}resultCardinality>"
            )
        )
        cardinality_mode = (
            "lax" if cardinality == f"{Namespaces.MF}LaxCardinality" else "strict"
        )

        # Extract entailment regime (check both legacy and SPARQL 1.1 predicates)
        has_entailment_regime = bool(
            self.extractor.get_obj_val(action_node, f"<{Namespaces.T}entailmentRegime>")
            or self.extractor.get_obj_val(
                action_node, f"<{Namespaces.SPARQL}entailmentRegime>"
            )
        )

        return {
            "is_withdrawn": is_withdrawn,
            "is_approved": is_approved,
            "cardinality_mode": cardinality_mode,
            "has_entailment_regime": has_entailment_regime,
        }

    def build_test_config(
        self,
        entry_uri_full: str,
        action_node: str,
        query_node: str,
        test_file: Path,
        execute: bool,
        expect: TestResult,
        language: str,
        query_type: str,
    ) -> TestConfig:
        """Build a TestConfig object from parsed manifest data.

        Args:
            entry_uri_full: Full URI of the test entry
            action_node: URI of the action node
            query_node: URI of the query node
            test_file: Path to the test file
            execute: Whether to execute the test
            expect: Expected test result
            language: SPARQL language version
            query_type: Test type string

        Returns:
            Constructed TestConfig object
        """
        # Extract metadata using helper methods
        metadata = self._extract_metadata(entry_uri_full)
        file_lists = self._extract_file_lists(entry_uri_full, action_node)
        test_flags = self._extract_test_flags(entry_uri_full, action_node)

        # Extract result file
        result_file = self.path_resolver.resolve_path(
            self.extractor.get_obj_val(entry_uri_full, f"<{Namespaces.MF}result>")
        )

        return TestConfig(
            name=metadata["name"],
            test_uri=metadata["test_uri"],
            test_file=test_file,
            data_files=file_lists["data_files"],
            named_data_files=file_lists["named_data_files"],
            result_file=result_file,
            extra_files=file_lists["extra_files"],
            expect=expect,
            test_type=query_type,
            execute=execute,
            language=language,
            cardinality_mode=test_flags["cardinality_mode"],
            is_withdrawn=test_flags["is_withdrawn"],
            is_approved=test_flags["is_approved"],
            has_entailment_regime=test_flags["has_entailment_regime"],
        )


# Define known test suites
KNOWN_TEST_SUITES = {
    # SPARQL Lexer tests (lexical analysis only)
    "sparql-lexer": TestSuiteConfig(
        name="sparql-lexer",
        execution_mode=TestExecutionMode.LEXER_ONLY,
        test_id_predicate="<http://www.w3.org/2001/sw/DataAccess/tests/test-query#query>",
        supports_negative_tests=False,
        supports_execution=False,
    ),
    # SPARQL Parser tests (syntax parsing only)
    "sparql-parser": TestSuiteConfig(
        name="sparql-parser",
        execution_mode=TestExecutionMode.PARSER_ONLY,
        test_id_predicate="<http://www.w3.org/2001/sw/DataAccess/tests/test-query#query>",
        supports_negative_tests=True,
        supports_execution=False,
    ),
    "sparql-parser-positive": TestSuiteConfig(
        name="sparql-parser-positive",
        execution_mode=TestExecutionMode.PARSER_ONLY,
        test_id_predicate="<http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#name>",
        supports_negative_tests=False,
        supports_execution=False,
    ),
    "sparql-parser-negative": TestSuiteConfig(
        name="sparql-parser-negative",
        execution_mode=TestExecutionMode.PARSER_ONLY,
        test_id_predicate="<http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#name>",
        supports_negative_tests=True,
        supports_execution=False,
    ),
    "sparql-bad-syntax": TestSuiteConfig(
        name="sparql-bad-syntax",
        execution_mode=TestExecutionMode.PARSER_ONLY,
        test_id_predicate="<http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#name>",
        supports_negative_tests=True,
        supports_execution=False,
    ),
    # SPARQL Query execution tests (full execution)
    "sparql-query": TestSuiteConfig(
        name="sparql-query",
        execution_mode=TestExecutionMode.FULL_EXECUTION,
        test_id_predicate="<http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#name>",
        supports_negative_tests=True,
        supports_execution=True,
    ),
    "sparql-query-negative": TestSuiteConfig(
        name="sparql-query-negative",
        execution_mode=TestExecutionMode.FULL_EXECUTION,
        test_id_predicate="<http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#name>",
        supports_negative_tests=True,
        supports_execution=True,
    ),
    "sparql-algebra": TestSuiteConfig(
        name="sparql-algebra",
        execution_mode=TestExecutionMode.FULL_EXECUTION,
        test_id_predicate="<http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#name>",
        supports_negative_tests=True,
        supports_execution=True,
    ),
    # SPARQL Warning tests
    "sparql-warnings": TestSuiteConfig(
        name="sparql-warnings",
        execution_mode=TestExecutionMode.FULL_EXECUTION,
        test_id_predicate="<http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#name>",
        supports_negative_tests=False,
        supports_execution=True,
    ),
    # SRJ Reader tests (SPARQL Results JSON format reading)
    "format-srj-read": TestSuiteConfig(
        name="format-srj-read",
        execution_mode=TestExecutionMode.FULL_EXECUTION,
        test_id_predicate="<http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#name>",
        supports_negative_tests=False,
        supports_execution=True,
    ),
    # SRJ Writer tests (SPARQL Results JSON format writing)
    "format-srj-write": TestSuiteConfig(
        name="format-srj-write",
        execution_mode=TestExecutionMode.FULL_EXECUTION,
        test_id_predicate="<http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#name>",
        supports_negative_tests=False,
        supports_execution=True,
    ),
    # LAQRS tests (Language for Advanced Querying of RDF Streams)
    "laqrs-parser-positive": TestSuiteConfig(
        name="laqrs-parser-positive",
        execution_mode=TestExecutionMode.PARSER_ONLY,
        test_id_predicate="<http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#name>",
        supports_negative_tests=False,
        supports_execution=False,
    ),
    "laqrs-parser-negative": TestSuiteConfig(
        name="laqrs-parser-negative",
        execution_mode=TestExecutionMode.PARSER_ONLY,
        test_id_predicate="<http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#name>",
        supports_negative_tests=True,
        supports_execution=False,
    ),
    # Engine tests (C executable tests)
    "engine": TestSuiteConfig(
        name="engine",
        execution_mode=TestExecutionMode.FULL_EXECUTION,
        test_id_predicate="<http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#name>",
        supports_negative_tests=False,
        supports_execution=True,
    ),
    "engine-limit": TestSuiteConfig(
        name="engine-limit",
        execution_mode=TestExecutionMode.FULL_EXECUTION,
        test_id_predicate="<http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#name>",
        supports_negative_tests=False,
        supports_execution=True,
    ),
    # CSV/TSV Format tests
    "format-csv-tsv": TestSuiteConfig(
        name="format-csv-tsv",
        execution_mode=TestExecutionMode.FULL_EXECUTION,
        test_id_predicate="<http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#name>",
        supports_negative_tests=False,
        supports_execution=True,
    ),
    # SPARQL 1.1 Query tests
    "sparql11-query": TestSuiteConfig(
        name="sparql11-query",
        execution_mode=TestExecutionMode.FULL_EXECUTION,
        test_id_predicate="<http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#name>",
        supports_negative_tests=True,
        supports_execution=True,
    ),
    # SPARQL 1.1 Update tests
    "sparql11-update": TestSuiteConfig(
        name="sparql11-update",
        execution_mode=TestExecutionMode.FULL_EXECUTION,
        test_id_predicate="<http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#name>",
        supports_negative_tests=True,
        supports_execution=True,
    ),
    # SPARQL 1.1 Results format tests
    "sparql11-results": TestSuiteConfig(
        name="sparql11-results",
        execution_mode=TestExecutionMode.FULL_EXECUTION,
        test_id_predicate="<http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#name>",
        supports_negative_tests=False,
        supports_execution=True,
    ),
    # SPARQL 1.1 Federated tests
    "sparql11-federated": TestSuiteConfig(
        name="sparql11-federated",
        execution_mode=TestExecutionMode.FULL_EXECUTION,
        test_id_predicate="<http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#name>",
        supports_negative_tests=True,
        supports_execution=True,
    ),
}


def get_test_suite_config(suite_name: str) -> TestSuiteConfig:
    """Get configuration for a test suite by name.

    Args:
        suite_name: Name of the test suite

    Returns:
        TestSuiteConfig for the named suite, or default config for unknown suites
    """
    if suite_name not in KNOWN_TEST_SUITES:
        # Default to full execution for unknown suites
        return TestSuiteConfig(
            name=suite_name,
            execution_mode=TestExecutionMode.FULL_EXECUTION,
            test_id_predicate="<http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#name>",
            supports_negative_tests=True,
            supports_execution=True,
        )
    return KNOWN_TEST_SUITES[suite_name]


def get_available_test_suites() -> List[str]:
    """Get list of available test suite names.

    Returns:
        List of known test suite names
    """
    return list(KNOWN_TEST_SUITES.keys())


def validate_test_suite_name(suite_name: str) -> bool:
    """Validate if a test suite name is known.

    Args:
        suite_name: Name to validate

    Returns:
        True if the suite name is known, False otherwise
    """
    return suite_name in KNOWN_TEST_SUITES


def get_default_comment_template(suite_name: str) -> str:
    """Generate a default comment template based on suite name and configuration.

    Args:
        suite_name: Name of the test suite

    Returns:
        Default comment template string
    """
    config = get_test_suite_config(suite_name)
    parts = suite_name.split("-")
    language = parts[0]  # sparql, laqrs, engine
    test_type = parts[1] if len(parts) > 1 else ""
    mode = parts[2] if len(parts) > 2 else ""

    if language == "engine":
        if test_type == "limit":
            return "engine limit test {test_id}"
        return "engine test {test_id}"
    elif test_type == "warnings":
        return f"{language} warning {{test_id}}"
    elif test_type == "lexer":
        return f"{language} lexer of {{query_file}}"
    elif test_type == "parser":
        if mode == "negative":
            return f"{language} failing to parse of {{query_file}}"
        else:
            return f"{language} parser of {{query_file}}"
    elif test_type == "query":
        return f"{language} query {{test_id}}"
    else:
        return f"{language} {test_type} {{test_id}}"


def get_comment_template(suite_name: str, custom_template: Optional[str] = None) -> str:
    """Get the comment template for a test suite, using custom override or default.

    Args:
        suite_name: Name of the test suite
        custom_template: Optional custom template override

    Returns:
        Comment template string
    """
    if custom_template:
        return custom_template

    config = get_test_suite_config(suite_name)
    if config.comment_template:
        return config.comment_template

    return get_default_comment_template(suite_name)

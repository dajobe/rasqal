#!/usr/bin/env python3
#
# rasqal_test_util.py - Shared library for Rasqal's Python-based test suite.
#
# Copyright (C) 2025, David Beckett http://www.dajobe.org/
#
# This package is Free Software and part of Redland http://librdf.org/
#
# It is licensed under the following three licenses as alternatives:
#    1. GNU Lesser General Public License (LGPL) V2.1 or any newer version
#    2. GNU General Public License (GPL) V2 or any newer version
#    3. Apache License, V2.0 or any newer version
#
# You may not use this file except in compliance with at least one of
# the above three licenses.
#
# See LICENSE.html or LICENSE.txt at the top of this package for the
# complete terms and further detail along with the license texts for
# the licenses in COPYING.LIB, COPYING and LICENSE-2.0.txt respectively.
#

import os
import sys
import subprocess
import logging
import shutil
from enum import Enum
from pathlib import Path
from typing import List, Dict, Any, Optional, Tuple, Union

import re
from dataclasses import dataclass, field
import argparse

# Add the parent directory to the Python path to find rasqal_test_util
sys.path.append(str(Path(__file__).resolve().parent))

logger = logging.getLogger(__name__)

# --- Test Suite Configuration System ---


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
    # SRJ Writer tests (SPARQL Results JSON format writing)
    "srj-writer": TestSuiteConfig(
        name="srj-writer",
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
}


def get_test_suite_config(suite_name: str) -> TestSuiteConfig:
    """Get configuration for a test suite by name."""
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
    """Get list of available test suite names."""
    return list(KNOWN_TEST_SUITES.keys())


def validate_test_suite_name(suite_name: str) -> bool:
    """Validate if a test suite name is known."""
    return suite_name in KNOWN_TEST_SUITES


def get_default_comment_template(suite_name: str) -> str:
    """Generate a default comment template based on suite name and configuration."""
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
    """Get the comment template for a test suite, using custom override or default."""
    if custom_template:
        return custom_template

    config = get_test_suite_config(suite_name)
    if config.comment_template:
        return config.comment_template

    return get_default_comment_template(suite_name)


# --- Custom Exceptions ---
class SparqlTestError(Exception):
    """Base exception for SPARQL test runner errors."""

    pass


class ManifestParsingError(SparqlTestError):
    """Raised when there's an issue parsing the test manifest."""

    pass


class UtilityNotFoundError(SparqlTestError):
    """Raised when an external utility (roqet, to-ntriples, diff) is not found."""

    pass


# --- Shared Constants and Enums ---


class Namespaces:
    """Convenience class for RDF/Manifest namespaces."""

    MF = "http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#"
    RDF = "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
    RDFS = "http://www.w3.org/2000/01/rdf-schema#"
    T = "http://ns.librdf.org/2009/test-manifest#"
    QT = "http://www.w3.org/2001/sw/DataAccess/tests/test-query#"
    DAWGT = "http://www.w3.org/2001/sw/DataAccess/tests/test-dawg#"
    UT = "http://www.w3.org/2009/sparql/tests/test-update#"
    RS = "http://www.w3.org/2001/sw/DataAccess/tests/result-set#"
    MFX = "http://jena.hpl.hp.com/2005/05/test-manifest-extra#"


class TestResult(Enum):
    PASSED = "passed"
    FAILED = "failed"
    SKIPPED = "skipped"
    XFAILED = "xfailed"  # Expected to fail and it did
    UXPASSED = "uxpassed"  # Expected to fail but unexpectedly passed

    def __str__(self):
        return self.value

    def display_char(self) -> str:
        """Returns a single character for compact progress display."""
        char_map = {
            TestResult.PASSED: ".",
            TestResult.FAILED: "F",
            TestResult.XFAILED: "*",
            TestResult.UXPASSED: "!",
            TestResult.SKIPPED: "-",
        }
        return char_map.get(self, "?")

    def display_name(self) -> str:
        """Returns a more verbose name for display."""
        return (
            self.value.upper()
            if self not in (TestResult.PASSED, TestResult.SKIPPED)
            else self.value
        )


class TestType(Enum):
    """Types of SPARQL tests as defined in W3C manifests."""

    CSV_RESULT_FORMAT_TEST = f"{Namespaces.MF}CSVResultFormatTest"
    POSITIVE_SYNTAX_TEST = f"{Namespaces.MF}PositiveSyntaxTest"
    POSITIVE_SYNTAX_TEST_11 = f"{Namespaces.MF}PositiveSyntaxTest11"
    POSITIVE_UPDATE_SYNTAX_TEST_11 = f"{Namespaces.UT}PositiveUpdateSyntaxTest11"
    NEGATIVE_SYNTAX_TEST = f"{Namespaces.MF}NegativeSyntaxTest"
    NEGATIVE_SYNTAX_TEST_11 = f"{Namespaces.MF}NegativeSyntaxTest11"
    NEGATIVE_UPDATE_SYNTAX_TEST_11 = f"{Namespaces.UT}NegativeUpdateSyntaxTest11"
    UPDATE_EVALUATION_TEST = f"{Namespaces.UT}UpdateEvaluationTest"
    PROTOCOL_TEST = f"{Namespaces.MF}ProtocolTest"
    BAD_SYNTAX_TEST = f"{Namespaces.MFX}TestBadSyntax"
    TEST_SYNTAX = f"{Namespaces.MFX}TestSyntax"


@dataclass
class TestConfig:
    """Represents the configuration for a single SPARQL test."""

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

    def should_run_test(self, approved_only: bool = False) -> bool:
        """
        Determines if this test should be run based on filtering criteria.

        Args:
            approved_only: If True, only run tests explicitly marked as approved

        Returns:
            True if the test should be run, False if it should be skipped
        """
        if self.is_withdrawn:
            return False
        if approved_only and not self.is_approved:
            return False
        if self.has_entailment_regime:
            return False
        if self.test_type in [
            TestType.UPDATE_EVALUATION_TEST.value,
            f"{Namespaces.MF}UpdateEvaluationTest",
            TestType.PROTOCOL_TEST.value,
        ]:
            return False
        return True

    def get_skip_reason(self, approved_only: bool = False) -> Optional[str]:
        """
        Gets the reason why this test would be skipped.

        Args:
            approved_only: If True, check approved-only filtering

        Returns:
            Reason string if test would be skipped, None if it would run
        """
        if self.is_withdrawn:
            return "withdrawn"
        if approved_only and not self.is_approved:
            return "not approved"
        if self.has_entailment_regime:
            return "has entailment regime"
        if self.test_type in [
            TestType.UPDATE_EVALUATION_TEST.value,
            f"{Namespaces.MF}UpdateEvaluationTest",
            TestType.PROTOCOL_TEST.value,
        ]:
            return "unsupported test type"
        return None


# --- Utility Functions ---


def setup_logging(
    debug: bool = False, level: Optional[int] = None, stream=sys.stderr
) -> logging.Logger:
    """
    Setup logging configuration and return logger.

    Args:
        debug: If True, sets log level to DEBUG
        level: Explicit log level (overrides debug parameter)
        stream: Output stream for logging (defaults to stderr)

    Returns:
        Configured logger instance
    """
    if level is None:
        level = logging.DEBUG if debug else logging.INFO

    logging.basicConfig(level=level, format="%(levelname)s: %(message)s", stream=stream)
    return logging.getLogger(__name__)


def add_common_arguments(parser: argparse.ArgumentParser) -> None:
    """
    Add common command-line arguments used across multiple test scripts.

    Args:
        parser: ArgumentParser instance to add arguments to
    """
    parser.add_argument(
        "-d", "--debug", action="store_true", help="Enable debug logging"
    )
    parser.add_argument(
        "--manifest-file", type=Path, help="Path to a manifest file (e.g., manifest.n3)"
    )
    parser.add_argument(
        "--srcdir",
        type=Path,
        help="The source directory (equivalent to $(srcdir) in Makefile.am)",
    )
    parser.add_argument(
        "--builddir",
        type=Path,
        help="The build directory (equivalent to $(top_builddir) in Makefile.am)",
    )


def find_tool(name: str) -> Optional[str]:
    """
    Finds a required tool (like 'roqet' or 'to-ntriples') by searching
    the environment, a relative 'utils' directory, and the system PATH.
    """
    # 1. Check environment variable
    env_var = name.upper().replace("-", "_")
    env_val = os.environ.get(env_var)
    if env_val and Path(env_val).is_file():
        logger.debug(f"Found {name} via environment variable {env_var}: {env_val}")
        return env_val

    # 2. Search in parent 'utils' directories
    current_dir = Path.cwd()
    for parent in [current_dir] + list(current_dir.parents):
        tool_path = parent / "utils" / name
        if tool_path.is_file() and os.access(tool_path, os.X_OK):
            logger.debug(f"Found {name} in relative utils directory: {tool_path}")
            return str(tool_path)

    # 3. Fallback to system PATH
    tool_path_in_path = shutil.which(name)
    if tool_path_in_path:
        logger.debug(f"Found {name} in system PATH: {tool_path_in_path}")
        return tool_path_in_path

    logger.warning(f"Tool '{name}' could not be found.")
    return None


def run_command(
    cmd: List[str], cwd: Path, error_prefix: str
) -> subprocess.CompletedProcess:
    """
    Runs a shell command and returns its CompletedProcess object, handling common errors.
    """
    try:
        logger.debug(f"Running command in {cwd}: {' '.join(cmd)}")
        process = subprocess.run(
            cmd, capture_output=True, text=True, cwd=cwd, check=False, encoding="utf-8"
        )
        return process
    except FileNotFoundError:
        raise RuntimeError(
            f"{error_prefix}. Command not found: '{cmd[0]}'. Please ensure it is installed and in your PATH."
        )
    except Exception as e:
        logger.error(
            f"An unexpected error occurred while running command '{cmd[0]}': {e}"
        )
        raise RuntimeError(
            f"{error_prefix}. Unexpected error running '{cmd[0]}'."
        ) from e


# --- Manifest Parsing ---


def decode_literal(lit_str: str) -> str:
    """
    Decodes an N-Triples literal string, handling common escapes.
    Expected to receive only valid literal strings (quoted, with optional suffix).
    """
    if not lit_str or not lit_str.startswith('"'):
        # If it's not a quoted string, it's either a URI or Blank Node already handled.
        # Or it's a malformed literal. Return as is.
        return lit_str

    # Attempt to extract the value part from within the quotes and unescape it.
    value_part_match = re.match(
        r'^"(?P<value>(?:[^"\\]|\\.)*)"(?:@[\w-]+|\^\^<[^>]+>)?$', lit_str
    )
    if not value_part_match:
        logger.warning(f"Malformed literal string passed to decode_literal: {lit_str}")
        return lit_str

    val = value_part_match.group("value")

    # Unescape common N-Triples sequences:
    # Order matters: replace longer sequences first to avoid partial replacements.
    val = val.replace('\\"', '"')
    val = val.replace("\\n", "\n")
    val = val.replace("\\r", "\r")
    val = val.replace("\\t", "\t")
    val = val.replace("\\\\", "\\")  # Unescape escaped backslashes last
    return val


def escape_turtle_literal(s: str) -> str:
    """Escapes a string for use as a triple-quoted Turtle literal.
    Handles backslashes, triple double quotes, and control characters (\n, \r, \t).
    """
    # Escape backslashes first
    s = s.replace("\\", "\\\\")
    # Handle triple quotes by replacing with escaped double quotes
    s = s.replace('"""', '\\"\\"\\"')
    if s.endswith('"'):  # avoid """{escaped ending with "}"""
        s = s[:-1] + '\\"'
    # Escape newlines, carriage returns, and tabs
    s = s.replace("\n", "\\n")
    s = s.replace("\r", "\\r")
    s = s.replace("\t", "\\t")
    return s


class ManifestTripleExtractor:
    """Helper class for extracting values from RDF triples."""

    def __init__(self, triples_by_subject: Dict[str, List[Dict[str, Any]]]):
        self.triples_by_subject = triples_by_subject

    def get_obj_val(self, subj: str, pred: str) -> Optional[str]:
        """Get a single object value for a subject-predicate pair."""
        return next(
            (
                t["o_full"]
                for t in self.triples_by_subject.get(subj, [])
                if t["p"] == pred
            ),
            None,
        )

    def get_obj_vals(self, subj: str, pred: str) -> List[str]:
        """Get all object values for a subject-predicate pair."""
        return [
            t["o_full"] for t in self.triples_by_subject.get(subj, []) if t["p"] == pred
        ]

    def unquote(self, val: Optional[str]) -> str:
        """Unquote a URI or literal value."""
        if not val:
            return ""
        if val.startswith("<") and val.endswith(">"):
            return val[1:-1]
        if val.startswith('"'):
            return decode_literal(val)
        return val


class PathResolver:
    """Helper class for resolving URIs to file paths."""

    def __init__(self, srcdir: Path, extractor: ManifestTripleExtractor):
        self.srcdir = srcdir
        self.extractor = extractor

    def resolve_path(self, uri_val: Optional[str]) -> Optional[Path]:
        """Resolve a URI to a file path."""
        if not uri_val:
            return None
        ps = self.extractor.unquote(uri_val)
        if ps.startswith("file:///"):
            p = Path(ps[len("file://") :])
        elif ps.startswith("file:/"):
            p = Path(ps[len("file:") :])
        else:
            p = Path(ps)
        return p.resolve() if p.is_absolute() else (self.srcdir / p).resolve()

    def resolve_paths(self, uri_vals: List[str]) -> List[Path]:
        """Resolve multiple URIs to file paths, filtering out None values."""
        return [
            p
            for p in [self.resolve_path(uri_val) for uri_val in uri_vals]
            if p is not None
        ]


class TestTypeResolver:
    """Helper class for determining test types and behavior."""

    @staticmethod
    def resolve_test_behavior(query_type: str) -> Tuple[bool, TestResult, str]:
        """
        Determine test execution behavior based on test type.

        Returns:
            Tuple of (execute, expect, language)
        """
        if not query_type:
            return True, TestResult.PASSED, "sparql"

        # Positive syntax tests
        if query_type in [
            TestType.POSITIVE_SYNTAX_TEST.value,
            TestType.POSITIVE_SYNTAX_TEST_11.value,
        ]:
            lang = "sparql11" if "11" in query_type else "sparql"
            return False, TestResult.PASSED, lang

        # Negative syntax tests
        elif query_type in [
            TestType.NEGATIVE_SYNTAX_TEST.value,
            TestType.NEGATIVE_SYNTAX_TEST_11.value,
            TestType.BAD_SYNTAX_TEST.value,
        ]:
            lang = "sparql11" if "11" in query_type else "sparql"
            return False, TestResult.FAILED, lang

        # Generic syntax tests
        elif query_type == TestType.TEST_SYNTAX.value:
            return False, TestResult.PASSED, "sparql"

        # Skip unsupported test types
        elif query_type in [
            TestType.UPDATE_EVALUATION_TEST.value,
            TestType.PROTOCOL_TEST.value,
        ]:
            return False, TestResult.SKIPPED, "sparql"

        # Default: execute normally
        return True, TestResult.PASSED, "sparql"

    @staticmethod
    def should_skip_test_type(query_type: str) -> bool:
        """Check if a test type should be skipped entirely."""
        return query_type in [
            TestType.UPDATE_EVALUATION_TEST.value,
            TestType.PROTOCOL_TEST.value,
        ]

    @staticmethod
    def classify_test_type(type_uri: str, suite_name: str, default_type: str) -> str:
        """
        Determine the test type based on the type URI and suite name.

        Args:
            type_uri: The RDF type URI from the manifest
            suite_name: Name of the test suite
            default_type: Default test type to return if no specific classification

        Returns:
            Classified test type string
        """
        suite_config = get_test_suite_config(suite_name)

        if "TestBadSyntax" in type_uri or "TestNegativeSyntax" in type_uri:
            if suite_config.supports_negative_tests:
                return "NegativeTest"
            else:
                return "PositiveTest"  # Skip negative tests for unsupported modes
        elif "XFailTest" in type_uri:
            # Expected failure tests should be marked as XFailTest for all suites
            return "XFailTest"
        elif "TestSyntax" in type_uri:
            return "PositiveTest"
        else:
            return default_type


class TestConfigBuilder:
    """Helper class for building TestConfig objects from manifest data."""

    def __init__(self, extractor: ManifestTripleExtractor, path_resolver: PathResolver):
        self.extractor = extractor
        self.path_resolver = path_resolver

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
        """Build a TestConfig object from parsed manifest data."""

        # Extract basic metadata
        name = self.extractor.unquote(
            self.extractor.get_obj_val(entry_uri_full, f"<{Namespaces.MF}name>")
        )

        # Extract file lists
        data_files = self.path_resolver.resolve_paths(
            self.extractor.get_obj_vals(action_node, f"<{Namespaces.QT}data>")
        )
        named_data_files = self.path_resolver.resolve_paths(
            self.extractor.get_obj_vals(action_node, f"<{Namespaces.QT}graphData>")
        )
        extra_files = self.path_resolver.resolve_paths(
            self.extractor.get_obj_vals(entry_uri_full, f"<{Namespaces.T}extraFile>")
        )

        # Extract result file
        result_file = self.path_resolver.resolve_path(
            self.extractor.get_obj_val(entry_uri_full, f"<{Namespaces.MF}result>")
        )

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

        # Extract entailment regime
        has_entailment_regime = bool(
            self.extractor.get_obj_val(action_node, f"<{Namespaces.T}entailmentRegime>")
        )

        return TestConfig(
            name=name,
            test_uri=self.extractor.unquote(entry_uri_full),
            test_file=test_file,
            data_files=data_files,
            named_data_files=named_data_files,
            result_file=result_file,
            extra_files=extra_files,
            expect=expect,
            test_type=query_type,
            execute=execute,
            language=language,
            cardinality_mode=cardinality_mode,
            is_withdrawn=is_withdrawn,
            is_approved=is_approved,
            has_entailment_regime=has_entailment_regime,
        )


class ManifestParser:
    """
    Parses a test manifest file by converting it to N-Triples and providing
    an API to access the test data.
    """

    def __init__(self, manifest_path: Path, to_ntriples_cmd: Optional[str] = None):
        self.manifest_path = manifest_path
        if to_ntriples_cmd is None:
            to_ntriples_cmd = find_tool("to-ntriples")
            if not to_ntriples_cmd:
                raise UtilityNotFoundError(
                    "Could not find 'to-ntriples' command. Please ensure it is built and available in PATH."
                )
        self.to_ntriples_cmd = to_ntriples_cmd
        self.triples_by_subject: Dict[str, List[Dict[str, Any]]] = {}
        self._parse()

    @classmethod
    def from_manifest_file(
        cls, manifest_file: Path, srcdir: Path, logger: logging.Logger
    ) -> "ManifestParser":
        """
        Create a ManifestParser for the given manifest file with proper setup.

        Args:
            manifest_file: Path to the manifest file
            srcdir: Source directory for resolving relative paths
            logger: Logger instance for debugging

        Returns:
            ManifestParser instance

        Raises:
            FileNotFoundError: If manifest file doesn't exist
            UtilityNotFoundError: If to-ntriples command is not found
            RuntimeError: If manifest parsing fails
        """
        if not manifest_file.exists():
            logger.error(f"Manifest file not found at {manifest_file}")
            raise FileNotFoundError(f"Manifest file not found at {manifest_file}")

        try:
            # Change to source directory before parsing manifest to ensure relative URIs are resolved correctly
            original_cwd = os.getcwd()
            try:
                os.chdir(srcdir)
                # Use just the filename for the manifest file when in source directory
                manifest_filename = Path(os.path.basename(manifest_file))
                manifest_parser = cls(manifest_filename)
            finally:
                os.chdir(original_cwd)

            return manifest_parser

        except RuntimeError as e:
            logger.error(f"Error parsing manifest: {e}")
            raise RuntimeError(f"Error parsing manifest: {e}")

    def _parse(self):
        """Parse the manifest file and populate triples_by_subject."""
        logger = logging.getLogger(__name__)
        cmd = [self.to_ntriples_cmd, str(self.manifest_path)]
        logger.debug(f"ManifestParser._parse: Running command: {cmd}")
        logger.debug(
            f"ManifestParser._parse: Working directory: {self.manifest_path.parent}"
        )

        # Try running from the current working directory instead of the manifest file's directory
        import os

        current_cwd = Path.cwd()
        logger.debug(f"ManifestParser._parse: Current working directory: {current_cwd}")

        process = run_command(
            cmd, current_cwd, f"Error running '{self.to_ntriples_cmd}'"
        )

        if process.returncode != 0:
            logger.debug(
                f"ManifestParser._parse: Command failed with return code {process.returncode}"
            )
            logger.debug(f"ManifestParser._parse: stderr: {process.stderr}")
            logger.error(
                f"'{self.to_ntriples_cmd}' failed for {self.manifest_path} with exit code {process.returncode}.\n{process.stderr}"
            )
            raise RuntimeError(
                f"'{self.to_ntriples_cmd}' failed for {self.manifest_path} with exit code {process.returncode}.\n{process.stderr}"
            )

        logger.debug(
            f"ManifestParser._parse: Command succeeded, stdout length: {len(process.stdout)}"
        )
        logger.debug(
            f"ManifestParser._parse: First 200 chars of stdout: {process.stdout[:200]}"
        )
        logger.debug(f"N-Triples output from {self.to_ntriples_cmd}:\n{process.stdout}")

        line_count = 0
        for line in process.stdout.splitlines():
            line_count += 1
            s, p, o_full = self._parse_nt_line(line)
            if s:
                self.triples_by_subject.setdefault(s, []).append(
                    {"p": p, "o_full": o_full}
                )
                logger.debug(f"Parsed and stored triple: (S: {s}, P: {p}, O: {o_full})")

        logger.debug(
            f"ManifestParser._parse: Processed {line_count} lines, found {len(self.triples_by_subject)} subjects"
        )
        logger.debug(f"Finished parsing. Triples by subject: {self.triples_by_subject}")

    def _parse_nt_line(
        self, line: str
    ) -> Tuple[Optional[str], Optional[str], Optional[str]]:
        line = line.strip()
        if not line.endswith(" ."):
            return None, None, None
        line = line[:-2].strip()

        # Regex to capture subject, predicate, and the entire object part
        match = re.match(r"^(\S+)\s+(\S+)\s+(.*)", line)
        if not match:
            return None, None, None

        s_raw, p_raw, o_raw = match.groups()

        # Keep URIs with angle brackets for now, normalize later if needed
        s = s_raw
        p = p_raw
        o = o_raw

        return s, p, o

    def get_tests(
        self, srcdir: Path, unique_test_filter: Optional[str] = None
    ) -> List[TestConfig]:
        """Parses the full manifest to extract a list of TestConfig objects."""
        logger = logging.getLogger(__name__)
        tests: List[TestConfig] = []

        # Create helper instances
        extractor = ManifestTripleExtractor(self.triples_by_subject)
        path_resolver = PathResolver(srcdir, extractor)
        config_builder = TestConfigBuilder(extractor, path_resolver)

        # Find the manifest node and iterate over entries
        manifest_node_uri = self.find_manifest_node()

        # Iterate over manifest entries
        for entry_uri_full in self.iter_manifest_entries(manifest_node_uri):
            # Extract basic entry information
            action_node = extractor.get_obj_val(
                entry_uri_full, f"<{Namespaces.MF}action>"
            )
            if not action_node:
                continue

            query_type = extractor.unquote(
                extractor.get_obj_val(entry_uri_full, f"<{Namespaces.RDF}type>")
            )

            # Skip unsupported test types
            if TestTypeResolver.should_skip_test_type(query_type):
                continue

            # Determine test behavior and query node
            execute, expect, language = TestTypeResolver.resolve_test_behavior(
                query_type
            )

            # For syntax tests, the action node is the query node
            if not execute:
                query_node = action_node
            else:
                query_node = extractor.get_obj_val(
                    action_node, f"<{Namespaces.QT}query>"
                )
                if not query_node:
                    continue

            # Resolve test file path
            test_file = path_resolver.resolve_path(query_node)
            if test_file is None:
                logger.warning(f"Could not resolve test file for {entry_uri_full}")
                continue

            # Build the test config
            config = config_builder.build_test_config(
                entry_uri_full,
                action_node,
                query_node,
                test_file,
                execute,
                expect,
                language,
                query_type,
            )

            logger.debug(
                f"Test config created: name={config.name}, expect={config.expect.value}, test_type={config.test_type}"
            )

            # Apply filtering
            if not unique_test_filter or (
                config.name == unique_test_filter
                or unique_test_filter in config.test_uri
            ):
                tests.append(config)
                if unique_test_filter and (
                    config.name == unique_test_filter
                    or unique_test_filter in config.test_uri
                ):
                    break

        return tests

    def get_test_ids_from_manifest(self, suite_name: str) -> List[str]:
        """Extracts test identifiers from the manifest for plan generation."""
        logger = logging.getLogger(__name__)
        test_ids = set()
        logger.debug(f"get_test_ids_from_manifest called for suite: {suite_name}")

        # Find the main manifest node
        manifest_node_uri = None
        for s, triples in self.triples_by_subject.items():
            for triple in triples:
                if (
                    triple["p"] == f"<{Namespaces.RDF}type>"
                    and triple["o_full"] == f"<{Namespaces.MF}Manifest>"
                ):
                    manifest_node_uri = s
                    break
            if manifest_node_uri:
                break

        if not manifest_node_uri:
            logger.warning("No main manifest node found.")
            return []

        # Get suite configuration
        suite_config = get_test_suite_config(suite_name)
        predicate = suite_config.get_test_id_predicate()
        logger.debug(f"Using predicate '{predicate}' for suite '{suite_name}'")

        # Iterate over manifest entries
        for entry_node_full in self.iter_manifest_entries(manifest_node_uri):
            entry_triples = self.triples_by_subject.get(entry_node_full, [])

            for triple in entry_triples:
                if triple["p"] == predicate:
                    if suite_config.execution_mode in [
                        TestExecutionMode.LEXER_ONLY,
                        TestExecutionMode.PARSER_ONLY,
                    ]:
                        # For lexer/parser tests, extract URI from <...>
                        test_id = triple["o_full"][1:-1]
                        logger.debug(
                            f"    Found {suite_config.execution_mode.value} test ID: {test_id}"
                        )
                    else:
                        # For execution tests, use decode_literal for literals
                        test_id = decode_literal(triple["o_full"])
                        logger.debug(
                            f"    Found {suite_config.execution_mode.value} test ID: {test_id}"
                        )
                    test_ids.add(test_id)

        logger.debug(f"Final extracted test_ids: {sorted(list(test_ids))}")
        return sorted(list(test_ids))

    def iter_manifest_entries(self, manifest_node_uri: str):
        """
        Iterate over manifest entries and yield each entry node URI.

        Args:
            manifest_node_uri: The URI of the manifest node

        Yields:
            str: Each entry node URI from the manifest
        """
        # Find entries list head
        entries_list_head = next(
            (
                t["o_full"]
                for t in self.triples_by_subject.get(manifest_node_uri, [])
                if t["p"] == f"<{Namespaces.MF}entries>"
            ),
            None,
        )

        if not entries_list_head:
            raise ManifestParsingError("Could not find mf:entries list.")

        # Traverse list and yield each entry
        current_list_item_node = entries_list_head
        while (
            current_list_item_node
            and current_list_item_node != f"<{Namespaces.RDF}nil>"
        ):
            list_node_triples = self.triples_by_subject.get(current_list_item_node, [])
            entry_node_full = next(
                (
                    t["o_full"]
                    for t in list_node_triples
                    if t["p"] == f"<{Namespaces.RDF}first>"
                ),
                None,
            )

            if entry_node_full:
                yield entry_node_full

            current_list_item_node = next(
                (
                    t["o_full"]
                    for t in list_node_triples
                    if t["p"] == f"<{Namespaces.RDF}rest>"
                ),
                None,
            )

    def find_manifest_node(self) -> str:
        """Find the manifest node URI from the parsed triples."""
        logger = logging.getLogger(__name__)
        # Look for a subject that has mf:entries property
        for subject, triples in self.triples_by_subject.items():
            if any(t["p"] == f"<{Namespaces.MF}entries>" for t in triples):
                logger.debug(f"Found manifest node: {subject}")
                return subject

        logger.error("Could not find manifest node with mf:entries property")
        raise ManifestParsingError(
            "Could not find manifest node with mf:entries property"
        )


def validate_required_paths(*paths: Tuple[Path, str]) -> None:
    """
    Validate that required paths exist.

    Args:
        paths: Tuples of (path, description) to validate

    Raises:
        FileNotFoundError: If any required path doesn't exist
    """
    for path, description in paths:
        if path and not path.exists():
            raise FileNotFoundError(f"{description} not found at {path}")

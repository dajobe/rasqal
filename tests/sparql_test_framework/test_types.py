"""
Type definitions and resolution for SPARQL Test Framework

This module contains all type-related classes including test types, results,
namespace constants, and type resolution logic.

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

from enum import Enum
from typing import Tuple


class Namespaces:
    """Convenience class for RDF/Manifest namespaces."""

    # Namespace URI constants
    MF = "http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#"
    RDF = "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
    RDFS = "http://www.w3.org/2000/01/rdf-schema#"
    T = "http://ns.librdf.org/2009/test-manifest#"
    QT = "http://www.w3.org/2001/sw/DataAccess/tests/test-query#"
    DAWGT = "http://www.w3.org/2001/sw/DataAccess/tests/test-dawg#"
    UT = "http://www.w3.org/2009/sparql/tests/test-update#"
    RS = "http://www.w3.org/2001/sw/DataAccess/tests/result-set#"
    MFX = "http://jena.hpl.hp.com/2005/05/test-manifest-extra#"
    SPARQL = "http://www.w3.org/ns/sparql#"

    # Configuration mapping namespace URIs to their short prefixes
    _NAMESPACE_PREFIXES = {
        T: "t",
        MF: "mf",
        UT: "ut",
        MFX: "mfx",
        RDF: "rdf",
        RDFS: "rdfs",
        QT: "qt",
        DAWGT: "dawgt",
        RS: "rs",
        SPARQL: "sparql",
    }

    # Reverse mapping for efficient prefix-to-namespace lookup
    _PREFIX_TO_NAMESPACE = {prefix: uri for uri, prefix in _NAMESPACE_PREFIXES.items()}

    # Configuration mapping simple test type names to prefixed forms
    _TEST_TYPE_MAPPING = {
        "PositiveTest": "t:PositiveTest",
        "NegativeTest": "t:NegativeTest",
        "XFailTest": "t:XFailTest",
        "WarningTest": "t:WarningTest",
        "PositiveSyntaxTest": "mf:PositiveSyntaxTest",
        "NegativeSyntaxTest": "mf:NegativeSyntaxTest",
        "QueryEvaluationTest": "mf:QueryEvaluationTest",
    }

    @staticmethod
    def uri_to_prefixed_name(uri: str) -> str:
        """
        Convert a full URI or simple test type name to a prefixed local name for Turtle output.

        Args:
            uri: The full URI or simple test type name to convert

        Returns:
            Prefixed local name (e.g., 't:PositiveTest') or the original URI if no mapping found
        """
        if not uri:
            return uri

        # Handle simple test type names (e.g., "PositiveTest", "NegativeTest", "XFailTest")
        if not uri.startswith("http") and not uri.startswith("<"):
            return Namespaces._TEST_TYPE_MAPPING.get(uri, uri)

        # Handle full URIs
        if uri.startswith("http"):
            # Find matching namespace and extract local name
            for namespace_uri, prefix in Namespaces._NAMESPACE_PREFIXES.items():
                if uri.startswith(namespace_uri):
                    local_name = uri[len(namespace_uri) :]
                    return f"{prefix}:{local_name}"

            # Fallback: extract the local name after the last '#' or '/'
            if "#" in uri:
                return uri.split("#")[-1]
            else:
                return uri.split("/")[-1]

        return uri

    @staticmethod
    def prefixed_name_to_uri(prefixed_name: str) -> str:
        """
        Convert a prefixed local name back to a full URI.

        Args:
            prefixed_name: The prefixed local name (e.g., 't:PositiveTest', 'mfx:TestBadSyntax')

        Returns:
            Full URI or the original prefixed name if no mapping found
        """
        if not prefixed_name or ":" not in prefixed_name:
            return prefixed_name

        prefix, local_name = prefixed_name.split(":", 1)

        # Use reverse mapping for efficient lookup
        namespace_uri = Namespaces._PREFIX_TO_NAMESPACE.get(prefix)
        if namespace_uri:
            return f"{namespace_uri}{local_name}"

        return prefixed_name


class TestResult(Enum):
    """Enumeration of possible test execution results."""

    PASSED = "passed"
    FAILED = "failed"
    SKIPPED = "skipped"
    XFAILED = "xfailed"  # Expected to fail and it did
    UXPASSED = "uxpassed"  # Expected to fail but unexpectedly passed

    def __str__(self):
        return self.value

    def display_char(self) -> str:
        """Returns a single character for compact progress display."""
        # Configuration mapping results to display characters
        display_chars = {
            "passed": ".",
            "failed": "F",
            "xfailed": "*",
            "uxpassed": "!",
            "skipped": "-",
        }
        return display_chars.get(self.value, "?")

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
    # SPARQL 1.1 specific test types
    QUERY_EVALUATION_TEST = f"{Namespaces.MF}QueryEvaluationTest"
    SERVICE_TEST = f"{Namespaces.MF}QueryEvaluationTest"  # SERVICE tests are QueryEvaluationTest with SERVICE
    # Custom test types
    XFAIL_TEST = f"{Namespaces.T}XFailTest"
    WARNING_TEST = f"{Namespaces.T}WarningTest"


class TestTypeResolver:
    """Helper class for determining test types and behavior."""

    # Default behavior for unknown test types
    _DEFAULT_BEHAVIOR = (True, TestResult.PASSED, "sparql")

    # Common behavior patterns for different test categories
    _SYNTAX_ONLY_PASS_SPARQL = (False, TestResult.PASSED, "sparql")
    _SYNTAX_ONLY_PASS_SPARQL11 = (False, TestResult.PASSED, "sparql11")
    _SYNTAX_ONLY_FAIL_SPARQL = (False, TestResult.FAILED, "sparql")
    _SYNTAX_ONLY_FAIL_SPARQL11 = (False, TestResult.FAILED, "sparql11")
    _EXECUTE_PASS_SPARQL = (True, TestResult.PASSED, "sparql")
    _EXECUTE_PASS_SPARQL11 = (True, TestResult.PASSED, "sparql11")
    _SKIP_TEST = (False, TestResult.SKIPPED, "sparql")
    _EXECUTE_XFAIL = (True, TestResult.XFAILED, "sparql")

    # Test types that should be entirely skipped
    _SKIPPED_TEST_TYPES = {
        TestType.UPDATE_EVALUATION_TEST.value,
        TestType.PROTOCOL_TEST.value,
    }

    # Test type behavior mapping: test_type -> (execute, expect, language)
    TEST_BEHAVIOR_MAP = {
        TestType.POSITIVE_SYNTAX_TEST.value: _SYNTAX_ONLY_PASS_SPARQL,
        TestType.POSITIVE_SYNTAX_TEST_11.value: _SYNTAX_ONLY_PASS_SPARQL11,
        TestType.POSITIVE_UPDATE_SYNTAX_TEST_11.value: _SYNTAX_ONLY_PASS_SPARQL11,
        TestType.NEGATIVE_SYNTAX_TEST.value: _SYNTAX_ONLY_FAIL_SPARQL,
        TestType.NEGATIVE_SYNTAX_TEST_11.value: _SYNTAX_ONLY_FAIL_SPARQL11,
        TestType.NEGATIVE_UPDATE_SYNTAX_TEST_11.value: _SYNTAX_ONLY_FAIL_SPARQL11,
        TestType.BAD_SYNTAX_TEST.value: _SYNTAX_ONLY_FAIL_SPARQL,
        TestType.TEST_SYNTAX.value: _SYNTAX_ONLY_PASS_SPARQL,
        TestType.UPDATE_EVALUATION_TEST.value: _SKIP_TEST,
        TestType.PROTOCOL_TEST.value: _SKIP_TEST,
        TestType.QUERY_EVALUATION_TEST.value: _EXECUTE_PASS_SPARQL11,
        TestType.CSV_RESULT_FORMAT_TEST.value: _EXECUTE_PASS_SPARQL,
        TestType.SERVICE_TEST.value: _EXECUTE_PASS_SPARQL11,
        TestType.XFAIL_TEST.value: _EXECUTE_XFAIL,
        TestType.WARNING_TEST.value: _EXECUTE_PASS_SPARQL11,
    }

    @staticmethod
    def resolve_test_behavior(query_type: str) -> Tuple[bool, TestResult, str]:
        """
        Determine test execution behavior based on test type.

        Args:
            query_type: The test type URI string

        Returns:
            Tuple of (execute, expect, language)
            - execute: Whether to run the query (False for syntax-only tests)
            - expect: Expected test result
            - language: SPARQL language version ("sparql" or "sparql11")
        """
        if not query_type:
            return TestTypeResolver._DEFAULT_BEHAVIOR

        # Use mapping table for known test types
        return TestTypeResolver.TEST_BEHAVIOR_MAP.get(
            query_type, TestTypeResolver._DEFAULT_BEHAVIOR
        )

    @staticmethod
    def should_skip_test_type(query_type: str) -> bool:
        """
        Check if a test type should be skipped entirely.

        Args:
            query_type: The test type URI string

        Returns:
            True if the test type is not supported and should be skipped
        """
        return query_type in TestTypeResolver._SKIPPED_TEST_TYPES

    @staticmethod
    def classify_test_type(type_uri: str, suite_name: str, default_type: str) -> str:
        """
        Determine the test type based on the type URI and suite name.

        Args:
            type_uri: The RDF type URI from the manifest
            suite_name: Name of the test suite
            default_type: Default type to use if no specific mapping found

        Returns:
            Resolved test type string
        """
        # If we have a direct URI, use it
        if type_uri and type_uri.startswith("http"):
            return type_uri

        # Apply suite-specific defaults for common patterns
        if suite_name and "syntax" in suite_name.lower():
            if "negative" in suite_name.lower() or "bad" in suite_name.lower():
                return TestType.NEGATIVE_SYNTAX_TEST.value
            else:
                return TestType.POSITIVE_SYNTAX_TEST.value

        return default_type or TestType.QUERY_EVALUATION_TEST.value

    @staticmethod
    def determine_test_result(expected_status: TestResult, actual_status: TestResult) -> Tuple[TestResult, str]:
        """
        Centralized logic for determining final test result given expected and actual status.
        
        This function handles all test result scenarios including:
        - Tests that pass when expected to pass
        - Tests that fail when expected to fail
        - Tests that fail when expected to pass (unexpected failures)
        - Tests that pass when expected to fail (unexpected passes)
        - XFailTests that fail as expected
        - XFailTests that unexpectedly pass
        
        Args:
            expected_status: The expected test status
            actual_status: The actual test execution status
            
        Returns:
            Tuple of (final_result, detail_message)
        """
        # Handle XFailTests (expected to fail)
        if expected_status == TestResult.XFAILED:
            if actual_status == TestResult.FAILED:
                return TestResult.XFAILED, "Test failed as expected"
            else:
                return TestResult.UXPASSED, "Test passed (XFailTest - expected to fail but passed)"
        
        # Handle regular tests expected to fail
        elif expected_status == TestResult.FAILED:
            if actual_status == TestResult.FAILED:
                return TestResult.XFAILED, "Test failed as expected"
            else:
                return TestResult.UXPASSED, "Test passed but was expected to fail"
        
        # Handle tests expected to pass
        elif expected_status == TestResult.PASSED:
            if actual_status == TestResult.PASSED:
                return TestResult.PASSED, "Test passed as expected"
            else:
                return TestResult.FAILED, "Test failed unexpectedly"
        
        # Handle skipped tests
        elif expected_status == TestResult.SKIPPED:
            if actual_status == TestResult.SKIPPED:
                return TestResult.SKIPPED, "Test skipped as expected"
            else:
                return actual_status, "Test was expected to be skipped"
        
        # Default case: return actual status
        else:
            return actual_status, ""



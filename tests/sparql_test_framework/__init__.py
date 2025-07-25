"""
SPARQL Test Framework for Rasqal

A comprehensive framework for running W3C SPARQL test suites against Rasqal.
Supports SPARQL 1.0, SPARQL 1.1, and custom test formats with multiple runners.

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

# Framework metadata
__version__ = "2.0.0"
__author__ = "David Beckett"
__email__ = "dave@dajobe.org"
__license__ = "LGPL/GPL/Apache"

# Re-export main classes for backward compatibility and convenience
from .config import (
    TestConfig,
    TestConfigBuilder,
    TestSuiteConfig,
    TestExecutionMode,
    KNOWN_TEST_SUITES,
    get_test_suite_config,
    get_available_test_suites,
    validate_test_suite_name,
    get_default_comment_template,
    get_comment_template,
)
from .manifest import ManifestParser, ManifestParsingError, UtilityNotFoundError
from .test_types import TestType, TestResult, TestTypeResolver, Namespaces
from .execution import (
    run_roqet_with_format,
    filter_format_output,
    run_manifest_test,
    validate_required_paths,
)
from .utils import (
    PathResolver,
    ManifestTripleExtractor,
    setup_logging,
    find_tool,
    add_common_arguments,
    run_command,
    decode_literal,
    escape_turtle_literal,
    SparqlTestError,
)

# Re-export runners for direct access
from .runners import (
    TestOrchestrator,
    SparqlTestRunner,
    FormatTestRunner,
    CsvTsvTestRunner,
    SrjTestRunner,
    AlgebraTestRunner,
)

# Re-export tools
from .tools import PlanGenerator, TestPackager

__all__ = [
    # Core framework
    "TestConfig",
    "TestConfigBuilder",
    "TestSuiteConfig",
    "TestExecutionMode",
    "ManifestParser",
    "ManifestTripleExtractor",
    "ManifestParsingError",
    "UtilityNotFoundError",
    "TestType",
    "TestResult",
    "TestTypeResolver",
    "Namespaces",
    "run_roqet_with_format",
    "filter_format_output",
    "run_manifest_test",
    "validate_required_paths",
    "PathResolver",
    "setup_logging",
    "find_tool",
    "add_common_arguments",
    "run_command",
    "decode_literal",
    "escape_turtle_literal",
    "SparqlTestError",
    # Configuration functions
    "KNOWN_TEST_SUITES",
    "get_test_suite_config",
    "get_available_test_suites",
    "validate_test_suite_name",
    "get_default_comment_template",
    "get_comment_template",
    # Runners
    "TestOrchestrator",
    "SparqlTestRunner",
    "FormatTestRunner",
    "CsvTsvTestRunner",
    "SrjTestRunner",
    "AlgebraTestRunner",
    # Tools
    "PlanGenerator",
    "TestPackager",
]

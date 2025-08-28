"""
Test Execution Handlers Module for SPARQL Test Results

This module handles different test execution outcomes (warning test failures,
roqet failures, unexpected successes, successful executions), extracting complex
test execution handling logic from the main sparql.py file.

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
from typing import Dict, Any

logger = logging.getLogger(__name__)


class TestExecutionHandlers:
    """Handles different test execution outcomes and their processing."""

    def __init__(self):
        self.logger = logging.getLogger(__name__)

    def handle_warning_test_failure(
        self,
        test_result_summary: Dict[str, Any],
        config: "TestConfig",
        roqet_execution_data: Dict[str, Any],
        global_debug_level: int,
    ) -> Dict[str, Any]:
        """Handle warning tests that got exit code 0 instead of 2."""
        self.logger.warning(
            f"Test '{config.name}': FAILED (warning test got exit code 0, expected 2)"
        )
        # Import here to avoid circular imports
        from .sparql import finalize_test_result

        finalize_test_result(test_result_summary, config.expect)
        return test_result_summary

    def handle_roqet_failure(
        self,
        test_result_summary: Dict[str, Any],
        config: "TestConfig",
        roqet_execution_data: Dict[str, Any],
        global_debug_level: int,
    ) -> Dict[str, Any]:
        """Handle roqet execution failures."""
        # Import here to avoid circular imports
        from .sparql import (
            TestType,
            TestResult,
            _show_debug_output_for_expected_failure,
            finalize_test_result,
        )

        # For warning tests, exit code 2 (warning) is considered success
        # For other tests, exit code 0 (success) is expected
        if config.test_type == TestType.WARNING_TEST.value:
            test_result_summary["result"] = (
                "success" if roqet_execution_data["returncode"] in [0, 2] else "failure"
            )
        else:
            test_result_summary["result"] = (
                "success" if roqet_execution_data["returncode"] == 0 else "failure"
            )

        # Handle non-zero exit codes (but not for warning tests with exit code 2)
        if roqet_execution_data["returncode"] != 0 and not (
            config.test_type == TestType.WARNING_TEST.value
            and roqet_execution_data["returncode"] == 2
        ):
            outcome_msg = f"exited with status {roqet_execution_data['returncode']}"
            if global_debug_level > 0:
                self.logger.debug(f"roqet for '{config.name}' {outcome_msg}")

            if config.expect == TestResult.FAILED:
                _show_debug_output_for_expected_failure(config, global_debug_level)
                self.logger.info(
                    f"Test '{config.name}': OK (roqet failed as expected: {outcome_msg})"
                )
            else:
                self.logger.warning(
                    f"Test '{config.name}': FAILED (roqet command failed: {outcome_msg})"
                )
                if roqet_execution_data["stderr"]:
                    self.logger.warning(
                        f"  Stderr:\n{roqet_execution_data['stderr'].strip()}"
                    )

            finalize_test_result(test_result_summary, config.expect)
            return test_result_summary

        return test_result_summary

    def handle_unexpected_success(
        self,
        test_result_summary: Dict[str, Any],
        config: "TestConfig",
        global_debug_level: int,
    ) -> Dict[str, Any]:
        """Handle tests that were expected to fail but succeeded."""
        # Import here to avoid circular imports
        from .sparql import (
            TestResult,
            TestTypeResolver,
            _show_debug_output_for_unexpected_success,
            finalize_test_result,
        )

        # Test was expected to fail but succeeded - use centralized logic
        final_result, detail = TestTypeResolver.determine_test_result(
            config.expect, TestResult.PASSED
        )

        # For expected failure tests that succeeded, show debug output to understand why
        if global_debug_level > 0:
            _show_debug_output_for_unexpected_success(config, global_debug_level)

        finalize_test_result(test_result_summary, config.expect)
        return test_result_summary

    def handle_successful_execution(
        self,
        test_result_summary: Dict[str, Any],
        config: "TestConfig",
        roqet_execution_data: Dict[str, Any],
        global_debug_level: int,
        use_rasqal_compare: bool,
    ) -> Dict[str, Any]:
        """Handle successful roqet execution with result comparison."""
        # Import here to avoid circular imports
        from .sparql import (
            TestResult,
            TestTypeResolver,
            _show_debug_output_for_unexpected_success,
            finalize_test_result,
        )

        # roqet command succeeded (exit code 0) - for non-warning tests
        if config.expect == TestResult.FAILED or config.expect == TestResult.XFAILED:
            # Test was expected to fail but succeeded - use centralized logic
            final_result, detail = TestTypeResolver.determine_test_result(
                config.expect, TestResult.PASSED
            )

            # For expected failure tests that succeeded, show debug output to understand why
            if global_debug_level > 0:
                _show_debug_output_for_unexpected_success(config, global_debug_level)

            finalize_test_result(test_result_summary, config.expect)
            return test_result_summary

        # Test was expected to pass and roqet succeeded
        # This is the normal case - proceed with result comparison
        test_result_summary["result"] = "success"
        return test_result_summary


# Convenience functions for backward compatibility
def handle_warning_test_failure(
    test_result_summary: Dict[str, Any],
    config: "TestConfig",
    roqet_execution_data: Dict[str, Any],
    global_debug_level: int,
) -> Dict[str, Any]:
    """Backward compatibility wrapper for TestExecutionHandlers.handle_warning_test_failure."""
    handler = TestExecutionHandlers()
    return handler.handle_warning_test_failure(
        test_result_summary, config, roqet_execution_data, global_debug_level
    )


def handle_roqet_failure(
    test_result_summary: Dict[str, Any],
    config: "TestConfig",
    roqet_execution_data: Dict[str, Any],
    global_debug_level: int,
) -> Dict[str, Any]:
    """Backward compatibility wrapper for TestExecutionHandlers.handle_roqet_failure."""
    handler = TestExecutionHandlers()
    return handler.handle_roqet_failure(
        test_result_summary, config, roqet_execution_data, global_debug_level
    )


def handle_unexpected_success(
    test_result_summary: Dict[str, Any], config: "TestConfig", global_debug_level: int
) -> Dict[str, Any]:
    """Backward compatibility wrapper for TestExecutionHandlers.handle_unexpected_success."""
    handler = TestExecutionHandlers()
    return handler.handle_unexpected_success(
        test_result_summary, config, global_debug_level
    )


def handle_successful_execution(
    test_result_summary: Dict[str, Any],
    config: "TestConfig",
    roqet_execution_data: Dict[str, Any],
    global_debug_level: int,
    use_rasqal_compare: bool,
) -> Dict[str, Any]:
    """Backward compatibility wrapper for TestExecutionHandlers.handle_successful_execution."""
    handler = TestExecutionHandlers()
    return handler.handle_successful_execution(
        test_result_summary,
        config,
        roqet_execution_data,
        global_debug_level,
        use_rasqal_compare,
    )

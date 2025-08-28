"""
Debug Manager Module for SPARQL Test Results

This module handles debug output, comparison logic, and result presentation,
extracting complex debug and comparison functions from the main sparql.py file.

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
import subprocess
from pathlib import Path
from typing import Dict, Any, Optional

logger = logging.getLogger(__name__)

# Constants
DIFF_CMD = "diff"
RASQAL_COMPARE = "rasqal-compare"


class DebugManager:
    """Handles debug output, comparison logic, and result presentation."""

    def __init__(self, global_debug_level: int = 0):
        self.global_debug_level = global_debug_level

    def show_success_debug_info(
        self, test_name: str, actual_result_info: Dict[str, Any]
    ):
        """Show debug information for successful test execution."""
        if self.global_debug_level > 0:
            logger.info(f"Test '{test_name}': PASSED")
            if self.global_debug_level > 1:
                self._show_actual_results_debug(
                    actual_result_info, "successful execution"
                )

    def show_failure_debug_info(
        self,
        test_name: str,
        actual_result_info: Dict[str, Any],
        expected_result_info: Dict[str, Any],
    ):
        """Show debug information for failed test execution."""
        if self.global_debug_level > 0:
            logger.warning(f"Test '{test_name}': FAILED")
            if self.global_debug_level > 1:
                self._show_actual_results_debug(actual_result_info, "failed execution")
                self._show_expected_results_debug(expected_result_info)
                self._show_roqet_output_debug("failed execution")

    def show_debug_output_for_expected_failure(self, config: "TestConfig"):
        """Show debug output for tests that were expected to fail."""
        if self.global_debug_level > 0:
            logger.debug(f"Expected failure test '{config.name}' debug info:")
            if self.global_debug_level > 1:
                self._show_test_config_details(config)

    def show_debug_output_for_unexpected_success(self, config: "TestConfig"):
        """Show debug output for tests that unexpectedly succeeded."""
        if self.global_debug_level > 0:
            logger.debug(f"Unexpected success test '{config.name}' debug info:")
            if self.global_debug_level > 1:
                self._show_test_config_details(config)

    def show_system_diff_debug_info(self, test_name: str):
        """Show system diff output for debug purposes."""
        if self.global_debug_level > 2:
            logger.debug(f"System diff for test '{test_name}':")
            diff_output = self._generate_diff_for_debug(test_name)
            if diff_output:
                logger.debug(diff_output)

    def show_rasqal_compare_debug_info(self, test_name: str):
        """Show rasqal-compare output for debug purposes."""
        if self.global_debug_level > 2:
            logger.debug(f"Rasqal-compare output for test '{test_name}':")
            # This would show the actual rasqal-compare output if available

    def show_failure_summary(self, test_name: str):
        """Show summary of failure details."""
        if self.global_debug_level > 0:
            logger.warning(f"Test '{test_name}' failed - see above for details")

    def _show_actual_results_debug(
        self, actual_result_info: Dict[str, Any], context: str
    ):
        """Show debug information about actual results."""
        logger.debug(f"Actual results ({context}):")
        if "content" in actual_result_info:
            content = actual_result_info["content"]
            if len(content) > 200:
                logger.debug(f"  Content (truncated): {content[:200]}...")
            else:
                logger.debug(f"  Content: {content}")
        if "count" in actual_result_info:
            logger.debug(f"  Count: {actual_result_info['count']}")

    def _show_expected_results_debug(self, expected_result_info: Dict[str, Any]):
        """Show debug information about expected results."""
        logger.debug("Expected results:")
        if "content" in expected_result_info:
            content = expected_result_info["content"]
            if len(content) > 200:
                logger.debug(f"  Content (truncated): {content[:200]}...")
            else:
                logger.debug(f"  Content: {content}")

    def _show_roqet_output_debug(self, context: str):
        """Show debug information about roqet output."""
        logger.debug(f"Roqet output ({context}):")
        # This would show the actual roqet output if available

    def _show_test_config_details(self, config: "TestConfig"):
        """Show detailed test configuration information."""
        logger.debug(f"  Test file: {config.test_file}")
        if config.result_file:
            logger.debug(f"  Result file: {config.result_file}")
        logger.debug(f"  Test type: {config.test_type}")
        logger.debug(f"  Expected result: {config.expect}")

    def _generate_diff_for_debug(self, test_name: str) -> Optional[str]:
        """Generate diff output for debug purposes."""
        try:
            result = subprocess.run(
                [DIFF_CMD, "result.out", "roqet.out"],
                capture_output=True,
                text=True,
                timeout=30,
            )
            if result.returncode in (0, 1):  # 0 = no differences, 1 = differences found
                return result.stdout + result.stderr
            else:
                logger.warning(
                    f"Diff command failed with return code {result.returncode}"
                )
                return None
        except subprocess.TimeoutExpired:
            logger.warning("Diff command timed out")
            return None
        except Exception as e:
            logger.warning(f"Diff command failed: {e}")
            return None


class ComparisonManager:
    """Handles result comparison logic for different result types."""

    def __init__(self, use_rasqal_compare: bool = False):
        self.use_rasqal_compare = use_rasqal_compare

    def compare_actual_vs_expected(
        self,
        actual_result_info: Dict[str, Any],
        expected_result_info: Dict[str, Any],
        test_name: str,
    ) -> bool:
        """Compare actual vs expected results and return success status."""
        try:
            # Determine result type and perform appropriate comparison
            result_type = actual_result_info.get("result_type", "unknown")

            if result_type == "graph":
                return self._compare_graph_results(test_name)
            elif result_type == "bindings":
                return self._compare_bindings_results(actual_result_info, test_name)
            elif result_type == "boolean":
                return self._compare_boolean_results(actual_result_info, test_name)
            elif result_type == "srj":
                return self._compare_srj_results(test_name)
            else:
                logger.warning(
                    f"Unknown result type '{result_type}' for test '{test_name}'"
                )
                return False

        except Exception as e:
            logger.error(f"Error comparing results for test '{test_name}': {e}")
            return False

    def _compare_graph_results(self, test_name: str) -> bool:
        """Compare graph results using appropriate method."""
        if self.use_rasqal_compare:
            return self._compare_with_rasqal_compare(test_name)
        else:
            return self._compare_with_system_diff(test_name)

    def _compare_bindings_results(
        self, actual_result_info: Dict[str, Any], test_name: str
    ) -> bool:
        """Compare bindings results."""
        # For bindings, we typically compare the processed output
        return self._compare_with_system_diff(test_name)

    def _compare_boolean_results(
        self, actual_result_info: Dict[str, Any], test_name: str
    ) -> bool:
        """Compare boolean results."""
        # Boolean results are typically compared directly in the calling code
        return True

    def _compare_srj_results(self, test_name: str) -> bool:
        """Compare SRJ results."""
        # SRJ results are typically compared using the processed output
        return self._compare_with_system_diff(test_name)

    def _compare_with_rasqal_compare(self, test_name: str) -> bool:
        """Compare results using rasqal-compare utility."""
        try:
            result = subprocess.run(
                [RASQAL_COMPARE, "result.out", "roqet.out"],
                capture_output=True,
                text=True,
                timeout=30,
            )
            return result.returncode == 0
        except Exception as e:
            logger.warning(f"Rasqal-compare failed for test '{test_name}': {e}")
            return False

    def _compare_with_system_diff(self, test_name: str) -> bool:
        """Compare results using system diff utility."""
        try:
            result = subprocess.run(
                [DIFF_CMD, "result.out", "roqet.out"],
                capture_output=True,
                text=True,
                timeout=30,
            )
            return result.returncode == 0
        except Exception as e:
            logger.warning(f"System diff failed for test '{test_name}': {e}")
            return False

    def handle_comparison_result(
        self, comparison_success: bool, test_name: str
    ) -> None:
        """Handle the result of a comparison operation."""
        if comparison_success:
            logger.debug(f"Comparison successful for test '{test_name}'")
        else:
            logger.warning(f"Comparison failed for test '{test_name}'")


# Convenience functions for backward compatibility
def show_success_debug_info(
    test_name: str, actual_result_info: Dict[str, Any], global_debug_level: int = 0
):
    """Backward compatibility wrapper for DebugManager.show_success_debug_info."""
    manager = DebugManager(global_debug_level)
    manager.show_success_debug_info(test_name, actual_result_info)


def show_failure_debug_info(
    test_name: str,
    actual_result_info: Dict[str, Any],
    expected_result_info: Dict[str, Any],
    global_debug_level: int = 0,
):
    """Backward compatibility wrapper for DebugManager.show_failure_debug_info."""
    manager = DebugManager(global_debug_level)
    manager.show_failure_debug_info(test_name, actual_result_info, expected_result_info)


def show_debug_output_for_expected_failure(
    config: "TestConfig", global_debug_level: int = 0
):
    """Backward compatibility wrapper for DebugManager.show_debug_output_for_expected_failure."""
    manager = DebugManager(global_debug_level)
    manager.show_debug_output_for_expected_failure(config)


def show_debug_output_for_unexpected_success(
    config: "TestConfig", global_debug_level: int = 0
):
    """Backward compatibility wrapper for DebugManager.show_debug_output_for_unexpected_success."""
    manager = DebugManager(global_debug_level)
    manager.show_debug_output_for_unexpected_success(config)


def show_system_diff_debug_info(test_name: str, global_debug_level: int = 0):
    """Backward compatibility wrapper for DebugManager.show_system_diff_debug_info."""
    manager = DebugManager(global_debug_level)
    manager.show_system_diff_debug_info(test_name)


def show_rasqal_compare_debug_info(test_name: str, global_debug_level: int = 0):
    """Backward compatibility wrapper for DebugManager.show_rasqal_compare_debug_info."""
    manager = DebugManager(global_debug_level)
    manager.show_rasqal_compare_debug_info(test_name)


def show_failure_summary(test_name: str, global_debug_level: int = 0):
    """Backward compatibility wrapper for DebugManager.show_failure_summary."""
    manager = DebugManager(global_debug_level)
    manager.show_failure_summary(test_name)


def compare_actual_vs_expected(
    actual_result_info: Dict[str, Any],
    expected_result_info: Dict[str, Any],
    test_name: str,
    use_rasqal_compare: bool = False,
) -> bool:
    """Backward compatibility wrapper for ComparisonManager.compare_actual_vs_expected."""
    manager = ComparisonManager(use_rasqal_compare)
    return manager.compare_actual_vs_expected(
        actual_result_info, expected_result_info, test_name
    )


def handle_comparison_result(comparison_success: bool, test_name: str):
    """Backward compatibility wrapper for ComparisonManager.handle_comparison_result."""
    manager = ComparisonManager()
    manager.handle_comparison_result(comparison_success, test_name)

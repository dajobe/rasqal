"""
DebugOutputManager class for handling debug output and result presentation.

This module provides the DebugOutputManager class that handles debug output,
logging, and result presentation for test execution.

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
import sys
from pathlib import Path
from typing import Optional, Dict, Any

from ..config import TestConfig


class DebugOutputManager:
    """Handles debug output, logging, and result presentation."""

    def __init__(self, debug_level: int = 0):
        self.debug_level = debug_level
        self.logger = logging.getLogger(__name__)

        # Configure logging based on debug level
        self._configure_logging()

    def _configure_logging(self):
        """Configure logging based on debug level."""
        if self.debug_level >= 3:
            level = logging.DEBUG
        elif self.debug_level >= 2:
            level = logging.INFO
        elif self.debug_level >= 1:
            level = logging.WARNING
        else:
            level = logging.ERROR

        # Configure handler if not already configured
        if not self.logger.handlers:
            handler = logging.StreamHandler(sys.stderr)
            formatter = logging.Formatter(
                "%(asctime)s - %(name)s - %(levelname)s - %(message)s"
            )
            handler.setFormatter(formatter)
            self.logger.addHandler(handler)

        self.logger.setLevel(level)

    def set_debug_level(self, level: int):
        """Set the debug level and reconfigure logging."""
        self.debug_level = level
        self._configure_logging()

    def show_success_debug_info(
        self, test_name: str, result: Any, result_type: str = "unknown"
    ):
        """Show debug information for successful test execution."""
        if self.debug_level < 1:
            return

        self.logger.info(f"Test '{test_name}' completed successfully")

        if self.debug_level >= 2:
            self._log_result_details(result, result_type)

    def show_failure_debug_info(
        self,
        test_name: str,
        actual: Any,
        expected: Any,
        result_type: str = "unknown",
        diff_output: Optional[str] = None,
    ):
        """Show debug information for failed test execution."""
        if self.debug_level < 1:
            return

        self.logger.error(f"Test '{test_name}' failed")

        if self.debug_level >= 2:
            self._log_failure_details(actual, expected, result_type, diff_output)

    def _log_result_details(self, result: Any, result_type: str):
        """Log detailed information about the result."""
        if result_type == "graph":
            self._log_graph_result(result)
        elif result_type == "bindings":
            self._log_bindings_result(result)
        elif result_type == "boolean":
            self._log_boolean_result(result)
        elif result_type == "srj":
            self._log_srj_result(result)
        else:
            self.logger.debug(f"Result type '{result_type}' details: {result}")

    def _log_failure_details(
        self,
        actual: Any,
        expected: Any,
        result_type: str,
        diff_output: Optional[str] = None,
    ):
        """Log detailed information about the failure."""
        self.logger.debug(f"Expected {result_type} result: {expected}")
        self.logger.debug(f"Actual {result_type} result: {actual}")

        if diff_output:
            self.logger.debug(f"Diff output:\n{diff_output}")

    def _log_graph_result(self, result: Any):
        """Log graph result details."""
        if hasattr(result, "content"):
            content = result.content
            if hasattr(result, "count"):
                self.logger.debug(f"Graph result: {result.count} triples")
            else:
                self.logger.debug(f"Graph result: {len(content.splitlines())} lines")

            if self.debug_level >= 3:
                self.logger.debug(f"Graph content preview:\n{content[:500]}...")
        else:
            self.logger.debug(f"Graph result: {result}")

    def _log_bindings_result(self, result: Any):
        """Log bindings result details."""
        if hasattr(result, "content"):
            content = result.content
            if hasattr(result, "count"):
                self.logger.debug(f"Bindings result: {result.count} bindings")
            else:
                lines = content.splitlines()
                count = max(0, len(lines) - 1)  # Subtract header
                self.logger.debug(f"Bindings result: {count} bindings")

            if self.debug_level >= 3:
                self.logger.debug(f"Bindings content preview:\n{content[:500]}...")
        else:
            self.logger.debug(f"Bindings result: {result}")

    def _log_boolean_result(self, result: Any):
        """Log boolean result details."""
        if hasattr(result, "boolean_value"):
            self.logger.debug(f"Boolean result: {result.boolean_value}")
        else:
            self.logger.debug(f"Boolean result: {result}")

    def _log_srj_result(self, result: Any):
        """Log SRJ result details."""
        if hasattr(result, "json_content"):
            if hasattr(result, "bindings_count"):
                self.logger.debug(f"SRJ result: {result.bindings_count} bindings")
            elif hasattr(result, "boolean_value"):
                self.logger.debug(f"SRJ result: {result.boolean_value}")
            else:
                self.logger.debug("SRJ result: unknown type")

            if self.debug_level >= 3:
                content = result.json_content
                self.logger.debug(f"SRJ content preview:\n{content[:500]}...")
        else:
            self.logger.debug(f"SRJ result: {result}")

    def generate_diff_for_debug(self, actual_file: Path, expected_file: Path) -> str:
        """Generate diff output for debugging purposes."""
        try:
            if not actual_file.exists():
                return f"Actual file does not exist: {actual_file}"

            if not expected_file.exists():
                return f"Expected file does not exist: {expected_file}"

            # Read file contents
            actual_content = actual_file.read_text()
            expected_content = expected_file.read_text()

            # Generate simple diff
            return self._generate_simple_file_diff(actual_content, expected_content)

        except Exception as e:
            return f"Error generating diff: {e}"

    def _generate_simple_file_diff(self, actual: str, expected: str) -> str:
        """Generate simple diff output between two file contents."""
        actual_lines = actual.splitlines()
        expected_lines = expected.splitlines()

        diff_output = []
        diff_output.append("=== FILE DIFF ===")
        diff_output.append(f"Actual file: {len(actual_lines)} lines")
        diff_output.append(f"Expected file: {len(expected_lines)} lines")
        diff_output.append("")

        # Show first few lines of each file
        max_preview = 10

        if actual_lines:
            diff_output.append("--- ACTUAL (first few lines) ---")
            for i, line in enumerate(actual_lines[:max_preview]):
                diff_output.append(f"{i+1:3d}: {line}")
            if len(actual_lines) > max_preview:
                diff_output.append(
                    f"... and {len(actual_lines) - max_preview} more lines"
                )
            diff_output.append("")

        if expected_lines:
            diff_output.append("--- EXPECTED (first few lines) ---")
            for i, line in enumerate(expected_lines[:max_preview]):
                diff_output.append(f"{i+1:3d}: {line}")
            if len(expected_lines) > max_preview:
                diff_output.append(
                    f"... and {len(expected_lines) - max_preview} more lines"
                )
            diff_output.append("")

        return "\n".join(diff_output)

    def log_test_start(self, test_name: str, test_type: str = "unknown"):
        """Log the start of a test execution."""
        if self.debug_level >= 1:
            self.logger.info(f"Starting test '{test_name}' (type: {test_type})")

    def log_test_end(
        self, test_name: str, success: bool, duration: Optional[float] = None
    ):
        """Log the end of a test execution."""
        if self.debug_level >= 1:
            status = "PASSED" if success else "FAILED"
            duration_str = f" in {duration:.3f}s" if duration else ""
            self.logger.info(f"Test '{test_name}' {status}{duration_str}")

    def log_configuration_info(self, config: TestConfig):
        """Log configuration information for debugging."""
        if self.debug_level >= 2:
            self.logger.debug("Test configuration:")
            self.logger.debug(f"  Query file: {config.query_file}")
            self.logger.debug(f"  Data file: {config.data_file}")
            self.logger.debug(f"  Expected result file: {config.expected_result_file}")
            self.logger.debug(f"  Result format: {config.result_format}")
            self.logger.debug(f"  Debug level: {config.debug_level}")

    def log_execution_details(
        self, command: list, exit_code: int, stdout: str, stderr: str
    ):
        """Log detailed execution information."""
        if self.debug_level >= 3:
            self.logger.debug(f"Command executed: {' '.join(command)}")
            self.logger.debug(f"Exit code: {exit_code}")

            if stdout:
                self.logger.debug(f"STDOUT:\n{stdout}")
            if stderr:
                self.logger.debug(f"STDERR:\n{stderr}")

    def show_summary(
        self,
        total_tests: int,
        passed_tests: int,
        failed_tests: int,
        duration: Optional[float] = None,
    ):
        """Show test execution summary."""
        if self.debug_level >= 1:
            self.logger.info("=" * 50)
            self.logger.info("TEST EXECUTION SUMMARY")
            self.logger.info("=" * 50)
            self.logger.info(f"Total tests: {total_tests}")
            self.logger.info(f"Passed: {passed_tests}")
            self.logger.info(f"Failed: {failed_tests}")

            if duration:
                self.logger.info(f"Total duration: {duration:.3f}s")
                if total_tests > 0:
                    avg_duration = duration / total_tests
                    self.logger.info(f"Average per test: {avg_duration:.3f}s")

            self.logger.info("=" * 50)

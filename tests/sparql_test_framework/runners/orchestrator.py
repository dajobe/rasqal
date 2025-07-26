"""
Test Orchestrator

This module provides the main test orchestration functionality for running
multiple test suites and coordinating their execution.

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

import argparse
import logging
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Any

from ..test_types import TestResult, Namespaces, TestTypeResolver
from ..utils import find_tool, run_command, decode_literal
from ..manifest import ManifestParser


# Configure logging
logging.basicConfig(level=logging.WARNING, format="%(levelname)s: %(message)s")
logger = logging.getLogger(__name__)


# --- Constants and Global Configuration ---

NS = Namespaces()

# Counters for test results
COUNTERS = [e.value for e in TestResult]

# Formatting constants for output
LINE_WRAP = 78
BANNER_WIDTH = LINE_WRAP - 10
INDENT_STR = "  "  # Using 2 spaces for indentation

# Global command variables - initialized once at script start
MAKE_CMD = os.environ.get("MAKE", "make")


def format_testsuite_result(
    file_handle,
    result_summary: Dict[str, List[Dict[str, Any]]],
    indent_prefix: str,
    verbose_format: bool,
):
    """
    Formats and prints the summary of a test suite's results to the given file handle.
    """
    failed_tests = result_summary.get(TestResult.FAILED.value, [])
    uxpassed_tests = result_summary.get(TestResult.UXPASSED.value, [])

    if failed_tests:
        file_handle.write(f"{indent_prefix}Failed tests:\n")
        for ftest in failed_tests:
            if verbose_format:
                file_handle.write(f"{indent_prefix}{INDENT_STR}{'=' * BANNER_WIDTH}\n")
            name_detail = ftest.get("name", "Unknown Test")
            if verbose_format:
                name_detail += (
                    f" in suite {ftest.get('testsuite_name', 'N/A')} "
                    f"in {ftest.get('dir', 'N/A')}"
                )
            file_handle.write(f"{indent_prefix}{INDENT_STR}{name_detail}\n")
            if verbose_format and ftest.get("detail"):
                file_handle.write(f"{indent_prefix}{INDENT_STR}{ftest['detail']}\n")
            if verbose_format and ftest.get("log"):
                file_handle.write(f"{indent_prefix}{INDENT_STR}Log: {ftest['log']}\n")
            if verbose_format:
                file_handle.write(f"{indent_prefix}{INDENT_STR}{'=' * BANNER_WIDTH}\n")

    if uxpassed_tests:
        file_handle.write(f"{indent_prefix}Unexpectedly passed tests:\n")
        for utest in uxpassed_tests:
            name_detail = utest.get("name", "Unknown Test")
            if verbose_format:
                name_detail += (
                    f" in suite {utest.get('testsuite_name', 'N/A')} "
                    f"in {utest.get('dir', 'N/A')}"
                )
            file_handle.write(f"{indent_prefix}{INDENT_STR}{name_detail}\n")

    # Add counts summary (like original improve.py)
    passed_count = len(result_summary.get(TestResult.PASSED.value, []))
    failed_count = len(result_summary.get(TestResult.FAILED.value, []))
    skipped_count = len(result_summary.get(TestResult.SKIPPED.value, []))
    xfailed_count = len(result_summary.get(TestResult.XFAILED.value, []))
    uxpassed_count = len(result_summary.get(TestResult.UXPASSED.value, []))

    file_handle.write(
        f"{indent_prefix}Passed: {passed_count}    Failed: {failed_count}    Skipped: {skipped_count}    Xfailed: {xfailed_count}    Uxpassed: {uxpassed_count}    \n"
    )


class Testsuite:
    """
    Represents a single test suite, handling its preparation and execution.
    """

    def __init__(self, directory: Path, name: str, args: argparse.Namespace):
        self.directory = directory
        self.name = name
        self.args = args
        self.description: str = ""
        self.path_for_suite: Optional[str] = None
        self.tests: List[Dict[str, Any]] = []
        self.plan_file: Optional[Path] = None
        self._original_env_path: Optional[str] = None
        self.abort_requested: bool = False

    def prepare(self) -> Dict[str, Any]:
        """
        Prepares the test suite by generating its plan file using 'make' and
        then parsing that plan file.
        """
        self.plan_file = self.directory / f"{self.name}-plan.ttl"

        if not self.args.prepared:
            # Original logic: Generate plan file by calling make
            try:
                if self.plan_file.exists():
                    self.plan_file.unlink()
            except OSError as e:
                logger.warning(
                    f"Could not remove existing plan file {self.plan_file}: {e}"
                )

            make_cmd_list = [MAKE_CMD, f"get-testsuite-{self.name}"]
            logger.debug(
                f"Running command to generate plan in {self.directory}: {' '.join(make_cmd_list)}"
            )

            try:
                logger.debug(
                    f"PATH before subprocess.run: {os.environ.get('PATH', '(not set)')}"
                )
                process = subprocess.run(
                    make_cmd_list,
                    cwd=self.directory,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                    check=False,
                    encoding="utf-8",
                )

                # Filter out g?make[<NUMBER>] lines from stdout
                filtered_lines = []
                if process.stdout:
                    for line in process.stdout.splitlines():
                        if not re.match(r"^g?make\[\d+\]", line):
                            filtered_lines.append(line)
                    filtered_output = "\n".join(filtered_lines)
                    if filtered_lines and not filtered_output.endswith("\n"):
                        filtered_output += "\n"
                else:
                    filtered_output = ""

                # Debug output
                logger.debug(f"Process returncode: {process.returncode}")
                logger.debug(
                    f"Process stdout length: {len(process.stdout) if process.stdout else 0}"
                )
                logger.debug(f"Process stderr: {process.stderr}")
                logger.debug(f"Filtered output length: {len(filtered_output)}")
                logger.debug(
                    f"Filtered output first 200 chars: {filtered_output[:200]}"
                )

                # Write filtered output to plan file
                with self.plan_file.open("w", encoding="utf-8") as f_out:
                    f_out.write(filtered_output)

                if process.returncode != 0:
                    details = (
                        f"'{' '.join(make_cmd_list)}' failed (exit {process.returncode}) "
                        f"in {self.directory}."
                    )
                    if process.stderr:
                        details += f"\nStderr:\n{process.stderr.strip()}"
                    return {"status": "fail", "details": details}

                if not self.plan_file.exists() or self.plan_file.stat().st_size == 0:
                    return {
                        "status": "fail",
                        "details": (
                            f"No testsuite plan file {self.plan_file} created or "
                            f"it is empty in {self.directory}."
                        ),
                    }

            except FileNotFoundError:
                return {
                    "status": "fail",
                    "details": f"'{MAKE_CMD}' not found. Please ensure 'make' is in your PATH.",
                }
            except Exception as e:
                return {
                    "status": "fail",
                    "details": f"An unexpected error occurred generating plan for {self.name}: {e}",
                }
        else:
            # If --prepared, assume plan file already exists and is up-to-date.
            if not self.plan_file.exists():
                return {
                    "status": "fail",
                    "details": f"Plan file {self.plan_file} does not exist in {self.directory}.",
                }

        # Parse the plan file
        try:
            self.tests = self._parse_plan_file(self.plan_file)
            logger.debug(f"Extracted {len(self.tests)} tests from {self.plan_file}")
        except Exception as e:
            return {
                "status": "fail",
                "details": f"Failed to parse plan file {self.plan_file}: {e}",
            }

        return {"status": "success"}

    def _parse_plan_file(self, plan_file: Path) -> List[Dict[str, Any]]:
        """
        Parses a plan file (Turtle format) and extracts test information.

        Plan files are generated by the plan generator and contain test entries
        in Turtle format with specific properties.
        """
        tests = []

        try:
            with open(plan_file, "r") as f:
                content = f.read()
        except Exception as e:
            logger.error(f"Failed to read plan file {plan_file}: {e}")
            return tests

        # Parse Turtle format to extract test entries with action commands
        import re

        # Use a more robust pattern that matches complete test entries
        # Look for entries that start with "[ a" and end with "]"
        entry_pattern = r"\[\s*a\s+[^]]+\]"
        entries = re.findall(entry_pattern, content, re.DOTALL)

        for entry in entries:
            # Extract test type, name, and action from each entry
            type_match = re.search(r"a\s+([^;]+);", entry)
            name_match = re.search(r'mf:name\s+"""([^"]*)""";', entry, re.DOTALL)
            # More robust action pattern that handles multi-line content with escaped quotes
            action_match = re.search(
                r'mf:action\s+"""((?:[^"]|"[^"]*")*)"""', entry, re.DOTALL
            )

            if type_match and name_match and action_match:
                test_type_raw = type_match.group(1).strip()
                test_name = name_match.group(1).strip()
                action = action_match.group(1).strip()

                # Determine expected result based on test type
                from ..test_types import TestTypeResolver, TestResult, Namespaces

                # Remove angle brackets and expand prefixes to get full URI
                test_type_uri = test_type_raw
                if test_type_uri.startswith("<") and test_type_uri.endswith(">"):
                    test_type_uri = test_type_uri[1:-1]
                else:
                    # Expand prefixed name to full URI
                    test_type_uri = Namespaces.prefixed_name_to_uri(test_type_uri)

                should_run, expected_result, test_category = (
                    TestTypeResolver.resolve_test_behavior(test_type_uri)
                )

                test_details = {
                    "name": test_name,
                    "testsuite_name": self.name,
                    "dir": str(self.directory),
                    "test_type": test_type_uri,
                    "action": action,
                    "expect": expected_result,
                    "is_xfail_test": (expected_result == TestResult.XFAILED),
                }

                tests.append(test_details)

        return tests

    def run(self, indent_prefix: str, script_globals: Dict[str, Any]) -> Dict[str, Any]:
        """
        Runs the test suite and returns results.
        """

        class PathManager:
            def __init__(self, original_path: str, suite_path: str):
                self.original_path = original_path
                self.suite_path = suite_path

            def __enter__(self_path_mgr):
                # Add suite path to PATH
                os.environ["PATH"] = (
                    f"{self_path_mgr.suite_path}:{self_path_mgr.original_path}"
                )
                return self_path_mgr

            def __exit__(self_path_mgr, exc_type, exc_val, exc_tb):
                # Restore original PATH
                os.environ["PATH"] = self_path_mgr.original_path

        # Store original PATH
        original_path = os.environ.get("PATH", "")

        # Determine suite path
        if self.path_for_suite:
            suite_path = self.path_for_suite
        else:
            suite_path = str(self.directory)

        # Run tests with path management
        with PathManager(original_path, suite_path):
            results = {
                TestResult.PASSED.value: [],
                TestResult.FAILED.value: [],
                TestResult.SKIPPED.value: [],
                TestResult.XFAILED.value: [],
                TestResult.UXPASSED.value: [],
            }

            # Show suite start message (like original improve.py)
            print(f"{indent_prefix}Running testsuite {self.name}")

            # Show interactive output during test execution
            column = len(indent_prefix)
            if not self.args.verbose:
                print(indent_prefix, end="", flush=True)

            for test in self.tests:
                if script_globals.get("script_aborted_by_user") or self.abort_requested:
                    break

                if self.args.dryrun:
                    # Dryrun mode: mark test as SKIPPED without running it
                    test_result = {
                        "name": test["name"],
                        "testsuite_name": self.name,
                        "dir": str(self.directory),
                        "result": TestResult.SKIPPED.value,
                        "detail": "(dryrun)",
                        "log": "",
                    }
                else:
                    try:
                        test_result = self._run_single_test(test)
                    except KeyboardInterrupt:
                        logger.warning(
                            f"\n{indent_prefix}Test execution aborted by user (SIGINT received)."
                        )
                        test_result = {
                            "name": test["name"],
                            "testsuite_name": self.name,
                            "dir": str(self.directory),
                            "result": TestResult.FAILED.value,
                            "detail": "Aborted by user.",
                            "log": "",
                        }
                        self.abort_requested = True
                        script_globals["script_aborted_by_user"] = True
                        results[TestResult.FAILED.value].append(test_result)
                        break

                if test_result:
                    result_type = test_result.get("result", TestResult.FAILED.value)
                    results[result_type].append(test_result)

                    # Show interactive output (like original improve.py)
                    if not self.args.verbose:
                        # Show status character
                        status_enum = TestResult(result_type)
                        print(status_enum.display_char(), end="", flush=True)
                        column += 1
                        if column >= LINE_WRAP:
                            print(f"\n{indent_prefix}", end="", flush=True)
                            column = len(indent_prefix)
                    else:
                        # Show verbose output
                        status_enum = TestResult(result_type)
                        detail_str = (
                            f" - {test_result.get('detail', '')}"
                            if test_result.get("detail")
                            else ""
                        )
                        print(
                            f"{indent_prefix}  {test_result['name']}: {status_enum.display_name()}{detail_str}"
                        )

                        # Show log tail for failed/unexpectedly passed tests with -vv
                        if (
                            self.args.verbose > 1
                            and status_enum in (TestResult.FAILED, TestResult.UXPASSED)
                            and test_result.get("log")
                        ):
                            log_lines = test_result["log"].splitlines()
                            log_to_show = (
                                ["..."] + log_lines[-15:]
                                if len(log_lines) > 15
                                else log_lines
                            )
                            indented_log = f"\n{indent_prefix}{INDENT_STR*2}".join(
                                log_to_show
                            )
                            print(f"{indent_prefix}{INDENT_STR*2}{indented_log}")

            # Add newline after test execution
            if not self.args.verbose:
                print()

        # Clean up the generated plan file
        if self.plan_file:
            try:
                self.plan_file.unlink(missing_ok=True)
            except OSError as e:
                logger.warning(f"Could not remove plan file {self.plan_file}: {e}")

        # Determine the overall status of this test suite (like original improve.py)
        # Only FAILED and UXPASSED count as failures
        # XFAILED and PASSED (including XFailTest that pass) are considered successes
        overall_suite_status = (
            "fail"
            if results.get(TestResult.FAILED.value)
            or results.get(TestResult.UXPASSED.value)
            else "pass"
        )

        return {
            "summary": results,
            "status": overall_suite_status,
            "abort_requested": self.abort_requested,
        }

    def _run_single_test(
        self, test_details: Dict[str, Any]
    ) -> Optional[Dict[str, Any]]:
        """
        Runs a single test defined in test_details within its test suite context.
        Returns test result with status, detail, and log.
        """
        test_name = test_details["name"]
        action_cmd = test_details["action"]
        expected_status_enum: TestResult = test_details["expect"]

        # Initialize result
        result = {
            "name": test_name,
            "testsuite_name": self.name,
            "dir": str(self.directory),
            "result": TestResult.FAILED.value,
            "detail": "",
            "log": "",
        }

        path_prefix = (
            f"PATH={os.environ.get('PATH','(not set)')} " if self.path_for_suite else ""
        )
        if self.args.debug:
            logger.debug(f"Running test '{test_name}': {path_prefix}{action_cmd}")

        name_slug = re.sub(r"[^\w\-.]", "-", test_name) if test_name else "unnamed-test"
        log_file_path = self.directory / f"{name_slug}.log"

        actual_run_status: TestResult = TestResult.FAILED

        try:
            full_cmd_for_shell = f'{action_cmd} > "{str(log_file_path)}" 2>&1'
            logger.debug(
                f"PATH before subprocess.run: {os.environ.get('PATH', '(not set)')}"
            )
            process = subprocess.run(
                full_cmd_for_shell,
                cwd=self.directory,
                shell=True,
                text=True,
                check=False,
                encoding="utf-8",
            )
            return_code = process.returncode

            if return_code != 0:
                result["detail"] = (
                    f"Action '{action_cmd}' exited with code {return_code}"
                )
                actual_run_status = TestResult.FAILED
            else:
                actual_run_status = TestResult.PASSED

            if log_file_path.exists():
                result["log"] = log_file_path.read_text(encoding="utf-8")
                if (
                    actual_run_status == TestResult.FAILED
                    and self.args.verbose
                    and not result["detail"]
                ):
                    log_lines = result["log"].splitlines()
                    if log_lines:
                        result["detail"] += "\nLog tail:\n" + "\n".join(log_lines[-5:])

        except Exception as e:
            result["detail"] = f"Failed to execute action '{action_cmd}': {e}"
            actual_run_status = TestResult.FAILED
        finally:
            try:
                if log_file_path.exists():
                    log_file_path.unlink()
            except OSError as e_unlink:
                logger.warning(
                    f"Could not remove test log file {log_file_path}: {e_unlink}"
                )

        # Determine the final test status based on actual execution outcome vs. expected outcome
        is_xfail_test = test_details.get("is_xfail_test", False)

        # Use centralized test result determination logic
        final_result, detail = TestTypeResolver.determine_test_result(
            expected_status_enum, actual_run_status
        )
        result["result"] = final_result.value
        if detail:
            result["detail"] = result.get("detail", "") + " (" + detail + ")"

        return result


class TestOrchestrator:
    """Main test orchestrator for coordinating multiple test suites."""

    def __init__(self):
        self.args = None
        self.script_globals = {}

    def setup_argument_parser(self) -> argparse.ArgumentParser:
        """Setup argument parser for the orchestrator."""
        parser = argparse.ArgumentParser(
            description="Run Rasqal test suites",
            formatter_class=argparse.RawDescriptionHelpFormatter,
            epilog="""
Examples:
  %(prog)s /path/to/test/directory
  %(prog)s /path/to/test/directory sparql11
  %(prog)s --prepared /path/to/test/directory
            """,
        )

        parser.add_argument(
            "directory",
            type=Path,
            help="Directory containing test suites",
        )
        parser.add_argument(
            "testsuites",
            nargs="*",
            help="Optional list of specific test suites to run within the specified DIR. "
            "If not provided, all known test suites in DIR will be run.",
        )
        parser.add_argument(
            "--prepared",
            action="store_true",
            help="Assume test suite plan files are already prepared",
        )
        parser.add_argument(
            "-v",
            "--verbose",
            action="count",
            default=0,
            help="Enable extra verbosity when running tests (use multiple -v for more detail).\n"
            "  -v: Show each test name and its status on a new line.\n"
            "  -vv: In addition to -v, show tail of test logs for failed/unexpectedly passed tests.",
        )
        parser.add_argument(
            "-d",
            "--debug",
            action="count",
            default=0,
            help="Enable extra debugging output (use multiple -d for more detail).\n"
            "  -d: DEBUG level for logger.\n"
            "  -dd: More internal debugging for parsing and execution details.",
        )
        parser.add_argument(
            "-n",
            "--dryrun",
            action="store_true",
            help="Do not run tests, just simulate the process and show what would be done.",
        )
        parser.add_argument(
            "-r",
            "--recursive",
            action="store_true",
            help="Run all testsuites found below the given DIR (requires Makefiles in subdirectories).",
        )

        return parser

    def process_arguments(self, args: argparse.Namespace) -> None:
        """Process arguments and setup orchestrator state."""
        self.args = args

        # Setup logging (like original improve.py)
        if args.debug > 0:
            logging.getLogger().setLevel(logging.DEBUG)
        elif args.verbose > 0:
            logging.getLogger().setLevel(logging.INFO)
        else:
            logging.getLogger().setLevel(logging.WARNING)

    def get_testsuites_from_dir(self, directory: Path) -> List[str]:
        """Get list of available test suites from directory."""
        suites = []

        # First check if make is available
        try:
            subprocess.run([MAKE_CMD, "--version"], capture_output=True, check=True)
        except (subprocess.CalledProcessError, FileNotFoundError):
            logger.error(
                f"'{MAKE_CMD}' not found. Please ensure 'make' is in your PATH."
            )
            return []

        # Try to get initial make working
        try:
            subprocess.run([MAKE_CMD], cwd=directory, capture_output=True, check=False)
        except Exception as e_init:
            logger.error(f"Error running initial '{MAKE_CMD}' in {directory}: {e_init}")
            return []

        # Use make get-testsuites-list to discover test suites
        make_cmd_list = [MAKE_CMD, "get-testsuites-list"]
        logger.debug(
            f"Running command to get testsuites list in {directory}: {' '.join(make_cmd_list)}"
        )

        try:
            process = subprocess.run(
                make_cmd_list,
                cwd=directory,
                capture_output=True,
                text=True,
                encoding="utf-8",
                check=False,
            )

            if process.returncode != 0:
                logger.warning(
                    f"'{' '.join(make_cmd_list)}' target failed in {directory} "
                    f"(exit code {process.returncode}). Stderr: {process.stderr.strip()}"
                )
                return []

            lines = process.stdout.splitlines()
            for line in reversed(lines):
                line_strip = line.strip()
                if line_strip and "ing directory" not in line_strip:
                    return line_strip.split()
            return []

        except Exception as e:
            logger.error(
                f"An unexpected error occurred running get-testsuites-list in {directory}: {e}"
            )
            return []

    def process_directory(
        self,
        directory_path: Path,
        specified_testsuites: Optional[List[str]],
    ) -> Tuple[Dict[str, List[Dict[str, Any]]], int, bool]:
        """
        Process a directory containing test suites.
        """
        if not directory_path.exists():
            logger.error(f"Directory {directory_path} does not exist")
            return {}, 1, False

        if not directory_path.is_dir():
            logger.error(f"{directory_path} is not a directory")
            return {}, 1, False

        # Get available test suites
        available_suites = self.get_testsuites_from_dir(directory_path)
        if not available_suites:
            logger.info(f"No test suites found in {directory_path}")
            return (
                {
                    TestResult.PASSED.value: [],
                    TestResult.FAILED.value: [],
                    TestResult.SKIPPED.value: [],
                    TestResult.XFAILED.value: [],
                    TestResult.UXPASSED.value: [],
                },
                0,
                False,
            )

        # Determine which suites to run
        suites_to_run = (
            specified_testsuites if specified_testsuites else available_suites
        )

        # Validate specified suites
        invalid_suites = [s for s in suites_to_run if s not in available_suites]
        if invalid_suites:
            logger.error(f"Invalid test suites: {invalid_suites}")
            logger.error(f"Available suites: {available_suites}")
            return {}, 1, False

        # Run test suites
        all_results = {
            TestResult.PASSED.value: [],
            TestResult.FAILED.value: [],
            TestResult.SKIPPED.value: [],
            TestResult.XFAILED.value: [],
            TestResult.UXPASSED.value: [],
        }

        total_failures = 0
        abort_requested = False

        for suite_name in suites_to_run:
            if abort_requested:
                break

            logger.info(f"Running test suite: {suite_name}")

            suite = Testsuite(directory_path, suite_name, self.args)

            # Prepare the suite
            prep_result = suite.prepare()
            if prep_result["status"] != "success":
                logger.error(
                    f"Failed to prepare suite {suite_name}: {prep_result['details']}"
                )
                total_failures += 1
                continue

            # Run the suite
            suite_results = suite.run("  ", self.script_globals)

            # Show per-suite summary by default (like original improve.py)
            print(f"  Summary for suite {suite_name}:")
            format_testsuite_result(
                sys.stdout, suite_results["summary"], "    ", self.args.verbose
            )
            if not self.args.verbose:
                print()

            # Aggregate results
            for result_type in all_results:
                all_results[result_type].extend(suite_results["summary"][result_type])

            # Check for failures
            failed_count = len(suite_results["summary"][TestResult.FAILED.value])
            if failed_count > 0:
                total_failures += failed_count

            # Check for abort
            if suite_results["abort_requested"]:
                abort_requested = True

        return all_results, total_failures, abort_requested

    def main(self) -> int:
        """Main entry point for test orchestration."""
        parser = self.setup_argument_parser()
        args = parser.parse_args()
        self.process_arguments(args)

        # Determine test suites to run (like original improve.py)
        specified_suites = args.testsuites if args.testsuites else None

        # Process the directory
        results, failures, aborted = self.process_directory(
            args.directory, specified_suites
        )

        # Directory summary (like original improve.py)
        if results:
            print(f"\n  Testsuites summary for directory {args.directory}:")
            format_testsuite_result(sys.stdout, results, "    ", self.args.verbose)
            print()

        # Total summary (like original improve.py)
        print("---")
        print("Total of all Testsuites (completed):")
        format_testsuite_result(sys.stdout, results, "  ", self.args.verbose)

        return 1 if failures > 0 else 0


def main():
    """Main entry point for test orchestration."""
    orchestrator = TestOrchestrator()
    return orchestrator.main()


if __name__ == "__main__":
    sys.exit(main())

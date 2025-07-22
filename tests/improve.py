#!/usr/bin/env python3
#
# improve - Run Rasqal test suites
#
# USAGE: improve [options] [DIRECTORY [TESTSUITE]]
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
# REQUIRES:
#    GNU 'make' in the PATH (or where envariable MAKE is)
#    TO_NTRIPLES (Rasqal utility) for parsing generated manifests
#

import argparse
import logging
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Any

import sys
from pathlib import Path

# Add the parent directory to the Python path to find rasqal_test_util
sys.path.append(str(Path(__file__).resolve().parent))

from rasqal_test_util import (
    Namespaces,
    TestResult,
    find_tool,
    run_command,
    ManifestParser,
    decode_literal,
)

# Configure logging: Set up a basic logger that will be configured further by argparse
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
                log_lines = ftest["log"].splitlines()
                log_to_show = (
                    ["..."] + log_lines[-15:] if len(log_lines) > 15 else log_lines
                )
                indented_log = f"\n{indent_prefix}{INDENT_STR * 2}".join(log_to_show)
                file_handle.write(f"{indent_prefix}{INDENT_STR * 2}{indented_log}\n")
            if verbose_format:
                file_handle.write(f"{indent_prefix}{INDENT_STR}{'=' * BANNER_WIDTH}\n")

    if uxpassed_tests:
        file_handle.write(f"{indent_prefix}Unexpected passed tests:\n")
        for utest in uxpassed_tests:
            name_detail = utest.get("name", "Unknown Test")
            if logger.level == logging.DEBUG:
                name_detail += f" ({utest.get('test_uri', 'N/A')})"
            file_handle.write(f"{indent_prefix}{INDENT_STR}{name_detail}\n")

    file_handle.write(indent_prefix)
    for counter_name in COUNTERS:
        file_handle.write(
            f"{counter_name.capitalize()}: {len(result_summary.get(counter_name, []))}    "
        )
    file_handle.write("\n")


# --- Main Logic Classes ---


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
                if process.stdout:
                    filtered_lines = []
                    for line in process.stdout.splitlines():
                        if not re.match(r"^g?make\[\d+\]", line):
                            filtered_lines.append(line)
                    filtered_output = "\n".join(filtered_lines)
                    if filtered_lines and not filtered_output.endswith("\n"):
                        filtered_output += "\n"
                else:
                    filtered_output = ""

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
                    "details": f"Prepared mode: Plan file {self.plan_file} does not exist.",
                }
            logger.debug(f"Using pre-generated plan file: {self.plan_file}")

        # This part (reading and parsing the plan file) runs in both modes.
        try:
            parser = ManifestParser(self.plan_file)
            self.tests = self._extract_tests_from_parser(parser)
        except (RuntimeError, FileNotFoundError) as e:
            return {
                "status": "fail",
                "details": f"Failed to parse plan file {self.plan_file}: {e}",
            }

        if self.path_for_suite:
            self._original_env_path = os.environ.get("PATH", "")
            new_path_segment = str((self.directory / self.path_for_suite).resolve())
            os.environ["PATH"] = f"{new_path_segment}{os.pathsep}{os.environ['PATH']}"
            if self.args.debug > 0:
                logger.debug(
                    f"Prepended to PATH for suite '{self.name}': {new_path_segment}. "
                    f"New PATH: {os.environ['PATH']}"
                )

        return {"status": "pass", "details": ""}

    def _extract_tests_from_parser(
        self, parser: ManifestParser
    ) -> List[Dict[str, Any]]:
        """Extracts test details from a parsed manifest."""
        tests = []
        manifest_node_uri = None
        for s, triples in parser.triples_by_subject.items():
            for t in triples:
                if t["p"] == f"<{NS.RDF}type>" and t["o_full"] == f"<{NS.MF}Manifest>":
                    manifest_node_uri = s
                    break
            if manifest_node_uri:
                break

        if not manifest_node_uri:
            return []

        # Extract suite-level properties
        for triple in parser.triples_by_subject.get(manifest_node_uri, []):
            if triple["p"] == f"<{NS.RDFS}comment>":
                self.description = decode_literal(triple["o_full"])
            elif triple["p"] == f"<{NS.T}path>":
                logger.debug(
                    f"Found t:path triple: p='{triple['p']}', o_full='{triple['o_full']}'"
                )
                self.path_for_suite = decode_literal(triple["o_full"])
                logger.debug(f"Decoded t:path value: '{self.path_for_suite}'")

        entries_list_head = next(
            (
                t["o_full"]
                for t in parser.triples_by_subject.get(manifest_node_uri, [])
                if t["p"] == f"<{NS.MF}entries>"
            ),
            None,
        )

        current_list_item_node = entries_list_head
        while current_list_item_node and current_list_item_node != f"<{NS.RDF}nil>":
            list_node_triples = parser.triples_by_subject.get(
                current_list_item_node, []
            )
            entry_node_full = next(
                (
                    t["o_full"]
                    for t in list_node_triples
                    if t["p"] == f"<{NS.RDF}first>"
                ),
                None,
            )

            if entry_node_full:
                entry_triples = parser.triples_by_subject.get(entry_node_full, [])
                test_name = decode_literal(
                    next(
                        (
                            t["o_full"]
                            for t in entry_triples
                            if t["p"] == f"<{NS.MF}name>"
                        ),
                        '""',
                    )
                )
                test_action = decode_literal(
                    next(
                        (
                            t["o_full"]
                            for t in entry_triples
                            if t["p"] == f"<{NS.MF}action>"
                        ),
                        '""',
                    )
                )
                entry_type_full = next(
                    (t["o_full"] for t in entry_triples if t["p"] == f"<{NS.RDF}type>"),
                    "",
                )

                expect = TestResult.PASSED
                is_xfail_test = False
                if entry_type_full == f"<{NS.T}NegativeTest>":
                    expect = TestResult.FAILED
                elif entry_type_full == f"<{NS.T}XFailTest>":
                    expect = TestResult.FAILED
                    is_xfail_test = True

                tests.append(
                    {
                        "name": test_name,
                        "action": test_action,
                        "expect": expect,
                        "is_xfail_test": is_xfail_test,
                        "dir": self.directory,
                        "test_uri": (
                            entry_node_full[1:-1]
                            if entry_node_full.startswith("<")
                            else entry_node_full
                        ),
                    }
                )

            current_list_item_node = next(
                (t["o_full"] for t in list_node_triples if t["p"] == f"<{NS.RDF}rest>"),
                None,
            )

        return tests

    def run(self, indent_prefix: str, script_globals: Dict[str, Any]) -> Dict[str, Any]:
        """
        Runs all tests within this test suite.
        """
        print(f"{indent_prefix}Running testsuite {self.name}: {self.description}")

        results = {counter: [] for counter in COUNTERS}
        column = len(indent_prefix)

        # Context manager for PATH modification
        class PathManager:
            def __enter__(self_path_mgr):
                logger.debug(
                    f"PathManager.__enter__: path_for_suite='{self.path_for_suite}', debug={self.args.debug}"
                )
                if self.path_for_suite:
                    self_path_mgr._original_path = os.environ.get("PATH", "")
                    new_path_segment = str(
                        (self.directory / self.path_for_suite).resolve()
                    )
                    os.environ["PATH"] = (
                        f"{new_path_segment}{os.pathsep}{os.environ['PATH']}"
                    )
                    logger.debug(
                        f"PathManager set PATH for suite '{self.name}': {new_path_segment}. "
                        f"New PATH: {os.environ['PATH']}"
                    )

            def __exit__(self_path_mgr, exc_type, exc_val, exc_tb):
                if self.path_for_suite and hasattr(self_path_mgr, '_original_path'):
                    os.environ["PATH"] = self_path_mgr._original_path
                    if self.args.debug > 0:
                        logger.debug(f"Restored PATH to: {self_path_mgr._original_path}")

        with PathManager():
            if not self.args.verbose:
                print(indent_prefix, end="", flush=True)

            for test_detail in self.tests:
                if script_globals.get("script_aborted_by_user") or self.abort_requested:
                    break

                test_detail.update({"testsuite_name": self.name, "dir": self.directory})

                if self.args.dryrun:
                    test_detail["status"] = TestResult.SKIPPED.value
                    test_detail["detail"] = "(dryrun)"
                else:
                    try:
                        self._run_single_test(test_detail)
                    except KeyboardInterrupt:
                        logger.warning(
                            f"\n{indent_prefix}Test execution aborted by user (SIGINT received)."
                        )
                        test_detail.update(
                            {
                                "status": TestResult.FAILED.value,
                                "detail": "Aborted by user.",
                            }
                        )
                        self.abort_requested = True
                        script_globals["script_aborted_by_user"] = True
                        results[TestResult.FAILED.value].append(test_detail)
                        break

                results[test_detail["status"]].append(test_detail)

                if self.args.verbose == 0:
                    status_enum = TestResult(test_detail["status"])
                    print(status_enum.display_char(), end="", flush=True)
                    column += 1
                    if column >= LINE_WRAP:
                        print(f"\n{indent_prefix}", end="", flush=True)
                        column = len(indent_prefix)
                else:
                    status_enum = TestResult(test_detail["status"])
                    detail_str = (
                        f" - {test_detail['detail']}"
                        if test_detail.get("detail")
                        else ""
                    )
                    print(
                        f"{indent_prefix}{INDENT_STR}{test_detail['name']}: {status_enum.display_name()}{detail_str}"
                    )
                    if (
                        self.args.verbose > 1
                        and status_enum in (TestResult.FAILED, TestResult.UXPASSED)
                        and test_detail.get("log")
                    ):
                        log_lines = test_detail["log"].splitlines()
                        log_to_show = (
                            ["..."] + log_lines[-15:]
                            if len(log_lines) > 15
                            else log_lines
                        )
                        indented_log = f"\n{indent_prefix}{INDENT_STR*2}".join(
                            log_to_show
                        )
                        print(f"{indent_prefix}{INDENT_STR*2}{indented_log}")

            if not self.args.verbose:
                print()

        # Clean up the generated plan file
        if self.plan_file:
            try:
                self.plan_file.unlink(missing_ok=True)
            except OSError as e:
                logger.warning(f"Could not remove plan file {self.plan_file}: {e}")

        # Determine the overall status of this test suite
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

    def _run_single_test(self, test_details: Dict[str, Any]) -> None:
        """
        Runs a single test defined in test_details within its test suite context.
        Updates test_details with status, detail, and log.
        """
        test_name = test_details["name"]
        action_cmd = test_details["action"]
        expected_status_enum: TestResult = test_details["expect"]

        test_details.update(
            {"status": TestResult.FAILED.value, "detail": "", "log": ""}
        )

        path_prefix = (
            f"PATH={os.environ.get('PATH','(not set)')} " if self.path_for_suite else ""
        )
        if self.args.debug > 1:
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
                test_details["detail"] = (
                    f"Action '{action_cmd}' exited with code {return_code}"
                )
                actual_run_status = TestResult.FAILED
            else:
                actual_run_status = TestResult.PASSED

            if log_file_path.exists():
                test_details["log"] = log_file_path.read_text(encoding="utf-8")
                if (
                    actual_run_status == TestResult.FAILED
                    and self.args.verbose > 0
                    and not test_details["detail"]
                ):
                    log_lines = test_details["log"].splitlines()
                    if log_lines:
                        test_details["detail"] += "\nLog tail:\n" + "\n".join(
                            log_lines[-5:]
                        )

        except Exception as e:
            test_details["detail"] = f"Failed to execute action '{action_cmd}': {e}"
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

        if expected_status_enum == TestResult.FAILED:
            if actual_run_status == TestResult.FAILED:
                test_details["status"] = TestResult.XFAILED.value
                test_details["detail"] = (
                    test_details.get("detail", "") + " (Test failed as expected)"
                )
            else:
                # For XFailTest, if it passes, mark it as PASSED (not UXPASSED)
                if is_xfail_test:
                    test_details["status"] = TestResult.PASSED.value
                    test_details["detail"] = (
                        "Test passed (XFailTest - expected to fail but passed)"
                    )
                else:
                    test_details["status"] = TestResult.UXPASSED.value
                    test_details["detail"] = "Test passed but was expected to fail."
        else:  # expected_status_enum == TestResult.PASSED
            if actual_run_status == TestResult.PASSED:
                test_details["status"] = TestResult.PASSED.value
            else:
                test_details["status"] = TestResult.FAILED.value


def get_testsuites_from_dir(directory: Path, args: argparse.Namespace) -> List[str]:
    """
    Executes 'make get-testsuites-list' in the given directory to find available test suites.
    """
    logger.debug(
        f"Running initial '{MAKE_CMD}' in {directory} to ensure it's up-to-date."
    )
    try:
        # Run a simple 'make' to ensure the directory is ready.
        logger.debug(
            f"PATH before subprocess.run: {os.environ.get('PATH', '(not set)')}"
        )
        subprocess.run(
            [MAKE_CMD],
            cwd=directory,
            capture_output=True,
            text=True,
            encoding="utf-8",
            check=False,
        )
    except FileNotFoundError:
        logger.error(
            f"'{MAKE_CMD}' command not found for initial make in {directory}. Please ensure 'make' is in your PATH."
        )
        return []
    except Exception as e_init:
        logger.error(f"Error running initial '{MAKE_CMD}' in {directory}: {e_init}")
        return []

    make_cmd_list = [MAKE_CMD, "get-testsuites-list"]
    logger.debug(
        f"Running command to get testsuites list in {directory}: {' '.join(make_cmd_list)}"
    )
    try:
        logger.debug(
            f"PATH before subprocess.run: {os.environ.get('PATH', '(not set)')}"
        )
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

    except FileNotFoundError:
        logger.error(
            f"'{MAKE_CMD}' not found when trying to get testsuites list. Please ensure 'make' is in your PATH."
        )
        return []
    except Exception as e:
        logger.error(
            f"An unexpected error occurred running get-testsuites-list in {directory}: {e}"
        )
        return []


def process_directory(
    directory_path: Path,
    specified_testsuites: Optional[List[str]],
    args: argparse.Namespace,
    script_globals: Dict[str, Any],
) -> Tuple[Dict[str, List[Dict[str, Any]]], int, bool]:
    """
    Processes a single directory, finding and running specified or all test suites within it.
    Returns a summary of results, an overall status code (0 for pass, 1 for fail),
    and a flag indicating if the script was aborted by the user.
    """
    logger.info(f"Processing directory: {directory_path}")
    dir_indent = INDENT_STR

    # If specified_testsuites is provided (e.g., from make in --prepared mode), use it directly.
    # Otherwise, discover suites using get_testsuites_from_dir (for standalone mode).
    if specified_testsuites:
        suites_to_process = specified_testsuites
        # Optional: You might want to validate these against known_suites_in_dir
        # if you want to catch cases where make passed a non-existent suite.
        # For now, we trust make in --prepared mode.
    else:
        known_suites_in_dir = get_testsuites_from_dir(directory_path, args)
        suites_to_process = known_suites_in_dir

    if not suites_to_process:
        logger.info(
            f"{dir_indent}No test suites to process in directory {directory_path}."
        )
        return (
            {counter: [] for counter in COUNTERS},
            0,
            script_globals.get("script_aborted_by_user", False),
        )

    logger.info(
        f"{dir_indent}Running testsuites: {', '.join(suites_to_process)} in {directory_path}"
    )

    dir_summary = {counter: [] for counter in COUNTERS}
    dir_overall_status = 0

    for suite_name in suites_to_process:
        if script_globals.get("script_aborted_by_user"):
            break

        testsuite = Testsuite(directory_path, suite_name, args)
        prep_result = testsuite.prepare()

        if prep_result["status"] == "fail":
            logger.error(
                f"{dir_indent}{INDENT_STR}Failed to prepare testsuite '{suite_name}': {prep_result['details']}"
            )
            dir_overall_status = 1
            continue

        if not testsuite.tests and not args.dryrun:
            logger.info(
                f"{dir_indent}{INDENT_STR}Testsuite '{suite_name}': No tests found in plan."
            )
            results_for_empty_suite = {counter: [] for counter in COUNTERS}
            print(f"{dir_indent}{INDENT_STR}Summary for suite {suite_name}:")
            format_testsuite_result(
                sys.stdout,
                results_for_empty_suite,
                dir_indent + INDENT_STR + INDENT_STR,
                args.verbose > 0,
            )
            if args.verbose == 0:
                print()
            continue

        try:
            suite_run_result = testsuite.run(dir_indent + INDENT_STR, script_globals)
            print(f"{dir_indent}{INDENT_STR}Summary for suite {suite_name}:")
            format_testsuite_result(
                sys.stdout,
                suite_run_result["summary"],
                dir_indent + INDENT_STR + INDENT_STR,
                args.verbose > 0,
            )
            if args.verbose == 0 and not testsuite.tests and not args.dryrun:
                print()

            for counter_name in COUNTERS:
                dir_summary[counter_name].extend(
                    suite_run_result["summary"].get(counter_name, [])
                )

            if suite_run_result["status"] == "fail":
                dir_overall_status = 1

            if suite_run_result["abort_requested"]:
                script_globals["script_aborted_by_user"] = True
                break

        except KeyboardInterrupt:
            logger.warning(
                f"\n{dir_indent}Run of suite '{suite_name}' aborted by user (SIGINT received)."
            )
            dir_overall_status = 1
            script_globals["script_aborted_by_user"] = True
            break

    if suites_to_process and not script_globals.get("script_aborted_by_user"):
        print(f"\n{dir_indent}Testsuites summary for directory {directory_path}:")
        format_testsuite_result(
            sys.stdout, dir_summary, dir_indent + INDENT_STR, args.verbose > 0
        )

    return (
        dir_summary,
        dir_overall_status,
        script_globals.get("script_aborted_by_user", False),
    )


def main():
    """
    Main entry point for the improve script.
    Handles argument parsing, directory scanning, and overall test execution flow.
    """

    parser = argparse.ArgumentParser(
        description="Run Rasqal test suites.",
        formatter_class=argparse.RawTextHelpFormatter,
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
        "DIR",
        nargs="?",
        default=".",
        help="Base directory to find test suites (defaults to current directory '.').",
    )
    parser.add_argument(
        "TESTSUITES",
        nargs="*",
        help="Optional list of specific test suites to run within the specified DIR. "
        "If not provided, all known test suites in DIR will be run.",
    )
    parser.add_argument(
        "--prepared",
        action="store_true",
        help="Internal use: Assume test plans are pre-generated by make. "
        "Skips calling 'make' for plan generation.",
    )

    args = parser.parse_args()

    if args.debug > 0:
        logger.setLevel(logging.DEBUG)
    elif args.verbose > 0:
        logger.setLevel(logging.INFO)
    else:
        logger.setLevel(logging.WARNING)

    logger.debug(f"Starting {sys.argv[0]} with arguments: {args}")

    if args.prepared:
        # In --prepared mode, the first positional argument is the base directory,
        # and subsequent positional arguments are the specific test suites.
        base_dir = Path(args.DIR).resolve()
        suites_to_process_in_base_dir = (
            args.TESTSUITES
        )  # These are the suites make passed
        dirs_to_scan = [base_dir]  # Only process the base directory
        logger.info(
            f"Running in --prepared mode. Base directory: {base_dir}, Suites: {suites_to_process_in_base_dir}"
        )
    else:
        # Original logic for finding directories and suites (for standalone execution)
        base_dir = Path(args.DIR).resolve()
        dirs_to_scan: List[Path] = []

        if args.recursive:
            logger.info(
                f"Initiating recursive scan for testsuites starting from {base_dir}"
            )
            for item in base_dir.rglob("*"):
                if item.is_dir() and (
                    (item / "Makefile").is_file() or (item / "Makefile.am").is_file()
                ):
                    # In standalone mode, we still need to ask make for suites
                    if get_testsuites_from_dir(item, args):
                        dirs_to_scan.append(item)
                    else:
                        logger.debug(f"    No testsuites listed by 'make' in {item}")
            if not dirs_to_scan and (
                (base_dir / "Makefile").is_file()
                or (base_dir / "Makefile.am").is_file()
            ):
                if get_testsuites_from_dir(base_dir, args):
                    dirs_to_scan.append(base_dir)
        else:
            dirs_to_scan.append(base_dir)

        # In standalone mode, if specific TESTSUITES were provided, they apply to the base_dir
        suites_to_process_in_base_dir = args.TESTSUITES if not args.recursive else []

    if not dirs_to_scan:
        logger.info(
            "No directories with test suites found based on specified criteria. Exiting cleanly."
        )
        sys.exit(0)

    logger.info(
        f"Effective directories to scan for testsuites: {[str(d) for d in dirs_to_scan]}"
    )

    overall_final_status_code = 0
    script_globals = {"script_aborted_by_user": False}
    grand_total_results_summary = {counter: [] for counter in COUNTERS}
    processed_dirs_count = 0

    for current_run_dir in dirs_to_scan:
        if script_globals["script_aborted_by_user"]:
            logger.info("Skipping remaining directories due to user interrupt.")
            break

        # Pass the correct list of suites to process_directory
        # If in --prepared mode, use suites_to_process_in_base_dir
        # Otherwise, process_directory will discover them or use args.TESTSUITES
        suites_for_this_dir = (
            suites_to_process_in_base_dir
            if args.prepared and current_run_dir == base_dir
            else args.TESTSUITES
        )

        dir_summary, dir_status, aborted_this_dir = process_directory(
            current_run_dir, suites_for_this_dir, args, script_globals
        )
        processed_dirs_count += 1

        for counter_name in COUNTERS:
            grand_total_results_summary[counter_name].extend(
                dir_summary.get(counter_name, [])
            )

        if dir_status != 0:
            overall_final_status_code = 1

        if aborted_this_dir:
            script_globals["script_aborted_by_user"] = True
            break

    if args.recursive or processed_dirs_count == 1:
        print(f"\n---")
        print(
            f"Total of all Testsuites ({'partial due to abort' if script_globals['script_aborted_by_user'] else 'completed'}):"
        )
        format_testsuite_result(
            sys.stdout, grand_total_results_summary, INDENT_STR, True
        )

    if script_globals["script_aborted_by_user"]:
        logger.info("Exiting script due to user abort (Ctrl+C).")
        sys.exit(130)
    else:
        sys.exit(overall_final_status_code)


if __name__ == "__main__":
    main()

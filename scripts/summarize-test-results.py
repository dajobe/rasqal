#!/usr/bin/env python3
#
# Summarize test results from 'make check' output log
#
# USAGE:
#   summarize-test-results.py [OPTIONS] [LOG-FILE]
#
#   If LOG-FILE is not provided, reads from stdin (useful for piping):
#     make check 2>&1 | summarize-test-results.py
#
# Copyright (C) 2025, David Beckett https://www.dajobe.org/
#
# This package is Free Software and part of Redland https://librdf.org/
#
# It is licensed under the following three licenses as alternatives:
#   1. GNU Lesser General Public License (LGPL) V2.1 or any newer version
#   2. GNU General Public License (GPL) V2 or any newer version
#   3. Apache License, V2.0 or any newer version
#
# You may not use this file except in compliance with at least one of
# the above three licenses.
#
# See LICENSE.html or LICENSE.txt at the top of this package for the
# complete terms and further detail along with the license texts for
# the licenses in COPYING.LIB, COPYING and LICENSE-2.0.txt respectively.
#

import argparse
import os
import re
import sys
from collections import defaultdict
from pathlib import Path
from typing import Dict, Optional, Tuple


# ANSI color codes
class Colors:
    """ANSI color codes for terminal output."""

    RESET = "\033[0m"
    BOLD = "\033[1m"
    RED = "\033[31m"
    GREEN = "\033[32m"
    YELLOW = "\033[33m"
    BLUE = "\033[34m"
    MAGENTA = "\033[35m"
    CYAN = "\033[36m"
    ORANGE = "\033[38;5;208m"  # 256-color mode orange

    @staticmethod
    def disable():
        """Disable all colors (for non-terminal output)."""
        Colors.RESET = ""
        Colors.BOLD = ""
        Colors.RED = ""
        Colors.GREEN = ""
        Colors.YELLOW = ""
        Colors.BLUE = ""
        Colors.MAGENTA = ""
        Colors.CYAN = ""
        Colors.ORANGE = ""


class TestSummary:
    """Container for test summary statistics."""

    def __init__(self):
        self.unit_tests = None
        self.library_tests = {}
        self.sparql_suites = defaultdict(
            lambda: {"passed": 0, "failed": 0, "skipped": 0, "xfailed": 0}
        )
        self.compare_tests = None
        self.log_file = None
        self.line_count = 0
        # Detailed test information
        # Each entry is a dict with: suite, test_id, directory, error_detail, log_path
        self.failed_tests = []
        self.xfailed_tests = []
        # Unit test failures with details
        self.unit_test_failures = []

    def has_data(self) -> bool:
        """Check if any test data was found."""
        return (
            self.unit_tests is not None
            or len(self.library_tests) > 0
            or len(self.sparql_suites) > 0
            or self.compare_tests is not None
        )


def parse_unit_test_failures(content: str) -> list:
    """Parse detailed unit test failure information from log content."""
    failures = []

    # Look for FAIL: test_name lines in the src section
    fail_pattern = r"^FAIL:\s+(\S+)"
    for match in re.finditer(fail_pattern, content, re.MULTILINE):
        test_name = match.group(1)

        # Try to find details about this failure in test-suite.log section
        # Look for the test name followed by error information
        start_pos = match.end()
        context_size = 10000  # Look ahead for details
        context = content[start_pos:start_pos + context_size]

        # Try to extract error summary
        error_detail = None
        leak_summary = None

        # Look for LeakSanitizer summary for this test
        leak_pattern = rf"FAIL:\s+{re.escape(test_name)}.*?SUMMARY:\s+AddressSanitizer:\s+(.+?)(?=\n(?:FAIL|PASS|$))"
        leak_match = re.search(leak_pattern, content, re.DOTALL)
        if leak_match:
            leak_summary = leak_match.group(1).strip()
            # Extract just the summary line
            if '\n' in leak_summary:
                leak_summary = leak_summary.split('\n')[0]

        # Look for segfault/crash info
        crash_pattern = rf"FAIL:\s+{re.escape(test_name)}.*?(AddressSanitizer:\s*(?:SEGV|DEADLYSIGNAL).*?)(?=\n\n|SUMMARY)"
        crash_match = re.search(crash_pattern, content, re.DOTALL)
        if crash_match:
            error_detail = "CRASHED: " + crash_match.group(1).strip().split('\n')[0]
        elif leak_summary:
            error_detail = f"Memory leak: {leak_summary}"

        failures.append({
            "test_name": test_name,
            "error_detail": error_detail,
            "leak_summary": leak_summary
        })

    return failures


def parse_unit_tests(content: str) -> Optional[Dict[str, int]]:
    """Parse unit test results from log content."""
    # Look for the summary that appears after "Making check in src"
    # The pattern needs to be flexible about field order
    pattern = (
        r"Testsuite summary.*?\n"
        r"# TOTAL: (\d+)\n"
        r"# PASS:\s+(\d+)\n"
        r"(?:# SKIP:\s+\d+\n)?"
        r"# XFAIL:\s*(\d+)\n"
        r"# FAIL:\s+(\d+)"
    )

    # First try to find summary in src section
    src_start = content.find("Making check in src")
    if src_start != -1:
        # Look for summary in the section after "Making check in src"
        # but before the next "Making check in" section
        src_section_end = content.find("Making check in", src_start + 20)
        if src_section_end == -1:
            src_section = content[src_start:]
        else:
            src_section = content[src_start:src_section_end]

        # Try the standard pattern first
        match = re.search(pattern, src_section, re.DOTALL)
        if match:
            return {
                "total": int(match.group(1)),
                "passed": int(match.group(2)),
                "xfailed": int(match.group(3)),
                "failed": int(match.group(4)),
            }

        # Fallback: extract fields individually (more flexible)
        summary_match = re.search(
            r"Testsuite summary.*?============================================================================",
            src_section,
            re.DOTALL,
        )
        if summary_match:
            summary_text = summary_match.group(0)
            total_match = re.search(r"# TOTAL:\s+(\d+)", summary_text)
            pass_match = re.search(r"# PASS:\s+(\d+)", summary_text)
            xfail_match = re.search(r"# XFAIL:\s+(\d+)", summary_text)
            fail_match = re.search(r"# FAIL:\s+(\d+)", summary_text)

            if total_match and pass_match:
                return {
                    "total": int(total_match.group(1)),
                    "passed": int(pass_match.group(1)),
                    "xfailed": int(xfail_match.group(1)) if xfail_match else 0,
                    "failed": int(fail_match.group(1)) if fail_match else 0,
                }

    # Fall back: find all matches and use the one with largest total
    matches = list(re.finditer(pattern, content, re.DOTALL))
    if matches:
        best_match = max(matches, key=lambda m: int(m.group(1)))
        return {
            "total": int(best_match.group(1)),
            "passed": int(best_match.group(2)),
            "xfailed": int(best_match.group(3)),
            "failed": int(best_match.group(4)),
        }

    return None


def parse_library_tests(content: str) -> Dict[str, Dict[str, int]]:
    """Parse library test results (libsv, libmtwist, etc.)."""
    libraries = {}
    # Look for summaries after "Making check in libsv" or "Making check in libmtwist"
    for lib_name in ("libsv", "libmtwist"):
        # Find "Making check in libsv" or "Making check in libmtwist"
        lib_pattern = rf"Making check in {lib_name}"
        lib_match = re.search(lib_pattern, content)
        if not lib_match:
            continue

        # Look for the testsuite summary after this point
        start_pos = lib_match.end()
        section_content = content[start_pos : start_pos + 2000]
        pattern = r"Testsuite summary.*?\n" r"# TOTAL: (\d+)\n" r"# PASS:\s+(\d+)"
        match = re.search(pattern, section_content, re.DOTALL)
        if match:
            libraries[lib_name] = {
                "total": int(match.group(1)),
                "passed": int(match.group(2)),
            }
    return libraries


def parse_sparql_suites(content: str) -> Dict[str, Dict[str, int]]:
    """Parse SPARQL test suite results."""
    suites = defaultdict(lambda: {"passed": 0, "failed": 0, "skipped": 0, "xfailed": 0})
    pattern = (
        r"Summary for suite (\S+):\s+"
        r"Passed: (\d+)\s+"
        r"Failed: (\d+)\s+"
        r"Skipped: (\d+)\s+"
        r"Xfailed: (\d+)"
    )
    for match in re.finditer(pattern, content):
        suite = match.group(1)
        suites[suite]["passed"] += int(match.group(2))
        suites[suite]["failed"] += int(match.group(3))
        suites[suite]["skipped"] += int(match.group(4))
        suites[suite]["xfailed"] += int(match.group(5))
    return dict(suites)


def parse_compare_tests(content: str) -> Optional[Dict[str, int]]:
    """Parse rasqal-compare test results."""
    pattern = r"rasqal-compare.*?\n# TOTAL: (\d+)\n# PASS:\s+(\d+)"
    match = re.search(pattern, content)
    if match:
        return {"total": int(match.group(1)), "passed": int(match.group(2))}
    return None


def parse_failed_tests(content: str) -> Tuple[list, list]:
    """Parse failed and xfailed test details from log content.

    Returns:
        Tuple of (failed_tests, xfailed_tests) where each is a list of dicts
        with keys: suite, test_id, directory, error_detail, log_path
    """
    failed_tests = []
    xfailed_tests = []

    # First, find suites with xfailed tests and their directories
    # This handles cases where individual test names aren't in the log
    suite_xfailed_pattern = (
        r"Summary for suite (\S+):\s+"
        r"Passed: \d+\s+"
        r"Failed: \d+\s+"
        r"Skipped: \d+\s+"
        r"Xfailed: (\d+)"
    )
    for match in re.finditer(suite_xfailed_pattern, content):
        suite = match.group(1)
        xfailed_count = int(match.group(2))
        if xfailed_count > 0:
            # Find directory for this suite - look backwards for "Making check in"
            start_pos = max(0, match.start() - 5000)
            context_before = content[start_pos : match.start()]

            # Find the most recent "Making check in" before this suite
            dir_matches = list(
                re.finditer(r"Making check in\s+([^\s\n]+)", context_before)
            )
            if dir_matches:
                directory = dir_matches[-1].group(1)
            else:
                # Try to find directory from path patterns
                dir_match = re.search(
                    r"(tests/[^\s\n/]+(?:/[^\s\n/]+)?)", context_before
                )
                directory = dir_match.group(1) if dir_match else "unknown"

            # Check if we already have individual test entries for this suite
            # (from patterns below that might have found specific test names)
            existing_for_suite = [t for t in xfailed_tests if t.get("suite") == suite]
            if len(existing_for_suite) < xfailed_count:
                # We don't have all individual test names, create a summary entry
                # Only create one entry per suite, not per test
                xfailed_tests.append(
                    {
                        "directory": directory,
                        "suite": suite,
                        "test_id": f"<{xfailed_count} xfailed test{'s' if xfailed_count > 1 else ''} - individual names not in log>",
                        "error_detail": f"Suite has {xfailed_count} expected failure(s). Individual test names are not available in the log output.",
                        "log_path": None,
                    }
                )

    # Pattern 1: "test_name: FAILED - description" or "test_name: XFAILED - description"
    # Look for the full context including directory and suite
    failed_pattern1 = r"^\s+(\S+):\s+(FAILED|XFAILED)\s+-\s*(.+?)(?=\n\s+\w+:|$)"
    for match in re.finditer(failed_pattern1, content, re.MULTILINE | re.DOTALL):
        test_id = match.group(1)
        result_type = match.group(2)
        error_detail = match.group(3).strip()

        # Find context: directory, suite
        start_pos = max(0, match.start() - 2000)
        end_pos = min(len(content), match.end() + 500)
        context_before = content[start_pos : match.start()]
        context_after = content[match.end() : end_pos]
        full_context = context_before + context_after

        # Skip if this appears to be a test framework test (not a real test failure)
        # Look for indicators that this is a framework test:
        # - "test-suite" as suite name (often used in framework tests)
        # - Test name is generic like "test1", "test_name"
        # - Appears in Python unittest output (has "ok" or "FAILED" test result markers)
        if re.search(r"Running testsuite\s+test-suite", context_before):
            # This is likely a framework test, skip it
            continue

        # Skip generic test names that are likely placeholders
        if test_id in ("test1", "test_name", "test", "example_test"):
            # Check if this is in a real test suite context (not framework test)
            if not re.search(r"Summary for suite\s+sparql-", context_before):
                # Likely a placeholder in framework tests, skip
                continue

        # Find directory - look for "Making check in" patterns
        # This appears before test suites run, e.g., "Making check in tests/sparql/part1"
        dir_match = re.search(r"Making check in\s+([^\s\n]+)", context_before)
        directory = dir_match.group(1) if dir_match else None

        # Also look for directory in paths like "tests/sparql/part1" or "tests/algebra"
        if not directory:
            # Look for test directory patterns
            dir_match = re.search(r"(tests/[^\s\n/]+(?:/[^\s\n/]+)?)", context_before)
            if dir_match:
                directory = dir_match.group(1)
            else:
                # Look for any directory path pattern
                dir_match = re.search(r"([^\s\n/]+/[^\s\n/]+)", context_before)
                directory = dir_match.group(1) if dir_match else "unknown"

        # Find suite - look for "Running testsuite" or "Summary for suite"
        suite_match = re.search(
            r"Running testsuite\s+(\S+)|Summary for suite\s+(\S+):", context_before
        )
        suite = (
            (suite_match.group(1) or suite_match.group(2)) if suite_match else "unknown"
        )

        # Skip if suite is "test-suite" (framework test placeholder)
        if suite == "test-suite":
            continue

        # Look for log file path
        log_match = re.search(r"Log:\s+([^\s\n]+)", full_context)
        log_path = log_match.group(1) if log_match else None

        test_info = {
            "directory": directory,
            "suite": suite,
            "test_id": test_id,
            "error_detail": error_detail,
            "log_path": log_path,
        }

        if result_type == "FAILED":
            failed_tests.append(test_info)
        else:  # XFAILED
            xfailed_tests.append(test_info)

    # Pattern 2: "INFO: ✗ FAILED: test_name" or "INFO: ✓ XFAILED: test_name"
    failed_pattern2 = r"INFO:\s+[✗✓]\s+(FAILED|XFAILED):\s+(\S+)"
    for match in re.finditer(failed_pattern2, content):
        result_type = match.group(1)
        test_id = match.group(2)

        # Find context
        start_pos = max(0, match.start() - 2000)
        context = content[start_pos : match.start()]

        # Find directory - look for "Making check in" patterns
        dir_match = re.search(r"Making check in\s+([^\s\n]+)", context)
        directory = dir_match.group(1) if dir_match else None
        if not directory:
            # Look for test directory patterns with optional subdirectory
            dir_match = re.search(r"(tests/[^\s\n/]+(?:/[^\s\n/]+)?)", context)
            if dir_match:
                directory = dir_match.group(1)
            else:
                directory = "unknown"

        # Find suite
        suite_match = re.search(
            r"Running testsuite\s+(\S+)|Summary for suite\s+(\S+):", context
        )
        suite = (
            (suite_match.group(1) or suite_match.group(2)) if suite_match else "unknown"
        )

        # Look for error detail after the match
        end_pos = min(len(content), match.end() + 500)
        after_context = content[match.end() : end_pos]
        error_match = re.search(r"-\s*(.+?)(?=\n|$)", after_context, re.DOTALL)
        error_detail = error_match.group(1).strip() if error_match else None

        # Look for log path
        log_match = re.search(r"Log:\s+([^\s\n]+)", context + after_context)
        log_path = log_match.group(1) if log_match else None

        test_info = {
            "directory": directory,
            "suite": suite,
            "test_id": test_id,
            "error_detail": error_detail,
            "log_path": log_path,
        }

        if result_type == "FAILED":
            failed_tests.append(test_info)
        else:  # XFAILED
            xfailed_tests.append(test_info)

    # Pattern 3: Look for test names in error messages
    # "ERROR: Test 'test_name' failed"
    error_pattern = r"ERROR:\s+Test\s+'([^']+)'\s+failed"
    for match in re.finditer(error_pattern, content):
        test_id = match.group(1)

        # Skip generic placeholder names that are likely from framework tests
        if test_id in ("test_name", "test1", "test", "example_test"):
            # Check context to see if this is a real test or framework test
            start_pos = max(0, match.start() - 500)
            end_pos = min(len(content), match.end() + 200)
            context_before = content[start_pos : match.start()]
            context_after = content[match.end() : end_pos]

            # Skip if this appears in a test framework test (has "ok" result after it)
            if re.search(r"ok\s*$", context_after[:100], re.MULTILINE):
                # This is a passing framework test that's testing error output, skip it
                continue

            # Skip if it's in a debug output manager test
            if re.search(
                r"test_show_failure_debug_info|TestDebugOutputManager", context_before
            ):
                continue

        # Find context
        start_pos = max(0, match.start() - 2000)
        context = content[start_pos : match.start()]

        # Find directory - look for "Making check in" patterns
        dir_match = re.search(r"Making check in\s+([^\s\n]+)", context)
        directory = dir_match.group(1) if dir_match else None
        if not directory:
            # Look for test directory patterns with optional subdirectory
            dir_match = re.search(r"(tests/[^\s\n/]+(?:/[^\s\n/]+)?)", context)
            if dir_match:
                directory = dir_match.group(1)
            else:
                directory = "unknown"

        # Find suite
        suite_match = re.search(
            r"Running testsuite\s+(\S+)|Summary for suite\s+(\S+):", context
        )
        suite = (
            (suite_match.group(1) or suite_match.group(2)) if suite_match else "unknown"
        )

        # Look for more error detail
        end_pos = min(len(content), match.end() + 500)
        after_context = content[match.end() : end_pos]
        error_detail = None
        detail_match = re.search(
            r"failed\s+(.+?)(?=\n\n|\n[A-Z]|$)", after_context, re.DOTALL
        )
        if detail_match:
            error_detail = detail_match.group(1).strip()

        # Look for log path
        log_match = re.search(r"Log:\s+([^\s\n]+)", context + after_context)
        log_path = log_match.group(1) if log_match else None

        failed_tests.append(
            {
                "directory": directory,
                "suite": suite,
                "test_id": test_id,
                "error_detail": error_detail,
                "log_path": log_path,
            }
        )

    # Now, add entries for suites with xfailed tests that we didn't find individual test names for
    # This handles cases where individual test names aren't in the log (non-verbose mode)
    suite_xfailed_pattern = (
        r"Summary for suite (\S+):\s+"
        r"Passed: \d+\s+"
        r"Failed: \d+\s+"
        r"Skipped: \d+\s+"
        r"Xfailed: (\d+)"
    )
    for match in re.finditer(suite_xfailed_pattern, content):
        suite = match.group(1)
        xfailed_count = int(match.group(2))
        if xfailed_count > 0:
            # Find directory for this suite - look backwards for "Making check in"
            start_pos = max(0, match.start() - 5000)
            context_before = content[start_pos : match.start()]

            # Find the most recent "Making check in" before this suite
            dir_matches = list(
                re.finditer(r"Making check in\s+([^\s\n]+)", context_before)
            )
            if dir_matches:
                directory = dir_matches[-1].group(1)
            else:
                # Try to find directory from path patterns
                dir_match = re.search(
                    r"(tests/[^\s\n/]+(?:/[^\s\n/]+)?)", context_before
                )
                directory = dir_match.group(1) if dir_match else "unknown"

            # Check if we already have individual test entries for this suite+directory combination
            # (from patterns above that might have found specific test names)
            existing_for_suite_dir = [
                t
                for t in xfailed_tests
                if t.get("suite") == suite
                and t.get("directory") == directory
                and not t.get("test_id", "").startswith("<")
            ]
            # If we don't have individual test names for this directory/suite, create a summary entry
            if len(existing_for_suite_dir) < xfailed_count:
                # Check if we already have a summary entry for this directory/suite
                has_summary_entry = any(
                    t.get("suite") == suite
                    and t.get("directory") == directory
                    and t.get("test_id", "").startswith("<")
                    for t in xfailed_tests
                )
                if not has_summary_entry:
                    # Create one entry per directory/suite combination
                    xfailed_tests.append(
                        {
                            "directory": directory,
                            "suite": suite,
                            "test_id": f"<{xfailed_count} xfailed test{'s' if xfailed_count > 1 else ''} - individual names not in log>",
                            "error_detail": f"Suite has {xfailed_count} expected failure(s). Individual test names are not available in the log output.",
                            "log_path": None,
                        }
                    )

    # Remove duplicates based on (suite, test_id) while preserving order
    seen_failed = set()
    unique_failed = []
    for test_info in failed_tests:
        key = (test_info["suite"], test_info["test_id"])
        if key not in seen_failed:
            seen_failed.add(key)
            unique_failed.append(test_info)

    seen_xfailed = set()
    unique_xfailed = []
    for test_info in xfailed_tests:
        # For entries without individual test names, include directory in the key to avoid deduplication
        # across different directories
        if test_info["test_id"].startswith("<"):
            key = (test_info["directory"], test_info["suite"], test_info["test_id"])
        else:
            key = (test_info["suite"], test_info["test_id"])
        if key not in seen_xfailed:
            seen_xfailed.add(key)
            unique_xfailed.append(test_info)

    return unique_failed, unique_xfailed


def parse_log_content(content: str, log_source: str = "stdin") -> TestSummary:
    """Parse log content and extract test results."""
    summary = TestSummary()
    summary.log_file = log_source
    summary.line_count = len(content.splitlines())

    summary.unit_tests = parse_unit_tests(content)
    summary.unit_test_failures = parse_unit_test_failures(content)

    # If we found unit test failures but no details, try to read src/test-suite.log
    if summary.unit_test_failures and log_source != "stdin":
        for failure in summary.unit_test_failures:
            if not failure.get('error_detail'):
                # Try to read from src/test-suite.log
                log_dir = Path(log_source).parent if log_source else Path(".")
                test_suite_log = log_dir / "src" / "test-suite.log"
                if test_suite_log.exists():
                    try:
                        with open(test_suite_log, 'r') as f:
                            test_content = f.read()
                        # Re-parse with the detailed content
                        detailed_failures = parse_unit_test_failures(test_content)
                        # Update failures with details from test-suite.log
                        for detailed in detailed_failures:
                            for f in summary.unit_test_failures:
                                if f['test_name'] == detailed['test_name'] and detailed.get('error_detail'):
                                    f['error_detail'] = detailed['error_detail']
                                    f['leak_summary'] = detailed.get('leak_summary')
                        break  # Only need to read once
                    except:
                        pass  # If we can't read it, just continue

    summary.library_tests = parse_library_tests(content)
    summary.sparql_suites = parse_sparql_suites(content)
    summary.compare_tests = parse_compare_tests(content)
    summary.failed_tests, summary.xfailed_tests = parse_failed_tests(content)

    return summary


def parse_log_file(log_file: Path) -> TestSummary:
    """Parse a make check log file and extract test results."""
    try:
        with log_file.open("r", encoding="utf-8", errors="replace") as f:
            content = f.read()
    except FileNotFoundError:
        sys.exit(
            f"{os.path.basename(sys.argv[0])}: Cannot read {log_file} - "
            f"No such file or directory"
        )
    except Exception as e:
        sys.exit(f"{os.path.basename(sys.argv[0])}: Error reading {log_file}: {e}")

    return parse_log_content(content, str(log_file))


def format_section_header(title: str, width: int = 60, markdown: bool = False) -> str:
    """Format a section header."""
    if markdown:
        return f"\n### {title}\n" if title else "\n"
    return f"\n{title}\n" if title else "\n"


def format_markdown_table(header: list, rows: list, align: list = None) -> list:
    """Format a markdown table with aligned columns.

    Args:
        header: List of header strings
        rows: List of lists, each containing cell values for a row
        align: List of alignment chars ('l', 'r', 'c') for each column, default is 'l'

    Returns:
        List of strings representing the markdown table
    """
    if not header or not rows:
        return []

    num_cols = len(header)
    if align is None:
        align = ["l"] * num_cols
    elif len(align) < num_cols:
        align.extend(["l"] * (num_cols - len(align)))

    # Convert all values to strings and calculate column widths
    str_rows = [[str(cell) for cell in row] for row in rows]
    str_header = [str(h) for h in header]

    # Calculate max width for each column
    col_widths = []
    for i in range(num_cols):
        max_width = len(str_header[i])
        for row in str_rows:
            if i < len(row):
                max_width = max(max_width, len(row[i]))
        col_widths.append(max_width)

    # Build table
    lines = []

    # Header row
    header_cells = []
    for i, h in enumerate(str_header):
        width = col_widths[i]
        header_cells.append(f" {h:<{width}} ")
    lines.append("|" + "|".join(header_cells) + "|")

    # Separator row
    sep_cells = []
    for i, width in enumerate(col_widths):
        if align[i] == "r":
            sep_cells.append("-" * (width + 2))
        elif align[i] == "c":
            sep_cells.append(":" + "-" * (width) + ":")
        else:  # 'l'
            sep_cells.append("-" * (width + 2))
    lines.append("|" + "|".join(sep_cells) + "|")

    # Data rows
    for row in str_rows:
        cells = []
        for i in range(num_cols):
            width = col_widths[i]
            value = row[i] if i < len(row) else ""
            if align[i] == "r":
                cells.append(f" {value:>{width}} ")
            elif align[i] == "c":
                cells.append(f" {value:^{width}} ")
            else:  # 'l'
                cells.append(f" {value:<{width}} ")
        lines.append("|" + "|".join(cells) + "|")

    return lines


def format_unit_tests(
    unit_tests: Dict[str, int], markdown: bool = False, use_color: bool = False
) -> str:
    """Format unit test results."""
    if not unit_tests:
        return ""

    if markdown:
        header = ["Metric", "Count"]
        rows = [
            ["Total", unit_tests["total"]],
            ["Passed", unit_tests["passed"]],
            ["XFailed", unit_tests["xfailed"]],
            ["Failed", unit_tests["failed"]],
        ]
        lines = ["#### Unit Tests (src/)", ""]
        lines.extend(format_markdown_table(header, rows, ["l", "r"]))
        lines.append("")
    else:
        # Text table with optional colors
        passed_str = f"{unit_tests['passed']:>8}"
        xfailed_str = f"{unit_tests['xfailed']:>8}"
        failed_str = f"{unit_tests['failed']:>8}"

        if use_color:
            if unit_tests["passed"] > 0:
                passed_str = f"{Colors.GREEN}{passed_str}{Colors.RESET}"
            if unit_tests["xfailed"] > 0:
                xfailed_str = f"{Colors.ORANGE}{xfailed_str}{Colors.RESET}"
            if unit_tests["failed"] > 0:
                failed_str = f"{Colors.RED}{failed_str}{Colors.RESET}"

        lines = [
            "UNIT TESTS (src/)",
            "",
            f"{'Metric':<12} {'Count':>8}",
            f"{'-' * 12} {'-' * 8}",
            f"{'Total':<12} {unit_tests['total']:>8}",
            f"{'Passed':<12} {passed_str}",
            f"{'XFailed':<12} {xfailed_str}",
            f"{'Failed':<12} {failed_str}",
            "",
        ]
    return "\n".join(lines)


def format_unit_test_failures(
    unit_test_failures: list, markdown: bool = False, use_color: bool = False
) -> str:
    """Format detailed unit test failure information."""
    if not unit_test_failures:
        return ""

    if markdown:
        header = ["Test Name", "Error Summary"]
        rows = []
        for failure in unit_test_failures:
            test_name = f"`{failure['test_name']}`"
            error = failure.get('error_detail', 'No details available')
            if error and len(error) > 150:
                error = error[:147] + "..."
            error = error.replace("|", "\\|").replace("\n", " ") if error else "No details"
            rows.append([test_name, error])
        lines = ["#### Unit Test Failures (Details)", ""]
        lines.extend(format_markdown_table(header, rows, ["l", "l"]))
        lines.append("")
    else:
        header = "UNIT TEST FAILURES (Details):"
        if use_color:
            header = f"{Colors.RED}{Colors.BOLD}{header}{Colors.RESET}"
        lines = [header, ""]
        for i, failure in enumerate(unit_test_failures, 1):
            test_name = failure['test_name']
            if use_color:
                test_name = f"{Colors.RED}{test_name}{Colors.RESET}"
            lines.append(f"  {i}. {test_name}")
            if failure.get('error_detail'):
                error = failure['error_detail']
                # Wrap long error messages
                if len(error) > 100:
                    lines.append(f"     Error: {error[:100]}")
                    lines.append(f"            {error[100:]}")
                else:
                    lines.append(f"     Error: {error}")
            else:
                lines.append(f"     Error: No details available - check src/test-suite.log")
            lines.append("")

    return "\n".join(lines)


def format_library_tests(
    library_tests: Dict[str, Dict[str, int]], markdown: bool = False
) -> str:
    """Format library test results."""
    if not library_tests:
        return ""

    if markdown:
        lib_names = sorted(library_tests.keys())
        header = ["Library", "Passed", "Total"]
        rows = [
            [
                lib_name,
                library_tests[lib_name]["passed"],
                library_tests[lib_name]["total"],
            ]
            for lib_name in lib_names
        ]
        lines = ["#### Library Tests", ""]
        lines.extend(format_markdown_table(header, rows, ["l", "r", "r"]))
        lines.append("")
    else:
        lines = [
            "LIBRARY TESTS:",
            "",
            f"{'Library':<15} {'Passed':>8} {'Total':>8}",
            f"{'-' * 15} {'-' * 8} {'-' * 8}",
        ]
        for lib_name in sorted(library_tests.keys()):
            stats = library_tests[lib_name]
            lines.append(f"{lib_name:<15} {stats['passed']:>8} {stats['total']:>8}")
        lines.append("")
    return "\n".join(lines)


def format_sparql_suites(
    sparql_suites: Dict[str, Dict[str, int]],
    markdown: bool = False,
    use_color: bool = False,
) -> Tuple[str, Dict[str, int]]:
    """Format SPARQL test suite results and return totals."""
    if not sparql_suites:
        return "", {"passed": 0, "failed": 0, "xfailed": 0, "skipped": 0}

    totals = {"passed": 0, "failed": 0, "xfailed": 0, "skipped": 0}

    if markdown:
        suite_names = sorted(sparql_suites.keys())
        header = ["Suite", "Passed", "Failed", "XFailed", "Skipped", "Total"]
        rows = []
        for suite in suite_names:
            stats = sparql_suites[suite]
            total = (
                stats["passed"] + stats["failed"] + stats["skipped"] + stats["xfailed"]
            )
            if total > 0:
                rows.append(
                    [
                        suite,
                        stats["passed"],
                        stats["failed"],
                        stats["xfailed"],
                        stats["skipped"],
                        total,
                    ]
                )
                totals["passed"] += stats["passed"]
                totals["failed"] += stats["failed"]
                totals["xfailed"] += stats["xfailed"]
                totals["skipped"] += stats["skipped"]

        lines = ["#### SPARQL Test Suites", ""]
        lines.extend(
            format_markdown_table(header, rows, ["l", "r", "r", "r", "r", "r"])
        )

        total_tests = totals["passed"] + totals["failed"] + totals["xfailed"]
        header = ["Metric", "Count"]
        rows = [
            ["Total Passed", totals["passed"]],
            ["Total Failed", totals["failed"]],
            ["Total XFailed", totals["xfailed"]],
            ["Total SPARQL Tests", total_tests],
        ]
        lines.extend(["", "#### SPARQL Suites Totals", ""])
        lines.extend(format_markdown_table(header, rows, ["l", "r"]))
        lines.append("")
    else:
        lines = [
            "SPARQL TEST SUITES:",
            "",
            f"{'Suite':<25} {'Passed':>8} {'Failed':>8} {'XFailed':>8} {'Skipped':>8} {'Total':>8}",
            f"{'-' * 25} {'-' * 8} {'-' * 8} {'-' * 8} {'-' * 8} {'-' * 8}",
        ]
        for suite in sorted(sparql_suites.keys()):
            stats = sparql_suites[suite]
            total = (
                stats["passed"] + stats["failed"] + stats["skipped"] + stats["xfailed"]
            )
            if total > 0:
                passed_str = f"{stats['passed']:>8}"
                failed_str = f"{stats['failed']:>8}"
                xfailed_str = f"{stats['xfailed']:>8}"

                if use_color:
                    if stats["passed"] > 0:
                        passed_str = f"{Colors.GREEN}{passed_str}{Colors.RESET}"
                    if stats["failed"] > 0:
                        failed_str = f"{Colors.RED}{failed_str}{Colors.RESET}"
                    if stats["xfailed"] > 0:
                        xfailed_str = f"{Colors.ORANGE}{xfailed_str}{Colors.RESET}"

                lines.append(
                    f"{suite:<25} {passed_str} {failed_str} "
                    f"{xfailed_str} {stats['skipped']:>8} {total:>8}"
                )
                totals["passed"] += stats["passed"]
                totals["failed"] += stats["failed"]
                totals["xfailed"] += stats["xfailed"]
                totals["skipped"] += stats["skipped"]

        passed_total_str = f"{totals['passed']:>8}"
        failed_total_str = f"{totals['failed']:>8}"
        xfailed_total_str = f"{totals['xfailed']:>8}"

        if use_color:
            if totals["passed"] > 0:
                passed_total_str = f"{Colors.GREEN}{passed_total_str}{Colors.RESET}"
            if totals["failed"] > 0:
                failed_total_str = f"{Colors.RED}{failed_total_str}{Colors.RESET}"
            if totals["xfailed"] > 0:
                xfailed_total_str = f"{Colors.ORANGE}{xfailed_total_str}{Colors.RESET}"

        lines.extend(
            [
                "",
                "SPARQL SUITES TOTALS:",
                "",
                f"{'Metric':<20} {'Count':>8}",
                f"{'-' * 20} {'-' * 8}",
                f"{'Total Passed':<20} {passed_total_str}",
                f"{'Total Failed':<20} {failed_total_str}",
                f"{'Total XFailed':<20} {xfailed_total_str}",
            ]
        )
        total_tests = totals["passed"] + totals["failed"] + totals["xfailed"]
        lines.append(f"{'Total SPARQL Tests':<20} {total_tests:>8}")
        lines.append("")

    return "\n".join(lines), totals


def format_compare_tests(compare_tests: Dict[str, int], markdown: bool = False) -> str:
    """Format compare test results."""
    if not compare_tests:
        return ""

    if markdown:
        header = ["Metric", "Count"]
        rows = [["Total", compare_tests["total"]], ["Passed", compare_tests["passed"]]]
        lines = ["#### Compare Tests", ""]
        lines.extend(format_markdown_table(header, rows, ["l", "r"]))
        lines.append("")
    else:
        lines = [
            "COMPARE TESTS:",
            "",
            f"{'Metric':<12} {'Count':>8}",
            f"{'-' * 12} {'-' * 8}",
            f"{'Total':<12} {compare_tests['total']:>8}",
            f"{'Passed':<12} {compare_tests['passed']:>8}",
            "",
        ]
    return "\n".join(lines)


def format_failed_tests_details(
    failed_tests: list, markdown: bool = False, use_color: bool = False
) -> str:
    """Format detailed information about failed tests."""
    if not failed_tests:
        return ""

    if markdown:
        # Directory first, then suite, then test ID
        header = ["Directory", "Suite", "Test ID", "Error Detail", "Log Path"]
        rows = []
        for test in failed_tests:
            error = test.get("error_detail", "")
            # Truncate long error messages for table display
            if len(error) > 100:
                error = error[:97] + "..."
            error = error.replace("|", "\\|").replace("\n", " ")
            log_path = test.get("log_path", "") or ""
            test_id = f"`{test.get('test_id', 'unknown')}`"
            rows.append(
                [
                    test.get("directory", "unknown"),
                    test.get("suite", "unknown"),
                    test_id,
                    error,
                    log_path,
                ]
            )
        lines = ["#### Failed Tests Details", ""]
        lines.extend(format_markdown_table(header, rows, ["l", "l", "l", "l", "l"]))
        lines.append("")
    else:
        lines = ["FAILED TESTS DETAILS:", ""]
        for i, test in enumerate(failed_tests, 1):
            lines.append(f"  {i}. Directory: {test.get('directory', 'unknown')}")
            lines.append(f"     Suite: {test.get('suite', 'unknown')}")
            lines.append(f"     Test ID: {test.get('test_id', 'unknown')}")
            if test.get("error_detail"):
                error = test["error_detail"]
                # Limit error detail length, show first few lines
                error_lines = error.split("\n")[:5]
                error_preview = "\n".join(error_lines)
                if len(error.split("\n")) > 5:
                    error_preview += "\n     ... (truncated)"
                lines.append(f"     Error: {error_preview}")
            if test.get("log_path"):
                lines.append(f"     Log: {test.get('log_path')}")
            lines.append("")

    return "\n".join(lines)


def format_xfailed_tests_details(
    xfailed_tests: list, markdown: bool = False, use_color: bool = False
) -> str:
    """Format detailed information about xfailed (expected to fail) tests."""
    if not xfailed_tests:
        return ""

    if markdown:
        # Directory first, then suite, then test ID
        header = ["Directory", "Suite", "Test ID", "Notes", "Log Path"]
        rows = []
        for test in xfailed_tests:
            notes = test.get("error_detail", "Expected failure") or "Expected failure"
            if len(notes) > 100:
                notes = notes[:97] + "..."
            notes = notes.replace("|", "\\|").replace("\n", " ")
            log_path = test.get("log_path", "") or ""
            test_id = f"`{test.get('test_id', 'unknown')}`"
            rows.append(
                [
                    test.get("directory", "unknown"),
                    test.get("suite", "unknown"),
                    test_id,
                    notes,
                    log_path,
                ]
            )
        lines = ["#### XFailed Tests Details (Expected Failures)", ""]
        lines.extend(format_markdown_table(header, rows, ["l", "l", "l", "l", "l"]))
        lines.append("")
    else:
        header = "XFAILED TESTS DETAILS (Expected Failures):"
        if use_color:
            header = f"{Colors.ORANGE}{Colors.BOLD}{header}{Colors.RESET}"
        lines = [header, ""]
        for i, test in enumerate(xfailed_tests, 1):
            test_id = test.get("test_id", "unknown")
            if use_color:
                test_id = f"{Colors.ORANGE}{test_id}{Colors.RESET}"
            lines.append(f"  {i}. Directory: {test.get('directory', 'unknown')}")
            lines.append(f"     Suite: {test.get('suite', 'unknown')}")
            lines.append(f"     Test ID: {test_id}")
            if test.get("error_detail"):
                notes = test["error_detail"]
                notes_lines = notes.split("\n")[:3]
                notes_preview = "\n".join(notes_lines)
                if len(notes.split("\n")) > 3:
                    notes_preview += "\n     ... (truncated)"
                notes_label = "Notes:"
                if use_color:
                    notes_label = f"{Colors.ORANGE}{notes_label}{Colors.RESET}"
                lines.append(f"     {notes_label} {notes_preview}")
            if test.get("log_path"):
                lines.append(f"     Log: {test.get('log_path')}")
            lines.append("")

    return "\n".join(lines)


def format_overall_summary(
    summary: TestSummary,
    sparql_totals: Dict[str, int],
    markdown: bool = False,
    use_color: bool = False,
) -> str:
    """Format overall summary statistics."""
    # Calculate total tests executed
    total_tests = 0
    if summary.unit_tests:
        total_tests += summary.unit_tests["total"]
    total_tests += (
        sparql_totals["passed"] + sparql_totals["failed"] + sparql_totals["xfailed"]
    )
    if summary.compare_tests:
        total_tests += summary.compare_tests["passed"]

    # Check if all tests are passing
    all_passing = True
    if summary.unit_tests and summary.unit_tests["failed"] > 0:
        all_passing = False
    if sparql_totals["failed"] > 0:
        all_passing = False
    if (
        summary.compare_tests
        and summary.compare_tests["passed"] < summary.compare_tests["total"]
    ):
        all_passing = False

    xfailed_total = sparql_totals["xfailed"]
    if summary.unit_tests and summary.unit_tests["xfailed"] > 0:
        xfailed_total += summary.unit_tests["xfailed"]

    if markdown:
        header = ["Metric", "Value"]
        rows = [
            ["Total Tests Executed", total_tests],
            ["All Tests Passing", "✅ YES" if all_passing else "❌ NO"],
            ["Known Issues (XFailed)", xfailed_total],
        ]
        lines = ["#### Overall Summary", ""]
        lines.extend(format_markdown_table(header, rows, ["l", "r"]))
        lines.append("")
    else:
        passing_str = "YES" if all_passing else "NO"
        xfailed_str = f"{xfailed_total:>15}"

        if use_color:
            if all_passing:
                passing_str = f"{Colors.GREEN}{Colors.BOLD}{passing_str}{Colors.RESET}"
            else:
                passing_str = f"{Colors.RED}{Colors.BOLD}{passing_str}{Colors.RESET}"
            if xfailed_total > 0:
                xfailed_str = f"{Colors.ORANGE}{xfailed_total:>15}{Colors.RESET}"

        lines = [
            "OVERALL SUMMARY:",
            "",
            f"{'Metric':<25} {'Value':>15}",
            f"{'-' * 25} {'-' * 15}",
            f"{'Total Tests Executed':<25} {total_tests:>15}",
            f"{'All Tests Passing':<25} {passing_str}",
            f"{'Known Issues (XFailed)':<25} {xfailed_str}",
        ]
    return "\n".join(lines)


def generate_summary(
    summary: TestSummary,
    output_file: Optional[Path] = None,
    markdown: bool = False,
    use_color: bool = False,
) -> str:
    """Generate formatted summary text."""
    if markdown:
        output_lines = [
            "# Test Suite Summary",
        ]
    else:
        output_lines = [
            "TEST SUITE SUMMARY",
        ]

    # Unit tests
    if summary.unit_tests:
        output_lines.append(format_section_header("", markdown=markdown))
        output_lines.append(
            format_unit_tests(
                summary.unit_tests, markdown=markdown, use_color=use_color
            )
        )

    # Unit test failure details
    if summary.unit_test_failures:
        output_lines.append(format_section_header("", markdown=markdown))
        output_lines.append(
            format_unit_test_failures(
                summary.unit_test_failures, markdown=markdown, use_color=use_color
            )
        )

    # Library tests
    if summary.library_tests:
        output_lines.append(format_section_header("", markdown=markdown))
        output_lines.append(
            format_library_tests(summary.library_tests, markdown=markdown)
        )

    # SPARQL suites
    if summary.sparql_suites:
        output_lines.append(format_section_header("", markdown=markdown))
        sparql_text, sparql_totals = format_sparql_suites(
            summary.sparql_suites, markdown=markdown, use_color=use_color
        )
        output_lines.append(sparql_text)
    else:
        sparql_totals = {"passed": 0, "failed": 0, "xfailed": 0, "skipped": 0}

    # Compare tests
    if summary.compare_tests:
        output_lines.append(format_section_header("", markdown=markdown))
        output_lines.append(
            format_compare_tests(summary.compare_tests, markdown=markdown)
        )

    # Failed tests details (if any)
    if summary.failed_tests:
        output_lines.append(format_section_header("", markdown=markdown))
        output_lines.append(
            format_failed_tests_details(
                summary.failed_tests, markdown=markdown, use_color=use_color
            )
        )

    # XFailed tests details (if any)
    if summary.xfailed_tests:
        output_lines.append(format_section_header("", markdown=markdown))
        output_lines.append(
            format_xfailed_tests_details(
                summary.xfailed_tests, markdown=markdown, use_color=use_color
            )
        )

    # Overall summary
    output_lines.append(format_section_header("", markdown=markdown))
    output_lines.append(
        format_overall_summary(
            summary, sparql_totals, markdown=markdown, use_color=use_color
        )
    )

    result = "\n".join(output_lines)

    if output_file:
        try:
            with output_file.open("w", encoding="utf-8") as f:
                f.write(result)
        except Exception as e:
            sys.exit(
                f"{os.path.basename(sys.argv[0])}: Error writing " f"{output_file}: {e}"
            )

    return result


def main():
    parser = argparse.ArgumentParser(
        description="Summarize test results from 'make check' output log",
        epilog="Parse a make check log file and generate a comprehensive "
        "summary of test results including unit tests, library tests, "
        "SPARQL test suites, and compare tests. If LOG-FILE is not "
        "provided, reads from stdin (useful for piping).",
    )
    parser.add_argument(
        "log_file",
        metavar="LOG-FILE",
        nargs="?",
        type=Path,
        help="The make check output log file to parse (default: read from stdin)",
    )
    parser.add_argument(
        "-o",
        "--output",
        metavar="OUTPUT-FILE",
        type=Path,
        help="Write summary to OUTPUT-FILE instead of stdout",
    )
    parser.add_argument(
        "-m",
        "--markdown",
        action="store_true",
        help="Generate output in Markdown format with tables",
    )

    args = parser.parse_args()

    # Read from stdin if no log file provided
    if args.log_file:
        summary = parse_log_file(args.log_file)
        log_source = str(args.log_file)
    else:
        # Read from stdin
        try:
            content = sys.stdin.read()
            summary = parse_log_content(content, "stdin")
            log_source = "stdin"
        except Exception as e:
            sys.exit(f"{os.path.basename(sys.argv[0])}: Error reading from stdin: {e}")

    if not summary.has_data():
        print(
            f"{os.path.basename(sys.argv[0])}: Warning: No test data found "
            f"in {log_source}",
            file=sys.stderr,
        )
        sys.exit(1)

    # Determine if we should use colors (only for terminal output, not markdown, not when writing to file)
    use_color = False
    if not args.markdown and not args.output and sys.stdout.isatty():
        use_color = True
    elif args.output and not args.markdown:
        # Check if output file is a terminal (e.g., /dev/tty)
        try:
            if args.output.exists() and args.output.is_char_device():
                use_color = True
        except:
            pass

    result = generate_summary(
        summary, args.output, markdown=args.markdown, use_color=use_color
    )

    if not args.output:
        print(result)

    # Determine exit code: 0 if all tests pass, 1 otherwise
    # Use the official summary statistics rather than detailed parsing
    # (detailed parsing may pick up test examples or framework output)
    all_passing = True

    # Check unit tests
    if summary.unit_tests and summary.unit_tests["failed"] > 0:
        all_passing = False

    # Check SPARQL suites - use the aggregated statistics
    total_sparql_failed = sum(s["failed"] for s in summary.sparql_suites.values())
    if total_sparql_failed > 0:
        all_passing = False

    # Check compare tests
    if summary.compare_tests:
        if summary.compare_tests["passed"] < summary.compare_tests["total"]:
            all_passing = False

    # Only check detailed failed_tests list if summary stats show failures
    # This avoids false positives from test examples in the log
    if total_sparql_failed > 0 or (
        summary.unit_tests and summary.unit_tests["failed"] > 0
    ):
        # If summary shows failures, verify with detailed list
        if len(summary.failed_tests) > 0:
            all_passing = False

    sys.exit(0 if all_passing else 1)


if __name__ == "__main__":
    main()

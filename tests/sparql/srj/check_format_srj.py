#!/usr/bin/env python3
"""
check_srj_writer.py - Test SRJ (SPARQL Results JSON) writer functionality

Tests the SRJ writer by running queries with -r srj output format
and comparing the generated JSON against expected results.

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

import os
import sys
import json
from pathlib import Path
from typing import Dict, Any, List, Optional, Callable

# Add parent directory to path for imports
sys.path.append(str(Path(__file__).parent.parent.parent))
sys.path.append(str(Path(__file__).parent.parent))
from rasqal_test_util import run_roqet_with_format, filter_format_output
from format_test_base import FormatTestRunner


class SRJTestRunner(FormatTestRunner):
    """SRJ (SPARQL Results JSON) format test runner."""

    def __init__(self):
        super().__init__("SRJ", "manifest-writer.ttl")

    def normalize_json(self, json_str: str) -> Dict[str, Any]:
        """Parse and normalize JSON for comparison"""
        try:
            return json.loads(json_str)
        except json.JSONDecodeError as e:
            print(f"JSON parse error: {e}")
            return {}

    def run_roqet_srj(
        self, query_file: str, data_file: str, roqet_path: str, timeout: int = 30
    ) -> Optional[str]:
        """Run roqet with SRJ output format"""
        stdout, stderr, returncode = run_roqet_with_format(
            query_file, data_file, "srj", roqet_path, timeout=timeout
        )

        if returncode != 0:
            print(f"roqet failed with return code {returncode}")
            print(f"stderr: {stderr}")
            return None

        # Filter out debug output using the utility function
        filtered_output = filter_format_output(stdout, "srj")
        if filtered_output is None:
            print("No JSON output found")

        return filtered_output

    def compare_srj_output(self, generated: str, expected: str, test_name: str) -> bool:
        """Compare generated SRJ output with expected output"""
        gen_json = self.normalize_json(generated)
        exp_json = self.normalize_json(expected)

        if not gen_json or not exp_json:
            print(f"SRJ test failed: Invalid JSON")
            return False

        # Compare structure
        if gen_json == exp_json:
            return True
        else:
            print(f"SRJ output does not match expected result")
            print(f"Generated: {json.dumps(gen_json, indent=2)}")
            print(f"Expected:  {json.dumps(exp_json, indent=2)}")
            return False

    def run_format_test(self, test_config: Any, srcdir: Path, timeout: int = 30) -> int:
        """Run a format-specific test."""
        return self.run_srj_writer_test(test_config, srcdir, timeout)

    def run_srj_writer_test(self, test_config, srcdir: Path, timeout: int = 30) -> int:
        """Run SRJ writer test from manifest configuration."""
        from rasqal_test_util import find_tool

        query_file = str(test_config.test_file)
        data_file = str(test_config.data_files[0]) if test_config.data_files else None
        # Use extra_files for expected results (following Turtle manifest pattern)
        expected_file = (
            str(test_config.extra_files[0]) if test_config.extra_files else None
        )

        self.debug_print(f"Running SRJ test with query: {query_file}", 1)
        self.debug_print(f"Data file: {data_file}", 2)
        self.debug_print(f"Expected file: {expected_file}", 2)

        # Use find_tool for roqet path
        roqet_path = find_tool("roqet") or "roqet"

        # Build full paths relative to srcdir
        query_path = (
            str(srcdir / query_file) if not os.path.isabs(query_file) else query_file
        )
        data_path = (
            str(srcdir / data_file)
            if data_file and not os.path.isabs(data_file)
            else data_file
        )
        expected_path = (
            str(srcdir / expected_file)
            if expected_file and not os.path.isabs(expected_file)
            else expected_file
        )

        # Check files exist
        for path in [query_path, data_path, expected_path]:
            if path and not os.path.exists(path):
                print(f"SRJ test failed: Missing file {path}")
                return 1

        # Run roqet to generate SRJ output
        generated_output = self.run_roqet_srj(
            query_path, data_path, roqet_path, timeout
        )
        if generated_output is None:
            print(f"SRJ generation failed")
            return 1

        self.debug_print(f"Generated SRJ output length: {len(generated_output)}", 1)
        self.debug_print(f"Generated SRJ content:\n{generated_output}", 2)

        # Validate JSON syntax
        try:
            json.loads(generated_output)
        except json.JSONDecodeError:
            print(f"SRJ test failed: Generated output is not valid JSON")
            print(f"Output: {generated_output}")
            return 1

        # Read expected output
        try:
            with open(expected_path, "r") as f:
                expected_output = f.read()
        except Exception as e:
            print(f"SRJ test failed: Could not read expected file: {e}")
            return 1

        self.debug_print(f"Expected SRJ content:\n{expected_output}", 2)

        # Compare outputs
        success = self.compare_srj_output(
            generated_output, expected_output, test_config.name
        )
        if success:
            if self.debug_level == 0:
                print("SRJ test passed")
            else:
                self.debug_print("SRJ test passed", 1)
            return 0
        else:
            return 1


def main():
    """Run SRJ format tests"""
    runner = SRJTestRunner()

    epilog = """
USAGE EXAMPLES:

  Framework Integration (most common):
    ../../improve.py . format-srj-write  # Run all tests via improve.py framework
    python3 check_format_srj.py --test-case srj-write-basic-bindings --srcdir .
                                         # Run single test for framework

  Development/Debugging (comprehensive):
    python3 check_format_srj.py         # Run full test suite from manifest
    python3 check_format_srj.py -v      # Same with verbose output
    python3 check_format_srj.py --debug 2
                                         # Maximum debug output with JSON content
    python3 check_format_srj.py --list-tests
                                         # List all available test cases

  Individual Test Debugging:
    python3 check_format_srj.py --test-case srj-write-ask-true --srcdir . --debug 2
                                         # Debug single test case with full JSON output
    python3 check_format_srj.py srj-write-basic-bindings srj-write-ask-true
                                         # Run specific tests by name

AVAILABLE TEST CASES:
  (Dynamically loaded from manifest file specified by --manifest-file)

All tests validate both JSON syntax and semantic correctness of SRJ output.
    """

    return runner.main(
        "Test SRJ writer functionality - SPARQL Results JSON format writing", epilog
    )


if __name__ == "__main__":
    sys.exit(main())

"""
SRJ (SPARQL Results JSON) Format Test Runner

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
from typing import Dict, List, Optional, Callable, Any

from ..config import TestConfig

from .format_base import FormatTestRunner
from ..execution import run_roqet_with_format, filter_format_output
from ..utils import find_tool


class SrjTestRunner(FormatTestRunner):
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

    def run_format_test(
        self, test_config: TestConfig, srcdir: Path, timeout: int = 30
    ) -> int:
        """Run a format-specific test."""
        return self.run_srj_writer_test(test_config, srcdir, timeout)

    def run_srj_writer_test(
        self, test_config: TestConfig, srcdir: Path, timeout: int = 30
    ) -> int:
        """Run SRJ writer test from manifest configuration."""
        query_file = test_config.test_file
        data_file = test_config.data_files[0] if test_config.data_files else None
        expected_file = test_config.extra_files[0] if test_config.extra_files else None

        if not query_file:
            print("Missing query file in test configuration")
            return 1

        print(f"Running SRJ test: {test_config.name}")

        # Run roqet with SRJ format
        roqet_path = self.roqet_path or find_tool("roqet")
        if not roqet_path:
            print("roqet not found")
            return 1

        generated_output = self.run_roqet_srj(
            str(query_file), str(data_file) if data_file else None, roqet_path, timeout
        )
        if generated_output is None:
            print("Failed to generate SRJ output")
            return 1

        # Read expected output
        expected_path = srcdir / expected_file if expected_file else None
        if expected_path and expected_path.exists():
            with open(expected_path, "r") as f:
                expected_output = f.read().strip()

            if self.compare_srj_output(
                generated_output, expected_output, test_config.name
            ):
                print("SRJ test passed")
                return 0
            else:
                print("SRJ test failed: output mismatch")
                return 1
        else:
            # No expected file, just verify JSON is valid
            if self.normalize_json(generated_output):
                print("SRJ test passed (no expected file, JSON is valid)")
                return 0
            else:
                print("SRJ test failed: invalid JSON")
                return 1

    def get_legacy_tests(self) -> list:
        """Return list of (test_name, test_function) tuples for legacy mode."""
        return [
            ("SRJ Writer Test", self.test_srj_writer),
        ]

    def test_srj_writer(self) -> bool:
        """Test SRJ writer functionality."""
        print("\n=== SRJ Writer Test ===")

        # Get test data from manifest
        test_data = self.get_manifest_test_data("srj-writer-test")
        if not test_data:
            print("Failed to load SRJ test data from manifest")
            return False

        query_file = test_data.get("query_file")
        data_file = test_data.get("data_file")
        expected_file = test_data.get("expected_file")

        if not query_file:
            print("Missing query file in test data")
            return False

        print("Testing SRJ generation...")
        roqet_path = self.roqet_path or find_tool("roqet")
        if not roqet_path:
            print("roqet not found")
            return False

        generated_output = self.run_roqet_srj(
            query_file, data_file, roqet_path, self.timeout
        )
        if generated_output is None:
            print("SRJ generation failed")
            return False

        print(f"SRJ generation successful ({len(generated_output)} characters)")

        # Test JSON parsing
        json_data = self.normalize_json(generated_output)
        if json_data:
            print("SRJ JSON parsing successful")
            return True
        else:
            print("SRJ JSON parsing failed")
            return False

    def get_manifest_test_data(self, test_name: str) -> dict:
        """Get test data from manifest for a specific test."""
        from ..manifest import ManifestParser

        manifest_path = Path("manifest-writer.ttl")
        if not manifest_path.exists():
            return None

        parser = ManifestParser()
        tests = parser.get_tests(manifest_path)

        for test in tests:
            if test.get("name") == test_name:
                return test

        return None


def main():
    """Main entry point for SRJ format testing."""
    runner = SrjTestRunner()
    description = "Test SRJ (SPARQL Results JSON) format functionality"
    epilog = """
Examples:
  %(prog)s --test-case srj-writer-test --srcdir /path/to/tests
  %(prog)s --list-tests
  %(prog)s --test-dir /path/to/test/directory
    """
    return runner.main(description, epilog)


if __name__ == "__main__":
    sys.exit(main())

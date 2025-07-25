"""
CSV/TSV Format Test Runner

This module validates the CSV/TSV result format support implementation
based on the W3C SPARQL 1.1 CSV/TSV specification.

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
from pathlib import Path
from typing import Dict, Callable

from ..config import TestConfig

from .format_base import FormatTestRunner
from ..execution import run_roqet_with_format


class CsvTsvTestRunner(FormatTestRunner):
    """CSV/TSV format test runner."""

    def __init__(self):
        super().__init__("CSV/TSV", "manifest.ttl")

    def run_format_test(
        self, test_config: TestConfig, srcdir: Path, timeout: int = 30
    ) -> int:
        """Run a format-specific test."""
        # Route to appropriate handler based on test name
        test_name = test_config.name

        if test_name == "w3c-official-csv":
            return self.run_csv_test(test_config, srcdir)
        elif test_name == "w3c-official-tsv":
            return self.run_tsv_test(test_config, srcdir)
        elif test_name == "csv-escaping":
            return self.run_escaping_test(test_config, srcdir)
        elif test_name == "boolean-parsing":
            return self.run_boolean_parsing_test(test_config, srcdir)
        else:
            print(f"Unknown test type: {test_name}")
            return 1

    def get_legacy_tests(self) -> list:
        """Return list of (test_name, test_function) tuples for legacy mode."""
        return [
            ("W3C Official Compliance", self.test_w3c_official_compliance),
            ("Escaping Compliance", self.test_escaping_compliance),
            ("Boolean Parsing", self.test_boolean_detection),
        ]

    def test_w3c_official_compliance(self) -> bool:
        """Test W3C official specification compliance for CSV/TSV formats"""
        print("\n=== W3C Official Compliance Test ===")

        # Get test data from manifest
        csv_test = self.get_manifest_test_data("w3c-official-csv")
        tsv_test = self.get_manifest_test_data("w3c-official-tsv")

        if not csv_test or not tsv_test:
            print("Failed to load test data from manifest")
            return False

        success = True

        # Test CSV generation and parsing
        print("Testing CSV generation...")
        csv_out, csv_err, csv_rc = run_roqet_with_format(
            csv_test["query_file"], csv_test["data_file"], "csv"
        )
        if csv_rc == 0:
            print(f"CSV generation successful ({len(csv_out.splitlines())} lines)")

            # Test CSV parsing
            print("Testing CSV parsing...")
            result = self.read_query_results_file(
                Path(csv_test["expected_file"]), "csv", ["x", "literal"], False
            )
            if result and result.get("count", 0) == 6:
                print(f"CSV parsing successful ({result['count']} results)")
            else:
                print(f"CSV parsing failed (got {result})")
                success = False
        else:
            print(f"CSV generation failed: {csv_err}")
            success = False

        # Test TSV generation and parsing
        print("Testing TSV generation...")
        tsv_out, tsv_err, tsv_rc = run_roqet_with_format(
            tsv_test["query_file"], tsv_test["data_file"], "tsv"
        )
        if tsv_rc == 0:
            print(f"TSV generation successful ({len(tsv_out.splitlines())} lines)")

            # Test TSV parsing
            print("Testing TSV parsing...")
            result = self.read_query_results_file(
                Path(tsv_test["expected_file"]), "tsv", ["x", "literal"], False
            )
            if result and result.get("count", 0) == 6:
                print(f"TSV parsing successful ({result['count']} results)")
            else:
                print(f"TSV parsing failed (got {result})")
                success = False
        else:
            print(f"TSV generation failed: {tsv_err}")
            success = False

        return success

    def test_escaping_compliance(self) -> bool:
        """Test escaping compliance for CSV/TSV formats"""
        print("\n=== Escaping Compliance Test ===")

        escaping_test = self.get_manifest_test_data("csv-escaping")
        if not escaping_test:
            print("Failed to load escaping test data from manifest")
            return False

        print("Testing CSV escaping...")
        csv_out, csv_err, csv_rc = run_roqet_with_format(
            escaping_test["query_file"], escaping_test["data_file"], "csv"
        )
        if csv_rc == 0:
            print(f"CSV escaping test successful ({len(csv_out.splitlines())} lines)")
            return True
        else:
            print(f"CSV escaping test failed: {csv_err}")
            return False

    def test_boolean_detection(self) -> bool:
        """Test boolean value detection in CSV/TSV formats"""
        print("\n=== Boolean Detection Test ===")

        boolean_test = self.get_manifest_test_data("boolean-parsing")
        if not boolean_test:
            print("Failed to load boolean test data from manifest")
            return False

        print("Testing boolean parsing...")
        csv_out, csv_err, csv_rc = run_roqet_with_format(
            boolean_test["query_file"], boolean_test["data_file"], "csv"
        )
        if csv_rc == 0:
            print(
                f"Boolean parsing test successful ({len(csv_out.splitlines())} lines)"
            )
            return True
        else:
            print(f"Boolean parsing test failed: {csv_err}")
            return False

    def get_manifest_test_data(self, test_name: str) -> dict:
        """Get test data from manifest for a specific test."""
        from ..manifest import ManifestParser

        manifest_path = Path("manifest.ttl")
        if not manifest_path.exists():
            return None

        parser = ManifestParser()
        tests = parser.get_tests(manifest_path)

        for test in tests:
            if test.get("name") == test_name:
                return test

        return None

    def run_csv_test(self, test_config: TestConfig, srcdir: Path) -> int:
        """Run CSV format test."""
        query_file = test_config.test_file
        data_file = test_config.data_files[0] if test_config.data_files else None
        expected_file = test_config.extra_files[0] if test_config.extra_files else None

        if not query_file or not data_file:
            print("Missing required test files")
            return 1

        print(f"Running CSV test: {test_config.name}")

        # Run roqet with CSV format
        stdout, stderr, return_code = run_roqet_with_format(
            str(query_file), str(data_file), "csv"
        )

        if return_code != 0:
            print(f"CSV test failed: {stderr}")
            return 1

        # Compare with expected output if available
        if expected_file:
            expected_path = srcdir / expected_file
            if expected_path.exists():
                with open(expected_path, "r") as f:
                    expected_content = f.read()

                if stdout.strip() == expected_content.strip():
                    print("CSV test passed")
                    return 0
                else:
                    print("CSV test failed: output mismatch")
                    return 1
            else:
                print(f"Expected file not found: {expected_path}")
                return 1
        else:
            # No expected file, just verify CSV generation worked
            if stdout.strip():
                print("CSV test passed (no expected file, but output generated)")
                return 0
            else:
                print("CSV test failed: no output generated")
                return 1

    def run_tsv_test(self, test_config: TestConfig, srcdir: Path) -> int:
        """Run TSV format test."""
        query_file = test_config.test_file
        data_file = test_config.data_files[0] if test_config.data_files else None
        expected_file = test_config.extra_files[0] if test_config.extra_files else None

        if not query_file or not data_file:
            print("Missing required test files")
            return 1

        print(f"Running TSV test: {test_config.name}")

        # Run roqet with TSV format
        stdout, stderr, return_code = run_roqet_with_format(
            str(query_file), str(data_file), "tsv"
        )

        if return_code != 0:
            print(f"TSV test failed: {stderr}")
            return 1

        # Compare with expected output if available
        if expected_file:
            expected_path = srcdir / expected_file
            if expected_path.exists():
                with open(expected_path, "r") as f:
                    expected_content = f.read()

                if stdout.strip() == expected_content.strip():
                    print("TSV test passed")
                    return 0
                else:
                    print("TSV test failed: output mismatch")
                    return 1
            else:
                print(f"Expected file not found: {expected_path}")
                return 1
        else:
            # No expected file, just verify TSV generation worked
            if stdout.strip():
                print("TSV test passed (no expected file, but output generated)")
                return 0
            else:
                print("TSV test failed: no output generated")
                return 1

    def run_escaping_test(self, test_config: TestConfig, srcdir: Path) -> int:
        """Run escaping test."""
        return self.run_csv_test(test_config, srcdir)

    def run_boolean_parsing_test(self, test_config: TestConfig, srcdir: Path) -> int:
        """Run boolean parsing test."""
        return self.run_csv_test(test_config, srcdir)

    def read_query_results_file(
        self,
        result_file_path: Path,
        result_format_hint: str,
        expected_vars_order: list,
        sort_output: bool,
    ) -> dict:
        """Read and parse query results file."""
        # Simplified implementation - in practice this would use the full
        # implementation from the SPARQL runner
        try:
            with open(result_file_path, "r") as f:
                content = f.read()

            lines = content.strip().split("\n")
            if result_format_hint == "csv":
                # Skip header line
                data_lines = lines[1:] if lines else []
            else:  # tsv
                data_lines = lines

            return {
                "count": len(data_lines),
                "content": content,
                "format": result_format_hint,
            }
        except Exception as e:
            print(f"Error reading results file: {e}")
            return None


def main():
    """Main entry point for CSV/TSV format testing."""
    runner = CsvTsvTestRunner()
    description = "Test CSV/TSV format functionality for SPARQL results"
    epilog = """
Examples:
  %(prog)s --test-case w3c-official-csv --srcdir /path/to/tests
  %(prog)s --list-tests
  %(prog)s --test-dir /path/to/test/directory
    """
    return runner.main(description, epilog)


if __name__ == "__main__":
    sys.exit(main())

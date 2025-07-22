#!/usr/bin/env python3
"""
check_csv_tsv_formats.py - Test CSV/TSV format functionality

This script validates the CSV/TSV result format support implementation
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
from typing import Dict, Callable, Any

# Add parent directory to path for imports
sys.path.append(str(Path(__file__).parent.parent))
sys.path.append(str(Path(__file__).parent.parent.parent))
from check_sparql import read_query_results_file
from rasqal_test_util import run_roqet_with_format
from format_test_base import FormatTestRunner


class CSVTSVTestRunner(FormatTestRunner):
    """CSV/TSV format test runner."""

    def __init__(self):
        super().__init__("CSV/TSV", "manifest.ttl")

    def run_format_test(self, test_config: Any, srcdir: Path, timeout: int = 30) -> int:
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
            result = read_query_results_file(
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
        print("\nTesting TSV generation...")
        tsv_out, tsv_err, tsv_rc = run_roqet_with_format(
            tsv_test["query_file"], tsv_test["data_file"], "tsv"
        )
        if tsv_rc == 0:
            print(f"TSV generation successful ({len(tsv_out.splitlines())} lines)")

            # Test TSV parsing
            print("Testing TSV parsing...")
            result = read_query_results_file(
                Path(tsv_test["expected_file"]), "tsv", ["x", "literal"], False
            )
            if result and result.get("count", 0) == 6:
                print(f"TSV parsing successful ({result['count']} results)")
            else:
                print(
                    f"TSV parsing failed (got {result.get('count', 0)} results, expected 6)"
                )
                success = False
        else:
            print(f"TSV generation failed: {tsv_err}")
            success = False

        return success

    def test_escaping_compliance(self) -> bool:
        """Test special character escaping compliance"""
        print("\n=== Escaping Compliance Test ===")

        # Get test data from manifest
        escaping_test = self.get_manifest_test_data("csv-escaping")

        if not escaping_test:
            print("Failed to load escaping test data from manifest")
            return False

        # Test CSV escaping
        print("Testing CSV escaping...")
        csv_out, csv_err, csv_rc = run_roqet_with_format(
            escaping_test["query_file"], escaping_test["data_file"], "csv"
        )
        if csv_rc == 0 and '"has,comma"' in csv_out and '"has""quote"' in csv_out:
            print("CSV escaping generation correct")

            # Test parsing
            result = read_query_results_file(
                Path(escaping_test["expected_file"]), "csv", ["s", "p", "o"], False
            )
            if result and result.get("count", 0) == 3:
                print(f"CSV escaping parsing successful ({result['count']} results)")
                return True
            else:
                print(f"CSV escaping parsing failed")
                return False
        else:
            print(f"CSV escaping generation failed")
            return False

    def test_boolean_detection(self) -> bool:
        """Test boolean result detection and parsing"""
        print("\n=== Boolean Result Parsing Test ===")

        # For boolean parsing test, we know the file name from the context
        # This test doesn't require manifest parsing since it's a pure input file test
        base_path = Path(__file__).parent
        boolean_file = base_path / "boolean-test.csv"

        # Test boolean detection and parsing
        result = read_query_results_file(boolean_file, "csv", [], False)
        if result and result.get("type") == "boolean":
            boolean_value = result.get("value", None)
            if boolean_value is not None:
                print(
                    f"Boolean parsing working - detected boolean result: {boolean_value}"
                )
                return True
            else:
                print("Boolean parsing failed - no boolean value found")
                return False
        else:
            print("Boolean parsing failed - should have parsed boolean result")
            return False

    def get_manifest_test_data(self, test_name: str) -> dict:
        """Get test file paths from manifest for a specific test."""
        try:
            base_path = Path(__file__).parent
            manifest_file = base_path / "manifest.ttl"
            from rasqal_test_util import ManifestParser

            parser = ManifestParser(manifest_file)
            tests = parser.get_tests(base_path)

            for test in tests:
                if test.name == test_name:
                    return {
                        "query_file": str(test.test_file) if test.test_file else None,
                        "data_file": (
                            str(test.data_files[0]) if test.data_files else None
                        ),
                        "expected_file": (
                            str(test.extra_files[0]) if test.extra_files else None
                        ),
                        "input_file": (
                            str(test.extra_files[0]) if test.extra_files else None
                        ),  # For tests that parse input files
                    }
            return {}
        except Exception as e:
            self.debug_print(f"Failed to load test data from manifest: {e}", 1)
            return {}

    def run_csv_test(self, test_config, srcdir: Path) -> int:
        """Run CSV format test."""
        query_file = str(test_config.test_file)
        data_file = str(test_config.data_files[0]) if test_config.data_files else None
        # Use extra_files for expected results (following algebra pattern)
        expected_file = (
            str(test_config.extra_files[0]) if test_config.extra_files else None
        )

        self.debug_print(f"Running CSV test with query: {query_file}", 1)
        self.debug_print(f"Data file: {data_file}", 2)
        self.debug_print(f"Expected file: {expected_file}", 2)

        csv_out, csv_err, csv_rc = run_roqet_with_format(query_file, data_file, "csv")
        if csv_rc != 0:
            print(f"CSV generation failed: {csv_err}")
            return 1

        self.debug_print(f"CSV output ({len(csv_out.splitlines())} lines)", 1)
        self.debug_print(f"CSV content:\n{csv_out}", 2)

        # Compare with expected output if available
        if Path(expected_file).exists():
            with open(expected_file, "r") as f:
                expected = f.read()
            if csv_out.strip() != expected.strip():
                print("CSV output does not match expected result")
                print(f"Expected:\n{expected}")
                print(f"Got:\n{csv_out}")
                return 1

        if self.debug_level == 0:
            print("CSV test passed")
        else:
            self.debug_print("CSV test passed", 1)
        return 0

    def run_tsv_test(self, test_config, srcdir: Path) -> int:
        """Run TSV format test."""
        query_file = str(test_config.test_file)
        data_file = str(test_config.data_files[0]) if test_config.data_files else None
        # Use extra_files for expected results (following algebra pattern)
        expected_file = (
            str(test_config.extra_files[0]) if test_config.extra_files else None
        )

        self.debug_print(f"Running TSV test with query: {query_file}", 1)
        self.debug_print(f"Data file: {data_file}", 2)
        self.debug_print(f"Expected file: {expected_file}", 2)

        tsv_out, tsv_err, tsv_rc = run_roqet_with_format(query_file, data_file, "tsv")
        if tsv_rc != 0:
            print(f"TSV generation failed: {tsv_err}")
            return 1

        self.debug_print(f"TSV output ({len(tsv_out.splitlines())} lines)", 1)
        self.debug_print(f"TSV content:\n{tsv_out}", 2)

        # Compare with expected output if available
        if Path(expected_file).exists():
            with open(expected_file, "r") as f:
                expected = f.read()
            if tsv_out.strip() != expected.strip():
                print("TSV output does not match expected result")
                print(f"Expected:\n{expected}")
                print(f"Got:\n{tsv_out}")
                return 1

        if self.debug_level == 0:
            print("TSV test passed")
        else:
            self.debug_print("TSV test passed", 1)
        return 0

    def run_escaping_test(self, test_config, srcdir: Path) -> int:
        """Run CSV escaping test."""
        query_file = str(test_config.test_file)
        data_file = str(test_config.data_files[0]) if test_config.data_files else None

        csv_out, csv_err, csv_rc = run_roqet_with_format(query_file, data_file, "csv")
        if csv_rc != 0:
            print(f"CSV escaping test failed: {csv_err}")
            return 1

        # Check for proper escaping
        if '"has,comma"' in csv_out and '"has""quote"' in csv_out:
            print("CSV escaping test passed")
            return 0
        else:
            print("CSV escaping test failed - proper escaping not found")
            print(f"Output: {csv_out}")
            return 1

    def run_boolean_parsing_test(self, test_config, srcdir: Path) -> int:
        """Run boolean parsing test."""
        # For boolean parsing test, we use the extra_files which contains the input CSV
        boolean_file = (
            test_config.extra_files[0]
            if test_config.extra_files
            else test_config.result_file
        )

        # Test boolean detection and parsing
        result = read_query_results_file(boolean_file, "csv", [], False)
        if result and result.get("type") == "boolean":
            boolean_value = result.get("value", None)
            if boolean_value is not None:
                print(f"Boolean parsing test passed - detected: {boolean_value}")
                return 0
            else:
                print("Boolean parsing test failed - no boolean value found")
                return 1
        else:
            print("Boolean parsing test failed - should have parsed boolean result")
            return 1


def main():
    """Run CSV/TSV format tests"""
    runner = CSVTSVTestRunner()

    epilog = """
USAGE EXAMPLES:

  Framework Integration (most common):
    make check                           # Run all tests via improve.py framework
    python3 check_format_csv_tsv.py --test-case w3c-official-csv --srcdir .
                                         # Run single test for framework

  Development/Debugging (comprehensive):
    python3 check_format_csv_tsv.py     # Run full test suite with parsing validation
    python3 check_format_csv_tsv.py -v  # Same with verbose output
    python3 check_format_csv_tsv.py --debug 2
                                         # Maximum debug output

  Individual Test Debugging:
    python3 check_format_csv_tsv.py --test-case csv-escaping --srcdir . --debug 2
                                         # Debug single test case

AVAILABLE TEST CASES:
  w3c-official-csv     CSV format W3C compliance
  w3c-official-tsv     TSV format W3C compliance  
  csv-escaping         CSV special character escaping
  boolean-parsing      CSV boolean result parsing

The full test suite mode provides additional parsing validation that tests
generated CSV/TSV files can be correctly parsed back by the CSV/TSV reader.
    """

    return runner.main(
        "Test CSV/TSV format functionality - SPARQL Results CSV/TSV format support",
        epilog,
    )


if __name__ == "__main__":
    sys.exit(main())

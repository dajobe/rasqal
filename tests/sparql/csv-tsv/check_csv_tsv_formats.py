#!/usr/bin/env python3
"""
check_csv_tsv_formats.py - Test CSV/TSV format functionality

This script validates the CSV/TSV result format support implementation
based on the W3C SPARQL 1.1 CSV/TSV specification.

Copyright (C) 2025, David Beckett http://www.dajobe.org/

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
import subprocess
import tempfile
from pathlib import Path
from typing import Tuple, Optional

# Add parent directory to path for imports
sys.path.append(str(Path(__file__).parent.parent))
sys.path.append(str(Path(__file__).parent.parent.parent))
from check_sparql import read_query_results_file
from rasqal_test_util import find_tool


def run_roqet_csv(
    query_file: str, data_file: str, roqet_path: Optional[str] = None
) -> Tuple[str, str, int]:
    """Run roqet with CSV output format"""
    if roqet_path is None:
        roqet_path = find_tool("roqet") or "roqet"
    cmd = [roqet_path, "-r", "csv", "-W", "0", "-q", query_file, "-D", data_file]
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    return result.stdout, result.stderr, result.returncode


def run_roqet_tsv(
    query_file: str, data_file: str, roqet_path: Optional[str] = None
) -> Tuple[str, str, int]:
    """Run roqet with TSV output format"""
    if roqet_path is None:
        roqet_path = find_tool("roqet") or "roqet"
    cmd = [roqet_path, "-r", "tsv", "-W", "0", "-q", query_file, "-D", data_file]
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
    return result.stdout, result.stderr, result.returncode


def test_w3c_official_compliance() -> bool:
    """Test W3C official specification compliance for CSV/TSV formats"""
    print("\n=== W3C Official Compliance Test ===")

    base_path = Path(__file__).parent
    query_file = str(base_path / "queries" / "w3c-official-test.rq")
    data_file = str(base_path / "test-data" / "w3c-test-data.ttl")
    csv_file = str(base_path / "expected-results" / "w3c-official-test.csv")
    tsv_file = str(base_path / "expected-results" / "w3c-official-test.tsv")

    success = True

    # Test CSV generation
    print("Testing CSV generation...")
    csv_out, csv_err, csv_rc = run_roqet_csv(query_file, data_file)
    if csv_rc == 0:
        print(f"CSV generation successful ({len(csv_out.splitlines())} lines)")

        # Test CSV parsing
        print("Testing CSV parsing...")
        result = read_query_results_file(Path(csv_file), "csv", ["x", "literal"], False)
        if result and result.get("count", 0) == 6:
            print(f"CSV parsing successful ({result['count']} results)")
        else:
            print(f"CSV parsing failed (got {result})")
            success = False
    else:
        print(f"CSV generation failed: {csv_err}")
        success = False

    # Test TSV generation
    print("\nTesting TSV generation...")
    tsv_out, tsv_err, tsv_rc = run_roqet_tsv(query_file, data_file)
    if tsv_rc == 0:
        print(f"TSV generation successful ({len(tsv_out.splitlines())} lines)")

        # Test TSV parsing
        print("Testing TSV parsing...")
        result = read_query_results_file(Path(tsv_file), "tsv", ["x", "literal"], False)
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


def test_escaping_compliance() -> bool:
    """Test special character escaping compliance"""
    print("\n=== Escaping Compliance Test ===")

    base_path = Path(__file__).parent
    query_file = str(base_path / "queries" / "test-escaping.rq")
    data_file = str(base_path / "test-data" / "escaping-test-data.ttl")
    csv_file = str(base_path / "expected-results" / "test-escaping.csv")

    # Test CSV escaping
    print("Testing CSV escaping...")
    csv_out, csv_err, csv_rc = run_roqet_csv(query_file, data_file)
    if csv_rc == 0 and '"has,comma"' in csv_out and '"has""quote"' in csv_out:
        print("CSV escaping generation correct")

        # Test parsing
        result = read_query_results_file(Path(csv_file), "csv", ["s", "p", "o"], False)
        if result and result.get("count", 0) == 3:
            print(f"CSV escaping parsing successful ({result['count']} results)")
            return True
        else:
            print(f"CSV escaping parsing failed")
            return False
    else:
        print(f"CSV escaping generation failed")
        return False


def test_boolean_detection() -> bool:
    """Test boolean result detection and parsing"""
    print("\n=== Boolean Result Parsing Test ===")

    base_path = Path(__file__).parent
    boolean_file = base_path / "test-data" / "boolean-test.csv"

    # Test boolean detection and parsing
    result = read_query_results_file(boolean_file, "csv", [], False)
    if result and result.get("type") == "boolean":
        boolean_value = result.get("value", None)
        if boolean_value is not None:
            print(f"Boolean parsing working - detected boolean result: {boolean_value}")
            return True
        else:
            print("Boolean parsing failed - no boolean value found")
            return False
    else:
        print("Boolean parsing failed - should have parsed boolean result")
        return False


def main():
    """Run all CSV/TSV format tests"""
    print("CSV/TSV Format Support Test Suite")
    print("Based on W3C SPARQL 1.1 CSV/TSV specification")
    print("=" * 50)

    # Change to project root directory
    project_root = Path(__file__).parent.parent.parent.parent
    os.chdir(project_root)
    print(f"Working directory: {os.getcwd()}")

    tests = [
        ("W3C Official Compliance", test_w3c_official_compliance),
        ("Escaping Compliance", test_escaping_compliance),
        ("Boolean Parsing", test_boolean_detection),
    ]

    passed = 0
    total = len(tests)

    for test_name, test_func in tests:
        try:
            if test_func():
                passed += 1
                print(f"\n{test_name}: PASSED")
            else:
                print(f"\n{test_name}: FAILED")
        except Exception as e:
            print(f"\n{test_name}: ERROR - {e}")

    print("\n" + "=" * 50)
    print(f"Test Results: {passed}/{total} tests passed")

    if passed == total:
        print("All tests passed.")
        return 0
    else:
        print("Some tests failed. See output above for details.")
        return 1


if __name__ == "__main__":
    sys.exit(main())

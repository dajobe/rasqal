#!/usr/bin/env python3
#
# check_srj_writer.py - Test SRJ (SPARQL Results JSON) writer functionality
#
# Copyright (C) 2025, David Beckett http://www.dajobe.org/
#
# This package is Free Software and part of Redland http://librdf.org/
#
# Tests the SRJ writer by running queries with -r srj output format
# and comparing the generated JSON against expected results.
#

import os
import sys
import json
import subprocess
import tempfile
import argparse
from pathlib import Path
from typing import Dict, Any, List, Optional


def normalize_json(json_str: str) -> Dict[str, Any]:
    """Parse and normalize JSON for comparison"""
    try:
        return json.loads(json_str)
    except json.JSONDecodeError as e:
        print(f"JSON parse error: {e}")
        return {}


def run_roqet_srj(query_file: str, data_file: str, roqet_path: str) -> Optional[str]:
    """Run roqet with SRJ output format"""
    try:
        cmd = [roqet_path, "-r", "srj", "-W", "0", "-q", query_file, "-D", data_file]
        print(f"running command {' '.join(cmd)}")
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)

        if result.returncode != 0:
            print(f"roqet failed with return code {result.returncode}")
            print(f"stderr: {result.stderr}")
            return None

        # Filter out debug output - look for JSON start
        output = result.stdout
        json_start = output.find("{")
        if json_start >= 0:
            return output[json_start:]
        else:
            print("No JSON output found")
            return None
    except subprocess.TimeoutExpired:
        print("roqet timed out")
        return None
    except Exception as e:
        print(f"Error running roqet: {e}")
        return None


def compare_srj_output(generated: str, expected: str, test_name: str) -> bool:
    """Compare generated SRJ output with expected output"""
    gen_json = normalize_json(generated)
    exp_json = normalize_json(expected)

    if not gen_json or not exp_json:
        print(f"FAIL {test_name}: Invalid JSON")
        return False

    # Compare structure
    if gen_json == exp_json:
        print(f"PASS {test_name}: Output matches expected")
        return True
    else:
        print(f"FAIL {test_name}: Output differs from expected")
        print(f"Generated: {json.dumps(gen_json, indent=2)}")
        print(f"Expected:  {json.dumps(exp_json, indent=2)}")
        return False


def validate_json_syntax(json_str: str) -> bool:
    """Validate that output is valid JSON"""
    try:
        json.loads(json_str)
        return True
    except json.JSONDecodeError:
        return False


def run_writer_test(
    test_name: str,
    query_file: str,
    data_file: str,
    expected_file: str,
    roqet_path: str,
    test_dir: str,
) -> bool:
    """Run a single SRJ writer test"""
    print(f"Running test: {test_name}")

    # Build full paths
    query_path = os.path.join(test_dir, query_file)
    data_path = os.path.join(test_dir, data_file)
    expected_path = os.path.join(test_dir, expected_file)

    # Check files exist
    for path in [query_path, data_path, expected_path]:
        if not os.path.exists(path):
            print(f"FAIL {test_name}: Missing file {path}")
            return False

    # Run roqet to generate SRJ output
    generated_output = run_roqet_srj(query_path, data_path, roqet_path)
    if generated_output is None:
        print(f"FAIL {test_name}: Could not generate output")
        return False

    # Validate JSON syntax
    if not validate_json_syntax(generated_output):
        print(f"FAIL {test_name}: Generated output is not valid JSON")
        print(f"Output: {generated_output}")
        return False

    # Read expected output
    try:
        with open(expected_path, "r") as f:
            expected_output = f.read()
    except Exception as e:
        print(f"FAIL {test_name}: Could not read expected file: {e}")
        return False

    # Compare outputs
    return compare_srj_output(generated_output, expected_output, test_name)


def main():
    parser = argparse.ArgumentParser(description="Test SRJ writer functionality")
    parser.add_argument(
        "--roqet", default="../../../utils/roqet", help="Path to roqet executable"
    )
    parser.add_argument(
        "--test-dir", default=".", help="Directory containing test files"
    )
    parser.add_argument("tests", nargs="*", help="Specific tests to run (default: all)")

    args = parser.parse_args()

    # Define test cases
    test_cases = [
        {
            "name": "srj-write-basic-bindings",
            "query": "query-basic-bindings.rq",
            "data": "data-basic-bindings.n3",
            "expected": "expected-write-basic-bindings.srj",
        },
        {
            "name": "srj-write-ask-true",
            "query": "query-ask-true.rq",
            "data": "data-basic-bindings.n3",
            "expected": "expected-write-ask-true.srj",
        },
        {
            "name": "srj-write-ask-false",
            "query": "query-ask-false.rq",
            "data": "data-basic-bindings.n3",
            "expected": "expected-write-ask-false.srj",
        },
        {
            "name": "srj-write-data-types",
            "query": "query-data-types.rq",
            "data": "data-data-types.n3",
            "expected": "expected-write-types.srj",
        },
        {
            "name": "srj-write-empty-results",
            "query": "query-empty.rq",
            "data": "data-basic-bindings.n3",
            "expected": "expected-write-empty.srj",
        },
        {
            "name": "srj-write-special-chars",
            "query": "query-special.rq",
            "data": "data-special-chars.n3",
            "expected": "expected-write-special-chars.srj",
        },
        {
            "name": "srj-write-unbound-vars",
            "query": "query-unbound.rq",
            "data": "data-basic-bindings.n3",
            "expected": "expected-write-unbound.srj",
        },
    ]

    # Filter tests if specific ones requested
    if args.tests:
        test_cases = [tc for tc in test_cases if tc["name"] in args.tests]

    if not test_cases:
        print("No tests to run")
        return 1

    # Run tests
    passed = 0
    failed = 0

    for test_case in test_cases:
        if run_writer_test(
            test_case["name"],
            test_case["query"],
            test_case["data"],
            test_case["expected"],
            args.roqet,
            args.test_dir,
        ):
            passed += 1
        else:
            failed += 1

    # Summary
    total = passed + failed
    print(f"\nSRJ Writer Test Summary:")
    print(f"  Passed: {passed}/{total}")
    print(f"  Failed: {failed}/{total}")

    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())

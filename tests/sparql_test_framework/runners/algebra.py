"""
Algebra Test Runner

This module runs SPARQL algebra graph pattern tests using the convert_graph_pattern utility.

Copyright (C) 2009, David Beckett http://www.dajobe.org/

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
import argparse
from pathlib import Path
from typing import Dict, Optional

from ..config import TestConfig

from ..utils import run_command, setup_logging, find_tool


# Global flag for file preservation
_preserve_debug_files = False

# Temporary file names
CONVERT_OUT = Path("convert.out")
CONVERT_ERR = Path("convert.err")
DIFF_OUT = Path("diff.out")


def cleanup_temp_files() -> None:
    """Clean up temporary files if not preserving them."""
    if not _preserve_debug_files:
        temp_files = [
            CONVERT_OUT,
            CONVERT_ERR,
            DIFF_OUT,
        ]
        for temp_file in temp_files:
            if temp_file.exists():
                try:
                    temp_file.unlink()
                except OSError:
                    pass  # Ignore cleanup errors


class AlgebraTestRunner:
    """SPARQL algebra graph pattern test runner."""

    def __init__(self):
        self.debug_level = 0
        self.logger = None

    def setup_argument_parser(self) -> argparse.ArgumentParser:
        """Setup argument parser for algebra tests."""
        parser = argparse.ArgumentParser(
            description="Run SPARQL algebra graph pattern test",
            formatter_class=argparse.RawDescriptionHelpFormatter,
            epilog="""
Examples:
  %(prog)s test-01.rq http://www.w3.org/TR/2008/REC-rdf-sparql-query-20080115/#
            """,
        )
        parser.add_argument(
            "--debug", "-d", action="store_true", help="Enable extra debugging output"
        )
        parser.add_argument("test", help="Test file (.rq)")
        parser.add_argument("base_uri", help="Base URI for the test")

        return parser

    def process_arguments(self, args: argparse.Namespace) -> None:
        """Process arguments and setup runner state."""
        self.debug_level = 1 if args.debug else 0
        self.logger = setup_logging(args.debug)
        self.test_file = args.test
        self.base_uri = args.base_uri

    def run_single_test(self, test_file: str, base_uri: str) -> int:
        """Run a single algebra test."""
        diff_cmd = os.environ.get("DIFF", "diff")

        # Setup file paths
        convert_graph_pattern = find_tool("convert_graph_pattern")
        if not convert_graph_pattern:
            self.logger.error("convert_graph_pattern not found in PATH")
            return 1

        out_file = str(CONVERT_OUT)
        err_file = str(CONVERT_ERR)
        diff_file = str(DIFF_OUT)

        # Setup paths
        test_path = Path(test_file)
        base_name = test_path.stem  # removes .rq extension

        if not test_path.exists():
            self.logger.error(f"Test file '{test_file}' does not exist")
            return 1

        srcdir = test_path.parent
        expected_file = srcdir / f"{base_name}.out"

        self.logger.debug(f"Running test {test_file} with base URI {base_uri}")

        # Run convert_graph_pattern
        cmd = [convert_graph_pattern, test_file, base_uri]
        self.logger.debug(f"Running '{' '.join(cmd)}'")

        try:
            with open(out_file, "w") as out_f, open(err_file, "w") as err_f:
                result = run_command(
                    cmd, Path("."), f"Error running {convert_graph_pattern}"
                )
                out_f.write(result.stdout)
                err_f.write(result.stderr)

            self.logger.debug(f"Result was {result.returncode}")
            if result.returncode != 0:
                self.logger.error(
                    f"Graph pattern conversion '{test_file}' FAILED to execute"
                )
                self.logger.error(f"Failing program was: {' '.join(cmd)}")
                with open(err_file, "r") as f:
                    self.logger.error(f.read())
                # Don't cleanup on failure to preserve debug files
                return 1
        except FileNotFoundError:
            self.logger.error(f"{convert_graph_pattern} not found in PATH")
            cleanup_temp_files()
            return 1
        except Exception as e:
            self.logger.error(f"Error running {convert_graph_pattern}: {e}")
            cleanup_temp_files()
            return 1

        # Check if expected output file exists
        if not expected_file.exists():
            self.logger.error(f"Expected output file '{expected_file}' does not exist")
            cleanup_temp_files()
            return 1

        # Compare output with expected
        try:
            with open(out_file, "r") as out_f, open(expected_file, "r") as exp_f:
                actual_output = out_f.read()
                expected_output = exp_f.read()

            if actual_output == expected_output:
                self.logger.info(f"Test '{test_file}' PASSED")
                cleanup_temp_files()
                return 0
            else:
                self.logger.error(f"Test '{test_file}' FAILED - output mismatch")

                # Generate diff
                with open(diff_file, "w") as diff_f:
                    diff_result = run_command(
                        [diff_cmd, "-u", str(expected_file), out_file],
                        Path("."),
                        "Error running diff",
                    )
                    diff_f.write(diff_result.stdout)
                    if diff_result.stderr:
                        diff_f.write(diff_result.stderr)

                self.logger.error(f"Diff saved to {diff_file}")
                # Don't cleanup on failure to preserve debug files
                return 1

        except Exception as e:
            self.logger.error(f"Error comparing output: {e}")
            cleanup_temp_files()
            return 1

    def run_format_test(
        self, test_config: TestConfig, srcdir: Path, timeout: int = 30
    ) -> int:
        """Run a format-specific test (compatibility with FormatTestRunner)."""
        # Extract test parameters from config
        test_file = test_config.get("test_file")
        base_uri = test_config.get("base_uri")

        if not test_file or not base_uri:
            print("Missing test_file or base_uri in test configuration")
            return 1

        # Resolve test file path relative to srcdir
        test_path = srcdir / test_file
        if not test_path.exists():
            print(f"Test file not found: {test_path}")
            return 1

        return self.run_single_test(str(test_path), base_uri)

    def get_legacy_tests(self) -> list:
        """Return list of legacy tests (for backward compatibility)."""
        return []

    def main(self) -> int:
        """Main entry point for algebra testing."""
        parser = self.setup_argument_parser()
        args = parser.parse_args()
        self.process_arguments(args)
        return self.run_single_test(args.test, args.base_uri)


def main():
    """Main entry point for algebra testing."""
    runner = AlgebraTestRunner()
    return runner.main()


if __name__ == "__main__":
    sys.exit(main())

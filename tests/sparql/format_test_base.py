#!/usr/bin/env python3
"""
format_test_base.py - Base class for format-specific test runners

This module provides a shared base class for format test runners to reduce
code duplication and ensure consistency across different format test suites.

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
import argparse
from pathlib import Path
from typing import Dict, Callable, Any, Optional
from abc import ABC, abstractmethod

# Add parent directory to path for imports
sys.path.append(str(Path(__file__).parent.parent.parent))
from rasqal_test_util import (
    run_manifest_test,
    find_tool,
)


class FormatTestRunner(ABC):
    """Base class for format-specific test runners."""

    def __init__(self, format_name: str, default_manifest: str = "manifest.ttl"):
        self.format_name = format_name
        self.default_manifest = default_manifest
        self.debug_level = 0

    def debug_print(self, message: str, level: int = 1):
        """Print debug message if current debug level is >= level"""
        if self.debug_level >= level:
            print(message)

    def setup_argument_parser(
        self, description: str, epilog: str = ""
    ) -> argparse.ArgumentParser:
        """Setup common argument parser for format test runners."""
        parser = argparse.ArgumentParser(
            description=description,
            formatter_class=argparse.RawDescriptionHelpFormatter,
            epilog=epilog,
        )

        # Common arguments
        parser.add_argument(
            "--test-case",
            help="Run specific test case from manifest (use with --srcdir for framework integration)",
        )
        parser.add_argument(
            "--list-tests",
            action="store_true",
            help="List available test cases from manifest",
        )
        parser.add_argument(
            "--manifest-file",
            default=self.default_manifest,
            help=f"Manifest file name (default: {self.default_manifest})",
        )
        parser.add_argument(
            "--srcdir",
            type=Path,
            help="Source directory path (required with --test-case for framework integration)",
        )
        parser.add_argument(
            "--roqet",
            help="Path to roqet executable (auto-detected if not specified)",
        )
        parser.add_argument(
            "--timeout",
            type=int,
            default=30,
            help="Command timeout in seconds (default: 30)",
        )
        parser.add_argument(
            "--test-dir", default=".", help="Directory containing test files"
        )
        parser.add_argument(
            "--debug",
            type=int,
            choices=[0, 1, 2],
            default=0,
            help="Debug level: 0=minimal, 1=normal, 2=verbose",
        )
        parser.add_argument(
            "-v",
            "--verbose",
            action="store_true",
            help="Enable verbose output (equivalent to --debug 1)",
        )
        parser.add_argument(
            "tests", nargs="*", help="Specific tests to run by name (default: all)"
        )

        return parser

    def process_arguments(self, args: argparse.Namespace) -> None:
        """Process and validate common arguments."""
        # Handle verbose flag
        if args.verbose:
            args.debug = max(args.debug, 1)

        # Set global debug level
        self.debug_level = args.debug

        # Auto-detect roqet path if not specified
        if not args.roqet:
            args.roqet = find_tool("roqet") or "roqet"

    def run_single_test_case(
        self,
        test_id: str,
        srcdir: Path,
        manifest_filename: str = None,
        timeout: int = 30,
    ) -> int:
        """Run a single test case identified by test_id from manifest."""
        if manifest_filename is None:
            manifest_filename = self.default_manifest

        # Get test handler mapping from subclass
        test_handler_map = self.get_test_handler_map(srcdir, timeout)

        return run_manifest_test(test_id, srcdir, test_handler_map, manifest_filename)

    def get_test_handler_map(
        self, srcdir: Path, timeout: int = 30
    ) -> Dict[str, Callable[[Any, Path], int]]:
        """Return mapping of test_id to handler function. Can be overridden by subclasses."""
        # Default implementation: dynamically generate from manifest
        return self._generate_handler_map_from_manifest(srcdir, timeout)

    def _generate_handler_map_from_manifest(
        self, srcdir: Path, timeout: int = 30
    ) -> Dict[str, Callable[[Any, Path], int]]:
        """Generate test handler map dynamically from manifest."""
        from rasqal_test_util import ManifestParser

        try:
            manifest_file = srcdir / self.default_manifest
            parser = ManifestParser(manifest_file)
            tests = parser.get_tests(srcdir)

            # Create handler for each test
            handler_map = {}
            for test in tests:
                handler_map[test.name] = (
                    lambda test_config, srcdir_path, t=test, to=timeout: self.run_format_test(
                        t, srcdir_path, to
                    )
                )

            return handler_map
        except Exception as e:
            self.debug_print(f"Failed to generate handler map from manifest: {e}", 1)
            # Fallback to empty map
            return {}

    @abstractmethod
    def run_format_test(self, test_config: Any, srcdir: Path, timeout: int = 30) -> int:
        """Run a format-specific test. Must be implemented by subclasses."""
        pass

    def run_legacy_tests(self, args: argparse.Namespace) -> int:
        """Run tests in legacy mode (backward compatibility)."""
        # This can be overridden by subclasses if they need custom legacy behavior
        print(f"{self.format_name.upper()} Format Support Test Suite")
        print("=" * 50)

        # Change to project root directory
        project_root = Path(__file__).parent.parent.parent.parent
        os.chdir(project_root)
        print(f"Working directory: {os.getcwd()}")

        # Run all tests
        tests = self.get_legacy_tests()
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

    def get_legacy_tests(self) -> list:
        """Return list of (test_name, test_function) tuples for legacy mode. Can be overridden."""
        return []

    def main(self, description: str, epilog: str = "") -> int:
        """Main entry point for format test runners."""
        parser = self.setup_argument_parser(description, epilog)
        args = parser.parse_args()

        self.process_arguments(args)

        # For manifest-driven test execution (improve.py integration)
        if args.test_case and args.srcdir:
            return self.run_single_test_case(
                args.test_case, args.srcdir, args.manifest_file, args.timeout
            )

        # Handle --list-tests option
        if args.list_tests:
            from rasqal_test_util import ManifestParser

            try:
                manifest_file = (
                    args.srcdir / args.manifest_file
                    if args.srcdir
                    else Path(args.test_dir) / args.manifest_file
                )
                parser = ManifestParser(manifest_file)
                tests = parser.get_tests(Path(args.test_dir))
                if tests:
                    print("Available test cases:")
                    for test in tests:
                        print(f"  {test.name}")
                else:
                    print("No test cases found in manifest")
                return 0
            except Exception as e:
                print(f"Failed to list tests: {e}")
                return 1

        # Run legacy tests
        return self.run_legacy_tests(args)

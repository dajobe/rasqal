"""
Format Test Runner Base Class

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
from typing import Dict, Callable

from ..config import TestConfig, Optional
from abc import ABC, abstractmethod

from ..execution import run_manifest_test
from ..utils import find_tool


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
            help="Debug level (0=quiet, 1=normal, 2=verbose)",
        )

        return parser

    def process_arguments(self, args: argparse.Namespace) -> None:
        """Process common arguments and setup runner state."""
        self.debug_level = args.debug
        self.roqet_path = args.roqet or find_tool("roqet")
        self.timeout = args.timeout

    def run_single_test_case(
        self,
        test_id: str,
        srcdir: Path,
        manifest_filename: str = None,
        timeout: int = 30,
    ) -> int:
        """Run a single test case by ID."""
        if manifest_filename is None:
            manifest_filename = self.default_manifest

        manifest_path = srcdir / manifest_filename
        if not manifest_path.exists():
            print(f"Manifest file not found: {manifest_path}")
            return 1

        return run_manifest_test(
            test_id,
            srcdir,
            self.get_test_handler_map(srcdir, timeout, manifest_filename),
            manifest_filename,
        )

    def get_test_handler_map(
        self, srcdir: Path, timeout: int = 30, manifest_filename: str = None
    ) -> Dict[str, Callable[[TestConfig, Path], int]]:
        """Get the test handler map for this format."""
        if manifest_filename is None:
            manifest_filename = self.default_manifest
        return self._generate_handler_map_from_manifest(
            srcdir, timeout, manifest_filename
        )

    def _generate_handler_map_from_manifest(
        self, srcdir: Path, timeout: int = 30, manifest_filename: str = None
    ) -> Dict[str, Callable[[TestConfig, Path], int]]:
        """Generate test handler map from manifest file."""
        from ..manifest import ManifestParser

        if manifest_filename is None:
            manifest_filename = self.default_manifest
        manifest_path = srcdir / manifest_filename
        if not manifest_path.exists():
            return {}

        parser = ManifestParser(manifest_path)
        tests = parser.get_tests(srcdir)

        handler_map = {}
        for test in tests:
            test_id = test.name
            if test_id:
                # Create a closure to capture the test configuration
                def create_handler(test_config: TestConfig):
                    def handler(roqet_path: str, srcdir: Path) -> int:
                        return self.run_format_test(test_config, srcdir, timeout)

                    return handler

                handler_map[test_id] = create_handler(test)

        return handler_map

    @abstractmethod
    def run_format_test(
        self, test_config: TestConfig, srcdir: Path, timeout: int = 30
    ) -> int:
        """Run a format-specific test. Must be implemented by subclasses."""
        pass

    def run_legacy_tests(self, args: argparse.Namespace) -> int:
        """Run legacy test format (for backward compatibility)."""
        test_dir = Path(args.test_dir)
        manifest_file = args.manifest_file

        if args.test_case:
            if not args.srcdir:
                print("Error: --srcdir is required when using --test-case")
                return 1
            return self.run_single_test_case(
                args.test_case, args.srcdir, manifest_file, args.timeout
            )

        if args.list_tests:
            from ..manifest import ManifestParser

            manifest_path = test_dir / manifest_file
            if not manifest_path.exists():
                print(f"Manifest file not found: {manifest_path}")
                return 1

            parser = ManifestParser(manifest_path)
            tests = parser.get_tests(test_dir)
            for test in tests:
                print(test.name)
            return 0

        # Run all tests in the manifest
        handler_map = self.get_test_handler_map(test_dir, args.timeout, manifest_file)
        if not handler_map:
            print(f"No tests found in {test_dir / manifest_file}")
            return 1

        return run_manifest_test(None, test_dir, handler_map, manifest_file)

    def get_legacy_tests(self) -> list:
        """Get list of legacy tests (for backward compatibility)."""
        return []

    def main(self, description: str, epilog: str = "") -> int:
        """Main entry point for format test runners."""
        parser = self.setup_argument_parser(description, epilog)
        args = parser.parse_args()
        self.process_arguments(args)
        return self.run_legacy_tests(args)

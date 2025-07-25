"""
Test Packager

This module provides functionality for packaging SPARQL test files for distribution
by collecting all files referenced in manifest files.

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

import argparse
import logging
import shutil
import sys
from pathlib import Path
from typing import List, Set

from ..manifest import ManifestParser
from ..utils import setup_logging
from ..execution import validate_required_paths


class TestPackager:
    """
    Packages SPARQL test files for distribution by collecting all files referenced in manifest files.
    """

    def __init__(self, debug: bool = False):
        """
        Initialize the test packager.

        Args:
            debug: Enable debug logging
        """
        self.logger = setup_logging(debug=debug)

    def extract_files_from_manifest(
        self, manifest_path: Path, srcdir: Path
    ) -> List[str]:
        """
        Parse a manifest file to find all referenced file paths.

        Args:
            manifest_path: Path to the manifest file
            srcdir: Source directory for resolving relative paths

        Returns:
            List of file paths found in the manifest (relative to srcdir)
        """
        try:
            # Create manifest parser
            manifest_parser = ManifestParser(manifest_path)

            # Get all test configurations
            test_configs = manifest_parser.get_tests(srcdir)

            all_files = set()

            for config in test_configs:
                # Add test file
                if config.test_file:
                    try:
                        if config.test_file.is_absolute():
                            # Convert absolute path to relative path from srcdir
                            rel_path = config.test_file.relative_to(srcdir.resolve())
                        else:
                            rel_path = config.test_file
                        all_files.add(str(rel_path))
                    except ValueError:
                        # Path is not relative to srcdir, skip it
                        self.logger.debug(
                            f"Skipping test file not in srcdir: {config.test_file}"
                        )

                # Add data files
                for data_file in config.data_files:
                    if data_file:
                        try:
                            if data_file.is_absolute():
                                rel_path = data_file.relative_to(srcdir.resolve())
                            else:
                                rel_path = data_file
                            all_files.add(str(rel_path))
                        except ValueError:
                            self.logger.debug(
                                f"Skipping data file not in srcdir: {data_file}"
                            )

                # Add named data files
                for named_data_file in config.named_data_files:
                    if named_data_file:
                        try:
                            if named_data_file.is_absolute():
                                rel_path = named_data_file.relative_to(srcdir.resolve())
                            else:
                                rel_path = named_data_file
                            all_files.add(str(rel_path))
                        except ValueError:
                            self.logger.debug(
                                f"Skipping named data file not in srcdir: {named_data_file}"
                            )

                # Add result file
                if config.result_file:
                    try:
                        if config.result_file.is_absolute():
                            rel_path = config.result_file.relative_to(srcdir.resolve())
                        else:
                            rel_path = config.result_file
                        all_files.add(str(rel_path))
                    except ValueError:
                        self.logger.debug(
                            f"Skipping result file not in srcdir: {config.result_file}"
                        )

                # Add extra files
                for extra_file in config.extra_files:
                    if extra_file:
                        try:
                            if extra_file.is_absolute():
                                rel_path = extra_file.relative_to(srcdir.resolve())
                            else:
                                rel_path = extra_file
                            all_files.add(str(rel_path))
                        except ValueError:
                            self.logger.debug(
                                f"Skipping extra file not in srcdir: {extra_file}"
                            )

            self.logger.debug(
                f"Found {len(all_files)} files in manifest {manifest_path}"
            )
            return sorted(list(all_files))

        except Exception as e:
            raise RuntimeError(f"Error parsing manifest {manifest_path}: {e}")

    def package_files(
        self,
        manifest_files: List[Path],
        srcdir: Path,
        distdir: Path,
    ) -> int:
        """
        Package all files referenced in manifest files to a distribution directory.

        Args:
            manifest_files: List of manifest file paths
            srcdir: Source directory
            distdir: Destination directory

        Returns:
            Number of files successfully copied
        """
        # Validate required paths
        try:
            validate_required_paths((srcdir, "Source directory"))
            for manifest_file in manifest_files:
                validate_required_paths(
                    (manifest_file, f"Manifest file {manifest_file}")
                )
        except FileNotFoundError as e:
            raise RuntimeError(str(e))

        # Ensure the destination directory exists
        distdir.mkdir(parents=True, exist_ok=True)

        # Collect all files from all manifest files
        all_files_to_copy: Set[str] = set()
        for manifest_file in manifest_files:
            files_from_manifest = self.extract_files_from_manifest(
                manifest_file, srcdir
            )
            all_files_to_copy.update(files_from_manifest)
            self.logger.info(
                f"Found {len(files_from_manifest)} files in {manifest_file}"
            )

        # Convert to sorted list for consistent output
        files_to_copy = sorted(list(all_files_to_copy))
        self.logger.info(
            f"Copying {len(files_to_copy)} unique test files to {distdir}..."
        )

        copied_count = 0
        for f in files_to_copy:
            src_path = srcdir / f
            dest_path = distdir / f
            if src_path.exists():
                # Ensure the destination subdirectory exists
                dest_path.parent.mkdir(parents=True, exist_ok=True)

                # Check if destination file already exists
                if dest_path.exists():
                    self.logger.warning(f"Overwriting existing file: {dest_path}")

                try:
                    shutil.copy2(src_path, dest_path)
                    copied_count += 1
                except PermissionError as e:
                    self.logger.error(
                        f"Permission denied when copying {src_path} to {dest_path}: {e}"
                    )
                    self.logger.info("Attempting to remove existing file and retry...")
                    try:
                        dest_path.unlink()
                        shutil.copy2(src_path, dest_path)
                        copied_count += 1
                    except Exception as e2:
                        raise RuntimeError(
                            f"Failed to copy {src_path} after removing existing file: {e2}"
                        )
            else:
                self.logger.warning(f"File not found for copying: {src_path}")

        self.logger.info(f"Successfully copied {copied_count} files.")
        return copied_count


def main():
    """Main function - CLI entry point for test packaging."""
    parser = argparse.ArgumentParser(
        description="Collects all test files referenced in SPARQL manifests and copies them to a distribution directory.",
        formatter_class=argparse.RawTextHelpFormatter,
    )

    parser.add_argument(
        "-d", "--debug", action="store_true", help="Enable debug logging"
    )
    parser.add_argument(
        "--srcdir",
        type=Path,
        required=True,
        help="The source directory (equivalent to $(srcdir) in Makefile.am)",
    )
    parser.add_argument(
        "--builddir",
        type=Path,
        help="The build directory (equivalent to $(top_builddir) in Makefile.am)",
    )
    parser.add_argument(
        "--manifest-file",
        type=Path,
        action="append",
        required=True,
        help="Path to a manifest file (e.g., manifest.n3) to extract file lists from. Can be specified multiple times.",
    )
    parser.add_argument(
        "--distdir",
        type=Path,
        required=True,
        help="The destination directory to copy the files to (equivalent to $(distdir)).",
    )

    args = parser.parse_args()

    # Create packager
    packager = TestPackager(debug=args.debug)

    try:
        # Package files
        copied_count = packager.package_files(
            manifest_files=args.manifest_file,
            srcdir=args.srcdir,
            distdir=args.distdir,
        )
        print(f"Successfully packaged {copied_count} test files.")
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()

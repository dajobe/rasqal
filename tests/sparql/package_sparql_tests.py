#!/usr/bin/env python3
#
# package_sparql_tests.py - Helper script to package SPARQL test files for distribution.
#
# Copyright (C) 2025, David Beckett http://www.dajobe.org/
#
# This package is Free Software and part of Redland http://librdf.org/
#
# It is licensed under the following three licenses as alternatives:
#    1. GNU Lesser General Public License (LGPL) V2.1 or any newer version
#    2. GNU General Public License (GPL) V2 or any newer version
#    3. Apache License, V2.0 or any newer version
#
# You may not use this file except in compliance with at least one of
# the above three licenses.
#
# See LICENSE.html or LICENSE.txt at the top of this package for the
# complete terms and further detail along with the license texts for
# the licenses in COPYING.LIB, COPYING and LICENSE-2.0.txt respectively.
#

import argparse
import sys
import logging
from pathlib import Path
import shutil

# Add the parent directory to the Python path to find rasqal_test_util
sys.path.append(str(Path(__file__).resolve().parent.parent))

from rasqal_test_util import (
    ManifestParser,
    find_tool,
    Namespaces,
    setup_logging,
    validate_required_paths,
)


def extract_all_files_from_manifest(
    manifest_path: Path, srcdir: Path, logger: logging.Logger
) -> list:
    """
    Parses a manifest file using ManifestParser to find all referenced file paths.

    Args:
        manifest_path: Path to the manifest file
        srcdir: Source directory for resolving relative paths
        logger: Logger instance for debug output

    Returns:
        List of file paths found in the manifest
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
                    logger.debug(
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
                        logger.debug(f"Skipping data file not in srcdir: {data_file}")

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
                        logger.debug(
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
                    logger.debug(
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
                        logger.debug(f"Skipping extra file not in srcdir: {extra_file}")

        logger.debug(f"Found {len(all_files)} files in manifest {manifest_path}")
        return sorted(list(all_files))

    except Exception as e:
        logger.error(f"Error parsing manifest {manifest_path}: {e}")
        sys.exit(1)


def main():
    parser = argparse.ArgumentParser(
        description="Collects all test files referenced in SPARQL manifests and copies them to a distribution directory.",
        formatter_class=argparse.RawTextHelpFormatter,
    )

    # Add common arguments (but not --manifest-file since we need a different type)
    parser.add_argument(
        "-d", "--debug", action="store_true", help="Enable debug logging"
    )
    parser.add_argument(
        "--srcdir",
        type=Path,
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

    # Setup logging
    logger = setup_logging(args.debug)

    # Validate required paths
    try:
        validate_required_paths(
            (args.srcdir, "Source directory"),
        )
        for manifest_file in args.manifest_file:
            validate_required_paths((manifest_file, f"Manifest file {manifest_file}"))
    except FileNotFoundError as e:
        logger.error(str(e))
        sys.exit(1)

    # Ensure the destination directory exists
    args.distdir.mkdir(parents=True, exist_ok=True)

    # Collect all files from all manifest files
    all_files_to_copy = set()
    for manifest_file in args.manifest_file:
        files_from_manifest = extract_all_files_from_manifest(
            manifest_file, args.srcdir, logger
        )
        all_files_to_copy.update(files_from_manifest)
        logger.info(f"Found {len(files_from_manifest)} files in {manifest_file}")

    # Convert to sorted list for consistent output
    files_to_copy = sorted(list(all_files_to_copy))
    logger.info(f"Copying {len(files_to_copy)} unique test files to {args.distdir}...")

    for f in files_to_copy:
        src_path = args.srcdir / f
        dest_path = args.distdir / f
        if src_path.exists():
            # Ensure the destination subdirectory exists
            dest_path.parent.mkdir(parents=True, exist_ok=True)

            # Check if destination file already exists
            if dest_path.exists():
                logger.warning(f"Overwriting existing file: {dest_path}")

            try:
                shutil.copy2(src_path, dest_path)
            except PermissionError as e:
                logger.error(
                    f"Permission denied when copying {src_path} to {dest_path}: {e}"
                )
                logger.info("Attempting to remove existing file and retry...")
                try:
                    dest_path.unlink()
                    shutil.copy2(src_path, dest_path)
                except Exception as e2:
                    logger.error(
                        f"Failed to copy {src_path} after removing existing file: {e2}"
                    )
                    sys.exit(1)
        else:
            logger.warning(f"File not found for copying: {src_path}")

    logger.info("Done.")


if __name__ == "__main__":
    main()

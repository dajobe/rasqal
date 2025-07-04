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
from pathlib import Path
import re
import shutil

def extract_all_files_from_manifest(manifest_path):
    """
    Parses a manifest file with regex to find all referenced file paths.
    """
    manifest_content = manifest_path.read_text()

    all_files = set()
    # Find files in qt:data, qt:query, and mf:result properties
    all_files.update(re.findall(r'qt:data\s+<([^>]+)>', manifest_content))
    all_files.update(re.findall(r'qt:query\s+<([^>]+)>', manifest_content))
    all_files.update(re.findall(r'mf:result\s+<([^>]+)>', manifest_content))

    return sorted(list(all_files))

def main():
    parser = argparse.ArgumentParser(
        description="Collects all test files referenced in a SPARQL manifest and copies them to a distribution directory.",
        formatter_class=argparse.RawTextHelpFormatter,
    )
    parser.add_argument(
        "--manifest-file",
        type=Path,
        required=True,
        help="Path to the manifest file (e.g., manifest.n3) to extract file lists from."
    )
    parser.add_argument(
        "--srcdir",
        type=Path,
        required=True,
        help="The source directory where the test files are located (equivalent to $(srcdir))."
    )
    parser.add_argument(
        "--distdir",
        type=Path,
        required=True,
        help="The destination directory to copy the files to (equivalent to $(distdir))."
    )

    args = parser.parse_args()

    if not args.manifest_file.exists():
        print(f"Error: Manifest file not found at {args.manifest_file}", file=sys.stderr)
        sys.exit(1)
    if not args.srcdir.is_dir():
        print(f"Error: Source directory not found at {args.srcdir}", file=sys.stderr)
        sys.exit(1)

    # Ensure the destination directory exists
    args.distdir.mkdir(parents=True, exist_ok=True)

    files_to_copy = extract_all_files_from_manifest(args.manifest_file)
    print(f"Copying {len(files_to_copy)} test files to {args.distdir}..." )

    for f in files_to_copy:
        src_path = args.srcdir / f
        dest_path = args.distdir / f
        if src_path.exists():
            # Ensure the destination subdirectory exists
            dest_path.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src_path, dest_path)
        else:
            print(f"Warning: File not found for copying: {src_path}", file=sys.stderr)

    print("Done.")

if __name__ == "__main__":
    main()

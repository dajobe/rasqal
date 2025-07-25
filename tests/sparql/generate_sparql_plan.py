#!/usr/bin/env python3
#
# generate_sparql_plan.py - Helper script to generate SPARQL test plan files in Turtle format.
#
# DEPRECATED: This script is deprecated. Use 'create-test-plan' from tests/bin/ instead.
# This script will be removed in a future version.
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
import warnings
from pathlib import Path
import os
import subprocess
import shlex
from typing import Optional

# Issue deprecation warning
warnings.warn(
    "The 'generate_sparql_plan.py' script is deprecated. Use 'create-test-plan' from tests/bin/ instead. "
    "This script will be removed in a future version.",
    DeprecationWarning,
    stacklevel=1,
)

# Add the parent directory to the Python path to find rasqal_test_util
sys.path.append(str(Path(__file__).resolve().parent.parent))

from rasqal_test_util import (
    ManifestParser,
    find_tool,
    Namespaces,
    decode_literal,
    escape_turtle_literal,
    ManifestParsingError,
    UtilityNotFoundError,
    setup_logging,
)


def _parse_arguments() -> argparse.Namespace:
    """Parse command line arguments and return parsed namespace."""
    parser = argparse.ArgumentParser(
        description="Generate a Turtle-formatted SPARQL test plan file or extract file lists from a manifest.",
        formatter_class=argparse.RawTextHelpFormatter,
    )
    parser.add_argument(
        "-d", "--debug", action="store_true", help="Enable debug logging"
    )
    # Import here to avoid circular imports
    from rasqal_test_util import get_available_test_suites

    available_suites = get_available_test_suites()

    parser.add_argument(
        "--suite-name",
        help=f"The name of the test suite. Available: {', '.join(available_suites)}. Required for plan generation.",
    )
    parser.add_argument(
        "--srcdir",
        type=Path,
        help="The source directory (equivalent to $(srcdir) in Makefile.am). Required for plan generation and --copy-to-distdir.",
    )
    parser.add_argument(
        "--builddir",
        type=Path,
        help="The build directory (equivalent to $(top_builddir) in Makefile.am). Required for plan generation.",
    )
    parser.add_argument(
        "--test-type",
        default="PositiveTest",
        help="The type of test (e.g., 'PositiveTest', 'NegativeTest', 'XFailTest'). Required for plan generation.",
    )
    parser.add_argument(
        "--test-category",
        choices=["sparql", "engine", "generic"],
        default="sparql",
        help="The category of tests to generate. 'sparql' for SPARQL tests, 'engine' for C executable tests, 'generic' for custom tests.",
    )
    parser.add_argument(
        "--action-template",
        required=True,
        help="A template string for the mf:action. Placeholders: {srcdir}, {builddir}, {test_id}, {query_file}, {abs_srcdir}, {abs_builddir}. Required for plan generation.",
    )
    parser.add_argument(
        "--comment-template",
        default=None,
        help="A template string for the rdfs:comment. Placeholders: {suite_name}, {test_id}, {query_file}. If not provided, uses default based on suite name.",
    )
    parser.add_argument(
        "--manifest-file",
        type=Path,
        help="Path to a manifest file (e.g., manifest.n3) to extract test IDs from or to generate file lists.",
    )
    parser.add_argument(
        "--test-data-file-map",
        default=None,
        help="Comma-separated list of test_id:data_file pairs, e.g. 'rasqal_order_test:animals.nt,rasqal_graph_test:graph-a.ttl'",
    )
    parser.add_argument(
        "--path",
        help="Path to be prepended to PATH environment variable when running tests (adds t:path to manifest)",
    )
    parser.add_argument(
        "TEST_IDS",
        nargs="*",
        help="List of test identifiers (e.g., dawg-triple-pattern-001, dawg-tp-01.rq). Used if --manifest-file is not provided.",
    )

    args = parser.parse_args()

    # Validate required arguments
    if not all([args.suite_name, args.srcdir, args.builddir, args.action_template]):
        parser.error(
            "--suite-name, --srcdir, --builddir, and --action-template are required for plan generation."
        )

    # Validate suite name
    from rasqal_test_util import validate_test_suite_name

    if args.suite_name and not validate_test_suite_name(args.suite_name):
        available_suites = get_available_test_suites()
        parser.error(
            f"Unknown test suite '{args.suite_name}'. Available suites: {', '.join(available_suites)}"
        )

    # Parse test-data-file-map into a dictionary if provided
    args.test_data_file_map_dict = {}
    if args.test_data_file_map:
        for pair in args.test_data_file_map.split(","):
            if ":" in pair:
                test_id, data_file = pair.split(":", 1)
                args.test_data_file_map_dict[test_id.strip()] = data_file.strip()

    return args


def _process_manifest_entries(
    manifest_parser: ManifestParser,
    manifest_node_uri: str,
    args: argparse.Namespace,
    logger: logging.Logger,
) -> list:
    """Process manifest entries using the existing get_tests() method."""
    # For engine tests, use a simpler approach since they don't follow W3C format
    if args.test_category == "engine":
        return _process_engine_manifest_entries(
            manifest_parser, manifest_node_uri, args, logger
        )

    # Use the existing get_tests() method to get TestConfig objects
    test_configs = manifest_parser.get_tests(args.srcdir)

    tests = []
    for config in test_configs:
        test_id = config.name

        # Determine test type using the existing logic
        test_type = args.test_type  # Default
        if config.test_type:
            from rasqal_test_util import TestTypeResolver

            test_type = TestTypeResolver.classify_test_type(
                config.test_type, args.suite_name, args.test_type
            )

        # Convert URI to prefixed local name for Turtle output
        test_type = Namespaces.uri_to_prefixed_name(test_type)

        # Extract query file and data file
        query_file = None
        data_file = None

        if config.test_file:
            # Handle both relative and absolute paths
            try:
                query_file = str(config.test_file.relative_to(args.srcdir))
            except ValueError:
                # If the file is not relative to srcdir, use the filename
                query_file = config.test_file.name

            # Handle data files similarly
            data_file = None
            if config.data_files:
                try:
                    data_file = str(config.data_files[0].relative_to(args.srcdir))
                except ValueError:
                    data_file = config.data_files[0].name

        tests.append(
            {
                "id": test_id,
                "query_file": query_file or "",
                "data_file": data_file or "",
                "test_type": test_type,
            }
        )

    return tests


def _process_engine_manifest_entries(
    manifest_parser: ManifestParser,
    manifest_node_uri: str,
    args: argparse.Namespace,
    logger: logging.Logger,
) -> list:
    """Process engine manifest entries which use a simpler format."""
    tests = []

    # Find the manifest node and iterate over entries
    entries_list_head = None
    for s, triples in manifest_parser.triples_by_subject.items():
        for t in triples:
            if t["p"] == f"<{Namespaces.MF}entries>":
                entries_list_head = t["o_full"]
                break
        if entries_list_head:
            break

    if not entries_list_head:
        logger.warning("Could not find mf:entries list in engine manifest")
        return tests

    # Traverse list and extract each entry
    current_list_item_node = entries_list_head
    while current_list_item_node and current_list_item_node != f"<{Namespaces.RDF}nil>":
        list_node_triples = manifest_parser.triples_by_subject.get(
            current_list_item_node, []
        )
        entry_node_full = next(
            (
                t["o_full"]
                for t in list_node_triples
                if t["p"] == f"<{Namespaces.RDF}first>"
            ),
            None,
        )

        if entry_node_full:
            entry_triples = manifest_parser.triples_by_subject.get(entry_node_full, [])

            # Extract test name
            test_name = None
            for t in entry_triples:
                if t["p"] == f"<{Namespaces.MF}name>":
                    test_name = decode_literal(t["o_full"])
                    break

            if test_name:
                # Convert test type to prefixed name for Turtle output
                test_type = Namespaces.uri_to_prefixed_name(args.test_type)
                tests.append(
                    {
                        "id": test_name,
                        "query_file": "",
                        "data_file": "",
                        "test_type": test_type,
                    }
                )

        current_list_item_node = next(
            (
                t["o_full"]
                for t in list_node_triples
                if t["p"] == f"<{Namespaces.RDF}rest>"
            ),
            None,
        )

    return tests


def _process_test_ids(test_ids: list, test_category: str) -> list:
    """Process test IDs when no manifest file is provided."""
    tests = []
    for test_id in test_ids:
        tests.append({"id": test_id, "query_file": test_id})
    return tests


def _print_turtle_header(
    test_category: str, suite_name: str, path: Optional[str] = None
):
    """Print the Turtle header for the test plan."""
    path_line = f'    t:path """{path}""" ;\n' if path else ""
    print(
        f"""@prefix rdfs:  <http://www.w3.org/2000/01/rdf-schema#> .
@prefix mf:    <http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#> .
@prefix qt:    <http://www.w3.org/2001/sw/DataAccess/tests/test-query#> .
@prefix t:     <http://ns.librdf.org/2009/test-manifest#> .
@prefix mfx:   <http://jena.hpl.hp.com/2005/05/test-manifest-extra#> .

<> a mf:Manifest ;
    rdfs:comment "{test_category.upper()} {suite_name} tests" ;
{path_line}    mf:entries (
"""
    )


def _print_test_entries(tests: list, args: argparse.Namespace):
    """Print the test entries in Turtle format."""
    if not tests:
        print("  ) .")
        return

    abs_srcdir = str(args.srcdir.resolve()) if args.srcdir else ""
    abs_builddir = str(args.builddir.resolve()) if args.builddir else ""

    for i, test_info in enumerate(tests):
        test_id = test_info["id"].strip()
        if not test_id:
            continue
        query_file = test_info.get("query_file", "")
        data_file = test_info.get("data_file", "")
        # Use full path for query_file
        query_file_full = str(args.srcdir / query_file) if query_file else ""
        data_file_full = str(args.srcdir / data_file) if data_file else ""
        test_type = test_info.get("test_type", args.test_type)
        # Add data_file if mapping is provided and template uses it (override manifest if present)
        data_file_override = (
            args.test_data_file_map_dict.get(test_id, "")
            if hasattr(args, "test_data_file_map_dict")
            else ""
        )
        if data_file_override:
            data_file = data_file_override
            data_file_full = str(args.srcdir / data_file) if data_file else ""
        template_vars = dict(
            srcdir=str(args.srcdir),
            builddir=str(args.builddir),
            abs_srcdir=abs_srcdir,
            abs_builddir=abs_builddir,
            test_id=shlex.quote(test_id),
            query_file=test_info.get("query_file", ""),
            data_file=data_file,
            query_file_full=query_file_full,
            data_file_full=data_file_full,
        )
        # Use pre-computed action if available, otherwise format the template
        if "action_raw" in test_info:
            action_raw = test_info["action_raw"]
        else:
            action_raw = args.action_template.format(**template_vars)
        # Use the new comment template system
        from rasqal_test_util import get_comment_template

        comment_template = get_comment_template(args.suite_name, args.comment_template)
        comment_raw = comment_template.format(
            suite_name=args.suite_name, test_id=test_id, query_file=query_file
        )
        escaped_test_id_for_print = escape_turtle_literal(test_id)
        escaped_action_for_print = escape_turtle_literal(action_raw)
        escaped_comment_for_print = escape_turtle_literal(comment_raw)
        print(
            f'''  [ a {test_type};
    mf:name """{escaped_test_id_for_print}""";
    rdfs:comment """{escaped_comment_for_print}""";
    mf:action """{escaped_action_for_print}""" ]'''
        )
    print("    ) .")


def main():
    """Main function - orchestrates the test plan generation process."""
    args = _parse_arguments()
    logger = setup_logging(debug=args.debug)

    # Debug: print manifest file, cwd, sys.path
    logger.debug(f"manifest_file: {args.manifest_file}")
    logger.debug(f"cwd: {Path.cwd()}")
    logger.debug(f"sys.path: {sys.path}")

    # Process tests based on input method
    if args.manifest_file:
        try:
            manifest_parser = ManifestParser.from_manifest_file(
                args.manifest_file, args.srcdir, logger
            )
            manifest_node_uri = manifest_parser.find_manifest_node()
            logger.debug(f"Found manifest node: {manifest_node_uri}")
        except (FileNotFoundError, UtilityNotFoundError, RuntimeError) as e:
            logger.error(f"Error creating manifest parser: {e}")
            sys.exit(1)
        except ManifestParsingError as e:
            logger.error(f"Could not find manifest node: {e}")
            # Debug: print all subjects and their types for troubleshooting
            logger.debug(
                f"Total subjects found: {len(manifest_parser.triples_by_subject)}"
            )
            for s, triples in manifest_parser.triples_by_subject.items():
                types = [
                    t["o_full"] for t in triples if t["p"] == f"<{Namespaces.RDF}type>"
                ]
                logger.debug(f"Subject: {s} Types: {types}")
            sys.exit(1)
        tests = _process_manifest_entries(
            manifest_parser, manifest_node_uri, args, logger
        )
    else:
        if not args.TEST_IDS:
            logger.error(
                "Either --manifest-file or TEST_IDS must be provided for plan generation."
            )
            sys.exit(1)
        tests = _process_test_ids(args.TEST_IDS, args.test_category)

    # Generate output
    _print_turtle_header(args.test_category, args.suite_name, args.path)
    _print_test_entries(tests, args)


if __name__ == "__main__":
    main()

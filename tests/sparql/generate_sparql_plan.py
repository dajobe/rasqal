#!/usr/bin/env python3
#
# generate_sparql_plan.py - Helper script to generate SPARQL test plan files in Turtle format.
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
import os
import subprocess
import shlex
from typing import Optional

# Add the parent directory to the Python path to find rasqal_test_util
sys.path.append(str(Path(__file__).resolve().parent.parent))

from rasqal_test_util import (
    ManifestParser,
    find_tool,
    Namespaces,
    decode_literal,
    ManifestParsingError,
    UtilityNotFoundError,
    setup_logging,
)


def _escape_turtle_literal(s: str) -> str:
    """Escapes a string for use as a triple-quoted Turtle literal.
    Handles backslashes, triple double quotes, and control characters (\n, \r, \t).
    """
    # Escape backslashes first
    s = s.replace("\\", "\\\\")
    # Handle triple quotes by replacing with escaped double quotes
    s = s.replace('"""', '\\"\\"\\"')
    if s.endswith('"'):  # avoid """{escaped ending with "}"""
        s = s[:-1] + '\\"'
    # Escape newlines, carriage returns, and tabs
    s = s.replace("\n", "\\n")
    s = s.replace("\r", "\\r")
    s = s.replace("\t", "\\t")
    return s


def _escape_shell_argument(s: str) -> str:
    """Escapes a string for use as a shell argument.
    Handles spaces, quotes, and special shell characters.
    """
    return shlex.quote(s)


def _parse_arguments() -> argparse.Namespace:
    """Parse command line arguments and return parsed namespace."""
    parser = argparse.ArgumentParser(
        description="Generate a Turtle-formatted SPARQL test plan file or extract file lists from a manifest.",
        formatter_class=argparse.RawTextHelpFormatter,
    )
    parser.add_argument(
        "-d", "--debug", action="store_true", help="Enable debug logging"
    )
    parser.add_argument(
        "--suite-name",
        help="The name of the test suite (e.g., 'sparql-query', 'sparql-lexer'). Required for plan generation.",
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
        help="A template string for the mf:action. Placeholders: {srcdir}, {builddir}, {test_id}, {query_file}, {abs_srcdir}, {abs_builddir}, {abs_executable}. Required for plan generation.",
    )
    parser.add_argument(
        "--comment-template",
        default="Test {suite_name} {test_id}",
        help="A template string for the rdfs:comment. Placeholders: {suite_name}, {test_id}, {query_file}.",
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

    # Parse test-data-file-map into a dictionary if provided
    args.test_data_file_map_dict = {}
    if args.test_data_file_map:
        for pair in args.test_data_file_map.split(","):
            if ":" in pair:
                test_id, data_file = pair.split(":", 1)
                args.test_data_file_map_dict[test_id.strip()] = data_file.strip()

    return args


def _determine_test_type(type_uri: str, suite_name: str, default_type: str) -> str:
    """Determine the test type based on the type URI and suite name."""
    if "TestBadSyntax" in type_uri or "TestNegativeSyntax" in type_uri:
        # Negative syntax tests should be negative for all suites
        # that can execute queries (parser, query)
        if suite_name in ["sparql-parser", "sparql-query"]:
            return "NegativeTest"
        else:
            return "PositiveTest"  # Skip negative tests for lexer
    elif "XFailTest" in type_uri:
        # Expected failure tests should be marked as XFailTest for all suites
        return "XFailTest"
    elif "TestSyntax" in type_uri:
        return "PositiveTest"
    else:
        return default_type


def _extract_query_file_and_action(
    action_node: str,
    action_triples: list,
    test_category: str,
    srcdir: Path,
    builddir: Optional[Path] = None,
) -> tuple:
    """Extract query file, data file, and action from action node."""
    query_file = None
    data_file = None
    action = None

    # If the action is a literal, decode it
    if action_node.startswith('"'):
        action_value = decode_literal(action_node)
    # If the action is a URI or blank node, try to extract a literal from the referenced node
    else:
        literal_action = next(
            (
                t["o_full"]
                for t in action_triples
                if t["p"] == f"<{Namespaces.MF}action>" and t["o_full"].startswith('"')
            ),
            None,
        )
        if literal_action:
            action_value = decode_literal(literal_action)
        else:
            action_value = decode_literal(action_node)
        # For sparql tests, extract qt:query as query_file
        if test_category == "sparql":
            qt_query = next(
                (
                    t["o_full"]
                    for t in action_triples
                    if t["p"] == f"<{Namespaces.QT}query>"
                ),
                None,
            )
            if qt_query and qt_query.startswith("<") and qt_query.endswith(">"):
                query_file = os.path.basename(qt_query[1:-1])
                if not query_file:
                    query_file = qt_query[1:-1]  # Use full URI if basename is empty
            # Extract qt:data as data_file (relative to srcdir)
            qt_data = next(
                (
                    t["o_full"]
                    for t in action_triples
                    if t["p"] == f"<{Namespaces.QT}data>"
                ),
                None,
            )
            if qt_data and qt_data.startswith("<") and qt_data.endswith(">"):
                data_file = os.path.basename(qt_data[1:-1])
                if not data_file:
                    data_file = qt_data[1:-1]
    # For generic tests, extract qt:query as query_file (handles DAWG-style manifests)
    if test_category == "generic":
        qt_query = next(
            (
                t["o_full"]
                for t in action_triples
                if t["p"] == f"<{Namespaces.QT}query>"
            ),
            None,
        )
        if qt_query and qt_query.startswith("<") and qt_query.endswith(">"):
            query_file = os.path.basename(qt_query[1:-1])
            if not query_file:
                query_file = qt_query[1:-1]  # Use full URI if basename is empty
        # For generic tests, also try to extract from mf:action if it's a URI (fallback)
        elif action_node.startswith("<") and action_node.endswith(">"):
            query_file = os.path.basename(action_node[1:-1])
        action = query_file if query_file else action_value
    # For sparql tests, set query_file to the filename from mf:action if it's a URI
    elif (
        test_category == "sparql"
        and action_node.startswith("<")
        and action_node.endswith(">")
    ):
        query_file = os.path.basename(action_node[1:-1])
        action = action_value
    else:
        action = action_value
    return query_file, data_file, action


def _process_manifest_entries(
    manifest_parser: ManifestParser,
    manifest_node_uri: str,
    args: argparse.Namespace,
    logger: logging.Logger,
) -> list:
    """Process manifest entries and return list of test information."""
    tests = []

    # Use the new iteration method
    for entry_node_full in manifest_parser.iter_manifest_entries(manifest_node_uri):
        entry_triples = manifest_parser.triples_by_subject.get(entry_node_full, [])

        # Extract test ID
        test_id_triple = next(
            (t for t in entry_triples if t["p"] == f"<{Namespaces.MF}name>"), None
        )
        if not test_id_triple:
            continue
        test_id = decode_literal(test_id_triple["o_full"])

        # Determine test type based on manifest
        test_type = args.test_type  # Default
        type_triple = next(
            (t for t in entry_triples if t["p"] == f"<{Namespaces.RDF}type>"), None
        )
        if type_triple:
            test_type = _determine_test_type(
                type_triple["o_full"], args.suite_name, args.test_type
            )

        query_file = None
        data_file = None
        action = None
        action_node = next(
            (
                t["o_full"]
                for t in entry_triples
                if t["p"] == f"<{Namespaces.MF}action>"
            ),
            None,
        )
        if action_node:
            action_triples = manifest_parser.triples_by_subject.get(action_node, [])
            query_file, data_file, action = _extract_query_file_and_action(
                action_node,
                action_triples,
                args.test_category,
                args.srcdir,
                args.builddir,
            )
            tests.append(
                {
                    "id": test_id,
                    "query_file": query_file or "",
                    "data_file": data_file or "",
                    "test_type": test_type,
                    "action": action,
                }
            )
        else:
            tests.append(
                {
                    "id": test_id,
                    "query_file": query_file or "",
                    "data_file": data_file or "",
                    "test_type": test_type,
                }
            )

    return tests


def _process_test_ids(test_ids: list, test_category: str) -> list:
    """Process test IDs when no manifest file is provided."""
    tests = []
    for test_id in test_ids:
        tests.append({"id": test_id, "query_file": test_id})
    return tests


def _compute_executable_paths(tests: list, args: argparse.Namespace) -> list:
    """Compute absolute executable paths for each test based on test category."""
    for test_info in tests:
        if args.test_category == "engine":
            # For engine tests, use the build directory path to the test binary
            abs_executable = (
                str((args.builddir / "tests" / "engine" / test_info["id"]).resolve())
                if args.builddir
                else test_info["id"]
            )
            test_info["abs_executable"] = abs_executable
        else:
            test_info["abs_executable"] = ""
    return tests


def _print_turtle_header(test_category: str, suite_name: str):
    """Print the Turtle header for the test plan."""
    print(
        f"""@prefix rdfs:  <http://www.w3.org/2000/01/rdf-schema#> .
@prefix mf:    <http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#> .
@prefix qt:    <http://www.w3.org/2001/sw/DataAccess/tests/test-query#> .
@prefix t:     <http://ns.librdf.org/2009/test-manifest#> .

<> a mf:Manifest ;
    rdfs:comment "{test_category.upper()} {suite_name} tests" ;
    mf:entries (
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
            abs_executable=test_info.get("abs_executable", ""),
            test_id=_escape_shell_argument(test_id),
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
        comment_raw = args.comment_template.format(
            suite_name=args.suite_name, test_id=test_id, query_file=query_file
        )
        escaped_test_id_for_print = _escape_turtle_literal(test_id)
        escaped_action_for_print = _escape_turtle_literal(action_raw)
        escaped_comment_for_print = _escape_turtle_literal(comment_raw)
        print(
            f'''  [ a t:{test_type};
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

    # Compute executable paths
    tests = _compute_executable_paths(tests, args)

    # Generate output
    _print_turtle_header(args.test_category, args.suite_name)
    _print_test_entries(tests, args)


if __name__ == "__main__":
    main()

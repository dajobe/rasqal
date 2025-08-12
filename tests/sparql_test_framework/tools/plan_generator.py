"""
Test Plan Generator

This module provides functionality for generating Turtle-formatted SPARQL test plan files
from manifest files or test ID lists.

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
import shlex
import sys
from pathlib import Path
from typing import Dict, List, Optional
import os

from ..manifest import ManifestParser, ManifestParsingError, UtilityNotFoundError
from ..test_types import Namespaces, TestTypeResolver, TestResult
from ..utils import find_tool, decode_literal, escape_turtle_literal, setup_logging
from ..config import (
    get_available_test_suites,
    validate_test_suite_name,
    get_comment_template,
)


class PlanGenerator:
    """
    Generates Turtle-formatted SPARQL test plan files from manifest files or test ID lists.
    """

    def __init__(self, debug: bool = False):
        """
        Initialize the plan generator.

        Args:
            debug: Enable debug logging
        """
        self.logger = setup_logging(debug=debug)

    def generate_plan_from_manifest(
        self,
        manifest_file: Path,
        suite_name: str,
        srcdir: Path,
        builddir: Path,
        action_template: str,
        test_type: str = "PositiveTest",
        test_category: str = "sparql",
        comment_template: Optional[str] = None,
        test_data_file_map: Optional[str] = None,
        path: Optional[str] = None,
    ) -> str:
        """
        Generate a test plan from a manifest file.

        Args:
            manifest_file: Path to the manifest file
            suite_name: Name of the test suite
            srcdir: Source directory
            builddir: Build directory
            action_template: Template for the mf:action
            test_type: Type of test (e.g., 'PositiveTest', 'NegativeTest', 'XFailTest')
            test_category: Category of tests ('sparql', 'engine', 'generic')
            comment_template: Template for rdfs:comment
            test_data_file_map: Comma-separated list of test_id:data_file pairs
            path: Path to prepend to PATH environment variable

        Returns:
            Generated Turtle content as string
        """
        # Parse test-data-file-map into a dictionary if provided
        test_data_file_map_dict = {}
        if test_data_file_map:
            for pair in test_data_file_map.split(","):
                if ":" in pair:
                    test_id, data_file = pair.split(":", 1)
                    test_data_file_map_dict[test_id.strip()] = data_file.strip()

        # Create manifest parser
        try:
            manifest_parser = ManifestParser.from_manifest_file(
                manifest_file, srcdir, self.logger
            )
            manifest_node_uri = manifest_parser.find_manifest_node()
            self.logger.debug(f"Found manifest node: {manifest_node_uri}")
        except (FileNotFoundError, UtilityNotFoundError, RuntimeError) as e:
            raise RuntimeError(f"Error creating manifest parser: {e}")
        except ManifestParsingError as e:
            raise RuntimeError(f"Could not find manifest node: {e}")

        # Process manifest entries
        tests = self._process_manifest_entries(
            manifest_parser,
            manifest_node_uri,
            suite_name,
            test_type,
            test_category,
            srcdir,
        )

        # Generate output
        return self._generate_turtle_output(
            tests,
            test_category,
            suite_name,
            srcdir,
            builddir,
            action_template,
            comment_template,
            test_data_file_map_dict,
            path,
        )

    def generate_plan_from_test_ids(
        self,
        test_ids: List[str],
        suite_name: str,
        srcdir: Path,
        builddir: Path,
        action_template: str,
        test_type: str = "PositiveTest",
        test_category: str = "sparql",
        comment_template: Optional[str] = None,
        path: Optional[str] = None,
    ) -> str:
        """
        Generate a test plan from a list of test IDs.

        Args:
            test_ids: List of test identifiers
            suite_name: Name of the test suite
            srcdir: Source directory
            builddir: Build directory
            action_template: Template for the mf:action
            test_type: Type of test
            test_category: Category of tests
            comment_template: Template for rdfs:comment
            path: Path to prepend to PATH environment variable

        Returns:
            Generated Turtle content as string
        """
        tests = []
        for test_id in test_ids:
            tests.append({"id": test_id, "query_file": test_id})

        return self._generate_turtle_output(
            tests,
            test_category,
            suite_name,
            srcdir,
            builddir,
            action_template,
            comment_template,
            {},
            path,
        )

    def _process_manifest_entries(
        self,
        manifest_parser: ManifestParser,
        manifest_node_uri: str,
        suite_name: str,
        test_type: str,
        test_category: str,
        srcdir: Path,
    ) -> List[Dict[str, str]]:
        """Process manifest entries using the existing get_tests() method."""
        # For engine tests, use a simpler approach since they don't follow W3C format
        if test_category == "engine":
            return self._process_engine_manifest_entries(
                manifest_parser, manifest_node_uri, test_type
            )

        # Use the existing get_tests() method to get TestConfig objects
        test_configs = manifest_parser.get_tests(srcdir)

        # Get test suite configuration to check if it supports negative tests
        from ..config import get_test_suite_config

        suite_config = get_test_suite_config(suite_name)

        tests = []
        for config in test_configs:
            test_id = config.name

            # Determine test type using the existing logic
            resolved_test_type = test_type  # Default
            if config.test_type:
                resolved_test_type = TestTypeResolver.classify_test_type(
                    config.test_type, suite_name, test_type
                )

            # Skip negative tests if the test suite doesn't support them
            if not suite_config.supports_negative_tests:
                # Check if this is a negative test by looking at the test type
                should_run, expected_result, _ = TestTypeResolver.resolve_test_behavior(
                    resolved_test_type
                )
                if expected_result == TestResult.FAILED:
                    self.logger.debug(
                        f"Skipping negative test '{test_id}' for suite '{suite_name}'"
                    )
                    continue

            # Convert URI to prefixed local name for Turtle output
            resolved_test_type = Namespaces.uri_to_prefixed_name(resolved_test_type)

            # Extract query file and data file
            query_file = None
            data_file = None

            if config.test_file:
                # Handle both relative and absolute paths
                try:
                    query_file = str(config.test_file.relative_to(srcdir))
                except ValueError:
                    # If the file is not relative to srcdir, use the filename
                    query_file = config.test_file.name

                # Handle data files similarly
                data_file = None
                if config.data_files:
                    try:
                        data_file = str(config.data_files[0].relative_to(srcdir))
                    except ValueError:
                        data_file = config.data_files[0].name

            tests.append(
                {
                    "id": test_id,
                    "query_file": query_file or "",
                    "data_file": data_file or "",
                    "test_type": resolved_test_type,
                }
            )

        return tests

    def _process_engine_manifest_entries(
        self,
        manifest_parser: ManifestParser,
        manifest_node_uri: str,
        test_type: str,
    ) -> List[Dict[str, str]]:
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
            self.logger.warning("Could not find mf:entries list in engine manifest")
            return tests

        # Traverse list and extract each entry
        current_list_item_node = entries_list_head
        while (
            current_list_item_node
            and current_list_item_node != f"<{Namespaces.RDF}nil>"
        ):
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
                entry_triples = manifest_parser.triples_by_subject.get(
                    entry_node_full, []
                )

                # Extract test name
                test_name = None
                for t in entry_triples:
                    if t["p"] == f"<{Namespaces.MF}name>":
                        test_name = decode_literal(t["o_full"])
                        break

                if test_name:
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

    def _generate_turtle_output(
        self,
        tests: List[Dict[str, str]],
        test_category: str,
        suite_name: str,
        srcdir: Path,
        builddir: Path,
        action_template: str,
        comment_template: Optional[str],
        test_data_file_map_dict: Dict[str, str],
        path: Optional[str],
    ) -> str:
        """Generate the complete Turtle output."""
        output_lines = []

        # --- Collect all prefixes needed ---
        from ..test_types import Namespaces

        used_prefixes = set(["rdfs", "mf", "qt", "t"])  # always present
        test_type_qnames = []
        for test_info in tests:
            test_type_uri = test_info.get("test_type", "PositiveTest")
            # Convert to valid prefixed name if possible
            test_type_qname = Namespaces.uri_to_prefixed_name(test_type_uri)
            test_type_qnames.append(test_type_qname)
            if ":" in test_type_qname:
                prefix = test_type_qname.split(":", 1)[0]
                used_prefixes.add(prefix)

        # --- Emit prefix declarations ---
        for prefix in sorted(used_prefixes):
            ns_uri = None
            # Use Namespaces._NAMESPACE_PREFIXES for mapping
            for uri, pfx in Namespaces._NAMESPACE_PREFIXES.items():
                if pfx == prefix:
                    ns_uri = uri
                    break
            if ns_uri:
                output_lines.append(f"@prefix {prefix}:  <{ns_uri}> .")

        path_line = f'    t:path """{path}""" ;\n' if path else ""
        output_lines.append(
            f"""
<> a mf:Manifest ;
    rdfs:comment \"{test_category.upper()} {suite_name} tests\" ;
{path_line}    mf:entries ("""
        )

        # Test entries
        if tests:
            abs_srcdir = str(srcdir.resolve()) if srcdir else ""
            abs_builddir = str(builddir.resolve()) if builddir else ""

            for i, test_info in enumerate(tests):
                test_id = test_info["id"].strip()
                if not test_id:
                    continue
                query_file = test_info.get("query_file", "")
                data_file = test_info.get("data_file", "")
                query_file_full = str(srcdir / query_file) if query_file else ""
                data_file_full = str(srcdir / data_file) if data_file else ""
                test_type_uri = test_info.get("test_type", "PositiveTest")
                test_type_qname = Namespaces.uri_to_prefixed_name(test_type_uri)
                data_file_override = test_data_file_map_dict.get(test_id, "")
                if data_file_override:
                    data_file = data_file_override
                    data_file_full = str(srcdir / data_file) if data_file else ""
                template_vars = dict(
                    srcdir=str(srcdir).strip(),
                    builddir=str(builddir).strip(),
                    abs_srcdir=abs_srcdir.strip(),
                    abs_builddir=abs_builddir.strip(),
                    test_id=shlex.quote(test_id),
                    query_file=test_info.get("query_file", ""),
                    data_file=data_file,
                    query_file_full=query_file_full,
                    data_file_full=data_file_full,
                )
                if "action_raw" in test_info:
                    action_raw = test_info["action_raw"]
                else:
                    action_raw = action_template.format(**template_vars)
                    # Normalize the action by removing newlines and extra whitespace
                    action_raw = " ".join(action_raw.split())
                comment_template_func = get_comment_template(
                    suite_name, comment_template
                )
                comment_raw = comment_template_func.format(
                    suite_name=suite_name, test_id=test_id, query_file=query_file
                )
                escaped_test_id_for_print = escape_turtle_literal(test_id)
                escaped_action_for_print = escape_turtle_literal(action_raw)
                escaped_comment_for_print = escape_turtle_literal(comment_raw)
                output_lines.append(
                    f'''  [ a {test_type_qname};\n    mf:name """{escaped_test_id_for_print}""";\n    rdfs:comment """{escaped_comment_for_print}""";\n    mf:action """{escaped_action_for_print}""" ]'''
                )

        output_lines.append("    ) .")
        return "\n".join(output_lines)


def main():
    """Main function - CLI entry point for plan generation."""
    parser = argparse.ArgumentParser(
        description="Generate a Turtle-formatted SPARQL test plan file or extract file lists from a manifest.",
        formatter_class=argparse.RawTextHelpFormatter,
    )
    parser.add_argument(
        "-d", "--debug", action="store_true", help="Enable debug logging"
    )

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

    # Check for RASQAL_COMPARE_ENABLE environment variable
    if os.environ.get("RASQAL_COMPARE_ENABLE", "").lower() == "yes":
        # Automatically append --use-rasqal-compare to action template
        if (
            "run-sparql-tests" in args.action_template
            and "--use-rasqal-compare" not in args.action_template
        ):
            args.action_template += " --use-rasqal-compare"
            print(
                "WARNING: RASQAL_COMPARE_ENABLE=yes: automatically adding --use-rasqal-compare to action template",
                file=sys.stderr,
            )

    # Validate required arguments
    if not all([args.suite_name, args.srcdir, args.builddir, args.action_template]):
        parser.error(
            "--suite-name, --srcdir, --builddir, and --action-template are required for plan generation."
        )

    # Validate suite name
    if args.suite_name and not validate_test_suite_name(args.suite_name):
        available_suites = get_available_test_suites()
        parser.error(
            f"Unknown test suite '{args.suite_name}'. Available suites: {', '.join(available_suites)}"
        )

    # Create plan generator
    generator = PlanGenerator(debug=args.debug)

    try:
        # Generate plan based on input method
        if args.manifest_file:
            output = generator.generate_plan_from_manifest(
                manifest_file=args.manifest_file,
                suite_name=args.suite_name,
                srcdir=args.srcdir,
                builddir=args.builddir,
                action_template=args.action_template,
                test_type=args.test_type,
                test_category=args.test_category,
                comment_template=args.comment_template,
                test_data_file_map=args.test_data_file_map,
                path=args.path,
            )
        else:
            if not args.TEST_IDS:
                parser.error(
                    "Either --manifest-file or TEST_IDS must be provided for plan generation."
                )
            output = generator.generate_plan_from_test_ids(
                test_ids=args.TEST_IDS,
                suite_name=args.suite_name,
                srcdir=args.srcdir,
                builddir=args.builddir,
                action_template=args.action_template,
                test_type=args.test_type,
                test_category=args.test_category,
                comment_template=args.comment_template,
                path=args.path,
            )

        # Print output
        print(output)

    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()

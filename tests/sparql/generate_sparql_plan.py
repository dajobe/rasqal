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
from pathlib import Path

# Add the parent directory to the Python path to find rasqal_test_util
sys.path.append(str(Path(__file__).resolve().parent.parent))

from rasqal_test_util import ManifestParser, find_tool, Namespaces, decode_literal

def _escape_turtle_literal(s: str) -> str:
    """Escapes a string for use as a triple-quoted Turtle literal.
    Handles backslashes, triple double quotes, and control characters (\n, \r, \t).
    """
    # Escape backslashes first, then triple quotes
    s = s.replace('\\', '\\\\')
    s = s.replace('"""', '\"""')
    if s.endswith('"'): # avoid """{escaped ending with "}"""
        s = s[:-1] + '\\"'
    # Escape newlines, carriage returns, and tabs
    s = s.replace('\n', '\\n')
    s = s.replace('\r', '\\r')
    s = s.replace('\t', '\\t')
    return s


def _escape_shell_argument(s: str) -> str:
    """Escapes a string for use as a shell argument.
    Handles spaces, quotes, and special shell characters.
    """
    import shlex
    return shlex.quote(s)


def main():

    parser = argparse.ArgumentParser(
        description="Generate a Turtle-formatted SPARQL test plan file or extract file lists from a manifest.",
        formatter_class=argparse.RawTextHelpFormatter,
    )
    parser.add_argument(
        "--suite-name",
        help="The name of the test suite (e.g., 'sparql-query', 'sparql-lexer'). Required for plan generation."
    )
    parser.add_argument(
        "--srcdir",
        type=Path,
        help="The source directory (equivalent to $(srcdir) in Makefile.am). Required for plan generation and --copy-to-distdir."
    )
    parser.add_argument(
        "--builddir",
        type=Path,
        help="The build directory (equivalent to $(top_builddir) in Makefile.am). Required for plan generation."
    )
    parser.add_argument(
        "--test-type",
        default="PositiveTest",
        help="The type of test (e.g., 'PositiveTest', 'NegativeTest', 'XFailTest'). Required for plan generation."
    )
    parser.add_argument(
        "--test-category",
        choices=["sparql", "laqrs", "engine", "generic"],
        default="sparql",
        help="The category of tests to generate. 'sparql' for SPARQL tests, 'laqrs' for LAQRS tests, 'engine' for C executable tests, 'generic' for custom tests."
    )
    parser.add_argument(
        "--action-template",
        help="A template string for the mf:action. Placeholders: {srcdir}, {builddir}, {test_id}, {query_file}. Required for plan generation."
    )
    parser.add_argument(
        "--comment-template",
        default="sparql {suite_name} of {test_id}",
        help="A template string for the rdfs:comment. Placeholders: {suite_name}, {test_id}, {query_file}."
    )
    parser.add_argument(
        "--manifest-file",
        type=Path,
        help="Path to a manifest file (e.g., manifest.n3) to extract test IDs from or to generate file lists."
    )
    parser.add_argument(
        "TEST_IDS",
        nargs='*',
        help="List of test identifiers (e.g., dawg-triple-pattern-001, dawg-tp-01.rq). Used if --manifest-file is not provided."
    )

    args = parser.parse_args()

    # Logic for generating test plan (original functionality)
    if args.test_category == "generic":
        if not all([args.suite_name, args.srcdir, args.builddir, args.action_template]):
            parser.error("For generic test generation, --suite-name, --srcdir, --builddir, and --action-template are required.")
    elif args.test_category in ["sparql", "laqrs", "engine"]:
        if not all([args.suite_name, args.srcdir, args.builddir]):
            parser.error("For {args.test_category} test generation, --suite-name, --srcdir, and --builddir are required.")

    tests = []
    if args.manifest_file:
        if not args.manifest_file.exists():
            print(f"Error: Manifest file not found at {args.manifest_file}", file=sys.stderr)
            sys.exit(1)
        to_ntriples_cmd = find_tool('to-ntriples')
        if not to_ntriples_cmd:
            sys.exit(1)
        try:
            # Parse the ORIGINAL manifest file
            manifest_parser = ManifestParser(args.manifest_file, to_ntriples_cmd)
            
            # Find manifest URI
            manifest_node_uri = None
            for s, triples in manifest_parser.triples_by_subject.items():
                if any(t['p'] == f"<{Namespaces.RDF}type>" and t['o_full'] == f"<{Namespaces.MF}Manifest>" for t in triples):
                    manifest_node_uri = s
                    break
            
            if not manifest_node_uri:
                print("Error: Could not find mf:Manifest entry.", file=sys.stderr)
                sys.exit(1)

            # Find entries list head
            entries_list_head = next((t['o_full'] for t in manifest_parser.triples_by_subject.get(manifest_node_uri, []) if t['p'] == f"<{Namespaces.MF}entries>"), None)

            if not entries_list_head:
                print("Error: Could not find mf:entries list.", file=sys.stderr)
                sys.exit(1)

            # Traverse list
            current_list_item_node = entries_list_head
            while current_list_item_node and current_list_item_node != f"<{Namespaces.RDF}nil>":
                list_node_triples = manifest_parser.triples_by_subject.get(current_list_item_node, [])
                entry_node_full = next((t['o_full'] for t in list_node_triples if t['p'] == f"<{Namespaces.RDF}first>"), None)

                if entry_node_full:
                    entry_triples = manifest_parser.triples_by_subject.get(entry_node_full, [])
                    
                    test_id = decode_literal(next((t['o_full'] for t in entry_triples if t['p'] == f"<{Namespaces.MF}name>"), '""'))
                    
                    # Determine test type based on rdf:type
                    test_type = args.test_type  # default
                    type_triple = next((t for t in entry_triples if t['p'] == f"<{Namespaces.RDF}type>"), None)
                    if type_triple:
                        type_uri = type_triple['o_full']
                        if 'TestBadSyntax' in type_uri or 'TestNegativeSyntax' in type_uri:
                            # Negative syntax tests should be negative for all suites
                            # that can execute queries (parser, query)
                            if args.suite_name in ['sparql-parser', 'sparql-query']:
                                test_type = 'NegativeTest'
                            else:
                                test_type = 'PositiveTest'  # Skip negative tests for lexer
                        elif 'XFailTest' in type_uri:
                            # Expected failure tests should be marked as XFailTest for all suites
                            test_type = 'XFailTest'
                        elif 'TestSyntax' in type_uri:
                            test_type = 'PositiveTest'
                    
                    query_file = None
                    action_node = next((t['o_full'] for t in entry_triples if t['p'] == f"<{Namespaces.MF}action>"), None)
                    if action_node:
                        # Handle both formats:
                        # 1. mf:action <file.rq> (direct file reference)
                        # 2. mf:action [ qt:query <file://path/to/file.rq> ] (complex action)
                        
                        # First, try direct file reference
                        if action_node.startswith('<') and action_node.endswith('>'):
                            uri_string = action_node[1:-1]
                            if uri_string.startswith('file://'):
                                # Handle file:// URIs
                                path_part = uri_string[len('file://'):]
                                query_file = Path(path_part).name
                            elif not uri_string.startswith('http://') and not uri_string.startswith('https://'):
                                # Assume it's a relative file reference
                                query_file = Path(uri_string).name
                        
                        # If not found, try complex action format
                        if not query_file:
                            action_triples = manifest_parser.triples_by_subject.get(action_node, [])
                            query_triple = next((t for t in action_triples if t['p'] == f"<{Namespaces.QT}query>"), None)
                            if query_triple:
                                raw_uri = query_triple['o_full']
                                if raw_uri.startswith('<') and raw_uri.endswith('>'):
                                    uri_string = raw_uri[1:-1]
                                    if uri_string.startswith('file://'):
                                        path_part = uri_string[len('file://'):]
                                        query_file = Path(path_part).name
                    
                    if test_id:
                        tests.append({'id': test_id, 'query_file': query_file or '', 'test_type': test_type})

                current_list_item_node = next((t['o_full'] for t in list_node_triples if t['p'] == f"<{Namespaces.RDF}rest>"), None)

        except RuntimeError as e:
            print(f"Error parsing manifest: {e}", file=sys.stderr)
            sys.exit(1)
    else:
        if not args.TEST_IDS:
            parser.error("Either --manifest-file or TEST_IDS must be provided for plan generation.")
        # Handle TEST_IDS - set query_file to test_id for LAQRS and similar test types
        for test_id in args.TEST_IDS:
            tests.append({'id': test_id, 'query_file': test_id})

    abs_srcdir = str(args.srcdir.resolve()) if args.srcdir else ''
    abs_builddir = str(args.builddir.resolve()) if args.builddir else ''

    # For engine/laqrs, compute abs_executable for each test
    for test_info in tests:
        if args.test_category == "engine":
            abs_executable = str((args.srcdir / test_info['id']).resolve()) if args.srcdir else test_info['id']
            test_info['abs_executable'] = abs_executable
        elif args.test_category == "laqrs":
            abs_executable = str((args.builddir / 'src' / 'sparql_parser_test').resolve()) if args.builddir else 'sparql_parser_test'
            test_info['abs_executable'] = abs_executable
        else:
            test_info['abs_executable'] = ''

    # Generate action template based on test category
    if args.test_category == "generic":
        action_template = args.action_template
        comment_template = args.comment_template
    elif args.test_category == "sparql":
        # Use provided action_template if available, otherwise default
        if args.action_template:
            action_template = args.action_template
            comment_template = args.comment_template if args.comment_template else "SPARQL {suite_name} {test_id}"
        else:
            action_template = "$(PYTHON3) $(srcdir)/../check_sparql.py -s $(srcdir) {test_id}"
            comment_template = "SPARQL {suite_name} {test_id}"
    elif args.test_category == "laqrs":
        action_template = "{abs_executable} -i laqrs {query_file}"
        comment_template = "laqrs parsing of {query_file}"
    elif args.test_category == "engine":
        action_template = "{abs_executable} ../../data/"
        comment_template = "rdql query {test_id}"
        for test_info in tests:
            if test_info['id'] == 'rasqal_limit_test':
                test_info['action_template'] = f"{{abs_executable}} ../../data/letters.nt"
            else:
                test_info['action_template'] = action_template
    else:
        # Default for algebra or unknown: use check_algebra.py
        action_template = "$(PYTHON3) $(srcdir)/check_algebra.py {query_file} $(BASE_URI){query_file}"
        comment_template = "SPARQL algebra {query_file}"

    # Print Turtle header
    print(f'''@prefix rdfs:  <http://www.w3.org/2000/01/rdf-schema#> .
@prefix mf:    <http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#> .
@prefix qt:    <http://www.w3.org/2001/sw/DataAccess/tests/test-query#> .
@prefix t:     <http://ns.librdf.org/2009/test-manifest#> .

<> a mf:Manifest ;
    rdfs:comment "{args.test_category.upper()} {args.suite_name} tests" ;
    mf:entries (
''')

    if tests:
        for i, test_info in enumerate(tests):
            test_id = test_info['id'].strip()
            if not test_id:
                continue
            query_file = test_info['query_file']
            test_type = test_info.get('test_type', args.test_type)
            template_vars = dict(
                srcdir=args.srcdir,
                builddir=args.builddir,
                abs_srcdir=abs_srcdir,
                abs_builddir=abs_builddir,
                abs_executable=test_info.get('abs_executable', ''),
                test_id=_escape_shell_argument(test_id),
                query_file=query_file
            )
            if args.test_category == "engine" and 'action_template' in test_info:
                action_raw = test_info['action_template'].format(**template_vars)
            else:
                action_raw = action_template.format(**template_vars)
            comment_raw = comment_template.format(
                suite_name=args.suite_name,
                test_id=test_id,
                query_file=query_file
            )
            escaped_test_id_for_print = _escape_turtle_literal(test_id)
            escaped_action_for_print = _escape_turtle_literal(action_raw)
            escaped_comment_for_print = _escape_turtle_literal(comment_raw)
            print(f'''  [ a t:{test_type};
    mf:name """{escaped_test_id_for_print}""";
    rdfs:comment """{escaped_comment_for_print}""";
    mf:action """{escaped_action_for_print}""" ]''')
        print('    ) .')
    else:
        print('    ) .')

if __name__ == "__main__":
    main()

#!/usr/bin/env python3
#
# check_algebra.py - Run Rasqal against W3C DAWG SPARQL testsuite
#
# USAGE: check_algebra.py [options] [TEST] [BASE-URI]
# 
# Copyright (C) 2009, David Beckett http://www.dajobe.org/
# 
# This package is Free Software and part of Redland http://librdf.org/
# 
# It is licensed under the following three licenses as alternatives:
#   1. GNU Lesser General Public License (LGPL) V2.1 or any newer version
#   2. GNU General Public License (GPL) V2 or any newer version
#   3. Apache License, V2.0 or any newer version
# 
# You may not use this file except in compliance with at least one of
# the above three licenses.
# 
# See LICENSE.html or LICENSE.txt at the top of this package for the
# complete terms and further detail along with the license texts for
# the licenses in COPYING.LIB, COPYING and LICENSE-2.0.txt respectively.
# 
# Requires:
#   convert_graph_pattern utility in the PATH
# 

import os
import sys
import subprocess
import argparse
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(
        description='Run SPARQL algebra graph pattern test',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s test-01.rq http://www.w3.org/TR/2008/REC-rdf-sparql-query-20080115/#
        """
    )
    parser.add_argument('--debug', '-d', action='store_true',
                       help='Enable extra debugging output')
    parser.add_argument('test', help='Test file (.rq)')
    parser.add_argument('base_uri', help='Base URI for the test')
    
    args = parser.parse_args()
    
    # Get environment variables
    diff_cmd = os.environ.get('DIFF', 'diff')
    debug = args.debug or 'RASQAL_DEBUG' in os.environ
    
    # Setup file paths
    convert_graph_pattern = "convert_graph_pattern"
    out_file = "convert.out"
    err_file = "convert.err"
    diff_file = "diff.out"
    
    program = Path(sys.argv[0]).name
    
    # Validate arguments
    if not os.path.exists(args.test):
        print(f"{program}: Test file '{args.test}' does not exist", file=sys.stderr)
        sys.exit(1)
    
    # Setup paths
    test_path = Path(args.test)
    base_name = test_path.stem  # removes .rq extension
    srcdir = test_path.parent
    expected_file = srcdir / f"{base_name}.out"
    
    if debug:
        print(f"{program}: Running test {args.test} with base URI {args.base_uri}", file=sys.stderr)
    
    # Run convert_graph_pattern
    cmd = [convert_graph_pattern, args.test, args.base_uri]
    if debug:
        print(f"{program}: Running '{' '.join(cmd)}'", file=sys.stderr)
    
    try:
        with open(out_file, 'w') as out_f, open(err_file, 'w') as err_f:
            result = subprocess.run(cmd, stdout=out_f, stderr=err_f)
        
        if debug:
            print(f"{program}: Result was {result.returncode}", file=sys.stderr)
        
        if result.returncode != 0:
            print(f"{program}: Graph pattern conversion '{args.test}' FAILED to execute", file=sys.stderr)
            print(f"Failing program was:", file=sys.stderr)
            print(f"  {' '.join(cmd)}", file=sys.stderr)
            with open(err_file, 'r') as f:
                print(f.read(), file=sys.stderr)
            sys.exit(1)
    
    except FileNotFoundError:
        print(f"{program}: convert_graph_pattern not found in PATH", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"{program}: Error running convert_graph_pattern: {e}", file=sys.stderr)
        sys.exit(1)
    
    # Compare output with expected
    if not expected_file.exists():
        print(f"{program}: Expected output file '{expected_file}' does not exist", file=sys.stderr)
        sys.exit(1)
    
    cmp_cmd = [diff_cmd, "-u", str(expected_file), out_file]
    if debug:
        print(f"{program}: Running '{' '.join(cmp_cmd)}'", file=sys.stderr)
    
    try:
        with open(diff_file, 'w') as diff_f:
            result = subprocess.run(cmp_cmd, stdout=diff_f, stderr=subprocess.STDOUT)
        
        if debug:
            print(f"{program}: Result was {result.returncode}", file=sys.stderr)
        
        if result.returncode != 0:
            print(f"{program}: Graph pattern conversion '{args.test}' FAILED to give correct answer", file=sys.stderr)
            print(f"Failing program was:", file=sys.stderr)
            print(f"  {' '.join(cmd)}", file=sys.stderr)
            with open(err_file, 'r') as f:
                print(f.read(), file=sys.stderr)
            print("Difference is:", file=sys.stderr)
            with open(diff_file, 'r') as f:
                print(f.read(), file=sys.stderr)
            sys.exit(1)
    
    except FileNotFoundError:
        print(f"{program}: {diff_cmd} not found in PATH", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"{program}: Error running diff: {e}", file=sys.stderr)
        sys.exit(1)
    
    # Clean up temporary files
    try:
        os.unlink(out_file)
        os.unlink(err_file)
        os.unlink(diff_file)
    except OSError:
        pass  # Ignore errors if files don't exist
    
    sys.exit(0)


if __name__ == '__main__':
    main() 
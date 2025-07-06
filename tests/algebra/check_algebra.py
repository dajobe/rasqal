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
import argparse
from pathlib import Path

# Add the parent directory to the Python path to find rasqal_test_util
sys.path.append(str(Path(__file__).resolve().parent.parent))
from rasqal_test_util import run_command, setup_logging


def main():
    parser = argparse.ArgumentParser(
        description="Run SPARQL algebra graph pattern test",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s test-01.rq http://www.w3.org/TR/2008/REC-rdf-sparql-query-20080115/#
        """,
    )
    parser.add_argument(
        "--debug", "-d", action="store_true", help="Enable extra debugging output"
    )
    parser.add_argument("test", help="Test file (.rq)")
    parser.add_argument("base_uri", help="Base URI for the test")

    args = parser.parse_args()
    logger = setup_logging(args.debug)

    diff_cmd = os.environ.get("DIFF", "diff")

    # Setup file paths
    convert_graph_pattern = "convert_graph_pattern"
    out_file = "convert.out"
    err_file = "convert.err"
    diff_file = "diff.out"

    program = Path(sys.argv[0]).name

    # Setup paths
    test_path = Path(args.test)
    base_name = test_path.stem  # removes .rq extension

    if not test_path.exists():
        logger.error(f"Test file '{args.test}' does not exist")
        sys.exit(1)
    srcdir = test_path.parent
    test_file = str(test_path)

    expected_file = srcdir / f"{base_name}.out"

    logger.debug(f"Running test {args.test} with base URI {args.base_uri}")

    # Run convert_graph_pattern
    cmd = [convert_graph_pattern, test_file, args.base_uri]
    logger.debug(f"Running '{' '.join(cmd)}'")
    try:
        with open(out_file, "w") as out_f, open(err_file, "w") as err_f:
            result = run_command(
                cmd, Path("."), f"Error running {convert_graph_pattern}"
            )
            out_f.write(result.stdout)
            err_f.write(result.stderr)
        logger.debug(f"Result was {result.returncode}")
        if result.returncode != 0:
            logger.error(f"Graph pattern conversion '{args.test}' FAILED to execute")
            logger.error(f"Failing program was: {' '.join(cmd)}")
            with open(err_file, "r") as f:
                logger.error(f.read())
            sys.exit(1)
    except FileNotFoundError:
        logger.error(f"{convert_graph_pattern} not found in PATH")
        sys.exit(1)
    except Exception as e:
        logger.error(f"Error running {convert_graph_pattern}: {e}")
        sys.exit(1)

    # Compare output with expected
    if not expected_file.exists():
        logger.error(f"Expected output file '{expected_file}' does not exist")
        sys.exit(1)

    cmp_cmd = [diff_cmd, "-u", str(expected_file), out_file]
    logger.debug(f"Running '{' '.join(cmp_cmd)}'")
    try:
        with open(diff_file, "w") as diff_f:
            result = run_command(cmp_cmd, Path("."), f"Error running diff")
            diff_f.write(result.stdout)
        logger.debug(f"Result was {result.returncode}")
        if result.returncode != 0:
            logger.error(
                f"Graph pattern conversion '{args.test}' FAILED to give correct answer"
            )
            logger.error(f"Failing program was: {' '.join(cmd)}")
            with open(err_file, "r") as f:
                logger.error(f.read())
            logger.error("Difference is:")
            with open(diff_file, "r") as f:
                logger.error(f.read())
            sys.exit(1)
    except FileNotFoundError:
        logger.error(f"{diff_cmd} not found in PATH")
        sys.exit(1)
    except Exception as e:
        logger.error(f"Error running diff: {e}")
        sys.exit(1)

    # Clean up temporary files
    try:
        os.unlink(out_file)
        os.unlink(err_file)
        os.unlink(diff_file)
    except OSError:
        pass  # Ignore errors if files don't exist

    sys.exit(0)


if __name__ == "__main__":
    main()

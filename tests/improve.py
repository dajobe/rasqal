#!/usr/bin/env python3
#
# improve - Run Rasqal test suites
#
# USAGE: improve [options] [DIRECTORY [TESTSUITE]]
#
# Copyright (C) 2009, David Beckett http://www.dajobe.org/
# Copyright (C) 2024, Jules the AI Agent
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
# REQUIRES:
#   GNU 'make' in the PATH (or where envariable MAKE is)
#   TO_NTRIPLES (Rasqal utility) for parsing generated manifests
#

import os
import sys
import subprocess
import argparse
import logging
from pathlib import Path
import re # For N-Triples parsing and other text manipulation

# Configure logging
# Default to INFO, can be changed by --debug or --verbose
logging.basicConfig(level=logging.WARNING, format='%(levelname)s: %(message)s')
logger = logging.getLogger(__name__)

CURDIR = Path.cwd()

# Constants from the Perl script
MF_NS = "http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#"
RDF_NS = "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
RDFS_NS = "http://www.w3.org/2000/01/rdf-schema#"
T_NS = "http://ns.librdf.org/2009/test-manifest#" # Test Manifest Extensions

COUNTERS = ['passed', 'failed', 'skipped', 'xfailed', 'uxpassed']
LINE_WRAP = 78
BANNER_WIDTH = LINE_WRAP - 10
INDENT_STR = '  ' # Two spaces, as in Perl script

# Global/module-level variables (will be initialized properly)
MAKE_CMD = os.environ.get('MAKE', 'make')
TO_NTRIPLES_CMD = '' # Will be set by get_to_ntriples_path

def get_to_ntriples_path():
    """Determines the path to the to-ntriples utility."""
    global TO_NTRIPLES_CMD
    env_val = os.environ.get('TO_NTRIPLES')
    if env_val:
        TO_NTRIPLES_CMD = env_val
        return

    # Try to find utils dir, similar to Perl script's get_utils_dir
    # This assumes improve.py is in tests/ and utils/ is a sibling of tests/
    # or in a parent directory that also contains utils/
    # CURDIR for this script is /app when run by the tool.
    # The script itself is at /app/tests/improve.py
    # So, utils should be at /app/utils

    # Simplified: assume it's relative to this script's dir parent's utils
    # script_dir = Path(__file__).resolve().parent # /app/tests
    # utils_dir_guess = script_dir.parent / "utils" # /app/utils

    # Using the same logic as check-sparql.py for UTILS_DIR
    # This is more robust if improve.py is called from different working directories.
    # However, for now, let's assume a simpler structure or rely on PATH.
    # The Perl script's get_utils_dir starts from CWD.

    # For now, let's assume it will be found via get_utils_dir_from_cwd
    # and if not, it must be in PATH.

    # Replicating Perl's get_utils_dir logic:
    current_search_dir = CURDIR
    utils_found_dir = None
    while current_search_dir != Path('/'):
        utils_dir_try = current_search_dir / 'utils'
        if utils_dir_try.is_dir():
            utils_found_dir = utils_dir_try
            break
        current_search_dir = current_search_dir.parent

    if utils_found_dir:
        TO_NTRIPLES_CMD = str(utils_found_dir / 'to-ntriples')
    else:
        # Fallback: assume to-ntriples is in PATH
        TO_NTRIPLES_CMD = 'to-ntriples'
        logger.info("Could not find 'utils' directory with 'to-ntriples', assuming 'to-ntriples' is in PATH.")


def main_improve():
    # Initialize TO_NTRIPLES_CMD path
    get_to_ntriples_path()

    parser = argparse.ArgumentParser(description="Run Rasqal test suites.", add_help=False)
    # Keep help generation manual to match Pod::Usage style if needed, or use argparse's
    parser.add_argument(
        "-d", "--debug", action="count", default=0,
        help="Enable extra debugging output (incremental)."
    )
    parser.add_argument(
        "-n", "--dryrun", action="store_true",
        help="Do not run tests, just show what would be done."
    )
    parser.add_argument(
        "-r", "--recursive", action="store_true",
        help="Run all testsuites below the given DIR."
    )
    parser.add_argument(
        "-v", "--verbose", action="count", default=0,
        help="Enable extra verbosity when running tests (incremental)."
    )
    parser.add_argument(
        "-h", "--help", action="store_true",
        help="Give help summary."
    )
    parser.add_argument(
        "DIR", nargs="?", default=".",
        help="Directory to find test suites (defaults to '.')."
    )
    parser.add_argument(
        "TESTSUITES", nargs="*",
        help="Optional list of specific test suites to run in DIR."
    )

    args = parser.parse_args()

    if args.help:
        # Replicate Pod::Usage output style manually or with a helper
        print("USAGE: improve [options] [DIRECTORY [TESTSUITE]]\n")
        print("Run Rasqal testsuites from a Turtle manifest in the DIR.")
        print("If TESTSUITES are not given, provides a list of known testsuites in DIR.")
        print("DIR defaults to '.' if not given.\n")
        print("Options:")
        print("  -d, --debug        Enable extra debugging output.")
        print("  -n, --dryrun       Do not run tests.")
        print("  -r, --recursive    Run all testsuites below the given DIR.")
        print("  -h, --help         Give help summary.")
        print("  -v, --verbose      Enable extra verbosity when running tests.")
        sys.exit(0)

    # Configure logging level based on debug and verbose flags
    if args.debug > 0:
        logger.setLevel(logging.DEBUG)
        # For this script, let's make verbose also imply debug for simplicity of logging control
        if args.verbose == 0 : args.verbose = args.debug
    if args.verbose > 0 and args.debug == 0: # If only verbose is set
         logger.setLevel(logging.DEBUG) # Or a custom level if needed

    logger.debug(f"Starting {sys.argv[0]} with arguments: {args}")
    logger.debug(f"Resolved TO_NTRIPLES command: {TO_NTRIPLES_CMD}")

    # Further implementation will go here
    # For now, just print what would be done
    logger.info(f"Directory: {Path(args.DIR).resolve()}")
    if args.TESTSUITES:
        logger.info(f"Specific testsuites: {args.TESTSUITES}")
    if args.recursive:
        logger.info("Recursive mode enabled.")
    if args.dryrun:
        logger.info("Dry run mode enabled.")

    # Placeholder for actual logic
    if not Path(TO_NTRIPLES_CMD).is_file() and TO_NTRIPLES_CMD == str(CURDIR / 'utils' / 'to-ntriples'): # crude check
        program_name_to_check = TO_NTRIPLES_CMD.split('/')[-1]
        # Check if it's in PATH if not found at specific location
        if shutil.which(program_name_to_check) is None: # More robust check
             logger.warning(f"'{program_name_to_check}' utility not found at '{TO_NTRIPLES_CMD}' or in PATH. Manifest parsing will likely fail.")

    base_dir = Path(args.DIR).resolve()
    dirs_to_scan = []

    if args.recursive:
        logger.info(f"Recursive scan for testsuites starting from {base_dir}")
        # Find all subdirectories
        for item in base_dir.rglob('*'):
            if item.is_dir():
                # Check if this directory might contain Makefile that can list testsuites
                if (item / "Makefile").is_file() or (item / "Makefile.am").is_file():
                    # Attempt to get testsuites from this dir. If any, add dir.
                    # This is slightly different from perl's find | grep .svn then get_testsuites for each.
                    # Perl's get_testsuites runs make, then make get-testsuites-list.
                    # We'll do the same.
                    suites = get_testsuites_from_dir(item, args)
                    if suites:
                        logger.debug(f"  Found suites {suites} in {item}")
                        dirs_to_scan.append(item)
                    else:
                        logger.debug(f"  No suites found by make get-testsuites-list in {item}")

        if not dirs_to_scan and ( (base_dir / "Makefile").is_file() or (base_dir / "Makefile.am").is_file()):
             # If recursive found nothing, but base_dir itself might be a candidate
             suites = get_testsuites_from_dir(base_dir, args)
             if suites: dirs_to_scan.append(base_dir)

    else:
        dirs_to_scan.append(base_dir)

    logger.info(f"Effective directories to scan for test suites: {dirs_to_scan}")

    total_results_summary = {counter: [] for counter in COUNTERS}
    overall_status_code = 0
    script_aborted_by_user = False # For global abort status

    for current_run_dir in dirs_to_scan:
        if script_aborted_by_user: break # Check before processing next dir

        logger.info(f"Processing directory: {current_run_dir}")
        # Determine which test suites to run in this directory
        # If args.TESTSUITES is provided, it applies ONLY if current_run_dir is the initial args.DIR
        # For recursive scans, we run all found suites in subdirs.
        suites_to_run_in_this_dir = args.TESTSUITES if current_run_dir == base_dir and args.TESTSUITES else None

        # dir_results = run_testsuites_in_dir(current_run_dir, suites_to_run_in_this_dir, INDENT_STR, args.verbose, args.dryrun)
        # if dir_results.get('status_code', 1) != 0:
        #     overall_status_code = 1
        # for counter in COUNTERS:
        #     total_results_summary[counter].extend(dir_results.get(counter, []))

        # This is where we would call run_testsuites_in_dir
        # For now, let's call prepare_testsuite_py to test that part
        known_suites_in_dir = get_testsuites_from_dir(current_run_dir, args)

        suites_to_process = []
        if suites_to_run_in_this_dir: # User specified specific suites for the main DIR
            for s_name in suites_to_run_in_this_dir:
                if s_name in known_suites_in_dir:
                    suites_to_process.append(s_name)
                else:
                    logger.warning(f"Specified testsuite '{s_name}' not found in {current_run_dir}. Known: {known_suites_in_dir}")
        else: # Run all known suites in this directory
            suites_to_process = known_suites_in_dir

        for suite_name in suites_to_process:
            logger.info(f"{INDENT_STR}Preparing testsuite: {suite_name} in {current_run_dir}")
            testsuite_info = {'dir': current_run_dir, 'name': suite_name, 'tests': []}

            prep_result = prepare_testsuite_py(testsuite_info, args)
            if prep_result['status'] == 'fail':
                logger.error(f"{INDENT_STR}Failed to prepare testsuite {suite_name}: {prep_result['details']}")
                overall_status_code = 1
                continue # Skip to next suite or dir

            # Store testsuite_info with populated tests for later execution by run_testsuite_py
            # For now, just log count.
            logger.info(f"{INDENT_STR}Successfully prepared {suite_name}, found {len(testsuite_info.get('tests',[]))} tests.")
            if args.debug > 1 and testsuite_info.get('tests'):
                for tst in testsuite_info['tests'][:2]: # Log first couple of tests
                    logger.debug(f"{INDENT_STR*2}Test: {tst.get('name')}, Action: {tst.get('action')}")

            # Actual running of the suite would happen here.

            try:
                suite_run_result = run_testsuite_py(testsuite_info, INDENT_STR, args)

                format_testsuite_result_py(sys.stdout, suite_run_result, INDENT_STR + INDENT_STR, args.verbose)

                for counter_name in COUNTERS:
                    total_results_summary[counter_name].extend(suite_run_result.get(counter_name, []))

                if suite_run_result.get('status', 'fail') == 'fail':
                    overall_status_code = 1

                if suite_run_result.get('abort_requested'):
                    logger.warning("Test run aborted by user. Exiting.")
                    break # Break from iterating suites in the current directory

            except KeyboardInterrupt: # Catch Ctrl+C during the suite run or formatting
                logger.warning(f"\nRun of suite {suite_name} aborted by user (SIGINT received).")
                overall_status_code = 1 # Mark as failure
                testsuite_info['abort_requested'] = True # Ensure outer loop can see this

            if testsuite_info.get('abort_requested'):
                 break # Break from iterating suites in the current directory

            if testsuite_info.get('abort_requested'):
                script_aborted_by_user = True # Propagate abort status
                break # Break from iterating suites in the current directory

        if script_aborted_by_user: # Check if inner loop (suites) was aborted
            break # Break from iterating directories


    if args.recursive and len(dirs_to_scan) > 0:
        print(f"\nTotal of all Testsuites ({'partial due to abort' if script_aborted_by_user else 'completed'})")
        format_testsuite_result_py(sys.stdout, total_results_summary, INDENT_STR, True)

    if script_aborted_by_user:
        logger.info("Exiting due to user abort.")
        sys.exit(130)

    sys.exit(overall_status_code)


def run_test_py(testsuite_info, test_details, args):
    """Executes a single test action and determines its outcome."""
    test_name = test_details['name']
    action_cmd = test_details['action'] # This is the command string
    expected_status = test_details['expect'] # 'pass' or 'fail'

    # Initialize test_details results
    test_details['status'] = 'fail' # Default to fail
    test_details['detail'] = ''
    test_details['log'] = ''

    # Note: PATH modification is handled by prepare_testsuite_py modifying os.environ
    # which is inherited by subprocess.run.

    if args.debug > 1:
        # For logging, show the PATH that will be used if it was specially set
        path_prefix = ""
        if testsuite_info.get('path_modified_for_suite'): # A flag we might set in prepare_testsuite
            path_prefix = f"PATH={os.environ.get('PATH','(not set)')} "
        logger.debug(f"Running test {test_name}: {path_prefix}{action_cmd}")

    # Create a slug for the log file name to avoid special characters
    name_slug = re.sub(r'\W+', '-', test_name) if test_name else "unnamed-test"
    # Ensure log file is in the test suite's directory for relative paths in action_cmd
    # or use a common temp area if actions are always absolute/PATH-resolved.
    # Perl script uses current dir, which is chdir'd to suite dir.
    # Let's ensure subprocess also runs in the suite's directory.
    suite_dir = testsuite_info['dir']
    log_file_path = suite_dir / f"{name_slug}.log"

    final_status = 'fail' # Actual outcome of the command execution

    try:
        # Using shell=True can be a security risk if action_cmd is not trusted.
        # However, the original Perl script uses system(), which is like shell=True.
        # Manifest actions are generally trusted in this context.
        # For commands with redirection/pipes, shell=True is often easier.
        # If action_cmd is a simple executable + args, shell=False is safer.
        # Given the examples (like check-sparql calls), they might be simple.
        # Let's try shell=False first by splitting the command.
        # This might break if actions in manifests rely on shell features.
        # The Perl script uses: system "$action > '$log' 2>&1"; this implies shell.

        full_cmd_for_shell = f"{action_cmd} > \"{str(log_file_path)}\" 2>&1"

        process = subprocess.run(full_cmd_for_shell, cwd=suite_dir, shell=True, text=True)
        # Note: subprocess.run waits for completion.
        # stdout/stderr are already redirected by the shell command.

        rc = process.returncode

        if rc != 0:
            # exec()ed and exited with non-0 (or shell itself had an issue)
            test_details['detail'] = f"Action '{action_cmd}' exited with code {rc}"
            final_status = 'fail'
        else:
            # exec()ed and exit 0
            final_status = 'pass'

        # Read log file content
        if log_file_path.exists():
            log_content = log_file_path.read_text()
            test_details['log'] = log_content
            if final_status == 'fail' and args.verbose > 0 and not test_details['detail']: # If no detail yet, use log
                 # Add snippet of log to detail if verbose and test failed
                log_snippet = "\n".join(log_content.splitlines()[-5:]) # Last 5 lines
                test_details['detail'] += f"\nLog tail:\n{log_snippet}"

    except FileNotFoundError: # Should not happen with shell=True if command is simple like 'perl script.pl'
        test_details['detail'] = f"Failed to execute: command not found within action '{action_cmd}' (or shell issue)."
        final_status = 'fail'
    except Exception as e: # Other errors during subprocess.run
        test_details['detail'] = f"Failed to run action '{action_cmd}': {e}"
        final_status = 'fail'
    finally:
        if log_file_path.exists():
            try:
                log_file_path.unlink()
            except OSError as e_unlink:
                logger.warning(f"Could not remove temporary log file {log_file_path}: {e_unlink}")

    # Adjust status based on expectation
    if expected_status == 'fail':
        if final_status == 'fail':
            test_details['status'] = 'xfail' # Expected failure
            test_details['detail'] = test_details.get('detail', "") + " (Test failed as expected)"
        else: # final_status == 'pass'
            test_details['status'] = 'uxpass' # Unexpected pass
            test_details['detail'] = "Test passed but was expected to fail."
    else: # expected_status == 'pass'
        test_details['status'] = final_status # 'pass' or 'fail'
        # Detail is already set if it failed. If it passed, detail remains empty or as set.

    # SIGINT handling (abort) - This is harder with subprocess.run as it blocks.
    # The Perl script checks $? & 127 for signals.
    # If a KeyboardInterrupt is caught at a higher level, we'd need to propagate an abort.
    # For now, this direct subprocess call won't easily set testsuite_info['abort'] on SIGINT
    # unless main_improve handles KeyboardInterrupt.

    return test_details['status']


def format_testsuite_result_py(fh, result_summary, indent_prefix, verbose_format):
    """Formats and prints the results of a test suite run."""
    if result_summary.get('failed'):
        fh.write(f"{indent_prefix}Failed tests:\n")
        for ftest in result_summary['failed']:
            fh.write(f"{indent_prefix}{INDENT_STR}{'=' * BANNER_WIDTH}\n" if verbose_format else "")
            name_detail = ftest.get('name', 'Unknown Test')
            if verbose_format:
                name_detail += f" in suite {ftest.get('testsuite_name', 'N/A')} in {ftest.get('dir', 'N/A')}"
            fh.write(f"{indent_prefix}{INDENT_STR}{name_detail}\n")
            if verbose_format and ftest.get('detail'):
                fh.write(f"{indent_prefix}{INDENT_STR}{ftest['detail']}\n")
            if verbose_format and ftest.get('log'):
                log_lines = ftest['log'].splitlines()
                # Show last 15 lines of log, similar to Perl
                log_to_show = log_lines
                if len(log_lines) > 15:
                    log_to_show = ["..."] + log_lines[-15:]
                indented_log = f"\n{indent_prefix}{INDENT_STR*2}".join(log_to_show)
                fh.write(f"{indent_prefix}{INDENT_STR*2}{indented_log}\n")
            fh.write(f"{indent_prefix}{INDENT_STR}{'=' * BANNER_WIDTH}\n" if verbose_format else "")

    if result_summary.get('uxpassed'):
        fh.write(f"{indent_prefix}Unexpected passed tests:\n")
        for utest in result_summary['uxpassed']:
            name_detail = utest.get('name', 'Unknown Test')
            if logger.level == logging.DEBUG: # Or a specific verbose condition
                 name_detail += f" ({utest.get('test_uri', 'N/A')})"
            fh.write(f"{indent_prefix}{INDENT_STR}{name_detail}\n")

    fh.write(indent_prefix)
    for counter_name in COUNTERS:
        count = len(result_summary.get(counter_name, []))
        fh.write(f"{counter_name.capitalize()}: {count}  ")
    fh.write("\n")


def run_testsuite_py(testsuite_info, indent_prefix, args):
    """Runs all tests in a prepared testsuite_info and returns results."""
    suite_dir = testsuite_info['dir']
    suite_name = testsuite_info['name']
    tests_to_run = testsuite_info.get('tests', [])
    suite_desc = testsuite_info.get('desc', suite_name)

    print(f"{indent_prefix}Running testsuite {suite_name}: {suite_desc}")

    results = {counter: [] for counter in COUNTERS}
    expected_failures_count = 0
    column = len(indent_prefix)

    # Prepare for potential PATH modification if t:path was set
    original_env_path = os.environ.get('PATH', None)
    path_modified_for_this_suite = False
    if testsuite_info.get('path'): # This was set in prepare_testsuite_py
        # The PATH is already modified in os.environ by prepare_testsuite_py
        # We just need to ensure it's restored if it was changed.
        path_modified_for_this_suite = True
        # Mark that this suite might have a custom PATH for run_test_py logging
        testsuite_info['path_modified_for_suite'] = True


    if not args.verbose: print(indent_prefix, end='', flush=True)

    for test_detail in tests_to_run:
        test_detail['testsuite_name'] = suite_name # For richer reporting
        test_detail['dir'] = suite_dir

        if args.dryrun:
            status = 'skipped'
            test_detail['status'] = status
            test_detail['detail'] = "(dryrun)"
        else:
            try:
                status = run_test_py(testsuite_info, test_detail, args)
            except KeyboardInterrupt: # Handle Ctrl+C during a test
                logger.warning(f"\n{indent_prefix}Test execution aborted by user (SIGINT received).")
                test_detail['status'] = 'failed' # Or a special 'aborted' status
                test_detail['detail'] = "Aborted by user."
                # Set a flag to stop further processing in this suite / all suites
                testsuite_info['abort_requested'] = True
                # This flag needs to be checked by the caller loop in main_improve

        if test_detail['expect'] == 'fail':
            expected_failures_count += 1

        results[test_detail['status']].append(test_detail)

        if args.verbose == 0:
            char_map = {'pass': '.', 'fail': 'F', 'xfail': '*', 'uxpass': '!', 'skipped': '-'}
            print(char_map.get(test_detail['status'], '?'), end='', flush=True)
            column += 1
            if column >= LINE_WRAP:
                print(f"\n{indent_prefix}", end='', flush=True)
                column = len(indent_prefix)
        else: # Verbose output per test
            status_display = test_detail['status'].upper() if test_detail['status'] != 'pass' else test_detail['status']
            detail_str = f" - {test_detail['detail']}" if test_detail.get('detail') else ""
            print(f"{indent_prefix}{INDENT_STR}{test_detail['name']}: {status_display}{detail_str}")
            if args.verbose > 1 and test_detail['status'] == 'fail' and test_detail.get('log'):
                log_lines = test_detail['log'].splitlines()
                log_to_show = ["..."] + log_lines[-15:] if len(log_lines) > 15 else log_lines
                indented_log = f"\n{indent_prefix}{INDENT_STR*2}".join(log_to_show)
                print(f"{indent_prefix}{INDENT_STR*2}{indented_log}")

        if testsuite_info.get('abort_requested'):
            if args.verbose == 0: print("aborted", end='') # Print next to dots
            print(f"\n{indent_prefix}Aborting testsuite {suite_name} due to user interrupt.")
            break # Exit this suite's test loop

    if args.verbose == 0: print() # Newline after dots

    if plan_file_path := testsuite_info.get('plan_file'):
        try:
            plan_file_path.unlink(missing_ok=True)
        except OSError as e:
            logger.warning(f"Could not remove plan file {plan_file_path}: {e}")

    # Restore original PATH if it was modified for this suite
    if path_modified_for_this_suite and original_env_path is not None:
        os.environ['PATH'] = original_env_path
        if args.debug > 0: logger.debug(f"Restored PATH to: {original_env_path}")
    elif path_modified_for_this_suite and original_env_path is None: # PATH was not set before
        del os.environ['PATH'] # Unset it
        if args.debug > 0: logger.debug("Unset PATH as it was not originally set.")
    testsuite_info.pop('path_modified_for_suite', None) # Clean up flag


    # Determine overall suite status
    suite_status = 'pass'
    if results.get('failed') or results.get('uxpassed'): # Any actual failure or unexpected pass means suite fails
        suite_status = 'fail'

    results['status'] = suite_status # Overall status of this testsuite
    results['abort_requested'] = testsuite_info.get('abort_requested', False)
    return results


def _parse_nt_line_for_improve(line):
    """Rudimentary N-Triples line parser, specific for improve.py needs."""
    # This is similar to _parse_nt_line in check-sparql.py but might have slight variations
    # if improve's manifest parsing is simpler or targets different literal forms.
    # For now, assume it's very similar.
    line = line.strip()
    if not line.endswith(" ."): return None
    line = line[:-2].strip()

    parts = []
    current_part, in_literal, in_uri, escape = "", False, False, False
    for char in line:
        if escape: current_part += char; escape = False; continue
        if char == '\\': escape = True; current_part += char; continue
        if char == '<' and not in_literal:
            if current_part.strip(): logger.warning(f"Token err: {current_part} in {line}"); return None
            in_uri = True; current_part += char
        elif char == '>' and in_uri:
            in_uri = False; current_part += char; parts.append(current_part); current_part = ""
        elif char == '"' and not in_uri:
            if not in_literal:
                if current_part.strip(): logger.warning(f"Token err: {current_part} in {line}"); return None
                in_literal = True
            else: in_literal = False
            current_part += char
        elif char == ' ' and not in_literal and not in_uri:
            if current_part: parts.append(current_part); current_part = ""
        else: current_part += char
    if current_part: parts.append(current_part)
    if len(parts) != 3: logger.debug(f"NT parse fail: '{line}', parts: {parts}"); return None

    s, p, o_full = parts
    if not ((s.startswith("<") and s.endswith(">")) or s.startswith("_:")): return None
    if not (p.startswith("<") and p.endswith(">")): return None

    # Unlike check-sparql, original improve's decode_literal is simpler.
    # We primarily need the full object string for lookups and then decode specific literals.
    # For now, let's return the full object string and subject/predicate URIs without <>
    s_uri = s[1:-1] if s.startswith("<") else s
    p_uri = p[1:-1] if p.startswith("<") else p
    return s_uri, p_uri, o_full # o_full is like <uri>, "literal"@lang, _:bnode


def decode_literal_py(lit_str):
    """Decodes a literal string as found in the improve script's N-Triples parsing."""
    if not lit_str or not lit_str.startswith('"') or not lit_str.endswith('"'):
        # It might be a URI <...> or bnode _:... if used incorrectly
        if lit_str.startswith("<") and lit_str.endswith(">"): return lit_str[1:-1]
        return lit_str

    # Remove outer quotes
    val = lit_str[1:-1]
    # Basic unescaping for quotes, as in Perl: s/\\"/"/g;
    val = val.replace('\\"', '"')
    # The perl script doesn't show other unescaping like \t, \n for this specific function.
    return val

def read_plan_py(testsuite_info, plan_file_path, args):
    """Parses the N-Triples from plan_file_path and populates testsuite_info."""
    logger.debug(f"Reading plan file: {plan_file_path}")
    to_ntriples_error_file = Path(f"{testsuite_info['name']}-to_ntriples.err") # Suite-specific temp error file

    cmd_to_nt = [TO_NTRIPLES_CMD, str(plan_file_path)]
    if args.debug > 1: logger.debug(f"Running pipe from: {' '.join(cmd_to_nt)}")

    nt_lines = []
    try:
        # Corrected subprocess call: removed capture_output=True, added stdout=subprocess.PIPE
        with open(to_ntriples_error_file, "w") as f_err:
            process = subprocess.run(cmd_to_nt, stdout=subprocess.PIPE, stderr=f_err, text=True, check=False)

        err_content = ""
        if to_ntriples_error_file.exists():
            err_content = to_ntriples_error_file.read_text().strip()
            if err_content:
                 logger.warning(f"Content from {to_ntriples_error_file} for {plan_file_path}:\n{err_content}")
            to_ntriples_error_file.unlink(missing_ok=True)

        # Check returncode and error content AFTER reading and unlinking the error file
        if process.returncode != 0 or ("Error" in err_content and "parse error" in err_content.lower()): # More specific error check
            # Construct detail message, preferring err_content if it looks like a real error
            error_detail_source = err_content if (err_content and "Error" in err_content) else process.stderr # process.stderr will be None here
            if error_detail_source is None and process.returncode !=0 : # if stderr was redirected and empty, but still error
                error_detail_source = f"Process exited with code {process.returncode}."

            logger.warning(f"{TO_NTRIPLES_CMD} for {plan_file_path} failed or reported errors. Exit code: {process.returncode}. Details: {error_detail_source}")
            return {'status': 'fail', 'details': f"{TO_NTRIPLES_CMD} failed. Detail: {error_detail_source}"}

        nt_lines = process.stdout.splitlines()

    except FileNotFoundError:
        logger.error(f"{TO_NTRIPLES_CMD} not found.")
        if to_ntriples_error_file.exists(): to_ntriples_error_file.unlink(missing_ok=True)
        return {'status': 'fail', 'details': f"{TO_NTRIPLES_CMD} not found."}
    except Exception as e:
        logger.error(f"Error running {TO_NTRIPLES_CMD} for {plan_file_path}: {e}")
        if to_ntriples_error_file.exists(): to_ntriples_error_file.unlink(missing_ok=True)
        return {'status': 'fail', 'details': str(e)}

    triples_by_subject = {}
    manifest_node_uri = None # URI of the mf:Manifest
    entries_list_head = None # Head of the mf:entries list (bnode or URI string)

    for line in nt_lines:
        parsed = _parse_nt_line_for_improve(line)
        if parsed:
            s, p, o_full = parsed
            if s not in triples_by_subject: triples_by_subject[s] = []
            triples_by_subject[s].append({'p': p, 'o_full': o_full})

            if p == RDF_NS + "type" and o_full == f"<{MF_NS}Manifest>":
                manifest_node_uri = s
            if p == MF_NS + "entries": # Assuming subject is the manifest_node_uri or related blank node
                entries_list_head = o_full


    if manifest_node_uri and manifest_node_uri in triples_by_subject:
        for triple in triples_by_subject[manifest_node_uri]:
            if triple['p'] == RDFS_NS + "comment":
                testsuite_info['desc'] = decode_literal_py(triple['o_full'])
            elif triple['p'] == T_NS + "path": # Custom t:path predicate
                testsuite_info['path'] = decode_literal_py(triple['o_full'])

    if not entries_list_head:
        # Try finding entries via manifest_node_uri if not found directly
        if manifest_node_uri:
            entries_list_head = next((t['o_full'] for t in triples_by_subject.get(manifest_node_uri, []) if t['p'] == MF_NS + "entries"), None)
        if not entries_list_head:
            logger.error(f"Could not find mf:entries list head in {plan_file_path}")
            return {'status': 'fail', 'details': 'mf:entries not found'}

    logger.debug(f"Manifest node: {manifest_node_uri}, Entries list head: {entries_list_head}")

    current_list_item_node = entries_list_head
    parsed_tests = []
    processed_list_nodes = set() # To avoid loops in malformed lists

    while current_list_item_node and current_list_item_node != f"<{RDF_NS}nil>":
        if current_list_item_node in processed_list_nodes:
            logger.error(f"Detected loop in RDF list at {current_list_item_node} in {plan_file_path}"); break
        processed_list_nodes.add(current_list_item_node)

        entry_node_full = next((t['o_full'] for t in triples_by_subject.get(current_list_item_node, []) if t['p'] == RDF_NS + "first"), None)
        if not entry_node_full:
            logger.warning(f"List node {current_list_item_node} has no rdf:first. Skipping.");
            current_list_item_node = next((t['o_full'] for t in triples_by_subject.get(current_list_item_node, []) if t['p'] == RDF_NS + "rest"), None); continue

        entry_node_uri = entry_node_full[1:-1] if entry_node_full.startswith("<") else entry_node_full # URI or BNode ID

        test_name_lit = next((t['o_full'] for t in triples_by_subject.get(entry_node_full, []) if t['p'] == MF_NS + "name"), '""')
        test_name = decode_literal_py(test_name_lit)

        test_comment_lit = next((t['o_full'] for t in triples_by_subject.get(entry_node_full, []) if t['p'] == RDFS_NS + "comment"), '""')
        test_comment = decode_literal_py(test_comment_lit)

        test_action_lit = next((t['o_full'] for t in triples_by_subject.get(entry_node_full, []) if t['p'] == MF_NS + "action"), '""')
        test_action = decode_literal_py(test_action_lit)

        entry_type_full = next((t['o_full'] for t in triples_by_subject.get(entry_node_full, []) if t['p'] == RDF_NS + "type"), "")
        entry_type_uri = entry_type_full[1:-1] if entry_type_full.startswith("<") else entry_type_full

        expect = 'pass'
        if entry_type_uri == T_NS + "NegativeTest" or entry_type_uri == T_NS + "XFailTest":
            expect = 'fail'

        parsed_tests.append({
            'name': test_name, 'comment': test_comment, 'dir': testsuite_info['dir'],
            'expect': expect, 'test_uri': entry_node_uri, # This is the subject URI/BNode ID
            'action': test_action
        })
        if args.debug > 1: logger.debug(f"  Parsed test: Name='{test_name}', Action='{test_action}', Expect='{expect}'")

        current_list_item_node = next((t['o_full'] for t in triples_by_subject.get(current_list_item_node, []) if t['p'] == RDF_NS + "rest"), None)

    testsuite_info['tests'] = parsed_tests
    return {'status': 'pass', 'details': ''}


def prepare_testsuite_py(testsuite_info, args):
    """Generates and reads the test plan for a suite."""
    dir_path = testsuite_info['dir']
    suite_name = testsuite_info['name']
    plan_file = dir_path / f"{suite_name}-plan.ttl" # Path object
    testsuite_info['plan_file'] = plan_file # Store for potential cleanup

    try:
        if plan_file.exists():
            plan_file.unlink()
    except OSError as e:
        logger.warning(f"Could not remove existing plan file {plan_file}: {e}")

    # Command: make get-testsuite-SUITE_NAME > SUITE_NAME-plan.ttl
    make_cmd_list = [MAKE_CMD, f"get-testsuite-{suite_name}"]
    logger.debug(f"Running in {dir_path}: {' '.join(make_cmd_list)} > {plan_file.name}")

    try:
        with open(plan_file, "w") as f_out:
            # First, run 'make' in the directory to ensure everything is built (like Perl script)
            # This is a bit implicit in the Perl script's system("$MAKE >/dev/null 2>&1") call per dir.
            # Let's ensure essential tools are attempted to be built if relevant make targets exist.
            # A simple `make` call in the directory might be enough.
            # However, the original does this *before* get_testsuites_from_dir.
            # For prepare_testsuite, it directly calls get-testsuite-SUITE_NAME.

            process = subprocess.run(make_cmd_list, cwd=dir_path, stdout=f_out, stderr=subprocess.PIPE, text=True)

        if process.returncode != 0:
            details = f"'{' '.join(make_cmd_list)}' failed (exit {process.returncode}) in {dir_path}."
            if process.stderr: details += f"\nStderr:\n{process.stderr.strip()}"
            return {'status': 'fail', 'details': details}

        if not plan_file.exists() or plan_file.stat().st_size == 0:
            return {'status': 'fail', 'details': f"No testsuite plan file {plan_file} created or is empty in {dir_path}."}

    except FileNotFoundError:
        return {'status': 'fail', 'details': f"'{MAKE_CMD}' not found."}
    except Exception as e:
        return {'status': 'fail', 'details': f"Error generating plan for {suite_name}: {e}"}

    # Now parse the generated plan file
    parse_result = read_plan_py(testsuite_info, plan_file, args)

    # Handle PATH modification based on t:path in manifest
    if testsuite_info.get('path'):
        # Prepend to PATH. This only affects child processes of this script.
        original_path = os.environ.get('PATH', '')
        new_path_segment = str(Path(testsuite_info['path']).resolve()) # Ensure absolute
        os.environ['PATH'] = f"{new_path_segment}{os.pathsep}{original_path}"
        if args.debug > 0: logger.debug(f"Prepended to PATH: {new_path_segment}. New PATH: {os.environ['PATH']}")
        # Note: Reverting this PATH change after the suite runs might be complex if suites run in parallel
        # or if errors occur. For sequential runs, it might be manageable.
        # The Perl script doesn't show explicit reversion, relying on per-test `system` calls inheriting env.

    return parse_result


def get_testsuites_from_dir(directory: Path, args):
    """
    Runs 'make get-testsuites-list' in the given directory and returns a list of suite names.
    Caches 'make' call for efficiency if it was already run (e.g. by recursive scan).
    """
    # The Perl script runs 'make' first, then 'make get-testsuites-list'.
    # 'make' might generate files needed by 'get-testsuites-list'.
    # We should ensure 'make' is run if not already done for this dir.

    # First, run a general 'make' in the directory, similar to Perl's: system "cd $dir && $MAKE >/dev/null 2>&1";
    # This might be necessary to generate Makefiles from Makefile.am or other prerequisites.
    logger.debug(f"Running initial '{MAKE_CMD}' in {directory} to ensure it's up-to-date.")
    try:
        make_init_process = subprocess.run([MAKE_CMD], cwd=directory, capture_output=True, text=True)
        if make_init_process.returncode != 0:
            # This might not be fatal if get-testsuites-list still works (e.g. if Makefile exists but some targets fail)
            logger.debug(f"Initial '{MAKE_CMD}' in {directory} exited with {make_init_process.returncode}. Stderr: {make_init_process.stderr.strip()}")
    except FileNotFoundError:
        logger.error(f"'{MAKE_CMD}' command not found when running initial make in {directory}.")
        return [] # Cannot proceed without make
    except Exception as e_init:
        logger.error(f"Error running initial '{MAKE_CMD}' in {directory}: {e_init}")
        return []


    make_cmd_list = [MAKE_CMD, "get-testsuites-list"]
    logger.debug(f"Running in {directory}: {' '.join(make_cmd_list)}")
    try:
        process = subprocess.run(make_cmd_list, cwd=directory, capture_output=True, text=True)
        if process.returncode != 0:
            # This is a more critical failure if get-testsuites-list itself fails
            logger.warning(f"'{' '.join(make_cmd_list)}' target failed in {directory} (exit code {process.returncode}).")
            if process.stderr: logger.debug(f"Stderr for get-testsuites-list: {process.stderr.strip()}")
            return []

        lines = process.stdout.splitlines()
        relevant_lines = [line for line in lines if "ing directory" not in line] # Simulates grep -v

        if not relevant_lines:
            logger.debug(f"No relevant output from get-testsuites-list in {directory}")
            return []

        last_line = relevant_lines[-1].strip() # Simulates tail -1
        if last_line:
            return last_line.split()
        return []

    except FileNotFoundError:
        logger.error(f"'{MAKE_CMD}' command not found. Please ensure GNU make is installed and in PATH.")
        return [] # Cannot proceed without make
    except Exception as e:
        logger.error(f"Error running '{' '.join(make_cmd_list)}' in {directory}: {e}")
        return []

# Placeholder for shutil
import shutil

if __name__ == "__main__":
    main_improve()

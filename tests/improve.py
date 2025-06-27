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
import re
import time # Added for main_improve's start_time_suite, though not used yet
import shutil # Added for shutil.which

# Configure logging
logging.basicConfig(level=logging.WARNING, format='%(levelname)s: %(message)s')
logger = logging.getLogger(__name__)

CURDIR = Path.cwd()

MF_NS = "http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#"
RDF_NS = "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
RDFS_NS = "http://www.w3.org/2000/01/rdf-schema#"
T_NS = "http://ns.librdf.org/2009/test-manifest#"

COUNTERS = ['passed', 'failed', 'skipped', 'xfailed', 'uxpassed']
LINE_WRAP = 78
BANNER_WIDTH = LINE_WRAP - 10
INDENT_STR = '  '

MAKE_CMD = os.environ.get('MAKE', 'make')
TO_NTRIPLES_CMD = ''

def get_to_ntriples_path():
    global TO_NTRIPLES_CMD
    env_val = os.environ.get('TO_NTRIPLES')
    if env_val:
        TO_NTRIPLES_CMD = env_val
        return
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
        TO_NTRIPLES_CMD = 'to-ntriples'
        logger.info("Could not find 'utils' directory with 'to-ntriples', assuming 'to-ntriples' is in PATH.")

def _parse_nt_line_for_improve(line):
    line = line.strip()
    if not line.endswith(" ."): return None
    line = line[:-2].strip()
    parts, current_part, in_literal, in_uri, escape = [], "", False, False, False
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
    s_uri = s[1:-1] if s.startswith("<") else s
    p_uri = p[1:-1] if p.startswith("<") else p
    return s_uri, p_uri, o_full

def decode_literal_py(lit_str):
    if not lit_str or not lit_str.startswith('"') or not lit_str.endswith('"'):
        if lit_str and lit_str.startswith("<") and lit_str.endswith(">"): return lit_str[1:-1]
        return lit_str
    val = lit_str[1:-1]
    val = val.replace('\\"', '"')
    return val

def read_plan_py(testsuite_info, plan_file_path, args):
    logger.debug(f"Reading plan file: {plan_file_path}")
    to_ntriples_error_file = Path(f"{testsuite_info['name']}-to_ntriples.err")
    cmd_to_nt = [TO_NTRIPLES_CMD, str(plan_file_path)]
    if args.debug > 1: logger.debug(f"Running pipe from: {' '.join(cmd_to_nt)}")
    nt_lines = []
    try:
        with open(to_ntriples_error_file, "w") as f_err:
            process = subprocess.run(cmd_to_nt, stdout=subprocess.PIPE, stderr=f_err, text=True, check=False)
        err_content = ""
        if to_ntriples_error_file.exists():
            err_content = to_ntriples_error_file.read_text().strip()
            if err_content: logger.warning(f"Content from {to_ntriples_error_file} for {plan_file_path}:\n{err_content}")
            to_ntriples_error_file.unlink(missing_ok=True)
        if process.returncode != 0 or ("Error" in err_content and "parse error" in err_content.lower()):
            error_detail_source = err_content if (err_content and "Error" in err_content) else None
            if error_detail_source is None and process.returncode !=0 : error_detail_source = f"Process exited with code {process.returncode}."
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

    triples_by_subject, manifest_node_uri, entries_list_head = {}, None, None
    for line in nt_lines:
        parsed = _parse_nt_line_for_improve(line)
        if parsed:
            s, p, o_full = parsed
            if s not in triples_by_subject: triples_by_subject[s] = []
            triples_by_subject[s].append({'p': p, 'o_full': o_full})
            if p == RDF_NS + "type" and o_full == f"<{MF_NS}Manifest>": manifest_node_uri = s
            if p == MF_NS + "entries": entries_list_head = o_full
    if manifest_node_uri and manifest_node_uri in triples_by_subject:
        for triple in triples_by_subject[manifest_node_uri]:
            if triple['p'] == RDFS_NS + "comment": testsuite_info['desc'] = decode_literal_py(triple['o_full'])
            elif triple['p'] == T_NS + "path": testsuite_info['path'] = decode_literal_py(triple['o_full'])
    if not entries_list_head:
        if manifest_node_uri: entries_list_head = next((t['o_full'] for t in triples_by_subject.get(manifest_node_uri, []) if t['p'] == MF_NS + "entries"), None)
        if not entries_list_head: logger.error(f"Could not find mf:entries list head in {plan_file_path}"); return {'status': 'fail', 'details': 'mf:entries not found'}
    logger.debug(f"Manifest node: {manifest_node_uri}, Entries list head: {entries_list_head}")
    current_list_item_node, parsed_tests, processed_list_nodes = entries_list_head, [], set()
    while current_list_item_node and current_list_item_node != f"<{RDF_NS}nil>":
        if current_list_item_node in processed_list_nodes: logger.error(f"Detected loop in RDF list at {current_list_item_node} in {plan_file_path}"); break
        processed_list_nodes.add(current_list_item_node)
        entry_node_full = next((t['o_full'] for t in triples_by_subject.get(current_list_item_node, []) if t['p'] == RDF_NS + "first"), None)
        if not entry_node_full: logger.warning(f"List node {current_list_item_node} has no rdf:first. Skipping."); current_list_item_node = next((t['o_full'] for t in triples_by_subject.get(current_list_item_node, []) if t['p'] == RDF_NS + "rest"), None); continue
        entry_node_uri = entry_node_full[1:-1] if entry_node_full.startswith("<") else entry_node_full
        test_name = decode_literal_py(next((t['o_full'] for t in triples_by_subject.get(entry_node_full, []) if t['p'] == MF_NS + "name"), '""'))
        test_comment = decode_literal_py(next((t['o_full'] for t in triples_by_subject.get(entry_node_full, []) if t['p'] == RDFS_NS + "comment"), '""'))
        test_action = decode_literal_py(next((t['o_full'] for t in triples_by_subject.get(entry_node_full, []) if t['p'] == MF_NS + "action"), '""'))
        entry_type_full = next((t['o_full'] for t in triples_by_subject.get(entry_node_full, []) if t['p'] == RDF_NS + "type"), "")
        entry_type_uri = entry_type_full[1:-1] if entry_type_full.startswith("<") else entry_type_full
        expect = 'pass'
        if entry_type_uri == T_NS + "NegativeTest" or entry_type_uri == T_NS + "XFailTest": expect = 'fail'
        parsed_tests.append({'name': test_name, 'comment': test_comment, 'dir': testsuite_info['dir'], 'expect': expect, 'test_uri': entry_node_uri, 'action': test_action})
        if args.debug > 1: logger.debug(f"  Parsed test: Name='{test_name}', Action='{test_action}', Expect='{expect}'")
        current_list_item_node = next((t['o_full'] for t in triples_by_subject.get(current_list_item_node, []) if t['p'] == RDF_NS + "rest"), None)
    testsuite_info['tests'] = parsed_tests
    return {'status': 'pass', 'details': ''}

def prepare_testsuite_py(testsuite_info, args):
    dir_path, suite_name = testsuite_info['dir'], testsuite_info['name']
    plan_file = dir_path / f"{suite_name}-plan.ttl"
    testsuite_info['plan_file'] = plan_file
    try:
        if plan_file.exists(): plan_file.unlink()
    except OSError as e: logger.warning(f"Could not remove existing plan file {plan_file}: {e}")
    make_cmd_list = [MAKE_CMD, f"get-testsuite-{suite_name}"]
    logger.debug(f"Running in {dir_path}: {' '.join(make_cmd_list)} > {plan_file.name}")
    try:
        with open(plan_file, "w") as f_out:
            process = subprocess.run(make_cmd_list, cwd=dir_path, stdout=f_out, stderr=subprocess.PIPE, text=True)
        if process.returncode != 0:
            details = f"'{' '.join(make_cmd_list)}' failed (exit {process.returncode}) in {dir_path}."
            if process.stderr: details += f"\nStderr:\n{process.stderr.strip()}"
            return {'status': 'fail', 'details': details}
        if not plan_file.exists() or plan_file.stat().st_size == 0:
            return {'status': 'fail', 'details': f"No testsuite plan file {plan_file} created or is empty in {dir_path}."}
    except FileNotFoundError: return {'status': 'fail', 'details': f"'{MAKE_CMD}' not found."}
    except Exception as e: return {'status': 'fail', 'details': f"Error generating plan for {suite_name}: {e}"}
    parse_result = read_plan_py(testsuite_info, plan_file, args)
    if testsuite_info.get('path'):
        original_path = os.environ.get('PATH', '')
        new_path_segment = str(Path(testsuite_info['path']).resolve())
        os.environ['PATH'] = f"{new_path_segment}{os.pathsep}{original_path}"
        testsuite_info['original_path_for_restore'] = original_path # Store for restoration
        if args.debug > 0: logger.debug(f"Prepended to PATH: {new_path_segment}. New PATH: {os.environ['PATH']}")
    return parse_result

def get_testsuites_from_dir(directory: Path, args):
    logger.debug(f"Running initial '{MAKE_CMD}' in {directory} to ensure it's up-to-date.")
    try:
        make_init_process = subprocess.run([MAKE_CMD], cwd=directory, capture_output=True, text=True)
        if make_init_process.returncode != 0:
            logger.debug(f"Initial '{MAKE_CMD}' in {directory} exited with {make_init_process.returncode}. Stderr: {make_init_process.stderr.strip()}")
    except FileNotFoundError: logger.error(f"'{MAKE_CMD}' command not found for initial make in {directory}."); return []
    except Exception as e_init: logger.error(f"Error running initial '{MAKE_CMD}' in {directory}: {e_init}"); return []
    make_cmd_list = [MAKE_CMD, "get-testsuites-list"]
    logger.debug(f"Running in {directory}: {' '.join(make_cmd_list)}")
    try:
        process = subprocess.run(make_cmd_list, cwd=directory, capture_output=True, text=True)
        if process.returncode != 0:
            logger.warning(f"'{' '.join(make_cmd_list)}' target failed in {directory} (exit code {process.returncode}).")
            if process.stderr: logger.debug(f"Stderr for get-testsuites-list: {process.stderr.strip()}")
            return []
        lines = process.stdout.splitlines()
        relevant_lines = [line for line in lines if "ing directory" not in line]
        if not relevant_lines: logger.debug(f"No relevant output from get-testsuites-list in {directory}"); return []
        last_line = relevant_lines[-1].strip()
        return last_line.split() if last_line else []
    except FileNotFoundError: logger.error(f"'{MAKE_CMD}' not found for get-testsuites-list."); return []
    except Exception as e: logger.error(f"Error running get-testsuites-list in {directory}: {e}"); return []

def run_test_py(testsuite_info, test_details, args):
    test_name, action_cmd, expected_status = test_details['name'], test_details['action'], test_details['expect']
    test_details.update({'status': 'failed', 'detail': '', 'log': ''})
    if args.debug > 1:
        path_prefix = f"PATH={os.environ.get('PATH','(not set)')} " if testsuite_info.get('path') else ""
        logger.debug(f"Running test {test_name}: {path_prefix}{action_cmd}")
    name_slug = re.sub(r'\W+', '-', test_name) if test_name else "unnamed-test"
    suite_dir = testsuite_info['dir']
    log_file_path = suite_dir / f"{name_slug}.log"
    final_status = 'fail'
    try:
        full_cmd_for_shell = f"{action_cmd} > \"{str(log_file_path)}\" 2>&1"
        process = subprocess.run(full_cmd_for_shell, cwd=suite_dir, shell=True, text=True)
        rc = process.returncode
        if rc != 0: test_details['detail'] = f"Action '{action_cmd}' exited with code {rc}"
        else: final_status = 'pass'
        if log_file_path.exists():
            log_content = log_file_path.read_text()
            test_details['log'] = log_content
            if final_status == 'fail' and args.verbose > 0 and not test_details['detail']:
                test_details['detail'] += f"\nLog tail:\n" + "\n".join(log_content.splitlines()[-5:])
    except Exception as e: test_details['detail'] = f"Failed to run action '{action_cmd}': {e}"
    finally:
        if log_file_path.exists():
            try: log_file_path.unlink()
            except OSError as e_unlink: logger.warning(f"Could not remove log file {log_file_path}: {e_unlink}")
    if expected_status == 'fail':
        if final_status == 'fail': test_details['status'], test_details['detail'] = 'xfailed', test_details.get('detail', "") + " (Test failed as expected)"
        else: test_details['status'], test_details['detail'] = 'uxpassed', "Test passed but was expected to fail."
    else: test_details['status'] = 'passed' if final_status == 'pass' else 'failed'
    return test_details['status']

def format_testsuite_result_py(fh, result_summary, indent_prefix, verbose_format):
    if result_summary.get('failed'):
        fh.write(f"{indent_prefix}Failed tests:\n")
        for ftest in result_summary['failed']:
            if verbose_format: fh.write(f"{indent_prefix}{INDENT_STR}{'=' * BANNER_WIDTH}\n")
            name_detail = ftest.get('name', 'Unknown Test')
            if verbose_format: name_detail += f" in suite {ftest.get('testsuite_name', 'N/A')} in {ftest.get('dir', 'N/A')}"
            fh.write(f"{indent_prefix}{INDENT_STR}{name_detail}\n")
            if verbose_format and ftest.get('detail'): fh.write(f"{indent_prefix}{INDENT_STR}{ftest['detail']}\n")
            if verbose_format and ftest.get('log'):
                log_lines = ftest['log'].splitlines()
                log_to_show = ["..."] + log_lines[-15:] if len(log_lines) > 15 else log_lines
                indented_log = f"\n{indent_prefix}{INDENT_STR*2}".join(log_to_show)
                fh.write(f"{indent_prefix}{INDENT_STR*2}{indented_log}\n")
            if verbose_format: fh.write(f"{indent_prefix}{INDENT_STR}{'=' * BANNER_WIDTH}\n")
    if result_summary.get('uxpassed'):
        fh.write(f"{indent_prefix}Unexpected passed tests:\n")
        for utest in result_summary['uxpassed']:
            name_detail = utest.get('name', 'Unknown Test')
            if logger.level == logging.DEBUG: name_detail += f" ({utest.get('test_uri', 'N/A')})"
            fh.write(f"{indent_prefix}{INDENT_STR}{name_detail}\n")
    fh.write(indent_prefix)
    for counter_name in COUNTERS: fh.write(f"{counter_name.capitalize()}: {len(result_summary.get(counter_name, []))}  ")
    fh.write("\n")

def run_testsuite_py(testsuite_info, indent_prefix, args):
    suite_dir, suite_name, tests_to_run, suite_desc = testsuite_info['dir'], testsuite_info['name'], testsuite_info.get('tests', []), testsuite_info.get('desc', testsuite_info['name'])
    print(f"{indent_prefix}Running testsuite {suite_name}: {suite_desc}")
    results = {counter: [] for counter in COUNTERS}
    expected_failures_count, column = 0, len(indent_prefix)
    original_env_path, path_modified_for_this_suite = os.environ.get('PATH', None), False
    if testsuite_info.get('path'): path_modified_for_this_suite = True; testsuite_info['path_modified_for_suite'] = True
    if not args.verbose: print(indent_prefix, end='', flush=True)
    for test_detail in tests_to_run:
        test_detail.update({'testsuite_name': suite_name, 'dir': suite_dir})
        if args.dryrun: test_detail['status'], test_detail['detail'] = 'skipped', "(dryrun)"
        else:
            try: run_test_py(testsuite_info, test_detail, args) # status is set in test_detail
            except KeyboardInterrupt:
                logger.warning(f"\n{indent_prefix}Test execution aborted by user (SIGINT received).")
                test_detail.update({'status': 'failed', 'detail': "Aborted by user."})
                testsuite_info['abort_requested'] = True
        if test_detail['expect'] == 'fail': expected_failures_count += 1
        results[test_detail['status']].append(test_detail)
        if args.verbose == 0:
            char_map = {'passed': '.', 'failed': 'F', 'xfailed': '*', 'uxpassed': '!', 'skipped': '-'}
            print(char_map.get(test_detail['status'], '?'), end='', flush=True)
            column += 1
            if column >= LINE_WRAP: print(f"\n{indent_prefix}", end='', flush=True); column = len(indent_prefix)
        else:
            status_display = test_detail['status'].upper() if test_detail['status'] != 'passed' else test_detail['status']
            detail_str = f" - {test_detail['detail']}" if test_detail.get('detail') else ""
            print(f"{indent_prefix}{INDENT_STR}{test_detail['name']}: {status_display}{detail_str}")
            if args.verbose > 1 and test_detail['status'] == 'failed' and test_detail.get('log'):
                log_lines = test_detail['log'].splitlines()
                log_to_show = ["..."] + log_lines[-15:] if len(log_lines) > 15 else log_lines
                indented_log = f"\n{indent_prefix}{INDENT_STR*2}".join(log_to_show)
                print(f"{indent_prefix}{INDENT_STR*2}{indented_log}")
        if testsuite_info.get('abort_requested'):
            if args.verbose == 0: print("aborted", end='')
            print(f"\n{indent_prefix}Aborting testsuite {suite_name} due to user interrupt."); break
    if args.verbose == 0: print()
    if plan_file_path := testsuite_info.get('plan_file'):
        try: plan_file_path.unlink(missing_ok=True)
        except OSError as e: logger.warning(f"Could not remove plan file {plan_file_path}: {e}")
    if path_modified_for_this_suite:
        if original_env_path is not None: os.environ['PATH'] = original_env_path; logger.debug(f"Restored PATH to: {original_env_path}") if args.debug > 0 else None
        else: del os.environ['PATH']; logger.debug("Unset PATH as it was not originally set.") if args.debug > 0 else None
        testsuite_info.pop('path_modified_for_suite', None)
    results['status'] = 'fail' if results.get('failed') or results.get('uxpassed') else 'pass'
    results['abort_requested'] = testsuite_info.get('abort_requested', False)
    return results

def process_directory_py(directory_path: Path, specified_testsuites: list | None, args, script_globals: dict):
    logger.info(f"Processing directory: {directory_path}")
    dir_indent = INDENT_STR
    known_suites_in_dir = get_testsuites_from_dir(directory_path, args)
    suites_to_process = []
    if specified_testsuites and directory_path == Path(args.DIR).resolve():
        for s_name in specified_testsuites:
            if s_name in known_suites_in_dir: suites_to_process.append(s_name)
            else: logger.warning(f"{dir_indent}Specified testsuite '{s_name}' not found in {directory_path}. Known: {known_suites_in_dir}")
    else: suites_to_process = known_suites_in_dir
    if not suites_to_process:
        logger.info(f"{dir_indent}No test suites to process in directory {directory_path}")
        return ({counter: [] for counter in COUNTERS}, 0, script_globals.get('script_aborted_by_user', False))
    logger.info(f"{dir_indent}Running testsuites {', '.join(suites_to_process)} in {directory_path}")
    dir_summary, dir_overall_status = {counter: [] for counter in COUNTERS}, 0
    for suite_name in suites_to_process:
        if script_globals.get('script_aborted_by_user'): break
        testsuite_info = {'dir': directory_path, 'name': suite_name, 'tests': [], 'abort_requested': False}
        prep_result = prepare_testsuite_py(testsuite_info, args)
        if prep_result['status'] == 'fail':
            logger.error(f"{dir_indent}{INDENT_STR}Failed to prepare testsuite {suite_name}: {prep_result['details']}")
            dir_overall_status = 1; continue
        if not testsuite_info.get('tests') and not args.dryrun:
             logger.info(f"{dir_indent}{INDENT_STR}Testsuite {suite_name}: No tests found in plan.")
             results_for_empty_suite = {counter: [] for counter in COUNTERS}; results_for_empty_suite['status'] = 'pass'
             print(f"{dir_indent}{INDENT_STR}Summary for suite {suite_name}:")
             format_testsuite_result_py(sys.stdout, results_for_empty_suite, dir_indent + INDENT_STR + INDENT_STR, args.verbose > 0)
             if args.verbose == 0: print(); continue
        try:
            suite_run_result = run_testsuite_py(testsuite_info, dir_indent + INDENT_STR, args)
            print(f"{dir_indent}{INDENT_STR}Summary for suite {suite_name}:") # Moved here to always print after suite runs or if empty.
            format_testsuite_result_py(sys.stdout, suite_run_result, dir_indent + INDENT_STR + INDENT_STR, args.verbose > 0)
            if args.verbose == 0 and not testsuite_info.get('tests') and not args.dryrun : print()

            for counter_name in COUNTERS: dir_summary[counter_name].extend(suite_run_result.get(counter_name, []))
            if suite_run_result.get('status', 'fail') == 'fail': dir_overall_status = 1
            if suite_run_result.get('abort_requested'): script_globals['script_aborted_by_user'] = True; break
        except KeyboardInterrupt:
            logger.warning(f"\n{dir_indent}Run of suite {suite_name} aborted by user (SIGINT received).")
            dir_overall_status = 1; script_globals['script_aborted_by_user'] = True; break
    if suites_to_process and not script_globals.get('script_aborted_by_user'): # Only print if not aborted and suites were attempted
        print(f"\n{dir_indent}Testsuites summary for directory {directory_path}:")
        format_testsuite_result_py(sys.stdout, dir_summary, dir_indent + INDENT_STR, args.verbose > 0)
    return dir_summary, dir_overall_status, script_globals.get('script_aborted_by_user', False)

def main_improve():
    get_to_ntriples_path()
    parser = argparse.ArgumentParser(description="Run Rasqal test suites.", add_help=False)
    parser.add_argument("-d", "--debug", action="count", default=0, help="Enable extra debugging output (incremental).")
    parser.add_argument("-n", "--dryrun", action="store_true", help="Do not run tests, just show what would be done.")
    parser.add_argument("-r", "--recursive", action="store_true", help="Run all testsuites below the given DIR.")
    parser.add_argument("-v", "--verbose", action="count", default=0, help="Enable extra verbosity when running tests (incremental).")
    parser.add_argument("-h", "--help", action="store_true", help="Give help summary.")
    parser.add_argument("DIR", nargs="?", default=".", help="Directory to find test suites (defaults to '.').")
    parser.add_argument("TESTSUITES", nargs="*", help="Optional list of specific test suites to run in DIR.")
    args = parser.parse_args()

    if args.help:
        print("USAGE: improve [options] [DIRECTORY [TESTSUITE]]\n\nRun Rasqal testsuites from a Turtle manifest in the DIR.\nIf TESTSUITES are not given, provides a list of known testsuites in DIR.\nDIR defaults to '.' if not given.\n\nOptions:\n  -d, --debug        Enable extra debugging output.\n  -n, --dryrun       Do not run tests.\n  -r, --recursive    Run all testsuites below the given DIR.\n  -h, --help         Give help summary.\n  -v, --verbose      Enable extra verbosity when running tests.")
        sys.exit(0)
    if args.debug > 0: logger.setLevel(logging.DEBUG)
    if args.verbose > 0 and args.debug == 0 : logger.setLevel(logging.INFO) # Verbose implies INFO if debug not set higher
    elif args.verbose == 0 and args.debug == 0 : logger.setLevel(logging.WARNING) # Default level

    logger.debug(f"Starting {sys.argv[0]} with arguments: {args}")
    logger.debug(f"Resolved TO_NTRIPLES command: {TO_NTRIPLES_CMD}")
    if not (shutil.which(TO_NTRIPLES_CMD) or Path(TO_NTRIPLES_CMD).is_file()):
         logger.warning(f"'{TO_NTRIPLES_CMD}' utility not found. Manifest parsing will likely fail.")

    base_dir = Path(args.DIR).resolve()
    dirs_to_scan = []
    if args.recursive:
        logger.info(f"Recursive scan for testsuites starting from {base_dir}")
        for item in base_dir.rglob('*'):
            if item.is_dir() and ( (item / "Makefile").is_file() or (item / "Makefile.am").is_file() ):
                suites = get_testsuites_from_dir(item, args)
                if suites: logger.debug(f"  Found suites {suites} in {item}"); dirs_to_scan.append(item)
                else: logger.debug(f"  No suites listed by make in {item}")
        if not dirs_to_scan and ((base_dir / "Makefile").is_file() or (base_dir / "Makefile.am").is_file()):
             if get_testsuites_from_dir(base_dir, args): dirs_to_scan.append(base_dir)
    else:
        dirs_to_scan.append(base_dir)
    logger.info(f"Effective directories to scan: {dirs_to_scan if dirs_to_scan else 'None'}")

    overall_final_status_code = 0
    script_globals = {'script_aborted_by_user': False}
    grand_total_results_summary = {counter: [] for counter in COUNTERS}
    processed_dirs_count = 0

    for current_run_dir in dirs_to_scan:
        if script_globals['script_aborted_by_user']: break

        dir_summary, dir_status, aborted = process_directory_py(current_run_dir, args.TESTSUITES, args, script_globals)
        processed_dirs_count +=1

        for counter_name in COUNTERS:
            grand_total_results_summary[counter_name].extend(dir_summary.get(counter_name, []))
        if dir_status != 0: overall_final_status_code = 1
        if aborted: break

    if args.recursive and processed_dirs_count > 0: # Always print if recursive and at least one dir was processed
        print(f"\nTotal of all Testsuites ({'partial due to abort' if script_globals['script_aborted_by_user'] else 'completed'}):")
        format_testsuite_result_py(sys.stdout, grand_total_results_summary, INDENT_STR, True)

    if script_globals['script_aborted_by_user']:
        logger.info("Exiting due to user abort.")
        sys.exit(130)
    sys.exit(overall_final_status_code)

if __name__ == "__main__":
    main_improve()

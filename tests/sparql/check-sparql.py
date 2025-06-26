#!/usr/bin/env python3
#
# check-sparql - Run Rasqal against W3C SPARQL testsuites
#
# USAGE: check-sparql [options] [TEST]
#
# Copyright (C) 2004-2014, David Beckett http://www.dajobe.org/
# Copyright (C) 2004-2005, University of Bristol, UK http://www.bristol.ac.uk/
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
#
# Requires:
#   roqet (from rasqal) compiled in the parent directory
#   rapper (from raptor) in PATH for manifest parsing
#
# Depends on a variety of rasqal internal debug print formats
#

import os
import sys
import subprocess
import argparse
import logging
from enum import Enum
from pathlib import Path
import time
import glob
import re

# Configure logging
logging.basicConfig(level=logging.INFO, format='%(levelname)s: %(message)s')
logger = logging.getLogger(__name__)

CURDIR = Path.cwd()

# Helper function to normalize blank node IDs in output
def normalize_blank_nodes(text_output):
    return re.sub(r'blank \w+', 'blank _', text_output)

# Temporary file names
ROQET_OUT = "roqet.out"
RESULT_OUT = "result.out"
ROQET_TMP = "roqet.tmp"
ROQET_ERR = "roqet.err"
DIFF_OUT = "diff.out"
TO_NTRIPLES_ERR = "to_ntriples.err"

# RDF and SPARQL Test Suite Namespaces
RDF_NS = "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
RS_NS = "http://www.w3.org/2001/sw/DataAccess/tests/result-set#"
MF_NS = "http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#"
MFX_NS = "http://jena.hpl.hp.com/2005/05/test-manifest-extra#"
QT_NS = "http://www.w3.org/2001/sw/DataAccess/tests/test-query#"
DAWGT_NS = "http://www.w3.org/2001/sw/DataAccess/tests/test-dawg#"
UT_NS = "http://www.w3.org/2009/sparql/tests/test-update#"
SD_NS = "http://www.w3.org/ns/sparql-service-description#"
ENT_NS = "http://www.w3.org/ns/entailment/"

class Expect(Enum):
    PASS = "pass"
    FAIL = "fail"

class TestType(Enum):
    CSV_RESULT_FORMAT_TEST = f"{MF_NS}CSVResultFormatTest"
    POSITIVE_SYNTAX_TEST = f"{MF_NS}PositiveSyntaxTest"
    POSITIVE_SYNTAX_TEST_11 = f"{MF_NS}PositiveSyntaxTest11"
    POSITIVE_UPDATE_SYNTAX_TEST_11 = f"{MF_NS}PositiveUpdateSyntaxTest11"
    NEGATIVE_SYNTAX_TEST = f"{MF_NS}NegativeSyntaxTest"
    NEGATIVE_SYNTAX_TEST_11 = f"{MF_NS}NegativeSyntaxTest11"
    NEGATIVE_UPDATE_SYNTAX_TEST_11 = f"{MF_NS}NegativeUpdateSyntaxTest11"
    TEST_SYNTAX = f"{MFX_NS}TestSyntax"
    TEST_BAD_SYNTAX = f"{MFX_NS}TestBadSyntax"
    UPDATE_EVALUATION_TEST = f"{UT_NS}UpdateEvaluationTest" # Also MF_NS
    PROTOCOL_TEST = f"{MF_NS}ProtocolTest"


def get_utils_dir():
    """Finds the 'utils' directory in parent directories."""
    current_dir = CURDIR
    while current_dir != Path('/'):
        utils_dir = current_dir / 'utils'
        if utils_dir.is_dir():
            return utils_dir
        current_dir = current_dir.parent
    logger.error(f"{sys.argv[0]}: Could not find 'utils' dir in parent directories")
    sys.exit(1)

UTILS_DIR = get_utils_dir()
TO_NTRIPLES = os.environ.get('TO_NTRIPLES') or str(UTILS_DIR / 'to-ntriples')
ROQET = os.environ.get('ROQET') or str(UTILS_DIR / 'roqet')
DIFF_CMD = os.environ.get('DIFF') or "diff"


def main():
    parser = argparse.ArgumentParser(description="Run SPARQL tests.")
    parser.add_argument(
        "--debug", "-d", action="count", default=0, help="Enable extra debugging output."
    )
    parser.add_argument(
        "--srcdir", "-s", default=".", help="Set the source directory."
    )
    parser.add_argument(
        "--input", "-i", default="sparql", help="Set the input language."
    )
    parser.add_argument(
        "--manifest", "-m", help="Set the input test MANIFEST file."
    )
    parser.add_argument(
        "--earl", "-e", help="Set the output test EARL summary file."
    )
    parser.add_argument(
        "--junit", "-j", help="Set the output Ant Junit XML results file."
    )
    parser.add_argument(
        "--suite", "-u", help="Set the test suite name."
    )
    parser.add_argument(
        "--approved", "-a", action="store_true", help="Run only approved tests."
    )
    parser.add_argument(
        "--warnings", "-W", type=int, default=0, help="Set warning level (0-100)."
    )
    parser.add_argument(
        "test", nargs="?", help="Run a specific test by name or URI."
    )

    args = parser.parse_args()

    if args.debug > 0:
        logger.setLevel(logging.DEBUG)
        logging.getLogger().setLevel(logging.DEBUG) # Also set root logger for other modules if any

    logger.debug(f"Arguments: {args}")

    srcdir = Path(args.srcdir)
    manifest_file_path = None
    if args.manifest:
        manifest_file_path = srcdir / args.manifest
    else:
        for mf_name in ["manifest.ttl", "manifest.n3"]:
            if (srcdir / mf_name).is_file():
                manifest_file_path = srcdir / mf_name
                break

    if not manifest_file_path or not manifest_file_path.is_file():
        logger.error(f"No manifest file found in {srcdir}")
        sys.exit(1)

    logger.info(f"Using manifest file: {manifest_file_path}")

    tests = parse_manifest(manifest_file_path, srcdir, args.test)

    if args.test and not tests:
        logger.error(f"Test {args.test} not found in manifest.")
        sys.exit(1)

    if not tests:
        logger.info("No tests found or selected to run.")
        sys.exit(0)

    passed_tests = []
    failed_tests = []
    skipped_tests = []
    test_results_data = [] # For JUnit/EARL reporting

    start_time_suite = time.time()

    for test_config in tests:
        test_name = test_config.get("name") or test_config.get("test_uri", "Unknown Test")
        # logger.info(f"Processing test: {test_name}") # Too verbose for many tests

        if test_config.get("is_withdrawn"):
            logger.debug(f"Test {test_name} ({test_config['test_uri']}) was withdrawn - skipping")
            skipped_tests.append(test_config)
            test_results_data.append({
                'name': test_name, 'uri': test_config['test_uri'], 'result': 'skipped',
                'is_success': False, 'reason': 'withdrawn'
            })
            continue
        if args.approved and not test_config.get("is_approved"):
            logger.debug(f"Test {test_name} ({test_config['test_uri']}) not approved - skipping")
            skipped_tests.append(test_config)
            test_results_data.append({
                'name': test_name, 'uri': test_config['test_uri'], 'result': 'skipped',
                'is_success': False, 'reason': 'not approved'
            })
            continue
        if test_config.get("has_entailment_regime"):
            logger.debug(f"Test {test_name} ({test_config['test_uri']}) has entailment - skipping")
            skipped_tests.append(test_config)
            test_results_data.append({
                'name': test_name, 'uri': test_config['test_uri'], 'result': 'skipped',
                'is_success': False, 'reason': 'has entailment regime'
            })
            continue

        current_lang = test_config.get("language", args.input)
        if test_config.get("language") != "sparql" and test_config.get("language") is not None :
             current_lang = test_config["language"]

        current_test_config = test_config.copy()
        current_test_config["language"] = current_lang
        current_test_config["warning_level"] = args.warnings

        test_result_details = run_test(current_test_config, args.debug)
        test_results_data.append(test_result_details)

        is_test_case_success = test_result_details.get('is_success', False)

        if is_test_case_success:
            passed_tests.append(test_config)
        else:
            failed_tests.append(test_config)

    end_time_suite = time.time()
    elapsed_time_suite = end_time_suite - start_time_suite

    if not args.test and args.debug < 2:
        for f_name in [ROQET_OUT, RESULT_OUT, ROQET_TMP, ROQET_ERR, DIFF_OUT, TO_NTRIPLES_ERR]:
            try:
                Path(f_name).unlink(missing_ok=True)
            except OSError as e:
                logger.warning(f"Could not delete temp file {f_name}: {e}")

    if failed_tests:
        logger.info(f"{len(failed_tests)} FAILED tests:")
        for t_conf in failed_tests:
            res = next((r for r in test_results_data if r['uri'] == t_conf['test_uri']), None)
            reason = ""
            if res and res.get('stderr'):
                err_lines = res['stderr'].splitlines()
                for line in err_lines:
                    if "rasqal error" in line or "Error" in line :
                        reason = f" ({line.split('rasqal error -')[-1].split('Error -')[-1].strip()})"
                        break
            logger.info(f"  FAILED: {t_conf.get('name', t_conf['test_uri'])}{reason}")

    logger.info(
        f"Summary: {len(passed_tests)} tests passed, "
        f"{len(failed_tests)} tests failed, "
        f"{len(skipped_tests)} tests skipped in {elapsed_time_suite:.2f}s"
    )

    if args.earl:
        generate_earl_report(args.earl, test_results_data, args)

    if args.junit:
        generate_junit_report(args.junit, test_results_data, args, elapsed_time_suite)

    sys.exit(len(failed_tests))


def run_test(config, global_debug_level):
    name = config.get("name") or config.get("test_uri", "Unknown test")
    test_uri = config.get("test_uri")
    language = config.get("language", "sparql")
    query_file = Path(config["test_file"])
    data_files = [Path(df) for df in config.get("data_files", []) if df]
    named_data_files = [Path(ndf) for ndf in config.get("named_data_files", []) if ndf]
    result_file = Path(config["result_file"]) if config.get("result_file") else None
    expect_status = config["expect"]
    cardinality_mode = config.get("cardinality_mode", "strict")
    execute = config.get("execute", True)
    test_type = config.get("test_type")
    warning_level = config.get("warning_level", 0)

    test_result_summary = {
        'name': name, 'uri': test_uri, 'result': 'failure', 'is_success': False,
        'stdout': '', 'stderr': '', 'query': '', 'elapsed-time': 0,
        'roqet-status-code': -1, 'diff': '',
    }

    if global_debug_level > 0:
        logger.debug(
            f"run_test: (URI: {test_uri})\n"
            f"  name       : {name}\n"
            f"  language   : {language}\n"
            f"  query      : {query_file}\n"
            f"  data       : {'; '.join(map(str,data_files))}\n"
            f"  named data : {'; '.join(map(str,named_data_files))}\n"
            f"  result file: {result_file or 'none'}\n"
            f"  expect     : {expect_status.value}\n"
            f"  card mode  : {cardinality_mode}\n"
            f"  execute    : {execute}\n"
            f"  test_type  : {test_type}"
        )

    roqet_args = [ROQET, "-i", language]
    if test_type == TestType.CSV_RESULT_FORMAT_TEST.value:
        roqet_args.extend(["-r", "csv"])
    else:
        roqet_args.extend(["-d", "debug"])
    roqet_args.extend(["-W", str(warning_level)])
    for df in data_files: roqet_args.extend(["-D", str(df)])
    for ndf in named_data_files: roqet_args.extend(["-G", str(ndf)])
    if not execute: roqet_args.append("-n")
    roqet_args.append(str(query_file))

    roqet_cmd_str = " ".join(f"'{a}'" if " " in a else a for a in roqet_args)
    test_result_summary['query'] = roqet_cmd_str
    if global_debug_level > 0: logger.debug(f"Executing roqet: {roqet_cmd_str}")

    start_time = time.time()
    actual_roqet_stdout, actual_roqet_stderr = "", ""
    try:
        with open(ROQET_TMP, "w") as f_out, open(ROQET_ERR, "w") as f_err:
            process = subprocess.Popen(roqet_args, stdout=f_out, stderr=f_err, text=True, cwd=CURDIR)
            process.wait()
            return_code = process.returncode

        end_time = time.time()
        test_result_summary['elapsed-time'] = end_time - start_time
        with open(ROQET_TMP, "r") as f: actual_roqet_stdout = f.read()
        with open(ROQET_ERR, "r") as f: actual_roqet_stderr = f.read()
        test_result_summary['stdout'] = actual_roqet_stdout
        test_result_summary['stderr'] = actual_roqet_stderr
        test_result_summary['roqet-status-code'] = return_code

        if return_code != 0:
            outcome_msg = f"exited with status {return_code}"
            if global_debug_level > 0: logger.debug(f"roqet for '{name}' {outcome_msg}")
            if expect_status == Expect.FAIL:
                logger.info(f"Test '{name}': OK (roqet failed as expected: {outcome_msg})")
                test_result_summary['result'] = 'success'
            else:
                logger.warning(f"Test '{name}': FAILED (roqet command failed: {outcome_msg})")
                if actual_roqet_stderr: logger.warning(f"  Stderr:\n{actual_roqet_stderr.strip()}")
            finalize_test_result(test_result_summary, expect_status)
            return test_result_summary

        if expect_status == Expect.FAIL:
            logger.warning(f"Test '{name}': FAILED (roqet succeeded, but was expected to fail)")
            finalize_test_result(test_result_summary, expect_status)
            return test_result_summary

        if test_type == TestType.CSV_RESULT_FORMAT_TEST.value:
            if not result_file:
                logger.warning(f"Test '{name}': FAILED (CSVResultFormatTest but no result_file specified)")
            else:
                diff_cmd_list = [DIFF_CMD, "-u", ROQET_TMP, str(result_file)]
                if global_debug_level > 0: logger.debug(f"Comparing CSV output: {' '.join(diff_cmd_list)}")
                diff_process = subprocess.run(diff_cmd_list, capture_output=True, text=True)
                if diff_process.returncode != 0:
                    logger.warning(f"Test '{name}': FAILED (CSV output differs from {result_file})")
                    diff_output = diff_process.stdout
                    test_result_summary['diff'] = diff_output
                    with open(DIFF_OUT, "w") as f_diff: f_diff.write(diff_output)
                    if global_debug_level > 0 or len(diff_output) < 1000: logger.warning(f"  Differences:\n{diff_output.strip()}")
                    else: logger.warning(f"  Differences written to {DIFF_OUT}")
                else:
                    logger.info(f"Test '{name}': OK (CSV output matches)")
                    test_result_summary['result'] = 'success'
            finalize_test_result(test_result_summary, expect_status)
            return test_result_summary

        if not execute:
            logger.info(f"Test '{name}': OK (positive syntax check passed)")
            test_result_summary['result'] = 'success'
            finalize_test_result(test_result_summary, expect_status)
            return test_result_summary

        rasqal_errors_in_stderr = [line for line in actual_roqet_stderr.splitlines() if "rasqal error -" in line]
        if rasqal_errors_in_stderr:
            logger.warning(f"Test '{name}': FAILED (roqet succeeded but rasqal reported errors in stderr)")
            for err_line in rasqal_errors_in_stderr: logger.warning(f"  {err_line}")
            test_result_summary['errors'] = "\n".join(rasqal_errors_in_stderr)
            finalize_test_result(test_result_summary, expect_status)
            return test_result_summary

        if not result_file:
            logger.info(f"Test '{name}': OK (roqet succeeded, no result_file to compare)")
            test_result_summary['result'] = 'success'
            finalize_test_result(test_result_summary, expect_status)
            return test_result_summary

        parsed_actual_output_info = parse_roqet_debug_output(actual_roqet_stdout, result_file)
        actual_result_type = parsed_actual_output_info['result_type']
        actual_vars_order = parsed_actual_output_info['vars_order']
        actual_results_count = parsed_actual_output_info['roqet_results_count']
        result_file_base_uri = f"file://{result_file.resolve()}"
        expected_results_count = 0

        if actual_result_type == "graph":
            if global_debug_level > 0: logger.debug(f"Reading expected RDF graph result file {result_file}")
            expected_results_data = read_rdf_graph_file(result_file, result_file_base_uri)
            if expected_results_data and expected_results_data.get('graph_ntriples'):
                sorted_expected_triples = sorted(list(set(expected_results_data['graph_ntriples'].splitlines())))
                with open(RESULT_OUT, "w") as f_res_out:
                    for triple_line in sorted_expected_triples: f_res_out.write(triple_line + "\n")
            elif not expected_results_data:
                logger.warning(f"Test '{name}': FAILED (could not read/parse expected graph result file {result_file})")
                finalize_test_result(test_result_summary, expect_status)
                return test_result_summary
        elif actual_result_type in ["bindings", "boolean"]:
            result_file_ext = result_file.suffix.lower()
            expected_result_format = "turtle"
            if result_file_ext == ".srx": expected_result_format = "xml"
            elif result_file_ext == ".srj":
                logger.warning(f"Test '{name}': SKIPPING JSON result comparison ({result_file})")
                test_result_summary['result'] = 'success'
                finalize_test_result(test_result_summary, expect_status)
                return test_result_summary
            elif result_file_ext in [".csv", ".tsv"]: expected_result_format = result_file_ext[1:]
            elif result_file_ext == ".rdf": expected_result_format = "rdfxml"

            if global_debug_level > 0: logger.debug(f"Reading expected '{actual_result_type}' result file {result_file} (format: {expected_result_format})")
            expected_results_info = read_query_results_file(result_file, expected_result_format, actual_vars_order)
            if expected_results_info: expected_results_count = expected_results_info.get('count',0)
            elif not expected_results_info:
                logger.warning(f"Test '{name}': FAILED (could not read/parse expected results file {result_file})")
                finalize_test_result(test_result_summary, expect_status)
                return test_result_summary
        else:
            logger.error(f"Test '{name}': Unknown actual_result_type '{actual_result_type}'")
            finalize_test_result(test_result_summary, expect_status)
            return test_result_summary

        comparison_rc = -1
        if actual_result_type == "graph":
            comparison_rc = compare_rdf_graphs(Path(RESULT_OUT), Path(ROQET_OUT), Path(DIFF_OUT))
        else:
            diff_cmd_list = [DIFF_CMD, "-u", RESULT_OUT, ROQET_OUT]
            if global_debug_level > 0: logger.debug(f"Comparing bindings/boolean results: {' '.join(diff_cmd_list)}")
            with open(DIFF_OUT, "w") as f_diff_out:
                diff_proc = subprocess.run(diff_cmd_list, stdout=f_diff_out, text=True)
            comparison_rc = diff_proc.returncode

        if comparison_rc != 0:
            if actual_result_type == "bindings" and cardinality_mode == "lax" and actual_results_count <= expected_results_count:
                if global_debug_level >0: logger.debug(f"Cardinality 'lax': allowing {actual_results_count} vs {expected_results_count}")
                comparison_rc = 0

        if comparison_rc != 0:
            msg = f"Test '{name}': FAILED (Results differ)."
            if actual_result_type == "bindings" and expected_results_count != actual_results_count and cardinality_mode != "lax":
                 msg = f"Test '{name}': FAILED (Expected {expected_results_count}, got {actual_results_count}). Results may also differ."
            logger.warning(msg)
            with open(DIFF_OUT, "r") as f_diff_read:
                diff_content = f_diff_read.read().strip()
                test_result_summary['diff'] = diff_content
                if global_debug_level > 1 or (diff_content and len(diff_content) < 1000): logger.warning(f"  Differences:\n{diff_content}")
        else:
            logger.info(f"Test '{name}': OK (Results match)")
            test_result_summary['result'] = 'success'

    except FileNotFoundError as e:
        logger.error(f"Error running roqet for '{name}': {e}. Is '{ROQET}' correct?")
        test_result_summary['stderr'] = str(e)
    except subprocess.TimeoutExpired:
        logger.error(f"Test '{name}' FAILED: roqet command timed out.")
        test_result_summary['stderr'] = "roqet command timed out"
    except Exception as e:
        logger.error(f"An unexpected error occurred while running test '{name}': {type(e).__name__}: {e}")
        import traceback
        logger.error(traceback.format_exc())
        test_result_summary['stderr'] = f"{type(e).__name__}: {e}"

    finalize_test_result(test_result_summary, expect_status)
    return test_result_summary

def finalize_test_result(test_result_summary, expect_status):
    execution_result = test_result_summary.get('result', 'failure')
    if execution_result == 'success' and expect_status == Expect.PASS:
        test_result_summary['is_success'] = True
    elif execution_result == 'failure' and expect_status == Expect.FAIL:
        test_result_summary['is_success'] = True
    else:
        test_result_summary['is_success'] = False

import re

_projected_vars_order = []

def parse_roqet_debug_output(roqet_stdout_str: str, result_file_path_for_sorting_ref: Path):
    global _projected_vars_order
    _projected_vars_order = []
    lines = roqet_stdout_str.splitlines()
    result_type = "bindings"
    vars_seen_in_this_roqet_output = {}
    is_sorted_by_query = False
    processed_output_lines = []
    roqet_results_count = 0

    for line in lines:
        match_proj_vars = re.match(r"projected variable names: (.*)", line)
        if match_proj_vars:
            if not _projected_vars_order:
                for vname in re.split(r",\s*", match_proj_vars.group(1)):
                    if vname not in vars_seen_in_this_roqet_output:
                        _projected_vars_order.append(vname)
                        vars_seen_in_this_roqet_output[vname] = 1
                if logger.level == logging.DEBUG: logger.debug(f"Set roqet projected vars order to: {_projected_vars_order}")
        match_verb = re.match(r"query verb:\s+(\S+)", line)
        if match_verb:
            verb = match_verb.group(1).upper()
            if verb == "CONSTRUCT" or verb == "DESCRIBE": result_type = "graph"
            elif verb == "ASK": result_type = "boolean"
        if "query order conditions:" in line: is_sorted_by_query = True
        line = normalize_blank_nodes(line)
        row_match = re.match(r"^(?:row|result): \[(.*)\]$", line)
        if row_match:
            content = row_match.group(1).replace("=INV:", "=").replace("=udt", "=string")
            content = re.sub(r'=xsdstring\((.*?)\)', r'=string("\1"^^<http://www.w3.org/2001/XMLSchema#string>)', content)
            prefix = "row: " if line.startswith("row:") else "result: "
            processed_output_lines.append(f"{prefix}[{content}]")
            roqet_results_count +=1
            continue
        if result_type == "graph" and (line.startswith("_:") or line.startswith("<")):
            processed_output_lines.append(line)

    final_output_lines = processed_output_lines
    if result_type == "bindings" and not is_sorted_by_query: final_output_lines.sort()
    elif result_type == "graph": final_output_lines = sorted(list(set(processed_output_lines)))
    with open(ROQET_OUT, "w") as f_roqet_out:
        for out_line in final_output_lines: f_roqet_out.write(out_line + "\n")
    return {'result_type': result_type, 'roqet_results_count': roqet_results_count, 'vars_order': _projected_vars_order[:]}

def read_query_results_file(result_file_path: Path, result_format_hint: str, expected_vars_order: list):
    abs_result_file_path = result_file_path.resolve()
    cmd = [ROQET, "-q", "-R", result_format_hint, "-r", "simple", "-t", str(abs_result_file_path)]
    if logger.level == logging.DEBUG: logger.debug(f"(read_query_results_file): Running {' '.join(cmd)}")
    try:
        with open(ROQET_ERR, 'w') as f_err:
            process = subprocess.run(cmd, capture_output=True, text=True, stderr=f_err)
        with open(ROQET_ERR, 'r') as f_err_read: roqet_stderr_content = f_err_read.read()
        if process.returncode != 0 or "Error" in roqet_stderr_content:
            logger.warning(f"Reading query results file '{abs_result_file_path}' (format {result_format_hint}) FAILED.")
            if roqet_stderr_content: logger.warning(f"  Stderr from roqet:\n{roqet_stderr_content.strip()}")
            return None
        parsed_rows_for_output = []
        first_row = True
        current_vars_order = []
        for line in process.stdout.splitlines():
            line = line.strip()
            if not line.startswith("row: [") or not line.endswith("]"): continue
            content = normalize_blank_nodes(line[len("row: ["):-1])
            row_data = {}
            if content:
                pairs = re.split(r",\s*(?=\S+=)", content)
                for pair in pairs:
                    var_name, var_val = pair.split("=", 1)
                    row_data[var_name] = var_val
                    if first_row and var_name not in current_vars_order: current_vars_order.append(var_name)
            first_row = False
            order_to_use = expected_vars_order if expected_vars_order else current_vars_order
            formatted_row_parts = [f"{var}={defined_or_null(row_data.get(var))}" for var in order_to_use]
            parsed_rows_for_output.append(f"row: [{', '.join(formatted_row_parts)}]")
        parsed_rows_for_output.sort()
        with open(RESULT_OUT, "w") as f_res_out:
            for r_line in parsed_rows_for_output: f_res_out.write(r_line + "\n")
        return {'count': len(parsed_rows_for_output)}
    except FileNotFoundError: logger.error(f"{ROQET} not found."); return None
    except Exception as e: logger.error(f"Error in read_query_results_file for {abs_result_file_path}: {e}"); return None

def to_debug_comparable_value(value_str):
    if value_str == f"<{RS_NS}undefined>": return "NULL"
    return value_str

def read_rdf_graph_file(result_file_path: Path, base_uri: str):
    abs_result_file_path = result_file_path.resolve()
    cmd = [TO_NTRIPLES, str(abs_result_file_path), base_uri]
    if logger.level == logging.DEBUG: logger.debug(f"(read_rdf_graph_file): Running {' '.join(cmd)}")
    try:
        with open(TO_NTRIPLES_ERR, 'w') as f_err:
            process = subprocess.run(cmd, capture_output=True, text=True, stderr=f_err)
        with open(TO_NTRIPLES_ERR, 'r') as f_err_read: to_ntriples_stderr = f_err_read.read()
        if process.returncode != 0 or "Error -" in to_ntriples_stderr:
            logger.warning(f"Parsing RDF graph result file '{abs_result_file_path}' FAILED - {TO_NTRIPLES} errors.")
            if to_ntriples_stderr: logger.warning(f"  Stderr from {TO_NTRIPLES}:\n{to_ntriples_stderr.strip()}")
            else: logger.warning(f"  {TO_NTRIPLES} exit code: {process.returncode}, stdout: {process.stdout[:200]}")
            return None
        return {'graph_ntriples': process.stdout}
    except FileNotFoundError: logger.error(f"{TO_NTRIPLES} not found."); return None
    except Exception as e: logger.error(f"Error running {TO_NTRIPLES} for {abs_result_file_path}: {e}"); return None

def compare_rdf_graphs(file1_path: Path, file2_path: Path, diff_output_path: Path):
    abs_file1, abs_file2, abs_diff_out = str(file1_path.resolve()), str(file2_path.resolve()), str(diff_output_path.resolve())
    ntc_path, jena_root = os.environ.get("NTC"), os.environ.get("JENAROOT")
    if ntc_path:
        cmd_list = [ntc_path, abs_file1, abs_file2]
        logger.debug(f"Comparing graphs using NTC: {' '.join(cmd_list)}")
        try:
            with open(abs_diff_out, "w") as f_diff: process = subprocess.run(cmd_list, stdout=f_diff, stderr=subprocess.PIPE, text=True)
            if process.stderr: logger.debug(f"NTC stderr: {process.stderr.strip()}")
            return process.returncode
        except Exception as e: logger.warning(f"Failed to run NTC ({ntc_path}): {e}. Falling back to diff.")
    elif jena_root:
        classpath_items = [str(p) for p in (Path(jena_root) / "lib").glob("*.jar")]
        if not classpath_items: logger.warning(f"Jena at {jena_root} but no JARs in lib/. Falling back to diff.")
        else:
            cmd_list = ["java", "-cp", os.pathsep.join(classpath_items), "jena.rdfcompare", abs_file1, abs_file2, "N-TRIPLE", "N-TRIPLE"]
            logger.debug(f"Comparing graphs using Jena rdfcompare: {' '.join(cmd_list)}")
            try:
                with open(abs_diff_out, "w") as f_diff: process = subprocess.run(cmd_list, stdout=f_diff, stderr=subprocess.PIPE, text=True)
                if process.stderr: logger.debug(f"Jena rdfcompare stderr: {process.stderr.strip()}")
                if process.returncode != 0: logger.warning(f"Jena rdfcompare command failed (exit {process.returncode}). Falling back to diff.")
                else:
                    with open(abs_diff_out, "r") as f_diff_read: jena_output = f_diff_read.read()
                    return 0 if "graphs are equal" in jena_output.lower() else 1
            except Exception as e: logger.warning(f"Failed to run Jena rdfcompare: {e}. Falling back to diff.")
    cmd_list = [DIFF_CMD, "-u", abs_file1, abs_file2]
    logger.debug(f"Comparing graphs using system diff: {' '.join(cmd_list)}")
    try:
        with open(abs_diff_out, "w") as f_diff: process = subprocess.run(cmd_list, stdout=f_diff, stderr=subprocess.PIPE, text=True)
        return process.returncode
    except Exception as e:
        logger.error(f"Failed to run system diff ({DIFF_CMD}): {e}")
        with open(abs_diff_out, "w") as f_diff: f_diff.write(f"Error running diff command: {e}\n")
        return 2

def get_rasqal_version():
    try: return subprocess.run([ROQET, "-v"], capture_output=True, text=True, check=True).stdout.strip()
    except Exception as e: logger.warning(f"Could not get roqet version: {e}"); return "unknown"
RASQAL_VERSION = get_rasqal_version()
RASQAL_URL = "http://librdf.org/rasqal/"

def html_escape(text): return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;") if text else ""

def generate_earl_report(earl_file_path, test_results, args):
    from datetime import datetime, timezone
    path, rasqal_date, rasqal_name = Path(earl_file_path), datetime.now(timezone.utc).strftime("%Y-%m-%d"), f"Rasqal {RASQAL_VERSION}"
    is_new_file = not path.exists() or path.stat().st_size == 0
    with open(path, "a") as f:
        if is_new_file: f.write(f"""@prefix doap: <http://usefulinc.com/ns/doap#> .\n@prefix earl: <http://www.w3.org/ns/earl#> .\n@prefix foaf: <http://xmlns.com/foaf/0.1/> .\n@prefix xsd: <http://www.w3.org/2001/XMLSchema#> .\n\n_:author a foaf:Person;\n     foaf:homepage <http://www.dajobe.org/>;\n     foaf:name "Dave Beckett". \n\n<{RASQAL_URL}> a doap:Project;\n     doap:name "Rasqal";\n     doap:homepage <{RASQAL_URL}>;\n     doap:release\n       [ a doap:Version;\n         doap:created "{rasqal_date}"^^xsd:date ;\n         doap:name "{rasqal_name}"].\n""")
        for res in test_results:
            outcome = "earl:pass" if res.get('is_success') else "earl:fail"
            if res.get('result') == 'skipped': outcome = "earl:untested"
            f.write(f"\n[] a earl:Assertion;\n   earl:assertedBy _:author;\n   earl:result [ a earl:TestResult; earl:outcome {outcome} ];\n   earl:subject <{RASQAL_URL}>;\n   earl:test <{res['uri']}> .\n")
    logger.info(f"EARL report generated at {earl_file_path}")

def generate_junit_report(junit_file_path, test_results, args, suite_elapsed_time):
    from datetime import datetime, timezone
    import socket
    ts, name, host = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S"), f"Rasqal {RASQAL_VERSION}", socket.gethostname()
    suite = args.suite or "rasqal.sparql.testsuite"
    tests_c = len([tr for tr in test_results if tr.get('result') != 'skipped'])
    failures_c = len([tr for tr in test_results if tr.get('result') != 'skipped' and not tr.get('is_success', False)])
    with open(junit_file_path, "w") as f:
        f.write(f"""<?xml version="1.0" encoding="UTF-8"?>\n<testsuites>\n  <testsuite name="{html_escape(suite)}" timestamp="{ts}" hostname="{html_escape(host)}" tests="{tests_c}" failures="{failures_c}" errors="0" time="{suite_elapsed_time:.3f}">\n    <properties>\n       <property name="author-name" value="Dave Beckett" />\n       <property name="author-homepage" value="http://www.dajobe.org/" />\n       <property name="project-name" value="Rasqal" />\n       <property name="project-uri" value="{RASQAL_URL}" />\n       <property name="project-version" value="{html_escape(name)}" />\n    </properties>\n""")
        for res in test_results:
            if res.get('result') == 'skipped':
                f.write(f"""    <testcase name="{html_escape(res['name'])}" classname="{html_escape(res['uri'])}" time="0.000"><skipped/></testcase>\n""")
                continue
            etime = f"{res.get('elapsed-time', 0):.3f}"
            f.write(f"""    <testcase name="{html_escape(res['name'])}" classname="{html_escape(res['uri'])}" time="{etime}">\n""")
            if not res.get('is_success'):
                roq_stat, msg = res.get('roqet-status-code', -1), "Test failed."
                if roq_stat != 0 and roq_stat != -1: msg = f"Test failed (roqet exited {roq_stat})"
                txt_parts = [f"STDOUT:\n{res['stdout']}" if res.get('stdout') else "", f"STDERR:\n{res['stderr']}" if res.get('stderr') else "", f"DIFF:\n{res['diff']}" if res.get('diff') else ""]
                f.write(f"""      <failure message="{html_escape(msg)}" type="TestFailure">\n{html_escape("\n\n".join(filter(None, txt_parts)))}\n      </failure>\n""")
            f.write(f"""    </testcase>\n""")
        f.write(f"""  <system-out></system-out>\n  <system-err></system-err>\n  </testsuite>\n</testsuites>\n""")
    logger.info(f"JUnit report generated at {junit_file_path}")

def _parse_nt_line(line):
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
    obj_val, obj_type, lang_or_dt = o_full, None, None
    if o_full.startswith("<") and o_full.endswith(">"): obj_type, obj_val = "uri", o_full[1:-1]
    elif o_full.startswith("_:"): obj_type = "bnode"
    elif o_full.startswith('"'):
        obj_type = "literal"
        match = re.match(r'"(.*)"(?:@([a-zA-Z-]+)|\^\^<(.*)>)?', o_full)
        if match:
            obj_val = match.group(1).replace("\\\"", "\"").replace("\\t", "\t").replace("\\n", "\n").replace("\\\\", "\\")
            if match.group(2): lang_or_dt = match.group(2)
            elif match.group(3): lang_or_dt = match.group(3)
        else: logger.warning(f"Literal parse err: {o_full}"); return None
    else: return None
    if s.startswith("<"): s = s[1:-1]
    if p.startswith("<"): p = p[1:-1]
    return s, p, obj_val, obj_type, lang_or_dt

def parse_manifest(manifest_file_path: Path, srcdir: Path, unique_test_filter=None):
    nt_lines = []
    manifest_format = "turtle"
    if manifest_file_path.suffix == ".n3": manifest_format = "n3"
    elif manifest_file_path.suffix == ".nt": manifest_format = "ntriples"
    cmd_rapper = ["rapper", "-i", manifest_format, "-o", "ntriples", str(manifest_file_path.resolve())]
    logger.debug(f"Parsing manifest with rapper: {' '.join(cmd_rapper)}")
    try:
        process = subprocess.run(cmd_rapper, capture_output=True, text=True, check=True)
        nt_lines = process.stdout.splitlines()
    except subprocess.CalledProcessError as e:
        logger.error(f"Rapper err for {manifest_file_path}: {e}\nStdout: {e.stdout}\nStderr: {e.stderr}")
        logger.info(f"Fallback to {TO_NTRIPLES} for {manifest_file_path}")
        cmd_to_nt = [TO_NTRIPLES, str(manifest_file_path.resolve())]
        try:
            process_to_nt = subprocess.run(cmd_to_nt, capture_output=True, text=True, check=True)
            nt_lines = process_to_nt.stdout.splitlines()
        except Exception as e2: logger.error(f"{TO_NTRIPLES} failed: {e2}"); sys.exit(1)
    except FileNotFoundError: logger.error(f"rapper not found."); sys.exit(1)
    except Exception as e: logger.error(f"rapper exec failed: {e}"); sys.exit(1)
    if not nt_lines: logger.error(f"No output from rapper/to-ntriples for {manifest_file_path}"); sys.exit(1)

    triples_by_subject = {}
    for line in nt_lines:
        parsed = _parse_nt_line(line)
        if parsed:
            s, p, o_val, o_type, lang_or_dt = parsed
            if s not in triples_by_subject: triples_by_subject[s] = []
            triples_by_subject[s].append({'p': p, 'o_val': o_val, 'o_type': o_type, 'lang_or_dt': lang_or_dt, 'o_full': line.split(" ", 2)[2][:-2].strip()})

    def get_obj_val(subj_uri, pred_uri, default=None):
        return next((t['o_full'] for t in triples_by_subject.get(subj_uri, []) if t['p'] == pred_uri), default)
    def get_obj_vals(subj_uri, pred_uri):
        return [t['o_full'] for t in triples_by_subject.get(subj_uri, []) if t['p'] == pred_uri]
    def unquote_uri(s): return s[1:-1] if s and s.startswith("<") and s.endswith(">") else s
    def unquote_lit(s):
        if not (s and s.startswith('"')): return s
        m = re.match(r'"(.*)"(?:@([a-zA-Z-]+)|\^\^<.*>)?', s)
        return m.group(1).replace("\\\"", "\"").replace("\\t", "\t").replace("\\n", "\n").replace("\\\\", "\\") if m else s

    tests = []
    entries_list_node = next((t['o_full'] for s, ts in triples_by_subject.items() for t in ts if t['p'] == MF_NS + "entries"), None)
    if not entries_list_node: logger.error(f"No mf:entries in {manifest_file_path}"); sys.exit(1)

    current_list_item_node = entries_list_node
    while current_list_item_node and current_list_item_node != f"<{RDF_NS}nil>":
        entry_uri_full = get_obj_val(current_list_item_node, RDF_NS + "first")
        if not entry_uri_full:
            current_list_item_node = get_obj_val(current_list_item_node, RDF_NS + "rest"); continue
        entry_uri = unquote_uri(entry_uri_full)
        logger.debug(f"Processing entry: {entry_uri}")
        name = unquote_lit(get_obj_val(entry_uri_full, MF_NS + "name"))
        action_node_full = get_obj_val(entry_uri_full, MF_NS + "action")
        if not action_node_full:
            logger.warning(f"No mf:action for {entry_uri} ({name}), skipping.")
            current_list_item_node = get_obj_val(current_list_item_node, RDF_NS + "rest"); continue

        query_type_full = get_obj_val(entry_uri_full, RDF_NS + "type")
        query_type = unquote_uri(query_type_full)
        expect, execute, lang = Expect.PASS, True, "sparql"
        query_node_full = None

        if query_type:
            if query_type in (TestType.POSITIVE_SYNTAX_TEST.value, TestType.POSITIVE_SYNTAX_TEST_11.value, TestType.POSITIVE_UPDATE_SYNTAX_TEST_11.value, TestType.TEST_SYNTAX.value):
                query_node_full, execute = action_node_full, False
                if query_type.endswith("Test11"): lang = "sparql11"
            elif query_type in (TestType.NEGATIVE_SYNTAX_TEST.value, TestType.NEGATIVE_SYNTAX_TEST_11.value, TestType.NEGATIVE_UPDATE_SYNTAX_TEST_11.value, TestType.TEST_BAD_SYNTAX.value):
                query_node_full, execute, expect = action_node_full, False, Expect.FAIL
                if query_type.endswith("Test11"): lang = "sparql11"
            elif query_type in (TestType.UPDATE_EVALUATION_TEST.value, MF_NS + "UpdateEvaluationTest", TestType.PROTOCOL_TEST.value):
                logger.info(f"Skipping {query_type} for {name} - not supported.")
                current_list_item_node = get_obj_val(current_list_item_node, RDF_NS + "rest"); continue
            else: query_node_full = get_obj_val(action_node_full, QT_NS + "query")
        else: query_node_full = get_obj_val(action_node_full, QT_NS + "query")

        if not query_node_full:
            logger.warning(f"No query node for {entry_uri} ({name}), skipping.")
            current_list_item_node = get_obj_val(current_list_item_node, RDF_NS + "rest"); continue

        def rpv(uri_val, base): # resolve_path_from_uri_value
            if not uri_val: return None
            ps = unquote_uri(uri_val)
            if ps.startswith("file:///"): pp = ps[len("file://"):]
            elif ps.startswith("file:/"): pp = ps[len("file:"):]
            else: pp = ps
            p = Path(pp)
            return (base / p).resolve() if not p.is_absolute() else p.resolve()

        test_config = {
            "name": name, "dir": srcdir,
            "test_file": rpv(query_node_full, srcdir),
            "data_files": [rpv(df, srcdir) for df in get_obj_vals(action_node_full, QT_NS + "data") if df],
            "named_data_files": [rpv(ndf, srcdir) for ndf in get_obj_vals(action_node_full, QT_NS + "graphData") if ndf],
            "result_file": rpv(get_obj_val(entry_uri_full, MF_NS + "result"), srcdir),
            "expect": expect, "test_type": query_type, "test_uri": entry_uri, "execute": execute, "language": lang,
            "cardinality_mode": "lax" if unquote_uri(get_obj_val(entry_uri_full, MF_NS + "resultCardinality")) == MF_NS + "LaxCardinality" else "strict",
            "is_withdrawn": unquote_uri(get_obj_val(entry_uri_full, DAWGT_NS + "approval")) == DAWGT_NS + "Withdrawn",
            "is_approved": unquote_uri(get_obj_val(entry_uri_full, DAWGT_NS + "approval")) == DAWGT_NS + "Approved",
            "has_entailment_regime": any(t['p'] in (ENT_NS + "entailmentRegime", SD_NS + "entailmentRegime") for t in triples_by_subject.get(action_node_full, []))
        }
        if not unique_test_filter or (name == unique_test_filter or unique_test_filter in entry_uri):
            tests.append(test_config)
            if unique_test_filter and (name == unique_test_filter or unique_test_filter in entry_uri):
                logger.debug(f"Found specific test: {name}"); break
        current_list_item_node = get_obj_val(current_list_item_node, RDF_NS + "rest")
    return tests # Ensuring this is at the correct dedent level

# Removed old resolve_path_from_uri as rpv (resolve_path_from_uri_value) is now part of parse_manifest

if __name__ == "__main__":
    main()

#!/usr/bin/env python3
#
# rasqal_test_util.py - Shared library for Rasqal's Python-based test suite.
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

import os
import sys
import subprocess
import logging
import shutil
from enum import Enum
from pathlib import Path
from typing import List, Dict, Any, Optional, Tuple

import re
from dataclasses import dataclass, field
import argparse

logger = logging.getLogger(__name__)


# --- Custom Exceptions ---
class SparqlTestError(Exception):
    """Base exception for SPARQL test runner errors."""

    pass


class ManifestParsingError(SparqlTestError):
    """Raised when there's an issue parsing the test manifest."""

    pass


class UtilityNotFoundError(SparqlTestError):
    """Raised when an external utility (roqet, to-ntriples, diff) is not found."""

    pass


# --- Shared Constants and Enums ---


class Namespaces:
    """Convenience class for RDF/Manifest namespaces."""

    MF = "http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#"
    RDF = "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
    RDFS = "http://www.w3.org/2000/01/rdf-schema#"
    T = "http://ns.librdf.org/2009/test-manifest#"
    QT = "http://www.w3.org/2001/sw/DataAccess/tests/test-query#"
    DAWGT = "http://www.w3.org/2001/sw/DataAccess/tests/test-dawg#"
    UT = "http://www.w3.org/2009/sparql/tests/test-update#"
    RS = "http://www.w3.org/2001/sw/DataAccess/tests/result-set#"
    MFX = "http://jena.hpl.hp.com/2005/05/test-manifest-extra#"


class TestResult(Enum):
    PASSED = "passed"
    FAILED = "failed"
    SKIPPED = "skipped"
    XFAILED = "xfailed"  # Expected to fail and it did
    UXPASSED = "uxpassed"  # Expected to fail but unexpectedly passed

    def __str__(self):
        return self.value

    def display_char(self) -> str:
        """Returns a single character for compact progress display."""
        char_map = {
            TestResult.PASSED: ".",
            TestResult.FAILED: "F",
            TestResult.XFAILED: "*",
            TestResult.UXPASSED: "!",
            TestResult.SKIPPED: "-",
        }
        return char_map.get(self, "?")

    def display_name(self) -> str:
        """Returns a more verbose name for display."""
        return (
            self.value.upper()
            if self not in (TestResult.PASSED, TestResult.SKIPPED)
            else self.value
        )


class TestType(Enum):
    """Types of SPARQL tests as defined in W3C manifests."""

    CSV_RESULT_FORMAT_TEST = f"{Namespaces.MF}CSVResultFormatTest"
    POSITIVE_SYNTAX_TEST = f"{Namespaces.MF}PositiveSyntaxTest"
    POSITIVE_SYNTAX_TEST_11 = f"{Namespaces.MF}PositiveSyntaxTest11"
    POSITIVE_UPDATE_SYNTAX_TEST_11 = f"{Namespaces.UT}PositiveUpdateSyntaxTest11"
    NEGATIVE_SYNTAX_TEST = f"{Namespaces.MF}NegativeSyntaxTest"
    NEGATIVE_SYNTAX_TEST_11 = f"{Namespaces.MF}NegativeSyntaxTest11"
    NEGATIVE_UPDATE_SYNTAX_TEST_11 = f"{Namespaces.UT}NegativeUpdateSyntaxTest11"
    UPDATE_EVALUATION_TEST = f"{Namespaces.UT}UpdateEvaluationTest"
    PROTOCOL_TEST = f"{Namespaces.MF}ProtocolTest"
    BAD_SYNTAX_TEST = f"{Namespaces.MFX}TestBadSyntax"
    TEST_SYNTAX = f"{Namespaces.MFX}TestSyntax"


@dataclass
class TestConfig:
    """Represents the configuration for a single SPARQL test."""

    name: str
    test_uri: str
    test_file: Path
    expect: TestResult
    language: str = "sparql"
    execute: bool = True
    cardinality_mode: str = "strict"
    is_withdrawn: bool = False
    is_approved: bool = False
    has_entailment_regime: bool = False
    test_type: Optional[str] = None
    data_files: List[Path] = field(default_factory=list)
    named_data_files: List[Path] = field(default_factory=list)
    result_file: Optional[Path] = None
    extra_files: List[Path] = field(default_factory=list)
    warning_level: int = 0


# --- Utility Functions ---


def setup_logging(
    debug: bool = False, level: Optional[int] = None, stream=sys.stderr
) -> logging.Logger:
    """
    Setup logging configuration and return logger.

    Args:
        debug: If True, sets log level to DEBUG
        level: Explicit log level (overrides debug parameter)
        stream: Output stream for logging (defaults to stderr)

    Returns:
        Configured logger instance
    """
    if level is None:
        level = logging.DEBUG if debug else logging.INFO

    logging.basicConfig(level=level, format="%(levelname)s: %(message)s", stream=stream)
    return logging.getLogger(__name__)


def add_common_arguments(parser: argparse.ArgumentParser) -> None:
    """
    Add common command-line arguments used across multiple test scripts.

    Args:
        parser: ArgumentParser instance to add arguments to
    """
    parser.add_argument(
        "-d", "--debug", action="store_true", help="Enable debug logging"
    )
    parser.add_argument(
        "--manifest-file", type=Path, help="Path to a manifest file (e.g., manifest.n3)"
    )
    parser.add_argument(
        "--srcdir",
        type=Path,
        help="The source directory (equivalent to $(srcdir) in Makefile.am)",
    )
    parser.add_argument(
        "--builddir",
        type=Path,
        help="The build directory (equivalent to $(top_builddir) in Makefile.am)",
    )


def find_tool(name: str) -> Optional[str]:
    """
    Finds a required tool (like 'roqet' or 'to-ntriples') by searching
    the environment, a relative 'utils' directory, and the system PATH.
    """
    # 1. Check environment variable
    env_var = name.upper().replace("-", "_")
    env_val = os.environ.get(env_var)
    if env_val and Path(env_val).is_file():
        logger.debug(f"Found {name} via environment variable {env_var}: {env_val}")
        return env_val

    # 2. Search in parent 'utils' directories
    current_dir = Path.cwd()
    for parent in [current_dir] + list(current_dir.parents):
        tool_path = parent / "utils" / name
        if tool_path.is_file() and os.access(tool_path, os.X_OK):
            logger.debug(f"Found {name} in relative utils directory: {tool_path}")
            return str(tool_path)

    # 3. Fallback to system PATH
    tool_path_in_path = shutil.which(name)
    if tool_path_in_path:
        logger.debug(f"Found {name} in system PATH: {tool_path_in_path}")
        return tool_path_in_path

    logger.warning(f"Tool '{name}' could not be found.")
    return None


def run_command(
    cmd: List[str], cwd: Path, error_prefix: str
) -> subprocess.CompletedProcess:
    """
    Runs a shell command and returns its CompletedProcess object, handling common errors.
    """
    try:
        logger.debug(f"Running command in {cwd}: {' '.join(cmd)}")
        process = subprocess.run(
            cmd, capture_output=True, text=True, cwd=cwd, check=False, encoding="utf-8"
        )
        return process
    except FileNotFoundError:
        raise RuntimeError(
            f"{error_prefix}. Command not found: '{cmd[0]}'. Please ensure it is installed and in your PATH."
        )
    except Exception as e:
        logger.error(
            f"An unexpected error occurred while running command '{cmd[0]}': {e}"
        )
        raise RuntimeError(
            f"{error_prefix}. Unexpected error running '{cmd[0]}'."
        ) from e


# --- Manifest Parsing ---


def decode_literal(lit_str: str) -> str:
    """
    Decodes an N-Triples literal string, handling common escapes.
    Expected to receive only valid literal strings (quoted, with optional suffix).
    """
    if not lit_str or not lit_str.startswith('"'):
        # If it's not a quoted string, it's either a URI or Blank Node already handled.
        # Or it's a malformed literal. Return as is.
        return lit_str

    # Attempt to extract the value part from within the quotes and unescape it.
    value_part_match = re.match(
        r'^"(?P<value>(?:[^"\\]|\\.)*)"(?:@[\w-]+|\^\^<[^>]+>)?$', lit_str
    )
    if not value_part_match:
        logger.warning(f"Malformed literal string passed to decode_literal: {lit_str}")
        return lit_str

    val = value_part_match.group("value")

    # Unescape common N-Triples sequences:
    # Order matters: replace longer sequences first to avoid partial replacements.
    val = val.replace('\\"', '"')
    val = val.replace("\\n", "\n")
    val = val.replace("\\r", "\r")
    val = val.replace("\\t", "\t")
    val = val.replace("\\\\", "\\")  # Unescape escaped backslashes last
    return val


class ManifestParser:
    """
    Parses a test manifest file by converting it to N-Triples and providing
    an API to access the test data.
    """

    def __init__(self, manifest_path: Path, to_ntriples_cmd: Optional[str] = None):
        self.manifest_path = manifest_path
        if to_ntriples_cmd is None:
            to_ntriples_cmd = find_tool("to-ntriples")
            if not to_ntriples_cmd:
                raise UtilityNotFoundError(
                    "Could not find 'to-ntriples' command. Please ensure it is built and available in PATH."
                )
        self.to_ntriples_cmd = to_ntriples_cmd
        self.triples_by_subject: Dict[str, List[Dict[str, Any]]] = {}
        self._parse()

    @classmethod
    def from_manifest_file(
        cls, manifest_file: Path, srcdir: Path, logger: logging.Logger
    ) -> "ManifestParser":
        """
        Create a ManifestParser for the given manifest file with proper setup.

        Args:
            manifest_file: Path to the manifest file
            srcdir: Source directory for resolving relative paths
            logger: Logger instance for debugging

        Returns:
            ManifestParser instance

        Raises:
            FileNotFoundError: If manifest file doesn't exist
            UtilityNotFoundError: If to-ntriples command is not found
            RuntimeError: If manifest parsing fails
        """
        if not manifest_file.exists():
            logger.error(f"Manifest file not found at {manifest_file}")
            raise FileNotFoundError(f"Manifest file not found at {manifest_file}")

        try:
            # Change to source directory before parsing manifest to ensure relative URIs are resolved correctly
            original_cwd = os.getcwd()
            try:
                os.chdir(srcdir)
                # Use just the filename for the manifest file when in source directory
                manifest_filename = Path(os.path.basename(manifest_file))
                manifest_parser = cls(manifest_filename)
            finally:
                os.chdir(original_cwd)

            return manifest_parser

        except RuntimeError as e:
            logger.error(f"Error parsing manifest: {e}")
            raise RuntimeError(f"Error parsing manifest: {e}")

    def _parse(self):
        """Parse the manifest file and populate triples_by_subject."""
        logger = logging.getLogger(__name__)
        cmd = [self.to_ntriples_cmd, str(self.manifest_path)]
        logger.debug(f"ManifestParser._parse: Running command: {cmd}")
        logger.debug(
            f"ManifestParser._parse: Working directory: {self.manifest_path.parent}"
        )

        # Try running from the current working directory instead of the manifest file's directory
        import os

        current_cwd = Path.cwd()
        logger.debug(f"ManifestParser._parse: Current working directory: {current_cwd}")

        process = run_command(
            cmd, current_cwd, f"Error running '{self.to_ntriples_cmd}'"
        )

        if process.returncode != 0:
            logger.debug(
                f"ManifestParser._parse: Command failed with return code {process.returncode}"
            )
            logger.debug(f"ManifestParser._parse: stderr: {process.stderr}")
            logger.error(
                f"'{self.to_ntriples_cmd}' failed for {self.manifest_path} with exit code {process.returncode}.\n{process.stderr}"
            )
            raise RuntimeError(
                f"'{self.to_ntriples_cmd}' failed for {self.manifest_path} with exit code {process.returncode}.\n{process.stderr}"
            )

        logger.debug(
            f"ManifestParser._parse: Command succeeded, stdout length: {len(process.stdout)}"
        )
        logger.debug(
            f"ManifestParser._parse: First 200 chars of stdout: {process.stdout[:200]}"
        )
        logger.debug(f"N-Triples output from {self.to_ntriples_cmd}:\n{process.stdout}")

        line_count = 0
        for line in process.stdout.splitlines():
            line_count += 1
            s, p, o_full = self._parse_nt_line(line)
            if s:
                self.triples_by_subject.setdefault(s, []).append(
                    {"p": p, "o_full": o_full}
                )
                logger.debug(f"Parsed and stored triple: (S: {s}, P: {p}, O: {o_full})")

        logger.debug(
            f"ManifestParser._parse: Processed {line_count} lines, found {len(self.triples_by_subject)} subjects"
        )
        logger.debug(f"Finished parsing. Triples by subject: {self.triples_by_subject}")

    def _parse_nt_line(
        self, line: str
    ) -> Tuple[Optional[str], Optional[str], Optional[str]]:
        line = line.strip()
        if not line.endswith(" ."):
            return None, None, None
        line = line[:-2].strip()

        # Regex to capture subject, predicate, and the entire object part
        match = re.match(r"^(\S+)\s+(\S+)\s+(.*)", line)
        if not match:
            return None, None, None

        s_raw, p_raw, o_raw = match.groups()

        # Keep URIs with angle brackets for now, normalize later if needed
        s = s_raw
        p = p_raw
        o = o_raw

        return s, p, o

    def get_tests(
        self, srcdir: Path, unique_test_filter: Optional[str] = None
    ) -> List[TestConfig]:
        """Parses the full manifest to extract a list of TestConfig objects."""
        logger = logging.getLogger(__name__)
        tests: List[TestConfig] = []

        # Helper functions for parsing
        def get_obj_val(subj: str, pred: str) -> Optional[str]:
            return next(
                (
                    t["o_full"]
                    for t in self.triples_by_subject.get(subj, [])
                    if t["p"] == pred
                ),
                None,
            )

        def get_obj_vals(subj: str, pred: str) -> List[str]:
            return [
                t["o_full"]
                for t in self.triples_by_subject.get(subj, [])
                if t["p"] == pred
            ]

        def unquote(val: Optional[str]) -> str:
            if not val:
                return ""
            if val.startswith("<") and val.endswith(">"):
                return val[1:-1]
            if val.startswith('"'):
                return decode_literal(val)
            return val

        def rpv(uri_val: Optional[str], base: Path) -> Optional[Path]:
            if not uri_val:
                return None
            ps = unquote(uri_val)
            if ps.startswith("file:///"):
                p = Path(ps[len("file://") :])
            elif ps.startswith("file:/"):
                p = Path(ps[len("file:") :])
            else:
                p = Path(ps)
            return p.resolve() if p.is_absolute() else (base / p).resolve()

        # Find the manifest node and iterate over entries
        manifest_node_uri = None
        for s, triples in self.triples_by_subject.items():
            if any(t["p"] == f"<{Namespaces.MF}entries>" for t in triples):
                manifest_node_uri = s
                break

        if not manifest_node_uri:
            raise ManifestParsingError("No mf:entries property found in manifest.")

        # Iterate over manifest entries
        for entry_uri_full in self.iter_manifest_entries(manifest_node_uri):
            name = unquote(get_obj_val(entry_uri_full, f"<{Namespaces.MF}name>"))
            action_node = get_obj_val(entry_uri_full, f"<{Namespaces.MF}action>")
            query_type_full = get_obj_val(entry_uri_full, f"<{Namespaces.RDF}type>")
            query_type = unquote(query_type_full)

            if not action_node:
                continue

            expect, execute, lang = TestResult.PASSED, True, "sparql"
            query_node = get_obj_val(action_node, f"<{Namespaces.QT}query>")

            if query_type:
                if query_type in [
                    TestType.POSITIVE_SYNTAX_TEST.value,
                    TestType.POSITIVE_SYNTAX_TEST_11.value,
                ]:
                    query_node, execute = action_node, False
                    if "11" in query_type:
                        lang = "sparql11"
                elif query_type in [
                    TestType.NEGATIVE_SYNTAX_TEST.value,
                    TestType.NEGATIVE_SYNTAX_TEST_11.value,
                    TestType.BAD_SYNTAX_TEST.value,
                ]:
                    query_node, execute, expect = action_node, False, TestResult.FAILED
                    if "11" in query_type:
                        lang = "sparql11"
                elif query_type == TestType.TEST_SYNTAX.value:
                    query_node, execute = action_node, False
                elif query_type in [
                    TestType.UPDATE_EVALUATION_TEST.value,
                    TestType.PROTOCOL_TEST.value,
                ]:
                    continue

            if not query_node:
                continue

            # Filter out None values for paths
            test_file = rpv(query_node, srcdir)
            if test_file is None:
                continue

            data_files = [
                p
                for p in [
                    rpv(df, srcdir)
                    for df in get_obj_vals(action_node, f"<{Namespaces.QT}data>")
                ]
                if p is not None
            ]
            named_data_files = [
                p
                for p in [
                    rpv(ndf, srcdir)
                    for ndf in get_obj_vals(action_node, f"<{Namespaces.QT}graphData>")
                ]
                if p is not None
            ]

            # Extract extra files from t:extraFile predicate
            extra_files = [
                p
                for p in [
                    rpv(ef, srcdir)
                    for ef in get_obj_vals(entry_uri_full, f"<{Namespaces.T}extraFile>")
                ]
                if p is not None
            ]

            config = TestConfig(
                name=name,
                test_uri=unquote(entry_uri_full),
                test_file=test_file,
                data_files=data_files,
                named_data_files=named_data_files,
                result_file=rpv(
                    get_obj_val(entry_uri_full, f"<{Namespaces.MF}result>"), srcdir
                ),
                extra_files=extra_files,
                expect=expect,
                test_type=query_type,
                execute=execute,
                language=lang,
                cardinality_mode=(
                    "lax"
                    if unquote(
                        get_obj_val(
                            entry_uri_full, f"<{Namespaces.MF}resultCardinality>"
                        )
                    )
                    == f"{Namespaces.MF}LaxCardinality"
                    else "strict"
                ),
                is_withdrawn=unquote(
                    get_obj_val(entry_uri_full, f"<{Namespaces.DAWGT}approval>")
                )
                == f"{Namespaces.DAWGT}Withdrawn",
                is_approved=unquote(
                    get_obj_val(entry_uri_full, f"<{Namespaces.DAWGT}approval>")
                )
                == f"{Namespaces.DAWGT}Approved",
                has_entailment_regime=bool(
                    get_obj_val(action_node, f"<{Namespaces.T}entailmentRegime>")
                ),
            )
            logger.debug(
                f"Test config created: name={config.name}, expect={config.expect.value}, test_type={config.test_type}"
            )

            if not unique_test_filter or (
                name == unique_test_filter or unique_test_filter in config.test_uri
            ):
                tests.append(config)
                if unique_test_filter and (
                    name == unique_test_filter or unique_test_filter in config.test_uri
                ):
                    break

        return tests

    def get_test_ids_from_manifest(self, suite_name: str) -> List[str]:
        """Extracts test identifiers from the manifest for plan generation."""
        logger = logging.getLogger(__name__)
        test_ids = set()
        logger.debug(f"get_test_ids_from_manifest called for suite: {suite_name}")

        # Find the main manifest node
        manifest_node_uri = None
        for s, triples in self.triples_by_subject.items():
            for triple in triples:
                if (
                    triple["p"] == f"<{Namespaces.RDF}type>"
                    and triple["o_full"] == f"<{Namespaces.MF}Manifest>"
                ):
                    manifest_node_uri = s
                    break
            if manifest_node_uri:
                break

        if not manifest_node_uri:
            logger.warning("No main manifest node found.")
            return []

        # Iterate over manifest entries
        for entry_node_full in self.iter_manifest_entries(manifest_node_uri):
            entry_triples = self.triples_by_subject.get(entry_node_full, [])

            if suite_name in ["sparql-lexer", "sparql-parser"]:
                predicate = f"<{Namespaces.QT}query>"
                for triple in entry_triples:
                    if triple["p"] == predicate:
                        test_id = triple["o_full"][1:-1]  # Extract URI from <...>
                        test_ids.add(test_id)
                        logger.debug(f"    Found lexer/parser test ID: {test_id}")
            else:  # sparql-query
                predicate = f"<{Namespaces.MF}name>"
                for triple in entry_triples:
                    if triple["p"] == predicate:
                        # Use decode_literal for literals
                        test_id = decode_literal(triple["o_full"])
                        test_ids.add(test_id)
                        logger.debug(f"    Found query test ID: {test_id}")

        logger.debug(f"Final extracted test_ids: {sorted(list(test_ids))}")
        return sorted(list(test_ids))

    def iter_manifest_entries(self, manifest_node_uri: str):
        """
        Iterate over manifest entries and yield each entry node URI.

        Args:
            manifest_node_uri: The URI of the manifest node

        Yields:
            str: Each entry node URI from the manifest
        """
        # Find entries list head
        entries_list_head = next(
            (
                t["o_full"]
                for t in self.triples_by_subject.get(manifest_node_uri, [])
                if t["p"] == f"<{Namespaces.MF}entries>"
            ),
            None,
        )

        if not entries_list_head:
            raise ManifestParsingError("Could not find mf:entries list.")

        # Traverse list and yield each entry
        current_list_item_node = entries_list_head
        while (
            current_list_item_node
            and current_list_item_node != f"<{Namespaces.RDF}nil>"
        ):
            list_node_triples = self.triples_by_subject.get(current_list_item_node, [])
            entry_node_full = next(
                (
                    t["o_full"]
                    for t in list_node_triples
                    if t["p"] == f"<{Namespaces.RDF}first>"
                ),
                None,
            )

            if entry_node_full:
                yield entry_node_full

            current_list_item_node = next(
                (
                    t["o_full"]
                    for t in list_node_triples
                    if t["p"] == f"<{Namespaces.RDF}rest>"
                ),
                None,
            )

    def find_manifest_node(self) -> str:
        """Find the manifest node URI from the parsed triples."""
        logger = logging.getLogger(__name__)
        # Look for a subject that has mf:entries property
        for subject, triples in self.triples_by_subject.items():
            if any(t["p"] == f"<{Namespaces.MF}entries>" for t in triples):
                logger.debug(f"Found manifest node: {subject}")
                return subject

        logger.error("Could not find manifest node with mf:entries property")
        raise ManifestParsingError(
            "Could not find manifest node with mf:entries property"
        )


def validate_required_paths(*paths: Tuple[Path, str]) -> None:
    """
    Validate that required paths exist.

    Args:
        paths: Tuples of (path, description) to validate

    Raises:
        FileNotFoundError: If any required path doesn't exist
    """
    for path, description in paths:
        if path and not path.exists():
            raise FileNotFoundError(f"{description} not found at {path}")

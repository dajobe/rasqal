"""
Manifest parsing and test discovery for SPARQL test suites.

This module provides classes and functions for parsing test manifest files
and discovering test cases within them.

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

import logging
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional, Set, Tuple

from .test_types import TestResult, Namespaces, TestTypeResolver
from .config import TestConfig, TestConfigBuilder, get_test_suite_config

# Import utility classes from the utils package
from .utils import (
    SparqlTestError,
    ManifestTripleExtractor,
    PathResolver,
    find_tool,
    run_command,
)


class UtilityNotFoundError(SparqlTestError):
    """Raised when an external utility (roqet, to-ntriples, diff) is not found."""

    pass


class ManifestParsingError(SparqlTestError):
    """Raised when there's an issue parsing the test manifest."""

    pass


class ManifestParser:
    """
    Parses a test manifest file by converting it to N-Triples and providing
    an API to access the test data.
    """

    def __init__(
        self,
        manifest_path: Path,
        to_ntriples_cmd: Optional[str] = None,
        skip_tool_validation: bool = False,
        debug_level: int = 0,
        builddir: Optional[Path] = None,
    ):
        """
        Initialize manifest parser.

        Args:
            manifest_path: Path to the manifest file
            to_ntriples_cmd: Optional path to to-ntriples command
            skip_tool_validation: Skip validation of to-ntriples tool (for packaging only)
            debug_level: Debug level for logging
            builddir: Build directory for finding tools (optional)

        Raises:
            UtilityNotFoundError: If to-ntriples command is not found and skip_tool_validation is False
        """
        self.manifest_path = manifest_path
        self.builddir = builddir
        if not skip_tool_validation:
            if to_ntriples_cmd is None:
                to_ntriples_cmd = find_tool("to-ntriples", str(builddir) if builddir else None)
                if not to_ntriples_cmd:
                    raise UtilityNotFoundError(
                        "Could not find 'to-ntriples' command. Please ensure it is built and available in PATH."
                    )
        self.to_ntriples_cmd = to_ntriples_cmd
        self.debug_level = debug_level
        self.triples_by_subject: Dict[str, List[Dict[str, Any]]] = {}
        if not skip_tool_validation:
            self._parse()

    @classmethod
    def from_manifest_file(
        cls, manifest_file: Path, srcdir: Path, logger: logging.Logger, builddir: Optional[Path] = None
    ) -> "ManifestParser":
        """
        Create a ManifestParser for the given manifest file with proper setup.

        Args:
            manifest_file: Path to the manifest file
            srcdir: Source directory for resolving relative paths
            logger: Logger instance for debugging
            builddir: Build directory for finding tools (optional)

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
                manifest_parser = cls(manifest_filename, builddir=builddir)
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
        current_cwd = Path.cwd()
        logger.debug(f"ManifestParser._parse: Current working directory: {current_cwd}")

        process = run_command(cmd, str(current_cwd))

        # Unpack the tuple returned by run_command
        returncode, stdout, stderr = process

        if returncode != 0:
            logger.debug(
                f"ManifestParser._parse: Command failed with return code {returncode}"
            )
            logger.debug(f"ManifestParser._parse: stderr: {stderr}")
            logger.error(
                f"'{self.to_ntriples_cmd}' failed for {self.manifest_path} with exit code {returncode}.\n{stderr}"
            )
            raise RuntimeError(
                f"'{self.to_ntriples_cmd}' failed for {self.manifest_path} with exit code {returncode}.\n{stderr}"
            )

        logger.debug(
            f"ManifestParser._parse: Command succeeded, stdout length: {len(stdout)}"
        )
        logger.debug(
            f"ManifestParser._parse: First 200 chars of stdout: {stdout[:200]}"
        )
        if self.debug_level >= 2:
            logger.debug(f"N-Triples output from {self.to_ntriples_cmd}:\n{stdout}")

        line_count = 0
        for line in stdout.splitlines():
            line_count += 1
            s, p, o_full = self._parse_nt_line(line)
            if s:
                self.triples_by_subject.setdefault(s, []).append(
                    {"p": p, "o_full": o_full}
                )
                if self.debug_level >= 2:
                    logger.debug(
                        f"Parsed and stored triple: (S: {s}, P: {p}, O: {o_full})"
                    )

        logger.debug(
            f"ManifestParser._parse: Processed {line_count} lines, found {len(self.triples_by_subject)} subjects"
        )
        if self.debug_level >= 2:
            logger.debug(
                f"Finished parsing. Triples by subject: {self.triples_by_subject}"
            )

    def _parse_nt_line(
        self, line: str
    ) -> Tuple[Optional[str], Optional[str], Optional[str]]:
        """
        Parse a single N-Triples line.

        Args:
            line: N-Triples line to parse

        Returns:
            Tuple of (subject, predicate, object) or (None, None, None) if invalid
        """
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

    def _create_helpers(self, srcdir: Path) -> Dict[str, Any]:
        """Create helper objects for test processing.

        Args:
            srcdir: Source directory for resolving paths

        Returns:
            Dictionary containing helper objects: extractor, path_resolver, config_builder
        """
        extractor = ManifestTripleExtractor(self.triples_by_subject)
        path_resolver = PathResolver(srcdir, extractor)
        config_builder = TestConfigBuilder(extractor, path_resolver)

        return {
            "extractor": extractor,
            "path_resolver": path_resolver,
            "config_builder": config_builder,
        }

    def _has_nested_manifests(
        self, manifest_node_uri: str, extractor: ManifestTripleExtractor
    ) -> bool:
        """Check if this manifest includes other manifests (SPARQL 1.1 style).

        Args:
            manifest_node_uri: URI of the manifest node
            extractor: Triple extractor for accessing manifest data

        Returns:
            True if the manifest has nested manifests, False otherwise
        """
        included_manifests = extractor.get_obj_vals(
            manifest_node_uri, f"<{Namespaces.MF}include>"
        )
        return bool(included_manifests)

    def _extract_included_files(self, list_head: str) -> List[str]:
        """
        Extract included manifest files from an RDF list structure.

        Args:
            list_head: The head of the RDF list (blank node)

        Returns:
            List of included manifest file URIs
        """
        included_files = []
        current_list_item = list_head

        while current_list_item and current_list_item != f"<{Namespaces.RDF}nil>":
            list_node_triples = self.triples_by_subject.get(current_list_item, [])

            # Get the first item in the list
            first_item = next(
                (
                    t["o_full"]
                    for t in list_node_triples
                    if t["p"] == f"<{Namespaces.RDF}first>"
                ),
                None,
            )

            if first_item:
                included_files.append(first_item)

            # Get the rest of the list
            current_list_item = next(
                (
                    t["o_full"]
                    for t in list_node_triples
                    if t["p"] == f"<{Namespaces.RDF}rest>"
                ),
                None,
            )

        return included_files

    def _process_nested_manifests(
        self,
        manifest_node_uri: str,
        srcdir: Path,
        unique_test_filter: Optional[str],
        helpers: Dict[str, Any],
    ) -> List[TestConfig]:
        """Process SPARQL 1.1 style nested manifests.

        Args:
            manifest_node_uri: URI of the manifest node
            srcdir: Source directory for resolving paths
            unique_test_filter: Optional filter for specific test
            helpers: Dictionary containing helper objects

        Returns:
            List of TestConfig objects from all nested manifests
        """
        logger = logging.getLogger(__name__)
        tests: List[TestConfig] = []
        extractor = helpers["extractor"]
        path_resolver = helpers["path_resolver"]

        included_manifests = extractor.get_obj_vals(
            manifest_node_uri, f"<{Namespaces.MF}include>"
        )

        for included_manifest_list_head in included_manifests:
            # Traverse the RDF list to extract all included manifest files
            included_files = self._extract_included_files(included_manifest_list_head)

            for included_file in included_files:
                included_path = path_resolver.resolve_path(included_file)
                if included_path and included_path.exists():
                    logger.debug(f"Processing included manifest: {included_path}")
                    try:
                        nested_parser = ManifestParser(
                            included_path,
                            self.to_ntriples_cmd,
                            debug_level=self.debug_level,
                        )
                        nested_tests = nested_parser.get_tests(
                            srcdir, unique_test_filter
                        )
                        tests.extend(nested_tests)
                    except Exception as e:
                        logger.warning(
                            f"Failed to process included manifest {included_path}: {e}"
                        )
                else:
                    logger.warning(
                        f"Could not resolve included manifest: {included_file}"
                    )

        return tests

    def _process_direct_entries(
        self,
        manifest_node_uri: str,
        srcdir: Path,
        unique_test_filter: Optional[str],
        helpers: Dict[str, Any],
    ) -> List[TestConfig]:
        """Process traditional direct manifest entries.

        Args:
            manifest_node_uri: URI of the manifest node
            srcdir: Source directory for resolving paths
            unique_test_filter: Optional filter for specific test
            helpers: Dictionary containing helper objects

        Returns:
            List of TestConfig objects from direct entries
        """
        logger = logging.getLogger(__name__)
        tests: List[TestConfig] = []
        extractor = helpers["extractor"]
        path_resolver = helpers["path_resolver"]
        config_builder = helpers["config_builder"]

        for entry_uri_full in self.iter_manifest_entries(manifest_node_uri):
            # Extract basic entry information
            action_node = extractor.get_obj_val(
                entry_uri_full, f"<{Namespaces.MF}action>"
            )
            if not action_node:
                continue

            query_type = extractor.unquote(
                extractor.get_obj_val(entry_uri_full, f"<{Namespaces.RDF}type>")
            )

            # Skip unsupported test types
            if TestTypeResolver.should_skip_test_type(query_type):
                continue

            # Determine test behavior and query node
            execute, expect, language = TestTypeResolver.resolve_test_behavior(
                query_type
            )

            # For syntax tests, the action node is the query node
            if not execute:
                query_node = action_node
            else:
                query_node = extractor.get_obj_val(
                    action_node, f"<{Namespaces.QT}query>"
                )
                if not query_node:
                    continue

            # Resolve test file path
            test_file = path_resolver.resolve_path(query_node)
            if test_file is None:
                logger.warning(f"Could not resolve test file for {entry_uri_full}")
                continue

            # Build the test config
            config = config_builder.build_test_config(
                entry_uri_full,
                action_node,
                query_node,
                test_file,
                execute,
                expect,
                language,
                query_type,
            )

            logger.debug(
                f"Test config created: name={config.name}, expect={config.expect.value}, test_type={config.test_type}"
            )

            # Apply filtering
            if self._should_include_test(config, unique_test_filter):
                tests.append(config)
                if unique_test_filter and self._matches_filter(
                    config, unique_test_filter
                ):
                    break

        return tests

    def _should_include_test(
        self, config: TestConfig, unique_test_filter: Optional[str]
    ) -> bool:
        """Determine if a test should be included based on filtering criteria.

        Args:
            config: TestConfig object to evaluate
            unique_test_filter: Optional filter string

        Returns:
            True if the test should be included, False otherwise
        """
        if not unique_test_filter:
            return True
        return self._matches_filter(config, unique_test_filter)

    def _matches_filter(self, config: TestConfig, unique_test_filter: str) -> bool:
        """Check if a test matches the given filter.

        Args:
            config: TestConfig object to check
            unique_test_filter: Filter string to match against

        Returns:
            True if the test matches the filter, False otherwise
        """
        return (
            config.name == unique_test_filter or unique_test_filter in config.test_uri
        )

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
        """
        Find the manifest node URI from the parsed triples.

        Returns:
            URI of the manifest node

        Raises:
            ManifestParsingError: If no manifest node is found
        """
        logger = logging.getLogger(__name__)
        # Look for a subject that has mf:entries property (traditional manifests)
        for subject, triples in self.triples_by_subject.items():
            if any(t["p"] == f"<{Namespaces.MF}entries>" for t in triples):
                logger.debug(f"Found manifest node with mf:entries: {subject}")
                return subject

        # Look for a subject that has mf:include property (SPARQL 1.1 nested manifests)
        for subject, triples in self.triples_by_subject.items():
            if any(t["p"] == f"<{Namespaces.MF}include>" for t in triples):
                logger.debug(f"Found manifest node with mf:include: {subject}")
                return subject

        logger.error("No manifest node found in parsed triples")
        raise ManifestParsingError("No manifest node found")

    def get_tests(
        self, srcdir: Path, unique_test_filter: Optional[str] = None
    ) -> List[TestConfig]:
        """
        Parses the full manifest to extract a list of TestConfig objects.

        Args:
            srcdir: Source directory for resolving file paths
            unique_test_filter: Optional filter to select specific test

        Returns:
            List of TestConfig objects representing all tests in the manifest
        """
        # Create helper instances
        helpers = self._create_helpers(srcdir)

        # Find the manifest node
        manifest_node_uri = self.find_manifest_node()

        # Check if this manifest includes other manifests (SPARQL 1.1 style)
        if self._has_nested_manifests(manifest_node_uri, helpers["extractor"]):
            return self._process_nested_manifests(
                manifest_node_uri, srcdir, unique_test_filter, helpers
            )
        else:
            return self._process_direct_entries(
                manifest_node_uri, srcdir, unique_test_filter, helpers
            )

    def get_test_ids_from_manifest(self, suite_name: str) -> List[str]:
        """
        Extracts test identifiers from the manifest for plan generation.

        Args:
            suite_name: Name of the test suite

        Returns:
            List of test identifier strings
        """
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

        # Get suite configuration
        suite_config = get_test_suite_config(suite_name)
        predicate = suite_config.get_test_id_predicate()
        logger.debug(f"Using predicate '{predicate}' for suite '{suite_name}'")

        # Check if this manifest includes other manifests (SPARQL 1.1 style)
        included_manifests = self.triples_by_subject.get(manifest_node_uri, [])
        included_manifest_list_heads = [
            t["o_full"]
            for t in included_manifests
            if t["p"] == f"<{Namespaces.MF}include>"
        ]

        if included_manifest_list_heads:
            # Process nested manifests
            for included_manifest_list_head in included_manifest_list_heads:
                included_files = self._extract_included_files(
                    included_manifest_list_head
                )
                for included_file in included_files:
                    logger.debug(f"Processing included file: {included_file}")
                    # For each included file, create a new parser and extract test IDs
                    # This is a simplified version - in practice you'd need proper path resolution
                    try:
                        included_parser = ManifestParser(
                            Path(included_file.strip("<>"))
                        )
                        nested_ids = included_parser.get_test_ids_from_manifest(
                            suite_name
                        )
                        test_ids.update(nested_ids)
                    except Exception as e:
                        logger.warning(
                            f"Failed to process included file {included_file}: {e}"
                        )
        else:
            # Process direct entries
            for entry_uri_full in self.iter_manifest_entries(manifest_node_uri):
                test_id_value = self.get_test_id_from_entry(entry_uri_full, predicate)
                if test_id_value:
                    test_ids.add(test_id_value)
                    logger.debug(f"Found test ID: {test_id_value}")

        return list(test_ids)

    def get_test_id_from_entry(self, entry_uri: str, predicate: str) -> Optional[str]:
        """
        Extract test ID from a manifest entry using the specified predicate.

        Args:
            entry_uri: URI of the manifest entry
            predicate: RDF predicate to use for extraction

        Returns:
            Test ID string if found, None otherwise
        """
        extractor = ManifestTripleExtractor(self.triples_by_subject)

        # First try to get the value directly
        test_id_raw = extractor.get_obj_val(entry_uri, predicate)
        if test_id_raw:
            return extractor.unquote(test_id_raw)

        # If not found, try getting from action node for some test types
        action_node = extractor.get_obj_val(entry_uri, f"<{Namespaces.MF}action>")
        if action_node:
            test_id_raw = extractor.get_obj_val(action_node, predicate)
            if test_id_raw:
                return extractor.unquote(test_id_raw)

        return None

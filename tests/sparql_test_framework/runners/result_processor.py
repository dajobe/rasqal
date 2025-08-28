"""
Result Processor Module for SPARQL Test Results

This module handles the processing of expected results files for different
result types (graph, bindings, boolean), extracting complex result processing
logic from the main sparql.py file.

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
from pathlib import Path
from typing import Dict, Any, List, Optional

logger = logging.getLogger(__name__)


class ResultProcessor:
    """Handles processing of expected results files for different result types."""

    def __init__(self):
        self.logger = logging.getLogger(__name__)

    def process_expected_graph_results(
        self, expected_result_file: Path, name: str, global_debug_level: int
    ) -> Optional[int]:
        """Process expected RDF graph results file."""
        if global_debug_level > 0:
            self.logger.debug(
                f"Reading expected RDF graph result file {expected_result_file}"
            )

        # Import here to avoid circular imports
        from .sparql import (
            read_rdf_graph_file,
            normalize_blank_nodes,
            get_temp_file_path,
        )

        expected_ntriples = read_rdf_graph_file(expected_result_file)
        if expected_ntriples is not None:  # Allow empty graph
            # Normalize blank nodes in the expected result for consistent comparison
            normalized_expected = normalize_blank_nodes(expected_ntriples)
            sorted_expected_triples = sorted(
                list(set(normalized_expected.splitlines()))
            )
            # Always write to file for comparison, cleanup later if not preserving
            # Write normalized N-Triples to output path used by comparisons
            expected_sorted_nt = (
                "\n".join(sorted_expected_triples) + "\n"
                if sorted_expected_triples
                else ""
            )
            get_temp_file_path("result.out").write_text(expected_sorted_nt)
            return len(sorted_expected_triples)
        else:
            self.logger.warning(
                f"Test '{name}': FAILED (could not read/parse expected graph result file {expected_result_file} or it's not explicitly empty)"
            )
            return None

    def process_expected_bindings_results(
        self,
        expected_result_file: Path,
        actual_result_type: str,
        actual_vars_order: List[str],
        is_sorted_by_query_from_actual: bool,
        name: str,
        global_debug_level: int,
    ) -> Optional[int]:
        """Process expected bindings/boolean results file."""
        # Import here to avoid circular imports
        from .sparql import (
            detect_result_format,
            read_query_results_file,
            get_temp_file_path,
        )

        expected_result_format = detect_result_format(expected_result_file)

        if global_debug_level > 0:
            self.logger.debug(
                f"Reading expected '{actual_result_type}' result file {expected_result_file} (format: {expected_result_format})"
            )

        should_sort_expected = not is_sorted_by_query_from_actual
        expected_results_info = read_query_results_file(
            expected_result_file,
            expected_result_format,
            actual_vars_order,
            sort_output=should_sort_expected,
        )

        if expected_results_info:
            return expected_results_info.get("count", 0)
        else:
            # Handle cases where result_file exists but is empty or unparsable as empty
            if (
                expected_result_file.exists()
                and expected_result_file.stat().st_size == 0
            ):
                get_temp_file_path("result.out").write_text("")
                return 0
            else:
                self.logger.warning(
                    f"Test '{name}': FAILED (could not read/parse expected results file {expected_result_file} or it's not explicitly empty)"
                )
                return None


# Convenience functions for backward compatibility
def process_expected_graph_results(
    expected_result_file: Path, name: str, global_debug_level: int
) -> Optional[int]:
    """Backward compatibility wrapper for ResultProcessor.process_expected_graph_results."""
    processor = ResultProcessor()
    return processor.process_expected_graph_results(
        expected_result_file, name, global_debug_level
    )


def process_expected_bindings_results(
    expected_result_file: Path,
    actual_result_type: str,
    actual_vars_order: List[str],
    is_sorted_by_query_from_actual: bool,
    name: str,
    global_debug_level: int,
) -> Optional[int]:
    """Backward compatibility wrapper for ResultProcessor.process_expected_bindings_results."""
    processor = ResultProcessor()
    return processor.process_expected_bindings_results(
        expected_result_file,
        actual_result_type,
        actual_vars_order,
        is_sorted_by_query_from_actual,
        name,
        global_debug_level,
    )

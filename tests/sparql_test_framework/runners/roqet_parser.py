"""
Roqet Parser Module for SPARQL Test Results

This module handles parsing of results files using the roqet command-line tool,
extracting complex roqet parsing logic from the main sparql.py file.

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

# Constants
ROQET = "roqet"
CURDIR = Path.cwd()


class RoqetParser:
    """Handles parsing of results files using the roqet command-line tool."""

    def __init__(self):
        self.logger = logging.getLogger(__name__)

    def parse_with_roqet(
        self,
        result_file_path: Path,
        result_format_hint: str,
        expected_vars_order: List[str],
        sort_output: bool,
    ) -> Optional[Dict[str, Any]]:
        """Parse results file using roqet command-line tool."""
        abs_result_file_path = result_file_path.resolve()
        cmd = [
            ROQET,
            "-q",
            "-R",
            result_format_hint,
            "-r",
            "simple",
            "-t",
            str(abs_result_file_path),
        ]
        if self.logger.level == logging.DEBUG:
            self.logger.debug(f"(read_query_results_file): Running {' '.join(cmd)}")
        try:
            self.logger.debug(
                f"read_query_results_file: About to run command for {result_format_hint} format"
            )
            # Import here to avoid circular imports
            from .sparql import run_command

            process = run_command(
                cmd=cmd,
                cwd=str(CURDIR),
                error_msg=f"Error reading results file '{abs_result_file_path}' ({result_format_hint})",
            )
            returncode, stdout, stderr = process
            self.logger.debug(
                f"read_query_results_file: Command completed with rc={returncode}"
            )
            self.logger.debug(f"read_query_results_file: stdout length={len(stdout)}")
            self.logger.debug(f"read_query_results_file: stderr length={len(stderr)}")

            roqet_stderr_content = stderr

            # Accept roqet exit code 2 as a warning (success) when converting results
            if (returncode not in (0, 2)) or "Error" in roqet_stderr_content:
                self.logger.warning(
                    f"Reading results file '{abs_result_file_path}' ({result_format_hint}) FAILED."
                )
                if roqet_stderr_content:
                    self.logger.warning(
                        f"  Stderr(roqet):\n{roqet_stderr_content.strip()}"
                    )
                return None

            return self._process_roqet_output(stdout, expected_vars_order, sort_output)

        except Exception as e:
            self.logger.error(f"Error reading results file: {e}")
            return None

    def _process_roqet_output(
        self, stdout: str, expected_vars_order: List[str], sort_output: bool
    ) -> Dict[str, Any]:
        """Process the output from roqet command."""
        parsed_rows_for_output: List[str] = []
        current_vars_order: List[str] = []
        first_row = True
        order_to_use: List[str] = expected_vars_order if expected_vars_order else []

        # Handle empty result sets - if no stdout, it means the file was read successfully but contains no results
        if not stdout.strip():
            # This is a valid case for empty result sets
            self.logger.debug("Empty result set detected")
            # For empty results, we still need to determine variable order from the expected vars
            if expected_vars_order:
                order_to_use = expected_vars_order
            else:
                # If no expected vars, create an empty result
                order_to_use = []
        else:
            # Process non-empty results
            self.logger.debug(
                f"Processing non-empty results, stdout lines: {len(stdout.splitlines())}"
            )
            parsed_rows_for_output, order_to_use = self._parse_roqet_rows(
                stdout, expected_vars_order
            )

        self.logger.debug(f"Parsed rows count: {len(parsed_rows_for_output)}")
        self.logger.debug(f"Order to use: {order_to_use}")

        if sort_output:
            parsed_rows_for_output.sort()
            self.logger.debug(f"After sorting: {parsed_rows_for_output}")

        # Always write to file for comparison, cleanup later if not preserving
        # Import here to avoid circular imports
        from .sparql import get_temp_file_path

        result_out_path = get_temp_file_path("result.out")
        content_to_write = (
            "\n".join(parsed_rows_for_output) + "\n" if parsed_rows_for_output else ""
        )
        result_out_path.write_text(content_to_write)

        return {
            "content": (
                "\n".join(parsed_rows_for_output) + "\n"
                if parsed_rows_for_output
                else ""
            ),
            "count": len(parsed_rows_for_output),
            "vars_order": order_to_use,
            "is_sorted_by_query": not sort_output,
        }

    def _parse_roqet_rows(
        self, stdout: str, expected_vars_order: List[str]
    ) -> tuple[List[str], List[str]]:
        """Parse roqet output rows and extract variable order."""
        # Import here to avoid circular imports
        from .sparql import _parse_roqet_rows

        return _parse_roqet_rows(stdout, expected_vars_order)


# Convenience functions for backward compatibility
def parse_with_roqet(
    result_file_path: Path,
    result_format_hint: str,
    expected_vars_order: List[str],
    sort_output: bool,
) -> Optional[Dict[str, Any]]:
    """Backward compatibility wrapper for RoqetParser.parse_with_roqet."""
    parser = RoqetParser()
    return parser.parse_with_roqet(
        result_file_path, result_format_hint, expected_vars_order, sort_output
    )

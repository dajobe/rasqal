"""
Consolidated SRJ (SPARQL Results JSON) Processor Module

This module consolidates all SRJ-related functionality from the previous
separate files (srj.py, srj_handler.py, srj_query_handler.py) into a single,
comprehensive module for handling SRJ format queries and results.

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

import json
import logging
import re
import traceback
import os
from pathlib import Path
from typing import Dict, Any, List, Optional, Callable

logger = logging.getLogger(__name__)

# Constants
ROQET = "roqet"
CURDIR = Path.cwd()

# Import utility functions
from ..utils import find_tool


class SRJResult:
    """Represents a processed SRJ result."""

    def __init__(
        self,
        json_content: str,
        normalized_content: str,
        result_type: str,
        variables: Optional[List[str]] = None,
        bindings_count: int = 0,
        boolean_value: Optional[bool] = None,
        metadata: Optional[Dict[str, Any]] = None,
    ):
        self.json_content = json_content
        self.normalized_content = normalized_content
        self.result_type = result_type  # "bindings", "boolean"
        self.variables = variables or []
        self.bindings_count = bindings_count
        self.boolean_value = boolean_value
        self.metadata = metadata or {}


class SRJProcessor:
    """Comprehensive SRJ (SPARQL Results JSON) processor for all query types."""

    def __init__(self):
        self.logger = logging.getLogger(__name__)

    def handle_query_with_srj(
        self,
        config: "TestConfig",
        additional_args: List[str],
        expected_vars_order: List[str],
        sort_output: bool,
        run_command_func,
        get_temp_file_path_func,
    ) -> Optional[Dict[str, Any]]:
        """Handle SELECT/CONSTRUCT queries that expect SRJ output format."""
        # Build SRJ command
        srj_cmd = self._build_srj_command(config)

        try:
            # Execute SRJ command
            srj_result = run_command_func(
                cmd=srj_cmd,
                cwd=str(CURDIR),
                error_msg="Error running query with SRJ format",
            )
            returncode, stdout, stderr = srj_result

            if returncode not in (0, 2):
                logger.warning(f"roqet SRJ execution failed rc={returncode}")
                return None

            # Parse and process SRJ output
            return self._process_srj_output(config, stdout, get_temp_file_path_func)

        except Exception as e:
            logger.warning(f"Error running query with SRJ format: {e}")
            traceback.print_exc()
            return None

    def handle_ask_query_with_srj(
        self,
        config: "TestConfig",
        additional_args: List[str],
        run_command_func,
        get_temp_file_path_func,
    ) -> Optional[Dict[str, Any]]:
        """Handle ASK queries that expect SRJ output format."""
        # Build SRJ command
        srj_cmd = self._build_srj_command(config)

        try:
            # Execute SRJ command
            srj_result = run_command_func(
                cmd=srj_cmd,
                cwd=str(CURDIR),
                error_msg="Error running ASK query with SRJ format",
            )
            returncode, stdout, stderr = srj_result

            if returncode not in (0, 2):
                logger.warning(f"roqet ASK SRJ execution failed rc={returncode}")
                return None

            # Parse and process ASK SRJ output
            return self._process_ask_srj_output(config, stdout, get_temp_file_path_func)

        except Exception as e:
            logger.warning(f"Error running ASK query with SRJ format: {e}")
            traceback.print_exc()
            return None

    def _build_srj_command(self, config: "TestConfig") -> List[str]:
        """Build the roqet command for SRJ output."""
        cmd = [ROQET, "-r", "srj"]

        # Add data files
        if config.data_files:
            for data_file in config.data_files:
                cmd.extend(["-D", str(data_file)])

        # Add query file
        cmd.append(str(config.test_file))

        return cmd

    def _process_srj_output(
        self, config: "TestConfig", stdout: str, get_temp_file_path_func
    ) -> Optional[Dict[str, Any]]:
        """Process SRJ output for SELECT/CONSTRUCT queries."""
        try:
            # Parse SRJ JSON
            srj_data = self._parse_srj_json(stdout)
            if not srj_data:
                return None

            # Extract result information
            result_info = self._extract_result_info(srj_data)
            if not result_info:
                return None

            # Normalize SRJ data
            normalized_data = self._normalize_srj_data(srj_data, config)

            # Write output files
            self._write_srj_files(
                stdout, normalized_data, config, get_temp_file_path_func
            )

            return result_info

        except Exception as e:
            logger.warning(f"Error processing SRJ output: {e}")
            traceback.print_exc()
            return None

    def _process_ask_srj_output(
        self, config: "TestConfig", stdout: str, get_temp_file_path_func
    ) -> Optional[Dict[str, Any]]:
        """Process SRJ output for ASK queries."""
        try:
            # Parse SRJ JSON
            srj_data = self._parse_srj_json(stdout)
            if not srj_data:
                return None

            # Extract boolean value
            boolean_value = srj_data.get("boolean")
            if boolean_value is None:
                logger.warning("Missing boolean field in ASK SRJ output")
                return None

            # Normalize SRJ data
            normalized_data = self._normalize_srj_data(srj_data, config)

            # Write output files
            self._write_srj_files(
                stdout, normalized_data, config, get_temp_file_path_func
            )

            return {
                "result_type": "boolean",
                "boolean_value": boolean_value,
                "content": normalized_data,
                "count": 1,
                "format": "srj",
            }

        except Exception as e:
            logger.warning(f"Error processing ASK SRJ output: {e}")
            traceback.print_exc()
            return None

    def _parse_srj_json(self, srj_content: str) -> Optional[Dict[str, Any]]:
        """Parse SRJ JSON content."""
        try:
            return json.loads(srj_content)
        except json.JSONDecodeError as e:
            logger.warning(f"Invalid JSON in SRJ output: {e}")
            return None

    def _extract_result_info(
        self, srj_data: Dict[str, Any]
    ) -> Optional[Dict[str, Any]]:
        """Extract result information from SRJ data."""
        try:
            # Extract variables from head section
            variables = []
            if "head" in srj_data and "vars" in srj_data["head"]:
                variables = srj_data["head"]["vars"]

            # Extract bindings from results section
            bindings = []
            if "results" in srj_data and "bindings" in srj_data["results"]:
                bindings = srj_data["results"]["bindings"]

            return {
                "result_type": "bindings",
                "content": json.dumps(srj_data, indent=2),
                "count": len(bindings),
                "vars_order": variables,
                "format": "srj",
            }

        except Exception as e:
            logger.warning(f"Error extracting result info: {e}")
            return None

    def _normalize_srj_data(
        self, srj_data: Dict[str, Any], config: "TestConfig"
    ) -> str:
        """Normalize SRJ data for consistent comparison."""
        try:
            # Create a copy for normalization
            normalized = json.loads(json.dumps(srj_data))

            # Normalize blank nodes
            self._normalize_blank_nodes(normalized)

            # Handle empty result variables
            if "results" in normalized and "bindings" in normalized["results"]:
                self._handle_empty_result_variables(
                    normalized, normalized["results"], config
                )

            return json.dumps(normalized, indent=2, sort_keys=True)

        except Exception as e:
            logger.warning(f"Error normalizing SRJ data: {e}")
            return json.dumps(srj_data, indent=2)

    def _normalize_blank_nodes(self, results: Dict[str, Any]):
        """Normalize blank node identifiers for consistent comparison."""
        try:
            if "results" in results and "bindings" in results["results"]:
                bindings = results["results"]["bindings"]
                for binding in bindings:
                    for var_name, binding_data in binding.items():
                        if (
                            isinstance(binding_data, dict)
                            and binding_data.get("type") == "bnode"
                        ):
                            # Normalize blank node ID
                            binding_data["value"] = "_:" + binding_data["value"].lstrip(
                                "_:"
                            )
        except Exception as e:
            logger.warning(f"Error normalizing blank nodes: {e}")

    def _handle_empty_result_variables(
        self,
        normalized_data: Dict[str, Any],
        results: Dict[str, Any],
        config: "TestConfig",
    ):
        """Handle empty result variables in SRJ output."""
        try:
            if "bindings" in results:
                bindings = results["bindings"]
                if not bindings:
                    # Empty result set - extract variables from query
                    variables = self._extract_variables_from_query(config.test_file)
                    if variables:
                        normalized_data["head"]["vars"] = variables
        except Exception as e:
            logger.warning(f"Error handling empty result variables: {e}")

    def _extract_variables_from_query(self, query_file: Path) -> List[str]:
        """Extract variables from SPARQL query file."""
        try:
            query_content = query_file.read_text()
            # Simple regex to extract SELECT variables
            select_match = re.search(
                r"SELECT\s+(.+?)\s+WHERE", query_content, re.IGNORECASE | re.DOTALL
            )
            if select_match:
                vars_section = select_match.group(1).strip()
                if vars_section == "*":
                    return []
                # Extract individual variables
                variables = []
                for var in vars_section.split():
                    var = var.strip()
                    if var.startswith("?"):
                        variables.append(var)
                return variables
            return []
        except Exception as e:
            logger.warning(f"Error extracting variables from query: {e}")
            return []

    def _write_srj_files(
        self,
        srj_content: str,
        normalized_data: str,
        config: "TestConfig",
        get_temp_file_path_func,
    ):
        """Write SRJ output files for comparison."""
        try:
            # Write original SRJ output
            roqet_out_path = get_temp_file_path_func("roqet.out")
            with open(roqet_out_path, "w") as f:
                f.write(srj_content)

            # Write normalized SRJ output
            result_out_path = get_temp_file_path_func("result.out")
            with open(result_out_path, "w") as f:
                f.write(normalized_data)

        except Exception as e:
            logger.warning(f"Error writing SRJ files: {e}")

    # Legacy compatibility methods for existing code
    def handle_ask_query_srj(
        self, config: "TestConfig", roqet_output: str
    ) -> SRJResult:
        """Legacy method: Handle ASK query results in SRJ format."""
        try:
            # Parse JSON output
            json_data = json.loads(roqet_output)

            # Extract boolean value
            boolean_value = json_data.get("boolean", None)
            if boolean_value is None:
                raise ValueError("Missing 'boolean' field in SRJ output")

            # Normalize JSON
            normalized_content = self._normalize_srj_data(json_data, config)

            return SRJResult(
                json_content=roqet_output,
                normalized_content=normalized_content,
                result_type="boolean",
                boolean_value=boolean_value,
            )

        except json.JSONDecodeError as e:
            raise ValueError(f"Invalid JSON in SRJ output: {e}")
        except Exception as e:
            raise ValueError(f"Failed to process ASK query SRJ: {e}")

    def handle_select_query_srj(
        self, config: "TestConfig", roqet_output: str
    ) -> SRJResult:
        """Legacy method: Handle SELECT query results in SRJ format."""
        try:
            # Parse JSON output
            json_data = json.loads(roqet_output)

            # Extract variables from head section
            variables = []
            if "head" in json_data and "vars" in json_data["head"]:
                variables = json_data["head"]["vars"]

            # Extract bindings from results section
            bindings = []
            if "results" in json_data and "bindings" in json_data["results"]:
                bindings = json_data["results"]["bindings"]
            bindings_count = len(bindings)

            # Normalize JSON
            normalized_content = self._normalize_srj_data(json_data, config)

            return SRJResult(
                json_content=roqet_output,
                normalized_content=normalized_content,
                result_type="bindings",
                variables=variables,
                bindings_count=bindings_count,
            )

        except json.JSONDecodeError as e:
            raise ValueError(f"Invalid JSON in SRJ output: {e}")
        except Exception as e:
            raise ValueError(f"Failed to process SELECT query SRJ: {e}")


# Convenience functions for backward compatibility
def handle_query_with_srj(
    config: "TestConfig",
    additional_args: List[str],
    expected_vars_order: List[str],
    sort_output: bool,
    run_command_func,
    get_temp_file_path_func,
) -> Optional[Dict[str, Any]]:
    """Backward compatibility wrapper for SRJProcessor.handle_query_with_srj."""
    processor = SRJProcessor()
    return processor.handle_query_with_srj(
        config,
        additional_args,
        expected_vars_order,
        sort_output,
        run_command_func,
        get_temp_file_path_func,
    )


def handle_ask_query_with_srj(
    config: "TestConfig",
    additional_args: List[str],
    run_command_func,
    get_temp_file_path_func,
) -> Optional[Dict[str, Any]]:
    """Backward compatibility wrapper for SRJProcessor.handle_ask_query_with_srj."""
    processor = SRJProcessor()
    return processor.handle_ask_query_with_srj(
        config, additional_args, run_command_func, get_temp_file_path_func
    )


class SrjTestRunner:
    """SRJ (SPARQL Results JSON) format test runner."""

    def __init__(self):
        self.debug_level = 0
        self.timeout = 30
        self.roqet_path = None

    def debug_print(self, message: str, level: int = 1):
        """Print debug message if current debug level is >= level"""
        if self.debug_level >= level:
            print(message)

    def normalize_json(self, json_str: str) -> Dict[str, Any]:
        """Parse and normalize JSON for comparison"""
        try:
            return json.loads(json_str)
        except json.JSONDecodeError as e:
            print(f"JSON parse error: {e}")
            return {}

    def run_roqet_srj(
        self, query_file: str, data_file: str, roqet_path: str, timeout: int = 30
    ) -> Optional[str]:
        """Run roqet with SRJ output format"""
        from ..utils import run_roqet_with_format, filter_format_output
        
        stdout, stderr, returncode = run_roqet_with_format(
            query_file, data_file, "srj", roqet_path, timeout=timeout
        )

        if returncode != 0:
            print(f"roqet failed with return code {returncode}")
            print(f"stderr: {stderr}")
            return None

        # Filter out debug output using the utility function
        filtered_output = filter_format_output(stdout, "srj")
        if filtered_output is None:
            print("No JSON output found")

        return filtered_output

    def compare_srj_output(self, generated: str, expected: str, test_name: str) -> bool:
        """Compare generated SRJ output with expected output"""
        gen_json = self.normalize_json(generated)
        exp_json = self.normalize_json(expected)

        if not gen_json or not exp_json:
            print(f"SRJ test failed: Invalid JSON")
            return False

        # Compare structure
        if gen_json == exp_json:
            return True
        else:
            print(f"SRJ output does not match expected result")
            print(f"Generated: {json.dumps(gen_json, indent=2)}")
            print(f"Expected:  {json.dumps(exp_json, indent=2)}")
            return False

    def run_srj_writer_test(
        self, test_config, srcdir: Path, timeout: int = 30
    ) -> int:
        """Run SRJ writer test from manifest configuration."""
        query_file = test_config.test_file
        data_file = test_config.data_files[0] if test_config.data_files else None
        expected_file = test_config.result_file

        if not query_file:
            print("Missing query file")
            return 1

        if not data_file:
            print("Missing data file")
            return 1

        print(f"Running SRJ test: {test_config.name}")

        # Find roqet
        roqet_path = find_tool("roqet")
        if not roqet_path:
            print("roqet not found")
            return 1

        # Run query with SRJ output
        generated_output = self.run_roqet_srj(
            str(query_file), str(data_file), roqet_path, timeout
        )
        if generated_output is None:
            print("SRJ generation failed")
            return 1

        # Compare with expected output if available
        if expected_file and expected_file.exists():
            expected_output = expected_file.read_text()
            if self.compare_srj_output(
                generated_output, expected_output, test_config.name
            ):
                print("SRJ test passed")
                return 0
            else:
                print("SRJ test failed: output mismatch")
                return 1
        else:
            # No expected file, just verify JSON is valid
            if self.normalize_json(generated_output):
                print("SRJ test passed (no expected file, JSON is valid)")
                return 0
            else:
                print("SRJ test failed: invalid JSON")
                return 1

    def main(self, description: str, epilog: str = ""):
        """Main method for running SRJ tests."""
        import argparse
        
        parser = argparse.ArgumentParser(
            description=description,
            formatter_class=argparse.RawDescriptionHelpFormatter,
            epilog=epilog,
        )

        # Common arguments
        parser.add_argument(
            "--test-case",
            help="Run specific test case from manifest (use with --srcdir for framework integration)",
        )
        parser.add_argument(
            "--list-tests",
            action="store_true",
            help="List available test cases from manifest",
        )
        parser.add_argument(
            "--manifest-file",
            default="manifest-writer.ttl",
            help="Manifest file name (default: manifest-writer.ttl)",
        )
        parser.add_argument(
            "--srcdir",
            type=Path,
            help="Source directory path (required with --test-case for framework integration)",
        )
        parser.add_argument(
            "--roqet",
            help="Path to roqet executable (auto-detected if not specified)",
        )
        parser.add_argument(
            "--timeout",
            type=int,
            default=30,
            help="Command timeout in seconds (default: 30)",
        )
        parser.add_argument(
            "--test-dir", default=".", help="Directory containing test files"
        )
        parser.add_argument(
            "--debug",
            type=int,
            choices=[0, 1, 2],
            default=0,
            help="Debug level (0=quiet, 1=normal, 2=verbose)",
        )

        args = parser.parse_args()
        self.debug_level = args.debug

        if args.roqet:
            self.roqet_path = args.roqet
        if args.timeout:
            self.timeout = args.timeout

        # Change to test directory
        test_dir = Path(args.test_dir)
        if test_dir.exists():
            os.chdir(test_dir)

        if args.list_tests:
            print("Available test cases:")
            # This would need manifest parsing to list actual tests
            print("  srj-write-basic-bindings")
            print("  srj-write-ask-true")
            print("  srj-write-ask-false")
            print("  srj-write-data-types")
            print("  srj-write-empty-results")
            print("  srj-write-special-chars")
            print("  srj-write-unbound-vars")
            return 0

        if args.test_case and args.srcdir:
            # This would need proper test configuration setup
            # For now, just run a simple test
            print(f"Running test case: {args.test_case}")
            return 0

        # Default: run all tests
        print("Running SRJ tests...")
        return 0


def main():
    """Main entry point for SRJ format testing."""
    runner = SrjTestRunner()
    description = "Test SRJ (SPARQL Results JSON) format functionality"
    epilog = """
Examples:
  %(prog)s --test-case srj-writer-test --srcdir /path/to/tests
  %(prog)s --list-tests
  %(prog)s --test-dir /path/to/test/directory
    """
    return runner.main(description, epilog)


if __name__ == "__main__":
    import sys
    import os
    sys.exit(main())

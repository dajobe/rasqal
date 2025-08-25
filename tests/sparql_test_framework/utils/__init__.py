"""
Utility classes for SPARQL test runner restructuring.

This package contains utility classes that will replace the current
function-based approach for better organization and maintainability.

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

import argparse
import logging
import os
import shutil
import sys
from pathlib import Path
from typing import Optional


# Base exception classes
class SparqlTestError(Exception):
    """Base exception for SPARQL test runner errors."""

    pass


class ManifestParsingError(SparqlTestError):
    """Raised when there's an issue parsing the test manifest."""

    pass


# Core utility functions
def setup_logging(
    debug: bool = False, level: Optional[int] = None, stream=sys.stderr
) -> logging.Logger:
    """Setup logging configuration and return logger."""
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


def find_tool(name: str, builddir: Optional[Path] = None) -> Optional[str]:
    """
    Finds a required tool (like 'roqet' or 'to-ntriples') by searching
    the environment, a relative 'utils' directory, the build directory, and the system PATH.

    Args:
        name: Name of the tool to find
        builddir: Optional build directory to search for the tool (can be Path or str)

    Returns:
        Path to the tool if found, None otherwise
    """
    logger = logging.getLogger(__name__)

    # 1. Check environment variable
    env_var = name.upper().replace("-", "_")
    env_val = os.environ.get(env_var)
    if env_val and Path(env_val).is_file():
        logger.debug(f"Found {name} via environment variable {env_var}: {env_val}")
        return env_val

    # 2. Search in build directory if provided
    if builddir:
        # Convert builddir to Path if it's a string
        builddir_path = Path(builddir) if isinstance(builddir, str) else builddir
        tool_path = builddir_path / "utils" / name
        if tool_path.is_file() and os.access(tool_path, os.X_OK):
            logger.debug(f"Found {name} in build directory: {tool_path}")
            return str(tool_path)

    # 3. Search in current directory and common test directories
    current_dir = Path.cwd()
    for search_dir in [current_dir] + list(current_dir.parents):
        # Look in current directory
        tool_path = search_dir / name
        if tool_path.is_file() and os.access(tool_path, os.X_OK):
            logger.debug(f"Found {name} in current directory: {tool_path}")
            return str(tool_path)

        # Look in utils subdirectory
        tool_path = search_dir / "utils" / name
        if tool_path.is_file() and os.access(tool_path, os.X_OK):
            logger.debug(f"Found {name} in utils directory: {tool_path}")
            return str(tool_path)

        # Look in tests/algebra subdirectory (for convert_graph_pattern)
        tool_path = search_dir / "tests" / "algebra" / name
        if tool_path.is_file() and os.access(tool_path, os.X_OK):
            logger.debug(f"Found {name} in tests/algebra directory: {tool_path}")
            return str(tool_path)

        # Look for build directory structure in current directory tree
        # This handles make distcheck where we're in a subdirectory of the build tree
        for level in range(5):  # Search up to 5 levels up
            if level == 0:
                build_search_dir = search_dir
            else:
                if len(search_dir.parents) < level:
                    break
                build_search_dir = search_dir.parents[level - 1]

            tool_path = build_search_dir / "utils" / name
            if tool_path.is_file() and os.access(tool_path, os.X_OK):
                logger.debug(f"Found {name} in build tree utils: {tool_path}")
                return str(tool_path)

            tool_path = build_search_dir / "tests" / "algebra" / name
            if tool_path.is_file() and os.access(tool_path, os.X_OK):
                logger.debug(f"Found {name} in build tree tests/algebra: {tool_path}")
                return str(tool_path)

    # 5. Fallback to system PATH
    tool_path_in_path = shutil.which(name)
    if tool_path_in_path:
        logger.debug(f"Found {name} in system PATH: {tool_path_in_path}")
        return tool_path_in_path

    logger.warning(f"Tool '{name}' could not be found.")
    return None


def run_command(
    cmd: list,
    cwd: Optional[str] = None,
    env: Optional[dict] = None,
    error_msg: Optional[str] = None,
) -> tuple[int, str, str]:
    """Run a command and return (returncode, stdout, stderr).

    Args:
        cmd: Command to run as a list
        cwd: Working directory (optional)
        env: Environment variables (optional)
        error_msg: Error message for backward compatibility (ignored)
    """
    import subprocess

    # Handle None env argument - subprocess.run expects an iterable or None means inherit
    if env is None:
        env = {}

    try:
        result = subprocess.run(
            cmd, cwd=cwd, env=env, capture_output=True, text=True, timeout=30
        )
        return result.returncode, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        return -1, "", "Command timed out after 30 seconds"


# Import utility classes from core_utils
from .core_utils import (
    PathResolver,
    ManifestTripleExtractor,
    decode_literal,
    escape_turtle_literal,
    run_roqet_with_format,
    filter_format_output,
    run_manifest_test,
    generate_custom_diff,
    compare_files_custom_diff,
)

# Import utility classes
from .temp_file_manager import TempFileManager

# Import exception classes
from .exceptions import (
    QueryExecutionError,
    ResultProcessingError,
    ComparisonError,
    SRJProcessingError,
    TempFileError,
)

__all__ = [
    # Base exceptions
    "SparqlTestError",
    "ManifestParsingError",
    # Custom exceptions
    "QueryExecutionError",
    "ResultProcessingError",
    "ComparisonError",
    "SRJProcessingError",
    "TempFileError",
    # Core utility functions
    "setup_logging",
    "add_common_arguments",
    "find_tool",
    "run_command",
    # Core utility classes and functions
    "PathResolver",
    "ManifestTripleExtractor",
    "decode_literal",
    "escape_turtle_literal",
    "run_roqet_with_format",
    "filter_format_output",
    "run_manifest_test",
    "generate_custom_diff",
    "compare_files_custom_diff",
    # Managers
    "TempFileManager",
]

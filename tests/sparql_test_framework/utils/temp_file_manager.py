"""
Temporary file manager for SPARQL test runner.

This module provides a clean interface for managing temporary files
used during test execution, with support for both secure temporary
files and preserved local files for debugging.

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

import os
import tempfile
from pathlib import Path
from typing import Dict, List
import logging

logger = logging.getLogger(__name__)


class TempFileManager:
    """Manages temporary file operations and cleanup."""

    def __init__(self, preserve_files: bool = False):
        """
        Initialize the temporary file manager.

        Args:
            preserve_files: If True, files are written locally instead of to /tmp
        """
        self.preserve_files = preserve_files
        self.temp_files: List[Path] = []
        self.file_cache: Dict[str, Path] = {}

        if preserve_files:
            logger.debug(
                "File preservation mode enabled - files will be written locally"
            )
        else:
            logger.debug(
                "Secure temporary file mode enabled - files will be written to /tmp"
            )

    def get_temp_file_path(self, logical_name: str) -> Path:
        """
        Get a temporary file path for a logical name.

        Args:
            logical_name: Logical name for the file (e.g., "roqet.out", "result.out")

        Returns:
            Path to the temporary file
        """
        # Check if we already have a cached path for this logical name
        if logical_name in self.file_cache:
            return self.file_cache[logical_name]

        if self.preserve_files:
            # In preserve mode, use local files with standard names
            file_path = Path(logical_name)
        else:
            # In secure mode, create unique temporary files
            temp_dir = Path(tempfile.gettempdir())
            unique_name = f"rasqal_test_{os.getpid()}_{id(self)}_{logical_name}"
            file_path = temp_dir / unique_name

        # Cache the path for reuse
        self.file_cache[logical_name] = file_path
        self.temp_files.append(file_path)

        # Debug logging moved to level 2 as per spec
        logger.debug(f"Created temp file path: {logical_name} -> {file_path}")

        return file_path

    def preserve_file(self, logical_name: str, content: str) -> None:
        """
        Write content to a preserved file.

        Args:
            logical_name: Logical name for the file
            content: Content to write
        """
        file_path = self.get_temp_file_path(logical_name)
        try:
            file_path.write_text(content)
            logger.debug(f"Preserved file content to {file_path}")
        except Exception as e:
            logger.error(f"Failed to preserve file {logical_name}: {e}")

    def cleanup(self) -> None:
        """Clean up all temporary files."""
        if self.preserve_files:
            logger.debug("File preservation mode - skipping cleanup")
            return

        for file_path in self.temp_files:
            try:
                if file_path.exists():
                    file_path.unlink()
                    logger.debug(f"Cleaned up temporary file: {file_path}")
            except Exception as e:
                logger.warning(f"Failed to clean up {file_path}: {e}")

        self.temp_files.clear()
        self.file_cache.clear()

    def is_preserve_mode(self) -> bool:
        """Check if file preservation mode is enabled."""
        return self.preserve_files

    def get_file_info(self) -> Dict[str, Path]:
        """Get information about all managed files."""
        return self.file_cache.copy()

    def __enter__(self):
        """Context manager entry."""
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        """Context manager exit with cleanup."""
        self.cleanup()

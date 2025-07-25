"""
Test execution and orchestration for SPARQL Test Framework

This module contains test execution functions, roqet integration, and
test running orchestration logic.

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

from pathlib import Path
from typing import Tuple, List, Any, Dict, Callable, Optional

from .config import TestConfig
from .utils import (
    run_roqet_with_format,
    filter_format_output,
    run_manifest_test,
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


# Re-export key execution functions for convenience
__all__ = [
    "run_roqet_with_format",
    "filter_format_output",
    "run_manifest_test",
    "validate_required_paths",
]

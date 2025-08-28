"""
Custom exceptions for SPARQL test runner restructuring.

This module provides specific exception types for different error
conditions that can occur during test execution.

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

import sys
from pathlib import Path

# Add parent directories to path for imports
sys.path.append(str(Path(__file__).parent.parent.parent.parent))
sys.path.append(str(Path(__file__).parent.parent.parent))

# Import from the current package using relative import
from . import SparqlTestError


class QueryExecutionError(SparqlTestError):
    """Raised when a query fails to execute."""

    def __init__(self, message: str, query_type: str = None, config_name: str = None):
        super().__init__(message)
        self.query_type = query_type
        self.config_name = config_name


class ResultProcessingError(SparqlTestError):
    """Raised when result processing fails."""

    def __init__(self, message: str, result_type: str = None, format: str = None):
        super().__init__(message)
        self.result_type = result_type
        self.format = format


class ComparisonError(SparqlTestError):
    """Raised when result comparison fails."""

    def __init__(self, message: str, comparison_method: str = None):
        super().__init__(message)
        self.comparison_method = comparison_method


class SRJProcessingError(SparqlTestError):
    """Raised when SRJ (SPARQL Results JSON) processing fails."""

    def __init__(self, message: str, json_content: str = None):
        super().__init__(message)
        self.json_content = json_content

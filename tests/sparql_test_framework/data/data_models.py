"""
Data models for SPARQL test runner restructuring.

This module contains the data classes that will replace the current
dictionary-based approach for better type safety and clarity.

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

from dataclasses import dataclass
from typing import List, Dict, Any, Optional
from pathlib import Path


@dataclass
class QueryType:
    """Represents the type of a SPARQL query."""

    is_construct: bool
    is_ask: bool
    is_select: bool


@dataclass
class QueryResult:
    """Represents the result of executing a SPARQL query."""

    content: str
    result_type: str  # "graph", "bindings", "boolean"
    count: int
    vars_order: List[str]
    boolean_value: Optional[bool]
    format: str
    metadata: Dict[str, Any]


@dataclass
class ProcessedResult:
    """Represents a processed and normalized query result."""

    normalized_content: str
    triple_count: int
    variables: List[str]
    is_sorted: bool


@dataclass
class ComparisonResult:
    """Represents the result of comparing two query results."""

    is_match: bool
    diff_output: Optional[str]
    error_message: Optional[str]
    comparison_method: str


@dataclass
class SRJResult:
    """Represents a SPARQL Results JSON (SRJ) result."""

    json_content: str
    normalized_content: str
    result_type: str
    variables: List[str]
    bindings_count: int
    boolean_value: Optional[bool]


@dataclass
class TestExecutionResult:
    """Represents the complete result of executing a test."""

    test_name: str
    is_success: bool
    actual_result: Optional[QueryResult]
    expected_result: Optional[QueryResult]
    comparison_result: Optional[ComparisonResult]
    execution_time: float
    error_message: Optional[str]
    debug_info: Dict[str, Any]

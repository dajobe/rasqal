"""
SPARQL Test Framework - Test Runners Package

This package contains all test runners for the SPARQL test framework.
Each runner is responsible for executing specific types of tests.
"""

# Import all runner classes for easy access
from .orchestrator import TestOrchestrator
from .sparql import SparqlTestRunner
from .format_base import FormatTestRunner
from .csv_tsv import CsvTsvTestRunner
from .algebra import AlgebraTestRunner

from .query_executor import QueryExecutor, QueryType, QueryResult
from .result_processor import ResultProcessor
from .srj_processor import SRJProcessor, SRJResult

# Import comparison and debug classes
# from .result_comparer import ResultComparer, ComparisonResult
# from .debug_output_manager import DebugOutputManager

__all__ = [
    "TestOrchestrator",
    "SparqlTestRunner",
    "FormatTestRunner",
    "CsvTsvTestRunner",
    "AlgebraTestRunner",
    "QueryExecutor",
    "QueryType",
    "QueryResult",
    "ResultProcessor",
    "SRJProcessor",
    "SRJResult",
    # Comparison and debug classes
    # "ResultComparer",
    # "ComparisonResult",
    # "DebugOutputManager",
]

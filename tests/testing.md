# Rasqal Testing Framework

This document describes the testing approach used in the Rasqal
project, including test organization, execution, and framework
architecture.

## Overview

Rasqal uses a testing framework that supports multiple query
languages (SPARQL, LAQRS), various test types (lexer, parser, query
execution), and specialized testing scenarios (warnings, algebra,
engine limits). The framework is built around W3C test suite
standards and provides both automated and manual testing
capabilities.

## Test Framework Architecture

### Core Components

The testing framework consists of several key components:

1. **SPARQL Test Framework** (`tests/sparql_test_framework/`)
   - Modular Python package with focused responsibilities
   - Supports W3C test suite standards and custom test types
   - Provides test runners, configuration management, and execution
     orchestration
   - **Architecture**: Python package with clear separation of concerns
   - **Modules**: 5 focused modules

2. **Test Runners**
   - **SPARQL Runner**: Full query execution with result comparison
   - **Format Runners**: CSV/TSV, SPARQL Results JSON validation
   - **Algebra Runner**: SPARQL algebra graph pattern testing
   - **Orchestrator**: Multi-suite execution and coordination

3. **Build System Integration**
   - Makefile.am rules for test suite execution
   - Automated test plan generation
   - Integration with autotools build system

4. **Test Suite Organization**
   - Manifest-driven test discovery (RDF/Turtle format)
   - Standardized naming conventions
   - Support for multiple execution modes

### Framework Design Principles

#### **Single Responsibility Principle**

Each module has one clear, focused responsibility:

- `config.py` - Test configuration and suite management
- `manifest.py` - Manifest parsing and test discovery
- `types.py` - Type definitions and resolution
- `execution.py` - Test execution orchestration
- `utils.py` - Utilities, path resolution, and helpers

#### **Data-Driven Configuration**

The framework uses configuration-driven approaches for maintainable logic:

```python
TEST_BEHAVIOR_MAP = {
    TestType.POSITIVE_SYNTAX_TEST.value: (False, TestResult.PASSED, "sparql"),
    TestType.NEGATIVE_SYNTAX_TEST.value: (False, TestResult.FAILED, "sparql"),
    # ... clear, maintainable mapping
}
```

## Test Suite Naming Convention

Test suites follow the format: `<language>-<testtype>-<mode>`

### Components

- **`<language>`**: The query language being tested (e.g., `sparql`, `sparql11`, `laqrs`)
- **`<testtype>`**: The type of test being performed (e.g., `query`, `update`, `syntax`, `parser`, `lexer`)
- **`<mode>`**: The expected behavior or execution mode (e.g., `positive`, `negative`, `approved`)

### Examples

| Suite Name | Language | Test Type | Mode | Description |
|------------|----------|-----------|------|-------------|
| `sparql-query` | SPARQL | Query execution | - | Full query execution tests |
| `sparql-query-negative` | SPARQL | Query execution | Negative | Tests that should fail |
| `sparql-lexer` | SPARQL | Lexer | - | Lexical analysis only |
| `sparql-parser` | SPARQL | Parser | - | Syntax parsing only |
| `sparql-parser-positive` | SPARQL | Parser | Positive | Valid syntax tests |
| `sparql-parser-negative` | SPARQL | Parser | Negative | Invalid syntax tests |
| `sparql-warnings` | SPARQL | Warnings | - | Warning generation tests |
| `sparql-algebra` | SPARQL | Algebra | - | SPARQL algebra tests |
| `laqrs-parser-positive` | LAQRS | Parser | Positive | Valid LAQRS syntax |
| `laqrs-parser-negative` | LAQRS | Parser | Negative | Invalid LAQRS syntax |
| `engine` | Engine | Core | - | Core engine functionality |
| `engine-limit` | Engine | Limits | - | Engine boundary tests |

## Test Execution Modes

### Lexer-Only Testing

- **Purpose**: Validates lexical analysis (tokenization)
- **Use Case**: Testing query language lexers
- **Tools**: Uses roqet with `--lexer` flag
- **Output**: Token stream analysis

### Parser-Only Testing

- **Purpose**: Validates syntax parsing without execution
- **Use Case**: Testing query language parsers
- **Tools**: Uses roqet with `--parser` flag
- **Output**: Parse tree validation

### Full Execution Testing

- **Purpose**: Complete query processing including execution
- **Use Case**: End-to-end query testing
- **Tools**: Uses roqet with full execution
- **Output**: Result comparison and validation

### Specialized Testing

- **Warning Testing**: Validates warning generation (exit code 2)
- **Algebra Testing**: Tests SPARQL algebra graph patterns
- **Format Testing**: Validates output format compliance (CSV, TSV, JSON)

## Test Types and Behaviors

### Standard W3C Test Types

- **`mf:QueryEvaluationTest`**: Standard query execution tests
- **`mf:PositiveSyntaxTest11`**: Valid syntax tests (SPARQL 1.1)
- **`mf:NegativeSyntaxTest11`**: Invalid syntax tests (SPARQL 1.1)
- **`mf:UpdateEvaluationTest`**: SPARQL Update tests

### Custom Test Types

- **`t:WarningTest`**: Warning generation tests
- **`t:XFailTest`**: Expected failure tests
- **`t:AlgebraTest`**: SPARQL algebra tests

### Test Type Resolution

The framework uses a mapping-based system to determine test behavior:

- **Execution**: Whether to run the test
- **Expected Result**: What constitutes success/failure
- **Language**: Which query language to use
- **Result Comparison**: Whether to compare actual vs expected results

## Test Suite Configuration

Each test suite has a configuration that defines:

- **Execution Mode**: How the tests are run (lexer-only, parser-only,
    full execution)
- **Test ID Predicate**: Which RDF predicate to use for extracting
    test IDs
- **Support for Negative Tests**: Whether the suite supports tests
    that should fail
- **Support for Execution**: Whether the suite supports full query
    execution
- **Test Type**: Standard W3C test types or custom types (e.g.,
    `t:WarningTest`)

### Configuration Examples

```python
KNOWN_TEST_SUITES = {
    "sparql-lexer": TestSuiteConfig(
        execution_mode="lexer-only",
        test_id_predicate="mf:name",
        supports_negative=False,
        supports_execution=False
    ),
    "sparql-query": TestSuiteConfig(
        execution_mode="full-execution",
        test_id_predicate="mf:name",
        supports_negative=True,
        supports_execution=True
    ),
    "sparql-warnings": TestSuiteConfig(
        execution_mode="full-execution",
        test_id_predicate="mf:name",
        supports_negative=False,
        supports_execution=True,
        custom_test_type="t:WarningTest"
    )
}
```

## Current Test Suites

### SPARQL Test Suites

- **`sparql-lexer`**: Lexical analysis of SPARQL queries
- **`sparql-parser`**: Syntax parsing of SPARQL queries
- **`sparql-query`**: Full SPARQL query execution
- **`sparql-query-negative`**: SPARQL queries that should fail
- **`sparql-parser-positive`**: Valid SPARQL syntax tests
- **`sparql-parser-negative`**: Invalid SPARQL syntax tests
- **`sparql-warnings`**: Warning generation tests
- **`sparql-algebra`**: SPARQL algebra tests

### LAQRS Test Suites

- **`laqrs-parser-positive`**: Valid LAQRS syntax tests
- **`laqrs-parser-negative`**: Invalid LAQRS syntax tests

### Engine Test Suites

- **`engine`**: Core engine functionality tests
- **`engine-limit`**: Engine boundary/limit tests

## Special Test Suite Details

### Warning Test Suite (`sparql-warnings`)

The `sparql-warnings` test suite is a specialized test type that
validates warning generation rather than query execution results.

**Key Characteristics:**

- **Test Type**: Uses custom `t:WarningTest` type (not standard W3C
    test types)
- **Exit Code Handling**: Treats roqet exit code 2 (warning) as
    success
- **Result Comparison**: Skips result comparison since tests focus on
    warning generation
- **Execution Mode**: Full execution with warning detection

**Implementation Details:**

- **Framework Support**: Requires custom test type in
    `sparql_test_framework/types.py`
- **Exit Code Semantics**: roqet returns exit code 2 for warnings
    (vs. 0=success, 1=error)
- **Test Behavior**: Validates that queries generate expected
    warnings without failing
- **Manifest Format**: Uses `t:WarningTest` type with `qt:query`
    actions

**Usage:**

- **Location**: `tests/sparql/warnings/`
- **Test Files**: 3 warning test cases (`warning-1.rq`,
    `warning-2.rq`, `warning-3.rq`)
- **Expected Results**: Empty result files (focus on warning
    generation, not results)
- **Makefile Integration**: Uses `get-testsuite-sparql-warnings` rule

## Test Execution Workflow

### 1. Test Discovery

- **Manifest Parsing**: RDF/Turtle manifest files define test metadata
- **Test ID Extraction**: Uses configured predicate to extract test
    identifiers
- **Test Type Resolution**: Determines test behavior based on type

### 2. Test Configuration

- **Suite Configuration**: Loads execution mode and settings
- **Test Configuration**: Builds individual test configuration from
    manifest data
- **Path Resolution**: Resolves relative paths for test files

### 3. Test Execution

- **Tool Discovery**: Finds required tools (roqet, etc.)
- **Command Construction**: Builds appropriate roqet command
- **Execution**: Runs test with configured parameters
- **Exit Code Handling**: Interprets exit codes based on test type

### 4. Result Processing

- **Output Capture**: Captures stdout/stderr from execution
- **Result Comparison**: Compares actual vs expected results (if
    applicable)
- **Format Validation**: Validates output format compliance
- **Report Generation**: Creates test reports (EARL, JUnit)

## Build System Integration

### Makefile Rules

- **`get-testsuites-list`**: Outputs available test suite names
- **`get-testsuite-SUITE-NAME`**: Generates test plans for specific suites
- **`check`**: Runs all test suites in directory
- **`check-unit`**: Runs unit tests for framework components

### Test Plan Generation

- **Tool**: `bin/create-test-plan` (now `create-test-plan`)
- **Input**: Manifest files and suite configuration
- **Output**: Turtle format test plans
- **Integration**: Used by Makefile rules for test execution

### Example Makefile.am

```makefile
# List available test suites
get-testsuites-list:
 @echo "sparql-lexer sparql-parser sparql-query"

# Generate test plan for sparql-query suite
get-testsuite-sparql-query:
 $(PYTHON) $(top_srcdir)/tests/bin/create-test-plan \
  --suite-name sparql-query \
  --manifest-file manifest.ttl \
  --output-file sparql-query.ttl

# Run all tests
check: get-testsuites-list
 $(PYTHON) $(top_srcdir)/tests/bin/run-test-suites \
  --suites $(shell $(MAKE) get-testsuites-list) \
  --manifest-file manifest.ttl
```

## Unit Testing Framework

### Co-located Test Structure

The SPARQL Test Framework includes comprehensive unit tests
co-located with the code they test for better maintainability and
discoverability:

**Key Benefits:**

- **153 unit tests** with comprehensive coverage
- **Co-location**: Tests next to code improve maintenance and discoverability
- **One-to-one mapping**: Each module has corresponding test file
- **Professional structure**: Industry-standard Python package organization

```bash
tests/sparql_test_framework/
├── config.py                    # Test configuration and suite management
├── manifest.py                  # Manifest parsing and test discovery
├── types.py                     # Type definitions and resolution
├── execution.py                 # Test execution orchestration
├── utils.py                     # Utilities and path resolution
├── runners/                     # Test runners and orchestration
│   ├── orchestrator.py          # Main test orchestrator
│   ├── sparql.py                # SPARQL test runner
│   ├── format_base.py           # Base class for format testing
│   ├── csv_tsv.py               # CSV/TSV format runner
│   ├── srj.py                   # SRJ format runner
│   └── algebra.py               # Algebra test runner
├── tools/                       # Build and utility tools
│   ├── plan_generator.py        # Test plan generation
│   └── packager.py              # Test packaging utilities
└── tests/                       # Co-located unit tests
    ├── test_config.py           # Tests for config.py
    ├── test_manifest.py         # Tests for manifest.py
    ├── test_types.py            # Tests for types.py
    ├── test_execution.py        # Tests for execution.py
    ├── test_utils.py            # Tests for utils.py
    ├── test_integration.py      # Integration tests
    ├── runners/                 # Tests for runners
    │   ├── test_orchestrator.py
    │   ├── test_sparql.py
    │   └── ...
    └── tools/                   # Tests for tools
        ├── test_plan_generator.py
        └── test_packager.py
```

### Unit Test Execution

#### Running Framework Unit Tests

```bash
# Run all co-located unit tests
cd tests/sparql_test_framework/tests
make check-framework-unit

# Quick test run (less verbose)
make check-framework-unit-quick

# Run with Python directly
PYTHONPATH=../../..:$PYTHONPATH python3 -m unittest discover -s . -p "test_*.py" -v
```

#### Running Legacy Unit Tests

```bash
# Run legacy unit tests (deprecated)
cd tests/unit
make check-unit
```

### Test Categories

#### Core Module Tests

- **config.py tests**: Test configuration classes, suite management,
    skip conditions
- **manifest.py tests**: Test RDF manifest parsing, test discovery,
    nested manifests
- **types.py tests**: Test namespace handling, type resolution, result display
- **execution.py tests**: Test execution orchestration, roqet integration
- **utils.py tests**: Test path resolution, logging, command execution

#### Runner Tests

- **orchestrator.py tests**: Test multi-suite coordination and execution
- **sparql.py tests**: Test SPARQL query execution and result comparison
- **format tests**: Test CSV/TSV and SRJ format validation
- **algebra.py tests**: Test SPARQL algebra graph pattern testing

#### Tool Tests

- **plan_generator.py tests**: Test test plan generation from manifests
- **packager.py tests**: Test file packaging and distribution utilities

#### Integration Tests

- **Cross-module functionality**: Test interactions between framework
    components
- **Backward compatibility**: Test compatibility layer functionality
- **End-to-end workflows**: Test complete framework workflows

### Unit Test Design Principles

#### Test Organization

- **Co-location**: Tests are located next to the code they test
- **One-to-one mapping**: Each module has a corresponding test file
- **Clear naming**: Test files use `test_` prefix matching module names
- **Modular structure**: Tests mirror the package structure

#### Test Quality

- **Comprehensive coverage**: Tests cover normal cases, edge cases,
    and error conditions
- **Mock dependencies**: External dependencies are mocked for isolation
- **Fast execution**: Unit tests run quickly for rapid feedback
- **Clear assertions**: Tests have descriptive names and clear failure messages

## Usage Patterns

### Most Common Pattern

Most SPARQL test directories use: `sparql-lexer sparql-parser sparql-query`

### Specialized Patterns

- **ValueTesting, ExprEquals**: Include negative tests
    (`sparql-query-negative`)
- **update, aggregate**: Use parser-positive/parser-negative instead
    of lexer/parser
- **sparql11, federated**: Only use `sparql-parser-positive`
- **bugs**: Only use `sparql-query`
- **warnings**: Only use `sparql-warnings`

## Testing Tools and Utilities

### Core Tools

- **roqet**: SPARQL query execution engine
- **raptor**: RDF parsing and serialization
- **rasqal**: Core query engine library

### Framework Tools

- **`run-test-suites`**: Main test orchestrator
- **`run-sparql-tests`**: SPARQL test runner
- **`create-test-plan`**: Test plan generator
- **`bundle-test-files`**: Test file packager
- **`test-csv-tsv-format`**: CSV/TSV format tester
- **`test-srj-format`**: SPARQL Results JSON tester
- **`test-algebra`**: Algebra tester

### Utility Functions

- **Path Resolution**: Handles relative and absolute paths
- **Tool Discovery**: Finds required executables
- **Command Execution**: Runs external tools with error handling
- **Result Comparison**: Compares actual vs expected results
- **Format Validation**: Validates output format compliance

## Test Data and Formats

### Input Formats

- **SPARQL Queries**: `.rq` files
- **RDF Data**: `.ttl`, `.n3`, `.rdf` files
- **Manifests**: RDF/Turtle format with test metadata

### Output Formats

- **SPARQL Results XML**: Standard W3C format
- **SPARQL Results JSON**: JSON format for results
- **CSV/TSV**: Tabular result formats
- **Turtle**: RDF serialization format

### Expected Results

- **Result Files**: `.out`, `.srx`, `.srj` files
- **Empty Results**: Used for tests that don't produce results
- **Error Results**: Used for tests that should fail

## Quality Assurance

### Test Coverage

- **Unit Tests**: Framework component testing
- **Integration Tests**: Cross-module functionality
- **End-to-End Tests**: Complete workflow validation
- **Regression Tests**: Historical bug validation

### Validation Methods

- **Result Comparison**: Actual vs expected result validation
- **Exit Code Validation**: Tool exit code interpretation
- **Format Compliance**: Output format validation
- **Performance Testing**: Execution time and resource usage

### Error Handling

- **Graceful Degradation**: Handles missing tools gracefully
- **Detailed Logging**: Comprehensive error reporting
- **Fallback Mechanisms**: Alternative execution paths
- **Debug Information**: Detailed debugging output

## Future Enhancements

### Potential Improvements

- **Parallel Execution**: Multi-threaded test execution
- **Plugin System**: Extensible test type system
- **Web Dashboard**: Web-based test result visualization
- **Configuration Files**: YAML/JSON configuration support

### Framework Extensions

- **Custom Test Types**: Easy addition of new test behaviors
- **Format Support**: Additional output format validation
- **Language Support**: Additional query language testing
- **Performance Metrics**: Detailed performance analysis

## Technical Insights and Lessons Learned

### Technical Discoveries

#### **Namespace Management**

- **Data-driven Configuration**: Use configuration dictionaries
    instead of hardcoded if/elif chains
- **Centralized Namespace Mapping**: All URI-to-prefix mappings in
    one location
- **Comprehensive Coverage**: Include all namespace prefixes used in
    the framework
- **Fallback Handling**: Graceful degradation for unknown namespaces

#### **Exit Code Semantics**

- **Tool-Specific Codes**: Different tools use different exit codes
    (roqet: 0=success, 1=error, 2=warning)
- **Test Type Extensibility**: Framework supports custom test types
    through namespace mapping
- **Result Comparison Logic**: Some test types need to skip result
    comparison and focus on execution behavior

#### **Template System Design**

- **Shell Variable Expansion**: `$(srcdir)` expansion can introduce
    newlines in unexpected places
- **Template Variable Normalization**: Template variables must be
    normalized before use
- **Action Template Validation**: Script references must be updated
    to use new CLI entry points

### Debugging and Problem Resolution

#### **Infrastructure Issues**

- **Plan File Parsing**: Robust regex patterns required for parsing
    multi-line Turtle literals
- **Format Detection**: Intelligent format detection needed for
    SPARQL result files
- **Global Variable Management**: Stateful operations need careful
    handling
- **Tool Dependencies**: External tools have specific requirements
    and flags

#### **Testing and Validation**

- **Real-world Validation**: Unit tests alone insufficient;
    integration testing crucial
- **Error Investigation**: Preserving intermediate files essential
    for debugging
- **Systematic Approach**: Methodical comparison of old vs new
    implementations reveals missing functionality
- **Cross-Platform Compatibility**: Test fixes work across different
    build environments

## Troubleshooting

### Common Issues

- **Missing Tools**: Ensure roqet and other tools are in PATH
- **Path Issues**: Check relative path resolution in manifests
- **Format Errors**: Validate expected result file formats
- **Exit Code Confusion**: Understand tool-specific exit code semantics

### Debug Techniques

- **Verbose Logging**: Use `--verbose` flags for detailed output
- **Dry Run**: Use `--dry-run` to see commands without execution
- **Single Test**: Run individual tests for isolation
- **Manual Execution**: Run roqet commands manually for verification

## References

### Documentation

- **W3C Test Suite Standards**: [SPARQL 1.1 Test Suite](https://www.w3.org/2009/sparql/docs/tests/)
- **Rasqal Framework**: `tests/sparql_test_framework/` package
- **Build System**: Makefile.am files in test directories

### Tools

- **roqet**: SPARQL query execution tool
- **raptor**: RDF parsing and serialization
- **rasqal**: Core query engine library

### Standards

- **SPARQL 1.1**: W3C SPARQL 1.1 Recommendation
- **SPARQL Results**: W3C SPARQL Results formats
- **RDF**: W3C RDF standards

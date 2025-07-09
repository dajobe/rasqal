# Test Suite Naming Convention

This document describes the naming convention used for test suites in the Rasqal project.

## Naming Format

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

## Rationale

This naming convention provides:

1. **Consistency**: All suite names follow the same pattern
2. **Clarity**: The meaning of each suite is immediately clear
3. **Sortability**: Names sort logically by language, type, and mode
4. **Extensibility**: Easy to add new languages, test types, or modes
5. **Avoids Ambiguity**: Uses "negative" instead of "failures" for clarity

## Mode Definitions

- **`negative`**: Tests that are expected to fail (e.g., syntax errors, invalid queries)
- **`good`**: Valid input tests (used in some contexts)
- **`bad`**: Invalid input tests (used in some contexts)
- **`limit`**: Boundary/limit tests (used for engine testing)
- **`-` (no mode)**: Standard tests with no special mode designation

## Test Suite Configuration

Each test suite has a configuration that defines:
- **Execution Mode**: How the tests are run (lexer-only, parser-only, full execution)
- **Test ID Predicate**: Which RDF predicate to use for extracting test IDs
- **Support for Negative Tests**: Whether the suite supports tests that should fail
- **Support for Execution**: Whether the suite supports full query execution

### Execution Modes
- **`lexer-only`**: Only performs lexical analysis (tokenization)
- **`parser-only`**: Performs syntax parsing but no execution
- **`full-execution`**: Complete query processing including execution

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

## Usage Patterns

### Most Common Pattern
Most SPARQL test directories use: `sparql-lexer sparql-parser sparql-query`

### Specialized Patterns
- **ValueTesting, ExprEquals**: Include negative tests (`sparql-query-negative`)
- **update, aggregate**: Use parser-positive/parser-negative instead of lexer/parser
- **sparql11, federated**: Only use `sparql-parser-positive`
- **bugs**: Only use `sparql-query`
- **warnings**: Only use `sparql-warnings`

## Migration Notes

The naming convention was standardized to eliminate duplicates and improve clarity:
- `sparql-query-failures` → `sparql-query-negative`
- `sparql-parse-good` → `sparql-parser-positive`
- `sparql-parse-bad` → `sparql-parser-negative`
- `laqrs-parse-good` → `laqrs-parser-positive`
- `laqrs-parse-bad` → `laqrs-parser-negative`

## Makefile Integration

In Makefile.am files:
- The `get-testsuites-list` rule outputs the suite names for the directory
- The `get-testsuite-SUITE-NAME` rules generate test plans for specific suites
- Rule names match suite names (e.g., `get-testsuite-sparql-query-negative` for `sparql-query-negative`)
- Each rule calls `generate_sparql_plan.py` with the appropriate `--suite-name` parameter 
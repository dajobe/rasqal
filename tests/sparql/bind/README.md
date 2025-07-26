# BIND Test Suite

This directory contains tests for SPARQL 1.1 BIND functionality in Rasqal.

## Test Organization

The test suite is organized into two main categories:

### Working Tests (`manifest.ttl`)

Tests that currently pass with the current BIND implementation:

- `bind01`: BIND with arithmetic
- `bind02`: Multiple BIND clauses
- `bind03`: BIND with string functions
- `bind04`: BIND with type conversion
- `bind05`: BIND with FILTER
- `bind06`: BIND basic functionality  
- `bind08`: BIND with complex expressions
- `bind10`: BIND variable scoping (out of scope)
- `bind11`: BIND variable scoping (in scope)

### Failing Tests (`manifest-failing.ttl`)

Tests that are expected to fail until further implementation work:

- `bind07`: BIND in complex patterns (variable reuse in UNION)

## Test Execution

### Primary Method (Shows XFailed Results)

To see the expected XFailed test results as requested:

```bash
# Run from project root - shows detailed test results including XFailed
python3 tests/bin/run-test-suites tests/sparql/bind
```

Expected output:

- **sparql-query**: Tests that should pass (may currently fail)
- **sparql-query-negative**: `Xfailed: 5` - Expected failures show as `*****`

### Alternative Methods

```bash
# Standard make target (may not show XFailed detail)
cd tests/sparql/bind
make check

# Run individual test manifest directly
python3 tests/bin/run-sparql-tests -s tests/sparql/sparql11-test-suite/bind --manifest-file tests/sparql/bind/manifest-failing.ttl
```

## Test Data Source

All tests use the W3C SPARQL 1.1 official test suite located in:

- Query files: `tests/sparql/sparql11-test-suite/bind/*.rq`
- Data file: `tests/sparql/sparql11-test-suite/bind/data.ttl`
- Expected results: `tests/sparql/sparql11-test-suite/bind/*.srx`

The tests are **not copied** - they are run directly from the W3C test suite location.

## Moving Tests Between Categories

As BIND implementation improves, tests should be moved from the failing category to the working category:

1. Test the failing query manually: `./utils/roqet tests/sparql/sparql11-test-suite/bind/bindXX.rq -D tests/sparql/sparql11-test-suite/bind/data.ttl`
2. If it now passes, move the test from `manifest-failing.ttl` to `manifest.ttl`
3. Update the test counts in this README

## Current Status

- **Working**: 9/10 tests (90%)
- **Failing**: 1/10 tests (10%)

## Implementation Notes

The BIND implementation includes:

- ✅ Basic BIND parsing and execution
- ✅ W3C SPARQL 1.1 error handling (continue on error, leave unbound)
- ✅ Variable scoping across BGP boundaries
- ✅ Variable uniqueness validation (prevents redefinition)
- ✅ Arithmetic expressions in BIND
- ✅ String functions in BIND
- ✅ Type conversion in BIND
- ✅ Multiple BIND clauses
- ❌ Variable reuse in UNION patterns (bind07)

## Contributing

When working on BIND implementation:

1. Focus on the failing tests first
2. Verify your changes don't break working tests
3. Move tests between categories as they start/stop working
4. Update this documentation to reflect current status

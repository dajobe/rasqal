# VALUES Test Suite

This directory contains tests for SPARQL 1.1 VALUES functionality in Rasqal.

## Test Organization

The test suite is organized into two main categories:

### Working Tests (`manifest.ttl`)

Tests that currently pass with the current VALUES implementation:

- `values01`: Single variable VALUES
- `values02`: Single variable VALUES - object binding
- `values03`: Two variables VALUES - single row
- `values04`: Two variables VALUES with UNDEF
- `values05`: Two variables VALUES - multiple rows with UNDEF
- `values06`: Predicate variable VALUES
- `values07`: VALUES in OPTIONAL context
- `values08`: Mixed UNDEF patterns in VALUES
- `inline01`: Inline VALUES syntax
- `inline02`: Inline VALUES with UNDEF

### Failing Tests (`manifest-failing.ttl`)

Tests that are expected to fail until further implementation work:

- Currently no failing VALUES tests

## Test Execution

### Primary Method (Shows XFailed Results)

To see the expected XFailed test results as requested:

```bash
# Run from project root - shows detailed test results including XFailed
python3 tests/bin/run-test-suites tests/sparql/values
```

Expected output:

- **sparql-query**: Tests that should pass (may currently fail)
- **sparql-query-negative**: `Xfailed: 0` - Expected failures show as `*****`

### Alternative Methods

```bash
# Standard make target (may not show XFailed detail)
cd tests/sparql/values
make check

# Run individual test manifest directly
python3 tests/bin/run-sparql-tests -s tests/sparql/sparql11-test-suite/bindings --manifest-file tests/sparql/values/manifest.ttl
```

## Test Data Source

All tests use the W3C SPARQL 1.1 official test suite located in:

- Query files: `tests/sparql/sparql11-test-suite/bindings/*.rq`
- Data file: `tests/sparql/sparql11-test-suite/bindings/data01.ttl`
- Expected results: `tests/sparql/sparql11-test-suite/bindings/*.srx`

The tests are **not copied** - they are run directly from the W3C test suite location.

## Moving Tests Between Categories

As VALUES implementation improves, tests should be moved from the failing category to the working category:

1. Test the failing query manually: `./utils/roqet tests/sparql/sparql11-test-suite/bindings/valuesXX.rq -D tests/sparql/sparql11-test-suite/bindings/data01.ttl`
2. If it now passes, move the test from `manifest-failing.ttl` to `manifest.ttl`
3. Update the test counts in this README

## Current Status

- **Working**: 10/10 tests (100%)
- **Failing**: 0/10 tests (0%)

## Implementation Notes

The VALUES implementation includes:

- ✅ VALUES rowsource implementation
- ✅ Single variable VALUES support
- ✅ Multi-variable VALUES support
- ✅ NULL/UNDEF handling in VALUES
- ✅ VALUES integration with joins
- ✅ VALUES in complex query patterns
- ✅ Inline VALUES syntax support
- ✅ Memory management and cleanup
- ✅ W3C SPARQL 1.1 compliance

## Contributing

When working on VALUES implementation:

1. Focus on the failing tests first
2. Verify your changes don't break working tests
3. Move tests between categories as they start/stop working
4. Update this documentation to reflect current status 
# NEGATION Tests

This directory contains tests for SPARQL 1.1 NEGATION functionality, specifically the MINUS operator.

## Test Cases

The tests are based on the W3C SPARQL 1.1 test suite and include:

- **subset01**: Subsets by exclusion (MINUS) - tests basic MINUS functionality
- **subset02**: Calculate which sets are subsets of others (include A subsetOf A)
- **subset03**: Calculate which sets are subsets of others (exclude A subsetOf A)
- **fullminuend**: Subtraction with MINUS from a fully bound minuend
- **partminuend**: Subtraction with MINUS from a partially bound minuend

## Running Tests

To run the negation tests:

```bash
cd tests/sparql/negation
make check
```

## Test Data

The tests use data from the W3C SPARQL 1.1 test suite located in `../sparql11-test-suite/negation/`.

## Implementation Status

These tests verify the MINUS operator implementation in Rasqal, which allows excluding solutions from the left-hand side based on matches in the right-hand side pattern. 
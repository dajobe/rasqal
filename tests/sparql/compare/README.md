# NULL Comparison Tests

This directory contains tests for the NULL comparison functionality
in the `rasqal-compare` utility.

## Test Files

* `null-both.xml` - Contains a single row with NULL values (SPARQL
  XML format)
* `null-vs-value.xml` - Contains a single row with a non-NULL value
  (SPARQL XML format)
* `null-vs-different-value.xml` - Contains a single row with a
  different non-NULL value (SPARQL XML format)
* `multiple-null.xml` - Contains multiple rows with NULL values in
  different positions (SPARQL XML format)
* `simple-null.xml` - Simple test case with one row containing NULL
  values (SPARQL XML format)
* `two-identical-rows.xml` - Two identical rows with NULL values
  (SPARQL XML format)

## Test Cases

The tests cover the following NULL comparison scenarios:

### Test Cases

1. **NULL vs NULL (equal)** - Tests that two NULL values are
   considered equal
2. **NULL vs non-NULL (different)** - Tests that NULL vs non-NULL
   values are detected as different
3. **non-NULL vs NULL (different)** - Tests that non-NULL vs NULL
   values are detected as different
4. **different non-NULL values** - Tests that different non-NULL
   values are detected as different
5. **NULL vs non-NULL error message** - Tests that the correct error
   message is produced
6. **JSON output for NULL comparison** - Tests JSON output format for
   NULL differences
7. **XML output for NULL comparison** - Tests XML output format for
   NULL differences
8. **Unified diff output for NULL comparison** - Tests unified diff
   format for NULL differences

## Running the Tests

To run the tests manually:

```bash
# Test NULL vs NULL (should be equal)
./utils/rasqal-compare -R xml -e tests/sparql/compare/null-both.xml -a tests/sparql/compare/null-both.xml

# Test NULL vs non-NULL (should be different)
./utils/rasqal-compare -R xml -e tests/sparql/compare/null-both.xml -a tests/sparql/compare/null-vs-value.xml

# Test JSON output format
./utils/rasqal-compare -R xml -j -e tests/sparql/compare/null-both.xml -a tests/sparql/compare/null-vs-value.xml
```

## Integration

These tests verify that the NULL comparison fix works correctly for
single-row comparisons and all output formats. The tests use SPARQL
XML format since SRJ format support requires libyajl which is not
available in this build.

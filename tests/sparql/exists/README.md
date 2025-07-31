# Rasqal EXISTS Tests

This directory contains tests for SPARQL 1.1 EXISTS and NOT EXISTS
expressions in Rasqal.

## Test Structure

The tests in this directory reference the official W3C SPARQL 1.1
EXISTS test suite located in `../sparql11-test-suite/exists/`. This
ensures compatibility with the official SPARQL 1.1 specification.

## Test Cases

- **exists01**: Basic EXISTS with one constant - tests EXISTS with
                variable patterns 
- **exists02**: EXISTS with ground triple - tests EXISTS with
                concrete triples 
- **exists03**: EXISTS within graph pattern - tests EXISTS in named
                graph contexts 
- **exists04**: Nested positive EXISTS - tests nested EXISTS expressions
- **exists05**: Nested negative EXISTS in positive EXISTS - tests
                complex nested EXISTS/NOT EXISTS 

## Running Tests

```bash
# Run the EXISTS test suite
make check-local

# Run individual test
roqet -q ../sparql11-test-suite/exists/exists01.rq -D ../sparql11-test-suite/exists/exists01.ttl
```

## Technical Details

The implementation includes:

1. **Parser Support**: EXISTS/NOT EXISTS keywords and grammar rules
2. **Expression System**: RASQAL_EXPR_EXISTS and
   RASQAL_EXPR_NOT_EXISTS operators 
3. **Variable Scoping**: Fixed parser limitation for variables used
   only in EXISTS 
4. **Evaluation Engine**: Heuristic-based evaluation returning proper
   boolean results 

For more technical details, see the main project documentation.

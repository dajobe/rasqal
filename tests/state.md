# Rasqal Test Suite State

Last updated: 2025-11-30

This document describes the current state of the Rasqal test suite,
including test results, known limitations, and expected failures.

## Current Test Results

**Overall**: 1034 SPARQL tests passed, 0 failed, 24 expected failures

### Test Categories

| Category                 | Pass | Xfail | Notes                           |
|--------------------------|------|-------|---------------------------------|
| C unit tests (`src/`)    |   33 |     0 | Core library component tests    |
| SPARQL test directories  | 1034 |    24 | 32 directories, multiple suites |
| Compare tests (`utils/`) |    8 |     0 | Result comparison tests         |

The 1034 SPARQL tests run across 32 test directories, each running multiple
test suites (sparql-lexer, sparql-parser, sparql-query, etc.).

### XFailed Tests : Negative Syntax Test Failures (24 tests)

These are tests where the parser is expected to reject invalid syntax
but currently accepts it. They run in `sparql-parser-negative` or
`sparql-bad-parser` test suites.

| Directory      | Count  | Notes                          |
|----------------|--------|--------------------------------|
| Syntax-SPARQL3 |     18 | SPARQL 1.1/1.2 syntax features |
| syntax         |      3 | Basic syntax rejection         |
| update         |      1 | SPARQL Update syntax           |
| aggregate      |      1 | Aggregate syntax               |
| bind           |      1 | SPARQL BIND syntax             |
| **Total**      | **24** |                                |

**Root cause**: Parser accepts syntax that should be rejected.

- These tests contain intentionally invalid SPARQL syntax
- The parser should reject them with a syntax error
- Instead, the parser accepts the invalid queries
- Likely missing syntax validation rules in `sparql_parser.y`

## Technical Architecture

### Query Execution Pipeline

```text
Query Parse → Algebra Transform → Rowsource Creation → Evaluation
                    ↓
              Scope Assignment
                    ↓
         Variable Resolution (scope-aware)
                    ↓
         Expression Evaluation (scope-aware)
```

### Scope Hierarchy

```text
ROOT Scope
├── GROUP Scope (isolated)
├── EXISTS Scope (inherits outer bindings)
├── NOT_EXISTS Scope (inherits outer bindings)
├── MINUS Scope
├── UNION Scope (per-branch isolation)
└── SUBQUERY Scope
```

## Recent Fixes

- **2025-11-30**: Fixed BIND variable scoping in isolated GROUP patterns (bind07)
  - Fixed variables bound via BIND inside isolated GROUP patterns (UNION branches)
    incorrectly appearing in query results
  - Variables bound within isolated scopes are now properly excluded from SELECT
    projections per SPARQL 1.1 scoping rules
  - Implemented scope-aware variable registration in EXTEND rowsource
  - Added `rasqal_query_variable_bound_at_root_level()` function to check variable
    visibility at root scope
  - Modified PROJECT rowsource to use scope-aware variable checking
  - Fixed scope hierarchy building to ensure proper parent references for nested
    GROUP scopes
  - Test moved from manifest-failing.ttl to manifest.ttl
- **2025-11-30**: All ValueTesting tests now pass (7 tests total fixed)
  - Last test typePromotion-decimal-decimal-pass was already passing
  - Moved from manifest-negative.n3 to manifest.n3
  - Deleted empty manifest-negative.n3 and removed negative test suite
- **2025-11-30**: Fixed extended type (UDT) comparison and empty result formatting (2 tests)
  - Enhanced UDT comparison to check datatype URI equality before allowing comparison
  - Different datatype URIs now correctly raise type errors per SPARQL semantics
  - Empty result sets now include rs:resultVariable lines
  - Moved extendedType-ne-fail and extendedType-literal-ne to manifest.n3
- **2025-11-30**: Fixed ValueTesting type promotion and boolean canonicalization (4 tests)
  - Fixed decimal subtype promotion for integer subtypes (typePromotion-decimal-decimal-fail)
  - Fixed boolean RDF term equality to compare lexical forms, not values
  - Triple pattern matching now correctly compares boolean lexical forms
  - FILTER expressions correctly compare boolean values
  - Moved fixed tests from manifest-negative.n3 to manifest.n3
- **2025-11-30**: Fixed ExprEquals integer equality tests (3 tests)
  - Fixed integer equality canonicalization to use value comparison instead of lexical form comparison
  - Ensures that '01'^^xsd:integer equals '1'^^xsd:integer per SPARQL value equality semantics
  - Applied to integer, decimal, and floating point numeric types
  - Removed obsolete RDQL-specific code and updated documentation
  - Moved fixed tests from manifest-failures.n3 to manifest.n3
- **2025-11-30**: Fixed Error API tests (error-api-3, error-api-4)
  - Implemented runtime debug level control via RASQAL_DEBUG_LEVEL environment variable
  - Test framework now sets RASQAL_DEBUG_LEVEL=0 to suppress all debug output during testing
  - All Error API tests now pass cleanly
- **2025-11-29**: Fixed Error API tests (error-api-2, error-api-5)
  - Updated expected results to match SPARQL 1.2 §18.6 Extend semantics for BIND expressions
  - Fixed test framework format detection for Turtle output files
- **2025-11-28**: Fixed FILTER+MINUS tests (3 tests)
- **2025-11-27**: Fixed negation tests (3 tests)

## Future Work

- Property Paths implementation

## Test Framework Changes

- **2025-11-30**: Implemented runtime debug level control via `RASQAL_DEBUG_LEVEL` environment variable
  - Test framework now sets `RASQAL_DEBUG_LEVEL=0` to suppress all debug output during testing
  - Eliminates need for fragile string-based debug message filtering
  - See `specs/rasqal-runtime-debug-control.md` for details

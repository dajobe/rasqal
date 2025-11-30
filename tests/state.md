# Rasqal Test Suite State

Last updated: 2025-11-30

This document describes the current state of the Rasqal test suite,
including test results, known limitations, and expected failures.

## Current Test Results

**Overall**: 1004 SPARQL tests passed, 0 failed, 34 expected failures

### Test Categories

| Category                 | Pass | Xfail | Notes                           |
|--------------------------|------|-------|---------------------------------|
| C unit tests (`src/`)    |   33 |     0 | Core library component tests    |
| SPARQL test directories  | 1004 |    34 | 32 directories, multiple suites |
| Compare tests (`utils/`) |    8 |     0 | Result comparison tests         |

The 1004 SPARQL tests run across 32 test directories, each running multiple
test suites (sparql-lexer, sparql-parser, sparql-query, etc.).

### XFailed Breakdown

The 34 xfailed tests come from two sources:

| Source                 | Unique | Xfails | Notes                                 |
|------------------------|--------|--------|---------------------------------------|
| XFailTest in manifests |     11 |     11 | Explicitly marked expected failures   |
| Negative syntax tests  |     23 |     23 | Parser expected to reject but doesn't |
| **Total**              | **34** | **34** |                                       |

## XFailed Tests (Expected Failures)

### XFailTest-based Failures (11 tests)

These tests are explicitly marked with `t:XFailTest` in manifest files.

#### 1. ValueTesting (7 tests)

Location: `tests/sparql/ValueTesting/manifest-negative.n3`

**Complexity**: HIGH (8/10) - Requires core type system changes
**Risk**: Medium - Changes affect type promotion, boolean handling, extended types
**Priority**: DEFER - High complexity, edge cases, low real-world impact

| Test                                 | Description              |
|--------------------------------------|--------------------------|
| `typePromotion-decimal-decimal-pass` | Type promotion           |
| `typePromotion-decimal-decimal-fail` | Type promotion           |
| `boolean-false-canonical`            | Boolean canonicalization |
| `boolean-true-canonical`             | Boolean canonicalization |
| `boolean-EBV-canonical`              | Boolean EBV              |
| `extendedType-ne-fail`               | Extended type comparison |
| `extendedType-literal-ne`            | Extended type literal    |

**Root causes**:

- **Type promotion tests** (2 tests, MEDIUM-HIGH complexity): Engine type promotion bug within xsd:decimal type tree
- **Boolean tests** (3 tests, MEDIUM complexity): Engine boolean handling bug with lexical forms (TRUE/FALSE/EBV)
- **Extended type tests** (2 tests, MEDIUM complexity): Engine extended type comparison bug - opaque types like
  `loc:latitude` and `loc:ECEF_X` should fail with type error but don't

**Code Locations**: `src/rasqal_literal.c` (type promotion, boolean handling, comparison)

#### 2. ExprEquals Tests (3 tests)

Location: `tests/sparql/ExprEquals/manifest-failures.n3`

**Complexity**: MEDIUM (5/10) - Localized integer canonicalization fix
**Risk**: Low - Changes localized to comparison logic
**Priority**: MEDIUM-TERM TARGET - Well-defined problem, clear solution

| Test                                      | Description                |
|-------------------------------------------|----------------------------|
| `Equality 1-1 -- graph`                   | Graph equality             |
| `Equality 1-2 -- graph`                   | Graph equality             |
| `Equality - 2 var - test equals -- graph` | Variable equality in graph |

**Root cause**: Engine integer equality bug - `'01'^^xsd:integer` not recognized
as equal to `1`. The engine should canonicalize integer lexical forms before
comparison.

**Code Locations**: `src/rasqal_literal.c:rasqal_literal_equals()` (lines 2924-2928)

#### 3. BIND Test (1 test)

Location: `tests/sparql/bind/manifest-failing.ttl`

**Complexity**: MEDIUM (5/10) - Scope validation logic fix
**Risk**: Low-Medium - Scoping logic change
**Priority**: LOW - Very specific edge case

| Test     | Description            |
|----------|------------------------|
| `bind07` | BIND in UNION branches |

**Root cause**: UNION variable scoping bug.

- **Problem**: Variable scoping validation in `rasqal_query_transform.c` doesn't
  properly isolate UNION branch scopes
- **Effect**: Query `{ BIND(?o+1 AS ?z) } UNION { BIND(?o+2 AS ?z) }` fails with
  error "BIND variable z already used in group graph pattern"
- **Expected**: Each UNION branch should have independent variable scope,
  allowing the same variable to be bound in each branch
- **Affected function**: `rasqal_query_graph_pattern_build_variables_use_map_binds()`

**Code Locations**: `src/rasqal_query_transform.c` (BIND variable validation)

### Negative Syntax Test Failures (23 tests)

These are tests where the parser is expected to reject invalid syntax
but currently accepts it. They run in `sparql-parser-negative` or
`sparql-bad-parser` test suites.

| Directory      | Count  | Notes                          |
|----------------|--------|--------------------------------|
| Syntax-SPARQL3 |     18 | SPARQL 1.1/1.2 syntax features |
| syntax         |      3 | Basic syntax rejection         |
| update         |      1 | SPARQL Update syntax           |
| aggregate      |      1 | Aggregate syntax               |
| **Total**      | **23** |                                |

**Root cause**: Parser accepts syntax that should be rejected.

- These tests contain intentionally invalid SPARQL syntax
- The parser should reject them with a syntax error
- Instead, the parser accepts the invalid queries
- Likely missing syntax validation rules in `sparql_parser.y`

## Actual Engine Compliance

### Not Engine Bugs (23 tests)

- **23 Negative syntax tests**: Parser validation, not query execution

### Actual Query Engine Failures (11 tests)

| Category       | Count | Bug Type                              |
|----------------|-------|---------------------------------------|
| ValueTesting   |     7 | Type promotion, boolean, extended type|
| ExprEquals     |     3 | Integer equality canonicalization     |
| BIND           |     1 | UNION variable scope isolation        |
| **Total**      |**11** | (3 negation tests fixed 2025-11-27, 3 FILTER+MINUS tests fixed 2025-11-28, 4 Error API tests fixed 2025-11-29 and 2025-11-30) |

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

- Fix UNION variable scoping for BIND (1 test)
- Property Paths implementation

## Test Framework Changes

- **2025-11-30**: Implemented runtime debug level control via `RASQAL_DEBUG_LEVEL` environment variable
  - Test framework now sets `RASQAL_DEBUG_LEVEL=0` to suppress all debug output during testing
  - Eliminates need for fragile string-based debug message filtering
  - See `specs/rasqal-runtime-debug-control.md` for details

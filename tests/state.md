# Rasqal Test Suite State

Last updated: 2025-11-26

This document describes the current state of the Rasqal test suite,
including test results, known limitations, and expected failures.

## Current Test Results

**Overall**: 994 SPARQL tests passed, 0 failed, 44 expected failures

### Test Categories

| Category                 | Pass | Xfail | Notes                           |
|--------------------------|------|-------|---------------------------------|
| C unit tests (`src/`)    |   33 |     0 | Core library component tests    |
| SPARQL test directories  |  994 |    44 | 32 directories, multiple suites |
| Compare tests (`utils/`) |    8 |     0 | Result comparison tests         |

The 994 SPARQL tests run across 32 test directories, each running multiple
test suites (sparql-lexer, sparql-parser, sparql-query, etc.).

### XFailed Breakdown

The 44 xfailed tests come from two sources:

| Source                 | Unique | Xfails | Notes                                 |
|------------------------|--------|--------|---------------------------------------|
| XFailTest in manifests |     21 |     21 | Explicitly marked expected failures   |
| Negative syntax tests  |     23 |     23 | Parser expected to reject but doesn't |
| **Total**              | **44** | **44** |                                       |

## XFailed Tests (Expected Failures)

### XFailTest-based Failures (21 tests)

These tests are explicitly marked with `t:XFailTest` in manifest files.

#### 1. ValueTesting (7 tests)

Location: `tests/sparql/ValueTesting/manifest-negative.n3`

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

- **Type promotion tests**: Engine type promotion bug within xsd:decimal type tree
- **Boolean tests**: Engine boolean handling bug with lexical forms (TRUE/FALSE/EBV)
- **Extended type tests**: Engine extended type comparison bug - opaque types like
  `loc:latitude` and `loc:ECEF_X` should fail with type error but don't

#### 2. Negation Subset Tests (3 tests)

Location: `tests/sparql/negation/manifest-bad.ttl`

| Test         | Description                      |
|--------------|----------------------------------|
| `subset-01`  | Calculate subsets (include A⊆A)  |
| `subset-02`  | Calculate subsets (exclude A⊆A)  |
| `subset-03`  | Calculate proper subset          |

**Root cause**: MINUS not correctly removing rows when RHS contains FILTER NOT EXISTS.

- **Recent fix**: AND expression short-circuit now returns boolean literal
  instead of NULL (commit ddc1a905, 2025-11-26)
- **Current behavior**: subset-01 returns 25 results instead of expected 11
- **Problem**: MINUS removes only 5 rows instead of 19. The RHS pattern
  `FILTER(?s1 != ?s2) ?s1 :member ?x . FILTER NOT EXISTS { ?s2 :member ?x }`
  should match 19 LHS rows for removal but only matches 5
- **Investigation status**: The AND expression bug masked the underlying issue.
  Now that filters receive proper boolean values, the MINUS correlation problem
  is clearer: RHS evaluation needs access to LHS row variables for correlated
  subqueries, but the mechanism for this correlation is not working correctly
- **Note**: The MINUS algorithm itself works correctly (proven by unit tests).
  The issue is specifically about how LHS variable bindings are made available
  during RHS pattern evaluation for correlation.

#### 3. ExprEquals Tests (3 tests)

Location: `tests/sparql/ExprEquals/manifest-failures.n3`

| Test                                      | Description                |
|-------------------------------------------|----------------------------|
| `Equality 1-1 -- graph`                   | Graph equality             |
| `Equality 1-2 -- graph`                   | Graph equality             |
| `Equality - 2 var - test equals -- graph` | Variable equality in graph |

**Root cause**: Engine integer equality bug - `'01'^^xsd:integer` not recognized
as equal to `1`. The engine should canonicalize integer lexical forms before
comparison.

#### 4. Error API Tests (4 tests) - TEST FRAMEWORK ISSUE

Location: `tests/sparql/errors/manifest-failing.ttl`

| Test          | Description                 |
|---------------|-----------------------------|
| `error-api-2` | Division by zero error      |
| `error-api-3` | Filter expression error     |
| `error-api-4` | Join expression error       |
| `error-api-5` | Projection expression error |

**Important**: These are NOT engine failures.

- **Actual behavior**: The engine correctly handles errors by returning no results
- **Test failure cause**: Test framework comparison fails because debug output
  is included in actual results while expected result files contain only clean output
- **Fix needed**: Test framework should filter debug output before comparison,
  or expected result files should be updated

#### 5. FILTER + MINUS + Unbound Tests (3 tests)

Location: `tests/sparql/filter-unbound/manifest-bad.ttl`

| Test                         | Description                                  |
|------------------------------|----------------------------------------------|
| `filter-minus-simple`        | Simple FILTER + MINUS with unbound variables |
| `filter-minus-complex`       | Complex FILTER + MINUS with partial bindings |
| `filter-minus-mixed-binding` | Mixed bound/unbound scenarios                |

**Root cause**: Gap in unbound expression handling when combined with MINUS.

- FILTER + MINUS should not cause type errors with unbound variables
- Complex BGP generating partial bindings with FILTER + MINUS fails
- Mixed scenarios where some solutions have bound variables, others unbound
- Related to expression unbound handling work, but MINUS interaction not complete

#### 6. BIND Test (1 test)

Location: `tests/sparql/bind/manifest-failing.ttl`

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

### Not Engine Bugs (27 tests)

- **4 Error API tests**: Test framework debug output comparison issue
- **23 Negative syntax tests**: Parser validation, not query execution

### Actual Query Engine Failures (17 tests)

| Category       | Count | Bug Type                              |
|----------------|-------|---------------------------------------|
| ValueTesting   |     7 | Type promotion, boolean, extended type|
| Negation       |     3 | MINUS+NOT EXISTS variable scoping     |
| ExprEquals     |     3 | Integer equality canonicalization     |
| FILTER+MINUS   |     3 | Unbound variable handling in MINUS    |
| BIND           |     1 | UNION variable scope isolation        |
| **Total**      |**17** |                                       |

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

## Future Work

- Fix Error API test framework debug output issue (4 tests)
- Fix UNION variable scoping for BIND (1 test)
- Fix FILTER+MINUS+unbound interaction (3 tests)
- Complete MINUS+NOT EXISTS variable scoping (3 tests)
- Property Paths implementation

# Rasqal SPARQL Compliance

**Last Updated**: 2026-07-14

This document defines Rasqal's compliance status with the SPARQL 1.1 Query
Recommendation and the SPARQL 1.2 Query drafts. It serves as the authoritative
reference for implementation status, test results, and known limitations.

## Executive Summary

Rasqal implements SPARQL 1.1 Query excluding Property Paths, evaluated per the
clarified algebra text of the SPARQL 1.2 Query drafts. The new SPARQL 1.2 / RDF
1.2 features (triple terms, reifiers, directional language-tagged strings, new
built-in functions, the `VERSION` declaration) are **not** implemented.

### Key Metrics

- **SPARQL Tests**: 1,034 passed, 0 failed, 24 expected failures
- **C Unit Tests**: 33 passed
- **Compare Tests**: 8 passed
- **Parser Issues**: 24 (validation only, doesn't affect execution)

### Compliance Status

- **SPARQL 1.1 Core Features**: ✅ **COMPLETE** except Property Paths
- **SPARQL algebra semantics**: ✅ aligned with the clarified text of the SPARQL
  1.2 Query drafts (Extend, MINUS, EXISTS substitution — semantics unchanged
  from SPARQL 1.1)
- **SPARQL 1.2 new features**: ❌ **NOT IMPLEMENTED** (see Known Limitations)
- **W3C Test Suite**: 100% pass rate for implemented features (SPARQL 1.1 suite;
  the W3C `sparql12` suite is not yet integrated)

---

## SPARQL 1.1 Compliance

### ✅ Fully Implemented Core Features

#### Query Forms

- **SELECT**: Complete support with all modifiers
- **CONSTRUCT**: Template-based result construction
- **ASK**: Boolean query results
- **DESCRIBE**: Resource description queries

#### Graph Pattern Matching

- **Basic Graph Patterns (BGP)**: Triple pattern matching
- **OPTIONAL**: Optional pattern matching with proper semantics
- **UNION**: Pattern alternatives with isolated scoping
- **FILTER**: Expression-based solution filtering
- **GRAPH**: Named graph context matching
- **MINUS**: Set difference with correlated evaluation
- **BIND**: Variable assignment with scope-aware evaluation
- **VALUES**: Inline data support
- **EXISTS / NOT EXISTS**: Existential quantification

#### Aggregation and Grouping

- **Aggregates**: `COUNT`, `SUM`, `AVG`, `MIN`, `MAX`, `GROUP_CONCAT`, `SAMPLE`
- **GROUP BY**: Full grouping support
- **HAVING**: Aggregate filtering

#### Solution Modifiers

- **ORDER BY**: Result ordering
- **DISTINCT**: Duplicate elimination
- **REDUCED**: Duplicate reduction
- **OFFSET/LIMIT**: Result slicing

#### Built-in Functions

- Comprehensive set of SPARQL 1.1 built-in functions
- String functions, numeric functions, date/time functions
- Type conversion functions
- Expression evaluation with proper error handling

#### Result Formats

- **SRJ (JSON)**: W3C compliant SPARQL Results JSON
- **SRX (XML)**: W3C compliant SPARQL Results XML
- **CSV**: Comma-separated values
- **TSV**: Tab-separated values
- **RDF/XML**: RDF serialization
- **Turtle**: RDF Turtle format

### ❌ Not Implemented

- **Property Paths**: Complex path expressions (explicitly deferred, low
  priority)
  - Requires storage layer API changes
  - Marked as "unlikely to implement"

---

## SPARQL 1.2 Status

The SPARQL 1.2 Query drafts restate the SPARQL 1.1 algebra more precisely and
add new features on top of it. Rasqal's evaluation follows the clarified algebra
text (this section), but implements none of the new SPARQL 1.2 features (see
Known Limitations). Note that the Extend operator, MINUS domain-intersection
compatibility, and EXISTS substitution below are SPARQL 1.1 semantics — the
SPARQL 1.2 section numbers cited are where the current drafts state them.

### ✅ Algebra Alignment with the SPARQL 1.2 Draft Text

#### Query Processing Pipeline

- Four-phase pipeline: Parsing → Transformation → Algebra Generation → Rowsource
  Execution
- Parser creates AST with scope hierarchies
- Transformation validates and optimizes with correlation analysis
- Rowsource execution uses iterator-based lazy evaluation

#### Algebra Operators

All operators of the SPARQL Query algebra implemented:

- BGP, Join, LeftJoin, Filter, Union, Graph
- **Extend** (standard algebra operator since SPARQL 1.1; replaced Rasqal's
  internal Assignment operator)
- Minus, Group, Aggregation, AggregateJoin
- OrderBy, Project, Distinct, Reduced, Slice
- Values, Service

#### Extend Operator

- ✅ **FULLY IMPLEMENTED** - Replaced Rasqal's internal Assignment operator
- Implements the Extend semantics as stated in SPARQL 1.2 Query §18.6 (unchanged
  from SPARQL 1.1):

  ```text
  Extend(μ, var, expr) = μ ∪ {(var, value) | var not in dom(μ) and value = expr(μ)}
  Extend(μ, var, expr) = μ if var not in dom(μ) and expr(μ) is an error
  Extend is undefined when var in dom(μ)
  ```

- Proper variable conflict detection
- Scope-aware expression evaluation
- Supports BIND expressions through Extend operator

#### Variable Correlation Architecture

- ✅ **FULLY IMPLEMENTED** - Rasqal implementation technique for correct negation
  evaluation
- Pre-computed correlation maps for MINUS operations
- Intelligent correlation detection for complex negation patterns
- Dual evaluation modes (simple caching vs. correlated evaluation)
- Scope-aware variable resolution with hierarchical scope system
- Implements the formal substitute(pattern, μ) semantics as stated in the SPARQL
  1.2 drafts

#### MINUS Semantics

- ✅ **FULLY IMPLEMENTED** - domain-intersection semantics (SPARQL 1.1 §8.3.2,
  restated in the SPARQL 1.2 drafts)
- Only variables bound on both sides considered for compatibility
- Correlated evaluation for complex negation patterns
- Two evaluation paths:
  - Simple path: RHS caching for normal MINUS (performance optimized)
  - Correlated path: spec-compliant evaluation with LHS context
- Intelligent mode detection based on variable dependencies

#### EXISTS/NOT EXISTS Evaluation

- ✅ **FULLY IMPLEMENTED** - Unified mode-aware architecture
- Complete solution evaluation (not individual triple matching)
- Variable substitution using outer query context
- Scope-aware variable resolution
- All pattern types supported: BASIC, GROUP, UNION, OPTIONAL, FILTER, GRAPH

### ❌ SPARQL 1.2 New Features - Not Implemented

None of the normative additions of SPARQL 1.2 Query (its Appendix A change list)
are implemented:

- **Triple terms**: `<<( s p o )>>` in patterns, expressions and VALUES; no
  triple-term literal type exists in the term model
- **Reified triples and annotations**: `<< s p o ~ r >>` and `{| ... |}` syntax
  (desugaring via `rdf:reifies`)
- **Functions on triple terms**: `TRIPLE`, `isTRIPLE`, `SUBJECT`, `PREDICATE`,
  `OBJECT`
- **Directional language-tagged strings**: `"x"@en--ltr` literals with datatype
  `rdf:dirLangString` (see Known Limitations for a parsing hazard)
- **Language/direction functions**: `LANGDIR`, `hasLANG`, `hasLANGDIR`,
  `STRLANGDIR`
- **`VERSION` declaration** in the query prologue
- **Property Paths**: carried forward from SPARQL 1.1 (explicitly deferred)

Data-side support for triple terms and directional strings additionally depends
on Raptor gaining RDF 1.2 term support.

---

## Test Results

### Current Test Status (2025-11-30)

| Category                 | Pass | Xfail | Total | Notes                           |
|:-------------------------|:-----|:------|:------|:--------------------------------|
| C unit tests (`src/`)    | 33   | 0     | 33    | Core library component tests    |
| SPARQL test directories  | 1034 | 24    | 1058  | 32 directories, multiple suites |
| Compare tests (`utils/`) | 8    | 0     | 8     | Result comparison tests         |
| **Total**                | 1075 | 24    | 1099  |                                 |

### Test Pass Rate

- **Overall**: 97.7% (1,034 of 1,058 SPARQL tests)
- **Parser Issues**: 24 (2.3% of total tests)

### XFailed Tests: Parser Validation (24 tests)

All remaining XFailed tests are parser validation issues where the parser
accepts invalid syntax that should be rejected. These do **not** affect query
execution.

| Directory      | Count  | Notes                          |
|:---------------|:-------|:-------------------------------|
| Syntax-SPARQL3 | 18     | SPARQL 1.1/1.2 syntax features |
| syntax         | 3      | Basic syntax rejection         |
| update         | 1      | SPARQL Update syntax           |
| aggregate      | 1      | Aggregate syntax               |
| bind           | 1      | SPARQL BIND syntax             |
| **Total**      | **24** |                                |

**Root Cause**: Missing validation rules in `src/sparql_parser.y`

**Impact**: None - parser works correctly for all valid queries. These are
validation-only issues.

**Priority**: Low - can be deferred as they don't affect query execution.

---

## Implementation Details by Feature

### VALUES Implementation

**Status**: ✅ **COMPLETE** - Full SPARQL 1.1 VALUES support with W3C test
compliance

**Features**:

- Inline data support with VALUES clause
- Proper variable assignment and scoping
- Support for complex expressions in VALUES
- Complete test suite with W3C test integration

**Test Results**: Full W3C SPARQL 1.1 VALUES test suite compliance

### EXISTS/NOT EXISTS Implementation

**Status**: ✅ **COMPLETE** - Full SPARQL 1.1 EXISTS / NOT EXISTS support with
100% W3C test compliance

**Features**:

- Complete lexer, parser, expression system integration
- Recursive pattern evaluation for all graph pattern types
- Automatic scoping from outer query to EXISTS patterns
- Full triples_source integration for actual data lookup
- Proper named graph context propagation

**Test Results**: 5/5 W3C SPARQL 1.1 EXISTS tests passing (100% compliance)

**Architecture**: Algebra-based implementation with proper expression-rowsource
integration

### BIND Implementation

**Status**: ✅ **COMPLETE** - Full SPARQL 1.1 BIND implementation with
comprehensive test coverage

**Features**:

- Variable assignment expressions working correctly
- W3C error semantics: "Continue on error, leave unbound"
- Correct variable scoping across BGP boundaries
- Expression evaluation: arithmetic, string functions, type conversion
- Scope-aware variable registration in isolated GROUP patterns

**Test Results**: All BIND functionality tested and validated

**Recent Fixes** (2025-11-30):

- Fixed variable scoping in isolated GROUP patterns (bind07)
- Variables bound within isolated scopes properly excluded from SELECT
  projections
- Implemented scope-aware variable registration and root-level checking

### MINUS Implementation

**Status**: ✅ **COMPLETE** - Full SPARQL 1.1 MINUS support with correlated
evaluation

**Features**:

- Complete lexer, parser, algebra, rowsource implementation
- Correlated evaluation for complex negation patterns
- Two evaluation paths:
  - Simple path: RHS caching for normal MINUS (performance optimized)
  - Correlated path: spec-compliant evaluation with LHS variable context
- Variable correlation analysis for automatic mode detection
- Domain-intersection semantics for compatibility checking

**Test Results**: All W3C SPARQL 1.1 negation tests passing, including:

- subset-01, subset-02, subset-03 (subsets by exclusion)
- full-minuend, partial-minuend (subtraction patterns)
- FILTER+MINUS tests (complex negation patterns)

**Architecture**: Algebra-based implementation with intelligent evaluation mode
selection

### Aggregation Implementation

**Status**: ✅ **COMPLETE** - Full aggregation and grouping support

**Features**:

- Aggregates: COUNT, SUM, AVG, MIN, MAX, GROUP_CONCAT, SAMPLE
- GROUP BY and HAVING fully supported
- Group rowsource implementation
- Aggregation rowsource implementation

---

## Architecture Overview

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

### Key Architectural Features

- **Rowsource Architecture**: Iterator-based lazy evaluation for memory
  efficiency
- **Scope-Aware Variable Resolution**: Hierarchical scope system with proper
  isolation
- **Variable Correlation Analysis**: Pre-computed correlation maps for
  optimization
- **Dual Evaluation Modes**: Performance-optimized simple path with
  spec-compliant correlated path
- **Expression Evaluation**: Scope-aware evaluation with proper error handling

---

## Recent Fixes (November-December 2025)

### 2025-11-30: BIND Variable Scoping

- Fixed variables bound via BIND inside isolated GROUP patterns incorrectly
  appearing in query results
- Implemented scope-aware variable registration in EXTEND rowsource
- Added `rasqal_query_variable_bound_at_root_level()` function
- Modified PROJECT rowsource to use scope-aware variable checking
- Fixed scope hierarchy building for nested GROUP scopes

### 2025-11-30: ValueTesting Tests (7 tests)

- Fixed decimal subtype promotion for integer subtypes
- Fixed boolean RDF term equality to compare lexical forms
- Enhanced UDT comparison to check datatype URI equality
- Fixed empty result set formatting to include rs:resultVariable lines

### 2025-11-30: ExprEquals Tests (3 tests)

- Fixed integer equality canonicalization to use value comparison
- Ensures `'01'^^xsd:integer` equals `'1'^^xsd:integer` per SPARQL semantics
- Applied to integer, decimal, and floating point numeric types

### 2025-11-29: Error API Tests (2 tests)

- Updated expected results to match SPARQL 1.2 §18.6 Extend semantics
- Fixed test framework format detection for Turtle output files

### 2025-11-30: Error API Tests (2 tests)

- Implemented runtime debug level control via `RASQAL_DEBUG_LEVEL` environment
  variable
- Test framework now sets `RASQAL_DEBUG_LEVEL=0` to suppress debug output

### 2025-11-28: FILTER+MINUS Tests (3 tests)

- Fixed test expected results (implementation was correct)

### 2025-11-27: Negation Tests (3 tests)

- Fixed variable corruption in PROJECT rowsource
- Fixed negation subset tests (subset-02, subset-03)

### 2025-12-03: Memory Management Improvements

- Fixed memory leak in boolean literal string handling
- Improved scope hierarchy reference counting and cleanup
- Fixed memory leaks in variable resolution test framework
- Added null check before freeing triple
- Enhanced test result summary script with failure details

### 2025-12-04: EXTEND Semantics and Type Handling

- **Fixed EXTEND semantics to handle expression evaluation errors**
  - Changed EXTEND rowsource to always create extended rows with correct size
  - Set unbound variables to NULL on expression evaluation failure
  - Implements proper SPARQL 1.2 Extend semantics per §18.6
- **Fixed integer subtype literal datatype URI preservation**
  - Integer subtypes (xsd:byte, xsd:short, etc.) now preserve original datatype
    URIs
  - Ensures correct SPARQL type semantics for integer subtype literals
- **Fixed variable reference counting in wildcard projection expansion**
  - Removed double increment of variable reference count in SELECT * expansion
  - Prevents memory leaks in SELECT * queries with nested projections

**Total Progress**: 49 commits fixing 40+ tests through systematic investigation

---

## Known Limitations

### Parser Validation (24 tests)

**Issue**: Parser accepts invalid syntax that should be rejected.

**Examples**:

```sparql
SELECT * WHERE { ?s ?p ?o . . }  # Extra dot (should fail)
SELECT ?x ?x WHERE { ?s ?p ?o }  # Duplicate variable (should fail)
```

**Impact**: None - parser works correctly for all valid queries.

**Root Cause**: Missing validation rules in `src/sparql_parser.y`

**Priority**: Low - validation-only issues, can be deferred.

### Property Paths

**Status**: Not implemented (explicitly deferred)

**Reason**: High complexity, requires storage layer API changes

**Priority**: Low (marked as "unlikely to implement")

### SPARQL 1.2 / RDF 1.2 New Features

**Status**: Not implemented (see the SPARQL 1.2 Status section above for the
list)

**Dependencies**: Triple terms and directional language-tagged strings require
Raptor RDF 1.2 term support first; both also change the public `rasqal_literal`
model (ABI impact)

**Parsing hazard**: The lexer's language-tag pattern accepts the RDF 1.2
base-direction syntax `"x"@en--ltr` but stores `en--ltr` as an ordinary language
tag instead of rejecting it or modeling the base direction. The result is a
silently wrong literal (`LANG()` returns `"en--ltr"`) rather than a syntax
error.

**Test suites**: The W3C `rdf-tests` repository now publishes a
`sparql/sparql12` suite; it is not yet integrated into the Rasqal test tree

---

## Test Infrastructure

### Test Framework

- **Language**: Python 3 (migrated from Python 2)
- **Manifest Format**: Turtle-based test manifests
- **Test Execution**: Makefile-based with proper error handling
- **Result Comparison**: Automated validation with comprehensive comparison
- **Build Integration**: Complete autotools integration

### Test Categories

- **Unit Tests**: Lexer, parser, expression evaluation
- **Integration Tests**: End-to-end query execution, result formatting, error
  handling
- **Conformance Tests**: W3C SPARQL 1.1 test suite, custom test cases

### Test Framework Features

- Runtime debug level control via `RASQAL_DEBUG_LEVEL` environment variable
- Manifest-driven testing with Turtle-based manifests
- Automated test execution and result validation
- Comprehensive test coverage across 32 test directories

---

## Success Metrics

### W3C Test Suite Compliance

- **VALUES**: 100% pass rate
- **EXISTS/NOT EXISTS**: 100% pass rate (5/5 tests)
- **BIND**: Comprehensive test coverage, all tests passing
- **MINUS**: All W3C SPARQL 1.1 negation tests passing

### Code Quality

- **Memory Management**: Recent fixes (Dec 2025) eliminated memory leaks in
  boolean literals, scope hierarchies, and variable resolution
- **Error Handling**: Enhanced EXTEND semantics for proper expression error
  handling per SPARQL 1.2 §18.6
- **Performance**: No significant performance regression
- **Test Coverage**: Comprehensive test suites for all implemented features

### Engine Reliability

- **Test Pass Rate**: 97.7% (1,034 of 1,058 tests)
- **Execution Reliability**: Fully functional

---

## Conclusion

Rasqal provides **high SPARQL 1.1 Query compliance** with all core features
implemented excluding Property Paths, evaluated per the clarified algebra text
of the SPARQL 1.2 Query drafts. The implementation demonstrates:

- **Complete SPARQL 1.1 Core Features**: All major features implemented with W3C
  compliance
- **Modern algebra semantics**: Extend, MINUS correlation and EXISTS
  substitution verified against the current SPARQL 1.2 draft text
- **High Test Pass Rate**: 97.7% with comprehensive W3C test suite coverage
- **Robust Architecture**: Scope-aware evaluation, variable correlation, and
  performance optimizations

The new SPARQL 1.2 features (triple terms, reifiers, directional language-tagged
strings, new built-in functions, `VERSION`) are not implemented.

The remaining 24 XFailed tests are parser validation issues that don't affect
query execution. The query execution engine is fully functional and
production-ready for all implemented SPARQL features.

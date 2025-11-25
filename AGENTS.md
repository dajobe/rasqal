# Rasqal Agent Guide

This file provides guidance to AI coding assistants when working with
code in this repository.

## Project Overview

Rasqal is a C library that handles RDF query language syntaxes, query
construction and execution. It's part of the Redland RDF framework.

Key features:

- SPARQL 1.0 and partial SPARQL 1.1 query support
- Multiple result formats (XML, JSON, CSV, TSV, HTML, RDF/XML, Turtle)
- Pluggable triple store API
- Command-line utility `roqet` for query execution

## Build & Test Commands

- `./autogen.sh && ./configure` - First-time setup (requires autotools)
- `make` or `make all` - Build the library and utilities
- `make check` - Run all test suites (equivalent to `make test`)
- `python3 tests/bin/run-test-suites <directory>` - Run tests in specific directory
- `cd tests/<suite> && make check-local` - Run tests for specific test suite

### Additional Testing Commands

```bash
# Run SPARQL test suite
cd tests/sparql && python3 bin/run-sparql-tests

# Run algebra tests
cd tests/algebra && python3 bin/test-algebra

# Individual component tests (examples)
./src/rasqal_expr_test
./src/rasqal_literal_test
./src/rasqal_datetime_test
```

### Utilities

```bash
# Query utility (main CLI tool)
./utils/roqet -q "SELECT * WHERE { ?s ?p ?o }" data.ttl

# Configuration tool
./src/rasqal-config --help
```

## Architecture

- **Core Library**: C library for SPARQL query processing (`src/rasqal_*.c`)
- **Query Engine**: Algebra-based execution with rowsource pipeline (`rasqal_engine.c`, `rasqal_rowsource_*.c`)
- **Key Utilities**: `roqet` (query tool), `rasqal-compare` (result comparison) in `utils/`
- **Dependencies**: Requires Raptor 2.0.7+, optionally MPFR/GMP, libxml2, UUID and libyajl libraries
- **Build System**: Autotools (autoconf/automake) with extensive configuration options
- **Test Framework**: Python-based W3C SPARQL test suite orchestration in `tests/sparql_test_framework/`

### Additional Components

- **Query Parsing**: SPARQL lexer/parser (`sparql_lexer.l`, `sparql_parser.y`)
- **Expression Evaluation**: SPARQL built-in functions and operators (`rasqal_expr_*.c`)
- **Result Formatting**: Multiple output format handlers (`rasqal_format_*.c`)
- **Data Types**: Literals, variables, triples, graphs (`rasqal_literal.c`, `rasqal_variable.c`, etc.)

### Query Execution Pipeline

The query engine uses a "rowsource" architecture where data flows
through specialized components:

- Triples rowsource feeds basic graph patterns
- Filter, join, union, aggregation rowsources transform data
- Final projection and ordering before results output

## Key Files and Directories

### Source Code

- `src/`: Main library source code
  - `rasqal.h.in`: Public API header template
  - `rasqal_internal.h`: Internal API definitions
  - `rasqal_*_test.c`: Unit tests for individual modules

### Build System

- `configure.ac`: Autotools configuration
- `Makefile.am`: Automake build rules
- `autogen.sh`: Bootstrap script for development builds

### Testing

- `tests/`: Test framework and test suites
  - `bin/run-test-suites`: Main test runner script
  - `rasqal_test_util.py`: Test utilities and manifest parsing
  - `sparql/`: SPARQL conformance tests
  - `algebra/`: Query algebra tests

### Utility Files

- `utils/roqet.c`: Command-line query tool
- `scripts/`: Development and build scripts

## Dependencies

### Required

- **Raptor2**: RDF parsing/serializing library
- **Regex library**: PCRE2, PCRE, or POSIX regex

### Optional

- **libxml2**: XML Schema support
- **MPFR/GMP**: High-precision decimal arithmetic
- **mhash/gcrypt**: Message digest functions
- **libuuid**: UUID generation

## Code Style

- **Language**: C with C99/C11 features, 2-space indentation
- **Naming**: `rasqal_` prefixes for public API, descriptive function/variable names
- **Memory Management**: Use `RASQAL_MALLOC/FREE` macros, not direct malloc/free
- **Headers**: Include `rasqal.h.in` for public API, `rasqal_internal.h` for internals
- **Error Handling**: Use rasqal error codes and logging macros
- **Documentation**: Function comments in header files, minimal inline comments
- **Conventions**: No single-line functions, no {}s for single-line if statements
- **Symbols**: All public symbols prefixed with `rasqal_`
- **Modularity**: Extensive use of function pointers
- **Errors**: Return codes and error callbacks

## Development Notes

### Parser Generation

- Uses Flex for lexical analysis (`sparql_lexer.l`)
- Uses Bison for parsing (`sparql_parser.y`)
- Generated files are included in releases but must be regenerated in development

### Memory Management

- Extensive use of reference counting for objects
- Memory debugging available with `--with-memory-signing`
- Valgrind testing support via `make check`

## Notes

- This is part of the Redland RDF toolkit ecosystem
- Extensive compiler warnings enabled in maintainer mode
- Test framework supports multiple query languages but focuses on SPARQL and multiple test types
- After major changes, run 'make check' at the top of the repository

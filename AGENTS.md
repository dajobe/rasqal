# Rasqal Agent Guide

## Build & Test Commands
- `make` or `make all` - Build the library and utilities
- `make check` - Run all test suites (equivalent to `make test`)
- `make check-unit` - Run framework unit tests only
- `python3 tests/bin/run-test-suites <directory>` - Run tests in specific directory
- `cd tests/<suite> && make check-local` - Run tests for specific test suite

## Architecture
- **Core Library**: C library for SPARQL 1.0/1.1 query processing (`src/rasqal_*.c`)
- **Key Utilities**: `roqet` (query tool), `rasqal-compare` (result comparison) in `utils/`
- **Dependencies**: Requires Raptor 2.0.7+, optionally MPFR/GMP, libxml2, UUID libraries
- **Build System**: Autotools (autoconf/automake) with extensive configuration options
- **Test Framework**: Python-based W3C SPARQL test suite orchestration in `tests/sparql_test_framework/`

## Code Style
- **Language**: C with C11 features, 2-space indentation
- **Naming**: `rasqal_` prefixes for public API, descriptive function/variable names
- **Memory Management**: Use `RASQAL_MALLOC/FREE` macros, not direct malloc/free
- **Headers**: Include `rasqal.h` for public API, `rasqal_internal.h` for internals
- **Error Handling**: Use rasqal error codes and logging macros
- **Documentation**: Function comments in header files, minimal inline comments

## Notes
- This is part of the Redland RDF toolkit ecosystem
- Extensive compiler warnings enabled in maintainer mode
- Test framework supports multiple query languages (SPARQL, LAQRS) and test types

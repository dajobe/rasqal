#!/usr/bin/env python3
"""
Unit tests for the SPARQL Test Framework.

This package contains comprehensive unit tests for all modules in the
sparql_test_framework package. Tests are co-located with the code they test
for better maintainability and discoverability.

Test Structure:
- Core module tests (config, manifest, types, execution, utils)
- Runner tests (orchestrator, sparql, format runners, algebra)
- Tool tests (plan_generator, packager)
- Integration tests for cross-module functionality

Copyright (C) 2025, David Beckett https://www.dajobe.org/

This package is Free Software and part of Redland http://librdf.org/

It is licensed under the following three licenses as alternatives:
  1. GNU Lesser General Public License (LGPL) V2.1 or any newer version
  2. GNU General Public License (GPL) V2 or any newer version
  3. Apache License, V2.0 or any newer version

You may not use this file except in compliance with at least one of
the above three licenses.

See LICENSE.html or LICENSE.txt at the top of this package for the
complete terms and further detail along with the license texts for
the licenses in COPYING.LIB, COPYING and LICENSE-2.0.txt respectively.
"""

__version__ = "2.0.0"

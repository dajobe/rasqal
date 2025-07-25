# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for SPARQL Test Framework unit tests
#
# Copyright (C) 2025, David Beckett https://www.dajobe.org/
#
# This package is Free Software and part of Redland http://librdf.org/
#
# It is licensed under the following three licenses as alternatives:
#   1. GNU Lesser General Public License (LGPL) V2.1 or any newer version
#   2. GNU General Public License (GPL) V2 or any newer version
#   3. Apache License, V2.0 or any newer version
#
# You may not use this file except in compliance with at least one of
# the above three licenses.
#
# See LICENSE.html or LICENSE.txt at the top of this package for the
# complete terms and further detail along with the license texts for
# the licenses in COPYING.LIB, COPYING and LICENSE-2.0.txt respectively.
#

# Core module test files
CORE_TESTS = \
	__init__.py \
	test_base.py \
	test_config.py \
	test_execution.py \
	test_manifest.py \
	test_types.py \
	test_utils.py

# Runner test files
RUNNER_TESTS = \
	runners/__init__.py \
	runners/test_orchestrator.py \
	runners/test_sparql.py

# Tool test files
TOOL_TESTS = \
	tools/__init__.py \
	tools/test_plan_generator.py

# All test files for distribution
EXTRA_DIST = \
	$(CORE_TESTS) \
	$(RUNNER_TESTS) \
	$(TOOL_TESTS) \
	test_data

# Test execution targets
check-framework-unit: $(EXTRA_DIST)
	@echo "Running SPARQL Test Framework unit tests..."
	@PYTHONPATH=$(top_srcdir)/tests:$(srcdir)/../../..:$$PYTHONPATH python3 -m unittest discover -s $(srcdir) -p "test_*.py" -v

check-framework-unit-quick: $(EXTRA_DIST)
	@echo "Running SPARQL Test Framework unit tests (quick mode)..."
	@PYTHONPATH=$(top_srcdir)/tests:$(srcdir)/../../..:$$PYTHONPATH python3 -m unittest discover -s $(srcdir) -p "test_*.py"

# Integration with main test suite
check-local: check-framework-unit

.PHONY: check-framework-unit check-framework-unit-quick

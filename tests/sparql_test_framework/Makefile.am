# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for SPARQL Test Framework
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

# Subdirectory with co-located unit tests
SUBDIRS = tests

# Core framework modules
FRAMEWORK_MODULES = \
	__init__.py \
	config.py \
	execution.py \
	manifest.py \
	test_types.py

# Data package modules
DATA_MODULES = \
	data/__init__.py \
	data/data_models.py

# Utils package modules
UTILS_MODULES = \
	utils/__init__.py \
	utils/core_utils.py \
	utils/exceptions.py \
	utils/temp_file_manager.py

# Runner modules
RUNNER_MODULES = \
	runners/__init__.py \
	runners/algebra.py \
	runners/csv_tsv.py \
	runners/format_base.py \
	runners/orchestrator.py \
	runners/sparql.py \
	runners/srj.py

# Tool modules  
TOOL_MODULES = \
	tools/__init__.py \
	tools/packager.py \
	tools/plan_generator.py

# All framework files for distribution
EXTRA_DIST = \
	$(FRAMEWORK_MODULES) \
	$(DATA_MODULES) \
	$(UTILS_MODULES) \
	$(RUNNER_MODULES) \
	$(TOOL_MODULES)

# Unit test target
check-unit:
	cd tests && python3 -m unittest discover -v

# Clean target for unit tests
clean-unit:
	find tests -name "__pycache__" -type d -exec rm -rf {} + 2>/dev/null || true
	find tests -name "*.pyc" -delete 2>/dev/null || true

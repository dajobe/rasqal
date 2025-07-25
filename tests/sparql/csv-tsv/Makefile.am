# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal CSV/TSV format tests
#
# Copyright (C) 2025, David Beckett http://www.dajobe.org/
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

CSV_TSV_MANIFEST_FILE = manifest.ttl

EXTRA_DIST = \
$(CSV_TSV_MANIFEST_FILE)

.PHONY: build-core bbuild-roqet check-local get-testsuites-list get-testsuite-format-csv-tsv


# This sequence allows parallel building of build-core and then once
# that is built, parallel building of the dependent programs: roqet,
# sparql_lexer_test and sparql_parser_test
build-core:
	@(cd $(top_builddir)/src ; $(MAKE))
	@(cd $(top_builddir)/utils ; $(MAKE))

build-roqet: build-core
	@(cd $(top_builddir)/utils ; $(MAKE) roqet)

# Run tests locally through bin/run-test-suites
check-local: build-roqet
	@$(top_srcdir)/tests/bin/run-test-suites .

# Register with bin/run-test-suites test discovery
get-testsuites-list:
	@echo "format-csv-tsv"

# Generate test plan for bin/run-test-suites (following algebra pattern)
get-testsuite-format-csv-tsv:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(top_srcdir)/tests/bin/create-test-plan \
	--suite-name format-csv-tsv \
	--srcdir $(srcdir) \
	--builddir $(top_builddir) \
	--test-category generic \
	--test-type PositiveTest \
	--action-template "$(top_srcdir)/tests/bin/test-csv-tsv-format --test-case {test_id} --srcdir {srcdir}" \
	--comment-template "CSV/TSV format test {test_id}" \
	--manifest-file $(top_srcdir)/tests/sparql/csv-tsv/$(CSV_TSV_MANIFEST_FILE)

# Distribution support
dist-hook:
	$(PYTHON3) $(top_srcdir)/tests/bin/bundle-test-files \
		--manifest-file $(srcdir)/$(CSV_TSV_MANIFEST_FILE) \
		--srcdir $(srcdir) \
		--distdir $(distdir)

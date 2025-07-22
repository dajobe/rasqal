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
	check_format_csv_tsv.py \
	$(CSV_TSV_MANIFEST_FILE)

# Run tests locally through improve.py
check-local:
	@$(srcdir)/../../improve.py .

# Register with improve.py test discovery
get-testsuites-list:
	@echo "format-csv-tsv"

# Generate test plan for improve.py (following algebra pattern)
get-testsuite-format-csv-tsv:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(srcdir)/../generate_sparql_plan.py \
		--suite-name format-csv-tsv \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-category generic \
		--test-type PositiveTest \
		--action-template "$(PYTHON3) $(srcdir)/check_format_csv_tsv.py --test-case {test_id} --srcdir $(srcdir)" \
		--comment-template "CSV/TSV format test {test_id}" \
		--manifest-file $(top_srcdir)/tests/sparql/csv-tsv/$(CSV_TSV_MANIFEST_FILE)

# Distribution support
dist-hook:
	$(PYTHON3) $(srcdir)/../package_sparql_tests.py \
		--manifest-file $(srcdir)/$(CSV_TSV_MANIFEST_FILE) \
		--srcdir $(srcdir) \
		--distdir $(distdir)

.PHONY: check-local get-testsuites-list get-testsuite-format-csv-tsv

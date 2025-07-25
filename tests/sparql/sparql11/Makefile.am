# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal SPARQL 1.1 tests
#
# Copyright (C) 2010, David Beckett http://www.dajobe.org/
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

MANIFEST_FILE = manifest.ttl

EXTRA_DIST= \
data-1.ttl \
subquery-1.ttl \
manifest.ttl

CLEANFILES=sparql-parser-positive-plan.ttl sparql-parser-negative-plan.ttl sparql-query-plan.ttl

build-sparql-parser-test:
	@(cd $(top_builddir)/src ; $(MAKE) sparql_parser_test)

check-local: build-sparql-parser-test
	@$(PYTHON) $(top_srcdir)/tests/bin/run-test-suites .

get-testsuites-list:
	@echo "sparql-parser-positive"

get-testsuite-sparql-parser-positive:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(top_srcdir)/tests/bin/create-test-plan \
		--suite-name sparql-parser-positive \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type PositiveTest \
		--action-template "$(top_builddir)/src/sparql_parser_test -i sparql11 {abs_srcdir}/{query_file}" \		--manifest-file $(srcdir)/$(MANIFEST_FILE)

get-testsuite-sparql-parser-negative:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(top_srcdir)/tests/bin/create-test-plan \
		--suite-name sparql-parser-negative \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type NegativeTest \
		--action-template "$(top_builddir)/src/sparql_parser_test -i sparql11 {abs_srcdir}/{query_file}" \		--manifest-file $(srcdir)/$(MANIFEST_FILE)

get-testsuite-sparql-query:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(top_srcdir)/tests/bin/create-test-plan \
		--suite-name sparql-query \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type PositiveTest \
		--action-template "$(PYTHON3) $(top_srcdir)/tests/bin/run-sparql-tests -s {srcdir} --manifest-file {srcdir}/$(MANIFEST_FILE --test-case {test_id}" \
		--manifest-file $(srcdir)/$(MANIFEST_FILE)

dist-hook:
	$(PYTHON3) $(top_srcdir)/tests/bin/bundle-test-files \
		--manifest-file $(srcdir)/$(MANIFEST_FILE) \
		--srcdir $(srcdir) \
		--distdir $(distdir)

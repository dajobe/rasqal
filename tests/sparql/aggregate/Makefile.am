# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal SPARQL aggregate tests
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

SPARQL_MANIFEST_FILES= manifest-good.n3 manifest-bad.n3

SPARQL_MODEL_FILES= \
data-1.ttl \
data-2.ttl

SPARQL_TEST_FILES= \
agg-1.rq \
agg-2.rq \
agg-3.rq \
group-concat-1.rq \
group-concat-2.rq \
group-concat-3.rq \
group-concat-4.rq

SPARQL_BAD_TEST_FILES= \
bad-1.rq

SPARQL_RESULT_FILES= \
agg-1.ttl \
agg-2.ttl \
agg-3.ttl \
group-concat-1.ttl \
group-concat-2.ttl \
group-concat-3.ttl \
group-concat-4.ttl

EXTRA_DIST= \
$(SPARQL_MANIFEST_FILES) \
$(SPARQL_MODEL_FILES) \
$(SPARQL_TEST_FILES) \
$(SPARQL_BAD_TEST_FILES) \
$(SPARQL_RESULT_FILES)

CLEANFILES=diff.out roqet.err roqet.out roqet.tmp result.out

build-sparql-parser-test:
	@(cd $(top_builddir)/src ; $(MAKE) sparql_parser_test)

check-local: build-sparql-parser-test
	@$(PYTHON) $(srcdir)/../../improve.py .

get-testsuites-list:
	@echo "sparql-parse-good sparql-parse-bad sparql-query"

get-testsuite-sparql-parse-good:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(srcdir)/../generate_sparql_plan.py \
		--suite-name sparql-parse-good \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type PositiveTest \
		--action-template "$(top_builddir)/src/sparql_parser_test -i sparql11 {srcdir}/{query_file}" \
		--comment-template "sparql parsing of {query_file}" \
		--manifest-file $(srcdir)/manifest-good.n3

get-testsuite-sparql-parse-bad:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(srcdir)/../generate_sparql_plan.py \
		--suite-name sparql-parse-bad \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type NegativeTest \
		--action-template "$(top_builddir)/src/sparql_parser_test -i sparql11 {srcdir}/{query_file}" \
		--comment-template "sparql failing to parse of {query_file}" \
		--manifest-file $(srcdir)/manifest-bad.n3

get-testsuite-sparql-query:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(srcdir)/../generate_sparql_plan.py \
		--suite-name sparql-query \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type PositiveTest \
		--action-template "$(PYTHON) $(srcdir)/../check_sparql.py -s {srcdir} {test_id}" \
		--comment-template "sparql query {test_id}" \
		--manifest-file $(srcdir)/manifest-good.n3

dist-hook:
	$(PYTHON3) $(srcdir)/../package_sparql_tests.py \
		--manifest-file $(srcdir)/manifest-good.n3 \
		--srcdir $(srcdir) \
		--distdir $(distdir)
	$(PYTHON3) $(srcdir)/../package_sparql_tests.py \
		--manifest-file $(srcdir)/manifest-bad.n3 \
		--srcdir $(srcdir) \
		--distdir $(distdir)

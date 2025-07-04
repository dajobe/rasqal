# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal syntax SPARQL tests
#
# Copyright (C) 2005-2008, David Beckett http://www.dajobe.org/
# Copyright (C) 2005-2005, University of Bristol, UK http://www.bristol.ac.uk/
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

SPARQL_MANIFEST_FILES= manifest.n3

SPARQL_MODEL_FILES= \
data-eq.ttl

SPARQL_TEST_FILES= \
query-eq-1.rq query-eq-2.rq query-eq-3.rq query-eq-4.rq query-eq-5.rq	\
query-eq2-1.rq query-eq2-2.rq query-eq2-graph-1.rq \
query-eq-graph-1.rq query-eq-graph-2.rq query-eq-graph-3.rq	\
query-eq-graph-4.rq query-eq-graph-5.rq

EXPECTED_SPARQL_CORRECT= \
  "Equality 1-1" \
  "Equality 1-2" \
  "Equality 1-3" \
  "Equality 1-4" \
  "Equality 1-5" \
  "Equality - 2 var - test equals" \
  "Equality 1-3 -- graph" \
  "Equality 1-4 -- graph" \
  "Equality 1-5 -- graph"

EXPECTED_SPARQL_CORRECT_FAILURES= \
  "Equality 1-1 -- graph" \
  "Equality 1-2 -- graph" \
  "Equality - 2 var - test equals -- graph"

SPARQL_RESULT_FILES= \
result-eq-1.ttl result-eq-2.ttl result-eq-3.ttl result-eq-4.ttl	\
result-eq-5.ttl \
result-eq2-1.ttl result-eq2-2.ttl result-eq2-graph-1.ttl \
result-eq-graph-1.ttl result-eq-graph-2.ttl \
result-eq-graph-3.ttl result-eq-graph-4.ttl \
result-eq-graph-5.ttl


EXTRA_DIST= \
$(SPARQL_MANIFEST_FILES) \
$(SPARQL_MODEL_FILES) \
$(SPARQL_TEST_FILES) \
$(SPARQL_RESULT_FILES)

CLEANFILES=diff.out roqet.err roqet.out roqet.tmp result.out

.PHONY: build-core build-roqet build-sparql-test-utils check-local get-testsuites-list get-testsuite-sparql-lexer get-testsuite-sparql-parser get-testsuite-sparql-query get-testsuite-sparql-query-failures

# This sequence allows parallel building of build-core and then once
# that is built, parallel building of the dependent programs: roqet,
# sparql_lexer_test and sparql_parser_test
build-core:
	@(cd $(top_builddir)/src ; $(MAKE))

build-roqet: build-core
	@(cd $(top_builddir)/utils ; $(MAKE) roqet)

build-sparql-test-utils: build-core
	@(cd $(top_builddir)/src ; $(MAKE) sparql_lexer_test sparql_parser_test)

check-local: build-roqet build-sparql-test-utils
	@$(PYTHON) $(srcdir)/../../improve.py .

get-testsuites-list:
	@echo "sparql-lexer sparql-parser sparql-query"

get-testsuite-sparql-lexer:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(srcdir)/../generate_sparql_plan.py \
		--suite-name sparql-lexer \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type PositiveTest \
		--action-template "$(top_builddir)/src/sparql_lexer_test {srcdir}/{query_file}" \
		--comment-template "sparql lexer of {query_file}" \
		--manifest-file $(srcdir)/manifest.n3

get-testsuite-sparql-parser:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(srcdir)/../generate_sparql_plan.py \
		--suite-name sparql-parser \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type PositiveTest \
		--action-template "$(top_builddir)/src/sparql_parser_test {srcdir}/{query_file}" \
		--comment-template "sparql parser of {query_file}" \
		--manifest-file $(srcdir)/manifest.n3

get-testsuite-sparql-query:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(srcdir)/../generate_sparql_plan.py \
		--suite-name sparql-query \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type PositiveTest \
		--action-template "$(PYTHON) $(srcdir)/../check_sparql.py -s {srcdir} {test_id}" \
		--comment-template "sparql query {test_id}" \
		--manifest-file $(srcdir)/manifest.n3

get-testsuite-sparql-query-failures:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(srcdir)/../generate_sparql_plan.py \
		--suite-name sparql-query-failures \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type NegativeTest \
		--action-template "$(PYTHON) $(srcdir)/../check_sparql.py -s $(srcdir) {test_id}" \
		--comment-template "sparql query {test_id}" \
		--manifest-file $(srcdir)/manifest.n3

dist-hook:
	$(PYTHON3) $(srcdir)/../package_sparql_tests.py \
		--manifest-file $(srcdir)/$(SPARQL_MANIFEST_FILES) \
		--srcdir $(srcdir) \
		--distdir $(distdir)

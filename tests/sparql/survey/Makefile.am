# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal SPARQL tests - examples
#
# Copyright (C) 2005-2009, David Beckett http://www.dajobe.org/
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

SPARQL_MANIFEST_FILES= manifest.ttl

SPARQL_MODEL_FILES= \
survey-sample.ttl

SPARQL_TEST_FILES= \
query-survey-1.rq query-survey-2.rq query-survey-3.rq	\
query-survey-8.rq query-survey-9.rq query-survey-10.rq	\
query-survey-11.rq query-survey-12.rq query-survey-13.rq

SPARQL_RESULT_FILES= \
result-survey-1.ttl result-survey-2.ttl result-survey-3.ttl	\
result-survey-8.ttl result-survey-9.ttl result-survey-10.ttl	\
result-survey-11.ttl result-survey-12.ttl result-survey-13.ttl

EXPECTED_SPARQL_CORRECT= \
   survey-1 \
   survey-2 \
   survey-3 \
   survey-8 \
   survey-9 \
   survey-10 \
   survey-11 \
   survey-12 \
   survey-13

# Test failures
EXPECTED_SPARQL_CORRECT_FAILURES=

EXTRA_DIST= \
$(SPARQL_MANIFEST_FILES) \
$(SPARQL_MODEL_FILES) \
$(SPARQL_TEST_FILES) \
$(SPARQL_RESULT_FILES)

CLEANFILES=diff.out roqet.err roqet.out roqet.tmp result.out

.PHONY: build-core build-roqet build-sparql-test-utils check-local get-testsuites-list get-testsuite-sparql-lexer get-testsuite-sparql-parser

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

get-testsuite-sparql-lexer get-testsuite-sparql-parser:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(srcdir)/../generate_sparql_plan.py \
		--suite-name sparql-parser \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type PositiveTest \
		--action-template "$(top_builddir)/src/sparql_parser_test {srcdir}/{query_file}" \
		--comment-template "sparql parser of {query_file}" \
		--manifest-file $(srcdir)/manifest.ttl

get-testsuite-sparql-query:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(srcdir)/../generate_sparql_plan.py \
		--suite-name sparql-query \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type PositiveTest \
		--action-template "$(PYTHON) $(srcdir)/../check_sparql.py -s {srcdir} {test_id}" \
		--comment-template "sparql query {test_id}" \
		--manifest-file $(srcdir)/manifest.ttl

dist-hook:
	$(PYTHON3) $(srcdir)/../package_sparql_tests.py \
		--manifest-file $(srcdir)/$(SPARQL_MANIFEST_FILES) \
		--srcdir $(srcdir) \
		--distdir $(distdir)

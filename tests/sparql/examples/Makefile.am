# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal SPARQL tests
#
# Copyright (C) 2004-2008, David Beckett http://www.dajobe.org/
# Copyright (C) 2004-2005, University of Bristol, UK http://www.bristol.ac.uk/
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
ex2-1a.n3 ex2-1d.n3 ex2-1f.n3 ex2-2a.n3 ex2-3a.n3 ex2-4a.n3 ex3.n3	\
ex11.2.3.1_0.n3 ex11.2.3.1_1.n3 ex11.2.3.2_0.n3 ex11.2.3.3_0.n3		\
ex11.2.3.4_0.n3 ex11.2.3.5_0.n3 ex11.2.3.6_0.n3 ex11.2.3.7_0.n3		\
ex11.2.3.8_0.n3 ex11_0.n3

SPARQL_TEST_FILES= \
ex2-1a.rq ex2-1b.rq ex2-1c.rq ex2-1e.rq ex2-2a.rq ex2-3a.rq ex2-4a.rq	\
ex3.rq ex11.2.3.1_0.rq ex11.2.3.1_1.rq ex11.2.3.2_0.rq			\
ex11.2.3.2_1.rq ex11.2.3.3_0.rq ex11.2.3.4_0.rq ex11.2.3.5_0.rq		\
ex11.2.3.6_0.rq ex11.2.3.7_0.rq ex11.2.3.8_0.rq ex11_0.rq ex11_1.rq

EXPECTED_SPARQL_CORRECT= \
  sparql-query-example-a \
  sparql-query-example-b \
  sparql-query-example-c \
  sparql-query-example-d \
  sparql-query-example-e \
  sparql-query-example-Testing-Values-0 \
  sparql-query-example-Testing-Values-1 \
  sparql-query-example-Testing-Values-RDFterm-equal \
  sparql-query-example-Testing-Values-RDFterm-equal-2 \
  sparql-query-example-Testing-Values-isURI \
  sparql-query-example-Testing-Values-isLiteral \
  sparql-query-example-Testing-Values-str \
  sparql-query-example-Testing-Values-lang \
  sparql-query-example-Testing-Values-isBlank \
  sparql-query-example-Testing-Values-bound \
  sparql-query-example-Testing-Values-bound-2

EXPECTED_SPARQL_CORRECT_FAILURES=

SPARQL_RESULT_FILES= \
ex2-1a-result.n3 ex2-2a-result.n3 ex2-3a-result.n3 ex2-4a-result.n3	\
ex3-result.n3 ex11.2.3.1_0-result.n3 ex11.2.3.1_1-result.n3		\
ex11.2.3.2_0-result.n3 ex11.2.3.2_1-result.n3 ex11.2.3.3_0-result.n3	\
ex11.2.3.4_0-result.n3 ex11.2.3.5_0-result.n3 ex11.2.3.6_0-result.n3	\
ex11.2.3.7_0-result.n3 ex11.2.3.8_0-result.n3 ex11_0-result.n3		\
ex11_1-result.n3


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

dist-hook:
	$(PYTHON3) $(srcdir)/../package_sparql_tests.py \
		--manifest-file $(srcdir)/$(SPARQL_MANIFEST_FILES) \
		--srcdir $(srcdir) \
		--distdir $(distdir)

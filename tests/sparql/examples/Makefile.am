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
	@$(PERL) $(srcdir)/../../improve .

get-testsuites-list:
	@echo "sparql-lexer sparql-parser sparql-query"

get-testsuite-sparql-lexer get-testsuite-sparql-parser:
	@tests="$(SPARQL_TEST_FILES)"; \
	type=`echo $@ | sed -e 's/^.*sparql-//' -e 's/er$$//'`; \
	verb=`echo $$type`ing; \
	prog=sparql_`echo $$type`er_test; \
	$(RECHO) '@prefix rdfs:	<http://www.w3.org/2000/01/rdf-schema#> .'; \
	$(RECHO) '@prefix mf:     <http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#> .'; \
	$(RECHO) '@prefix t:     <http://ns.librdf.org/2009/test-manifest#> .'; \
	$(RECHO) ' '; \
	$(RECHO) "<> a mf:Manifest; rdfs:comment \"SPARQL legal $$verb\"; mf:entries ("; \
	for test in $$tests; do \
	  comment="sparql $$type of $$test"; \
	  $(RECHO) "  [ a t:PositiveTest; mf:name \"$$test\"; rdfs:comment \"$$comment\"; mf:action  \"$(top_builddir)/src/$$prog $(srcdir)/$$test\" ]"; \
	done; \
	$(RECHO) ")."

get-testsuite-sparql-query:
	@tests="$(SPARQL_TEST_NAMES)"; \
	$(RECHO) '@prefix rdfs:	<http://www.w3.org/2000/01/rdf-schema#> .'; \
	$(RECHO) '@prefix mf:     <http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#> .'; \
	$(RECHO) '@prefix t:     <http://ns.librdf.org/2009/test-manifest#> .'; \
	$(RECHO) ' '; \
	$(RECHO) "<> a mf:Manifest; rdfs:comment \"SPARQL Query Execution\"; mf:entries ("; \
	for test in $(EXPECTED_SPARQL_CORRECT); do \
	  comment="sparql query $$test"; \
	  expect="PositiveTest"; \
	  t=`echo $$test`; \
	  $(RECHO) "  [ a t:$$expect; mf:name \"$$test\"; rdfs:comment \"$$comment\"; mf:action  \"$(PERL) $(srcdir)/../check-sparql -s $(srcdir) $$t\" ]"; \
	done; \
	for test in $(EXPECTED_SPARQL_CORRECT_FAILURES); do \
	  comment="sparql query $$test"; \
	  expect="NegativeTest"; \
	  t=`echo $$test`; \
	  $(RECHO) "  [ a t:$$expect; mf:name \"$$test\"; rdfs:comment \"$$comment\"; mf:action  \"$(PERL) $(srcdir)/../check-sparql -s $(srcdir) $$t\" ]"; \
	done; \
	$(RECHO) ")."

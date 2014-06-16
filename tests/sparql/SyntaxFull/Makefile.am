# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal SyntaxFull SPARQL tests
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

SPARQL_MANIFEST_FILES= manifest.ttl

SPARQL_MODEL_FILES= empty.nt

SPARQL_TEST_FILES_CORE= \
syntax-basic-01.rq syntax-basic-02.rq syntax-basic-03.rq		\
syntax-basic-04.rq syntax-basic-05.rq syntax-basic-06.rq		\
syntax-bnodes-01.rq syntax-bnodes-02.rq \
syntax-bnodes-05.rq syntax-bnodes-06.rq		\
syntax-bnodes-07.rq							\
syntax-expr-01.rq syntax-expr-02.rq syntax-expr-03.rq			\
syntax-expr-04.rq syntax-expr-05.rq syntax-expr-06.rq			\
syntax-forms-01.rq syntax-forms-02.rq					\
syntax-limit-offset-07.rq syntax-limit-offset-07.rq			\
syntax-limit-offset-09.rq						\
syntax-lists-01.rq syntax-lists-02.rq syntax-lists-03.rq		\
syntax-lists-04.rq syntax-lists-05.rq					\
syntax-lit-01.rq syntax-lit-02.rq syntax-lit-03.rq syntax-lit-04.rq	\
syntax-lit-05.rq syntax-lit-06.rq syntax-lit-07.rq syntax-lit-08.rq	\
syntax-lit-09.rq syntax-lit-10.rq syntax-lit-11.rq syntax-lit-12.rq	\
syntax-lit-13.rq syntax-lit-14.rq					\
syntax-order-01.rq syntax-order-02.rq syntax-order-03.rq		\
syntax-order-04.rq syntax-order-05.rq syntax-order-06.rq		\
syntax-pat-01.rq syntax-pat-02.rq syntax-pat-03.rq syntax-pat-04.rq	\
syntax-qname-07.rq syntax-qname-08.rq syntax-qname-09.rq		\
syntax-qname-10.rq syntax-qname-11.rq syntax-qname-12.rq		\
syntax-qname-13.rq syntax-qname-14.rq					\
syntax-struct-01.rq syntax-struct-02.rq					\
syntax-struct-03.rq syntax-struct-04.rq syntax-struct-05.rq		\
syntax-struct-06.rq syntax-struct-07.rq syntax-struct-08.rq		\
syntax-struct-09.rq syntax-struct-10.rq					\
syntax-union-01.rq syntax-union-02.rq

SPARQL_TEST_FILES=$(SPARQL_TEST_FILES_CORE) \
syntax-keywords-01.rq syntax-keywords-02.rq syntax-keywords-03.rq \
syntax-qname-01.rq syntax-qname-02.rq syntax-qname-03.rq \
syntax-qname-04.rq syntax-qname-05.rq syntax-qname-06.rq \
syntax-struct-11.rq syntax-struct-12.rq

SPARQL_TEST_NAMES=$(SPARQL_TEST_FILES_CORE) \
  "keyword FILTER as a namespace prefix" \
  "keyword FILTER as a local name" \
  "keyword UNION as a namespace prefix"

EXPECTED_SPARQL_PARSER_FAILURES=

EXPECTED_SPARQL_PARSER_FAILURES_COUNT= 0

EXPECTED_SPARQL_CORRECT_FAILURES= \
$(EXPECTED_SPARQL_PARSER_FAILURES)

EXPECTED_SPARQL_CORRECT_FAILURES_COUNT = \
$(EXPECTED_SPARQL_PARSER_FAILURES_COUNT)

EXTRA_DIST= \
$(SPARQL_MANIFEST_FILES) \
$(SPARQL_MODEL_FILES) \
$(SPARQL_MANIFEST_FILES) \
$(SPARQL_TEST_FILES)

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
	  expect="PositiveTest"; \
	  $(RECHO) "  [ a t:$$expect; mf:name \"$$test\"; rdfs:comment \"$$comment\"; mf:action  \"$(top_builddir)/src/$$prog $(srcdir)/$$test\" ]"; \
	done; \
	$(RECHO) ")."

get-testsuite-sparql-query:
	@$(RECHO) '@prefix rdfs:	<http://www.w3.org/2000/01/rdf-schema#> .'; \
	$(RECHO) '@prefix mf:     <http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#> .'; \
	$(RECHO) '@prefix t:     <http://ns.librdf.org/2009/test-manifest#> .'; \
	$(RECHO) ' '; \
	$(RECHO) "<> a mf:Manifest; rdfs:comment \"SPARQL Query Execution\"; mf:entries ("; \
	for test in $(SPARQL_TEST_NAMES); do \
	  comment="sparql query $$test"; \
	  expect="PositiveTest"; \
	  $(RECHO) "  [ a t:$$expect; mf:name \"$$test\"; rdfs:comment \"$$comment\"; mf:action  \"$(PERL) $(srcdir)/../check-sparql -s $(srcdir) '$$test'\" ]"; \
	done; \
	$(RECHO) ")."

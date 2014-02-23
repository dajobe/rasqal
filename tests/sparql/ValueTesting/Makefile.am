# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal SPARQL tests
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
boolean-0.n3 \
extendedType-0.n3 \
typePromotion-0.n3

SPARQL_TEST_FILES= \
boolean-EBV-canonical.rq \
boolean-equiv-FALSE.rq \
boolean-equiv-TRUE.rq \
boolean-equiv-xsdType.rq \
boolean-false-canonical.rq \
boolean-logical-OR.rq \
boolean-true-canonical.rq \
extendedType-eq-pass.rq \
extendedType-graph.rq \
extendedType-literal-eq.rq \
extendedType-literal-ne.rq \
extendedType-ne-fail.rq \
typePromotion-decimal-decimal-fail.rq \
typePromotion-decimal-decimal-pass.rq

EXPECTED_SPARQL_CORRECT= \
   "extendedType-eq-pass" \
   "extendedType-ne-fail" \
   "extendedType-literal-eq" \
   "extendedType-literal-ne" \
   "extendedType-graph" \
   "typePromotion-decimal-decimal-fail" \
   "boolean-equiv-FALSE" \
   "boolean-equiv-TRUE" \
   "boolean-equiv-xsdType" \
   "boolean-logical-OR"

# Test failures
EXPECTED_SPARQL_CORRECT_FAILURES= \
  typePromotion-decimal-decimal-pass \
  boolean-false-canonical \
  boolean-true-canonical \
  boolean-EBV-canonical

SPARQL_RESULT_FILES= \
boolean-EBV-canonical-result.n3 \
boolean-equiv-FALSE-result.n3 \
boolean-equiv-TRUE-result.n3 \
boolean-equiv-xsdType-result.n3 \
boolean-false-canonical-result.n3 \
boolean-logical-OR-result.n3 \
boolean-true-canonical-result.n3 \
extendedType-eq-pass-result.n3 \
extendedType-graph-result.n3 \
extendedType-literal-eq-result.n3 \
extendedType-literal-ne-result.n3 \
extendedType-ne-fail-result.n3 \
typePromotion-decimal-decimal-fail-result.n3 \
typePromotion-decimal-decimal-pass-result.n3

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
	@type=`echo $@ | sed -e 's/^.*sparql-//' -e 's/er$$//'`; \
	verb=`echo $$type`ing; \
	prog=sparql_`echo $$type`er_test; \
	$(RECHO) '@prefix rdfs:	<http://www.w3.org/2000/01/rdf-schema#> .'; \
	$(RECHO) '@prefix mf:     <http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#> .'; \
	$(RECHO) '@prefix t:     <http://ns.librdf.org/2009/test-manifest#> .'; \
	$(RECHO) ' '; \
	$(RECHO) "<> a mf:Manifest; rdfs:comment \"SPARQL legal $$verb\"; mf:entries ("; \
	for test in $(SPARQL_TEST_FILES); do \
	  comment="sparql $$type of $$test"; \
	  $(RECHO) "  [ a t:PositiveTest; mf:name \"$$test\"; rdfs:comment \"$$comment\"; mf:action  \"$(top_builddir)/src/$$prog $(srcdir)/$$test\" ]"; \
	done; \
	$(RECHO) ")."

get-testsuite-sparql-query:
	@$(RECHO) '@prefix rdfs:	<http://www.w3.org/2000/01/rdf-schema#> .'; \
	$(RECHO) '@prefix mf:     <http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#> .'; \
	$(RECHO) '@prefix t:     <http://ns.librdf.org/2009/test-manifest#> .'; \
	$(RECHO) ' '; \
	$(RECHO) "<> a mf:Manifest; rdfs:comment \"SPARQL Query Execution\"; mf:entries ("; \
	for test in $(EXPECTED_SPARQL_CORRECT); do \
	  comment="sparql query $$test"; \
	  $(RECHO) "  [ a t:PositiveTest; mf:name \"$$test\"; rdfs:comment \"$$comment\"; mf:action  \"$(PERL) $(srcdir)/../check-sparql -s $(srcdir) '$$test'\" ]"; \
	done; \
	for test in $(EXPECTED_SPARQL_CORRECT_FAILURES); do \
	  comment="sparql query $$test"; \
	  $(RECHO) "  [ a t:NegativeTest; mf:name \"$$test\"; rdfs:comment \"$$comment\"; mf:action  \"$(PERL) $(srcdir)/../check-sparql -s $(srcdir) '$$test'\" ]"; \
	done; \
	$(RECHO) ")."

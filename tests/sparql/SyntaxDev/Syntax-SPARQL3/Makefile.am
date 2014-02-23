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

SPARQL_MANIFEST_FILES= manifest.ttl

SPARQL_TEST_FILES= \
syn-01.rq syn-02.rq syn-03.rq syn-04.rq syn-05.rq syn-06.rq syn-07.rq	\
syn-08.rq

SPARQL_TEST_BAD_FILES= \
syn-bad-01.rq syn-bad-02.rq syn-bad-03.rq syn-bad-04.rq syn-bad-05.rq	\
syn-bad-06.rq syn-bad-07.rq syn-bad-08.rq syn-bad-09.rq syn-bad-10.rq	\
bnode-dot.rq bnodes-missing-pvalues-01.rq				\
bnodes-missing-pvalues-02.rq empty-optional-01.rq			\
empty-optional-02.rq filter-missing-parens.rq lone-list.rq		\
lone-node.rq

EXPECTED_SPARQL_CORRECT= \
  "syn-01.rq" \
  "syn-02.rq" \
  "syn-03.rq" \
  "syn-04.rq" \
  "syn-05.rq" \
  "syn-06.rq" \
  "syn-07.rq" \
  "syn-08.rq" \
  "syn-bad-01.rq" \
  "syn-bad-02.rq" \
  "syn-bad-03.rq" \
  "syn-bad-04.rq" \
  "syn-bad-05.rq" \
  "syn-bad-06.rq" \
  "syn-bad-07.rq" \
  "syn-bad-08.rq" \
  "syn-bad-09.rq" \
  "syn-bad-10.rq" \
  "bnode-dot.rq" \
  "bnodes-missing-pvalues-01.rq" \
  "bnodes-missing-pvalues-02.rq" \
  "empty-optional-01.rq" \
  "empty-optional-02.rq" \
  "filter-missing-parens.rq" \
  "lone-list.rq" \
  "lone-node.rq"

EXTRA_DIST= \
$(SPARQL_MANIFEST_FILES) \
$(SPARQL_TEST_FILES) \
$(SPARQL_TEST_BAD_FILES)

CLEANFILES=diff.out roqet.err roqet.out roqet.tmp result.out

build-roqet:
	@(cd $(top_builddir)/utils ; $(MAKE) roqet)

build-sparql-lexer-test:
	@(cd $(top_builddir)/src ; $(MAKE) sparql_lexer_test)

build-sparql-parser-test:
	@(cd $(top_builddir)/src ; $(MAKE) sparql_parser_test)

check-local: build-roqet build-sparql-lexer-test build-sparql-parser-test
	@$(PERL) $(srcdir)/../../../improve .

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
	  $(RECHO) "  [ a t:PositiveTest; mf:name \"$$test\"; rdfs:comment \"$$comment\"; mf:action  \"$(PERL) $(srcdir)/../../check-sparql -s $(srcdir) '$$test'\" ]"; \
	done; \
	$(RECHO) ")."

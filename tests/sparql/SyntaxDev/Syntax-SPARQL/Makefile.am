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
syntax-basic-01.rq syntax-basic-02.rq syntax-basic-03.rq		\
syntax-basic-04.rq syntax-basic-05.rq syntax-basic-06.rq		\
syntax-bnodes-01.rq syntax-bnodes-02.rq		\
syntax-bnodes-05.rq syntax-bnodes-06.rq		\
syntax-bnodes-07.rq syntax-expr-01.rq syntax-expr-02.rq			\
syntax-expr-03.rq syntax-expr-04.rq syntax-expr-05.rq			\
syntax-forms-01.rq syntax-forms-02.rq syntax-limit-offset-07.rq		\
syntax-limit-offset-08.rq syntax-limit-offset-09.rq			\
syntax-lists-01.rq syntax-lists-02.rq syntax-lists-03.rq		\
syntax-lists-04.rq syntax-lists-05.rq syntax-lit-01.rq			\
syntax-lit-02.rq syntax-lit-03.rq syntax-lit-04.rq syntax-lit-05.rq	\
syntax-lit-06.rq syntax-lit-07.rq syntax-lit-08.rq syntax-lit-09.rq	\
syntax-lit-10.rq syntax-lit-11.rq syntax-lit-12.rq syntax-lit-13.rq	\
syntax-lit-14.rq syntax-order-01.rq syntax-order-02.rq			\
syntax-order-03.rq syntax-order-04.rq syntax-order-05.rq		\
syntax-order-06.rq syntax-pat-01.rq syntax-pat-02.rq syntax-pat-03.rq	\
syntax-pat-04.rq syntax-qname-01.rq syntax-qname-02.rq			\
syntax-qname-03.rq syntax-qname-04.rq syntax-qname-05.rq		\
syntax-qname-06.rq syntax-qname-07.rq syntax-qname-08.rq		\
syntax-struct-01.rq syntax-struct-02.rq syntax-struct-03.rq		\
syntax-struct-04.rq syntax-struct-05.rq syntax-struct-06.rq		\
syntax-struct-07.rq syntax-struct-08.rq syntax-struct-09.rq		\
syntax-struct-10.rq syntax-struct-11.rq syntax-struct-12.rq		\
syntax-union-01.rq syntax-union-02.rq

EXPECTED_SPARQL_CORRECT= \
  "syntax-basic-01.rq" \
  "syntax-basic-02.rq" \
  "syntax-basic-03.rq" \
  "syntax-basic-04.rq" \
  "syntax-basic-05.rq" \
  "syntax-basic-06.rq" \
  "syntax-qname-01.rq" \
  "syntax-qname-02.rq" \
  "syntax-qname-03.rq" \
  "syntax-qname-04.rq" \
  "syntax-qname-05.rq" \
  "syntax-qname-06.rq" \
  "syntax-qname-07.rq" \
  "syntax-qname-08.rq" \
  "syntax-lit-01.rq" \
  "syntax-lit-02.rq" \
  "syntax-lit-03.rq" \
  "syntax-lit-04.rq" \
  "syntax-lit-05.rq" \
  "syntax-lit-06.rq" \
  "syntax-lit-07.rq" \
  "syntax-lit-08.rq" \
  "syntax-lit-09.rq" \
  "syntax-lit-10.rq" \
  "syntax-lit-11.rq" \
  "syntax-lit-12.rq" \
  "syntax-lit-13.rq" \
  "syntax-lit-14.rq" \
  "syntax-struct-01.rq" \
  "syntax-struct-02.rq" \
  "syntax-struct-03.rq" \
  "syntax-struct-04.rq" \
  "syntax-struct-05.rq" \
  "syntax-struct-06.rq" \
  "syntax-struct-07.rq" \
  "syntax-struct-08.rq" \
  "syntax-struct-09.rq" \
  "syntax-struct-10.rq" \
  "syntax-struct-11.rq" \
  "syntax-struct-12.rq" \
  "syntax-lists-01.rq" \
  "syntax-lists-02.rq" \
  "syntax-lists-03.rq" \
  "syntax-lists-04.rq" \
  "syntax-lists-05.rq" \
  "syntax-bnodes-01.rq" \
  "syntax-bnodes-02.rq" \
  "syntax-bnodes-05.rq" \
  "syntax-bnodes-06.rq" \
  "syntax-bnodes-07.rq" \
  "syntax-forms-01.rq" \
  "syntax-forms-02.rq" \
  "syntax-union-01.rq" \
  "syntax-union-02.rq" \
  "syntax-expr-01.rq" \
  "syntax-expr-02.rq" \
  "syntax-expr-03.rq" \
  "syntax-expr-04.rq" \
  "syntax-expr-05.rq" \
  "syntax-order-01.rq" \
  "syntax-order-02.rq" \
  "syntax-order-03.rq" \
  "syntax-order-04.rq" \
  "syntax-order-05.rq" \
  "syntax-order-06.rq" \
  "syntax-limit-offset-07.rq" \
  "syntax-limit-offset-08.rq" \
  "syntax-limit-offset-09.rq" \
  "syntax-pat-01.rq" \
  "syntax-pat-02.rq" \
  "syntax-pat-03.rq" \
  "syntax-pat-04.rq"

EXTRA_DIST= \
$(SPARQL_MANIFEST_FILES) \
$(SPARQL_TEST_FILES)


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

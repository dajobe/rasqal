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

SPARQL_MANIFEST_FILES= manifest.n3

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

EXPECTED_SPARQL_CORRECT= \
  "Aggregate 1 - SUM with GROUP BY and HAVING" \
  "Aggregate 2 - SUM" \
  "Aggregate 3 - SAMPLE and GROUP BY" \
  "Group Concat 1 - Newline separator" \
  "Group Concat 2 - default separator" \
  "Group Concat 3 - HAVING" \
  "Group Concat 4 - DISTINCT"

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
	@$(PERL) $(srcdir)/../../improve .

get-testsuites-list:
	@echo "sparql-parse-good sparql-parse-bad sparql-query"

get-testsuite-sparql-parse-good:
	@prog=sparql_parser_test; \
	$(RECHO) '@prefix rdfs:	<http://www.w3.org/2000/01/rdf-schema#> .'; \
	$(RECHO) '@prefix mf:     <http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#> .'; \
	$(RECHO) '@prefix t:     <http://ns.librdf.org/2009/test-manifest#> .'; \
	$(RECHO) ' '; \
	$(RECHO) "<> a mf:Manifest; rdfs:comment \"SPARQL 1.1 Query aggregates legal parsing\"; mf:entries ("; \
	for test in $(SPARQL_TEST_FILES); do \
	  comment="sparql parsing of $$test"; \
	  $(RECHO) "  [ a t:PositiveTest; mf:name \"$$test\"; rdfs:comment \"$$comment\"; mf:action  \"$(top_builddir)/src/$$prog -i sparql11 $(srcdir)/$$test\" ]"; \
	done; \
	$(RECHO) ")."


get-testsuite-sparql-parse-bad:
	@prog=sparql_parser_test; \
	$(RECHO) '@prefix rdfs:	<http://www.w3.org/2000/01/rdf-schema#> .'; \
	$(RECHO) '@prefix mf:     <http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#> .'; \
	$(RECHO) '@prefix t:     <http://ns.librdf.org/2009/test-manifest#> .'; \
	$(RECHO) ' '; \
	$(RECHO) "<> a mf:Manifest; rdfs:comment \"SPARQL 1.1 Query aggregates illegal parsing\"; mf:entries ("; \
	for test in $(SPARQL_BAD_TEST_FILES); do \
	  comment="sparql failing to parse of $$test"; \
	  $(RECHO) "  [ a t:NegativeTest; mf:name \"$$test\"; rdfs:comment \"$$comment\"; mf:action  \"$(top_builddir)/src/$$prog -i sparql11 $(srcdir)/$$test\" ]"; \
	done; \
	$(RECHO) ")."

get-testsuite-sparql-query:
	@$(RECHO) '@prefix rdfs:	<http://www.w3.org/2000/01/rdf-schema#> .'; \
	$(RECHO) '@prefix mf:     <http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#> .'; \
	$(RECHO) '@prefix t:     <http://ns.librdf.org/2009/test-manifest#> .'; \
	$(RECHO) ' '; \
	$(RECHO) "<> a mf:Manifest; rdfs:comment \"SPARQL 1.1 Query Aggregates\"; mf:entries ("; \
	for test in $(EXPECTED_SPARQL_CORRECT); do \
	  comment="sparql query $$test"; \
	  $(RECHO) "  [ a t:PositiveTest; mf:name \"$$test\"; rdfs:comment \"$$comment\"; mf:action  \"$(PERL) $(srcdir)/../check-sparql -i sparql11 -s $(srcdir) '$$test'\" ]"; \
	done; \
	$(RECHO) ")."

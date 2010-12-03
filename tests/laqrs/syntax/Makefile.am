# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal syntax LAQRS tests
#
# Copyright (C) 2007-2010, David Beckett http://www.dajobe.org/
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

LAQRS_MODEL_FILES= \
data.n3

LAQRS_TEST_FILES= \
agg-function1.rq \
coalesce1.rq \
count1.rq \
count2.rq \
count3.rq \
count4.rq \
coverage.rq \
explain.rq \
function-distinct1.rq \
delete.rq \
in1.rq \
in2.rq \
insert.rq \
avg1.rq \
max1.rq \
min1.rq \
sum1.rq \
let1.rq \
let2.rq \
let3.rq \
let4.rq \
strdt1.rq \
strdt2.rq \
strlang1.rq \
bnode1.rq \
bnode2.rq \
uri1.rq \
iri1.rq \
if1.rq

LAQRS_BAD_TEST_FILES= \
bad-select-expr.rq

EXPECTED_LAQRS_CORRECT_FAILURES=

EXTRA_DIST= \
$(LAQRS_MODEL_FILES) \
$(LAQRS_TEST_FILES) \
$(LAQRS_BAD_TEST_FILES)

build-sparql-parser-test:
	@(cd $(top_builddir)/src ; $(MAKE) sparql_parser_test)

check-local: build-sparql-parser-test
	@$(PERL) $(srcdir)/../../improve .

get-testsuites-list:
	@echo "laqrs-parse-good laqrs-parse-bad"

get-testsuite-laqrs-parse-good:
	@prog=sparql_parser_test; \
	$(RECHO) '@prefix rdfs:	<http://www.w3.org/2000/01/rdf-schema#> .'; \
	$(RECHO) '@prefix mf:     <http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#> .'; \
	$(RECHO) '@prefix t:     <http://ns.librdf.org/2009/test-manifest#> .'; \
	$(RECHO) ' '; \
	$(RECHO) "<> a mf:Manifest; rdfs:comment \"LAQRS legal parsing\"; mf:entries ("; \
	for test in $(LAQRS_TEST_FILES); do \
	  comment="laqrs parsing of $$test"; \
	  $(RECHO) "  [ a t:PositiveTest; mf:name \"$$test\"; rdfs:comment \"$$comment\"; mf:action  \"$(top_builddir)/src/$$prog -i laqrs $(srcdir)/$$test\" ]"; \
	done; \
	$(RECHO) ")."


get-testsuite-laqrs-parse-bad:
	@prog=sparql_parser_test; \
	$(RECHO) '@prefix rdfs:	<http://www.w3.org/2000/01/rdf-schema#> .'; \
	$(RECHO) '@prefix mf:     <http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#> .'; \
	$(RECHO) '@prefix t:     <http://ns.librdf.org/2009/test-manifest#> .'; \
	$(RECHO) ' '; \
	$(RECHO) "<> a mf:Manifest; rdfs:comment \"LAQRS illegal parsing\"; mf:entries ("; \
	for test in $(LAQRS_BAD_TEST_FILES); do \
	  comment="laqrs failing to parse of $$test"; \
	  $(RECHO) "  [ a t:NegativeTest; mf:name \"$$test\"; rdfs:comment \"$$comment\"; mf:action  \"$(top_builddir)/src/$$prog -i laqrs $(srcdir)/$$test\" ]"; \
	done; \
	$(RECHO) ")."


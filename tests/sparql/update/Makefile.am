# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal SPARQL Update tests
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

SPARQL_MANIFEST_FILES= manifest.ttl

SPARQL_MODEL_FILES= \
data.ttl

EXPECTED_SPARQL_CORRECT= \
insert-data-1.rq \
insert-data-2.rq \
insert-data-3.rq \
insert-data-4.rq \
delete-data-1.rq \
delete-data-2.rq \
delete-1.rq \
delete-2.rq \
insert-1.rq \
insert-2.rq \
insert-3.rq \
load-1.rq \
load-2.rq \
load-3.rq \
load-4.rq \
clear-1.rq \
clear-2.rq \
clear-3.rq \
clear-4.rq \
create-1.rq \
create-2.rq \
delete-1.rq \
drop-1.rq \
drop-2.rq \
delete-insert-1.rq \
copy-1.rq \
copy-2.rq \
copy-3.rq \
move-1.rq \
move-2.rq \
move-3.rq \
add-1.rq \
add-2.rq \
add-3.rq \
multiple-1.rq \
delete-where-1.rq \
delete-where-2.rq



SPARQL_BAD_TEST_FILES= \
bad-1.rq

EXPECTED_SPARQL_CORRECT_FAILURES=

EXTRA_DIST= \
$(SPARQL_MANIFEST_FILES) \
$(SPARQL_MODEL_FILES) \
$(EXPECTED_SPARQL_CORRECT) \
$(SPARQL_BAD_TEST_FILES)

build-sparql-parser-test:
	@(cd $(top_builddir)/src ; $(MAKE) sparql_parser_test)

check-local: build-sparql-parser-test
	@$(PERL) $(srcdir)/../../improve .

get-testsuites-list:
	@echo "sparql-parse-good sparql-parse-bad"

get-testsuite-sparql-parse-good:
	@prog=sparql_parser_test; \
	$(RECHO) '@prefix rdfs:	<http://www.w3.org/2000/01/rdf-schema#> .'; \
	$(RECHO) '@prefix mf:     <http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#> .'; \
	$(RECHO) '@prefix t:     <http://ns.librdf.org/2009/test-manifest#> .'; \
	$(RECHO) ' '; \
	$(RECHO) "<> a mf:Manifest; rdfs:comment \"SPARQL 1.1 Update legal parsing\"; mf:entries ("; \
	for test in $(EXPECTED_SPARQL_CORRECT); do \
	  comment="sparql parsing of $$test"; \
	  $(RECHO) "  [ a t:PositiveTest; mf:name \"$$test\"; rdfs:comment \"$$comment\"; mf:action  \"$(top_builddir)/src/$$prog -i sparql11 $(srcdir)/$$test\" ]"; \
	done; \
	for test in $(EXPECTED_SPARQL_CORRECT_FAILURES); do \
	  comment="sparql parsing of $$test"; \
	  $(RECHO) "  [ a t:NegativeTest; mf:name \"$$test\"; rdfs:comment \"$$comment\"; mf:action  \"$(top_builddir)/src/$$prog -i sparql11 $(srcdir)/$$test\" ]"; \
	done; \
	$(RECHO) ")."


get-testsuite-sparql-parse-bad:
	@prog=sparql_parser_test; \
	$(RECHO) '@prefix rdfs:	<http://www.w3.org/2000/01/rdf-schema#> .'; \
	$(RECHO) '@prefix mf:     <http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#> .'; \
	$(RECHO) '@prefix t:     <http://ns.librdf.org/2009/test-manifest#> .'; \
	$(RECHO) ' '; \
	$(RECHO) "<> a mf:Manifest; rdfs:comment \"SPARQL 1.1 Update illegal parsing\"; mf:entries ("; \
	for test in $(SPARQL_BAD_TEST_FILES); do \
	  comment="sparql failing to parse of $$test"; \
	  $(RECHO) "  [ a t:NegativeTest; mf:name \"$$test\"; rdfs:comment \"$$comment\"; mf:action  \"$(top_builddir)/src/$$prog -i sparql11 $(srcdir)/$$test\" ]"; \
	done; \
	$(RECHO) ")."


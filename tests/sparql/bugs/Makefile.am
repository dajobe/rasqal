# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal bug tests
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

MANIFEST_FILES= \
manifest.ttl

BUG_DATA_FILES= \
352.ttl 354.nt

SPARQL_TEST_FILES= \
352.rq 354.rq

SPARQL_TEST_NAMES= \
352 354

SPARQL_RESULT_FILES= \
352-result.ttl \
354-result.ttl

EXTRA_DIST= \
$(MANIFEST_FILES) \
$(BUG_DATA_FILES) \
$(SPARQL_TEST_FILES) \
$(SPARQL_RESULT_FILES)

CLEANFILES=diff.out roqet.err roqet.out roqet.tmp result.out

build-roqet:
	@(cd $(top_builddir)/utils ; $(MAKE) roqet)

check-local: build-roqet
	@$(PERL) $(srcdir)/../../improve .

get-testsuites-list:
	@echo "sparql-query"

get-testsuite-sparql-query:
	@tests="$(SPARQL_TEST_NAMES)"; \
	$(ECHO) '@prefix rdfs:	<http://www.w3.org/2000/01/rdf-schema#> .'; \
	$(ECHO) '@prefix mf:     <http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#> .'; \
	$(ECHO) '@prefix t:     <http://ns.librdf.org/2009/test-manifest#> .'; \
	$(ECHO) ' '; \
	$(ECHO) "<> a mf:Manifest; rdfs:comment \"Bug Tests via SPARQL\"; t:path \"$(top_builddir)/utils:$(top_builddir)@RAPPER_PATH@:$$PATH\"; t:precondition \"$(PERL) -MXML::DOM -e 'exit 0'\"; mf:entries ("; \
	for test in $$tests; do \
	  comment="sparql query $$test"; \
	  expect="PositiveTest"; \
	  t=`echo $$test`; \
	  $(ECHO) "  [ a t:$$expect; mf:name \"$$test\"; rdfs:comment \"$$comment\"; mf:action  \"$(PERL) $(srcdir)/../check-sparql -s $(srcdir) $$t\" ]"; \
	done; \
	$(ECHO) ")."

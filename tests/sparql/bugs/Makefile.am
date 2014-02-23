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
352.ttl 353.ttl 354.nt 459.ttl 519.ttl

SPARQL_TEST_FILES= \
352.rq 353.rq 354.rq 459.rq 519.rq

SPARQL_TEST_NAMES= \
352 353 354 459 519

SPARQL_RESULT_FILES= \
352-result.ttl \
353-result.ttl \
354-result.ttl \
459-result.ttl \
519-result.ttl

EXTRA_DIST= \
$(MANIFEST_FILES) \
$(BUG_DATA_FILES) \
$(SPARQL_TEST_FILES) \
$(SPARQL_RESULT_FILES)

CLEANFILES=diff.out roqet.err roqet.out roqet.tmp result.out

RECHO=/bin/echo

.PHONY: build-core build-roqet check-lcoal

# This sequence allows parallel building of build-core and then once
# that is built, parallel building of the dependent programs: roqet

build-core:
	@(cd $(top_builddir)/src ; $(MAKE))

build-roqet: build-core
	@(cd $(top_builddir)/utils ; $(MAKE) roqet)

check-local: build-roqet
	@$(PERL) $(srcdir)/../../improve .

get-testsuites-list:
	@echo "sparql-query"

get-testsuite-sparql-query:
	@tests="$(SPARQL_TEST_NAMES)"; \
	$(RECHO) '@prefix rdfs:	<http://www.w3.org/2000/01/rdf-schema#> .'; \
	$(RECHO) '@prefix mf:     <http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#> .'; \
	$(RECHO) '@prefix t:     <http://ns.librdf.org/2009/test-manifest#> .'; \
	$(RECHO) ' '; \
	$(RECHO) "<> a mf:Manifest; rdfs:comment \"Bug Tests via SPARQL\"; mf:entries ("; \
	for test in $$tests; do \
	  comment="sparql query $$test"; \
	  expect="PositiveTest"; \
	  t=`echo $$test`; \
	  $(RECHO) "  [ a t:$$expect; mf:name \"$$test\"; rdfs:comment \"$$comment\"; mf:action  \"$(PERL) $(srcdir)/../check-sparql -s $(srcdir) $$t\" ]"; \
	done; \
	$(RECHO) ")."

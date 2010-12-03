# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal query engine tests
#
# Copyright (C) 2004-2008, David Beckett http://www.dajobe.org/
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

local_tests=rasqal_order_test$(EXEEXT) rasqal_graph_test$(EXEEXT) \
rasqal_construct_test$(EXEEXT) rasqal_limit_test$(EXEEXT) \
rasqal_triples_test$(EXEEXT)

EXTRA_PROGRAMS=$(local_tests)

AM_CPPFLAGS=@RASQAL_INTERNAL_CPPFLAGS@ -I$(top_srcdir)/src
AM_CFLAGS=@RASQAL_INTERNAL_CPPFLAGS@ $(MEM)
AM_LDFLAGS=@RASQAL_INTERNAL_LIBS@ @RASQAL_EXTERNAL_LIBS@ $(MEM_LIBS)

CLEANFILES=$(local_tests)

rasqal_order_test_SOURCES = rasqal_order_test.c
rasqal_order_test_LDADD = $(top_builddir)/src/librasqal.la

rasqal_graph_test_SOURCES = rasqal_graph_test.c
rasqal_graph_test_LDADD = $(top_builddir)/src/librasqal.la

rasqal_construct_test_SOURCES = rasqal_construct_test.c
rasqal_construct_test_LDADD = $(top_builddir)/src/librasqal.la

rasqal_limit_test_SOURCES = rasqal_limit_test.c
rasqal_limit_test_LDADD = $(top_builddir)/src/librasqal.la

rasqal_triples_test_SOURCES = rasqal_triples_test.c
rasqal_triples_test_LDADD = $(top_builddir)/src/librasqal.la


# These are compiled here and used elsewhere for running tests
check-local: $(local_tests) run-rasqal-tests

run-rasqal-tests: $(local_tests)
	@$(PERL) $(srcdir)/../improve .

$(top_builddir)/src/librasqal.la:
	cd $(top_builddir)/src && $(MAKE) librasqal.la

$(top_builddir)/../raptor/src/libraptor.la:
	cd $(top_builddir)/../raptor/src && $(MAKE) $(AM_MAKEFLAGS) libraptor.la

get-testsuites-list:
	@echo "engine"

get-testsuite-engine:
	@tests="$(local_tests)"; \
	$(RECHO) '@prefix rdfs:	<http://www.w3.org/2000/01/rdf-schema#> .'; \
	$(RECHO) '@prefix mf:     <http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#> .'; \
	$(RECHO) '@prefix t:     <http://ns.librdf.org/2009/test-manifest#> .'; \
	$(RECHO) ' '; \
	$(RECHO) "<> a mf:Manifest; rdfs:comment \"Query Engine Tests\"; mf:entries ("; \
	for test in $$tests; do \
	  comment="rdql query $$test"; \
	  expect="PositiveTest"; \
	  arg="$(top_srcdir)/data/"; \
	  if [ $$test = rasqal_limit_test$(EXEEXT) ]; then \
	    arg="$$arg/letters.nt"; \
          fi; \
	  $(RECHO) "  [ a t:$$expect; mf:name \"$$test\"; rdfs:comment \"$$comment\"; mf:action  \"./$$test $$arg\" ]"; \
	done; \
	$(RECHO) ")."

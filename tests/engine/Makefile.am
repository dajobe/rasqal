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
	@$(PYTHON) $(srcdir)/../improve.py .

$(top_builddir)/src/librasqal.la:
	cd $(top_builddir)/src && $(MAKE) librasqal.la

$(top_builddir)/../raptor/src/libraptor.la:
	cd $(top_builddir)/../raptor/src && $(MAKE) $(AM_MAKEFLAGS) libraptor.la

get-testsuites-list:
	@echo "engine"

get-testsuite-engine:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(srcdir)/../sparql/generate_sparql_plan.py \
		--suite-name engine \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type PositiveTest \
		--action-template "./{test_id} $(top_srcdir)/data/" \
		--comment-template "engine test {test_id}" \
		rasqal_order_test rasqal_graph_test rasqal_construct_test rasqal_triples_test

dist-hook:
	$(PYTHON3) $(srcdir)/../sparql/package_sparql_tests.py \
		--manifest-file $(srcdir)/manifest.ttl \
		--srcdir $(srcdir) \
		--distdir $(distdir)

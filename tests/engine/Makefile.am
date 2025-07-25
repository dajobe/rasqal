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

MANIFEST_FILE = manifest.ttl

EXTRA_PROGRAMS=$(local_tests)

EXTRA_DIST = $(MANIFEST_FILE) manifest-limit.ttl

AM_CPPFLAGS=@RASQAL_INTERNAL_CPPFLAGS@ -I$(top_srcdir)/src
AM_CFLAGS=@RASQAL_INTERNAL_CPPFLAGS@ $(MEM)
AM_LDFLAGS=@RASQAL_INTERNAL_LIBS@ @RASQAL_EXTERNAL_LIBS@ $(MEM_LIBS)

CLEANFILES=$(local_tests) \
	engine-plan.ttl engine-limit-plan.ttl

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
	@$(PYTHON) $(top_srcdir)/tests/bin/run-test-suites .

$(top_builddir)/src/librasqal.la:
	cd $(top_builddir)/src && $(MAKE) librasqal.la

$(top_builddir)/../raptor/src/libraptor.la:
	cd $(top_builddir)/../raptor/src && $(MAKE) $(AM_MAKEFLAGS) libraptor.la

get-testsuites-list:
	@echo "engine engine-limit"

get-testsuite-engine:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(top_srcdir)/tests/bin/create-test-plan \
		--suite-name engine \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-category engine \
		--test-type PositiveTest \
		--action-template "{builddir}/tests/engine/{test_id} {abs_srcdir}/../../data/" \
		--comment-template "engine test {test_id}" \
		--manifest-file $(top_srcdir)/tests/engine/$(MANIFEST_FILE)

get-testsuite-engine-limit:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(top_srcdir)/tests/bin/create-test-plan \
		--suite-name engine-limit \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-category engine \
		--test-type PositiveTest \
		--action-template "{builddir}/tests/engine/{test_id} {abs_srcdir}/../../data/{data_file}" \
		--test-data-file-map "rasqal_limit_test:letters.nt" \
		--comment-template "engine limit test {test_id}" \
		--manifest-file $(top_srcdir)/tests/engine/manifest-limit.ttl

dist-hook:
	$(PYTHON3) $(top_srcdir)/tests/bin/bundle-test-files \
		--manifest-file $(abs_srcdir)/$(MANIFEST_FILE) \
		--manifest-file $(abs_srcdir)/manifest-limit.ttl \
		--srcdir $(srcdir) \
		--distdir $(distdir)

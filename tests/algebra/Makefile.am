# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal algebra tests
#
# Copyright (C) 2008, David Beckett http://www.dajobe.org/
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

local_tests=convert_graph_pattern$(EXEEXT)

ALGEBRA_TEST_FILES=test-01.rq test-02.rq test-03.rq test-04.rq test-05.rq \
test-06.rq test-07.rq
ALGEBRA_RESULT_FILES=$(ALGEBRA_TEST_FILES:.rq=.out)

EXTRA_DIST= $(ALGEBRA_TEST_FILES) $(ALGEBRA_RESULT_FILES)

EXTRA_PROGRAMS=$(local_tests)

AM_CPPFLAGS=@RASQAL_INTERNAL_CPPFLAGS@ -I$(top_srcdir)/src
AM_CFLAGS=@RASQAL_INTERNAL_CPPFLAGS@ $(MEM)
AM_LDFLAGS=@RASQAL_INTERNAL_LIBS@ @RASQAL_EXTERNAL_LIBS@ $(MEM_LIBS)

CLEANFILES=$(local_tests)

convert_graph_pattern_SOURCES = convert_graph_pattern.c
convert_graph_pattern_LDADD = $(top_builddir)/src/librasqal.la

BASE_URI=http://www.w3.org/TR/2008/REC-rdf-sparql-query-20080115/#

# These are compiled here and used elsewhere for running tests
check-local: run-rasqal-tests

run-rasqal-tests: convert_graph_pattern $(ALGEBRA_TEST_FILES)
	@status=0; \
	out="convert.out"; \
	for tst in $(ALGEBRA_TEST_FILES); do \
	  baseuri=$(BASE_URI)$$tst; \
	  base=`echo $$tst | sed -e 's/\.rq//'`; \
	  result="$$base.out"; \
	  if test -f ./$$tst; then dir=./; \
	  elif test -f $$tst; then dir=; \
	  else dir="$(top_srcdir)/"; fi; \
	  ./convert_graph_pattern $${dir}$$tst $$baseuri > $$out; \
	  if cmp $${dir}$$result $$out >/dev/null 2>&1; then \
	    echo "PASS: $$tst"; \
	  else \
	    echo "FAIL: $$tst"; \
	    echo "./convert_graph_pattern $${dir}$$tst $$baseuri > $$out"; \
	    diff -u $${dir}$$result $$out; \
	    status=1; \
	    break; \
	  fi; \
	done; \
	rm -f $$out; \
	exit $$status


$(top_builddir)/src/librasqal.la:
	cd $(top_builddir)/src && $(MAKE) librasqal.la

$(top_builddir)/../raptor/src/libraptor.la:
	cd $(top_builddir)/../raptor/src && $(MAKE) $(AM_MAKEFLAGS) libraptor.la

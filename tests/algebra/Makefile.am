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

ALGEBRA_MANIFEST_FILE= manifest.ttl

local_tests=convert_graph_pattern$(EXEEXT)

EXTRA_DIST= $(ALGEBRA_MANIFEST_FILE)

EXTRA_PROGRAMS=$(local_tests)

AM_CPPFLAGS=@RASQAL_INTERNAL_CPPFLAGS@ -I$(top_srcdir)/src
AM_CFLAGS=@RASQAL_INTERNAL_CPPFLAGS@ $(MEM)
AM_LDFLAGS=@RASQAL_INTERNAL_LIBS@ @RASQAL_EXTERNAL_LIBS@ $(MEM_LIBS)

CLEANFILES=$(local_tests) \
convert.out convert.err diff.out \
sparql-algebra-plan.ttl

convert_graph_pattern_SOURCES = convert_graph_pattern.c
convert_graph_pattern_LDADD = $(top_builddir)/src/librasqal.la

BASE_URI=http://www.w3.org/TR/2008/REC-rdf-sparql-query-20080115/#

# These are compiled here and used elsewhere for running tests
check-local: convert_graph_pattern$(EXEEXT) $(ALGEBRA_TEST_FILES)
	@$(top_srcdir)/tests/bin/run-test-suites .

get-testsuites-list:
	@echo "sparql-algebra"

get-testsuite-sparql-algebra:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(top_srcdir)/tests/bin/create-test-plan \
		--suite-name sparql-algebra \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-category generic \
		--test-type PositiveTest \
		--action-template "$(PYTHON3) $(top_srcdir)/tests/bin/test-algebra {srcdir}/{query_file} $(BASE_URI)" \
		--comment-template "SPARQL algebra {query_file}" \
		--manifest-file $(top_srcdir)/tests/algebra/$(ALGEBRA_MANIFEST_FILE) \
		--path $(top_builddir)/tests/algebra

dist-hook:
	$(PYTHON3) $(top_srcdir)/tests/bin/bundle-test-files \
		--manifest-file $(srcdir)/$(ALGEBRA_MANIFEST_FILE) \
		--srcdir $(srcdir) \
		--distdir $(distdir)

$(top_builddir)/src/librasqal.la:
	cd $(top_builddir)/src && $(MAKE) librasqal.la


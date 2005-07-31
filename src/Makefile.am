# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal RDF query language library
#
# $Id$
#
# Copyright (C) 2003-2005, David Beckett http://purl.org/net/dajobe/
# Institute for Learning and Research Technology http://www.ilrt.bristol.ac.uk/
# University of Bristol, UK http://www.bristol.ac.uk/
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

bin_SCRIPTS = rasqal-config
lib_LTLIBRARIES = librasqal.la

include_HEADERS = rasqal.h

noinst_HEADERS = rasqal_internal.h win32_rasqal_config.h

man_MANS = rasqal-config.1

TESTS=rasqal_expr_test strcasecmp_test rasqal_skiplist_test

local_tests=rasqal_query_test
if RASQAL_DATATYPES
local_tests += rasqal_xsd_datatypes_test
endif
if RASQAL_QUERY_RDQL
local_tests += rdql_lexer_test rdql_parser_test
endif
if RASQAL_QUERY_SPARQL
local_tests += sparql_lexer_test sparql_parser_test
endif

EXTRA_PROGRAMS=$(TESTS) $(local_tests)

CLEANFILES=$(TESTS) $(local_tests)
MAINTAINERCLEANFILES=rdql_lexer.c rdql_lexer.h \
rdql_parser.c rdql_parser.h rdql_parser.output \
sparql_lexer.c sparql_lexer.h \
sparql_parser.c sparql_parser.h sparql_parser.output

MEM=@MEM@
MEM_LIBS=@MEM_LIBS@

AM_CPPFLAGS=@CPPFLAGS@ @RASQAL_INTERNAL_CPPFLAGS@ @RASQAL_EXTERNAL_CPPFLAGS@
AM_CFLAGS=@CFLAGS@ @RASQAL_INTERNAL_CPPFLAGS@ @RASQAL_EXTERNAL_CPPFLAGS@ $(MEM)

librasqal_la_SOURCES = \
rasqal_expr.c rasqal_general.c rasqal_query.c \
rasqal_engine.c rasqal_raptor.c rasqal_literal.c \
rasqal_graph_pattern.c rasqal_skiplist.c rasqal_map.c

if RASQAL_QUERY_RDQL
librasqal_la_SOURCES += rdql_lexer.c rdql_lexer.h \
rdql_parser.c rdql_parser.h rdql_common.h
endif
if RASQAL_QUERY_SPARQL
librasqal_la_SOURCES += sparql_lexer.c sparql_lexer.h \
sparql_parser.c sparql_parser.h sparql_common.h
endif

if GETOPT
librasqal_la_SOURCES += strcasecmp.c
endif
librasqal_la_LDFLAGS = -version-info @RASQAL_LIBTOOL_VERSION@
librasqal_la_LIBADD = @LTLIBOBJS@ @RASQAL_INTERNAL_LIBS@ @RASQAL_EXTERNAL_LIBS@ $(MEM_LIBS)
librasqal_la_DEPENDENCIES = @LTLIBOBJS@ @RASQAL_INTERNAL_LIBS@


EXTRA_DIST= \
rdql_lexer.l rdql_parser.y \
sparql_lexer.l sparql_parser.y \
rasqal-config.in \
$(man_MANS) \
fix-flex fix-bison \
rasqal_query_test.c


LEX=@LEX@
YACC=@YACC@

# Create some text files from HTML sources
LYNX=lynx
HTML_TO_TEXT=TERM=vt100 $(LYNX) -dump -nolist

SUFFIXES = .html .txt

if MAINTAINER_MODE
# Actually it needs rdql_parser.h but nevermind
rdql_lexer.c: $(srcdir)/rdql_lexer.l rdql_parser.c $(srcdir)/fix-flex
	$(LEX) -o$@ $(srcdir)/rdql_lexer.l
	$(PERL) $(srcdir)/fix-flex $@ > rdql_lexer.t
	mv rdql_lexer.t $@
	$(PERL) $(srcdir)/fix-flex rdql_lexer.h > rdql_lexer.t
	mv rdql_lexer.t rdql_lexer.h

rdql_parser.c: $(srcdir)/rdql_parser.y $(srcdir)/fix-bison
	$(YACC) -b rdql_parser -p rdql_parser_ -d -v $(srcdir)/rdql_parser.y
	$(PERL) $(srcdir)/fix-bison rdql_parser.tab.c > $@
	mv rdql_parser.tab.h rdql_parser.h
	rm -f rdql_parser.tab.c

# Actually it needs sparql_parser.h but nevermind
sparql_lexer.c: $(srcdir)/sparql_lexer.l sparql_parser.c $(srcdir)/fix-flex
	$(LEX) -o$@ $(srcdir)/sparql_lexer.l
	$(PERL) $(srcdir)/fix-flex $@ > sparql_lexer.t
	mv sparql_lexer.t $@
	$(PERL) $(srcdir)/fix-flex sparql_lexer.h > sparql_lexer.t
	mv sparql_lexer.t sparql_lexer.h

sparql_parser.c: $(srcdir)/sparql_parser.y $(srcdir)/fix-bison
	$(YACC) -b sparql_parser -p sparql_parser_ -d -v $(srcdir)/sparql_parser.y
	$(PERL) $(srcdir)/fix-bison sparql_parser.tab.c > $@
	mv sparql_parser.tab.h sparql_parser.h
	rm -f sparql_parser.tab.c
endif


# These are compiled here and used elsewhere for running tests
check-local: $(local_tests) run-rasqal-query-test

run-rasqal-query-test: $(rasqal_query_test)
	@tst=rasqal_query_test; \
	status=0; \
	if test -f ./$$tst; then dir=./; \
	elif test -f $$tst; then dir=; \
	else dir="$(top_srcdir)/"; fi; \
	if $${dir}$$tst $(top_srcdir)/data/dc.rdf; then \
	  echo "PASS: $$tst"; \
	else \
	  echo "FAIL: $$tst"; \
	  status=1; \
	fi; \
	exit $$status

rdql_lexer_test_SOURCES = rdql_lexer.c
rdql_lexer_test_CPPFLAGS = -DSTANDALONE
rdql_lexer_test_LDADD = librasqal.la @RASQAL_EXTERNAL_LIBS@ $(MEM_LIBS)

rdql_parser_test_SOURCES = rdql_parser.c
rdql_parser_test_CPPFLAGS = -DSTANDALONE
rdql_parser_test_LDADD = librasqal.la @RASQAL_EXTERNAL_LIBS@ $(MEM_LIBS)

sparql_lexer_test_SOURCES = sparql_lexer.c
sparql_lexer_test_CPPFLAGS = -DSTANDALONE
sparql_lexer_test_LDADD = librasqal.la @RASQAL_EXTERNAL_LIBS@ $(MEM_LIBS)

sparql_parser_test_SOURCES = sparql_parser.c
sparql_parser_test_CPPFLAGS = -DSTANDALONE
sparql_parser_test_LDADD = librasqal.la @RASQAL_EXTERNAL_LIBS@ $(MEM_LIBS)

rasqal_expr_test_SOURCES = rasqal_expr.c
rasqal_expr_test_CPPFLAGS = -DSTANDALONE
rasqal_expr_test_LDADD = librasqal.la @RASQAL_EXTERNAL_LIBS@ $(MEM_LIBS)

strcasecmp_test_SOURCES = strcasecmp.c
strcasecmp_test_CPPFLAGS = -DSTANDALONE

rasqal_query_test_SOURCES = rasqal_query_test.c
rasqal_query_test_CPPFLAGS = -DSTANDALONE
rasqal_query_test_LDADD = librasqal.la @RASQAL_EXTERNAL_LIBS@ $(MEM_LIBS)

rasqal_xsd_datatypes_test_SOURCES = rasqal_xsd_datatypes.c
rasqal_xsd_datatypes_test_CPPFLAGS = -DSTANDALONE @RASQAL_EXTERNAL_CPPFLAGS@
rasqal_xsd_datatypes_test_LDADD = librasqal.la @RASQAL_EXTERNAL_LIBS@ $(MEM_LIBS)

rasqal_skiplist_test_SOURCES = rasqal_skiplist.c
rasqal_skiplist_test_CPPFLAGS = -DSTANDALONE @RASQAL_EXTERNAL_CPPFLAGS@
rasqal_skiplist_test_LDADD = librasqal.la @RASQAL_EXTERNAL_LIBS@ $(MEM_LIBS)

$(top_builddir)/../raptor/libraptor.la:
	cd $(top_builddir)/../raptor && $(MAKE) $(AM_MAKEFLAGS) libraptor.la

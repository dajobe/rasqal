# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for RDF query language library
#
# $Id$
#
# Copyright (C) 2003-2004, David Beckett http://purl.org/net/dajobe/
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

bin_PROGRAMS = roqet
bin_SCRIPTS = rasqal-config
noinst_SCRIPTS = rasqal-src-config
lib_LTLIBRARIES = librasqal.la

include_HEADERS = rasqal.h

noinst_HEADERS = rasqal_internal.h win32_rasqal_config.h

man_MANS = roqet.1 librasqal.3 rasqal-config.1

SUBDIRS=data tests docs win32

TESTS=rasqal_expr_test strcasecmp_test rasqal_query_test
local_tests=
if RASQAL_QUERY_RDQL
local_tests += rdql_lexer_test rdql_parser_test
endif
if RASQAL_QUERY_SPARQL
local_tests += sparql_lexer_test sparql_parser_test
endif

CLEANFILES=$(TESTS) $(local_tests)
MAINTAINERCLEANFILES=rdql_lexer.c rdql_lexer.h \
rdql_parser.c rdql_parser.h rdql_parser.output \
sparql_lexer.c sparql_lexer.h \
sparql_parser.c sparql_parser.h sparql_parser.output

MEM=@MEM@
MEM_LIBS=@MEM_LIBS@

AM_CPPFLAGS=@CPPFLAGS@ @RASQAL_INTERNAL_CPPFLAGS@ @RASQAL_EXTERNAL_CPPFLAGS@
AM_CFLAGS=@CFLAGS@ @RASQAL_INTERNAL_CPPFLAGS@ @RASQAL_EXTERNAL_CPPFLAGS@ $(MEM)
AM_LDFLAGS=@RASQAL_EXTERNAL_LIBS@ $(MEM_LIBS)


librasqal_la_SOURCES = \
rasqal_expr.c rasqal_general.c rasqal_query.c \
rasqal_engine.c rasqal_raptor.c rasqal_literal.c \
rasqal_xml.c
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
librasqal_la_LIBADD = @LTLIBOBJS@ @RASQAL_INTERNAL_LIBS@ @RASQAL_EXTERNAL_LIBS@
librasqal_la_DEPENDENCIES = @LTLIBOBJS@ @RASQAL_INTERNAL_LIBS@


roqet_SOURCES = roqet.c
if GETOPT
roqet_SOURCES += getopt.c rasqal_getopt.h
endif
roqet_LDADD = librasqal.la


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = rasqal.pc


EXTRA_DIST=ChangeLog \
README      NEWS      LICENSE.txt \
README.html NEWS.html LICENSE.html INSTALL.html \
librasqal.html roqet.html RELEASE.html \
LICENSE-2.0.txt NOTICE \
rasqal.rdf.in \
rdql_lexer.l rdql_parser.y \
sparql_lexer.l sparql_parser.y \
rasqal-config.in rasqal-src-config.in \
autogen.sh \
rasqal.spec.in \
fix-groff-xhtml \
$(man_MANS) \
rasqal.pc.in \
fix-flex fix-bison \
rasqal_query_test.c


LEX=@LEX@
YACC=@YACC@

# Create some text files from HTML sources
LYNX=lynx
HTML_TO_TEXT=TERM=vt100 $(LYNX) -dump -nolist

SUFFIXES = .html .txt

.html.txt:
	$(HTML_TO_TEXT) $< > $@

README: README.html
	$(HTML_TO_TEXT) $< > $@

NEWS: NEWS.html
	$(HTML_TO_TEXT) $< > $@


if MAINTAINER_MODE
librasqal.html: $(srcdir)/librasqal.3 $(srcdir)/fix-groff-xhtml
	-groff -man -Thtml -P-l $< | tidy -asxml -wrap 1000 2>/dev/null | perl $(srcdir)/fix-groff-xhtml $@

roqet.html: $(srcdir)/roqet.1 $(srcdir)/fix-groff-xhtml
	-groff -man -Thtml -P-l $< | tidy -asxml -wrap 1000 2>/dev/null | perl $(srcdir)/fix-groff-xhtml $@


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
check-local: $(local_tests)

rdql_lexer_test: $(srcdir)/rdql_lexer.c librasqal.la
	$(LINK) $(DEFS) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE @RASQAL_INTERNAL_CPPFLAGS@ $(srcdir)/rdql_lexer.c librasqal.la $(LIBS)

rdql_parser_test: $(srcdir)/rdql_parser.c librasqal.la
	$(LINK) $(DEFS) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE @RASQAL_INTERNAL_CPPFLAGS@ $(srcdir)/rdql_parser.c librasqal.la $(LIBS)

sparql_lexer_test: $(srcdir)/sparql_lexer.c librasqal.la
	$(LINK) $(DEFS) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE @RASQAL_INTERNAL_CPPFLAGS@ $(srcdir)/sparql_lexer.c librasqal.la $(LIBS)

sparql_parser_test: $(srcdir)/sparql_parser.c librasqal.la
	$(LINK) $(DEFS) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE @RASQAL_INTERNAL_CPPFLAGS@ $(srcdir)/sparql_parser.c librasqal.la $(LIBS)

rasqal_expr_test$(EXEEXT): $(srcdir)/rasqal_expr.c librasqal.la
	$(LINK) $(DEFS) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE @RASQAL_INTERNAL_CPPFLAGS@ $(srcdir)/rasqal_expr.c librasqal.la $(LIBS)

strcasecmp_test$(EXEEXT): $(srcdir)/strcasecmp.c
	$(LINK) $(DEFS) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE $(srcdir)/strcasecmp.c

rasqal_query_test$(EXEEXT): $(srcdir)/rasqal_query_test.c librasqal.la
	$(LINK) $(DEFS) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE $(srcdir)/rasqal_query_test.c librasqal.la $(LIBS)

$(top_builddir)/raptor/libraptor.la:
	cd $(top_builddir)/raptor && $(MAKE) $(AM_MAKEFLAGS) libraptor.la

# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for RDF query language library
#
# $Id$
#
# Copyright (C) 2003-2004 David Beckett - http://purl.org/net/dajobe/
# Institute for Learning and Research Technology - http://www.ilrt.bris.ac.uk/
# University of Bristol - http://www.bristol.ac.uk/
# 
# This package is Free Software or Open Source available under the
# following licenses (these are alternatives):
#   1. GNU Lesser General Public License (LGPL)
#   2. GNU General Public License (GPL)
#   3. Mozilla Public License (MPL)
# 
# See LICENSE.html or LICENSE.txt at the top of this package for the
# full license terms.
# 

noinst_PROGRAMS = roqet
bin_SCRIPTS = rasqal-config
noinst_SCRIPTS = rasqal-src-config
lib_LTLIBRARIES = librasqal.la

include_HEADERS = rasqal.h

noinst_HEADERS = rasqal_internal.h win32_config.h

man_MANS = librasqal.3 rasqal-config.1

librasqal_la_SOURCES = rdql_lexer.c rdql_lexer.h \
rdql_parser.tab.c rdql_parser.tab.h rdql_common.h \
rasqal_expr.c rasqal_general.c rasqal_query.c rasqal_sequence.c \
rasqal_engine.c
librasqal_la_LDFLAGS = -version-info @RASQAL_LIBTOOL_VERSION@
librasqal_la_LIBADD = @RASQAL_INTERNAL_LIBS@
librasqal_la_DEPENDENCIES = @RASQAL_INTERNAL_LIBS@

roqet_SOURCES = roqet.c rasqal_getopt.h
roqet_LDADD = librasqal.la @RASQAL_INTERNAL_LIBS@
roqet_DEPENDENCIES = librasqal.la @RASQAL_INTERNAL_LIBS@
EXTRA_roqet_SOURCES = getopt.c


EXTRA_DIST=ChangeLog \
README      NEWS      LICENSE.txt \
README.html NEWS.html LICENSE.html INSTALL.html \
MPL.html librasqal.html \
rdql_lexer.l rdql_parser.y \
rasqal-config.in rasqal-src-config.in \
autogen.sh \
rasqal.spec.in \
$(man_MANS) \
rasqal.pc.in

SUBDIRS=tests rdql-testsuite

TESTS=rasqal_sequence_test

CLEANFILES=$(TESTS) rdql_lexer_test rdql_parser_test rasqal_query_test
MAINTAINERCLEANFILES=rdql_lexer.c rdql_lexer.h \
rdql_parser.tab.c rdql_parser.tab.h rdql_parser.output

MEM=@MEM@ `redland-config --cflags`
# FIXME - for testing
MEM_LIBS=@MEM_LIBS@ `redland-config --libs`

AM_CPPFLAGS=@CPPFLAGS@ @RASQAL_INTERNAL_CPPFLAGS@
AM_CFLAGS=@CFLAGS@ $(MEM)
LIBS=@LIBS@ $(MEM_LIBS)

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

@MAINT@librasqal.html: $(srcdir)/librasqal.3
@MAINT@	-groff -man -Thtml -P-l $< | tidy -asxml -wrap 1000 2>/dev/null | perl $(srcdir)/fix-groff-xhtml $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = rasqal.pc

# Actually it needs rdql_parser.h but nevermind
rdql_lexer.c: $(srcdir)/rdql_lexer.l rdql_parser.tab.c
	$(LEX) -o$@ $(srcdir)/rdql_lexer.l
	mv $@ rdql_lexer.t
	sed -e 's/\(^[ \t]*free(.*ptr.*\)$$/if(ptr) \1/' rdql_lexer.t > $@
	rm -f rdql_lexer.t

rdql_parser.tab.c: $(srcdir)/rdql_parser.y
	$(YACC) -b rdql_parser -p rdql_parser_ -d -v $(srcdir)/rdql_parser.y
	mv $@ rdql_parser.t
	sed -e '/Suppress GCC warning that yyerrlab1/,/^\#endif/d' rdql_parser.t > $@
	rm -f rdql_parser.t

check-local: rdql_lexer_test rdql_parser_test rasqal_query_test

rdql_lexer_test: $(srcdir)/rdql_lexer.c librasqal.la
	$(LINK) $(DEFS) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE @RASQAL_INTERNAL_CPPFLAGS@ $(srcdir)/rdql_lexer.c librasqal.la $(LIBS)

rdql_parser_test: $(srcdir)/rdql_parser.tab.c librasqal.la
	$(LINK) $(DEFS) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE @RASQAL_INTERNAL_CPPFLAGS@ $(srcdir)/rdql_parser.tab.c librasqal.la $(LIBS)

rasqal_query_test: $(srcdir)/rasqal_query.c librasqal.la
	$(LINK) $(DEFS) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE @RASQAL_INTERNAL_CPPFLAGS@ $(srcdir)/rasqal_query.c librasqal.la $(LIBS)

rasqal_sequence_test: $(srcdir)/rasqal_sequence.c librasqal.la
	$(LINK) $(DEFS) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE @RASQAL_INTERNAL_CPPFLAGS@ $(srcdir)/rasqal_sequence.c librasqal.la $(LIBS)

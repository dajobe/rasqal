# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for RDF query language library
#
# $Id$
#
# Copyright (C) 2003 David Beckett - http://purl.org/net/dajobe/
# Institute for Learning and Research Technology - http://www.ilrt.org/
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

bin_SCRIPTS = rasqal-config
noinst_SCRIPTS = rasqal-src-config
lib_LTLIBRARIES = librasqal.la

include_HEADERS = rasqal.h

noinst_HEADERS = rasqal_internal.h win32_config.h

man_MANS = librasqal.3 rasqal-config.1

librasqal_la_SOURCES = rdql_lexer.c rdql_lexer.h \
rdql_parser.tab.c rdql_parser.tab.h rdql_common.h \
rasqal_expr.c rasqal_general.c rasqal_query.c rasqal_sequence.c
librasqal_la_LDFLAGS = -version-info @RASQAL_LIBTOOL_VERSION@
librasqal_la_LIBADD = @RASQAL_INTERNAL_LIBS@
librasqal_la_DEPENDENCIES = @RASQAL_INTERNAL_LIBS@

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

SUBDIRS=tests

TESTS=rasqal_sequence_test

CLEANFILES=$(TESTS) rdql_lexer_test rdql_parser_test rasqal_query_test
MAINTAINERCLEANFILES=rdql_lexer.c rdql_lexer.h \
rdql_parser.tab.c rdql_parser.tab.h rdql_parser.output

MEM=@MEM@
MEM_LIBS=@MEM_LIBS@

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
@MAINT@rdql_lexer.c: $(srcdir)/rdql_lexer.l rdql_parser.tab.c
@MAINT@	$(LEX) -o$@ $(srcdir)/rdql_lexer.l
@MAINT@	mv $@ rdql_lexer.t
@MAINT@	sed -e 's/\(^[ \t]*free(.*ptr.*\)$$/if(ptr) \1/' rdql_lexer.t > $@
@MAINT@	rm -f rdql_lexer.t

@MAINT@rdql_parser.tab.c: $(srcdir)/rdql_parser.y
@MAINT@	$(YACC) -b rdql_parser -p rdql_parser_ -d -v $(srcdir)/rdql_parser.y


# Actually it needs rdql_parser.h but nevermind
# Needs raptor
rdql_lexer_test: $(srcdir)/rdql_lexer.c $(srcdir)/rdql_parser.tab.c
	$(LINK) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE @RASQAL_INTERNAL_CPPFLAGS@ $(srcdir)/rdql_lexer.c librasqal.la

# Needs raptor
rdql_parser_test: $(srcdir)/rdql_parser.tab.c librasqal.la
	$(LINK) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE @RASQAL_INTERNAL_CPPFLAGS@ $(srcdir)/rdql_parser.tab.c librasqal.la

rasqal_query_test: $(srcdir)/rasqal_query.c librasqal.la
	$(LINK) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE @RASQAL_INTERNAL_CPPFLAGS@ $(srcdir)/rasqal_query.c librasqal.la

rasqal_sequence_test: $(srcdir)/rasqal_sequence.c
	$(LINK) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE $(srcdir)/rasqal_sequence.c librasqal.la

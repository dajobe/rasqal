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

bin_PROGRAMS = roqet
bin_SCRIPTS = rasqal-config
noinst_SCRIPTS = rasqal-src-config
lib_LTLIBRARIES = librasqal.la

include_HEADERS = rasqal.h

noinst_HEADERS = rasqal_internal.h win32_rasqal_config.h

man_MANS = roqet.1 librasqal.3 rasqal-config.1

SUBDIRS=data tests docs win32

TESTS=rasqal_expr_test strcasecmp_test rasqal_query_test
if RASQAL_QUERY_RDQL
TESTS += rdql_lexer_test rdql_parser_test
endif
if RASQAL_QUERY_BRQL
TESTS += brql_lexer_test brql_parser_test
endif

CLEANFILES=$(TESTS)
MAINTAINERCLEANFILES=rdql_lexer.c rdql_lexer.h \
rdql_parser.c rdql_parser.h rdql_parser.output \
brql_lexer.c brql_lexer.h \
brql_parser.c brql_parser.h brql_parser.output

MEM=@MEM@
MEM_LIBS=@MEM_LIBS@

AM_CPPFLAGS=@CPPFLAGS@ @RASQAL_INTERNAL_CPPFLAGS@ @RASQAL_EXTERNAL_CPPFLAGS@
AM_CFLAGS=@CFLAGS@ @RASQAL_INTERNAL_CPPFLAGS@ @RASQAL_EXTERNAL_CPPFLAGS@ $(MEM)
AM_LDFLAGS=@RASQAL_EXTERNAL_LIBS@ $(MEM_LIBS)


librasqal_la_SOURCES = \
rasqal_expr.c rasqal_general.c rasqal_query.c \
rasqal_engine.c rasqal_raptor.c rasqal_literal.c
if RASQAL_QUERY_RDQL
librasqal_la_SOURCES += rdql_lexer.c rdql_lexer.h \
rdql_parser.c rdql_parser.h rdql_common.h
endif
if RASQAL_QUERY_BRQL
librasqal_la_SOURCES += brql_lexer.c brql_lexer.h \
brql_parser.c brql_parser.h brql_common.h
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
MPL.html librasqal.html roqet.html RELEASE.html \
rdql_lexer.l rdql_parser.y \
brql_lexer.l brql_parser.y \
rasqal-config.in rasqal-src-config.in \
autogen.sh \
rasqal.spec.in \
fix-groff-xhtml \
$(man_MANS) \
rasqal.pc.in \
fix-flex \
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

rdql_parser.c: $(srcdir)/rdql_parser.y
	$(YACC) -b rdql_parser -p rdql_parser_ -d -v $(srcdir)/rdql_parser.y
	sed -e '/Suppress GCC warning that yyerrlab1/,/^\#endif/d'  -e "s/rdql_parser.tab.c/$@/" rdql_parser.tab.c > $@
	mv rdql_parser.tab.h rdql_parser.h
	rm -f rdql_parser.tab.c

# Actually it needs brql_parser.h but nevermind
brql_lexer.c: $(srcdir)/brql_lexer.l brql_parser.c $(srcdir)/fix-flex
	$(LEX) -o$@ $(srcdir)/brql_lexer.l
	$(PERL) $(srcdir)/fix-flex $@ > brql_lexer.t
	mv brql_lexer.t $@
	$(PERL) $(srcdir)/fix-flex brql_lexer.h > brql_lexer.t
	mv brql_lexer.t brql_lexer.h

brql_parser.c: $(srcdir)/brql_parser.y
	$(YACC) -b brql_parser -p brql_parser_ -d -v $(srcdir)/brql_parser.y
	sed -e '/Suppress GCC warning that yyerrlab1/,/^\#endif/d'  -e "s/brql_parser.tab.c/$@/" brql_parser.tab.c > $@
	mv brql_parser.tab.h brql_parser.h
	rm -f brql_parser.tab.c
endif


rdql_lexer_test: $(srcdir)/rdql_lexer.c librasqal.la
	$(LINK) $(DEFS) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE @RASQAL_INTERNAL_CPPFLAGS@ $(srcdir)/rdql_lexer.c librasqal.la $(LIBS)

rdql_parser_test: $(srcdir)/rdql_parser.c librasqal.la
	$(LINK) $(DEFS) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE @RASQAL_INTERNAL_CPPFLAGS@ $(srcdir)/rdql_parser.c librasqal.la $(LIBS)

brql_lexer_test: $(srcdir)/brql_lexer.c librasqal.la
	$(LINK) $(DEFS) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE @RASQAL_INTERNAL_CPPFLAGS@ $(srcdir)/brql_lexer.c librasqal.la $(LIBS)

brql_parser_test: $(srcdir)/brql_parser.c librasqal.la
	$(LINK) $(DEFS) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE @RASQAL_INTERNAL_CPPFLAGS@ $(srcdir)/brql_parser.c librasqal.la $(LIBS)

rasqal_expr_test: $(srcdir)/rasqal_expr.c librasqal.la
	$(LINK) $(DEFS) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE @RASQAL_INTERNAL_CPPFLAGS@ $(srcdir)/rasqal_expr.c librasqal.la $(LIBS)

strcasecmp_test: $(srcdir)/strcasecmp.c
	$(LINK) $(DEFS) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE $(srcdir)/strcasecmp.c

rasqal_query_test: $(srcdir)/rasqal_query_test.c librasqal.la
	$(LINK) $(DEFS) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE $(srcdir)/rasqal_query_test.c librasqal.la $(LIBS)

$(top_builddir)/raptor/libraptor.la:
	cd $(top_builddir)/raptor && $(MAKE) $(AM_MAKEFLAGS) libraptor.la

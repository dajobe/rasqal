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
rasqal_general.c rasqal_query.c rasqal_sequence.c
librasqal_la_LDFLAGS = -version-info @RASQAL_LIBTOOL_VERSION@
librasqal_la_LIBADD = @RASQAL_INTERNAL_LIBS@
librasqal_la_DEPENDENCIES = @RASQAL_INTERNAL_LIBS@

RDQL_EXAMPLES=example1.rdql example2.rdql example3.rdql example4.rdql	\
example5.rdql example_at_1.rdql example_at_2.rdql example_at_3.rdql	\
example_at_4.rdql example_at_5.rdql example_at_6.rdql			\
example_at_7.rdql

EXTRA_DIST=ChangeLog \
README      NEWS      LICENSE.txt \
README.html NEWS.html LICENSE.html INSTALL.html \
MPL.html librasqal.html \
rdql_lexer.l rdql_parser.y \
rasqal-config.in rasqal-src-config.in \
autogen.sh \
rasqal.spec.in \
$(man_MANS) \
$(RDQL_EXAMPLES) \
rasqal.pc.in

TESTS=rasqal_sequence_test

CLEANFILES=$(TESTS) rdql_lexer_test rdql_parser_test rasqal_query_test
MAINTAINERCLEANFILES=rdql_lexer.c rdql_lexer.h \
rdql_parser.tab.c rdql_parser.tab.h rdql_parser.output

MEM=@MEM@
MEM_LIBS=@MEM_LIBS@

AM_CPPFLAGS=@CPPFLAGS@ @RASQAL_INTERNAL_CPPFLAGS@
AM_CFLAGS=@CFLAGS@ @REDLAND_CFLAGS@ $(MEM)
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

@MAINT@rdql_parser.tab.c: $(srcdir)/rdql_parser.y
@MAINT@	$(YACC) -b rdql_parser -p rdql_parser_ -d -v $(srcdir)/rdql_parser.y


# Actually it needs rdql_parser.h but nevermind
rdql_lexer_test: $(srcdir)/rdql_lexer.c $(srcdir)/rdql_parser.tab.c
	$(LINK) $(DEFS) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE $(srcdir)/rdql_lexer.c librasqal.la @REDLAND_LIBS@

rdql_parser_test: $(srcdir)/rdql_parser.tab.c librasqal.la
	$(LINK) $(DEFS) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE $(srcdir)/rdql_parser.tab.c librasqal.la librasqal.la @REDLAND_LIBS@

rasqal_query_test: $(srcdir)/rasqal_query.c librasqal.la
	$(LINK) $(DEFS) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE $(srcdir)/rasqal_query.c librasqal.la librasqal.la @REDLAND_LIBS@

rasqal_sequence_test: $(srcdir)/rasqal_sequence.c
	$(LINK) $(DEFS) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE $(srcdir)/rasqal_sequence.c librasqal.la @REDLAND_LIBS@



check-local: check-rdql-lexer check-rdql-parser check-rdql-queries

check-rdql-lexer: rdql_lexer_test
	@result=0; \
	$(ECHO) "Testing legal RDQL lexing"; \
	for test in $(RDQL_EXAMPLES); do \
	  $(ECHO) $(ECHO_N) "Checking $$test $(ECHO_C)"; \
	  $(top_builddir)/rdql_lexer_test $(srcdir)/$$test >/dev/null; \
	  status=$$?; \
	  if test $$status != 0 -a $$status != 2 ; then \
	    $(ECHO) FAILED returned status $$status; result=1; \
	  else \
	    $(ECHO) ok; \
	  fi; \
	done; \
	exit $$result

check-rdql-parser: rdql_parser_test
	@result=0; \
	$(ECHO) "Testing legal RDQL parsing"; \
	for test in $(RDQL_EXAMPLES); do \
	  $(ECHO) $(ECHO_N) "Checking $$test $(ECHO_C)"; \
	  $(top_builddir)/rdql_parser_test $(srcdir)/$$test >/dev/null; \
	  status=$$?; \
	  if test $$status != 0 -a $$status != 2 ; then \
	    $(ECHO) FAILED returned status $$status; result=1; \
	  else \
	    $(ECHO) ok; \
	  fi; \
	done; \
	exit $$result

check-rdql-queries: rasqal_query_test
	@result=0; \
	$(ECHO) "Testing query construction (RDQL)"; \
	for test in $(RDQL_EXAMPLES); do \
	  $(ECHO) $(ECHO_N) "Checking $$test $(ECHO_C)"; \
	  $(top_builddir)/rasqal_query_test rdql - < $(srcdir)/$$test >/dev/null; \
	  status=$$?; \
	  if test $$status != 0 -a $$status != 2 ; then \
	    $(ECHO) FAILED returned status $$status; result=1; \
	  else \
	    $(ECHO) ok; \
	  fi; \
	done; \
	exit $$result

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

librasqal_la_SOURCES = rdql_lexer.c rdql_parser.c rdql_parser.h \
rasqal_general.c
librasqal_la_LDFLAGS = -version-info @RASQAL_LIBTOOL_VERSION@

RDQL_EXAMPLES=example1.rdql example2.rdql example3.rdql example4.rdql	\
example_at_1.rdql example_at_2.rdql example_at_3.rdql			\
example_at_4.rdql example_at_5.rdql example_at_6.rdql			\
example_at_7.rdql


EXTRA_DIST=ChangeLog \
README      NEWS      LICENSE.txt \
MPL.html \
rdql_lexer.l rdql_parser.y \
rasqal-config.in rasqal-src-config.in \
autogen.sh \
rasqal.spec.in \
$(man_MANS) \
$(RDQL_EXAMPLES) \
rasqal.pc.in

CLEANFILES=$(TESTS) rdql_lexer_test rdql_parser_test
MAINTAINERCLEANFILES=rdql_lexer.c rdql_parser.c rdql_parser.h

AM_CPPFLAGS=-I$(top_srcdir)
LIBS=@LIBS@ $(MEM_LIBS)

LEX=@LEX@
YACC=@YACC@

# Memory debugging alternatives
MEM=@MEM@
MEM_LIBS=@MEM_LIBS@

# 1) None (use standard functions directly)
#MEM=
#MEM_LIBS=

# 2) Use dmalloc library
#MEM=-DRAPTOR_MEMORY_DEBUG_DMALLOC=1
#MEM_LIBS=-ldmalloc


# Actually it needs rdql_parser.h but nevermind
@MAINT@rdql_lexer.c: $(srcdir)/rdql_lexer.l rdql_parser.c
@MAINT@	$(LEX) -t $(srcdir)/rdql_lexer.l > $@
@MAINT@#	if [ -r lex.yy.c ]; then \
@MAINT@#	  mv lex.yy.c $@; \
@MAINT@#	fi


@MAINT@rdql_parser.c: $(srcdir)/rdql_parser.y
@MAINT@	$(YACC) -d $(srcdir)/rdql_parser.y
@MAINT@	if [ -r y.tab.c ]; then \
@MAINT@	  sed -e 's,y.tab.c,rdql_parser.c,g' < y.tab.c > rdql_parser.c; \
@MAINT@	  sed -e 's,y.tab.h,rdql_parser.h,g' < y.tab.h > rdql_parser.h; \
@MAINT@	  rm -f y.tab.c y.tab.h; \
@MAINT@	fi

rdql_lexer.lo: $(srcdir)/rdql_parser.c

# Actually it needs rdql_parser.h but nevermind
rdql_lexer_test: $(srcdir)/rdql_lexer.c $(srcdir)/rdql_parser.c
	$(LINK) $(DEFS) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE -static $(srcdir)/rdql_lexer.c $(LIBS)

rdql_parser_test: $(srcdir)/rdql_lexer.lo $(srcdir)/rdql_parser.c 
	$(LINK) $(DEFS) $(CPPFLAGS) -I$(srcdir) -I. -DSTANDALONE -static $(srcdir)/rdql_parser.c rdql_lexer.lo -o $@


check-local: check-rdql-lexer check-rdql-parser

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


# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal utilities
#
# Copyright (C) 2005-2011, David Beckett http://www.dajobe.org/
# Copyright (C) 2005-2005, University of Bristol, UK http://www.bristol.ac.uk/
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

bin_PROGRAMS = roqet rasqal-compare
noinst_PROGRAMS = to-ntriples
EXTRA_PROGRAMS = srxread srxwrite testrunner rasqal-mcp-server

CLEANFILES = $(EXTRA_PROGRAMS) *.plist

man_MANS = roqet.1 rasqal-compare.1

AM_CPPFLAGS= @RASQAL_INTERNAL_CPPFLAGS@ -I$(top_srcdir)/src
AM_CFLAGS = $(MEM)
STANDARD_CFLAGS= @STANDARD_CFLAGS@ $(MEM)
LIBS= @RASQAL_INTERNAL_LIBS@ $(MEM_LIBS)

ANALYZE = clang
ANALYZE_FLAGS = "--analyze"
# Based on COMPILE target
ANALYZE_COMMAND = $(ANALYZE) \
	$(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) \
	$(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) \
	$(ANALYZE_FLAGS)


noinst_LTLIBRARIES = librasqalcmdline.la

librasqalcmdline_la_SOURCES = \
read_files.c \
results.c \
rasqalcmdline.h


roqet_SOURCES = roqet.c
roqet_CPPFLAGS = $(AM_CPPFLAGS)
roqet_LDADD = $(top_builddir)/src/librasqal.la librasqalcmdline.la
roqet_DEPENDENCIES = $(top_builddir)/src/librasqal.la librasqalcmdline.la
roqet_LDFLAGS = @RAPTOR2_LIBS@
if GETOPT
roqet_CPPFLAGS += -I$(top_srcdir)/getopt
roqet_LDADD += $(top_builddir)/getopt/libgetopt.la
roqet_DEPENDENCIES += $(top_builddir)/libgetopt/libgetopt.la
endif



rasqal_compare_SOURCES = rasqal-compare.c
rasqal_compare_CPPFLAGS = $(AM_CPPFLAGS)
rasqal_compare_LDADD = $(top_builddir)/src/librasqal.la librasqalcmdline.la
rasqal_compare_DEPENDENCIES = $(top_builddir)/src/librasqal.la librasqalcmdline.la
rasqal_compare_LDFLAGS = @RAPTOR2_LIBS@
if GETOPT
rasqal_compare_CPPFLAGS += -I$(top_srcdir)/getopt
rasqal_compare_LDADD += $(top_builddir)/getopt/libgetopt.la
rasqal_compare_DEPENDENCIES += $(top_builddir)/libgetopt/libgetopt.la
endif

rasqal_mcp_server_SOURCES = rasqal-mcp-server.c
rasqal_mcp_server_CPPFLAGS = $(AM_CPPFLAGS)
rasqal_mcp_server_LDADD = $(top_builddir)/src/librasqal.la librasqalcmdline.la
rasqal_mcp_server_DEPENDENCIES = $(top_builddir)/src/librasqal.la librasqalcmdline.la
rasqal_mcp_server_LDFLAGS = @RAPTOR2_LIBS@
if GETOPT
rasqal_mcp_server_CPPFLAGS += -I$(top_srcdir)/getopt
rasqal_mcp_server_LDADD += $(top_builddir)/getopt/libgetopt.la
rasqal_mcp_server_DEPENDENCIES += $(top_builddir)/libgetopt/libgetopt.la
endif

testrunner_SOURCES = testrunner.c
testrunner_CPPFLAGS = $(AM_CPPFLAGS)
testrunner_LDADD = $(top_builddir)/src/librasqal.la librasqalcmdline.la
testrunner_DEPENDENCIES = $(top_builddir)/src/librasqal.la librasqalcmdline.la
testrunner_LDFLAGS = @RAPTOR2_LIBS@
if GETOPT
testrunner_CPPFLAGS += -I$(top_srcdir)/getopt
testrunner_LDADD += $(top_builddir)/getopt/libgetopt.la
testrunner_DEPENDENCIES += $(top_builddir)/libgetopt/libgetopt.la
endif

to_ntriples_SOURCES = to-ntriples.c
to_ntriples_CPPFLAGS = $(AM_CPPFLAGS)
to_ntriples_LDFLAGS = @RAPTOR2_LIBS@

srxread_SOURCES = srxread.c
srxread_LDADD = $(top_builddir)/src/librasqal.la
srxread_DEPENDENCIES = $(top_builddir)/src/librasqal.la
srxread_LDFLAGS = @RAPTOR2_LIBS@

srxwrite_SOURCES = srxwrite.c
srxwrite_LDADD = $(top_builddir)/src/librasqal.la
srxwrite_DEPENDENCIES = $(top_builddir)/src/librasqal.la
srxwrite_LDFLAGS = @RAPTOR2_LIBS@

EXTRA_DIST= \
$(man_MANS) \
roqet.html \
rasqal-compare.html \
test-mcp-server.py

if MAINTAINER_MODE
roqet.html: $(srcdir)/roqet.1 $(srcdir)/../scripts/fix-groff-xhtml.py
	-groff -man -Thtml -P-l $< | tidy -asxml -wrap 1000 2>/dev/null | $(PYTHON3) $(srcdir)/../scripts/fix-groff-xhtml.py $@

rasqal-compare.html: $(srcdir)/rasqal-compare.1 $(srcdir)/../scripts/fix-groff-xhtml.py
	-groff -man -Thtml -P-l $< | tidy -asxml -wrap 1000 2>/dev/null | $(PYTHON3) $(srcdir)/../scripts/fix-groff-xhtml.py $@

endif

$(top_builddir)/src/librasqal.la:
	cd $(top_builddir)/src && $(MAKE) librasqal.la

$(top_builddir)/libgetopt/libgetopt.la:
	cd $(top_builddir)/libgetopt && $(MAKE) libgetopt.la

examples: srxread$(EXTEXE) srxwrite$(EXTEXE)

if MAINTAINER_MODE
# Run Clang static analyzer over sources.
analyze: $(SOURCES)
	@list='$(SOURCES)'; \
	result=0; \
	for file in $$list; do \
	  if echo $$file | grep '\.c$$' >/dev/null 2>&1; then \
	    $(RECHO) "Analyzing $$file"; \
	    $(ANALYZE_COMMAND) $(srcdir)/$$file; \
	    status=$$?; \
	    if test $$status != 0; then \
	      result=1; \
	    fi; \
	  fi; \
	done; \
	set -e; exit $$result
endif

# Test the MCP server
test-mcp-server: rasqal-mcp-server
	@echo "Running MCP server smoke tests..."
	@$(PYTHON3) $(srcdir)/test-mcp-server.py ./rasqal-mcp-server

all-programs: $(PROGRAMS) $(EXTRA_PROGRAMS)

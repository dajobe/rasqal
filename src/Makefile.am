# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal RDF query language library
#
# Copyright (C) 2003-2010, David Beckett http://www.dajobe.org/
# Copyright (C) 2003-2005, University of Bristol, UK http://www.bristol.ac.uk/
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

nodist_pkginclude_HEADERS = rasqal.h

noinst_HEADERS = rasqal_internal.h win32_rasqal_config.h sv_config.h

man_MANS = rasqal-config.1

TESTS_ENVIRONMENT= \
  RDF_DATA_FILE=$(top_srcdir)/data/dc.rdf \
  NT_DATA_FILE=$(top_srcdir)/data/one.nt \
  SPARQL_QUERY_FILE=$(top_srcdir)/tests/sparql/examples/ex11_1.rq

TESTS=rasqal_algebra_test$(EXEEXT) rasqal_expr_test$(EXEEXT)	\
strcasecmp_test$(EXEEXT) \
rasqal_decimal_test$(EXEEXT) rasqal_datetime_test$(EXEEXT)	\
rasqal_variable_test$(EXEEXT) rasqal_rowsource_empty_test$(EXEEXT) \
rasqal_rowsource_union_test$(EXEEXT) \
rasqal_rowsource_rowsequence_test$(EXEEXT) \
rasqal_rowsource_project_test$(EXEEXT) \
rasqal_rowsource_join_test$(EXEEXT) \
rasqal_query_test$(EXEEXT) \
rasqal_rowsource_triples_test$(EXEEXT) \
rasqal_row_compatible_test$(EXEEXT) \
rasqal_rowsource_groupby_test$(EXEEXT) \
rasqal_rowsource_aggregation_test$(EXEEXT) \
rasqal_literal_test$(EXEEXT) \
rasqal_regex_test$(EXEEXT) \
rasqal_random_test$(EXEEXT) \
rasqal_xsd_datatypes_test$(EXEEXT) \
rasqal_results_compare_test$(EXEEXT) \
rasqal_query_results_test$(EXEEXT)

# These 2 test programs are compiled here and run here as 'smoke
# tests' but mostly used in tests in $(srcdir)/../tests/sparql
# to do detailed tests on the lext/parsers
if RASQAL_QUERY_SPARQL
TESTS += sparql_lexer_test$(EXEEXT) sparql_parser_test$(EXEEXT)
endif

BROKEN_TESTS=rasqal_rowsource_service_test$(EXEEXT)

EXTRA_PROGRAMS=$(TESTS) $(BROKEN_TESTS)

CLEANFILES=$(TESTS) \
*.plist \
git-version.h

MAINTAINERCLEANFILES= \
sparql_lexer.c sparql_lexer.h \
sparql_parser.c sparql_parser.h sparql_parser.output

MEM=@MEM@
MEM_LIBS=@MEM_LIBS@

AM_CFLAGS= @RASQAL_INTERNAL_CPPFLAGS@ @STANDARD_CFLAGS@ $(MEM)
LIBS= @RASQAL_INTERNAL_LIBS@ @RASQAL_EXTERNAL_LIBS@ $(MEM_LIBS)

ANALYZE = clang
ANALYZE_FLAGS = "--analyze"
# Based on COMPILE target
ANALYZE_COMMAND = $(ANALYZE) \
	$(DEFS) $(DEFAULT_INCLUDES) $(INCLUDES) $(AM_CPPFLAGS) \
	$(CPPFLAGS) $(AM_CFLAGS) $(CFLAGS) \
	$(ANALYZE_FLAGS)

librasqal_la_SOURCES = \
rasqal_algebra.c \
rasqal_expr.c rasqal_expr_evaluate.c \
rasqal_expr_datetimes.c rasqal_expr_numerics.c rasqal_expr_strings.c \
rasqal_general.c rasqal_query.c rasqal_query_results.c \
rasqal_engine.c rasqal_raptor.c rasqal_literal.c rasqal_formula.c \
rasqal_graph_pattern.c rasqal_map.c rasqal_feature.c \
rasqal_result_formats.c rasqal_xsd_datatypes.c rasqal_decimal.c \
rasqal_datetime.c rasqal_rowsource.c rasqal_format_sparql_xml.c \
rasqal_variable.c rasqal_rowsource_empty.c rasqal_rowsource_union.c \
rasqal_rowsource_rowsequence.c rasqal_query_transform.c rasqal_row.c \
rasqal_engine_algebra.c rasqal_triples_source.c \
rasqal_rowsource_triples.c rasqal_rowsource_filter.c \
rasqal_rowsource_sort.c rasqal_engine_sort.c \
rasqal_rowsource_project.c rasqal_rowsource_join.c \
rasqal_rowsource_graph.c rasqal_rowsource_distinct.c \
rasqal_rowsource_groupby.c rasqal_rowsource_aggregation.c \
rasqal_rowsource_having.c rasqal_rowsource_slice.c \
rasqal_rowsource_bindings.c rasqal_rowsource_service.c \
rasqal_row_compatible.c rasqal_format_table.c rasqal_query_write.c \
rasqal_format_json.c rasqal_format_sv.c rasqal_format_html.c \
rasqal_format_rdf.c \
rasqal_rowsource_assignment.c rasqal_update.c \
rasqal_triple.c rasqal_data_graph.c rasqal_prefix.c \
rasqal_solution_modifier.c rasqal_projection.c rasqal_bindings.c \
rasqal_service.c \
rasqal_dataset.c \
rasqal_random.c \
rasqal_digest.c \
rasqal_iostream.c \
rasqal_regex.c \
snprintf.c \
rasqal_double.c \
rasqal_ntriples.c \
rasqal_results_compare.c \
ssort.h

if RASQAL_QUERY_SPARQL
librasqal_la_SOURCES += sparql_lexer.c sparql_lexer.h \
sparql_parser.c sparql_parser.h sparql_common.h
endif

if STRCASECMP
librasqal_la_SOURCES += strcasecmp.c
endif
if TIMEGM
librasqal_la_SOURCES += timegm.c
endif
if GETTIMEOFDAY
librasqal_la_SOURCES += gettimeofday.c
endif
if RASQAL_SORT
librasqal_la_SOURCES += rasqal_sort.c
endif

if RASQAL_DIGEST_INTERNAL
librasqal_la_SOURCES += rasqal_digest_md5.c rasqal_digest_sha1.c
endif

nodist_librasqal_la_SOURCES =

if MAINTAINER_MODE
nodist_librasqal_la_SOURCES += git-version.h

# rasqal_algebra.c is the first source file so this ensures
# git-version.h is created before it and before the actual file
# dependencies are calculated as a side-effect of compilation
rasqal_algebra.c: git-version.h
endif

librasqal_la_LDFLAGS = -version-info @RASQAL_LIBTOOL_VERSION@
librasqal_la_LIBADD = @LTLIBOBJS@ @RASQAL_INTERNAL_LIBS@ @RASQAL_EXTERNAL_LIBS@ $(MEM_LIBS)
librasqal_la_DEPENDENCIES = @LTLIBOBJS@ @RASQAL_INTERNAL_LIBS@

AM_CFLAGS += -DSV_CONFIG -I$(top_srcdir)/libsv
librasqal_la_LIBADD += $(top_builddir)/libsv/libsv.la
librasqal_la_DEPENDENCIES += $(top_builddir)/libsv/libsv.la

$(top_builddir)/libsv/libsv.la:
	cd $(top_builddir)/libsv && $(MAKE) libsv.la

if RASQAL_MTWIST

AM_CFLAGS += -DMTWIST_CONFIG -I$(top_srcdir)/libmtwist
librasqal_la_LIBADD += $(top_builddir)/libmtwist/libmtwist.la
librasqal_la_DEPENDENCIES += $(top_builddir)/libmtwist/libmtwist.la

$(top_builddir)/libmtwist/libmtwist.la:
	cd $(top_builddir)/libmtwist && $(MAKE) libmtwist.la

endif

if GETOPT
$(top_builddir)/libgetopt/libgetopt.la:
	cd $(top_builddir)/libgetopt && $(MAKE) libgetopt.la
endif


EXTRA_DIST= \
sparql_lexer.l sparql_parser.y \
rasqal-config.in \
$(man_MANS) \
rasqal_query_test.c \
mtwist_config.h

LEX=@LEX@
YACC=@YACC@

# Create some text files from HTML sources
LYNX=lynx
HTML_TO_TEXT=TERM=vt100 $(LYNX) -dump -nolist

SUFFIXES = .html .txt

if MAINTAINER_MODE
# Actually it needs sparql_parser.h but nevermind
sparql_lexer.c: $(srcdir)/sparql_lexer.l sparql_parser.c $(srcdir)/../scripts/fix-flex.pl
	$(AM_V_GEN) \
	$(LEX) -o$@ $(srcdir)/sparql_lexer.l; \
	$(PERL) $(srcdir)/../scripts/fix-flex.pl $@ > sparql_lexer.t; \
	mv sparql_lexer.t $@

sparql_parser.c: $(srcdir)/sparql_parser.y $(srcdir)/../scripts/fix-bison.pl
	$(AM_V_GEN) \
	$(BISON) $(srcdir)/sparql_parser.y; \
	$(PERL) $(srcdir)/../scripts/fix-bison.pl sparql_parser.tab.c > $@; \
	mv sparql_parser.tab.h sparql_parser.h; \
	rm -f sparql_parser.tab.c

sparql_parser.h: sparql_parser.c ; @exit 0

endif


sparql_lexer_test_SOURCES = sparql_lexer.c
sparql_lexer_test_CPPFLAGS = -DSTANDALONE
sparql_lexer_test_LDADD = librasqal.la

sparql_parser_test_SOURCES = sparql_parser.c
sparql_parser_test_CPPFLAGS = -DSTANDALONE
sparql_parser_test_LDADD = librasqal.la
if GETOPT
sparql_parser_test_CPPFLAGS += -I$(top_srcdir)/getopt
sparql_parser_test_LDADD += $(top_builddir)/getopt/libgetopt.la
endif

rasqal_expr_test_SOURCES = rasqal_expr.c
rasqal_expr_test_CPPFLAGS = -DSTANDALONE
rasqal_expr_test_LDADD = librasqal.la

strcasecmp_test_SOURCES = strcasecmp.c
strcasecmp_test_CPPFLAGS = -DSTANDALONE
strcasecmp_test_LDADD = librasqal.la

rasqal_query_test_SOURCES = rasqal_query_test.c
rasqal_query_test_CPPFLAGS = -DSTANDALONE
rasqal_query_test_LDADD = librasqal.la

rasqal_decimal_test_SOURCES = rasqal_decimal.c
rasqal_decimal_test_CPPFLAGS = -DSTANDALONE
rasqal_decimal_test_LDADD = librasqal.la

rasqal_datetime_test_SOURCES = rasqal_datetime.c
rasqal_datetime_test_CPPFLAGS = -DSTANDALONE
rasqal_datetime_test_LDADD = librasqal.la

rasqal_algebra_test_SOURCES = rasqal_algebra.c
rasqal_algebra_test_CPPFLAGS = -DSTANDALONE
rasqal_algebra_test_LDADD = librasqal.la

rasqal_variable_test_SOURCES = rasqal_variable.c
rasqal_variable_test_CPPFLAGS = -DSTANDALONE
rasqal_variable_test_LDADD = librasqal.la

rasqal_rowsource_aggregation_test_SOURCES = rasqal_rowsource_aggregation.c
rasqal_rowsource_aggregation_test_CPPFLAGS = -DSTANDALONE
rasqal_rowsource_aggregation_test_LDADD = librasqal.la

rasqal_rowsource_empty_test_SOURCES = rasqal_rowsource_empty.c
rasqal_rowsource_empty_test_CPPFLAGS = -DSTANDALONE
rasqal_rowsource_empty_test_LDADD = librasqal.la

rasqal_rowsource_groupby_test_SOURCES = rasqal_rowsource_groupby.c
rasqal_rowsource_groupby_test_CPPFLAGS = -DSTANDALONE
rasqal_rowsource_groupby_test_LDADD = librasqal.la

rasqal_rowsource_union_test_SOURCES = rasqal_rowsource_union.c
rasqal_rowsource_union_test_CPPFLAGS = -DSTANDALONE
rasqal_rowsource_union_test_LDADD = librasqal.la

rasqal_rowsource_rowsequence_test_SOURCES = rasqal_rowsource_rowsequence.c
rasqal_rowsource_rowsequence_test_CPPFLAGS = -DSTANDALONE
rasqal_rowsource_rowsequence_test_LDADD = librasqal.la

rasqal_rowsource_triples_test_SOURCES = rasqal_rowsource_triples.c
rasqal_rowsource_triples_test_CPPFLAGS = -DSTANDALONE
rasqal_rowsource_triples_test_LDADD = librasqal.la

rasqal_rowsource_project_test_SOURCES = rasqal_rowsource_project.c
rasqal_rowsource_project_test_CPPFLAGS = -DSTANDALONE
rasqal_rowsource_project_test_LDADD = librasqal.la

rasqal_rowsource_join_test_SOURCES = rasqal_rowsource_join.c
rasqal_rowsource_join_test_CPPFLAGS = -DSTANDALONE
rasqal_rowsource_join_test_LDADD = librasqal.la

rasqal_rowsource_service_test_SOURCES = rasqal_rowsource_service.c
rasqal_rowsource_service_test_CPPFLAGS = -DSTANDALONE
rasqal_rowsource_service_test_LDADD = librasqal.la

rasqal_row_compatible_test_SOURCES = rasqal_row_compatible.c
rasqal_row_compatible_test_CPPFLAGS = -DSTANDALONE
rasqal_row_compatible_test_LDADD = librasqal.la

rasqal_literal_test_SOURCES = rasqal_literal.c
rasqal_literal_test_CPPFLAGS = -DSTANDALONE
rasqal_literal_test_LDADD = librasqal.la

rasqal_regex_test_SOURCES = rasqal_regex.c
rasqal_regex_test_CPPFLAGS = -DSTANDALONE
rasqal_regex_test_LDADD = librasqal.la

rasqal_random_test_SOURCES = rasqal_random.c
rasqal_random_test_CPPFLAGS = -DSTANDALONE
rasqal_random_test_LDADD = librasqal.la

rasqal_xsd_datatypes_test_SOURCES = rasqal_xsd_datatypes.c
rasqal_xsd_datatypes_test_CPPFLAGS = -DSTANDALONE
rasqal_xsd_datatypes_test_LDADD = librasqal.la

rasqal_results_compare_test_SOURCES = rasqal_results_compare.c
rasqal_results_compare_test_CPPFLAGS = -DSTANDALONE
rasqal_results_compare_test_LDADD = librasqal.la

rasqal_query_results_test_SOURCES = rasqal_query_results.c
rasqal_query_results_test_CPPFLAGS = -DSTANDALONE
rasqal_query_results_test_LDADD = librasqal.la

$(top_builddir)/../raptor/src/libraptor.la:
	cd $(top_builddir)/../raptor/src && $(MAKE) $(AM_MAKEFLAGS) libraptor.la


if MAINTAINER_MODE
git-version.h: check-version

# Always run this in maintainer mode but do not always change git-version.h
.PHONY: check-version
check-version:
	@file="git-version.h" ; \
	if test -d ../.git; then \
	  git_version=`cd .. && git rev-parse HEAD`; \
	else \
	  git_version="unknown"; \
	fi; \
	$(RECHO) "GIT version $$git_version"; \
	tmp="$$file$$$$.tmp"; \
	$(RECHO) "#define GIT_VERSION \"$$git_version\"" > $$tmp; \
        if test -f $$file; then \
          if cmp $$file $$tmp >/dev/null 2>&1; then \
	    rm $$tmp; \
	  else \
	    mv $$tmp $$file; \
          fi; \
	else \
	  mv $$tmp $$file; \
	fi

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

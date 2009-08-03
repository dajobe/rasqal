# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal RDQL tests
#
# Copyright (C) 2004-2008, David Beckett http://www.dajobe.org/
# Copyright (C) 2004-2004, University of Bristol, UK http://www.bristol.ac.uk/
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

SUBDIRS=testsuite

RDQL_TEST_FILES= \
example1.rdql example2.rdql example3.rdql example4.rdql example5.rdql \
example_at_1.rdql example_at_2.rdql example_at_3.rdql example_at_4.rdql \
example_at_5.rdql example_at_6.rdql example_at_7.rdql \
note-ex1.rdql note-ex2.rdql note-ex3.rdql note-ex4.rdql


EXTRA_DIST = \
$(RDQL_TEST_FILES) \
dc.rdf

build-rdql-lexer-test:
	@(cd $(top_builddir)/src ; $(MAKE) rdql_lexer_test)

build-rdql-parser-test:
	@(cd $(top_builddir)/src ; $(MAKE) rdql_parser_test)

check-local: check-rdql-lexer check-rdql-parser

check-rdql-lexer: build-rdql-lexer-test
	@$(PERL) $(srcdir)/../improve $(srcdir) rdql-lexer

check-rdql-parser: build-rdql-parser-test
	@$(PERL) $(srcdir)/../improve $(srcdir) rdql-parser

get-testsuites-list:
	@echo "rdql-lexer rdql-parser"

get-testsuite-rdql-lexer get-testsuite-rdql-parser:
	@tests="$(RDQL_TEST_FILES)"; \
	type=`echo $@ | sed -e 's/^.*rdql-//' -e 's/er$$//'`; \
	verb=`echo $$type`ing; \
	prog=rdql_`echo $$type`er_test; \
	$(ECHO) '@prefix rdfs:	<http://www.w3.org/2000/01/rdf-schema#> .'; \
	$(ECHO) '@prefix mf:     <http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#> .'; \
	$(ECHO) '@prefix t:     <http://ns.librdf.org/2009/test-manifest#> .'; \
	$(ECHO) ' '; \
	$(ECHO) "<> a mf:Manifest; rdfs:comment \"RDQL legal $$verb\"; mf:entries ("; \
	for test in $$tests; do \
	  comment="rdql $$type of $$test"; \
	  $(ECHO) "  [ a t:PositiveTest; mf:name \"$$test\"; rdfs:comment \"$$comment\"; mf:action  \"$(top_builddir)/src/$$prog $(srcdir)/$$test\" ]"; \
	done; \
	$(ECHO) ")."

# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal tests
#
# $Id$
#
# Copyright (C) 2004 David Beckett - http://purl.org/net/dajobe/
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

RDQL_TEST_FILES= \
example1.rdql example2.rdql example3.rdql example4.rdql example5.rdql \
example_at_1.rdql example_at_2.rdql example_at_3.rdql example_at_4.rdql \
example_at_5.rdql example_at_6.rdql example_at_7.rdql example_at_8.rdql \
note-ex1.rdql note-ex2.rdql note-ex3.rdql note-ex4.rdql


EXTRA_DIST = \
$(RDQL_TEST_FILES) \
dc.rdf

$(top_builddir)/rdql_lexer_test:
	@(cd $(top_builddir) ; $(MAKE) rdql_lexer_test)

$(top_builddir)/rdql_parser_test:
	@(cd $(top_builddir) ; $(MAKE) rdql_parser_test)

check-local: check-rdql-lexer check-rdql-parser

check-rdql-lexer: $(top_builddir)/rdql_lexer_test
	@set +e; result=0; \
	$(ECHO) "Testing legal RDQL lexing"; \
	for test in $(RDQL_TEST_FILES); do \
	  $(ECHO) $(ECHO_N) "Checking $$test $(ECHO_C)"; \
	  $(top_builddir)/rdql_lexer_test $(srcdir)/$$test >/dev/null; \
	  status=$$?; \
	  if test $$status != 0 -a $$status != 2 ; then \
	    $(ECHO) FAILED returned status $$status; result=1; \
	  else \
	    $(ECHO) ok; \
	  fi; \
	done; \
	set -e; exit $$result

check-rdql-parser: $(top_builddir)/rdql_parser_test
	@set +e; result=0; \
	$(ECHO) "Testing legal RDQL parsing"; \
	for test in $(RDQL_TEST_FILES); do \
	  $(ECHO) $(ECHO_N) "Checking $$test $(ECHO_C)"; \
	  $(top_builddir)/rdql_parser_test $(srcdir)/$$test >/dev/null; \
	  status=$$?; \
	  if test $$status != 0 -a $$status != 2 ; then \
	    $(ECHO) FAILED returned status $$status; result=1; \
	  else \
	    $(ECHO) ok; \
	  fi; \
	done; \
	set -e; exit $$result

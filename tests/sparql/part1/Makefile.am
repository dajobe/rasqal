# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal SPARQL tests
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

SPARQL_MANIFEST_FILES= manifest.n3

SPARQL_MODEL_FILES= \
dawg-data-01.n3

SPARQL_TEST_FILES= \
dawg-query-001 dawg-query-002 dawg-query-003

EXPECTED_SPARQL_CORRECT_FAILURES_COUNT=3

SPARQL_RESULT_FILES= \
dawg-result-001.n3 dawg-result-002.n3 dawg-result-003.n3

EXTRA_DIST= \
$(SPARQL_MANIFEST_FILES) \
$(SPARQL_MODEL_FILES) \
$(SPARQL_TEST_FILES) \
$(SPARQL_RESULT_FILES)

$(top_builddir)/roqet:
	@(cd $(top_builddir) ; $(MAKE) roqet)

$(top_builddir)/sparql_lexer_test:
	@(cd $(top_builddir) ; $(MAKE) sparql_lexer_test)

$(top_builddir)/sparql_parser_test:
	@(cd $(top_builddir) ; $(MAKE) sparql_parser_test)


check-local: check-sparql-lexer check-sparql-parser \
check-sparql-works check-sparql-correct

check-sparql-lexer: $(top_builddir)/sparql_lexer_test
	@set +e; result=0; \
	$(ECHO) "Testing legal SPARQL lexing"; \
	for test in $(SPARQL_TEST_FILES); do \
	  $(ECHO) $(ECHO_N) "Checking $$test $(ECHO_C)"; \
	  $(top_builddir)/sparql_lexer_test $(srcdir)/$$test >/dev/null; \
	  status=$$?; \
	  if test $$status != 0 -a $$status != 2 ; then \
	    $(ECHO) FAILED returned status $$status; result=1; \
	  else \
	    $(ECHO) ok; \
	  fi; \
	done; \
	set -e; exit $$result

check-sparql-parser: $(top_builddir)/sparql_parser_test
	@set +e; result=0; \
	$(ECHO) "Testing legal SPARQL parsing"; \
	for test in $(SPARQL_TEST_FILES); do \
	  $(ECHO) $(ECHO_N) "Checking $$test $(ECHO_C)"; \
	  $(top_builddir)/sparql_parser_test $(srcdir)/$$test >/dev/null; \
	  status=$$?; \
	  if test $$status != 0 -a $$status != 2 ; then \
	    $(ECHO) FAILED returned status $$status; result=1; \
	    fails="$$fails $$test"; \
	  else \
	    $(ECHO) ok; \
	  fi; \
	done; \
	set -e; exit $$result

check-sparql-works: $(top_builddir)/roqet
	@set +e; result=0; \
	$(ECHO) "Testing SPARQL works"; \
	for test in $(SPARQL_TEST_FILES); do \
	  $(ECHO) $(ECHO_N) "Checking $$test $(ECHO_C)"; \
	  $(top_builddir)/roqet -i sparql -s dawg-data-01.n3 $(srcdir)/$$test >/dev/null 2>&1; \
	  status=$$?; \
	  if test $$status != 0 -a $$status != 2 ; then \
	    $(ECHO) FAILED returned status $$status; result=1; \
	    fails="$$fails $$test"; \
	  else \
	    $(ECHO) ok; \
	  fi; \
	done; \
	set -e; exit $$result

check-sparql-correct: $(top_builddir)/roqet $(srcdir)/../check-sparql
	@set +e; \
	$(ECHO) "Testing SPARQL correct"; \
	TOP_SRCDIR=$(top_srcdir) TOP_BUILDDIR=$(top_builddir) \
	  perl $(srcdir)/../check-sparql; \
	result=$$?; \
	if test $$result != 0 ; then \
	  echo "Expecting test failures: $(EXPECTED_SPARQL_CORRECT_FAILURES)"; \
	  if test $$result = $(EXPECTED_SPARQL_CORRECT_FAILURES_COUNT) ; then \
	    echo "OK: Ignoring expected test failures"; result=0; \
          else \
	    echo "FAILED: $$result failures does not match expected $(EXPECTED_SPARQL_CORRECT_FAILURES_COUNT)"; result=1; \
          fi; \
	fi; \
	set -e; exit $$result

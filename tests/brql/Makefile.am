# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal BRQL tests
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

BRQL_MODEL_FILES= D.n3 E.n3

BRQL_TEST_FILES= \
Q-C-1 Q-C-2 Q-D-1 Q-D-2 Q-D-3 Q-OPT-1 Q-OPT-2 Q-S-1 Q-S-2 \
Q-SOURCE-1 Q-SOURCE-2

EXPECTED_BRQL_TEST_FAILURES=Q-D-3

BRQL_RESULT_FILES=

EXTRA_DIST= \
$(BRQL_MODEL_FILES) \
$(BRQL_TEST_FILES) \
$(BRQL_RESULT_FILES)

$(top_builddir)/roqet:
	@(cd $(top_builddir) ; $(MAKE) roqet)

$(top_builddir)/brql_lexer_test:
	@(cd $(top_builddir) ; $(MAKE) brql_lexer_test)

$(top_builddir)/brql_parser_test:
	@(cd $(top_builddir) ; $(MAKE) brql_parser_test)


check-local: check-brql-lexer check-brql-parser

check-brql-lexer: $(top_builddir)/brql_lexer_test
	@set +e; result=0; \
	$(ECHO) "Testing legal BRQL lexing"; \
	for test in $(BRQL_TEST_FILES); do \
	  $(ECHO) $(ECHO_N) "Checking $$test $(ECHO_C)"; \
	  $(top_builddir)/brql_lexer_test $(srcdir)/$$test >/dev/null; \
	  status=$$?; \
	  if test $$status != 0 -a $$status != 2 ; then \
	    $(ECHO) FAILED returned status $$status; result=1; \
	  else \
	    $(ECHO) ok; \
	  fi; \
	done; \
	set -e; exit $$result

check-brql-parser: $(top_builddir)/brql_parser_test
	@set +e; result=0; \
	$(ECHO) "Testing legal BRQL parsing"; \
	for test in $(BRQL_TEST_FILES); do \
	  $(ECHO) $(ECHO_N) "Checking $$test $(ECHO_C)"; \
	  $(top_builddir)/brql_parser_test $(srcdir)/$$test >/dev/null; \
	  status=$$?; \
	  if test $$status != 0 -a $$status != 2 ; then \
	    $(ECHO) FAILED returned status $$status; result=1; \
	    fails="$$fails $$test"; \
	  else \
	    $(ECHO) ok; \
	  fi; \
	done; \
	if test $$result = 1 ; then \
	  echo "Expecting test failures: $(EXPECTED_BRQL_TEST_FAILURES)"; \
	  if test "X$$fails" = "X $(EXPECTED_BRQL_TEST_FAILURES)" ; then \
	    echo "OK: Ignoring expected test failures"; result=0; \
          else \
	    echo "FAILED: failures $$fails does not match expected"; result=1; \
          fi; \
	fi; \
	set -e; exit $$result

check-brql-works: $(top_builddir)/roqet
	@set +e; result=0; \
	$(ECHO) "Testing BRQL"; \
	for test in $(BRQL_TEST_FILES); do \
	  $(ECHO) $(ECHO_N) "Checking $$test $(ECHO_C)"; \
	  $(top_builddir)/roqet -i brql $(srcdir)/$$test >/dev/null 2>&1; \
	  status=$$?; \
	  if test $$status != 0 -a $$status != 2 ; then \
	    $(ECHO) FAILED returned status $$status; result=1; \
	    fails="$$fails $$test"; \
	  else \
	    $(ECHO) ok; \
	  fi; \
	done; \
	set -e; exit $$result

# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal SPARQL Compare tests
#
# Copyright (C) 2004-2009, David Beckett http://www.dajobe.org/
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

EXTRA_DIST= \
null-both.xml \
null-vs-value.xml \
null-vs-different-value.xml \
multiple-null.xml \
simple-null.xml \
two-identical-rows.xml \
README.md

compare_clean_files = \
compare-test-1.out compare-test-2.out compare-test-3.out compare-test-4.out \
compare-test-5.out compare-test-6.out compare-test-7.out

CLEANFILES=diff.out roqet.err roqet.out roqet.tmp \
$(compare_clean_files)

.PHONY: build-core build-rasqal-compare check-local check all \
  compare-test-1 compare-test-2 compare-test-3 compare-test-4 \
  compare-test-5 compare-test-6 compare-test-7 compare-test-8 \
  compare-tests

# This sequence allows parallel building of build-core and then once
# that is built, parallel building of the dependent programs
build-core:
	@(cd $(top_builddir)/src ; $(MAKE))
	@(cd $(top_builddir)/utils ; $(MAKE))

build-rasqal-compare: build-core
	@(cd $(top_builddir)/utils ; $(MAKE) rasqal-compare)

all: build-rasqal-compare

#
# Individual tests (so failures are easier to diagnose)
#
compare-test-1: build-rasqal-compare
	@echo "Test 1: NULL vs NULL (should be equal)..."
	@$(top_builddir)/utils/rasqal-compare -R xml -e $(srcdir)/null-both.xml -a $(srcdir)/null-both.xml > $@.out 2>&1 \
	  && echo "PASS" \
	  || (echo "FAIL: NULL vs NULL should be equal" && exit 1)

compare-test-2: build-rasqal-compare
	@echo "Test 2: NULL vs non-NULL (should be different)..."
	@$(top_builddir)/utils/rasqal-compare -R xml -e $(srcdir)/null-both.xml -a $(srcdir)/null-vs-value.xml > $@.out 2>&1 \
	  && (echo "FAIL: NULL vs non-NULL should be different" && exit 1) \
	  || echo "PASS"

compare-test-3: build-rasqal-compare
	@echo "Test 3: non-NULL vs NULL (should be different)..."
	@$(top_builddir)/utils/rasqal-compare -R xml -e $(srcdir)/null-vs-value.xml -a $(srcdir)/null-both.xml > $@.out 2>&1 \
	  && (echo "FAIL: non-NULL vs NULL should be different" && exit 1) \
	  || echo "PASS"

compare-test-4: build-rasqal-compare
	@echo "Test 4: different non-NULL values (should be different)..."
	@$(top_builddir)/utils/rasqal-compare -R xml -e $(srcdir)/null-vs-value.xml -a $(srcdir)/null-vs-different-value.xml > $@.out 2>&1 \
	  && (echo "FAIL: different non-NULL values should be different" && exit 1) \
	  || echo "PASS"

compare-test-5: build-rasqal-compare
	@echo "Test 5: NULL vs non-NULL debug output..."
	@$(top_builddir)/utils/rasqal-compare -R xml -k -e $(srcdir)/null-both.xml -a $(srcdir)/null-vs-value.xml > $@.out 2>&1 ; \
	  if grep -E -q "expected: o='NULL'.*actual: o='(non-NULL|[^']+)'" $@.out; then \
	    echo "PASS"; \
	  else \
	    echo "FAIL: Expected debug output to include expected: o='NULL' and actual: value"; \
	    exit 1; \
	  fi

compare-test-6: build-rasqal-compare
	@echo "Test 6: JSON output format..."
	@$(top_builddir)/utils/rasqal-compare -R xml -j -e $(srcdir)/null-both.xml -a $(srcdir)/null-vs-value.xml > $@.out 2>&1 ; \
	  if grep -F -q '"expected": "o='\''NULL'\''"' $@.out && \
	     grep -E -q '"actual": "o='\''(non-NULL|[^'\'']+)'\''"' $@.out; then \
	    echo "PASS"; \
	  else \
	    echo "FAIL: Expected JSON fields with expected o='NULL' and actual value"; \
	    exit 1; \
	  fi

compare-test-7: build-rasqal-compare
	@echo "Test 7: XML output format..."
	@$(top_builddir)/utils/rasqal-compare -R xml -x -e $(srcdir)/null-both.xml -a $(srcdir)/null-vs-value.xml > $@.out 2>&1 ; \
	  if grep -F -q "<expected>o='NULL'</expected>" $@.out && \
	     grep -E -q "<actual>o='(non-NULL|[^']+)'</actual>" $@.out; then \
	    echo "PASS"; \
	  else \
	    echo "FAIL: Expected XML fields with <expected>o='NULL'</expected> and actual value"; \
	    exit 1; \
	  fi

compare-test-8: build-rasqal-compare
	@echo "Test 8: Unified diff output format..."
	@$(top_builddir)/utils/rasqal-compare -R xml -u -e $(srcdir)/null-both.xml -a $(srcdir)/null-vs-value.xml 2>&1 | grep -q "o='NULL'" \
	  && echo "PASS" \
	  || (echo "FAIL: Expected unified diff output with NULL reference" && exit 1)

compare-tests: \
	compare-test-1 \
	compare-test-2 \
	compare-test-3 \
	compare-test-4 \
	compare-test-5 \
	compare-test-6 \
	compare-test-7 \
	compare-test-8
	@echo "All NULL comparison tests passed!"

check-local: build-rasqal-compare
	@echo "Running NULL comparison tests..."
	@$(MAKE) $(AM_MAKEFLAGS) compare-tests && \
	rm -f $(compare_clean_files)

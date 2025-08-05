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

CLEANFILES=diff.out roqet.err roqet.out roqet.tmp result.out

.PHONY: build-core build-rasqal-compare check-local check all

# This sequence allows parallel building of build-core and then once
# that is built, parallel building of the dependent programs
build-core:
	@(cd $(top_builddir)/src ; $(MAKE))
	@(cd $(top_builddir)/utils ; $(MAKE))

build-rasqal-compare: build-core
	@(cd $(top_builddir)/utils ; $(MAKE) rasqal-compare)

all: build-rasqal-compare

check: check-local

check-local: build-rasqal-compare
	@echo "Running NULL comparison tests..."
	@echo "Test 1: NULL vs NULL (should be equal)..."
	@$(top_builddir)/utils/rasqal-compare -R xml -e $(srcdir)/null-both.xml -a $(srcdir)/null-both.xml > /dev/null 2>&1 || (echo "FAIL: NULL vs NULL should be equal" && exit 1)
	@echo "PASS"
	@echo "Test 2: NULL vs non-NULL (should be different)..."
	@$(top_builddir)/utils/rasqal-compare -R xml -e $(srcdir)/null-both.xml -a $(srcdir)/null-vs-value.xml > /dev/null 2>&1 && (echo "FAIL: NULL vs non-NULL should be different" && exit 1) || echo "PASS"
	@echo "Test 3: non-NULL vs NULL (should be different)..."
	@$(top_builddir)/utils/rasqal-compare -R xml -e $(srcdir)/null-vs-value.xml -a $(srcdir)/null-both.xml > /dev/null 2>&1 && (echo "FAIL: non-NULL vs NULL should be different" && exit 1) || echo "PASS"
	@echo "Test 4: different non-NULL values (should be different)..."
	@$(top_builddir)/utils/rasqal-compare -R xml -e $(srcdir)/null-vs-value.xml -a $(srcdir)/null-vs-different-value.xml > /dev/null 2>&1 && (echo "FAIL: different non-NULL values should be different" && exit 1) || echo "PASS"
	@echo "Test 5: NULL vs non-NULL error message..."
	@$(top_builddir)/utils/rasqal-compare -R xml -k -e $(srcdir)/null-both.xml -a $(srcdir)/null-vs-value.xml 2>&1 | grep -q "o='NULL' vs row.*o='some value'" || (echo "FAIL: Expected 'o='NULL' vs row.*o='some value'' in output" && exit 1)
	@echo "PASS"
	@echo "Test 6: JSON output format..."
	@$(top_builddir)/utils/rasqal-compare -R xml -j -e $(srcdir)/null-both.xml -a $(srcdir)/null-vs-value.xml 2>&1 | grep -q '"row.*o='\''NULL'\'' vs row.*o='\''some value'\''"' || (echo "FAIL: Expected JSON output with correct description" && exit 1)
	@echo "PASS"
	@echo "Test 7: XML output format..."
	@$(top_builddir)/utils/rasqal-compare -R xml -x -e $(srcdir)/null-both.xml -a $(srcdir)/null-vs-value.xml 2>&1 | grep -q '<difference>row.*o='\''NULL'\'' vs row.*o='\''some value'\''</difference>' || (echo "FAIL: Expected XML output with correct description" && exit 1)
	@echo "PASS"
	@echo "Test 8: Unified diff output format..."
	@$(top_builddir)/utils/rasqal-compare -R xml -u -e $(srcdir)/null-both.xml -a $(srcdir)/null-vs-value.xml 2>&1 | grep -q "o='NULL'" || (echo "FAIL: Expected unified diff output with NULL reference" && exit 1)
	@echo "PASS"
	@echo "All NULL comparison tests passed!" 
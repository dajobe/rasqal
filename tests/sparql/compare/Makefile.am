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
# licenses in COPYING.LIB, COPYING and LICENSE-2.0.txt respectively.
# 

# Use Automake test harness with shell test scripts
TEST_EXTENSIONS = .test

# Export path to rasqal-compare for tests; rely on top-level build order
AM_TESTS_ENVIRONMENT = \
  RASQAL_COMPARE='$(top_builddir)/utils/rasqal-compare'; \
  export RASQAL_COMPARE;

# Test programs
TESTS = \
  compare-test-1.test \
  compare-test-2.test \
  compare-test-3.test \
  compare-test-4.test \
  compare-test-5.test \
  compare-test-6.test \
  compare-test-7.test \
  compare-test-8.test

# Test data and scripts to distribute
EXTRA_DIST = \
  compare-test-1.test \
  compare-test-2.test \
  compare-test-3.test \
  compare-test-4.test \
  compare-test-5.test \
  compare-test-6.test \
  compare-test-7.test \
  compare-test-8.test \
  test-common.sh \
  null-both.xml \
  null-vs-value.xml \
  null-vs-different-value.xml \
  multiple-null.xml \
  simple-null.xml \
  two-identical-rows.xml \
  README.md

# Clean up test output files
CLEANFILES = \
  diff.out \
  roqet.err \
  roqet.out \
  roqet.tmp \
  *.log \
  *.trs

# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal warnings tests
#
# Copyright (C) 2011, David Beckett http://www.dajobe.org/
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

SPARQL_MANIFEST_FILES= manifest.ttl

SPARQL_MODEL_FILES= \
empty.nt

SPARQL_TEST_FILES= \
warning-1.rq warning-2.rq warning-3.rq

SPARQL_TEST_NAMES= \
$(SPARQL_TEST_FILES)

EXPECTED_SPARQL_WARNINGS= $(SPARQL_TEST_NAMES)

SPARQL_RESULT_FILES=

EXTRA_DIST= \
$(SPARQL_MANIFEST_FILES) \
$(SPARQL_MODEL_FILES) \
$(SPARQL_TEST_FILES) \
$(SPARQL_RESULT_FILES)

CLEANFILES=diff.out roqet.err roqet.out roqet.tmp result.out

.PHONY: build-core build-roqet check-lcoal

# This sequence allows parallel building of build-core and then once
# that is built, parallel building of the dependent programs: roqet

build-core:
	@(cd $(top_builddir)/src ; $(MAKE))

build-roqet: build-core
	@(cd $(top_builddir)/utils ; $(MAKE) roqet)

check-local: build-roqet
	@$(PYTHON) $(srcdir)/../../improve.py .

get-testsuites-list:
	@echo "sparql-warnings"

get-testsuite-sparql-warnings:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(srcdir)/../generate_sparql_plan.py \
		--suite-name sparql-warnings \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type NegativeTest \
		--action-template "$(PYTHON) $(srcdir)/../check_sparql.py -s $(srcdir) -W 100 {test_id}" \
		--comment-template "sparql warning {test_id}" \
		--manifest-file $(srcdir)/manifest.ttl

dist-hook:
	$(PYTHON3) $(srcdir)/../package_sparql_tests.py \
		--manifest-file $(srcdir)/$(SPARQL_MANIFEST_FILES) \
		--srcdir $(srcdir) \
		--distdir $(distdir)

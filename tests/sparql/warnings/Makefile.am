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

# Include failing tests in distribution but don't run them until scope-aware warning detection is implemented
EXTRA_DIST= \
$(SPARQL_MANIFEST_FILES) \
warning-1.rq warning-1.out warning-2.rq warning-2.out \
README.md

CLEANFILES=diff.out roqet.err roqet.out roqet.tmp result.out \
	sparql-warnings-plan.ttl

.PHONY: build-core build-roqet check-local

# This sequence allows parallel building of build-core and then once
# that is built, parallel building of the dependent programs: roqet

build-core:
	@(cd $(top_builddir)/src ; $(MAKE))

build-roqet: build-core
	@(cd $(top_builddir)/utils ; $(MAKE) roqet)

check-local: build-roqet
	@$(PYTHON3) $(top_srcdir)/tests/bin/run-test-suites .

get-testsuites-list:
	@echo "sparql-warnings"

get-testsuite-sparql-warnings:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(top_srcdir)/tests/bin/create-test-plan \
		--suite-name sparql-warnings \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type WarningTest \
		--action-template "$(PYTHON3) $(top_srcdir)/tests/bin/run-sparql-tests -s {srcdir} --manifest-file {srcdir}/manifest.ttl -W 100 --test-case {test_id}" \
		--comment-template "sparql warning --test-case {test_id}" \
		--manifest-file $(srcdir)/manifest.ttl

dist-hook:
	$(PYTHON3) $(top_srcdir)/tests/bin/bundle-test-files \
		--manifest-file $(srcdir)/manifest.ttl \
		--srcdir $(srcdir) \
		--distdir $(distdir)

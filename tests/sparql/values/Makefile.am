# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal VALUES tests
#
# Copyright (C) 2025, David Beckett https://www.dajobe.org/
#
# This package is Free Software and part of Redland https://librdf.org/
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

EXTRA_DIST= \
$(SPARQL_MANIFEST_FILES) \
README.md

CLEANFILES=diff.out roqet.err roqet.out roqet.tmp result.out \
	sparql-query-plan.ttl sparql-query-negative-plan.ttl

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
	@echo "sparql-query"

get-testsuite-sparql-query:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(top_srcdir)/tests/bin/create-test-plan \
		--suite-name sparql-query \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type PositiveTest \
		--action-template "$(PYTHON3) $(top_srcdir)/tests/bin/run-sparql-tests -s $(top_srcdir)/tests/sparql/sparql11-test-suite/bindings --manifest-file {srcdir}/manifest.ttl --test-case {test_id}" \
		--comment-template "W3C SPARQL 1.1 VALUES tests --test-case {test_id}" \
		--manifest-file $(srcdir)/manifest.ttl

dist-hook:
	$(PYTHON3) $(top_srcdir)/tests/bin/bundle-test-files \
		--manifest-file $(srcdir)/manifest.ttl \
		--srcdir $(srcdir) \
		--distdir $(distdir) 
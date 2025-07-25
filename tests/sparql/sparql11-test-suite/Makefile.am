# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal SPARQL 1.1 test suite
#
# Copyright (C) 2025, David Beckett http://www.dajobe.org/
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

# Main manifest files for SPARQL 1.1 test categories
SPARQL11_QUERY_MANIFEST_FILE = manifest-sparql11-query.ttl
SPARQL11_UPDATE_MANIFEST_FILE = manifest-sparql11-update.ttl
SPARQL11_RESULTS_MANIFEST_FILE = manifest-sparql11-results.ttl
SPARQL11_FEDERATED_MANIFEST_FILE = manifest-sparql11-fed.ttl
SPARQL11_ROOT_MANIFEST_FILE = manifest.ttl

EXTRA_DIST = \
	$(SPARQL11_ROOT_MANIFEST_FILE) \
	$(SPARQL11_QUERY_MANIFEST_FILE) \
	$(SPARQL11_UPDATE_MANIFEST_FILE) \
	$(SPARQL11_RESULTS_MANIFEST_FILE) \
	$(SPARQL11_FEDERATED_MANIFEST_FILE)

CLEANFILES = diff.out roqet.err roqet.out roqet.tmp result.out

.PHONY: build-core build-roqet build-sparql-test-utils check-sparql11 \
	get-testsuites-list \
	get-testsuite-sparql11-query \
	get-testsuite-sparql11-update \
	get-testsuite-sparql11-results \
	get-testsuite-sparql11-federated

# This sequence allows parallel building of build-core and then once
# that is built, parallel building of the dependent programs: roqet,
# sparql_lexer_test and sparql_parser_test
build-core:
	@(cd $(top_builddir)/src ; $(MAKE))
	@(cd $(top_builddir)/utils ; $(MAKE))

build-roqet: build-core
	@(cd $(top_builddir)/utils ; $(MAKE) roqet)

build-sparql-test-utils: build-core
	@(cd $(top_builddir)/src ; $(MAKE) sparql_lexer_test sparql_parser_test)

# Run tests locally through bin/run-test-suites
# This is NOT check-local so we do not run this by default
check-sparql11: build-roqet build-sparql-test-utils
	@echo "SPARQL 1.1 test suite - note: most tests expected to fail due to limited SPARQL 1.1 support"
	@$(top_srcdir)/tests/bin/run-test-suites .

# Register with bin/run-test-suites test discovery
get-testsuites-list:
	@echo "sparql11-query sparql11-update sparql11-results sparql11-federated"

# Generate test plan for SPARQL 1.1 Query tests
get-testsuite-sparql11-query:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(top_srcdir)/tests/bin/create-test-plan \
		--suite-name sparql11-query \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type PositiveTest \
		--action-template "$(PYTHON3) $(top_srcdir)/tests/bin/run-sparql-tests -s {srcdir} --test-case {test_id}" \
		--comment-template "SPARQL 1.1 Query test --test-case {test_id}" \
		--manifest-file $(top_srcdir)/tests/sparql/sparql11-test-suite/$(SPARQL11_QUERY_MANIFEST_FILE)

# Generate test plan for SPARQL 1.1 Update tests
get-testsuite-sparql11-update:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(top_srcdir)/tests/bin/create-test-plan \
		--suite-name sparql11-update \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type PositiveTest \
		--action-template "$(PYTHON3) $(top_srcdir)/tests/bin/run-sparql-tests -s {srcdir} --test-case {test_id}" \
		--comment-template "SPARQL 1.1 Update test --test-case {test_id}" \
		--manifest-file $(top_srcdir)/tests/sparql/sparql11-test-suite/$(SPARQL11_UPDATE_MANIFEST_FILE)

# Generate test plan for SPARQL 1.1 Results format tests
get-testsuite-sparql11-results:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(top_srcdir)/tests/bin/create-test-plan \
		--suite-name sparql11-results \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type PositiveTest \
		--action-template "$(PYTHON3) $(top_srcdir)/tests/bin/run-sparql-tests -s {srcdir} --test-case {test_id}" \
		--comment-template "SPARQL 1.1 Results format test --test-case {test_id}" \
		--manifest-file $(top_srcdir)/tests/sparql/sparql11-test-suite/$(SPARQL11_RESULTS_MANIFEST_FILE)

# Generate test plan for SPARQL 1.1 Federated tests
get-testsuite-sparql11-federated:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(top_srcdir)/tests/bin/create-test-plan \
		--suite-name sparql11-federated \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type PositiveTest \
		--action-template "$(PYTHON3) $(top_srcdir)/tests/bin/run-sparql-tests -s {srcdir} --test-case {test_id}" \
		--comment-template "SPARQL 1.1 Federated test --test-case {test_id}" \
		--manifest-file $(top_srcdir)/tests/sparql/sparql11-test-suite/$(SPARQL11_FEDERATED_MANIFEST_FILE)

# Distribution support
dist-hook:
	$(PYTHON3) $(top_srcdir)/tests/bin/bundle-test-files \
		--manifest-file $(srcdir)/$(SPARQL11_ROOT_MANIFEST_FILE) \
		--manifest-file $(srcdir)/$(SPARQL11_QUERY_MANIFEST_FILE) \
		--manifest-file $(srcdir)/$(SPARQL11_UPDATE_MANIFEST_FILE) \
		--manifest-file $(srcdir)/$(SPARQL11_RESULTS_MANIFEST_FILE) \
		--manifest-file $(srcdir)/$(SPARQL11_FEDERATED_MANIFEST_FILE) \
		--srcdir $(srcdir) \
		--distdir $(distdir) 

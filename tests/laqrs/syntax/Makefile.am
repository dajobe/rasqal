# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal syntax LAQRS tests
#
# Copyright (C) 2007-2010, David Beckett http://www.dajobe.org/
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

MANIFEST_FILE= manifest.ttl

EXTRA_DIST = \
    $(MANIFEST_FILE)

build-sparql-parser-test:
	@(cd $(top_builddir)/src ; $(MAKE) sparql_parser_test)

check-local: build-sparql-parser-test
	@$(PYTHON) $(srcdir)/../../improve.py .

get-testsuites-list:
	@echo "laqrs-parse-good laqrs-parse-bad"

get-testsuite-laqrs-parse-good:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(srcdir)/../../sparql/generate_sparql_plan.py \
		--suite-name laqrs-parse-good \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type PositiveTest \
		--action-template "$(top_builddir)/src/sparql_parser_test -i laqrs {srcdir}/{query_file}" \
		--comment-template "laqrs parsing of {query_file}" \
		--manifest-file $(srcdir)/$(MANIFEST_FILE)

get-testsuite-laqrs-parse-bad:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(srcdir)/../../sparql/generate_sparql_plan.py \
		--suite-name laqrs-parse-bad \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type NegativeTest \
		--action-template "$(top_builddir)/src/sparql_parser_test -i laqrs {srcdir}/{query_file}" \
		--comment-template "laqrs failing to parse of {query_file}" \
		--manifest-file $(srcdir)/manifest-bad.ttl

dist-hook:
	$(PYTHON3) $(srcdir)/../../sparql/package_sparql_tests.py \
		--manifest-file $(srcdir)/$(MANIFEST_FILE) \
		--srcdir $(srcdir) \
		--distdir $(distdir)


# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal SPARQL tests
#
# Copyright (C) 2005-2008, David Beckett http://www.dajobe.org/
# Copyright (C) 2005-2005, University of Bristol, UK http://www.bristol.ac.uk/
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

EXTRA_DIST= \
$(SPARQL_MANIFEST_FILES)

CLEANFILES=diff.out roqet.err roqet.out roqet.tmp result.out

build-roqet:
	@(cd $(top_builddir)/utils ; $(MAKE) roqet)

build-sparql-lexer-test:
	@(cd $(top_builddir)/src ; $(MAKE) sparql_lexer_test)

build-sparql-parser-test:
	@(cd $(top_builddir)/src ; $(MAKE) sparql_parser_test)

check-local: build-roqet build-sparql-lexer-test build-sparql-parser-test
	@$(PYTHON) $(srcdir)/../../../improve.py .

get-testsuites-list:
	@echo "sparql-lexer sparql-parser sparql-query"

get-testsuite-sparql-lexer:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(srcdir)/../../generate_sparql_plan.py \
		--suite-name sparql-lexer \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type PositiveTest \
		--action-template "$(top_builddir)/src/sparql_lexer_test {srcdir}/{query_file}" \
		--comment-template "sparql lexer of {query_file}" \
		--manifest-file $(srcdir)/manifest.ttl

get-testsuite-sparql-parser:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(srcdir)/../../generate_sparql_plan.py \
		--suite-name sparql-parser \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type PositiveTest \
		--action-template "$(top_builddir)/src/sparql_parser_test {srcdir}/{query_file}" \
		--comment-template "sparql parser of {query_file}" \
		--manifest-file $(srcdir)/manifest.ttl

get-testsuite-sparql-query:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(srcdir)/../../generate_sparql_plan.py \
		--suite-name sparql-query \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type PositiveTest \
		--action-template "$(PYTHON) $(srcdir)/../../check_sparql.py -s {srcdir} {test_id}" \
		--comment-template "sparql query {test_id}" \
		--manifest-file $(srcdir)/manifest.ttl

dist-hook:
	$(PYTHON3) $(srcdir)/../../package_sparql_tests.py \
		--manifest-file $(srcdir)/$(SPARQL_MANIFEST_FILES) \
		--srcdir $(srcdir) \
		--distdir $(distdir)

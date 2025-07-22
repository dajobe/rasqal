# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal SPARQL SRJ tests
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

SPARQL_MANIFEST_FILE= manifest.n3
SPARQL_WRITER_MANIFEST_FILE= manifest-writer.ttl

EXTRA_DIST= \
$(SPARQL_MANIFEST_FILE) \
$(SPARQL_WRITER_MANIFEST_FILE) \
check_format_srj.py

CLEANFILES=diff.out roqet.err roqet.out roqet.tmp result.out

.PHONY: build-core build-roqet build-sparql-test-utils check-local get-testsuites-list get-testsuite-format-srj-read get-testsuite-format-srj-write


# This sequence allows parallel building of build-core and then once
# that is built, parallel building of the dependent programs: roqet,
# sparql_lexer_test and sparql_parser_test
build-core:
	@(cd $(top_builddir)/src ; $(MAKE))

build-roqet: build-core
	@(cd $(top_builddir)/utils ; $(MAKE) roqet)

build-sparql-test-utils: build-core
	@(cd $(top_builddir)/src ; $(MAKE) sparql_lexer_test sparql_parser_test)

check-local: build-roqet build-sparql-test-utils
if RASQAL_RESULT_FORMAT_SRJ
	@$(srcdir)/../../improve.py .
else
	@echo "SRJ tests skipped - SRJ format support not available"
endif

get-testsuites-list:
	@echo "format-srj-read format-srj-write"

get-testsuite-format-srj-read:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(srcdir)/../generate_sparql_plan.py \
		--suite-name format-srj-read \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type PositiveTest \
		--action-template "$(PYTHON3) $(top_srcdir)/tests/sparql/check_sparql.py -s $(srcdir) {test_id}" \
		--manifest-file $(srcdir)/$(SPARQL_MANIFEST_FILE)

get-testsuite-format-srj-write:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(srcdir)/../generate_sparql_plan.py \
		--suite-name format-srj-write \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type PositiveTest \
		--action-template "$(PYTHON3) $(srcdir)/check_format_srj.py --test-case {test_id} --srcdir $(srcdir)" \
		--manifest-file $(srcdir)/$(SPARQL_WRITER_MANIFEST_FILE)

dist-hook:
	$(PYTHON3) $(srcdir)/../package_sparql_tests.py \
		--manifest-file $(srcdir)/$(SPARQL_MANIFEST_FILE) \
		--srcdir $(srcdir) \
		--distdir $(distdir) 

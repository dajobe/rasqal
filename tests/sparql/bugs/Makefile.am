# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal bug tests
#
# Copyright (C) 2010, David Beckett http://www.dajobe.org/
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

EXTRA_DIST= \
$(MANIFEST_FILE)

CLEANFILES=diff.out roqet.err roqet.out roqet.tmp result.out \
	sparql-query-plan.ttl

RECHO=/bin/echo

.PHONY: build-core build-roqet check-lcoal

# This sequence allows parallel building of build-core and then once
# that is built, parallel building of the dependent programs: roqet

build-core:
	@(cd $(top_builddir)/src ; $(MAKE))

build-roqet: build-core
	@(cd $(top_builddir)/utils ; $(MAKE) roqet)

check-local: build-roqet
	@$(PYTHON) $(top_srcdir)/tests/bin/run-test-suites .

get-testsuites-list:
	@echo "sparql-query"

get-testsuite-sparql-query:
	@PYTHONPATH=$(top_srcdir)/tests $(PYTHON3) $(top_srcdir)/tests/bin/create-test-plan \
		--suite-name sparql-query \
		--srcdir $(srcdir) \
		--builddir $(top_builddir) \
		--test-type PositiveTest \
		--action-template "$(PYTHON3) $(top_srcdir)/tests/bin/run-sparql-tests -s {srcdir} --manifest-file {srcdir}/$(MANIFEST_FILE) --test-case {test_id}" \
		--manifest-file $(srcdir)/$(MANIFEST_FILE)

dist-hook:
	$(PYTHON3) $(top_srcdir)/tests/bin/bundle-test-files \
		--manifest-file $(srcdir)/$(MANIFEST_FILE) \
		--srcdir $(srcdir) \
		--distdir $(distdir)

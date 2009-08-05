# -*- Mode: Makefile -*-
#
# Makefile.am - automake file for Rasqal RDQL Query Testsuite
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

rdql_testsuite_model_files=model0.nt model1.nt model2.nt model3.nt	\
model4.nt model5.nt model6.nt model7.nt model8.n3 model9.n3 modelA.nt	\
model5.rdf vc-db-1.rdf vc-db-2.rdf vc-db-3.rdf

rdql_testsuite_test_files=test-0-01 test-0-02 test-0-03 test-0-04	\
test-1-01 test-1-02 test-1-03 test-1-04 test-1-05 test-1-06 test-1-07	\
test-1-08 test-1-09 test-1-10 test-2-01 test-2-02 test-2-03 test-2-04	\
test-2-05 test-2-06 test-2-07 test-2-08 test-2-09 test-2-10 test-3-01	\
test-3-02 test-3-03 test-3-04 test-3-05 test-3-06 test-3-07 test-4-01	\
test-4-02 test-4-03 test-4-04 test-4-05 test-4-06 test-4-07 test-5-01	\
test-5-02 test-5-03 test-5-04 test-6-01 test-6-02 test-6-03 test-6-04	\
test-7-01 test-7-02 test-7-03 test-7-04 test-8-01 test-8-02 test-8-03	\
test-8-04 test-8-05 test-9-01 test-9-02 test-A-01 test-A-02 test-B-01	\
test-B-02 test-B-03 test-B-04 test-B-05 test-B-06 test-B-07 test-B-08	\
test-B-09 test-B-10 test-B-11 test-B-12 test-B-13 test-B-14 test-B-15	\
test-B-16 test-B-17 test-B-18 test-B-19 test-B-20 test-S-01 test-S-02	\
test-S-03 test-S-04 test-S-05 test-T-01 test-T-02 test-T-03 test-T-04	\
test-T-05 test-T-06 test-T-07 test-T-08 test-T-09 test-T-10


rdql_testsuite_result_files=result-0-01.n3 result-0-02.n3	\
result-0-03.n3 result-0-04.n3 result-1-01.n3 result-1-02.n3	\
result-1-03.n3 result-1-04.n3 result-1-05.n3 result-1-06.n3	\
result-1-07.n3 result-1-08.n3 result-1-09.n3 result-1-10.n3	\
result-2-01.n3 result-2-02.n3 result-2-03.n3 result-2-04.n3	\
result-2-05.n3 result-2-06.n3 result-2-07.n3 result-2-08.n3	\
result-2-09.n3 result-2-10.n3 result-3-01.n3 result-3-02.n3	\
result-3-03.n3 result-3-04.n3 result-3-05.n3 result-3-06.n3	\
result-3-07.n3 result-4-01.n3 result-4-02.n3 result-4-03.n3	\
result-4-04.n3 result-4-05.n3 result-4-06.n3 result-4-07.n3	\
result-5-01.n3 result-5-02.n3 result-5-03.n3 result-5-04.n3	\
result-6-01.n3 result-6-02.n3 result-6-03.n3 result-6-04.n3	\
result-7-01.n3 result-7-02.n3 result-7-03.n3 result-7-04.n3	\
result-8-01.n3 result-8-02.n3 result-8-03.n3 result-8-04.n3	\
result-8-05.n3 result-9-01.n3 result-9-02.n3 result-A-01.n3	\
result-A-02.n3 result-B-01.n3 result-B-02.n3 result-B-03.n3	\
result-B-04.n3 result-B-05.n3 result-B-06.n3 result-B-07.n3	\
result-B-08.n3 result-B-09.n3 result-B-10.n3 result-B-11.n3	\
result-B-12.n3 result-B-13.n3 result-B-14.n3 result-B-15.n3	\
result-B-16.n3 result-B-17.n3 result-B-18.n3 result-B-19.n3	\
result-B-20.n3 result-S-01.n3 result-S-02.n3 result-S-03.n3	\
result-S-04.n3 result-S-05.n3 result-T-01.n3 result-T-02.n3	\
result-T-03.n3 result-T-04.n3 result-T-05.n3 result-T-06.n3	\
result-T-07.n3 result-T-08.n3 result-T-09.n3 result-T-10.n3


rdql_testsuite_extra_files=query-dump rdql-tests.n3	\
README-RDQL-tests.txt copyright.txt

# 1-09: No support for expanding 'prefix' in '<prefix:/r-1>'
# B-17: No support for "true" matching "true"^^xsd:boolean
EXPECTED_RDQL_CORRECT_FAILURES=1-09 B-17
EXPECTED_RDQL_CORRECT_FAILURES_COUNT=2

EXTRA_DIST = README \
check-rdql \
$(rdql_testsuite_model_files) \
$(rdql_testsuite_test_files) \
$(rdql_testsuite_result_files) \
$(rdql_testsuite_extra_files)

CLEANFILES=diff.out roqet.err roqet.out result.out

build-roqet:
	@(cd $(top_builddir)/utils ; $(MAKE) roqet)

check-local: build-roqet $(srcdir)/check-rdql
	@$(PERL) $(srcdir)/../../improve .

get-testsuites-list:
	@echo "rdql-query"

get-testsuite-rdql-query:
	@tests="$(rdql_testsuite_test_files)"; \
	$(ECHO) '@prefix rdfs:	<http://www.w3.org/2000/01/rdf-schema#> .'; \
	$(ECHO) '@prefix mf:     <http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#> .'; \
	$(ECHO) '@prefix t:     <http://ns.librdf.org/2009/test-manifest#> .'; \
	$(ECHO) ' '; \
	$(ECHO) "<> a mf:Manifest; rdfs:comment \"RDQL Query Testsuite\"; t:path \"$(top_builddir)/utils:$(top_builddir)@RAPPER_PATH@:$$PATH\"; mf:entries ("; \
	for test in $$tests; do \
	  comment="rdql query $$test"; \
	  expect="PositiveTest"; \
	  if [ $$test = "test-1-09" -o $$test = "test-B-17" ]; then \
	    expect="NegativeTest"; \
          fi; \
	  t=`echo $$test | sed -e 's/^test-//'`; \
	  $(ECHO) "  [ a t:$$expect; mf:name \"$$test\"; rdfs:comment \"$$comment\"; mf:action  \"$(PERL) $(srcdir)/check-rdql -s $(srcdir) $$t\" ]"; \
	done; \
	$(ECHO) ")."

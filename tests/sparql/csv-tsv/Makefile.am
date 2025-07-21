# tests/sparql/csv-tsv/Makefile.am
# CSV/TSV result format support tests

EXTRA_DIST = \
	README.md \
	check_csv_tsv_formats.py \
	queries/w3c-official-test.rq \
	queries/test-escaping.rq \
	test-data/w3c-test-data.ttl \
	test-data/escaping-test-data.ttl \
	test-data/boolean-test.csv \
	expected-results/w3c-official-test.csv \
	expected-results/w3c-official-test.tsv \
	expected-results/test-escaping.csv

# This sequence allows parallel building of build-core and then once
# that is built, parallel building of the dependent programs: roqet,
# sparql_lexer_test and sparql_parser_test
build-core:
	@(cd $(top_builddir)/src ; $(MAKE))

build-roqet: build-core
	@(cd $(top_builddir)/utils ; $(MAKE) roqet)

check-local: build-roqet
	@echo "Running CSV/TSV format tests..."
	@$(PYTHON) $(srcdir)/check_csv_tsv_formats.py

.PHONY: check-local

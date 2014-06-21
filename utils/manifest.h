/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * manifest.h - Rasqal RDF Query library test runner header
 *
f * Copyright (C) 2014, David Beckett http://www.dajobe.org/
 * 
 * This package is Free Software and part of Redland http://librdf.org/
 * 
 * It is licensed under the following three licenses as alternatives:
 *   1. GNU Lesser General Public License (LGPL) V2.1 or any newer version
 *   2. GNU General Public License (GPL) V2 or any newer version
 *   3. Apache License, V2.0 or any newer version
 * 
 * You may not use this file except in compliance with at least one of
 * the above three licenses.
 * 
 * See LICENSE.html or LICENSE.txt at the top of this package for the
 * complete terms and further detail along with the license texts for
 * the licenses in COPYING.LIB, COPYING and LICENSE-2.0.txt respectively.
 * 
 */

#ifndef MANIFEST_H
#define MANIFEST_H


#ifdef __cplusplus
extern "C" {
#endif


typedef enum
{
  STATE_PASS,
  STATE_FAIL,
  STATE_XFAIL,
  STATE_UXPASS,
  STATE_SKIP,
  STATE_LAST = STATE_SKIP
} manifest_test_state;


typedef enum {
  /* these are alternatives */
  FLAG_IS_QUERY     = 1, /* SPARQL query; lang="sparql10" or "sparql11" */
  FLAG_IS_UPDATE    = 2, /* SPARQL update; lang="sparql-update" */
  FLAG_IS_PROTOCOL  = 4, /* SPARQL protocol */
  FLAG_IS_SYNTAX    = 8, /* syntax test: implies no execution */

  /* these are extras */
  FLAG_LANG_SPARQL_11 = 16, /* "sparql11" else "sparql10" */
  FLAG_MUST_FAIL      = 32, /* must FAIL otherwise must PASS  */
  FLAG_HAS_ENTAILMENT_REGIME = 64,
  FLAG_RESULT_CARDINALITY_LAX = 128, /* else strict (exact match) */
  FLAG_TEST_APPROVED  = 256, /* else unapproved */
  FLAG_TEST_WITHDRAWN = 512, /* else live */
  FLAG_ENTAILMENT     = 1024, /* else does not require entailment */
} manifest_test_type_bitflags;


typedef struct
{
  rasqal_world* world;
  raptor_world* raptor_world_ptr;

  /* Namespace URIs */
  raptor_uri* rdfs_namespace_uri;
  raptor_uri* mf_namespace_uri;
  raptor_uri* t_namespace_uri;
  raptor_uri* qt_namespace_uri;
  raptor_uri* dawgt_namespace_uri;
  raptor_uri* sd_namespace_uri;

  /* URIs */
  raptor_uri* mf_Manifest_uri;
  raptor_uri* mf_entries_uri;
  raptor_uri* mf_name_uri;
  raptor_uri* mf_action_uri;
  raptor_uri* mf_result_uri;
  raptor_uri* mf_resultCardinality_uri;
  raptor_uri* rdf_type_uri;
  raptor_uri* rdf_first_uri;
  raptor_uri* rdf_rest_uri;
  raptor_uri* rdf_nil_uri;
  raptor_uri* rdfs_comment_uri;
  raptor_uri* t_path_uri;
  raptor_uri* qt_data_uri;
  raptor_uri* qt_graphData_uri;
  raptor_uri* qt_query_uri;
  raptor_uri* dawgt_approval_uri;
  raptor_uri* sd_entailmentRegime_uri;

  /* Literals */
  rasqal_literal* mf_Manifest_literal;
  rasqal_literal* mf_entries_literal;
  rasqal_literal* mf_name_literal;
  rasqal_literal* mf_action_literal;
  rasqal_literal* mf_result_literal;
  rasqal_literal* mf_resultCardinality_literal;
  rasqal_literal* rdf_type_literal;
  rasqal_literal* rdf_first_literal;
  rasqal_literal* rdf_rest_literal;
  rasqal_literal* rdfs_comment_literal;
  rasqal_literal* t_path_literal;
  rasqal_literal* qt_data_literal;
  rasqal_literal* qt_graphData_literal;
  rasqal_literal* qt_query_literal;
  rasqal_literal* dawgt_approval_literal;
  rasqal_literal* sd_entailmentRegime_literal;
} manifest_world;

  
typedef struct
{
  manifest_test_state state;
  char* details;
  char* log; /* error log */
  raptor_sequence* states[STATE_LAST + 1];
} manifest_test_result;


typedef struct
{
  manifest_world* mw;
  int usage;
  char* dir;
  rasqal_literal* test_node; /* the test node (URI or blank node) */
  char* name; /* <test-uri> mf:name ?value */
  char* desc; /* <test-uri> rdfs:comment ?value */
  manifest_test_state expect; /* derived from <test-uri> rdf:type ?value */
  raptor_uri* query; /* <test-uri> qt:query ?uri */
  raptor_sequence* data_graphs;  /* sequence of data graphs. background graph <test-uri> qt:data ?uri and named graphs <test-uri> qt:dataGraph ?uri */
  raptor_uri* expected_result; /* <test-uri> mf:result ?uri */
  unsigned int flags; /* bit flags from #manifest_test_type_bitflags */

  /* Test output */
  manifest_test_result* result;
  int error_count;
} manifest_test;


typedef struct
{
  manifest_world* mw;
  manifest_test_state state;
  char* name; /* short name */
  char* desc; /* description from ?manifest rdfs:comment ?value */
  char* dir; /* directory */
  char* path; /* for envariable PATH */
  raptor_sequence* tests; /* sequence of manifest_test */
  char* details; /* error details */
} manifest_testsuite;


/* world */
manifest_world* manifest_new_world(rasqal_world* world);
void manifest_free_world(manifest_world* mw);

/* test */
const char* manifest_test_get_query_language(manifest_test* t);

manifest_test_result* manifest_manifests_run(manifest_world* mw, raptor_sequence* manifest_uris, raptor_uri* base_uri, const char* test_string, unsigned int indent, int dryrun, int verbose, int approved);

manifest_test_result* manifest_testsuite_run_suite(manifest_testsuite* ts, unsigned int indent, int dryrun, int verbose, int approved);

/* test results */
void manifest_free_test_result(manifest_test_result* result);

#ifdef __cplusplus
}
#endif

#endif

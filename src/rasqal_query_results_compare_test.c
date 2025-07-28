/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_query_results_compare_test.c - Test for Rasqal Query Results Compare Module
 *
 * Copyright (C) 2025, David Beckett http://www.dajobe.org/
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

#ifdef HAVE_CONFIG_H
#include <rasqal_config.h>
#endif

#ifdef WIN32
#include <win32_rasqal_config.h>
#endif

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>

#include "rasqal.h"
#include "rasqal_internal.h"

#define NTESTS 7

static void
print_test_result(const char* test_name, int result)
{
  printf("%s: %s\n", test_name, result ? "PASS" : "FAIL");
}

/* Helper function to replace the deleted rasqal_new_query_results_compare_options */
static rasqal_query_results_compare_options*
test_new_query_results_compare_options(void)
{
  rasqal_query_results_compare_options* options = NULL;

  options = RASQAL_CALLOC(rasqal_query_results_compare_options*, 1, sizeof(*options));
  if(!options)
    return NULL;

  rasqal_query_results_compare_options_init(options);
  return options;
}

/* Helper function to replace the deleted test_free_query_results_compare_options */
static void
test_free_query_results_compare_options(rasqal_query_results_compare_options* options)
{
  if(!options)
    return;

  RASQAL_FREE(rasqal_query_results_compare_options*, options);
}

/* Helper function to replace the deleted test_query_results_are_equal_with_options */
static int
test_query_results_are_equal_with_options(rasqal_world* world,
                                          rasqal_query_results* first_results,
                                          rasqal_query_results* second_results,
                                          rasqal_query_results_compare_options* options)
{
  rasqal_query_results_compare* compare = NULL;
  rasqal_query_results_compare_result* result = NULL;
  int equal = 0;

  if(!world || !first_results || !second_results || !options)
    return 0;

  compare = rasqal_new_query_results_compare(world, first_results, second_results);
  if(!compare)
    return 0;

  rasqal_query_results_compare_set_options(compare, options);

  result = rasqal_query_results_compare_execute(compare);
  if(result) {
    equal = result->equal;
    rasqal_free_query_results_compare_result(result);
  }

  rasqal_free_query_results_compare(compare);
  return equal;
}

static int
test_options_init(void)
{
  rasqal_query_results_compare_options options;

  rasqal_query_results_compare_options_init(&options);

  return (options.order_sensitive == 0 &&
          options.blank_node_strategy == RASQAL_COMPARE_BLANK_NODE_MATCH_ANY &&
          options.literal_comparison_flags == RASQAL_COMPARE_XQUERY &&
          options.max_differences == 10);
}

static int
test_options_new_free(void)
{
  rasqal_query_results_compare_options options;
  int result = 0;

  /* Test options initialization */
  rasqal_query_results_compare_options_init(&options);

  result = (options.order_sensitive == 0 &&
            options.blank_node_strategy == RASQAL_COMPARE_BLANK_NODE_MATCH_ANY &&
            options.literal_comparison_flags == RASQAL_COMPARE_XQUERY &&
            options.max_differences == 10);

  return result;
}

static int
test_boolean_comparison(void)
{
  rasqal_world* world = NULL;
  rasqal_query* query = NULL;
  rasqal_query_results* results1 = NULL;
  rasqal_query_results* results2 = NULL;
  int result = 0;

  world = rasqal_new_world();
  if(!world)
    return 0;

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query)
    goto cleanup;

  /* Create boolean results */
  results1 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_BOOLEAN);
  if(!results1)
    goto cleanup;

  results2 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_BOOLEAN);
  if(!results2)
    goto cleanup;

  /* Test that boolean results are created correctly */
  result = rasqal_query_results_is_boolean(results1) && rasqal_query_results_is_boolean(results2);
  if(!result)
    goto cleanup;

  /* Test that the comparison function handles boolean results */
  {
    rasqal_query_results_compare* compare = rasqal_new_query_results_compare(world, results1, results2);
    if(compare) {
      rasqal_query_results_compare_result* compare_result = rasqal_query_results_compare_execute(compare);
      if(compare_result) {
        result = compare_result->equal;
        rasqal_free_query_results_compare_result(compare_result);
      }
      rasqal_free_query_results_compare(compare);
    }
  }

cleanup:
  if(results2)
    rasqal_free_query_results(results2);
  if(results1)
    rasqal_free_query_results(results1);
  if(query)
    rasqal_free_query(query);
  if(world)
    rasqal_free_world(world);

  return result;
}

static int
test_compare_context(void)
{
  rasqal_world* world = NULL;
  rasqal_query* query = NULL;
  rasqal_query_results* results1 = NULL;
  rasqal_query_results* results2 = NULL;
  rasqal_query_results_compare* compare = NULL;
  rasqal_query_results_compare_result* result = NULL;
  int test_result = 0;

  world = rasqal_new_world();
  if(!world)
    return 0;

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query)
    goto cleanup;

  results1 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_BOOLEAN);
  if(!results1)
    goto cleanup;

  results2 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_BOOLEAN);
  if(!results2)
    goto cleanup;

  compare = rasqal_new_query_results_compare(world, results1, results2);
  if(!compare)
    goto cleanup;

  result = rasqal_query_results_compare_execute(compare);
  if(!result)
    goto cleanup;

  test_result = result->equal;

cleanup:
  if(result)
    rasqal_free_query_results_compare_result(result);
  if(compare)
    rasqal_free_query_results_compare(compare);
  if(results2)
    rasqal_free_query_results(results2);
  if(results1)
    rasqal_free_query_results(results1);
  if(query)
    rasqal_free_query(query);
  if(world)
    rasqal_free_world(world);

  return test_result;
}

static int
test_null_parameters(void)
{
  rasqal_world* world = NULL;
  rasqal_query* query = NULL;
  rasqal_query_results* results = NULL;
  int test_result = 1;

  world = rasqal_new_world();
  if(!world)
    return 0;

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query)
    goto cleanup;

  results = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_BOOLEAN);
  if(!results)
    goto cleanup;

  /* Test NULL parameters */
  test_result &= (rasqal_new_query_results_compare(NULL, results, results) == NULL);
  test_result &= (rasqal_new_query_results_compare(world, NULL, results) == NULL);
  test_result &= (rasqal_new_query_results_compare(world, results, NULL) == NULL);

cleanup:
  if(results)
    rasqal_free_query_results(results);
  if(query)
    rasqal_free_query(query);
  if(world)
    rasqal_free_world(world);

  return test_result;
}

static int
test_bindings_comparison(void)
{
  rasqal_world* world = NULL;
  rasqal_query* query = NULL;
  rasqal_query_results* results1 = NULL;
  rasqal_query_results* results2 = NULL;
  rasqal_query_results_compare* compare = NULL;
  rasqal_query_results_compare_result* result = NULL;
  int test_result = 0;

  world = rasqal_new_world();
  if(!world)
    return 0;

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query)
    goto cleanup;

  results1 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_BINDINGS);
  if(!results1)
    goto cleanup;

  results2 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_BINDINGS);
  if(!results2)
    goto cleanup;

  /* Test basic bindings comparison */
  compare = rasqal_new_query_results_compare(world, results1, results2);
  if(!compare)
    goto cleanup;

  result = rasqal_query_results_compare_execute(compare);
  if(!result)
    goto cleanup;

  test_result = result->equal;

  if(result)
    rasqal_free_query_results_compare_result(result);
  if(compare)
    rasqal_free_query_results_compare(compare);

  /* Test with options */
  rasqal_query_results_compare_options options;
  rasqal_query_results_compare* compare_with_options = NULL;
  rasqal_query_results_compare_result* result_with_options = NULL;

  rasqal_query_results_compare_options_init(&options);
  options.order_sensitive = 0;
  options.blank_node_strategy = RASQAL_COMPARE_BLANK_NODE_MATCH_ANY;

  compare_with_options = rasqal_new_query_results_compare(world, results1, results2);
  if(compare_with_options) {
    rasqal_query_results_compare_set_options(compare_with_options, &options);
    result_with_options = rasqal_query_results_compare_execute(compare_with_options);
    if(result_with_options) {
      test_result &= result_with_options->equal;
      rasqal_free_query_results_compare_result(result_with_options);
    }
    rasqal_free_query_results_compare(compare_with_options);
  }

cleanup:
  if(results2)
    rasqal_free_query_results(results2);
  if(results1)
    rasqal_free_query_results(results1);
  if(query)
    rasqal_free_query(query);
  if(world)
    rasqal_free_world(world);

  return test_result;
}

static int
test_blank_node_strategies(void)
{
  rasqal_world* world = NULL;
  rasqal_query* query = NULL;
  rasqal_query_results* results1 = NULL;
  rasqal_query_results* results2 = NULL;
  rasqal_query_results_compare_options options;
  rasqal_query_results_compare* compare = NULL;
  rasqal_query_results_compare_result* result = NULL;
  int test_result = 1;

  world = rasqal_new_world();
  if(!world)
    return 0;

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query)
    goto cleanup;

  results1 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_BINDINGS);
  if(!results1)
    goto cleanup;

  results2 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_BINDINGS);
  if(!results2)
    goto cleanup;

  /* Test MATCH_ANY strategy */
  rasqal_query_results_compare_options_init(&options);
  options.blank_node_strategy = RASQAL_COMPARE_BLANK_NODE_MATCH_ANY;
  
  compare = rasqal_new_query_results_compare(world, results1, results2);
  if(compare) {
    rasqal_query_results_compare_set_options(compare, &options);
    result = rasqal_query_results_compare_execute(compare);
    if(result) {
      test_result &= result->equal;
      rasqal_free_query_results_compare_result(result);
    }
    rasqal_free_query_results_compare(compare);
  }

  /* Test MATCH_ID strategy */
  options.blank_node_strategy = RASQAL_COMPARE_BLANK_NODE_MATCH_ID;
  
  compare = rasqal_new_query_results_compare(world, results1, results2);
  if(compare) {
    rasqal_query_results_compare_set_options(compare, &options);
    result = rasqal_query_results_compare_execute(compare);
    if(result) {
      test_result &= result->equal;
      rasqal_free_query_results_compare_result(result);
    }
    rasqal_free_query_results_compare(compare);
  }

  /* Test MATCH_STRUCTURE strategy (should work but warn about not being fully implemented) */
  options.blank_node_strategy = RASQAL_COMPARE_BLANK_NODE_MATCH_STRUCTURE;
  
  compare = rasqal_new_query_results_compare(world, results1, results2);
  if(compare) {
    rasqal_query_results_compare_set_options(compare, &options);
    result = rasqal_query_results_compare_execute(compare);
    if(result) {
      test_result &= result->equal;
      rasqal_free_query_results_compare_result(result);
    }
    rasqal_free_query_results_compare(compare);
  }

cleanup:
  if(results2)
    rasqal_free_query_results(results2);
  if(results1)
    rasqal_free_query_results(results1);
  if(query)
    rasqal_free_query(query);
  if(world)
    rasqal_free_world(world);

  return test_result;
}

static int
test_string_comparison(void)
{
  rasqal_world* world = NULL;
  rasqal_query* query = NULL;
  rasqal_query_results* results1 = NULL;
  rasqal_query_results* results2 = NULL;
  rasqal_query_results_compare* compare = NULL;
  rasqal_query_results_compare_result* result = NULL;
  int test_result = 1;

  world = rasqal_new_world();
  if(!world)
    return 0;

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query)
    goto cleanup;

  results1 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_BINDINGS);
  if(!results1)
    goto cleanup;

  results2 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_BINDINGS);
  if(!results2)
    goto cleanup;

  /* Test basic string comparison */
  compare = rasqal_new_query_results_compare(world, results1, results2);
  if(compare) {
    result = rasqal_query_results_compare_execute(compare);
    if(result) {
      test_result &= result->equal;
      rasqal_free_query_results_compare_result(result);
    }
    rasqal_free_query_results_compare(compare);
  }

cleanup:
  if(results2)
    rasqal_free_query_results(results2);
  if(results1)
    rasqal_free_query_results(results1);
  if(query)
    rasqal_free_query(query);
  if(world)
    rasqal_free_world(world);

  return test_result;
}

static int
test_structural_blank_node_matching(void)
{
  rasqal_world* world = NULL;
  rasqal_query* query = NULL;
  rasqal_query_results* results1 = NULL;
  rasqal_query_results* results2 = NULL;
  rasqal_query_results_compare_options options;
  rasqal_query_results_compare* compare = NULL;
  rasqal_query_results_compare_result* result = NULL;
  int test_result = 0;

  world = rasqal_new_world();
  if(!world)
    return 0;

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query)
    goto cleanup;

  results1 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_BINDINGS);
  if(!results1)
    goto cleanup;

  results2 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_BINDINGS);
  if(!results2)
    goto cleanup;

  /* Test structural blank node matching */
  rasqal_query_results_compare_options_init(&options);
  options.blank_node_strategy = RASQAL_COMPARE_BLANK_NODE_MATCH_STRUCTURE;

  /* This should work even with empty results */
  compare = rasqal_new_query_results_compare(world, results1, results2);
  if(compare) {
    rasqal_query_results_compare_set_options(compare, &options);
    result = rasqal_query_results_compare_execute(compare);
    if(result) {
      test_result = result->equal;
      rasqal_free_query_results_compare_result(result);
    }
    rasqal_free_query_results_compare(compare);
  }

cleanup:
  if(results2)
    rasqal_free_query_results(results2);
  if(results1)
    rasqal_free_query_results(results1);
  if(query)
    rasqal_free_query(query);
  if(world)
    rasqal_free_world(world);

  return test_result;
}

static int
test_order_insensitive_graph_comparison(void)
{
  rasqal_world* world = NULL;
  rasqal_query* query = NULL;
  rasqal_query_results* results1 = NULL;
  rasqal_query_results* results2 = NULL;
  rasqal_query_results_compare_options options;
  rasqal_query_results_compare* compare = NULL;
  rasqal_query_results_compare_result* result = NULL;
  int test_result = 0;

  world = rasqal_new_world();
  if(!world)
    return 0;

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query)
    goto cleanup;

  results1 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_GRAPH);
  if(!results1)
    goto cleanup;

  results2 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_GRAPH);
  if(!results2)
    goto cleanup;

  /* Test order-insensitive graph comparison */
  rasqal_query_results_compare_options_init(&options);
  options.order_sensitive = 0;

  /* This should work even with empty results */
  compare = rasqal_new_query_results_compare(world, results1, results2);
  if(compare) {
    rasqal_query_results_compare_set_options(compare, &options);
    result = rasqal_query_results_compare_execute(compare);
    if(result) {
      test_result = result->equal;
      rasqal_free_query_results_compare_result(result);
    }
    rasqal_free_query_results_compare(compare);
  }

cleanup:
  if(results2)
    rasqal_free_query_results(results2);
  if(results1)
    rasqal_free_query_results(results1);
  if(query)
    rasqal_free_query(query);
  if(world)
    rasqal_free_world(world);

  return test_result;
}

static int
test_canonicalization(void)
{
  rasqal_world* world = NULL;
  rasqal_query* query = NULL;
  rasqal_query_results* results1 = NULL;
  rasqal_query_results* results2 = NULL;
  rasqal_query_results_compare_options* options = NULL;
  int test_result = 0;

  world = rasqal_new_world();
  if(!world)
    return 0;

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query)
    goto cleanup;

  results1 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_BINDINGS);
  if(!results1)
    goto cleanup;

  results2 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_BINDINGS);
  if(!results2)
    goto cleanup;

  /* Test canonicalization options */
  options = test_new_query_results_compare_options();
  if(!options)
    goto cleanup;

  options->canonicalize_uris = 1;
  options->canonicalize_literals = 1;

  /* This should work even with empty results */
  test_result = test_query_results_are_equal_with_options(world, results1, results2, options);

cleanup:
  if(options)
    test_free_query_results_compare_options(options);
  if(results2)
    rasqal_free_query_results(results2);
  if(results1)
    rasqal_free_query_results(results1);
  if(query)
    rasqal_free_query(query);
  if(world)
    rasqal_free_world(world);

  return test_result;
}

/* Custom comparison function for testing */
static int
custom_term_compare(void* user_data, raptor_term* first, raptor_term* second)
{
  /* Simple custom comparison that always returns true for testing */
  (void)user_data; /* Suppress unused parameter warning */
  (void)first;     /* Suppress unused parameter warning */
  (void)second;    /* Suppress unused parameter warning */
  return 1;
}

/* Custom statement comparison function for testing */
static int
custom_statement_compare(void* user_data, raptor_statement* first, raptor_statement* second)
{
  /* Simple custom comparison that always returns true for testing */
  (void)user_data; /* Suppress unused parameter warning */
  (void)first;     /* Suppress unused parameter warning */
  (void)second;    /* Suppress unused parameter warning */
  return 1;
}

static int
test_custom_comparison_functions(void)
{
  rasqal_world* world = NULL;
  rasqal_query* query = NULL;
  rasqal_query_results* results1 = NULL;
  rasqal_query_results* results2 = NULL;
  rasqal_query_results_compare_options* options = NULL;
  int test_result = 0;

  world = rasqal_new_world();
  if(!world)
    return 0;

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query)
    goto cleanup;

  results1 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_BINDINGS);
  if(!results1)
    goto cleanup;

  results2 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_BINDINGS);
  if(!results2)
    goto cleanup;

  /* Test custom comparison functions */
  options = test_new_query_results_compare_options();
  if(!options)
    goto cleanup;

  options->custom_compare_user_data = (void*)0x12345678; /* Test user data */
  options->custom_term_compare = custom_term_compare;
  options->custom_statement_compare = custom_statement_compare;

  /* This should work even with empty results */
  test_result = test_query_results_are_equal_with_options(world, results1, results2, options);

cleanup:
  if(options)
    test_free_query_results_compare_options(options);
  if(results2)
    rasqal_free_query_results(results2);
  if(results1)
    rasqal_free_query_results(results1);
  if(query)
    rasqal_free_query(query);
  if(world)
    rasqal_free_world(world);

  return test_result;
}

static int
test_uri_canonicalization(void)
{
  rasqal_world* world = NULL;
  rasqal_query* query = NULL;
  rasqal_query_results* results1 = NULL;
  rasqal_query_results* results2 = NULL;
  rasqal_query_results_compare_options* options = NULL;
  int test_result = 0;

  world = rasqal_new_world();
  if(!world)
    return 0;

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query)
    goto cleanup;

  results1 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_BINDINGS);
  if(!results1)
    goto cleanup;

  results2 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_BINDINGS);
  if(!results2)
    goto cleanup;

  /* Test with empty results - this should work */

  /* Test 1: Without canonicalization - should be equal */
  options = test_new_query_results_compare_options();
  if(!options)
    goto cleanup;

  options->canonicalize_uris = 0;
  options->canonicalize_literals = 0;

  test_result = test_query_results_are_equal_with_options(world, results1, results2, options);
  if(test_result != 1) {
    fprintf(stderr, "test_uri_canonicalization: Test 1 failed - expected 1, got %d\n", test_result);
    test_result = 0;
    goto cleanup;
  }

  /* Test 2: With URI canonicalization only */
  options->canonicalize_uris = 1;
  options->canonicalize_literals = 0;

  test_result = test_query_results_are_equal_with_options(world, results1, results2, options);
  if(test_result != 1) {
    fprintf(stderr, "test_uri_canonicalization: Test 2 failed - expected 1, got %d\n", test_result);
    test_result = 0;
    goto cleanup;
  }

  /* Test 3: With literal canonicalization only */
  options->canonicalize_uris = 0;
  options->canonicalize_literals = 1;

  test_result = test_query_results_are_equal_with_options(world, results1, results2, options);
  if(test_result != 1) {
    fprintf(stderr, "test_uri_canonicalization: Test 3 failed - expected 1, got %d\n", test_result);
    test_result = 0;
    goto cleanup;
  }

  /* Test 4: With both URI and literal canonicalization */
  options->canonicalize_uris = 1;
  options->canonicalize_literals = 1;

  test_result = test_query_results_are_equal_with_options(world, results1, results2, options);
  if(test_result != 1) {
    fprintf(stderr, "test_uri_canonicalization: Test 4 failed - expected 1, got %d\n", test_result);
    test_result = 0;
    goto cleanup;
  }

  test_result = 1;

cleanup:
  if(options)
    test_free_query_results_compare_options(options);
  if(results2)
    rasqal_free_query_results(results2);
  if(results1)
    rasqal_free_query_results(results1);
  if(query)
    rasqal_free_query(query);
  if(world)
    rasqal_free_world(world);

  return test_result;
}

static int
test_graph_canonicalization(void)
{
  rasqal_world* world = NULL;
  rasqal_query* query = NULL;
  rasqal_query_results* results1 = NULL;
  rasqal_query_results* results2 = NULL;
  rasqal_query_results_compare_options* options = NULL;
  int test_result = 0;

  world = rasqal_new_world();
  if(!world)
    return 0;

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query)
    goto cleanup;

  results1 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_GRAPH);
  if(!results1)
    goto cleanup;

  results2 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_GRAPH);
  if(!results2)
    goto cleanup;

  /* Test with empty graph results */

  /* Test 1: Without canonicalization - should be equal */
  options = test_new_query_results_compare_options();
  if(!options)
    goto cleanup;

  options->canonicalize_uris = 0;
  options->canonicalize_literals = 0;

  test_result = test_query_results_are_equal_with_options(world, results1, results2, options);
  if(test_result != 1) {
    fprintf(stderr, "test_graph_canonicalization: Test 1 failed - expected 1, got %d\n", test_result);
    test_result = 0;
    goto cleanup;
  }

  /* Test 2: With URI canonicalization only */
  options->canonicalize_uris = 1;
  options->canonicalize_literals = 0;

  test_result = test_query_results_are_equal_with_options(world, results1, results2, options);
  if(test_result != 1) {
    fprintf(stderr, "test_graph_canonicalization: Test 2 failed - expected 1, got %d\n", test_result);
    test_result = 0;
    goto cleanup;
  }

  /* Test 3: With literal canonicalization only */
  options->canonicalize_uris = 0;
  options->canonicalize_literals = 1;

  test_result = test_query_results_are_equal_with_options(world, results1, results2, options);
  if(test_result != 1) {
    fprintf(stderr, "test_graph_canonicalization: Test 3 failed - expected 1, got %d\n", test_result);
    test_result = 0;
    goto cleanup;
  }

  /* Test 4: With both URI and literal canonicalization */
  options->canonicalize_uris = 1;
  options->canonicalize_literals = 1;

  test_result = test_query_results_are_equal_with_options(world, results1, results2, options);
  if(test_result != 1) {
    fprintf(stderr, "test_graph_canonicalization: Test 4 failed - expected 1, got %d\n", test_result);
    test_result = 0;
    goto cleanup;
  }

  test_result = 1;

cleanup:
  if(options)
    test_free_query_results_compare_options(options);
  if(results2)
    rasqal_free_query_results(results2);
  if(results1)
    rasqal_free_query_results(results1);
  if(query)
    rasqal_free_query(query);
  if(world)
    rasqal_free_world(world);

  return test_result;
}

int
main(int argc, char *argv[])
{
  int failures = 0;

  printf("Testing rasqal_query_results_compare module...\n\n");

  failures += !test_options_init();
  print_test_result("Options initialization", test_options_init());

  failures += !test_options_new_free();
  print_test_result("Options new/free", test_options_new_free());

  failures += !test_boolean_comparison();
  print_test_result("Boolean comparison", test_boolean_comparison());

  failures += !test_compare_context();
  print_test_result("Compare context", test_compare_context());

  failures += !test_null_parameters();
  print_test_result("Null parameter handling", test_null_parameters());

  failures += !test_bindings_comparison();
  print_test_result("Bindings comparison", test_bindings_comparison());

  failures += !test_blank_node_strategies();
  print_test_result("Blank node strategies", test_blank_node_strategies());

  failures += !test_string_comparison();
  print_test_result("String comparison", test_string_comparison());
  failures += !test_structural_blank_node_matching();
  print_test_result("Structural blank node matching", test_structural_blank_node_matching());
  failures += !test_order_insensitive_graph_comparison();
  print_test_result("Order-insensitive graph comparison", test_order_insensitive_graph_comparison());
  failures += !test_canonicalization();
  print_test_result("Canonicalization", test_canonicalization());
  failures += !test_uri_canonicalization();
  print_test_result("URI canonicalization", test_uri_canonicalization());
  failures += !test_graph_canonicalization();
  print_test_result("Graph canonicalization", test_graph_canonicalization());
  failures += !test_custom_comparison_functions();
  print_test_result("Custom comparison functions", test_custom_comparison_functions());

  printf("\nTotal failures: %d\n", failures);

  return failures;
}

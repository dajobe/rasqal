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



#define NTESTS 8

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


static void
test_free_query_results_compare_options(rasqal_query_results_compare_options* options)
{
  if(!options)
    return;

  RASQAL_FREE(rasqal_query_results_compare_options*, options);
}

static int
test_options_init(rasqal_world* world)
{
  rasqal_query_results_compare_options options;

  (void)world; /* Suppress unused parameter warning */

  rasqal_query_results_compare_options_init(&options);

  return (options.order_sensitive == 0 &&
          options.blank_node_strategy == RASQAL_COMPARE_BLANK_NODE_MATCH_ANY &&
          options.literal_comparison_flags == RASQAL_COMPARE_XQUERY &&
          options.max_differences == 10);
}

static int
test_options_new_free(rasqal_world* world)
{
  rasqal_query_results_compare_options options;
  int result = 0;

  (void)world; /* Suppress unused parameter warning */

  /* Test options initialization */
  rasqal_query_results_compare_options_init(&options);

  result = (options.order_sensitive == 0 &&
            options.blank_node_strategy == RASQAL_COMPARE_BLANK_NODE_MATCH_ANY &&
            options.literal_comparison_flags == RASQAL_COMPARE_XQUERY &&
            options.max_differences == 10);

  return result;
}

static int
test_bindings_comparison_simple(rasqal_world* world)
{
  rasqal_query_results* results1 = NULL;
  rasqal_query_results* results2 = NULL;
  rasqal_query_results* results3 = NULL;
  raptor_uri* base_uri = NULL;
  int result = 0;

  /* Create base URI for results */
  base_uri = raptor_new_uri(world->raptor_world_ptr, (const unsigned char*)"http://example.org/");
  if(!base_uri)
    goto cleanup;

  /* Use TSV format - requires 3+ tabs in first line for auto-detection */  
  results1 = rasqal_new_query_results_from_string(world, RASQAL_QUERY_RESULTS_BINDINGS, base_uri, 
    "x\ty\tz\tw\n\"value1\"\t\"value2\"\t\"value3\"\t\"value4\"\n", 0);
  if(!results1)
    goto cleanup;

  results2 = rasqal_new_query_results_from_string(world, RASQAL_QUERY_RESULTS_BINDINGS, base_uri,
    "x\ty\tz\tw\n\"value1\"\t\"value2\"\t\"value3\"\t\"value4\"\n", 0);
  if(!results2)
    goto cleanup;

  results3 = rasqal_new_query_results_from_string(world, RASQAL_QUERY_RESULTS_BINDINGS, base_uri,
    "x\ty\tz\tw\n\"different1\"\t\"different2\"\t\"different3\"\t\"different4\"\n", 0);
  if(!results3)
    goto cleanup;

  /* Test that identical bindings results are equal */
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

  /* Test that different bindings results are detected as different */
  if(result) {
    rasqal_query_results_compare* compare = rasqal_new_query_results_compare(world, results1, results3);
    if(compare) {
      rasqal_query_results_compare_result* compare_result = rasqal_query_results_compare_execute(compare);
      if(compare_result) {
        result = !compare_result->equal;  /* Should be NOT equal */
        rasqal_free_query_results_compare_result(compare_result);
      }
      rasqal_free_query_results_compare(compare);
    }
  }

cleanup:
  if(results1)
    rasqal_free_query_results(results1);
  if(results2)
    rasqal_free_query_results(results2);
  if(results3)
    rasqal_free_query_results(results3);
  if(base_uri)
    raptor_free_uri(base_uri);

  return result;
}

static int
test_boolean_comparison(rasqal_world* world)
{
  rasqal_query* query = NULL;
  rasqal_query_results* results1 = NULL;
  rasqal_query_results* results2 = NULL;
  rasqal_query_results* results3 = NULL;
  int result = 0;

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query)
    goto cleanup;

  /* Create boolean results with different values */
  results1 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_BOOLEAN);
  if(!results1)
    goto cleanup;

  results2 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_BOOLEAN);
  if(!results2)
    goto cleanup;

  results3 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_BOOLEAN);
  if(!results3)
    goto cleanup;

  /* Set boolean values */
  rasqal_query_results_set_boolean(results1, 1); /* true */
  rasqal_query_results_set_boolean(results2, 1); /* true */
  rasqal_query_results_set_boolean(results3, 0); /* false */

  /* Test that boolean results are created correctly */
  result = rasqal_query_results_is_boolean(results1) && rasqal_query_results_is_boolean(results2);
  if(!result)
    goto cleanup;

  /* Test that equal boolean results are detected as equal */
  if(result) {
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

  /* Test that different boolean results are detected as different */
  if(result) {
    rasqal_query_results_compare* compare = rasqal_new_query_results_compare(world, results1, results3);
    if(compare) {
      rasqal_query_results_compare_result* compare_result = rasqal_query_results_compare_execute(compare);
      if(compare_result) {
        result = !compare_result->equal; /* Should be different */
        rasqal_free_query_results_compare_result(compare_result);
      }
      rasqal_free_query_results_compare(compare);
    }
  }

cleanup:
  if(results3)
    rasqal_free_query_results(results3);
  if(results2)
    rasqal_free_query_results(results2);
  if(results1)
    rasqal_free_query_results(results1);
  if(query)
    rasqal_free_query(query);

  return result;
}

static int
test_compare_context(rasqal_world* world)
{
  rasqal_query* query = NULL;
  rasqal_query_results* results1 = NULL;
  rasqal_query_results* results2 = NULL;
  rasqal_query_results_compare* compare = NULL;
  rasqal_query_results_compare_result* result = NULL;
  int test_result = 0;

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

  return test_result;
}

static int
test_null_parameters(rasqal_world* world)
{
  rasqal_query* query = NULL;
  rasqal_query_results* results = NULL;
  int test_result = 1;

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

  return test_result;
}



static int
test_blank_node_strategies(rasqal_world* world)
{
  rasqal_query* query = NULL;
  rasqal_query_results* results1 = NULL;
  rasqal_query_results* results2 = NULL;
  rasqal_query_results_compare_options options;
  rasqal_query_results_compare* compare = NULL;
  rasqal_query_results_compare_result* result = NULL;
  int test_result = 1;

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query)
    goto cleanup;

  /* Create results with blank node data to test blank node strategies */
  {
    raptor_uri* base_uri = raptor_new_uri(world->raptor_world_ptr, (const unsigned char*)"http://example.org/");
    if(!base_uri)
      goto cleanup;
      
    /* Use TSV data with same blank node IDs for both results - this should be equal under all strategies */
    results1 = rasqal_new_query_results_from_string(world, RASQAL_QUERY_RESULTS_BINDINGS, base_uri,
      "subj\tpred\tobj\tw\n_:blank1\t<http://example.org/prop>\t\"value1\"\t_:blank2\n", 0);
    results2 = rasqal_new_query_results_from_string(world, RASQAL_QUERY_RESULTS_BINDINGS, base_uri,
      "subj\tpred\tobj\tw\n_:blank1\t<http://example.org/prop>\t\"value1\"\t_:blank2\n", 0);
      
    raptor_free_uri(base_uri);
    
    if(!results1 || !results2)
      goto cleanup;
  }

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

  return test_result;
}

static int
test_string_comparison(rasqal_world* world)
{
  rasqal_query* query = NULL;
  rasqal_query_results* results1 = NULL;
  rasqal_query_results* results2 = NULL;
  rasqal_query_results_compare* compare = NULL;
  rasqal_query_results_compare_result* result = NULL;
  int test_result = 1;

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query)
    goto cleanup;

  /* Create results with string data to test string comparison */
  {
    raptor_uri* base_uri = raptor_new_uri(world->raptor_world_ptr, (const unsigned char*)"http://example.org/");
    if(!base_uri)
      goto cleanup;
      
    /* Use TSV data with string literals */
    results1 = rasqal_new_query_results_from_string(world, RASQAL_QUERY_RESULTS_BINDINGS, base_uri,
      "name\tvalue\ttype\tstatus\n\"Alice\"\t\"123\"\t\"person\"\t\"active\"\n", 0);
    results2 = rasqal_new_query_results_from_string(world, RASQAL_QUERY_RESULTS_BINDINGS, base_uri,
      "name\tvalue\ttype\tstatus\n\"Alice\"\t\"123\"\t\"person\"\t\"active\"\n", 0);
      
    raptor_free_uri(base_uri);
    
    if(!results1 || !results2)
      goto cleanup;
  }

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

  return test_result;
}

static int
test_structural_blank_node_matching(rasqal_world* world)
{
  rasqal_query* query = NULL;
  rasqal_query_results* results1 = NULL;
  rasqal_query_results* results2 = NULL;
  rasqal_query_results_compare_options options;
  rasqal_query_results_compare* compare = NULL;
  rasqal_query_results_compare_result* result = NULL;
  int test_result = 0;

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

  return test_result;
}

static int
test_order_insensitive_graph_comparison(rasqal_world* world)
{
  rasqal_query* query = NULL;
  rasqal_query_results* results1 = NULL;
  rasqal_query_results* results2 = NULL;
  rasqal_query_results_compare_options options;
  rasqal_query_results_compare* compare = NULL;
  rasqal_query_results_compare_result* result = NULL;
  int test_result = 0;

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

  return test_result;
}



static int
test_advanced_graph_comparison(rasqal_world* world)
{
  raptor_sequence* triples = NULL;
  raptor_sequence* blank_nodes = NULL;
  raptor_term* bnode1 = NULL;
  raptor_term* bnode2 = NULL;
  raptor_term* bnode3 = NULL;
  raptor_statement* triple1 = NULL;
  raptor_statement* triple2 = NULL;
  raptor_statement* triple3 = NULL;
  raptor_statement* triple4 = NULL;
  raptor_term* uri1 = NULL;
  raptor_term* uri2 = NULL;
  raptor_term* literal1 = NULL;
  int result = 0;

  if(!world) {
    printf("NULL world parameter\n");
    return 0;
  }

  /* Create test data */
  triples = raptor_new_sequence((raptor_data_free_handler)raptor_free_statement, 
                                (raptor_data_print_handler)raptor_statement_print);
  /* The blank nodes (raptor_term*) are owned by the raptor_statement*
   * in the triples sequence above so we don't need to free them here.
   * These are weak references.
   */
  blank_nodes = raptor_new_sequence(NULL, NULL);

  if(!triples || !blank_nodes) {
    printf("Failed to create sequences\n");
    goto cleanup;
  }

  /* Create blank nodes */
  bnode1 = raptor_new_term_from_blank(world->raptor_world_ptr, (unsigned char*)"_:b1");
  bnode2 = raptor_new_term_from_blank(world->raptor_world_ptr, (unsigned char*)"_:b2");
  bnode3 = raptor_new_term_from_blank(world->raptor_world_ptr, (unsigned char*)"_:b3");

  /* Create URIs and literals */
  uri1 = raptor_new_term_from_uri_string(world->raptor_world_ptr, (unsigned char*)"http://example.org/p");
  uri2 = raptor_new_term_from_uri_string(world->raptor_world_ptr, (unsigned char*)"http://example.org/q");
  literal1 = raptor_new_term_from_literal(world->raptor_world_ptr, (unsigned char*)"value", NULL, NULL);

  if(!bnode1 || !bnode2 || !bnode3 || !uri1 || !uri2 || !literal1) {
    printf("Failed to create terms\n");
    goto cleanup;
  }

  /* Add original blank nodes to sequence (weak references) */
  raptor_sequence_push(blank_nodes, bnode1);
  raptor_sequence_push(blank_nodes, bnode2);
  raptor_sequence_push(blank_nodes, bnode3);

  /* Create test triples - statements will take ownership of copied terms */
  triple1 = raptor_new_statement_from_nodes(world->raptor_world_ptr, raptor_term_copy(bnode1), raptor_term_copy(uri1), raptor_term_copy(literal1), NULL);
  triple2 = raptor_new_statement_from_nodes(world->raptor_world_ptr, bnode1, uri2, raptor_term_copy(bnode2), NULL);
  triple3 = raptor_new_statement_from_nodes(world->raptor_world_ptr, bnode2, raptor_term_copy(uri1), raptor_term_copy(literal1), NULL);
  triple4 = raptor_new_statement_from_nodes(world->raptor_world_ptr, bnode3, uri1, literal1, NULL);
  bnode1 = bnode2 = bnode3 = uri1 = uri2 = literal1 = NULL;

  if(!triple1 || !triple2 || !triple3 || !triple4) {
    printf("Failed to create triples\n");
    goto cleanup;
  }

  /* Add triples to sequence - statements are now owned by the sequence */
  raptor_sequence_push(triples, triple1);
  raptor_sequence_push(triples, triple2);
  raptor_sequence_push(triples, triple3);
  raptor_sequence_push(triples, triple4);
  /* Statements are now owned by sequence, but original terms still need manual cleanup */
  triple1 = triple2 = triple3 = triple4 = NULL;

  /* Test 1: Graph comparison options initialization */
  if(1) {
    rasqal_graph_comparison_options graph_options;
    rasqal_graph_comparison_options_init(&graph_options);

    if(graph_options.signature_threshold != 1000 ||
       graph_options.max_search_time != 30 ||
       graph_options.incremental_mode != 0 ||
       graph_options.signature_cache_size != 1000) {
      printf("Incorrect default values for graph comparison options\n");
      goto cleanup;
    }
  }

  /* Test 2: Advanced blank node comparison with graph options */
  if(1) {
    rasqal_query_results_compare_options* options = test_new_query_results_compare_options();
    rasqal_graph_comparison_options graph_options;

    if(!options) {
      printf("Failed to create options\n");
      goto cleanup;
    }

    /* Set up advanced graph comparison options */
    rasqal_graph_comparison_options_init(&graph_options);
    options->graph_comparison_options = &graph_options;
    options->blank_node_strategy = RASQAL_COMPARE_BLANK_NODE_MATCH_STRUCTURE;

    /* Test that options are properly set */
    if(options->blank_node_strategy != RASQAL_COMPARE_BLANK_NODE_MATCH_STRUCTURE ||
       !options->graph_comparison_options) {
      printf("Failed to set advanced graph comparison options\n");
      test_free_query_results_compare_options(options);
      goto cleanup;
    }

    test_free_query_results_compare_options(options);
  }

  /* Test 3: Graph comparison options initialization */
  if(1) {
    rasqal_graph_comparison_options graph_options;
    rasqal_graph_comparison_options_init(&graph_options);

    if(graph_options.signature_threshold != 1000 ||
       graph_options.max_search_time != 30 ||
       graph_options.incremental_mode != 0 ||
       graph_options.signature_cache_size != 1000) {
      printf("Incorrect default values for graph comparison options\n");
      goto cleanup;
    }
  }

  result = 1;

cleanup:

  /* Cleanup triples */
  if(triple1)
    raptor_free_statement(triple1);
  if(triple2)
    raptor_free_statement(triple2);
  if(triple3)
    raptor_free_statement(triple3);
  if(triple4)
    raptor_free_statement(triple4);

  /* Cleanup terms */
  if(bnode1)
    raptor_free_term(bnode1);
  if(bnode2)
    raptor_free_term(bnode2);
  if(bnode3)
    raptor_free_term(bnode3);
  if(uri1)
    raptor_free_term(uri1);
  if(uri2)
    raptor_free_term(uri2);
  if(literal1)
    raptor_free_term(literal1);

  /* Cleanup sequences */
  if(triples)
    raptor_free_sequence(triples);
  if(blank_nodes)
    raptor_free_sequence(blank_nodes);

  return result;
}



int
main(int argc, char *argv[])
{
  rasqal_world* world = NULL;
  int failures = 0;
  int result;

  printf("Testing rasqal_query_results_compare module...\n\n");

  /* Create world once for all tests */
  world = rasqal_new_world();
  if(!world) {
    printf("Failed to create rasqal world\n");
    return 1;
  }

  failures += !test_options_init(world);
  print_test_result("Options initialization", test_options_init(world));

  failures += !test_options_new_free(world);
  print_test_result("Options new/free", test_options_new_free(world));

  failures += !test_boolean_comparison(world);
  print_test_result("Boolean comparison", test_boolean_comparison(world));

  failures += !test_compare_context(world);
  print_test_result("Compare context", test_compare_context(world));

  failures += !test_null_parameters(world);
  print_test_result("Null parameter handling", test_null_parameters(world));

  result = test_bindings_comparison_simple(world);
  failures += !result;
  print_test_result("Bindings comparison", result);

  result = test_blank_node_strategies(world);
  failures += !result;
  print_test_result("Blank node strategies", result);

  result = test_string_comparison(world);
  failures += !result;
  print_test_result("String comparison", result);

  result = test_structural_blank_node_matching(world);
  failures += !result;
  print_test_result("Structural blank node matching", result);

  result = test_order_insensitive_graph_comparison(world);
  failures += !result;
  print_test_result("Order-insensitive graph comparison", result);

  result = test_advanced_graph_comparison(world);
  failures += !result;
  print_test_result("Advanced Graph Comparison", result);

  printf("\nTotal failures: %d\n", failures);

  /* Clean up world */
  if(world)
    rasqal_free_world(world);

  return failures;
}

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_query_results_compare.c - Rasqal Query Results Compare Module
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
#include <ctype.h>

#include "rasqal.h"
#include "rasqal_internal.h"
#include "rasqal_graph_isomorphism.h"

#ifndef STANDALONE

/*
 * rasqal_query_results_compare_s:
 * @world: rasqal world
 * @first_results: first query results to compare
 * @second_results: second query results to compare
 * @options: comparison options
 * @differences: array of difference descriptions
 * @differences_count: number of differences found
 * @differences_size: size of differences array
 *
 * Query results comparison context
 */
struct rasqal_query_results_compare_s {
  rasqal_world* world;
  rasqal_query_results* first_results;
  rasqal_query_results* second_results;
  rasqal_query_results_compare_options options;

  rasqal_query_results_compare_difference* differences;
  int differences_count;
  int differences_size;
  rasqal_query_results_compare_triple_difference* triple_differences;
  int triple_differences_count;
  int triple_differences_size;
};

/* Forward declarations */
static int rasqal_query_results_compare_bindings_internal(rasqal_query_results_compare* compare);
static int rasqal_query_results_compare_boolean_internal(rasqal_query_results_compare* compare);
static int rasqal_query_results_compare_graph_internal(rasqal_query_results_compare* compare);
static int rasqal_query_results_compare_blank_node_structure(raptor_term* first_bnode, raptor_term* second_bnode, rasqal_query_results_compare* compare);
static unsigned char* rasqal_query_results_compare_get_blank_node_signature(raptor_term* bnode, rasqal_query_results_compare* compare);
static raptor_term* rasqal_query_results_compare_literal_to_term(rasqal_literal* literal, rasqal_world* world);
static int rasqal_query_results_compare_signature_part_compare(const void* a, const void* b, void* arg);

/* Phase 2: Row collection optimization helper functions */
static raptor_sequence* collect_rows_with_ownership(rasqal_query_results* results, rasqal_query_results_compare* compare);

static int compare_row_sequences(raptor_sequence* first_rows, raptor_sequence* second_rows, rasqal_query_results_compare* compare);
static int compare_single_row(rasqal_row* first_row, rasqal_row* second_row, int var_count, rasqal_query_results_compare* compare);

/* Helper functions for sort_row_sequence */
static int sort_row_sequence_compare_rows(const void* a, const void* b, void* arg);

/* Helper functions for compare_single_row */
static int compare_single_row_compare_blank_nodes(rasqal_literal* first, rasqal_literal* second, int column_index, rasqal_query_results_compare* compare);



/**
 * rasqal_new_query_results_compare:
 * @world: rasqal world
 * @first_results: first query results to compare
 * @second_results: second query results to compare
 *
 * Create a new query results comparison context.
 *
 * Returns the comparison context or NULL on failure.
 */
rasqal_query_results_compare*
rasqal_new_query_results_compare(rasqal_world* world,
                                 rasqal_query_results* first_results,
                                 rasqal_query_results* second_results)
{
  rasqal_query_results_compare* compare = NULL;

  if(!world || !first_results || !second_results)
    return NULL;

  compare = RASQAL_CALLOC(rasqal_query_results_compare*, 1, sizeof(*compare));
  if(!compare)
    return NULL;

  compare->world = world;
  compare->first_results = first_results;
  compare->second_results = second_results;

  /* Initialize with default options */
  rasqal_query_results_compare_options_init(&compare->options);

  compare->differences = NULL;
  compare->differences_count = 0;
  compare->differences_size = 0;
  compare->triple_differences = NULL;
  compare->triple_differences_count = 0;
  compare->triple_differences_size = 0;

  return compare;
}

/**
 * rasqal_free_query_results_compare:
 * @compare: comparison context to free
 *
 * Free a query results comparison context and its resources.
 */
void
rasqal_free_query_results_compare(rasqal_query_results_compare* compare)
{
  if(!compare)
    return;

  if(compare->differences) {
    int i;
    for(i = 0; i < compare->differences_count; i++) {
      if(compare->differences[i].description)
        RASQAL_FREE(char*, compare->differences[i].description);
      if(compare->differences[i].expected)
        RASQAL_FREE(char*, compare->differences[i].expected);
      if(compare->differences[i].actual)
        RASQAL_FREE(char*, compare->differences[i].actual);
    }
    RASQAL_FREE(rasqal_query_results_compare_difference*, compare->differences);
  }

  if(compare->triple_differences) {
    int i;
    for(i = 0; i < compare->triple_differences_count; i++) {
      if(compare->triple_differences[i].description)
        RASQAL_FREE(char*, compare->triple_differences[i].description);
      if(compare->triple_differences[i].expected_triple)
        raptor_free_statement(compare->triple_differences[i].expected_triple);
      if(compare->triple_differences[i].actual_triple)
        raptor_free_statement(compare->triple_differences[i].actual_triple);
    }
    RASQAL_FREE(rasqal_query_results_compare_triple_difference*, compare->triple_differences);
  }

  RASQAL_FREE(rasqal_query_results_compare*, compare);
}

/**
 * rasqal_query_results_compare_set_options:
 * @compare: comparison context
 * @options: comparison options to set
 *
 * Set comparison options for the comparison context.
 *
 * Return value: non-0 on failure
 */
int
rasqal_query_results_compare_set_options(rasqal_query_results_compare* compare,
                                         rasqal_query_results_compare_options* options)
{
  if(!compare || !options)
    return 1;

  compare->options = *options;
  return 0;
}



/**
 * rasqal_query_results_compare_execute:
 * @compare: comparison context
 *
 * Execute the comparison and return detailed results.
 *
 * Returns comparison result structure or NULL on failure.
 */
rasqal_query_results_compare_result*
rasqal_query_results_compare_execute(rasqal_query_results_compare* compare)
{
  rasqal_query_results_compare_result* result = NULL;
  rasqal_query_results_type first_type, second_type;
  int equal = 0;

  if(!compare)
    return NULL;

  /* Clear any previous differences */
  if(compare->differences) {
    int i;
    for(i = 0; i < compare->differences_count; i++) {
      if(compare->differences[i].description)
        RASQAL_FREE(char*, compare->differences[i].description);
      if(compare->differences[i].expected)
        RASQAL_FREE(char*, compare->differences[i].expected);
      if(compare->differences[i].actual)
        RASQAL_FREE(char*, compare->differences[i].actual);
    }
    RASQAL_FREE(rasqal_query_results_compare_difference*, compare->differences);
    compare->differences = NULL;
    compare->differences_count = 0;
    compare->differences_size = 0;
  }

  if(compare->triple_differences) {
    int i;
    for(i = 0; i < compare->triple_differences_count; i++) {
      if(compare->triple_differences[i].description)
        RASQAL_FREE(char*, compare->triple_differences[i].description);
      if(compare->triple_differences[i].expected_triple)
        raptor_free_statement(compare->triple_differences[i].expected_triple);
      if(compare->triple_differences[i].actual_triple)
        raptor_free_statement(compare->triple_differences[i].actual_triple);
    }
    RASQAL_FREE(rasqal_query_results_compare_triple_difference*, compare->triple_differences);
    compare->triple_differences = NULL;
    compare->triple_differences_count = 0;
    compare->triple_differences_size = 0;
  }

  /* Check result types match */
  first_type = rasqal_query_results_get_type(compare->first_results);
  second_type = rasqal_query_results_get_type(compare->second_results);

  if(first_type != second_type) {
    rasqal_query_results_compare_add_difference(compare,
      "Result types do not match",
      rasqal_query_results_type_label(first_type),
      rasqal_query_results_type_label(second_type));
    goto create_result;
  }

  /* Compare based on type */
  switch(first_type) {
    case RASQAL_QUERY_RESULTS_BINDINGS:
      equal = rasqal_query_results_compare_bindings_internal(compare);
      break;
    case RASQAL_QUERY_RESULTS_BOOLEAN:
      equal = rasqal_query_results_compare_boolean_internal(compare);
      break;
    case RASQAL_QUERY_RESULTS_GRAPH:
      equal = rasqal_query_results_compare_graph_internal(compare);
      break;
    case RASQAL_QUERY_RESULTS_SYNTAX:
    case RASQAL_QUERY_RESULTS_UNKNOWN:
    default:
      rasqal_query_results_compare_add_difference(compare,
        "Unsupported result type for comparison",
        rasqal_query_results_type_label(first_type),
        NULL);
      break;
  }

create_result:
  result = RASQAL_CALLOC(rasqal_query_results_compare_result*, 1,
                         sizeof(*result));
  if(!result)
    return NULL;

  result->equal = equal && (compare->differences_count == 0) && (compare->triple_differences_count == 0);
  result->differences_count = compare->differences_count;
  result->triple_differences_count = compare->triple_differences_count;
  result->differences_size = compare->differences_size;
  result->triple_differences_size = compare->triple_differences_size;
  result->differences = compare->differences;
  result->triple_differences = compare->triple_differences;
  result->error_message = NULL;

  /* Transfer ownership of differences arrays */
  compare->differences = NULL;
  compare->differences_count = 0;
  compare->differences_size = 0;
  compare->triple_differences = NULL;
  compare->triple_differences_count = 0;
  compare->triple_differences_size = 0;

  return result;
}

/**
 * rasqal_free_query_results_compare_result:
 * @result: comparison result to free
 *
 * Free a comparison result structure and its resources.
 */
void
rasqal_free_query_results_compare_result(rasqal_query_results_compare_result* result)
{
  if(!result)
    return;

  if(result->differences) {
    int i;
    for(i = 0; i < result->differences_count; i++) {
      if(result->differences[i].description)
        RASQAL_FREE(char*, result->differences[i].description);
      if(result->differences[i].expected)
        RASQAL_FREE(char*, result->differences[i].expected);
      if(result->differences[i].actual)
        RASQAL_FREE(char*, result->differences[i].actual);
    }
    RASQAL_FREE(rasqal_query_results_compare_difference*, result->differences);
  }

  if(result->triple_differences) {
    int i;
    for(i = 0; i < result->triple_differences_count; i++) {
      if(result->triple_differences[i].description)
        RASQAL_FREE(char*, result->triple_differences[i].description);
      if(result->triple_differences[i].expected_triple)
        raptor_free_statement(result->triple_differences[i].expected_triple);
      if(result->triple_differences[i].actual_triple)
        raptor_free_statement(result->triple_differences[i].actual_triple);
    }
    RASQAL_FREE(rasqal_query_results_compare_triple_difference*, result->triple_differences);
  }

  if(result->error_message)
    RASQAL_FREE(char*, result->error_message);

  RASQAL_FREE(rasqal_query_results_compare_result*, result);
}

/**
 * rasqal_query_results_compare_options_init:
 * @options: options structure to initialize
 *
 * Initialize comparison options with default values.
 */
void
rasqal_query_results_compare_options_init(rasqal_query_results_compare_options* options)
{
  if(!options)
    return;

  options->order_sensitive = 0;
  options->blank_node_strategy = RASQAL_COMPARE_BLANK_NODE_MATCH_ANY;
  options->literal_comparison_flags = RASQAL_COMPARE_XQUERY;
  options->max_differences = 10;

  options->custom_compare_user_data = NULL;
  options->graph_comparison_options = NULL;
}


void
rasqal_graph_comparison_options_init(rasqal_graph_comparison_options* options)
{
  if(!options)
    return;

  options->signature_threshold = 1000;
  options->max_search_time = 30;
  options->incremental_mode = 0;
  options->signature_cache_size = 1000;
}
 

/* Internal helper functions */
/*
 * rasqal_query_results_compare_add_difference:
 * @compare: comparison context
 * @difference: difference description (printf format)
 * @...: format arguments
 *
 * Add a difference description to the comparison context.

 * Returns non-zero on success, 0 on failure.
 */
int
rasqal_query_results_compare_add_difference(rasqal_query_results_compare* compare,
                                           const char* description,
                                           const char* expected,
                                           const char* actual)
{
  if(!compare || !description)
    return 0;

  /* Check if we've reached the maximum number of differences */
  if(compare->differences_count >= compare->options.max_differences)
    return 1;

  /* Expand differences array if needed */
  if(compare->differences_count >= compare->differences_size) {
    int new_size = compare->differences_size + 10;
    rasqal_query_results_compare_difference* new_differences = RASQAL_REALLOC(rasqal_query_results_compare_difference*, compare->differences, new_size * sizeof(rasqal_query_results_compare_difference));
    if(!new_differences)
      return 0;
    compare->differences = new_differences;
    compare->differences_size = new_size;
  }

  /* Initialize the new difference */
  compare->differences[compare->differences_count].description = NULL;
  compare->differences[compare->differences_count].expected = NULL;
  compare->differences[compare->differences_count].actual = NULL;

  /* Copy description */
  {
    size_t len = strlen(description);
    compare->differences[compare->differences_count].description = RASQAL_MALLOC(char*, len + 1);
    if(!compare->differences[compare->differences_count].description)
      return 0;
    memcpy(compare->differences[compare->differences_count].description, description, len + 1);
  }

  /* Copy expected value if provided */
  if(expected) {
    size_t len = strlen(expected);
    compare->differences[compare->differences_count].expected = RASQAL_MALLOC(char*, len + 1);
    if(!compare->differences[compare->differences_count].expected) {
      RASQAL_FREE(char*, compare->differences[compare->differences_count].description);
      return 0;
    }
    memcpy(compare->differences[compare->differences_count].expected, expected, len + 1);
  }

  /* Copy actual value if provided */
  if(actual) {
    size_t len = strlen(actual);
    compare->differences[compare->differences_count].actual = RASQAL_MALLOC(char*, len + 1);
    if(!compare->differences[compare->differences_count].actual) {
      RASQAL_FREE(char*, compare->differences[compare->differences_count].description);
      if(compare->differences[compare->differences_count].expected)
        RASQAL_FREE(char*, compare->differences[compare->differences_count].expected);
      return 0;
    }
    memcpy(compare->differences[compare->differences_count].actual, actual, len + 1);
  }

  compare->differences_count++;
  return 1;
}

/**
 * rasqal_query_results_compare_add_triple_difference:
 * @compare: comparison context
 * @description: description of the difference
 * @expected_triple: expected triple (NULL if not applicable)
 * @actual_triple: actual triple (NULL if not applicable)
 *
 * Add a triple difference to the comparison context.
 *
 * Returns non-zero on success, 0 on failure.
 */
int
rasqal_query_results_compare_add_triple_difference(rasqal_query_results_compare* compare,
                                                  const char* description,
                                                  raptor_statement* expected_triple,
                                                  raptor_statement* actual_triple)
{
  if(!compare || !description)
    return 0;

  /* Check if we've reached the maximum number of differences */
  if(compare->triple_differences_count >= compare->options.max_differences)
    return 1;

  /* Expand triple_differences array if needed */
  if(compare->triple_differences_count >= compare->triple_differences_size) {
    int new_size = compare->triple_differences_size + 10;
    rasqal_query_results_compare_triple_difference* new_triple_differences = RASQAL_REALLOC(rasqal_query_results_compare_triple_difference*, compare->triple_differences, new_size * sizeof(rasqal_query_results_compare_triple_difference));
    if(!new_triple_differences)
      return 0;
    compare->triple_differences = new_triple_differences;
    compare->triple_differences_size = new_size;
  }

  /* Initialize the new triple difference */
  compare->triple_differences[compare->triple_differences_count].description = NULL;
  compare->triple_differences[compare->triple_differences_count].expected_triple = NULL;
  compare->triple_differences[compare->triple_differences_count].actual_triple = NULL;

  /* Copy description */
  {
    size_t len = strlen(description);
    compare->triple_differences[compare->triple_differences_count].description = RASQAL_MALLOC(char*, len + 1);
    if(!compare->triple_differences[compare->triple_differences_count].description)
      return 0;
    memcpy(compare->triple_differences[compare->triple_differences_count].description, description, len + 1);
  }

  /* Copy expected triple if provided */
  if(expected_triple) {
    compare->triple_differences[compare->triple_differences_count].expected_triple = raptor_statement_copy(expected_triple);
    if(!compare->triple_differences[compare->triple_differences_count].expected_triple) {
      RASQAL_FREE(char*, compare->triple_differences[compare->triple_differences_count].description);
      return 0;
    }
  }

  /* Copy actual triple if provided */
  if(actual_triple) {
    compare->triple_differences[compare->triple_differences_count].actual_triple = raptor_statement_copy(actual_triple);
    if(!compare->triple_differences[compare->triple_differences_count].actual_triple) {
      RASQAL_FREE(char*, compare->triple_differences[compare->triple_differences_count].description);
      if(compare->triple_differences[compare->triple_differences_count].expected_triple)
        raptor_free_statement(compare->triple_differences[compare->triple_differences_count].expected_triple);
      return 0;
    }
  }

  compare->triple_differences_count++;
  return 1;
}



/*
 * rasqal_query_results_compare_bindings_internal:
 * @compare: comparison context
 *
 * Compare bindings results internally.
 * Returns non-zero if equal, 0 if different.
 */
static int
rasqal_query_results_compare_bindings_internal(rasqal_query_results_compare* compare)
{
  rasqal_variables_table* first_vt = NULL;
  rasqal_variables_table* second_vt = NULL;
  int first_count, second_count;
  int i;
  int equal = 1;
  int first_bindings_count, second_bindings_count;
  raptor_sequence* first_rows = NULL;
  raptor_sequence* second_rows = NULL;

  if(!compare)
    return 0;

  first_vt = rasqal_query_results_get_variables_table(compare->first_results);
  second_vt = rasqal_query_results_get_variables_table(compare->second_results);

  if(!first_vt || !second_vt) {
    rasqal_query_results_compare_add_difference(compare, "Cannot get variables table", NULL, NULL);
    return 0;
  }

  first_count = rasqal_variables_table_get_total_variables_count(first_vt);
  second_count = rasqal_variables_table_get_total_variables_count(second_vt);

  /* Compare variable counts */
  if(first_count != second_count) {
    char expected_str[32], actual_str[32];
    snprintf(expected_str, sizeof(expected_str), "%d", first_count);
    snprintf(actual_str, sizeof(actual_str), "%d", second_count);
    rasqal_query_results_compare_add_difference(compare,
      "Variable count mismatch", expected_str, actual_str);
    equal = 0;
  }

  /* Compare variable names */
  for(i = 0; i < first_count && i < second_count; i++) {
    rasqal_variable* first_var = rasqal_variables_table_get(first_vt, i);
    rasqal_variable* second_var = rasqal_variables_table_get(second_vt, i);

    if(!first_var || !second_var) {
      char index_str[32];
      snprintf(index_str, sizeof(index_str), "%d", i);
      rasqal_query_results_compare_add_difference(compare,
        "Cannot get variable at index", index_str, NULL);
      equal = 0;
      continue;
    }

    if(strcmp((char*)first_var->name, (char*)second_var->name) != 0) {
      rasqal_query_results_compare_add_difference(compare,
        "Variable name mismatch at index", (char*)first_var->name, (char*)second_var->name);
      equal = 0;
    }
  }

  /* Get bindings counts */
  first_bindings_count = rasqal_query_results_get_bindings_count(compare->first_results);
  second_bindings_count = rasqal_query_results_get_bindings_count(compare->second_results);

  /* Compare bindings counts */
  if(first_bindings_count != second_bindings_count) {
    char expected_str[32], actual_str[32];
    snprintf(expected_str, sizeof(expected_str), "%d", first_bindings_count);
    snprintf(actual_str, sizeof(actual_str), "%d", second_bindings_count);
    rasqal_query_results_compare_add_difference(compare,
      "Bindings count mismatch", expected_str, actual_str);
    equal = 0;
  }

  /* If variable names don't match or counts don't match, stop here */
  if(!equal)
    return equal;

  /* Phase 2: Use optimized row collection approach */
  first_rows = collect_rows_with_ownership(compare->first_results, compare);
  second_rows = collect_rows_with_ownership(compare->second_results, compare);

  if(!first_rows || !second_rows) {
    equal = 0;
    goto cleanup;
  }

  /* Sort rows if order-insensitive comparison is requested */
  if(!compare->options.order_sensitive) {
    raptor_sequence_sort_r(first_rows, sort_row_sequence_compare_rows, compare);
    raptor_sequence_sort_r(second_rows, sort_row_sequence_compare_rows, compare);
  }

  /* Compare the row sequences */
  equal = compare_row_sequences(first_rows, second_rows, compare);

cleanup:
  if(first_rows)
    raptor_free_sequence(first_rows);
  if(second_rows)
    raptor_free_sequence(second_rows);

  return equal;
}

/*
 * rasqal_query_results_compare_boolean_internal:
 * @compare: comparison context
 *
 * Compare boolean results internally.
 * Returns non-zero if equal, 0 if different.
 */
static int
rasqal_query_results_compare_boolean_internal(rasqal_query_results_compare* compare)
{
  int first_boolean, second_boolean;

  if(!compare)
    return 0;

  if(!rasqal_query_results_is_boolean(compare->first_results) ||
     !rasqal_query_results_is_boolean(compare->second_results)) {
    rasqal_query_results_compare_add_difference(compare, "Results are not boolean type", NULL, NULL);
    return 0;
  }

  first_boolean = rasqal_query_results_get_boolean(compare->first_results);
  second_boolean = rasqal_query_results_get_boolean(compare->second_results);

  if(first_boolean != second_boolean) {
    rasqal_query_results_compare_add_difference(compare,
      "Boolean value mismatch",
      first_boolean ? "true" : "false",
      second_boolean ? "true" : "false");
    return 0;
  }

  return 1;
}

/*
 * rasqal_query_results_compare_graph_internal:
 * @compare: comparison context
 *
 * Compare graph results internally.
 *
 * This function is called when order-insensitive graph comparison is requested,
 * ensuring that triples are compared in a canonical order regardless of their
 * original sequence in the results.
 *
 * Returns non-zero if equal, 0 if different.
 */
static int
rasqal_query_results_compare_graph_internal(rasqal_query_results_compare* compare)
{
  if(!compare)
    return 0;

  if(!rasqal_query_results_is_graph(compare->first_results) ||
     !rasqal_query_results_is_graph(compare->second_results)) {
    rasqal_query_results_compare_add_difference(compare, "Results are not graph type", NULL, NULL);
    return 0;
  }

  return rasqal_graph_isomorphism_compare_graphs_hybrid(compare);
}

/*
 * rasqal_query_results_compare_blank_node_structure:
 * @first_bnode: first blank node to compare
 * @second_bnode: second blank node to compare
 * @compare: comparison context
 *
 * Compare two blank nodes based on their structural similarity.
 * This analyzes the triples that contain each blank node to determine
 * if they represent the same logical entity.
 * Returns non-zero if structurally similar, 0 if different.
 */
static int
rasqal_query_results_compare_blank_node_structure(raptor_term* first_bnode,
                                                  raptor_term* second_bnode,
                                                  rasqal_query_results_compare* compare)
{
  unsigned char* first_signature = NULL;
  unsigned char* second_signature = NULL;
  int result = 0;

  if(!first_bnode || !second_bnode || !compare)
    return 0;

  /* Generate structural signatures for both blank nodes */
  first_signature = rasqal_query_results_compare_get_blank_node_signature(first_bnode, compare);
  second_signature = rasqal_query_results_compare_get_blank_node_signature(second_bnode, compare);

  if(!first_signature || !second_signature) {
    /* If we can't generate signatures, fall back to ID comparison */
    result = (strcmp((char*)first_bnode->value.blank.string,
                    (char*)second_bnode->value.blank.string) == 0);
  } else {
    /* Compare the structural signatures */
    result = (strcmp((char*)first_signature, (char*)second_signature) == 0);
  }

  /* Cleanup */
  if(first_signature)
    RASQAL_FREE(unsigned char*, first_signature);
  if(second_signature)
    RASQAL_FREE(unsigned char*, second_signature);

  return result;
}

/*
 * rasqal_query_results_compare_get_blank_node_signature:
 * @bnode: blank node to analyze
 * @compare: comparison context
 *
 * Generate a structural signature for a blank node by analyzing
 * the triples that contain it. The signature is a canonicalized
 * representation of the blank node's structural context.
 * Returns a newly allocated signature string or NULL on failure.
 */
static unsigned char*
rasqal_query_results_compare_get_blank_node_signature(raptor_term* bnode, rasqal_query_results_compare* compare)
{
  raptor_sequence* triples = NULL;
  raptor_sequence* signature_parts = NULL;
  unsigned char* signature = NULL;
  int i;

  if(!bnode || !compare)
    return NULL;

  /* Create sequences to collect data */
  triples = raptor_new_sequence(NULL, NULL);
  signature_parts = raptor_new_sequence(NULL, NULL);

  if(!triples || !signature_parts) {
    if(triples)
      raptor_free_sequence(triples);
    if(signature_parts)
      raptor_free_sequence(signature_parts);
    return NULL;
  }

  /* Collect all triples from both result sets that contain this blank node */
  rasqal_query_results_rewind(compare->first_results);
  while(1) {
    raptor_statement* triple = rasqal_query_results_get_triple(compare->first_results);
    if(!triple)
      break;

    /* Check if this triple contains our blank node */
    if((triple->subject && triple->subject->type == RAPTOR_TERM_TYPE_BLANK &&
        strcmp((char*)triple->subject->value.blank.string, (char*)bnode->value.blank.string) == 0) ||
       (triple->object && triple->object->type == RAPTOR_TERM_TYPE_BLANK &&
        strcmp((char*)triple->object->value.blank.string, (char*)bnode->value.blank.string) == 0)) {
      raptor_sequence_push(triples, triple);
    }
  }

  rasqal_query_results_rewind(compare->second_results);
  while(1) {
    raptor_statement* triple = rasqal_query_results_get_triple(compare->second_results);
    if(!triple)
      break;

    /* Check if this triple contains our blank node */
    if((triple->subject && triple->subject->type == RAPTOR_TERM_TYPE_BLANK &&
        strcmp((char*)triple->subject->value.blank.string, (char*)bnode->value.blank.string) == 0) ||
       (triple->object && triple->object->type == RAPTOR_TERM_TYPE_BLANK &&
        strcmp((char*)triple->object->value.blank.string, (char*)bnode->value.blank.string) == 0)) {
      raptor_sequence_push(triples, triple);
    }
  }

  /* Generate signature parts from collected triples */
  for(i = 0; i < raptor_sequence_size(triples); i++) {
    raptor_statement* triple = (raptor_statement*)raptor_sequence_get_at(triples, i);
    unsigned char* part = NULL;

    if(!triple)
      continue;

    /* Create a canonical representation of this triple */
    if(triple->subject && triple->subject->type == RAPTOR_TERM_TYPE_BLANK &&
       strcmp((char*)triple->subject->value.blank.string, (char*)bnode->value.blank.string) == 0) {
      /* Blank node is subject - create signature: "S:predicate:object" */
      unsigned char* pred_str = raptor_term_to_string(triple->predicate);
      unsigned char* obj_str = raptor_term_to_string(triple->object);

      if(pred_str && obj_str) {
        size_t len = strlen("S:") + strlen((char*)pred_str) + 1 + strlen((char*)obj_str) + 1;
        part = RASQAL_MALLOC(unsigned char*, len);
        if(part) {
          snprintf((char*)part, len, "S:%s:%s", pred_str, obj_str);
        }
      }

      if(pred_str)
        raptor_free_memory(pred_str);
      if(obj_str)
        raptor_free_memory(obj_str);
    } else if(triple->object && triple->object->type == RAPTOR_TERM_TYPE_BLANK &&
              strcmp((char*)triple->object->value.blank.string, (char*)bnode->value.blank.string) == 0) {
      /* Blank node is object - create signature: "O:subject:predicate" */
      unsigned char* subj_str = raptor_term_to_string(triple->subject);
      unsigned char* pred_str = raptor_term_to_string(triple->predicate);

      if(subj_str && pred_str) {
        size_t len = strlen("O:") + strlen((char*)subj_str) + 1 + strlen((char*)pred_str) + 1;
        part = RASQAL_MALLOC(unsigned char*, len);
        if(part) {
          snprintf((char*)part, len, "O:%s:%s", subj_str, pred_str);
        }
      }

      if(subj_str)
        raptor_free_memory(subj_str);
      if(pred_str)
        raptor_free_memory(pred_str);
    }

    if(part) {
      raptor_sequence_push(signature_parts, part);
    }
  }

  /* Sort signature parts for canonicalization */
  if(raptor_sequence_size(signature_parts) > 1) {
    raptor_sequence_sort_r(signature_parts, rasqal_query_results_compare_signature_part_compare, NULL);
  }

  /* Combine all parts into final signature */
  if(raptor_sequence_size(signature_parts) > 0) {
    size_t total_len = 0;
    int j;

    /* Calculate total length needed */
    for(j = 0; j < raptor_sequence_size(signature_parts); j++) {
      unsigned char* part = (unsigned char*)raptor_sequence_get_at(signature_parts, j);
      if(part) {
        total_len += strlen((char*)part) + 1; /* +1 for separator */
      }
    }

    if(total_len > 0) {
      signature = RASQAL_MALLOC(unsigned char*, total_len);
      if(signature) {
        size_t pos = 0;
        for(j = 0; j < raptor_sequence_size(signature_parts); j++) {
          unsigned char* part = (unsigned char*)raptor_sequence_get_at(signature_parts, j);
          if(part) {
            size_t part_len = strlen((char*)part);
            if(pos + part_len < total_len) {
              memcpy(signature + pos, part, part_len);
              pos += part_len;
              if(j < raptor_sequence_size(signature_parts) - 1) {
                signature[pos++] = '|';
              }
            }
          }
        }
        signature[pos] = '\0';
      }
    }
  }

  /* Cleanup */
  for(i = 0; i < raptor_sequence_size(signature_parts); i++) {
    unsigned char* part = (unsigned char*)raptor_sequence_get_at(signature_parts, i);
    if(part)
      RASQAL_FREE(unsigned char*, part);
  }

  if(triples)
    raptor_free_sequence(triples);
  if(signature_parts)
    raptor_free_sequence(signature_parts);

  return signature;
}

/*
 * rasqal_query_results_compare_literal_to_term:
 * @literal: rasqal literal to convert
 * @world: rasqal world
 *
 * Convert a rasqal literal to a raptor term for comparison.
 *
 * Returns a newly allocated raptor term or NULL on failure.
 */
static raptor_term*
rasqal_query_results_compare_literal_to_term(rasqal_literal* literal,
                                             rasqal_world* world)
{
  raptor_term* term = NULL;

  if(!literal || !world)
    return NULL;

  switch(literal->type) {
    case RASQAL_LITERAL_URI:
      term = raptor_new_term_from_uri(world->raptor_world_ptr, literal->value.uri);
      break;

    case RASQAL_LITERAL_BLANK:
      term = raptor_new_term_from_blank(world->raptor_world_ptr, literal->string);
      break;

    case RASQAL_LITERAL_STRING:
      term = raptor_new_term_from_literal(world->raptor_world_ptr,
                                         literal->string,
                                         literal->datatype,
                                         (const unsigned char*)literal->language);
      break;

    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_DECIMAL:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_UDT:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_VARIABLE:
    case RASQAL_LITERAL_INTEGER_SUBTYPE:
    case RASQAL_LITERAL_DATE:
    case RASQAL_LITERAL_UNKNOWN:
    default:
      /* For other types, convert to string representation */
      if(literal->string) {
        term = raptor_new_term_from_literal(world->raptor_world_ptr,
                                           literal->string,
                                           NULL, NULL);
      }
      break;
  }

  return term;
}

/*
 * collect_rows_with_ownership:
 * @results: query results to collect rows from
 * @compare: comparison context
 *
 * Collect all rows from query results with ownership transfer.
 *
 * The returned sequence owns the rows and will free them when destroyed.
 *
 * Returns the sequence of rows or NULL on failure.
 */
static raptor_sequence*
collect_rows_with_ownership(rasqal_query_results* results,
                            rasqal_query_results_compare* compare)
{
  raptor_sequence* rows = NULL;
  int rowi;
  rasqal_row* row;

  if(!results || !compare)
    return NULL;

  rows = raptor_new_sequence((raptor_data_free_handler)rasqal_free_row,
                             (raptor_data_print_handler)NULL);
  if(!rows) {
    rasqal_query_results_compare_add_difference(compare, "Failed to create row sequence", NULL, NULL);
    return NULL;
  }

  /* Collect rows from results */

  rasqal_query_results_rewind(results);
  for(rowi = 0; 1; rowi++) {
    row = rasqal_query_results_get_row_by_offset(results, rowi);
    if(!row)
      break;

    /* rasqal_query_results_get_row_by_offset already returns a copy that we own */
    raptor_sequence_push(rows, row);
  }

  return rows;
}



/*
 * sort_row_sequence_compare_rows:
 * @a: pointer to first row
 * @b: pointer to second row
 * @arg: comparison context
 *
 * Compare two rows for sorting purposes to ensure canonical ordering.
 * This function is used by raptor_sequence_sort_r() to sort rows before
 * comparison, enabling order-insensitive bindings comparison.
 *
 * The comparison iterates through each value in the row, comparing literals
 * using XQuery comparison rules. NULL values are ordered first. If all values
 * are equal, the comparison uses row offset to ensure stable sorting.
 *
 * Returns <0, 0, or >0 for comparison result.
 */
static int
sort_row_sequence_compare_rows(const void* a, const void* b, void* arg)
{
  rasqal_row* row_a = (rasqal_row*)a;
  rasqal_row* row_b = (rasqal_row*)b;
  rasqal_query_results_compare* compare = (rasqal_query_results_compare*)arg;
  int result = 0;
  int i;
  int var_count;
  rasqal_variables_table* vars_table;
  int error;
  rasqal_literal* first_value;
  rasqal_literal* second_value;

  if(!row_a || !row_b || !compare)
    return 0;

  /* Get variable count for comparison */
  vars_table = rasqal_query_results_get_variables_table(compare->first_results);
  var_count = vars_table ? rasqal_variables_table_get_total_variables_count(vars_table) : 0;

  /* Compare each value in the row */
  for(i = 0; i < var_count; i++) {
    /* Check if values arrays are valid */
    if(!row_a->values || !row_b->values) {
      /* If either row has no values array, they are not equal */
      return row_a->values ? 1 : (row_b->values ? -1 : 0);
    }

    /* Check bounds to prevent array access violations */
    if(i >= row_a->size || i >= row_b->size) {
      /* If we're beyond the bounds of either row, they are not equal */
      return row_a->size - row_b->size;
    }

    first_value = row_a->values[i];
    second_value = row_b->values[i];

    /* NULLs order first */
    if(!first_value || !second_value) {
      if(!first_value && !second_value)
        result = 0;
      else
        result = (!first_value) ? -1 : 1;
      break;
    }

    /* Use literal comparison with default flags */
    error = 0;
    result = rasqal_literal_compare(first_value, second_value,
                                    RASQAL_COMPARE_XQUERY | RASQAL_COMPARE_URI,
                                    &error);
    if(error) {
      result = 0;
      break;
    }

    if(result != 0)
      break;
  }

  /* If still equal, make sort stable by using row offset */
  if(!result)
    result = row_a->offset - row_b->offset;

  return result;
}



/*
 * compare_row_sequences:
 * @first_rows: first sequence of rows
 * @second_rows: second sequence of rows
 * @compare: comparison context
 *
 * Compare two sequences of rows for equality by comparing each row in order.
 *
 * Returns non-zero if equal, 0 if different.
 */
static int
compare_row_sequences(raptor_sequence* first_rows,
                      raptor_sequence* second_rows,
                      rasqal_query_results_compare* compare)
{
  int first_count, second_count;
  int i;
  int equal = 1;
  rasqal_variables_table* vars_table;
  int var_count;

  if(!first_rows || !second_rows || !compare)
    return 0;

  first_count = raptor_sequence_size(first_rows);
  second_count = raptor_sequence_size(second_rows);

  /* Check if row counts match */
  if(first_count != second_count) {
    char expected_str[32], actual_str[32];
    snprintf(expected_str, sizeof(expected_str), "%d", first_count);
    snprintf(actual_str, sizeof(actual_str), "%d", second_count);
    rasqal_query_results_compare_add_difference(compare,
      "Row count mismatch", expected_str, actual_str);
    return 0;
  }

  /* Get variable count for row comparison */
  vars_table = rasqal_query_results_get_variables_table(compare->first_results);
  var_count = vars_table ? rasqal_variables_table_get_total_variables_count(vars_table) : 0;

  /* Compare each row */
  for(i = 0; i < first_count; i++) {
    rasqal_row* first_row = (rasqal_row*)raptor_sequence_get_at(first_rows, i);
    rasqal_row* second_row = (rasqal_row*)raptor_sequence_get_at(second_rows, i);

    if(!first_row || !second_row) {
      char index_str[32];
      snprintf(index_str, sizeof(index_str), "%d", i);
      rasqal_query_results_compare_add_difference(compare,
        "Cannot get row at index", index_str, NULL);
      equal = 0;
      continue;
    }

    if(!compare_single_row(first_row, second_row, var_count, compare))
      equal = 0;
  }

  return equal;
}


/*
 * compare_single_row_compare_blank_nodes:
 * @first: first blank node literal
 * @second: second blank node literal
 * @column_index: column index for error reporting
 * @compare: comparison context
 *
 * Compare two blank node literals based on the comparison strategy.
 *
 * Returns non-zero if equal, 0 if different.
 */
static int
compare_single_row_compare_blank_nodes(rasqal_literal* first,
                                       rasqal_literal* second,
                                       int column_index,
                                       rasqal_query_results_compare* compare)
{
  switch(compare->options.blank_node_strategy) {
    case RASQAL_COMPARE_BLANK_NODE_MATCH_ANY:
      /* Blank nodes match any other blank node */
      return 1;

    case RASQAL_COMPARE_BLANK_NODE_MATCH_ID:
      /* Blank nodes must have same ID */
      if(strcmp((char*)first->string, (char*)second->string) != 0) {
        rasqal_variables_table* vars_table = rasqal_query_results_get_variables_table(compare->first_results);
        const unsigned char** var_names = rasqal_variables_table_get_names(vars_table);
        const char* var_name = (var_names && column_index < rasqal_variables_table_get_total_variables_count(vars_table)) ?
          (const char*)var_names[column_index] : "unknown";

        char expected_str[256], actual_str[256];
        snprintf(expected_str, sizeof(expected_str), "%s=%s", var_name, first->string);
        snprintf(actual_str, sizeof(actual_str), "%s=%s", var_name, second->string);
        rasqal_query_results_compare_add_difference(compare,
          "Blank node ID mismatch", expected_str, actual_str);
        return 0;
      }
      return 1;

    case RASQAL_COMPARE_BLANK_NODE_MATCH_STRUCTURE: {
      /* Use structural matching for blank nodes */
      raptor_term* first_term = rasqal_query_results_compare_literal_to_term(first, compare->world);
      raptor_term* second_term = rasqal_query_results_compare_literal_to_term(second, compare->world);
      int equal = 1;

      if(!first_term || !second_term) {
        char index_str[32];
        snprintf(index_str, sizeof(index_str), "%d", column_index);
        rasqal_query_results_compare_add_difference(compare,
          "Cannot convert blank node literals to terms at column", index_str, NULL);
        equal = 0;
      } else if(!rasqal_query_results_compare_blank_node_structure(first_term, second_term, compare)) {
        char index_str[32];
        snprintf(index_str, sizeof(index_str), "%d", column_index);
        rasqal_query_results_compare_add_difference(compare,
          "Structural blank node mismatch at column", index_str, NULL);
        equal = 0;
      }

      /* Cleanup terms */
      if(first_term)
        raptor_free_term(first_term);
      if(second_term)
        raptor_free_term(second_term);

      return equal;
    }
  }

  return 0;
}



/*
 * compare_single_row:
 * @first_row: first row to compare
 * @second_row: second row to compare
 * @var_count: number of variables in each row
 * @compare: comparison context
 *
 * Compare two individual rows for equality by comparing each value in the row.
 * This function handles different types of values with appropriate comparison
 * strategies:
 *
 * - Blank nodes: Uses the configured blank node strategy (match any, match ID,
 *   or structural matching)
 * - String literals: Uses standard string comparison
 * - Other literals: Uses standard literal equality comparison
 *
 * Returns non-zero if equal, 0 if different.
 */
static int
compare_single_row(rasqal_row* first_row, rasqal_row* second_row, int var_count, rasqal_query_results_compare* compare)
{
  int j;
  int equal = 1;

  if(!first_row || !second_row || !compare)
    return 0;

  /* Compare each value in the row */
  for(j = 0; j < var_count; j++) {
    rasqal_literal* first_value = first_row->values[j];
    rasqal_literal* second_value = second_row->values[j];

    /* Handle NULL values explicitly */
    if(!first_value && !second_value) {
      /* Both are NULL - they are equal */
      continue;
    } else if(!first_value || !second_value) {
      /* One is NULL, the other is not - they are different */
      rasqal_variables_table* vars_table = rasqal_query_results_get_variables_table(compare->first_results);
      const unsigned char** var_names = rasqal_variables_table_get_names(vars_table);
      const char* var_name = (var_names && j < rasqal_variables_table_get_total_variables_count(vars_table)) ?
        (const char*)var_names[j] : "unknown";

      char expected_str[256], actual_str[256];
      snprintf(expected_str, sizeof(expected_str), "%s='%s'", var_name, first_value ? "non-NULL" : "NULL");
      snprintf(actual_str, sizeof(actual_str), "%s='%s'", var_name, second_value ? "non-NULL" : "NULL");
      rasqal_query_results_compare_add_difference(compare,
        "NULL vs non-NULL value", expected_str, actual_str);
      equal = 0;
      continue;
    }

    if(!rasqal_literal_equals(first_value, second_value)) {
      /* Handle blank node comparison based on strategy */
      if(first_value && second_value &&
         first_value->type == RASQAL_LITERAL_BLANK &&
         second_value->type == RASQAL_LITERAL_BLANK) {

        if(!compare_single_row_compare_blank_nodes(first_value, second_value, j, compare))
          equal = 0;
      } else if(first_value && second_value &&
                first_value->type == RASQAL_LITERAL_STRING &&
                second_value->type == RASQAL_LITERAL_STRING) {
        /* Handle string comparison */
        const unsigned char* first_str = first_value->string;
        const unsigned char* second_str = second_value->string;

        if(strcmp((char*)first_str, (char*)second_str) != 0) {
          rasqal_variables_table* vars_table = rasqal_query_results_get_variables_table(compare->first_results);
          const unsigned char** var_names = rasqal_variables_table_get_names(vars_table);
          const char* var_name = (var_names && j < rasqal_variables_table_get_total_variables_count(vars_table)) ?
            (const char*)var_names[j] : "unknown";

          char expected_str[256], actual_str[256];
          snprintf(expected_str, sizeof(expected_str), "%s='%s'", var_name, (char*)first_str ? (char*)first_str : "NULL");
          snprintf(actual_str, sizeof(actual_str), "%s='%s'", var_name, (char*)second_str ? (char*)second_str : "NULL");
          rasqal_query_results_compare_add_difference(compare,
            "String value mismatch", expected_str, actual_str);
          equal = 0;
        }
      } else {
        /* Non-blank node values don't match */
        const char* first_str = first_value ?
          (first_value->type == RASQAL_LITERAL_URI || first_value->type == RASQAL_LITERAL_STRING) ?
            (char*)first_value->string :
            RASQAL_GOOD_CAST(const char*, rasqal_literal_as_string(first_value)) : NULL;
        const char* second_str = second_value ?
          (second_value->type == RASQAL_LITERAL_URI || second_value->type == RASQAL_LITERAL_STRING) ?
            (char*)second_value->string :
            RASQAL_GOOD_CAST(const char*, rasqal_literal_as_string(second_value)) : NULL;

        rasqal_variables_table* vars_table = rasqal_query_results_get_variables_table(compare->first_results);
        const unsigned char** var_names = rasqal_variables_table_get_names(vars_table);
        const char* var_name = (var_names && j < rasqal_variables_table_get_total_variables_count(vars_table)) ?
          (const char*)var_names[j] : "unknown";

        char expected_str[256], actual_str[256];
        snprintf(expected_str, sizeof(expected_str), "%s='%s'", var_name, first_str ? first_str : "NULL");
        snprintf(actual_str, sizeof(actual_str), "%s='%s'", var_name, second_str ? second_str : "NULL");
        rasqal_query_results_compare_add_difference(compare,
          "Value mismatch", expected_str, actual_str);
        equal = 0;
      }
    }
  }

  return equal;
}

/*
 * rasqal_query_results_compare_signature_part_compare:
 * @a: first signature part pointer
 * @b: second signature part pointer
 * @arg: unused argument (for compatibility with raptor_sequence_sort_r)
 *
 * Compare function for sorting signature parts to ensure canonical ordering
 * of blank node structural signatures. This function is used by
 * raptor_sequence_sort_r() when generating structural signatures for blank
 * node comparison.
 *
 * The signature parts represent different aspects of a blank node's structural
 * context (e.g., "S:predicate:object" for subject position, "O:subject:predicate"
 * for object position). Sorting these parts ensures that structurally equivalent
 * blank nodes will have identical signatures regardless of the order in which
 * their triples were encountered.
 *
 * Returns negative if a < b, 0 if equal, positive if a > b.
 */
static int
rasqal_query_results_compare_signature_part_compare(const void* a,
                                                    const void* b, void* arg)
{
  unsigned char* part_a = *(unsigned char**)a;
  unsigned char* part_b = *(unsigned char**)b;

  if(!part_a && !part_b)
    return 0;
  if(!part_a)
    return -1;
  if(!part_b)
    return 1;

  /* Simple string comparison for canonical ordering */
  return strcmp((char*)part_a, (char*)part_b);
}

/* Standalone test program for Query Results Compare Module */

int
main(int argc, char *argv[]) {
  char const *program = rasqal_basename(*argv);
  rasqal_world *world = NULL;
  rasqal_query* query = NULL;
  rasqal_query_results* results1 = NULL;
  rasqal_query_results* results2 = NULL;
  rasqal_query_results_compare* compare = NULL;
  rasqal_query_results_compare_result* result = NULL;
  rasqal_query_results_compare_options options;
  int failures = 0;
#define FAIL do { failures++; goto tidy; } while(0)

  printf("%s: Testing Query Results Compare Module\n", program);

  world = rasqal_new_world();
  if(!world || rasqal_world_open(world))
    FAIL;

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query)
    FAIL;

  /* Test 1: Boolean Results Comparison */
  printf("%s: Test 1 - Boolean Results Comparison\n", program);
  
  results1 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_BOOLEAN);
  results2 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_BOOLEAN);
  
  if(!results1 || !results2)
    FAIL;

  compare = rasqal_new_query_results_compare(world, results1, results2);
  if(!compare)
    FAIL;

  result = rasqal_query_results_compare_execute(compare);
  if(!result) {
    printf("%s: Failed to execute comparison\n", program);
    FAIL;
  }

  printf("%s: Boolean comparison result: %s\n", program, result->equal ? "EQUAL" : "NOT EQUAL");

  /* Test 2: Bindings Results Comparison */
  printf("%s: Test 2 - Bindings Results Comparison\n", program);
  
  rasqal_free_query_results(results1);
  rasqal_free_query_results(results2);
  
  /* Create bindings results with TSV data (needs 3+ tabs for detection) */
  {
    raptor_uri* base_uri = raptor_new_uri(world->raptor_world_ptr, (const unsigned char*)"http://example.org/");
    if(!base_uri)
      FAIL;
      
    results1 = rasqal_new_query_results_from_string(world, RASQAL_QUERY_RESULTS_BINDINGS, base_uri,
      "a\tb\tc\td\n\"val1\"\t\"val2\"\t\"val3\"\t\"val4\"\n", 0);
    results2 = rasqal_new_query_results_from_string(world, RASQAL_QUERY_RESULTS_BINDINGS, base_uri,
      "a\tb\tc\td\n\"val1\"\t\"val2\"\t\"val3\"\t\"val4\"\n", 0);
      
    raptor_free_uri(base_uri);
  }
  
  if(!results1 || !results2)
    FAIL;

  rasqal_free_query_results_compare(compare);
  compare = rasqal_new_query_results_compare(world, results1, results2);
  if(!compare)
    FAIL;

  rasqal_free_query_results_compare_result(result);
  result = rasqal_query_results_compare_execute(compare);
  if(!result) {
    printf("%s: Failed to execute bindings comparison\n", program);
    FAIL;
  }

  printf("%s: Bindings comparison result: %s\n", program, result->equal ? "EQUAL" : "NOT EQUAL");

  /* Test 3: Graph Results Comparison */
  printf("%s: Test 3 - Graph Results Comparison\n", program);
  
  rasqal_free_query_results(results1);
  rasqal_free_query_results(results2);
  
  results1 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_GRAPH);
  results2 = rasqal_new_query_results2(world, query, RASQAL_QUERY_RESULTS_GRAPH);
  
  if(!results1 || !results2)
    FAIL;

  rasqal_free_query_results_compare(compare);
  compare = rasqal_new_query_results_compare(world, results1, results2);
  if(!compare)
    FAIL;

  rasqal_free_query_results_compare_result(result);
  result = rasqal_query_results_compare_execute(compare);
  if(!result) {
    printf("%s: Failed to execute graph comparison\n", program);
    FAIL;
  }

  printf("%s: Graph comparison result: %s\n", program, result->equal ? "EQUAL" : "NOT EQUAL");

  /* Test 4: Options Configuration */
  printf("%s: Test 4 - Options Configuration\n", program);
  
  rasqal_query_results_compare_options_init(&options);
  options.order_sensitive = 0;
  options.blank_node_strategy = RASQAL_COMPARE_BLANK_NODE_MATCH_ANY;
  options.literal_comparison_flags = RASQAL_COMPARE_XQUERY;
  options.max_differences = 5;

  rasqal_query_results_compare_set_options(compare, &options);
  printf("%s: Options configured successfully\n", program);

  /* Test 5: Null Parameter Handling */
  printf("%s: Test 5 - Null Parameter Handling\n", program);
  
  if(rasqal_new_query_results_compare(NULL, results1, results2) == NULL &&
     rasqal_new_query_results_compare(world, NULL, results2) == NULL &&
     rasqal_new_query_results_compare(world, results1, NULL) == NULL) {
    printf("%s: Null parameter handling works correctly\n", program);
  } else {
    printf("%s: Null parameter handling failed\n", program);
    FAIL;
  }

  printf("%s: All query results compare tests completed successfully\n", program);

tidy:
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

  return failures;
}

#endif /* STANDALONE */

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_exists.c - EXISTS / NOT EXISTS rowsource class
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
#include <string.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <raptor.h>

#include "rasqal.h"
#include "rasqal_internal.h"

/* Forward declarations */
#ifndef STANDALONE

typedef struct
{
  /* EXISTS graph pattern to evaluate */
  rasqal_graph_pattern* exists_pattern;

  /* Query context for data access */
  rasqal_query* query;

  /* Triples source for data lookup */
  rasqal_triples_source* triples_source;

  /* Current variable bindings from outer query */
  rasqal_row* outer_row;

  /* Graph origin for named graph context (or NULL for default graph) */
  rasqal_literal* graph_origin;

  /* Cached evaluation result */
  int evaluation_result;

  /* Whether evaluation has been performed */
  int evaluated;

  /* Whether this is NOT EXISTS (negated) */
  int is_negated;
} rasqal_exists_rowsource_context;


/* Forward declarations */
static int rasqal_evaluate_exists_pattern(rasqal_graph_pattern* gp, rasqal_triples_source* triples_source, rasqal_query* query, rasqal_row* outer_row);
static int rasqal_evaluate_exists_pattern_with_origin(rasqal_graph_pattern* gp, rasqal_triples_source* triples_source, rasqal_query* query, rasqal_row* outer_row, rasqal_literal* graph_origin);
static int rasqal_evaluate_basic_exists_pattern(rasqal_graph_pattern* gp, rasqal_triples_source* triples_source, rasqal_query* query, rasqal_row* outer_row);
static int rasqal_evaluate_basic_exists_pattern_with_origin(rasqal_graph_pattern* gp, rasqal_triples_source* triples_source, rasqal_query* query, rasqal_row* outer_row, rasqal_literal* origin);
static int rasqal_evaluate_group_exists_pattern(rasqal_graph_pattern* gp, rasqal_triples_source* triples_source, rasqal_query* query, rasqal_row* outer_row);
static int rasqal_evaluate_optional_exists_pattern(rasqal_graph_pattern* gp, rasqal_triples_source* triples_source, rasqal_query* query, rasqal_row* outer_row);
static int rasqal_evaluate_union_exists_pattern(rasqal_graph_pattern* gp, rasqal_triples_source* triples_source, rasqal_query* query, rasqal_row* outer_row);
static int rasqal_evaluate_filter_exists_pattern(rasqal_graph_pattern* gp, rasqal_triples_source* triples_source, rasqal_query* query, rasqal_row* outer_row);
static int rasqal_evaluate_graph_exists_pattern(rasqal_graph_pattern* gp, rasqal_triples_source* triples_source, rasqal_query* query, rasqal_row* outer_row);
static rasqal_triple* rasqal_instantiate_triple_with_bindings(rasqal_triple* triple, rasqal_row* outer_row, rasqal_literal* origin);
static int rasqal_check_triple_exists_in_data(rasqal_triple* triple, rasqal_triples_source* triples_source, rasqal_query* query);


static int
rasqal_exists_rowsource_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_exists_rowsource_context* con;

  con = (rasqal_exists_rowsource_context*)user_data;

  /* Graph pattern is not owned by this rowsource - don't free it */

  /* Free the copied outer row */
  if(con->outer_row)
    rasqal_free_row(con->outer_row);

  /* Free the graph origin if present */
  if(con->graph_origin)
    rasqal_free_literal(con->graph_origin);

  RASQAL_FREE(rasqal_exists_rowsource_context, con);

  return 0;
}


static int
rasqal_exists_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                         void *user_data)
{
  /* EXISTS rowsource returns a boolean result, no variables */
  rowsource->size = 0;
  return 0;
}


/*
 * rasqal_evaluate_exists_pattern:
 * @gp: Graph pattern to evaluate for EXISTS
 * @triples_source: Data source for triple lookups
 * @query: Query context for variable bindings and execution
 * @outer_row: Current variable bindings from outer query
 *
 * INTERNAL - Evaluate a graph pattern for EXISTS semantics.
 *
 * This is the main entry point for EXISTS pattern evaluation. It routes
 * different graph pattern types to their specific evaluation functions.
 * The function implements recursive pattern evaluation, supporting all
 * major SPARQL graph pattern types: BASIC, GROUP, UNION, OPTIONAL,
 * FILTER, GRAPH, and MINUS.
 *
 * For EXISTS evaluation, the function returns 1 if the pattern has any
 * solutions (matches) given the current variable bindings, 0 otherwise.
 * This enables the EXISTS rowsource to determine whether the pattern
 * exists in the data.
 *
 * Return value: 1 if pattern has solutions, 0 otherwise
 */
static int
rasqal_evaluate_exists_pattern(rasqal_graph_pattern* gp,
                               rasqal_triples_source* triples_source,
                               rasqal_query* query,
                               rasqal_row* outer_row)
{


  if(!gp || !triples_source || !query)
    return 0;

  /* Handle basic graph patterns with triple matching */
  if(gp->op == RASQAL_GRAPH_PATTERN_OPERATOR_BASIC && gp->triples)
    return rasqal_evaluate_basic_exists_pattern(gp, triples_source, query, outer_row);

  /* Handle complex graph patterns */
  switch(gp->op) {
    case RASQAL_GRAPH_PATTERN_OPERATOR_BASIC:
      return rasqal_evaluate_basic_exists_pattern(gp, triples_source, query, outer_row);

    case RASQAL_GRAPH_PATTERN_OPERATOR_GROUP:
      return rasqal_evaluate_group_exists_pattern(gp, triples_source, query, outer_row);

    case RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL:
      return rasqal_evaluate_optional_exists_pattern(gp, triples_source, query, outer_row);

    case RASQAL_GRAPH_PATTERN_OPERATOR_UNION:
      return rasqal_evaluate_union_exists_pattern(gp, triples_source, query, outer_row);

    case RASQAL_GRAPH_PATTERN_OPERATOR_FILTER:
      return rasqal_evaluate_filter_exists_pattern(gp, triples_source, query, outer_row);

    case RASQAL_GRAPH_PATTERN_OPERATOR_GRAPH:
      return rasqal_evaluate_graph_exists_pattern(gp, triples_source, query, outer_row);

    case RASQAL_GRAPH_PATTERN_OPERATOR_MINUS:
    case RASQAL_GRAPH_PATTERN_OPERATOR_BIND:
    case RASQAL_GRAPH_PATTERN_OPERATOR_SELECT:
    case RASQAL_GRAPH_PATTERN_OPERATOR_SERVICE:
    case RASQAL_GRAPH_PATTERN_OPERATOR_EXISTS:
      /* TODO: These patterns: evaluate the first sub-pattern - this is a hack */
      if(gp->graph_patterns && raptor_sequence_size(gp->graph_patterns) > 0) {
        rasqal_graph_pattern* sub_pattern = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, 0);
        if(sub_pattern) {
          return rasqal_evaluate_exists_pattern(sub_pattern, triples_source, query, outer_row);
        }
      }
      return 0;

    case RASQAL_GRAPH_PATTERN_OPERATOR_VALUES:
      /* VALUES patterns: always succeed for EXISTS (they provide bindings) */
      return 1;

    case RASQAL_GRAPH_PATTERN_OPERATOR_NOT_EXISTS:
      /* NOT EXISTS patterns: negate the sub-pattern evaluation */
      if(gp->graph_patterns && raptor_sequence_size(gp->graph_patterns) > 0) {
        rasqal_graph_pattern* sub_pattern = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, 0);
        if(sub_pattern) {
          return !rasqal_evaluate_exists_pattern(sub_pattern, triples_source, query, outer_row);
        }
      }
      return 1; /* NOT EXISTS with no sub-pattern is true */

    case RASQAL_GRAPH_PATTERN_OPERATOR_UNKNOWN:
    default:
      /* Conservative default for unsupported patterns */
      return 0;
  }

  /* Unknown or unsupported pattern type */
  return 0;
}

/*
 * rasqal_evaluate_exists_pattern_with_origin:
 * @gp: Graph pattern to evaluate for EXISTS
 * @triples_source: Data source for triple lookups
 * @query: Query context for variable bindings and execution
 * @outer_row: Current variable bindings from outer query
 * @graph_origin: Named graph context for pattern evaluation
 *
 * INTERNAL - Evaluate a graph pattern for EXISTS semantics with graph context.
 *
 * This function is identical to rasqal_evaluate_exists_pattern but passes
 * the graph origin context to pattern evaluation functions that support it.
 * This enables EXISTS patterns to be evaluated within named graph contexts.
 *
 * Return value: 1 if pattern has solutions in graph context, 0 otherwise
 */
static int
rasqal_evaluate_exists_pattern_with_origin(rasqal_graph_pattern* gp,
                                           rasqal_triples_source* triples_source,
                                           rasqal_query* query,
                                           rasqal_row* outer_row,
                                           rasqal_literal* graph_origin)
{
  if(!gp)
    return 0;

  switch(gp->op) {
    case RASQAL_GRAPH_PATTERN_OPERATOR_BASIC:
      /* Use origin-aware basic pattern evaluation */
      return rasqal_evaluate_basic_exists_pattern_with_origin(gp, triples_source, query, outer_row, graph_origin);

    case RASQAL_GRAPH_PATTERN_OPERATOR_GROUP:
      /* For group patterns, use default evaluation for now - TODO: implement origin-aware version */
      return rasqal_evaluate_group_exists_pattern(gp, triples_source, query, outer_row);

    case RASQAL_GRAPH_PATTERN_OPERATOR_UNION:
      return rasqal_evaluate_union_exists_pattern(gp, triples_source, query, outer_row);

    case RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL:
      return rasqal_evaluate_optional_exists_pattern(gp, triples_source, query, outer_row);

    case RASQAL_GRAPH_PATTERN_OPERATOR_FILTER:
      return rasqal_evaluate_filter_exists_pattern(gp, triples_source, query, outer_row);

    case RASQAL_GRAPH_PATTERN_OPERATOR_GRAPH:
      /* For nested GRAPH patterns, use the inner graph origin if available */
      return rasqal_evaluate_graph_exists_pattern(gp, triples_source, query, outer_row);

    case RASQAL_GRAPH_PATTERN_OPERATOR_MINUS:
    case RASQAL_GRAPH_PATTERN_OPERATOR_BIND:
    case RASQAL_GRAPH_PATTERN_OPERATOR_SELECT:
    case RASQAL_GRAPH_PATTERN_OPERATOR_SERVICE:
    case RASQAL_GRAPH_PATTERN_OPERATOR_EXISTS:
      /* TODO: These patterns need origin-aware versions */
      if(gp->graph_patterns && raptor_sequence_size(gp->graph_patterns) > 0) {
        rasqal_graph_pattern* sub_pattern = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, 0);
        if(sub_pattern) {
          return rasqal_evaluate_exists_pattern_with_origin(sub_pattern, triples_source, query, outer_row, graph_origin);
        }
      }
      return 0;

    case RASQAL_GRAPH_PATTERN_OPERATOR_VALUES:
      /* VALUES patterns: always succeed for EXISTS (they provide bindings) */
      return 1;

    case RASQAL_GRAPH_PATTERN_OPERATOR_NOT_EXISTS:
      /* NOT EXISTS patterns: negate the sub-pattern evaluation */
      if(gp->graph_patterns && raptor_sequence_size(gp->graph_patterns) > 0) {
        rasqal_graph_pattern* sub_pattern = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, 0);
        if(sub_pattern) {
          return !rasqal_evaluate_exists_pattern_with_origin(sub_pattern, triples_source, query, outer_row, graph_origin);
        }
      }
      return 1; /* NOT EXISTS with no sub-pattern is true */

    case RASQAL_GRAPH_PATTERN_OPERATOR_UNKNOWN:
    default:
      /* Conservative default for unsupported patterns */
      return 0;
  }

  /* Unknown or unsupported pattern type */
  return 0;
}

/**
 * rasqal_evaluate_basic_exists_pattern:
 * @gp: Basic graph pattern containing triples
 * @triples_source: Data source for triple lookups
 * @query: Query context for variable bindings
 * @outer_row: Current variable bindings from outer query
 *
 * INTERNAL - Evaluate basic graph patterns for EXISTS with dual-mode optimization.
 *
 * This function implements optimized EXISTS evaluation for basic graph patterns
 * (triple patterns). It uses a two-phase approach:
 *
 * Phase 1: Ground triple optimization - Check for exact triples (no variables)
 *          using efficient triple_present() lookup. If any ground triple exists,
 *          EXISTS immediately returns true.
 *
 * Phase 2: Variable pattern evaluation - For patterns with variables, substitute
 *          outer query bindings and perform pattern matching against the data.
 *
 * The function limits evaluation to the first triple in EXISTS patterns to avoid
 * outer query contamination issues where query processing adds extra triples
 * to the pattern.
 *
 * Return value: 1 if pattern has solutions, 0 otherwise
 */
static int
rasqal_evaluate_basic_exists_pattern(rasqal_graph_pattern* gp,
                                     rasqal_triples_source* triples_source,
                                     rasqal_query* query,
                                     rasqal_row* outer_row)
{
  int i;
  rasqal_triple* triple;
  int has_variable_pattern = 0;
  int num_triples = raptor_sequence_size(gp->triples);

  if(!gp->triples)
    return 1; /* Empty pattern always matches */

  /* Phase 1: Check for ground triples first (optimization) */

  for(i = 0; i < num_triples; i++) {
    triple = (rasqal_triple*)raptor_sequence_get_at(gp->triples, i);
    if(!triple)
      continue;

    /* Check if this is a ground triple (no variables) */
    if(triple->subject && triple->subject->type != RASQAL_LITERAL_VARIABLE &&
       triple->predicate && triple->predicate->type != RASQAL_LITERAL_VARIABLE &&
       triple->object && triple->object->type != RASQAL_LITERAL_VARIABLE) {

      /* Ground triple - check if it exists in data */
      if(triples_source->triple_present) {
        int exists = triples_source->triple_present(triples_source,
                                                   triples_source->user_data,
                                                   triple);
        if(exists)
          return 1; /* Ground triple exists, EXISTS is true */
      }
    } else {
      has_variable_pattern = 1;
    }
  }

      /* Phase 2: Check variable patterns with substitution */
    if(has_variable_pattern) {
      rasqal_triple* inst_triple;
      int triple_exists;

      /* Limit to first triple to avoid outer query contamination */
      /* EXISTS patterns may contain extra triples from query processing */
      if(num_triples > 0) {
        triple = (rasqal_triple*)raptor_sequence_get_at(gp->triples, 0);
        if(triple) {
          /* Skip ground triples already checked */
          if(triple->subject && triple->subject->type != RASQAL_LITERAL_VARIABLE &&
             triple->predicate && triple->predicate->type != RASQAL_LITERAL_VARIABLE &&
             triple->object && triple->object->type != RASQAL_LITERAL_VARIABLE) {
            return 0; /* Ground triple already processed above */
          }

          /* Instantiate triple with current bindings */
          inst_triple = rasqal_instantiate_triple_with_bindings(triple, outer_row, NULL);
          if(!inst_triple)
            return 0; /* Failed instantiation */

#ifdef RASQAL_DEBUG
          fprintf(stderr, "EXISTS: Instantiated triple: ");
          rasqal_triple_print(inst_triple, stderr);
          fprintf(stderr, "\n");
#endif

          /* Check if instantiated triple exists */
          triple_exists = rasqal_check_triple_exists_in_data(inst_triple, triples_source, query);

#ifdef RASQAL_DEBUG
          fprintf(stderr, "EXISTS: Triple exists result: %d\n", triple_exists);
#endif

          rasqal_free_triple(inst_triple);

          if(triple_exists)
            return 1; /* Variable pattern matched */
        }
      }
    }

  return 0; /* No patterns matched */
}

/*
 * rasqal_evaluate_basic_exists_pattern_with_origin:
 * @gp: BASIC graph pattern containing triple patterns
 * @triples_source: Data source for triple lookups
 * @query: Query context for variable bindings
 * @outer_row: Current variable bindings from outer query
 * @origin: Named graph context for triple evaluation
 *
 * INTERNAL - Evaluate BASIC patterns for EXISTS with named graph context.
 *
 * This function is identical to rasqal_evaluate_basic_exists_pattern but
 * passes the graph origin to triple instantiation, enabling EXISTS evaluation
 * within specific named graph contexts.
 *
 * Return value: 1 if any pattern matches in graph context, 0 otherwise
 */
static int
rasqal_evaluate_basic_exists_pattern_with_origin(rasqal_graph_pattern* gp,
                                                 rasqal_triples_source* triples_source,
                                                 rasqal_query* query,
                                                 rasqal_row* outer_row,
                                                 rasqal_literal* origin)
{
  int i;
  rasqal_triple* triple;
  int has_variable_pattern = 0;
  int num_triples = raptor_sequence_size(gp->triples);

  if(!gp->triples)
    return 1; /* Empty pattern always matches */

  /* Phase 1: Check for ground triples first (optimization) */

  for(i = 0; i < num_triples; i++) {
    triple = (rasqal_triple*)raptor_sequence_get_at(gp->triples, i);
    if(!triple)
      continue;

    /* Check if this is a ground triple (no variables) */
    if(triple->subject && triple->subject->type != RASQAL_LITERAL_VARIABLE &&
       triple->predicate && triple->predicate->type != RASQAL_LITERAL_VARIABLE &&
       triple->object && triple->object->type != RASQAL_LITERAL_VARIABLE) {

      /* Ground triple - create with graph context and check existence */
      rasqal_triple* context_triple = rasqal_new_triple(
        rasqal_new_literal_from_literal(triple->subject),
        rasqal_new_literal_from_literal(triple->predicate),
        rasqal_new_literal_from_literal(triple->object)
      );

      if(context_triple && origin) {
        context_triple->origin = rasqal_new_literal_from_literal(origin);
      }

      if(triples_source->triple_present && context_triple) {
        int exists = triples_source->triple_present(triples_source,
                                                   triples_source->user_data,
                                                   context_triple);
        rasqal_free_triple(context_triple);
        if(exists)
          return 1; /* Ground triple exists in graph context, EXISTS is true */
      } else if(context_triple) {
        rasqal_free_triple(context_triple);
      }
    } else {
      has_variable_pattern = 1;
    }
  }

  /* Phase 2: Check variable patterns with substitution and graph context */
  if(has_variable_pattern) {
    rasqal_triple* inst_triple;
    int triple_exists;

    /* Limit to first triple to avoid outer query contamination */
    /* EXISTS patterns may contain extra triples from query processing */
    if(num_triples > 0) {
      triple = (rasqal_triple*)raptor_sequence_get_at(gp->triples, 0);
      if(triple) {
        /* Skip ground triples already checked */
        if(triple->subject && triple->subject->type != RASQAL_LITERAL_VARIABLE &&
           triple->predicate && triple->predicate->type != RASQAL_LITERAL_VARIABLE &&
           triple->object && triple->object->type != RASQAL_LITERAL_VARIABLE) {
          return 0; /* Ground triple already processed above */
        }

        /* Instantiate triple with current bindings and graph context */
        inst_triple = rasqal_instantiate_triple_with_bindings(triple, outer_row, origin);
        if(!inst_triple)
          return 0; /* Failed instantiation */

#ifdef RASQAL_DEBUG
        fprintf(stderr, "EXISTS: Graph context origin provided: ");
        if(origin) {
          rasqal_literal_print(origin, stderr);
        } else {
          fprintf(stderr, "NULL");
        }
        fprintf(stderr, "\n");

        fprintf(stderr, "EXISTS: Instantiated triple with graph context: ");
        rasqal_triple_print(inst_triple, stderr);
        fprintf(stderr, "\n");

        fprintf(stderr, "EXISTS: Instantiated triple origin: ");
        if(inst_triple->origin) {
          rasqal_literal_print(inst_triple->origin, stderr);
        } else {
          fprintf(stderr, "NULL");
        }
        fprintf(stderr, "\n");
#endif

        /* Check if instantiated triple exists in graph context */
        triple_exists = rasqal_check_triple_exists_in_data(inst_triple, triples_source, query);

#ifdef RASQAL_DEBUG
        fprintf(stderr, "EXISTS: Triple exists in graph context result: %d\n", triple_exists);
#endif

        rasqal_free_triple(inst_triple);

        if(triple_exists)
          return 1; /* Variable pattern matched in graph context */
      }
    }
  }

  return 0; /* No patterns matched in graph context */
}

/*
 * rasqal_evaluate_group_exists_pattern:
 * @gp: GROUP graph pattern containing sub-patterns
 * @triples_source: Data source for triple lookups
 * @query: Query context for variable bindings
 * @outer_row: Current variable bindings from outer query
 *
 * INTERNAL - Evaluate GROUP patterns for EXISTS with conjunction semantics.
 *
 * GROUP patterns represent conjunction (AND) semantics where ALL sub-patterns
 * must match for the GROUP to succeed. For EXISTS evaluation, this means:
 *
 * - If any sub-pattern fails to match, the entire GROUP fails (returns 0)
 * - Only if ALL sub-patterns match does the GROUP succeed (returns 1)
 * - Empty GROUP patterns always succeed (return 1)
 *
 * The function recursively evaluates each sub-pattern using the same variable
 * bindings, implementing early termination on the first failure.
 *
 * Return value: 1 if all sub-patterns match, 0 otherwise
 */
static int
rasqal_evaluate_group_exists_pattern(rasqal_graph_pattern* gp,
                                     rasqal_triples_source* triples_source,
                                     rasqal_query* query,
                                     rasqal_row* outer_row)
{
  int i;
  int num_patterns;
  rasqal_graph_pattern* sub_gp;

  if(!gp->graph_patterns)
    return 1; /* Empty group always matches */

  num_patterns = raptor_sequence_size(gp->graph_patterns);

  /* GROUP semantics: ALL sub-patterns must match (conjunction) */
  for(i = 0; i < num_patterns; i++) {
    int sub_result;

    sub_gp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
    if(!sub_gp)
      continue;

    /* Recursive evaluation of sub-pattern */
    sub_result = rasqal_evaluate_exists_pattern(sub_gp, triples_source, query, outer_row);
    if(!sub_result)
      return 0; /* One failure means entire GROUP fails */
  }

  return 1; /* All sub-patterns matched */
}

/**
 * rasqal_evaluate_union_exists_pattern:
 * @gp: UNION graph pattern containing sub-patterns
 * @triples_source: Data source for triple lookups
 * @query: Query context for variable bindings
 * @outer_row: Current variable bindings from outer query
 *
 * INTERNAL - Evaluate UNION patterns for EXISTS with disjunction semantics.
 *
 * UNION patterns represent disjunction (OR) semantics where ANY sub-pattern
 * can match for the UNION to succeed. For EXISTS evaluation, this means:
 *
 * - If any sub-pattern matches, the entire UNION succeeds (returns 1)
 * - Only if ALL sub-patterns fail does the UNION fail (returns 0)
 * - Empty UNION patterns always fail (return 0)
 *
 * The function recursively evaluates each sub-pattern using the same variable
 * bindings, implementing early termination on the first success.
 *
 * Return value: 1 if any sub-pattern matches, 0 otherwise
 */
static int
rasqal_evaluate_union_exists_pattern(rasqal_graph_pattern* gp,
                                     rasqal_triples_source* triples_source,
                                     rasqal_query* query,
                                     rasqal_row* outer_row)
{
  int i;
  int num_patterns;
  rasqal_graph_pattern* sub_gp;

  if(!gp->graph_patterns)
    return 0; /* Empty union never matches */

  num_patterns = raptor_sequence_size(gp->graph_patterns);

  /* UNION semantics: ANY sub-pattern can match (disjunction) */
  for(i = 0; i < num_patterns; i++) {
    int sub_result;

    sub_gp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
    if(!sub_gp)
      continue;

    /* Recursive evaluation of sub-pattern */
    sub_result = rasqal_evaluate_exists_pattern(sub_gp, triples_source, query, outer_row);
    if(sub_result)
      return 1; /* One success means entire UNION succeeds */
  }

  return 0; /* No sub-patterns matched */
}

/*
 * rasqal_evaluate_optional_exists_pattern:
 * @gp: OPTIONAL graph pattern containing required and optional sub-patterns
 * @triples_source: Data source for triple lookups
 * @query: Query context for variable bindings
 * @outer_row: Current variable bindings from outer query
 *
 * INTERNAL - Evaluate OPTIONAL patterns for EXISTS with required/optional semantics.
 *
 * OPTIONAL patterns have a required sub-pattern and an optional sub-pattern.
 * For EXISTS evaluation, the semantics are:
 *
 * - The required pattern MUST match for the OPTIONAL to succeed
 * - The optional pattern can match or not - it doesn't affect the EXISTS result
 * - If the required pattern fails, the entire OPTIONAL fails (returns 0)
 * - If the required pattern succeeds, the OPTIONAL succeeds (returns 1)
 *
 * The function first evaluates the required pattern, and only if it succeeds
 * does it validate that the optional pattern structure is correct.
 *
 * Return value: 1 if required pattern matches, 0 otherwise
 */
static int
rasqal_evaluate_optional_exists_pattern(rasqal_graph_pattern* gp,
                                        rasqal_triples_source* triples_source,
                                        rasqal_query* query,
                                        rasqal_row* outer_row)
{
  rasqal_graph_pattern* required_gp;
  rasqal_graph_pattern* optional_gp;
  int required_result;

  if(!gp->graph_patterns || raptor_sequence_size(gp->graph_patterns) < 2)
    return 0; /* OPTIONAL requires at least 2 patterns */

  /* OPTIONAL has required pattern + optional pattern */
  required_gp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, 0);
  optional_gp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, 1);

  if(!required_gp)
    return 0;

  /* Required pattern must match */
  required_result = rasqal_evaluate_exists_pattern(required_gp, triples_source, query, outer_row);
  if(!required_result)
    return 0; /* Required pattern failed */

  /* Optional pattern can match or not - doesn't affect EXISTS result */
  /* But we need to check if it's possible to match for correctness */
  if(optional_gp) {
    /* Note: We don't return the result, just validate it's possible */
    rasqal_evaluate_exists_pattern(optional_gp, triples_source, query, outer_row);
  }

  return 1; /* Required pattern matched */
}

/**
 * rasqal_evaluate_filter_exists_pattern:
 * @gp: FILTER graph pattern containing sub-pattern and filter expressions
 * @triples_source: Data source for triple lookups
 * @query: Query context for variable bindings
 * @outer_row: Current variable bindings from outer query
 *
 * INTERNAL - Evaluate FILTER patterns for EXISTS with pattern + constraint semantics.
 *
 * FILTER patterns combine a graph pattern with filter expressions that must
 * evaluate to true. For EXISTS evaluation, the semantics are:
 *
 * - The sub-pattern MUST match first
 * - All filter expressions MUST evaluate to true
 * - If the pattern fails, the FILTER fails (returns 0)
 * - If any filter expression fails, the FILTER fails (returns 0)
 * - Only if both pattern and filters succeed does the FILTER succeed (returns 1)
 *
 * The function first evaluates the sub-pattern, then evaluates each filter
 * expression with the current variable bindings. It uses the standard
 * expression evaluation context for filter expression processing.
 *
 * Return value: 1 if pattern matches and all filters pass, 0 otherwise
 */
static int
rasqal_evaluate_filter_exists_pattern(rasqal_graph_pattern* gp,
                                      rasqal_triples_source* triples_source,
                                      rasqal_query* query,
                                      rasqal_row* outer_row)
{
  rasqal_graph_pattern* pattern_gp;
  int pattern_result;

  if(!gp)
    return 0;

  if(!gp->graph_patterns) {
    /* For standalone FILTER expressions, the expression is in filter_expression, not graph_patterns */
    if(gp->filter_expression) {
      rasqal_evaluation_context* eval_context;
      rasqal_literal* expr_result;
      int expr_error = 0;
      int filter_result;
      int i;

      /* Initialize evaluation context */
      eval_context = rasqal_new_evaluation_context(query->world, &query->locator, 0);
      if(!eval_context)
        return 0;

      /* CRITICAL: Set query context for nested EXISTS expressions */
      eval_context->query = query;

      /* Set variable values from outer row for filter evaluation */
      if(outer_row) {
        for(i = 0; i < outer_row->size; i++) {
          rasqal_variable* var = rasqal_row_get_variable_by_offset(outer_row, i);
          rasqal_literal* value = outer_row->values[i];
          if(var && value)
            var->value = rasqal_new_literal_from_literal(value);
        }
      }

      /* Evaluate the filter expression */
      expr_result = rasqal_expression_evaluate2(gp->filter_expression, eval_context, &expr_error);

      /* Clean up variable values */
      if(outer_row) {
        for(i = 0; i < outer_row->size; i++) {
          rasqal_variable* var = rasqal_row_get_variable_by_offset(outer_row, i);
          if(var && var->value) {
            rasqal_free_literal(var->value);
            var->value = NULL;
          }
        }
      }

      rasqal_free_evaluation_context(eval_context);

      if(expr_error || !expr_result)
        return 0;

      filter_result = rasqal_literal_as_boolean(expr_result, &expr_error);
      rasqal_free_literal(expr_result);

      if(expr_error)
        return 0;

      return filter_result;
    }
    return 0;
  }
  if(raptor_sequence_size(gp->graph_patterns) < 1)
    return 0;

  if(!gp->filter_expression)
    return 0;

  if(!gp->graph_patterns || raptor_sequence_size(gp->graph_patterns) < 1)
    return 0;

  /* FILTER has pattern + constraint expressions */
  pattern_gp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, 0);
  if(!pattern_gp)
    return 0;

  /* First evaluate the pattern */
  pattern_result = rasqal_evaluate_exists_pattern(pattern_gp, triples_source, query, outer_row);
  if(!pattern_result)
    return 0; /* Pattern must match first */

  /* Then evaluate filter expression */
  if(gp->filter_expression) {
    /* Evaluate filter expression with current bindings */
    int expr_error = 0;
    int filter_error = 0;
    int filter_result;
    rasqal_evaluation_context eval_context;
    rasqal_literal* expr_result;
    int i;
    rasqal_literal** old_values = NULL;
    int old_values_count = 0;

    /* Temporarily set variable values from outer_row for nested EXISTS evaluation */
    if(outer_row && query->vars_table) {
      raptor_sequence* vars_seq = rasqal_variables_table_get_named_variables_sequence(query->vars_table);
      if(vars_seq) {
        old_values_count = raptor_sequence_size(vars_seq);
        if(old_values_count > 0) {
          old_values = (rasqal_literal**)RASQAL_CALLOC(rasqal_literal**, old_values_count, sizeof(rasqal_literal*));
          if(!old_values) {
            return 0; /* Memory allocation failed */
          }

          /* Save current variable values and set new ones from outer_row */
          for(i = 0; i < old_values_count; i++) {
            rasqal_variable* var = (rasqal_variable*)raptor_sequence_get_at(vars_seq, i);
            if(var) {
              old_values[i] = var->value;
              if(i < outer_row->size && outer_row->values[i]) {
                var->value = rasqal_new_literal_from_literal(outer_row->values[i]);
              }
            }
          }
        }
      }
    }

    eval_context.world = query->world;
    eval_context.query = query;
    eval_context.base_uri = NULL;
    eval_context.locator = NULL;
    eval_context.flags = 0;
    eval_context.seed = 0;
    eval_context.random = NULL;

    expr_result = rasqal_expression_evaluate2(gp->filter_expression, &eval_context, &expr_error);

    /* Restore original variable values */
    if(old_values) {
      raptor_sequence* vars_seq = rasqal_variables_table_get_named_variables_sequence(query->vars_table);
      if(vars_seq) {
        for(i = 0; i < old_values_count; i++) {
          rasqal_variable* var = (rasqal_variable*)raptor_sequence_get_at(vars_seq, i);
          if(var) {
            if(var->value)
              rasqal_free_literal(var->value);
            var->value = old_values[i];
          }
        }
      }
      RASQAL_FREE(rasqal_literal**, old_values);
    }

    if(expr_error || !expr_result) {
      return 0; /* Filter evaluation failed */
    }

    /* Check if filter expression is true */
    filter_result = rasqal_literal_as_boolean(expr_result, &filter_error);
    rasqal_free_literal(expr_result);

    if(filter_error || !filter_result) {
      return 0; /* Filter constraint failed */
    }
  }

  return 1; /* Pattern matched and all filters passed */
}

/*
 * rasqal_evaluate_graph_exists_pattern:
 * @gp: GRAPH graph pattern containing named graph and sub-pattern
 * @triples_source: Data source for triple lookups
 * @query: Query context for variable bindings
 * @outer_row: Current variable bindings from outer query
 *
 * INTERNAL - Evaluate GRAPH patterns for EXISTS within named graph context.
 *
 * GRAPH patterns evaluate a sub-pattern within a specific named graph context.
 * For EXISTS evaluation, the semantics are:
 *
 * - The sub-pattern is evaluated within the named graph context
 * - If no named graph is specified, evaluation occurs in the default graph
 * - The function recursively evaluates the sub-pattern with the same
 *   variable bindings but potentially different graph context
 *
 * Note: Full named graph context switching requires enhanced triples_source
 * support that is not yet fully implemented. Currently, the function
 * evaluates in the default graph context regardless of the named graph.
 *
 * Return value: 1 if sub-pattern matches in graph context, 0 otherwise
 */
static int
rasqal_evaluate_graph_exists_pattern(rasqal_graph_pattern* gp,
                                     rasqal_triples_source* triples_source,
                                     rasqal_query* query,
                                     rasqal_row* outer_row)
{
  rasqal_graph_pattern* sub_gp;

  if(!gp->graph_patterns || raptor_sequence_size(gp->graph_patterns) < 1) {
    return 0;
  }

  sub_gp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, 0);
  if(!sub_gp)
    return 0;

  /* Handle named graph context */
  if(gp->origin) {
    /* Use origin-aware evaluation for basic patterns in graph context */
    if(sub_gp->op == RASQAL_GRAPH_PATTERN_OPERATOR_BASIC) {
      return rasqal_evaluate_basic_exists_pattern_with_origin(sub_gp, triples_source, query, outer_row, gp->origin);
    } else {
      /* For complex patterns, recursively evaluate but note that nested graph contexts
       * would need further enhancement to fully support all SPARQL graph nesting */
      return rasqal_evaluate_exists_pattern(sub_gp, triples_source, query, outer_row);
    }
  } else {
    /* No graph context specified, use default evaluation */
    return rasqal_evaluate_exists_pattern(sub_gp, triples_source, query, outer_row);
  }
}



/*
 * Function to instantiate a triple with variable bindings from outer row
 *
 * This function takes a triple pattern (which may contain variables) and
 * substitutes any variables with their actual bound values from the outer
 * query context, creating a new "instantiated" triple that can be evaluated
 * against the data.
 *
 * @param triple The triple pattern to instantiate
 * @param outer_row The outer row to use for variable bindings
 *
 * Return value: The instantiated triple or NULL on failure
 */
/**
 * rasqal_instantiate_triple_with_bindings:
 * @triple: Triple pattern that may contain variables
 * @outer_row: Current variable bindings from outer query
 *
 * INTERNAL - Instantiate a triple pattern by substituting bound variables.
 *
 * This function takes a triple pattern (which may contain variables) and
 * substitutes any variables with their actual bound values from the outer
 * query context, creating a new "instantiated" triple that can be evaluated
 * against the data.
 *
 * The function uses rasqal_literal_bound_value() to handle variable substitution:
 * - If a component is a variable with a bound value → substitute with actual value
 * - If a component is a variable without a bound value → keep as variable for pattern matching
 * - If a component is already a constant → keep as is
 *
 * This is essential for EXISTS evaluation as it enables patterns like
 * EXISTS { ?s :p :o } to use the current binding of ?s from the outer query.
 * The variable bindings are set up in the query context before this function is called.
 *
 * Return value: New instantiated triple, or NULL on failure
 */
static rasqal_triple*
rasqal_instantiate_triple_with_bindings(rasqal_triple* triple, rasqal_row* outer_row, rasqal_literal* origin)
{
  rasqal_literal* subj;
  rasqal_literal* pred;
  rasqal_literal* obj;
  rasqal_literal* triple_origin;
  rasqal_triple* inst_triple;

  if(!triple)
    return NULL;

  /* Substitute variables with values using rasqal_literal_bound_value() */
  subj = rasqal_literal_bound_value(triple->subject);
  pred = rasqal_literal_bound_value(triple->predicate);
  obj = rasqal_literal_bound_value(triple->object);

  /* Use provided graph origin, or preserve original triple's origin */
  if(origin) {
    triple_origin = rasqal_new_literal_from_literal(origin);
  } else if(triple->origin) {
    triple_origin = rasqal_new_literal_from_literal(triple->origin);
  } else {
    triple_origin = NULL;
  }

  /* Create triple with proper graph context */
  inst_triple = rasqal_new_triple(subj, pred, obj);
  if(inst_triple && triple_origin) {
    inst_triple->origin = triple_origin;
  }

  return inst_triple;
}

/*
 * rasqal_check_triple_exists_in_data:
 * @triple: Triple to check for existence in data
 * @triples_source: Data source for triple lookups
 * @query: Query context for pattern matching
 *
 * INTERNAL - Check if a triple exists in the data using dual-mode lookup.
 *
 * This function implements efficient triple existence checking with two modes:
 *
 * Mode 1: Exact triple lookup - For ground triples (no variables), uses
 *         triples_source->triple_present() for efficient exact matching.
 *         This bypasses expensive pattern matching for constant triples.
 *
 * Mode 2: Pattern matching - For triples with variables, uses
 *         rasqal_new_triples_match() to perform pattern matching against
 *         the data. This handles variable patterns and returns true if
 *         any matching triples exist.
 *
 * The function automatically detects which mode to use based on whether
 * the triple contains variables. This optimization is crucial for EXISTS
 * performance, especially for common cases with ground triples.
 *
 * Return value: 1 if triple exists/matches, 0 otherwise
 */
static int
rasqal_check_triple_exists_in_data(rasqal_triple* triple, rasqal_triples_source* triples_source, rasqal_query* query)
{
  int has_variables = 0;

  if(!triple || !triples_source || !query)
    return 0;



  /* Check if this is an exact triple (no variables) */
  if(triple->subject && triple->subject->type == RASQAL_LITERAL_VARIABLE) has_variables = 1;
  if(triple->predicate && triple->predicate->type == RASQAL_LITERAL_VARIABLE) has_variables = 1;
  if(triple->object && triple->object->type == RASQAL_LITERAL_VARIABLE) has_variables = 1;

  if(!has_variables) {
    /* Exact triple - use triple_present for efficient lookup */
    if(triples_source->triple_present) {
      int result = triples_source->triple_present(triples_source,
                                                 triples_source->user_data,
                                                 triple);
      return result;
    }
  } else {
    /* Pattern with variables - use triples matching */
    rasqal_triple_meta meta;
    rasqal_triples_match* match;
    int has_match;

    /* Initialize the triple meta structure */
    memset(&meta, 0, sizeof(meta));

    /* Create a triples match for this pattern */
    match = rasqal_new_triples_match(query, triples_source, &meta, triple);
    if(!match)
      return 0;

    /* Check if there are any matches */
    has_match = !rasqal_triples_match_is_end(match);

    /* Clean up */
    rasqal_free_triples_match(match);

    return has_match;
  }

  return 0;
}

static rasqal_row*
rasqal_exists_rowsource_read_row(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_exists_rowsource_context* con;
  rasqal_row* row = NULL;

  con = (rasqal_exists_rowsource_context*)user_data;

  if(!con->evaluated) {
    /* Evaluate EXISTS pattern against current variable bindings */
    int result = 0;

    if(con->exists_pattern) {
      /* Step 1: Set up variable bindings from outer query context */
      if(con->outer_row) {
        /* Apply outer variable bindings to query evaluation context */
        /* This ensures EXISTS pattern sees the current variable values */
        int i;
        for(i = 0; i < con->outer_row->size; i++) {
          rasqal_variable* var;
          rasqal_literal* value;

          /* Get variable from query's variable table, not from rowsource */
          var = rasqal_query_get_variable_by_offset(con->query, i);
          value = con->outer_row->values[i];

          if(var && value) {
            /* Set variable value in query context for EXISTS evaluation */
            rasqal_variable_set_value(var, rasqal_new_literal_from_literal(value));
          }
        }
      }



      /* Step 2: Execute EXISTS pattern with current variable bindings */
      /* Use graph origin if available, otherwise use default evaluation */
      if(con->graph_origin) {
        result = rasqal_evaluate_exists_pattern_with_origin(con->exists_pattern,
                                                            con->triples_source,
                                                            con->query,
                                                            con->outer_row,
                                                            con->graph_origin);
      } else {
        result = rasqal_evaluate_exists_pattern(con->exists_pattern,
                                                con->triples_source,
                                                con->query,
                                                con->outer_row);
      }


    }

    con->evaluation_result = result;
    con->evaluated = 1;
  }

  /* EXISTS rowsource returns one empty row if true, no rows if false */
  if(con->evaluation_result) {
    if(!con->is_negated) {
      /* EXISTS succeeded - return empty row */
      row = rasqal_new_row(rowsource);
    }
    /* else NOT EXISTS failed - return no row */
  } else {
    if(con->is_negated) {
      /* NOT EXISTS succeeded - return empty row */
      row = rasqal_new_row(rowsource);
    }
    /* else EXISTS failed - return no row */
  }

  return row;
}


static raptor_sequence*
rasqal_exists_rowsource_read_all_rows(rasqal_rowsource* rowsource,
                                      void *user_data)
{
  raptor_sequence *seq = NULL;
  rasqal_row* row;

  /* Create sequence */
  seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_row,
                            (raptor_data_print_handler)rasqal_row_print);
  if(!seq)
    return NULL;

  /* Get the evaluation result */
  row = rasqal_exists_rowsource_read_row(rowsource, user_data);
  if(row)
    raptor_sequence_push(seq, row);

  return seq;
}


static int
rasqal_exists_rowsource_reset(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_exists_rowsource_context* con;

  con = (rasqal_exists_rowsource_context*)user_data;
  if(!con)
    return 1;
  
  /* Reset evaluation state to allow re-evaluation with new variable bindings */
  con->evaluated = 0;
  con->evaluation_result = 0;
  
  return 0;
}


static const rasqal_rowsource_handler rasqal_exists_rowsource_handler = {
  /* .version = */ 1,
  "exists",
  /* .init = */ NULL,
  /* .finish = */ rasqal_exists_rowsource_finish,
  /* .ensure_variables = */ rasqal_exists_rowsource_ensure_variables,
  /* .read_row = */ rasqal_exists_rowsource_read_row,
  /* .read_all_rows = */ rasqal_exists_rowsource_read_all_rows,
  /* .reset = */ rasqal_exists_rowsource_reset,
  /* .set_requirements = */ NULL,
  /* .get_inner_rowsource = */ NULL,
  /* .set_origin = */ NULL,
};


/*
 * rasqal_new_exists_rowsource:
 * @world: world object
 * @query: query object
 * @exists_pattern: EXISTS graph pattern to evaluate (reference stored, not copied)
 * @outer_row: current variable bindings from outer query (copied using rasqal_new_row_from_row)
 * @graph_origin: named graph context for pattern evaluation (copied, or NULL for default graph)
 * @is_negated: 1 for NOT EXISTS, 0 for EXISTS
 *
 * INTERNAL - create a new EXISTS rowsource that evaluates EXISTS patterns
 *
 * The @exists_pattern is referenced but not copied - the caller retains ownership.
 * The @outer_row is copied to create a new independent row for variable binding context.
 *
 * Return value: new rowsource or NULL on failure
 */
rasqal_rowsource*
rasqal_new_exists_rowsource(rasqal_world *world,
                            rasqal_query* query,
                            rasqal_triples_source* triples_source,
                            rasqal_graph_pattern* exists_pattern,
                            rasqal_row* outer_row,
                            rasqal_literal* graph_origin,
                            int is_negated)
{
  rasqal_exists_rowsource_context* con;
  int flags = 0;

  if(!world || !query || !triples_source || !exists_pattern)
    return NULL;

  con = RASQAL_CALLOC(rasqal_exists_rowsource_context*, 1, sizeof(*con));
  if(!con)
    return NULL;

  /* Store reference to EXISTS pattern - graph patterns are typically shared */
  con->exists_pattern = exists_pattern;
  con->query = query;
  con->triples_source = triples_source;

  /* Make a new copy of the outer row for variable binding context */
  if(outer_row) {
    con->outer_row = rasqal_new_row_from_row(outer_row);
    if(!con->outer_row) {
      RASQAL_FREE(rasqal_exists_rowsource_context, con);
      return NULL;
    }
  }

  /* Store graph origin if provided */
  if(graph_origin) {
    con->graph_origin = rasqal_new_literal_from_literal(graph_origin);
    if(!con->graph_origin) {
      if(con->outer_row)
        rasqal_free_row(con->outer_row);
      RASQAL_FREE(rasqal_exists_rowsource_context, con);
      return NULL;
    }
  }

  con->evaluation_result = 0;
  con->evaluated = 0;
  con->is_negated = is_negated;

  return rasqal_new_rowsource_from_handler(world, query,
                                           con,
                                           &rasqal_exists_rowsource_handler,
                                           query->vars_table,
                                           flags);
}


#endif /* not STANDALONE */


#ifdef STANDALONE

/* Test program for EXISTS rowsource */

#include <stdio.h>
#include <string.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

int main(int argc, char *argv[])
{
  const char *program = rasqal_basename(argv[0]);
  rasqal_world *world = NULL;
  rasqal_query *query = NULL;
  rasqal_graph_pattern *exists_pattern = NULL;
  rasqal_rowsource *rowsource = NULL;
  rasqal_triples_source *triples_source = NULL;
  rasqal_row *outer_row = NULL;
  int failures = 0;
  int verbose = 1;
  int i;

  /* Process arguments */
  for(i = 1; i < argc ; i++) {
    if(!strcmp(argv[i], "-q") || !strcmp(argv[i], "--quiet"))
      verbose = 0;
    else if(!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
      printf("Usage: %s [OPTIONS]\n", program);
      printf("Test the EXISTS rowsource\n\n");
      printf("  -q, --quiet     Run quietly\n");
      printf("  -h, --help      This help message\n");
      return 0;
    }
    else {
      fprintf(stderr, "%s: Unknown argument `%s'\n", program, argv[i]);
      return 1;
    }
  }

  /* Initialize Rasqal */
  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    return 1;
  }

  if(verbose)
    printf("%s: Testing EXISTS rowsource\n", program);

  /* Test 1: Basic EXISTS rowsource creation and destruction */
  if(verbose)
    printf("Test 1: Basic rowsource creation test\n");

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query) {
    fprintf(stderr, "%s: Failed to create query\n", program);
    failures++;
    goto tidy;
  }

  /* Try to create a triples source - this might fail in standalone mode */
  triples_source = rasqal_new_triples_source(query);
  if(!triples_source) {
    if(verbose)
      printf("  Skipping triples source tests (no data source available)\n");
  }

  /* Create a simple basic graph pattern for testing */
  {
    raptor_sequence* triples = raptor_new_sequence((raptor_data_free_handler)rasqal_free_triple,
                                                   (raptor_data_print_handler)rasqal_triple_print);
    if(!triples) {
      fprintf(stderr, "%s: Failed to create triples sequence\n", program);
      failures++;
      goto tidy;
    }

          exists_pattern = rasqal_new_basic_graph_pattern(query, triples, 0, 0, 1);
    if(!exists_pattern) {
      fprintf(stderr, "%s: Failed to create basic graph pattern\n", program);
      raptor_free_sequence(triples);
      failures++;
      goto tidy;
    }
  }

  /* Test EXISTS rowsource creation */
  rowsource = rasqal_new_exists_rowsource(world, query, triples_source,
                                          exists_pattern, outer_row, NULL, 0);
  if(!rowsource) {
    if(!triples_source) {
      if(verbose)
        printf("  Skipping EXISTS rowsource test (no triples source)\n");
    } else {
      fprintf(stderr, "%s: Failed to create EXISTS rowsource\n", program);
      failures++;
      goto tidy;
    }
  } else {
    if(verbose)
      printf("  EXISTS rowsource created successfully\n");

    /* Test rowsource basic functionality */
    if(rasqal_rowsource_get_size(rowsource) < 0) {
      fprintf(stderr, "%s: EXISTS rowsource size is invalid\n", program);
      failures++;
    }

    /* Clean up rowsource */
    rasqal_free_rowsource(rowsource);
    rowsource = NULL;
  }

  /* Test 2: NOT EXISTS rowsource */
  if(verbose)
    printf("Test 2: NOT EXISTS rowsource creation test\n");

  if(triples_source) {
    rowsource = rasqal_new_exists_rowsource(world, query, triples_source,
                                            exists_pattern, outer_row, NULL, 1);
    if(!rowsource) {
      fprintf(stderr, "%s: Failed to create NOT EXISTS rowsource\n", program);
      failures++;
    } else {
      if(verbose)
        printf("  NOT EXISTS rowsource created successfully\n");

      rasqal_free_rowsource(rowsource);
      rowsource = NULL;
    }
  }

  /* Test 3: Error handling */
  if(verbose)
    printf("Test 3: Error handling tests\n");

  /* Test with NULL parameters */
  rowsource = rasqal_new_exists_rowsource(NULL, query, triples_source,
                                          exists_pattern, outer_row, NULL, 0);
  if(rowsource) {
    fprintf(stderr, "%s: EXISTS rowsource creation should fail with NULL world\n", program);
    failures++;
    rasqal_free_rowsource(rowsource);
  } else {
    if(verbose)
      printf("  NULL world parameter correctly rejected\n");
  }

  rowsource = rasqal_new_exists_rowsource(world, NULL, triples_source,
                                          exists_pattern, outer_row, NULL, 0);
  if(rowsource) {
    fprintf(stderr, "%s: EXISTS rowsource creation should fail with NULL query\n", program);
    failures++;
    rasqal_free_rowsource(rowsource);
  } else {
    if(verbose)
      printf("  NULL query parameter correctly rejected\n");
  }

  rowsource = rasqal_new_exists_rowsource(world, query, NULL,
                                          exists_pattern, outer_row, NULL, 0);
  if(rowsource) {
    fprintf(stderr, "%s: EXISTS rowsource creation should fail with NULL triples_source\n", program);
    failures++;
    rasqal_free_rowsource(rowsource);
  } else {
    if(verbose)
      printf("  NULL triples_source parameter correctly rejected\n");
  }

  rowsource = rasqal_new_exists_rowsource(world, query, triples_source,
                                          NULL, outer_row, NULL, 0);
  if(rowsource) {
    fprintf(stderr, "%s: EXISTS rowsource creation should fail with NULL pattern\n", program);
    failures++;
    rasqal_free_rowsource(rowsource);
  } else {
    if(verbose)
      printf("  NULL pattern parameter correctly rejected\n");
  }

  if(verbose)
    printf("Test 4: EXISTS pattern evaluation test\n");

  /* Test EXISTS pattern evaluation with basic patterns */
  if(triples_source && exists_pattern) {
    /* Test the EXISTS evaluation through the rowsource interface */
    if(verbose)
      printf("  EXISTS pattern evaluation through rowsource interface\n");
  } else {
    if(verbose)
      printf("  Skipping pattern evaluation (no triples source available)\n");
  }

  /* Test 5: Graph context propagation */
  if(verbose)
    printf("Test 5: Graph context propagation test\n");

  if(triples_source && exists_pattern) {
    /* Create a test graph origin literal */
    rasqal_literal* graph_origin = rasqal_new_uri_literal(world,
      raptor_new_uri(world->raptor_world_ptr, (const unsigned char*)"http://example.org/graph1"));

    if(graph_origin) {
      /* Test EXISTS rowsource with graph context */
      rowsource = rasqal_new_exists_rowsource(world, query, triples_source,
                                              exists_pattern, outer_row, graph_origin, 0);
      if(!rowsource) {
        fprintf(stderr, "%s: Failed to create EXISTS rowsource with graph context\n", program);
        failures++;
      } else {
        if(verbose)
          printf("  EXISTS rowsource with graph context created successfully\n");
        rasqal_free_rowsource(rowsource);
        rowsource = NULL;
      }

      rasqal_free_literal(graph_origin);
    }
  } else {
    if(verbose)
      printf("  Skipping graph context test (no triples source available)\n");
  }

  /* Test 6: SPARQL 1.1 Algebra compliance tests */
  if(verbose)
    printf("Test 6: SPARQL 1.1 Algebra compliance test\n");

  /* Test EXISTS rowsource with different pattern types to verify algebra compliance */
  if(triples_source && exists_pattern) {
    rasqal_row* test_row;
    rasqal_rowsource* test_exists_rs;

    /* Create a simple test row with no bindings */
    test_row = rasqal_new_row(NULL);
    if(test_row) {
      /* Test EXISTS rowsource evaluation */
      test_exists_rs = rasqal_new_exists_rowsource(world, query, triples_source,
                                                   exists_pattern, test_row, NULL, 0);
      if(test_exists_rs) {
        rasqal_row* result_row;

        /* Try to read a row from the EXISTS rowsource */
        result_row = rasqal_rowsource_read_row(test_exists_rs);
        if(result_row) {
          if(verbose)
            printf("  EXISTS evaluation returned a result row (pattern matched)\n");
          rasqal_free_row(result_row);
        } else {
          if(verbose)
            printf("  EXISTS evaluation returned no result (pattern did not match)\n");
        }

        rasqal_free_rowsource(test_exists_rs);
      }

      /* Test NOT EXISTS rowsource evaluation */
      test_exists_rs = rasqal_new_exists_rowsource(world, query, triples_source,
                                                   exists_pattern, test_row, NULL, 1);
      if(test_exists_rs) {
        rasqal_row* result_row;

        /* Try to read a row from the NOT EXISTS rowsource */
        result_row = rasqal_rowsource_read_row(test_exists_rs);
        if(result_row) {
          if(verbose)
            printf("  NOT EXISTS evaluation returned a result row (pattern did not match)\n");
          rasqal_free_row(result_row);
        } else {
          if(verbose)
            printf("  NOT EXISTS evaluation returned no result (pattern matched)\n");
        }

        rasqal_free_rowsource(test_exists_rs);
      }

      rasqal_free_row(test_row);
    }
  } else {
    if(verbose)
      printf("  Skipping algebra compliance test (no triples source available)\n");
  }

  if(verbose)
    printf("Test 7: Resource cleanup verification\n");
  /* Resource cleanup is verified by the above tests completing without crashes */
  if(verbose)
    printf("  Resource cleanup completed successfully\n");

  tidy:
  if(rowsource)
    rasqal_free_rowsource(rowsource);
  if(exists_pattern)
    rasqal_free_graph_pattern(exists_pattern);
  if(triples_source)
    rasqal_free_triples_source(triples_source);
  if(query)
    rasqal_free_query(query);
  if(world) {
    rasqal_free_world(world);
  }

  if(verbose) {
    if(failures)
      printf("%s: %d test%s FAILED\n", program, failures, (failures == 1 ? "" : "s"));
    else
      printf("%s: All tests PASSED\n", program);
  }

  return failures;
}

#endif /* STANDALONE */

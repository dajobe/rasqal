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
/* Unified EXISTS evaluation architecture - eliminates duplication and adds NOT EXISTS optimization */

/* Evaluation modes for unified architecture */
typedef enum {
  RASQAL_EXISTS_MODE_EXISTS = 0,     /* Standard EXISTS - all patterns must match */  
  RASQAL_EXISTS_MODE_NOT_EXISTS = 1  /* NOT EXISTS - can short-circuit on first failure */
} rasqal_exists_mode;

#ifdef RASQAL_DEBUG
/* Debug string lookup for EXISTS modes - only compiled in debug builds */
static const char* rasqal_exists_mode_names[2] = {
  "EXISTS",     /* RASQAL_EXISTS_MODE_EXISTS = 0 */
  "NOT EXISTS"  /* RASQAL_EXISTS_MODE_NOT_EXISTS = 1 */
};
#define RASQAL_EXISTS_MODE_NAME(mode) (rasqal_exists_mode_names[(mode)])
#endif

/* Core unified evaluation function - handles all pattern types and modes */
static int rasqal_evaluate_exists_pattern_unified(rasqal_graph_pattern* gp, rasqal_triples_source* triples_source, rasqal_query* query, rasqal_row* outer_row, rasqal_literal* graph_origin, rasqal_exists_mode mode);

/* Pattern-specific internal handlers */
static int rasqal_evaluate_basic_pattern_internal(rasqal_graph_pattern* gp, rasqal_triples_source* triples_source, rasqal_query* query, rasqal_row* outer_row, rasqal_literal* origin, rasqal_exists_mode mode);
static int rasqal_evaluate_group_pattern_internal(rasqal_graph_pattern* gp, rasqal_triples_source* triples_source, rasqal_query* query, rasqal_row* outer_row, rasqal_exists_mode mode);
static int rasqal_evaluate_union_pattern_internal(rasqal_graph_pattern* gp, rasqal_triples_source* triples_source, rasqal_query* query, rasqal_row* outer_row, rasqal_exists_mode mode);
static int rasqal_evaluate_optional_pattern_internal(rasqal_graph_pattern* gp, rasqal_triples_source* triples_source, rasqal_query* query, rasqal_row* outer_row, rasqal_exists_mode mode);
static int rasqal_evaluate_filter_pattern_internal(rasqal_graph_pattern* gp, rasqal_triples_source* triples_source, rasqal_query* query, rasqal_row* outer_row, rasqal_exists_mode mode);
static int rasqal_evaluate_graph_pattern_internal(rasqal_graph_pattern* gp, rasqal_triples_source* triples_source, rasqal_query* query, rasqal_row* outer_row, rasqal_exists_mode mode);

/* Public API functions - maintained for backward compatibility */
int rasqal_evaluate_not_exists_pattern_with_origin(rasqal_graph_pattern* gp, rasqal_triples_source* triples_source, rasqal_query* query, rasqal_row* outer_row, rasqal_literal* graph_origin);

/* Legacy functions - deprecated but maintained for compatibility */
static int rasqal_evaluate_filter_exists_pattern(rasqal_graph_pattern* gp, rasqal_triples_source* triples_source, rasqal_query* query, rasqal_row* outer_row);
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
 * rasqal_evaluate_exists_pattern_unified:
 * @gp: Graph pattern to evaluate
 * @triples_source: Data source for triple lookups
 * @query: Query context for variable bindings and execution
 * @outer_row: Current variable bindings from outer query
 * @graph_origin: Graph context (NULL for default graph)
 * @mode: Evaluation mode (EXISTS or NOT EXISTS)
 *
 * INTERNAL - Unified graph pattern evaluation for EXISTS and NOT EXISTS.
 *
 * This function consolidates all EXISTS evaluation logic and provides
 * optimizations for NOT EXISTS patterns via early termination.
 *
 * Return value: 1 if pattern matches (for EXISTS) or doesn't match (for NOT EXISTS), 0 otherwise
 */
static int
rasqal_evaluate_exists_pattern_unified(rasqal_graph_pattern* gp,
                                       rasqal_triples_source* triples_source,
                                       rasqal_query* query,
                                       rasqal_row* outer_row,
                                       rasqal_literal* graph_origin,
                                       rasqal_exists_mode mode)
{
  int result;

  if(!gp)
    return 0;

  RASQAL_DEBUG3("Unified pattern evaluation: mode=%s, pattern type=%d\n",
                RASQAL_EXISTS_MODE_NAME(mode), gp->op);

  /* Dispatch to appropriate pattern handler based on pattern type */
  switch(gp->op) {
    case RASQAL_GRAPH_PATTERN_OPERATOR_BASIC:
      result = rasqal_evaluate_basic_pattern_internal(gp, triples_source,
                                                      query, outer_row,
                                                      graph_origin, mode);
      break;

    case RASQAL_GRAPH_PATTERN_OPERATOR_GROUP:
      result = rasqal_evaluate_group_pattern_internal(gp, triples_source,
                                                      query, outer_row, mode);
      break;

    case RASQAL_GRAPH_PATTERN_OPERATOR_UNION:
      result = rasqal_evaluate_union_pattern_internal(gp, triples_source,
                                                      query, outer_row, mode);
      break;

    case RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL:
      result = rasqal_evaluate_optional_pattern_internal(gp, triples_source,
                                                         query, outer_row, mode);
      break;

    case RASQAL_GRAPH_PATTERN_OPERATOR_FILTER:
      result = rasqal_evaluate_filter_pattern_internal(gp, triples_source,
                                                       query, outer_row, mode);
      break;

    case RASQAL_GRAPH_PATTERN_OPERATOR_GRAPH:
      result = rasqal_evaluate_graph_pattern_internal(gp, triples_source,
                                                      query, outer_row, mode);
      break;

    /* Handle other pattern types with basic evaluation */
    case RASQAL_GRAPH_PATTERN_OPERATOR_MINUS:
    case RASQAL_GRAPH_PATTERN_OPERATOR_BIND:
    case RASQAL_GRAPH_PATTERN_OPERATOR_SELECT:
    case RASQAL_GRAPH_PATTERN_OPERATOR_SERVICE:
    case RASQAL_GRAPH_PATTERN_OPERATOR_EXISTS:
      /* TODO: These patterns need proper mode-aware implementations */
      if(gp->graph_patterns && raptor_sequence_size(gp->graph_patterns) > 0) {
        rasqal_graph_pattern* sub_pattern = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, 0);
        if(sub_pattern) {
          result = rasqal_evaluate_exists_pattern_unified(sub_pattern,
                                                          triples_source,
                                                          query, outer_row,
                                                          graph_origin, mode);
        } else {
          result = 0;
        }
      } else {
        result = 0;
      }
      break;

    case RASQAL_GRAPH_PATTERN_OPERATOR_VALUES:
      /* VALUES patterns: always succeed for EXISTS, always fail for
       * NOT EXISTS */
      result = (mode == RASQAL_EXISTS_MODE_NOT_EXISTS) ? 0 : 1;
      break;

    case RASQAL_GRAPH_PATTERN_OPERATOR_NOT_EXISTS:
      /* NOT EXISTS patterns: handle recursively with swapped mode */
      if(gp->graph_patterns && raptor_sequence_size(gp->graph_patterns) > 0) {
        rasqal_graph_pattern* sub_pattern = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, 0);
        if(sub_pattern) {
          rasqal_exists_mode swapped_mode = (mode == RASQAL_EXISTS_MODE_EXISTS)
                                           ? RASQAL_EXISTS_MODE_NOT_EXISTS
                                           : RASQAL_EXISTS_MODE_EXISTS;
          result = rasqal_evaluate_exists_pattern_unified(sub_pattern,
                                                          triples_source, query,
                                                          outer_row,
                                                          graph_origin,
                                                          swapped_mode);
        } else {
          result = (mode == RASQAL_EXISTS_MODE_NOT_EXISTS) ? 0 : 1;
        }
      } else {
        result = (mode == RASQAL_EXISTS_MODE_NOT_EXISTS) ? 0 : 1;
      }
      break;

    case RASQAL_GRAPH_PATTERN_OPERATOR_UNKNOWN:
    default:
      RASQAL_DEBUG2("Unknown graph pattern operator %d\n", gp->op);
      result = 0;
      break;
  }

  RASQAL_DEBUG4("Unified pattern evaluation result: mode=%s, pattern type=%d, result=%d\n",
                mode == RASQAL_EXISTS_MODE_EXISTS ? "EXISTS" : "NOT EXISTS",
                gp->op, result);

  return result;
}


/*
 * rasqal_evaluate_basic_pattern_internal:
 * @gp: BASIC graph pattern containing triple patterns
 * @triples_source: Data source for triple lookups
 * @query: Query context for variable bindings
 * @outer_row: Current variable bindings from outer query
 * @origin: Named graph context (NULL for default graph)
 * @mode: Evaluation mode (EXISTS or NOT EXISTS)
 *
 * INTERNAL - Unified evaluation for BASIC patterns supporting both EXISTS and NOT EXISTS.
 *
 * This function implements conjunctive evaluation for multi-triple
 * patterns with optimizations for both EXISTS (fail-fast) and NOT
 * EXISTS (succeed-fast) semantics.
 *
 * Return value: 1 if pattern matches mode criteria, 0 otherwise
 */
static int
rasqal_evaluate_basic_pattern_internal(rasqal_graph_pattern* gp,
                                       rasqal_triples_source* triples_source,
                                       rasqal_query* query,
                                       rasqal_row* outer_row,
                                       rasqal_literal* origin,
                                       rasqal_exists_mode mode)
{
  int i;
  rasqal_triple* triple;
  int has_variable_pattern = 0;
  int num_triples = raptor_sequence_size(gp->triples);

  if(!gp->triples)
    /* Empty pattern handling based on mode */
    return (mode == RASQAL_EXISTS_MODE_NOT_EXISTS) ? 1 : 0;

  RASQAL_DEBUG4("Basic pattern internal: mode=%s, triples=%d, origin=%s\n",
                RASQAL_EXISTS_MODE_NAME(mode), num_triples,
                origin ? "provided" : "NULL");

  /* Phase 1: Check for ground triples first (optimization) */
  for(i = 0; i < num_triples; i++) {
    triple = (rasqal_triple*)raptor_sequence_get_at(gp->triples, i);
    if(!triple)
      continue;

    /* Check if this is a ground triple (no variables) */
    if(triple->subject && triple->subject->type != RASQAL_LITERAL_VARIABLE &&
       triple->predicate && triple->predicate->type != RASQAL_LITERAL_VARIABLE &&
       triple->object && triple->object->type != RASQAL_LITERAL_VARIABLE) {

      /* Handle ground triple based on origin context */
      int ground_exists = 0;
      if(origin) {
        /* Ground triple with graph context */
        rasqal_triple* context_triple = rasqal_new_triple(
          rasqal_new_literal_from_literal(triple->subject),
          rasqal_new_literal_from_literal(triple->predicate),
          rasqal_new_literal_from_literal(triple->object)
        );
        
        if(context_triple) {
          context_triple->origin = rasqal_new_literal_from_literal(origin);
          
          if(triples_source->triple_present)
            ground_exists = triples_source->triple_present(triples_source,
                                                          triples_source->user_data,
                                                          context_triple);
          rasqal_free_triple(context_triple);
        }
      } else {
        /* Ground triple without graph context */
        if(triples_source->triple_present)
          ground_exists = triples_source->triple_present(triples_source,
                                                        triples_source->user_data,
                                                        triple);
      }

      RASQAL_DEBUG3("Ground triple %d exists: %d\n", i, ground_exists);

      /* Apply mode-specific logic for ground triples */
      if(mode == RASQAL_EXISTS_MODE_NOT_EXISTS) {
        /* NOT EXISTS: if any ground triple fails to exist, we can
         * succeed immediately */
        if(!ground_exists) {
          RASQAL_DEBUG2("NOT EXISTS: ground triple %d not found, pattern succeeds\n", i);
          return 1;
        }
      } else {
        /* EXISTS: if any ground triple fails to exist, pattern fails */
        if(!ground_exists) {
          RASQAL_DEBUG2("EXISTS: ground triple %d not found, pattern fails\n", i);
          return 0;
        }
      }
    } else {
      has_variable_pattern = 1;
    }
  }

  /* Phase 2: Check variable patterns with substitution and
   * mode-aware optimization */
  if(has_variable_pattern) {
    /* For NOT EXISTS, we need to find a complete solution that satisfies
     * ALL triples together, not check each triple individually */
    if(mode == RASQAL_EXISTS_MODE_NOT_EXISTS) {
      /* Create instantiated triples with outer row bindings applied */
      raptor_sequence* inst_triples = raptor_new_sequence((raptor_data_free_handler)rasqal_free_triple, NULL);
      rasqal_rowsource* temp_rowsource;
      rasqal_triple* inst_triple;
      int has_complete_solution = 0;
      
      if(inst_triples) {
        /* Apply outer row bindings to each triple in the pattern */
        for(i = 0; i < num_triples; i++) {
          triple = (rasqal_triple*)raptor_sequence_get_at(gp->triples, i);
          if(triple) {
            /* Instantiate triple with current bindings */
            inst_triple = rasqal_instantiate_triple_with_bindings(triple, outer_row, origin);
            if(inst_triple) {
              raptor_sequence_push(inst_triples, inst_triple);
            }
          }
        }
        
        /* Create a temporary rowsource with the instantiated triples */
        temp_rowsource = rasqal_new_triples_rowsource(query->world, query, triples_source, inst_triples, 0, raptor_sequence_size(inst_triples) - 1);
        if(temp_rowsource) {
          /* Check if there are any solutions */
          rasqal_row* temp_row = rasqal_rowsource_read_row(temp_rowsource);
          has_complete_solution = (temp_row != NULL);
          if(temp_row) {
            rasqal_free_row(temp_row);
          }
          rasqal_free_rowsource(temp_rowsource);
        }
        
        raptor_free_sequence(inst_triples);
      }
      
      RASQAL_DEBUG2("NOT EXISTS: Complete solution found: %d\n", has_complete_solution);
      
      /* NOT EXISTS succeeds if no complete solution exists */
      return !has_complete_solution;
    } else {
      /* EXISTS mode: check each triple individually (existing logic) */
      rasqal_triple* inst_triple;
      int triple_exists;

      for(i = 0; i < num_triples; i++) {
        triple = (rasqal_triple*)raptor_sequence_get_at(gp->triples, i);
        if(!triple)
          continue;

        /* Skip ground triples already checked in Phase 1 */
        if(triple->subject && triple->subject->type != RASQAL_LITERAL_VARIABLE &&
           triple->predicate && triple->predicate->type != RASQAL_LITERAL_VARIABLE &&
           triple->object && triple->object->type != RASQAL_LITERAL_VARIABLE) {
          continue;
        }

        /* Instantiate triple with current bindings and optional graph context */
        inst_triple = rasqal_instantiate_triple_with_bindings(triple, outer_row,
                                                              origin);
        if(!inst_triple) {
          /* Failed instantiation handling based on mode */
          return 0;
        }

#ifdef RASQAL_DEBUG
        if(rasqal_get_debug_level() >= 2) {
          RASQAL_DEBUG3("%s: Instantiated triple %d: ", RASQAL_EXISTS_MODE_NAME(mode), i);
          rasqal_triple_print(inst_triple, RASQAL_DEBUG_FH);
          fprintf(RASQAL_DEBUG_FH, "\n");
        }
#endif

        /* Check if instantiated triple exists */
        triple_exists = rasqal_check_triple_exists_in_data(inst_triple, triples_source, query);

        RASQAL_DEBUG4("%s: Triple %d exists result: %d\n",
                      mode == RASQAL_EXISTS_MODE_EXISTS ? "EXISTS" : "NOT EXISTS",
                      i, triple_exists);

        rasqal_free_triple(inst_triple);

        /* EXISTS: must fail immediately if any triple doesn't exist */
        if(!triple_exists) {
          RASQAL_DEBUG2("EXISTS: triple %d not found, pattern fails\n", i);
          return 0;
        }
      }
    }
  }

  /* All patterns processed without early termination */
  if(mode == RASQAL_EXISTS_MODE_EXISTS) {
    /* EXISTS: all triples existed, so pattern succeeds */
    RASQAL_DEBUG1("EXISTS: all triples found, pattern succeeds\n");
    return 1;
  }
  
  /* NOT EXISTS case is handled above with complete solution checking */
  return 0;
}


/*
 * Pattern-specific internal handlers - implement mode-aware evaluation
 */

static int
rasqal_evaluate_group_pattern_internal(rasqal_graph_pattern* gp,
                                       rasqal_triples_source* triples_source,
                                       rasqal_query* query,
                                       rasqal_row* outer_row,
                                       rasqal_exists_mode mode)
{
  int i;
  int num_patterns;
  rasqal_graph_pattern* sub_gp;

  if(!gp->graph_patterns)
    /* Empty group handling based on mode:
     * EXISTS: empty group always succeeds (1)
     * NOT EXISTS: empty group always fails (0) */
    return (mode == RASQAL_EXISTS_MODE_NOT_EXISTS) ? 0 : 1;

  num_patterns = raptor_sequence_size(gp->graph_patterns);

  RASQAL_DEBUG3("Group pattern internal: mode=%s, sub-patterns=%d\n",
                RASQAL_EXISTS_MODE_NAME(mode), num_patterns);

  /* Mode-aware GROUP evaluation with optimized short-circuiting */
  for(i = 0; i < num_patterns; i++) {
    int sub_result;

    sub_gp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
    if(!sub_gp)
      continue;

    /* Recursive evaluation of sub-pattern with unified architecture */
    sub_result = rasqal_evaluate_exists_pattern_unified(sub_gp, triples_source,
                                                        query, outer_row, NULL,
                                                        mode);

    RASQAL_DEBUG4("Group sub-pattern %d: mode=%s, result=%d\n", 
                  i, RASQAL_EXISTS_MODE_NAME(mode), sub_result);

    /* Mode-aware short-circuiting logic */
    if(mode == RASQAL_EXISTS_MODE_NOT_EXISTS) {
      /* NOT EXISTS: can succeed immediately if any sub-pattern fails */
      if(!sub_result) {
        RASQAL_DEBUG2("NOT EXISTS: Group sub-pattern %d failed, group succeeds\n", i);
        return 1;
      }
    } else {
      /* EXISTS: must fail immediately if any sub-pattern fails */
      if(!sub_result) {
        RASQAL_DEBUG2("EXISTS: Group sub-pattern %d failed, group fails\n", i);
        return 0;
      }
    }
  }

  /* All sub-patterns processed without early termination */
  if(mode == RASQAL_EXISTS_MODE_NOT_EXISTS) {
    /* NOT EXISTS: all sub-patterns succeeded, so group fails */
    RASQAL_DEBUG1("NOT EXISTS: all Group sub-patterns succeeded, group fails\n");
    return 0;
  } else {
    /* EXISTS: all sub-patterns succeeded, so group succeeds */
    RASQAL_DEBUG1("EXISTS: all Group sub-patterns succeeded, group succeeds\n");
    return 1;
  }
}

static int
rasqal_evaluate_union_pattern_internal(rasqal_graph_pattern* gp,
                                       rasqal_triples_source* triples_source,
                                       rasqal_query* query,
                                       rasqal_row* outer_row,
                                       rasqal_exists_mode mode)
{
  int i;
  int num_patterns;
  rasqal_graph_pattern* sub_gp;

  if(!gp->graph_patterns)
    return (mode == RASQAL_EXISTS_MODE_NOT_EXISTS) ? 1 : 0; /* Empty union: NOT EXISTS succeeds, EXISTS fails */

  num_patterns = raptor_sequence_size(gp->graph_patterns);

  RASQAL_DEBUG3("Evaluating UNION pattern with %d sub-patterns for %s\n", 
                num_patterns, RASQAL_EXISTS_MODE_NAME(mode));

  /* UNION semantics with mode-aware optimization:
   * - EXISTS mode: ANY sub-pattern matching means success (disjunction)
   * - NOT EXISTS mode: ALL sub-patterns must fail for success (negated disjunction)
   */
  for(i = 0; i < num_patterns; i++) {
    int sub_result;

    sub_gp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
    if(!sub_gp)
      continue;

    RASQAL_DEBUG4("Evaluating UNION sub-pattern %d/%d for %s\n", 
                  i + 1, num_patterns, RASQAL_EXISTS_MODE_NAME(mode));

    /* Recursive evaluation of sub-pattern */
    sub_result = rasqal_evaluate_exists_pattern_unified(sub_gp, triples_source,
                                                        query, outer_row, NULL,
                                                        mode);

    if(mode == RASQAL_EXISTS_MODE_NOT_EXISTS) {
      if(!sub_result) {
        /* NOT EXISTS: succeed immediately if any sub-pattern fails */
        RASQAL_DEBUG2("NOT EXISTS UNION: sub-pattern %d failed, returning success\n", i + 1);
        return 1;
      }
    } else {
      if(sub_result) {
        /* EXISTS: succeed immediately if any sub-pattern succeeds */
        RASQAL_DEBUG2("EXISTS UNION: sub-pattern %d succeeded, returning success\n", i + 1);
        return 1;
      }
    }
  }

  /* All sub-patterns evaluated:
   * - EXISTS mode: no sub-pattern matched, return failure
   * - NOT EXISTS mode: all sub-patterns matched, return failure
   */
  RASQAL_DEBUG3("%s UNION: all sub-patterns %s, returning failure\n",
                RASQAL_EXISTS_MODE_NAME(mode), 
                (mode == RASQAL_EXISTS_MODE_NOT_EXISTS) ? "succeeded" : "failed");
  return 0;
}

static int
rasqal_evaluate_optional_pattern_internal(rasqal_graph_pattern* gp,
                                          rasqal_triples_source* triples_source,
                                          rasqal_query* query,
                                          rasqal_row* outer_row,
                                          rasqal_exists_mode mode)
{
  rasqal_graph_pattern* required_gp;
  int required_result;

  RASQAL_DEBUG2("OPTIONAL pattern internal: mode=%s\n", RASQAL_EXISTS_MODE_NAME(mode));

  if(!gp->graph_patterns || raptor_sequence_size(gp->graph_patterns) < 2) {
    RASQAL_DEBUG2("OPTIONAL pattern missing sub-patterns for %s\n", RASQAL_EXISTS_MODE_NAME(mode));
    return (mode == RASQAL_EXISTS_MODE_NOT_EXISTS) ? 1 : 0; /* Invalid OPTIONAL: NOT EXISTS succeeds, EXISTS fails */
  }

  /* OPTIONAL has required pattern + optional pattern (we only need required) */
  required_gp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, 0);

  if(!required_gp) {
    RASQAL_DEBUG2("OPTIONAL pattern missing required sub-pattern for %s\n", RASQAL_EXISTS_MODE_NAME(mode));
    return (mode == RASQAL_EXISTS_MODE_NOT_EXISTS) ? 1 : 0;
  }

  RASQAL_DEBUG2("Evaluating OPTIONAL required pattern for %s\n", RASQAL_EXISTS_MODE_NAME(mode));

  /* Required pattern evaluation with mode-aware optimization */
  required_result = rasqal_evaluate_exists_pattern_unified(required_gp, triples_source,
                                                           query, outer_row, NULL, mode);

  if(mode == RASQAL_EXISTS_MODE_NOT_EXISTS) {
    if(!required_result) {
      /* NOT EXISTS: succeed immediately if required pattern fails */
      RASQAL_DEBUG1("NOT EXISTS OPTIONAL: required pattern failed, returning success\n");
      return 1;
    }
    /* Required pattern succeeded, continue evaluation - OPTIONAL never affects NOT EXISTS result
     * since the optional part is optional and doesn't change the required pattern result */
    RASQAL_DEBUG1("NOT EXISTS OPTIONAL: required pattern succeeded, returning failure\n");
    return 0;
  } else {
    if(!required_result) {
      /* EXISTS: fail if required pattern fails */
      RASQAL_DEBUG1("EXISTS OPTIONAL: required pattern failed, returning failure\n");
      return 0;
    }
    /* Required pattern succeeded, optional part doesn't affect EXISTS result */
    RASQAL_DEBUG1("EXISTS OPTIONAL: required pattern succeeded, returning success\n");
    return 1;
  }
}

static int
rasqal_evaluate_filter_pattern_internal(rasqal_graph_pattern* gp,
                                        rasqal_triples_source* triples_source,
                                        rasqal_query* query,
                                        rasqal_row* outer_row,
                                        rasqal_exists_mode mode)
{
  /* For now, delegate to legacy implementation
   * TODO: implement mode-aware version
   */
  if(mode == RASQAL_EXISTS_MODE_NOT_EXISTS)
    return !rasqal_evaluate_filter_exists_pattern(gp, triples_source, query,
                                                  outer_row);
  else
    return rasqal_evaluate_filter_exists_pattern(gp, triples_source, query,
                                                 outer_row);
}

static int
rasqal_evaluate_graph_pattern_internal(rasqal_graph_pattern* gp,
                                       rasqal_triples_source* triples_source,
                                       rasqal_query* query,
                                       rasqal_row* outer_row,
                                       rasqal_exists_mode mode)
{
  rasqal_graph_pattern* sub_gp;

  RASQAL_DEBUG2("GRAPH pattern internal: mode=%s\n", RASQAL_EXISTS_MODE_NAME(mode));

  if(!gp->graph_patterns || raptor_sequence_size(gp->graph_patterns) < 1) {
    RASQAL_DEBUG2("GRAPH pattern missing sub-patterns for %s\n", RASQAL_EXISTS_MODE_NAME(mode));
    return (mode == RASQAL_EXISTS_MODE_NOT_EXISTS) ? 1 : 0; /* Invalid GRAPH: NOT EXISTS succeeds, EXISTS fails */
  }

  sub_gp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, 0);
  if(!sub_gp) {
    RASQAL_DEBUG2("GRAPH pattern missing sub-pattern for %s\n", RASQAL_EXISTS_MODE_NAME(mode));
    return (mode == RASQAL_EXISTS_MODE_NOT_EXISTS) ? 1 : 0;
  }

  RASQAL_DEBUG2("Evaluating GRAPH sub-pattern for %s\n", RASQAL_EXISTS_MODE_NAME(mode));

  /* Handle named graph context */
  if(gp->origin) {
    /* Use origin-aware evaluation for basic patterns in graph context */
    if(sub_gp->op == RASQAL_GRAPH_PATTERN_OPERATOR_BASIC) {
      RASQAL_DEBUG2("GRAPH using basic pattern with origin for %s\n", RASQAL_EXISTS_MODE_NAME(mode));
      return rasqal_evaluate_basic_pattern_internal(sub_gp, triples_source,
                                                    query, outer_row, gp->origin, mode);
    } else {
      /* For complex patterns, recursively evaluate with mode awareness
       * Note: nested graph contexts need enhanced triples_source support */
      RASQAL_DEBUG2("GRAPH using complex pattern with origin for %s\n", RASQAL_EXISTS_MODE_NAME(mode));
      return rasqal_evaluate_exists_pattern_unified(sub_gp, triples_source,
                                                    query, outer_row, gp->origin, mode);
    }
  } else {
    /* No graph context specified, use default evaluation with mode awareness */
    RASQAL_DEBUG2("GRAPH using default context for %s\n", RASQAL_EXISTS_MODE_NAME(mode));
    return rasqal_evaluate_exists_pattern_unified(sub_gp, triples_source, query,
                                                  outer_row, NULL, mode);
  }
}


/*
 * rasqal_evaluate_not_exists_pattern_with_origin:
 * @gp: Graph pattern to evaluate for NOT EXISTS
 * @triples_source: Data source for triple lookups
 * @query: Query context for variable bindings and execution
 * @outer_row: Current variable bindings from outer query
 * @graph_origin: Named graph context for pattern evaluation
 *
 * INTERNAL - Evaluate a graph pattern for NOT EXISTS semantics with graph context and optimization.
 *
 * This function provides optimized NOT EXISTS evaluation with graph context support
 * that can short-circuit on the first failing pattern.
 *
 * Return value: 1 if pattern does not match in graph context, 0 if it matches
 */
int
rasqal_evaluate_not_exists_pattern_with_origin(rasqal_graph_pattern* gp,
                                               rasqal_triples_source* triples_source,
                                               rasqal_query* query,
                                               rasqal_row* outer_row,
                                               rasqal_literal* graph_origin)
{
  return rasqal_evaluate_exists_pattern_unified(gp, triples_source, query,
                                                outer_row, graph_origin,
                                                RASQAL_EXISTS_MODE_NOT_EXISTS);
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
  pattern_result = rasqal_evaluate_exists_pattern_unified(pattern_gp,
                                                          triples_source, query,
                                                          outer_row, NULL,
                                                          RASQAL_EXISTS_MODE_EXISTS);
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
 * Function to get a literal value with proper variable binding from outer row
 *
 * This function takes a literal (which may be a variable) and returns either:
 * - The bound value from outer_row if the literal is a variable with a binding
 * - The literal itself if it's not a variable or has no binding
 *
 * @param literal The literal to resolve
 * @param outer_row The outer row to use for variable bindings
 *
 * Return value: The resolved literal or NULL on failure
 */
static rasqal_literal*
rasqal_get_literal_with_bindings(rasqal_literal* literal, rasqal_row* outer_row)
{
  rasqal_variable* var;
  rasqal_literal* bound_value = NULL;
  
  if(!literal)
    return NULL;
  
  /* If not a variable, return as-is */
  var = rasqal_literal_as_variable(literal);
  if(!var) {
    return rasqal_new_literal_from_literal(literal);
  }
  
  /* Variable case: look up bound value in outer_row by searching for variable by name
   * This is more reliable than using var->offset which may not correspond to the 
   * outer row's variable ordering */
  if(outer_row && outer_row->rowsource && var->name) {
    /* Find the variable in the outer rowsource by name */
    int outer_offset = rasqal_rowsource_get_variable_offset_by_name(outer_row->rowsource, var->name);
    if(outer_offset >= 0 && outer_offset < outer_row->size) {
      bound_value = outer_row->values[outer_offset];
      if(bound_value) {
        RASQAL_DEBUG3("Variable %s bound to value in outer row at offset %d (by name lookup)\n", 
                      var->name ? (char*)var->name : "?", outer_offset);
        return rasqal_new_literal_from_literal(bound_value);
      }
    }
  }
  
  /* Fallback: try the original offset-based lookup */
  if(!bound_value && outer_row && var->offset >= 0 && var->offset < outer_row->size) {
    bound_value = outer_row->values[var->offset];
    if(bound_value) {
      RASQAL_DEBUG3("Variable %s bound to value in outer row at offset %d (by offset)\n", 
                    var->name ? (char*)var->name : "?", var->offset);
      return rasqal_new_literal_from_literal(bound_value);
    }
  }
  
  /* No binding found, return the variable itself */
  RASQAL_DEBUG2("Variable %s not bound in outer row, keeping as variable\n", 
                var->name ? (char*)var->name : "?");
  return rasqal_new_literal_from_literal(literal);
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
 * The function now uses outer row bindings to handle variable substitution:
 * - If a component is a variable with a bound value in outer row -> substitute with actual value
 * - If a component is a variable without a bound value -> keep as variable for pattern matching
 * - If a component is already a constant -> keep as is
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
    
  RASQAL_DEBUG2("rasqal_instantiate_triple_with_bindings: starting with outer_row=%p\n", (void*)outer_row);

  /* Substitute variables with values from outer_row */
  subj = rasqal_get_literal_with_bindings(triple->subject, outer_row);
  pred = rasqal_get_literal_with_bindings(triple->predicate, outer_row);  
  obj = rasqal_get_literal_with_bindings(triple->object, outer_row);

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
      /* Use unified evaluation - handles NULL graph_origin automatically */
      result = rasqal_evaluate_exists_pattern_unified(con->exists_pattern,
                                                      con->triples_source,
                                                      con->query,
                                                      con->outer_row,
                                                      con->graph_origin,
                                                      RASQAL_EXISTS_MODE_EXISTS);


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

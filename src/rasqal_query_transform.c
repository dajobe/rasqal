/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_query_transform.c - Rasqal query transformations
 *
 * Copyright (C) 2004-2009, David Beckett http://www.dajobe.org/
 * Copyright (C) 2004-2005, University of Bristol, UK http://www.bristol.ac.uk/
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
#include <stdarg.h>

#include "rasqal.h"
#include "rasqal_internal.h"


#if 0
#undef RASQAL_DEBUG
#define RASQAL_DEBUG 2
#endif

#define DEBUG_FH stderr

/* prototype for later */
static int rasqal_query_build_variables_use_map(rasqal_query* query);
static void rasqal_query_graph_build_variables_use_map_in_internal(rasqal_query* query, short *use_map, rasqal_literal *origin);
static void rasqal_query_filter_build_variables_use_map_in_internal(rasqal_query* query, short *use_map, rasqal_expression* e);
static void rasqal_query_let_build_variables_use_map_in_internal(rasqal_query* query, short *use_map, rasqal_variable *var, rasqal_expression* e);


int
rasqal_query_expand_triple_qnames(rasqal_query* rq)
{
  int i;

  if(!rq->triples)
    return 0;
  
  /* expand qnames in triples */
  for(i = 0; i< raptor_sequence_size(rq->triples); i++) {
    rasqal_triple* t = (rasqal_triple*)raptor_sequence_get_at(rq->triples, i);
    if(rasqal_literal_expand_qname(rq, t->subject) ||
       rasqal_literal_expand_qname(rq, t->predicate) ||
       rasqal_literal_expand_qname(rq, t->object))
      return 1;
  }

  return 0;
}


int
rasqal_sequence_has_qname(raptor_sequence *seq)
{
  int i;

  if(!seq)
    return 0;
  
  /* expand qnames in triples */
  for(i = 0; i< raptor_sequence_size(seq); i++) {
    rasqal_triple* t = (rasqal_triple*)raptor_sequence_get_at(seq, i);
    if(rasqal_literal_has_qname(t->subject) ||
       rasqal_literal_has_qname(t->predicate) ||
       rasqal_literal_has_qname(t->object))
      return 1;
  }

  return 0;
}


static int
rasqal_graph_pattern_constraints_has_qname(rasqal_graph_pattern* gp) 
{
  int i;
  
  /* check for qnames in sub graph patterns */
  if(gp->graph_patterns) {
    /* check for constraint qnames in rasqal_graph_patterns */
    for(i = 0; i < raptor_sequence_size(gp->graph_patterns); i++) {
      rasqal_graph_pattern *sgp;
      sgp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      if(rasqal_graph_pattern_constraints_has_qname(sgp))
        return 1;
    }
  }

  if(!gp->filter_expression)
    return 0;
  
  /* check for qnames in constraint expressions */
  if(rasqal_expression_visit(gp->filter_expression,
                             rasqal_expression_has_qname, gp))
    return 1;

  return 0;
}


int
rasqal_query_constraints_has_qname(rasqal_query* rq) 
{
  if(!rq->query_graph_pattern)
    return 0;
  
  return rasqal_graph_pattern_constraints_has_qname(rq->query_graph_pattern);
}


int
rasqal_query_expand_graph_pattern_constraints_qnames(rasqal_query *rq,
                                                     rasqal_graph_pattern* gp)
{
  int i;
  
  /* expand qnames in sub graph patterns */
  if(gp->graph_patterns) {
    /* check for constraint qnames in rasqal_graph_patterns */
    for(i = 0; i < raptor_sequence_size(gp->graph_patterns); i++) {
      rasqal_graph_pattern *sgp;
      sgp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      if(rasqal_query_expand_graph_pattern_constraints_qnames(rq, sgp))
        return 1;
    }
  }

  if(!gp->filter_expression)
    return 0;
  
  /* expand qnames in constraint expressions */
  if(rasqal_expression_visit(gp->filter_expression,
                             rasqal_expression_expand_qname, rq))
    return 1;

  return 0;
}


int
rasqal_query_expand_query_constraints_qnames(rasqal_query *rq) 
{
  return rasqal_query_expand_graph_pattern_constraints_qnames(rq, 
                                                              rq->query_graph_pattern);
}


static int
rasqal_query_convert_blank_node_to_anonymous_variable(rasqal_query *rq,
                                                       rasqal_literal *l)
{
  rasqal_variable* v;
  
  v = rasqal_new_variable_typed(rq, 
                                RASQAL_VARIABLE_TYPE_ANONYMOUS,
                                (unsigned char*)l->string, NULL);
  /* rasqal_new_variable_typed took ownership of the l->string name.
   * Set to NULL to prevent double delete. */
  l->string = NULL;
  
  if(!v)
    return 1; /* error */

  /* Convert the blank node literal into a variable literal */
  l->type = RASQAL_LITERAL_VARIABLE;
  l->value.variable = v;

  return 0; /* success */
}


/**
 * rasqal_query_build_anonymous_variables:
 * @rq: query
 *
 * INTERNAL - Turn triple blank node parts into anonymous variables
 *
 * These are the blank nodes such as (Turtle/SPARQL):
 *   _:name or [] or [ prop value ] or ( collection of things )
 *
 * Return value: non-0 on failure
 */
int
rasqal_query_build_anonymous_variables(rasqal_query* rq)
{
  int i;
  int rc = 1;
  raptor_sequence *s = rq->triples;
  
  for(i = 0; i < raptor_sequence_size(s); i++) {
    rasqal_triple* t = (rasqal_triple*)raptor_sequence_get_at(s, i);
    if(t->subject->type == RASQAL_LITERAL_BLANK &&
       rasqal_query_convert_blank_node_to_anonymous_variable(rq, t->subject))
      goto done;
    if(t->predicate->type == RASQAL_LITERAL_BLANK &&
       rasqal_query_convert_blank_node_to_anonymous_variable(rq, t->predicate))
      goto done;
    if(t->object->type == RASQAL_LITERAL_BLANK &&
       rasqal_query_convert_blank_node_to_anonymous_variable(rq, t->object))
      goto done;
  }

  rc = 0;

  done:
  return rc;
}


/**
 * rasqal_query_expand_wildcards:
 * @rq: query
 *
 * INTERNAL - expand RDQL/SPARQL SELECT * to a full list of select variables
 *
 * Return value: non-0 on failure
 */
int
rasqal_query_expand_wildcards(rasqal_query* rq)
{
  int i;
  int size;
  
  if(rq->verb != RASQAL_QUERY_VERB_SELECT || !rq->wildcard)
    return 0;
  
  /* If 'SELECT *' was given, make the selects be a list of all variables */
  rq->selects = raptor_new_sequence(NULL, (raptor_sequence_print_handler*)rasqal_variable_print);
  if(!rq->selects)
    return 1;
  
  size = rasqal_variables_table_get_named_variables_count(rq->vars_table);
  for(i = 0; i < size; i++) {
    rasqal_variable* v = rasqal_variables_table_get(rq->vars_table, i);
    if(raptor_sequence_push(rq->selects, v))
      return 1;
  }

  rq->select_variables_count = size;

  return 0;
}


/**
 * rasqal_query_remove_duplicate_select_vars:
 * @rq: query
 *
 * INTERNAL - remove duplicate variables in SELECT sequence and warn
 *
 * The order of the select variables is preserved.
 *
 * Return value: non-0 on failure
 */
int
rasqal_query_remove_duplicate_select_vars(rasqal_query* rq)
{
  int i;
  int modified = 0;
  int size;
  raptor_sequence* seq = rq->selects;
  raptor_sequence* new_seq;
  
  if(!seq)
    return 1;

  size = raptor_sequence_size(seq);
  if(!size)
    return 0;
  
  new_seq = raptor_new_sequence(NULL, (raptor_sequence_print_handler*)rasqal_variable_print);
  if(!new_seq)
    return 1;
  
#if RASQAL_DEBUG > 1
  RASQAL_DEBUG1("bound variables before deduping: "); 
  raptor_sequence_print(rq->selects, DEBUG_FH);
  fputs("\n", DEBUG_FH); 
#endif

  for(i = 0; i < size; i++) {
    int j;
    rasqal_variable *v;
    int warned = 0;
    
    v = (rasqal_variable*)raptor_sequence_get_at(seq, i);
    if(!v)
      continue;

    for(j = 0; j < i; j++) {
      rasqal_variable *v2;
      v2 = (rasqal_variable*)raptor_sequence_get_at(seq, j);
      
      if(v == v2) {
        if(!warned) {
          rasqal_log_error_simple(rq->world, RAPTOR_LOG_LEVEL_WARNING,
                                  &rq->locator,
                                  "Variable %s duplicated in SELECT.", 
                                  v->name);
          warned = 1;
        }
      }
    }
    if(!warned) {
      raptor_sequence_push(new_seq, v);
      modified = 1;
    }
  }
  
  if(modified) {
#if RASQAL_DEBUG > 1
    RASQAL_DEBUG1("bound variables after deduping: "); 
    raptor_sequence_print(new_seq, DEBUG_FH);
    fputs("\n", DEBUG_FH); 
#endif
    raptor_free_sequence(rq->selects);
    rq->selects = new_seq;
    rq->select_variables_count = raptor_sequence_size(rq->selects);
  } else
    raptor_free_sequence(new_seq);

  return 0;
}


/**
 * rasqal_query_triples_build_bound_in_internal:
 * @query: the #rasqal_query to find the variables in
 * @bound_in: array to write bound_in
 * @start_column: first column in triples array
 * @end_column: last column in triples array
 *
 * INTERNAL - Mark triples where variables are bound in a sequence of triples
 * 
 **/
static void
rasqal_query_triples_build_bound_in_internal(rasqal_query* query,
                                             int *bound_in,
                                             int start_column,
                                             int end_column)
{
  int col;
  
  for(col = start_column; col <= end_column; col++) {
    rasqal_triple *t;
    rasqal_variable *v;
    
    t = (rasqal_triple*)raptor_sequence_get_at(query->triples, col);

    if((v = rasqal_literal_as_variable(t->subject))) {
      if(bound_in[v->offset] < 0)
        bound_in[v->offset] = col;
    }
    if((v = rasqal_literal_as_variable(t->predicate))) {
      if(bound_in[v->offset] < 0)
        bound_in[v->offset] = col;
    }
    if((v = rasqal_literal_as_variable(t->object))) {
      if(bound_in[v->offset] < 0)
        bound_in[v->offset] = col;
    }
    if(t->origin) {
      if((v = rasqal_literal_as_variable(t->origin))) {
        if(bound_in[v->offset] < 0)
          bound_in[v->offset] = col;
      }
    }

  }
}


/**
 * rasqal_query_triples_build_bound_in:
 * @query: the #rasqal_query to find the variables in
 * @size: number of variables / size of return bound_in array
 * @start_column: first column in triples array
 * @end_column: last column in triples array
 *
 * INTERNAL - Mark where variables are bound in a graph_pattern tree walk
 *
 * Return value: array of integers (column numbers) marking in which triple pattern a variable is bound
 **/
int*
rasqal_query_triples_build_bound_in(rasqal_query* query,
                                    int size,
                                    int start_column,
                                    int end_column)
{
  int i;
  int *bound_in;
  
  bound_in = (int*)RASQAL_CALLOC(intarray, size+1, sizeof(int));
  if(!bound_in)
    return NULL;

  for(i = 0; i < size; i++)
    bound_in[i] = -1;

  rasqal_query_triples_build_bound_in_internal(query, bound_in,
                                               start_column,
                                               end_column);
  return bound_in;
}


/**
 * rasqal_query_graph_pattern_build_bound_in:
 * @query: the #rasqal_query to find the variables in
 * @bound_in: array to write bound_in
 * @gp: graph pattern to use
 *
 * INTERNAL - Mark where variables are bound in a graph_pattern tree walk
 * 
 **/
static int
rasqal_query_graph_pattern_build_bound_in(rasqal_query* query,
                                          int *bound_in,
                                          rasqal_graph_pattern *gp)
{
  if(gp->graph_patterns) {
    int i;

    for(i = 0; i < raptor_sequence_size(gp->graph_patterns); i++) {
      rasqal_graph_pattern *sgp;
      sgp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      if(rasqal_query_graph_pattern_build_bound_in(query, bound_in, sgp))
        return 1;
    }
  }

  if(!gp->triples)
    return 0;

  rasqal_query_triples_build_bound_in_internal(query, bound_in,
                                               gp->start_column,
                                               gp->end_column);
  return 0;
}


/**
 * rasqal_query_build_bound_in:
 * @query: the #rasqal_query to find the variables in
 *
 * INTERNAL - Record the triple columns where variables are bound in a query
 *
 * Constructs an array indexed by variable offset of columns where
 * the variable is bound.  The order used is a tree walk of
 * the graph patterns.  Later mentions of the variable are not marked.
 *
 * Return value: non-0 on failure
 **/
static int
rasqal_query_build_bound_in(rasqal_query* query)
{
  int i;
  int size;

  size = rasqal_variables_table_get_total_variables_count(query->vars_table);
  
  query->variables_bound_in = (int*)RASQAL_CALLOC(intarray, size+1, sizeof(int));
  if(!query->variables_bound_in)
    return 1;

  for(i = 0; i < size; i++)
    query->variables_bound_in[i] = -1;

  return rasqal_query_graph_pattern_build_bound_in(query,
                                                   query->variables_bound_in,
                                                   query->query_graph_pattern);
}


/**
 * rasqal_query_check_unused_variables:
 * @query: the #rasqal_query to check
 * @bound_in: array of columns where variables are bound as created by rasqal_query_build_bound_in()
 *
 * INTERNAL - warn variables that are selected but not bound in a triple
 *
 * NOTE: Fails to handle variables bound in GRAPH expression, SELECT
 * expressions (LAQRS, SPARQL 1.1) or in other places (none at present).
 *
 * Return value: non-0 on failure
 */
static int
rasqal_query_check_unused_variables(rasqal_query* query, int *bound_in)
{
  int i;
  int size;
  
  /* check bound_in only for named variables since only they can
   * appear in SELECT $vars 
   */
  size = rasqal_variables_table_get_named_variables_count(query->vars_table);
  for(i = 0; i < size; i++) {
    int column = bound_in[i];
    rasqal_variable *v;

    v = rasqal_variables_table_get(query->vars_table, i);
    if(column >= 0) {
#if RASQAL_DEBUG > 1
      RASQAL_DEBUG4("Variable %s (%d) was bound in column %d\n",
                    v->name, i, column);
#endif
    } else if(!v->expression)
      rasqal_log_error_simple(query->world, RAPTOR_LOG_LEVEL_WARNING,
                              &query->locator,
                              "Variable %s was selected but is unused in the query.", 
                              v->name);
  }

  return 0;
}


/**
 * rasqal_query_merge_triple_patterns:
 * @query: query (not used here)
 * @gp: current graph pattern
 * @data: visit data (not used here)
 *
 * INTERNAL - Join triple patterns in adjacent basic graph patterns into
 * single basic graph pattern.
 *
 * For group graph pattern move all triples
 *  from { { a } { b } { c }  D... } 
 *  to { a b c  D... }
 *  if the types of a, b, c are all BASIC GPs (just triples)
 *   D... is anything else
 * 
 */
static int
rasqal_query_merge_triple_patterns(rasqal_query* query,
                                   rasqal_graph_pattern* gp,
                                   void* data)
{
  int* modified = (int*)data;
  int checking;
  int offset;

#if RASQAL_DEBUG > 1
  printf("rasqal_query_merge_triple_patterns: Checking graph pattern #%d:\n  ", gp->gp_index);
  rasqal_graph_pattern_print(gp, stdout);
  fputs("\n", stdout);
  RASQAL_DEBUG3("Columns %d to %d\n", gp->start_column, gp->end_column);
#endif
    
  if(!gp->graph_patterns) {
#if RASQAL_DEBUG > 1
    RASQAL_DEBUG2("Ending graph patterns %d - no sub-graph patterns\n", gp->gp_index);
#endif
    return 0;
  }

  if(gp->op != RASQAL_GRAPH_PATTERN_OPERATOR_GROUP) {
#if RASQAL_DEBUG > 1
    RASQAL_DEBUG3("Ending graph patterns %d - operator %s\n", gp->gp_index,
                  rasqal_graph_pattern_operator_as_string(gp->op));
#endif
    return 0;
  }


  checking = 1;
  offset = 0;
  while(checking) {
    int bgp_count;
    rasqal_graph_pattern *dest_bgp;
    raptor_sequence *seq;
    int i, j;
    int first = 0, last = 0;
    int size = raptor_sequence_size(gp->graph_patterns);
    
    /* find first basic graph pattern starting at offset */
    for(i= offset; i < size; i++) {
      rasqal_graph_pattern *sgp;
      sgp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);

      if(sgp->op == RASQAL_GRAPH_PATTERN_OPERATOR_BASIC) {
        first = i;
        break;
      }
    }
    
    /* None found */
    if(i >= size)
      break;

    /* Next time, start after this BGP */
    offset = i+1;
    
    /* count basic graph patterns */
    bgp_count = 0;
    dest_bgp = NULL; /* destination graph pattern */
    for(j = i; j < size; j++) {
      rasqal_graph_pattern *sgp;
      sgp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, j);

      if(sgp->op == RASQAL_GRAPH_PATTERN_OPERATOR_BASIC) {
        bgp_count++;
        if(!dest_bgp)
          dest_bgp = sgp;
        last = j;
      } else
        break;
    }


  #if RASQAL_DEBUG > 1
    RASQAL_DEBUG3("Found sequence of %d basic sub-graph patterns in %d\n", bgp_count, gp->gp_index);
  #endif
    if(bgp_count < 2)
      continue;

  #if RASQAL_DEBUG > 1
    RASQAL_DEBUG3("OK to merge %d basic sub-graph patterns of %d\n", bgp_count, gp->gp_index);

    RASQAL_DEBUG3("Initial columns %d to %d\n", gp->start_column, gp->end_column);
  #endif
    seq = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_graph_pattern, (raptor_sequence_print_handler*)rasqal_graph_pattern_print);
    if(!seq)
      return 1;
    for(i = 0; raptor_sequence_size(gp->graph_patterns) > 0; i++) {
      rasqal_graph_pattern *sgp;
      sgp = (rasqal_graph_pattern*)raptor_sequence_unshift(gp->graph_patterns);
      if(i >= first && i <= last) {
        if(sgp != dest_bgp) {
          if(rasqal_graph_patterns_join(dest_bgp, sgp)) {
            RASQAL_DEBUG1("Cannot join graph patterns\n");
            *modified = -1; /* error flag */
          }
          rasqal_free_graph_pattern(sgp);
        } else
          raptor_sequence_push(seq, sgp);
      } else
        raptor_sequence_push(seq, sgp);
    }
    raptor_free_sequence(gp->graph_patterns);
    gp->graph_patterns = seq;

    if(!*modified)
      *modified = 1;

  } /* end while checking */
  

#if RASQAL_DEBUG > 1
  RASQAL_DEBUG3("Ending columns %d to %d\n", gp->start_column, gp->end_column);

  RASQAL_DEBUG2("Ending graph pattern #%d\n  ", gp->gp_index);
  rasqal_graph_pattern_print(gp, stdout);
  fputs("\n\n", stdout);
#endif

  return 0;
}


/**
 * rasqal_graph_pattern_move_constraints:
 * @dest_gp: destination graph pattern
 * @src_gp: src graph pattern
 *
 * INTERNAL - copy all constraints from @src_gp graph pattern to @src_gp graph pattern
 *
 * Return value: non-0 on error
 */
int
rasqal_graph_pattern_move_constraints(rasqal_graph_pattern* dest_gp, 
                                      rasqal_graph_pattern* src_gp)
{
  int rc = 0;
  rasqal_expression* fs = NULL;
  rasqal_expression* e;
  
  if(!src_gp->filter_expression)
    return 0; /* no constraints is not an error */
  
  e = rasqal_new_expression_from_expression(src_gp->filter_expression);
  fs = dest_gp->filter_expression;
  if(fs)
    e = rasqal_new_2op_expression(e->world, RASQAL_EXPR_AND, fs, e);
  dest_gp->filter_expression = e;

  return rc;
}


/**
 * rasqal_query_remove_empty_group_graph_patterns:
 * @query: query (not used here)
 * @gp: current graph pattern
 * @data: visit data (not used here)
 *
 * INTERNAL - Remove empty group graph patterns
 *
 * Return value: non-0 on failure
 */
static int
rasqal_query_remove_empty_group_graph_patterns(rasqal_query* query,
                                               rasqal_graph_pattern* gp,
                                               void* data)
{
  int i;
  int saw_empty_gp = 0;
  raptor_sequence *seq;
  int* modified = (int*)data;
  
  if(!gp->graph_patterns)
    return 0;

#if RASQAL_DEBUG > 1
  printf("rasqal_query_remove_empty_group_graph_patterns: Checking graph pattern #%d:\n  ", gp->gp_index);
  rasqal_graph_pattern_print(gp, stdout);
  fputs("\n", stdout);
#endif

  for(i = 0; i < raptor_sequence_size(gp->graph_patterns); i++) {
    rasqal_graph_pattern *sgp;
    sgp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
    if(sgp->graph_patterns && !raptor_sequence_size(sgp->graph_patterns)) {
      /* One is enough to know we need to rewrite */
      saw_empty_gp = 1;
      break;
    }
  }

  if(!saw_empty_gp) {
#if RASQAL_DEBUG > 1
    RASQAL_DEBUG2("Ending graph patterns %d - saw no empty groups\n", gp->gp_index);
#endif
    return 0;
  }
  
  
  seq = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_graph_pattern, (raptor_sequence_print_handler*)rasqal_graph_pattern_print);
  if(!seq) {
    RASQAL_DEBUG1("Cannot create new gp sequence\n");
    *modified = -1;
    return 1;
  }
  while(raptor_sequence_size(gp->graph_patterns) > 0) {
    rasqal_graph_pattern *sgp;
    sgp = (rasqal_graph_pattern*)raptor_sequence_unshift(gp->graph_patterns);
    if(sgp->graph_patterns && !raptor_sequence_size(sgp->graph_patterns)) {
      rasqal_graph_pattern_move_constraints(gp, sgp);
      rasqal_free_graph_pattern(sgp);
      continue;
    }
    raptor_sequence_push(seq, sgp);
  }
  raptor_free_sequence(gp->graph_patterns);
  gp->graph_patterns = seq;

  if(!*modified)
    *modified = 1;
  
#if RASQAL_DEBUG > 1
  RASQAL_DEBUG2("Ending graph pattern #%d\n  ", gp->gp_index);
  rasqal_graph_pattern_print(gp, stdout);
  fputs("\n\n", stdout);
#endif

  return 0;
}


/**
 * rasqal_query_merge_graph_patterns:
 * @query: query (not used here)
 * @gp: current graph pattern
 * @data: pointer to int modified flag
 *
 * INTERNAL - Merge graph patterns where possible
 *
 * When size = 1 (never for UNION)
 * GROUP { A } -> A
 * OPTIONAL { A } -> OPTIONAL { A }
 *
 * When size > 1
 * GROUP { BASIC{2,} } -> merge-BASIC
 * OPTIONAL { BASIC{2,} } -> OPTIONAL { merge-BASIC }
 *
 * Never merged: UNION
 */
int
rasqal_query_merge_graph_patterns(rasqal_query* query,
                                  rasqal_graph_pattern* gp,
                                  void* data)
{
  rasqal_graph_pattern_operator op;
  int merge_gp_ok = 0;
  int all_gp_op_same = 0;
  int i;
  int size;
  int* modified = (int*)data;
  
#if RASQAL_DEBUG > 1
  printf("rasqal_query_merge_graph_patterns: Checking graph pattern #%d:\n  ",
         gp->gp_index);
  rasqal_graph_pattern_print(gp, stdout);
  fputs("\n", stdout);
  RASQAL_DEBUG3("Columns %d to %d\n", gp->start_column, gp->end_column);
#endif

  if(!gp->graph_patterns) {
#if RASQAL_DEBUG > 1
    RASQAL_DEBUG3("Ending graph pattern #%d - operator %s: no sub-graph patterns\n", gp->gp_index,
                  rasqal_graph_pattern_operator_as_string(gp->op));
#endif
    return 0;
  }

  if(gp->op != RASQAL_GRAPH_PATTERN_OPERATOR_GROUP) {
#if RASQAL_DEBUG > 1
    RASQAL_DEBUG3("Ending graph patterns %d - operator %s: not GROUP\n", gp->gp_index,
                  rasqal_graph_pattern_operator_as_string(gp->op));
#endif
    return 0;
  }

  size = raptor_sequence_size(gp->graph_patterns);
#if RASQAL_DEBUG > 1
  RASQAL_DEBUG3("Doing %d sub-graph patterns of %d\n", size, gp->gp_index);
#endif
  op = RASQAL_GRAPH_PATTERN_OPERATOR_UNKNOWN;
  all_gp_op_same = 1;
  for(i = 0; i < size; i++) {
    rasqal_graph_pattern *sgp;
    sgp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
    if(op == RASQAL_GRAPH_PATTERN_OPERATOR_UNKNOWN) {
      op = sgp->op;
    } else {
      if(op != sgp->op) {
#if RASQAL_DEBUG > 1
        RASQAL_DEBUG4("Sub-graph pattern #%d is %s different from first %s, cannot merge\n", 
                      i, rasqal_graph_pattern_operator_as_string(sgp->op), 
                      rasqal_graph_pattern_operator_as_string(op));
#endif
        all_gp_op_same = 0;
      }
    }
  }
#if RASQAL_DEBUG > 1
  RASQAL_DEBUG2("Sub-graph patterns of %d done\n", gp->gp_index);
#endif
  
  if(!all_gp_op_same) {
    merge_gp_ok = 0;
    goto merge_check_done;
  }

  if(size == 1) {
    merge_gp_ok = 1;
    goto merge_check_done;
  }


  /* if size > 1 check if ALL sub-graph patterns are basic graph
   * patterns and either:
   *   1) a single triple
   *   2) a single constraint
   */
  for(i = 0; i < size; i++) {
    rasqal_graph_pattern *sgp;
    sgp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
    
    if(sgp->op != RASQAL_GRAPH_PATTERN_OPERATOR_BASIC) {
#if RASQAL_DEBUG > 1
      RASQAL_DEBUG3("Found %s sub-graph pattern #%d\n",
                    rasqal_graph_pattern_operator_as_string(sgp->op), 
                    sgp->gp_index);
#endif
      merge_gp_ok = 0;
      break;
    }
    
    /* not ok if there are >1 triples */
    if(sgp->triples && (sgp->end_column-sgp->start_column+1) > 1) {
#if RASQAL_DEBUG > 1
      RASQAL_DEBUG2("Found >1 triples in sub-graph pattern #%d\n", sgp->gp_index);
#endif
      merge_gp_ok = 0;
      break;
    }
    
    /* not ok if there are triples and constraints */
    if(sgp->triples && sgp->filter_expression) {
#if RASQAL_DEBUG > 1
      RASQAL_DEBUG2("Found triples and constraints in sub-graph pattern #%d\n", sgp->gp_index);
#endif
      merge_gp_ok = 0;
      break;
    }
    
    /* was at least 1 OK sub graph-pattern */
    merge_gp_ok = 1;
  }

  merge_check_done:
  
  if(merge_gp_ok) {
    raptor_sequence *seq;
    
#if RASQAL_DEBUG > 1
    RASQAL_DEBUG2("OK to merge sub-graph patterns of %d\n", gp->gp_index);

    RASQAL_DEBUG3("Initial columns %d to %d\n", gp->start_column, gp->end_column);
#endif

    /* Pretend dest is an empty basic graph pattern */
    seq = gp->graph_patterns;
    gp->graph_patterns = NULL;
    gp->op = op;
    
    while(raptor_sequence_size(seq) > 0) {
      rasqal_graph_pattern *sgp;
      sgp = (rasqal_graph_pattern*)raptor_sequence_unshift(seq);

      /* fake this so that the join happens */
      sgp->op = gp->op;
      if(rasqal_graph_patterns_join(gp, sgp)) {
        RASQAL_DEBUG1("Cannot join graph patterns\n");
        *modified = -1; /* error flag */
      }
      rasqal_free_graph_pattern(sgp);
    }

    /* If result is 'basic' but contains graph patterns, turn it into a group */
    if(gp->graph_patterns && gp->op == RASQAL_GRAPH_PATTERN_OPERATOR_BASIC)
      gp->op = RASQAL_GRAPH_PATTERN_OPERATOR_GROUP;

    /* Delete any evidence of sub graph patterns */
    raptor_free_sequence(seq);

    if(!*modified)
      *modified = 1;
    
  } else {
#if RASQAL_DEBUG > 1
    RASQAL_DEBUG2("NOT OK to merge sub-graph patterns of %d\n", gp->gp_index);
#endif
  }

#if RASQAL_DEBUG > 1
  if(merge_gp_ok) {
    RASQAL_DEBUG2("Ending graph pattern #%d\n  ", gp->gp_index);
    rasqal_graph_pattern_print(gp, stdout);
    fputs("\n\n", stdout);
  }
#endif

  return 0;
}


struct folding_state {
  rasqal_query* query;
  int changes;
  int failed;
};
  

static int
rasqal_expression_foreach_fold(void *user_data, rasqal_expression *e)
{
  struct folding_state *st = (struct folding_state*)user_data;
  rasqal_query* query;  
  rasqal_literal* l;

  /* skip if already a  literal or this expression tree is not constant */
  if(e->op == RASQAL_EXPR_LITERAL || !rasqal_expression_is_constant(e))
    return 0;
  
#ifdef RASQAL_DEBUG
  RASQAL_DEBUG2("folding expression %p: ", e);
  rasqal_expression_print(e, DEBUG_FH);
  fprintf(DEBUG_FH, "\n");
#endif
  
  query = st->query;
  l = rasqal_expression_evaluate_v2(query->world, &query->locator,
                                    e, query->compare_flags);
  if(!l) {
    st->failed++;
    return 1;
  }

  /* In-situ conversion of 'e' to a literal expression */
  rasqal_expression_convert_to_literal(e, l);
  
#ifdef RASQAL_DEBUG
  RASQAL_DEBUG1("folded expression now: ");
  rasqal_expression_print(e, DEBUG_FH);
  fputc('\n', DEBUG_FH);
#endif

  /* change made */
  st->changes++;
  
  return 0;
}


static int
rasqal_query_expression_fold(rasqal_query* rq, rasqal_expression* e)
{
  struct folding_state st;

  st.query = rq;
  while(1) {
    st.changes = 0;
    st.failed = 0;
    rasqal_expression_visit(e, rasqal_expression_foreach_fold, 
                            (void*)&st);
    if(!st.changes || st.failed)
      break;
  }

  return st.failed;
}


static int
rasqal_graph_pattern_fold_expressions(rasqal_query* rq,
                                      rasqal_graph_pattern* gp)
{
  if(!gp)
    return 1;
  
  /* fold expressions in sub graph patterns */
  if(gp->graph_patterns) {
    int i;
    
    for(i = 0; i < raptor_sequence_size(gp->graph_patterns); i++) {
      rasqal_graph_pattern *sgp;
      sgp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      if(rasqal_graph_pattern_fold_expressions(rq, sgp))
        return 1;
    }
  }

  if(gp->filter_expression)
    return rasqal_query_expression_fold(rq, gp->filter_expression);

  return 0;
}


static int
rasqal_query_fold_expressions(rasqal_query* rq)
{
  rasqal_graph_pattern *gp = rq->query_graph_pattern;
  int order_size;

  if(gp)
    rasqal_graph_pattern_fold_expressions(rq, gp);

  if(!rq->order_conditions_sequence)
    return 0;
  
  order_size = raptor_sequence_size(rq->order_conditions_sequence);
  if(order_size) {
    int i;
    
    for(i = 0; i < order_size; i++) {
      rasqal_expression* e;
      e = (rasqal_expression*)raptor_sequence_get_at(rq->order_conditions_sequence, i);
      rasqal_query_expression_fold(rq, e);
    }
  }

  return 0;
}


static int
rasqal_query_prepare_count_graph_patterns(rasqal_query* query,
                                          rasqal_graph_pattern* gp,
                                          void* data)
{
  raptor_sequence* seq = (raptor_sequence*)data;

  if(raptor_sequence_push(seq, gp)) {
    query->failed = 1;
    return 1;
  }
  gp->gp_index = (query->graph_pattern_count++);
  return 0;
}


/**
 * rasqal_query_prepare_common:
 * @query: query
 *
 * INTERNAL - initialise the remainder of the query structures
 *
 * Does not do any execution prepration - this is once-only stuff.
 *
 * NOTE: The caller is responsible for ensuring this is called at
 * most once.  This is currently enforced by rasqal_query_prepare()
 * using the query->prepared flag when it calls the query factory
 * prepare method which does the query string parsing and ends by
 * calling this function.
 *
 * Return value: non-0 on failure
 */
int
rasqal_query_prepare_common(rasqal_query *query)
{
  int rc = 1;

  if(!query->triples)
    goto done;
  
  /* turn SELECT $a, $a into SELECT $a - editing query->selects */
  if(query->selects) {
    if(rasqal_query_remove_duplicate_select_vars(query))
      goto done;
  }

  rasqal_query_fold_expressions(query);

  if(query->query_graph_pattern) {
    /* This query prepare processing requires a query graph pattern.
     * Not the case for a legal query like 'DESCRIBE <uri>'
     */

#ifndef RASQAL_NO_GP_MERGE
    int modified;
    
#if RASQAL_DEBUG > 1
    fputs("Initial query graph pattern:\n  ", DEBUG_FH);
    rasqal_graph_pattern_print(query->query_graph_pattern, DEBUG_FH);
    fputs("\n", DEBUG_FH);
#endif

    do {
      modified = 0;
      
      rasqal_query_graph_pattern_visit(query, 
                                       rasqal_query_merge_triple_patterns,
                                       &modified);
      
#if RASQAL_DEBUG > 1
      fprintf(DEBUG_FH, "modified=%d after merge triples, query graph pattern now:\n  ", modified);
      rasqal_graph_pattern_print(query->query_graph_pattern, DEBUG_FH);
      fputs("\n", DEBUG_FH);
#endif

      rasqal_query_graph_pattern_visit(query,
                                       rasqal_query_remove_empty_group_graph_patterns,
                                       &modified);
      
#if RASQAL_DEBUG > 1
      fprintf(DEBUG_FH, "modified=%d after remove empty groups, query graph pattern now:\n  ", modified);
      rasqal_graph_pattern_print(query->query_graph_pattern, DEBUG_FH);
      fputs("\n", DEBUG_FH);
#endif

      rasqal_query_graph_pattern_visit(query, 
                                       rasqal_query_merge_graph_patterns,
                                       &modified);

#if RASQAL_DEBUG > 1
      fprintf(DEBUG_FH, "modified=%d  after merge graph patterns, query graph pattern now:\n  ", modified);
      rasqal_graph_pattern_print(query->query_graph_pattern, DEBUG_FH);
      fputs("\n", DEBUG_FH);
#endif

    } while(modified>0);

    rc = modified; /* error if modified<0, success if modified==0 */
    if(rc)
      goto done;

#endif /* !RASQAL_NO_GP_MERGE */

    /* Label all graph patterns with an index 0.. for use in discovering
     * the size of the graph pattern execution data array
     */
    query->graph_pattern_count = 0;

    /* This sequence stores shared pointers to the graph patterns it
     * finds, indexed by the gp_index
     */
    query->graph_patterns_sequence = raptor_new_sequence(NULL, NULL);
    if(!query->graph_patterns_sequence) {
      rc = 1;
      goto done;
    }

    rasqal_query_graph_pattern_visit(query, 
                                     rasqal_query_prepare_count_graph_patterns,
                                     query->graph_patterns_sequence);

    /* create query->variables_use_map that marks where a variable is 
     * mentioned, bound or used in a graph pattern.
     */
    rc = rasqal_query_build_variables_use_map(query); 
    if(rc)
      goto done;

    /* create query->variables_bound_in to find triples where a variable
     * is bound and look for variables selected that are not used
     * in the execution order (graph pattern tree walk order).
     *
     * The query->variables_bound_in array is used in
     * rasqal_engine_graph_pattern_init() when trying to figure out
     * which parts of a triple pattern need to bind to a variable:
     * only the first reference to it.
     */
    rc = rasqal_query_build_bound_in(query);
    if(rc)
      goto done;

    /* warn if any of the selected named variables are not in a triple */
    rc = rasqal_query_check_unused_variables(query, query->variables_bound_in);
    if(rc)
      goto done;

  }


  rc = 0;

  done:
  return rc;
}


/**
 * rasqal_graph_patterns_join:
 * @dest_gp: destination graph pattern
 * @src_gp: src graph pattern
 *
 * INTERNAL - merge @src_gp graph pattern into @dest_gp graph pattern
 *
 * Return value: non-0 on error
 */
int
rasqal_graph_patterns_join(rasqal_graph_pattern *dest_gp,
                           rasqal_graph_pattern *src_gp)
{
  int rc;

  if(!src_gp || !dest_gp)
    return 0;

  if(src_gp->op != dest_gp->op) {
    RASQAL_DEBUG3("Source operator %s != Destination operator %s, ending\n",
                  rasqal_graph_pattern_operator_as_string(src_gp->op),
                  rasqal_graph_pattern_operator_as_string(dest_gp->op));
    return 1;
  }

#if RASQAL_DEBUG > 1
  RASQAL_DEBUG2("Joining graph pattern #%d\n  ", src_gp->gp_index);
  rasqal_graph_pattern_print(src_gp, DEBUG_FH);
  fprintf(DEBUG_FH, "\nto graph pattern #%d\n  ", dest_gp->gp_index);
  rasqal_graph_pattern_print(dest_gp, DEBUG_FH);
  fprintf(DEBUG_FH, "\nboth of operator %s\n",
          rasqal_graph_pattern_operator_as_string(src_gp->op));
#endif
    

  if(src_gp->graph_patterns) {
    if(!dest_gp->graph_patterns) {
      dest_gp->graph_patterns = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_graph_pattern,
                                                    (raptor_sequence_print_handler*)rasqal_graph_pattern_print);
      if(!dest_gp->graph_patterns)
        return -1;
    }

    rc = raptor_sequence_join(dest_gp->graph_patterns, src_gp->graph_patterns);
    if(rc)
      return rc;
  }

  if(src_gp->triples) {
    int start_c = src_gp->start_column;
    int end_c = src_gp->end_column;
    
    /* if this is our first triple, save a free/alloc */
    dest_gp->triples = src_gp->triples;
    src_gp->triples = NULL;
    
    if((dest_gp->start_column < 0) || start_c < dest_gp->start_column)
      dest_gp->start_column = start_c;
    if((dest_gp->end_column < 0) || end_c > dest_gp->end_column)
      dest_gp->end_column = end_c;
    
#if RASQAL_DEBUG > 1
    RASQAL_DEBUG3("Moved triples from columns %d to %d\n", start_c, end_c);
    RASQAL_DEBUG3("Columns now %d to %d\n", dest_gp->start_column, dest_gp->end_column);
#endif
  }

  rc = rasqal_graph_pattern_move_constraints(dest_gp, src_gp);

  if(src_gp->origin) {
    dest_gp->origin = src_gp->origin;
    src_gp->origin = NULL;
  }

#if RASQAL_DEBUG > 1
  RASQAL_DEBUG2("Result graph pattern #%d\n  ", dest_gp->gp_index);
  rasqal_graph_pattern_print(dest_gp, stdout);
  fputs("\n", stdout);
#endif

  return rc;
}


/**
 * rasqal_query_triples_build_variables_use_map_internal:
 * @query: the #rasqal_query to find the variables in
 * @use_map: 1D array of size num. variables to write use_map
 * @start_column: first column in triples array
 * @end_column: last column in triples array
 *
 * INTERNAL - Mark variables mentioned in a sequence of triples
 * 
 **/
static void
rasqal_query_triples_build_variables_use_map_internal(rasqal_query* query,
                                                      short *use_map,
                                                      int start_column,
                                                      int end_column)
{
  int col;
  
  for(col = start_column; col <= end_column; col++) {
    rasqal_triple *t;
    rasqal_variable *v;
    
    t = (rasqal_triple*)raptor_sequence_get_at(query->triples, col);

    if((v = rasqal_literal_as_variable(t->subject)))
      use_map[v->offset] |= RASQAL_VAR_USE_MENTIONED_HERE;
    if((v = rasqal_literal_as_variable(t->predicate)))
      use_map[v->offset] |= RASQAL_VAR_USE_MENTIONED_HERE;
    if((v = rasqal_literal_as_variable(t->object)))
      use_map[v->offset] |= RASQAL_VAR_USE_MENTIONED_HERE;
    if(t->origin) {
      if((v = rasqal_literal_as_variable(t->origin)))
        use_map[v->offset] |= RASQAL_VAR_USE_MENTIONED_HERE;
    }

  }
}


/**
 * rasqal_query_triples_build_variables_use_map:
 * @query: the #rasqal_query to find the variables in
 * @width: width of array to create
 * @start_column: first column in triples array
 * @end_column: last column in triples array
 *
 * INTERNAL - Mark variables mentioned in a sequence of triples
 * 
 * Return value: 1D array of size num. variables with mentioned information
 **/
short*
rasqal_query_triples_build_variables_use_map(rasqal_query* query,
                                             int width,
                                             int start_column,
                                             int end_column)
{
  short *use_map;
  
  use_map = (short*)RASQAL_CALLOC(intarray, width, sizeof(short));
  if(!use_map)
    return NULL;

  rasqal_query_triples_build_variables_use_map_internal(query, use_map,
                                                        start_column,
                                                        end_column);
  return use_map;
}


/**
 * rasqal_query_graph_pattern_build_variables_use_map:
 * @query: the #rasqal_query to find the variables in
 * @use_map: 2D array of (num. variables x num. GPs) to write use_map
 * @width: width of array (num. variables)
 * @gp: graph pattern to use
 *
 * INTERNAL - Mark where variables are first mentioned in a graph_pattern tree walk
 * 
 **/
static int
rasqal_query_graph_pattern_build_variables_use_map(rasqal_query* query,
                                                   short *use_map,
                                                   int width,
                                                   rasqal_graph_pattern *gp)
{
  int offset;

  if(gp->graph_patterns) {
    int i;

    for(i = 0; i < raptor_sequence_size(gp->graph_patterns); i++) {
      rasqal_graph_pattern *sgp;
      sgp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      if(rasqal_query_graph_pattern_build_variables_use_map(query, use_map,
                                                            width, sgp))
        return 1;
    }
  }


  /* write to the 1D array for this GP */
  offset = (gp->gp_index + RASQAL_VAR_USE_MAP_OFFSET_LAST + 1) * width;
  switch(gp->op) {
    case RASQAL_GRAPH_PATTERN_OPERATOR_BASIC:
      rasqal_query_triples_build_variables_use_map_internal(query, 
                                                            &use_map[offset],
                                                            gp->start_column,
                                                            gp->end_column);
      break;

    case RASQAL_GRAPH_PATTERN_OPERATOR_GRAPH:
      rasqal_query_graph_build_variables_use_map_in_internal(query, 
                                                             &use_map[offset],
                                                             gp->origin);
      break;
      
    case RASQAL_GRAPH_PATTERN_OPERATOR_FILTER:
      rasqal_query_filter_build_variables_use_map_in_internal(query, 
                                                              &use_map[offset],
                                                              gp->filter_expression);
      break;

    case RASQAL_GRAPH_PATTERN_OPERATOR_LET:
      rasqal_query_let_build_variables_use_map_in_internal(query, 
                                                           &use_map[offset],
                                                           gp->var,
                                                           gp->filter_expression);
      break;
      
    case RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL:
    case RASQAL_GRAPH_PATTERN_OPERATOR_UNION:
    case RASQAL_GRAPH_PATTERN_OPERATOR_GROUP:
    case RASQAL_GRAPH_PATTERN_OPERATOR_UNKNOWN:
      break;
  }
  
  return 0;
}


#ifdef RASQAL_DEBUG
static const char* const use_map_offset_labels[RASQAL_VAR_USE_MAP_OFFSET_LAST + 1] = {
  "Verbs",
  "GROUP BY",
  "HAVING",
  "ORDER BY"
};


#define N_MAP_FLAGS_LABELS 8
static const char* const use_map_flags_labels[N_MAP_FLAGS_LABELS + 1] = {
  "   ",
  "  I",
  " M ",
  "  I",
  "B  ",
  "B I",
  "BM ",
  "B I",
  "???"
};


static void
rasqal_query_print_variables_use_map(FILE* fh, rasqal_query* query)
{
  int width;
  int height;
  int i;
  int row_index;
  
  width = rasqal_variables_table_get_total_variables_count(query->vars_table);
  height = (RASQAL_VAR_USE_MAP_OFFSET_LAST + 1) + query->graph_pattern_count;
  
  fprintf(fh, "Query variables-use map (B=bound, M=mentioned, U=used):\n");
  fputs("GP#  Type      ", fh);
  for(i = 0; i < width; i++) {
    rasqal_variable* v = rasqal_variables_table_get(query->vars_table, i);
    fprintf(fh, "%-10s ", v->name);
  }
  fputc('\n', fh);

  for(row_index = 0; row_index < height; row_index++) {
    short *row = &query->variables_use_map[row_index * width];
    int gp_index = row_index - (RASQAL_VAR_USE_MAP_OFFSET_LAST + 1);

    if(gp_index < 0)
      fprintf(fh, "--   %-8s ", use_map_offset_labels[row_index]);
    else {
      rasqal_graph_pattern* gp;
      gp = (rasqal_graph_pattern*)raptor_sequence_get_at(query->graph_patterns_sequence, gp_index);
      fprintf(fh, "%-2d   %-8s ", gp_index, 
              rasqal_graph_pattern_operator_as_string(gp->op));
    }
    
    for(i = 0; i < width; i++) {
      int flag_index = row[i];
      /* Turn unknown flags into "???" */
      if(flag_index > N_MAP_FLAGS_LABELS - 1)
        flag_index = N_MAP_FLAGS_LABELS;
      fprintf(fh, "%-10s ", use_map_flags_labels[flag_index]);
    }
    fputc('\n', fh);
  }
}
#endif


/* for use with rasqal_expression_visit and user_data=rasqal_query */
static int
rasqal_expression_expr_build_variables_use_map(short *use_map,
                                               rasqal_expression *e)
{
  if(e->literal) {
    rasqal_variable* v;
    v = rasqal_literal_as_variable(e->literal);
    if(v) 
      use_map[v->offset] |= RASQAL_VAR_USE_MENTIONED_HERE;
  }

  return 0;
}


/*
 * Mark variables seen in a sequence of variables (with optional expression)
 */
static int
rasqal_query_build_variables_sequence_use_map(rasqal_query *query,
                                              short* use_map,
                                              raptor_sequence *vars_seq)
{
  int rc = 0;
  int idx;

  for(idx = 0; 1; idx++) {
    rasqal_variable* v;
    rasqal_expression *e;
    int flags = RASQAL_VAR_USE_MENTIONED_HERE;

    v = (rasqal_variable*)raptor_sequence_get_at(vars_seq, idx);
    if(!v)
      break;

    e = v->expression;
    if(e) {
      rc = rasqal_expression_expr_build_variables_use_map(use_map,
                                                          v->expression);
      if(rc)
        break;

      flags |= RASQAL_VAR_USE_BOUND_HERE;
    }

    use_map[v->offset] |= flags;
  }

  return rc;
}
  

/**
 * rasqal_query_build_variables_use_map:
 * @query: the #rasqal_query to find the variables in
 *
 * INTERNAL - Record where variables are mentioned in query GPs
 *
 * Constructs a 2D array of width num. variables x height num. graph patterns
 * indicating that a variable is mentioned in a GP.  That is, GP index 0
 * is the first <number of variables> in the result.
 *
 * Return value: non-0 on failure
 **/
static int
rasqal_query_build_variables_use_map(rasqal_query* query)
{
  int width;
  int height;
  int rc = 0;
  short *use_map;
  
  width = rasqal_variables_table_get_total_variables_count(query->vars_table);
  height = RASQAL_VAR_USE_MAP_OFFSET_LAST + 1 + query->graph_pattern_count;
  
  /* Need to walk query components that may mention or bind variables
   * and record their variable use:
   *
   * 1) Query verbs: ASK SELECT CONSTRUCT DESCRIBE (SPARQL 1.0)
   *   1a) SELECT project-expressions (SPARQL 1.1)
   * 2) GROUP BY expr/var (SPARQL 1.1 TBD)
   * 3) HAVING expr (SPARQL 1.1 TBD)
   * 4) ORDER list-of-expr (SPARQL 1.0)
   *
   * But they are not GPs so they are recorded at offsets into the use_map
   * defined by the #rasqal_var_use_map_offset enum such as 
   * #RASQAL_VAR_USE_MAP_OFFSET_VERBS for the query verbs - item 1 above.
   */
  
  use_map = (short*)RASQAL_CALLOC(intarray,  width * height, sizeof(short));
  if(!use_map)
    return 1;

  query->variables_use_map = use_map;

  /* record variable use for 1) Query verbs */
  switch(query->verb) {
    case RASQAL_QUERY_VERB_SELECT:
      /* This also handles 1a) select/project expressions */
      rasqal_query_build_variables_sequence_use_map(query,
                                                    &use_map[RASQAL_VAR_USE_MAP_OFFSET_VERBS],
                                                    query->selects);
      break;
  
    case RASQAL_QUERY_VERB_DESCRIBE:
      rasqal_query_build_variables_sequence_use_map(query,
                                                    &use_map[RASQAL_VAR_USE_MAP_OFFSET_VERBS],
                                                    query->describes);
      break;
      
    case RASQAL_QUERY_VERB_CONSTRUCT:
    case RASQAL_QUERY_VERB_DELETE:
    case RASQAL_QUERY_VERB_INSERT:
      /* FIXME - should mark these triple patterns as using vars */
      break;
      
    case RASQAL_QUERY_VERB_UNKNOWN:
    case RASQAL_QUERY_VERB_ASK:
    default:
      break;
  }

  /* FIXME: record variable use for 2) GROUP BY expr/var (SPARQL 1.1 TBD) */

  /* FIXME: record variable use for 3) HAVING expr (SPARQL 1.1 TBD) */

  /* FIXME: record variable use for 4) ORDER list-of-expr (SPARQL 1.0) */


  /* record variable use for graph patterns */
  rc = rasqal_query_graph_pattern_build_variables_use_map(query,
                                                          use_map,
                                                          width,
                                                          query->query_graph_pattern);

#ifdef RASQAL_DEBUG
  if(!rc)
    rasqal_query_print_variables_use_map(stderr, query);
#endif    
  
  return rc;
}


/**
 * rasqal_query_graph_build_variables_use_map_in_internal:
 * @query: the #rasqal_query to find the variables in
 * @use_map: 1D array of size num. variables to write use_map
 *
 * INTERNAL - Mark variables mentioned in a GRAPH graph pattern
 * 
 **/
static void
rasqal_query_graph_build_variables_use_map_in_internal(rasqal_query* query,
                                                       short *use_map,
                                                       rasqal_literal *origin)
{
  rasqal_variable *v;
  
  if((v = rasqal_literal_as_variable(origin)))
    use_map[v->offset] |= RASQAL_VAR_USE_MENTIONED_HERE;
}



/**
 * rasqal_query_filter_build_variables_use_map_in_internal:
 * @query: the #rasqal_query to find the variables in
 * @use_map: 1D array of size num. variables to write use_map
 * @e: filter expression to use
 *
 * INTERNAL - Mark variables mentioned in a FILTER graph pattern
 * 
 **/
static void
rasqal_query_filter_build_variables_use_map_in_internal(rasqal_query* query,
                                                        short *use_map,
                                                        rasqal_expression* e)
{
  rasqal_expression_visit(e, 
                          (rasqal_expression_visit_fn)rasqal_expression_expr_build_variables_use_map,
                          use_map);
}


/**
 * rasqal_query_let_build_variables_use_map_in_internal:
 * @query: the #rasqal_query to find the variables in
 * @use_map: 1D array of size num. variables to write use_map
 * @e: let expression to use
 *
 * INTERNAL - Mark variables mentioned in a LET graph pattern
 * 
 **/
static void
rasqal_query_let_build_variables_use_map_in_internal(rasqal_query* query,
                                                     short *use_map,
                                                     rasqal_variable *var,
                                                     rasqal_expression* e)
{
  use_map[var->offset] |= RASQAL_VAR_USE_BOUND_HERE | 
                          RASQAL_VAR_USE_MENTIONED_HERE;

  rasqal_expression_visit(e, 
                          (rasqal_expression_visit_fn)rasqal_expression_expr_build_variables_use_map,
                          use_map);
}


/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_query_transform.c - Rasqal query transformations
 *
 * Copyright (C) 2004-2011, David Beckett http://www.dajobe.org/
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
static int rasqal_query_build_variables_use_map(rasqal_query* query, rasqal_projection* projection);
static int rasqal_query_graph_build_variables_use_map_binds(rasqal_graph_pattern* gp, unsigned short* vars_scope);
static void rasqal_query_expression_build_variables_use_map(unsigned short *use_map, rasqal_expression* e);
static void rasqal_query_let_build_variables_use_map(rasqal_query* query, unsigned short *use_map, rasqal_expression* e);
static int rasqal_query_let_build_variables_use_map_binds(rasqal_graph_pattern* gp, unsigned short* vars_scope);
static int rasqal_query_select_build_variables_use_map(rasqal_query* query, unsigned short *use_map, int width, rasqal_graph_pattern* gp);
static int rasqal_query_select_build_variables_use_map_binds(rasqal_query* query, unsigned short *use_map, int width, rasqal_graph_pattern* gp, unsigned short* vars_scope);
static int rasqal_query_union_build_variables_use_map_binds(rasqal_query* query, unsigned short *use_map, int width, rasqal_graph_pattern* gp, unsigned short* vars_scope);
static int rasqal_query_values_build_variables_use_map_binds(rasqal_query* query, unsigned short *use_map, int width, rasqal_graph_pattern* gp, unsigned short* vars_scope);


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
  
  v = rasqal_variables_table_add2(rq->vars_table,
                                  RASQAL_VARIABLE_TYPE_ANONYMOUS,
                                  RASQAL_GOOD_CAST(unsigned char*, l->string),
                                  l->string_len,
                                  NULL);
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
 * INTERNAL - expand SPARQL SELECT * to a full list of select variables
 *
 * Return value: non-0 on failure
 */
int
rasqal_query_expand_wildcards(rasqal_query* rq, rasqal_projection* projection)
{
  int i;
  int size;

  if(rq->verb != RASQAL_QUERY_VERB_SELECT || 
     !projection || !projection->wildcard)
    return 0;
  
  /* If 'SELECT *' was given, make the selects be a list of all variables */
  size = rasqal_variables_table_get_named_variables_count(rq->vars_table);
  for(i = 0; i < size; i++) {
    rasqal_variable* v = rasqal_variables_table_get(rq->vars_table, i);

    rasqal_query_add_variable(rq, v);
  }

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
rasqal_query_remove_duplicate_select_vars(rasqal_query* rq,
                                          rasqal_projection* projection)
{
  int i;
  int modified = 0;
  int size;
  raptor_sequence* seq;
  raptor_sequence* new_seq;

  if(!projection)
    return 1;

  seq = projection->variables;
  if(!seq)
    return 0;

  size = raptor_sequence_size(seq);
  if(!size)
    return 0;

  new_seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_variable,
                                (raptor_data_print_handler)rasqal_variable_print);
  if(!new_seq)
    return 1;
  
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG1("bound variables before deduping: "); 
  raptor_sequence_print(seq, DEBUG_FH);
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
          rasqal_log_warning_simple(rq->world,
                                    RASQAL_WARNING_LEVEL_DUPLICATE_VARIABLE,
                                    &rq->locator,
                                    "Variable %s duplicated in SELECT.", 
                                    v->name);
          warned = 1;
        }
      }
    }
    if(!warned) {
      v = rasqal_new_variable_from_variable(v);
      raptor_sequence_push(new_seq, v);
      modified = 1;
    }
  }
  
  if(modified) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("bound variables after deduping: "); 
    raptor_sequence_print(new_seq, DEBUG_FH);
    fputs("\n", DEBUG_FH); 
#endif
    raptor_free_sequence(projection->variables);
    projection->variables = new_seq;
  } else
    raptor_free_sequence(new_seq);

  return 0;
}


/**
 * rasqal_query_build_variable_agg_use:
 * @query: the query 
 *
 * INTERNAL - calculate the usage of variables across all parts of the query
 *
 * Return value: array of variable usage info or NULL on failure
 */
static unsigned short*
rasqal_query_build_variable_agg_use(rasqal_query* query)
{
  int width;
  int height;
  unsigned short* agg_row;
  int row_index;
  
  width = rasqal_variables_table_get_total_variables_count(query->vars_table);
  height = RASQAL_VAR_USE_MAP_OFFSET_LAST + 1 + query->graph_pattern_count;

  agg_row = RASQAL_CALLOC(unsigned short*, RASQAL_GOOD_CAST(size_t, width), sizeof(unsigned short));
  if(!agg_row)
    return NULL;

  for(row_index = 0; row_index < height; row_index++) {
    unsigned short *row;
    int i;

    row = &query->variables_use_map[row_index * width];

    for(i = 0; i < width; i++)
      agg_row[i] |= row[i];
  }
  
  return agg_row;
}



/**
 * rasqal_query_check_unused_variables:
 * @query: the #rasqal_query to check
 *
 * INTERNAL - warn variables that are selected but not bound in a triple
 *
 * FIXME: Fails to handle variables bound in LET
 *
 * Return value: non-0 on failure
 */
static int
rasqal_query_check_unused_variables(rasqal_query* query)
{
  int i;
  int size;

  /* check only for named variables since only they can
   * appear in SELECT $vars 
   */
  size = rasqal_variables_table_get_named_variables_count(query->vars_table);
  for(i = 0; i < size; i++) {
    rasqal_variable *v;

    v = rasqal_variables_table_get(query->vars_table, i);

    if(!rasqal_query_variable_is_bound(query, v)) {
      rasqal_log_warning_simple(query->world,
                                RASQAL_WARNING_LEVEL_UNUSED_SELECTED_VARIABLE,
                                &query->locator,
                                "Variable %s was selected but is unused in the query", 
                                v->name);
    }
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

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  printf("rasqal_query_merge_triple_patterns: Checking graph pattern #%d:\n  ", gp->gp_index);
  rasqal_graph_pattern_print(gp, stdout);
  fputs("\n", stdout);
  RASQAL_DEBUG3("Columns %d to %d\n", gp->start_column, gp->end_column);
#endif
    
  if(!gp->graph_patterns) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG2("Ending graph patterns %d - no sub-graph patterns\n", gp->gp_index);
#endif
    return 0;
  }

  if(gp->op != RASQAL_GRAPH_PATTERN_OPERATOR_GROUP) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
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


  #if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG3("Found sequence of %d basic sub-graph patterns in %d\n", bgp_count, gp->gp_index);
  #endif
    if(bgp_count < 2)
      continue;

  #if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG3("OK to merge %d basic sub-graph patterns of %d\n", bgp_count, gp->gp_index);

    RASQAL_DEBUG3("Initial columns %d to %d\n", gp->start_column, gp->end_column);
  #endif

    seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_graph_pattern, (raptor_data_print_handler)rasqal_graph_pattern_print);
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
  

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
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

  if(gp->op != RASQAL_GRAPH_PATTERN_OPERATOR_GROUP)
    return 0;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
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
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG2("Ending graph patterns %d - saw no empty groups\n", gp->gp_index);
#endif
    return 0;
  }
  
  seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_graph_pattern, (raptor_data_print_handler)rasqal_graph_pattern_print);
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
  
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
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
  
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  printf("rasqal_query_merge_graph_patterns: Checking graph pattern #%d:\n  ",
         gp->gp_index);
  rasqal_graph_pattern_print(gp, stdout);
  fputs("\n", stdout);
  RASQAL_DEBUG3("Columns %d to %d\n", gp->start_column, gp->end_column);
#endif

  if(!gp->graph_patterns) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG3("Ending graph pattern #%d - operator %s: no sub-graph patterns\n", gp->gp_index,
                  rasqal_graph_pattern_operator_as_string(gp->op));
#endif
    return 0;
  }

  if(gp->op != RASQAL_GRAPH_PATTERN_OPERATOR_GROUP) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG3("Ending graph patterns %d - operator %s: not GROUP\n", gp->gp_index,
                  rasqal_graph_pattern_operator_as_string(gp->op));
#endif
    return 0;
  }

  size = raptor_sequence_size(gp->graph_patterns);
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
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
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
        RASQAL_DEBUG4("Sub-graph pattern #%d is %s different from first %s, cannot merge\n", 
                      i, rasqal_graph_pattern_operator_as_string(sgp->op), 
                      rasqal_graph_pattern_operator_as_string(op));
#endif
        all_gp_op_same = 0;
      }
    }
  }

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG2("Sub-graph patterns of %d done\n", gp->gp_index);
#endif
  
  if(!all_gp_op_same) {
    merge_gp_ok = 0;
    goto merge_check_done;
  }

  if(size == 1) {
    /* Never merge a FILTER to an outer GROUP since you lose
     * knowledge about variable scope
     */
    merge_gp_ok = (op != RASQAL_GRAPH_PATTERN_OPERATOR_FILTER);
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
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      RASQAL_DEBUG3("Found %s sub-graph pattern #%d\n",
                    rasqal_graph_pattern_operator_as_string(sgp->op), 
                    sgp->gp_index);
#endif
      merge_gp_ok = 0;
      break;
    }
    
    /* not ok if there are >1 triples */
    if(sgp->triples && (sgp->end_column-sgp->start_column+1) > 1) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      RASQAL_DEBUG2("Found >1 triples in sub-graph pattern #%d\n", sgp->gp_index);
#endif
      merge_gp_ok = 0;
      break;
    }
    
    /* not ok if there are triples and constraints */
    if(sgp->triples && sgp->filter_expression) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
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
    
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
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
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG2("NOT OK to merge sub-graph patterns of %d\n", gp->gp_index);
#endif
  }

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  if(merge_gp_ok) {
    RASQAL_DEBUG2("Ending graph pattern #%d\n  ", gp->gp_index);
    rasqal_graph_pattern_print(gp, stdout);
    fputs("\n\n", stdout);
  }
#endif

  return 0;
}


/**
 * rasqal_query_filter_variable_scope:
 * @query: query
 * @gp: current graph pattern
 * @data: pointer to int modified flag
 *
 * Replace a FILTER in a GROUP refering to out-of-scope var with FALSE
 *
 * For each variable in a FILTER expression, if there is one defined
 * outside the GROUP, the FILTER is always FALSE - so set it thus and
 * the tree of GP is modified
 *
 * Return value: 0
 */
static int
rasqal_query_filter_variable_scope(rasqal_query* query,
                                   rasqal_graph_pattern* gp,
                                   void* data)
{
  int vari;
  int* modified = (int*)data;
  rasqal_graph_pattern *qgp;
  int size;
  
  /* Scan up from FILTER GPs */
  if(gp->op != RASQAL_GRAPH_PATTERN_OPERATOR_FILTER)
    return 0;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG2("Checking FILTER graph pattern #%d:\n  ", gp->gp_index);
  rasqal_graph_pattern_print(gp, stderr);
  fputs("\n", stderr);
#endif

  qgp = rasqal_query_get_query_graph_pattern(query);

  size = rasqal_variables_table_get_named_variables_count(query->vars_table);

  for(vari = 0; vari < size; vari++) { 
    rasqal_variable* v = rasqal_variables_table_get(query->vars_table, vari);
    int var_in_scope = 2;
    rasqal_graph_pattern *sgp;

    if(!rasqal_expression_mentions_variable(gp->filter_expression, v))
      continue;
    RASQAL_DEBUG3("FILTER GP #%d expression mentions %s\n",
                  gp->gp_index, v->name);
    
    sgp = gp;
    while(1) {
      int bound_here;

      sgp = rasqal_graph_pattern_get_parent(query, sgp, qgp);
      if(!sgp)
        break;

      bound_here = rasqal_graph_pattern_variable_bound_below(sgp, v);
      RASQAL_DEBUG4("Checking parent GP #%d op %s - bound below: %d\n",
                    sgp->gp_index,
                    rasqal_graph_pattern_operator_as_string(sgp->op),
                    bound_here);

      if(sgp->op == RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL) {
        /* Collapse OPTIONAL { GROUP } */
        var_in_scope++;
      }
      
      if(sgp->op == RASQAL_GRAPH_PATTERN_OPERATOR_GROUP) {
        var_in_scope--;
        if(bound_here) {
          /* It was defined in first GROUP so life is good - done */
          if(var_in_scope == 1)
            break;
        
          /* It was defined in an outer GROUP so this is bad */
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
          RASQAL_DEBUG3("FILTER Variable %s defined in GROUP GP #%d and now out of scope\n", v->name, sgp->gp_index);
#endif
          var_in_scope = 0;
          break;
        }
      }
    }
    
    if(!var_in_scope) {
      rasqal_literal* l;
      
      l = rasqal_new_boolean_literal(query->world, 0);
      /* In-situ conversion of filter_expression to a literal expression */
      rasqal_expression_convert_to_literal(gp->filter_expression, l);
      *modified = 1;

      RASQAL_DEBUG2("FILTER Variable %s was defined outside FILTER's parent group\n", v->name);
      break;
    }
    
  }
  
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
  int error = 0;
  
  /* skip if already a  literal or this expression tree is not constant */
  if(e->op == RASQAL_EXPR_LITERAL || !rasqal_expression_is_constant(e))
    return 0;
  
#ifdef RASQAL_DEBUG
  RASQAL_DEBUG2("folding expression %p: ", e);
  rasqal_expression_print(e, DEBUG_FH);
  fprintf(DEBUG_FH, "\n");
#endif
  
  query = st->query;
  l = rasqal_expression_evaluate2(e, query->eval_context, &error);
  if(error) {
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
  raptor_sequence *order_seq = rasqal_query_get_order_conditions_sequence(rq);

  if(gp)
    rasqal_graph_pattern_fold_expressions(rq, gp);

  if(!order_seq)
    return 0;
  
  order_size = raptor_sequence_size(order_seq);
  if(order_size) {
    int i;
    
    for(i = 0; i < order_size; i++) {
      rasqal_expression* e;

      e = (rasqal_expression*)raptor_sequence_get_at(order_seq, i);
      rasqal_query_expression_fold(rq, e);
    }
  }

  return 0;
}


static int
rasqal_query_prepare_count_graph_pattern(rasqal_query* query,
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
 * rasqal_query_enumerate_graph_patterns:
 * @query: query object
 *
 * INTERNAL - Label all graph patterns in query graph patterns with an index 0.. 
 *
 * Used for the size of the graph pattern execution data array.
 * Used to allocate in rasqal_query_build_variables_use_map() 
 * and rasqal_query_build_variable_agg_use() and used in
 * rasqal_query_print_variables_use_map() and
 * rasqal_query_variable_is_bound().
 *
 * Return value: non-0 on failure
 */
static int
rasqal_query_enumerate_graph_patterns(rasqal_query *query)
{
  query->graph_pattern_count = 0;
  
  if(query->graph_patterns_sequence)
    raptor_free_sequence(query->graph_patterns_sequence);

  /* This sequence stores shared pointers to the graph patterns it
   * finds, indexed by the gp_index
   */
  query->graph_patterns_sequence = raptor_new_sequence(NULL, NULL);
  if(!query->graph_patterns_sequence)
    return 1;
  
  return rasqal_query_graph_pattern_visit2(query, 
                                           rasqal_query_prepare_count_graph_pattern,
                                           query->graph_patterns_sequence);
}


/**
 * rasqal_query_build_variables_use:
 * @query: query
 *
 * INTERNAL - build structures recording variable use in the query
 *
 * Should be called if the query variable usage is modified such as
 * a variable added during query planning.
 *
 * Return value: non-0 on failure
 */
int
rasqal_query_build_variables_use(rasqal_query* query,
                                 rasqal_projection* projection)
{
  int rc;
  
  /* create query->variables_use_map that marks where a variable is 
   * mentioned, bound or used in a graph pattern.
   */
  rc = rasqal_query_build_variables_use_map(query, projection);
  if(rc)
    return rc;
  
  if(1) {
    unsigned short* agg_row;
    int i;
    int errors = 0;
    
    agg_row = rasqal_query_build_variable_agg_use(query);
    if(!agg_row)
      return 1;

    for(i = 0; 1; i++) {
      rasqal_variable* v = rasqal_variables_table_get(query->vars_table, i);
      if(!v)
        break;

      if( (agg_row[i] & RASQAL_VAR_USE_BOUND_HERE) && 
         !(agg_row[i] & RASQAL_VAR_USE_MENTIONED_HERE)) {
          rasqal_log_warning_simple(query->world,
                                    RASQAL_WARNING_LEVEL_VARIABLE_UNUSED,
                                    &query->locator,
                                    "Variable %s was bound but is unused in the query", 
                                v->name);
      } else if(!(agg_row[i] & RASQAL_VAR_USE_BOUND_HERE) && 
                 (agg_row[i] & RASQAL_VAR_USE_MENTIONED_HERE)) {
        rasqal_log_warning_simple(query->world,
                                  RASQAL_WARNING_LEVEL_SELECTED_NEVER_BOUND,
                                  &query->locator,
                                  "Variable %s was used but is not bound in the query",
                                  v->name);
      } else if(!(agg_row[i] & RASQAL_VAR_USE_BOUND_HERE) &&
                !(agg_row[i] & RASQAL_VAR_USE_MENTIONED_HERE)) {
        rasqal_log_error_simple(query->world,
                                RAPTOR_LOG_LEVEL_ERROR,
                                &query->locator,
                                "Variable %s was not bound and not used in the query (where is it from?)", 
                                v->name);
        errors++;
      }
    }

    RASQAL_FREE(shortarray, agg_row);

    if(errors)
      return 1;
  }
  

  
  return rc;
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
  rasqal_projection* projection;

  if(!query->triples)
    goto done;
  
  /* turn SELECT $a, $a into SELECT $a - editing the projection */
  projection = rasqal_query_get_projection(query);
  if(projection) {
    if(rasqal_query_remove_duplicate_select_vars(query, projection))
      goto done;
  }

  rasqal_query_fold_expressions(query);

  if(query->query_graph_pattern) {
    /* This query prepare processing requires a query graph pattern.
     * Not the case for a legal query like 'DESCRIBE <uri>'
     */

    int modified;
    
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    fputs("Initial query graph pattern:\n  ", DEBUG_FH);
    rasqal_graph_pattern_print(query->query_graph_pattern, DEBUG_FH);
    fputs("\n", DEBUG_FH);
#endif

    do {
      modified = 0;
      
      rc = rasqal_query_graph_pattern_visit2(query, 
                                             rasqal_query_merge_triple_patterns,
                                             &modified);
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      fprintf(DEBUG_FH, "modified=%d after merge triples, query graph pattern now:\n  ", modified);
      rasqal_graph_pattern_print(query->query_graph_pattern, DEBUG_FH);
      fputs("\n", DEBUG_FH);
#endif
      if(rc) {
        modified = rc;
        break;
      }
      
      rc = rasqal_query_graph_pattern_visit2(query,
                                             rasqal_query_remove_empty_group_graph_patterns,
                                             &modified);
      
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      fprintf(DEBUG_FH, "modified=%d after remove empty groups, query graph pattern now:\n  ", modified);
      rasqal_graph_pattern_print(query->query_graph_pattern, DEBUG_FH);
      fputs("\n", DEBUG_FH);
#endif
      if(rc) {
        modified = rc;
        break;
      }
      
      rc = rasqal_query_graph_pattern_visit2(query, 
                                             rasqal_query_merge_graph_patterns,
                                             &modified);

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      fprintf(DEBUG_FH, "modified=%d  after merge graph patterns, query graph pattern now:\n  ", modified);
      rasqal_graph_pattern_print(query->query_graph_pattern, DEBUG_FH);
      fputs("\n", DEBUG_FH);
#endif
      if(rc) {
        modified = rc;
        break;
      }
      
    } while(modified > 0);

    rc = modified; /* error if modified<0, success if modified==0 */
    if(rc)
      goto done;

    rc = rasqal_query_enumerate_graph_patterns(query);
    if(rc)
      goto done;

    rc = rasqal_query_build_variables_use(query, projection);
    if(rc)
      goto done;

    /* Turn FILTERs that refer to out-of-scope variables into FALSE */
    (void)rasqal_query_graph_pattern_visit2(query,
                                            rasqal_query_filter_variable_scope,
                                            &modified);
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    fprintf(DEBUG_FH, "modified=%d  after filter variable scope, query graph pattern now:\n  ", modified);
    rasqal_graph_pattern_print(query->query_graph_pattern, DEBUG_FH);
    fputs("\n", DEBUG_FH);
#endif

    /* warn if any of the selected named variables are not in a triple */
    rc = rasqal_query_check_unused_variables(query);
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

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG2("Joining graph pattern #%d\n  ", src_gp->gp_index);
  rasqal_graph_pattern_print(src_gp, DEBUG_FH);
  fprintf(DEBUG_FH, "\nto graph pattern #%d\n  ", dest_gp->gp_index);
  rasqal_graph_pattern_print(dest_gp, DEBUG_FH);
  fprintf(DEBUG_FH, "\nboth of operator %s\n",
          rasqal_graph_pattern_operator_as_string(src_gp->op));
#endif
    

  if(src_gp->graph_patterns) {
    if(!dest_gp->graph_patterns) {
      dest_gp->graph_patterns = raptor_new_sequence((raptor_data_free_handler)rasqal_free_graph_pattern,
                                                    (raptor_data_print_handler)rasqal_graph_pattern_print);
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
    
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG3("Moved triples from columns %d to %d\n", start_c, end_c);
    RASQAL_DEBUG3("Columns now %d to %d\n", dest_gp->start_column, dest_gp->end_column);
#endif
  }

  rc = rasqal_graph_pattern_move_constraints(dest_gp, src_gp);

  if(src_gp->origin) {
    dest_gp->origin = src_gp->origin;
    src_gp->origin = NULL;
  }

  if(src_gp->var) {
    dest_gp->var = src_gp->var;
    src_gp->var = NULL;
  }

  if(src_gp->projection) {
    dest_gp->projection = src_gp->projection;
    src_gp->projection = NULL;
  }

  if(src_gp->modifier) {
    dest_gp->modifier = src_gp->modifier;
    src_gp->modifier = NULL;
  }

  if(src_gp->bindings) {
    dest_gp->bindings = src_gp->bindings;
    src_gp->bindings = NULL;
  }

  dest_gp->silent = src_gp->silent;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG2("Result graph pattern #%d\n  ", dest_gp->gp_index);
  rasqal_graph_pattern_print(dest_gp, stdout);
  fputs("\n", stdout);
#endif

  return rc;
}


/**
 * rasqal_query_triples_build_variables_use_map_row:
 * @triples: triples sequence to use
 * @use_map_row: 1D array of size num. variables to write
 * @start_column: first column in triples array
 * @end_column: last column in triples array
 *
 * INTERNAL - Mark variables mentioned in a sequence of triples
 * 
 **/
static int
rasqal_query_triples_build_variables_use_map_row(raptor_sequence *triples,
                                                 unsigned short *use_map_row,
                                                 int start_column,
                                                 int end_column)
{
  int rc = 0;
  int col;
  
  for(col = start_column; col <= end_column; col++) {
    rasqal_triple *t;
    rasqal_variable *v;
    
    t = (rasqal_triple*)raptor_sequence_get_at(triples, col);

    if((v = rasqal_literal_as_variable(t->subject))) {
      use_map_row[v->offset] |= RASQAL_VAR_USE_MENTIONED_HERE;
    }
    
    if((v = rasqal_literal_as_variable(t->predicate))) {
      use_map_row[v->offset] |= RASQAL_VAR_USE_MENTIONED_HERE;
    }

    if((v = rasqal_literal_as_variable(t->object))) {
      use_map_row[v->offset] |= RASQAL_VAR_USE_MENTIONED_HERE;
    }

    if(t->origin) {
      if((v = rasqal_literal_as_variable(t->origin))) {
        use_map_row[v->offset] |= RASQAL_VAR_USE_MENTIONED_HERE;
      }
    }

  }

  return rc;
}


/**
 * rasqal_query_graph_build_variables_use_map:
 * @use_map_row: 1D array of size num. variables to write
 * @literal: graph origin literal
 *
 * INTERNAL - Mark variables mentioned in a GRAPH graph pattern
 * 
 **/
static int
rasqal_query_graph_build_variables_use_map(unsigned short *use_map_row,
                                           rasqal_literal *origin)
{
  rasqal_variable* v = rasqal_literal_as_variable(origin);

  if(v)
    use_map_row[v->offset] |= RASQAL_VAR_USE_MENTIONED_HERE;

  return 0;
}



/**
 * rasqal_query_graph_pattern_build_variables_use_map:
 * @query: the #rasqal_query to find the variables in
 * @use_map: 2D array of (num. variables x num. GPs) to write
 * @width: width of array (num. variables)
 * @gp: graph pattern to use
 *
 * INTERNAL - Mark where variables are used (mentioned) in a graph_pattern tree walk
 * 
 **/
static int
rasqal_query_graph_pattern_build_variables_use_map(rasqal_query* query,
                                                   unsigned short *use_map,
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
      /* BGP (part 1) - everything is a mention */
      rasqal_query_triples_build_variables_use_map_row(query->triples, 
                                                       &use_map[offset],
                                                       gp->start_column,
                                                       gp->end_column);
      break;

    case RASQAL_GRAPH_PATTERN_OPERATOR_GRAPH:
      /* Mentions the graph variable */
      rasqal_query_graph_build_variables_use_map(&use_map[offset],
                                                 gp->origin);
      break;
      
    case RASQAL_GRAPH_PATTERN_OPERATOR_FILTER:
      /* Only mentions */
      rasqal_query_expression_build_variables_use_map(&use_map[offset],
                                                      gp->filter_expression);
      break;

    case RASQAL_GRAPH_PATTERN_OPERATOR_LET:
      /* Mentions in expression */
      rasqal_query_let_build_variables_use_map(query, &use_map[offset],
                                               gp->filter_expression);
      break;
      
    case RASQAL_GRAPH_PATTERN_OPERATOR_SELECT:
      rasqal_query_select_build_variables_use_map(query, &use_map[offset],
                                                  width, gp);
      break;
      
    case RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL:
    case RASQAL_GRAPH_PATTERN_OPERATOR_UNION:
    case RASQAL_GRAPH_PATTERN_OPERATOR_GROUP:
    case RASQAL_GRAPH_PATTERN_OPERATOR_SERVICE:
    case RASQAL_GRAPH_PATTERN_OPERATOR_MINUS:
    case RASQAL_GRAPH_PATTERN_OPERATOR_VALUES:
    case RASQAL_GRAPH_PATTERN_OPERATOR_UNKNOWN:
      break;
  }
  
  return 0;
}


/**
 * rasqal_graph_pattern_mentions_variable:
 * @gp: graph pattern
 * @v: variable
 *
 * INTERNAL - test if a variable is bound in a graph pattern directly
 *
 * Return value: non-0 if variable is bound in the given graph pattern
 */
static int
rasqal_graph_pattern_mentions_variable(rasqal_graph_pattern* gp,
                                       rasqal_variable* v)
{
  rasqal_query* query = gp->query;
  int width;
  int gp_offset;
  unsigned short *row;
  
  width = rasqal_variables_table_get_total_variables_count(query->vars_table);
  gp_offset = (gp->gp_index + RASQAL_VAR_USE_MAP_OFFSET_LAST + 1) * width;
  row = &query->variables_use_map[gp_offset];

  return (row[v->offset] & RASQAL_VAR_USE_MENTIONED_HERE);
}


/**
 * rasqal_graph_pattern_tree_mentions_variable:
 * @query: query
 * @gp: graph pattern
 * @v: variable
 *
 * INTERNAL - test if a variable is mentioned in a graph pattern tree
 *
 * Return value: non-0 if variable is mentioned in GP tree
 */
static int
rasqal_graph_pattern_tree_mentions_variable(rasqal_graph_pattern* gp,
                                            rasqal_variable* v)
{
  if(gp->graph_patterns) {
    int size = raptor_sequence_size(gp->graph_patterns);
    int i;

    for(i = 0; i < size; i++) {
      rasqal_graph_pattern *sgp;
      sgp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      if(rasqal_graph_pattern_tree_mentions_variable(sgp, v))
        return 1;
    }
  }

  return rasqal_graph_pattern_mentions_variable(gp, v);
}



/**
 * rasqal_graph_pattern_promote_variable_mention_to_bind:
 * @gp: graph pattern
 * @v: variable
 * @vars_scope: variable in scope array
 * 
 * INTERNAL - Promote a variable from a mention to a bind - for a basic graph pattenr
 *
 * Return value: non-0 on failure
 */
static int
rasqal_graph_pattern_promote_variable_mention_to_bind(rasqal_graph_pattern* gp,
                                                      rasqal_variable* v,
                                                      unsigned short* vars_scope)
{
  rasqal_query* query = gp->query;
  int width;
  int gp_offset;
  unsigned short* row;

  /* If already bound, do nothing - not an error */
  if(vars_scope[v->offset])
    return 0;

  RASQAL_DEBUG3("Converting variable %s from mention to bound in GP #%d\n",
                v->name, gp->gp_index);
  
  width = rasqal_variables_table_get_total_variables_count(query->vars_table);
  gp_offset = (gp->gp_index + RASQAL_VAR_USE_MAP_OFFSET_LAST + 1) * width;
  row = &query->variables_use_map[gp_offset];

  /* new variable - bind it */
  row[v->offset] |= RASQAL_VAR_USE_BOUND_HERE;

  vars_scope[v->offset] = 1;

  return 0;
}


/**
 * rasqal_query_triples_build_variables_use_map_binds:
 * @use_map: 2D array of (num. variables x num. GPs) to READ and WRITE
 * @width: width of array (num. variables)
 * @gp: graph pattern to use
 * @vars_scope: variables bound in current scope
 *
 * INTERNAL - Mark variables bound in a BASIC graph pattern (triple pattern)
 * 
 **/
static int
rasqal_query_triples_build_variables_use_map_binds(rasqal_query* query,
                                                   unsigned short *use_map,
                                                   int width,
                                                   rasqal_graph_pattern* gp,
                                                   unsigned short* vars_scope)
{
  int start_column = gp->start_column;
  int end_column = gp->end_column;
  int col;
  int gp_offset;
  unsigned short* gp_use_map_row;
  int var_index;

  gp_offset = (gp->gp_index + RASQAL_VAR_USE_MAP_OFFSET_LAST + 1) * width;
  gp_use_map_row = &query->variables_use_map[gp_offset];

  /* Scan all triples and mark per-triple use/bind and for entire BGP use*/  
  for(col = start_column; col <= end_column; col++) {
    rasqal_triple *t;
    rasqal_variable *v;
    unsigned short* triple_row = &query->triples_use_map[col * width];
    
    t = (rasqal_triple*)raptor_sequence_get_at(gp->triples, col);

    if((v = rasqal_literal_as_variable(t->subject))) {
      if(!vars_scope[v->offset]) {
        /* v needs binding in this triple as SUBJECT */
        triple_row[v->offset] |= RASQAL_TRIPLES_BOUND_SUBJECT;
      } else
        triple_row[v->offset] |= RASQAL_TRIPLES_USE_SUBJECT;
    }
    
    if((v = rasqal_literal_as_variable(t->predicate))) {
      if(!vars_scope[v->offset]) {
        /* v needs binding in this triple as PREDICATE */
        triple_row[v->offset] |= RASQAL_TRIPLES_BOUND_PREDICATE;
      } else
        triple_row[v->offset] |= RASQAL_TRIPLES_USE_PREDICATE;
    }

    if((v = rasqal_literal_as_variable(t->object))) {
      if(!vars_scope[v->offset]) {
        /* v needs binding in this triple as OBJECT */
        triple_row[v->offset] |= RASQAL_TRIPLES_BOUND_OBJECT;
      } else
        triple_row[v->offset] |= RASQAL_TRIPLES_USE_OBJECT;
    }

    if(t->origin) {
      if((v = rasqal_literal_as_variable(t->origin))) {
        if(!vars_scope[v->offset]) {
          /* v needs binding in this triple as GRAPH */
          triple_row[v->offset] |= RASQAL_TRIPLES_BOUND_GRAPH;
        } else
          triple_row[v->offset] |= RASQAL_TRIPLES_USE_GRAPH;
      }
    }

  
    /* Promote first use of a variable into a bind. */
    if((v = rasqal_literal_as_variable(t->subject)) &&
       triple_row[v->offset] & RASQAL_TRIPLES_BOUND_SUBJECT) {
      rasqal_graph_pattern_promote_variable_mention_to_bind(gp, v, vars_scope);
    }
    
    if((v = rasqal_literal_as_variable(t->predicate)) &&
       triple_row[v->offset] & RASQAL_TRIPLES_BOUND_PREDICATE) {
      rasqal_graph_pattern_promote_variable_mention_to_bind(gp, v, vars_scope);
    }

    if((v = rasqal_literal_as_variable(t->object)) &&
       triple_row[v->offset] & RASQAL_TRIPLES_BOUND_OBJECT) {
      rasqal_graph_pattern_promote_variable_mention_to_bind(gp, v, vars_scope);
    }

    if(t->origin) {
      if((v = rasqal_literal_as_variable(t->origin)) &&
         triple_row[v->offset] & RASQAL_TRIPLES_BOUND_GRAPH) {
        rasqal_graph_pattern_promote_variable_mention_to_bind(gp, v, vars_scope);
      }
    }

  }

  /* Scan all triples for USEs of variables and update BGP mentioned bits */
  for(var_index = 0; var_index < width; var_index++) {
    int mentioned = 0;

    for(col = start_column; col <= end_column; col++) {
      unsigned short* triple_row = &query->triples_use_map[col * width];
      
      if(triple_row[var_index] & RASQAL_TRIPLES_USE_MASK) {
        mentioned = 1;
        break;
      }
    }

    if(mentioned)
      gp_use_map_row[var_index] |= RASQAL_VAR_USE_MENTIONED_HERE;
    else
      gp_use_map_row[var_index] = RASQAL_GOOD_CAST(unsigned short, gp_use_map_row[var_index] & ~RASQAL_VAR_USE_MENTIONED_HERE);
    
  }

  return 0;
}


#ifdef RASQAL_DEBUG
static void
rasqal_query_dump_vars_scope(rasqal_query* query, int width, unsigned short *vars_scope)
{
  int j;
  
  for(j = 0; j < width; j++) {
    rasqal_variable* v = rasqal_variables_table_get(query->vars_table, j);
    fprintf(DEBUG_FH, "%8s ", v->name);
  }
  fputs("\n  ", DEBUG_FH);
  for(j = 0; j < width; j++) {
    fprintf(DEBUG_FH, "%8d ", vars_scope[j]);
  }
  fputc('\n', DEBUG_FH);
}
#endif


/**
 * rasqal_query_graph_pattern_build_variables_use_map_binds:
 * @use_map: 2D array of (num. variables x num. GPs) to READ and WRITE
 * @width: width of array (num. variables)
 * @gp: graph pattern to use
 * @vars_scope: variables bound in current scope
 *
 * INTERNAL - Calculate which GPs bind variables in a graph_pattern tree walk
 * 
 * Return value: non-0 on failure
 **/
static int
rasqal_query_graph_pattern_build_variables_use_map_binds(rasqal_query* query,
                                                         unsigned short *use_map,
                                                         int width,
                                                         rasqal_graph_pattern *gp,
                                                         unsigned short *vars_scope)
{
  int rc = 0;
  
  /* This a query in-order walk so SELECT ... AS ?var, BIND .. AS
   * ?var and GRAPH ?var come before any sub-graph patterns
   */

  switch(gp->op) {
    case RASQAL_GRAPH_PATTERN_OPERATOR_BASIC: 
      rc = rasqal_query_triples_build_variables_use_map_binds(query,
                                                              use_map,
                                                              width,
                                                              gp, vars_scope);
      break;

    case RASQAL_GRAPH_PATTERN_OPERATOR_GRAPH:
      rc = rasqal_query_graph_build_variables_use_map_binds(gp, vars_scope);
      break;
      
    case RASQAL_GRAPH_PATTERN_OPERATOR_FILTER:
      /* Only mentions */
      break;

    case RASQAL_GRAPH_PATTERN_OPERATOR_LET:
      rc = rasqal_query_let_build_variables_use_map_binds(gp, vars_scope);
      break;
      
    case RASQAL_GRAPH_PATTERN_OPERATOR_SELECT:
      rc = rasqal_query_select_build_variables_use_map_binds(query,
                                                             use_map,
                                                             width,
                                                             gp, vars_scope);
      break;
      
    case RASQAL_GRAPH_PATTERN_OPERATOR_UNION:
    case RASQAL_GRAPH_PATTERN_OPERATOR_GROUP:
    case RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL:
      rc = rasqal_query_union_build_variables_use_map_binds(query,
                                                            use_map,
                                                            width,
                                                            gp, vars_scope);
      break;
      
    case RASQAL_GRAPH_PATTERN_OPERATOR_VALUES:
      rc = rasqal_query_values_build_variables_use_map_binds(query,
                                                             use_map, width,
                                                             gp,
                                                             vars_scope);
      break;

    case RASQAL_GRAPH_PATTERN_OPERATOR_SERVICE:
    case RASQAL_GRAPH_PATTERN_OPERATOR_MINUS:
    case RASQAL_GRAPH_PATTERN_OPERATOR_UNKNOWN:
      break;
  }
  
#ifdef RASQAL_DEBUG
  RASQAL_DEBUG3("vars_scope after %s graph pattern #%d verb is now:\n  ",
                rasqal_graph_pattern_operator_as_string(gp->op), gp->gp_index);
  rasqal_query_dump_vars_scope(query, width, vars_scope);
#endif

  /* Bind sub-graph patterns but not sub-SELECT gp twice */
  if(gp->op != RASQAL_GRAPH_PATTERN_OPERATOR_SELECT && gp->graph_patterns) {
    int gp_size = raptor_sequence_size(gp->graph_patterns);
    int i;
    
    /* recursively call binds */
    for(i = 0; i < gp_size; i++) {
      rasqal_graph_pattern *sgp;
      
      sgp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      RASQAL_DEBUG2("checking vars_scope for SGP #%d\n", sgp->gp_index);

      rc = rasqal_query_graph_pattern_build_variables_use_map_binds(query,
                                                                    use_map,
                                                                    width, sgp,
                                                                    vars_scope);
      if(rc)
        goto done;

#ifdef RASQAL_DEBUG
      RASQAL_DEBUG2("vars_scope after SGP #%d is now:\n  ", sgp->gp_index);
      rasqal_query_dump_vars_scope(query, width, vars_scope);
#endif
    }
  }


  done:
  return rc;
}


/**
 * rasqal_query_build_variables_use_map_binds:
 * @query: the #rasqal_query to find the variables in
 * @use_map: 2D array of (num. variables x num. GPs) to READ and WRITE
 * @width: width of array (num. variables)
 * @gp: graph pattern to use
 *
 * INTERNAL - Calculate which GPs bind variables in a graph_pattern tree walk
 * 
 * Return value: non-0 on failure
 **/
static int
rasqal_query_build_variables_use_map_binds(rasqal_query* query,
                                           unsigned short *use_map,
                                           int width,
                                           rasqal_graph_pattern *gp)
{
  int rc;
  unsigned short* vars_scope;
  raptor_sequence* seq;

  vars_scope = RASQAL_CALLOC(unsigned short*, RASQAL_GOOD_CAST(size_t, width), sizeof(unsigned short));
  if(!vars_scope)
    return 1;
  
  rc = rasqal_query_graph_pattern_build_variables_use_map_binds(query,
                                                                use_map,
                                                                width,
                                                                gp,
                                                                vars_scope);

  /* Record variable binding for GROUP BY expressions (SPARQL 1.1) */
  seq = rasqal_query_get_group_conditions_sequence(query);
  if(seq) {
    int size = raptor_sequence_size(seq);
    int i;
    unsigned short *use_map_row;

    use_map_row = &use_map[RASQAL_VAR_USE_MAP_OFFSET_GROUP_BY * width];

    /* sequence of rasqal_expression of operation RASQAL_EXPR_LITERAL
     * containing a variable literal, with the variable having
     * ->expression set to the expression
     */
    for(i = 0; i < size; i++) {
      rasqal_expression *e;
      rasqal_literal *l;

      e = (rasqal_expression*)raptor_sequence_get_at(seq, i);

      l = e->literal;
      if(l) {
        rasqal_variable* v = l->value.variable;
        if(v && v->expression) {
          use_map_row[v->offset] |= RASQAL_VAR_USE_BOUND_HERE;

          vars_scope[v->offset] = 1;
        }
      }
    }
  }

  RASQAL_FREE(intarray, vars_scope);

  return rc;
}


#ifdef RASQAL_DEBUG
static const char* const use_map_offset_labels[RASQAL_VAR_USE_MAP_OFFSET_LAST + 1] = {
  "Verbs",
  "GROUP BY",
  "HAVING",
  "ORDER BY",
  "VALUES"
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
  
  fprintf(fh, "Query GP variables-use map (B=bound, M=mentioned, U=used):\n");
  fputs("GP#  Type      ", fh);

  for(i = 0; i < width; i++) {
    rasqal_variable* v = rasqal_variables_table_get(query->vars_table, i);
    fprintf(fh, "%-12s ", v->name);
  }

  fputc('\n', fh);

  for(row_index = 0; row_index < height; row_index++) {
    unsigned short *row = &query->variables_use_map[row_index * width];
    int gp_index = row_index - (RASQAL_VAR_USE_MAP_OFFSET_LAST + 1);

    if(gp_index < 0)
      fprintf(fh, "--   %-8s  ", use_map_offset_labels[row_index]);
    else {
      rasqal_graph_pattern* gp;
      gp = (rasqal_graph_pattern*)raptor_sequence_get_at(query->graph_patterns_sequence, gp_index);
      fprintf(fh, "%-2d   %-8s  ", gp_index, 
              rasqal_graph_pattern_operator_as_string(gp->op));
    }
    
    for(i = 0; i < width; i++) {
      int flag_index = row[i];

      /* Turn unknown flags into "???" */
      if(flag_index > N_MAP_FLAGS_LABELS - 1)
        flag_index = N_MAP_FLAGS_LABELS;
      fprintf(fh, "%-12s ", use_map_flags_labels[flag_index]);

    }
    fputc('\n', fh);
  }
}


static const char bit_label[9] = "spogSPOG";

static void
rasqal_query_print_triples_use_map(FILE* fh, rasqal_query* query)
{
  int width;
  int height;
  int i;
  int column;

  width = rasqal_variables_table_get_total_variables_count(query->vars_table);
  height = raptor_sequence_size(query->triples);
  
  fprintf(fh,
          "Query triples variables-use map (mentioned: spog, bound: SPOG):\n");

  fputs("Triple# ", fh);
  for(i = 0; i < width; i++) {
    rasqal_variable* v = rasqal_variables_table_get(query->vars_table, i);
    fprintf(fh, "%-12s ", v->name);
  }
  fputc('\n', fh);

  for(column = 0; column < height; column++) {
    unsigned short *row = &query->triples_use_map[column * width];
    rasqal_triple* t;

    fprintf(fh, "%-7d ", column);
    for(i = 0; i < width; i++) {
      int flag = row[i];
      char label[14] = "             ";
      int bit;
      
      /* bit 0: RASQAL_TRIPLES_USE_SUBJECT
       * to
       * bit 7: RASQAL_TRIPLES_BOUND_GRAPH 
       */
      for(bit = 0; bit < 8; bit++) {
        if(flag & (1 << bit))
          label[(bit & 3)] = bit_label[bit];
      }
      fputs(label, fh);
    }
    fputc(' ', fh);
    t = (rasqal_triple*)raptor_sequence_get_at(query->triples, column);
    rasqal_triple_print(t, fh);
    fputc('\n', fh);
  }
}
#endif


/* for use with rasqal_expression_visit and user_data=rasqal_query */
static int
rasqal_query_expression_build_variables_use_map_row(unsigned short *use_map_row,
                                                    rasqal_expression *e)
{
  if(e->literal) {
    rasqal_variable* v;

    v = rasqal_literal_as_variable(e->literal);

    if(v) 
      use_map_row[v->offset] |= RASQAL_VAR_USE_MENTIONED_HERE;
  }

  return 0;
}


/*
 * rasqal_query_build_variables_sequence_use_map_row:
 * @use_map_row: row to write to
 * @vars_seq: sequence of variables
 * @bind: force bind; otherwise binds only if var has an expression
 *
 * INTERNAL: Mark variables seen / bound in a sequence of variables (with optional expression)
 *
 * Return value: non-0 on failure
 */
static int
rasqal_query_build_variables_sequence_use_map_row(unsigned short* use_map_row,
                                                  raptor_sequence *vars_seq,
                                                  int bind)
{
  int rc = 0;
  int idx;

  for(idx = 0; 1; idx++) {
    rasqal_variable* v;
    int flags = RASQAL_VAR_USE_MENTIONED_HERE;

    v = (rasqal_variable*)raptor_sequence_get_at(vars_seq, idx);
    if(!v)
      break;

    if(bind)
      flags |= RASQAL_VAR_USE_BOUND_HERE;
    else {
      rasqal_expression *e;
      e = v->expression;
      if(e) {
        rasqal_query_expression_build_variables_use_map(use_map_row, e);
        flags |= RASQAL_VAR_USE_BOUND_HERE;
      }
    }

    use_map_row[v->offset] = RASQAL_GOOD_CAST(unsigned short, use_map_row[v->offset] | flags);
  }

  return rc;
}
  

/*
 * Mark variables seen in a sequence of literals
 */
static int
rasqal_query_build_literals_sequence_use_map_row(unsigned short* use_map_row,
                                                 raptor_sequence *lits_seq)
{
  int idx;

  for(idx = 0; 1; idx++) {
    rasqal_literal* l;
    rasqal_variable* v;

    l = (rasqal_literal*)raptor_sequence_get_at(lits_seq, idx);
    if(!l)
      break;

    v = rasqal_literal_as_variable(l);
    if(v) 
      use_map_row[v->offset] |= RASQAL_VAR_USE_MENTIONED_HERE;
  }

  return 0;
}
  

/*
 * Mark variables seen in a sequence of expressions
 */
static int
rasqal_query_build_expressions_sequence_use_map_row(unsigned short* use_map_row,
                                                    raptor_sequence *exprs_seq)
{
  int rc = 0;
  int idx;

  for(idx = 0; 1; idx++) {
    rasqal_expression *e;

    e = (rasqal_expression*)raptor_sequence_get_at(exprs_seq, idx);
    if(!e)
      break;

    rasqal_query_expression_build_variables_use_map(use_map_row, e);
  }

  return rc;
}
  

/**
 * rasqal_query_build_variables_use_map:
 * @query: the #rasqal_query to find the variables in
 *
 * INTERNAL - Record where variables are mentioned in query structures
 *
 * Need to walk query components that may mention or bind variables
 * and record their variable use:
 * 1) Query verbs: ASK SELECT CONSTRUCT DESCRIBE (SPARQL 1.0)
 *   1a) SELECT project-expressions (SPARQL 1.1)
 * 2) GROUP BY expr/var (SPARQL 1.1 TBD)
 * 3) HAVING expr (SPARQL 1.1 TBD)
 * 4) ORDER list-of-expr (SPARQL 1.0)
 *
 * Constructs a 2D array of 
 *   width: number of variables
 *   height: (number of graph patterns + #RASQAL_VAR_USE_MAP_OFFSET_LAST+1)
 * where each row records how a variable is bound/used in a GP or query
 * structure.
 *
 * The first #RASQAL_VAR_USE_MAP_OFFSET_LAST+1 rows are used for the
 * query structures above defined by the #rasqal_var_use_map_offset
 * enum.  Example: row 0 (#RASQAL_VAR_USE_MAP_OFFSET_VERBS) is used
 * to record variable use for the query verbs - item 1) in the list
 * above.
 *
 * Graph pattern rows are recorded at row
 * <GP ID> + #RASQAL_VAR_USE_MAP_OFFSET_LAST + 1
 *
 * Return value: non-0 on failure
 **/
static int
rasqal_query_build_variables_use_map(rasqal_query* query,
                                     rasqal_projection* projection)
{
  int width;
  int height;
  int rc = 0;
  unsigned short *use_map;
  raptor_sequence* seq;
  unsigned short *use_map_row;
  
  width = rasqal_variables_table_get_total_variables_count(query->vars_table);
  height = RASQAL_VAR_USE_MAP_OFFSET_LAST + 1 + query->graph_pattern_count;
  
  use_map = RASQAL_CALLOC(unsigned short*, RASQAL_GOOD_CAST(size_t, width * height),
                          sizeof(unsigned short));
  if(!use_map)
    return 1;

  if(query->variables_use_map)
    RASQAL_FREE(shortarray, query->variables_use_map);

  query->variables_use_map = use_map;

  height = raptor_sequence_size(query->triples);
  use_map = RASQAL_CALLOC(unsigned short*, RASQAL_GOOD_CAST(size_t, width * height),
                          sizeof(unsigned short));
  if(!use_map) {
    RASQAL_FREE(shortarray, query->variables_use_map);
    query->variables_use_map = NULL;
    return 1;
  }

  if(query->triples_use_map)
    RASQAL_FREE(shortarray, query->triples_use_map);

  query->triples_use_map = use_map;

  use_map = query->variables_use_map;
  use_map_row = &use_map[RASQAL_VAR_USE_MAP_OFFSET_VERBS * width];
  
  /* record variable use for 1) Query verbs */
  switch(query->verb) {
    case RASQAL_QUERY_VERB_SELECT:
      /* This also handles 1a) select/project expressions */
      if(projection && projection->variables)
        rc = rasqal_query_build_variables_sequence_use_map_row(use_map_row,
                                                               projection->variables, 0);
      break;
  
    case RASQAL_QUERY_VERB_DESCRIBE:
      /* This is a list of rasqal_literal not rasqal_variable */
      rc = rasqal_query_build_literals_sequence_use_map_row(use_map_row,
                                                            query->describes);
      break;
      
    case RASQAL_QUERY_VERB_CONSTRUCT:
      if(1) {
        int last_column = raptor_sequence_size(query->constructs)-1;
        
        rc = rasqal_query_triples_build_variables_use_map_row(query->constructs, 
                                                              use_map_row,
                                                              0,
                                                              last_column);
      }
      break;

    case RASQAL_QUERY_VERB_DELETE:
    case RASQAL_QUERY_VERB_INSERT:
    case RASQAL_QUERY_VERB_UPDATE:
      /* FIXME - should mark the verbs using triple patterns as using vars */
      break;
      
    case RASQAL_QUERY_VERB_UNKNOWN:
    case RASQAL_QUERY_VERB_ASK:
    default:
      break;
  }

  if(rc)
    goto done;
  

  /* Record variable use for 2) GROUP BY expressions (SPARQL 1.1) */
  seq = rasqal_query_get_group_conditions_sequence(query);
  if(seq) {
    use_map_row = &use_map[RASQAL_VAR_USE_MAP_OFFSET_GROUP_BY * width];
    rc = rasqal_query_build_expressions_sequence_use_map_row(use_map_row, seq);
    if(rc)
      goto done;
  }
  

  /* Record variable use for 3) HAVING expr (SPARQL 1.1) */
  seq = rasqal_query_get_having_conditions_sequence(query);
  if(seq) {
    use_map_row = &use_map[RASQAL_VAR_USE_MAP_OFFSET_HAVING * width];
    rc = rasqal_query_build_expressions_sequence_use_map_row(use_map_row, seq);
    if(rc)
      goto done;
  }

  /* record variable use for 4) ORDER list-of-expr (SPARQL 1.0) */
  seq = rasqal_query_get_order_conditions_sequence(query);
  if(seq) {
    use_map_row = &use_map[RASQAL_VAR_USE_MAP_OFFSET_ORDER_BY * width];
    rc = rasqal_query_build_expressions_sequence_use_map_row(use_map_row, seq);
    if(rc)
      goto done;
  }
  

  /* record variable use for 5) VALUES (SPARQL 1.1) */
  if(query->bindings) {
    use_map_row = &use_map[RASQAL_VAR_USE_MAP_OFFSET_VALUES * width];
    rc = rasqal_query_build_variables_sequence_use_map_row(use_map_row,
                                                           query->bindings->variables, 1);
    if(rc)
      goto done;
  }
  

  /* record variable use for graph patterns */
  rc = rasqal_query_graph_pattern_build_variables_use_map(query,
                                                          use_map,
                                                          width,
                                                          query->query_graph_pattern);
  if(rc)
    goto done;
  
#ifdef RASQAL_DEBUG
  RASQAL_DEBUG1("variables use map after mentions: ");
  rasqal_query_print_variables_use_map(DEBUG_FH, query);
  fputs("\n", DEBUG_FH); 
#endif    

  /* calculate which GPs bind variables for all query graph patterns
   * reading from the use_map
   */
  rc = rasqal_query_build_variables_use_map_binds(query, use_map, width,
                                                  query->query_graph_pattern);
  if(rc)
    goto done;

#ifdef RASQAL_DEBUG
  RASQAL_DEBUG1("use map after binds and mentions: ");
  rasqal_query_print_variables_use_map(DEBUG_FH, query);
  fputs("\n", DEBUG_FH); 

  RASQAL_DEBUG1("triples use map after binds and mentions: ");
  rasqal_query_print_triples_use_map(DEBUG_FH, query);
  fputs("\n", DEBUG_FH); 
#endif    

  done:
  return rc;
}


/**
 * rasqal_query_graph_build_variables_use_map_binds:
 * @gp: the #rasqal_graph_pattern GRAPH pattern
 * @vars_scope: variable scope array
 *
 * INTERNAL - Mark variables bound in a GRAPH graph pattern
 * 
 **/
static int
rasqal_query_graph_build_variables_use_map_binds(rasqal_graph_pattern* gp,
                                                 unsigned short* vars_scope)
{
  rasqal_variable *v;

  v = rasqal_literal_as_variable(gp->origin);

  if(v)
    rasqal_graph_pattern_promote_variable_mention_to_bind(gp, v, vars_scope);

  return 0;
}



/**
 * rasqal_query_expression_build_variables_use_map:
 * @use_map_row: 1D array of size num. variables to write
 * @e: filter expression to use
 *
 * INTERNAL - Mark variables mentioned in an expression
 * 
 **/
static void
rasqal_query_expression_build_variables_use_map(unsigned short *use_map_row,
                                                rasqal_expression* e)
{
  rasqal_expression_visit(e, 
                          (rasqal_expression_visit_fn)rasqal_query_expression_build_variables_use_map_row,
                          use_map_row);
}


/**
 * rasqal_query_let_build_variables_use_map:
 * @query: the #rasqal_query to find the variables in
 * @use_map_row: 1D array of size num. variables to write
 * @e: let expression to use
 *
 * INTERNAL - Mark variables mentioned in a LET graph pattern
 * 
 **/
static void
rasqal_query_let_build_variables_use_map(rasqal_query* query,
                                         unsigned short *use_map_row,
                                         rasqal_expression* e)
{
  rasqal_expression_visit(e, 
                          (rasqal_expression_visit_fn)rasqal_query_expression_build_variables_use_map_row,
                          use_map_row);
}


/**
 * rasqal_query_let_build_variables_use_map_binds:
 * @gp: the #rasqal_graph_pattern LET graph pattern
 * @vars_scope: variable scope array
 *
 * INTERNAL - Mark variables bound in a LET graph pattern
 * 
 **/
static int
rasqal_query_let_build_variables_use_map_binds(rasqal_graph_pattern* gp,
                                               unsigned short* vars_scope)
{
  rasqal_variable* v = gp->var;

  rasqal_graph_pattern_promote_variable_mention_to_bind(gp, v, vars_scope);

  return 0;
}



/**
 * rasqal_query_select_build_variables_use_map:
 * @query: the #rasqal_query to find the variables in
 * @use_map_row: 1D array of size num. variables to write
 * @width: width of array (num. variables)
 * @gp: the SELECT graph pattern
 *
 * INTERNAL - Mark variables mentioned in a sub-SELECT graph pattern
 * 
 **/
static int
rasqal_query_select_build_variables_use_map(rasqal_query* query,
                                            unsigned short *use_map_row,
                                            int width,
                                            rasqal_graph_pattern* gp)
{
  int rc = 0;
  raptor_sequence* seq;

  /* mention any variables in the projection */
  seq = rasqal_projection_get_variables_sequence(gp->projection);

  if(!seq && gp->graph_patterns) {
    int var_index;
    int gp_size;
    
    seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_variable,
                              (raptor_data_print_handler)rasqal_variable_print);

    gp_size = raptor_sequence_size(gp->graph_patterns);

    /* No variables; must be SELECT * so form it from all mentioned
     * variables in the sub graph patterns
     */
    for(var_index = 0; var_index < width; var_index++) {
      rasqal_variable *v;
      int gp_index;

      v = rasqal_variables_table_get(query->vars_table, var_index);
      
      for(gp_index = 0; gp_index < gp_size; gp_index++) {
        rasqal_graph_pattern *sgp;
        
        sgp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns,
                                                            gp_index);
        if(rasqal_graph_pattern_tree_mentions_variable(sgp, v)) {
          raptor_sequence_push(seq, rasqal_new_variable_from_variable(v));
  
          /* if any sub-GP mentions the variable we can end the SGP loop */
          break;
        }
      }
    }

    /* FIXME: uses internal knowledge of projection structure */
    gp->projection->variables = seq;
  }
  
  rc = rasqal_query_build_variables_sequence_use_map_row(use_map_row, seq, 0);
  if(rc)
    return rc;
  
  if(gp->bindings) {
    rc = rasqal_query_build_variables_sequence_use_map_row(use_map_row,
                                                           gp->bindings->variables, 1);
    if(rc)
      return rc;
  }

  return rc;
}


/**
 * rasqal_query_select_build_variables_use_map_binds:
 * @use_map: 2D array of (num. variables x num. GPs) to READ and WRITE
 * @width: width of array (num. variables)
 * @gp: graph pattern to use
 * @vars_scope: variables bound in current scope
 *
 * INTERNAL - Mark variables bound in a sub-SELECT graph pattern
 * 
 **/
static int
rasqal_query_select_build_variables_use_map_binds(rasqal_query* query,
                                                  unsigned short *use_map,
                                                  int width,
                                                  rasqal_graph_pattern* gp,
                                                  unsigned short* vars_scope)
{
  unsigned short* inner_vars_scope;
  raptor_sequence* seq;
  int size;
  int i;

  inner_vars_scope = RASQAL_CALLOC(unsigned short*, RASQAL_GOOD_CAST(size_t, width),
                                   sizeof(unsigned short));
  if(!inner_vars_scope)
    return 1;

  seq = gp->graph_patterns;
  size = raptor_sequence_size(seq);
  for(i = 0; i < size; i++) {
    rasqal_graph_pattern *sgp;
    
    sgp = (rasqal_graph_pattern*)raptor_sequence_get_at(seq, i);

    rasqal_query_graph_pattern_build_variables_use_map_binds(query,
                                                             use_map, width,
                                                             sgp,
                                                             inner_vars_scope);
  }
  
  RASQAL_FREE(intarray, inner_vars_scope);
  inner_vars_scope = NULL;
  
  /* Mark as binding in the OUTER scope all variables that were bound
   * in the INNER SELECT projection
   */
  seq = rasqal_projection_get_variables_sequence(gp->projection);
  size = raptor_sequence_size(seq);
  for(i = 0; i < size; i++) {
    rasqal_variable * v;
    
    v = (rasqal_variable*)raptor_sequence_get_at(seq, i);

    rasqal_graph_pattern_promote_variable_mention_to_bind(gp, v, vars_scope);
  }

  return 0;
}


/**
 * rasqal_query_union_build_variables_use_map_binds:
 * @use_map: 2D array of (num. variables x num. GPs) to READ and WRITE
 * @width: width of array (num. variables)
 * @gp: graph pattern to use
 * @vars_scope: variables bound in current scope
 *
 * INTERNAL - Mark variables bound in a UNION sub-graph patterns
 * 
 **/
static int
rasqal_query_union_build_variables_use_map_binds(rasqal_query* query,
                                                 unsigned short *use_map,
                                                 int width,
                                                 rasqal_graph_pattern* gp,
                                                 unsigned short* vars_scope)
{
  unsigned short* inner_vars_scope;
  raptor_sequence* seq;
  int gp_size;
  int i;
  int rc = 0;

  seq = gp->graph_patterns;
  gp_size = raptor_sequence_size(seq);
  
  inner_vars_scope = RASQAL_CALLOC(unsigned short*, RASQAL_GOOD_CAST(size_t, width),
                                   sizeof(unsigned short));
  if(!inner_vars_scope)
    return 1;

  for(i = 0; i < gp_size; i++) {
    rasqal_graph_pattern *sgp;
    
    /* UNION starts with a copy of all scoped outer variables */
    memcpy(inner_vars_scope, vars_scope, RASQAL_GOOD_CAST(size_t,
                                                          RASQAL_GOOD_CAST(size_t, width) * sizeof(unsigned short)));

    sgp = (rasqal_graph_pattern*)raptor_sequence_get_at(seq, i);

    rc = rasqal_query_graph_pattern_build_variables_use_map_binds(query,
                                                                  use_map,
                                                                  width,
                                                                  sgp,
                                                                  inner_vars_scope);
    if(rc)
      goto done;

  }
  
  done:
  RASQAL_FREE(intarray, inner_vars_scope);
  
  return rc;
}


/**
 * rasqal_query_values_build_variables_use_map_binds:
 * @use_map: 2D array of (num. variables x num. GPs) to READ and WRITE
 * @width: width of array (num. variables)
 * @gp: graph pattern to use
 * @vars_scope: variables bound in current scope
 *
 * INTERNAL - Mark variables bound in a VALUES sub-graph patterns
 *
 **/
static int
rasqal_query_values_build_variables_use_map_binds(rasqal_query* query,
                                                  unsigned short *use_map,
                                                  int width,
                                                  rasqal_graph_pattern* gp,
                                                  unsigned short* vars_scope)
{
  raptor_sequence* seq;
  int size;
  int i;
  int rc = 0;

  seq = gp->bindings->variables;
  size = raptor_sequence_size(seq);

  for(i = 0; i < size; i++) {
    rasqal_variable * v;
    v = (rasqal_variable*)raptor_sequence_get_at(seq, i);

    rasqal_graph_pattern_promote_variable_mention_to_bind(gp, v, vars_scope);
  }

  return rc;
}


/*
 * rasqal_query_variable_is_bound:
 * @gp: #rasqal_graph_pattern object
 * @variable: variable
 * 
 * INTERNAL - Test if a variable is bound in the given GP
 *
 * Return value: non-0 if bound
 */
int
rasqal_query_variable_is_bound(rasqal_query* query, rasqal_variable* v)
{
  unsigned short *use_map = query->variables_use_map;
  int width;
  int height;
  int row_index;
  
  width = rasqal_variables_table_get_total_variables_count(query->vars_table);
  height = (RASQAL_VAR_USE_MAP_OFFSET_LAST + 1) + query->graph_pattern_count;

  for(row_index = 0; row_index < height; row_index++) {
    unsigned short *row = &use_map[row_index * width];
    if(row[v->offset])
      return 1;
  }
  
  return 0;
}

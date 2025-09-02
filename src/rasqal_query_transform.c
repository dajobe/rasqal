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


static int rasqal_query_build_scope_hierarchy_recursive(rasqal_query* query, rasqal_graph_pattern* gp, rasqal_query_scope* parent_scope);
static int rasqal_query_triple_in_exists_pattern(rasqal_query* query, int triple_index);
static void rasqal_query_print_scope_variable_usage(FILE* fh, rasqal_query* query);
static void rasqal_query_scope_print_variable_analysis(FILE* fh, rasqal_query_scope* scope, int depth);
static int rasqal_helper_gp_tree_uses_variable(rasqal_graph_pattern* gp, rasqal_variable* v);
static int rasqal_helper_gp_tree_uses_variable_excluding_basic(rasqal_graph_pattern* gp, rasqal_variable* v);


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
  if(!v)
    return 1; /* error */

  /* rasqal_new_variable_typed copied the l->string blank node name
   * so we need to free it */
  if(l->string) {
    RASQAL_FREE(char*, l->string);
    l->string = NULL;
  }

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
  
  /* If query graph pattern is not available yet, defer expansion */
  if(!rq->query_graph_pattern) {
    /* Keep the wildcard flag set for later expansion */
    return 0;
  }

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
  raptor_sequence_print(seq, RASQAL_DEBUG_FH);
  fputs("\n", RASQAL_DEBUG_FH); 
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
    raptor_sequence_print(new_seq, RASQAL_DEBUG_FH);
    fputs("\n", RASQAL_DEBUG_FH); 
#endif
    raptor_free_sequence(projection->variables);
    projection->variables = new_seq;
  } else
    raptor_free_sequence(new_seq);

  return 0;
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
  rasqal_expression_print(e, RASQAL_DEBUG_FH);
  fprintf(RASQAL_DEBUG_FH, "\n");
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
  rasqal_expression_print(e, RASQAL_DEBUG_FH);
  fputc('\n', RASQAL_DEBUG_FH);
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
 * and rasqal_query_build_variable_agg_use() and used in
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
  /* Simple validation using scope system */
  int i;

#ifdef RASQAL_DEBUG
  RASQAL_DEBUG1("query scope:");
  rasqal_query_print_scope_variable_usage(RASQAL_DEBUG_FH, query);
  fputs("\n", RASQAL_DEBUG_FH);
#endif

  for(i = 0; 1; i++) {
    rasqal_variable* v = rasqal_variables_table_get(query->vars_table, i);
    if(!v)
      break;

    /* Check if variable is bound but not used in projection */
    if(rasqal_query_variable_is_bound(query, v)) {
      /* Check if variable is in projection */
      int in_projection = 0;
      if(projection && projection->variables) {
        int proj_size = raptor_sequence_size(projection->variables);
        int j;
        for(j = 0; j < proj_size; j++) {
          rasqal_variable* proj_var = (rasqal_variable*)raptor_sequence_get_at(projection->variables, j);
          if(proj_var && !strcmp((const char*)v->name, (const char*)proj_var->name)) {
            in_projection = 1;
            break;
          }
        }
      }
      
      if(!in_projection) {
        /* SCOPE-AWARE VALIDATION: Check if variable is used in other
         * query parts */
        int used_elsewhere = 0;

        /* Check if variable is used in FILTER expressions or complex
         * graph patterns */
        if(query->query_graph_pattern) {
          /* Only check non-basic graph patterns (FILTER, EXISTS,
           * etc.) for "used elsewhere" */
          used_elsewhere = rasqal_helper_gp_tree_uses_variable_excluding_basic(query->query_graph_pattern, v);
        }

        /* Check if variable is used in ORDER BY, GROUP BY, HAVING clauses */
        if(!used_elsewhere) {
          raptor_sequence* order_seq = rasqal_query_get_order_conditions_sequence(query);
          if(order_seq) {
            int j, size = raptor_sequence_size(order_seq);
            for(j = 0; j < size; j++) {
              rasqal_expression* expr = (rasqal_expression*)raptor_sequence_get_at(order_seq, j);
              if(expr && rasqal_expression_mentions_variable(expr, v)) {
                used_elsewhere = 1;
                break;
              }
            }
          }
        }

        /* Only warn if variable is truly unused */
        if(!used_elsewhere) {
        rasqal_log_warning_simple(query->world,
                                  RASQAL_WARNING_LEVEL_VARIABLE_UNUSED,
                                  &query->locator,
                                  "Variable %s was bound but is unused in the query", 
                                  v->name);
        }
      }
    }

    /* Check if variable is selected but never bound */
    if(!rasqal_query_variable_is_bound(query, v)) {
      /* Check if variable is in projection */
      int in_projection = 0;
      if(projection && projection->variables) {
        int proj_size = raptor_sequence_size(projection->variables);
        int j;
        for(j = 0; j < proj_size; j++) {
          rasqal_variable* proj_var;
          proj_var = (rasqal_variable*)raptor_sequence_get_at(projection->variables, j);
          if(proj_var && !strcmp((const char*)v->name,
                                 (const char*)proj_var->name)) {
            in_projection = 1;
            break;
          }
        }
      }

      if(in_projection) {
        /* SCOPE-AWARE VALIDATION: Check if variable is bound elsewhere */
        int bound_elsewhere = 0;

        /* Check if variable is bound in graph patterns (triples, etc.) */
        if(query->query_graph_pattern) {
          bound_elsewhere = rasqal_graph_pattern_variable_bound_below(query->query_graph_pattern, v);
        }

        /* ORDER BY, GROUP BY, HAVING clauses typically don't bind variables,
         * they use existing variables. The main binding happens in graph patterns. */

        /* Only warn if variable is truly unbound */
        if(!bound_elsewhere) {
          rasqal_log_warning_simple(query->world,
                                    RASQAL_WARNING_LEVEL_SELECTED_NEVER_BOUND,
                                    &query->locator,
                                    "Variable %s was selected but is never bound in the query",
                                    v->name);
        }
      }
    }
  }
  
  return 0;
}


/* Helpers for rasqal_query_variable_only_in_exists() */

typedef struct rasqal_exists_var_search_state_s {
  rasqal_variable* variable;
  int found;
} rasqal_exists_var_search_state;

static int
rasqal_helper_expr_visit_exists_checker(void *user_data, rasqal_expression *e)
{
  rasqal_exists_var_search_state* st = (rasqal_exists_var_search_state*)user_data;
  if(!e || !st || st->found)
    return st ? st->found : 0;

  if(e->op == RASQAL_EXPR_EXISTS || e->op == RASQAL_EXPR_NOT_EXISTS) {
    if(e->args && raptor_sequence_size(e->args) > 0) {
      rasqal_graph_pattern* exists_gp;
      exists_gp = (rasqal_graph_pattern*)raptor_sequence_get_at(e->args, 0);

      if(rasqal_helper_gp_tree_uses_variable(exists_gp, st->variable)) {
        st->found = 1;
        return 1; /* stop traversal */
      }
    }
    /* Do not descend further through EXISTS args as they are handled */
    return 0;
  }

  return 0;
}
static int
rasqal_helper_triple_uses_variable(rasqal_triple* t, rasqal_variable* v)
{
  rasqal_variable* gv;

  if(!t || !v)
    return 0;
  if(t->subject && t->subject->type == RASQAL_LITERAL_VARIABLE &&
     (t->subject->value.variable == v || (t->subject->value.variable && v->name && t->subject->value.variable->name && strcmp((const char*)t->subject->value.variable->name, (const char*)v->name) == 0)))
    return 1;
  if(t->predicate && t->predicate->type == RASQAL_LITERAL_VARIABLE &&
     (t->predicate->value.variable == v || (t->predicate->value.variable && v->name && t->predicate->value.variable->name && strcmp((const char*)t->predicate->value.variable->name, (const char*)v->name) == 0)))
    return 1;
  if(t->object && t->object->type == RASQAL_LITERAL_VARIABLE &&
     (t->object->value.variable == v || (t->object->value.variable && v->name && t->object->value.variable->name && strcmp((const char*)t->object->value.variable->name, (const char*)v->name) == 0)))
    return 1;
  if(t->origin) {
    gv = rasqal_literal_as_variable(t->origin);
    if(gv == v || (gv && v->name && gv->name && strcmp((const char*)gv->name, (const char*)v->name) == 0))
      return 1;
  }
  return 0;
}

static int
rasqal_helper_gp_tree_uses_variable(rasqal_graph_pattern* gp, rasqal_variable* v)
{
  int col;
  int sz;
  int i;

  if(!gp)
    return 0;

  if(gp->op == RASQAL_GRAPH_PATTERN_OPERATOR_BASIC && gp->triples) {
    int start;
    int end;

    start = gp->start_column;
    end = gp->end_column;
    if(!(start >= 0 && end >= start)) {
      /* Fallback for patterns (e.g., EXISTS) that have local triples without
       * global start/end column indices. */
      start = 0;
      end = raptor_sequence_size(gp->triples) - 1;
    }

    for(col = start; col <= end; col++) {
      rasqal_triple* t;
      t = (rasqal_triple*)raptor_sequence_get_at(gp->triples, col);
      if(rasqal_helper_triple_uses_variable(t, v)) {
        return 1;
      }
    }
  }

  if(gp->graph_patterns) {
    sz = raptor_sequence_size(gp->graph_patterns);
    for(i = 0; i < sz; i++) {
      rasqal_graph_pattern* sgp;
      sgp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      if(rasqal_helper_gp_tree_uses_variable(sgp, v))
        return 1;
    }
  }

  return 0;
}

/* Same as rasqal_helper_gp_tree_uses_variable but excludes BASIC graph patterns (WHERE clause) */
static int
rasqal_helper_gp_tree_uses_variable_excluding_basic(rasqal_graph_pattern* gp, rasqal_variable* v)
{
  int sz;
  int i;

  if(!gp)
    return 0;

  /* Check FILTER expression */
  if(gp->filter_expression && rasqal_expression_mentions_variable(gp->filter_expression, v))
    return 1;

  /* Check child graph patterns */
  if(gp->graph_patterns) {
    sz = raptor_sequence_size(gp->graph_patterns);
    for(i = 0; i < sz; i++) {
      rasqal_graph_pattern* sgp;
      sgp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      /* Recursively check child patterns, but skip BASIC patterns */
      if(sgp->op != RASQAL_GRAPH_PATTERN_OPERATOR_BASIC) {
        if(rasqal_helper_gp_tree_uses_variable_excluding_basic(sgp, v))
          return 1;
      }
    }
  }

  return 0;
}

static int
rasqal_helper_expr_contains_var_in_exists(rasqal_expression* e, rasqal_variable* v)
{
  rasqal_exists_var_search_state st;
  if(!e)
    return 0;
  st.variable = v;
  st.found = 0;
  rasqal_expression_visit(e, rasqal_helper_expr_visit_exists_checker, &st);
  return st.found;
}

/* Recursively search a GP tree for FILTER expressions that contain
 * EXISTS/NOT EXISTS mentioning the given variable name. */
static int
rasqal_helper_gp_tree_contains_var_in_exists(rasqal_graph_pattern* gp, rasqal_variable* v)
{
  int i;
  int size;

  if(!gp)
    return 0;

  if(gp->op == RASQAL_GRAPH_PATTERN_OPERATOR_FILTER && gp->filter_expression) {
    if(rasqal_helper_expr_contains_var_in_exists(gp->filter_expression, v))
      return 1;
  }

  if(gp->graph_patterns) {
    size = raptor_sequence_size(gp->graph_patterns);
    for(i = 0; i < size; i++) {
      rasqal_graph_pattern* sgp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      if(rasqal_helper_gp_tree_contains_var_in_exists(sgp, v))
        return 1;
    }
  }

  return 0;
}

/*
 * rasqal_query_variable_only_in_exists:
 * @query: query object
 * @var: variable to check
 *
 * INTERNAL - Check if a variable is only used in EXISTS expressions
 * 
 * Returns: 1 if variable is only used in EXISTS patterns, 0 otherwise
 */
int
rasqal_query_variable_only_in_exists(rasqal_query* query, rasqal_variable* var)
{
  int used_in_exists = 0;
  int gp_index;

  if(!query || !var)
    return 0;

  /* Traverse enumerated graph patterns and inspect their own triple sequences.
   * This correctly accounts for EXISTS patterns that use separate triples
   * sequences and do not contribute to query->triples. */
  for(gp_index = 0; ; gp_index++) {
    rasqal_graph_pattern* gp = rasqal_query_get_graph_pattern(query, gp_index);
    if(!gp)
      break;

    if(gp->op == RASQAL_GRAPH_PATTERN_OPERATOR_BASIC && gp->triples) {
      int start = gp->start_column;
      int end = gp->end_column;
      int col;
      if(!(start >= 0 && end >= start)) {
        start = 0;
        end = raptor_sequence_size(gp->triples) - 1;
      }
      for(col = start; col <= end; col++) {
        rasqal_triple* triple = (rasqal_triple*)raptor_sequence_get_at(gp->triples, col);
        if(triple && rasqal_helper_triple_uses_variable(triple, var)) {
          if(gp->is_exists_pattern)
            used_in_exists = 1;
        }
      }
    }

    /* Additionally, scan FILTER expressions for EXISTS/NOT EXISTS usage */
    if(gp->op == RASQAL_GRAPH_PATTERN_OPERATOR_FILTER && gp->filter_expression) {
      if(rasqal_helper_expr_contains_var_in_exists(gp->filter_expression, var)) {
        used_in_exists = 1;
      }
    }
  }

  /* Fallback: scan global triples list and map columns back to EXISTS patterns */
  if(!used_in_exists && query->triples) {
    int tcount = raptor_sequence_size(query->triples);
    int col;
    for(col = 0; col < tcount; col++) {
      rasqal_triple* t = (rasqal_triple*)raptor_sequence_get_at(query->triples, col);
      if(!t)
        continue;
      if(rasqal_helper_triple_uses_variable(t, var) &&
         rasqal_query_triple_in_exists_pattern(query, col)) {
        used_in_exists = 1;
        break;
      }
    }
  }

  /* As a fallback, search the entire GP tree for EXISTS mentions */
  if(!used_in_exists && query->query_graph_pattern) {
    if(rasqal_helper_gp_tree_contains_var_in_exists(query->query_graph_pattern, var))
      used_in_exists = 1;
  }

  /* Treat variables mentioned inside EXISTS/NOT EXISTS as valid usage,
   * regardless of whether they also appear elsewhere.
   */
  return used_in_exists;
}


/*
 * rasqal_query_triple_in_exists_pattern:
 * @query: query object
 * @triple_index: index of triple in query->triples sequence
 *
 * INTERNAL - Check if a triple belongs to an EXISTS pattern
 * 
 * Returns: 1 if triple is part of an EXISTS pattern, 0 otherwise
 */
static int
rasqal_query_triple_in_exists_pattern(rasqal_query* query, int triple_index)
{
  rasqal_graph_pattern* gp;
  int i;
  
  if(!query || !query->query_graph_pattern)
    return 0;
  
  /* Look through all graph patterns to find one marked as EXISTS
   * that covers this triple index.  100 is an arbitrary limit to
   * prevent infinite loops */  
  for(i = 0; i < 100; i++) {
    gp = rasqal_query_get_graph_pattern(query, i);
    if(!gp)
      break;
    
    if(gp->is_exists_pattern && 
       gp->op == RASQAL_GRAPH_PATTERN_OPERATOR_BASIC &&
       triple_index >= gp->start_column && 
       triple_index <= gp->end_column) {
      return 1;
    }
  }
  
  return 0;
}


/*
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

    /* Expand SELECT * to include only mentioned variables */
    if(projection->wildcard) {
      if(rasqal_query_expand_wildcards(query, projection))
        goto done;
    }
  }

  rasqal_query_fold_expressions(query);

  if(query->query_graph_pattern) {
    /* This query prepare processing requires a query graph pattern.
     * Not the case for a legal query like 'DESCRIBE <uri>'
     */

    int modified;
    
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    fputs("Initial query graph pattern:\n  ", RASQAL_DEBUG_FH);
    rasqal_graph_pattern_print(query->query_graph_pattern, RASQAL_DEBUG_FH);
    fputs("\n", RASQAL_DEBUG_FH);
#endif

    do {
      modified = 0;
      
      rc = rasqal_query_graph_pattern_visit2(query, 
                                             rasqal_query_merge_triple_patterns,
                                             &modified);
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      fprintf(RASQAL_DEBUG_FH, "modified=%d after merge triples, query graph pattern now:\n  ", modified);
      rasqal_graph_pattern_print(query->query_graph_pattern, RASQAL_DEBUG_FH);
      fputs("\n", RASQAL_DEBUG_FH);
#endif
      if(rc) {
        modified = rc;
        break;
      }
      
      rc = rasqal_query_graph_pattern_visit2(query,
                                             rasqal_query_remove_empty_group_graph_patterns,
                                             &modified);
      
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      fprintf(RASQAL_DEBUG_FH, "modified=%d after remove empty groups, query graph pattern now:\n  ", modified);
      rasqal_graph_pattern_print(query->query_graph_pattern, RASQAL_DEBUG_FH);
      fputs("\n", RASQAL_DEBUG_FH);
#endif
      if(rc) {
        modified = rc;
        break;
      }
      
      rc = rasqal_query_graph_pattern_visit2(query, 
                                             rasqal_query_merge_graph_patterns,
                                             &modified);

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      fprintf(RASQAL_DEBUG_FH, "modified=%d  after merge graph patterns, query graph pattern now:\n  ", modified);
      rasqal_graph_pattern_print(query->query_graph_pattern, RASQAL_DEBUG_FH);
      fputs("\n", RASQAL_DEBUG_FH);
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

    /* Build scope hierarchy before variable analysis for warning detection */
    rc = rasqal_query_build_scope_hierarchy(query);
    if(rc)
      goto done;

    rc = rasqal_query_build_variables_use(query, projection);
    if(rc)
      goto done;

    /* Remove unbound variables from SELECT * projection */
    if(projection && projection->wildcard) {
      raptor_sequence* vars_seq = rasqal_projection_get_variables_sequence(projection);
      if(vars_seq) {
        int j;
        int vars_size = raptor_sequence_size(vars_seq);

        /* Remove unbound variables from the end to avoid index issues */
        for(j = vars_size - 1; j >= 0; j--) {
          rasqal_variable* v = (rasqal_variable*)raptor_sequence_get_at(vars_seq, j);
          if(!rasqal_query_variable_is_bound(query, v)) {
            raptor_sequence_delete_at(vars_seq, j);
            rasqal_free_variable(v);
          }
        }
      }
    }

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    fprintf(RASQAL_DEBUG_FH, "modified=%d  after filter variable scope, query graph pattern now:\n  ", modified);
    rasqal_graph_pattern_print(query->query_graph_pattern, RASQAL_DEBUG_FH);
    fputs("\n", RASQAL_DEBUG_FH);
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
  rasqal_graph_pattern_print(src_gp, RASQAL_DEBUG_FH);
  fprintf(RASQAL_DEBUG_FH, "\nto graph pattern #%d\n  ", dest_gp->gp_index);
  rasqal_graph_pattern_print(dest_gp, RASQAL_DEBUG_FH);
  fprintf(RASQAL_DEBUG_FH, "\nboth of operator %s\n",
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


static void
rasqal_query_print_scope_variable_usage(FILE* fh, rasqal_query* query)
{
  int i;
  int size;
  int bound_count = 0, unbound_count = 0;
  
  fprintf(fh, "========================================\n");
  fprintf(fh, "Rasqal Query Scope Variable Analysis\n");
  fprintf(fh, "========================================\n\n");
  
  /* Print all variables and their binding status */
  size = rasqal_variables_table_get_total_variables_count(query->vars_table);
  fprintf(fh, "Variable Binding Analysis (%d total variables):\n", size);
  fprintf(fh, "----------------------------------------\n");
  
  for(i = 0; i < size; i++) {
    rasqal_variable* v = rasqal_variables_table_get(query->vars_table, i);
    if(v) {
      int is_bound = rasqal_query_variable_is_bound(query, v);
      fprintf(fh, "  %-20s : %-6s (offset=%d)\n",
              v->name ? (const char*)v->name : "NULL",
              is_bound ? "BOUND" : "UNBOUND", v->offset);
      if(is_bound) bound_count++;
      else unbound_count++;
    }
  }

  fprintf(fh, "\nSummary: %d bound, %d unbound\n\n", bound_count, unbound_count);
  
  /* Print scope hierarchy if available */
  if(query->query_graph_pattern && query->query_graph_pattern->execution_scope) {
    fprintf(fh, "Scope Hierarchy Analysis:\n");
    fprintf(fh, "========================\n");
    rasqal_query_scope_print_variable_analysis(fh, query->query_graph_pattern->execution_scope, 0);
  } else {
    fprintf(fh, "No scope hierarchy available for analysis.\n");
  }

  fprintf(fh, "\n========================================\n");
}

static void
rasqal_query_scope_print_variable_analysis(FILE* fh, rasqal_query_scope* scope, int depth)
{
  int i;
  char indent[64];
  const char* scope_type_name = "UNKNOWN";
  
  if(!scope) return;
  
  /* Create indentation with tree-like structure */
  memset(indent, ' ', sizeof(indent));
  for(i = 0; i < depth * 4 && i < 60; i += 4) {
    indent[i] = '|';
    indent[i+1] = ' ';
    indent[i+2] = ' ';
    indent[i+3] = ' ';
  }
  if(depth > 0) {
    indent[(depth-1) * 4] = (depth == 1) ? '+' : '|';
    indent[(depth-1) * 4 + 1] = '-';
    indent[(depth-1) * 4 + 2] = ' ';
  }
  indent[depth * 4] = '\0';

  /* Get scope type name */
  switch(scope->scope_type) {
    case RASQAL_QUERY_SCOPE_TYPE_ROOT: scope_type_name = "ROOT"; break;
    case RASQAL_QUERY_SCOPE_TYPE_EXISTS: scope_type_name = "EXISTS"; break;
    case RASQAL_QUERY_SCOPE_TYPE_NOT_EXISTS: scope_type_name = "NOT_EXISTS"; break;
    case RASQAL_QUERY_SCOPE_TYPE_MINUS: scope_type_name = "MINUS"; break;
    case RASQAL_QUERY_SCOPE_TYPE_UNION: scope_type_name = "UNION"; break;
    case RASQAL_QUERY_SCOPE_TYPE_SUBQUERY: scope_type_name = "SUBQUERY"; break;
    case RASQAL_QUERY_SCOPE_TYPE_GROUP: scope_type_name = "GROUP"; break;
    default: break;
  }

  fprintf(fh, "%s%s_%d (%s, id=%d)\n",
          indent,
          scope->scope_name ? scope->scope_name : "SCOPE",
          scope->scope_id,
          scope_type_name,
          scope->scope_id);

  /* Print local variables with details */
  if(scope->local_vars) {
    int var_count = rasqal_variables_table_get_total_variables_count(scope->local_vars);
    fprintf(fh, "%s|  +- Local Variables: %d\n", depth > 0 ? indent : "", var_count);
    if(var_count > 0) {
      for(i = 0; i < var_count; i++) {
        rasqal_variable* v = rasqal_variables_table_get(scope->local_vars, i);
        if(v) {
          fprintf(fh, "%s|  |  \\- %s (offset=%d)\n",
                  depth > 0 ? indent : "",
                  v->name ? (const char*)v->name : "NULL", v->offset);
        }
      }
    }
  } else {
    fprintf(fh, "%s|  +- Local Variables: 0\n", depth > 0 ? indent : "");
  }

  /* Print owned triples with more detail */
  if(scope->owned_triples) {
    int triple_count = raptor_sequence_size(scope->owned_triples);
    fprintf(fh, "%s|  +- Owned Triples: %d\n", depth > 0 ? indent : "", triple_count);
    if(triple_count > 0 && triple_count <= 5) { /* Show details for small numbers */
      for(i = 0; i < triple_count; i++) {
        rasqal_triple* t = (rasqal_triple*)raptor_sequence_get_at(scope->owned_triples, i);
        if(t) {
          fprintf(fh, "%s|  |  \\- [%d] %s %s %s\n",
                  depth > 0 ? indent : "", i,
                  t->subject ? (t->subject->string ? (const char*)t->subject->string : "?var") : "NULL",
                  t->predicate ? (t->predicate->string ? (const char*)t->predicate->string : "?var") : "NULL",
                  t->object ? (t->object->string ? (const char*)t->object->string : "?var") : "NULL");
        }
      }
    }
  } else {
    fprintf(fh, "%s|  +- Owned Triples: 0\n", depth > 0 ? indent : "");
  }

  /* Print visible variables (computed) */
  if(scope->visible_vars) {
    int visible_count = rasqal_variables_table_get_total_variables_count(scope->visible_vars);
    fprintf(fh, "%s|  +- Visible Variables: %d (local + inherited)\n",
            depth > 0 ? indent : "", visible_count);
  }

  /* Print child scopes with better formatting */
  if(scope->child_scopes) {
    int child_count = raptor_sequence_size(scope->child_scopes);
    fprintf(fh, "%s|  \\- Child Scopes: %d\n", depth > 0 ? indent : "", child_count);

    for(i = 0; i < child_count; i++) {
      rasqal_query_scope* child_scope = (rasqal_query_scope*)raptor_sequence_get_at(scope->child_scopes, i);
      rasqal_query_scope_print_variable_analysis(fh, child_scope, depth + 1);
    }
  } else {
    fprintf(fh, "%s\\- No child scopes\n", depth > 0 ? indent : "");
  }
}


/*
 * rasqal_query_variable_is_bound:
 * @query: #rasqal_query object
 * @v: variable
 * 
 * INTERNAL - Test if a variable is bound in the query
 *
 * Return value: non-0 if bound
 */
int
rasqal_query_variable_is_bound(rasqal_query* query, rasqal_variable* v)
{
  /* Check if variable is bound anywhere in the query by walking all graph patterns */
  if(!query->query_graph_pattern)
    return 0;

  return rasqal_graph_pattern_variable_bound_below(query->query_graph_pattern, v);
}


/*
 * rasqal_query_variable_is_bound_in_scope:
 * @query: #rasqal_query object
 * @v: variable
 * @scope: scope to check variable binding in
 *
 * INTERNAL - Test if a variable is bound in a specific scope using
 * scope-aware variable resolution
 *
 * Return value: non-0 if bound
 */
int
rasqal_query_variable_is_bound_in_scope(rasqal_query* query,
                                       rasqal_variable* v,
                                       rasqal_query_scope* scope)
{
  if(!query || !v || !scope)
    return 0;

  /* Check if variable is bound in this scope's local variables */
  if(scope->local_vars && rasqal_variables_table_get_by_name(scope->local_vars, RASQAL_VARIABLE_TYPE_NORMAL, (const unsigned char*)v->name))
    return 1;

  /* Recursively check child scopes */
  if(scope->child_scopes) {
    int i;
    int size = raptor_sequence_size(scope->child_scopes);
    for(i = 0; i < size; i++) {
      rasqal_query_scope* child_scope = (rasqal_query_scope*)raptor_sequence_get_at(scope->child_scopes, i);
      if(rasqal_query_variable_is_bound_in_scope(query, v, child_scope))
        return 1;
    }
  }

  return 0;
}


/*
 * rasqal_query_get_variable_in_graph_pattern:
 * @query: #rasqal_query object
 * @name: variable name to lookup
 * @gp: graph pattern to search in
 *
 * INTERNAL - Get variable visible in specific graph pattern using
 * scope-aware variable resolution
 *
 * Return value: variable if found, NULL otherwise
 */
rasqal_variable*
rasqal_query_get_variable_in_graph_pattern(rasqal_query* query,
                                          const char* name,
                                          rasqal_graph_pattern* gp)
{
  rasqal_variable_lookup_context ctx;
  rasqal_variable* found;

  if(!query || !name || !gp || !gp->execution_scope)
    return NULL;

  /* Initialize scope-aware lookup context */
  memset(&ctx, 0, sizeof(ctx));
  ctx.current_scope = gp->execution_scope;
  ctx.search_scope = gp->execution_scope;
  ctx.query = query;

  /* Use appropriate search flags based on scope type */
  if(gp->execution_scope->scope_type == RASQAL_QUERY_SCOPE_TYPE_GROUP)
    ctx.search_flags = RASQAL_VAR_SEARCH_LOCAL_ONLY;  /* Isolated */
  else
    ctx.search_flags = RASQAL_VAR_SEARCH_INHERIT_PARENT;  /* Inherit */

  /* Use scope-aware variable resolution */
  found = rasqal_resolve_variable_with_scope(name, &ctx);
  return found;
}


/*
 * rasqal_query_build_scope_hierarchy:
 * @query: query object
 *
 * INTERNAL - Create scope hierarchy for graph patterns
 *
 * This function creates a hierarchical scope structure for the query's
 * graph patterns, ensuring proper variable isolation and inheritance.
 * Each graph pattern gets assigned an execution scope that defines
 * its variable visibility boundaries.
 *
 * Return value: non-0 on failure
 */
int
rasqal_query_build_scope_hierarchy(rasqal_query* query)
{
  rasqal_query_scope* root_scope;
  int rc = 0;



  if(!query || !query->query_graph_pattern)
    return 0;

  /* Create root scope for the entire query */
  root_scope = rasqal_new_query_scope(query, RASQAL_QUERY_SCOPE_TYPE_ROOT, NULL);
  if(!root_scope) {
    RASQAL_DEBUG1("Failed to create root scope\n");
    return 1;
  }

  fprintf(stderr, "DEBUG: Created root scope: %s\n", root_scope->scope_name);

  /* Assign root scope to the main query graph pattern */
  query->query_graph_pattern->execution_scope = root_scope;

  /* Create scopes for all graph patterns in the query */
  if(rasqal_query_build_scope_hierarchy_recursive(query, query->query_graph_pattern, root_scope)) {
    RASQAL_DEBUG1("Failed to build scope hierarchy recursively\n");
    return 1;
  }

  RASQAL_DEBUG1("Created complete scope hierarchy\n");

  return rc;
}


/**
 * rasqal_query_build_scope_hierarchy_recursive:
 * @query: query object
 * @gp: current graph pattern
 * @parent_scope: parent scope (or NULL for root)
 *
 * INTERNAL - Recursively build scope hierarchy for graph patterns
 *
 * This function walks through the graph pattern tree and creates
 * appropriate scopes for each pattern, establishing parent-child
 * relationships for proper variable isolation.
 *
 * Return value: non-0 on failure
 */
static int
rasqal_query_build_scope_hierarchy_recursive(rasqal_query* query,
                                            rasqal_graph_pattern* gp,
                                            rasqal_query_scope* parent_scope)
{
  rasqal_query_scope* current_scope;
  int rc = 0;
  int i;

  if(!gp)
    return 0;

  /* Create scope for this graph pattern */
  if(gp->op == RASQAL_GRAPH_PATTERN_OPERATOR_GROUP) {
    if(!parent_scope) {
      /* Root group pattern - create root scope */
      current_scope = rasqal_new_query_scope(query, RASQAL_QUERY_SCOPE_TYPE_ROOT, NULL);
      if(!current_scope) {
        RASQAL_DEBUG1("Failed to create root scope\n");
        return 1;
      }

    } else {
      /* Nested group pattern - create isolated scope */
      current_scope = rasqal_new_query_scope(query, RASQAL_QUERY_SCOPE_TYPE_GROUP, NULL);
      if(!current_scope) {
        RASQAL_DEBUG1("Failed to create group scope\n");
        return 1;
      }
      

      
      /* Group scopes do NOT inherit from parent - they are isolated */
      /* This is the key for bind10: nested patterns can't see outer variables */
    }
  } else {
    /* Other patterns inherit from parent scope */
    current_scope = parent_scope;
    RASQAL_DEBUG3("Using parent scope: %s for graph pattern %d\n", 
                  parent_scope ? parent_scope->scope_name : "NULL", gp->gp_index);
  }

  /* Assign scope to this graph pattern */
  gp->execution_scope = current_scope;
  


  /* Recursively process sub-graph patterns */
  if(gp->graph_patterns) {
    for(i = 0; i < raptor_sequence_size(gp->graph_patterns); i++) {
      rasqal_graph_pattern* sub_gp;
      
      sub_gp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      if(sub_gp) {
        rc = rasqal_query_build_scope_hierarchy_recursive(query, sub_gp, current_scope);
        if(rc)
          return rc;
      }
    }
  }

  return rc;
}

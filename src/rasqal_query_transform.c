/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_query_transform.c - Rasqal query transformations
 *
 * Copyright (C) 2004-2008, David Beckett http://www.dajobe.org/
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


int
rasqal_query_expand_triple_qnames(rasqal_query* rq)
{
  int i;

  if(!rq->triples)
    return 0;
  
  /* expand qnames in triples */
  for(i=0; i< raptor_sequence_size(rq->triples); i++) {
    rasqal_triple* t=(rasqal_triple*)raptor_sequence_get_at(rq->triples, i);
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
  for(i=0; i< raptor_sequence_size(seq); i++) {
    rasqal_triple* t=(rasqal_triple*)raptor_sequence_get_at(seq, i);
    if(rasqal_literal_has_qname(t->subject) ||
       rasqal_literal_has_qname(t->predicate) ||
       rasqal_literal_has_qname(t->object))
      return 1;
  }

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
rasqal_graph_pattern_constraints_has_qname(rasqal_graph_pattern* gp) 
{
  int i;
  
  /* check for qnames in sub graph patterns */
  if(gp->graph_patterns) {
    /* check for constraint qnames in rasqal_graph_patterns */
    for(i=0; i < raptor_sequence_size(gp->graph_patterns); i++) {
      rasqal_graph_pattern *sgp;
      sgp=(rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
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
rasqal_query_expand_graph_pattern_constraints_qnames(rasqal_query *rq,
                                                     rasqal_graph_pattern* gp)
{
  int i;
  
  /* expand qnames in sub graph patterns */
  if(gp->graph_patterns) {
    /* check for constraint qnames in rasqal_graph_patterns */
    for(i=0; i < raptor_sequence_size(gp->graph_patterns); i++) {
      rasqal_graph_pattern *sgp;
      sgp=(rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
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
  
  v=rasqal_new_variable_typed(rq, 
                              RASQAL_VARIABLE_TYPE_ANONYMOUS,
                              (unsigned char*)l->string, NULL);
  /* rasqal_new_variable_typed took ownership of the l->string name.
   * Set to NULL to prevent double delete. */
  l->string=NULL;
  
  if(!v)
    return 1; /* error */

  /* Convert the blank node literal into a variable literal */
  l->type=RASQAL_LITERAL_VARIABLE;
  l->value.variable=v;

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
  int rc=1;
  raptor_sequence *s=rq->triples;
  
  for(i=0; i < raptor_sequence_size(s); i++) {
    rasqal_triple* t=(rasqal_triple*)raptor_sequence_get_at(s, i);
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

  rc=0;

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
rasqal_query_expand_wildcards(rasqal_query* rq)
{
  int i;
  int size;
  
  if(rq->verb != RASQAL_QUERY_VERB_SELECT || !rq->wildcard)
    return 0;
  
  /* If 'SELECT *' was given, make the selects be a list of all variables */
  rq->selects=raptor_new_sequence(NULL, (raptor_sequence_print_handler*)rasqal_variable_print);
  if(!rq->selects)
    return 1;
  
  size=rasqal_variables_table_get_named_variables_count(rq->vars_table);
  for(i=0; i < size; i++) {
    rasqal_variable* v=rasqal_variables_table_get(rq->vars_table, i);
    if(raptor_sequence_push(rq->selects, v))
      return 1;
  }

  rq->select_variables_count=size;

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
  int modified=0;
  int size;
  raptor_sequence* seq=rq->selects;
  raptor_sequence* new_seq;
  
  if(!seq)
    return 1;

  size=raptor_sequence_size(seq);
  if(!size)
    return 0;
  
  new_seq=raptor_new_sequence(NULL, (raptor_sequence_print_handler*)rasqal_variable_print);
  if(!new_seq)
    return 1;
  
#if RASQAL_DEBUG > 1
  RASQAL_DEBUG1("bound variables before deduping: "); 
  raptor_sequence_print(rq->selects, DEBUG_FH);
  fputs("\n", DEBUG_FH); 
#endif

  for(i=0; i < size; i++) {
    int j;
    rasqal_variable *v;
    int warned=0;
    
    v=(rasqal_variable*)raptor_sequence_get_at(seq, i);
    if(!v)
      continue;

    for(j=0; j < i; j++) {
      rasqal_variable *v2;
      v2=(rasqal_variable*)raptor_sequence_get_at(seq, j);
      
      if(v == v2) {
        if(!warned) {
          rasqal_log_error_simple(rq->world, RAPTOR_LOG_LEVEL_WARNING,
                                  &rq->locator,
                                  "Variable %s duplicated in SELECT.", 
                                  v->name);
          warned=1;
        }
      }
    }
    if(!warned) {
      raptor_sequence_push(new_seq, v);
      modified=1;
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
    rq->select_variables_count=raptor_sequence_size(rq->selects);
  } else
    raptor_free_sequence(new_seq);

  return 0;
}


static RASQAL_INLINE void
rasqal_query_graph_pattern_build_declared_in_variable(rasqal_query* query,
                                                      rasqal_variable *v,
                                                      int col)
{
  if(!v)
    return;
  
  if(query->variables_declared_in[v->offset] < 0)
    query->variables_declared_in[v->offset]=col;
}


/**
 * rasqal_query_graph_pattern_build_declared_in:
 * @query: the #rasqal_query to find the variables in
 * @gp: graph pattern to use
 *
 * INTERNAL - Mark where variables are first declared in a graph_pattern.
 * 
 **/
static void
rasqal_query_graph_pattern_build_declared_in(rasqal_query* query,
                                             rasqal_graph_pattern *gp)
{
  int col;
      
  if(gp->graph_patterns) {
    int i;

    for(i=0; i < raptor_sequence_size(gp->graph_patterns); i++) {
      rasqal_graph_pattern *sgp;
      sgp=(rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      rasqal_query_graph_pattern_build_declared_in(query, sgp);
    }
  }

  if(!gp->triples)
    return;
    
  for(col=gp->start_column; col <= gp->end_column; col++) {
    rasqal_triple *t;
    t=(rasqal_triple*)raptor_sequence_get_at(gp->triples, col);

    rasqal_query_graph_pattern_build_declared_in_variable(query,
                                                          rasqal_literal_as_variable(t->subject),
                                                          col);
    rasqal_query_graph_pattern_build_declared_in_variable(query,
                                                          rasqal_literal_as_variable(t->predicate),
                                                          col);
    rasqal_query_graph_pattern_build_declared_in_variable(query,
                                                          rasqal_literal_as_variable(t->object),
                                                          col);
    if(t->origin)
      rasqal_query_graph_pattern_build_declared_in_variable(query,
                                                            rasqal_literal_as_variable(t->origin),
                                                            col);
  }
  
}


/**
 * rasqal_query_build_declared_in:
 * @query: the #rasqal_query to find the variables in
 *
 * INTERNAL - Record the triple columns where variables are first declared.
 * and warn variables that are selected but not defined.
 *
 * The query->variables_declared_in array is used in
 * rasqal_engine_graph_pattern_init() when trying to figure out which
 * parts of a triple pattern need to bind to a variable: only the first
 * reference to it.
 *
 * Return value: non-0 on failure
 **/
static int
rasqal_query_build_declared_in(rasqal_query* query) 
{
  int i;
  rasqal_graph_pattern *gp=query->query_graph_pattern;
  int size=rasqal_variables_table_get_total_variables_count(query->vars_table) ;

  if(!gp)
    /* It is not an error for a query to have no graph patterns */
    return 0;

  query->variables_declared_in=(int*)RASQAL_CALLOC(intarray, size+1, sizeof(int));
  if(!query->variables_declared_in)
    return 1;

  for(i=0; i < size; i++)
    query->variables_declared_in[i]= -1;
  
  rasqal_query_graph_pattern_build_declared_in(query, gp);

  /* check declared in only for named variables since only they can
   * appear in SELECT $vars 
   */
  size=rasqal_variables_table_get_named_variables_count(query->vars_table) ;
  for(i=0; i< size; i++) {
    int column=query->variables_declared_in[i];
    rasqal_variable *v;

    v=rasqal_variables_table_get(query->vars_table, i);

    if(column >= 0) {
#if RASQAL_DEBUG > 1
      RASQAL_DEBUG4("Variable %s (%d) was declared in column %d\n",
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
  int rc=1;

  if(!query->triples)
    goto done;
  
  /* turn SELECT $a, $a into SELECT $a - editing query->selects */
  if(query->selects) {
    if(rasqal_query_remove_duplicate_select_vars(query))
      goto done;
  }

  if(query->query_graph_pattern) {
    /* This query prepare processing requires a query graph pattern.
     * Not the case for a legal query like 'DESCRIBE <uri>'
     */

    /* create query->variables_declared_in to find triples where a variable
     * is first used and look for variables selected that are not used
     */
    if(rasqal_query_build_declared_in(query))
      goto done;
  }

  rasqal_engine_query_fold_expressions(query);

  rc=0;

  done:
  return rc;
}



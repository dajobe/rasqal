/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_graph_pattern.c - Rasqal graph pattern class
 *
 * $Id$
 *
 * Copyright (C) 2004-2005, David Beckett http://purl.org/net/dajobe/
 * Institute for Learning and Research Technology http://www.ilrt.bristol.ac.uk/
 * University of Bristol, UK http://www.bristol.ac.uk/
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


/**
 * rasqal_new_graph_pattern - create a new graph pattern object
 * @query: &rasqal_graph_pattern query object
 * 
 * Return value: a new &rasqal_graph_pattern object or NULL on failure
 **/
rasqal_graph_pattern*
rasqal_new_graph_pattern(rasqal_query* query) {
  rasqal_graph_pattern* gp=(rasqal_graph_pattern*)RASQAL_CALLOC(rasqal_graph_pattern, sizeof(rasqal_graph_pattern), 1);

  gp->query=query;

  return gp;
}


/**
 * rasqal_new_graph_pattern_from_triples - create a new graph pattern object over triples
 * @query: &rasqal_graph_pattern query object
 * @triples: triples sequence containing the graph pattern
 * @start_column: first triple in the pattern
 * @end_column: last triple in the pattern
 * @flags: enum &rasqal_triple_flags such as RASQAL_TRIPLE_FLAGS_OPTIONAL
 * 
 * Return value: a new &rasqal_graph_pattern object or NULL on failure
 **/
rasqal_graph_pattern*
rasqal_new_graph_pattern_from_triples(rasqal_query* query,
                                      raptor_sequence *triples,
                                      int start_column, int end_column,
                                      int flags)
{
  rasqal_graph_pattern* gp=rasqal_new_graph_pattern(query);

  rasqal_graph_pattern_add_triples(gp, 
                                   triples, start_column, end_column, flags);
  return gp;
}


/**
 * rasqal_new_graph_pattern_from_sequence - create a new graph pattern from a sequence of graph_patterns
 * @query: &rasqal_graph_pattern query object
 * @graph_patterns: sequence containing the graph patterns
 * @flags: enum &rasqal_triple_flags such as RASQAL_TRIPLE_FLAGS_OPTIONAL
 * 
 * Return value: a new &rasqal_graph_pattern object or NULL on failure
 **/
rasqal_graph_pattern*
rasqal_new_graph_pattern_from_sequence(rasqal_query* query,
                                       raptor_sequence *graph_patterns, 
                                       int flags)
{
  rasqal_graph_pattern* gp;

  if(raptor_sequence_size(graph_patterns)==1) {
    /* fold sequence of 1 graph_pattern */
    RASQAL_DEBUG1("Folding sequence of 1 graph_patterns\n");
    gp=(rasqal_graph_pattern*)raptor_sequence_pop(graph_patterns);
    raptor_free_sequence(graph_patterns);
  } else {
    gp=rasqal_new_graph_pattern(query);
    gp->graph_patterns=graph_patterns;
    gp->flags=flags;
  }

  gp->column= -1;
  gp->optional_graph_pattern= -1;
  gp->finished=0;
  gp->matches_returned=0;

  return gp;
}


/**
 * rasqal_graph_pattern_add_triples - add triples to a graph pattern object
 * @graph_pattern: &rasqal_graph_pattern object
 * @triples: triples sequence containing the graph pattern
 * @start_column: first triple in the pattern
 * @end_column: last triple in the pattern
 * @flags: enum &rasqal_triple_flags such as RASQAL_TRIPLE_FLAGS_OPTIONAL
 * 
 * Return value: a new &rasqal_graph_pattern object or NULL on failure
 **/
void
rasqal_graph_pattern_add_triples(rasqal_graph_pattern* gp,
                                 raptor_sequence* triples,
                                 int start_column, int end_column,
                                 int flags)
{
  gp->triples=triples;
  gp->column= -1;
  gp->start_column=start_column;
  gp->end_column=end_column;
  gp->optional_graph_pattern= -1;
  gp->finished=0;
  gp->matches_returned=0;
  gp->flags=flags;
}


/**
 * rasqal_free_graph_pattern - free a graph pattern object
 * @gp: &rasqal_graph_pattern object
 * 
 **/
void
rasqal_free_graph_pattern(rasqal_graph_pattern* gp)
{
  if(gp->triple_meta) {
    while(gp->column >= gp->start_column) {
      rasqal_triple_meta *m=&gp->triple_meta[gp->column - gp->start_column];
      rasqal_reset_triple_meta(m);
      gp->column--;
    }
    RASQAL_FREE(rasqal_triple_meta, gp->triple_meta);
    gp->triple_meta=NULL;
  }

  if(gp->graph_patterns)
    raptor_free_sequence(gp->graph_patterns);
  
  if(gp->constraints_expression) {
    rasqal_free_expression(gp->constraints_expression);
    if(gp->constraints)
      raptor_free_sequence(gp->constraints);
  } else if(gp->constraints) {
    int i;
    
    /* free rasqal_expressions that are normally assembled into an
     * expression tree pointed at query->constraints_expression
     * when query construction succeeds.
     */
    for(i=0; i< raptor_sequence_size(gp->constraints); i++) {
      rasqal_expression* e=(rasqal_expression*)raptor_sequence_get_at(gp->constraints, i);
      rasqal_free_expression(e);
    }
    raptor_free_sequence(gp->constraints);
  }

  RASQAL_FREE(rasqal_graph_pattern, gp);
}


static int
rasqal_graph_pattern_order(const void *a, const void *b)
{
  rasqal_graph_pattern *gp_a=*(rasqal_graph_pattern**)a;
  rasqal_graph_pattern *gp_b=*(rasqal_graph_pattern**)b;

  return (gp_a->flags & RASQAL_PATTERN_FLAGS_OPTIONAL) -
         (gp_b->flags & RASQAL_PATTERN_FLAGS_OPTIONAL);
}


/**
 * rasqal_graph_pattern_init - initialise a graph pattern for execution
 * @gp &rasqal_graph_pattern object
 * 
 **/
void
rasqal_graph_pattern_init(rasqal_graph_pattern *gp)
{
  rasqal_query *query=gp->query;
  
  gp->optional_graph_pattern= -1;

  if(gp->graph_patterns) {
    int i;
    
    /* sort graph patterns, optional graph triples last */
    raptor_sequence_sort(gp->graph_patterns, rasqal_graph_pattern_order);
  
    for(i=0; i < raptor_sequence_size(gp->graph_patterns); i++) {
      rasqal_graph_pattern *sgp=(rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      rasqal_graph_pattern_init(sgp);
      
      if((sgp->flags & RASQAL_PATTERN_FLAGS_OPTIONAL) &&
         gp->optional_graph_pattern < 0)
        gp->optional_graph_pattern=i;
    }

  }
  
  if(gp->triples) {
    int triples_count=gp->end_column - gp->start_column+1;
    int i;
    
    gp->column=gp->start_column;
    if(gp->triple_meta) {
      /* reset any previous execution */
      rasqal_reset_triple_meta(gp->triple_meta);
      memset(gp->triple_meta, '\0', sizeof(rasqal_triple_meta)*triples_count);
    } else
      gp->triple_meta=(rasqal_triple_meta*)RASQAL_CALLOC(rasqal_triple_meta, sizeof(rasqal_triple_meta), triples_count);

    for(i=gp->start_column; i <= gp->end_column; i++) {
      rasqal_triple_meta *m=&gp->triple_meta[i - gp->start_column];
      rasqal_triple *t=(rasqal_triple*)raptor_sequence_get_at(gp->triples, i);
      rasqal_variable* v;

      m->parts=(rasqal_triple_parts)0;
      
      if((v=rasqal_literal_as_variable(t->subject)) &&
         query->variables_declared_in[v->offset] == i)
        m->parts= (rasqal_triple_parts)(m->parts | RASQAL_TRIPLE_SUBJECT);
      
      if((v=rasqal_literal_as_variable(t->predicate)) &&
         query->variables_declared_in[v->offset] == i)
        m->parts= (rasqal_triple_parts)(m->parts | RASQAL_TRIPLE_PREDICATE);
      
      if((v=rasqal_literal_as_variable(t->object)) &&
         query->variables_declared_in[v->offset] == i)
        m->parts= (rasqal_triple_parts)(m->parts | RASQAL_TRIPLE_OBJECT);

      if(t->origin &&
         (v=rasqal_literal_as_variable(t->origin)) &&
         query->variables_declared_in[v->offset] == i)
        m->parts= (rasqal_triple_parts)(m->parts | RASQAL_TRIPLE_ORIGIN);

      RASQAL_DEBUG4("Graph pattern %p Triple %d has parts %d\n",
                    gp, i, m->parts);
    }

  }

  gp->matched= 0;
  gp->finished= 0;
  gp->matches_returned= 0;
}


/**
 * rasqal_graph_pattern_adjust - Adjust the column in a graph pattern by the offset
 * @gp: &rasqal_graph_pattern graph pattern
 * @offset: adjustment
 * 
 **/
void
rasqal_graph_pattern_adjust(rasqal_graph_pattern* gp, int offset)
{
  gp->start_column += offset;
  gp->end_column += offset;
}


/**
 * rasqal_graph_pattern_add_constraint - Add a constraint expression to the graph_pattern
 * @query: &rasqal_graph_pattern query object
 * @expr: &rasqal_expression expr
 *
 * Return value: non-0 on failure
 **/
int
rasqal_graph_pattern_add_constraint(rasqal_graph_pattern* gp,
                                    rasqal_expression* expr)
{
  if(!gp->constraints)
    gp->constraints=raptor_new_sequence(NULL, (raptor_sequence_print_handler*)rasqal_expression_print);
  raptor_sequence_push(gp->constraints, (void*)expr);

  return 0;
}


/**
 * rasqal_graph_pattern_get_constraint_sequence - Get the sequence of constraints expressions in the query
 * @query: &rasqal_graph_pattern query object
 *
 * Return value: a &raptor_sequence of &rasqal_expression pointers.
 **/
raptor_sequence*
rasqal_graph_pattern_get_constraint_sequence(rasqal_graph_pattern* gp)
{
  return gp->constraints;
}


/**
 * rasqal_graph_pattern_get_constraint - Get a constraint in the sequence of constraint expressions in the query
 * @query: &rasqal_graph_pattern query object
 * @idx: index into the sequence (0 or larger)
 *
 * Return value: a &rasqal_expression pointer or NULL if out of the sequence range
 **/
rasqal_expression*
rasqal_graph_pattern_get_constraint(rasqal_graph_pattern* gp, int idx)
{
  if(!gp->constraints)
    return NULL;

  return (rasqal_expression*)raptor_sequence_get_at(gp->constraints, idx);
}


/**
 * rasqal_graph_pattern_print - Print a Rasqal graph_pattern in a debug format
 * @v: the &rasqal_graph_pattern object
 * @fh: the &FILE* handle to print to
 * 
 * The print debug format may change in any release.
 * 
 **/
void
rasqal_graph_pattern_print(rasqal_graph_pattern* gp, FILE* fh)
{
  fputs("graph_pattern(", fh);
  if(gp->triples) {
    int i;
    fputs("over triples[", fh);

    for(i=gp->start_column; i <= gp->end_column; i++) {
      rasqal_triple *t=(rasqal_triple*)raptor_sequence_get_at(gp->triples, i);
      rasqal_triple_print(t, fh);
      if(i < gp->end_column)
        fputs(", ", fh);
    }
    fputs("]", fh);
  }
  if(gp->graph_patterns) {
    fputs("over graph_patterns", fh);
    raptor_sequence_print(gp->graph_patterns, fh);
  }
  if(gp->constraints) {
    fprintf(fh, " with constraints: ");
    raptor_sequence_print(gp->constraints, fh);
  }
  if(gp->flags) {
    fputs(", flags=", fh);
    if(gp->flags & RASQAL_PATTERN_FLAGS_OPTIONAL)
      fputs("OPTIONAL", fh);
  }
  fputs(")", fh);
}



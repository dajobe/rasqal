/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_engine.c - Rasqal query engine
 *
 * $Id$
 *
 * Copyright (C) 2004, David Beckett http://purl.org/net/dajobe/
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


#if 0
/* FIXME not used */

typedef struct {
  int seen;
  int score;
#ifdef RASQAL_DEBUG
  int original_i;
#endif
} rq_ordering;


static rasqal_triple*
rasqal_query_order_triples_score(rasqal_query* query,
                                 rq_ordering* ord,
                                 int* position) {
  int i;
  rasqal_triple* nt=NULL;
  int triples_size=raptor_sequence_size(query->triples);
  int next_score=0;
  int next_i= -1;
  
#if RASQAL_DEBUG >2
  RASQAL_DEBUG1("scoring triples\n");
#endif

  for(i=0; i< triples_size; i++) {
    rasqal_triple* t=(rasqal_triple*)raptor_sequence_get_at(query->triples, i);
    rasqal_variable* v;

    if((v=rasqal_literal_as_variable(t->subject))) {
#if RASQAL_DEBUG >2
      RASQAL_DEBUG3("triple %i: has variable %s in subject\n", i, v->name);
#endif
      if(rasqal_query_has_variable(query, v->name))
        ord[i].score++;
    }

    if((v=rasqal_literal_as_variable(t->predicate))) {
#if RASQAL_DEBUG >2
      RASQAL_DEBUG3("triple %i: has variable %s in predicate\n", i, v->name);
#endif
      if(rasqal_query_has_variable(query, v->name))
        ord[i].score++;
    }

    if((v=rasqal_literal_as_variable(t->object))) {
#if RASQAL_DEBUG >2
      RASQAL_DEBUG3("triple %i: has variable %s in predicate\n", i, v->name);
#endif
      if(rasqal_query_has_variable(query, v->name))
        ord[i].score++;
    }

#if RASQAL_DEBUG >2
    RASQAL_DEBUG3("triple %i: score %d\n", i, ord[i].score);
#endif
  }

  /* Pick triples providing least result variables first */
  for(i=0; i< triples_size; i++) {
    rasqal_triple* t=(rasqal_triple*)raptor_sequence_get_at(query->triples, i);
    if(!ord[i].seen && (!nt || (ord[i].score < next_score))) {
#if RASQAL_DEBUG >2
      RASQAL_DEBUG4("chose triple %i with score %d, less than %d\n", 
             i, ord[i].score, next_score);
#endif
      nt=t;
      next_score=ord[i].score;
      next_i=i;
    }
  }

#if RASQAL_DEBUG >2
  RASQAL_DEBUG3("final choice triple %i with score %d\n", 
         next_i, next_score);
#endif
  *position=next_i;

  return nt;
}


int
rasqal_query_order_triples(rasqal_query* query) {
  int i;
  rq_ordering* ord;
  int triples_size=raptor_sequence_size(query->triples);
#ifdef RASQAL_DEBUG
  int ordering_changed=0;
#endif

  if(query->ordered_triples)
    raptor_free_sequence(query->ordered_triples);

  /* NOTE: Shared triple pointers with the query->triples - 
   * entries in the ordered_triples sequence are not freed 
   */
  query->ordered_triples=raptor_new_sequence(NULL, (raptor_sequence_print_handler*)rasqal_triple_print);

  if(triples_size == 1) {
    raptor_sequence_push(query->ordered_triples, 
                         raptor_sequence_get_at(query->triples, 0));
    return 0;
  }
  

  ord=(rq_ordering*)RASQAL_CALLOC(intarray, sizeof(rq_ordering), triples_size);
  if(!ord)
    return 1;

  for(i=0; i<triples_size; i++) {
    int original_i;
    rasqal_triple *nt =rasqal_query_order_triples_score(query, ord, &original_i);
    if(!nt)
      break;

    raptor_sequence_push(query->ordered_triples, nt);

    ord[original_i].seen=1;
#ifdef RASQAL_DEBUG
    ord[i].original_i=original_i;
    if(i != original_i)
      ordering_changed=1;
#endif
  }

#ifdef RASQAL_DEBUG
  if(ordering_changed) {
    RASQAL_DEBUG1("triple ordering changed\n");
    for(i=0; i<triples_size; i++)
      RASQAL_DEBUG3("triple %i: was %i\n", i, ord[i].original_i);
  } else
    RASQAL_DEBUG1("triple ordering was not changed\n");
    
#endif  

  RASQAL_FREE(ordarray, ord);

  return 0;
}

#endif


int
rasqal_engine_declare_prefix(rasqal_query *rq, rasqal_prefix *p)
{
  if(p->declared)
    return 0;
  
  if(raptor_namespaces_start_namespace_full(rq->namespaces, 
                                            p->prefix, 
                                            raptor_uri_as_string(p->uri),
                                            rq->prefix_depth))
    return 1;
  p->declared=1;
  rq->prefix_depth++;
  return 0;
}


int
rasqal_engine_undeclare_prefix(rasqal_query *rq, rasqal_prefix *prefix)
{
  if(!prefix->declared) {
    prefix->declared=1;
    return 0;
  }
  
  raptor_namespaces_end_for_depth(rq->namespaces, prefix->depth);
  return 0;
}


int
rasqal_engine_declare_prefixes(rasqal_query *rq) 
{
  int i;
  
  if(!rq->prefixes)
    return 0;
  
  for(i=0; i< raptor_sequence_size(rq->prefixes); i++) {
    rasqal_prefix* p=(rasqal_prefix*)raptor_sequence_get_at(rq->prefixes, i);
    if(rasqal_engine_declare_prefix(rq, p))
      return 1;
  }

  return 0;
}


int
rasqal_engine_expand_triple_qnames(rasqal_query* rq)
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
rasqal_engine_sequence_has_qname(raptor_sequence *seq) {
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
rasqal_engine_constraints_has_qname(rasqal_query* rq) 
{
  int i;
  
  if(!rq->constraints)
    return 0;
  
  /* check for qnames in constraint expressions */
  for(i=0; i<raptor_sequence_size(rq->constraints); i++) {
    rasqal_expression* e=(rasqal_expression*)raptor_sequence_get_at(rq->constraints, i);
    if(rasqal_expression_foreach(e, rasqal_expression_has_qname, rq))
      return 1;
  }

  return 0;
}


int
rasqal_engine_expand_constraints_qnames(rasqal_query* rq)
{
  int i;
  
  if(!rq->constraints)
    return 0;
  
  /* expand qnames in constraint expressions */
  for(i=0; i<raptor_sequence_size(rq->constraints); i++) {
    rasqal_expression* e=(rasqal_expression*)raptor_sequence_get_at(rq->constraints, i);
    if(rasqal_expression_foreach(e, rasqal_expression_expand_qname, rq))
      return 1;
  }

  return 0;
}


int
rasqal_engine_build_constraints_expression(rasqal_query* rq)
{
  rasqal_expression* newe=NULL;
  int i;
  
  if(!rq->constraints)
    return 0;
  
  for(i=raptor_sequence_size(rq->constraints)-1; i>=0 ; i--) {
    rasqal_expression* e=(rasqal_expression*)raptor_sequence_get_at(rq->constraints, i);
    if(!newe)
      newe=e;
    else
      /* must make a conjunction */
      newe=rasqal_new_2op_expression(RASQAL_EXPR_AND, e, newe);
  }
  rq->constraints_expression=newe;

  return 0;
}


int
rasqal_engine_assign_variables(rasqal_query* rq)
{
  int i;

  /* If 'SELECT *' was given, make the selects be a list of all variables */
  if(rq->select_all) {
    rq->selects=raptor_new_sequence(NULL, (raptor_sequence_print_handler*)rasqal_variable_print);
    
    for(i=0; i< rq->variables_count; i++)
      raptor_sequence_push(rq->selects, raptor_sequence_get_at(rq->variables_sequence, i));
  }

  if(rq->selects)
    rq->select_variables_count=raptor_sequence_size(rq->selects);

  if(rq->select_variables_count) {
    rq->variable_names=(const unsigned char**)RASQAL_MALLOC(cstrings,sizeof(const unsigned char*)*(rq->select_variables_count+1));
    rq->binding_values=(rasqal_literal**)RASQAL_MALLOC(rasqal_literals,sizeof(rasqal_literal*)*(rq->select_variables_count+1));
  }
  
  rq->variables=(rasqal_variable**)RASQAL_MALLOC(varrary, sizeof(rasqal_variable*)*rq->variables_count);
  rq->variables_declared_in=(rasqal_graph_pattern**)RASQAL_CALLOC(graphpatternarray, sizeof(rasqal_graph_pattern*), rq->variables_count+1);

  for(i=0; i< rq->variables_count; i++) {
    rq->variables[i]=(rasqal_variable*)raptor_sequence_get_at(rq->variables_sequence, i);
    if(i< rq->select_variables_count)
      rq->variable_names[i]=rq->variables[i]->name;
  }

  if(rq->variable_names) {
    rq->variable_names[rq->select_variables_count]=NULL;
    rq->binding_values[rq->select_variables_count]=NULL;
  }
  return 0;
}
  

/* static */
static rasqal_triples_source_factory Triples_Source_Factory;


/**
 * rasqal_set_triples_source_factory - Register the factory to return triple sources
 * @register_fn: registration function
 * @user_data: user data for registration
 * 
 * Registers the factory that returns triples sources.  Note that
 * there is only one of these per runtime. 
 *
 * The rasqal_triples_source_factory factory method new_triples_source is
 * called with the user data for some query and rasqal_triples_source.
 * 
 **/
RASQAL_API void
rasqal_set_triples_source_factory(void (*register_fn)(rasqal_triples_source_factory *factory), void* user_data) {
  Triples_Source_Factory.user_data=user_data;
  register_fn(&Triples_Source_Factory);
}


rasqal_triples_source*
rasqal_new_triples_source(rasqal_query *query) {
  rasqal_triples_source* rts;
  
  rts=(rasqal_triples_source*)RASQAL_CALLOC(rasqal_triples_source, sizeof(rasqal_triples_source), 1);
  if(!rts)
    return NULL;

  rts->user_data=RASQAL_CALLOC(user_data,
                               Triples_Source_Factory.user_data_size, 1);
  if(!rts->user_data) {
    RASQAL_FREE(rasqal_triples_source, rts);
    return NULL;
  }
  rts->query=query;

  if(Triples_Source_Factory.new_triples_source(query, 
                                               Triples_Source_Factory.user_data,
                                               rts->user_data, rts)) {
    RASQAL_FREE(user_data, rts->user_data);
    RASQAL_FREE(rasqal_triples_source, rts);
    return NULL;
  }
  
  return rts;
}


void
rasqal_free_triples_source(rasqal_triples_source *rts) {
  if(rts->user_data) {
    rts->free_triples_source(rts->user_data);
    RASQAL_FREE(user_data, rts->user_data);
    rts->user_data=NULL;
  }
  
  RASQAL_FREE(rasqal_triples_source, rts);
}


static int
rasqal_triples_source_triple_present(rasqal_triples_source *rts,
                                     rasqal_triple *t) {
  return rts->triple_present(rts, rts->user_data, t);
}


static rasqal_triples_match*
rasqal_new_triples_match(rasqal_query *query, void *user_data,
                         rasqal_triple_meta *m, rasqal_triple *t) {
  if(!query->triples_source)
    return NULL;
  
  return query->triples_source->new_triples_match(query->triples_source,
                                                  query->triples_source->user_data,
                                                  m, t);
}


static void
rasqal_free_triples_match(rasqal_triples_match* rtm) {
  rtm->finish(rtm, rtm->user_data);
  RASQAL_FREE(rasqal_triples_match, rtm);
}


/* methods */
static int
rasqal_triples_match_bind_match(struct rasqal_triples_match_s* rtm, 
                                rasqal_variable *bindings[4],
                                rasqal_triple_parts parts) {
  return rtm->bind_match(rtm,  rtm->user_data, bindings, parts);
}

static void
rasqal_triples_match_next_match(struct rasqal_triples_match_s* rtm) {
  rtm->next_match(rtm,  rtm->user_data);
}

static int
rasqal_triples_match_is_end(struct rasqal_triples_match_s* rtm) {
  return rtm->is_end(rtm,  rtm->user_data);
}



/**
 * rasqal_new_graph_pattern_from_triples - create a new graph pattern object over triples
 * @triples: triples sequence containing the graph pattern
 * @start_column: first triple in the pattern
 * @end_column: last triple in the pattern
 * @flags: enum &rasqal_triple_flags such as RASQAL_TRIPLE_FLAGS_OPTIONAL
 * 
 * Return value: a new &rasqal_graph-pattern object or NULL on failure
 **/
rasqal_graph_pattern*
rasqal_new_graph_pattern_from_triples(rasqal_query* query,
                                      raptor_sequence *triples,
                                      int start_column, int end_column,
                                      int flags)
{
  rasqal_graph_pattern* gp=(rasqal_graph_pattern*)RASQAL_CALLOC(rasqal_graph_pattern, sizeof(rasqal_graph_pattern), 1);

  gp->query=query;
  gp->triples=triples;
  gp->column= -1;
  gp->start_column=start_column;
  gp->end_column=end_column;
  gp->optional_graph_pattern= -1;
  gp->finished=0;
  gp->matches_returned=0;
  gp->flags=flags;

  return gp;
}


/**
 * rasqal_new_graph_pattern_from_sequence - create a new graph pattern from a sequence of graph_patterns
 * @flags: enum &rasqal_triple_flags such as RASQAL_TRIPLE_FLAGS_OPTIONAL
 * 
 * Return value: a new &rasqal_graph-pattern object or NULL on failure
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
    gp=(rasqal_graph_pattern*)RASQAL_CALLOC(rasqal_graph_pattern, sizeof(rasqal_graph_pattern), 1);
    
    gp->graph_patterns=graph_patterns;
    gp->flags=flags;
  }

  gp->query=query;
  gp->column= -1;
  gp->optional_graph_pattern= -1;
  gp->finished=0;
  gp->matches_returned=0;

  return gp;
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

      if(m->bindings[0] && (m->parts & RASQAL_TRIPLE_SUBJECT)) 
        rasqal_variable_set_value(m->bindings[0],  NULL);
      if(m->bindings[1] && (m->parts & RASQAL_TRIPLE_PREDICATE)) 
        rasqal_variable_set_value(m->bindings[1],  NULL);
      if(m->bindings[2] && (m->parts & RASQAL_TRIPLE_OBJECT)) 
        rasqal_variable_set_value(m->bindings[2],  NULL);
      if(m->bindings[3] && (m->parts & RASQAL_TRIPLE_ORIGIN)) 
        rasqal_variable_set_value(m->bindings[3],  NULL);

      if(m->triples_match) {
        rasqal_free_triples_match(m->triples_match);
        m->triples_match=NULL;
      }
      gp->column--;
    }
    RASQAL_FREE(rasqal_triple_meta, gp->triple_meta);
  }

  if(gp->graph_patterns)
    raptor_free_sequence(gp->graph_patterns);
  
  RASQAL_FREE(rasqal_graph_pattern, gp);
}


/**
 * rasqal_graph_pattern_init - initialise a graph pattern for execution
 * @gp &rasqal_graph-pattern object
 * 
 **/
void
rasqal_graph_pattern_init(rasqal_graph_pattern *gp) {
  rasqal_query *query=gp->query;
  
  if(gp->triples) {
    int triples_count=gp->end_column - gp->start_column+1;
    int i;
    
    gp->column=gp->start_column;
    if(gp->triple_meta)
      memset(gp->triple_meta, '\0', sizeof(rasqal_triple_meta)*triples_count);
    else
      gp->triple_meta=(rasqal_triple_meta*)RASQAL_CALLOC(rasqal_triple_meta, sizeof(rasqal_triple_meta), triples_count);

    for(i=gp->start_column; i <= gp->end_column; i++) {
      rasqal_triple_meta *m=&gp->triple_meta[i - gp->start_column];
      rasqal_triple *t=(rasqal_triple*)raptor_sequence_get_at(gp->triples, i);
      rasqal_variable* v;

      m->parts=0;
      
      if((v=rasqal_literal_as_variable(t->subject)) &&
         query->variables_declared_in[v->offset] == gp)
        m->parts |= RASQAL_TRIPLE_SUBJECT;
      
      if((v=rasqal_literal_as_variable(t->predicate)) &&
         query->variables_declared_in[v->offset] == gp)
        m->parts |= RASQAL_TRIPLE_PREDICATE;
      
      if((v=rasqal_literal_as_variable(t->object)) &&
         query->variables_declared_in[v->offset] == gp)
        m->parts |= RASQAL_TRIPLE_OBJECT;

      /* FIXME origin */

      RASQAL_DEBUG4("Graph pattern %p Triple %d has parts %d\n",
                    gp, i, m->parts);
    }

  }

  gp->optional_graph_pattern= -1;
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
  if(gp->flags) {
    fputs(", flags=", fh);
    if(gp->flags & RASQAL_PATTERN_FLAGS_OPTIONAL)
      fputs("OPTIONAL", fh);
  }
  fputs(")", fh);
}


/**
 * rasqal_query_build_declared_in - Mark where variables are first declared
 * @query; the &rasqal_query to find the variables in
 * 
 **/
static void
rasqal_query_build_declared_in(rasqal_query* query) 
{
  int i;

  for(i=0; i < raptor_sequence_size(query->graph_patterns); i++) {
    rasqal_graph_pattern *gp=(rasqal_graph_pattern*)raptor_sequence_get_at(query->graph_patterns, i);
    int col;
    
    if(!gp->triples)
      return;
    
    for(col=gp->start_column; col <= gp->end_column; col++) {
      rasqal_triple *t=(rasqal_triple*)raptor_sequence_get_at(gp->triples, col);
      rasqal_variable* v;

      if((v=rasqal_literal_as_variable(t->subject)) &&
         !query->variables_declared_in[v->offset])
        query->variables_declared_in[v->offset]=gp;

      if((v=rasqal_literal_as_variable(t->predicate)) &&
         !query->variables_declared_in[v->offset])
        query->variables_declared_in[v->offset]=gp;
      
      if((v=rasqal_literal_as_variable(t->object)) &&
         !query->variables_declared_in[v->offset])
        query->variables_declared_in[v->offset]=gp;

    }
    
  }

  for(i=0; i< query->variables_count; i++) {
    rasqal_variable *v=query->variables[i];
    rasqal_graph_pattern *gp=query->variables_declared_in[i];

    if(gp)
      RASQAL_DEBUG4("Variable %s (%d) was declared in graph pattern %p\n",
                    v->name, i, gp);
    else 
      rasqal_query_warning(query, 
                           "Variable %s was selected but is unused in query.\n", 
                           v->name);
  }


}


/*
 *
 * return: <0 failure, 0 end of results, >0 match
 */
static int
rasqal_graph_pattern_get_next_match(rasqal_query *query,
                                    rasqal_graph_pattern* gp) 
{
  int rc;

  if(gp->graph_patterns) {
    /* FIXME - sequence of graph_patterns not implemented, finish */
    RASQAL_DEBUG1("Failing query with sequence of graph_patterns\n");
    return 0;
  }
    
  while(gp->column >= gp->start_column) {
    rasqal_triple_meta *m=&gp->triple_meta[gp->column - gp->start_column];
    rasqal_triple *t=(rasqal_triple*)raptor_sequence_get_at(gp->triples,
                                                            gp->column);

    rc=1;

    if(!m) {
      /* error recovery - no match */
      gp->column--;
      rc= -1;
      return rc;
    } else if (t->flags & RASQAL_TRIPLE_FLAGS_EXACT) {
      /* exact triple match wanted */
      if(!rasqal_triples_source_triple_present(query->triples_source, t)) {
        /* failed */
        RASQAL_DEBUG2("exact match failed for column %d\n", gp->column);
        gp->column--;
      } else
        RASQAL_DEBUG2("exact match OK for column %d\n", gp->column);
    } else if(!m->triples_match) {
      /* Column has no triplesMatch so create a new query */
      m->triples_match=rasqal_new_triples_match(query, m, m, t);
      if(!m->triples_match) {
        rasqal_query_error(query, "Failed to make a triple match for column%d",
                           gp->column);
        /* failed to match */
        gp->column--;
        rc= -1;
        return rc;
      }
      RASQAL_DEBUG2("made new triplesMatch for column %d\n", gp->column);
    }


    if(m->triples_match) {
      if(rasqal_triples_match_is_end(m->triples_match)) {
        RASQAL_DEBUG2("end of triplesMatch for column %d\n", gp->column);

        if(m->bindings[0] && (m->parts & RASQAL_TRIPLE_SUBJECT)) 
          rasqal_variable_set_value(m->bindings[0],  NULL);
        if(m->bindings[1] && (m->parts & RASQAL_TRIPLE_PREDICATE)) 
          rasqal_variable_set_value(m->bindings[1],  NULL);
        if(m->bindings[2] && (m->parts & RASQAL_TRIPLE_OBJECT)) 
          rasqal_variable_set_value(m->bindings[2],  NULL);
        if(m->bindings[3] && (m->parts & RASQAL_TRIPLE_ORIGIN)) 
          rasqal_variable_set_value(m->bindings[3],  NULL);

        rasqal_free_triples_match(m->triples_match);
        m->triples_match=NULL;
        
        gp->column--;
        continue;
      }

      if(!rasqal_triples_match_bind_match(m->triples_match, m->bindings,
                                          m->parts))
        rc=0;

      rasqal_triples_match_next_match(m->triples_match);
      if(!rc)
        continue;
    }
    
    if(gp->column == gp->end_column) {
      /* Done all conjunctions */ 
      
      /* exact match, so column must have ended */
      if(t->flags & RASQAL_TRIPLE_FLAGS_EXACT)
        gp->column--;

      /* return with result (rc is 1) */
      return rc;
    } else if (gp->column >= gp->start_column)
      gp->column++;

  }

  if(gp->column < gp->start_column)
    rc=0;
  
  return rc;
}



/*
 * rasqal_engine_prepare - initialise the remainder of the query structures INTERNAL
 * Does not do any execution prepration - this is once-only stuff.
 */
int
rasqal_engine_prepare(rasqal_query *query) {
  int i;

  if(!query->triples)
    return 1;
  
  if(!query->variables) {
    /* Expand 'SELECT *' and create the query->variables array */
    rasqal_engine_assign_variables(query);

    rasqal_query_build_declared_in(query);
    
    rasqal_engine_build_constraints_expression(query);
  }

  for(i=0; i < raptor_sequence_size(query->triples); i++) {
    rasqal_triple *t=(rasqal_triple*)raptor_sequence_get_at(query->triples, i);
    
    t->flags |= RASQAL_TRIPLE_FLAGS_EXACT;
    if(rasqal_literal_as_variable(t->predicate) ||
       rasqal_literal_as_variable(t->subject) ||
       rasqal_literal_as_variable(t->object))
      t->flags &= ~RASQAL_TRIPLE_FLAGS_EXACT;
  }


  return 0;
}


static int rasqal_graph_pattern_order(const void *a, const void *b) {
  rasqal_graph_pattern *gp_a=(rasqal_graph_pattern*)a;
  rasqal_graph_pattern *gp_b=(rasqal_graph_pattern*)b;

  return (gp_a->flags & RASQAL_PATTERN_FLAGS_OPTIONAL) -
         (gp_b->flags & RASQAL_PATTERN_FLAGS_OPTIONAL);
}



int
rasqal_engine_execute_init(rasqal_query *query) {
  int i;
  
  if(!query->triples)
    return 1;
  
  if(!query->triples_source) {
    query->triples_source=rasqal_new_triples_source(query);
    if(!query->triples_source) {
      query->failed=1;
      rasqal_query_error(query, "Failed to make triples source.");
      return 1;
    }
  }

  /* sort graph patterns, optional graph triples last */
  raptor_sequence_sort(query->graph_patterns,
                       rasqal_graph_pattern_order);
  
  for(i=0; i < raptor_sequence_size(query->graph_patterns); i++) {
    rasqal_graph_pattern *gp=(rasqal_graph_pattern*)raptor_sequence_get_at(query->graph_patterns, i);
    if(gp) {
      rasqal_graph_pattern_init(gp);

      if((gp->flags & RASQAL_PATTERN_FLAGS_OPTIONAL) &&
         query->optional_graph_pattern < 0)
        query->optional_graph_pattern=i;
    } else {
      RASQAL_FATAL2("query graph patterns sequence has a NULL graph_pattern at entry %d\n", i);
    }
  }
  
  query->abort=0;
  query->result_count=0;
  query->finished=0;
  query->failed=0;
  
  if(raptor_sequence_size(query->graph_patterns))
    query->current_graph_pattern= 0;
  else
    /* FIXME - no graph patterns in query */
    query->current_graph_pattern= -1;
  
  return 0;
}


int
rasqal_engine_execute_finish(rasqal_query *query) {
  if(query->triples_source) {
    rasqal_free_triples_source(query->triples_source);
    query->triples_source=NULL;
  }

  return 0;
}


static void
rasqal_engine_move_to_graph_pattern(rasqal_query *query,
                                    int offset) {
  int i;
  
  RASQAL_DEBUG2("Moving to graph pattern %d\n", offset);
  query->current_graph_pattern=offset;

  if(query->optional_graph_pattern >=0) {
    if(query->current_graph_pattern == query->optional_graph_pattern) {
      RASQAL_DEBUG1("Moved to first optional graph pattern\n");
      for(i=query->current_graph_pattern; 
          i < raptor_sequence_size(query->graph_patterns); 
          i++) {
        rasqal_graph_pattern *gp2=(rasqal_graph_pattern*)raptor_sequence_get_at(query->graph_patterns, i);
        rasqal_graph_pattern_init(gp2);
      }
    }
    query->optional_graph_pattern_matches_count=0;
  }
}


/*
 *
 * return: <0 failure, 0 end of results, >0 match
 */
int
rasqal_engine_get_next_result(rasqal_query *query) {
  int graph_patterns_size;
  int rc=1;
  int i;
  
  if(query->failed)
    return -1;

  if(query->finished)
    return 0;

  if(!query->triples)
    return -1;

  graph_patterns_size=raptor_sequence_size(query->graph_patterns);
  if(!graph_patterns_size) {
    /* FIXME - no graph patterns in query - end results */
    query->finished=1;
    return 0;
  }

  for(i=0; i < graph_patterns_size; i++) {
    rasqal_graph_pattern *gp2=(rasqal_graph_pattern*)raptor_sequence_get_at(query->graph_patterns, i);
    if(query->optional_graph_pattern >= 0 &&
       i >= query->optional_graph_pattern)
      gp2->matched=0;
  }
  

  while(rc > 0) {
    rasqal_graph_pattern* gp=(rasqal_graph_pattern*)raptor_sequence_get_at(query->graph_patterns, query->current_graph_pattern);

    RASQAL_DEBUG3("Handling graph_pattern %d %s\n",
                  query->current_graph_pattern,
                  (gp->flags & RASQAL_PATTERN_FLAGS_OPTIONAL) ? "(OPTIONAL)" : "");

    if(gp->graph_patterns) {
      /* FIXME - sequence of graph_patterns not implemented, finish */
      RASQAL_DEBUG1("Failing query with sequence of graph_patterns\n");
      rc=0;
      break;
    }

    if(gp->finished) {
      if(!query->current_graph_pattern) {
        RASQAL_DEBUG1("Ended first graph pattern - finished\n");
        query->finished=1;
        return 0;
      }

      RASQAL_DEBUG2("Ended graph pattern %d, backtracking\n",
                    query->current_graph_pattern);

      /* backtrack optionals */
      rasqal_engine_move_to_graph_pattern(query, 
                                          query->current_graph_pattern-1);
      rc=1;
      continue;
    }
    

    /*  return: <0 failure, 0 end of results, >0 match */
    rc=rasqal_graph_pattern_get_next_match(query, gp);

    RASQAL_DEBUG3("Graph pattern %d returned %d\n",
                  query->current_graph_pattern, rc);

    /* count real matches */
    if(rc > 0) {
      gp->matched=1;
      if(gp->flags & RASQAL_PATTERN_FLAGS_OPTIONAL)
        query->optional_graph_pattern_matches_count++;
    } else
      gp->matched=0;

    if(rc < 0) {
      /* failure to match */

      /* optional always matches */
      if(!(gp->flags & RASQAL_PATTERN_FLAGS_OPTIONAL)) {
        RASQAL_DEBUG2("Non-optional Graph pattern %d failed to match\n",
                      query->current_graph_pattern);
        break;
      }

      RASQAL_DEBUG2("Optional graph pattern %d failed to match\n", 
                    query->current_graph_pattern);
      rc=1;
    }

    if(!rc) {
      /* end of graph_pattern results */

      if(gp->flags & RASQAL_PATTERN_FLAGS_OPTIONAL) {
        /* if this is not the last (optional graph pattern) in the
         * sequence, move on and continue 
         */
        RASQAL_DEBUG2("End of optionals graph pattern %d\n",
                      query->current_graph_pattern);

        /* Next time we get here, backtrack */
        gp->finished=1;

        if(query->current_graph_pattern < graph_patterns_size-1) {
          rasqal_engine_move_to_graph_pattern(query, 
                                              query->current_graph_pattern+1);
          rc=1;
          continue;
        } else {
          int mandatory_matches=0;
          int optional_matches=0;
          
          /* Last optional match ended.
           * If we got any non optional matches, then we have a result.
           */
          for(i=0; i < graph_patterns_size; i++) {
            rasqal_graph_pattern *gp2=(rasqal_graph_pattern*)raptor_sequence_get_at(query->graph_patterns, i);
            if(query->optional_graph_pattern >= 0 &&
               i >= query->optional_graph_pattern)
              optional_matches += gp2->matched;
            else
              mandatory_matches += gp2->matched;
          }


          RASQAL_DEBUG2("Graph pattern has %d matches returned\n", 
                        gp->matches_returned);

          RASQAL_DEBUG2("Found %d query optional graph pattern matches\n", 
                        query->optional_graph_pattern_matches_count);

          RASQAL_DEBUG3("Found %d mandatory matches, %d optional matches\n", 
                        mandatory_matches, optional_matches);

          if(optional_matches) {
            RASQAL_DEBUG1("Found some matches, returning a result\n");
            rc=1;
            break;
          } else if (gp->matches_returned) {
            RASQAL_DEBUG1("No matches this time, some earlier, backtracking\n");
            rasqal_engine_move_to_graph_pattern(query, 0);
            rc=1;
            continue;
          } else {
            RASQAL_DEBUG1("No non-optional matches, returning a final result\n");
            rc=1;
            break;
          }

        }
      }

      /* otherwise this is the end of the results */
      RASQAL_DEBUG2("End of non-optional graph pattern %d\n",
                    query->current_graph_pattern);

      break;
    }

    /* got all matches - check any constraints */
    if(query->constraints_expression) {
      rasqal_literal* result;
      int bresult=1; /* constraint succeeds */
      int error=0;

#ifdef RASQAL_DEBUG
      RASQAL_DEBUG1("constraint expression:\n");
      rasqal_expression_print(query->constraints_expression, stderr);
      fputc('\n', stderr);
#endif

      result=rasqal_expression_evaluate(query, query->constraints_expression);
      if(result) {
#ifdef RASQAL_DEBUG
        RASQAL_DEBUG1("constraint expression result:\n");
        rasqal_literal_print(result, stderr);
        fputc('\n', stderr);
#endif
        bresult=rasqal_literal_as_boolean(result, &error);
        if(error) {
          RASQAL_DEBUG1("constraint boolean expression returned error\n");
          rc= -1;
        } else
          RASQAL_DEBUG2("constraint boolean expression result: %d\n", bresult);
        rasqal_free_literal(result);
        rc=bresult;

        if(!rc) {
          /* Constraint failed so move to try next match */
          rc=1;
          continue;
        }
        
      } else {
        RASQAL_DEBUG1("constraint expression failed with error\n");
        rc= -1;
      }

    } /* end check for constraints */

    if(rc) {

      /* if this is a match but not the last graph pattern in the
       * sequence move to the next graph pattern
       */
      if(query->current_graph_pattern < graph_patterns_size-1) {
        RASQAL_DEBUG1("Not last graph pattern\n");
        rasqal_engine_move_to_graph_pattern(query,
                                            query->current_graph_pattern+1);
        rc=1;
        continue;
      }
      
      RASQAL_DEBUG1("Got solution\n");

      /* is the last graph pattern so we have a solution */
      break;
    }

  }
  
  if(!rc)
    query->finished=1;

  if(rc > 0) {
    for(i=0; i < graph_patterns_size; i++) {
      rasqal_graph_pattern *gp2=(rasqal_graph_pattern*)raptor_sequence_get_at(query->graph_patterns, i);
      if(gp2->matched)
        gp2->matches_returned++;
    }

    /* Got a valid result */
    query->result_count++;
#ifdef RASQAL_DEBUG
    RASQAL_DEBUG1("Returning solution[");
    for(i=0; i< query->select_variables_count; i++) {
      const unsigned char *name=query->variables[i]->name;
      rasqal_literal *value=query->variables[i]->value;
      
      if(i>0)
        fputs(", ", stderr);
      fprintf(stderr, "%s=", name);
      if(value)
        rasqal_literal_print(value, stderr);
      else
        fputs("NULL", stderr);
    }
    fputs("]\n", stderr);
#endif
  }
  
  return rc;
}


int
rasqal_engine_run(rasqal_query *query) {
  int rc=0;
  
  while(!query->finished) {
    if(query->abort)
      break;
    
    /* rc<0 error rc=0 end of results,  rc>0 finished */
    rc=rasqal_engine_get_next_result(query);
    if(rc<1)
      break;
    
    if(rc > 0) {
      /* matched ok, so print out the variable bindings */
#ifdef RASQAL_DEBUG
      RASQAL_DEBUG1("result: ");
      raptor_sequence_print(query->selects, stderr);
      fputc('\n', stderr);
#if 0
      RASQAL_DEBUG1("result as triples: ");
      raptor_sequence_print(query->triples, stderr);
      fputc('\n', stderr);
#endif
#endif
    }
    rc=0;
  }
  
  return rc;
}


void
rasqal_engine_assign_binding_values(rasqal_query *query) {
  int i;
  
  for(i=0; i< query->select_variables_count; i++)
    query->binding_values[i]=query->variables[i]->value;
}

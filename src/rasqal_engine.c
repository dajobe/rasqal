/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_engine.c - Rasqal query engine
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
rasqal_engine_sequence_has_qname(raptor_sequence *seq)
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
rasqal_engine_query_constraints_has_qname(rasqal_query* rq) 
{
  return rasqal_engine_graph_pattern_constraints_has_qname(rq->query_graph_pattern);
}


int
rasqal_engine_graph_pattern_constraints_has_qname(rasqal_graph_pattern* gp) 
{
  int i;
  
  /* check for qnames in sub graph patterns */
  if(gp->graph_patterns) {
    /* check for constraint qnames in rasqal_graph_patterns */
    for(i=0; i < raptor_sequence_size(gp->graph_patterns); i++) {
      rasqal_graph_pattern *sgp=(rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      if(rasqal_engine_graph_pattern_constraints_has_qname(sgp))
        return 1;
    }
  }

  if(!gp->constraints)
    return 0;
  
  /* check for qnames in constraint expressions */
  for(i=0; i<raptor_sequence_size(gp->constraints); i++) {
    rasqal_expression* e=(rasqal_expression*)raptor_sequence_get_at(gp->constraints, i);
    if(rasqal_expression_foreach(e, rasqal_expression_has_qname, gp))
      return 1;
  }

  return 0;
}


int
rasqal_engine_expand_graph_pattern_constraints_qnames(rasqal_query *rq,
                                                      rasqal_graph_pattern* gp)
{
  int i;
  
  /* expand qnames in sub graph patterns */
  if(gp->graph_patterns) {
    /* check for constraint qnames in rasqal_graph_patterns */
    for(i=0; i < raptor_sequence_size(gp->graph_patterns); i++) {
      rasqal_graph_pattern *sgp=(rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      if(rasqal_engine_expand_graph_pattern_constraints_qnames(rq, sgp))
        return 1;
    }
  }

  if(!gp->constraints)
    return 0;
  
  /* expand qnames in constraint expressions */
  for(i=0; i<raptor_sequence_size(gp->constraints); i++) {
    rasqal_expression* e=(rasqal_expression*)raptor_sequence_get_at(gp->constraints, i);
    if(rasqal_expression_foreach(e, rasqal_expression_expand_qname, rq))
      return 1;
  }

  return 0;
}


int
rasqal_engine_expand_query_constraints_qnames(rasqal_query *rq) 
{
  return rasqal_engine_expand_graph_pattern_constraints_qnames(rq, 
                                                               rq->query_graph_pattern);
}


int
rasqal_engine_build_constraints_expression(rasqal_graph_pattern* gp)
{
  rasqal_expression* newe=NULL;
  int i;
  
  /* build constraints in sub graph patterns */
  if(gp->graph_patterns) {

    for(i=0; i < raptor_sequence_size(gp->graph_patterns); i++) {
      rasqal_graph_pattern *sgp=(rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      if(rasqal_engine_build_constraints_expression(sgp))
        return 1;
    }
  }

  if(!gp->constraints)
    return 0;
  
  for(i=raptor_sequence_size(gp->constraints)-1; i>=0 ; i--) {
    rasqal_expression* e=(rasqal_expression*)raptor_sequence_get_at(gp->constraints, i);
    if(!newe)
      newe=e;
    else
      /* must make a conjunction */
      newe=rasqal_new_2op_expression(RASQAL_EXPR_AND, e, newe);
  }
  gp->constraints_expression=newe;

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

  /* If 'CONSTRUCT *' was given, make the constructs be all triples */
  if(rq->construct_all) {
    raptor_sequence *s;
    
    rq->constructs=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_triple, (raptor_sequence_print_handler*)rasqal_triple_print);
    s=((rasqal_query*)rq)->triples;

    for(i=0; i < raptor_sequence_size(s); i++) {
      rasqal_triple *t=(rasqal_triple*)raptor_sequence_get_at(s, i);
      raptor_sequence_push(rq->constructs, rasqal_new_triple_from_triple(t));
    }
  }

  if(rq->selects)
    rq->select_variables_count=raptor_sequence_size(rq->selects);

  if(rq->select_variables_count) {
    rq->variable_names=(const unsigned char**)RASQAL_MALLOC(cstrings,sizeof(const unsigned char*)*(rq->select_variables_count+1));
    rq->binding_values=(rasqal_literal**)RASQAL_MALLOC(rasqal_literals,sizeof(rasqal_literal*)*(rq->select_variables_count+1));
  }
  
  rq->variables=(rasqal_variable**)RASQAL_MALLOC(varrary, sizeof(rasqal_variable*)*rq->variables_count);
  rq->variables_declared_in=(int*)RASQAL_CALLOC(intarray, sizeof(int), rq->variables_count+1);

  for(i=0; i< rq->variables_count; i++) {
    rq->variables_declared_in[i]= -1;
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
  int rc=0;
  
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

  rc=Triples_Source_Factory.new_triples_source(query, 
                                               Triples_Source_Factory.user_data,
                                               rts->user_data, rts);
  if(rc) {
    query->failed=1;
    if(rc > 0) {
      rasqal_query_error(query, "Failed to make triples source.");
    } else {
      rasqal_query_error(query, "No data to query.");
    }
    RASQAL_FREE(user_data, rts->user_data);
    RASQAL_FREE(rasqal_triples_source, rts);
    return NULL;
  }
  
  return rts;
}


void
rasqal_free_triples_source(rasqal_triples_source *rts)
{
  if(rts->user_data) {
    rts->free_triples_source(rts->user_data);
    RASQAL_FREE(user_data, rts->user_data);
    rts->user_data=NULL;
  }
  
  RASQAL_FREE(rasqal_triples_source, rts);
}


static int
rasqal_triples_source_triple_present(rasqal_triples_source *rts,
                                     rasqal_triple *t)
{
  return rts->triple_present(rts, rts->user_data, t);
}


static rasqal_triples_match*
rasqal_new_triples_match(rasqal_query *query, void *user_data,
                         rasqal_triple_meta *m, rasqal_triple *t)
{
  if(!query->triples_source)
    return NULL;
  
  return query->triples_source->new_triples_match(query->triples_source,
                                                  query->triples_source->user_data,
                                                  m, t);
}


static void
rasqal_free_triples_match(rasqal_triples_match* rtm)
{
  rtm->finish(rtm, rtm->user_data);
  RASQAL_FREE(rasqal_triples_match, rtm);
}


/* methods */
static int
rasqal_triples_match_bind_match(struct rasqal_triples_match_s* rtm, 
                                rasqal_variable *bindings[4],
                                rasqal_triple_parts parts)
{
  return rtm->bind_match(rtm,  rtm->user_data, bindings, parts);
}

static void
rasqal_triples_match_next_match(struct rasqal_triples_match_s* rtm)
{
  rtm->next_match(rtm,  rtm->user_data);
}

static int
rasqal_triples_match_is_end(struct rasqal_triples_match_s* rtm)
{
  return rtm->is_end(rtm,  rtm->user_data);
}


int
rasqal_reset_triple_meta(rasqal_triple_meta* m)
{
  int resets=0;
  
  if(m->triples_match) {
    rasqal_free_triples_match(m->triples_match);
    m->triples_match=NULL;
  }

  if(m->bindings[0] && (m->parts & RASQAL_TRIPLE_SUBJECT)) {
    rasqal_variable_set_value(m->bindings[0],  NULL);
    resets++;
  }
  if(m->bindings[1] && (m->parts & RASQAL_TRIPLE_PREDICATE)) {
    rasqal_variable_set_value(m->bindings[1],  NULL);
    resets++;
  }
  if(m->bindings[2] && (m->parts & RASQAL_TRIPLE_OBJECT)) {
    rasqal_variable_set_value(m->bindings[2],  NULL);
    resets++;
  }
  if(m->bindings[3] && (m->parts & RASQAL_TRIPLE_ORIGIN)) {
    rasqal_variable_set_value(m->bindings[3],  NULL);
    resets++;
  }
  
  if(m->triples_match) {
    rasqal_free_triples_match(m->triples_match);
    m->triples_match=NULL;
  }

  return resets;
}


/**
 * rasqal_query_graph_pattern_build_declared_in - Mark where variables are first declared in a graph_pattern
 * @query; the &rasqal_query to find the variables in
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
      rasqal_graph_pattern *sgp=(rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      rasqal_query_graph_pattern_build_declared_in(query, sgp);
    }
  }

  if(!gp->triples)
    return;
    
  for(col=gp->start_column; col <= gp->end_column; col++) {
    rasqal_triple *t=(rasqal_triple*)raptor_sequence_get_at(gp->triples, col);
    rasqal_variable* v;
    
    if((v=rasqal_literal_as_variable(t->subject)) &&
       query->variables_declared_in[v->offset] < 0)
      query->variables_declared_in[v->offset]=col;
    
    if((v=rasqal_literal_as_variable(t->predicate)) &&
       query->variables_declared_in[v->offset] < 0)
      query->variables_declared_in[v->offset]=col;
    
    if((v=rasqal_literal_as_variable(t->object)) &&
       query->variables_declared_in[v->offset] < 0)
      query->variables_declared_in[v->offset]=col;
    
  }
  
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
  rasqal_graph_pattern *gp=query->query_graph_pattern;

  rasqal_query_graph_pattern_build_declared_in(query, gp);

  for(i=0; i< query->variables_count; i++) {
    rasqal_variable *v=query->variables[i];
    int column=query->variables_declared_in[i];

    if(column >= 0) {
#if RASQAL_DEBUG > 1
      RASQAL_DEBUG4("Variable %s (%d) was declared in column %d\n",
                    v->name, i, column);
#endif
    } else 
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
      int parts;
      
      if(rasqal_triples_match_is_end(m->triples_match)) {
        int resets=0;

        RASQAL_DEBUG2("end of triplesMatch for column %d\n", gp->column);
        resets=rasqal_reset_triple_meta(m);
        query->new_bindings_count-= resets;
        if(query->new_bindings_count < 0)
          query->new_bindings_count=0;

        gp->column--;
        continue;
      }

      if(m->parts) {
        parts=rasqal_triples_match_bind_match(m->triples_match, m->bindings,
                                              m->parts);
        RASQAL_DEBUG3("bind_match for column %d returned parts %d\n",
                      gp->column, parts);
        if(!parts)
          rc=0;
        if(parts & RASQAL_TRIPLE_SUBJECT)
          query->new_bindings_count++;
        if(parts & RASQAL_TRIPLE_PREDICATE)
          query->new_bindings_count++;
        if(parts & RASQAL_TRIPLE_OBJECT)
          query->new_bindings_count++;
        if(parts & RASQAL_TRIPLE_ORIGIN)
          query->new_bindings_count++;
      } else {
        RASQAL_DEBUG2("Nothing to bind_match for column %d\n", gp->column);
      }
      
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
rasqal_engine_prepare(rasqal_query *query)
{
  int i;

  if(!query->triples)
    return 1;
  
  if(!query->variables) {
    /* Expand 'SELECT *' and create the query->variables array */
    rasqal_engine_assign_variables(query);

    rasqal_query_build_declared_in(query);
    
    rasqal_engine_build_constraints_expression(query->query_graph_pattern);
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


int
rasqal_engine_execute_init(rasqal_query* query, rasqal_query_results *results) 
{
  rasqal_graph_pattern *gp;
  
  if(!query->triples)
    return 1;
  
  if(!query->triples_source) {
    query->triples_source=rasqal_new_triples_source(query);
    if(!query->triples_source) {
      query->failed=1;
      return 1;
    }
  }

  gp=query->query_graph_pattern;

  rasqal_graph_pattern_init(gp);
  
  query->abort=0;
  query->result_count=0;
  query->finished=0;
  query->failed=0;
  
  if(gp->graph_patterns && raptor_sequence_size(gp->graph_patterns))
    gp->current_graph_pattern= 0;
  else
    /* FIXME - no graph patterns in query */
    gp->current_graph_pattern= -1;
  
  return 0;
}


int
rasqal_engine_execute_finish(rasqal_query *query)
{
  if(query->triples_source) {
    rasqal_free_triples_source(query->triples_source);
    query->triples_source=NULL;
  }

  return 0;
}


static void
rasqal_engine_move_to_graph_pattern(rasqal_graph_pattern *gp,
                                    int delta)
{
  int graph_patterns_size=raptor_sequence_size(gp->graph_patterns);
  int i;
  
  if(gp->optional_graph_pattern  < 0 ) {
    gp->current_graph_pattern += delta;
    RASQAL_DEBUG3("Moved to graph pattern %d (delta %d)\n", 
                  gp->current_graph_pattern, delta);
    return;
  }
  
  /* Otherwise, there are optionals */

  if(delta > 0) {
    gp->current_graph_pattern++;
    if(gp->current_graph_pattern == gp->optional_graph_pattern) {
      RASQAL_DEBUG1("Moved to first optional graph pattern\n");
      for(i=gp->current_graph_pattern; i < graph_patterns_size; i++) {
        rasqal_graph_pattern *gp2=(rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
        rasqal_graph_pattern_init(gp2);
      }
      gp->max_optional_graph_pattern=graph_patterns_size-1;
    }
    gp->optional_graph_pattern_matches_count=0;
  } else {
    RASQAL_DEBUG1("Moving to previous graph pattern\n");

    if(gp->current_graph_pattern > gp->optional_graph_pattern) {
      rasqal_graph_pattern *gp2=(rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, gp->current_graph_pattern);
      rasqal_graph_pattern_init(gp2);
    }
    gp->current_graph_pattern--;
  }
}


typedef enum {
  STEP_UNKNOWN,
  STEP_SEARCHING,
  STEP_GOT_MATCH,
  STEP_FINISHED,
  STEP_ERROR,

  STEP_LAST=STEP_ERROR
} rasqal_engine_step;


static const char * rasqal_engine_step_names[STEP_LAST+1]={
  "<unknown>",
  "searching",
  "got match",
  "finished",
  "error"
};


static rasqal_engine_step
rasqal_engine_check_constraint(rasqal_query *query, rasqal_graph_pattern *gp)
{
  rasqal_engine_step step=STEP_SEARCHING;
  rasqal_literal* result;
  int bresult=1; /* constraint succeeds */
  int error=0;
    
#ifdef RASQAL_DEBUG
  RASQAL_DEBUG1("constraint expression:\n");
  rasqal_expression_print(gp->constraints_expression, stderr);
  fputc('\n', stderr);
#endif
    
  result=rasqal_expression_evaluate(query, gp->constraints_expression);
  if(!result)
    return STEP_ERROR;
  
#ifdef RASQAL_DEBUG
  RASQAL_DEBUG1("constraint expression result:\n");
  rasqal_literal_print(result, stderr);
  fputc('\n', stderr);
#endif
  bresult=rasqal_literal_as_boolean(result, &error);
  if(error) {
    RASQAL_DEBUG1("constraint boolean expression returned error\n");
    step= STEP_ERROR;
  } else
    RASQAL_DEBUG2("constraint boolean expression result: %d\n", bresult);
  rasqal_free_literal(result);

  if(!bresult)
    /* Constraint failed so move to try next match */
    return STEP_SEARCHING;
  
  return STEP_GOT_MATCH;
}


static rasqal_engine_step
rasqal_engine_do_step(rasqal_query *query,
                      rasqal_graph_pattern *outergp, rasqal_graph_pattern *gp)
{
  int graph_patterns_size=raptor_sequence_size(outergp->graph_patterns);
  rasqal_engine_step step=STEP_SEARCHING;
  int rc;
  
  /*  return: <0 failure, 0 end of results, >0 match */
  rc=rasqal_graph_pattern_get_next_match(query, gp);
  
  RASQAL_DEBUG3("Graph pattern %d returned %d\n",
                outergp->current_graph_pattern, rc);
  
  /* no matches is always a failure */
  if(rc < 0)
    return STEP_FINISHED;
  
  if(!rc) {
    /* otherwise this is the end of the results */
    RASQAL_DEBUG2("End of non-optional graph pattern %d\n",
                  outergp->current_graph_pattern);
    
    return STEP_FINISHED;
  }


  if(gp->constraints_expression) {
    step=rasqal_engine_check_constraint(query, gp);
    if(step != STEP_GOT_MATCH)
      return step;
  }


  /* got match */
  RASQAL_DEBUG1("Got match\n");
  gp->matched=1;
    
  /* if this is a match but not the last graph pattern in the
   * sequence move to the next graph pattern
   */
  if(outergp->current_graph_pattern < graph_patterns_size-1) {
    RASQAL_DEBUG1("Not last graph pattern\n");
    rasqal_engine_move_to_graph_pattern(outergp, +1);
    return STEP_SEARCHING;
  }
  
  return STEP_GOT_MATCH;
}


static rasqal_engine_step
rasqal_engine_do_optional_step(rasqal_query *query, 
                               rasqal_graph_pattern *outergp,
                               rasqal_graph_pattern *gp)
{
  int graph_patterns_size=raptor_sequence_size(outergp->graph_patterns);
  rasqal_engine_step step=STEP_SEARCHING;
  int rc;
  
  if(gp->finished) {
    if(!outergp->current_graph_pattern) {
      step=STEP_FINISHED;
      RASQAL_DEBUG1("Ended first graph pattern - finished\n");
      query->finished=1;
      return STEP_FINISHED;
    }
    
    RASQAL_DEBUG2("Ended graph pattern %d, backtracking\n",
                  outergp->current_graph_pattern);
    
    /* backtrack optionals */
    rasqal_engine_move_to_graph_pattern(outergp, -1);
    return STEP_SEARCHING;
  }
  
  
  /*  return: <0 failure, 0 end of results, >0 match */
  rc=rasqal_graph_pattern_get_next_match(query, gp);
  
  RASQAL_DEBUG3("Graph pattern %d returned %d\n",
                outergp->current_graph_pattern, rc);
  
  /* count all optional matches */
  if(rc > 0)
    outergp->optional_graph_pattern_matches_count++;

  if(rc < 0) {
    /* optional always matches */
    RASQAL_DEBUG2("Optional graph pattern %d failed to match, continuing\n", 
                  outergp->current_graph_pattern);
    step=STEP_SEARCHING;
  }
  
  if(!rc) {
    int i;
    int mandatory_matches=0;
    int optional_matches=0;
    
    /* end of graph_pattern results */
    step=STEP_FINISHED;
    
    /* if this is not the last (optional graph pattern) in the
     * sequence, move on and continue 
     */
    RASQAL_DEBUG2("End of optionals graph pattern %d\n",
                  outergp->current_graph_pattern);

    gp->matched=0;
    
    /* Next time we get here, backtrack */
    gp->finished=1;
    
    if(outergp->current_graph_pattern < outergp->max_optional_graph_pattern) {
      RASQAL_DEBUG1("More optionals graph patterns to search\n");
      rasqal_engine_move_to_graph_pattern(outergp, +1);
      return STEP_SEARCHING;
    }

    outergp->max_optional_graph_pattern--;
    RASQAL_DEBUG2("Max optional graph patterns lowered to %d\n",
                  outergp->max_optional_graph_pattern);
    
    /* Last optional match ended.
     * If we got any non optional matches, then we have a result.
     */
    for(i=0; i < graph_patterns_size; i++) {
      rasqal_graph_pattern *gp2=(rasqal_graph_pattern*)raptor_sequence_get_at(outergp->graph_patterns, i);
      if(outergp->optional_graph_pattern >= 0 &&
         i >= outergp->optional_graph_pattern)
        optional_matches += gp2->matched;
      else
        mandatory_matches += gp2->matched;
    }
    
    
    RASQAL_DEBUG2("Optional graph pattern has %d matches returned\n", 
                  outergp->matches_returned);
    
    RASQAL_DEBUG2("Found %d query optional graph pattern matches\n", 
                  outergp->optional_graph_pattern_matches_count);
    
    RASQAL_DEBUG3("Found %d mandatory matches, %d optional matches\n", 
                  mandatory_matches, optional_matches);
    RASQAL_DEBUG2("Found %d new binds\n", query->new_bindings_count);
    
    if(optional_matches) {
      RASQAL_DEBUG1("Found some matches, returning a result\n");
      return STEP_GOT_MATCH;
    }

    if(gp->matches_returned) { 
      if(!outergp->current_graph_pattern) {
        RASQAL_DEBUG1("No matches this time and first graph pattern was optional, finished\n");
        return STEP_FINISHED;
      }

      RASQAL_DEBUG1("No matches this time, some earlier, backtracking\n");
      rasqal_engine_move_to_graph_pattern(outergp, -1);
      return STEP_SEARCHING;
    }


    if(query->new_bindings_count > 0) {
      RASQAL_DEBUG2("%d new bindings, returning a result\n",
                    query->new_bindings_count);
      return STEP_GOT_MATCH;
    }
    RASQAL_DEBUG1("no new bindings, continuing searching\n");
    return STEP_SEARCHING;
  }

  
  if(gp->constraints_expression) {
    step=rasqal_engine_check_constraint(query, gp);
    if(step != STEP_GOT_MATCH) {
      /* The constraint failed or we have an error - no bindings count */
      query->new_bindings_count=0;
      return step;
    }
  }


  /* got match */
   
 /* if this is a match but not the last graph pattern in the
  * sequence move to the next graph pattern
  */
 if(outergp->current_graph_pattern < graph_patterns_size-1) {
   RASQAL_DEBUG1("Not last graph pattern\n");
   rasqal_engine_move_to_graph_pattern(outergp, +1);
   return STEP_SEARCHING;
 }
 
 /* is the last graph pattern so we have a solution */

  RASQAL_DEBUG1("Got match\n");
  gp->matched=1;

  return STEP_GOT_MATCH;
}


/*
 *
 * return: <0 failure, 0 end of results, >0 match
 */
int
rasqal_engine_get_next_result(rasqal_query *query)
{
  int graph_patterns_size;
  rasqal_engine_step step;
  int i;
  rasqal_graph_pattern *outergp;
  
  
  if(query->failed)
    return -1;

  if(query->finished)
    return 0;

  if(!query->triples)
    return -1;

  outergp=query->query_graph_pattern;
  
  graph_patterns_size=raptor_sequence_size(outergp->graph_patterns);
  if(!graph_patterns_size) {
    /* FIXME - no graph patterns in query - end results */
    query->finished=1;
    return 0;
  }

  query->new_bindings_count=0;

  step=STEP_SEARCHING;
  while(step == STEP_SEARCHING) {
    rasqal_graph_pattern* gp=(rasqal_graph_pattern*)raptor_sequence_get_at(outergp->graph_patterns, outergp->current_graph_pattern);
    int values_returned=0;
    int optional_step;
    
    RASQAL_DEBUG3("Handling graph_pattern %d %s\n",
                  outergp->current_graph_pattern,
                  (gp->flags & RASQAL_PATTERN_FLAGS_OPTIONAL) ? "(OPTIONAL)" : "");

    if(gp->graph_patterns) {
      /* FIXME - sequence of graph_patterns not implemented, finish */
      RASQAL_DEBUG1("Failing query with sequence of graph_patterns\n");
      step=STEP_FINISHED;
      break;
    }

    gp->matched=0;
    optional_step=(gp->flags & RASQAL_PATTERN_FLAGS_OPTIONAL);
    
    if(optional_step)
      step=rasqal_engine_do_optional_step(query, outergp, gp);
    else
      step=rasqal_engine_do_step(query, outergp, gp);

    RASQAL_DEBUG2("Returned step is %s\n",
                  rasqal_engine_step_names[step]);

    /* Count actual bound values */
    for(i=0; i< query->select_variables_count; i++) {
      if(query->variables[i]->value)
        values_returned++;
    }
    RASQAL_DEBUG2("Solution binds %d values\n", values_returned);
    RASQAL_DEBUG2("New bindings %d\n", query->new_bindings_count);

    if(!values_returned && optional_step &&
       step != STEP_FINISHED && step != STEP_SEARCHING) {
      RASQAL_DEBUG1("An optional pass set no bindings, continuing searching\n");
      step=STEP_SEARCHING;
    }

  }


  RASQAL_DEBUG3("Ending with step %s and graph pattern %d\n",
                rasqal_engine_step_names[step],
                outergp->current_graph_pattern);
  
  
  if(step != STEP_GOT_MATCH)
    query->finished=1;

  if(step == STEP_GOT_MATCH) {
    for(i=0; i < graph_patterns_size; i++) {
      rasqal_graph_pattern *gp2=(rasqal_graph_pattern*)raptor_sequence_get_at(outergp->graph_patterns, i);
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

  return (step == STEP_GOT_MATCH);
}


int
rasqal_engine_run(rasqal_query *query)
{
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
rasqal_engine_assign_binding_values(rasqal_query *query)
{
  int i;
  
  for(i=0; i< query->select_variables_count; i++)
    query->binding_values[i]=query->variables[i]->value;
}

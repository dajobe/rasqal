/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_engine.c - Rasqal query engine
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

#define DEBUG_FH stderr



/* local types */

/* The execution data here is a sequence of
 * rasqal_graph_pattern_data execution data of size
 * query->graph_pattern_count with each rasqal_graph_pattern_data
 */
typedef struct {
  rasqal_query* query;
  rasqal_query_results* query_results;
                          
  raptor_sequence* seq;

  rasqal_triples_source* triples_source;

  /* New variables bound from during the current 'next result' run */
  int new_bindings_count;

  /* Source of rows that are filling the query result */
  rasqal_rowsource* rowsource;

  /* how many results already found (for get_row to check limit/offset) */
  int result_count;
} rasqal_engine_execution_data;


typedef enum {
  STEP_UNKNOWN,
  STEP_SEARCHING,
  STEP_GOT_MATCH,
  STEP_FINISHED,
  STEP_ERROR,

  STEP_LAST=STEP_ERROR
} rasqal_engine_step;


#ifdef RASQAL_DEBUG
static const char * rasqal_engine_step_names[STEP_LAST+1]={
  "<unknown>",
  "searching",
  "got match",
  "finished",
  "error"
};
#endif



/* local prototypes */
static rasqal_engine_step rasqal_engine_check_constraint(rasqal_query *query, rasqal_graph_pattern *gp);
static int rasqal_engine_graph_pattern_init(rasqal_engine_execution_data* execution_data, rasqal_graph_pattern *gp);


/**
 * rasqal_set_triples_source_factory:
 * @world: rasqal_world object
 * @register_fn: registration function
 * @user_data: user data for registration
 *
 * Register the factory to return triple sources.
 * 
 * Registers the factory that returns triples sources.  Note that
 * there is only one of these per runtime. 
 *
 * The rasqal_triples_source_factory factory method new_triples_source is
 * called with the user data for some query and rasqal_triples_source.
 *
 **/
RASQAL_EXTERN_C
void
rasqal_set_triples_source_factory(rasqal_world* world, void (*register_fn)(rasqal_triples_source_factory *factory), void* user_data)
{
  world->triples_source_factory.user_data=user_data;
  register_fn(&world->triples_source_factory);
}


/**
 * rasqal_new_triples_source:
 * @query_results: query results to initialize for
 *
 * Createa a new triples source
 *
 * Return value: a new triples source or NULL on failure
 */
static rasqal_triples_source*
rasqal_new_triples_source(rasqal_query_results* query_results)
{
  rasqal_query* query = rasqal_query_results_get_query(query_results);
  rasqal_triples_source_factory* rtsf=&query->world->triples_source_factory;
  rasqal_triples_source* rts;
  int rc=0;
  
  rts=(rasqal_triples_source*)RASQAL_CALLOC(rasqal_triples_source, 1, sizeof(rasqal_triples_source));
  if(!rts)
    return NULL;

  rts->user_data=RASQAL_CALLOC(user_data, 1,
                               rtsf->user_data_size);
  if(!rts->user_data) {
    RASQAL_FREE(rasqal_triples_source, rts);
    return NULL;
  }
  rts->query=query;

  rc=rtsf->new_triples_source(query, 
                              rtsf->user_data,
                              rts->user_data, rts);
  if(rc) {
    query_results->failed=1;
    query->failed=1;
    if(rc > 0) {
      rasqal_log_error_simple(query->world, RAPTOR_LOG_LEVEL_ERROR,
                              &query->locator,
                              "Failed to make triples source.");
    } else {
      rasqal_log_error_simple(query->world, RAPTOR_LOG_LEVEL_ERROR,
                              &query->locator,
                              "No data to query.");
    }
    RASQAL_FREE(user_data, rts->user_data);
    RASQAL_FREE(rasqal_triples_source, rts);
    return NULL;
  }
  
  return rts;
}


static void
rasqal_free_triples_source(rasqal_triples_source *rts)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(rts, rasqal_triples_source);
  
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


static void
rasqal_free_triples_match(rasqal_triples_match* rtm)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(rtm, rasqal_triples_match);
  
  rtm->finish(rtm, rtm->user_data);
  RASQAL_FREE(rasqal_triples_match, rtm);
}


static rasqal_triples_match*
rasqal_new_triples_match(rasqal_engine_execution_data* execution_data,
                         void *user_data,
                         rasqal_triple_meta *m, rasqal_triple *t)
{
  rasqal_triples_match* rtm;

  if(!execution_data->triples_source)
    return NULL;

  rtm=(rasqal_triples_match *)RASQAL_CALLOC(rasqal_triples_match, 1,
                                            sizeof(rasqal_triples_match));
  if(rtm) {
    rtm->world = execution_data->query->world;
    if(execution_data->triples_source->init_triples_match(rtm,
                                                          execution_data->triples_source,
                                                          execution_data->triples_source->user_data,
                                                          m, t)) {
      rasqal_free_triples_match(rtm);
      rtm=NULL;
    }
  }

  return rtm;
}


/* methods */
static int
rasqal_triples_match_bind_match(struct rasqal_triples_match_s* rtm, 
                                rasqal_variable *bindings[4],
                                rasqal_triple_parts parts)
{
  return rtm->bind_match(rtm, rtm->user_data, bindings, parts);
}


static void
rasqal_triples_match_next_match(struct rasqal_triples_match_s* rtm)
{
  rtm->next_match(rtm, rtm->user_data);
}


static int
rasqal_triples_match_is_end(struct rasqal_triples_match_s* rtm)
{
  return rtm->is_end(rtm, rtm->user_data);
}


/**
 * rasqal_reset_triple_meta:
 * @m: Triple pattern metadata
 * 
 * INTERNAL - reset the metadata associated with a triple pattern
 * 
 * Return value: number of parts of the triple that were reset (0..4)
 **/
static int
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

  m->executed=0;
  
  return resets;
}


typedef struct {
  rasqal_graph_pattern* gp;
  
  /* An array of items, one per triple in the pattern graph */
  rasqal_triple_meta* triple_meta;

  /* Executing column in the current graph pattern */
  int column;

  /* first graph_pattern in sequence with flags RASQAL_TRIPLE_FLAGS_OPTIONAL */
  int optional_graph_pattern;

  /* current position in the sequence */
  int current_graph_pattern;

  /* Count of all optional matches for the current mandatory matches */
  int optional_graph_pattern_matches_count;

  /* Number of matches returned */
  int matches_returned;

  /* true when this graph pattern matched last time */
  int matched;

  /* true when an optional graph pattern finished last time round */
  int finished;

  /* Max optional graph pattern allowed so far to stop backtracking
   * going over old graph patterns
   */
  int max_optional_graph_pattern;

} rasqal_engine_gp_data;


static rasqal_engine_gp_data*
rasqal_new_engine_gp_data(rasqal_graph_pattern* gp) 
{
  rasqal_engine_gp_data* gp_data;
  
  gp_data=(rasqal_engine_gp_data*)RASQAL_CALLOC(rasqal_engine_gp_data, 1,
                                                sizeof(rasqal_engine_gp_data));
  if(!gp_data)
    return NULL;
  
  gp_data->gp=gp;
  
  gp_data->optional_graph_pattern= -1;
  gp_data->matches_returned=0;
  gp_data->column= -1;

  return gp_data;
}


static void
rasqal_free_gp_data(rasqal_engine_gp_data* gp_data)
{
  rasqal_graph_pattern* gp;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN(gp_data, rasqal_engine_gp_data);
  
  gp=gp_data->gp;
  if(gp_data->triple_meta) {
    if(gp) {
      while(gp_data->column >= gp->start_column) {
        rasqal_triple_meta *m;
        m=&gp_data->triple_meta[gp_data->column - gp->start_column];
        rasqal_reset_triple_meta(m);
        gp_data->column--;
      }
    }
    RASQAL_FREE(rasqal_triple_meta, gp_data->triple_meta);
    gp_data->triple_meta=NULL;
  }

  RASQAL_FREE(rasqal_engine_gp_data, gp_data);
  return;
}


/**
 * rasqal_engine_group_graph_pattern_get_next_match:
 * @query_results: Query results to execute
 * @gp: group graph pattern
 *
 * INTERNAL - Get the next match in a group graph pattern
 *
 * return: <0 failure, 0 end of results, >0 match
 */
static int
rasqal_engine_group_graph_pattern_get_next_match(rasqal_engine_execution_data* execution_data,
                                                 rasqal_graph_pattern* gp)
{
  rasqal_query* query=execution_data->query;
#if 0
  rasqal_engine_gp_data* gp_data;

  gp_data=(rasqal_engine_gp_data*)raptor_sequence_get_at(execution_data->seq, 
                                                         gp->gp_index);
#endif

  /* FIXME - sequence of graph_patterns not implemented, finish */
  query->failed=1;
  rasqal_log_error_simple(query->world, RAPTOR_LOG_LEVEL_ERROR,
                          &query->locator,
                          "Graph pattern %s operation is not implemented yet. Ending query execution.", 
                          rasqal_graph_pattern_operator_as_string(gp->op));
  
  RASQAL_DEBUG1("Failing query with sequence of graph_patterns\n");
  return 0;
}


/**
 * rasqal_engine_triple_graph_pattern_get_next_match:
 * @query_results: Query results to execute
 * @gp: graph pattern to use
 *
 * INTERNAL - Get the next match in a triple graph pattern
 *
 * return: <0 failure, 0 end of results, >0 match
 */
static int
rasqal_engine_triple_graph_pattern_get_next_match(rasqal_engine_execution_data* execution_data,
                                                  rasqal_graph_pattern* gp) 
{
  rasqal_query* query;
  rasqal_query_results* query_results;
  int rc=0;
  rasqal_engine_gp_data* gp_data;
  
  query = execution_data->query;
  query_results = execution_data->query_results;
  gp_data=(rasqal_engine_gp_data*)raptor_sequence_get_at(execution_data->seq, 
                                                         gp->gp_index);

  while(gp_data->column >= gp->start_column) {
    rasqal_triple_meta *m;
    rasqal_triple *t;

    m=&gp_data->triple_meta[gp_data->column - gp->start_column];
    t=(rasqal_triple*)raptor_sequence_get_at(gp->triples, gp_data->column);

    rc=1;

    if(!m) {
      /* error recovery - no match */
      gp_data->column--;
      rc= -1;
      return rc;
    }
    
    if(m->executed) {
      RASQAL_DEBUG2("triplesMatch already executed in column %d\n", 
                    gp_data->column);
      gp_data->column--;
      continue;
    }
      
    if (m->is_exact) {
      /* exact triple match wanted */

      if(!rasqal_triples_source_triple_present(execution_data->triples_source, t)) {
        /* failed */
        RASQAL_DEBUG2("exact match failed for column %d\n", gp_data->column);
        gp_data->column--;
      }
#ifdef RASQAL_DEBUG
      else
        RASQAL_DEBUG2("exact match OK for column %d\n", gp_data->column);
#endif

      RASQAL_DEBUG2("end of exact triplesMatch for column %d\n", 
                    gp_data->column);
      m->executed=1;
      
    } else {
      /* triple pattern match wanted */
      int parts;

      if(!m->triples_match) {
        /* Column has no triplesMatch so create a new query */
        m->triples_match=rasqal_new_triples_match(execution_data, m, m, t);
        if(!m->triples_match) {
          query->failed=1;
          rasqal_log_error_simple(query->world, RAPTOR_LOG_LEVEL_ERROR,
                                  &query->locator,
                                  "Failed to make a triple match for column%d",
                                  gp_data->column);
          /* failed to match */
          gp_data->column--;
          rc= -1;
          return rc;
        }
        RASQAL_DEBUG2("made new triplesMatch for column %d\n", gp_data->column);
      }


      if(rasqal_triples_match_is_end(m->triples_match)) {
        int resets=0;

        RASQAL_DEBUG2("end of pattern triplesMatch for column %d\n",
                      gp_data->column);
        m->executed=1;

        resets=rasqal_reset_triple_meta(m);
        execution_data->new_bindings_count-= resets;
        if(execution_data->new_bindings_count < 0)
          execution_data->new_bindings_count=0;

        gp_data->column--;
        continue;
      }

      if(m->parts) {
        parts=rasqal_triples_match_bind_match(m->triples_match, m->bindings,
                                              m->parts);
        RASQAL_DEBUG3("bind_match for column %d returned parts %d\n",
                      gp_data->column, parts);
        if(!parts)
          rc=0;
        if(parts & RASQAL_TRIPLE_SUBJECT)
          execution_data->new_bindings_count++;
        if(parts & RASQAL_TRIPLE_PREDICATE)
          execution_data->new_bindings_count++;
        if(parts & RASQAL_TRIPLE_OBJECT)
          execution_data->new_bindings_count++;
        if(parts & RASQAL_TRIPLE_ORIGIN)
          execution_data->new_bindings_count++;
      } else {
        RASQAL_DEBUG2("Nothing to bind_match for column %d\n", gp_data->column);
      }

      rasqal_triples_match_next_match(m->triples_match);
      if(!rc)
        continue;

    }
    
    if(gp_data->column == gp->end_column) {
      /* Done all conjunctions */ 
      
      /* exact match, so column must have ended */
      if(m->is_exact)
        gp_data->column--;

      /* return with result (rc is 1) */
      return rc;
    } else if (gp_data->column >= gp->start_column)
      gp_data->column++;

  }

  if(gp_data->column < gp->start_column)
    rc=0;
  
  return rc;
}



/**
 * rasqal_engine_graph_pattern_get_next_match:
 * @query_results: Query results to execute
 * @gp: graph pattern to use
 *
 * INTERNAL -Get the next match in a graph pattern
 *
 * return: <0 failure, 0 end of results, >0 match
 */
static int
rasqal_engine_graph_pattern_get_next_match(rasqal_engine_execution_data* execution_data,
                                           rasqal_graph_pattern* gp) 
{
  if(gp->graph_patterns)
    return rasqal_engine_group_graph_pattern_get_next_match(execution_data,
                                                            gp);
  else
    return rasqal_engine_triple_graph_pattern_get_next_match(execution_data,
                                                             gp);
}



#if 0
static int
rasqal_engine_graph_pattern_order(const void *a, const void *b)
{
  rasqal_graph_pattern *gp_a=*(rasqal_graph_pattern**)a;
  rasqal_graph_pattern *gp_b=*(rasqal_graph_pattern**)b;

  return (gp_a->op == RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL) -
         (gp_b->op == RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL);
}
#endif


/**
 * rasqal_engine_graph_pattern_init:
 * @query_results: query results to execute
 * @gp: graph pattern in query results.
 *
 * INTERNAL - once only per execution initialisation of a graph pattern.
 *
 * Return value: non-0 on failure
 **/
static int
rasqal_engine_graph_pattern_init(rasqal_engine_execution_data* execution_data,
                                 rasqal_graph_pattern *gp)
{
  rasqal_query *query=execution_data->query;
  rasqal_engine_gp_data* gp_data;

  RASQAL_DEBUG2("Initing execution graph pattern #%d\n", gp->gp_index);

  gp_data=(rasqal_engine_gp_data*)raptor_sequence_get_at(execution_data->seq, 
                                                         gp->gp_index);
  if(!gp_data)
    return -1;
  
  gp_data->optional_graph_pattern= -1;
  gp_data->current_graph_pattern= -1;
  gp_data->column= -1;
  gp_data->matches_returned= 0;
  
  gp_data->matched= 0;
  gp_data->finished= 0;

  if(gp->graph_patterns)
    gp_data->current_graph_pattern=0;

  if(gp->triples) {
    int triples_count=gp->end_column - gp->start_column+1;
    
    gp_data->column=gp->start_column;
    if(gp_data->triple_meta) {
      /* reset any previous execution */
      rasqal_reset_triple_meta(gp_data->triple_meta);
      memset(gp_data->triple_meta, '\0',
             sizeof(rasqal_triple_meta)*triples_count);
    } else {
      gp_data->triple_meta=(rasqal_triple_meta*)RASQAL_CALLOC(rasqal_triple_meta, triples_count, sizeof(rasqal_triple_meta));
      if(!gp_data->triple_meta)
        return -1;
    }
  }

  if(gp->graph_patterns) {
    int i;

#if 0
    /* sort graph patterns, optional graph triples last */
    raptor_sequence_sort(gp->graph_patterns, rasqal_engine_graph_pattern_order);
#endif

    for(i=0; i < raptor_sequence_size(gp->graph_patterns); i++) {
      int rc;
      rasqal_graph_pattern *sgp;
      sgp=(rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      rc=rasqal_engine_graph_pattern_init(execution_data, sgp);
      if(rc)
        return rc;
      
      if((sgp->op == RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL) &&
         gp_data->optional_graph_pattern < 0)
        gp_data->optional_graph_pattern=i;
    }

  }
  
  if(gp->triples) {
    int i;
    
    for(i=gp->start_column; i <= gp->end_column; i++) {
      rasqal_triple_meta *m;
      rasqal_triple *t;
      rasqal_variable* v;

      m=&gp_data->triple_meta[i - gp->start_column];
      if(!m)
        return -1;
      m->parts=(rasqal_triple_parts)0;

      t=(rasqal_triple*)raptor_sequence_get_at(gp->triples, i);
      
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

      RASQAL_DEBUG4("graph pattern #%d Triple %d has parts %d\n",
                    gp->gp_index, i, m->parts);

      /* exact if there are no variables in the triple parts */
      m->is_exact = 1;
      if(rasqal_literal_as_variable(t->predicate) ||
         rasqal_literal_as_variable(t->subject) ||
         rasqal_literal_as_variable(t->object))
        m->is_exact = 0;

    }

  }
  
  return 0;
}


static int
rasqal_engine_remove_filter_graph_patterns(rasqal_query* query,
                                           rasqal_graph_pattern* gp,
                                           void *data)
{
  int i;
  int saw_filter_gp=0;
  raptor_sequence *seq;
  int* modified_p=(int*)data;
  rasqal_graph_pattern* prev_gp=NULL;
  
  if(!gp->graph_patterns)
    return 0;

#if RASQAL_DEBUG > 1
  RASQAL_DEBUG2("Checking graph pattern #%d:\n  ", gp->gp_index);
  rasqal_graph_pattern_print(gp, stdout);
  fputs("\n", stdout);
#endif

  for(i=0; i < raptor_sequence_size(gp->graph_patterns); i++) {
    rasqal_graph_pattern *sgp;
    sgp=(rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
    if(sgp->op == RASQAL_GRAPH_PATTERN_OPERATOR_FILTER) {
      /* One is enough to know we need to rewrite */
      saw_filter_gp=1;
      break;
    }
  }

  if(!saw_filter_gp) {
#if RASQAL_DEBUG > 1
    RASQAL_DEBUG2("Ending graph pattern #%d - saw no filter GPs\n",
                  gp->gp_index);
#endif
    return 0;
  }
  
  
  seq=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_graph_pattern, (raptor_sequence_print_handler*)rasqal_graph_pattern_print);
  if(!seq) {
    RASQAL_DEBUG1("Cannot create new gp sequence\n");
    *modified_p=-1;
    return 1;
  }


  while(raptor_sequence_size(gp->graph_patterns) > 0) {
    rasqal_graph_pattern *sgp;
    sgp=(rasqal_graph_pattern*)raptor_sequence_unshift(gp->graph_patterns);
    if(sgp->op == RASQAL_GRAPH_PATTERN_OPERATOR_FILTER) {
      if(prev_gp)
        rasqal_graph_pattern_move_constraints(prev_gp, sgp);
      rasqal_free_graph_pattern(sgp);
      continue;
    }
    if(raptor_sequence_push(seq, sgp)) {
      RASQAL_DEBUG1("Cannot push to gp sequence\n");
      raptor_free_sequence(seq);
      *modified_p=-1;
      return 1;
    }
    prev_gp=sgp;
  }
  raptor_free_sequence(gp->graph_patterns);
  gp->graph_patterns=seq;

  if(!*modified_p)
    *modified_p=1;
  
#if RASQAL_DEBUG > 1
  RASQAL_DEBUG2("Ending graph pattern #%d\n  ", gp->gp_index);
  rasqal_graph_pattern_print(gp, stdout);
  fputs("\n\n", stdout);
#endif

  return 0;
}


static void
rasqal_engine_move_to_graph_pattern(rasqal_engine_execution_data* execution_data,
                                    rasqal_graph_pattern *gp,
                                    int delta)
{
  rasqal_engine_gp_data* gp_data;
  int graph_patterns_size=raptor_sequence_size(gp->graph_patterns);
  int i;
  
  gp_data=(rasqal_engine_gp_data*)raptor_sequence_get_at(execution_data->seq, 
                                                         gp->gp_index);

  if(gp_data->optional_graph_pattern  < 0 ) {
    gp_data->current_graph_pattern += delta;
    RASQAL_DEBUG3("Moved to graph pattern %d (delta %d)\n", 
                  gp_data->current_graph_pattern, delta);
    return;
  }
  
  /* Otherwise, there are optionals */

  if(delta > 0) {
    gp_data->current_graph_pattern++;
    if(gp_data->current_graph_pattern == gp_data->optional_graph_pattern) {
      RASQAL_DEBUG1("Moved to first optional graph pattern\n");
      for(i=gp_data->current_graph_pattern; i < graph_patterns_size; i++) {
        rasqal_graph_pattern *gp2;
        gp2=(rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
        rasqal_engine_graph_pattern_init(execution_data, gp2);
      }
      gp_data->max_optional_graph_pattern=graph_patterns_size-1;
    }
    gp_data->optional_graph_pattern_matches_count=0;
  } else {
    RASQAL_DEBUG1("Moving to previous graph pattern\n");

    if(gp_data->current_graph_pattern > gp_data->optional_graph_pattern) {
      rasqal_graph_pattern *gp2;
      gp2=(rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns,
                                                        gp_data->current_graph_pattern);
      rasqal_engine_graph_pattern_init(execution_data, gp2);
    }
    gp_data->current_graph_pattern--;
  }
}


static rasqal_engine_step
rasqal_engine_check_constraint(rasqal_query *query, rasqal_graph_pattern *gp)
{
  rasqal_engine_step step=STEP_SEARCHING;
  rasqal_literal* result;
  int bresult=1; /* constraint succeeds */
  int error=0;
    
#ifdef RASQAL_DEBUG
  RASQAL_DEBUG1("filter expression:\n");
  rasqal_expression_print(gp->filter_expression, DEBUG_FH);
  fputc('\n', DEBUG_FH);
#endif
    
  result=rasqal_expression_evaluate(query, gp->filter_expression, 
                                    query->compare_flags);
#ifdef RASQAL_DEBUG
  RASQAL_DEBUG1("filter expression result:\n");
  if(!result)
    fputs("type error", DEBUG_FH);
  else
    rasqal_literal_print(result, DEBUG_FH);
  fputc('\n', DEBUG_FH);
#endif
  if(!result)
    bresult=0;
  else {
    bresult=rasqal_literal_as_boolean(result, &error);
    if(error) {
      RASQAL_DEBUG1("filter boolean expression returned error\n");
      step= STEP_ERROR;
    }
#ifdef RASQAL_DEBUG
    else
      RASQAL_DEBUG2("filter boolean expression result: %d\n", bresult);
#endif
    rasqal_free_literal(result);
  }

  if(!bresult)
    /* Constraint failed so move to try next match */
    return STEP_SEARCHING;
  
  return STEP_GOT_MATCH;
}


/**
 * rasqal_engine_do_step:
 * @execution_data: execution data
 * @outergp: outer graph pattern
 * @gp: inner graph pattern
 *
 * INTERNAL - Execute a graph pattern inside an outer graph pattern
 *
 * return: <0 failure, 0 end of results, >0 match
 */
static rasqal_engine_step
rasqal_engine_do_step(rasqal_engine_execution_data* execution_data,
                      rasqal_graph_pattern* outergp, rasqal_graph_pattern* gp)
{
  rasqal_query* query=execution_data->query;
  int graph_patterns_size=raptor_sequence_size(outergp->graph_patterns);
  rasqal_engine_step step=STEP_SEARCHING;
  int rc;
  rasqal_engine_gp_data* outergp_data;
  rasqal_engine_gp_data* gp_data;

  outergp_data=(rasqal_engine_gp_data*)raptor_sequence_get_at(execution_data->seq, 
                                                              outergp->gp_index);
  gp_data=(rasqal_engine_gp_data*)raptor_sequence_get_at(execution_data->seq, 
                                                         gp->gp_index);
  
  /*  return: <0 failure, 0 end of results, >0 match */
  rc=rasqal_engine_graph_pattern_get_next_match(execution_data, gp);
  
  RASQAL_DEBUG3("Graph pattern %d returned %d\n",
                outergp_data->current_graph_pattern, rc);
  
  /* no matches is always a failure */
  if(rc < 0)
    return STEP_FINISHED;
  
  if(!rc) {
    /* otherwise this is the end of the results */
    RASQAL_DEBUG2("End of non-optional graph pattern %d\n",
                  outergp_data->current_graph_pattern);
    
    return STEP_FINISHED;
  }


  if(gp->filter_expression) {
    step=rasqal_engine_check_constraint(query, gp);
    if(step != STEP_GOT_MATCH)
      return step;
  }

  if(outergp->filter_expression) {
    step=rasqal_engine_check_constraint(query, outergp);
    if(step != STEP_GOT_MATCH)
      return step;
  }
 
  /* got match */
  RASQAL_DEBUG1("Got match\n");
  gp_data->matched=1;
    
  /* if this is a match but not the last graph pattern in the
   * sequence move to the next graph pattern
   */
  if(outergp_data->current_graph_pattern < graph_patterns_size-1) {
    RASQAL_DEBUG1("Not last graph pattern\n");
    rasqal_engine_move_to_graph_pattern(execution_data, outergp, +1);
    return STEP_SEARCHING;
  }
  
  return STEP_GOT_MATCH;
}


/**
 * rasqal_engine_do_optional_step:
 * @execution_data: execution data
 * @outergp: outer graph pattern
 * @gp: inner graph pattern
 *
 * INTERNAL - Execute an OPTIONAL graph pattern inside an outer graph pattern
 *
 * return: <0 failure, 0 end of results, >0 match
 */
static rasqal_engine_step
rasqal_engine_do_optional_step(rasqal_engine_execution_data* execution_data,
                               rasqal_graph_pattern *outergp,
                               rasqal_graph_pattern *gp)
{
  rasqal_query* query; 
  rasqal_query_results* query_results;
  int graph_patterns_size=raptor_sequence_size(outergp->graph_patterns);
  rasqal_engine_step step=STEP_SEARCHING;
  int rc;
  rasqal_engine_gp_data* gp_data;
  rasqal_engine_gp_data* outergp_data;

  query = execution_data->query;
  query_results = execution_data->query_results;
  gp_data=(rasqal_engine_gp_data*)raptor_sequence_get_at(execution_data->seq, 
                                                         gp->gp_index);
  outergp_data=(rasqal_engine_gp_data*)raptor_sequence_get_at(execution_data->seq, 
                                                              outergp->gp_index);
  
  if(gp_data->finished) {
    if(!outergp_data->current_graph_pattern) {
      step=STEP_FINISHED;
      RASQAL_DEBUG1("Ended first graph pattern - finished\n");
      query_results->finished=1;
      return STEP_FINISHED;
    }
    
    RASQAL_DEBUG2("Ended graph pattern %d, backtracking\n",
                  outergp_data->current_graph_pattern);
    
    /* backtrack optionals */
    rasqal_engine_move_to_graph_pattern(execution_data, outergp, -1);
    return STEP_SEARCHING;
  }
  
  
  /*  return: <0 failure, 0 end of results, >0 match */
  rc=rasqal_engine_graph_pattern_get_next_match(execution_data, gp);
  
  RASQAL_DEBUG3("Graph pattern %d returned %d\n",
                outergp_data->current_graph_pattern, rc);
  
  /* count all optional matches */
  if(rc > 0)
    outergp_data->optional_graph_pattern_matches_count++;

  if(rc < 0) {
    /* optional always matches */
    RASQAL_DEBUG2("Optional graph pattern %d failed to match, continuing\n", 
                  outergp_data->current_graph_pattern);
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
                  outergp_data->current_graph_pattern);

    gp_data->matched=0;
    
    /* Next time we get here, backtrack */
    gp_data->finished=1;
    
    if(outergp_data->current_graph_pattern < outergp_data->max_optional_graph_pattern) {
      RASQAL_DEBUG1("More optionals graph patterns to search\n");
      rasqal_engine_move_to_graph_pattern(execution_data, outergp, +1);
      return STEP_SEARCHING;
    }

    outergp_data->max_optional_graph_pattern--;
    RASQAL_DEBUG2("Max optional graph patterns lowered to %d\n",
                  outergp_data->max_optional_graph_pattern);
    
    /* Last optional match ended.
     * If we got any non optional matches, then we have a result.
     */
    for(i=0; i < graph_patterns_size; i++) {
      rasqal_graph_pattern *gp2;
      rasqal_engine_gp_data* gp2_data;

      gp2=(rasqal_graph_pattern*)raptor_sequence_get_at(outergp->graph_patterns, i);
      gp2_data=(rasqal_engine_gp_data*)raptor_sequence_get_at(execution_data->seq, 
                                                              gp2->gp_index);

      if(outergp_data->optional_graph_pattern >= 0 &&
         i >= outergp_data->optional_graph_pattern)
        optional_matches += gp2_data->matched;
      else
        mandatory_matches += gp2_data->matched;
    }
    
    
    RASQAL_DEBUG2("Optional graph pattern has %d matches returned\n", 
                  outergp_data->matches_returned);
    
    RASQAL_DEBUG2("Found %d query optional graph pattern matches\n", 
                  outergp_data->optional_graph_pattern_matches_count);
    
    RASQAL_DEBUG3("Found %d mandatory matches, %d optional matches\n", 
                  mandatory_matches, optional_matches);
    RASQAL_DEBUG2("Found %d new binds\n", execution_data->new_bindings_count);
    
    if(optional_matches) {
      RASQAL_DEBUG1("Found some matches, returning a result\n");
      return STEP_GOT_MATCH;
    }

    if(gp_data->matches_returned) { 
      if(!outergp_data->current_graph_pattern) {
        RASQAL_DEBUG1("No matches this time and first graph pattern was optional, finished\n");
        return STEP_FINISHED;
      }

      RASQAL_DEBUG1("No matches this time, some earlier, backtracking\n");
      rasqal_engine_move_to_graph_pattern(execution_data, outergp, -1);
      return STEP_SEARCHING;
    }


    if(execution_data->new_bindings_count > 0) {
      RASQAL_DEBUG2("%d new bindings, returning a result\n",
                    execution_data->new_bindings_count);
      return STEP_GOT_MATCH;
    }
    RASQAL_DEBUG1("no new bindings, continuing searching\n");
    return STEP_SEARCHING;
  }

  
  if(gp->filter_expression) {
    step=rasqal_engine_check_constraint(query, gp);
    if(step != STEP_GOT_MATCH) {
      /* The constraint failed or we have an error - no bindings count */
      execution_data->new_bindings_count=0;
      return step;
    }
  }


  /* got match */
   
 /* if this is a match but not the last graph pattern in the
  * sequence move to the next graph pattern
  */
 if(outergp_data->current_graph_pattern < graph_patterns_size-1) {
   RASQAL_DEBUG1("Not last graph pattern\n");
   rasqal_engine_move_to_graph_pattern(execution_data, outergp, +1);
   return STEP_SEARCHING;
 }
 

  if(outergp->filter_expression) {
    step=rasqal_engine_check_constraint(query, outergp);
    if(step != STEP_GOT_MATCH) {
      /* The constraint failed or we have an error - no bindings count */
      execution_data->new_bindings_count=0;
      return STEP_SEARCHING;
    }
  }


 /* is the last graph pattern so we have a solution */

  RASQAL_DEBUG1("Got match\n");
  gp_data->matched=1;

  return STEP_GOT_MATCH;
}


/**
 * rasqal_engine_get_next_result:
 * @query_results: query results object
 *
 * INTERNAL - Get the next result from a query execution
 *
 * return: <0 failure, 0 end of results, >0 match
 */
static int
rasqal_engine_get_next_result(rasqal_engine_execution_data* execution_data)
{
  rasqal_query* query;
  rasqal_query_results *query_results;
  int graph_patterns_size;
  rasqal_engine_step step;
  int i;
  rasqal_graph_pattern *outergp;
  rasqal_engine_gp_data* outergp_data;
  int size;

  query = execution_data->query;
  query_results = execution_data->query_results;

  if(query_results->failed)
    return -1;

  if(query_results->finished)
    return 0;

  if(!query->triples)
    return -1;

  outergp=query->query_graph_pattern;
  if(!outergp || !outergp->graph_patterns) {
    /* FIXME - no graph patterns in query - end results */
    query->failed=1;
    rasqal_log_error_simple(query->world, RAPTOR_LOG_LEVEL_ERROR,
                            &query->locator,
                            "No graph patterns in query. Ending query execution.");
    query_results->finished=1;
    return 0;
  }
  
  graph_patterns_size=raptor_sequence_size(outergp->graph_patterns);
  if(!graph_patterns_size) {
    /* FIXME - no graph patterns in query - end results */
    query->failed=1;
    rasqal_log_error_simple(query->world, RAPTOR_LOG_LEVEL_ERROR,
                            &query->locator,
                            "No graph patterns in query. Ending query execution.");
    query_results->finished=1;
    return 0;
  }

  outergp_data=(rasqal_engine_gp_data*)raptor_sequence_get_at(execution_data->seq, 
                                                              outergp->gp_index);

  execution_data->new_bindings_count=0;

  if(query->constructs)
    size=rasqal_variables_table_get_named_variables_count(query->vars_table);
  else
    size=query->select_variables_count;

  step=STEP_SEARCHING;
  while(step == STEP_SEARCHING) {
    rasqal_graph_pattern* gp;
    rasqal_engine_gp_data* gp_data;
    int values_returned=0;
    int optional_step;
    
    gp=(rasqal_graph_pattern*)raptor_sequence_get_at(outergp->graph_patterns,
                                                     outergp_data->current_graph_pattern);
    gp_data=(rasqal_engine_gp_data*)raptor_sequence_get_at(execution_data->seq, 
                                                           gp->gp_index);
    if(!gp_data)
      return -1;

    RASQAL_DEBUG3("Handling graph_pattern %d %s\n",
                  outergp_data->current_graph_pattern,
                  rasqal_graph_pattern_operator_as_string(gp->op));

    if(gp->graph_patterns) {
      /* FIXME - sequence of graph_patterns not implemented, finish */
      query->failed=1;
      rasqal_log_error_simple(query->world, RAPTOR_LOG_LEVEL_ERROR,
                              &query->locator,
                              "Graph pattern %s operation is not implemented yet. Ending query execution.", 
                              rasqal_graph_pattern_operator_as_string(gp->op));

      RASQAL_DEBUG1("Failing query with sequence of graph_patterns\n");
      step=STEP_FINISHED;
      break;
    }

    gp_data->matched=0;
    optional_step=(gp->op == RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL);
    
    if(optional_step)
      step=rasqal_engine_do_optional_step(execution_data, outergp, gp);
    else
      step=rasqal_engine_do_step(execution_data, outergp, gp);

    RASQAL_DEBUG2("Returned step is %s\n",
                  rasqal_engine_step_names[step]);

    /* Count actual bound values */
    for(i=0; i< size; i++) {
      rasqal_variable* v=rasqal_variables_table_get(query->vars_table, i);
      if(v->value)
        values_returned++;
    }
    RASQAL_DEBUG2("Solution binds %d values\n", values_returned);
    RASQAL_DEBUG2("New bindings %d\n", execution_data->new_bindings_count);

    if(!values_returned && optional_step &&
       step != STEP_FINISHED && step != STEP_SEARCHING) {
      RASQAL_DEBUG1("An optional pass set no bindings, continuing searching\n");
      step=STEP_SEARCHING;
    }

  }


  RASQAL_DEBUG3("Ending with step %s and graph pattern %d\n",
                rasqal_engine_step_names[step],
                outergp_data->current_graph_pattern);
  
  
  if(step != STEP_GOT_MATCH)
    query_results->finished=1;

  if(step == STEP_GOT_MATCH) {
    for(i=0; i < graph_patterns_size; i++) {
      rasqal_graph_pattern *gp2;
      rasqal_engine_gp_data* gp2_data;

      gp2=(rasqal_graph_pattern*)raptor_sequence_get_at(outergp->graph_patterns, i);
      gp2_data=(rasqal_engine_gp_data*)raptor_sequence_get_at(execution_data->seq, 
                                                              gp2->gp_index);
      if(gp2_data->matched)
        gp2_data->matches_returned++;
    }

    /* Got a valid result */
#ifdef RASQAL_DEBUG
    RASQAL_DEBUG1("Returning solution[");
    for(i=0; i< size; i++) {
      rasqal_variable* v=rasqal_variables_table_get(query->vars_table, i);
      if(i>0)
        fputs(", ", DEBUG_FH);
      fprintf(DEBUG_FH, "%s=", v->name);
      if(v->value)
        rasqal_literal_print(v->value, DEBUG_FH);
      else
        fputs("NULL", DEBUG_FH);
    }
    fputs("]\n", DEBUG_FH);
#endif
  }

  return (step == STEP_GOT_MATCH);
}


/**
 * rasqal_engine_row_update:
 * @row: query result row
 * @offset: integer offset into result values array
 *
 * INTERNAL - Update row values from query variables
 *
 * FIXME: This function should not write to the query structure.
 *
 * Return value: non-0 on failure 
 */
static int
rasqal_engine_row_update(rasqal_engine_execution_data* execution_data,
                         rasqal_row* row, int offset)
{
  rasqal_query* query;
  rasqal_query_results* query_results;
  int i;
  int size;
  
  query = execution_data->query;
  query_results = execution_data->query_results;

  if(!rasqal_query_results_is_bindings(query_results) &&
     !rasqal_query_results_is_graph(query_results))
    return 1;

  if(query->constructs)
    size=rasqal_variables_table_get_named_variables_count(query->vars_table);
  else
    size=query->select_variables_count;

  for(i=0; i < row->size; i++) {
    rasqal_literal *l=rasqal_variables_table_get_value(query->vars_table, i);
    if(row->values[i])
      rasqal_free_literal(row->values[i]);
    row->values[i]=rasqal_new_literal_from_literal(l);
  }

  if(row->order_size) {
    for(i=0; i < row->order_size; i++) {
      rasqal_expression* e;
      rasqal_literal *l;

      e=(rasqal_expression*)raptor_sequence_get_at(query->order_conditions_sequence, i);
      l=rasqal_expression_evaluate(query, e, query->compare_flags);
      if(row->order_values[i])
        rasqal_free_literal(row->order_values[i]);
      if(l) {
        row->order_values[i]=rasqal_new_literal_from_literal(rasqal_literal_value(l));
        rasqal_free_literal(l);
      } else
        row->order_values[i]=NULL;
    }
  }
  
  row->offset=offset;
  
  return 0;
}


/**
 * rasqal_query_result_literal_sequence_compare:
 * @query: the #rasqal_query to use to comparekfind the variables in
 * @values_a: first array of literals
 * @values_b: second array of literals
 * @expr_sequence: array of expressions
 * @size: size of arrays
 *
 * INTERNAL - compare two arrays of literals evaluated in an array of expressions
 *
 * Return value: <0, 0 or >1 comparison
 */
static int
rasqal_query_result_literal_sequence_compare(rasqal_query* query,
                                             rasqal_literal** values_a,
                                             rasqal_literal** values_b,
                                             raptor_sequence* expr_sequence,
                                             int size)
{
  int result=0;
  int i;

  for(i=0; i < size; i++) {
    rasqal_expression* e=NULL;
    int error=0;
    rasqal_literal* literal_a=values_a[i];
    rasqal_literal* literal_b=values_b[i];
    
    if(expr_sequence)
      e=(rasqal_expression*)raptor_sequence_get_at(expr_sequence, i);

#ifdef RASQAL_DEBUG
    RASQAL_DEBUG1("Comparing ");
    rasqal_literal_print(literal_a, DEBUG_FH);
    fputs(" to ", DEBUG_FH);
    rasqal_literal_print(literal_b, DEBUG_FH);
    fputs("\n", DEBUG_FH);
#endif

    if(!literal_a || !literal_b) {
      if(!literal_a && !literal_b)
        result= 0;
      else {
        result= literal_a ? 1 : -1;
#ifdef RASQAL_DEBUG
        RASQAL_DEBUG2("Got one NULL literal comparison, returning %d\n", result);
#endif
        break;
      }
    }
    
    result=rasqal_literal_compare(literal_a, literal_b,
                                  query->compare_flags | RASQAL_COMPARE_URI,
                                  &error);

    if(error) {
#ifdef RASQAL_DEBUG
      RASQAL_DEBUG2("Got literal comparison error at expression %d, returning 0\n", i);
#endif
      result=0;
      break;
    }
        
    if(!result)
      continue;

    if(e && e->op == RASQAL_EXPR_ORDER_COND_DESC)
      result= -result;
    /* else Order condition is RASQAL_EXPR_ORDER_COND_ASC so nothing to do */
    
#ifdef RASQAL_DEBUG
    RASQAL_DEBUG3("Returning comparison result %d at expression %d\n", result, i);
#endif
    break;
  }

  return result;
}


/**
 * rasqal_query_result_literal_sequence_equals:
 * @query: the #rasqal_query to use to compare
 * @values_a: first array of literals
 * @values_b: second array of literals
 * @size: size of arrays
 *
 * INTERNAL - compare two arrays of literals for equality
 *
 * Return value: non-0 if equal
 */
static int
rasqal_query_result_literal_sequence_equals(rasqal_query* query,
                                            rasqal_literal** values_a,
                                            rasqal_literal** values_b,
                                            int size)
{
  int result=1; /* equal */
  int i;
  int error=0;

  for(i=0; i < size; i++) {
    rasqal_literal* literal_a=values_a[i];
    rasqal_literal* literal_b=values_b[i];
    
    result=rasqal_literal_equals_flags(literal_a, literal_b,
                                       RASQAL_COMPARE_RDF, &error);
#ifdef RASQAL_DEBUG
    RASQAL_DEBUG1("Comparing ");
    rasqal_literal_print(literal_a, DEBUG_FH);
    fputs(" to ", DEBUG_FH);
    rasqal_literal_print(literal_b, DEBUG_FH);
    fprintf(DEBUG_FH, " gave %s\n", (result ? "equality" : "not equal"));
#endif

    if(error)
      result=0;
    
    /* if different, end */
    if(!result)
      break;
  }

  return result;
}


/**
 * rasqal_engine_row_compare:
 * @a: pointer to address of first #row
 * @b: pointer to address of second #row
 *
 * INTERNAL - compare two pointers to #row objects
 *
 * Suitable for use as a compare function in qsort() or similar.
 *
 * Return value: <0, 0 or >1 comparison
 */
static int
rasqal_engine_row_compare(const void *a, const void *b)
{
  rasqal_row* row_a;
  rasqal_row* row_b;
  rasqal_query* query;
  int result=0;

  row_a=*(rasqal_row**)a;
  row_b=*(rasqal_row**)b;

  query=rasqal_rowsource_get_query(row_a->rowsource);
  
  if(query->distinct) {
    if(query->distinct == 1)
      result=!rasqal_query_result_literal_sequence_equals(query,
                                                          row_a->values,
                                                          row_b->values,
                                                          row_a->size);
    
    if(!result)
      /* duplicate, so return that */
      return 0;
  }
  
  /* now order it */
  result=rasqal_query_result_literal_sequence_compare(query,
                                                      row_a->order_values,
                                                      row_b->order_values,
                                                      query->order_conditions_sequence,
                                                      row_a->order_size);
  
  /* still equal?  make sort stable by using the original order */
  if(!result) {
    result= row_a->offset - row_b->offset;
    RASQAL_DEBUG2("Got equality result so using offsets, returning %d\n",
                  result);
  }
  
  return result;
}


static void
rasqal_engine_map_free_row(const void *key, const void *value)
{
  if(key)
    rasqal_free_row((rasqal_row*)key);
  if(value)
    rasqal_free_row((rasqal_row*)value);
}


static void
rasqal_engine_map_print_row(void *object, FILE *fh)
{
  if(object)
    rasqal_row_print((rasqal_row*)object, fh);
  else
    fputs("NULL", fh);
}


static void
rasqal_engine_map_add_to_sequence(void *key, void *value, void *user_data)
{
  rasqal_row* row;
  row=rasqal_new_row_from_row((rasqal_row*)key);
  raptor_sequence_push((raptor_sequence*)user_data, row);
}


typedef struct 
{
  rasqal_query_results* results;
  rasqal_engine_execution_data* execution_data;
  rasqal_map* map;
  raptor_sequence* seq;
  int need_store_results;
  int finished;
  int failed;
  int offset;
} rasqal_rowsource_engine_context;


/* Local handlers for getting rows from a query execution */

static int
rasqal_rowsource_engine_init(rasqal_rowsource* rowsource, void *user_data) 
{
  rasqal_rowsource_engine_context* con;

  con = (rasqal_rowsource_engine_context*)user_data;
  con->offset = 0;
  return 0;
}


static int
rasqal_rowsource_engine_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_rowsource_engine_context* con;

  con = (rasqal_rowsource_engine_context*)user_data;
  if(con->map)
    rasqal_free_map(con->map);
  if(con->seq)
    raptor_free_sequence(con->seq);
  RASQAL_FREE(rasqal_rowsource_engine_context, con);

  return 0;
}


static void
rasqal_rowsource_engine_process(rasqal_rowsource* rowsource,
                                rasqal_rowsource_engine_context* con,
                                int read_all)
{
  if(con->finished || con->failed)
    return;
  
  while(1) {
    rasqal_row* row;
    int rc;

    /* query_results->results_sequence is NOT assigned before here 
     * so that this function does the regular query results next
     * operation.
     */
    rc=rasqal_engine_get_next_result(con->execution_data);
    if(rc == 0) {
      /* =0 end of results */
      con->finished=1;
      break;
    }
    
    if(rc < 0) {
      /* <0 failure */
      con->results->finished=1;
      con->results->failed=1;
      
      if(con->map) {
        rasqal_free_map(con->map);
        con->map=NULL;
      }
      raptor_free_sequence(con->seq);
      con->seq=NULL;
      break;
    }
    
    /* otherwise is >0 match */
    row=rasqal_new_row(rowsource);
    if(!row) {
      raptor_free_sequence(con->seq); con->seq=NULL;
      if(con->map) {
        rasqal_free_map(con->map); con->map=NULL;
      }
      con->failed=1;
      return;
    }
    
    rasqal_engine_row_update(con->execution_data, row, con->offset);

    if(!con->map) {
      /* no map. after this, row is owned by sequence */
      raptor_sequence_push(con->seq, row);
      con->offset++;
    } else {
      /* map. after this, row is owned by map */
      if(!rasqal_map_add_kv(con->map, row, NULL)) {
        con->offset++;
      } else {
        /* duplicate, and not added so delete it */
#ifdef RASQAL_DEBUG
        RASQAL_DEBUG1("Got duplicate row ");
        rasqal_row_print(row, DEBUG_FH);
        fputc('\n', DEBUG_FH);
#endif
        rasqal_free_row(row);
        row=NULL;
      }
    }

    /* if a row was returned and not storing result, end loop */
    if(!read_all && !con->need_store_results)
      return;
  }
  
#ifdef RASQAL_DEBUG
  if(con->map) {
    fputs("resulting ", DEBUG_FH);
    rasqal_map_print(con->map, DEBUG_FH);
    fputs("\n", DEBUG_FH);
  }
#endif
  
  if(con->map) {
    /* do sort/distinct: walk map in order, adding rows to sequence */
    rasqal_map_visit(con->map, rasqal_engine_map_add_to_sequence,
                     (void*)con->seq);
    rasqal_free_map(con->map); con->map=NULL;
  }
}


static int
rasqal_rowsource_engine_ensure_variables(rasqal_rowsource* rowsource,
                                         void *user_data)
{
  rasqal_rowsource_engine_context* con;
  rasqal_query* query;

  con = (rasqal_rowsource_engine_context*)user_data;
  query = con->results->query;

  if(query->constructs)
    rowsource->size=rasqal_variables_table_get_named_variables_count(query->vars_table);
  else
    rowsource->size=query->select_variables_count;  
  if(query->order_conditions_sequence)
    rowsource->order_size=raptor_sequence_size(query->order_conditions_sequence);

  return 0;
}


static rasqal_row*
rasqal_rowsource_engine_read_row(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_rowsource_engine_context* con;

  con = (rasqal_rowsource_engine_context*)user_data;
  if(!con->finished)
    rasqal_rowsource_engine_process(rowsource, con, 0);

  return (rasqal_row*)raptor_sequence_unshift(con->seq);
}


static raptor_sequence*
rasqal_rowsource_engine_read_all_rows(rasqal_rowsource* rowsource,
                                      void *user_data)
{
  rasqal_rowsource_engine_context* con;
  raptor_sequence* seq;
  
  con = (rasqal_rowsource_engine_context*)user_data;
  if(!con->finished)
    rasqal_rowsource_engine_process(rowsource, con, 1);
  
  seq=con->seq;
  con->seq=NULL;
  
  return seq;
}


static rasqal_query*
rasqal_rowsource_engine_get_query(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_rowsource_engine_context* con;

  con = (rasqal_rowsource_engine_context*)user_data;
  return con->results->query;
}


static const rasqal_rowsource_handler rasqal_rowsource_engine_handler={
  /* .version = */ 1,
  /* .init = */ rasqal_rowsource_engine_init,
  /* .finish = */ rasqal_rowsource_engine_finish,
  /* .ensure_variables = */ rasqal_rowsource_engine_ensure_variables,
  /* .read_row = */ rasqal_rowsource_engine_read_row,
  /* .read_all_rows = */ rasqal_rowsource_engine_read_all_rows,
  /* .get_query = */ rasqal_rowsource_engine_get_query
};


static rasqal_rowsource*
rasqal_engine_make_rowsource(rasqal_query* query,
                             rasqal_query_results* results,
                             rasqal_engine_execution_data* execution_data) 
{
  rasqal_rowsource_engine_context* con;
  int flags = 0;
  
  con = (rasqal_rowsource_engine_context*)RASQAL_CALLOC(rasqal_rowsource_engine_context, 1, sizeof(rasqal_rowsource_engine_context));
  if(!con)
    return NULL;

  con->results = results;
  con->execution_data = execution_data;
  con->need_store_results = results->store_results;

  if(con->need_store_results) {
    /* make a row:NULL map in order to sort or do distinct */
    con->map = rasqal_new_map(rasqal_engine_row_compare,
                              rasqal_engine_map_free_row, 
                              rasqal_engine_map_print_row,
                              NULL,
                              0);
    if(!con->map) {
      rasqal_rowsource_engine_finish(NULL, con);
      return NULL;
    }
  }
  
  con->seq = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_row, (raptor_sequence_print_handler*)rasqal_row_print);
  if(!con->seq) {
    rasqal_rowsource_engine_finish(NULL, con);
    return NULL;
  }
  
  flags = con->need_store_results ? RASQAL_ROWSOURCE_FLAGS_ORDERING : 0;

  return rasqal_new_rowsource_from_handler(con,
                                           &rasqal_rowsource_engine_handler,
                                           flags);
}


/**
 * rasqal_query_engine_1_get_row:
 * @ex_data: Query results to execute
 * @error_p: execution error (OUT variable)
 *
 * INTERNAL - Execute a query to get one result, finished or failure.
 * 
 * Return value: new row or NULL on finished or failure
 */
static rasqal_row*
rasqal_query_engine_1_get_row(void* ex_data, rasqal_engine_error *error_p)
{
  rasqal_engine_execution_data* execution_data;
  rasqal_query_results* query_results;
  rasqal_row* row = NULL;
  
  execution_data=(rasqal_engine_execution_data*)ex_data;

  query_results = execution_data->query_results;

  while(1) {
    int rc;
    
    /* rc<0 error rc=0 end of results,  rc>0 got a result */
    rc = rasqal_engine_get_next_result(execution_data);

    if(rc < 1) {
      /* <0 failure OR =0 end of results */
      query_results->finished = 1;

      /* <0 failure */
      if(rc < 0)
        query_results->failed = 1;
      break;
    }
    
    /* otherwise is >0 match */
    execution_data->result_count++;

    /* finished if beyond result range */
    if(rasqal_query_results_check_limit_offset(query_results) > 0) {
      execution_data->result_count--;
      break;
    }

    /* continue if before start of result range */
    if(rasqal_query_results_check_limit_offset(query_results) < 0)
      continue;

    /* else got result or finished */
    break;

  } /* while */

  if(!query_results->finished) {
    row = rasqal_new_row(execution_data->rowsource);

    if(row) {
      rasqal_engine_row_update(execution_data, row,
                               execution_data->result_count);
      rasqal_row_to_nodes(row);
    }
  }
  
  return row;
}


/**
 * rasqal_query_engine_1_execute_transform_hack:
 * @query: query object
 *
 * INTERNAL - Transform queries in the new query algebra into an executable form understood by the old query engine.
 *
 * That means in particular:
 *
 * 1) removing RASQAL_GRAPH_PATTERN_OPERATOR_FILTER graph patterns
 * and moving the constraints to the previous GP in the sequence.
 * Filter GPs always appear after another GP.
 *
 * 2) Ensuring that the root graph pattern is a GROUP even if
 * there is only 1 GP inside it.
 *
 * Return value: non-0 on failure
 */
static int
rasqal_query_engine_1_execute_transform_hack(rasqal_query* query) 
{
  if(query->query_graph_pattern) {
    int modified = 0;

    /* modified is set to 1 if a change was made and -1 on failure */
    rasqal_query_graph_pattern_visit(query,
                                     rasqal_engine_remove_filter_graph_patterns,
                                     &modified);
      
#if RASQAL_DEBUG > 1
    fprintf(DEBUG_FH, "modified=%d after remove filter GPs, query graph pattern now:\n  ", modified);
    rasqal_graph_pattern_print(query->query_graph_pattern, DEBUG_FH);
    fputs("\n", DEBUG_FH);
#endif

    if(modified < 0)
      return 1;

    if(query->query_graph_pattern->op != RASQAL_GRAPH_PATTERN_OPERATOR_GROUP) {
      rasqal_graph_pattern* new_qgp;

      new_qgp = rasqal_new_graph_pattern_from_sequence(query, NULL,
                                                       RASQAL_GRAPH_PATTERN_OPERATOR_GROUP);
      if(!new_qgp)
        return 1;

      new_qgp->gp_index = (query->graph_pattern_count++);
      if(rasqal_graph_pattern_add_sub_graph_pattern(new_qgp,
                                                    query->query_graph_pattern)) {
        rasqal_free_graph_pattern(new_qgp);
        query->query_graph_pattern = NULL;
        return 1;
      }

      query->query_graph_pattern = new_qgp;

#if RASQAL_DEBUG > 1
    fprintf(DEBUG_FH, "after insert top level group GPs, query graph pattern now:\n");
    rasqal_graph_pattern_print(query->query_graph_pattern, DEBUG_FH);
    fputs("\n", DEBUG_FH);
#endif
    }

  }

  return 0;
}  


/**
 * rasqal_query_engine_1_execute_init:
 * @ex_data: execution data
 * @query: query to execute
 * @query_results: query results to put results in
 * @error_p: execution error (OUT variable)
 *
 * INTERNAL - Prepare to execute a query.
 *
 * Initialises all state for a new query execution but do not
 * start executing it.
 *
 * Return value: non-0 on failure
 */
static int
rasqal_query_engine_1_execute_init(void* ex_data,
                                   rasqal_query* query,
                                   rasqal_query_results* query_results,
                                   rasqal_engine_error *error_p)
{
  rasqal_engine_execution_data* execution_data;
  int rc=0;

  execution_data=(rasqal_engine_execution_data*)ex_data;

  if(!query->triples)
    return 1;
  
  /* FIXME - invoke a temporary transformation to turn queries in the
   * new query algebra into an executable form understood by this
   * query engine.
   */
  if(rasqal_query_engine_1_execute_transform_hack(query))
    return 1;
  

  /* initialise the execution_data filelds */
  execution_data->query = query;
  execution_data->query_results = query_results;
  execution_data->result_count = 0;

  if(!execution_data->triples_source) {
    execution_data->triples_source = rasqal_new_triples_source(query_results);
    if(!execution_data->triples_source)
      return 1;
  }

  execution_data->seq = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_gp_data, NULL);
  if(!execution_data->seq)
    return 1;


  /* create all graph pattern-specific execution data */
  if(query->graph_patterns_sequence) {
    int i;
    
    for(i = 0; i < query->graph_pattern_count; i++) {
      rasqal_graph_pattern* gp;
      rasqal_engine_gp_data* gp_data;
    
      gp = (rasqal_graph_pattern*)raptor_sequence_get_at(query->graph_patterns_sequence, i);
      gp_data = rasqal_new_engine_gp_data(gp);
      if(!gp_data || raptor_sequence_set_at(execution_data->seq, i, gp_data))
        return 1;
    }
  }

  /* initialise all the graph pattern-specific data */
  if(query->query_graph_pattern) {
    rc = rasqal_engine_graph_pattern_init(execution_data, 
                                          query->query_graph_pattern);
    if(rc)
      return 1;
  }


  /* Initialise the rowsource */
  if(execution_data->rowsource)
    rasqal_free_rowsource(execution_data->rowsource);
  execution_data->rowsource = rasqal_engine_make_rowsource(query, query_results,
                                                           execution_data);
  if(!execution_data->rowsource)
    return 1;

  return rc;
}


/**
 * rasqal_query_engine_1_get_all_rows:
 * @ex_data: execution data
 * @error_p: execution error (OUT variable)
 *
 * INTERNAL - Execute a query to get all results
 * 
 * Return value: all rows or NULL on failure
 */
static raptor_sequence*
rasqal_query_engine_1_get_all_rows(void* ex_data, rasqal_engine_error *error_p)
{
  rasqal_engine_execution_data* execution_data;
  raptor_sequence* seq;

  execution_data=(rasqal_engine_execution_data*)ex_data;

  seq = rasqal_rowsource_read_all_rows(execution_data->rowsource);
  rasqal_free_rowsource(execution_data->rowsource);
  execution_data->rowsource=NULL;

  return seq;
}


/**
 * rasqal_query_engine_1_execute_finish:
 * @ex_data: execution data
 * @error_p: execution error (OUT variable)
 *
 * INTERNAL - Finish execution of a query
 * 
 * Return value: non-0 on failure
 */
static int
rasqal_query_engine_1_execute_finish(void* ex_data,
                                     rasqal_engine_error *error_p)
{
  rasqal_engine_execution_data* execution_data;
  execution_data=(rasqal_engine_execution_data*)ex_data;

  if(!execution_data)
    return -1;

  if(execution_data->triples_source) {
    rasqal_free_triples_source(execution_data->triples_source);
    execution_data->triples_source=NULL;
  }

  if(execution_data->rowsource) {
    rasqal_free_rowsource(execution_data->rowsource);
    execution_data->rowsource=NULL;
  }

  if(execution_data->seq) {
    raptor_free_sequence(execution_data->seq);
    execution_data->seq=NULL;
  }

  return 0;
}


static void
rasqal_query_engine_1_finish_factory(rasqal_query_execution_factory* factory,
                                     rasqal_engine_error *error_p)
{
  return;
}


const rasqal_query_execution_factory rasqal_query_engine_1 =
{
  /* .name=                */ "rasqal 0.9.16 engine",
  /* .execution_data_size= */ sizeof(rasqal_engine_execution_data),
  /* .execute_init=        */ rasqal_query_engine_1_execute_init,
  /* .get_all_rows=        */ rasqal_query_engine_1_get_all_rows,
  /* .get_row=             */ rasqal_query_engine_1_get_row,
  /* .execute_finish=      */ rasqal_query_engine_1_execute_finish,
  /* .finish_factory=      */ rasqal_query_engine_1_finish_factory
};

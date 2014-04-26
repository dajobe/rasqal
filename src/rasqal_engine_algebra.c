/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_engine_algebra.c - Rasqal query engine over query algebra
 *
 * Copyright (C) 2008-2009, David Beckett http://www.dajobe.org/
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


typedef struct {
  rasqal_query* query;
  rasqal_query_results* query_results;

  /* query algebra representation of query */
  rasqal_algebra_node* algebra_node;

  /* number of nodes in #algebra_node tree */
  int nodes_count;

  /* rowsource that provides the result rows */
  rasqal_rowsource* rowsource;

  rasqal_triples_source* triples_source;
} rasqal_engine_algebra_data;


static rasqal_rowsource* rasqal_algebra_node_to_rowsource(rasqal_engine_algebra_data* execution_data, rasqal_algebra_node* node, rasqal_engine_error *error_p);


static int
rasqal_engine_algebra_count_nodes(rasqal_query* query,
                                  rasqal_algebra_node* node,
                                  void* data)
{
  int *count_p = (int*)data;
  (*count_p)++;
  
  return 0;
}


static rasqal_rowsource*
rasqal_algebra_basic_algebra_node_to_rowsource(rasqal_engine_algebra_data* execution_data,
                                               rasqal_algebra_node* node,
                                               rasqal_engine_error *error_p)
{
  rasqal_query *query = execution_data->query;
  
  return rasqal_new_triples_rowsource(query->world, query,
                                      execution_data->triples_source,
                                      node->triples,
                                      node->start_column, node->end_column);
}


static rasqal_rowsource*
rasqal_algebra_filter_algebra_node_to_rowsource(rasqal_engine_algebra_data* execution_data,
                                                rasqal_algebra_node* node,
                                                rasqal_engine_error *error_p)
{
  rasqal_query *query = execution_data->query;
  rasqal_rowsource *rs;

  if(node->node1) {
    rs = rasqal_algebra_node_to_rowsource(execution_data, node->node1, error_p);
  } else {
    rs = rasqal_new_empty_rowsource(query->world, query);
  }

  if((error_p && *error_p) && rs) {
    rasqal_free_rowsource(rs);
    rs = NULL;
  }
  if(!rs)
    return NULL;

  return rasqal_new_filter_rowsource(query->world, query, rs, node->expr);
}


static rasqal_rowsource*
rasqal_algebra_orderby_algebra_node_to_rowsource(rasqal_engine_algebra_data* execution_data,
                                                 rasqal_algebra_node* node,
                                                 rasqal_engine_error *error_p)
{
  rasqal_query *query = execution_data->query;
  rasqal_rowsource *rs;

  rs = rasqal_algebra_node_to_rowsource(execution_data, node->node1, error_p);
  if((error_p && *error_p) || !rs)
    return NULL;

  return rasqal_new_sort_rowsource(query->world, query, rs,
                                   node->seq, node->distinct);
}


static rasqal_rowsource*
rasqal_algebra_union_algebra_node_to_rowsource(rasqal_engine_algebra_data* execution_data,
                                               rasqal_algebra_node* node,
                                               rasqal_engine_error *error_p)
{
  rasqal_query *query = execution_data->query;
  rasqal_rowsource *left_rs;
  rasqal_rowsource *right_rs;

  left_rs = rasqal_algebra_node_to_rowsource(execution_data, node->node1,
                                             error_p);
  if((error_p && *error_p) || !left_rs)
    return NULL;

  right_rs = rasqal_algebra_node_to_rowsource(execution_data, node->node2,
                                              error_p);
  if((error_p && *error_p) || !right_rs) {
    rasqal_free_rowsource(left_rs);
    return NULL;
  }

  return rasqal_new_union_rowsource(query->world, query, left_rs, right_rs);
}


static rasqal_rowsource*
rasqal_algebra_project_algebra_node_to_rowsource(rasqal_engine_algebra_data* execution_data,
                                                 rasqal_algebra_node* node,
                                                 rasqal_engine_error *error_p)
{
  rasqal_query *query = execution_data->query;
  rasqal_rowsource *rs;

  rs = rasqal_algebra_node_to_rowsource(execution_data, node->node1,
                                        error_p);
  if((error_p && *error_p) || !rs)
    return NULL;

  return rasqal_new_project_rowsource(query->world, query, rs, node->vars_seq);
}


static rasqal_rowsource*
rasqal_algebra_leftjoin_algebra_node_to_rowsource(rasqal_engine_algebra_data* execution_data,
                                                  rasqal_algebra_node* node,
                                                  rasqal_engine_error *error_p)
{
  rasqal_query *query = execution_data->query;
  rasqal_rowsource *left_rs;
  rasqal_rowsource *right_rs;

  left_rs = rasqal_algebra_node_to_rowsource(execution_data, node->node1,
                                             error_p);
  if((error_p && *error_p) || !left_rs)
    return NULL;

  right_rs = rasqal_algebra_node_to_rowsource(execution_data, node->node2,
                                              error_p);
  if((error_p && *error_p) || !right_rs) {
    rasqal_free_rowsource(left_rs);
    return NULL;
  }

  return rasqal_new_join_rowsource(query->world, query, left_rs, right_rs, RASQAL_JOIN_TYPE_LEFT, node->expr);
}


static rasqal_rowsource*
rasqal_algebra_join_algebra_node_to_rowsource(rasqal_engine_algebra_data* execution_data,
                                              rasqal_algebra_node* node,
                                              rasqal_engine_error *error_p)
{
  rasqal_query *query = execution_data->query;
  rasqal_rowsource *left_rs;
  rasqal_rowsource *right_rs;

  left_rs = rasqal_algebra_node_to_rowsource(execution_data, node->node1,
                                             error_p);
  if((error_p && *error_p) || !left_rs)
    return NULL;

  right_rs = rasqal_algebra_node_to_rowsource(execution_data, node->node2,
                                              error_p);
  if((error_p && *error_p) || !right_rs) {
    rasqal_free_rowsource(left_rs);
    return NULL;
  }

  return rasqal_new_join_rowsource(query->world, query, left_rs, right_rs, RASQAL_JOIN_TYPE_NATURAL, node->expr);
}


static rasqal_rowsource*
rasqal_algebra_assignment_algebra_node_to_rowsource(rasqal_engine_algebra_data* execution_data,
                                                    rasqal_algebra_node* node,
                                                    rasqal_engine_error *error_p)
{
  rasqal_query *query = execution_data->query;

  return rasqal_new_assignment_rowsource(query->world, query, node->var, 
                                         node->expr);
}


static int
rasqal_algebra_visitor_set_origin(rasqal_query* query,
                                  rasqal_algebra_node* node,
                                  void *user_data)
{
  rasqal_literal *origin = (rasqal_literal*)user_data;
  int i;
  
  if(node->op != RASQAL_ALGEBRA_OPERATOR_BGP)
    return 0;

  for(i = node->start_column; i <= node->end_column; i++) {
    rasqal_triple *t;
    rasqal_literal *o = NULL;
    
    t = (rasqal_triple*)raptor_sequence_get_at(node->triples, i);
    if(origin)
      o = rasqal_new_literal_from_literal(origin);
    
    rasqal_triple_set_origin(t, o);
  }
  return 0;
}


static void
rasqal_algebra_node_set_origin(rasqal_query *query,
                               rasqal_algebra_node* node,
                               rasqal_literal *origin) 
{
  rasqal_algebra_node_visit(query, node, 
                            rasqal_algebra_visitor_set_origin,
                            origin);
}


static rasqal_rowsource*
rasqal_algebra_graph_algebra_node_to_rowsource(rasqal_engine_algebra_data* execution_data,
                                               rasqal_algebra_node* node,
                                               rasqal_engine_error *error_p)
{
  rasqal_query *query = execution_data->query;
  rasqal_rowsource *rs;
  rasqal_literal *graph = node->graph;
  rasqal_variable* v;

  if(!graph) {
    RASQAL_DEBUG1("graph algebra node has NULL graph\n");
    return NULL;
  }

/* 
This code checks that #1-#3 below are present and
then executes parts #1 and #2 here.

The graph rowsource created by rasqal_new_graph_rowsource() executes #3


http://www.w3.org/TR/2008/REC-rdf-sparql-query-20080115/#sparqlAlgebraEval

SPARQL Query Language for RDF - Evaluation of a Graph Pattern

#1 if IRI is a graph name in D
eval(D(G), Graph(IRI,P)) = eval(D(D[IRI]), P)

#2 if IRI is not a graph name in D
eval(D(G), Graph(IRI,P)) = the empty multiset

#3 eval(D(G), Graph(var,P)) =
     Let R be the empty multiset
     foreach IRI i in D
        R := Union(R, Join( eval(D(D[i]), P) , Omega(?var->i) )
     the result is R

*/
  v = rasqal_literal_as_variable(graph);
  if(!v && graph->type != RASQAL_LITERAL_URI) {
    /* value is neither a variable or URI literal - error */
    RASQAL_DEBUG1("graph algebra node is neither variable or URI\n");
    return NULL;
  }
  
  if(!v && graph->type == RASQAL_LITERAL_URI) {
    if(rasqal_query_dataset_contains_named_graph(query, graph->value.uri)) {
      /* case #1 - IRI is a graph name in D */

      /* Set the origin of all triple patterns inside node->node1 to
       * URI graph->value.uri
       *
       * FIXME - this is a hack.  The graph URI should be a parameter
       * to all rowsource constructors.
       */
      rasqal_algebra_node_set_origin(query, node->node1, graph);

      rs = rasqal_algebra_node_to_rowsource(execution_data, node->node1,
                                            error_p);
    } else {
      /* case #2 - IRI is not a graph name in D - return empty rowsource */
      rasqal_free_algebra_node(node->node1);
      node->node1 = NULL;
      
      rs = rasqal_new_empty_rowsource(query->world, query);
    }

    if((error_p && *error_p) && rs) {
      rasqal_free_rowsource(rs);
      rs = NULL;
    }

    return rs;
  }


  /* case #3 - a variable */
  rs = rasqal_algebra_node_to_rowsource(execution_data, node->node1, error_p);
  if((error_p && *error_p) || !rs)
    return NULL;

  return rasqal_new_graph_rowsource(query->world, query, rs, v);
}


static rasqal_rowsource*
rasqal_algebra_distinct_algebra_node_to_rowsource(rasqal_engine_algebra_data* execution_data,
                                                  rasqal_algebra_node* node,
                                                  rasqal_engine_error *error_p)
{
  rasqal_query *query = execution_data->query;
  rasqal_rowsource *rs;

  rs = rasqal_algebra_node_to_rowsource(execution_data, node->node1, error_p);
  if((error_p && *error_p) || !rs)
    return NULL;

  return rasqal_new_distinct_rowsource(query->world, query, rs);
}


static rasqal_rowsource*
rasqal_algebra_group_algebra_node_to_rowsource(rasqal_engine_algebra_data* execution_data,
                                               rasqal_algebra_node* node,
                                               rasqal_engine_error *error_p)
{
  rasqal_query *query = execution_data->query;
  rasqal_rowsource *rs;

  rs = rasqal_algebra_node_to_rowsource(execution_data, node->node1, error_p);
  if((error_p && *error_p) || !rs)
    return NULL;

  return rasqal_new_groupby_rowsource(query->world, query, rs, node->seq);
}


static rasqal_rowsource*
rasqal_algebra_aggregation_algebra_node_to_rowsource(rasqal_engine_algebra_data* execution_data,
                                                     rasqal_algebra_node* node,
                                                     rasqal_engine_error *error_p)
{
  rasqal_query *query = execution_data->query;
  rasqal_rowsource *rs;

  rs = rasqal_algebra_node_to_rowsource(execution_data, node->node1, error_p);
  if((error_p && *error_p) || !rs)
    return NULL;

  return rasqal_new_aggregation_rowsource(query->world, query, rs,
                                          node->seq,
                                          node->vars_seq);
}


static rasqal_rowsource*
rasqal_algebra_having_algebra_node_to_rowsource(rasqal_engine_algebra_data* execution_data,
                                                rasqal_algebra_node* node,
                                                rasqal_engine_error *error_p)
{
  rasqal_query *query = execution_data->query;
  rasqal_rowsource *rs;

  rs = rasqal_algebra_node_to_rowsource(execution_data, node->node1, error_p);
  if((error_p && *error_p) || !rs)
    return NULL;

  return rasqal_new_having_rowsource(query->world, query, rs, node->seq);
}


static rasqal_rowsource*
rasqal_algebra_slice_algebra_node_to_rowsource(rasqal_engine_algebra_data* execution_data,
                                               rasqal_algebra_node* node,
                                               rasqal_engine_error *error_p)
{
  rasqal_query *query = execution_data->query;
  rasqal_rowsource *rs;

  rs = rasqal_algebra_node_to_rowsource(execution_data, node->node1, error_p);
  if((error_p && *error_p) || !rs)
    return NULL;

  return rasqal_new_slice_rowsource(query->world, query, rs, node->limit, node->offset);
}


static rasqal_rowsource*
rasqal_algebra_values_algebra_node_to_rowsource(rasqal_engine_algebra_data* execution_data,
                                                rasqal_algebra_node* node,
                                                rasqal_engine_error *error_p)
{
  rasqal_query *query = execution_data->query;
  rasqal_bindings* bindings = rasqal_new_bindings_from_bindings(node->bindings);
  return rasqal_new_bindings_rowsource(query->world, query, bindings);
}


static rasqal_rowsource*
rasqal_algebra_service_algebra_node_to_rowsource(rasqal_engine_algebra_data* execution_data,
                                                 rasqal_algebra_node* node,
                                                 rasqal_engine_error *error_p)
{
  rasqal_query *query = execution_data->query;
  unsigned int flags = (node->flags & RASQAL_ENGINE_BITFLAG_SILENT);

  return rasqal_new_service_rowsource(query->world, query,
                                      node->service_uri,
                                      node->query_string,
                                      node->data_graphs,
                                      flags);
}


static rasqal_rowsource*
rasqal_algebra_node_to_rowsource(rasqal_engine_algebra_data* execution_data,
                                 rasqal_algebra_node* node,
                                 rasqal_engine_error *error_p)
{
  rasqal_rowsource* rs = NULL;
  
  switch(node->op) {
    case RASQAL_ALGEBRA_OPERATOR_BGP:
      rs = rasqal_algebra_basic_algebra_node_to_rowsource(execution_data,
                                                          node, error_p);
      break;

    case RASQAL_ALGEBRA_OPERATOR_FILTER:
      rs = rasqal_algebra_filter_algebra_node_to_rowsource(execution_data,
                                                           node, error_p);
      break;

    case RASQAL_ALGEBRA_OPERATOR_ORDERBY:
      rs = rasqal_algebra_orderby_algebra_node_to_rowsource(execution_data,
                                                            node, error_p);
      break;

    case RASQAL_ALGEBRA_OPERATOR_UNION:
      rs = rasqal_algebra_union_algebra_node_to_rowsource(execution_data,
                                                          node, error_p);
      break;

    case RASQAL_ALGEBRA_OPERATOR_PROJECT:
      rs = rasqal_algebra_project_algebra_node_to_rowsource(execution_data,
                                                            node, error_p);
      break;

    case RASQAL_ALGEBRA_OPERATOR_LEFTJOIN:
      rs = rasqal_algebra_leftjoin_algebra_node_to_rowsource(execution_data,
                                                             node, error_p);
      break;

    case RASQAL_ALGEBRA_OPERATOR_GRAPH:
      rs = rasqal_algebra_graph_algebra_node_to_rowsource(execution_data,
                                                          node, error_p);
      break;

    case RASQAL_ALGEBRA_OPERATOR_DISTINCT:
      rs = rasqal_algebra_distinct_algebra_node_to_rowsource(execution_data,
                                                             node, error_p);
      break;

    case RASQAL_ALGEBRA_OPERATOR_JOIN:
      rs = rasqal_algebra_join_algebra_node_to_rowsource(execution_data,
                                                         node, error_p);
      break;

    case RASQAL_ALGEBRA_OPERATOR_ASSIGN:
      rs = rasqal_algebra_assignment_algebra_node_to_rowsource(execution_data,
                                                               node, error_p);
      break;

    case RASQAL_ALGEBRA_OPERATOR_GROUP:
      rs = rasqal_algebra_group_algebra_node_to_rowsource(execution_data,
                                                          node, error_p);
      break;

    case RASQAL_ALGEBRA_OPERATOR_AGGREGATION:
      rs = rasqal_algebra_aggregation_algebra_node_to_rowsource(execution_data,
                                                                node, error_p);
      break;

    case RASQAL_ALGEBRA_OPERATOR_HAVING:
      rs = rasqal_algebra_having_algebra_node_to_rowsource(execution_data,
                                                           node, error_p);
      break;

    case RASQAL_ALGEBRA_OPERATOR_SLICE:
      rs = rasqal_algebra_slice_algebra_node_to_rowsource(execution_data,
                                                          node, error_p);
      break;

    case RASQAL_ALGEBRA_OPERATOR_VALUES:
      rs = rasqal_algebra_values_algebra_node_to_rowsource(execution_data,
                                                           node, error_p);
      break;

    case RASQAL_ALGEBRA_OPERATOR_SERVICE:
      rs = rasqal_algebra_service_algebra_node_to_rowsource(execution_data,
                                                            node, error_p);
      break;

    case RASQAL_ALGEBRA_OPERATOR_UNKNOWN:
    case RASQAL_ALGEBRA_OPERATOR_DIFF:
    case RASQAL_ALGEBRA_OPERATOR_TOLIST:
    case RASQAL_ALGEBRA_OPERATOR_REDUCED:
    default:
      RASQAL_DEBUG2("Unsupported algebra node operator %s\n",
                    rasqal_algebra_node_operator_as_counted_string(node->op,
                                                                   NULL));
      break;
  }

  if(!rs)
    *error_p = RASQAL_ENGINE_FAILED;
  
  return rs;
}



static int
rasqal_query_engine_algebra_execute_init(void* ex_data,
                                         rasqal_query* query,
                                         rasqal_query_results* query_results,
                                         int flags,
                                         rasqal_engine_error *error_p)
{
  rasqal_engine_algebra_data* execution_data;
  rasqal_engine_error error;
  int rc = 0;
  rasqal_projection* projection;
  rasqal_solution_modifier* modifier;
  rasqal_algebra_node* node;
  rasqal_algebra_aggregate* ae;
  
  execution_data = (rasqal_engine_algebra_data*)ex_data;

  /* initialise the execution_data fields */
  execution_data->query = query;
  execution_data->query_results = query_results;

  if(!execution_data->triples_source) {
    execution_data->triples_source = rasqal_new_triples_source(execution_data->query);
    if(!execution_data->triples_source) {
      *error_p = RASQAL_ENGINE_FAILED;
      return 1;
    }
  }

  projection = rasqal_query_get_projection(query);
  modifier = query->modifier;

  node = rasqal_algebra_query_to_algebra(query);
  if(!node)
    return 1;

  node = rasqal_algebra_query_add_group_by(query, node, modifier);
  if(!node)
    return 1;

  ae = rasqal_algebra_query_prepare_aggregates(query, node, projection,
                                               modifier);
  if(!ae)
    return 1;

  if(ae) {
    node = rasqal_algebra_query_add_aggregation(query, ae, node);
    ae = NULL;
    if(!node)
      return 1;
  }

  node = rasqal_algebra_query_add_having(query, node, modifier);
  if(!node)
    return 1;

  if(query->verb == RASQAL_QUERY_VERB_SELECT) {
    node = rasqal_algebra_query_add_projection(query, node, projection);
    if(!node)
      return 1;
  } else if(query->verb == RASQAL_QUERY_VERB_CONSTRUCT) {
    node = rasqal_algebra_query_add_construct_projection(query, node);
    if(!node)
      return 1;
  }

  node = rasqal_algebra_query_add_orderby(query, node, projection, modifier);
  if(!node)
    return 1;

  node = rasqal_algebra_query_add_distinct(query, node, projection);
  if(!node)
    return 1;


  execution_data->algebra_node = node;

  /* count final number of nodes */
  execution_data->nodes_count = 0; 
  rasqal_algebra_node_visit(query,
                            execution_data->algebra_node,
                            rasqal_engine_algebra_count_nodes,
                            &execution_data->nodes_count);
  
  
#ifdef RASQAL_DEBUG
  RASQAL_DEBUG1("algebra result: \n");
  rasqal_algebra_node_print(node, DEBUG_FH);
  fputc('\n', DEBUG_FH);
#endif
  RASQAL_DEBUG2("algebra nodes: %d\n", execution_data->nodes_count);

  error = RASQAL_ENGINE_OK;
  execution_data->rowsource = rasqal_algebra_node_to_rowsource(execution_data,
                                                               node,
                                                               &error);
#ifdef RASQAL_DEBUG
  RASQAL_DEBUG1("rowsource (query plan) result: \n");
  if(execution_data->rowsource)
    rasqal_rowsource_print(execution_data->rowsource, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fputc('\n', DEBUG_FH);
#endif
  if(error != RASQAL_ENGINE_OK)
    rc = 1;
  
  return rc;
}


static raptor_sequence*
rasqal_query_engine_algebra_get_all_rows(void* ex_data,
                                         rasqal_engine_error *error_p)
{
  rasqal_engine_algebra_data* execution_data;
  raptor_sequence *seq = NULL;
  
  execution_data = (rasqal_engine_algebra_data*)ex_data;

  if(execution_data->rowsource) {
    seq = rasqal_rowsource_read_all_rows(execution_data->rowsource);
    if(!seq)
      *error_p = RASQAL_ENGINE_FAILED;
  } else
    *error_p = RASQAL_ENGINE_FAILED;

  return seq;
}


static rasqal_row*
rasqal_query_engine_algebra_get_row(void* ex_data,
                                    rasqal_engine_error *error_p)
{
  rasqal_engine_algebra_data* execution_data;
  rasqal_row *row = NULL;
  
  execution_data = (rasqal_engine_algebra_data*)ex_data;

  if(execution_data->rowsource) {
    row = rasqal_rowsource_read_row(execution_data->rowsource);
    if(!row)
      *error_p = RASQAL_ENGINE_FINISHED;
  } else
    *error_p = RASQAL_ENGINE_FAILED;

  return row;
}


static int
rasqal_query_engine_algebra_execute_finish(void* ex_data,
                                           rasqal_engine_error *error_p)
{
  rasqal_engine_algebra_data* execution_data;

  execution_data = (rasqal_engine_algebra_data*)ex_data;

  if(execution_data) {
    if(execution_data->algebra_node)
      rasqal_free_algebra_node(execution_data->algebra_node);

    if(execution_data->triples_source) {
      rasqal_free_triples_source(execution_data->triples_source);
      execution_data->triples_source = NULL;
    }

    if(execution_data->rowsource)
      rasqal_free_rowsource(execution_data->rowsource);
  }

  return 0;
}


static void
rasqal_query_engine_algebra_finish_factory(rasqal_query_execution_factory* factory)
{
  return;
}


const rasqal_query_execution_factory rasqal_query_engine_algebra =
{
  /* .name=                */ "rasqal query algebra query engine",
  /* .execution_data_size= */ sizeof(rasqal_engine_algebra_data),
  /* .execute_init=        */ rasqal_query_engine_algebra_execute_init,
  /* .get_all_rows=        */ rasqal_query_engine_algebra_get_all_rows,
  /* .get_row=             */ rasqal_query_engine_algebra_get_row,
  /* .execute_finish=      */ rasqal_query_engine_algebra_execute_finish,
  /* .finish_factory=      */ rasqal_query_engine_algebra_finish_factory
};

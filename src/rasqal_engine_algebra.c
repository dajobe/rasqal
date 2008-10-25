/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_engine_algebra.c - Rasqal query engine over query algebra
 *
 * Copyright (C) 2008, David Beckett http://www.dajobe.org/
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
  
  return rasqal_new_triples_rowsource(query,
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

  rs = rasqal_algebra_node_to_rowsource(execution_data, node->node1, error_p);
  if(!rs || *error_p)
    return NULL;

  return rasqal_new_filter_rowsource(query, rs, node->expr);
}


static rasqal_rowsource*
rasqal_algebra_orderby_algebra_node_to_rowsource(rasqal_engine_algebra_data* execution_data,
                                                rasqal_algebra_node* node,
                                                rasqal_engine_error *error_p)
{
  rasqal_query *query = execution_data->query;
  rasqal_rowsource *rs;

  rs = rasqal_algebra_node_to_rowsource(execution_data, node->node1, error_p);
  if(!rs || *error_p)
    return NULL;

  return rasqal_new_sort_rowsource(query, rs, node->seq);
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

    case RASQAL_ALGEBRA_OPERATOR_UNKNOWN:
    case RASQAL_ALGEBRA_OPERATOR_JOIN:
    case RASQAL_ALGEBRA_OPERATOR_DIFF:
    case RASQAL_ALGEBRA_OPERATOR_LEFTJOIN:
    case RASQAL_ALGEBRA_OPERATOR_UNION:
    case RASQAL_ALGEBRA_OPERATOR_TOLIST:
    case RASQAL_ALGEBRA_OPERATOR_PROJECT:
    case RASQAL_ALGEBRA_OPERATOR_DISTINCT:
    case RASQAL_ALGEBRA_OPERATOR_REDUCED:
    case RASQAL_ALGEBRA_OPERATOR_SLICE:
    default:
      RASQAL_DEBUG2("Unsupported algebra node operator %s\n",
                    rasqal_algebra_node_operator_as_string(node->op));
      break;
  }

#if RASQAL_DEBUG
  if(!rs)
    abort();
#endif
  
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

  execution_data->algebra_node = rasqal_algebra_query_to_algebra(query);
  if(!execution_data->algebra_node)
    return 1;

  execution_data->nodes_count = 0; 
  rasqal_algebra_node_visit(query, execution_data->algebra_node,
                            rasqal_engine_algebra_count_nodes,
                            &execution_data->nodes_count);
  
#ifdef RASQAL_DEBUG
  RASQAL_DEBUG1("algebra result: \n");
  rasqal_algebra_node_print(execution_data->algebra_node, DEBUG_FH);
  fputc('\n', DEBUG_FH);
#endif
  RASQAL_DEBUG2("algebra nodes: %d\n", execution_data->nodes_count);

  error = RASQAL_ENGINE_OK;
  execution_data->rowsource = rasqal_algebra_node_to_rowsource(execution_data,
                                                               execution_data->algebra_node,
                                                               &error);
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

  if(execution_data->algebra_node)
    rasqal_free_algebra_node(execution_data->algebra_node);

  if(execution_data->triples_source)
    rasqal_free_triples_source(execution_data->triples_source);

  if(execution_data->rowsource)
    rasqal_free_rowsource(execution_data->rowsource);

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

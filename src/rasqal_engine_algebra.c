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
} rasqal_engine_algebra_data;



static int
rasqal_engine_algebra_count_nodes(rasqal_query* query,
                                  rasqal_algebra_node* node,
                                  void* data)
{
  int *count_p=(int*)data;
  
  (*count_p)++;
  
  return 0;
}


static int
rasqal_query_engine_algebra_execute_init(void* ex_data,
                                         rasqal_query* query,
                                         rasqal_query_results* query_results,
                                         int flags,
                                         rasqal_engine_error *error_p)
{
  rasqal_engine_algebra_data* execution_data;
  
  execution_data=(rasqal_engine_algebra_data*)ex_data;

  /* initialise the execution_data fields */
  execution_data->query = query;
  execution_data->query_results = query_results;

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

  return 0;
}


static raptor_sequence*
rasqal_query_engine_algebra_get_all_rows(void* ex_data,
                                         rasqal_engine_error *error_p)
{
  return NULL;
}


static rasqal_row*
rasqal_query_engine_algebra_get_row(void* ex_data,
                                    rasqal_engine_error *error_p)
{
  return NULL;
}


static int
rasqal_query_engine_algebra_execute_finish(void* ex_data,
                                           rasqal_engine_error *error_p)
{
  rasqal_engine_algebra_data* execution_data;

  execution_data=(rasqal_engine_algebra_data*)ex_data;

  if(execution_data->algebra_node)
    rasqal_free_algebra_node(execution_data->algebra_node);

  return 1;
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

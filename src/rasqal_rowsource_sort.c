/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_source.c - Rasqal source rowsource class
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

#include <raptor.h>

#include "rasqal.h"
#include "rasqal_internal.h"


#define DEBUG_FH stderr


typedef struct 
{
  /* inner rowsource to sort */
  rasqal_rowsource *rowsource;

  /* sequence of order conditions #rasqal_expression (SHARED with query) */
  raptor_sequence* order_seq;

  /* number of order conditions in order_seq */
  int order_size;

  /* distinct flag */
  int distinct;

  /* map for sorting */
  rasqal_map* map;

  /* sequence of rows (owned here) */
  raptor_sequence* seq;
} rasqal_sort_rowsource_context;


static int
rasqal_sort_rowsource_init(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_query *query = rowsource->query;
  rasqal_sort_rowsource_context *con;

  con = (rasqal_sort_rowsource_context*)user_data;
  
  if(con->order_seq)
    con->order_size = raptor_sequence_size(con->order_seq);
  else {
    RASQAL_DEBUG1("No order conditions for sort rowsource - passing through");
    con->order_size = -1;
  }
  
  con->map = NULL;

  if(con->order_size > 0 ) {
    /* make a row:NULL map in order to sort or do distinct
     * FIXME: should DISTINCT be separate? 
     */
    con->map = rasqal_engine_new_rowsort_map(con->distinct,
                                             query->compare_flags,
                                             con->order_seq);
    if(!con->map)
      return 1;
  }
  
  con->seq = NULL;

  return 0;
}


static int
rasqal_sort_rowsource_process(rasqal_rowsource* rowsource,
                              rasqal_sort_rowsource_context* con)
{
  int offset = 0;

  /* already processed */
  if(con->seq)
    return 0;

  con->seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_row,
                                 (raptor_data_print_handler)rasqal_row_print);
  if(!con->seq)
    return 1;
  
  while(1) {
    rasqal_row* row;

    row = rasqal_rowsource_read_row(con->rowsource);
    if(!row)
      break;

    if(rasqal_row_set_order_size(row, con->order_size)) {
      rasqal_free_row(row);
      return 1;
    }

    rasqal_engine_rowsort_calculate_order_values(rowsource->query, con->order_seq, row);

    row->offset = offset;

    /* after this, row is owned by map */
    if(!rasqal_engine_rowsort_map_add_row(con->map, row))
      offset++;
  }
  
#ifdef RASQAL_DEBUG
  fputs("resulting ", DEBUG_FH);
  rasqal_map_print(con->map, DEBUG_FH);
  fputs("\n", DEBUG_FH);
#endif
  
  /* do sort/distinct: walk map in order, adding rows to sequence */
  rasqal_engine_rowsort_map_to_sequence(con->map, con->seq);
  rasqal_free_map(con->map); con->map = NULL;

  return 0;
}


static int
rasqal_sort_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                       void *user_data)
{
  rasqal_sort_rowsource_context* con;
  con = (rasqal_sort_rowsource_context*)user_data; 

  if(rasqal_rowsource_ensure_variables(con->rowsource))
    return 1;

  rowsource->size = 0;
  rasqal_rowsource_copy_variables(rowsource, con->rowsource);
  
  return 0;
}


static int
rasqal_sort_rowsource_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_sort_rowsource_context *con;
  con = (rasqal_sort_rowsource_context*)user_data;

  if(con->rowsource)
    rasqal_free_rowsource(con->rowsource);
  
  if(con->map)
    rasqal_free_map(con->map);

  if(con->seq)
    raptor_free_sequence(con->seq);

  RASQAL_FREE(rasqal_sort_rowsource_context, con);

  return 0;
}


static raptor_sequence*
rasqal_sort_rowsource_read_all_rows(rasqal_rowsource* rowsource,
                                    void *user_data)
{
  rasqal_sort_rowsource_context *con;
  raptor_sequence *seq = NULL;
  
  con = (rasqal_sort_rowsource_context*)user_data;

  /* if there were no ordering conditions, pass it all on to inner rowsource */
  if(con->order_size <= 0)
    return rasqal_rowsource_read_all_rows(con->rowsource);


  /* need to sort */
  if(rasqal_sort_rowsource_process(rowsource, con))
    return NULL;

  if(con->seq) {
    /* pass ownership of seq back to caller */
    seq = con->seq;
    con->seq = NULL;
  }
  
  return seq;
}


static rasqal_rowsource*
rasqal_sort_rowsource_get_inner_rowsource(rasqal_rowsource* rowsource,
                                          void *user_data, int offset)
{
  rasqal_sort_rowsource_context *con;
  con = (rasqal_sort_rowsource_context*)user_data;

  if(offset == 0)
    return con->rowsource;
  return NULL;
}

 
static const rasqal_rowsource_handler rasqal_sort_rowsource_handler = {
  /* .version =          */ 1,
  "sort",
  /* .init =             */ rasqal_sort_rowsource_init,
  /* .finish =           */ rasqal_sort_rowsource_finish,
  /* .ensure_variables = */ rasqal_sort_rowsource_ensure_variables,
  /* .read_row =         */ NULL,
  /* .read_all_rows =    */ rasqal_sort_rowsource_read_all_rows,
  /* .reset =            */ NULL,
  /* .set_requirements = */ NULL,
  /* .get_inner_rowsource = */ rasqal_sort_rowsource_get_inner_rowsource,
  /* .set_origin =       */ NULL,
};


/**
 * rasqal_new_sort_rowsource:
 * @world: query world
 * @query: query results object
 * @rowsource: input rowsource
 * @order_seq: order sequence (shared, may be NULL)
 * @distinct: distinct flag
 *
 * INTERNAL - create a SORT over rows from input rowsource
 *
 * The @rowsource becomes owned by the new rowsource.
 *
 * Return value: new rowsource or NULL on failure
 */
rasqal_rowsource*
rasqal_new_sort_rowsource(rasqal_world *world,
                          rasqal_query *query,
                          rasqal_rowsource *rowsource,
                          raptor_sequence* order_seq,
                          int distinct)
{
  rasqal_sort_rowsource_context *con;
  int flags = 0;

  if(!world || !query || !rowsource)
    goto fail;
  
  con = RASQAL_CALLOC(rasqal_sort_rowsource_context*, 1, sizeof(*con));
  if(!con)
    goto fail;

  con->rowsource = rowsource;
  con->order_seq = order_seq;
  con->distinct = distinct;

  return rasqal_new_rowsource_from_handler(world, query,
                                           con,
                                           &rasqal_sort_rowsource_handler,
                                           query->vars_table,
                                           flags);

  fail:
  if(rowsource)
    rasqal_free_rowsource(rowsource);
  return NULL;
}

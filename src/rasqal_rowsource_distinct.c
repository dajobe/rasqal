/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_distinct.c - Rasqal distinct rowsource class
 *
 * Copyright (C) 2009, David Beckett http://www.dajobe.org/
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
  /* inner rowsource to distinct */
  rasqal_rowsource *rowsource;

  /* map for distincting row values */
  rasqal_map* map;

  /* offset into results for current row */
  int offset;
  
} rasqal_distinct_rowsource_context;


static int
rasqal_distinct_rowsource_init_common(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_query *query = rowsource->query;
  rasqal_distinct_rowsource_context *con;

  con = (rasqal_distinct_rowsource_context*)user_data;
  
  con->offset = 0;

  con->map = rasqal_engine_new_rowsort_map(1, query->compare_flags, NULL);
  if(!con->map)
    return 1;

  return 0;
}


static int
rasqal_distinct_rowsource_init(rasqal_rowsource* rowsource, void *user_data)
{
/*
  rasqal_distinct_rowsource_context *con;

  con = (rasqal_distinct_rowsource_context*)user_data;
*/

  return rasqal_distinct_rowsource_init_common(rowsource, user_data);
}


static int
rasqal_distinct_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                           void *user_data)
{
  rasqal_distinct_rowsource_context* con;
  
  con = (rasqal_distinct_rowsource_context*)user_data; 

  rasqal_rowsource_ensure_variables(con->rowsource);

  rowsource->size = 0;
  rasqal_rowsource_copy_variables(rowsource, con->rowsource);
  
  return 0;
}


static int
rasqal_distinct_rowsource_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_distinct_rowsource_context *con;
  con = (rasqal_distinct_rowsource_context*)user_data;

  if(con->rowsource)
    rasqal_free_rowsource(con->rowsource);
  
  if(con->map)
    rasqal_free_map(con->map);

  RASQAL_FREE(rasqal_distinct_rowsource_context, con);

  return 0;
}


static rasqal_row*
rasqal_distinct_rowsource_read_row(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_distinct_rowsource_context *con;
  rasqal_row *row = NULL;
  
  con = (rasqal_distinct_rowsource_context*)user_data;

  while(1) {
    int result;

    row = rasqal_rowsource_read_row(con->rowsource);
    if(!row)
      break;

    result = rasqal_engine_rowsort_map_add_row(con->map, row);
    RASQAL_DEBUG2("row is %s\n", result ? "not distinct" : "distinct");

    if(!result)
      /* row was distinct (not a duplicate) so return it */
      break;
  }

  if(row) {
    row = rasqal_new_row_from_row(row);
    rasqal_row_set_rowsource(row, rowsource);
    row->offset = con->offset++;
  }
  
  return row;
}


static int
rasqal_distinct_rowsource_reset(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_distinct_rowsource_context *con;
  int rc;

  con = (rasqal_distinct_rowsource_context*)user_data;

  if(con->map)
    rasqal_free_map(con->map);

  rc = rasqal_distinct_rowsource_init_common(rowsource, user_data);
  if(rc)
    return rc;

  return rasqal_rowsource_reset(con->rowsource);
}


static rasqal_rowsource*
rasqal_distinct_rowsource_get_inner_rowsource(rasqal_rowsource* rowsource,
                                              void *user_data, int offset)
{
  rasqal_distinct_rowsource_context *con;
  con = (rasqal_distinct_rowsource_context*)user_data;

  if(offset == 0)
    return con->rowsource;
  return NULL;
}


static const rasqal_rowsource_handler rasqal_distinct_rowsource_handler = {
  /* .version =          */ 1,
  "distinct",
  /* .init =             */ rasqal_distinct_rowsource_init,
  /* .finish =           */ rasqal_distinct_rowsource_finish,
  /* .ensure_variables = */ rasqal_distinct_rowsource_ensure_variables,
  /* .read_row =         */ rasqal_distinct_rowsource_read_row,
  /* .read_all_rows =    */ NULL,
  /* .reset =            */ rasqal_distinct_rowsource_reset,
  /* .set_requirements = */ NULL,
  /* .get_inner_rowsource = */ rasqal_distinct_rowsource_get_inner_rowsource,
  /* .set_origin =       */ NULL,
};


/**
 * rasqal_new_distinct_rowsource:
 * @world: world object
 * @query: query object
 * @rowsource: input rowsource
 *
 * INTERNAL - create a new DISTINCT rowsoruce
 *
 * The @rowsource becomes owned by the new rowsource
 *
 * Return value: new rowsource or NULL on failure
 */
rasqal_rowsource*
rasqal_new_distinct_rowsource(rasqal_world *world,
                              rasqal_query *query,
                              rasqal_rowsource* rowsource)
{
  rasqal_distinct_rowsource_context *con;
  int flags = 0;
  
  if(!world || !query || !rowsource)
    goto fail;
  
  con = RASQAL_CALLOC(rasqal_distinct_rowsource_context*, 1, sizeof(*con));
  if(!con)
    goto fail;

  con->rowsource = rowsource;

  return rasqal_new_rowsource_from_handler(world, query,
                                           con,
                                           &rasqal_distinct_rowsource_handler,
                                           query->vars_table,
                                           flags);

  fail:
  if(rowsource)
    rasqal_free_rowsource(rowsource);
  return NULL;
}

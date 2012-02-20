/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_slice.c - Rasqal slice rows rowsource class
 *
 * Copyright (C) 2011, David Beckett http://www.dajobe.org/
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
  /* inner rowsource to slice */
  rasqal_rowsource *rowsource;

  int row_limit;
  int row_offset;

  /* offset for input row */
  int input_offset;

  /* offset for output row */
  int output_offset;
} rasqal_slice_rowsource_context;


static int
rasqal_slice_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                         void *user_data)
{
  rasqal_slice_rowsource_context* con;
  
  con = (rasqal_slice_rowsource_context*)user_data; 

  if(rasqal_rowsource_ensure_variables(con->rowsource))
    return 1;

  rowsource->size = 0;
  if(rasqal_rowsource_copy_variables(rowsource, con->rowsource))
    return 1;
  
  return 0;
}


static int
rasqal_slice_rowsource_init(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_slice_rowsource_context *con;

  con = (rasqal_slice_rowsource_context*)user_data;

  con->input_offset = 1;
  con->output_offset = 1;

  return 0;
}


static int
rasqal_slice_rowsource_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_slice_rowsource_context *con;

  con = (rasqal_slice_rowsource_context*)user_data;

  if(con->rowsource)
    rasqal_free_rowsource(con->rowsource);
  
  RASQAL_FREE(rasqal_slice_rowsource_context, con);

  return 0;
}


static rasqal_row*
rasqal_slice_rowsource_read_row(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_slice_rowsource_context *con;
  rasqal_row *row = NULL;
  
  con = (rasqal_slice_rowsource_context*)user_data;

  while(1) {
    int check;

    row = rasqal_rowsource_read_row(con->rowsource);
    if(!row)
      break;

    check = rasqal_query_check_limit_offset_core(con->input_offset,
                                                 con->row_limit, con->row_offset);

    RASQAL_DEBUG4("slice rowsource %p found row #%d %s\n",
                  rowsource, con->input_offset,
                  (check > 0) ? "beyond range" : (!check ? "in range" : "before range"));

    con->input_offset++;

    /* finished if beyond result range */
    if(check > 0) {
      rasqal_free_row(row); row = NULL;
      break;
    }

    /* in range, return row */
    if(!check)
      break;
      
    /* otherwise before the start of result range so continue */
    rasqal_free_row(row); row = NULL;
  }

  if(row)
    row->offset = con->output_offset++;
  
  return row;
}


static int
rasqal_slice_rowsource_reset(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_slice_rowsource_context *con;

  con = (rasqal_slice_rowsource_context*)user_data;

  con->input_offset = 1;
  con->output_offset = 1;

  return rasqal_rowsource_reset(con->rowsource);
}


static rasqal_rowsource*
rasqal_slice_rowsource_get_inner_rowsource(rasqal_rowsource* rowsource,
                                            void *user_data, int offset)
{
  rasqal_slice_rowsource_context *con;
  con = (rasqal_slice_rowsource_context*)user_data;

  if(offset == 0)
    return con->rowsource;

  return NULL;
}


static const rasqal_rowsource_handler rasqal_slice_rowsource_handler = {
  /* .version =          */ 1,
  "slice",
  /* .init =             */ rasqal_slice_rowsource_init,
  /* .finish =           */ rasqal_slice_rowsource_finish,
  /* .ensure_variables = */ rasqal_slice_rowsource_ensure_variables,
  /* .read_row =         */ rasqal_slice_rowsource_read_row,
  /* .read_all_rows =    */ NULL,
  /* .reset =            */ rasqal_slice_rowsource_reset,
  /* .set_requirements = */ NULL,
  /* .get_inner_rowsource = */ rasqal_slice_rowsource_get_inner_rowsource,
  /* .set_origin =       */ NULL,
};


/**
 * rasqal_new_slice_rowsource:
 * @world: world object
 * @query: query object
 * @rowsource: input rowsource
 * @limit: max rows limit (or <0 for no limit)
 * @offset: start row offset (or <0 for no offset)
 *
 * INTERNAL - create a new slice (LIMIT, OFFSET) rowsource
 *
 * The @rowsource becomes owned by the new rowsource
 *
 * Return value: new rowsource or NULL on failure
 */
rasqal_rowsource*
rasqal_new_slice_rowsource(rasqal_world *world,
                           rasqal_query *query,
                           rasqal_rowsource* rowsource,
                           int limit,
                           int offset)
{
  rasqal_slice_rowsource_context *con;
  int flags = 0;
  
  if(!world || !query || !rowsource)
    goto fail;
  
  con = RASQAL_CALLOC(rasqal_slice_rowsource_context*, 1, sizeof(*con));
  if(!con)
    goto fail;

  con->rowsource = rowsource;
  con->row_limit = limit;
  con->row_offset = offset;

  return rasqal_new_rowsource_from_handler(world, query,
                                           con,
                                           &rasqal_slice_rowsource_handler,
                                           query->vars_table,
                                           flags);

  fail:
  if(rowsource)
    rasqal_free_rowsource(rowsource);
  return NULL;
}

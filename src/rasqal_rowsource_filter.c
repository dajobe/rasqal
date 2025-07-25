/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_filter.c - Rasqal filter rowsource class
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
  /* inner rowsource to filter */
  rasqal_rowsource *rowsource;

  /* FILTER expression */
  rasqal_expression* expr;

  /* offset into results for current row */
  int offset;
  
} rasqal_filter_rowsource_context;


static int
rasqal_filter_rowsource_init(rasqal_rowsource* rowsource, void *user_data)
{
  return 0;
}


static int
rasqal_filter_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                         void *user_data)
{
  rasqal_filter_rowsource_context* con;
  
  con = (rasqal_filter_rowsource_context*)user_data; 

  if(rasqal_rowsource_ensure_variables(con->rowsource))
    return 1;

  rowsource->size = 0;
  if(rasqal_rowsource_copy_variables(rowsource, con->rowsource))
    return 1;
  
  return 0;
}


static int
rasqal_filter_rowsource_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_filter_rowsource_context *con;
  con = (rasqal_filter_rowsource_context*)user_data;

  if(con->rowsource)
    rasqal_free_rowsource(con->rowsource);
  
  if(con->expr)
    rasqal_free_expression(con->expr);

  RASQAL_FREE(rasqal_filter_rowsource_context, con);

  return 0;
}


static rasqal_row*
rasqal_filter_rowsource_read_row(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_query *query = rowsource->query;
  rasqal_filter_rowsource_context *con;
  rasqal_row *row = NULL;
  
  con = (rasqal_filter_rowsource_context*)user_data;

  while(1) {
    rasqal_literal* result;
    int bresult = 1;
    int error = 0;
    
    row = rasqal_rowsource_read_row(con->rowsource);
    if(!row)
      break;

    result = rasqal_expression_evaluate2(con->expr, query->eval_context,
                                         &error);
#ifdef RASQAL_DEBUG
    RASQAL_DEBUG1("filter expression result: ");
    if(error)
      fputs("type error", DEBUG_FH);
    else
      rasqal_literal_print(result, DEBUG_FH);
    fputc('\n', DEBUG_FH);
#endif
    if(error) {
      bresult = 0;
      /* Set expression error on rowsource */
      rasqal_log_trace_simple(rowsource->world, NULL,
                             "Filter expression evaluation failed (error: %d)", error);
    } else {
      error = 0;
      bresult = rasqal_literal_as_boolean(result, &error);
#ifdef RASQAL_DEBUG
      if(error)
        RASQAL_DEBUG1("filter boolean expression returned error\n");
      else
        RASQAL_DEBUG2("filter boolean expression result: %d\n", bresult);
#endif
      if(error) {
        /* Set boolean conversion error on rowsource */
        rasqal_log_trace_simple(rowsource->world, NULL,
                             "Filter expression boolean conversion failed (error: %d)", error);
      }
      rasqal_free_literal(result);
    }
    if(bresult)
      /* Constraint succeeded so end */
      break;

    rasqal_free_row(row); row = NULL;
  }

  if(row) {
    int i;
    
    for(i = 0; i < row->size; i++) {
      rasqal_variable* v;
      v = rasqal_rowsource_get_variable_by_offset(rowsource, i);
      if(row->values[i])
        rasqal_free_literal(row->values[i]);
      row->values[i] = rasqal_new_literal_from_literal(v->value);
    }

    row->offset = con->offset++;
  }
  
  return row;
}


static int
rasqal_filter_rowsource_reset(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_filter_rowsource_context *con;
  con = (rasqal_filter_rowsource_context*)user_data;

  return rasqal_rowsource_reset(con->rowsource);
}


static rasqal_rowsource*
rasqal_filter_rowsource_get_inner_rowsource(rasqal_rowsource* rowsource,
                                            void *user_data, int offset)
{
  rasqal_filter_rowsource_context *con;
  con = (rasqal_filter_rowsource_context*)user_data;

  if(offset == 0)
    return con->rowsource;
  return NULL;
}


static const rasqal_rowsource_handler rasqal_filter_rowsource_handler = {
  /* .version =          */ 1,
  "filter",
  /* .init =             */ rasqal_filter_rowsource_init,
  /* .finish =           */ rasqal_filter_rowsource_finish,
  /* .ensure_variables = */ rasqal_filter_rowsource_ensure_variables,
  /* .read_row =         */ rasqal_filter_rowsource_read_row,
  /* .read_all_rows =    */ NULL,
  /* .reset =            */ rasqal_filter_rowsource_reset,
  /* .set_requirements = */ NULL,
  /* .get_inner_rowsource = */ rasqal_filter_rowsource_get_inner_rowsource,
  /* .set_origin =       */ NULL,
};


/**
 * rasqal_new_filter_rowsource:
 * @world: world object
 * @query: query object
 * @rowsource: input rowsource
 * @expr: filter expression
 *
 * INTERNAL - create a new FILTER rowsource
 *
 * The @rowsource becomes owned by the new rowsource
 *
 * Return value: new rowsource or NULL on failure
 */
rasqal_rowsource*
rasqal_new_filter_rowsource(rasqal_world *world,
                            rasqal_query *query,
                            rasqal_rowsource* rowsource,
                            rasqal_expression* expr)
{
  rasqal_filter_rowsource_context *con;
  int flags = 0;
  
  if(!world || !query || !rowsource || !expr)
    goto fail;
  
  con = RASQAL_CALLOC(rasqal_filter_rowsource_context*, 1, sizeof(*con));
  if(!con)
    goto fail;

  con->rowsource = rowsource;
  con->expr = rasqal_new_expression_from_expression(expr);

  return rasqal_new_rowsource_from_handler(world, query,
                                           con,
                                           &rasqal_filter_rowsource_handler,
                                           query->vars_table,
                                           flags);

  fail:
  if(rowsource)
    rasqal_free_rowsource(rowsource);
  if(expr)
    rasqal_free_expression(expr);
  return NULL;
}

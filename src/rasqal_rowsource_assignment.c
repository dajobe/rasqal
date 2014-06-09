/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_assignment.c - Rasqal assignment rowsource class
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
  /* assignment variable */
  rasqal_variable *var;

  /* assignment expression */
  rasqal_expression *expr;

  /* offset into results for current row */
  int offset;
  
} rasqal_assignment_rowsource_context;


static int
rasqal_assignment_rowsource_init(rasqal_rowsource* rowsource, void *user_data)
{
  return 0;
}


static int
rasqal_assignment_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                             void *user_data)
{
  rasqal_assignment_rowsource_context *con;
  con = (rasqal_assignment_rowsource_context*)user_data;

  rowsource->size = 0;
  rasqal_rowsource_add_variable(rowsource, con->var);
  
  return 0;
}


static int
rasqal_assignment_rowsource_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_assignment_rowsource_context *con;
  con = (rasqal_assignment_rowsource_context*)user_data;

  if(con->expr)
    rasqal_free_expression(con->expr);

  RASQAL_FREE(rasqal_assignment_rowsource_context, con);

  return 0;
}


static rasqal_row*
rasqal_assignment_rowsource_read_row(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_query *query = rowsource->query;
  rasqal_assignment_rowsource_context *con;
  rasqal_literal* result = NULL;
  rasqal_row *row = NULL;
  int error = 0;
  
  con = (rasqal_assignment_rowsource_context*)user_data;

  if(con->offset)
    return NULL;
  
  RASQAL_DEBUG1("evaluating assignment expression\n");
  result = rasqal_expression_evaluate2(con->expr, query->eval_context,
                                       &error);
#ifdef RASQAL_DEBUG
  RASQAL_DEBUG2("assignment %s expression result: ", con->var->name);
  if(error)
    fputs("type error", DEBUG_FH);
  else
    rasqal_literal_print(result, DEBUG_FH);
  fputc('\n', DEBUG_FH);
#endif

  if(!error) {
    rasqal_variable_set_value(con->var, result);
    row = rasqal_new_row_for_size(rowsource->world, rowsource->size);
    if(row) {
      rasqal_row_set_rowsource(row, rowsource);
      row->offset = con->offset++;
      row->values[0] = rasqal_new_literal_from_literal(result);
    }
  }
  
  return row;
}


static int
rasqal_assignment_rowsource_reset(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_assignment_rowsource_context *con;
  con = (rasqal_assignment_rowsource_context*)user_data;

  con->offset = 0;
  
  return 0;
}


static rasqal_rowsource*
rasqal_assignment_rowsource_get_inner_rowsource(rasqal_rowsource* rowsource,
                                                void *user_data, int offset)
{
  return NULL;
}


static const rasqal_rowsource_handler rasqal_assignment_rowsource_handler = {
  /* .version =          */ 1,
  "assignment",
  /* .init =             */ rasqal_assignment_rowsource_init,
  /* .finish =           */ rasqal_assignment_rowsource_finish,
  /* .ensure_variables = */ rasqal_assignment_rowsource_ensure_variables,
  /* .read_row =         */ rasqal_assignment_rowsource_read_row,
  /* .read_all_rows =    */ NULL,
  /* .reset =            */ rasqal_assignment_rowsource_reset,
  /* .set_requirements = */ NULL,
  /* .get_inner_rowsource = */ rasqal_assignment_rowsource_get_inner_rowsource,
  /* .set_origin =       */ NULL,
};


/**
 * rasqal_new_assignment_rowsource:
 * @world: world object
 * @query: query object
 * @var: variable to bind value to
 * @expr: expression to use to create value
 *
 * INTERNAL - create a new ASSIGNment
 *
 * Return value: new rowsource or NULL on failure
 */
rasqal_rowsource*
rasqal_new_assignment_rowsource(rasqal_world *world,
                                rasqal_query *query,
                                rasqal_variable* var,
                                rasqal_expression* expr)
{
  rasqal_assignment_rowsource_context *con;
  int flags = 0;
  
  if(!world || !query || !var || !expr)
    return NULL;
  
  con = RASQAL_CALLOC(rasqal_assignment_rowsource_context*, 1, sizeof(*con));
  if(!con)
    return NULL;

  con->var = rasqal_new_variable_from_variable(var);
  con->expr = rasqal_new_expression_from_expression(expr);

  return rasqal_new_rowsource_from_handler(world, query,
                                           con,
                                           &rasqal_assignment_rowsource_handler,
                                           query->vars_table,
                                           flags);
}

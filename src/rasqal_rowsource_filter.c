/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_filter.c - Rasqal filter rowsource class
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
  rasqal_filter_rowsource_context *con;

  con = (rasqal_filter_rowsource_context*)user_data;
  
  return 0;
}


static int
rasqal_filter_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                         void *user_data)
{
  rasqal_filter_rowsource_context* con;
  
  con = (rasqal_filter_rowsource_context*)user_data; 

  rasqal_rowsource_ensure_variables(con->rowsource);

  rowsource->size = 0;
  rasqal_rowsource_copy_variables(rowsource, con->rowsource);
  
  return 0;
}


static int
rasqal_filter_rowsource_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_filter_rowsource_context *con;
  con = (rasqal_filter_rowsource_context*)user_data;

  if(con->rowsource)
    rasqal_free_rowsource(con->rowsource);
  
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

    row = rasqal_rowsource_read_row(con->rowsource);
    if(!row)
      break;

    result = rasqal_expression_evaluate_v2(query->world, &query->locator,
                                           con->expr, query->compare_flags);
#ifdef RASQAL_DEBUG
    RASQAL_DEBUG1("filter expression result:\n");
    if(!result)
      fputs("type error", DEBUG_FH);
    else
      rasqal_literal_print(result, DEBUG_FH);
    fputc('\n', DEBUG_FH);
#endif
    if(!result) {
      bresult = 0;
    } else {
      int error = 0;
      bresult = rasqal_literal_as_boolean(result, &error);
      if(error)
        RASQAL_DEBUG1("filter boolean expression returned error\n");
#ifdef RASQAL_DEBUG
      else
        RASQAL_DEBUG2("filter boolean expression result: %d\n", bresult);
#endif
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
      rasqal_literal *l;
      l = rasqal_variables_table_get_value(query->vars_table, i);
      if(row->values[i])
        rasqal_free_literal(row->values[i]);
      row->values[i] = rasqal_new_literal_from_literal(l);
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


static int
rasqal_filter_rowsource_set_preserve(rasqal_rowsource* rowsource,
                                     void *user_data, int preserve)
{
  rasqal_filter_rowsource_context *con;
  con = (rasqal_filter_rowsource_context*)user_data;

  return rasqal_rowsource_set_preserve(con->rowsource, preserve);
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
  /* .set_preserve =     */ rasqal_filter_rowsource_set_preserve
};


rasqal_rowsource*
rasqal_new_filter_rowsource(rasqal_world *world,
                            rasqal_query *query,
                            rasqal_rowsource* rowsource,
                            rasqal_expression* expr)
{
  rasqal_filter_rowsource_context *con;
  int flags = 0;
  
  if(!world || !query || !rowsource || !expr)
    return NULL;
  
  con = (rasqal_filter_rowsource_context*)RASQAL_CALLOC(rasqal_filter_rowsource_context, 1, sizeof(rasqal_filter_rowsource_context));
  if(!con)
    return NULL;

  con->rowsource = rowsource;
  con->expr = expr;

  return rasqal_new_rowsource_from_handler(world, query,
                                           con,
                                           &rasqal_filter_rowsource_handler,
                                           query->vars_table,
                                           flags);
}

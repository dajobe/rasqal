/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_bindings.c - Rasqal bindings rows rowsource class
 *
 * Copyright (C) 2013, David Beckett http://www.dajobe.org/
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


typedef struct 
{
  /* bindings object */
  rasqal_bindings* bindings;

  /* bindings row offset */
  int offset;
} rasqal_bindings_rowsource_context;



static int
rasqal_bindings_rowsource_init(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_bindings_rowsource_context *con;

  con = (rasqal_bindings_rowsource_context*)user_data;

  con->offset = 0;

  return 0;
}


static int
rasqal_bindings_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                           void *user_data)
{
  rasqal_bindings_rowsource_context* con;
  raptor_sequence* vars_seq;
  int i;
  
  con = (rasqal_bindings_rowsource_context*)user_data;

  rowsource->size = 0;

  vars_seq = con->bindings->variables;
  if(!vars_seq)
    return 0;

  for(i = 0; 1; i++) {
    rasqal_variable* v;
    v = (rasqal_variable*)raptor_sequence_get_at(vars_seq, i);
    if(!v)
      break;

    if(rasqal_rowsource_add_variable(rowsource, v) < 0)
      return 1;
  }
  
  return 0;
}


static int
rasqal_bindings_rowsource_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_bindings_rowsource_context *con;

  con = (rasqal_bindings_rowsource_context*)user_data;

  if(con->bindings)
    rasqal_free_bindings(con->bindings);
  
  RASQAL_FREE(rasqal_bindings_rowsource_context, con);

  return 0;
}


static rasqal_row*
rasqal_bindings_rowsource_read_row(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_bindings_rowsource_context *con;
  rasqal_row *row;
  
  con = (rasqal_bindings_rowsource_context*)user_data;

  row = rasqal_bindings_get_row(con->bindings, con->offset++);
  
  return row;
}


static int
rasqal_bindings_rowsource_reset(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_bindings_rowsource_context *con;

  con = (rasqal_bindings_rowsource_context*)user_data;

  con->offset = 0;

  return 0;
}


static const rasqal_rowsource_handler rasqal_bindings_rowsource_handler = {
  /* .version =          */ 1,
  "bindings",
  /* .init =             */ rasqal_bindings_rowsource_init,
  /* .finish =           */ rasqal_bindings_rowsource_finish,
  /* .ensure_variables = */ rasqal_bindings_rowsource_ensure_variables,
  /* .read_row =         */ rasqal_bindings_rowsource_read_row,
  /* .read_all_rows =    */ NULL,
  /* .reset =            */ rasqal_bindings_rowsource_reset,
  /* .set_requirements = */ NULL,
  /* .get_inner_rowsource = */ NULL,
  /* .set_origin =       */ NULL,
};


/**
 * rasqal_new_bindings_rowsource:
 * @world: world object
 * @query: query object
 * @rowsource: input rowsource
 * @limit: max rows limit (or <0 for no limit)
 * @offset: start row offset (or <0 for no offset)
 *
 * INTERNAL - create a new bindings (LIMIT, OFFSET) rowsource
 *
 * The @rowsource becomes owned by the new rowsource
 *
 * Return value: new rowsource or NULL on failure
 */
rasqal_rowsource*
rasqal_new_bindings_rowsource(rasqal_world *world,
                              rasqal_query *query,
                              rasqal_bindings* bindings)
{
  rasqal_bindings_rowsource_context *con;
  int flags = 0;
  
  if(!world || !query || !bindings)
    goto fail;
  
  con = RASQAL_CALLOC(rasqal_bindings_rowsource_context*, 1, sizeof(*con));
  if(!con)
    goto fail;

  con->bindings = bindings;
  con->offset = 0;

  return rasqal_new_rowsource_from_handler(world, query,
                                           con,
                                           &rasqal_bindings_rowsource_handler,
                                           query->vars_table,
                                           flags);

  fail:
  if(bindings)
    rasqal_free_bindings(bindings);

  return NULL;
}

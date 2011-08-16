/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_having.c - Rasqal having rowsource class
 *
 * Copyright (C) 2010, David Beckett http://www.dajobe.org/
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
  /* inner rowsource to having */
  rasqal_rowsource *rowsource;

  /* sequence of HAVING conditions */
  raptor_sequence* exprs_seq;

  /* offset into results for current row */
  int offset;
  
} rasqal_having_rowsource_context;


static int
rasqal_having_rowsource_init(rasqal_rowsource* rowsource, void *user_data)
{
  return 0;
}


static int
rasqal_having_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                         void *user_data)
{
  rasqal_having_rowsource_context* con;
  
  con = (rasqal_having_rowsource_context*)user_data; 

  if(rasqal_rowsource_ensure_variables(con->rowsource))
    return 1;

  rowsource->size = 0;
  if(rasqal_rowsource_copy_variables(rowsource, con->rowsource))
    return 1;
  
  return 0;
}


static int
rasqal_having_rowsource_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_having_rowsource_context *con;

  con = (rasqal_having_rowsource_context*)user_data;

  if(con->rowsource)
    rasqal_free_rowsource(con->rowsource);
  
  if(con->exprs_seq)
    raptor_free_sequence(con->exprs_seq);

  RASQAL_FREE(rasqal_having_rowsource_context, con);

  return 0;
}


static rasqal_row*
rasqal_having_rowsource_read_row(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_having_rowsource_context *con;
  rasqal_row *row = NULL;
  
  con = (rasqal_having_rowsource_context*)user_data;

  while(1) {
    raptor_sequence* literal_seq = NULL;
    int bresult = 1;
    int error = 0;

    row = rasqal_rowsource_read_row(con->rowsource);
    if(!row)
      break;

    literal_seq  = rasqal_expression_sequence_evaluate(rowsource->query,
                                                       con->exprs_seq,
                                                       /* ignore_errors */ 0,
                                                       &error);
    if(error) {
      if(literal_seq)
        raptor_free_sequence(literal_seq);

      rasqal_free_row(row); row = NULL;
      continue;
    }

#ifdef RASQAL_DEBUG
    RASQAL_DEBUG1("having expression list result: ");
    if(!literal_seq)
      fputs("NULL", DEBUG_FH);
    else
      raptor_sequence_print(literal_seq, DEBUG_FH);
    fputc('\n', DEBUG_FH);
#endif

    if(!literal_seq) {
      bresult = 0;
    } else {
      rasqal_literal* result;
      int i;

      /* Assume all conditions must evaluate to true */
      for(i = 0;
          (result = (rasqal_literal*)raptor_sequence_get_at(literal_seq, i));
          i++) {
        bresult = rasqal_literal_as_boolean(result, &error);

#ifdef RASQAL_DEBUG
        if(error)
          RASQAL_DEBUG1("having boolean expression returned error\n");
        else
          RASQAL_DEBUG2("having boolean expression result: %d\n", bresult);
#endif

        if(error)
          bresult = 0;

        if(!bresult)
          break;
      }

      raptor_free_sequence(literal_seq);
    }
    
    if(bresult)
      /* Constraint succeeded so end */
      break;

    rasqal_free_row(row); row = NULL;
  }


  if(row) {
    /* HAVING never adds/removes to the selection order from the
     * input row, no need to re-bind row->values[] 
     */
    row->offset = con->offset++;
  }
  
  return row;
}


static int
rasqal_having_rowsource_reset(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_having_rowsource_context *con;
  con = (rasqal_having_rowsource_context*)user_data;

  return rasqal_rowsource_reset(con->rowsource);
}


static rasqal_rowsource*
rasqal_having_rowsource_get_inner_rowsource(rasqal_rowsource* rowsource,
                                            void *user_data, int offset)
{
  rasqal_having_rowsource_context *con;

  con = (rasqal_having_rowsource_context*)user_data;

  if(offset == 0)
    return con->rowsource;
  return NULL;
}


static const rasqal_rowsource_handler rasqal_having_rowsource_handler = {
  /* .version =          */ 1,
  "having",
  /* .init =             */ rasqal_having_rowsource_init,
  /* .finish =           */ rasqal_having_rowsource_finish,
  /* .ensure_variables = */ rasqal_having_rowsource_ensure_variables,
  /* .read_row =         */ rasqal_having_rowsource_read_row,
  /* .read_all_rows =    */ NULL,
  /* .reset =            */ rasqal_having_rowsource_reset,
  /* .set_requirements = */ NULL,
  /* .get_inner_rowsource = */ rasqal_having_rowsource_get_inner_rowsource,
  /* .set_origin =       */ NULL,
};


/**
 * rasqal_new_having_rowsource:
 * @world: world object
 * @query: query object
 * @rowsource: input rowsource
 * @expr_seq: sequence of HAVING expressions
 *
 * INTERNAL - create a new HAVING rowsource
 *
 * The @rowsource becomes owned by the new rowsource
 *
 * Return value: new rowsource or NULL on failure
 */
rasqal_rowsource*
rasqal_new_having_rowsource(rasqal_world *world,
                            rasqal_query *query,
                            rasqal_rowsource* rowsource,
                            raptor_sequence* exprs_seq)
{
  rasqal_having_rowsource_context *con;
  int flags = 0;
  
  if(!world || !query || !rowsource || !exprs_seq)
    goto fail;
  
  con = RASQAL_CALLOC(rasqal_having_rowsource_context*, 1, sizeof(*con));
  if(!con)
    goto fail;

  con->rowsource = rowsource;
  con->exprs_seq = rasqal_expression_copy_expression_sequence(exprs_seq);

  return rasqal_new_rowsource_from_handler(world, query,
                                           con,
                                           &rasqal_having_rowsource_handler,
                                           query->vars_table,
                                           flags);

  fail:
  if(rowsource)
    rasqal_free_rowsource(rowsource);

  if(exprs_seq)
    raptor_free_sequence(exprs_seq);

  return NULL;
}

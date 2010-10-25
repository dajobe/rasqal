/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_groupby.c - Rasqal GROUP BY and HAVING rowsource class
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


#ifndef STANDALONE

typedef struct 
{
  raptor_sequence* expr_seq;
  int groupid;
} rasqal_groupby_rowsource_context;


static int
rasqal_groupby_rowsource_init(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_groupby_rowsource_context* con;
  con = (rasqal_groupby_rowsource_context*)user_data;

  con->groupid = -1;
  
  return 0;
}


static int
rasqal_groupby_rowsource_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_groupby_rowsource_context* con;
  con = (rasqal_groupby_rowsource_context*)user_data;

  if(con->expr_seq)
    raptor_free_sequence(con->expr_seq);
  
  RASQAL_FREE(rasqal_groupby_rowsource_context, con);

  return 0;
}


static int
rasqal_groupby_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                        void *user_data)
{
  rowsource->size = 0;
  return 0;
}


static rasqal_row*
rasqal_groupby_rowsource_read_row(rasqal_rowsource* rowsource, void *user_data)
{
  /* rasqal_groupby_rowsource_context* con;
  con = (rasqal_groupby_rowsource_context*)user_data; */
  return NULL;
}


static raptor_sequence*
rasqal_groupby_rowsource_read_all_rows(rasqal_rowsource* rowsource,
                                       void *user_data)
{
  /* rasqal_groupby_rowsource_context* con;
  con = (rasqal_groupby_rowsource_context*)user_data; */
  return NULL;
}


static int
rasqal_groupby_rowsource_reset(rasqal_rowsource* rowsource, void *user_data)
{
  return 0;
}


static const rasqal_rowsource_handler rasqal_groupby_rowsource_handler = {
  /* .version = */ 1,
  "groupby",
  /* .init = */ rasqal_groupby_rowsource_init,
  /* .finish = */ rasqal_groupby_rowsource_finish,
  /* .ensure_variables = */ rasqal_groupby_rowsource_ensure_variables,
  /* .read_row = */ rasqal_groupby_rowsource_read_row,
  /* .read_all_rows = */ rasqal_groupby_rowsource_read_all_rows,
  /* .reset = */ rasqal_groupby_rowsource_reset,
  /* .set_preserve = */ NULL,
  /* .get_inner_rowsource = */ NULL,
  /* .set_origin = */ NULL,
};


rasqal_rowsource*
rasqal_new_groupby_rowsource(rasqal_world *world, rasqal_query* query,
                             raptor_sequence* expr_seq)
{
  rasqal_groupby_rowsource_context* con;
  int flags = 0;

  if(!world || !query)
    return NULL;
  
  con = (rasqal_groupby_rowsource_context*)RASQAL_CALLOC(rasqal_groupby_rowsource_context, 1, sizeof(*con));
  if(!con)
    return NULL;

  if(expr_seq) {
    con->expr_seq = rasqal_expression_copy_expression_sequence(expr_seq);

    if(!con->expr_seq) {
      RASQAL_FREE(rasqal_groupby_rowsource_context, con);
      return NULL;
    }
  }
  
  return rasqal_new_rowsource_from_handler(world, query,
                                           con,
                                           &rasqal_groupby_rowsource_handler,
                                           query->vars_table,
                                           flags);
}


#endif



#ifdef STANDALONE

/* one more prototype */
int main(int argc, char *argv[]);

int
main(int argc, char *argv[]) 
{
  const char *program = rasqal_basename(argv[0]);
  rasqal_rowsource *rowsource = NULL;
  rasqal_world* world = NULL;
  rasqal_query* query = NULL;
  raptor_sequence* expr_seq = NULL;
  int failures = 0;

  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    return(1);
  }
  
  query = rasqal_new_query(world, "sparql", NULL);
  
#ifdef HAVE_RAPTOR2_API
  expr_seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_expression,
                                 (raptor_data_print_handler)rasqal_expression_print);
#else
  expr_seq = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_expression,
                                 (raptor_sequence_print_handler*)rasqal_expression_print);
#endif
  
  rowsource = rasqal_new_groupby_rowsource(world, query, expr_seq);
  if(!rowsource) {
    fprintf(stderr, "%s: failed to create groupby rowsource\n", program);
    failures++;
    goto tidy;
  }


  tidy:
  if(expr_seq)
    raptor_free_sequence(expr_seq);
  if(rowsource)
    rasqal_free_rowsource(rowsource);
  if(query)
    rasqal_free_query(query);
  if(world)
    rasqal_free_world(world);

  return failures;
}

#endif

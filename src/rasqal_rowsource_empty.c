/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_empty.c - Rasqal empty rowsource class
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


#ifndef STANDALONE

typedef struct 
{
  int count;
} rasqal_empty_rowsource_context;


static int
rasqal_empty_rowsource_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_empty_rowsource_context* con;
  con = (rasqal_empty_rowsource_context*)user_data;
  RASQAL_FREE(rasqal_empty_rowsource_context, con);

  return 0;
}

static int
rasqal_empty_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                        void *user_data)
{
  rowsource->size = 0;
  return 0;
}

static rasqal_row*
rasqal_empty_rowsource_read_row(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_empty_rowsource_context* con;
  rasqal_row* row = NULL;
  
  con = (rasqal_empty_rowsource_context*)user_data;

  if(!con->count++)
    row = rasqal_new_row(rowsource);
    
  return row;
}

static raptor_sequence*
rasqal_empty_rowsource_read_all_rows(rasqal_rowsource* rowsource,
                                     void *user_data)
{
  /* rasqal_empty_rowsource_context* con;
  con = (rasqal_empty_rowsource_context*)user_data; */
  raptor_sequence *seq;

  seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_row,
                            (raptor_data_print_handler)rasqal_row_print);
  if(seq) {
    rasqal_row* row = rasqal_new_row(rowsource);

    raptor_sequence_push(seq, row);
  }
  
  return seq;
}

static const rasqal_rowsource_handler rasqal_empty_rowsource_handler = {
  /* .version = */ 1,
  "empty",
  /* .init = */ NULL,
  /* .finish = */ rasqal_empty_rowsource_finish,
  /* .ensure_variables = */ rasqal_empty_rowsource_ensure_variables,
  /* .read_row = */ rasqal_empty_rowsource_read_row,
  /* .read_all_rows = */ rasqal_empty_rowsource_read_all_rows,
  /* .reset = */ NULL,
  /* .set_requirements = */ NULL,
  /* .get_inner_rowsource = */ NULL,
  /* .set_origin = */ NULL,
};


/**
 * rasqal_new_empty_rowsource:
 * @world: world object
 * @query: query object
 *
 * INTERNAL - create a new EMPTY rowsource that always returns zero rows
 *
 * Return value: new rowsource or NULL on failure
 */
rasqal_rowsource*
rasqal_new_empty_rowsource(rasqal_world *world, rasqal_query* query)
{
  rasqal_empty_rowsource_context* con;
  int flags = 0;

  if(!world || !query)
    return NULL;
  
  con = RASQAL_CALLOC(rasqal_empty_rowsource_context*, 1, sizeof(*con));
  if(!con)
    return NULL;

  return rasqal_new_rowsource_from_handler(world, query,
                                           con,
                                           &rasqal_empty_rowsource_handler,
                                           query->vars_table,
                                           flags);
}


#endif /* not STANDALONE */



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
  rasqal_row* row = NULL;
  int count;
  raptor_sequence* seq = NULL;
  int failures = 0;

  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    return(1);
  }
  
  query = rasqal_new_query(world, "sparql", NULL);
  
  rowsource = rasqal_new_empty_rowsource(world, query);
  if(!rowsource) {
    fprintf(stderr, "%s: failed to create empty rowsource\n", program);
    failures++;
    goto tidy;
  }

  row = rasqal_rowsource_read_row(rowsource);
  if(!row) {
    fprintf(stderr,
            "%s: read_row failed to return a row for an empty rowsource\n",
            program);
    failures++;
    goto tidy;
  }

  if(row->size) {
    fprintf(stderr,
            "%s: read_row returned an non-empty row size %d for a empty stream\n",
            program, row->size);
    failures++;
    goto tidy;
  }
  
  count = rasqal_rowsource_get_rows_count(rowsource);
  if(count != 1) {
    fprintf(stderr, "%s: read_rows returned count %d for a empty stream\n",
            program, count);
    failures++;
    goto tidy;
  }

  rasqal_free_row(row); row = NULL;

  rasqal_free_rowsource(rowsource);

  /* re-init rowsource */
  rowsource = rasqal_new_empty_rowsource(world, query);
  
  seq = rasqal_rowsource_read_all_rows(rowsource);
  if(!seq) {
    fprintf(stderr, "%s: read_rows returned a NULL seq for a empty stream\n",
            program);
    failures++;
    goto tidy;
  }

  count = raptor_sequence_size(seq);
  if(count != 1) {
    fprintf(stderr, "%s: read_rows returned size %d seq for a empty stream\n",
            program, count);
    failures++;
    goto tidy;
  }


  tidy:
  if(row)
    rasqal_free_row(row);
  if(seq)
    raptor_free_sequence(seq);
  if(rowsource)
    rasqal_free_rowsource(rowsource);
  if(query)
    rasqal_free_query(query);
  if(world)
    rasqal_free_world(world);

  return failures;
}

#endif /* STANDALONE */

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_service.c - Rasqal SERVICE rowsource class
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


#ifndef STANDALONE

typedef struct 
{
  rasqal_service* svc;
  rasqal_query* query;
  rasqal_rowsource* rowsource;
  int count;
  /* bit flags; currently using RASQAL_ENGINE_BITFLAG_SILENT */
  unsigned int flags;
} rasqal_service_rowsource_context;


static int
rasqal_service_rowsource_init(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_service_rowsource_context* con;

  con = (rasqal_service_rowsource_context*)user_data;

  con->rowsource = rasqal_service_execute_as_rowsource(con->svc,
                                                       con->query->vars_table);

  if(!con->rowsource) {
    /* Silent errors return an empty rowsource */

    if(con->flags & RASQAL_ENGINE_BITFLAG_SILENT) {
      con->rowsource = rasqal_new_empty_rowsource(con->query->world,
                                                  con->query);
      return 0;
    }

    return 1;
  }
  
  return 0;
}


static int
rasqal_service_rowsource_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_service_rowsource_context* con;

  con = (rasqal_service_rowsource_context*)user_data;

  if(con->svc)
    rasqal_free_service(con->svc);

  if(con->rowsource)
    rasqal_free_rowsource(con->rowsource);

  RASQAL_FREE(rasqal_service_rowsource_context, con);

  return 0;
}

static int
rasqal_service_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                          void *user_data)
{
  rasqal_service_rowsource_context* con;
  int rc;
  
  con = (rasqal_service_rowsource_context*)user_data;

  rc = rasqal_rowsource_ensure_variables(con->rowsource);
  if(rc)
    return rc;
  /* copy in variables from format rowsource */
  rc = rasqal_rowsource_copy_variables(rowsource, con->rowsource);

  return rc;
}

static rasqal_row*
rasqal_service_rowsource_read_row(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_service_rowsource_context* con;
  
  con = (rasqal_service_rowsource_context*)user_data;

  return rasqal_rowsource_read_row(con->rowsource);
}

static raptor_sequence*
rasqal_service_rowsource_read_all_rows(rasqal_rowsource* rowsource,
                                       void *user_data)
{
  rasqal_service_rowsource_context* con;

  con = (rasqal_service_rowsource_context*)user_data;

  return rasqal_rowsource_read_all_rows(con->rowsource);
}

static const rasqal_rowsource_handler rasqal_service_rowsource_handler = {
  /* .version = */ 1,
  "service",
  /* .init = */ rasqal_service_rowsource_init,
  /* .finish = */ rasqal_service_rowsource_finish,
  /* .ensure_variables = */ rasqal_service_rowsource_ensure_variables,
  /* .read_row = */ rasqal_service_rowsource_read_row,
  /* .read_all_rows = */ rasqal_service_rowsource_read_all_rows,
  /* .reset = */ NULL,
  /* .set_preserve = */ NULL,
  /* .get_inner_rowsource = */ NULL,
  /* .set_origin = */ NULL,
};


/**
 * rasqal_new_service_rowsource:
 * @world: world object
 * @query: query object
 * @service_uri: service URI
 * @query_string: query to send to service
 * @data_graphs: sequence of data graphs (or NULL)
 * @rs_flags: service rowsource flags
 *
 * INTERNAL - create a new rowsource that takes rows from a service
 *
 * All arguments are copied.
 *
 * Return value: new rowsource or NULL on failure
 */
rasqal_rowsource*
rasqal_new_service_rowsource(rasqal_world *world, rasqal_query* query,
                             raptor_uri* service_uri,
                             const unsigned char* query_string,
                             raptor_sequence* data_graphs,
                             unsigned int rs_flags)
{
  rasqal_service_rowsource_context* con = NULL;
  rasqal_service* svc = NULL;
  int flags = 0;
  int silent = (rs_flags & RASQAL_ENGINE_BITFLAG_SILENT);

  if(!world || !query_string)
    goto fail;
  
  svc = rasqal_new_service(query->world, service_uri, query_string,
                           data_graphs);
  if(!svc) {
    if(!silent)
      goto fail;

    /* Silent errors so tidy up and return empty rowsource */
    RASQAL_FREE(cstring, query_string);
    if(data_graphs)
      raptor_free_sequence(data_graphs);

    return rasqal_new_empty_rowsource(world, query);
  }

  con = RASQAL_CALLOC(rasqal_service_rowsource_context*, 1, sizeof(*con));
  if(!con)
    goto fail;

  con->svc = svc;
  con->query = query;
  con->flags = rs_flags;

  return rasqal_new_rowsource_from_handler(world, query,
                                           con,
                                           &rasqal_service_rowsource_handler,
                                           query->vars_table,
                                           flags);

  fail:
  if(svc)
    rasqal_free_service(svc);
  if(con)
    RASQAL_FREE(rasqal_service_rowsource_context, con);
  if(query_string)
    RASQAL_FREE(cstring, query_string);
  if(data_graphs)
    raptor_free_sequence(data_graphs);

  return NULL;
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
  raptor_uri* service_uri;
  const unsigned char* query_string;
  raptor_sequence* data_graphs = NULL;
  unsigned int rs_flags = 0;

  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    return(1);
  }
  
  query = rasqal_new_query(world, "sparql", NULL);
  
  service_uri = raptor_new_uri(world->raptor_world_ptr,
                               (const unsigned char *)"http://example.org/service");
  query_string = (const unsigned char*)"SELECT * WHERE { ?s ?p ?o }";
  rowsource = rasqal_new_service_rowsource(world, query, service_uri,
                                           query_string, data_graphs,
                                           rs_flags);
  if(!rowsource) {
    fprintf(stderr, "%s: failed to create service rowsource\n", program);
    failures++;
    goto tidy;
  }

  row = rasqal_rowsource_read_row(rowsource);
  if(!row) {
    fprintf(stderr,
            "%s: read_row failed to return a row for an service rowsource\n",
            program);
    failures++;
    goto tidy;
  }

  if(row->size) {
    fprintf(stderr,
            "%s: read_row returned an non-service row size %d for a service stream\n",
            program, row->size);
    failures++;
    goto tidy;
  }
  
  count = rasqal_rowsource_get_rows_count(rowsource);
  if(count != 1) {
    fprintf(stderr, "%s: read_rows returned count %d for a service stream\n",
            program, count);
    failures++;
    goto tidy;
  }
  
  rasqal_free_rowsource(rowsource);

  /* re-init rowsource */
  rowsource = rasqal_new_service_rowsource(world, query, service_uri,
                                           query_string, data_graphs,
                                           rs_flags);
  
  seq = rasqal_rowsource_read_all_rows(rowsource);
  if(!seq) {
    fprintf(stderr, "%s: read_rows returned a NULL seq for a service stream\n",
            program);
    failures++;
    goto tidy;
  }

  count = raptor_sequence_size(seq);
  if(count != 1) {
    fprintf(stderr, "%s: read_rows returned size %d seq for a service stream\n",
            program, count);
    failures++;
    goto tidy;
  }


  tidy:
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

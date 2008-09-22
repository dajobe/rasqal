/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_engine_rowsource.c - Rasqal query engine rowsource class
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


#ifndef STANDALONE

typedef struct 
{
  rasqal_query_results* query_results;

  int run;
  
  int failed;
  
  int finished;
  
} rasqal_execution_rowsource_context;


static int
rasqal_execution_rowsource_init(rasqal_rowsource* rowsource, void *user_data) 
{
  rasqal_execution_rowsource_context* con;

  con = (rasqal_execution_rowsource_context*)user_data;

  con->failed = 0;

  /* FIXME - for now rasqal_query_execute() is always run before this */
  con->run = 1;
  
  return 0;
}


static int
rasqal_execution_rowsource_finish(rasqal_rowsource* rowsource,
                                  void *user_data)
{
  rasqal_execution_rowsource_context* con;

  con = (rasqal_execution_rowsource_context*)user_data;
  
  RASQAL_FREE(rasqal_execution_rowsource_context, con);

  return 0;
}


static int
rasqal_execution_rowsource_ensure_have_run(rasqal_execution_rowsource_context* con)
{
  if(con->finished || con->failed)
    return 1;

  if(!con->run) {
    con->failed = rasqal_engine_execute_run(con->query_results);
    con->run = 1;
  }

  return con->failed || con->finished;
}


static int
rasqal_execution_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                            void *user_data)
{
  rasqal_execution_rowsource_context* con;

  con = (rasqal_execution_rowsource_context*)user_data; 

  /* this handles finished or failed */
  if(rasqal_execution_rowsource_ensure_have_run(con))
    return 1;

  return 0;
}


static rasqal_query_result_row*
rasqal_execution_rowsource_read_row(rasqal_rowsource* rowsource,
                                    void *user_data)
{
  rasqal_execution_rowsource_context* con;
  rasqal_query_result_row* row = NULL;
  
  con = (rasqal_execution_rowsource_context*)user_data;

  /* this handles finished or failed */
  if(rasqal_execution_rowsource_ensure_have_run(con))
    return NULL;

  row = rasqal_engine_get_result_row(con->query_results);
  if(row) {
    row = rasqal_new_query_result_row_from_query_result_row_deep(row);
    row->rowsource = rowsource;
    con->finished = rasqal_engine_execute_next(con->query_results);
  } else
    con->finished = 1;
  
  return row;
}


#if 0
static raptor_sequence*
rasqal_execution_rowsource_read_all_rows(rasqal_rowsource* rowsource,
                                         void *user_data)
{
  rasqal_execution_rowsource_context* con;
  raptor_sequence* seq;
  
  con = (rasqal_execution_rowsource_context*)user_data;

  return NULL;
}
#endif


static rasqal_query*
rasqal_execution_rowsource_get_query(rasqal_rowsource* rowsource,
                                     void *user_data)
{
  rasqal_execution_rowsource_context* con;

  con = (rasqal_execution_rowsource_context*)user_data;
  return con->query_results->query;
}


static const rasqal_rowsource_handler rasqal_execution_rowsource_handler = {
  /* .version = */ 1,
  /* .init = */ rasqal_execution_rowsource_init,
  /* .finish = */ rasqal_execution_rowsource_finish,
  /* .ensure_variables = */ rasqal_execution_rowsource_ensure_variables,
  /* .read_row = */ rasqal_execution_rowsource_read_row,
  /* .read_all_rows = */ NULL, /* rasqal_execution_rowsource_read_all_rows, */
  /* .get_query = */ rasqal_execution_rowsource_get_query
};




rasqal_rowsource*
rasqal_new_execution_rowsource(rasqal_query_results* query_results)
{
  rasqal_execution_rowsource_context* con;
  rasqal_rowsource* rs;
  int flags = 0;

  con = (rasqal_execution_rowsource_context*)RASQAL_CALLOC(rasqal_execution_rowsource_context, 1, sizeof(rasqal_execution_rowsource_context));
  if(!con)
    return NULL;

  con->query_results = query_results;

  rs = rasqal_new_rowsource_from_handler(con,
                                         &rasqal_execution_rowsource_handler,
                                         flags);

  return rs;
}


#endif



#ifdef STANDALONE


#define QUERY_LANGUAGE "sparql"
#if 1
#define QUERY_FORMAT "PREFIX rdf: <http://www.w3.org/1999/02/22-rdf-syntax-ns#> \
         PREFIX foaf: <http://xmlns.com/foaf/0.1/> \
         SELECT $s $p $o \
         FROM <%s> \
         WHERE \
         { $s $p $o }"
#else
#define QUERY_FORMAT "PREFIX rdf: <http://www.w3.org/1999/02/22-rdf-syntax-ns#> \
         PREFIX foaf: <http://xmlns.com/foaf/0.1/> \
         SELECT $person \
         FROM <%s> \
         WHERE \
         { $person $x foaf:Person }"
#endif

#define EXPECTED_RESULTS_COUNT 1


int main(int argc, char *argv[]);

int
main(int argc, char *argv[]) 
{
  const char *program = rasqal_basename(argv[0]);
  rasqal_query *query = NULL;
  rasqal_query_results *results = NULL;
  raptor_uri *base_uri;
  unsigned char *data_string;
  unsigned char *uri_string;
  const char *query_language_name=QUERY_LANGUAGE;
  const char *query_format=QUERY_FORMAT;
  unsigned char *query_string;
  rasqal_world *world;
  int failures = 0;
  rasqal_rowsource* rowsource;
  int count = 0;

  world=rasqal_new_world();
  if(!world) {
    fprintf(stderr, "%s: rasqal_new_world() failed\n", program);
    return(1);
  }

#if 0  
  if(argc != 2) {
    fprintf(stderr, "USAGE: %s data-filename\n", program);
    return(1);
  }
    
  data_string=raptor_uri_filename_to_uri_string(argv[1]);
#else
  data_string=raptor_uri_filename_to_uri_string("../data/dc.rdf");
#endif

  query_string=(unsigned char*)RASQAL_MALLOC(cstring, strlen((const char*)data_string)+strlen(query_format)+1);
  sprintf((char*)query_string, query_format, data_string);
  raptor_free_memory(data_string);
  
  uri_string=raptor_uri_filename_to_uri_string("");
  base_uri=raptor_new_uri(uri_string);  
  raptor_free_memory(uri_string);

  query=rasqal_new_query(world, query_language_name, NULL);
  if(!query) {
    fprintf(stderr, "%s: creating query in language %s FAILED\n", program,
            query_language_name);
    return(1);
  }

  printf("%s: preparing %s query\n", program, query_language_name);
  if(rasqal_query_prepare(query, query_string, base_uri)) {
    fprintf(stderr, "%s: %s query prepare FAILED\n", program, 
            query_language_name);
    return(1);
  }

  RASQAL_FREE(cstring, query_string);

  printf("%s: executing query #1\n", program);
  results=rasqal_query_execute(query);
  if(!results) {
    fprintf(stderr, "%s: query execution 1 FAILED\n", program);
    return(1);
  }


  rowsource=rasqal_new_execution_rowsource(results);
  if(!rowsource) {
    fprintf(stderr, "%s: failed to create execution rowsource\n",
            program);
    failures++;
    goto tidy;
  }

  while(1) {
    rasqal_query_result_row* row;

    row = rasqal_rowsource_read_row(rowsource);
    if(!row)
      break;

  #ifdef RASQAL_DEBUG  
    RASQAL_DEBUG2("Result #%d:  ", count);
    rasqal_query_result_row_print(row, stderr);
    fputc('\n', stderr);
  #endif
    count++;

    rasqal_free_query_result_row(row); row = NULL;
  }

  tidy:
  if(results)
    rasqal_free_query_results(results);

  if(query)
     rasqal_free_query(query);

  if(rowsource)
    rasqal_free_rowsource(rowsource);

  if(base_uri)
    raptor_free_uri(base_uri);

  rasqal_free_world(world);

  return 0;
}

#endif

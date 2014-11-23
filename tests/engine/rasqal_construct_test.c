/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_construct_test.c - Rasqal RDF Query CONSTRUCT Tests
 *
 * Copyright (C) 2006, David Beckett http://purl.org/net/dajobe/
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
#include <stdarg.h>

#include "rasqal.h"
#include "rasqal_internal.h"

#ifdef RASQAL_QUERY_SPARQL

#define QUERY_LANGUAGE "sparql"

#define QUERY_FORMAT "\
CONSTRUCT {\
  ?s ?p ?o . \
  ?o ?p ?s \
}\n\
FROM <%s/%s>\n \
WHERE { \
   ?s ?p ?o \
  FILTER(!isLiteral(?o)) \
} \n\
"

#else
#define NO_QUERY_LANGUAGE
#endif

#define QUERY_DATA "dc.rdf"
#define QUERY_EXPECTED_COUNT 4

#ifdef NO_QUERY_LANGUAGE
int
main(int argc, char **argv) {
  const char *program = rasqal_basename(argv[0]);
  fprintf(stderr, "%s: No supported query language available, skipping test\n", program);
  return(0);
}
#else

int
main(int argc, char **argv) {
  const char *program = rasqal_basename(argv[0]);
  raptor_uri *base_uri = NULL;
  unsigned char *uri_string;
  const char *query_language_name = QUERY_LANGUAGE;
  const char *query_format = QUERY_FORMAT;
  const char *data_file = QUERY_DATA;
  const int expected_count = QUERY_EXPECTED_COUNT;
  int failures = 0;
  rasqal_world *world = NULL;
  rasqal_query *query = NULL;
  rasqal_query_results *results = NULL;
  unsigned char *data_dir_string;
  unsigned char *query_string;
  int count = 0;
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  raptor_serializer* serializer=NULL;
#endif

  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    failures++;
    goto done;
  }
  
  if(argc != 2) {
    fprintf(stderr, "USAGE: %s <path to data directory>\n", program);
    failures++;
    goto done;
  }

  uri_string = raptor_uri_filename_to_uri_string("");
  base_uri = raptor_new_uri(world->raptor_world_ptr, uri_string);
  raptor_free_memory(uri_string);

  data_dir_string = raptor_uri_filename_to_uri_string(argv[1]);

  query_string = RASQAL_MALLOC(unsigned char*, strlen((const char*)data_dir_string) + strlen(data_file) + strlen(query_format) + 1);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
  sprintf((char*)query_string, query_format, data_dir_string, data_file);
#pragma GCC diagnostic pop
  raptor_free_memory(data_dir_string);

  query = rasqal_new_query(world, query_language_name, NULL);
  if(!query) {
    fprintf(stderr, "%s: creating query in language %s FAILED\n", 
            program, query_language_name);
    failures++;
    goto done;
  }
  
  printf("%s: preparing %s query\n", program, query_language_name);
  if(rasqal_query_prepare(query, query_string, base_uri)) {
    fprintf(stderr, "%s: %s query prepare '%s' FAILED\n", program, 
            query_language_name, query_string);
    failures++;
    goto done;
  }
  
  RASQAL_FREE(char*, query_string);
  
  printf("%s: executing query\n", program);
  results = rasqal_query_execute(query);
  if(!results) {
    fprintf(stderr, "%s: query execution FAILED\n", program);
    failures++;
    goto done;
  }
  
  printf("%s: checking query results\n", program);
  if(!rasqal_query_results_is_graph(results)) {
    fprintf(stderr, "%s: query results is not a graph\n", program);
    failures++;
    goto done;
  }

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  serializer = raptor_new_serializer("ntriples");
  raptor_serialize_start_to_file_handle(serializer, base_uri, stdout);
#endif

  count = 0;
  while(results) {
    raptor_statement* triple = rasqal_query_results_get_triple(results);
    if(!triple)
      break;
    count++;
    
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    raptor_serialize_statement(serializer, triple);
#endif
    if(rasqal_query_results_next_triple(results))
      break;
  }
  
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  raptor_serialize_end(serializer);
  raptor_free_serializer(serializer);
#endif

  if(count != expected_count) {
    printf("%s: query execution FAILED returning %d triples, expected %d\n", 
           program, count, expected_count);
    failures++;
    goto done;
  }


  done:
  if(results)
    rasqal_free_query_results(results);

  if(query)
    rasqal_free_query(query);
  
  if(base_uri)
    raptor_free_uri(base_uri);

  rasqal_free_world(world);

  return failures;
}

#endif

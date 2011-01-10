/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_triples_test.c - Rasqal RDF Query Triple Patterns Tests
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


#define DATA_FILE_NAME (const unsigned char*)"triples.ttl"

#ifdef RASQAL_QUERY_SPARQL
/*
DAWG basic/list-4.rql test flattened to show triple patterns not
just collections but triples, bnode renamed to be more readable.
The triple patterns are not reordered so execution should be identical.

Expected answer is 1 row:
{
  p = <http://example.org/ns#list2>
  v = "11"^^<http://www.w3.org/2001/XMLSchema#integer>
  w = "22"^^<http://www.w3.org/2001/XMLSchema#integer>
}

Original query:
 SELECT ?p ?v ?w
 { :x ?p (?v ?w) . }
*/
#define QUERY_LANGUAGE "sparql"
#define QUERY_FORMAT " \
PREFIX : <http://example.org/ns#> \
PREFIX rdf: <http://www.w3.org/1999/02/22-rdf-syntax-ns#> \
SELECT ?p ?v ?w \
{ \
   :x ?p _:node1 . \
   _:node2 rdf:first ?w . \
   _:node2 rdf:rest  rdf:nil  . \
   _:node1 rdf:first ?v . \
   _:node1 rdf:rest  _:node2 \
} \
"
#else
#ifdef RASQAL_QUERY_RDQL
#define QUERY_LANGUAGE "rdql"
#define QUERY_FORMAT "SELECT ?p, ?v, ?w \
         WHERE \
         (?p, ?v, ?w) USING \
         rdf FOR <http://www.w3.org/1999/02/22-rdf-syntax-ns#>, \
         ex FOR <http://example.org/ns#>"
#else
#define NO_QUERY_LANGUAGE
#endif
#endif

#define EXPECTED_RESULTS_COUNT 1


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
  rasqal_query *query = NULL;
  rasqal_query_results *results = NULL;
  raptor_uri *base_uri;
  unsigned char *data_dir_string;
  raptor_uri* data_dir_uri;
  unsigned char *uri_string;
  const char *query_language_name = QUERY_LANGUAGE;
  const unsigned char *query_string = (const unsigned char*)QUERY_FORMAT;
  int count;
  rasqal_world *world;
  raptor_uri *data_file_uri;
  rasqal_data_graph* dg;
  
  world=rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    return(1);
  }

  uri_string = raptor_uri_filename_to_uri_string("");
  base_uri = raptor_new_uri(world->raptor_world_ptr, uri_string);
  raptor_free_memory(uri_string);

  data_dir_string  =raptor_uri_filename_to_uri_string(argv[1]);
  data_dir_uri = raptor_new_uri(world->raptor_world_ptr, data_dir_string);

  query = rasqal_new_query(world, query_language_name, NULL);
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

#ifdef RAPTOR_V2_AVAILABLE
  data_file_uri = raptor_new_uri_relative_to_base(world->raptor_world_ptr,
                                                  data_dir_uri,
                                                  DATA_FILE_NAME);
#else
  data_file_uri = raptor_new_uri_relative_to_base(data_dir_uri, DATA_FILE_NAME);
#endif

  dg = rasqal_new_data_graph_from_uri(world,
                                      /* source URI */ data_file_uri,
                                      /* name URI */ NULL,
                                      RASQAL_DATA_GRAPH_BACKGROUND,
                                      NULL, NULL, NULL);
  rasqal_query_add_data_graph(query, dg);

  raptor_free_uri(data_file_uri);
  data_file_uri = NULL;

  printf("%s: executing query\n", program);
  results = rasqal_query_execute(query);
  if(!results) {
    fprintf(stderr, "%s: query execution FAILED\n", program);
    return(1);
  }

  count = 0;
  while(results && !rasqal_query_results_finished(results)) {
    int i;
    for(i = 0; i < rasqal_query_results_get_bindings_count(results); i++) {
      const unsigned char *name = rasqal_query_results_get_binding_name(results, i);
      rasqal_literal *value = rasqal_query_results_get_binding_value(results, i);

      printf("result %d: variable %s=", count+1, (char*)name);
      rasqal_literal_print(value, stdout);
      putchar('\n');
    }
    rasqal_query_results_next(results);
    count++;
  }
  if(results)
    rasqal_free_query_results(results);
  if(count != EXPECTED_RESULTS_COUNT) {
    fprintf(stderr, "%s: query execution returned %d results, expected %d\n",
            program, count, EXPECTED_RESULTS_COUNT);
    return(1);
  }

  rasqal_free_query(query);

  raptor_free_uri(base_uri);

  rasqal_free_world(world);

  return 0;
}

#endif

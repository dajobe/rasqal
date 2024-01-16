/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_graph_test.c - Rasqal RDF Query GRAPH Tests
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


#define DATA_GRAPH_COUNT 4
static const char* graph_files[DATA_GRAPH_COUNT] = {
  "graph-a.ttl",
  "graph-b.ttl",
  "graph-c.ttl",
  "one.nt"
};

static const char* query_language_name = "sparql";

#define QUERY_VARIABLES_MAX_COUNT 1

struct test
{
  /* expected result count */
  int expected_count;
  /* data graph offsets (<0 for not used) */
  int data_graphs[DATA_GRAPH_COUNT];
  /* expected value answers */
  const char* value_answers[QUERY_VARIABLES_MAX_COUNT];
};

static const unsigned char* query_string = (unsigned char*)"\
SELECT (count(*) as ?count) WHERE {\
   ?s ?p ?o .\
}";

#define DATASETS_COUNT 2
static const struct test tests[DATASETS_COUNT] = {
  { /* expected_count */  1,
    /* data_graphs */ { 0, 1, 2, -1 },
    /* value_answers */ { "9" }
  },
  { /* expected_count */  1,
    /* data_graphs */ { 0, 3, -1, -1 },
    /* value_answers */ { "4" }
  }
};


#else
#define NO_QUERY_LANGUAGE
#endif


#ifdef NO_QUERY_LANGUAGE
int
main(int argc, char **argv) {
  const char *program=rasqal_basename(argv[0]);
  fprintf(stderr, "%s: No supported query language available, skipping test\n", program);
  return(0);
}
#else

int
main(int argc, char **argv) {
  const char *program=rasqal_basename(argv[0]);
  int failures=0;
  raptor_uri *base_uri;
  unsigned char *data_dir_string;
  raptor_uri* data_dir_uri;
  unsigned char *uri_string;
  int i, j, k;
  raptor_uri* graph_uris[DATA_GRAPH_COUNT];
  rasqal_world *world;
  rasqal_query *query = NULL;
  rasqal_data_graphs_set *graphs_set = NULL;
  int query_failed=0;

  if(argc != 2) {
    fprintf(stderr, "USAGE: %s <path to data directory>\n", program);
    return(1);
  }

  world=rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    return(1);
  }

  uri_string=raptor_uri_filename_to_uri_string("");
  base_uri = raptor_new_uri(world->raptor_world_ptr, uri_string);
  raptor_free_memory(uri_string);


  data_dir_string=raptor_uri_filename_to_uri_string(argv[1]);
  data_dir_uri = raptor_new_uri(world->raptor_world_ptr, data_dir_string);

  query=rasqal_new_query(world, query_language_name, NULL);
  if(!query) {
    fprintf(stderr, "%s: creating query in language %s FAILED\n",
            program, query_language_name);
    query_failed=1;
    goto tidy_query;
  }

  printf("%s: preparing %s query\n", program, query_language_name);
  if(rasqal_query_prepare(query, query_string, base_uri)) {
    fprintf(stderr, "%s: %s query prepare FAILED\n", program,
            query_language_name);
    query_failed=1;
    goto tidy_query;
  }

  for(i=0; i < DATA_GRAPH_COUNT; i++)
#ifdef RAPTOR_V2_AVAILABLE
    graph_uris[i] = raptor_new_uri_relative_to_base(world->raptor_world_ptr,
                                                    data_dir_uri,
                                                    (const unsigned char*)graph_files[i]);
#else
    graph_uris[i] = raptor_new_uri_relative_to_base(data_dir_uri,
                                                    (const unsigned char*)graph_files[i]);
#endif

  for(j=0; j < DATASETS_COUNT; ++j) {
    int count;
    rasqal_query_results *results = NULL;

    graphs_set = rasqal_data_graphs_set_new();
    for(k=0; k < DATA_GRAPH_COUNT; k++) {
      int offset=tests[j].data_graphs[k];
      if(offset >= 0) {
        rasqal_data_graph* dg;
        dg = rasqal_new_data_graph_from_uri(world,
                                            /* source URI */ graph_uris[offset],
                                            /* name URI */ NULL,
                                            RASQAL_DATA_GRAPH_BACKGROUND,
                                            NULL, NULL, NULL);
        rasqal_data_graphs_set_add_data_graph(graphs_set, dg);
      }
    }

    printf("%s: executing query with dataset %d\n", program, j);
    results=rasqal_query_execute2(query, graphs_set);
    if(!results) {
      fprintf(stderr, "%s: query execution with dataset %d FAILED\n", program, j);
      query_failed=1;
      goto tidy_query;
    }

    printf("%s: checking query with dataset %d results\n", program, j);
    count=0;
    query_failed=0;
    while(results && !rasqal_query_results_finished(results)) {
      rasqal_literal *value;
      const char *value_answer=tests[j].value_answers[count];
      const unsigned char* value_var=(const unsigned char*)"count";

      value=rasqal_query_results_get_binding_value_by_name(results,
                                                           value_var);
      if(strcmp((const char*)rasqal_literal_as_string(value), value_answer)) {
        printf("result %d FAILED: %s=", count, (char*)value_var);
        rasqal_literal_print(value, stdout);
        printf(" expected value '%s'\n", value_answer);
        query_failed=1;
        count++;
        break;
      }

      rasqal_query_results_next(results);
      count++;
    }
    if(results)
      rasqal_free_query_results(results);

    printf("%s: query with dataset %d results count returned %d results\n", program, j,
           count);
    if(count != tests[j].expected_count) {
      printf("%s: query execution with dataset %d FAILED returning %d results, expected %d\n",
             program, j, count, tests[j].expected_count);
      query_failed=1;
    }

    if(!query_failed)
      printf("%s: query with dataset %d OK\n", program, j);
    else {
      printf("%s: query with dataset %d FAILED\n", program, j);
      failures++;
    }

    rasqal_free_data_graphs_set(graphs_set);
    graphs_set = NULL;
  }

tidy_query:

  rasqal_free_query(query);
  if(graphs_set) rasqal_free_data_graphs_set(graphs_set);

  for(i=0; i < DATA_GRAPH_COUNT; i++) {
    if(graph_uris[i])
      raptor_free_uri(graph_uris[i]);
  }
  raptor_free_uri(data_dir_uri);
  raptor_free_memory(data_dir_string);

  raptor_free_uri(base_uri);

  rasqal_free_world(world);

  return failures;
}

#endif

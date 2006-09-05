/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_order_test.c - Rasqal RDF Query Order Tests
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

static const char* animalsList[27]={ "aardvark", "badger", "cow", "dog",
  "elephant", "fox", "goat", "horse", "iguana", "jackal", "koala", "lemur",
  "mouse", "newt", "owl", "penguin", "quail", "rat", "snake", "tiger",
  "uakari", "vole", "whale", "xantus", "yak", "zebra", NULL };

#define QUERY_FORMAT "\
PREFIX ex: <http://ex.example.org#> \
SELECT $animal \
FROM <%s> \
WHERE { \
  $zoo ex:hasAnimal $animal \
} ORDER BY $animal LIMIT 10"

#define EXPECTED_RESULTS_COUNT 10

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
  rasqal_query *query = NULL;
  rasqal_query_results *results = NULL;
  raptor_uri *base_uri;
  unsigned char *data_string;
  unsigned char *uri_string;
  const char *query_language_name=QUERY_LANGUAGE;
  const char *query_format=QUERY_FORMAT;
  unsigned char *query_string;
  int count;
  int failures=0;
  
  rasqal_init();
  
  if(argc != 2) {
    fprintf(stderr, "USAGE: %s <path to animals.nt>\n", program);
    return(1);
  }
    
  data_string=raptor_uri_filename_to_uri_string(argv[1]);
  query_string=(unsigned char*)RASQAL_MALLOC(cstring, strlen((const char*)data_string)+strlen(query_format)+1);
  sprintf((char*)query_string, query_format, data_string);
  raptor_free_memory(data_string);
  
  uri_string=raptor_uri_filename_to_uri_string("");
  base_uri=raptor_new_uri(uri_string);  
  raptor_free_memory(uri_string);

  query=rasqal_new_query(query_language_name, NULL);
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

  printf("%s: executing query\n", program);
  results=rasqal_query_execute(query);
  if(!results) {
    fprintf(stderr, "%s: query execution FAILED\n", program);
    return(1);
  }

  printf("%s: checking results\n", program);
  count=0;
  while(results && !rasqal_query_results_finished(results)) {
    const unsigned char *name=(const unsigned char *)"animal";
    rasqal_literal *value=rasqal_query_results_get_binding_value_by_name(results, name);
    
    const unsigned char *s=rasqal_literal_as_string(value);
    const char *answer=animalsList[count];
    if(strcmp((const char*)s, answer)) {
      printf("result %d FAILED: %s='", count+1, (char*)name);
      rasqal_literal_print(value, stdout);
      printf("' expected value '%s'\n", answer);
      failures++;
    }
    rasqal_query_results_next(results);
    count++;
  }
  if(results)
    rasqal_free_query_results(results);

  printf("%s: checking count\n", program);
  if(count != EXPECTED_RESULTS_COUNT) {
    fprintf(stderr, "%s: query execution 2 returned %d results, expected %d\n", 
            program, count, EXPECTED_RESULTS_COUNT);
    return(1);
  }

  printf("%s: done\n", program);

  rasqal_free_query(query);

  raptor_free_uri(base_uri);

  rasqal_finish();

  return failures;
}

#endif

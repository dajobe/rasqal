/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_query_test.c - Rasqal RDF Query Tests
 *
 * $Id$
 *
 * Copyright (C) 2004 David Beckett - http://purl.org/net/dajobe/
 * Institute for Learning and Research Technology - http://www.ilrt.bris.ac.uk/
 * University of Bristol - http://www.bristol.ac.uk/
 * 
 * This package is Free Software or Open Source available under the
 * following licenses (these are alternatives):
 *   1. GNU Lesser General Public License (LGPL)
 *   2. GNU General Public License (GPL)
 *   3. Mozilla Public License (MPL)
 * 
 * See LICENSE.html or LICENSE.txt at the top of this package for the
 * full license terms.
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


#define DATA_FILE "tests/dc.rdf"

#define QUERY "SELECT ?person \
         FROM <" DATA_FILE "> \
         WHERE \
         (?person, rdf:type, foaf:Person) USING \
         rdf FOR <http://www.w3.org/1999/02/22-rdf-syntax-ns#>, \
         foaf FOR <http://xmlns.com/foaf/0.1/>"

int
main(int argc, char **argv) {
  rasqal_query *query = NULL;
  rasqal_query_results *results = NULL;
  raptor_uri *base_uri;
  char *uri_string;
  const char *query_string=QUERY;

  rasqal_init();

  uri_string=raptor_uri_filename_to_uri_string("");
  base_uri=raptor_new_uri(uri_string);  
  raptor_free_memory(uri_string);

  query=rasqal_new_query("rdql", NULL);

  printf("Prepare\n");
  rasqal_query_prepare(query, query_string,base_uri);

  printf("Execute 1\n");
  results=rasqal_query_execute(query);
  while(results && !rasqal_query_results_finished(results)) {
    int i;
    for(i=0; i<rasqal_query_results_get_bindings_count(results); i++) {
      const char *name=rasqal_query_results_get_binding_name(results, i);
      rasqal_literal *value=rasqal_query_results_get_binding_value(results, i);

      printf("variable %s=", name);
      rasqal_literal_print(value, stdout);
      putchar('\n');
    }
    rasqal_query_results_next(results);
  }
  if(results)
    rasqal_free_query_results(results);
#if 0
  printf("Execute 2\n");
  results = rasqal_query_execute(query);
  results ? printf("Success!\n") : printf("Failure!\n");

  if(results)
    rasqal_free_query_results(results);
#endif

  rasqal_free_query(query);

  raptor_free_uri(base_uri);

  rasqal_finish();

  return 0;
}

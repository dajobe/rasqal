/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_format_turtle.c - Format results in Turtle
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


/*
 * rasqal_query_results_write_turtle:
 * @iostr: #raptor_iostream to write the query to
 * @results: #rasqal_query_results query results format
 * @base_uri: #raptor_uri base URI of the output format
 *
 * Write a Turtle version of the query results format to an
 * iostream in a format - INTERNAL.
 * 
 * If the writing succeeds, the query results will be exhausted.
 * 
 * Return value: non-0 on failure
 **/
static int
rasqal_query_results_write_turtle(rasqal_query_results_formatter* formatter,
                                  raptor_iostream *iostr,
                                  rasqal_query_results* results,
                                  raptor_uri *base_uri)
{
  rasqal_world* world = rasqal_query_results_get_world(results);
  int i;
  int row_semicolon;
  int column_semicolon = 0;
  int size;
  
  if(!rasqal_query_results_is_bindings(results)) {
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                            "Can only write Turtle format for variable binding results");
    return 1;
  }
  
  
  raptor_iostream_string_write("@prefix xsd:     <http://www.w3.org/2001/XMLSchema#> .\n", iostr);
  raptor_iostream_string_write("@prefix rs:      <http://www.w3.org/2001/sw/DataAccess/tests/result-set#> .\n", iostr);
  raptor_iostream_string_write("@prefix rdf:     <http://www.w3.org/1999/02/22-rdf-syntax-ns#> .\n", iostr);
  raptor_iostream_write_byte('\n', iostr);

  raptor_iostream_counted_string_write("[]    rdf:type      rs:ResultSet ;\n",
                                       35, iostr);


  /* Variable Binding Results */

  for(i = 0; 1; i++) {
    const unsigned char *name;
    
    name = rasqal_query_results_get_binding_name(results, i);
    if(!name)
      break;
      
    raptor_iostream_counted_string_write("      rs:resultVariable  \"",
                                         26, iostr);
    raptor_iostream_string_write(name, iostr);
    raptor_iostream_counted_string_write("\" ;\n", 4, iostr);
  }

  size = rasqal_query_results_get_bindings_count(results);
  row_semicolon = 0;

  while(!rasqal_query_results_finished(results)) {
    if(row_semicolon)
      raptor_iostream_counted_string_write(" ;\n", 3, iostr);

    /* Result row */
    raptor_iostream_counted_string_write("      rs:solution   [ ", 22, iostr);

    column_semicolon = 0;
    for(i = 0; i < size; i++) {
      const unsigned char *name;
      rasqal_literal *l;

      name = rasqal_query_results_get_binding_name(results, i);
      l = rasqal_query_results_get_binding_value(results, i);

      if(column_semicolon)
        raptor_iostream_counted_string_write("; \n                      ",
                                             25, iostr);

      /* binding */
      raptor_iostream_counted_string_write("rs:binding    [ ", 16, iostr);

      /* only emit rs:value and rs:variable triples if there is a value */
      if(l) {
        raptor_iostream_counted_string_write("rs:variable   \"", 15, iostr);
        raptor_iostream_string_write(name, iostr);
        raptor_iostream_counted_string_write("\" ;\n                                      rs:value      ", 56, iostr);
        rasqal_literal_write_turtle(l, iostr);
        raptor_iostream_counted_string_write("\n                                    ] ", 39, iostr);
        column_semicolon = 1;
      }

    }

    /* End Result Row */
    raptor_iostream_counted_string_write("\n      ]", 8, iostr);
    row_semicolon = 1;
    
    rasqal_query_results_next(results);
  }

  raptor_iostream_counted_string_write(" .\n", 3, iostr);

  return 0;
}


static const char* const turtle_names[2] = { "turtle", NULL};

#define TURTLE_TYPES_COUNT 1
static const raptor_type_q turtle_types[TURTLE_TYPES_COUNT + 1] = {
  { "application/turtle", 18, 10}, 
  { NULL, 0, 0}
};

static int
rasqal_query_results_turtle_register_factory(rasqal_query_results_format_factory *factory) 
{
  int rc = 0;

  factory->desc.names = turtle_names;

  factory->desc.mime_types = turtle_types;
  factory->desc.mime_types_count = TURTLE_TYPES_COUNT;

  factory->desc.label = "Turtle Query Results";

  factory->desc.uri_string = "http://www.w3.org/TeamSubmission/turtle/";

  factory->desc.flags = 0;
  
  factory->write         = rasqal_query_results_write_turtle;
  factory->get_rowsource = NULL;

  return rc;
}


int
rasqal_init_result_format_turtle(rasqal_world* world)
{
  return !rasqal_world_register_query_results_format_factory(world,
                                                             &rasqal_query_results_turtle_register_factory);
}

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
rasqal_query_results_write_turtle(raptor_iostream *iostr,
                                  rasqal_query_results* results,
                                  raptor_uri *base_uri)
{
  rasqal_world* world = rasqal_query_results_get_world(results);
  int i;
  int row_semicolon;
  int column_semicolon = 0;
  
  if(!rasqal_query_results_is_bindings(results) &&
     !rasqal_query_results_is_boolean(results)) {
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                            "Can only write Turtle format for variable binding and boolean results");
    return 1;
  }
  
  
  raptor_iostream_string_write("@prefix xsd:     <http://www.w3.org/2001/XMLSchema#> .\n", iostr);
  raptor_iostream_string_write("@prefix rs:      <http://www.w3.org/2001/sw/DataAccess/tests/result-set#> .\n", iostr);
  raptor_iostream_string_write("@prefix rdf:     <http://www.w3.org/1999/02/22-rdf-syntax-ns#> .\n", iostr);
  raptor_iostream_write_byte('\n', iostr);

  raptor_iostream_counted_string_write("[]    rdf:type      rs:ResultSet ;\n",
                                       35, iostr);


  /* Header */
  if(rasqal_query_results_is_bindings(results)) {
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
  }

  /* Variable Binding Results */


  row_semicolon = 0;
  while(!rasqal_query_results_finished(results)) {
    int size = rasqal_query_results_get_bindings_count(results);
    
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
        raptor_iostream_counted_string_write("; \n                      ", 25, iostr);

      /* binding */
      raptor_iostream_counted_string_write("rs:binding    [ ", 16, iostr);

      if(!l) {
        /* no value: do not emit rs:value or rs:variable triples */
        ;
      } else { 
        const unsigned char* str;
        size_t len;

        raptor_iostream_counted_string_write("rs:variable   \"", 15, iostr);
        raptor_iostream_string_write(name, iostr);
        raptor_iostream_counted_string_write("\" ;\n                                      rs:value      ", 56, iostr);
        
        switch(l->type) {
          case RASQAL_LITERAL_URI:
            str = (const unsigned char*)raptor_uri_as_counted_string(l->value.uri, &len);

            raptor_iostream_write_byte('<', iostr);
            if(str)
              raptor_string_ntriples_write(str, len, '>', iostr);
            raptor_iostream_write_byte('>', iostr);
            break;

          case RASQAL_LITERAL_BLANK:
            raptor_iostream_counted_string_write("_:", 2, iostr);
            raptor_iostream_counted_string_write(l->string, l->string_len, iostr);
            break;

          case RASQAL_LITERAL_STRING:
            raptor_iostream_write_byte('"', iostr);
            raptor_string_ntriples_write(l->string, l->string_len, '"', iostr);
            raptor_iostream_write_byte('"', iostr);

            if(l->language) {
              raptor_iostream_write_byte('@', iostr);
              raptor_iostream_string_write(l->language, iostr);
            }
            if(l->datatype) {
              str = (const unsigned char*)raptor_uri_as_counted_string(l->datatype, &len);
              raptor_iostream_counted_string_write("^^<", 3, iostr);
              raptor_string_ntriples_write(str, len, '>', iostr);
              raptor_iostream_write_byte('>', iostr);
            }

            break;

          case RASQAL_LITERAL_PATTERN:
          case RASQAL_LITERAL_QNAME:
          case RASQAL_LITERAL_INTEGER:
          case RASQAL_LITERAL_XSD_STRING:
          case RASQAL_LITERAL_BOOLEAN:
          case RASQAL_LITERAL_DOUBLE:
          case RASQAL_LITERAL_FLOAT:
          case RASQAL_LITERAL_VARIABLE:
          case RASQAL_LITERAL_DECIMAL:
          case RASQAL_LITERAL_DATETIME:
          case RASQAL_LITERAL_UDT:
          case RASQAL_LITERAL_INTEGER_SUBTYPE:

          case RASQAL_LITERAL_UNKNOWN:
          default:
            rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                                    "Cannot turn literal type %d into Turtle", 
                                    l->type);
        }

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


int
rasqal_init_result_format_turtle(rasqal_world* world)
{
  rasqal_query_results_formatter_func writer_fn = NULL;
  rasqal_query_results_formatter_func reader_fn = NULL;
  rasqal_query_results_get_rowsource_func get_rowsource_fn = NULL;
  int rc = 0;

  writer_fn = &rasqal_query_results_write_turtle;
  reader_fn = NULL;
  get_rowsource_fn = NULL;

  /* DAWG query results format in Turtle */
  rc += rasqal_query_results_format_register_factory(world,
                                                     "turtle",
                                                     "Turtle Query Results",
                                                     (unsigned char*)"http://www.w3.org/2001/sw/DataAccess/tests/result-set#",
                                                     writer_fn, reader_fn, get_rowsource_fn,
                                                     "application/json")
                                                     != 0;
  return rc;
}

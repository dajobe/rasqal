/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_format_csv.c - Format results in CSV
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


/*
 * rasqal_query_results_write_csv:
 * @iostr: #raptor_iostream to write the query to
 * @results: #rasqal_query_results query results format
 * @base_uri: #raptor_uri base URI of the output format
 *
 * INTERNAL - Write a CSV version of the query results format to an iostream.
 * 
 * If the writing succeeds, the query results will be exhausted.
 * 
 * Return value: non-0 on failure
 **/
static int
rasqal_query_results_write_csv(raptor_iostream *iostr,
                               rasqal_query_results* results,
                               raptor_uri *base_uri)
{
  rasqal_query* query = rasqal_query_results_get_query(results);
  int i;
  int column_comma = 0;
  int count = 1;
#define comma_str_len 1
  static const char comma_str[comma_str_len+1]=",";
#define nl_str_len 1
  static const char nl_str[nl_str_len+1]="\n";
  int vars_count;
  
  if(!rasqal_query_results_is_bindings(results)) {
    rasqal_log_error_simple(query->world, RAPTOR_LOG_LEVEL_ERROR,
                            &query->locator,
                            "Can only write CSV format for variable binding results");
    return 1;
  }
  
  
  /* Header */
  raptor_iostream_write_counted_string(iostr, "Variables", 9);
  raptor_iostream_write_counted_string(iostr, comma_str, comma_str_len);
 
  for(i = 0; 1; i++) {
    const unsigned char *name;
    
    name = rasqal_query_results_get_binding_name(results, i);
    if(!name)
      break;
    
    if(i > 0)
      raptor_iostream_write_counted_string(iostr, comma_str, comma_str_len);
    raptor_iostream_write_byte(iostr, '\"');
    raptor_iostream_write_string(iostr, name);
    raptor_iostream_write_byte(iostr, '\"');
  }
  raptor_iostream_write_counted_string(iostr, nl_str, nl_str_len);


  /* Variable Binding Results */
  vars_count = rasqal_query_results_get_bindings_count(results);
  while(!rasqal_query_results_finished(results)) {
    /* Result row */
    raptor_iostream_write_decimal(iostr, count++);
    raptor_iostream_write_counted_string(iostr, comma_str, comma_str_len);

    column_comma = 0;
    for(i = 0; i < vars_count; i++) {
      rasqal_literal *l = rasqal_query_results_get_binding_value(results, i);

      if(column_comma)
        raptor_iostream_write_counted_string(iostr, comma_str, comma_str_len);

      if(!l) {
        raptor_iostream_write_string(iostr, "\"null\"");
      } else switch(l->type) {
        const unsigned char* str;
        size_t len;
        
        case RASQAL_LITERAL_URI:
          raptor_iostream_write_string(iostr, "uri(");
#ifdef RAPTOR_V2_AVAILABLE
          str = (const unsigned char*)raptor_uri_as_counted_string_v2(l->world->raptor_world_ptr, l->value.uri, &len);
#else
          str = (const unsigned char*)raptor_uri_as_counted_string(l->value.uri, &len);
#endif
          raptor_iostream_write_string_ntriples(iostr, str, len, '"');
          raptor_iostream_write_byte(iostr, ')');
          break;

        case RASQAL_LITERAL_BLANK:
          raptor_iostream_write_string(iostr, "blank(");
          raptor_iostream_write_string_ntriples(iostr,
                                                (const unsigned char*)l->string, 
                                                l->string_len, '"');
          raptor_iostream_write_byte(iostr, ')');
          break;

        case RASQAL_LITERAL_STRING:
          raptor_iostream_write_byte(iostr, '"');
          raptor_iostream_write_string_ntriples(iostr,
                                                (const unsigned char*)l->string,
                                                l->string_len, '"');
          raptor_iostream_write_byte(iostr, '"');

          if(l->language) {
            raptor_iostream_write_byte(iostr, '@');
            raptor_iostream_write_string(iostr,
                                         (const unsigned char*)l->language);
          }
          
          if(l->datatype) {
            raptor_iostream_write_string(iostr, "^^uri(");
#ifdef RAPTOR_V2_AVAILABLE
            str = (const unsigned char*)raptor_uri_as_counted_string_v2(l->world->raptor_world_ptr, l->datatype, &len);
#else
            str = (const unsigned char*)raptor_uri_as_counted_string(l->datatype, &len);
#endif
            raptor_iostream_write_string_ntriples(iostr, str, len, '"');
            raptor_iostream_write_byte(iostr, ')');
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

        case RASQAL_LITERAL_UNKNOWN:
        default:
          rasqal_log_error_simple(query->world, RAPTOR_LOG_LEVEL_ERROR,
                                  &query->locator,
                                  "Cannot turn literal type %d into CSV", 
                                  l->type);
      }

      /* End Binding */

      column_comma = 1;
    }

    /* End Result Row */
    raptor_iostream_write_counted_string(iostr, nl_str, nl_str_len);
    
    rasqal_query_results_next(results);
  }

  /* end sparql */
  return 0;
}


int
rasqal_init_result_format_csv(rasqal_world* world)
{
  rasqal_query_results_formatter_func writer_fn=NULL;
  writer_fn = &rasqal_query_results_write_csv;

  return rasqal_query_results_format_register_factory(world,
                                                      "csv",
                                                      "Comma Separated Values (CSV)",
                                                      NULL,
                                                      writer_fn,
                                                      NULL, NULL,
                                                      "text/csv");
}

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_format_json.c - Format results in SPARQL JSON
 *
 * Copyright (C) 2003-2009, David Beckett http://www.dajobe.org/
 * Copyright (C) 2003-2005, University of Bristol, UK http://www.bristol.ac.uk/
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


static void
rasqal_iostream_write_json_boolean(raptor_iostream* iostr, 
                                   const char* name, int json_bool)
{
  raptor_iostream_write_byte(iostr, '\"');
  raptor_iostream_write_string(iostr, name);
  raptor_iostream_write_counted_string(iostr, "\" : ",4);

  if(json_bool)
    raptor_iostream_write_counted_string(iostr, "true", 4);
  else
    raptor_iostream_write_counted_string(iostr, "false", 5);

}


/*
 * rasqal_query_results_write_json1:
 * @iostr: #raptor_iostream to write the query to
 * @results: #rasqal_query_results query results format
 * @base_uri: #raptor_uri base URI of the output format
 *
 * Write a JSON version of the query results format to an
 * iostream in a format - INTERNAL.
 * 
 * If the writing succeeds, the query results will be exhausted.
 * 
 * Return value: non-0 on failure
 **/
static int
rasqal_query_results_write_json1(raptor_iostream *iostr,
                                 rasqal_query_results* results,
                                 raptor_uri *base_uri)
{
  rasqal_query* query = rasqal_query_results_get_query(results);
  int i;
  int row_comma;
  int column_comma = 0;
  
  if(!rasqal_query_results_is_bindings(results) &&
     !rasqal_query_results_is_boolean(results)) {
    rasqal_log_error_simple(query->world, RAPTOR_LOG_LEVEL_ERROR,
                            &query->locator,
                            "Can only write JSON format for variable binding and boolean results");
    return 1;
  }
  
  
  raptor_iostream_write_counted_string(iostr, "{\n", 2);
  
  /* Header */
  raptor_iostream_write_counted_string(iostr, "  \"head\": {\n", 12);
  
  if(rasqal_query_results_is_bindings(results)) {
    raptor_iostream_write_counted_string(iostr, "    \"vars\": [ ", 14);
    for(i = 0; 1; i++) {
      const unsigned char *name;
      
      name = rasqal_query_results_get_binding_name(results, i);
      if(!name)
        break;
      
      /*     'x', */
      if(i > 0)
        raptor_iostream_write_counted_string(iostr, ", ", 2);
      raptor_iostream_write_byte(iostr, '\"');
      raptor_iostream_write_string(iostr, name);
      raptor_iostream_write_byte(iostr, '\"');
    }
    raptor_iostream_write_counted_string(iostr, " ]\n", 3);
  }

  /* FIXME - could add link inside 'head': */
    
  /*   End Header */
  raptor_iostream_write_counted_string(iostr, "  },\n", 5);


  /* Boolean Results */
  if(rasqal_query_results_is_boolean(results)) {
    raptor_iostream_write_counted_string(iostr, "  ", 2);
    rasqal_iostream_write_json_boolean(iostr, "boolean", 
                                       rasqal_query_results_get_boolean(results));
    goto results3done;
  }

  /* Variable Binding Results */
  raptor_iostream_write_counted_string(iostr, "  \"results\": {\n", 15);

  raptor_iostream_write_counted_string(iostr, "    ", 4);
  rasqal_iostream_write_json_boolean(iostr, "ordered", 
                                     (rasqal_query_get_order_condition(query, 0) != NULL));
  raptor_iostream_write_counted_string(iostr, ",\n", 2);

  raptor_iostream_write_counted_string(iostr, "    ", 4);
  rasqal_iostream_write_json_boolean(iostr, "distinct", 
                                     rasqal_query_get_distinct(query));
  raptor_iostream_write_counted_string(iostr, ",\n", 2);

  raptor_iostream_write_counted_string(iostr, "    \"bindings\" : [\n", 19);

  row_comma = 0;
  while(!rasqal_query_results_finished(results)) {
    if(row_comma)
      raptor_iostream_write_counted_string(iostr, ",\n", 2);

    /* Result row */
    raptor_iostream_write_counted_string(iostr, "      {\n", 8);

    column_comma = 0;
    for(i = 0; i<rasqal_query_results_get_bindings_count(results); i++) {
      const unsigned char *name = rasqal_query_results_get_binding_name(results, i);
      rasqal_literal *l = rasqal_query_results_get_binding_value(results, i);

      if(column_comma)
        raptor_iostream_write_counted_string(iostr, ",\n", 2);

      /*       <binding> */
      raptor_iostream_write_counted_string(iostr, "        \"", 9);
      raptor_iostream_write_string(iostr, name);
      raptor_iostream_write_counted_string(iostr, "\" : { ", 6);

      if(!l) {
        raptor_iostream_write_string(iostr, "\"type\": \"unbound\", \"value\": null");
      } else switch(l->type) {
        const unsigned char* str;
        size_t len;
        
        case RASQAL_LITERAL_URI:
          raptor_iostream_write_string(iostr, "\"type\": \"uri\", \"value\": \"");
#ifdef RAPTOR_V2_AVAILABLE
          str = (const unsigned char*)raptor_uri_as_counted_string_v2(l->world->raptor_world_ptr, l->value.uri, &len);
#else
          str = (const unsigned char*)raptor_uri_as_counted_string(l->value.uri, &len);
#endif
          raptor_iostream_write_string_ntriples(iostr, str, len, '"');
          raptor_iostream_write_byte(iostr, '"');
          break;

        case RASQAL_LITERAL_BLANK:
          raptor_iostream_write_string(iostr, "\"type\": \"bnode\", \"value\": \"");
          raptor_iostream_write_string_ntriples(iostr, (const unsigned char*)l->string, 
                                                l->string_len, '"');
          raptor_iostream_write_byte(iostr, '"');
          break;

        case RASQAL_LITERAL_STRING:
          raptor_iostream_write_string(iostr, "\"type\": \"literal\", \"value\": \"");
          raptor_iostream_write_string_ntriples(iostr, (const unsigned char*)l->string,
                                                l->string_len, '"');
          raptor_iostream_write_byte(iostr, '"');

          if(l->language) {
            raptor_iostream_write_string(iostr, ",\n      \"xml:lang\" : \"");
            raptor_iostream_write_string(iostr, (const unsigned char*)l->language);
            raptor_iostream_write_byte(iostr, '"');
          }
          
          if(l->datatype) {
            raptor_iostream_write_string(iostr, ",\n      \"datatype\" : \"");
#ifdef RAPTOR_V2_AVAILABLE
            str = (const unsigned char*)raptor_uri_as_counted_string_v2(l->world->raptor_world_ptr, l->datatype, &len);
#else
            str = (const unsigned char*)raptor_uri_as_counted_string(l->datatype, &len);
#endif
            raptor_iostream_write_string_ntriples(iostr, str, len, '"');
            raptor_iostream_write_byte(iostr, '"');
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
                                  "Cannot turn literal type %d into XML", 
                                  l->type);
      }

      /* End Binding */
      raptor_iostream_write_counted_string(iostr, " }", 2);
      column_comma = 1;
    }

    /* End Result Row */
    raptor_iostream_write_counted_string(iostr, "\n      }", 8);
    row_comma = 1;
    
    rasqal_query_results_next(results);
  }

  raptor_iostream_write_counted_string(iostr, "\n    ]\n  }", 10);

  results3done:
  
  /* end sparql */
  raptor_iostream_write_counted_string(iostr, "\n}\n", 3);

  return 0;
}


int
rasqal_init_result_format_json(rasqal_world* world)
{
  rasqal_query_results_formatter_func writer_fn = NULL;
  rasqal_query_results_formatter_func reader_fn = NULL;
  rasqal_query_results_get_rowsource_func get_rowsource_fn = NULL;
  int rc = 0;

  writer_fn = &rasqal_query_results_write_json1;
  reader_fn = NULL;
  get_rowsource_fn = NULL;

  /* Released DAWG WG results in JSON */
  rc += rasqal_query_results_format_register_factory(world,
                                                     "json",
                                                     "SPARQL JSON Query Results",
                                                     (unsigned char*)"http://www.w3.org/TR/2007/NOTE-rdf-sparql-json-res-20070618/",
                                                     writer_fn, reader_fn, get_rowsource_fn,
                                                     "application/json")
                                                     != 0;
  /* URIs from 0.9.16 or earlier */
  rc+= rasqal_query_results_format_register_factory(world,
                                                    NULL,
                                                    NULL,
                                                    (unsigned char*)"http://www.w3.org/2001/sw/DataAccess/json-sparql/",
                                                    writer_fn, reader_fn, get_rowsource_fn,
                                                    "application/json")
                                                    != 0;
  rc+= rasqal_query_results_format_register_factory(world,
                                                    NULL,
                                                    NULL,
                                                    (unsigned char*)"http://www.mindswap.org/%7Ekendall/sparql-results-json/",
                                                    writer_fn, reader_fn, get_rowsource_fn,
                                                    "application/json")
                                                    != 0;
  return rc;
}

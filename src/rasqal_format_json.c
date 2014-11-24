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
  raptor_iostream_write_byte('\"', iostr);
  raptor_iostream_string_write(name, iostr);
  raptor_iostream_counted_string_write("\" : ",4, iostr);

  if(json_bool)
    raptor_iostream_counted_string_write("true", 4, iostr);
  else
    raptor_iostream_counted_string_write("false", 5, iostr);

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
rasqal_query_results_write_json1(rasqal_query_results_formatter* formatter,
                                 raptor_iostream *iostr,
                                 rasqal_query_results* results,
                                 raptor_uri *base_uri)
{
  rasqal_world* world = rasqal_query_results_get_world(results);
  rasqal_query* query = rasqal_query_results_get_query(results);
  int i;
  int row_comma;
  int column_comma = 0;
  rasqal_query_results_type type;

  type = rasqal_query_results_get_type(results);

  if(type != RASQAL_QUERY_RESULTS_BINDINGS &&
     type != RASQAL_QUERY_RESULTS_BOOLEAN) {
    rasqal_log_error_simple(query->world, RAPTOR_LOG_LEVEL_ERROR,
                            &query->locator,
                            "Cannot write JSON for %s query result format",
                            rasqal_query_results_type_label(type));
    return 1;
  }

  raptor_iostream_counted_string_write("{\n", 2, iostr);
  
  /* Header */
  raptor_iostream_counted_string_write("  \"head\": {\n", 12, iostr);
  
  if(rasqal_query_results_is_bindings(results)) {
    raptor_iostream_counted_string_write("    \"vars\": [ ", 14, iostr);
    for(i = 0; 1; i++) {
      const unsigned char *name;
      
      name = rasqal_query_results_get_binding_name(results, i);
      if(!name)
        break;
      
      /*     'x', */
      if(i > 0)
        raptor_iostream_counted_string_write(", ", 2, iostr);
      raptor_iostream_write_byte('\"', iostr);
      raptor_iostream_string_write(name, iostr);
      raptor_iostream_write_byte('\"', iostr);
    }
    raptor_iostream_counted_string_write(" ]\n", 3, iostr);
  }

  /* FIXME - could add link inside 'head': */
    
  /*   End Header */
  raptor_iostream_counted_string_write("  },\n", 5, iostr);


  /* Boolean Results */
  if(rasqal_query_results_is_boolean(results)) {
    raptor_iostream_counted_string_write("  ", 2, iostr);
    rasqal_iostream_write_json_boolean(iostr, "boolean", 
                                       rasqal_query_results_get_boolean(results));
    goto results3done;
  }

  /* Variable Binding Results */
  raptor_iostream_counted_string_write("  \"results\": {\n", 15, iostr);

  if(query) {
    raptor_iostream_counted_string_write("    ", 4, iostr);
    rasqal_iostream_write_json_boolean(iostr, "ordered", 
                                       (rasqal_query_get_order_condition(query, 0) != NULL));
    raptor_iostream_counted_string_write(",\n", 2, iostr);

    raptor_iostream_counted_string_write("    ", 4, iostr);
    rasqal_iostream_write_json_boolean(iostr, "distinct", 
                                       rasqal_query_get_distinct(query));
    raptor_iostream_counted_string_write(",\n", 2, iostr);
  }
  
  raptor_iostream_counted_string_write("    \"bindings\" : [\n", 19, iostr);

  row_comma = 0;
  while(!rasqal_query_results_finished(results)) {
    if(row_comma)
      raptor_iostream_counted_string_write(",\n", 2, iostr);

    /* Result row */
    raptor_iostream_counted_string_write("      {\n", 8, iostr);

    column_comma = 0;
    for(i = 0; i<rasqal_query_results_get_bindings_count(results); i++) {
      const unsigned char *name = rasqal_query_results_get_binding_name(results, i);
      rasqal_literal *l = rasqal_query_results_get_binding_value(results, i);

      if(column_comma)
        raptor_iostream_counted_string_write(",\n", 2, iostr);

      /*       <binding> */
      raptor_iostream_counted_string_write("        \"", 9, iostr);
      raptor_iostream_string_write(name, iostr);
      raptor_iostream_counted_string_write("\" : { ", 6, iostr);

      if(!l) {
        raptor_iostream_string_write("\"type\": \"unbound\", \"value\": null", iostr);
      } else {
        const unsigned char* str;
        size_t len;

        switch(l->type) {
          case RASQAL_LITERAL_URI:
            raptor_iostream_string_write("\"type\": \"uri\", \"value\": \"", iostr);
            str = RASQAL_GOOD_CAST(const unsigned char*, raptor_uri_as_counted_string(l->value.uri, &len));
            raptor_string_ntriples_write(str, len, '"', iostr);
            raptor_iostream_write_byte('"', iostr);
            break;

          case RASQAL_LITERAL_BLANK:
            raptor_iostream_string_write("\"type\": \"bnode\", \"value\": \"", iostr);
            raptor_string_ntriples_write(l->string, l->string_len, '"', iostr);
            raptor_iostream_write_byte('"', iostr);
            break;

          case RASQAL_LITERAL_STRING:
            raptor_iostream_string_write("\"type\": \"literal\", \"value\": \"", iostr);
            raptor_string_ntriples_write(l->string, l->string_len, '"', iostr);
            raptor_iostream_write_byte('"', iostr);

            if(l->language) {
              raptor_iostream_string_write(",\n      \"xml:lang\" : \"", iostr);
              raptor_iostream_string_write(RASQAL_GOOD_CAST(const unsigned char*, l->language), iostr);
              raptor_iostream_write_byte('"', iostr);
            }

            if(l->datatype) {
              raptor_iostream_string_write(",\n      \"datatype\" : \"", iostr);
              str = RASQAL_GOOD_CAST(const unsigned char*, raptor_uri_as_counted_string(l->datatype, &len));
              raptor_string_ntriples_write(str, len, '"', iostr);
              raptor_iostream_write_byte('"', iostr);
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
          case RASQAL_LITERAL_DATE:
          case RASQAL_LITERAL_DATETIME:
          case RASQAL_LITERAL_UDT:
          case RASQAL_LITERAL_INTEGER_SUBTYPE:

          case RASQAL_LITERAL_UNKNOWN:
          default:
            rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                                    "Cannot turn literal type %u into XML",
                                    l->type);
        }
      }

      /* End Binding */
      raptor_iostream_counted_string_write(" }", 2, iostr);
      column_comma = 1;
    }

    /* End Result Row */
    raptor_iostream_counted_string_write("\n      }", 8, iostr);
    row_comma = 1;
    
    rasqal_query_results_next(results);
  }

  raptor_iostream_counted_string_write("\n    ]\n  }", 10, iostr);

  results3done:
  
  /* end sparql */
  raptor_iostream_counted_string_write("\n}\n", 3, iostr);

  return 0;
}


static const char* const json_names[] = { "json", NULL};

static const char* const json_uri_strings[] = {
  "http://www.w3.org/ns/formats/SPARQL_Results_JSON",
  /* W3C Working Draft */
  "http://www.w3.org/TR/sparql11-results-json/",
  /* W3C Working Group Note */
  "http://www.w3.org/TR/rdf-sparql-json-res/",
  /* Released DAWG WG results in JSON */
  "http://www.w3.org/TR/2007/NOTE-rdf-sparql-json-res-20070618/",
  /* URIs from 0.9.16 or earlier */
  "http://www.w3.org/2001/sw/DataAccess/json-sparql/",
  "http://www.mindswap.org/%7Ekendall/sparql-results-json/",
  NULL
};

static const raptor_type_q json_types[] = {
  { "application/sparql-results+json", 31, 10},
  { "application/json", 16, 10}, 
  { NULL, 0, 0}
};

static int
rasqal_query_results_json_register_factory(rasqal_query_results_format_factory *factory) 
{
  int rc = 0;

  factory->desc.names = json_names;
  factory->desc.mime_types = json_types;

  factory->desc.label = "SPARQL JSON Query Results";
  factory->desc.uri_strings = json_uri_strings;

  factory->desc.flags = 0;
  
  factory->write         = rasqal_query_results_write_json1;
  factory->get_rowsource = NULL;

  return rc;
}


int
rasqal_init_result_format_json(rasqal_world* world)
{
  return !rasqal_world_register_query_results_format_factory(world,
                                                             &rasqal_query_results_json_register_factory);
}

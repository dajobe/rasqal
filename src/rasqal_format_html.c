/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_format_html.c - Format results in HTML Table
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

#include <raptor.h>

/* Rasqal includes */
#include <rasqal.h>
#include <rasqal_internal.h>


static int
rasqal_iostream_write_html_literal(rasqal_world* world,
                                   raptor_iostream *iostr,
                                   rasqal_literal* l)
{
  if(!l) {
    raptor_iostream_counted_string_write("<span class=\"unbound\">", 22, iostr);
    raptor_iostream_counted_string_write("unbound", 7, iostr);
  } else {
    const unsigned char* str;
    size_t len;

    switch(l->type) {
      case RASQAL_LITERAL_URI:
        str = RASQAL_GOOD_CAST(const unsigned char*, raptor_uri_as_counted_string(l->value.uri, &len));
        raptor_iostream_counted_string_write("<span class=\"uri\">", 18, iostr);
        raptor_iostream_counted_string_write("<a href=\"", 9, iostr);
        raptor_xml_escape_string_write(str, len, '"', iostr);
        raptor_iostream_counted_string_write("\">", 2, iostr);
        raptor_xml_escape_string_write(str, len, 0, iostr);
        raptor_iostream_counted_string_write("</a>", 4, iostr);
        break;

      case RASQAL_LITERAL_BLANK:
        raptor_iostream_counted_string_write("<span class=\"blank\">", 20, iostr);
        raptor_xml_escape_string_write(l->string, l->string_len, 0, iostr);
        break;

      case RASQAL_LITERAL_XSD_STRING:
      case RASQAL_LITERAL_BOOLEAN:
      case RASQAL_LITERAL_INTEGER:
      case RASQAL_LITERAL_DOUBLE:
      case RASQAL_LITERAL_STRING:
      case RASQAL_LITERAL_PATTERN:
      case RASQAL_LITERAL_QNAME:
      case RASQAL_LITERAL_FLOAT:
      case RASQAL_LITERAL_DECIMAL:
      case RASQAL_LITERAL_DATE:
      case RASQAL_LITERAL_DATETIME:
      case RASQAL_LITERAL_UDT:
      case RASQAL_LITERAL_INTEGER_SUBTYPE:
        raptor_iostream_counted_string_write("<span class=\"literal\">", 22, iostr);
        raptor_iostream_counted_string_write("<span class=\"value\"", 19, iostr);
        if(l->language) {
          str = RASQAL_GOOD_CAST(const unsigned char*, l->language);
          raptor_iostream_counted_string_write(" xml:lang=\"", 11, iostr);
          raptor_xml_escape_string_write(str, strlen(l->language), '"', iostr);
          raptor_iostream_write_byte('"', iostr);
        }
        raptor_iostream_write_byte('>', iostr);
        raptor_xml_escape_string_write(l->string, l->string_len, 0, iostr);
        raptor_iostream_counted_string_write("</span>", 7, iostr);

        if(l->datatype) {
          raptor_iostream_counted_string_write("^^&lt;<span class=\"datatype\">", 29, iostr);
          str = RASQAL_GOOD_CAST(const unsigned char*, raptor_uri_as_counted_string(l->datatype, &len));
          raptor_xml_escape_string_write(str, len, 0, iostr);
          raptor_iostream_counted_string_write("</span>&gt;", 11, iostr);
        }
        break;

      case RASQAL_LITERAL_VARIABLE:
        return rasqal_iostream_write_html_literal(world, iostr, l->value.variable->value);

      case RASQAL_LITERAL_UNKNOWN:
      default:
        rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                                "Cannot turn literal type %u into HTML",
                                l->type);
        return 1;
    }
  }

  raptor_iostream_counted_string_write("</span>", 7, iostr);
  
  return 0;
}


static int
rasqal_query_results_write_html_bindings(raptor_iostream *iostr,
                                         rasqal_query_results* results)
{
  rasqal_world* world = rasqal_query_results_get_world(results);
  int i;

  raptor_iostream_counted_string_write(
    "  <table id=\"results\" border=\"1\">\n", 34, iostr);

  raptor_iostream_counted_string_write("    <tr>\n", 9, iostr);
  for(i = 0; 1; i++) {
    const unsigned char *name;
    size_t len;
    
    name = rasqal_query_results_get_binding_name(results, i);
    if(!name)
      break;
    
    len = strlen(RASQAL_GOOD_CAST(char*, name));
    raptor_iostream_counted_string_write("      <th>?", 11, iostr);
    raptor_xml_escape_string_write(name, len, 0, iostr);
    raptor_iostream_counted_string_write("</th>\n", 6, iostr);
  }
  raptor_iostream_counted_string_write("    </tr>\n", 10, iostr);


  while(!rasqal_query_results_finished(results)) {
    raptor_iostream_counted_string_write("    <tr class=\"result\">\n", 24,
                                         iostr);

    for(i = 0; i < rasqal_query_results_get_bindings_count(results); i++) {
      rasqal_literal *l = rasqal_query_results_get_binding_value(results, i);

      raptor_iostream_counted_string_write("      <td>", 10, iostr);
      rasqal_iostream_write_html_literal(world, iostr, l);
      raptor_iostream_counted_string_write("</td>\n", 6, iostr);
    }

    raptor_iostream_counted_string_write("    </tr>\n", 10, iostr);
    rasqal_query_results_next(results);
  }

  raptor_iostream_counted_string_write("  </table>\n", 11, iostr);
  
  raptor_iostream_counted_string_write(
    "  <p>Total number of rows: <span class=\"count\">", 47, iostr);
  raptor_iostream_decimal_write(rasqal_query_results_get_count(results), iostr);
  raptor_iostream_counted_string_write("</span>.</p>\n", 13, iostr);

  return 0;
}


static int
rasqal_query_results_write_html_boolean(raptor_iostream *iostr,
                                        rasqal_query_results* results)
{
  raptor_iostream_counted_string_write("  <p>The result of your query is:\n",
                                       34, iostr);
  if(rasqal_query_results_get_boolean(results)) {
    raptor_iostream_counted_string_write("    <span id=\"result\">true</span>\n", 34, iostr);
  } else {
    raptor_iostream_counted_string_write("    <span id=\"result\">false</span>\n", 35, iostr);
  }
  raptor_iostream_counted_string_write("  </p>\n", 7, iostr);
  
  return 0;
}


/*
 * rasqal_query_results_write_html:
 * @iostr: #raptor_iostream to write the query results to
 * @results: #rasqal_query_results query results input
 * @base_uri: #raptor_uri base URI of the output format
 *
 * Write an HTML Table of the query results format to an iostream - INTERNAL.
 * 
 * If the writing succeeds, the query results will be exhausted.
 * 
 * Return value: non-0 on failure
 **/
static int
rasqal_query_results_write_html(rasqal_query_results_formatter* formatter,
                                raptor_iostream *iostr,
                                rasqal_query_results* results,
                                raptor_uri *base_uri)
{
  rasqal_query* query = rasqal_query_results_get_query(results);
  rasqal_query_results_type type;

  type = rasqal_query_results_get_type(results);

  if(type != RASQAL_QUERY_RESULTS_BINDINGS &&
     type != RASQAL_QUERY_RESULTS_BOOLEAN) {
    rasqal_log_error_simple(query->world, RAPTOR_LOG_LEVEL_ERROR,
                            &query->locator,
                            "Cannot write HTML Table for %s query result format",
                            rasqal_query_results_type_label(type));
    return 1;
  }

  /* XML and HTML declarations */
  raptor_iostream_counted_string_write(
    "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n", 39, iostr);
  raptor_iostream_counted_string_write(
    "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\"\n"
    "        \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">\n", 106, iostr);
  raptor_iostream_counted_string_write(
    "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n", 44, iostr);

  raptor_iostream_counted_string_write("<head>\n", 7, iostr);
  raptor_iostream_counted_string_write("  <title>SPARQL Query Results</title>\n", 38, iostr);
  raptor_iostream_counted_string_write("</head>\n", 8, iostr);
  raptor_iostream_counted_string_write("<body>\n", 7, iostr);

  if(rasqal_query_results_is_boolean(results))
    rasqal_query_results_write_html_boolean(iostr, results);
  else if(rasqal_query_results_is_bindings(results))
    rasqal_query_results_write_html_bindings(iostr, results);

  raptor_iostream_counted_string_write("</body>\n", 8, iostr);
  raptor_iostream_counted_string_write("</html>\n", 8, iostr);

  return 0;
}
  


static const char* const html_names[] = { "html", NULL};

static const char* const html_uri_strings[] = {
  "http://www.w3.org/1999/xhtml",
  NULL
};

static const raptor_type_q html_types[] = {
  { "application/xhtml+xml", 21, 10}, 
  { "text/html", 9, 10}, 
  { NULL, 0, 0}
};

static int
rasqal_query_results_html_register_factory(rasqal_query_results_format_factory *factory) 
{
  int rc = 0;

  factory->desc.names = html_names;
  factory->desc.mime_types = html_types;

  factory->desc.label = "HTML Table";
  factory->desc.uri_strings = html_uri_strings;

  factory->desc.flags = 0;
  
  factory->write         = rasqal_query_results_write_html;
  factory->get_rowsource = NULL;

  return rc;
}


int
rasqal_init_result_format_html(rasqal_world* world)
{
  return !rasqal_world_register_query_results_format_factory(world,
                                                             &rasqal_query_results_html_register_factory);
}

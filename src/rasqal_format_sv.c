/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_format_sv.c - Format results in CSV/TSV
 *
 * Intended to read and write the
 *   SPARQL 1.1 Query Results CSV and TSV Formats (DRAFT)
 *   http://www.w3.org/2009/sparql/docs/csv-tsv-results/results-csv-tsv.html
 * 
 * Copyright (C) 2009-2011, David Beckett http://www.dajobe.org/
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


static int
rasqal_iostream_write_csv_string(const unsigned char *string, size_t len,
                                 raptor_iostream *iostr)
{
  const char delim = '\x22';
  int quoting_needed = 0;
  size_t i;

  for(i = 0; i < len; i++) {
    char c = string[i];
    /* Quoting needed for delim (double quote), comma, linefeed or return */
    if(c == delim   || c == ',' || c == '\r' || c == '\n') {
      quoting_needed++;
      break;
    }
  }
  if(!quoting_needed)
    return raptor_iostream_counted_string_write(string, len, iostr);

  raptor_iostream_write_byte(delim, iostr);
  for(i = 0; i < len; i++) {
    char c = string[i];
    if(c == delim)
      raptor_iostream_write_byte(delim, iostr);
    raptor_iostream_write_byte(c, iostr);
  }
  raptor_iostream_write_byte(delim, iostr);

  return 0;
}

/*
 * rasqal_query_results_write_sv:
 * @iostr: #raptor_iostream to write the query to
 * @results: #rasqal_query_results query results format
 * @base_uri: #raptor_uri base URI of the output format
 * @label: name of this format for errors
 * @sep: column sep character
 * @csv_escape: non-0 if values are written escaped with CSV rules, else turtle
 * @variable_prefix: char to print before a variable name or NUL
 * @eol_str: end of line string
 * @eol_str_len: length of @eol_str
 *
 * INTERNAL - Write a @sep-separated values version of the query results format to an iostream.
 * 
 * If the writing succeeds, the query results will be exhausted.
 * 
 * Return value: non-0 on failure
 **/
static int
rasqal_query_results_write_sv(raptor_iostream *iostr,
                              rasqal_query_results* results,
                              raptor_uri *base_uri,
                              const char* label,
                              const char sep,
                              int csv_escape,
                              const char variable_prefix,
                              const char* eol_str,
                              size_t eol_str_len)
{
  rasqal_query* query = rasqal_query_results_get_query(results);
  int i;
#define empty_value_str_len 0
  static const char empty_value_str[empty_value_str_len+1] = "";
  int vars_count;
  
  if(!rasqal_query_results_is_bindings(results)) {
    rasqal_log_error_simple(query->world, RAPTOR_LOG_LEVEL_ERROR,
                            &query->locator,
                            "Can only write %s format for variable binding results",
                            label);
    return 1;
  }
  
  /* Header */
  for(i = 0; 1; i++) {
    const unsigned char *name;
    
    name = rasqal_query_results_get_binding_name(results, i);
    if(!name)
      break;

    if(i > 0)
      raptor_iostream_write_byte(sep, iostr);

    if(variable_prefix)
      raptor_iostream_write_byte(variable_prefix, iostr);
    raptor_iostream_string_write(name, iostr);
  }
  raptor_iostream_counted_string_write(eol_str, eol_str_len, iostr);


  /* Variable Binding Results */
  vars_count = rasqal_query_results_get_bindings_count(results);
  while(!rasqal_query_results_finished(results)) {
    /* Result row */
    for(i = 0; i < vars_count; i++) {
      rasqal_literal *l = rasqal_query_results_get_binding_value(results, i);

      if(i > 0)
        raptor_iostream_write_byte(sep, iostr);

      if(!l) {
        if(empty_value_str_len)
          raptor_iostream_counted_string_write(empty_value_str,
                                               empty_value_str_len, iostr);
      } else switch(l->type) {
        const unsigned char* str;
        size_t len;
        
        case RASQAL_LITERAL_URI:
          str = RASQAL_GOOD_CAST(const unsigned char*, raptor_uri_as_counted_string(l->value.uri, &len));
          if(csv_escape)
            rasqal_iostream_write_csv_string(str, len, iostr);
          else {
            raptor_iostream_write_byte('<', iostr);
            if(str && len > 0)
              raptor_string_ntriples_write(str, len, '"', iostr);
            raptor_iostream_write_byte('>', iostr);
          }
          break;

        case RASQAL_LITERAL_BLANK:
          raptor_bnodeid_ntriples_write(l->string, l->string_len, iostr);
          break;

        case RASQAL_LITERAL_STRING:
          if(csv_escape) {
            rasqal_iostream_write_csv_string(l->string, l->string_len, iostr);
          } else {
            if(l->datatype && l->valid) {
              rasqal_literal_type ltype;
              ltype = rasqal_xsd_datatype_uri_to_type(l->world, l->datatype);
              
              if(ltype >= RASQAL_LITERAL_INTEGER &&
                 ltype <= RASQAL_LITERAL_DECIMAL) {
                /* write integer, float, double and decimal XSD typed
                 * data without quotes, datatype or language 
                 */
                raptor_string_ntriples_write(l->string, l->string_len, '\0', iostr);
                break;
              }
            }

            raptor_iostream_write_byte('"', iostr);
            raptor_string_ntriples_write(l->string, l->string_len, '"', iostr);
            raptor_iostream_write_byte('"', iostr);

            if(l->language) {
              raptor_iostream_write_byte('@', iostr);
              raptor_iostream_string_write(RASQAL_GOOD_CAST(const unsigned char*, l->language), iostr);
            }
          
            if(l->datatype) {
              raptor_iostream_string_write("^^<", iostr);
              str = RASQAL_GOOD_CAST(const unsigned char*, raptor_uri_as_counted_string(l->datatype, &len));
              raptor_string_ntriples_write(str, len, '"', iostr);
              raptor_iostream_write_byte('>', iostr);
            }
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
          rasqal_log_error_simple(query->world, RAPTOR_LOG_LEVEL_ERROR,
                                  &query->locator,
                                  "Cannot turn literal type %d into %s", 
                                  l->type, label);
      }

      /* End Binding */
    }

    /* End Result Row */
    raptor_iostream_counted_string_write(eol_str, eol_str_len, iostr);
    
    rasqal_query_results_next(results);
  }

  /* end sparql */
  return 0;
}


static int
rasqal_query_results_write_csv(rasqal_query_results_formatter* formatter,
                               raptor_iostream *iostr,
                               rasqal_query_results* results,
                               raptor_uri *base_uri)
{
  return rasqal_query_results_write_sv(iostr, results, base_uri,
                                       "CSV", ',', 1, '\0',
                                       "\r\n", 2);
}


static int
rasqal_query_results_write_tsv(rasqal_query_results_formatter* formatter,
                               raptor_iostream *iostr,
                               rasqal_query_results* results,
                               raptor_uri *base_uri)
{
  return rasqal_query_results_write_sv(iostr, results, base_uri,
                                       "TSV", '\t', 0, '?',
                                       "\n", 1);
}


static const char* const csv_names[] = { "csv", NULL};

static const char* const csv_uri_strings[] = {
  "http://www.w3.org/ns/formats/SPARQL_Results_CSV",
  "http://www.w3.org/TR/sparql11-results-csv-tsv/",
  "http://www.ietf.org/rfc/rfc4180.txt",
  NULL
};

static const raptor_type_q csv_types[] = {
  { "text/csv", 8, 10}, 
  { "text/csv; header=present", 24, 10}, 
  { NULL, 0, 0}
};

static int
rasqal_query_results_csv_register_factory(rasqal_query_results_format_factory *factory) 
{
  int rc = 0;

  factory->desc.names = csv_names;
  factory->desc.mime_types = csv_types;

  factory->desc.label = "Comma Separated Values (CSV)";
  factory->desc.uri_strings = csv_uri_strings;

  factory->desc.flags = 0;
  
  factory->write         = rasqal_query_results_write_csv;
  factory->get_rowsource = NULL;

  return rc;
}


static const char* const tsv_names[] = { "tsv", NULL};

static const char* const tsv_uri_strings[] = {
  "http://www.w3.org/ns/formats/SPARQL_Results_TSV",
  "http://www.w3.org/TR/sparql11-results-csv-tsv/",
  "http://www.iana.org/assignments/media-types/text/tab-separated-values",
  NULL
};


static const raptor_type_q tsv_types[] = {
  { "text/tab-separated-values", 25, 10}, 
  { NULL, 0, 0}
};

static int
rasqal_query_results_tsv_register_factory(rasqal_query_results_format_factory *factory) 
{
  int rc = 0;

  factory->desc.names = tsv_names;
  factory->desc.mime_types = tsv_types;

  factory->desc.label = "Tab Separated Values (TSV)";
  factory->desc.uri_strings = tsv_uri_strings;

  factory->desc.flags = 0;
  
  factory->write         = rasqal_query_results_write_tsv;
  factory->get_rowsource = NULL;

  return rc;
}


int
rasqal_init_result_format_sv(rasqal_world* world)
{
  if(!rasqal_world_register_query_results_format_factory(world,
                                                         &rasqal_query_results_csv_register_factory))
    return 1;


  if(!rasqal_world_register_query_results_format_factory(world,
                                                         &rasqal_query_results_tsv_register_factory))
    return 1;
  
  return 0;
}

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_format_sv.c - Format results in CSV/TSV
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
 * rasqal_query_results_write_sv:
 * @iostr: #raptor_iostream to write the query to
 * @results: #rasqal_query_results query results format
 * @base_uri: #raptor_uri base URI of the output format
 * @label: name of this format for errors
 * @sep: column sep character
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
                              char sep)
{
  rasqal_query* query = rasqal_query_results_get_query(results);
  int i;
  int count = 1;
#define empty_value_str_len 0
  static const char empty_value_str[empty_value_str_len+1] = "";
#define nl_str_len 1
  static const char nl_str[nl_str_len+1] = "\n";
  int vars_count;
  
  if(!rasqal_query_results_is_bindings(results)) {
    rasqal_log_error_simple(query->world, RAPTOR_LOG_LEVEL_ERROR,
                            &query->locator,
                            "Can only write %s format for variable binding results",
                            label);
    return 1;
  }
  
  /* Header */
  raptor_iostream_write_counted_string(iostr, "Result", 6);
 
  for(i = 0; 1; i++) {
    const unsigned char *name;
    
    name = rasqal_query_results_get_binding_name(results, i);
    if(!name)
      break;
    
    raptor_iostream_write_byte(iostr, sep);
    raptor_iostream_write_string(iostr, name);
  }
  raptor_iostream_write_counted_string(iostr, nl_str, nl_str_len);


  /* Variable Binding Results */
  vars_count = rasqal_query_results_get_bindings_count(results);
  while(!rasqal_query_results_finished(results)) {
    /* Result row */
    raptor_iostream_write_decimal(iostr, count++);

    for(i = 0; i < vars_count; i++) {
      rasqal_literal *l = rasqal_query_results_get_binding_value(results, i);

      raptor_iostream_write_byte(iostr, sep);

      if(!l) {
        if(empty_value_str_len)
          raptor_iostream_write_counted_string(iostr, empty_value_str,
                                               empty_value_str_len);
      } else switch(l->type) {
        const unsigned char* str;
        size_t len;
        
        case RASQAL_LITERAL_URI:
          raptor_iostream_write_string(iostr, "uri(");
          str = (const unsigned char*)raptor_uri_as_counted_string(l->value.uri, &len);
#ifdef RAPTOR_V2_AVAILABLE
          raptor_string_ntriples_write(str, len, '"', iostr);
#else
          raptor_iostream_write_string_ntriples(iostr, str, len, '"');
#endif
          raptor_iostream_write_byte(iostr, ')');
          break;

        case RASQAL_LITERAL_BLANK:
          raptor_iostream_write_string(iostr, "blank(");
#ifdef RAPTOR_V2_AVAILABLE
          raptor_string_ntriples_write(l->string, l->string_len, '"', iostr);
#else
          raptor_iostream_write_string_ntriples(iostr,
                                                (const unsigned char*)l->string, 
                                                l->string_len, '"');
#endif
          raptor_iostream_write_byte(iostr, ')');
          break;

        case RASQAL_LITERAL_STRING:
          if(l->datatype && l->valid) {
            rasqal_literal_type ltype;
            ltype = rasqal_xsd_datatype_uri_to_type(l->world, l->datatype);
            
            if(ltype >= RASQAL_LITERAL_INTEGER &&
               ltype <= RASQAL_LITERAL_DECIMAL) {
              /* write integer, float, double and decimal XSD typed
               * data without quotes, datatype or language 
               */
#ifdef RAPTOR_V2_AVAILABLE
              raptor_string_ntriples_write(l->string, l->string_len, '\0', iostr);
#else
              raptor_iostream_write_string_ntriples(iostr,
                                                    (const unsigned char*)l->string,
                                                    l->string_len, '\0');
#endif
              break;
            }
          }
          
          raptor_iostream_write_byte(iostr, '"');
#ifdef RAPTOR_V2_AVAILABLE
          raptor_string_ntriples_write(l->string, l->string_len, '"', iostr);
#else
          raptor_iostream_write_string_ntriples(iostr,
                                                (const unsigned char*)l->string,
                                                l->string_len, '"');
#endif
          raptor_iostream_write_byte(iostr, '"');

          if(l->language) {
            raptor_iostream_write_byte(iostr, '@');
            raptor_iostream_write_string(iostr,
                                         (const unsigned char*)l->language);
          }
          
          if(l->datatype) {
            raptor_iostream_write_string(iostr, "^^uri(");
            str = (const unsigned char*)raptor_uri_as_counted_string(l->datatype, &len);
#ifdef RAPTOR_V2_AVAILABLE
            raptor_string_ntriples_write(str, len, '"', iostr);
#else
            raptor_iostream_write_string_ntriples(iostr, str, len, '"');
#endif
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
                                  "Cannot turn literal type %d into %s", 
                                  l->type, label);
      }

      /* End Binding */
    }

    /* End Result Row */
    raptor_iostream_write_counted_string(iostr, nl_str, nl_str_len);
    
    rasqal_query_results_next(results);
  }

  /* end sparql */
  return 0;
}


static int
rasqal_query_results_write_csv(raptor_iostream *iostr,
                              rasqal_query_results* results,
                              raptor_uri *base_uri)
{
  return rasqal_query_results_write_sv(iostr, results, base_uri, "CSV", ',');
}


static int
rasqal_query_results_write_tsv(raptor_iostream *iostr,
                               rasqal_query_results* results,
                               raptor_uri *base_uri)
{
  return rasqal_query_results_write_sv(iostr, results, base_uri, "TSV", '\t');
}


int
rasqal_init_result_format_sv(rasqal_world* world)
{
  int rc = 0;
  rasqal_query_results_formatter_func writer_fn=NULL;

  writer_fn = &rasqal_query_results_write_csv;
  rc += rasqal_query_results_format_register_factory(world,
                                                      "csv",
                                                      "Comma Separated Values (CSV)",
                                                      (const unsigned char*)"http://www.ietf.org/rfc/rfc4180.txt",
                                                      writer_fn,
                                                      NULL, NULL,
                                                      "text/csv") != 0;
  writer_fn = &rasqal_query_results_write_tsv;
  rc += rasqal_query_results_format_register_factory(world,
                                                     "tsv",
                                                     "Tab Separated Values (CSV)",
                                                     (const unsigned char*)"http://www.iana.org/assignments/media-types/text/tab-separated-values",
                                                     writer_fn,
                                                     NULL, NULL,
                                                     "text/tab-separated-values") != 0;

  return rc;
}

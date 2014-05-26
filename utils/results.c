/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * results.c - Rasqal command line results utility functions
 *
 * Copyright (C) 2013, David Beckett http://www.dajobe.org/
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
#include <stdarg.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif


#include <rasqal.h>

#include "rasqalcmdline.h"



rasqal_query_results*
rasqal_cmdline_read_results(rasqal_world* world,
                            raptor_world* raptor_world_ptr,
                            rasqal_query_results_type results_type,
                            raptor_iostream* result_iostr,
                            const char* result_filename,
                            const char* result_format_name)
{
  rasqal_query_results_formatter* qrf = NULL;
  unsigned char *query_results_base_uri_string = NULL;
  raptor_uri* query_results_base_uri = NULL;
  rasqal_query_results* results = NULL;
  int rc;

  query_results_base_uri_string = raptor_uri_filename_to_uri_string(result_filename);

  query_results_base_uri = raptor_new_uri(raptor_world_ptr,
                                          query_results_base_uri_string);
  raptor_free_memory(query_results_base_uri_string);

  results = rasqal_new_query_results2(world, NULL, results_type);
  if(!results)
    goto tidy_fail;

  if(result_format_name) {
    /* check name */
    if(!rasqal_query_results_formats_check2(world, result_format_name,
                               NULL /* uri */,
                               NULL /* mime type */,
                               RASQAL_QUERY_RESULTS_FORMAT_FLAG_READER))
      return NULL;
  } else {
    /* or use default */
    result_format_name = rasqal_world_guess_query_results_format_name(world,
                               NULL /* uri */,
                               NULL /* mime_type */,
                               NULL /* buffer */,
                               0,
                               (const unsigned char*)result_filename);
  }

  qrf = rasqal_new_query_results_formatter(world,
                                           result_format_name,
                                           NULL /* mime type */,
                                           NULL /* uri */);
  if(!qrf)
    goto tidy_fail;

  rc = rasqal_query_results_formatter_read(world, result_iostr,
                                           qrf, results,
                                           query_results_base_uri);
  rasqal_free_query_results_formatter(qrf); qrf = NULL;
  raptor_free_uri(query_results_base_uri); query_results_base_uri = NULL;
  if(rc)
    goto tidy_fail;

  return results;

  tidy_fail:
  if(results)
    rasqal_free_query_results(results);
  if(query_results_base_uri)
    raptor_free_uri(query_results_base_uri);

  return NULL;
}


void
rasqal_cmdline_print_bindings_results_simple(const char* program,
                                             rasqal_query_results *results,
                                             FILE* output, int quiet, int count)
{
  if(!quiet)
    fprintf(stderr, "%s: Query has a variable bindings result\n", program);

  while(!rasqal_query_results_finished(results)) {
    if(!count) {
      int i;

      fputs("row: [", output);
      for(i = 0; i < rasqal_query_results_get_bindings_count(results); i++) {
        const unsigned char *name;
        rasqal_literal *value;

        name = rasqal_query_results_get_binding_name(results, i);
        value = rasqal_query_results_get_binding_value(results, i);

        if(i > 0)
          fputs(", ", output);

        fprintf(output, "%s=", name);
        rasqal_literal_print(value, output);
      }
      fputs("]\n", output);
    }

    rasqal_query_results_next(results);
  }

  if(!quiet)
    fprintf(stderr, "%s: Query returned %d results\n", program,
            rasqal_query_results_get_count(results));
}

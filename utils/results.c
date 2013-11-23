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
  rasqal_variables_table* vars_table = NULL;
  const char* format_name = NULL;
  rasqal_query_results_formatter* qrf = NULL;
  unsigned char *query_results_base_uri_string = NULL;
  raptor_uri* query_results_base_uri = NULL;
  rasqal_query_results* results = NULL;

  query_results_base_uri_string = raptor_uri_filename_to_uri_string(result_filename);

  query_results_base_uri = raptor_new_uri(raptor_world_ptr,
                                          query_results_base_uri_string);

  vars_table = rasqal_new_variables_table(world);
  results = rasqal_new_query_results(world, NULL, results_type, vars_table);
  rasqal_free_variables_table(vars_table); vars_table = NULL;

  if(!results)
    goto tidy_fail;

  if(result_format_name) {
    /* FIXME validate result format name is legal query
     * results formatter name
     */
    format_name = result_format_name;
  }

  if(!format_name)
    format_name = rasqal_world_guess_query_results_format_name(world,
                                                               NULL /* uri */,
                                                               NULL /* mime_type */,
                                                               NULL /*buffer */,
                                                               0,
                                                               (const unsigned char*)result_filename);

  qrf = rasqal_new_query_results_formatter(world,
                                           format_name,
                                           NULL /* mime type */,
                                           NULL /* uri */);
  if(!qrf)
    goto tidy_fail;

  if(rasqal_query_results_formatter_read(world, result_iostr,
                                         qrf, results,
                                         query_results_base_uri))
    goto tidy_fail;

  rasqal_free_query_results_formatter(qrf); qrf = NULL;

  return results;

  tidy_fail:
  if(vars_table)
    rasqal_free_variables_table(vars_table);
  if(results)
    rasqal_free_query_results(results);
  if(query_results_base_uri)
    raptor_free_uri(query_results_base_uri);

  return NULL;
}

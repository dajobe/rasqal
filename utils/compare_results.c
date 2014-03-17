/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * compare_results.c - Rasqal compare query results utility functions
 *
 * Copyright (C) 2014, David Beckett http://www.dajobe.org/
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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <rasqal.h>
#include <rasqal_internal.h>

#include "rasqalcmdline.h"


struct compare_query_results_t
{
  rasqal_world* world;

  rasqal_query_results* qr1;
  const char* qr1_label;
  rasqal_query_results* qr2;
  const char* qr2_label;

  void* log_user_data;
  raptor_log_handler log_handler;
  raptor_log_message message;
};


/**
 * new_compare_query_results:
 * @world: rasqal world
 * @qr1: first query results
 * @qr1_label: labelf for @qr1
 * @qr2: first query results
 * @qr2_label: labelf for @qr2
 *
 * Constructor - create a new query results comparer
 *
 * Return value: new comparer or NULL on failure
 */
compare_query_results*
new_compare_query_results(rasqal_world* world,
                          rasqal_query_results* qr1, const char* qr1_label,
                          rasqal_query_results* qr2, const char* qr2_label)
{
  compare_query_results* cqr;

  cqr = RASQAL_CALLOC(compare_query_results*, 1, sizeof(*cqr));
  if(!cqr)
    return NULL;

  cqr->world = world;

  cqr->qr1 = qr1;
  cqr->qr1_label = qr1_label;
  cqr->qr2 = qr2;
  cqr->qr2_label = qr2_label;

  cqr->message.code = -1;
  cqr->message.domain = RAPTOR_DOMAIN_NONE;
  cqr->message.level = RAPTOR_LOG_LEVEL_NONE;
  cqr->message.locator = NULL;
  cqr->message.text = NULL;

  return cqr;
}


/**
 * free_compare_query_results:
 * @cqr: query results comparer
 *
 * Destructor - free a query results comparer
 */
void
free_compare_query_results(compare_query_results* cqr)
{
  if(!cqr)
    return;

  RASQAL_FREE(compare_query_results, cqr);
}


/**
 * compare_query_results_set_log_handler:
 * @cqr: query results comparer
 * @log_user_data: log handler user data
 * @log_handler: log handler
 *
 * Set query results comparer log handler
 *
 */
void
compare_query_results_set_log_handler(compare_query_results* cqr,
                                      void* log_user_data,
                                      raptor_log_handler log_handler)
{
  cqr->log_user_data = log_user_data;
  cqr->log_handler = log_handler;
}


/**
 * compare_query_results_compare:
 * @cqr: query results object
 *
 * Run a query results comparison
 *
 * Return value: non-0 if equal
 */
int
compare_query_results_compare(compare_query_results* cqr)
{
  int differences = 0;
  int i;
  int rowi;
  int size1;
  int size2;
  int row_differences_count = 0;

  size1 = rasqal_query_results_get_bindings_count(cqr->qr1);
  size2 = rasqal_query_results_get_bindings_count(cqr->qr2);

  if(size1 != size2) {
    cqr->message.level = RAPTOR_LOG_LEVEL_ERROR;
    cqr->message.text = "Results have different numbers of bindings";
    if(cqr->log_handler)
      cqr->log_handler(cqr->log_user_data, &cqr->message);

    differences++;
    goto done;
  }


  /* check variables in each results project the same variables */
  for(i = 0; 1; i++) {
    const unsigned char* v1;
    const unsigned char* v2;

    v1 = rasqal_query_results_get_binding_name(cqr->qr1, i);
    v2 = rasqal_query_results_get_binding_name(cqr->qr2, i);
    if(!v1 && !v2)
      break;

    if(v1 && v2) {
      if(strcmp((const char*)v1, (const char*)v2)) {
        /* different names */
        differences++;
      }
    } else
      /* one is NULL, the other is a name */
      differences++;
  }

  if(differences) {
    cqr->message.level = RAPTOR_LOG_LEVEL_ERROR;
    cqr->message.text = "Results have different binding names";
    if(cqr->log_handler)
      cqr->log_handler(cqr->log_user_data, &cqr->message);

    goto done;
  }


  /* set results to be stored? */

  /* sort rows by something ?  As long as the sort is the same it
   * probably does not matter what the method is. */

  /* what to do about blank nodes? */

  /* for each row */
  for(rowi = 0; 1; rowi++) {
    int bindingi;
    rasqal_row* row1 = rasqal_query_results_get_row_by_offset(cqr->qr1, rowi);
    rasqal_row* row2 = rasqal_query_results_get_row_by_offset(cqr->qr2, rowi);
    int this_row_different = 0;

    if(!row1 && !row2)
      break;

    /* for each variable in row1 (== same variables in row2) */
    for(bindingi = 0; bindingi < size1; bindingi++) {
      /* we know the binding names are the same */
      const unsigned char* name;
      rasqal_literal *value1;
      rasqal_literal *value2;
      int error = 0;

      name = rasqal_query_results_get_binding_name(cqr->qr1, bindingi);

      value1 = rasqal_query_results_get_binding_value(cqr->qr1, bindingi);
      value2 = rasqal_query_results_get_binding_value(cqr->qr2, bindingi);

      /* should have compare as native flag?
       * RASQAL_COMPARE_XQUERY doesn't compare all values
       */
      if(!rasqal_literal_equals_flags(value1, value2, RASQAL_COMPARE_XQUERY,
                                      &error)) {
        /* if different report it */
        raptor_world* raptor_world_ptr;
        void *string;
        size_t length;
        raptor_iostream* string_iostr;

        raptor_world_ptr = rasqal_world_get_raptor(cqr->world);

        string_iostr = raptor_new_iostream_to_string(raptor_world_ptr,
                                                     &string, &length,
                                                     (raptor_data_malloc_handler)malloc);

        raptor_iostream_counted_string_write("Difference in row ", 18,
                                             string_iostr);
        raptor_iostream_decimal_write(rowi + 1,
                                      string_iostr);
        raptor_iostream_counted_string_write(" binding '", 10,
                                             string_iostr);
        raptor_iostream_string_write(name,
                                     string_iostr);
        raptor_iostream_counted_string_write("' ", 2,
                                             string_iostr);
        raptor_iostream_string_write(cqr->qr1_label, string_iostr);
        raptor_iostream_counted_string_write(" value ", 7,
                                             string_iostr);
        rasqal_literal_write(value1,
                             string_iostr);
        raptor_iostream_write_byte(' ',
                                   string_iostr);
        raptor_iostream_string_write(cqr->qr2_label,
                                     string_iostr);
        raptor_iostream_counted_string_write(" value ", 7,
                                             string_iostr);
        rasqal_literal_write(value2,
                             string_iostr);
        raptor_iostream_write_byte(' ',
                                   string_iostr);

        /* this allocates and copies result into 'string' */
        raptor_free_iostream(string_iostr);

        cqr->message.level = RAPTOR_LOG_LEVEL_ERROR;
        cqr->message.text = (const char*)string;
        if(cqr->log_handler)
          cqr->log_handler(cqr->log_user_data, &cqr->message);

        free(string);

        differences++;
        this_row_different = 1;
      }
    } /* end for each var */

    if(row1)
      rasqal_free_row(row1);
    if(row2)
      rasqal_free_row(row2);

    if(this_row_different)
      row_differences_count++;

    rasqal_query_results_next(cqr->qr1);
    rasqal_query_results_next(cqr->qr2);
  } /* end for each row */

  if(row_differences_count) {
    cqr->message.level = RAPTOR_LOG_LEVEL_ERROR;
    cqr->message.text = "Results have different values";
    if(cqr->log_handler)
      cqr->log_handler(cqr->log_user_data, &cqr->message);
  }

  done:
  return (differences == 0);
}

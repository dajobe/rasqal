/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_format_table.c - Format Results in a Table
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


#include <raptor.h>

/* Rasqal includes */
#include <rasqal.h>
#include <rasqal_internal.h>


static void
rasqal_free_chararray(void* object)
{
  char** values = (char**)object;
  int i;
  for(i = 0; values[i] != (char*)-1; i++) {
    if(values[i])
      free(values[i]);
  }
  free(values);
}

#ifdef RAPTOR_V2_AVAILABLE
static void
rasqal_free_chararray_v2(void* context, void* object)
{
  rasqal_free_chararray(object);
}
#endif


static int
rasqal_iostream_write_counted_string_padded(raptor_iostream *iostr,
                                            const void *string, size_t len,
                                            const char pad, size_t width)
{
  int w = width-len;

  if(len)
    raptor_iostream_write_counted_string(iostr, string, len);
  
  if(w > 0) {
    int i;
    for(i = 0; i < w; i++)
      raptor_iostream_write_byte(iostr, pad);
  }

  return 0;
}
    

static int
rasqal_query_results_write_table(raptor_iostream *iostr,
                                 rasqal_query_results* results,
                                 raptor_uri *base_uri)
{
  rasqal_query* query = rasqal_query_results_get_query(results);
  raptor_sequence *seq = NULL;
  int *widths = NULL;
  int bindings_count = -1;
  int rows_count = 0;
  int rc = 0;
  int total_width = -1;
  int i;
  size_t sep_len;
  char *sep = NULL;
  
  if(!rasqal_query_results_is_bindings(results)) {
    rasqal_log_error_simple(query->world, RAPTOR_LOG_LEVEL_ERROR,
                            &query->locator,
                            "Can only write table format for variable binding results");
    return 1;
  }

  bindings_count = rasqal_query_results_get_bindings_count(results);
  widths = (int*)RASQAL_CALLOC(intarray, sizeof(int), bindings_count + 1);
  if(!widths) {
    rc = 1;
    goto tidy;
  }
  
  widths[bindings_count] = -1;
  
  for(i = 0; i < bindings_count; i++) {
    const unsigned char *name;
    int w;
    
    name = rasqal_query_results_get_binding_name(results, i);
    if(!name)
      break;

    w = strlen((const char*)name);
    if(w > widths[i])
      widths[i] = w;
  }

#ifdef RAPTOR_V2_AVAILABLE
  seq = raptor_new_sequence_v2((raptor_sequence_free_handler_v2*)rasqal_free_chararray_v2, NULL, NULL);
#else
  seq = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_chararray, NULL);
#endif
  if(!seq) {
    rc = 1;
    goto tidy;
  }

  while(!rasqal_query_results_finished(results)) {
    char **values;
    values = (char**)RASQAL_CALLOC(stringarray, sizeof(char*), bindings_count + 1);
    if(!values) {
      rc = 1;
      goto tidy;
    }

    for(i = 0; i < bindings_count; i++) {
      rasqal_literal *l = rasqal_query_results_get_binding_value(results, i);
      raptor_iostream* str_iostr;
      size_t v_len;
      
      if(!l)
        continue;

      str_iostr = raptor_new_iostream_to_string((void**)&values[i], &v_len,
                                                rasqal_alloc_memory);
      if(!str_iostr) {
        rc = 1;
        goto tidy;
      }
      rasqal_literal_write(l, str_iostr);
      raptor_free_iostream(str_iostr);
      
      if((int)v_len > widths[i])
        widths[i] = v_len;
    }
    values[i] = (char*)-1;

    raptor_sequence_push(seq, values);

    rasqal_query_results_next(results);
  }

  rows_count = raptor_sequence_size(seq);
  
  total_width = 0;
  for(i = 0; i < bindings_count; i++)
    total_width += widths[i];

#define VSEP "|"
#define VSEP_LEN 1
#define PAD " "
#define PAD_LEN 1
  sep_len = total_width + ((PAD_LEN+PAD_LEN) * bindings_count) + VSEP_LEN * (bindings_count+1);
  sep = (char*)RASQAL_MALLOC(cstring, sep_len + 1);
  if(!sep) {
    rc = 1;
    goto tidy;
  }
  for(i = 0 ; i < (int)sep_len; i++)
    sep[i]='-';
  sep[sep_len]='\0';

  if(1) {
    char * p = sep;
    strncpy(p, VSEP, VSEP_LEN);
    p += VSEP_LEN;
    for(i = 0; i < bindings_count; i++) {
      p += PAD_LEN + widths[i] + PAD_LEN;
      strncpy(p, VSEP, VSEP_LEN);
      p += VSEP_LEN;
    }
  }
  

  /* Generate separator */
#if 1
  rasqal_iostream_write_counted_string_padded(iostr, NULL, 0, '-', sep_len);
#else
  raptor_iostream_write_counted_string(iostr, sep, sep_len);
#endif
  raptor_iostream_write_byte(iostr, '\n');

  /* Generate variables header */
  raptor_iostream_write_counted_string(iostr, VSEP, VSEP_LEN);
  for(i = 0; i < bindings_count; i++) {
    const unsigned char *name;
    int w;
    name = rasqal_query_results_get_binding_name(results, i);
    if(!name)
      break;
    w = strlen((const char*)name);
    
    raptor_iostream_write_counted_string(iostr, PAD, PAD_LEN);
    rasqal_iostream_write_counted_string_padded(iostr, name, w,
                                                ' ', widths[i]);
    raptor_iostream_write_counted_string(iostr, PAD, PAD_LEN);
    raptor_iostream_write_counted_string(iostr, VSEP, VSEP_LEN);
  }
  raptor_iostream_write_byte(iostr, '\n');

  /* Generate separator */
#if 1
  rasqal_iostream_write_counted_string_padded(iostr, NULL, 0, '=', sep_len);
  raptor_iostream_write_byte(iostr, '\n');
#else
  raptor_iostream_write_counted_string(iostr, sep, sep_len);
#endif

  /* Write values */
  if(rows_count) {
    int rowi;
    for(rowi = 0; rowi < rows_count; rowi++) {
      char **values = raptor_sequence_get_at(seq, rowi);
      
      raptor_iostream_write_counted_string(iostr, VSEP, VSEP_LEN);
      for(i = 0; i < bindings_count; i++) {
        char *value = values[i];
        int w = strlen((const char*)value);
    
        raptor_iostream_write_counted_string(iostr, PAD, PAD_LEN);
        rasqal_iostream_write_counted_string_padded(iostr, value, w,
                                                    ' ', widths[i]);
        raptor_iostream_write_counted_string(iostr, PAD, PAD_LEN);
        raptor_iostream_write_counted_string(iostr, VSEP, VSEP_LEN);
      }
      raptor_iostream_write_byte(iostr, '\n');
    }

    /* Generate end separator */
#if 1
    rasqal_iostream_write_counted_string_padded(iostr, NULL, 0, '-', sep_len);
#else
    raptor_iostream_write_counted_string(iostr, sep, sep_len);
#endif
    raptor_iostream_write_byte(iostr, '\n');
  }
  

  tidy:
  if(sep)
    RASQAL_FREE(string, sep);
  if(widths)
    RASQAL_FREE(intarray, widths);
  if(seq)
    raptor_free_sequence(seq);
  
  return 0;
}


int
rasqal_init_result_format_table(rasqal_world* world)
{
  rasqal_query_results_formatter_func writer_fn=NULL;
  writer_fn = &rasqal_query_results_write_table;

  return rasqal_query_results_format_register_factory(world,
                                                      "table", "Table",
                                                      NULL,
                                                      writer_fn, NULL, NULL,
                                                      "text/plan");
}

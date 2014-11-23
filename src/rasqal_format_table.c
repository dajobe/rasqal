/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_format_table.c - Format Results in a Table
 *
 * Copyright (C) 2009-2010, David Beckett http://www.dajobe.org/
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
  char** values;
  int i;

  if(!object)
    return;
  
  values = RASQAL_GOOD_CAST(char**, object);
  for(i = 0; values[i] != RASQAL_GOOD_CAST(char*, -1); i++) {
    if(values[i])
      free(values[i]);
  }
  free(values);
}


static int
rasqal_iostream_write_counted_string_padded(raptor_iostream *iostr,
                                            const void *string, size_t len,
                                            const char pad, size_t width)
{
  size_t w = width - len;

  if(len)
    raptor_iostream_counted_string_write(string, len, iostr);
  
  if(w > 0) {
    unsigned int i;
    for(i = 0; i < w; i++)
      raptor_iostream_write_byte(pad, iostr);
  }

  return 0;
}
    

static int
rasqal_query_results_write_table_bindings(raptor_iostream *iostr,
                                          rasqal_query_results* results,
                                          raptor_uri *base_uri)
{
  rasqal_world* world = rasqal_query_results_get_world(results);
  raptor_sequence *seq = NULL;
  size_t *widths = NULL;
  int bindings_count = -1;
  int rows_count = 0;
  int rc = 0;
  size_t total_width = 0;
  int i;
  size_t si;
  size_t sep_len;
  char *sep = NULL;

  bindings_count = rasqal_query_results_get_bindings_count(results);
  widths = RASQAL_CALLOC(size_t*, RASQAL_GOOD_CAST(size_t, bindings_count + 1), sizeof(size_t));
  if(!widths) {
    rc = 1;
    goto tidy;
  }
  
  widths[bindings_count] = 0;
  
  for(i = 0; i < bindings_count; i++) {
    const unsigned char *name;
    size_t w;
    
    name = rasqal_query_results_get_binding_name(results, i);
    if(!name)
      break;

    w = strlen(RASQAL_GOOD_CAST(const char*, name));
    if(w > widths[i])
      widths[i] = w;
  }

  seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_chararray, NULL);
  if(!seq) {
    rc = 1;
    goto tidy;
  }

  while(!rasqal_query_results_finished(results)) {
    char **values;
    values = RASQAL_CALLOC(char**, RASQAL_GOOD_CAST(size_t, bindings_count + 1), sizeof(char*));
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

      str_iostr = raptor_new_iostream_to_string(world->raptor_world_ptr,
                                                (void**)&values[i], &v_len,
                                                rasqal_alloc_memory);
      if(!str_iostr) {
        rc = 1;
        goto tidy;
      }
      rasqal_literal_write(l, str_iostr);
      raptor_free_iostream(str_iostr);
      
      if(v_len > widths[i])
        widths[i] = v_len;
    }
    values[i] = RASQAL_GOOD_CAST(char*, -1);

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
  sep_len = total_width + RASQAL_GOOD_CAST(size_t, ((PAD_LEN+PAD_LEN) * bindings_count) + VSEP_LEN * (bindings_count + 1));
  sep = RASQAL_MALLOC(char*, sep_len + 1);
  if(!sep) {
    rc = 1;
    goto tidy;
  }
  for(si = 0 ; si < sep_len; si++)
    sep[si]='-';
  sep[sep_len]='\0';

  if(1) {
    char * p = sep;
    memcpy(p, VSEP, VSEP_LEN);
    p += VSEP_LEN;
    for(i = 0; i < bindings_count; i++) {
      p += PAD_LEN + widths[i] + PAD_LEN;
      memcpy(p, VSEP, VSEP_LEN);
      p += VSEP_LEN;
    }
  }
  

  /* Generate separator */
  rasqal_iostream_write_counted_string_padded(iostr, NULL, 0, '-', sep_len);
  raptor_iostream_write_byte('\n', iostr);

  /* Generate variables header */
  raptor_iostream_counted_string_write(VSEP, VSEP_LEN, iostr);
  for(i = 0; i < bindings_count; i++) {
    const unsigned char *name;
    size_t w;
    name = rasqal_query_results_get_binding_name(results, i);
    if(!name)
      break;
    w = strlen(RASQAL_GOOD_CAST(const char*, name));
    
    raptor_iostream_counted_string_write(PAD, PAD_LEN, iostr);
    rasqal_iostream_write_counted_string_padded(iostr, name, w,
                                                ' ', widths[i]);
    raptor_iostream_counted_string_write(PAD, PAD_LEN, iostr);
    raptor_iostream_counted_string_write(VSEP, VSEP_LEN, iostr);
  }
  raptor_iostream_write_byte('\n', iostr);

  /* Generate separator */
  rasqal_iostream_write_counted_string_padded(iostr, NULL, 0, '=', sep_len);
  raptor_iostream_write_byte('\n', iostr);

  /* Write values */
  if(rows_count) {
    int rowi;
    for(rowi = 0; rowi < rows_count; rowi++) {
      char **values = (char**)raptor_sequence_get_at(seq, rowi);
      
      raptor_iostream_counted_string_write(VSEP, VSEP_LEN, iostr);
      for(i = 0; i < bindings_count; i++) {
        char *value = values[i];
        size_t w = value ? strlen(RASQAL_GOOD_CAST(const char*, value)) : 0;
    
        raptor_iostream_counted_string_write(PAD, PAD_LEN, iostr);
        rasqal_iostream_write_counted_string_padded(iostr, value, w,
                                                    ' ', widths[i]);
        raptor_iostream_counted_string_write(PAD, PAD_LEN, iostr);
        raptor_iostream_counted_string_write(VSEP, VSEP_LEN, iostr);
      }
      raptor_iostream_write_byte('\n', iostr);
    }

    /* Generate end separator */
    rasqal_iostream_write_counted_string_padded(iostr, NULL, 0, '-', sep_len);
    raptor_iostream_write_byte('\n', iostr);
  }
  

  tidy:
  if(sep)
    RASQAL_FREE(string, sep);
  if(widths)
    RASQAL_FREE(intarray, widths);
  if(seq)
    raptor_free_sequence(seq);
  
  return rc;
}

static int
rasqal_query_results_write_table_boolean(raptor_iostream *iostr,
                                         rasqal_query_results* results,
                                         raptor_uri *base_uri)
{
  if (rasqal_query_results_get_boolean(results)) {
    raptor_iostream_counted_string_write("--------\n", 9, iostr);
    raptor_iostream_counted_string_write("| true |\n", 9, iostr);
    raptor_iostream_counted_string_write("--------\n", 9, iostr);
  } else {
    raptor_iostream_counted_string_write("---------\n", 10, iostr);
    raptor_iostream_counted_string_write("| false |\n", 10, iostr);
    raptor_iostream_counted_string_write("---------\n", 10, iostr);
  }

  return 0;
}

static int
rasqal_query_results_write_table(rasqal_query_results_formatter* formatter,
                                 raptor_iostream *iostr,
                                 rasqal_query_results* results,
                                 raptor_uri *base_uri)
{
  rasqal_query* query = rasqal_query_results_get_query(results);
  rasqal_query_results_type type;

  type = rasqal_query_results_get_type(results);


  if(type == RASQAL_QUERY_RESULTS_BINDINGS) {
    return rasqal_query_results_write_table_bindings(iostr, results, base_uri);
  } else if(type == RASQAL_QUERY_RESULTS_BOOLEAN) {
    return rasqal_query_results_write_table_boolean(iostr, results, base_uri);
  } else {
    rasqal_log_error_simple(query->world, RAPTOR_LOG_LEVEL_ERROR,
                            &query->locator,
                            "Cannot write table format for %s query result format",
                            rasqal_query_results_type_label(type));
    return 1;
  }
}


static const char* const table_names[] = { "table", NULL};

static const raptor_type_q table_types[] = {
  { "text/plain", 10, 10}, 
  { NULL, 0, 0}
};

static int
rasqal_query_results_table_register_factory(rasqal_query_results_format_factory *factory) 
{
  int rc = 0;

  factory->desc.names = table_names;
  factory->desc.mime_types = table_types;

  factory->desc.label = "Table";
  factory->desc.uri_strings = NULL;

  factory->desc.flags = 0;
  
  factory->write         = rasqal_query_results_write_table;
  factory->get_rowsource = NULL;

  return rc;
}


int
rasqal_init_result_format_table(rasqal_world* world)
{
  return !rasqal_world_register_query_results_format_factory(world,
                                                             &rasqal_query_results_table_register_factory);
}

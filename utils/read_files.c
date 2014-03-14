/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * common.c - Rasqal command line utility functions
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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif


#include <rasqal.h>

#include <rasqal_internal.h>


#ifdef BUFSIZ
#define FILE_READ_BUF_SIZE BUFSIZ
#else
#define FILE_READ_BUF_SIZE 1024
#endif

#include "rasqalcmdline.h"


unsigned char*
rasqal_cmdline_read_file_fh(rasqal_world *world,
                            FILE* fh,
                            const char* filename, 
                            const char* label,
                            size_t* len_p)
{
  raptor_stringbuffer *sb;
  size_t len;
  unsigned char* string = NULL;
  unsigned char* buffer = NULL;
  raptor_locator my_locator;

  memset(&my_locator, '\0', sizeof(my_locator));
  my_locator.file = filename;

  sb = raptor_new_stringbuffer();
  if(!sb)
    return NULL;

  buffer = (unsigned char*)rasqal_alloc_memory(FILE_READ_BUF_SIZE);
  if(!buffer)
    goto tidy;

  while(!feof(fh)) {
    size_t read_len;
    
    read_len = fread((char*)buffer, 1, FILE_READ_BUF_SIZE, fh);
    if(read_len > 0)
      raptor_stringbuffer_append_counted_string(sb, buffer, read_len, 1);
    
    if(read_len < FILE_READ_BUF_SIZE) {
      if(ferror(fh)) {
        rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR,
                                &my_locator,
                                "Read failed - %s\n",
                                strerror(errno));
        goto tidy;
      }
      
      break;
    }
  }
  len = raptor_stringbuffer_length(sb);
  
  string = (unsigned char*)rasqal_alloc_memory(len + 1);
  if(string) {
    raptor_stringbuffer_copy_to_string(sb, string, len);
    if(len_p)
      *len_p = len;
  }
  
  tidy:
  if(buffer)
    rasqal_free_memory(buffer);

  if(sb)
    raptor_free_stringbuffer(sb);

  return string;
}


unsigned char*
rasqal_cmdline_read_file_string(rasqal_world* world,
                                const char* filename, 
                                const char* label,
                                size_t* len_p)
{
  FILE *fh = NULL;
  unsigned char* string = NULL;
  raptor_locator my_locator;

  memset(&my_locator, '\0', sizeof(my_locator));
  my_locator.file = filename;
  
  fh = fopen(filename, "r");
  if(!fh) {
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR,
                            &my_locator,
                            "%s '%s' open failed - %s\n",
                            label, filename, strerror(errno));
    goto tidy;
  }

  string = rasqal_cmdline_read_file_fh(world, fh, filename, label, len_p);
  
  fclose(fh);
  
  tidy:

  return string;
}


/*
 * rasqal_cmdline_read_uri_file_stdin_contents:
 * @world: rasqal world
 * @uri: uri to use (or NULL)
 * @filename: filename to use (or NULL)
 * @len_p: address to store length of result (or NULL)
 *
 * INTERNAL - Read a query string from a @uri or @filename or otherwise from stdin
 *
 * Return value: query string at the source or NULL on failure
 */
unsigned char*
rasqal_cmdline_read_uri_file_stdin_contents(rasqal_world* world,
                                            raptor_uri* uri,
                                            const char* filename,
                                            size_t* len_p)
{
  raptor_world* raptor_world_ptr = rasqal_world_get_raptor(world);
  unsigned char* string = NULL;
  
  if(uri) {
    raptor_www *www;
    
    www = raptor_new_www(raptor_world_ptr);
    if(www) {
      raptor_www_fetch_to_string(www, uri, (void**)&string, len_p,
                                 rasqal_alloc_memory);
      raptor_free_www(www);
    }
  } else if(filename) {
    string = rasqal_cmdline_read_file_string(world, filename,
                                             "query file", len_p);
  } else {
    /* stdin */
    string = rasqal_cmdline_read_file_fh(world, stdin, "stdin",
                                         "query string stdin", len_p);
  }
  
  return string;
}


/*
 * rasqal_cmdline_read_data_graph:
 * @world: world
 * @type: data graph type
 * @name: graph name "-" (stdin) or filename or UI
 * @format_name: format name (or NULL)
 *
 * INTERNAL - Construct a data graph object from arguments
 *
 * Data graph or NULL on failure
 */
rasqal_data_graph*
rasqal_cmdline_read_data_graph(rasqal_world* world,
                               rasqal_data_graph_flags type,
                               const char* name,
                               const char* format_name)
{
  raptor_world* raptor_world_ptr = rasqal_world_get_raptor(world);
  rasqal_data_graph* dg = NULL;

  if(!strcmp(name, "-")) {
    /* stdin: use an iostream not a URI data graph */
    unsigned char* source_uri_string;
    raptor_uri* iostr_base_uri = NULL;
    raptor_uri* graph_name = NULL;

    /* FIXME - get base URI from somewhere else */
    source_uri_string = (unsigned char*)"file:///dev/stdin";

    iostr_base_uri = raptor_new_uri(raptor_world_ptr, source_uri_string);
    if(iostr_base_uri) {
      raptor_iostream* iostr;
      iostr = raptor_new_iostream_from_file_handle(raptor_world_ptr,
                                                   stdin);
      if(iostr)
        dg = rasqal_new_data_graph_from_iostream(world,
                                                 iostr, iostr_base_uri,
                                                 graph_name,
                                                 type,
                                                 NULL,
                                                 format_name,
                                                 NULL);
      raptor_free_uri(iostr_base_uri);
    }
  } else if(!access(name, R_OK)) {
    /* file: use URI */
    unsigned char* source_uri_string;
    raptor_uri* source_uri;
    raptor_uri* graph_name = NULL;

    source_uri_string = raptor_uri_filename_to_uri_string(name);
    source_uri = raptor_new_uri(raptor_world_ptr, source_uri_string);
    raptor_free_memory(source_uri_string);

    if(type == RASQAL_DATA_GRAPH_NAMED)
      graph_name = source_uri;

    if(source_uri)
      dg = rasqal_new_data_graph_from_uri(world,
                                          source_uri,
                                          graph_name,
                                          type,
                                          NULL, format_name,
                                          NULL);

    if(source_uri)
      raptor_free_uri(source_uri);
  } else {
    raptor_uri* source_uri;
    raptor_uri* graph_name = NULL;

    /* URI: use URI */
    source_uri = raptor_new_uri(raptor_world_ptr,
                                (const unsigned char*)name);
    if(type == RASQAL_DATA_GRAPH_NAMED)
      graph_name = source_uri;

    if(source_uri)
      dg = rasqal_new_data_graph_from_uri(world,
                                          source_uri,
                                          graph_name,
                                          type,
                                          NULL, format_name,
                                          NULL);

    if(source_uri)
      raptor_free_uri(source_uri);
  }

  return dg;
}

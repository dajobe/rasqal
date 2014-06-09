/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_service.c - Rasqal SPARQL Protocol Service
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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdarg.h>

#include "rasqal.h"
#include "rasqal_internal.h"


#define DEFAULT_FORMAT "application/sparql-results+xml"


struct rasqal_service_s
{
  rasqal_world* world;

  /* request fields */
  raptor_uri* service_uri;
  char* query_string;
  size_t query_string_len;
  raptor_sequence* data_graphs; /* background graph and named graphs */
  char* format; /* MIME Type to use as request HTTP Accept: */

  /* URL retrieval fields */
  raptor_www* www;
  int started;

  /* response fields */
  raptor_uri* final_uri;
  raptor_stringbuffer* sb;
  char* content_type;

  int usage;
};



/**
 * rasqal_new_service:
 * @world: rasqal_world object
 * @service_uri: sparql protocol service URI
 * @query_string: query string (or NULL)
 * @data_graphs: sequence of #rasqal_data_graph graphs for service
 *
 * Constructor - create a new rasqal protocol service object.
 *
 * Create a structure to execute a sparql protocol service at
 * @service_uri running the query @query_string and returning
 * a sparql result set.
 *
 * All arguments are copied by the service object
 *
 * Return value: a new #rasqal_query object or NULL on failure
 */
rasqal_service*
rasqal_new_service(rasqal_world* world, raptor_uri* service_uri,
                   const unsigned char* query_string,
                   raptor_sequence* data_graphs)
{
  rasqal_service* svc;
  size_t len = 0;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(service_uri, raptor_uri, NULL);
  
  svc = RASQAL_CALLOC(rasqal_service*, 1, sizeof(*svc));
  if(!svc)
    return NULL;

  svc->usage = 1;
  svc->world = world;
  svc->service_uri = raptor_uri_copy(service_uri);

  if(query_string) {
    len = strlen(RASQAL_GOOD_CAST(const char*, query_string));
    svc->query_string = RASQAL_MALLOC(char*, len + 1);
    if(!svc->query_string) {
      rasqal_free_service(svc);
      return NULL;
    }
  
    memcpy(svc->query_string, query_string, len + 1);
  }
  svc->query_string_len = len;

  if(data_graphs) {
    int i;
    rasqal_data_graph* dg;
    
    svc->data_graphs = raptor_new_sequence((raptor_data_free_handler)rasqal_free_data_graph,
                                           NULL);

    if(!svc->data_graphs) {
      rasqal_free_service(svc);
      return NULL;
    }

    for(i = 0;
        (dg = (rasqal_data_graph*)raptor_sequence_get_at(data_graphs, i));
        i++) {
      raptor_sequence_push(svc->data_graphs,
                           rasqal_new_data_graph_from_data_graph(dg));
    }
  }
  
  return svc;
}


/*
 * rasqal_new_service_from_service:
 * @service: #rasqal_service to copy
 *
 * INTERNAL - Copy Constructor - Create a new Rasqal service from an existing one
 *
 * This adds a new reference to the service, it does not do a deep copy
 *
 * Return value: a new #rasqal_service or NULL on failure.
 **/
rasqal_service*
rasqal_new_service_from_service(rasqal_service* svc)
{
  if(!svc)
    return NULL;
  
  svc->usage++;

  return svc;
}


/**
 * rasqal_free_service:
 * @svc: #rasqal_service object
 * 
 * Destructor - destroy a #rasqal_service object.
 **/
void
rasqal_free_service(rasqal_service* svc)
{
  if(!svc)
    return;
  
  if(--svc->usage)
    return;
  
  if(svc->service_uri)
    raptor_free_uri(svc->service_uri);

  if(svc->query_string)
    RASQAL_FREE(char*, svc->query_string);

  if(svc->data_graphs)
    raptor_free_sequence(svc->data_graphs);
  
  rasqal_service_set_www(svc, NULL);

  RASQAL_FREE(rasqal_service, svc);
}


/**
 * rasqal_service_set_www:
 * @svc: #rasqal_service service object
 * @www: WWW object (or NULL)
 *
 * Set the WWW object to use when executing the service
 * 
 * Return value: non 0 on failure
 **/
int
rasqal_service_set_www(rasqal_service* svc, raptor_www* www) 
{
  if(svc->www)
    raptor_free_www(svc->www);

  svc->www = www;

  return 0;
}


/**
 * rasqal_service_set_format:
 * @svc: #rasqal_service service object
 * @format: service mime type (or NULL)
 *
 * Set the MIME Type to use in HTTP Accept when executing the service
 * 
 * Return value: non 0 on failure
 **/
int
rasqal_service_set_format(rasqal_service* svc, const char *format)
{
  size_t len;
  
  if(svc->format) {
    RASQAL_FREE(char*, svc->format);
    svc->format = NULL;
  }

  if(!format)
    return 0;
  
  len = strlen(format);
  svc->format = RASQAL_MALLOC(char*, len + 1);
  if(!svc->format)
    return 1;

  memcpy(svc->format, format, len + 1);

  return 0;
}


static void
rasqal_service_write_bytes(raptor_www* www,
                           void *userdata, const void *ptr, 
                           size_t size, size_t nmemb)
{
  rasqal_service* svc = (rasqal_service*)userdata;
  size_t len = size * nmemb;

  if(!svc->started) {
    svc->final_uri = raptor_www_get_final_uri(www);
    svc->started = 1;
  }

  raptor_stringbuffer_append_counted_string(svc->sb,
                                            RASQAL_GOOD_CAST(const unsigned char*, ptr),
                                            len, 1);
}


static void
rasqal_service_content_type_handler(raptor_www* www, void* userdata, 
                                    const char* content_type)
{
  rasqal_service* svc = (rasqal_service*)userdata;
  size_t len;
  
  if(svc->content_type)
    RASQAL_FREE(char*, svc->content_type);

  len = strlen(content_type);
  svc->content_type = RASQAL_MALLOC(char*, len + 1);

  if(svc->content_type) {
    char* p;

    memcpy(svc->content_type, content_type, len + 1);

    for(p = svc->content_type; *p; p++) {
      if(*p == ';' || *p == ' ') {
        *p = '\0';
        break;
      }
    }
  }

}


/**
 * rasqal_service_execute_as_rowsource:
 * @svc: rasqal service
 *
 * INTERNAL - Execute a rasqal sparql protocol service to a rowsurce
 *
 * Return value: query results or NULL on failure
 */
rasqal_rowsource*
rasqal_service_execute_as_rowsource(rasqal_service* svc,
                                    rasqal_variables_table* vars_table)
{
  raptor_iostream* read_iostr = NULL;
  raptor_uri* read_base_uri = NULL;
  rasqal_query_results_formatter* read_formatter = NULL;
  raptor_uri* retrieval_uri = NULL;
  raptor_stringbuffer* uri_sb = NULL;
  size_t len;
  unsigned char* str;
  raptor_world* raptor_world_ptr = rasqal_world_get_raptor(svc->world);
  rasqal_rowsource* rowsource = NULL;
  
  if(!svc->www) {
    svc->www = raptor_new_www(raptor_world_ptr);

    if(!svc->www) {
      rasqal_log_error_simple(svc->world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                              "Failed to create WWW");
      goto error;
    }
  }
    
  svc->started = 0;
  svc->final_uri = NULL;
  svc->sb = raptor_new_stringbuffer();
  svc->content_type = NULL;
  
  if(svc->format)
    raptor_www_set_http_accept(svc->www, svc->format);
  else
    raptor_www_set_http_accept(svc->www, DEFAULT_FORMAT);

  raptor_www_set_write_bytes_handler(svc->www,
                                     rasqal_service_write_bytes, svc);
  raptor_www_set_content_type_handler(svc->www,
                                      rasqal_service_content_type_handler, svc);


  /* Construct a URI to retrieve following SPARQL protocol HTTP
   *  binding from concatenation of
   *
   * 1. service_uri
   * 2. '?'
   * 3. "query=" query_string
   * 4. "&default-graph-uri=" background graph URI if any
   * 5. "&named-graph-uri=" named graph URI for all named graphs
   * with URI-escaping of the values
   */

  uri_sb = raptor_new_stringbuffer();
  if(!uri_sb) {
    rasqal_log_error_simple(svc->world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                            "Failed to create stringbuffer");
    goto error;
  }

  str = raptor_uri_as_counted_string(svc->service_uri, &len);
  raptor_stringbuffer_append_counted_string(uri_sb, str, len, 1);

  raptor_stringbuffer_append_counted_string(uri_sb,
                                            RASQAL_GOOD_CAST(const unsigned char*, "?"), 1, 1);

  if(svc->query_string) {
    raptor_stringbuffer_append_counted_string(uri_sb,
                                              RASQAL_GOOD_CAST(const unsigned char*, "query="), 6, 1);
    raptor_stringbuffer_append_uri_escaped_counted_string(uri_sb,
                                                          svc->query_string,
                                                          svc->query_string_len,
                                                          1);
  }


  if(svc->data_graphs) {
    rasqal_data_graph* dg;
    int i;
    int bg_graph_count;
    
    for(i = 0, bg_graph_count = 0;
        (dg = (rasqal_data_graph*)raptor_sequence_get_at(svc->data_graphs, i));
        i++) {
      unsigned char* graph_str;
      size_t graph_len;
      raptor_uri* graph_uri;
      
      if(dg->flags & RASQAL_DATA_GRAPH_BACKGROUND) {

        if(bg_graph_count++) {
          if(bg_graph_count == 2) {
            /* Warn once, only when the second BG is seen */
            rasqal_log_warning_simple(svc->world,
                                      RASQAL_WARNING_LEVEL_MULTIPLE_BG_GRAPHS,
                                      NULL,
                                      "Attempted to add multiple background graphs");
          }
          /* always skip after first BG */
          continue;
        }
        
        raptor_stringbuffer_append_counted_string(uri_sb,
                                                  RASQAL_GOOD_CAST(const unsigned char*, "&default-graph-uri="), 19, 1);
        graph_uri = dg->uri;
      } else {
        raptor_stringbuffer_append_counted_string(uri_sb,
                                                  RASQAL_GOOD_CAST(const unsigned char*, "&named-graph-uri="), 17, 1);
        graph_uri = dg->name_uri;
      }
      
      graph_str = raptor_uri_as_counted_string(graph_uri, &graph_len);
      raptor_stringbuffer_append_uri_escaped_counted_string(uri_sb,
                                                            RASQAL_GOOD_CAST(const char*, graph_str), graph_len, 1);
    }
  }
  

  str = raptor_stringbuffer_as_string(uri_sb);

  retrieval_uri = raptor_new_uri(raptor_world_ptr, str);
  if(!retrieval_uri) {
    rasqal_log_error_simple(svc->world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                            "Failed to create retrieval URI %s",
                            raptor_uri_as_string(retrieval_uri));
    goto error;
  }

  raptor_free_stringbuffer(uri_sb); uri_sb = NULL;
  
  if(raptor_www_fetch(svc->www, retrieval_uri)) {
    rasqal_log_error_simple(svc->world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                            "Failed to fetch retrieval URI %s",
                            raptor_uri_as_string(retrieval_uri));
    goto error;
  }

  /* Takes ownership of svc->sb */
  read_iostr = rasqal_new_iostream_from_stringbuffer(raptor_world_ptr,
                                                     svc->sb);
  svc->sb = NULL;
  if(!read_iostr) {
    rasqal_log_error_simple(svc->world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                            "Failed to create iostream from string");
    goto error;
  }
    
  read_base_uri = svc->final_uri ? svc->final_uri : svc->service_uri;
  read_formatter = rasqal_new_query_results_formatter(svc->world,
                                                      /* format name */ NULL,
                                                      svc->content_type,
                                                      /* format URI */ NULL);
  if(!read_formatter) {
    rasqal_log_error_simple(svc->world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                            "Failed to find query results reader for content type %s returned from %s",
                            svc->content_type,
                            raptor_uri_as_string(read_base_uri));
    goto error;
  }

  /* Takes ownership of read_iostr with flags = 1 */
  rowsource = rasqal_query_results_formatter_get_read_rowsource(svc->world,
                                                                read_iostr,
                                                                read_formatter,
                                                                vars_table,
                                                                read_base_uri,
                                                                /* flags */ 1);
  read_iostr = NULL;
  if(!rowsource) {
    rasqal_log_error_simple(svc->world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                            "Failed to decode %s query results data returned from %s",
                            svc->content_type,
                            raptor_uri_as_string(read_base_uri));
    goto error;
  }


  error:
  if(retrieval_uri)
    raptor_free_uri(retrieval_uri);

  if(uri_sb)
    raptor_free_stringbuffer(uri_sb);

  if(read_formatter)
    rasqal_free_query_results_formatter(read_formatter);

  if(read_iostr)
    raptor_free_iostream(read_iostr);

  if(svc->final_uri) {
    raptor_free_uri(svc->final_uri);
    svc->final_uri = NULL;
  }

  if(svc->content_type) {
    RASQAL_FREE(char*, svc->content_type);
    svc->content_type = NULL;
  }

  if(svc->sb) {
    raptor_free_stringbuffer(svc->sb);
    svc->sb = NULL;
  }
  
  return rowsource;
}


/**
 * rasqal_service_execute:
 * @svc: rasqal service
 *
 * Execute a rasqal sparql protocol service
 *
 * Return value: query results or NULL on failure
 */
rasqal_query_results*
rasqal_service_execute(rasqal_service* svc)
{
  rasqal_query_results* results = NULL;
  rasqal_variables_table* vars_table;
  rasqal_rowsource* rowsource = NULL;

  results = rasqal_new_query_results2(svc->world, NULL, 
                                      RASQAL_QUERY_RESULTS_BINDINGS);

  if(!results) {
    rasqal_log_error_simple(svc->world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                            "Failed to create query results");
    goto error;
  }
  
  vars_table = rasqal_query_results_get_variables_table(results);

  rowsource = rasqal_service_execute_as_rowsource(svc, vars_table);
  if(!rowsource)
    goto error;

  while(1) {
    rasqal_row* row = rasqal_rowsource_read_row(rowsource);
    if(!row)
      break;
    rasqal_query_results_add_row(results, row);
  }


  error:
  if(rowsource)
    rasqal_free_rowsource(rowsource);

  return results;
}

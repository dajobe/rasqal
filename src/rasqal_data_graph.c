/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_data_graph.c - Rasqal data graph class
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
#include <ctype.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdarg.h>

#include "rasqal.h"
#include "rasqal_internal.h"


static rasqal_data_graph*
rasqal_new_data_graph_common(rasqal_world* world,
                             raptor_uri* uri,
                             raptor_iostream* iostr, raptor_uri* base_uri,
                             raptor_uri* name_uri,
                             unsigned int flags,
                             const char* format_type,
                             const char* format_name,
                             raptor_uri* format_uri)
{
  rasqal_data_graph* dg;

  dg = RASQAL_CALLOC(rasqal_data_graph*, 1, sizeof(*dg));
  if(dg) {
    dg->world = world;

    dg->usage = 1;

    if(iostr)
      dg->iostr = iostr;
    else if(uri)
      dg->uri = raptor_uri_copy(uri);

    if(name_uri)
      dg->name_uri = raptor_uri_copy(name_uri);

    dg->flags = flags;

    if(format_type) {
      size_t len = strlen(format_type);
      dg->format_type = RASQAL_MALLOC(char*, len + 1);
      if(!dg->format_type)
        goto error;

      memcpy(dg->format_type, format_type, len + 1);
    }

    if(format_name) {
      size_t len = strlen(format_name);
      dg->format_name = RASQAL_MALLOC(char*, len + 1);
      if(!dg->format_name)
        goto error;

      memcpy(dg->format_name, format_name, len + 1);
    }

    if(format_uri)
      dg->format_uri = raptor_uri_copy(format_uri);

    if(base_uri)
      dg->base_uri = raptor_uri_copy(base_uri);
  }

  return dg;

  error:
  rasqal_free_data_graph(dg);
  return NULL;
}


/**
 * rasqal_new_data_graph_from_uri:
 * @world: rasqal_world object
 * @uri: source URI
 * @name_uri: name of graph (or NULL)
 * @flags: %RASQAL_DATA_GRAPH_NAMED or %RASQAL_DATA_GRAPH_BACKGROUND
 * @format_type: MIME Type of data format at @uri (or NULL)
 * @format_name: Raptor parser Name of data format at @uri (or NULL)
 * @format_uri: URI of data format at @uri (or NULL)
 * 
 * Constructor - create a new #rasqal_data_graph.
 * 
 * The name_uri is only used when the flags are %RASQAL_DATA_GRAPH_NAMED.
 * 
 * Return value: a new #rasqal_data_graph or NULL on failure.
 **/
rasqal_data_graph*
rasqal_new_data_graph_from_uri(rasqal_world* world, raptor_uri* uri,
                               raptor_uri* name_uri, unsigned int flags,
                               const char* format_type,
                               const char* format_name,
                               raptor_uri* format_uri)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(uri, raptor_uri, NULL);

  return rasqal_new_data_graph_common(world,
                                      uri,
                                      /* iostr */ NULL, /* base URI */ NULL,
                                      name_uri, flags,
                                      format_type, format_name, format_uri);
}


/**
 * rasqal_new_data_graph_from_iostream:
 * @world: rasqal_world object
 * @iostr: source graph format iostream
 * @base_uri: base URI for iostream content
 * @name_uri: name of graph (or NULL)
 * @flags: %RASQAL_DATA_GRAPH_NAMED or %RASQAL_DATA_GRAPH_BACKGROUND
 * @format_type: MIME Type of data format at @uri (or NULL)
 * @format_name: Raptor parser Name of data format at @uri (or NULL)
 * @format_uri: URI of data format at @uri (or NULL)
 * 
 * Constructor - create a new #rasqal_data_graph from iostream content
 * 
 * The @name_uri is used when the flags are %RASQAL_DATA_GRAPH_NAMED.
 *
 * The @base_uri is used to provide the Raptor parser a base URI.  If
 * a base URI is required but none is given, the parsing will fail
 * and the query that uses this data source will fail.
 * 
 * Return value: a new #rasqal_data_graph or NULL on failure.
 **/
rasqal_data_graph*
rasqal_new_data_graph_from_iostream(rasqal_world* world,
                                    raptor_iostream* iostr,
                                    raptor_uri* base_uri,
                                    raptor_uri* name_uri,
                                    unsigned int flags,
                                    const char* format_type,
                                    const char* format_name,
                                    raptor_uri* format_uri)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);

  return rasqal_new_data_graph_common(world,
                                      /* uri */ NULL,
                                      iostr, base_uri,
                                      name_uri, flags,
                                      format_type, format_name, format_uri);
}


/**
 * rasqal_new_data_graph_from_data_graph:
 * @dg: #rasqal_data_graph object to copy or NULL
 *
 * Copy Constructor - create a new #rasqal_data_graph object from an existing #rasqal_data_graph object.
 *
 * Return value: a new #rasqal_data_graph object or NULL if @dg was NULL.
 **/
rasqal_data_graph*
rasqal_new_data_graph_from_data_graph(rasqal_data_graph* dg)
{
  dg->usage++;

  return dg;
}


/**
 * rasqal_free_data_graph:
 * @dg: #rasqal_data_graph object
 * 
 * Destructor - destroy a #rasqal_data_graph object.
 *
 **/
void
rasqal_free_data_graph(rasqal_data_graph* dg)
{
  if(!dg)
    return;

  if(--dg->usage)
    return;
  
  if(dg->uri)
    raptor_free_uri(dg->uri);
  if(dg->name_uri)
    raptor_free_uri(dg->name_uri);
  if(dg->format_type)
    RASQAL_FREE(char*, dg->format_type);
  if(dg->format_name)
    RASQAL_FREE(char*, dg->format_name);
  if(dg->format_uri)
    raptor_free_uri(dg->format_uri);
  if(dg->base_uri)
    raptor_free_uri(dg->base_uri);

  RASQAL_FREE(rasqal_data_graph, dg);
}


/**
 * rasqal_data_graph_print:
 * @dg: #rasqal_data_graph object
 * @fh: the FILE* handle to print to
 *
 * Print a Rasqal data graph in a debug format.
 * 
 * The print debug format may change in any release.
 *
 * Return value: non-0 on failure
 **/
int
rasqal_data_graph_print(rasqal_data_graph* dg, FILE* fh)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(dg, rasqal_data_graph, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(fh, FILE*, 1);

  if(dg->iostr) {
    if(dg->name_uri)
      fprintf(fh, "data graph(from iostream, named as %s, flags %u",
              raptor_uri_as_string(dg->name_uri), dg->flags);
    else
      fprintf(fh, "data graph(from iostream, %u", dg->flags);
  } else {
    /* dg->uri must exist */
    if(dg->name_uri)
      fprintf(fh, "data graph(from uri %s, named as %s, flags %u",
              raptor_uri_as_string(dg->uri),
              raptor_uri_as_string(dg->name_uri),
              dg->flags);
    else
      fprintf(fh, "data graph(from uri %s, flags %u",
              raptor_uri_as_string(dg->uri), dg->flags);
  }
  
  if(dg->format_type || dg->format_name || dg->format_uri) {
    fputs(" with format ", fh);
    if(dg->format_type)
      fprintf(fh, "type %s", dg->format_type);
    if(dg->format_name)
      fprintf(fh, "name %s", dg->format_name);
    if(dg->format_uri)
      fprintf(fh, "uri %s", raptor_uri_as_string(dg->format_uri));
    if(dg->base_uri)
      fprintf(fh, "base uri %s", raptor_uri_as_string(dg->base_uri));
  }
  fputc(')', fh);
  
  return 0;
}

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_expr.c - Rasqal data graph class
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


/**
 * rasqal_new_data_graph:
 * @world: rasqal_world object
 * @uri: source URI
 * @name_uri: name of graph (or NULL)
 * @flags: %RASQAL_DATA_GRAPH_NAMED or %RASQAL_DATA_GRAPH_BACKGROUND
 * 
 * Constructor - create a new #rasqal_data_graph.
 * 
 * The name_uri is only used when the flags are %RASQAL_DATA_GRAPH_NAMED.
 * 
 * Return value: a new #rasqal_data_graph or NULL on failure.
 **/
rasqal_data_graph*
rasqal_new_data_graph(rasqal_world* world, raptor_uri* uri,
                      raptor_uri* name_uri, int flags)
{
  rasqal_data_graph* dg;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(uri, raptor_uri, NULL);

  dg = (rasqal_data_graph*)RASQAL_CALLOC(rasqal_data_graph, 1, sizeof(*dg));
  if(dg) {
    dg->world = world;
    dg->uri = raptor_uri_copy(uri);
    if(name_uri)
      dg->name_uri = raptor_uri_copy(name_uri);
    dg->flags = flags;
  }

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
  
  if(dg->uri)
    raptor_free_uri(dg->uri);
  if(dg->name_uri)
    raptor_free_uri(dg->name_uri);
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

  if(dg->name_uri)
    fprintf(fh, "data graph(%s named as %s flags %d)", 
            raptor_uri_as_string(dg->uri),
            raptor_uri_as_string(dg->name_uri),
            dg->flags);
  else
    fprintf(fh, "data graph(%s, flags %d)", 
            raptor_uri_as_string(dg->uri), dg->flags);

  return 0;
}

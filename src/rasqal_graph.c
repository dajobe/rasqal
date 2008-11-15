/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_graph.c - Rasqal Graph API
 *
 * Copyright (C) 2008, David Beckett http://www.dajobe.org/
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

#include "rasqal.h"
#include "rasqal_internal.h"


typedef struct 
{
  int usage;
  rasqal_world *world;
  raptor_uri *uri;
  void *user_data;
} rasqal_graph;

typedef struct 
{
  rasqal_world *world;
  rasqal_graph *graph;
  void *user_data;
} rasqal_graph_match;

typedef struct 
{
  rasqal_world *world;
  rasqal_graph *graph;
  void *user_data;
} rasqal_graph_bindings;


struct rasqal_graph_factory_s
{
  int version; /* API version */

  /* One-time initialisation/termination of graph factory (optional) */
  void* (*init_factory)(rasqal_world* world);
  void (*terminate_factory)(void *graph_factory_user_data);
  
  /* RDF Graph API (required)
   *
   * (acts like librdf_model)
   */
  void *(*new_graph)(rasqal_world* world, raptor_uri* uri);
  void (*free_graph)(void *graph_user_data);
  /* Check for presence of a triple (NOT triple pattern) in a graph (required)
   * (acts like librdf_model_contains_statement)
   */
  int (*graph_triple_present)(void *graph_user_data, rasqal_triple *triple);

  /* Triple pattern matching API (required)
   *
   * Find triples matching a triple pattern
   * (acts like librdf_model_find_statements returning a librdf_stream of
   * librdf_statement)
   */
  void* (*new_graph_match)(rasqal_graph *graph, rasqal_triple *triple);
  rasqal_triple* (*graph_match_get_triple)(void *match_user_data);
  void (*free_graph_match)(void *match_user_data);

  /* Graph pattern binding API (optional)
   * 
   * Bind variables when triples in the graph match a graph pattern
   */
  void* (*new_graph_bindings)(rasqal_graph *graph, rasqal_triple **triples, int triples_count, rasqal_expression *filter);
  int (*graph_bindings_bind)(void *graph_bindings_user_data);
  void (*free_graph_bindings)(void *graph_bindings_user_data);
};


/* prototypes */
int rasqal_init_graph_factory(rasqal_world *world, rasqal_graph_factory *factory);
void rasqal_free_graph_factory(rasqal_world *world);
rasqal_graph* rasqal_new_graph(rasqal_world* world, raptor_uri *uri);
void rasqal_free_graph(rasqal_graph *graph);
int rasqal_graph_triple_present(rasqal_graph *graph, rasqal_triple *triple);
rasqal_graph_match* rasqal_new_graph_match(rasqal_graph *graph, rasqal_triple *triple);
void rasqal_free_graph_match(rasqal_graph_match *match);
rasqal_triple* rasqal_graph_match_get_triple(rasqal_graph_match *match);
rasqal_graph_bindings* rasqal_new_graph_bindings(rasqal_graph *graph, rasqal_triple **triples, int triples_count, rasqal_expression *filter);
void rasqal_free_graph_bindings(rasqal_graph_bindings *graph_bindings);
int rasqal_graph_bindings_bind(rasqal_graph_bindings *graph_bindings);


/**
 * rasqal_init_graph_factory:
 * @world: rasqal world
 * @factory: factory
 *
 * Set graph matching factory for rasqal world.
 *
 * Return value: non-0 on failure
 **/
int
rasqal_init_graph_factory(rasqal_world *world,
                          rasqal_graph_factory *factory)
{
  if(world->graph_factory)
    rasqal_free_graph_factory(world);
  
  world->graph_factory = factory;

  if(factory->init_factory) {
    world->graph_factory_user_data = factory->init_factory(world);
    return (world->graph_factory_user_data == NULL);
  }

  return 0;
}


/**
 * rasqal_free_graph_factory:
 * @world: rasqal world
 *
 * Free any resources attached to the graph factory
 */
void
rasqal_free_graph_factory(rasqal_world *world)
{
  rasqal_graph_factory *factory = world->graph_factory;

  if(factory->terminate_factory)
    factory->terminate_factory(world->graph_factory_user_data);
}


/**
 * rasqal_new_graph:
 * @world: rasqal world
 * @uri: graph URI (or NULL for default graph)
 * 
 * Constructor - Create a new graph API for a given URI
 * 
 * Returns: graph API object or NULL on failure
 **/
rasqal_graph*
rasqal_new_graph(rasqal_world* world, raptor_uri *uri)
{
  rasqal_graph* g;
  rasqal_graph_factory *factory = world->graph_factory;

  g = (rasqal_graph*)RASQAL_CALLOC(rasqal_graph, sizeof(rasqal_graph), 1);
  if(!g)
    return NULL;
  g->world = world;
  g->user_data = factory->new_graph(world, uri);
  if(!g->user_data) {
    RASQAL_FREE(rasqal_graph, g);
    return NULL;
  }

  g->usage = 1;
  return g;
}


/**
 * rasqal_free_graph:
 * @graph: graph API object
 * 
 * Destructor - Destroy a graph API object
 * 
 **/
void
rasqal_free_graph(rasqal_graph *graph)
{
  rasqal_graph_factory *factory = graph->world->graph_factory;

  if(graph->usage--)
    return;
  
  if(factory->free_graph)
    factory->free_graph(graph->user_data);
  RASQAL_FREE(rasqal_graph, graph);
}


/**
 * rasqal_new_graph_from_graph:
 * @graph: graph API object
 *
 * Copy constructor
 *
 * Return value: new graph object
 */
static rasqal_graph*
rasqal_new_graph_from_graph(rasqal_graph *graph)
{
  graph->usage++;

  return graph;
}


/**
 * rasqal_graph_triple_present:
 * @graph: graph API object
 * @triple: triple
 *
 * Test if a triple is in a graph
 *
 * Return value: non-0 if triple is present
 */
int
rasqal_graph_triple_present(rasqal_graph *graph, rasqal_triple *triple)
{
  rasqal_graph_factory *factory = graph->world->graph_factory;

  return factory->graph_triple_present(graph->user_data, triple);
}


/**
 * rasqal_new_graph_match:
 * @graph: graph API object
 * @triple: triple pattern
 * 
 * Constructor - create a new triple pattern matcher for a triple pattern
 * 
 * Returns: triple pattern matcher or NULL on failure
 **/
rasqal_graph_match*
rasqal_new_graph_match(rasqal_graph *graph, rasqal_triple *triple)
{
  rasqal_graph_match* gm;
  rasqal_graph_factory *factory = graph->world->graph_factory;

  gm = (rasqal_graph_match*)RASQAL_CALLOC(rasqal_graph_match,
                                          sizeof(rasqal_graph_match), 1);
  if(!gm)
    return NULL;

  gm->graph = rasqal_new_graph_from_graph(graph);
  gm->user_data = factory->new_graph_match(graph, triple);
  if(!gm->user_data) {
    RASQAL_FREE(graph_match, gm);
    return NULL;
  }

  return gm;
}


/**
 * rasqal_free_graph_match:
 * @match: triple pattern matcher object
 * 
 * Destructor - Delete a triple pattern matcher
 **/
void
rasqal_free_graph_match(rasqal_graph_match *match)
{
  rasqal_graph_factory *factory = match->world->graph_factory;

  rasqal_free_graph(match->graph);

  if(factory->free_graph_match)
    factory->free_graph_match(match->user_data);
  RASQAL_FREE(rasqal_graph_match, match);
}


/**
 * rasqal_graph_match_get_triple:
 * @match: triple pattern matcher object
 * 
 * Get the next triple from a triple pattern matcher
 * 
 * Returns: new triple object or NULL when no (more) triples match
 **/
rasqal_triple*
rasqal_graph_match_get_triple(rasqal_graph_match *match)
{
  rasqal_graph_factory *factory = match->world->graph_factory;

  return factory->graph_match_get_triple(match->user_data);
}


/**
 * rasqal_new_graph_bindings:
 * @graph: graph API object 
 * @triples: graph pattern as an array of triples of size @triples_count
 * @triples_count: number of triples in graph pattern
 * @filter: expression over the resulting bound variables to filter and constrain matches
 * 
 * Returns: 
 **/
rasqal_graph_bindings*
rasqal_new_graph_bindings(rasqal_graph *graph,
                          rasqal_triple **triples,
                          int triples_count,
                          rasqal_expression *filter)
{
  rasqal_graph_bindings* gb;
  rasqal_graph_factory *factory = graph->world->graph_factory;

  gb = (rasqal_graph_bindings*)RASQAL_CALLOC(rasqal_graph_bindings,
                                             sizeof(rasqal_graph_bindings), 1);
  if(!gb)
    return NULL;
  
  gb->graph = rasqal_new_graph_from_graph(graph);
  gb->user_data = factory->new_graph_bindings(graph, triples, triples_count,
                                              filter);
  if(!gb->user_data) {
    RASQAL_FREE(graph_bindings, gb);
    return NULL;
  }

  return gb;
}


/**
 * rasqal_free_graph_bindings:
 * @graph_bindings: graph binding object
 * 
 * Destructor - free a graph bindings object
 **/
void
rasqal_free_graph_bindings(rasqal_graph_bindings *graph_bindings)
{
  rasqal_graph_factory *factory = graph_bindings->world->graph_factory;

  rasqal_free_graph(graph_bindings->graph);

  if(factory->free_graph_bindings)
    factory->free_graph_bindings(graph_bindings->user_data);
  RASQAL_FREE(rasqal_graph_bindings, graph_bindings);
}


/**
 * rasqal_graph_bindings_bind:
 * @graph_bindings: graph bindings object
 *
 * Match a graph pattern and bind variables to the matches
 * 
 * Returns: non-0 on failure
 **/
int
rasqal_graph_bindings_bind(rasqal_graph_bindings *graph_bindings)
{
  rasqal_graph_factory *factory = graph_bindings->world->graph_factory;

  return factory->graph_bindings_bind(graph_bindings->user_data);
}

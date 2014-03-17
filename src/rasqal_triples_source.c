/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_triples_source.c - Rasqal triples source matching triple patterns against triples
 *
 * Copyright (C) 2004-2009, David Beckett http://www.dajobe.org/
 * Copyright (C) 2004-2005, University of Bristol, UK http://www.bristol.ac.uk/
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
#include <stdarg.h>

#include "rasqal.h"
#include "rasqal_internal.h"


/**
 * rasqal_set_triples_source_factory:
 * @world: rasqal_world object
 * @register_fn: registration function
 * @user_data: user data for registration
 *
 * Register a factory to generate triple sources.
 * 
 * Registers the factory that returns triples sources.  Note that
 * there is only one of these per runtime. 
 *
 * The #rasqal_triples_source_factory factory method new_triples_source is
 * called with the user data for some query and #rasqal_triples_source.
 *
 * Return value: non-zero on failure
 **/
RASQAL_EXTERN_C
int
rasqal_set_triples_source_factory(rasqal_world* world,
                                  rasqal_triples_source_factory_register_fn register_fn,
                                  void* user_data)
{
  int rc;
  int version;
  
  if(!world || !register_fn)
    return 1;
  
  /* for compatibility with old API that does not call this - FIXME Remove V2 */
  rasqal_world_open(world);
  
  world->triples_source_factory.user_data = user_data;
  rc = register_fn(&world->triples_source_factory);

  /* Failed if the factory API version is not in the supported range */
  version = world->triples_source_factory.version;
  if(!(version >= RASQAL_TRIPLES_SOURCE_FACTORY_MIN_VERSION &&
       version <= RASQAL_TRIPLES_SOURCE_FACTORY_MAX_VERSION)
     ) {
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                            "Failed to register triples source factory - API %d is not in supported range %d to %d", 
                            version,
                            RASQAL_TRIPLES_SOURCE_FACTORY_MIN_VERSION,
                            RASQAL_TRIPLES_SOURCE_FACTORY_MAX_VERSION);
    rc = 1;
  }

  return rc;
}


/**
 * rasqal_triples_source_error_handler:
 * @query: query
 * @locator: any locator information
 * @message: error message
 *
 * INTERNAL - Return an error during creation of a triples source
 */
void
rasqal_triples_source_error_handler(rasqal_query* rdf_query,
                                    raptor_locator* locator, 
                                    const char* message)
{
  rasqal_log_error_simple(rdf_query->world, RAPTOR_LOG_LEVEL_ERROR, locator,
                          "%s", message);
}


/**
 * rasqal_triples_source_error_handler2:
 * @world: world
 * @locator: any locator information
 * @message: error message
 *
 * INTERNAL - Return an error during creation of a triples source
 */
void
rasqal_triples_source_error_handler2(rasqal_world* world,
                                     raptor_locator* locator,
                                     const char* message)
{
  rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, locator,
                          "%s", message);
}


/**
 * rasqal_new_triples_source:
 * @query: query
 *
 * INTERNAL - Create a new triples source
 *
 * Return value: a new triples source or NULL on failure
 */
rasqal_triples_source*
rasqal_new_triples_source(rasqal_query* query)
{
  rasqal_triples_source_factory* rtsf = &query->world->triples_source_factory;
  rasqal_triples_source* rts;
  int rc = 0;
  
  rts = RASQAL_CALLOC(rasqal_triples_source*, 1, sizeof(*rts));
  if(!rts)
    return NULL;

  rts->user_data = RASQAL_CALLOC(void*, 1, rtsf->user_data_size);
  if(!rts->user_data) {
    RASQAL_FREE(rasqal_triples_source, rts);
    return NULL;
  }
  rts->query = query;

  if(rtsf->version >= 3 && rtsf->init_triples_source2) {
    /* rasqal_triples_source_factory API V3 */
    unsigned int flags = 0;

    if(query->features[RASQAL_FEATURE_NO_NET])
      flags |= 1;
    rc = rtsf->init_triples_source2(query->world, query->data_graphs,
                                    rtsf->user_data, rts->user_data, rts,
                                    rasqal_triples_source_error_handler2,
                                    flags);
    /* if there is an error, it will have been already reported more
     * specifically via the error handler so no need to do it again
     * below in a generic form.
     */
    goto error_tidy;
  } else if(rtsf->version >= 2 && rtsf->init_triples_source) {
    /* rasqal_triples_source_factory API V2 */
    rc = rtsf->init_triples_source(query, rtsf->user_data, rts->user_data, rts,
                                   rasqal_triples_source_error_handler);
    /* if there is an error, it will have been already reported more
     * specifically via the error handler so no need to do it again
     * below in a generic form.
     */
    goto error_tidy;
  } else
    /* rasqal_triples_source_factory API V1 */
    rc = rtsf->new_triples_source(query, rtsf->user_data, rts->user_data, rts);


  /* Failure if the returned triples source API version is not in the
   * supported range
   */
  if(!(rts->version >= RASQAL_TRIPLES_SOURCE_MIN_VERSION && 
       rts->version <= RASQAL_TRIPLES_SOURCE_MAX_VERSION)
     ) {
    rasqal_log_error_simple(query->world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                            "Failed to create triples source - API %d not in range %d to %d", 
                            rts->version,
                            RASQAL_TRIPLES_SOURCE_MIN_VERSION,
                            RASQAL_TRIPLES_SOURCE_MAX_VERSION);
    rc = 1;
  }

  if(rc) {
    if(rc > 0) {
      rasqal_log_error_simple(query->world, RAPTOR_LOG_LEVEL_ERROR,
                              &query->locator,
                              "Failed to make triples source.");
    } else {
      rasqal_log_error_simple(query->world, RAPTOR_LOG_LEVEL_ERROR,
                              &query->locator,
                              "No data to query.");
    }
  }

  error_tidy:
  if(rc) {
    RASQAL_FREE(user_data, rts->user_data);
    RASQAL_FREE(rasqal_triples_source, rts);
    return NULL;
  }
  
  return rts;
}


void
rasqal_free_triples_source(rasqal_triples_source *rts)
{
  if(!rts)
    return;
  
  if(rts->user_data) {
    rts->free_triples_source(rts->user_data);
    RASQAL_FREE(user_data, rts->user_data);
    rts->user_data = NULL;
  }
  
  RASQAL_FREE(rasqal_triples_source, rts);
}


int
rasqal_triples_source_triple_present(rasqal_triples_source *rts,
                                     rasqal_triple *t)
{
  return rts->triple_present(rts, rts->user_data, t);
}


static void
rasqal_free_triples_match(rasqal_triples_match* rtm)
{
  if(!rtm)
    return;

  if(!rtm->is_exact)
    rtm->finish(rtm, rtm->user_data);

  RASQAL_FREE(rasqal_triples_match, rtm);
}


rasqal_triples_match*
rasqal_new_triples_match(rasqal_query* query,
                         rasqal_triples_source* triples_source,
                         rasqal_triple_meta *m, rasqal_triple *t)
{
  rasqal_triples_match* rtm;

  if(!triples_source)
    return NULL;

  rtm = RASQAL_CALLOC(rasqal_triples_match*, 1, sizeof(*rtm));
  if(rtm) {
    rtm->world = query->world;

    /* exact if there are no variables in the triple parts */
    rtm->is_exact = 1;
    if(rasqal_literal_as_variable(t->predicate) ||
       rasqal_literal_as_variable(t->subject) ||
       rasqal_literal_as_variable(t->object))
      rtm->is_exact = 0;

    if(rtm->is_exact) {
      if(!triples_source->triple_present(triples_source,
                                         triples_source->user_data, t)) {
        rasqal_free_triples_match(rtm);
        rtm = NULL;
      }
    } else {
      if(triples_source->init_triples_match(rtm, triples_source,
                                            triples_source->user_data,
                                            m, t)) {
        rasqal_free_triples_match(rtm);
        rtm = NULL;
      }
    }
  }

  return rtm;
}


/* methods */
rasqal_triple_parts
rasqal_triples_match_bind_match(struct rasqal_triples_match_s* rtm, 
                                rasqal_variable *bindings[4],
                                rasqal_triple_parts parts)
{
  if(rtm->is_exact)
    return RASQAL_TRIPLE_SPO;
  
  return rtm->bind_match(rtm, rtm->user_data, bindings, parts);
}


void
rasqal_triples_match_next_match(struct rasqal_triples_match_s* rtm)
{
  if(rtm->is_exact) {
    rtm->finished++;
    return;
  }
  
  rtm->next_match(rtm, rtm->user_data);
}


int
rasqal_triples_match_is_end(struct rasqal_triples_match_s* rtm)
{
  if(rtm->finished)
    return 1;
  if(rtm->is_exact)
    return rtm->finished;

  return rtm->is_end(rtm, rtm->user_data);
}


/**
 * rasqal_reset_triple_meta:
 * @m: Triple pattern metadata
 * 
 * INTERNAL - reset the metadata associated with a triple pattern
 * 
 * Return value: number of parts of the triple that were reset (0..4)
 **/
int
rasqal_reset_triple_meta(rasqal_triple_meta* m)
{
  int resets = 0;
  
  if(m->triples_match) {
    rasqal_free_triples_match(m->triples_match);
    m->triples_match = NULL;
  }

  if(m->bindings[0] && (m->parts & RASQAL_TRIPLE_SUBJECT)) {
    rasqal_variable_set_value(m->bindings[0],  NULL);
    resets++;
  }
  if(m->bindings[1] && (m->parts & RASQAL_TRIPLE_PREDICATE)) {
    rasqal_variable_set_value(m->bindings[1],  NULL);
    resets++;
  }
  if(m->bindings[2] && (m->parts & RASQAL_TRIPLE_OBJECT)) {
    rasqal_variable_set_value(m->bindings[2],  NULL);
    resets++;
  }
  if(m->bindings[3] && (m->parts & RASQAL_TRIPLE_ORIGIN)) {
    rasqal_variable_set_value(m->bindings[3],  NULL);
    resets++;
  }

  m->executed = 0;
  
  return resets;
}



/*
 * rasqal_triples_source_support_feature:
 * @rts: triples source
 * @feature: triples source feature
 *
 * INTERNAL - Test support for a feature
 *
 * Return value: non-0 if @feature is supported
 */
int
rasqal_triples_source_support_feature(rasqal_triples_source *rts,
                                      rasqal_triples_source_feature feature)
{
  if(rts->version >= 2 && rts->support_feature)
    return rts->support_feature(rts->user_data, feature);
  else
    return 0;
}



/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_dataset.c - Rasqal dataset (set of graphs) class
 *
 * Intended to provide data interface for SPARQL Query 1.1 querying
 * and SPARQL Update 1.1 RDF Graph Management operations.
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


#define DEBUG_FH stderr


struct rasqal_dataset_triple_s {
  struct rasqal_dataset_triple_s *next;

  rasqal_triple *triple;
};

typedef struct rasqal_dataset_triple_s rasqal_dataset_triple;


struct rasqal_dataset_s
{
  rasqal_world* world;
  
  rasqal_literal* base_uri_literal;

  rasqal_dataset_triple *head;
  rasqal_dataset_triple *tail;
};


struct rasqal_dataset_term_iterator_s {
  rasqal_dataset* dataset;

  /* triple to match */
  rasqal_triple match;

  /* single triple part wanted returned */
  rasqal_triple_parts want;

  /* parts to match on - XOR of @want */
  unsigned int parts;

  /* current triple */
  rasqal_dataset_triple *cursor;
};



/*
 * rasqal_new_dataset:
 * @world: rasqal world
 *
 * INTERNAL - Constructor - Create a new dataset
 *
 * Return value: new dataset or NULL on failure
 */
rasqal_dataset*
rasqal_new_dataset(rasqal_world* world)
{
  rasqal_dataset* ds;
  
  if(!world)
    return NULL;

  ds = RASQAL_CALLOC(rasqal_dataset*, 1, sizeof(*ds));
  if(!ds)
    return NULL;
  
  ds->world = world;

  return ds;
}


/*
 * rasqal_free_dataset:
 * @ds: dataset
 *
 * INTERNAL - Destructor - destroy a dataset
 */
void
rasqal_free_dataset(rasqal_dataset* ds) 
{
  rasqal_dataset_triple *cur;

  if(!ds)
    return;

  cur = ds->head;
  while(cur) {
    rasqal_dataset_triple *next = cur->next;

    /* free shared URI literal (if present) */
    rasqal_triple_set_origin(cur->triple, NULL);

    rasqal_free_triple(cur->triple);
    RASQAL_FREE(rasqal_dataset_triple, cur);
    cur = next;
  }

  if(ds->base_uri_literal)
    rasqal_free_literal(ds->base_uri_literal);

  RASQAL_FREE(rasqal_dataset, ds);
}


static void
rasqal_dataset_statement_handler(void *user_data,
                                 raptor_statement *statement)
{
  rasqal_dataset* ds;
  rasqal_dataset_triple *triple;
  
  ds = (rasqal_dataset*)user_data;

  triple = RASQAL_MALLOC(rasqal_dataset_triple*, sizeof(*triple));
  triple->next = NULL;
  triple->triple = raptor_statement_as_rasqal_triple(ds->world,
                                                     statement);

  /* this origin URI literal is shared amongst the triples and
   * freed only in rasqal_free_dataset()
   */
  if(ds->base_uri_literal)
    rasqal_triple_set_origin(triple->triple, ds->base_uri_literal);

  if(ds->tail)
    ds->tail->next = triple;
  else
    ds->head = triple;

  ds->tail = triple;
}


/*
 * rasqal_dataset_load_graph_iostream:
 * @ds: dataset
 * @format_name: query result format name (or NULL to guess)
 * @iostr: iostream to read query result format from
 * @base_uri: base URI for reading from stream (or NULL)
 *
 * INTERNAL - Load a query result format from an iostream into a dataset
 *
 * Return value: non-0 on failure
 */
int
rasqal_dataset_load_graph_iostream(rasqal_dataset* ds,
                                   const char* format_name,
                                   raptor_iostream* iostr,
                                   raptor_uri* base_uri)
{
  raptor_parser* parser;

  if(!ds)
    return 1;

  if(base_uri) {
    if(ds->base_uri_literal)
      rasqal_free_literal(ds->base_uri_literal);
    
    ds->base_uri_literal = rasqal_new_uri_literal(ds->world,
                                                  raptor_uri_copy(base_uri));
  }

  if(format_name) {
    if(!raptor_world_is_parser_name(ds->world->raptor_world_ptr, format_name)) {
      rasqal_log_error_simple(ds->world, RAPTOR_LOG_LEVEL_ERROR,
                              /* locator */ NULL,
                              "Invalid format name %s ignored",
                              format_name);
      format_name = NULL;
    }
  }

  if(!format_name)
    format_name = "guess";

  /* parse iostr with parser and base_uri */
  parser = raptor_new_parser(ds->world->raptor_world_ptr, format_name);
  raptor_parser_set_statement_handler(parser, ds,
                                      rasqal_dataset_statement_handler);
  
  /* parse and store triples */
  raptor_parser_parse_iostream(parser, iostr, base_uri);
    
  raptor_free_parser(parser);

  return 0;
}


static rasqal_dataset_term_iterator*
rasqal_dataset_init_match_internal(rasqal_dataset* ds,
                                   rasqal_literal* subject,
                                   rasqal_literal* predicate,
                                   rasqal_literal* object)
{
  rasqal_dataset_term_iterator* iter;

  if(!ds)
    return NULL;
  
  iter = RASQAL_CALLOC(rasqal_dataset_term_iterator*, 1, sizeof(*iter));

  if(!iter)
    return NULL;

  iter->dataset = ds;
  
  iter->match.subject = subject;
  iter->match.predicate = predicate;
  iter->match.object = object;

  iter->cursor = NULL;

  if(!subject)
    iter->want = RASQAL_TRIPLE_SUBJECT;
  else if(!object)
    iter->want = RASQAL_TRIPLE_OBJECT;
  else
    iter->want = RASQAL_TRIPLE_PREDICATE;

  iter->parts = RASQAL_GOOD_CAST(unsigned int, RASQAL_TRIPLE_SPO) ^ iter->want;

  if(ds->base_uri_literal)
    iter->parts |= RASQAL_TRIPLE_ORIGIN;
  
  if(rasqal_dataset_term_iterator_next(iter)) {
    rasqal_free_dataset_term_iterator(iter);
    return NULL;
  }
  
  return iter;
}


/*
 * rasqal_free_dataset_term_Iterator:
 * @ds: dataset
 *
 * INTERNAL - Destructor - destroy a dataset term iterator
 */
void
rasqal_free_dataset_term_iterator(rasqal_dataset_term_iterator* iter)
{
  if(!iter)
    return;
  
  RASQAL_FREE(rasqal_dataset_term_iterator, iter);
}


/*
 * rasqal_dataset_term_iterator_get:
 * @iter: rasqal dataset term iterator
 *
 * INTERNAL - Get the current literal term from the iterator
 *
 * Return value: literal or NULL on failure / iterator is exhausted
 */
rasqal_literal*
rasqal_dataset_term_iterator_get(rasqal_dataset_term_iterator* iter)
{
  if(!iter)
    return NULL;
  
  if(!iter->cursor)
    return NULL;

  if(iter->want == RASQAL_TRIPLE_SUBJECT)
    return iter->cursor->triple->subject;
  else if(iter->want == RASQAL_TRIPLE_PREDICATE)
    return iter->cursor->triple->predicate;
  else
    return iter->cursor->triple->object;
}


/*
 * rasqal_dataset_term_iterator_next:
 * @iter: rasqal dataset term iterator
 *
 * INTERNAL - Move the iterator to the next object
 *
 * Return value: non-0 on error or if the iterator is exhausted
 */
int
rasqal_dataset_term_iterator_next(rasqal_dataset_term_iterator* iter)
{
  if(!iter)
    return 1;
  
  while(1) { 
    if(iter->cursor)
      iter->cursor = iter->cursor->next;
    else
      iter->cursor = iter->dataset->head;
    
    if(!iter->cursor)
      break;
    
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 2
    RASQAL_DEBUG1("Matching against triple: ");
    rasqal_triple_print(iter->cursor->triple, DEBUG_FH);
    fputc('\n', DEBUG_FH);
#endif

    if(rasqal_raptor_triple_match(iter->dataset->world,
                                  iter->cursor->triple, &iter->match,
                                  iter->parts))
      return 0;
  }
  
  return 1;
}




/*
 * rasqal_dataset_get_sources_iterator:
 * @ds: dataset
 * @predicate: literal predicate
 * @object: literal object
 *
 * INTERNAL - Get the sources / subjects of all triples matching (?, @predicate, @object)
 *
 * Return value: iterator or NULL on error or failure
 */
rasqal_dataset_term_iterator*
rasqal_dataset_get_sources_iterator(rasqal_dataset* ds,
                                    rasqal_literal* predicate,
                                    rasqal_literal* object)
{
  if(!predicate || !object)
    return NULL;
  
  return rasqal_dataset_init_match_internal(ds, NULL, predicate, object);
}


/*
 * rasqal_dataset_get_targets_iterator:
 * @ds: dataset
 * @subject: literal subject
 * @object: literal object
 *
 * INTERNAL - Get the targets / objects of all triples matching (@subject, @predicate, ?)
 *
 * Return value: iterator or NULL on error or failure
 */
rasqal_dataset_term_iterator*
rasqal_dataset_get_targets_iterator(rasqal_dataset* ds,
                                    rasqal_literal* subject,
                                    rasqal_literal* predicate)
{
  if(!subject || !predicate)
    return NULL;
  
  return rasqal_dataset_init_match_internal(ds, subject, predicate, NULL);
}


/*
 * rasqal_dataset_get_source:
 * @ds: dataset
 * @predicate: literal predicate
 * @object: literal object
 *
 * INTERNAL - Get the first source / subject of the triple that matches (?, @predicate, @object)
 *
 * Return value: iterator or NULL on error or failure
 */
rasqal_literal*
rasqal_dataset_get_source(rasqal_dataset* ds,
                          rasqal_literal* predicate,
                          rasqal_literal* object)
{
  rasqal_literal *literal;
  
  rasqal_dataset_term_iterator* iter;
  
  iter = rasqal_dataset_get_sources_iterator(ds, predicate, object);
  if(!iter)
    return NULL;

  literal = rasqal_dataset_term_iterator_get(iter);

  rasqal_free_dataset_term_iterator(iter);
  return literal;
}


/*
 * rasqal_dataset_get_target:
 * @ds: dataset
 * @subject: literal subject
 * @object: literal object
 *
 * INTERNAL - Get the first target / object of the triple that matches (@subject, @predicate, ?)
 *
 * Return value: iterator or NULL on error or failure
 */
rasqal_literal*
rasqal_dataset_get_target(rasqal_dataset* ds,
                          rasqal_literal* subject,
                          rasqal_literal* predicate)
{
  rasqal_literal *literal;
  
  rasqal_dataset_term_iterator* iter;
  
  iter = rasqal_dataset_get_targets_iterator(ds, subject, predicate);
  if(!iter)
    return NULL;

  literal = rasqal_dataset_term_iterator_get(iter);

  rasqal_free_dataset_term_iterator(iter);
  return literal;
}

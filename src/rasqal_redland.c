/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_redland.c - Rasqal redland interface
 *
 * $Id$
 * 
 * FIXME This is presently part of rasqal but will move to be part of
 * redland before rasqal releases.
 *
 * Copyright (C) 2004 David Beckett - http://purl.org/net/dajobe/
 * Institute for Learning and Research Technology - http://www.ilrt.bris.ac.uk/
 * University of Bristol - http://www.bristol.ac.uk/
 * 
 * This package is Free Software or Open Source available under the
 * following licenses (these are alternatives):
 *   1. GNU Lesser General Public License (LGPL)
 *   2. GNU General Public License (GPL)
 *   3. Mozilla Public License (MPL)
 * 
 * See LICENSE.html or LICENSE.txt at the top of this package for the
 * full license terms.
 * 
 * 
 */

#ifdef HAVE_CONFIG_H
#include <rasqal_config.h>
#endif

#ifdef WIN32
#include <win32_config.h>
#endif

#include <stdio.h>
#include <string.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdarg.h>

#include "rasqal.h"
#include "rasqal_internal.h"


#include <redland.h>


/* FIXME GLOBAL */
librdf_world *World=NULL;


static librdf_node*
rasqal_expression_to_redland_node(librdf_world *world, rasqal_expression* e) {
  if(e->op == RASQAL_EXPR_LITERAL) {
    rasqal_literal* l=e->literal;
    if(l->type == RASQAL_LITERAL_URI) {
      char *uri_string=raptor_uri_as_string(l->value.uri);
      return librdf_new_node_from_uri_string(world, uri_string);
    } else if (l->type == RASQAL_LITERAL_STRING)
      return librdf_new_node_from_literal(world, l->value.string, NULL, 0);
    else if (l->type == RASQAL_LITERAL_BLANK)
      return librdf_new_node_from_blank_identifier(world, l->value.string);
    else {
      RASQAL_DEBUG2("Unknown literal type %d", l->type);
      abort();
    }
  } else {
    RASQAL_DEBUG2("Unknown expr op %d", e->op);
    abort();
  }

  return NULL;
}


static rasqal_expression*
redland_node_to_rasqal_expression(librdf_node *node) {
  rasqal_literal* l;
  
  if(librdf_node_is_resource(node)) {
    raptor_uri* uri=raptor_new_uri(librdf_uri_as_string(librdf_node_get_uri(node)));
    l=rasqal_new_literal(RASQAL_LITERAL_URI, 0, 0.0, NULL, uri);
  } else if(librdf_node_is_literal(node)) {
    char *new_string=strdup(librdf_node_get_literal_value(node));
    l=rasqal_new_literal(RASQAL_LITERAL_STRING, 0, 0.0, new_string, NULL);
  } else {
    char *new_string=strdup(librdf_node_get_blank_identifier(node));
    l=rasqal_new_literal(RASQAL_LITERAL_BLANK, 0, 0.0, new_string, NULL);
  }

  return rasqal_new_literal_expression(l);
}


static char*
rasqal_redland_uri_heuristic_parser_name(librdf_uri *uri) {
  char *uri_string;
  size_t len;

  uri_string=librdf_uri_as_counted_string(uri, &len);
  if(!strncmp(uri_string+len-3, ".nt", 3))
    return "ntriples";
  
  if(!strncmp(uri_string+len-3, ".n3", 3))
    return "turtle";

  return "rdfxml";
}


typedef struct {
  librdf_model *model;
  librdf_storage *storage;
  librdf_uri *uri;
} rasqal_redland_triples_source_user_data;

/* prototypes */
static rasqal_triples_match* rasqal_redland_new_triples_match(rasqal_triples_source *rts, void *user_data, rasqal_triple_meta *m, rasqal_triple *t);
static int rasqal_redland_triple_present(rasqal_triples_source *rts, void *user_data, rasqal_triple *t);
static void rasqal_redland_free_triples_source(void *user_data);

static int
rasqal_redland_new_triples_source(rasqal_query* rdf_query, void *user_data,
                                  rasqal_triples_source *rts) {
  rasqal_redland_triples_source_user_data* rtsc=(rasqal_redland_triples_source_user_data*)user_data;
  librdf_parser *parser;
  char *parser_name;
  
  /* FIXME error checking */
  rtsc->uri=librdf_new_uri(World, raptor_uri_as_string(rts->uri));
  rtsc->storage = librdf_new_storage(World, NULL, NULL, NULL);
  rtsc->model = librdf_new_model(World, rtsc->storage, NULL);

  rts->new_triples_match=rasqal_redland_new_triples_match;
  rts->triple_present=rasqal_redland_triple_present;
  rts->free_triples_source=rasqal_redland_free_triples_source;

  parser_name=rasqal_redland_uri_heuristic_parser_name(rtsc->uri);
  parser=librdf_new_parser(World, parser_name, NULL, NULL);
  librdf_parser_parse_into_model(parser, rtsc->uri, NULL, rtsc->model);
  librdf_free_parser(parser);

  return 0;
}


static int
rasqal_redland_triple_present(rasqal_triples_source *rts, void *user_data, 
                              rasqal_triple *t) 
{
  rasqal_redland_triples_source_user_data* rtsc=(rasqal_redland_triples_source_user_data*)user_data;
  librdf_node* nodes[3];
  librdf_statement *s;

  /* ASSUMPTION: all the parts of the triple are not variables */
  /* FIXME: and no error checks */
  nodes[0]=rasqal_expression_to_redland_node(World, t->subject);
  nodes[1]=rasqal_expression_to_redland_node(World, t->predicate);
  nodes[2]=rasqal_expression_to_redland_node(World, t->object);

  s=librdf_new_statement_from_nodes(World, nodes[0], nodes[1], nodes[2]);
  
  int rc=!librdf_model_contains_statement(rtsc->model, s);
  librdf_free_statement(s);
  return rc;
}



static void
rasqal_redland_free_triples_source(void *user_data) {
  rasqal_redland_triples_source_user_data* rtsc=(rasqal_redland_triples_source_user_data*)user_data;

  librdf_free_uri(rtsc->uri);
  librdf_free_model(rtsc->model);
  librdf_free_storage(rtsc->storage);

  RASQAL_FREE(rasqal_redland_triples_source_context, rtsc);
}


static void
rasqal_redland_register_triples_source_factory(rasqal_triples_source_factory *factory) 
{
  factory->user_data_size=sizeof(rasqal_redland_triples_source_user_data);
  factory->new_triples_source=rasqal_redland_new_triples_source;
}


typedef struct {
  librdf_node* nodes[3];
  /* query statement, made from the nodes above (even when exact) */
  librdf_statement *qstatement;
  librdf_stream *stream;
} rasqal_redland_triples_match_context;


static int
rasqal_redland_bind_match(struct rasqal_triples_match_s* rtm,
                          void *user_data,
                          rasqal_variable* bindings[3]) 
{
  rasqal_redland_triples_match_context* rtmc=(rasqal_redland_triples_match_context*)rtm->user_data;
  librdf_statement* statement=librdf_stream_get_object(rtmc->stream);
  if(!statement)
    return 1;
  
#ifdef RASQAL_DEBUG
  RASQAL_DEBUG1("  matched statement ");
  librdf_statement_print(statement, stderr);
  fputc('\n', stderr);
#endif

  /* set 1 or 2 variable values from the fields of statement */

  if(bindings[0]) {
    RASQAL_DEBUG1("binding subject to variable\n");
    rasqal_variable_set_value(bindings[0],
                              redland_node_to_rasqal_expression(librdf_statement_get_subject(statement)));
  }

  if(bindings[1]) {
    RASQAL_DEBUG1("binding predicate to variable\n");
    rasqal_variable_set_value(bindings[1], 
                              redland_node_to_rasqal_expression(librdf_statement_get_predicate(statement)));
  }

  if(bindings[2]) {
    RASQAL_DEBUG1("binding object to variable\n");
    rasqal_variable_set_value(bindings[2],  
                              redland_node_to_rasqal_expression(librdf_statement_get_object(statement)));
  }

  return 0;
}


static void
rasqal_redland_next_match(struct rasqal_triples_match_s* rtm,
                          void *user_data)
{
  rasqal_redland_triples_match_context* rtmc=(rasqal_redland_triples_match_context*)rtm->user_data;

  librdf_stream_next(rtmc->stream);
}

static int
rasqal_redland_is_end(struct rasqal_triples_match_s* rtm,
                      void *user_data)
{
  rasqal_redland_triples_match_context* rtmc=(rasqal_redland_triples_match_context*)rtm->user_data;

  return librdf_stream_end(rtmc->stream);
}


static void
rasqal_redland_finish_triples_match(struct rasqal_triples_match_s* rtm,
                                    void *user_data) {
  rasqal_redland_triples_match_context* rtmc=(rasqal_redland_triples_match_context*)rtm->user_data;

  if(rtmc->stream) {
    librdf_free_stream(rtmc->stream);
    rtmc->stream=NULL;
  }
  librdf_free_statement(rtmc->qstatement);
}


static rasqal_triples_match*
rasqal_redland_new_triples_match(rasqal_triples_source *rts, void *user_data,
                                 rasqal_triple_meta *m, rasqal_triple *t) {
  rasqal_redland_triples_source_user_data* rtsc=(rasqal_redland_triples_source_user_data*)user_data;
  rasqal_triples_match *rtm;
  rasqal_redland_triples_match_context* rtmc;
  rasqal_variable* var;

  rtm=(rasqal_triples_match *)RASQAL_CALLOC(rasqal_triples_match, sizeof(rasqal_triples_match), 1);
  rtm->bind_match=rasqal_redland_bind_match;
  rtm->next_match=rasqal_redland_next_match;
  rtm->is_end=rasqal_redland_is_end;
  rtm->finish=rasqal_redland_finish_triples_match;

  rtmc=(rasqal_redland_triples_match_context*)RASQAL_CALLOC(rasqal_redland_triples_match_context, sizeof(rasqal_redland_triples_match_context), 1);

  rtm->user_data=rtmc;


  /* at least one of the triple terms is a variable and we need to
   * do a triplesMatching() aka librdf_model_find_statements
   *
   * redland find_statements will do the right thing and internally
   * pick the most efficient, indexed way to get the answer.
   */

  if((var=rasqal_expression_as_variable(t->subject))) {
    if(var->value)
      rtmc->nodes[0]=rasqal_expression_to_redland_node(World, var->value);
    else
      rtmc->nodes[0]=NULL;
  } else
    rtmc->nodes[0]=rasqal_expression_to_redland_node(World, t->subject);

  m->bindings[0]=var;
  

  if((var=rasqal_expression_as_variable(t->predicate))) {
    if(var->value)
      rtmc->nodes[1]=rasqal_expression_to_redland_node(World, var->value);
    else
      rtmc->nodes[1]=NULL;
  } else
    rtmc->nodes[1]=rasqal_expression_to_redland_node(World, t->predicate);

  m->bindings[1]=var;
  

  if((var=rasqal_expression_as_variable(t->object))) {
    if(var->value)
      rtmc->nodes[2]=rasqal_expression_to_redland_node(World, var->value);
    else
      rtmc->nodes[2]=NULL;
  } else
    rtmc->nodes[2]=rasqal_expression_to_redland_node(World, t->object);

  m->bindings[2]=var;
  

  rtmc->qstatement=librdf_new_statement_from_nodes(World, 
                                                   rtmc->nodes[0],
                                                   rtmc->nodes[1], 
                                                   rtmc->nodes[2]);
  if(!rtmc->qstatement)
    return NULL;

#ifdef RASQAL_DEBUG
  RASQAL_DEBUG1("query statement: ");
  librdf_statement_print(rtmc->qstatement, stderr);
  fputc('\n', stderr);
#endif
  
  rtmc->stream=librdf_model_find_statements(rtsc->model, rtmc->qstatement);

  RASQAL_DEBUG1("rasqal_new_triples_match done\n");

  return rtm;
}


void
rasqal_redland_init(void) {
  rasqal_set_triples_source_factory(rasqal_redland_register_triples_source_factory);
}


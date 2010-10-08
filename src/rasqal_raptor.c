/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_raptor.c - Rasqal triple store implementation with raptor
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


struct rasqal_raptor_triple_s {
  struct rasqal_raptor_triple_s *next;
  rasqal_triple *triple;
};

typedef struct rasqal_raptor_triple_s rasqal_raptor_triple;

typedef struct {
  rasqal_query* query;

  rasqal_raptor_triple *head;
  rasqal_raptor_triple *tail;

  /* index used while reading triples into the two arrays below.
   * This is used to connect a triple to the URI literal of the source
   */
  int source_index;

  /* size of the two arrays below */
  int sources_count;
  
  /* shared pointers into query->data_graph uris */
  raptor_uri* source_uri;

  /* array of URI literals (allocated here) */
  rasqal_literal **source_literals;

  /* genid base for mapping user bnodes */
  unsigned char* mapped_id_base;
  /* length of above string */
  int mapped_id_base_len;
} rasqal_raptor_triples_source_user_data;


/* prototypes */
static int rasqal_raptor_init_triples_match(rasqal_triples_match* rtm, rasqal_triples_source *rts, void *user_data, rasqal_triple_meta *m, rasqal_triple *t);
static int rasqal_raptor_triple_present(rasqal_triples_source *rts, void *user_data, rasqal_triple *t);
static void rasqal_raptor_free_triples_source(void *user_data);


#ifndef HAVE_RAPTOR2_API
static raptor_uri* 
ordinal_as_uri(rasqal_world* world, int ordinal) 
{
  int t = ordinal;
  size_t len; 
  unsigned char *buffer;
  raptor_uri* uri;
  
  len = raptor_rdf_namespace_uri_len + 1 + 1; /* 1 for min-length, 1 for '_' */
  while(t /= 10)
    len++;
  buffer = (unsigned char*)RASQAL_MALLOC(cstring, len + 1);
  if(!buffer)
    return NULL;
  
  sprintf((char*)buffer, "%s_%d", raptor_rdf_namespace_uri, ordinal);
  uri = raptor_new_uri(world->raptor_world_ptr, buffer);
  RASQAL_FREE(cstring, buffer);

  return uri;
}
#endif


#ifdef HAVE_RAPTOR2_API
static rasqal_triple*
raptor_statement_as_rasqal_triple(rasqal_world* world,
                                  const raptor_statement *statement)
{
  rasqal_literal *s, *p, *o;
  raptor_uri *uri;
  unsigned char *new_str;
  size_t len;

  /* subject */
  if(statement->subject->type == RAPTOR_TERM_TYPE_BLANK) {
    len = strlen((char*)statement->subject->value.blank.string);
    new_str = (unsigned char*)RASQAL_MALLOC(cstring, len + 1);
    memcpy(new_str, statement->subject->value.blank.string, len + 1);
    s = rasqal_new_simple_literal(world, RASQAL_LITERAL_BLANK, new_str);
  } else {
    uri = raptor_uri_copy((raptor_uri*)statement->subject->value.uri);
    s = rasqal_new_uri_literal(world, uri);
  }

  /* predicate */
  uri = raptor_uri_copy((raptor_uri*)statement->predicate->value.uri);
  p = rasqal_new_uri_literal(world, uri);
  
  /* object */
  if(statement->object->type == RAPTOR_TERM_TYPE_LITERAL) {
    char *language = NULL;

    len = strlen((char*)statement->object->value.literal.string);
    new_str = (unsigned char*)RASQAL_MALLOC(cstring, len + 1);
    memcpy(new_str, statement->object->value.literal.string, len + 1);

    if(statement->object->value.literal.language) {
      len = strlen((const char*)statement->object->value.literal.language);
      language = (char*)RASQAL_MALLOC(cstring, len + 1);
      memcpy(language, statement->object->value.literal.language, len + 1);
    }

    if(statement->object->value.literal.datatype)
      uri = raptor_uri_copy(statement->object->value.literal.datatype);
    else
      uri = NULL;

    o = rasqal_new_string_literal(world, new_str, language, uri, NULL);
  } else if(statement->object->type == RAPTOR_TERM_TYPE_BLANK) {
    len = strlen((char*)statement->object->value.blank.string);
    new_str = (unsigned char*)RASQAL_MALLOC(cstring, len + 1);
    memcpy(new_str, statement->object->value.blank.string, len + 1);
    o = rasqal_new_simple_literal(world, RASQAL_LITERAL_BLANK, new_str);
  } else {
    uri = raptor_uri_copy((raptor_uri*)statement->object->value.uri);
    o = rasqal_new_uri_literal(world, uri);
  }

  return rasqal_new_triple(s, p, o);
}
#else /* ifdef HAVE_RAPTOR_API */
static rasqal_triple*
raptor_statement_as_rasqal_triple(rasqal_world* world,
                                  const raptor_statement *statement)
{
  rasqal_literal *s, *p, *o;

  if(statement->subject_type == RAPTOR_IDENTIFIER_TYPE_ANONYMOUS) {
    unsigned char *new_blank;
    size_t blank_len;

    blank_len = strlen((char*)statement->subject);
    new_blank = (unsigned char*)RASQAL_MALLOC(cstring, blank_len + 1);
    memcpy(new_blank, statement->subject, blank_len + 1);
    s = rasqal_new_simple_literal(world, RASQAL_LITERAL_BLANK, new_blank);
  } else if(statement->subject_type == RAPTOR_IDENTIFIER_TYPE_ORDINAL) {
    raptor_uri* uri;

    uri = ordinal_as_uri(world, *((int*)statement->subject));
    if(!uri)
      return NULL;
    s = rasqal_new_uri_literal(world, uri);
  } else {
    raptor_uri *uri;

    uri = raptor_uri_copy((raptor_uri*)statement->subject);
    s = rasqal_new_uri_literal(world, uri);
  }

  if(statement->predicate_type == RAPTOR_IDENTIFIER_TYPE_ORDINAL) {
    raptor_uri* uri = ordinal_as_uri(world, *((int*)statement->predicate));

    if(!uri)
      return NULL;
    p = rasqal_new_uri_literal(world, uri);
  } else {
    raptor_uri *uri;

    uri = raptor_uri_copy((raptor_uri*)statement->predicate);
    p = rasqal_new_uri_literal(world, uri);
  }
  
  if(statement->object_type == RAPTOR_IDENTIFIER_TYPE_LITERAL || 
     statement->object_type == RAPTOR_IDENTIFIER_TYPE_XML_LITERAL) {
    unsigned char *string;
    char *language = NULL;
    raptor_uri *uri = NULL;
    size_t literal_len;

    literal_len = strlen((char*)statement->object);
    string = (unsigned char*)RASQAL_MALLOC(cstring, literal_len + 1);
    memcpy(string, statement->object, literal_len + 1);

    if(statement->object_literal_language) {
      size_t language_len;
      language_len = strlen((const char*)statement->object_literal_language);
      language = (char*)RASQAL_MALLOC(cstring, language_len + 1);
      memcpy(language, statement->object_literal_language, language_len + 1);
    }

    if(statement->object_type == RAPTOR_IDENTIFIER_TYPE_XML_LITERAL) {
      const unsigned char* dt_uri_string;

      dt_uri_string = (const unsigned char*)raptor_xml_literal_datatype_uri_string;
      uri = raptor_new_uri(world->raptor_world_ptr, dt_uri_string);
    } else if(statement->object_literal_datatype) {
      raptor_uri *dt_uri = (raptor_uri*)statement->object_literal_datatype;

      uri = raptor_uri_copy(dt_uri);
    }
    o = rasqal_new_string_literal(world, string, language, uri, NULL);
  } else if(statement->object_type == RAPTOR_IDENTIFIER_TYPE_ANONYMOUS) {
    char *blank = (char*)statement->object;
    unsigned char *new_blank;
    size_t blank_len;

    blank_len = strlen(blank);
    new_blank = (unsigned char*)RASQAL_MALLOC(cstring, blank_len + 1);
    memcpy(new_blank, blank, blank_len + 1);
    o = rasqal_new_simple_literal(world, RASQAL_LITERAL_BLANK, new_blank);
  } else if(statement->object_type == RAPTOR_IDENTIFIER_TYPE_ORDINAL) {
    raptor_uri* uri = ordinal_as_uri(world, *((int*)statement->object));

    if(!uri)
      return NULL;
    o = rasqal_new_uri_literal(world, uri);
  } else {
    raptor_uri *uri;
    uri = raptor_uri_copy((raptor_uri*)statement->object);
    o = rasqal_new_uri_literal(world, uri);
  }

  return rasqal_new_triple(s, p, o);
}
#endif /* ifdef HAVE_RAPTOR2_API ... else */


static void
rasqal_raptor_statement_handler(void *user_data,
#ifndef HAVE_RAPTOR2_API
                                const
#endif
                                raptor_statement *statement)
{
  rasqal_raptor_triples_source_user_data* rtsc;
  rasqal_raptor_triple *triple;
  
  rtsc = (rasqal_raptor_triples_source_user_data*)user_data;

  triple = (rasqal_raptor_triple*)RASQAL_MALLOC(rasqal_raptor_triple,
                                                sizeof(rasqal_raptor_triple));
  triple->next = NULL;
  triple->triple = raptor_statement_as_rasqal_triple(rtsc->query->world,
                                                     statement);

  /* this origin URI literal is shared amongst the triples and
   * freed only in rasqal_raptor_free_triples_source
   */
  rasqal_triple_set_origin(triple->triple, 
                           rtsc->source_literals[rtsc->source_index]);

  if(rtsc->tail)
    rtsc->tail->next = triple;
  else
    rtsc->head = triple;

  rtsc->tail = triple;
}


#ifndef HAVE_RAPTOR2_API
static void
rasqal_raptor_error_handler(void *user_data, 
                            raptor_locator* locator, const char *message)
{
  rasqal_query* query = (rasqal_query*)user_data;
  int locator_len;

  query->failed = 1;

  if(locator &&
#ifdef HAVE_RAPTOR2_API
     (locator_len = raptor_locator_format(NULL, 0, locator)) > 0
#else
     (locator_len = raptor_format_locator(NULL, 0, locator)) > 0
#endif
    ) {
    char *buffer = (char*)RASQAL_MALLOC(cstring, locator_len + 1);
#ifdef HAVE_RAPTOR2_API
    raptor_locator_format(buffer, locator_len, locator);
#else
    raptor_format_locator(buffer, locator_len, locator);
#endif

    rasqal_log_error_simple(query->world, RAPTOR_LOG_LEVEL_ERROR,
                            &query->locator,
                            "Failed to parse %s - %s", buffer, message);
    RASQAL_FREE(cstring, buffer);
  } else
    rasqal_log_error_simple(query->world, RAPTOR_LOG_LEVEL_ERROR,
                            &query->locator,
                            "Failed to parse - %s", message);
}
#endif


static unsigned char*
rasqal_raptor_generate_id_handler(void *user_data,
#ifdef HAVE_RAPTOR2_API
#else
                                  raptor_genid_type type,
#endif
                                  unsigned char *user_bnodeid) 
{
  rasqal_raptor_triples_source_user_data* rtsc;

  rtsc = (rasqal_raptor_triples_source_user_data*)user_data;

  if(user_bnodeid) {
    unsigned char *mapped_id;
    size_t user_bnodeid_len = strlen((const char*)user_bnodeid);
    
    mapped_id = (unsigned char*)RASQAL_MALLOC(cstring, 
                                              rtsc->mapped_id_base_len + 1 + 
                                              user_bnodeid_len + 1);
    memcpy(mapped_id, rtsc->mapped_id_base, rtsc->mapped_id_base_len);
    mapped_id[rtsc->mapped_id_base_len] = '_';
    memcpy(mapped_id + rtsc->mapped_id_base_len + 1,
           user_bnodeid, user_bnodeid_len + 1);

    raptor_free_memory(user_bnodeid);
    return mapped_id;
  }
  
  return rasqal_query_get_genid(rtsc->query, (const unsigned char*)"genid", -1);
}


static int
rasqal_raptor_support_feature(void *user_data,
                              rasqal_triple_source_feature feature)
{
  switch(feature) {
    case RASQAL_TRIPLE_SOURCE_FEATURE_IOSTREAM_DATA_GRAPH:
      return 1;
      break;
      
    default:
    case RASQAL_TRIPLE_SOURCE_FEATURE_NONE:
      return 0;
  }
}


static int
rasqal_raptor_init_triples_source(rasqal_query* rdf_query,
                                  void *factory_user_data,
                                  void *user_data,
                                  rasqal_triples_source *rts,
                                  rasqal_triples_error_handler handler)
{
  rasqal_raptor_triples_source_user_data* rtsc;
  raptor_parser *parser;
  int i;

  rtsc = (rasqal_raptor_triples_source_user_data*)user_data;

  if(!rdf_query->data_graphs) {
    if(handler)
      handler(rdf_query, /* locator */ NULL, "Query has no data graphs.");
    return -1;  /* no data */
  }

  /* Max API version this triples source generates */
  rts->version = 2;
  
  rts->init_triples_match = rasqal_raptor_init_triples_match;
  rts->triple_present = rasqal_raptor_triple_present;
  rts->free_triples_source = rasqal_raptor_free_triples_source;
  rts->support_feature = rasqal_raptor_support_feature;

  rtsc->sources_count = raptor_sequence_size(rdf_query->data_graphs);
  /* no default triple source possible */
  if(!rtsc->sources_count) {
    if(handler)
      handler(rdf_query, /* locator */ NULL, "Query has no data graphs.");
    return -1;  /* no data */
  }

  rtsc->source_literals = (rasqal_literal**)RASQAL_CALLOC(rasqal_literal_ptr,
                                                          rtsc->sources_count,
                                                          sizeof(rasqal_literal*));

  rtsc->query = rdf_query;

  for(i = 0; i < rtsc->sources_count; i++) {
    rasqal_data_graph *dg;
    raptor_uri* uri;
    raptor_uri* name_uri;
    int free_name_uri = 0;
    const char* parser_name;
    
    dg = (rasqal_data_graph*)raptor_sequence_get_at(rdf_query->data_graphs, i);
    uri = dg->uri;
    name_uri = dg->name_uri;

    rtsc->source_index = i;
    rtsc->source_uri = raptor_uri_copy(uri);

    if(name_uri)
      rtsc->source_literals[i] = rasqal_new_uri_literal(rdf_query->world, 
                                                        raptor_uri_copy(name_uri)
                                                        );
    else {
      name_uri = raptor_uri_copy(uri);
      free_name_uri = 1;
    }

    rtsc->mapped_id_base = rasqal_query_get_genid(rdf_query,
                                                  (const unsigned char*)"graphid",
                                                  i);
    rtsc->mapped_id_base_len = strlen((const char*)rtsc->mapped_id_base);

    parser_name = dg->format_name;
    if(parser_name) {
      if(!raptor_world_is_parser_name(rdf_query->world->raptor_world_ptr,
                                      parser_name)) {
        handler(rdf_query, /* locator */ NULL,
                "Invalid data graph parser name ignored");
        parser_name = NULL;
      }
    }
    if(!parser_name)
      parser_name = "guess";
    
#ifdef HAVE_RAPTOR2_API
    parser = raptor_new_parser(rdf_query->world->raptor_world_ptr, parser_name);
    raptor_parser_set_statement_handler(parser, rtsc, rasqal_raptor_statement_handler);
    raptor_world_set_generate_bnodeid_handler(rdf_query->world->raptor_world_ptr,
                                              rtsc,
                                              rasqal_raptor_generate_id_handler);
#else
    parser = raptor_new_parser(parser_name);
    raptor_set_statement_handler(parser, rtsc, rasqal_raptor_statement_handler);
    raptor_set_error_handler(parser, rdf_query, rasqal_raptor_error_handler);
    raptor_set_generate_id_handler(parser, rtsc,
                                   rasqal_raptor_generate_id_handler);
#endif

#ifdef RAPTOR_FEATURE_NO_NET
    if(rdf_query->features[RASQAL_FEATURE_NO_NET])
      raptor_set_feature(parser, RAPTOR_FEATURE_NO_NET,
                         rdf_query->features[RASQAL_FEATURE_NO_NET]);
#endif

#ifdef HAVE_RAPTOR2_API
    raptor_parser_parse_uri(parser, uri, name_uri);
#else
    raptor_parse_uri(parser, uri, name_uri);
#endif
    raptor_free_parser(parser);

    raptor_free_uri(rtsc->source_uri);

    if(free_name_uri)
      raptor_free_uri(name_uri);

    /* This is freed in rasqal_raptor_free_triples_source() */
    /* rasqal_free_literal(rtsc->source_literal); */
    RASQAL_FREE(cstring, rtsc->mapped_id_base);
    
    if(rdf_query->failed) {
      rasqal_raptor_free_triples_source(user_data);
      break;
    }
  }

  return rdf_query->failed;
}


static int
rasqal_raptor_new_triples_source(rasqal_query* rdf_query,
                                 void *factory_user_data,
                                 void *user_data,
                                 rasqal_triples_source *rts)
{
  return rasqal_raptor_init_triples_source(rdf_query, factory_user_data,
                                           user_data, rts,
                                           rasqal_triples_source_error_handler);
}


/**
 * rasqal_raptor_triple_match:
 * @world: rasqal_world object
 * @triple: #rasqal_triple to match against
 * @match: #rasqal_triple with wildcards
 * @parts; parts of the triple to match
 * .
 * 
 * Match a rasqal_triple against a rasqal_triple with NULL
 * signifying wildcard fields in the rasqal_triple.
 * 
 * Return value: non-0 on match
 **/
static int
rasqal_raptor_triple_match(rasqal_world* world,
                           rasqal_triple *triple,
                           rasqal_triple *match,
                           rasqal_triple_parts parts)
{
  int rc = 0;
  
#if RASQAL_DEBUG > 1
  RASQAL_DEBUG1("\ntriple ");
  rasqal_triple_print(triple, stderr);
  fputs("\nmatch  ", stderr);
  rasqal_triple_print(match, stderr);
  fputs("\n", stderr);
#endif

  if(match->subject && (parts & RASQAL_TRIPLE_SUBJECT)) {
    if(!rasqal_literal_equals_flags(triple->subject, match->subject,
                                    RASQAL_COMPARE_RDF, NULL))
      goto done;
  }

  if(match->predicate && (parts & RASQAL_TRIPLE_PREDICATE)) {
    if(!rasqal_literal_equals_flags(triple->predicate, match->predicate,
                                    RASQAL_COMPARE_RDF, NULL))
      goto done;
  }

  if(match->object && (parts & RASQAL_TRIPLE_OBJECT)) {
    if(!rasqal_literal_equals_flags(triple->object, match->object,
                                    RASQAL_COMPARE_RDF, NULL))
      goto done;
  }

  if(parts & RASQAL_TRIPLE_ORIGIN) {
    /* Binding a graph */
    
    /* If expecting a graph and triple has none then no match */
    if(!triple->origin)
      goto done;
      
    if(match->origin) {
      if(match->origin->type == RASQAL_LITERAL_URI ) {
        raptor_uri* triple_uri = triple->origin->value.uri;
        raptor_uri* match_uri = match->origin->value.uri;
        if(!raptor_uri_equals(triple_uri, match_uri))
          goto done;
      }
    }
    
  } else {
    /* Not binding a graph */
    
    /* If triple has a GRAPH and there is none in the triple pattern, no match */
    if(triple->origin)
      goto done;
  }
  
  rc = 1;
  done:
  
#if RASQAL_DEBUG > 1
  RASQAL_DEBUG2("result: %s\n", (rc ? "match" : "no match"));
#endif

  return rc;
}


/* non-0 if present */
static int
rasqal_raptor_triple_present(rasqal_triples_source *rts, void *user_data, 
                             rasqal_triple *t) 
{
  rasqal_raptor_triples_source_user_data* rtsc;
  rasqal_raptor_triple *triple;
  rasqal_triple_parts parts = RASQAL_TRIPLE_SPO;
  
  rtsc = (rasqal_raptor_triples_source_user_data*)user_data;

  if(t->origin)
    parts = (rasqal_triple_parts)(parts | RASQAL_TRIPLE_GRAPH);

  for(triple = rtsc->head; triple; triple = triple->next) {
    if(rasqal_raptor_triple_match(rtsc->query->world, triple->triple, t, parts))
      return 1;
  }

  return 0;
}



static void
rasqal_raptor_free_triples_source(void *user_data)
{
  rasqal_raptor_triples_source_user_data* rtsc;
  rasqal_raptor_triple *cur;
  int i;

  rtsc = (rasqal_raptor_triples_source_user_data*)user_data;
  cur = rtsc->head;
  while(cur) {
    rasqal_raptor_triple *next = cur->next;
    rasqal_triple_set_origin(cur->triple, NULL); /* shared URI literal */
    rasqal_free_triple(cur->triple);
    RASQAL_FREE(rasqal_raptor_triple, cur);
    cur = next;
  }

  for(i = 0; i < rtsc->sources_count; i++) {
    if(rtsc->source_literals[i])
      rasqal_free_literal(rtsc->source_literals[i]);
  }
  RASQAL_FREE(raptor_literal_ptr, rtsc->source_literals);
}



static int
rasqal_raptor_register_triples_source_factory(rasqal_triples_source_factory *factory) 
{
  factory->version = 2;
  factory->user_data_size = sizeof(rasqal_raptor_triples_source_user_data);
  /* V1 */
  factory->new_triples_source = rasqal_raptor_new_triples_source;
  /* V2 */
  factory->init_triples_source = rasqal_raptor_init_triples_source;

  return 0;
}


typedef struct {
  rasqal_raptor_triple *cur;
  rasqal_raptor_triples_source_user_data* source_context;
  rasqal_triple match;

  /* parts of the triple above to match: always (S,P,O) sometimes C */
  rasqal_triple_parts parts;
} rasqal_raptor_triples_match_context;


static rasqal_triple_parts
rasqal_raptor_bind_match(struct rasqal_triples_match_s* rtm,
                         void *user_data,
                         rasqal_variable* bindings[4],
                         rasqal_triple_parts parts)
{
  rasqal_raptor_triples_match_context* rtmc;
  int error = 0;
  rasqal_triple_parts result = (rasqal_triple_parts)0;
  
  rtmc = (rasqal_raptor_triples_match_context*)rtm->user_data;

#ifdef RASQAL_DEBUG
  if(rtmc->cur) {
    RASQAL_DEBUG1("  matched statement ");
    rasqal_triple_print(rtmc->cur->triple, stderr);
    fputc('\n', stderr);
  } else
    RASQAL_FATAL1("  matched NO statement - BUG\n");
#endif

  /* set variable values from the fields of statement */

  if(bindings[0] && (parts & RASQAL_TRIPLE_SUBJECT)) {
    rasqal_literal *l = rtmc->cur->triple->subject;
    RASQAL_DEBUG1("binding subject to variable\n");
    rasqal_variable_set_value(bindings[0], rasqal_new_literal_from_literal(l));
    result = RASQAL_TRIPLE_SUBJECT;
  }

  if(bindings[1] && (parts & RASQAL_TRIPLE_PREDICATE)) {
    if(bindings[0] == bindings[1]) {
      if(!rasqal_literal_equals_flags(rtmc->cur->triple->subject,
                                      rtmc->cur->triple->predicate,
                                      RASQAL_COMPARE_RDF, &error))
        return (rasqal_triple_parts)0;
      if(error)
        return (rasqal_triple_parts)0;
      
      RASQAL_DEBUG1("subject and predicate values match\n");
    } else {
      rasqal_literal *l = rtmc->cur->triple->predicate;
      RASQAL_DEBUG1("binding predicate to variable\n");
      rasqal_variable_set_value(bindings[1], rasqal_new_literal_from_literal(l));
      result = (rasqal_triple_parts)(result | RASQAL_TRIPLE_PREDICATE);
    }
  }

  if(bindings[2] && (parts & RASQAL_TRIPLE_OBJECT)) {
    int bind = 1;
    
    if(bindings[0] == bindings[2]) {
      if(!rasqal_literal_equals_flags(rtmc->cur->triple->subject,
                                      rtmc->cur->triple->object,
                                      RASQAL_COMPARE_RDF, &error))
        return (rasqal_triple_parts)0;
      if(error)
        return (rasqal_triple_parts)0;

      bind = 0;
      RASQAL_DEBUG1("subject and object values match\n");
    }
    if(bindings[1] == bindings[2] &&
       !(bindings[0] == bindings[1]) /* don't do this check if ?x ?x ?x */
       ) {
      if(!rasqal_literal_equals_flags(rtmc->cur->triple->predicate,
                                      rtmc->cur->triple->object,
                                      RASQAL_COMPARE_RDF, &error))
        return (rasqal_triple_parts)0;
      if(error)
        return (rasqal_triple_parts)0;

      bind = 0;
      RASQAL_DEBUG1("predicate and object values match\n");
    }
    
    if(bind) {
      rasqal_literal *l = rtmc->cur->triple->object;
      RASQAL_DEBUG1("binding object to variable\n");
      rasqal_variable_set_value(bindings[2], rasqal_new_literal_from_literal(l));
      result = (rasqal_triple_parts)(result | RASQAL_TRIPLE_OBJECT);
    }
  }

  if(bindings[3] && (parts & RASQAL_TRIPLE_ORIGIN)) {
    rasqal_literal *l;
    l = rasqal_new_literal_from_literal(rtmc->cur->triple->origin);
    RASQAL_DEBUG1("binding origin to variable\n");
    rasqal_variable_set_value(bindings[3], l);
    result = (rasqal_triple_parts)(result | RASQAL_TRIPLE_ORIGIN);
  }

  return result;
}



static void
rasqal_raptor_next_match(struct rasqal_triples_match_s* rtm, void *user_data)
{
  rasqal_raptor_triples_match_context* rtmc;

  rtmc = (rasqal_raptor_triples_match_context*)rtm->user_data;

  while(rtmc->cur) {
    rtmc->cur = rtmc->cur->next;
    if(rtmc->cur &&
       rasqal_raptor_triple_match(rtm->world, rtmc->cur->triple, &rtmc->match,
                                  rtmc->parts))
      break;
  }
}

static int
rasqal_raptor_is_end(struct rasqal_triples_match_s* rtm, void *user_data)
{
  rasqal_raptor_triples_match_context* rtmc;

  rtmc = (rasqal_raptor_triples_match_context*)rtm->user_data;

  return !rtmc || rtmc->cur == NULL;
}


static void
rasqal_raptor_finish_triples_match(struct rasqal_triples_match_s* rtm,
                                   void *user_data)
{
  rasqal_raptor_triples_match_context* rtmc;

  rtmc = (rasqal_raptor_triples_match_context*)rtm->user_data;

  if(rtmc->match.subject)
    rasqal_free_literal(rtmc->match.subject);

  if(rtmc->match.predicate)
    rasqal_free_literal(rtmc->match.predicate);

  if(rtmc->match.object)
    rasqal_free_literal(rtmc->match.object);

  if(rtmc->match.origin)
    rasqal_free_literal(rtmc->match.origin);

  RASQAL_FREE(rasqal_raptor_triples_match_context, rtmc);
}


static int
rasqal_raptor_init_triples_match(rasqal_triples_match* rtm,
                                 rasqal_triples_source *rts, void *user_data,
                                 rasqal_triple_meta *m, rasqal_triple *t)
{
  rasqal_raptor_triples_source_user_data* rtsc;
  rasqal_raptor_triples_match_context* rtmc;
  rasqal_variable* var;

  rtsc = (rasqal_raptor_triples_source_user_data*)user_data;

  rtm->bind_match = rasqal_raptor_bind_match;
  rtm->next_match = rasqal_raptor_next_match;
  rtm->is_end = rasqal_raptor_is_end;
  rtm->finish = rasqal_raptor_finish_triples_match;

  rtmc = (rasqal_raptor_triples_match_context*)RASQAL_CALLOC(rasqal_raptor_triples_match_context, 1, sizeof(rasqal_raptor_triples_match_context));
  if(!rtmc)
    return -1;

  rtm->user_data = rtmc;

  rtmc->source_context = rtsc;
  rtmc->cur = rtsc->head;
  
  /* at least one of the triple terms is a variable and we need to
   * do a triplesMatching() over the list of stored raptor_statements
   */

  if((var = rasqal_literal_as_variable(t->subject))) {
    if(var->value)
      rtmc->match.subject = rasqal_new_literal_from_literal(var->value);
  } else
    rtmc->match.subject = rasqal_new_literal_from_literal(t->subject);

  m->bindings[0] = var;
  

  if((var = rasqal_literal_as_variable(t->predicate))) {
    if(var->value)
      rtmc->match.predicate = rasqal_new_literal_from_literal(var->value);
  } else
    rtmc->match.predicate = rasqal_new_literal_from_literal(t->predicate);

  m->bindings[1] = var;
  

  if((var = rasqal_literal_as_variable(t->object))) {
    if(var->value)
      rtmc->match.object = rasqal_new_literal_from_literal(var->value);
  } else
    rtmc->match.object = rasqal_new_literal_from_literal(t->object);

  m->bindings[2] = var;
  
  rtmc->parts = RASQAL_TRIPLE_SPO;

  if(t->origin) {
    if((var = rasqal_literal_as_variable(t->origin))) {
      if(var->value)
        rtmc->match.origin = rasqal_new_literal_from_literal(var->value);
    } else
      rtmc->match.origin = rasqal_new_literal_from_literal(t->origin);
    m->bindings[3] = var;
    rtmc->parts = (rasqal_triple_parts)(rtmc->parts | RASQAL_TRIPLE_GRAPH);
  }
  

  while(rtmc->cur) {
    if(rasqal_raptor_triple_match(rtm->world, rtmc->cur->triple, &rtmc->match,
                                  rtmc->parts))
      break;
    rtmc->cur = rtmc->cur->next;
  }
  
  return 0;
}


int
rasqal_raptor_init(rasqal_world* world)
{
  rasqal_set_triples_source_factory(world,
                                    rasqal_raptor_register_triples_source_factory,
                                    (void*)NULL);
  return 0;
}

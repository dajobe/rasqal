/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_raptor.c - Rasqal triple store implementation with raptor
 *
 * Copyright (C) 2004-2008, David Beckett http://www.dajobe.org/
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
#ifdef RAPTOR_V2_AVAILABLE
  uri = raptor_new_uri_v2(world->raptor_world_ptr, buffer);
#else
  uri = raptor_new_uri(buffer);
#endif
  RASQAL_FREE(cstring, buffer);

  return uri;
}

  
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
    strncpy((char*)new_blank, (const char*)statement->subject, blank_len + 1);
    s = rasqal_new_simple_literal(world, RASQAL_LITERAL_BLANK, new_blank);
  } else if(statement->subject_type == RAPTOR_IDENTIFIER_TYPE_ORDINAL) {
    raptor_uri* uri;

    uri = ordinal_as_uri(world, *((int*)statement->subject));
    if(!uri)
      return NULL;
    s = rasqal_new_uri_literal(world, uri);
  } else {
    raptor_uri *uri;

#ifdef RAPTOR_V2_AVAILABLE
    uri = raptor_uri_copy_v2(world->raptor_world_ptr,
                             (raptor_uri*)statement->subject);
#else
    uri = raptor_uri_copy((raptor_uri*)statement->subject);
#endif
    s = rasqal_new_uri_literal(world, uri);
  }

  if(statement->predicate_type == RAPTOR_IDENTIFIER_TYPE_ORDINAL) {
    raptor_uri* uri = ordinal_as_uri(world, *((int*)statement->predicate));

    if(!uri)
      return NULL;
    p = rasqal_new_uri_literal(world, uri);
  } else {
    raptor_uri *uri;

#ifdef RAPTOR_V2_AVAILABLE
    uri = raptor_uri_copy_v2(world->raptor_world_ptr,
                             (raptor_uri*)statement->predicate);
#else
    uri = raptor_uri_copy((raptor_uri*)statement->predicate);
#endif
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
    strncpy((char*)string, (const char*)statement->object, literal_len + 1);

    if(statement->object_literal_language) {
      size_t language_len;
      language_len = strlen((const char*)statement->object_literal_language);
      language = (char*)RASQAL_MALLOC(cstring, language_len + 1);
      strncpy(language, (const char*)statement->object_literal_language,
              language_len + 1);
    }

    if(statement->object_type == RAPTOR_IDENTIFIER_TYPE_XML_LITERAL) {
      const unsigned char* dt_uri_string;

      dt_uri_string = (const unsigned char*)raptor_xml_literal_datatype_uri_string;
#ifdef RAPTOR_V2_AVAILABLE
      uri = raptor_new_uri_v2(world->raptor_world_ptr, dt_uri_string);
#else
      uri = raptor_new_uri(dt_uri_string);
#endif
    } else if(statement->object_literal_datatype) {
      raptor_uri *dt_uri = (raptor_uri*)statement->object_literal_datatype;

#ifdef RAPTOR_V2_AVAILABLE
      uri = raptor_uri_copy_v2(world->raptor_world_ptr, dt_uri);
#else
      uri = raptor_uri_copy(dt_uri);
#endif
    }
    o = rasqal_new_string_literal(world, string, language, uri, NULL);
  } else if(statement->object_type == RAPTOR_IDENTIFIER_TYPE_ANONYMOUS) {
    char *blank = (char*)statement->object;
    unsigned char *new_blank;
    size_t blank_len;

    blank_len = strlen(blank);
    new_blank = (unsigned char*)RASQAL_MALLOC(cstring, blank_len + 1);
    strncpy((char*)new_blank, (const char*)blank, blank_len + 1);
    o = rasqal_new_simple_literal(world, RASQAL_LITERAL_BLANK, new_blank);
  } else if(statement->object_type == RAPTOR_IDENTIFIER_TYPE_ORDINAL) {
    raptor_uri* uri = ordinal_as_uri(world, *((int*)statement->object));

    if(!uri)
      return NULL;
    o = rasqal_new_uri_literal(world, uri);
  } else {
    raptor_uri *uri;

#ifdef RAPTOR_V2_AVAILABLE
    uri = raptor_uri_copy_v2(world->raptor_world_ptr,
                             (raptor_uri*)statement->object);
#else
    uri = raptor_uri_copy((raptor_uri*)statement->object);
#endif
    o = rasqal_new_uri_literal(world, uri);
  }

  return rasqal_new_triple(s, p, o);
}



static void
rasqal_raptor_statement_handler(void *user_data, 
                                const raptor_statement *statement) 
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


static void
rasqal_raptor_error_handler(void *user_data, 
                            raptor_locator* locator, const char *message)
{
  rasqal_query* query = (rasqal_query*)user_data;
  int locator_len;

  query->failed = 1;

  if(locator &&
#ifdef RAPTOR_V2_AVAILABLE
     (locator_len = raptor_format_locator_v2(query->world->raptor_world_ptr,
                                             NULL, 0, locator)) > 0
#else
     (locator_len = raptor_format_locator(NULL, 0, locator)) > 0
#endif
    ) {
    char *buffer = (char*)RASQAL_MALLOC(cstring, locator_len + 1);
#ifdef RAPTOR_V2_AVAILABLE
    raptor_format_locator_v2(query->world->raptor_world_ptr, buffer,
                             locator_len, locator);
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


static unsigned char*
rasqal_raptor_generate_id_handler(void *user_data,
                                  raptor_genid_type type,
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
    strncpy((char*)mapped_id, (const char*)rtsc->mapped_id_base, 
            rtsc->mapped_id_base_len);
    mapped_id[rtsc->mapped_id_base_len] = '_';
    strncpy((char*)(mapped_id+rtsc->mapped_id_base_len + 1),
            (const char*)user_bnodeid, user_bnodeid_len + 1);

    raptor_free_memory(user_bnodeid);
    return mapped_id;
  }
  
  return rasqal_query_get_genid(rtsc->query, (const unsigned char*)"genid", -1);
}


static int
rasqal_raptor_new_triples_source(rasqal_query* rdf_query,
                                 void *factory_user_data,
                                 void *user_data,
                                 rasqal_triples_source *rts)
{
  rasqal_raptor_triples_source_user_data* rtsc;
  raptor_parser *parser;
  int i;

  rtsc = (rasqal_raptor_triples_source_user_data*)user_data;

  if(!rdf_query->data_graphs)
    return -1;  /* no data */

  rts->init_triples_match = rasqal_raptor_init_triples_match;
  rts->triple_present = rasqal_raptor_triple_present;
  rts->free_triples_source = rasqal_raptor_free_triples_source;

  rtsc->sources_count = raptor_sequence_size(rdf_query->data_graphs);
  /* no default triple source possible */
  if(!rtsc->sources_count)
    return -1;  /* no data */

  rtsc->source_literals = (rasqal_literal**)RASQAL_CALLOC(rasqal_literal_ptr,
                                                          rtsc->sources_count,
                                                          sizeof(rasqal_literal*));

  rtsc->query = rdf_query;

  for(i = 0; i < rtsc->sources_count; i++) {
    rasqal_data_graph *dg;
    raptor_uri* uri;
    raptor_uri* name_uri;

    dg = (rasqal_data_graph*)raptor_sequence_get_at(rdf_query->data_graphs, i);
    uri = dg->uri;
    name_uri = dg->name_uri;

    rtsc->source_index = i;
#ifdef RAPTOR_V2_AVAILABLE
    rtsc->source_uri = raptor_uri_copy_v2(rdf_query->world->raptor_world_ptr,
                                          uri);
#else
    rtsc->source_uri = raptor_uri_copy(uri);
#endif
    if(name_uri)
      rtsc->source_literals[i] = rasqal_new_uri_literal(rdf_query->world, 
#ifdef RAPTOR_V2_AVAILABLE
                                                        raptor_uri_copy_v2(rdf_query->world->raptor_world_ptr, name_uri)
#else
                                                        raptor_uri_copy(name_uri)
#endif
                                                        );
    else
      name_uri = raptor_uri_copy(uri);

    rtsc->mapped_id_base = rasqal_query_get_genid(rdf_query,
                                                  (const unsigned char*)"graphid",
                                                  i);
    rtsc->mapped_id_base_len = strlen((const char*)rtsc->mapped_id_base);

#ifdef RAPTOR_V2_AVAILABLE
    parser = raptor_new_parser_v2(rdf_query->world->raptor_world_ptr, "guess");
#else
    parser = raptor_new_parser("guess");
#endif
    raptor_set_statement_handler(parser, rtsc, rasqal_raptor_statement_handler);
    raptor_set_error_handler(parser, rdf_query, rasqal_raptor_error_handler);
    raptor_set_generate_id_handler(parser, rtsc,
                                   rasqal_raptor_generate_id_handler);

#ifdef RAPTOR_FEATURE_NO_NET
    if(rdf_query->features[RASQAL_FEATURE_NO_NET])
      raptor_set_feature(parser, RAPTOR_FEATURE_NO_NET,
                         rdf_query->features[RASQAL_FEATURE_NO_NET]);
#endif

    raptor_parse_uri(parser, uri, name_uri);
    raptor_free_parser(parser);

#ifdef RAPTOR_V2_AVAILABLE
    raptor_free_uri_v2(rdf_query->world->raptor_world_ptr, rtsc->source_uri);
#else
    raptor_free_uri(rtsc->source_uri);
#endif

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
#ifdef RAPTOR_V2_AVAILABLE
        if(!raptor_uri_equals_v2(world->raptor_world_ptr, triple_uri, match_uri))
          goto done;
#else
        if(!raptor_uri_equals(triple_uri, match_uri))
          goto done;
#endif
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



static void
rasqal_raptor_register_triples_source_factory(rasqal_triples_source_factory *factory) 
{
  factory->user_data_size = sizeof(rasqal_raptor_triples_source_user_data);
  factory->new_triples_source = rasqal_raptor_new_triples_source;
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
  
    
  RASQAL_DEBUG1("rasqal_init_triples_match done\n");

  return 0;
}


int
rasqal_raptor_init(rasqal_world* world)
{
  rasqal_set_triples_source_factory(world, rasqal_raptor_register_triples_source_factory, (void*)NULL);
  return 0;
}

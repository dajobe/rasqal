/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_raptor.c - Rasqal triple store implementation with raptor
 *
 * $Id$
 * 
 * Copyright (C) 2004, David Beckett http://purl.org/net/dajobe/
 * Institute for Learning and Research Technology http://www.ilrt.bristol.ac.uk/
 * University of Bristol, UK http://www.bristol.ac.uk/
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
  rasqal_raptor_triple *head;
  rasqal_raptor_triple *tail;

  /* index used while reading triples into the two arrays below.
   * This is used to connect a triple to the URI literal of the source
   */
  int source_index;

  /* size of the two arrays below */
  int source_uris_count;
  
  /* array of shared pointers into query->source uris */
  raptor_uri **source_uris;

  /* array of URI literals (allocated here) */
  rasqal_literal **source_literals;
} rasqal_raptor_triples_source_user_data;


/* prototypes */
static rasqal_triples_match* rasqal_raptor_new_triples_match(rasqal_triples_source *rts, void *user_data, rasqal_triple_meta *m, rasqal_triple *t);
static int rasqal_raptor_triple_present(rasqal_triples_source *rts, void *user_data, rasqal_triple *t);
static void rasqal_raptor_free_triples_source(void *user_data);


static
raptor_uri* ordinal_as_uri(int ordinal) 
{
  int t=ordinal;
  size_t len; 
  unsigned char *buffer;
  raptor_uri* uri;
  
  len=raptor_rdf_namespace_uri_len + 1; /* 1 for '_' */
  while(t/=10)
    len++;
  buffer=(unsigned char*)RASQAL_MALLOC(cstring, len+1);
  if(!buffer)
    return NULL;
  
  sprintf((char*)buffer, "%s_%d", raptor_rdf_namespace_uri, ordinal);
  uri=raptor_new_uri(buffer);
  RASQAL_FREE(cstring, buffer);

  return uri;
}

  
static rasqal_triple*
raptor_statement_as_rasqal_triple(const raptor_statement *statement) {
  rasqal_literal *s, *p, *o;

  if(statement->subject_type == RAPTOR_IDENTIFIER_TYPE_ANONYMOUS) {
    unsigned char *new_blank=(unsigned char*)RASQAL_MALLOC(cstring, strlen((char*)statement->subject)+1);
    strcpy((char*)new_blank, (const char*)statement->subject);
    s=rasqal_new_simple_literal(RASQAL_LITERAL_BLANK, new_blank);
  } else if(statement->subject_type == RAPTOR_IDENTIFIER_TYPE_ORDINAL) {
    raptor_uri* uri=ordinal_as_uri(*((int*)statement->subject));
    if(!uri)
      return NULL;
    s=rasqal_new_uri_literal(uri);
  } else
    s=rasqal_new_uri_literal(raptor_uri_copy((raptor_uri*)statement->subject));

  if(statement->predicate_type == RAPTOR_IDENTIFIER_TYPE_ORDINAL) {
    raptor_uri* uri=ordinal_as_uri(*((int*)statement->predicate));
    if(!uri)
      return NULL;
    p=rasqal_new_uri_literal(uri);
  } else
    p=rasqal_new_uri_literal(raptor_uri_copy((raptor_uri*)statement->predicate));


  if(statement->object_type == RAPTOR_IDENTIFIER_TYPE_LITERAL || 
     statement->object_type == RAPTOR_IDENTIFIER_TYPE_XML_LITERAL) {
    unsigned char *string;
    char *language=NULL;
    raptor_uri *uri=NULL;
    
    string=(unsigned char*)RASQAL_MALLOC(cstring, strlen((char*)statement->object)+1);
    strcpy((char*)string, (const char*)statement->object);

    if(statement->object_literal_language) {
      language=(char*)RASQAL_MALLOC(cstring, strlen((const char*)statement->object_literal_language)+1);
      strcpy(language, (const char*)statement->object_literal_language);
    }

    if(statement->object_type == RAPTOR_IDENTIFIER_TYPE_XML_LITERAL) {
      uri=raptor_new_uri((const unsigned char*)raptor_xml_literal_datatype_uri_string);
    } else if(statement->object_literal_datatype) {
      uri=raptor_uri_copy((raptor_uri*)statement->object_literal_datatype);
    }
    o=rasqal_new_string_literal(string, language, uri, NULL);
  } else if(statement->object_type == RAPTOR_IDENTIFIER_TYPE_ANONYMOUS) {
    char *blank=(char*)statement->object;
    unsigned char *new_blank=(unsigned char*)RASQAL_MALLOC(cstring, strlen(blank)+1);
    strcpy((char*)new_blank, (const char*)blank);
    o=rasqal_new_simple_literal(RASQAL_LITERAL_BLANK, new_blank);
  } else if(statement->object_type == RAPTOR_IDENTIFIER_TYPE_ORDINAL) {
    raptor_uri* uri=ordinal_as_uri(*((int*)statement->object));
    if(!uri)
      return NULL;
    o=rasqal_new_uri_literal(uri);
  } else {
    raptor_uri *uri=raptor_uri_copy((raptor_uri*)statement->object);
    o=rasqal_new_uri_literal(uri);
  }

  return rasqal_new_triple(s, p, o);
}



static void
rasqal_raptor_statement_handler(void *user_data, 
                                const raptor_statement *statement) 
{
  rasqal_raptor_triples_source_user_data* rtsc=(rasqal_raptor_triples_source_user_data*)user_data;
  rasqal_raptor_triple *triple;
  
  triple=(rasqal_raptor_triple*)RASQAL_MALLOC(rasqal_raptor_triple, sizeof(rasqal_raptor_triple));
  triple->next=NULL;
  triple->triple=raptor_statement_as_rasqal_triple(statement);

  /* this origin URI literal is shared amongst the triples and
   * freed only in rasqal_raptor_free_triples_source
   */
  rasqal_triple_set_origin(triple->triple, 
                           rtsc->source_literals[rtsc->source_index]);

  if(rtsc->tail)
    rtsc->tail->next=triple;
  else
    rtsc->head=triple;

  rtsc->tail=triple;
}


static void
rasqal_raptor_error_handler(void *user_data, 
                            raptor_locator* locator, const char *message) {
  rasqal_query* query=(rasqal_query*)user_data;

  query->failed=1;

  if(locator) {
    int locator_len=raptor_format_locator(NULL, 0, locator);
    char *buffer=(char*)RASQAL_MALLOC(cstring, locator_len+1);
    raptor_format_locator(buffer, locator_len, locator);

    rasqal_query_error(query, "Failed to parse %s - %s", buffer, message);
    RASQAL_FREE(cstring, buffer);
  } else
    rasqal_query_error(query, "Failed to parse - %s", message);
}


static int
rasqal_raptor_new_triples_source(rasqal_query* rdf_query,
                                 void *factory_user_data,
                                 void *user_data,
                                 rasqal_triples_source *rts) {
  rasqal_raptor_triples_source_user_data* rtsc=(rasqal_raptor_triples_source_user_data*)user_data;
  raptor_parser *parser;
  const char *parser_name;
  int i;

  if(!rdf_query->sources)
    return 1;

  rts->new_triples_match=rasqal_raptor_new_triples_match;
  rts->triple_present=rasqal_raptor_triple_present;
  rts->free_triples_source=rasqal_raptor_free_triples_source;

  rtsc->source_uris_count=raptor_sequence_size(rdf_query->sources);
  /* no default triple source possible */
  if(!rtsc->source_uris_count)
    return 1;

  rtsc->source_uris=(raptor_uri**)RASQAL_CALLOC(raptor_uri_ptr, rtsc->source_uris_count, sizeof(raptor_uri*));
  rtsc->source_literals=(rasqal_literal**)RASQAL_CALLOC(rasqal_literal_ptr, rtsc->source_uris_count, sizeof(rasqal_literal*));

  for(i=0; i< rtsc->source_uris_count; i++) {
    raptor_uri* uri=(raptor_uri*)raptor_sequence_get_at(rdf_query->sources, i);

    rtsc->source_index=i;
    /* not allocated here; these are shared pointers into query->sources */
    rtsc->source_uris[i]=uri;
    rtsc->source_literals[i]=rasqal_new_uri_literal(raptor_uri_copy(uri));

    parser_name=raptor_guess_parser_name(NULL, NULL, NULL, 0, 
                                         raptor_uri_as_string(uri));
    parser=raptor_new_parser(parser_name);
    raptor_set_statement_handler(parser, rtsc, rasqal_raptor_statement_handler);
    raptor_set_error_handler(parser, rdf_query, rasqal_raptor_error_handler);
    raptor_parse_uri(parser, uri, NULL);
    raptor_free_parser(parser);
    if(rdf_query->failed) {
      rasqal_raptor_free_triples_source(user_data);
      break;
    }
  }

  return rdf_query->failed;
}


/**
 * rasqal_raptor_triple_match:
 * @triple: &rasqal_triple to match against
 * @match: &rasqal_triple with wildcards
 * 
 * Match a rasqal_triple against a rasqal_triple with NULL
 * signifying wildcard fields in the rasqal_triple.
 * 
 * Return value: non-0 on match
 **/
static int
rasqal_raptor_triple_match(rasqal_triple *triple, rasqal_triple *match) {
  if(match->subject) {
    if(!rasqal_literal_equals(triple->subject, match->subject))
      return 0;
  }

  if(match->predicate) {
    if(!rasqal_literal_equals(triple->predicate, match->predicate))
      return 0;
  }

  if(match->object) {
    if(!rasqal_literal_equals(triple->object, match->object))
      return 0;
  }
  
  if(match->origin && match->origin->type == RASQAL_LITERAL_URI ) {
    raptor_uri* triple_uri=triple->origin->value.uri;
    raptor_uri* match_uri=match->origin->value.uri;
    if(!raptor_uri_equals(triple_uri, match_uri))
      return 0;
  }
  
  return 1;
}


/* non-0 if present */
static int
rasqal_raptor_triple_present(rasqal_triples_source *rts, void *user_data, 
                             rasqal_triple *t) 
{
  rasqal_raptor_triples_source_user_data* rtsc=(rasqal_raptor_triples_source_user_data*)user_data;
  rasqal_raptor_triple *triple;

  for(triple=rtsc->head; triple; triple=triple->next) {
    if(rasqal_raptor_triple_match(triple->triple, t))
      return 1;
  }

  return 0;
}



static void
rasqal_raptor_free_triples_source(void *user_data) {
  rasqal_raptor_triples_source_user_data* rtsc=(rasqal_raptor_triples_source_user_data*)user_data;
  rasqal_raptor_triple *cur=rtsc->head;
  int i;

  while(cur) {
    rasqal_raptor_triple *next=cur->next;
    rasqal_triple_set_origin(cur->triple, NULL); /* shared URI literal */
    rasqal_free_triple(cur->triple);
    RASQAL_FREE(rasqal_raptor_triple, cur);
    cur=next;
  }

  for(i=0; i< rtsc->source_uris_count; i++) {
    /* not freed here; these are shared pointers into query->sources */
    /* raptor_free_uri(rtsc->source_uris[i]); */
    rasqal_free_literal(rtsc->source_literals[i]);
  }
  RASQAL_FREE(raptor_uri_ptr, rtsc->source_uris);
  RASQAL_FREE(raptor_literal_ptr, rtsc->source_literals);
  
}



static void
rasqal_raptor_register_triples_source_factory(rasqal_triples_source_factory *factory) 
{
  factory->user_data_size=sizeof(rasqal_raptor_triples_source_user_data);
  factory->new_triples_source=rasqal_raptor_new_triples_source;
}


typedef struct {
  rasqal_raptor_triple *cur;
  rasqal_raptor_triples_source_user_data* source_context;
  rasqal_triple match;
} rasqal_raptor_triples_match_context;


static rasqal_triple_parts
rasqal_raptor_bind_match(struct rasqal_triples_match_s* rtm,
                         void *user_data,
                         rasqal_variable* bindings[4],
                         rasqal_triple_parts parts) {
  rasqal_raptor_triples_match_context* rtmc=(rasqal_raptor_triples_match_context*)rtm->user_data;
  int error=0;
  
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
    RASQAL_DEBUG1("binding subject to variable\n");
    rasqal_variable_set_value(bindings[0], rasqal_literal_as_node(rtmc->cur->triple->subject));
  }

  if(bindings[1] && (parts & RASQAL_TRIPLE_PREDICATE)) {
    if(bindings[0] == bindings[1]) {
      if(rasqal_literal_compare(rtmc->cur->triple->subject,
                                rtmc->cur->triple->predicate, 0, &error))
        return 0;
      if(error)
        return 0;
      
      RASQAL_DEBUG1("subject and predicate values match\n");
    } else {
      RASQAL_DEBUG1("binding predicate to variable\n");
      rasqal_variable_set_value(bindings[1], rasqal_literal_as_node(rtmc->cur->triple->predicate));
    }
  }

  if(bindings[2] && (parts & RASQAL_TRIPLE_OBJECT)) {
    int bind=1;
    
    if(bindings[0] == bindings[2]) {
      if(rasqal_literal_compare(rtmc->cur->triple->subject,
                                rtmc->cur->triple->object, 0, &error))
        return 0;
      if(error)
        return 0;

      bind=0;
      RASQAL_DEBUG1("subject and object values match\n");
    }
    if(bindings[1] == bindings[2] &&
       !(bindings[0] == bindings[1]) /* don't do this check if ?x ?x ?x */
       ) {
      if(rasqal_literal_compare(rtmc->cur->triple->predicate,
                                rtmc->cur->triple->object, 0, &error))
        return 0;
      if(error)
        return 0;

      bind=0;
      RASQAL_DEBUG1("predicate and object values match\n");
    }
    
    if(bind) {
      RASQAL_DEBUG1("binding object to variable\n");
      rasqal_variable_set_value(bindings[2],  rasqal_literal_as_node(rtmc->cur->triple->object));
    }
  }

  if(bindings[3] && (parts & RASQAL_TRIPLE_ORIGIN)) {
    rasqal_literal *l=rasqal_new_literal_from_literal(rtmc->cur->triple->origin);
    RASQAL_DEBUG1("binding origin to variable\n");
    rasqal_variable_set_value(bindings[3], l);
  }

  return 1;
}



static void
rasqal_raptor_next_match(struct rasqal_triples_match_s* rtm, void *user_data)
{
  rasqal_raptor_triples_match_context* rtmc=(rasqal_raptor_triples_match_context*)rtm->user_data;

  while(rtmc->cur) {
    rtmc->cur=rtmc->cur->next;
    if(rtmc->cur &&
       rasqal_raptor_triple_match(rtmc->cur->triple, &rtmc->match))
      break;
  }
}

static int
rasqal_raptor_is_end(struct rasqal_triples_match_s* rtm, void *user_data)
{
  rasqal_raptor_triples_match_context* rtmc=(rasqal_raptor_triples_match_context*)rtm->user_data;

  return !rtmc || rtmc->cur == NULL;
}


static void
rasqal_raptor_finish_triples_match(struct rasqal_triples_match_s* rtm,
                                    void *user_data) {
  rasqal_raptor_triples_match_context* rtmc=(rasqal_raptor_triples_match_context*)rtm->user_data;

  RASQAL_FREE(rasqal_raptor_triples_match_context, rtmc);
}


static rasqal_triples_match*
rasqal_raptor_new_triples_match(rasqal_triples_source *rts, void *user_data,
                                rasqal_triple_meta *m, rasqal_triple *t) {
  rasqal_raptor_triples_source_user_data* rtsc=(rasqal_raptor_triples_source_user_data*)user_data;
  rasqal_triples_match *rtm;
  rasqal_raptor_triples_match_context* rtmc;
  rasqal_variable* var;

  rtm=(rasqal_triples_match *)RASQAL_CALLOC(rasqal_triples_match, sizeof(rasqal_triples_match), 1);
  rtm->bind_match=rasqal_raptor_bind_match;
  rtm->next_match=rasqal_raptor_next_match;
  rtm->is_end=rasqal_raptor_is_end;
  rtm->finish=rasqal_raptor_finish_triples_match;

  rtmc=(rasqal_raptor_triples_match_context*)RASQAL_CALLOC(rasqal_raptor_triples_match_context, sizeof(rasqal_raptor_triples_match_context), 1);

  rtm->user_data=rtmc;

  rtmc->source_context=rtsc;
  rtmc->cur=rtsc->head;
  
  /* at least one of the triple terms is a variable and we need to
   * do a triplesMatching() over the list of stored raptor_statements
   */

  if((var=rasqal_literal_as_variable(t->subject))) {
    if(var->value)
      rtmc->match.subject=rasqal_new_literal_from_literal(var->value);
  } else
    rtmc->match.subject=rasqal_new_literal_from_literal(t->subject);

  m->bindings[0]=var;
  

  if((var=rasqal_literal_as_variable(t->predicate))) {
    if(var->value)
      rtmc->match.predicate=rasqal_new_literal_from_literal(var->value);
  } else
    rtmc->match.predicate=rasqal_new_literal_from_literal(t->predicate);

  m->bindings[1]=var;
  

  if((var=rasqal_literal_as_variable(t->object))) {
    if(var->value)
      rtmc->match.object=rasqal_new_literal_from_literal(var->value);
  } else
    rtmc->match.object=rasqal_new_literal_from_literal(t->object);

  m->bindings[2]=var;
  

  if(t->origin) {
    if((var=rasqal_literal_as_variable(t->origin))) {
      if(var->value)
        rtmc->match.origin=rasqal_new_literal_from_literal(var->value);
    } else
      rtmc->match.origin=rasqal_new_literal_from_literal(t->origin);
    m->bindings[3]=var;
  }
  

  while(rtmc->cur) {
    if(rasqal_raptor_triple_match(rtmc->cur->triple, &rtmc->match))
      break;
    rtmc->cur=rtmc->cur->next;
  }
  
    
  RASQAL_DEBUG1("rasqal_new_triples_match done\n");

  return rtm;
}


void
rasqal_raptor_init(void) {
  rasqal_set_triples_source_factory(rasqal_raptor_register_triples_source_factory, (void*)NULL);
}

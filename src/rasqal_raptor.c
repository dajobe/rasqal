/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_raptor.c - Rasqal triple store implementation with raptor
 *
 * $Id$
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


struct rasqal_raptor_triple_s {
  struct rasqal_raptor_triple_s *next;
  raptor_statement *statement;
};

typedef struct rasqal_raptor_triple_s rasqal_raptor_triple;

typedef struct {
  rasqal_raptor_triple *head;
  rasqal_raptor_triple *tail;
  raptor_uri *uri;
} rasqal_raptor_triples_source_user_data;


/* prototypes */
static rasqal_triples_match* rasqal_raptor_new_triples_match(rasqal_triples_source *rts, void *user_data, rasqal_triple_meta *m, rasqal_triple *t);
static int rasqal_raptor_triple_present(rasqal_triples_source *rts, void *user_data, rasqal_triple *t);
static void rasqal_raptor_free_triples_source(void *user_data);


static raptor_statement*
raptor_copy_statement(const raptor_statement *statement) {
  raptor_statement *copy;

  copy=RASQAL_MALLOC(raptor_statement, sizeof(raptor_statement));

  memcpy(copy, statement, sizeof(raptor_statement));

  if(statement->subject_type == RAPTOR_IDENTIFIER_TYPE_ANONYMOUS) {
    copy->subject=RASQAL_MALLOC(cstring, strlen((char*)statement->subject));
    strcpy((char*)copy->subject, (const char*)statement->subject);
  } else {
    copy->subject=raptor_uri_copy((raptor_uri*)statement->subject);
  }

  if(statement->predicate_type == RAPTOR_IDENTIFIER_TYPE_ORDINAL) {
    ; /* nop */
  } else {
    copy->predicate=raptor_uri_copy((raptor_uri*)statement->predicate);
  }

  if(statement->object_type == RAPTOR_IDENTIFIER_TYPE_LITERAL || 
     statement->object_type == RAPTOR_IDENTIFIER_TYPE_XML_LITERAL) {
    if(statement->object_type == RAPTOR_IDENTIFIER_TYPE_XML_LITERAL) {
      ; /* nop */
    } else if(statement->object_literal_datatype) {
      copy->object_literal_datatype=raptor_uri_copy((raptor_uri*)statement->object_literal_datatype);
    }

    copy->object=RASQAL_MALLOC(cstring, strlen((char*)statement->object));
    strcpy((char*)copy->object, (const char*)statement->object);
  } else if(statement->object_type == RAPTOR_IDENTIFIER_TYPE_ANONYMOUS) {
    copy->object=RASQAL_MALLOC(cstring, strlen((char*)statement->object));
    strcpy((char*)copy->object, (const char*)statement->object);
  } else if(statement->object_type == RAPTOR_IDENTIFIER_TYPE_ORDINAL) {
    /* nop */
  } else {
    copy->object=raptor_uri_copy((raptor_uri*)statement->object);
  }

  return copy;
}



static void
rasqal_raptor_statement_handler(void *user_data, 
                                const raptor_statement *statement) 
{
  rasqal_raptor_triples_source_user_data* rtsc=(rasqal_raptor_triples_source_user_data*)user_data;
  rasqal_raptor_triple *triple;
  
  triple=RASQAL_MALLOC(rasqal_raptor_triple, sizeof(rasqal_raptor_triple));
  triple->next=NULL;
  triple->statement=raptor_copy_statement(statement);

  if(rtsc->tail)
    rtsc->tail->next=triple;
  else
    rtsc->head=triple;

  rtsc->tail=triple;
}



static int
rasqal_raptor_new_triples_source(rasqal_query* rdf_query,
                                  void *factory_user_data,
                                  void *user_data,
                                  rasqal_triples_source *rts) {
  rasqal_raptor_triples_source_user_data* rtsc=(rasqal_raptor_triples_source_user_data*)user_data;
  raptor_parser *parser;
  const char *parser_name;
  
  rtsc->uri=raptor_uri_copy(rts->uri);

  rts->new_triples_match=rasqal_raptor_new_triples_match;
  rts->triple_present=rasqal_raptor_triple_present;
  rts->free_triples_source=rasqal_raptor_free_triples_source;

  parser_name=raptor_guess_parser_name(NULL, NULL, NULL, 0, 
                                       raptor_uri_as_string(rtsc->uri));
  parser=raptor_new_parser(parser_name);
  raptor_set_statement_handler(parser, rtsc, rasqal_raptor_statement_handler);

  raptor_parse_uri(parser, rtsc->uri, NULL);

  raptor_free_parser(parser);

  return 0;
}


/**
 * rasqal_raptor_triple_match:
 * @s: &raptor_statement to match against
 * @t: &rasqal_triple with 
 * 
 * Match a raptor_statement against a rasqal_triple with NULL
 * signifying wildcard fields in the rasqal_triple.
 * 
 * Return value: 
 **/
static int
rasqal_raptor_triple_match(raptor_statement *s, rasqal_triple *t) {
  if(t->subject) {
    if (1 /* FIXME match */)
      return 0;
  }

  if(t->predicate) {
    if (1 /* FIXME match */)
      return 0;
  }

  if(t->object) {
    if (1 /* FIXME match */)
      return 0;
  }

  return 1;
}


static int
rasqal_raptor_triple_present(rasqal_triples_source *rts, void *user_data, 
                             rasqal_triple *t) 
{
  rasqal_raptor_triples_source_user_data* rtsc=(rasqal_raptor_triples_source_user_data*)user_data;
  rasqal_raptor_triple *triple;

  for(triple=rtsc->head; triple; triple=triple->next) {
    if(rasqal_raptor_triple_match(triple->statement, t))
      return 1;
  }

  return 0;
}



static void
rasqal_raptor_free_triples_source(void *user_data) {
  rasqal_raptor_triples_source_user_data* rtsc=(rasqal_raptor_triples_source_user_data*)user_data;
  rasqal_raptor_triple *cur=rtsc->head;

  while(cur) {
    rasqal_raptor_triple *next=cur->next;
    RASQAL_FREE(rasqal_raptor_triple, cur);
    cur=next;
  }

  raptor_free_uri(rtsc->uri);
}


static inline rasqal_literal*
raptor_statement_subject_as_rasqal_literal(raptor_statement *statement) 
{
  rasqal_literal* l;
  
  if(statement->subject_type == RAPTOR_IDENTIFIER_TYPE_ANONYMOUS) {
    char *blank=(char*)statement->subject;
    char *new_blank=RASQAL_MALLOC(cstring, strlen(blank)+1);
    strcpy(new_blank, (const char*)blank);
    l=rasqal_new_simple_literal(RASQAL_LITERAL_BLANK, new_blank);
  } else {
    raptor_uri* uri=raptor_uri_copy((raptor_uri*)statement->subject);
    l=rasqal_new_uri_literal(uri);
  }

  return l;
}


static inline rasqal_literal*
raptor_statement_predicate_as_rasqal_literal(raptor_statement *statement) 
{
  rasqal_literal* l;
  
  if(statement->predicate_type == RAPTOR_IDENTIFIER_TYPE_ORDINAL) {
    /* FIXME - 46 for "<http://www.w3.org/1999/02/22-rdf-syntax-ns#_>" */
    size_t len=46 + 13; 
    unsigned char *buffer=(unsigned char*)RASQAL_MALLOC(cstring, len+1);
    if(!buffer)
      return NULL;

    sprintf((char*)buffer,
            "<http://www.w3.org/1999/02/22-rdf-syntax-ns#_%d>",
            *((int*)statement->predicate));
    raptor_uri* uri=raptor_new_uri(buffer);
    RASQAL_FREE(cstring, buffer);
    l=rasqal_new_uri_literal(uri);
  } else {
    raptor_uri* uri=raptor_uri_copy((raptor_uri*)statement->predicate);
    l=rasqal_new_uri_literal(uri);
  }

  return l;
}



static inline rasqal_literal*
raptor_statement_object_as_rasqal_literal(raptor_statement *statement) 
{
  rasqal_literal* l;
  
  if(statement->object_type == RAPTOR_IDENTIFIER_TYPE_LITERAL || 
     statement->object_type == RAPTOR_IDENTIFIER_TYPE_XML_LITERAL) {
    char *string;
    char *language=NULL;
    raptor_uri *uri=NULL;
    
    string=RASQAL_MALLOC(cstring, strlen((char*)statement->object));
    strcpy((char*)string, (const char*)statement->object);

    if(statement->object_type == RAPTOR_IDENTIFIER_TYPE_XML_LITERAL) {
      /* FIXME */
      extern const char *raptor_xml_literal_datatype_uri_string;
      uri=raptor_new_uri(raptor_xml_literal_datatype_uri_string);
    } else if(statement->object_literal_datatype) {
      uri=raptor_uri_copy((raptor_uri*)statement->object_literal_datatype);
    }
    l=rasqal_new_string_literal(string, language, uri, NULL);
  } else if(statement->object_type == RAPTOR_IDENTIFIER_TYPE_ANONYMOUS) {
    char *blank=(char*)statement->object;
    char *new_blank=RASQAL_MALLOC(cstring, strlen(blank)+1);
    strcpy(new_blank, (const char*)blank);
    l=rasqal_new_simple_literal(RASQAL_LITERAL_BLANK, new_blank);
  } else if(statement->object_type == RAPTOR_IDENTIFIER_TYPE_ORDINAL) {
    /* FIXME - 46 for "<http://www.w3.org/1999/02/22-rdf-syntax-ns#_>" */
    size_t len=46 + 13; 
    unsigned char *buffer=(unsigned char*)RASQAL_MALLOC(cstring, len+1);
    if(!buffer)
      return NULL;

    sprintf((char*)buffer,
            "<http://www.w3.org/1999/02/22-rdf-syntax-ns#_%d>",
            *((int*)statement->predicate));
    raptor_uri* uri=raptor_new_uri(buffer);
    RASQAL_FREE(cstring, buffer);
    l=rasqal_new_uri_literal(uri);
  } else {
    raptor_uri *uri=raptor_uri_copy((raptor_uri*)statement->object);
    l=rasqal_new_uri_literal(uri);
  }

  return l;
}


static void
rasqal_raptor_register_triples_source_factory(rasqal_triples_source_factory *factory) 
{
  factory->user_data_size=sizeof(rasqal_raptor_triples_source_user_data);
  factory->new_triples_source=rasqal_raptor_new_triples_source;
}


typedef struct {
  rasqal_raptor_triple *cur;
  raptor_statement match;
} rasqal_raptor_triples_match_context;


static int
rasqal_raptor_bind_match(struct rasqal_triples_match_s* rtm,
                          void *user_data,
                          rasqal_variable* bindings[3]) 
{
  rasqal_raptor_triples_match_context* rtmc=(rasqal_raptor_triples_match_context*)rtm->user_data;
  
#ifdef RASQAL_DEBUG
  if(rtmc->cur) {
    RASQAL_DEBUG1("  matched statement ");
    raptor_print_statement(rtmc->cur->statement, stderr);
    fputc('\n', stderr);
  } else
    RASQAL_FATAL1("  matched NO statement - BUG\n");
#endif

  /* set 1 or 2 variable values from the fields of statement */

  if(bindings[0]) {
    RASQAL_DEBUG1("binding subject to variable\n");
    rasqal_variable_set_value(bindings[0],
                              raptor_statement_subject_as_rasqal_literal(rtmc->cur->statement));
  }

  if(bindings[1]) {
    RASQAL_DEBUG1("binding predicate to variable\n");
    rasqal_variable_set_value(bindings[1], 
                              raptor_statement_predicate_as_rasqal_literal(rtmc->cur->statement));
  }

  if(bindings[2]) {
    RASQAL_DEBUG1("binding object to variable\n");
    rasqal_variable_set_value(bindings[2],  
                              raptor_statement_object_as_rasqal_literal(rtmc->cur->statement));
  }

  return 0;
}


/* non-zero if equal */
static int
raptor_node_equals(const void *id1,
                   raptor_identifier_type id1_type,
                   raptor_uri *id1_literal_datatype,
                   const unsigned char *id1_literal_language,
                   const void *id2,
                   raptor_identifier_type id2_type,
                   raptor_uri *id2_literal_datatype,
                   const unsigned char *id2_literal_language) 
{
  if(id1_type != id2_type)
    return 0;

  switch(id1_type) {
    case RAPTOR_IDENTIFIER_TYPE_RESOURCE:
    case RAPTOR_IDENTIFIER_TYPE_PREDICATE:
      if(raptor_uri_equals((raptor_uri*)id1, (raptor_uri*)id2))
        return 0;
      break;

    case RAPTOR_IDENTIFIER_TYPE_LITERAL:
      if(id1_literal_language || id2_literal_language) {
        /* if either is null, the comparison fails */
        if(!id1_literal_language || !id2_literal_language)
          return 0;
        if(rasqal_strcasecmp(id1_literal_language,id1_literal_language))
          return 0;
      }
      if(id1_literal_datatype || id2_literal_datatype) {
        /* if either is null, the comparison fails */
        if(!id1_literal_datatype || !id2_literal_datatype)
          return 0;
        if(raptor_uri_equals(id1_literal_datatype,id1_literal_datatype))
          return 0;
      }

      /* FALLTHROUGH */
    case RAPTOR_IDENTIFIER_TYPE_ANONYMOUS:
    case RAPTOR_IDENTIFIER_TYPE_XML_LITERAL:
      if(strcmp((char*)id1, (char*)id2))
        return 0;
      break;
      
    case RAPTOR_IDENTIFIER_TYPE_ORDINAL:
      if(*((int*)id1) != *((int*)id2))
        return 0;
      break;
      
    default:
      abort();
  }

  return 1;
}


/* non-zero if equal */
static int
raptor_statement_compare(raptor_statement* statement, 
                         raptor_statement* partial_statement) 
{
  if(partial_statement->subject) {
    if(!raptor_node_equals(statement->subject, statement->subject_type, NULL, NULL,
                           partial_statement->subject, partial_statement->subject_type, NULL, NULL))
      return 0;
  }

  if(partial_statement->predicate) {
    if(!raptor_node_equals(statement->predicate, statement->predicate_type, NULL, NULL,
                           partial_statement->predicate, partial_statement->predicate_type, NULL, NULL))
      return 0;
  }

  if(partial_statement->object) {
    if(!raptor_node_equals(statement->object,
                           statement->object_type,
                           statement->object_literal_datatype,
                           statement->object_literal_language,
                           partial_statement->object, 
                           partial_statement->object_type,
                           partial_statement->object_literal_datatype,
                           partial_statement->object_literal_language))
      return 0;
  }

  return 1;
}



static void
rasqal_literal_to_raptor_subject(rasqal_literal *l, raptor_statement *s) 
{
  switch(l->type) {
    case RASQAL_LITERAL_URI:
      s->subject_type=RAPTOR_IDENTIFIER_TYPE_RESOURCE;
      s->subject=(void*)raptor_uri_copy(l->value.uri);
      break;

    case RASQAL_LITERAL_BLANK:
      s->subject_type=RAPTOR_IDENTIFIER_TYPE_ANONYMOUS;
      s->subject=(char*)RASQAL_MALLOC(cstring, strlen(l->string)+1);
      strcpy((char*)s->subject, l->string);
      break;

    default:
      abort();
  }
}


static void
rasqal_literal_to_raptor_predicate(rasqal_literal *l, raptor_statement *s) 
{
  if(l->type ==RASQAL_LITERAL_URI) {
    s->predicate_type=RAPTOR_IDENTIFIER_TYPE_PREDICATE;
    s->predicate=(void*)raptor_uri_copy(l->value.uri);
  } else
    abort();
}


static void
rasqal_literal_to_raptor_object(rasqal_literal *l, raptor_statement *s) 
{
  switch(l->type) {
    case RASQAL_LITERAL_URI:
      s->object_type=RAPTOR_IDENTIFIER_TYPE_RESOURCE;
      s->object=(void*)raptor_uri_copy(l->value.uri);
      break;

    case RASQAL_LITERAL_BLANK:
      s->object_type=RAPTOR_IDENTIFIER_TYPE_ANONYMOUS;
      s->object=(char*)RASQAL_MALLOC(cstring, strlen(l->string)+1);
      strcpy((char*)s->object, l->string);
      break;

    case RASQAL_LITERAL_STRING:
      s->object_type=RAPTOR_IDENTIFIER_TYPE_LITERAL;
      s->object=(char*)RASQAL_MALLOC(cstring, strlen(l->string)+1);
      strcpy((char*)s->object, l->string);
      if(l->language) {
        s->object_literal_language=(char*)RASQAL_MALLOC(cstring, strlen(l->language)+1);
        strcpy((char*)s->object_literal_language, l->language);
      }
      if(l->datatype)
        s->object_literal_datatype=raptor_uri_copy(l->datatype);
      break;
    default:
      abort();
  }
}


static void
rasqal_raptor_next_match(struct rasqal_triples_match_s* rtm, void *user_data)
{
  rasqal_raptor_triples_match_context* rtmc=(rasqal_raptor_triples_match_context*)rtm->user_data;

  while(rtmc->cur) {
    rtmc->cur=rtmc->cur->next;
    if(rtmc->cur &&
       raptor_statement_compare(rtmc->cur->statement, &rtmc->match))
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

  rtmc->cur=rtsc->head;

  /* at least one of the triple terms is a variable and we need to
   * do a triplesMatching() over the list of stored raptor_statements
   */

  if((var=rasqal_literal_as_variable(t->subject))) {
    if(var->value)
      rasqal_literal_to_raptor_subject(var->value, &rtmc->match);
  } else
    rasqal_literal_to_raptor_subject(t->subject, &rtmc->match);

  m->bindings[0]=var;
  

  if((var=rasqal_literal_as_variable(t->predicate))) {
    if(var->value)
      rasqal_literal_to_raptor_predicate(var->value, &rtmc->match);
  } else
    rasqal_literal_to_raptor_predicate(t->predicate, &rtmc->match);

  m->bindings[1]=var;
  

  if((var=rasqal_literal_as_variable(t->object))) {
    if(var->value)
      rasqal_literal_to_raptor_object(var->value, &rtmc->match);
  } else
    rasqal_literal_to_raptor_object(t->object, &rtmc->match);

  m->bindings[2]=var;
  

#ifdef RASQAL_DEBUG
  RASQAL_DEBUG1("query statement: ");
  raptor_print_statement(&rtmc->match, stderr);
  fputc('\n', stderr);
#endif

  while(rtmc->cur) {
    if(raptor_statement_compare(rtmc->cur->statement, &rtmc->match))
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

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_engine.c - Rasqal query engine
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



typedef struct {
  int seen;
  int score;
#ifdef RASQAL_DEBUG
  int original_i;
#endif
} rq_ordering;


static rasqal_triple*
rasqal_query_order_triples_score(rasqal_query* query,
                                 rq_ordering* ord,
                                 int* position) {
  int i;
  rasqal_triple* nt=NULL;
  int triples_size=rasqal_sequence_size(query->triples);
  int next_score=0;
  int next_i= -1;
  
#if RASQAL_DEBUG >2
  RASQAL_DEBUG1("scoring triples\n");
#endif

  for(i=0; i< triples_size; i++) {
    rasqal_triple* t=rasqal_sequence_get_at(query->triples, i);
    rasqal_variable* v;

    if((v=rasqal_expression_as_variable(t->subject))) {
#if RASQAL_DEBUG >2
      RASQAL_DEBUG3("triple %i: has variable %s in subject\n", i, v->name);
#endif
      if(rasqal_query_has_variable(query, v->name))
        ord[i].score++;
    }

    if((v=rasqal_expression_as_variable(t->predicate))) {
#if RASQAL_DEBUG >2
      RASQAL_DEBUG3("triple %i: has variable %s in predicate\n", i, v->name);
#endif
      if(rasqal_query_has_variable(query, v->name))
        ord[i].score++;
    }

    if((v=rasqal_expression_as_variable(t->object))) {
#if RASQAL_DEBUG >2
      RASQAL_DEBUG3("triple %i: has variable %s in predicate\n", i, v->name);
#endif
      if(rasqal_query_has_variable(query, v->name))
        ord[i].score++;
    }

#if RASQAL_DEBUG >2
    RASQAL_DEBUG3("triple %i: score %d\n", i, ord[i].score);
#endif
  }

  /* Pick triples providing least result variables first */
  for(i=0; i< triples_size; i++) {
    rasqal_triple* t=rasqal_sequence_get_at(query->triples, i);
    if(!ord[i].seen && (!nt || (ord[i].score < next_score))) {
#if RASQAL_DEBUG >2
      RASQAL_DEBUG4("chose triple %i with score %d, less than %d\n", 
             i, ord[i].score, next_score);
#endif
      nt=t;
      next_score=ord[i].score;
      next_i=i;
    }
  }

#if RASQAL_DEBUG >2
  RASQAL_DEBUG3("final choice triple %i with score %d\n", 
         next_i, next_score);
#endif
  *position=next_i;

  return nt;
}


int
rasqal_query_order_triples(rasqal_query* query) {
  int i;
  rq_ordering* ord;
  int triples_size=rasqal_sequence_size(query->triples);
#ifdef RASQAL_DEBUG
  int ordering_changed=0;
#endif

  if(query->ordered_triples)
    rasqal_free_sequence(query->ordered_triples);

  /* NOTE: Shared triple pointers with the query->triples - 
   * entries in the ordered_triples sequence are not freed 
   */
  query->ordered_triples=rasqal_new_sequence(NULL, (rasqal_print_handler*)rasqal_print_triple);

  if(triples_size == 1) {
    rasqal_sequence_push(query->ordered_triples, 
                         rasqal_sequence_get_at(query->triples, 0));
    return 0;
  }
  

  ord=(rq_ordering*)RASQAL_CALLOC(intarray, sizeof(rq_ordering), triples_size);
  if(!ord)
    return 1;

  for(i=0; i<triples_size; i++) {
    int original_i;
    rasqal_triple *nt =rasqal_query_order_triples_score(query, ord, &original_i);
    if(!nt)
      break;

    rasqal_sequence_push(query->ordered_triples, nt);

    ord[original_i].seen=1;
#ifdef RASQAL_DEBUG
    ord[i].original_i=original_i;
    if(i != original_i)
      ordering_changed=1;
#endif
  }

#ifdef RASQAL_DEBUG
  if(ordering_changed) {
    RASQAL_DEBUG1("triple ordering changed\n");
    for(i=0; i<triples_size; i++)
      RASQAL_DEBUG3("triple %i: was %i\n", i, ord[i].original_i);
  } else
    RASQAL_DEBUG1("triple ordering was not changed\n");
    
#endif  

  RASQAL_FREE(ordarray, ord);

  return 0;
}



int
rasqal_engine_declare_prefixes(rasqal_query *rq) 
{
  int i;
  
  if(!rq->prefixes)
    return 0;
  
  for(i=0; i< rasqal_sequence_size(rq->prefixes); i++) {
    rasqal_prefix* p=rasqal_sequence_get_at(rq->prefixes, i);

    if(raptor_namespaces_start_namespace_full(rq->namespaces, 
                                              p->prefix, 
                                              raptor_uri_as_string(p->uri),
                                              0))
      return 1;
  }

  return 0;
}
 

int
rasqal_engine_expand_triple_qnames(rasqal_query* rq)
{
  int i;
  
  /* expand qnames in triples */
  for(i=0; i< rasqal_sequence_size(rq->triples); i++) {
    rasqal_triple* t=rasqal_sequence_get_at(rq->triples, i);

    if(t->subject->op == RASQAL_EXPR_LITERAL &&
       t->subject->literal->type == RASQAL_LITERAL_QNAME) {
      rasqal_literal *l=t->subject->literal;
      raptor_uri *uri=raptor_qname_string_to_uri(rq->namespaces,
                                                 l->value.string, 
                                                 strlen(l->value.string),
                                                 rasqal_query_simple_error, rq);
      if(!uri)
        return 1;
      RASQAL_FREE(cstring, l->value.string);
      l->type=RASQAL_LITERAL_URI;
      l->value.uri=uri; /* uri field is unioned with string field */
    }

    if(t->predicate->op == RASQAL_EXPR_LITERAL &&
       t->predicate->literal->type == RASQAL_LITERAL_QNAME) {
      rasqal_literal *l=t->predicate->literal;
      raptor_uri *uri=raptor_qname_string_to_uri(rq->namespaces,
                                                 l->value.string, 
                                                 strlen(l->value.string),
                                                 rasqal_query_simple_error, rq);
      if(!uri)
        return 1;
      RASQAL_FREE(cstring, l->value.string);
      l->type=RASQAL_LITERAL_URI;
      l->value.uri=uri; /* uri field is unioned with string field */
    }

    if(t->object->op == RASQAL_EXPR_LITERAL &&
       t->object->literal->type == RASQAL_LITERAL_QNAME) {
      rasqal_literal *l=t->object->literal;
      raptor_uri *uri=raptor_qname_string_to_uri(rq->namespaces,
                                                 l->value.string, 
                                                 strlen(l->value.string),
                                                 rasqal_query_simple_error, rq);
      if(!uri)
        return 1;
      RASQAL_FREE(cstring, l->value.string);
      l->type=RASQAL_LITERAL_URI;
      l->value.uri=uri; /* uri field is unioned with string field */
    }

  }

  return 0;
}

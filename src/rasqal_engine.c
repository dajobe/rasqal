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


int
rasqal_engine_assign_variables(rasqal_query* rq)
{
  int i;

  /* If 'SELECT *' was given, make the selects be a list of all variables */
  if(rq->select_all) {
    rq->selects=rasqal_new_sequence(NULL, (rasqal_print_handler*)rasqal_print_variable);
    
    for(i=0; i< rq->variables_count; i++)
      rasqal_sequence_push(rq->selects, rasqal_sequence_get_at(rq->variables_sequence, i));
  }
  
  rq->select_variables_count=rasqal_sequence_size(rq->selects);

  rq->variables=(rasqal_variable**)RASQAL_MALLOC(varrary, sizeof(rasqal_variable*)*rq->variables_count);

  for(i=0; i< rq->variables_count; i++)
    rq->variables[i]=(rasqal_variable*)rasqal_sequence_get_at(rq->variables_sequence, i);

  return 0;
}
  


static librdf_node*
rasqal_expression_to_redland_node(rasqal_query *q, rasqal_expression* e) {
  if(e->op == RASQAL_EXPR_LITERAL) {
    rasqal_literal* l=e->literal;
    if(l->type == RASQAL_LITERAL_URI) {
      char *uri_string=raptor_uri_as_string(l->value.uri);
      return librdf_new_node_from_uri_string(q->world, uri_string);
    } else if (l->type == RASQAL_LITERAL_STRING)
      return librdf_new_node_from_literal(q->world, l->value.string, NULL, 0);
    else if (l->type == RASQAL_LITERAL_BLANK)
      return librdf_new_node_from_blank_identifier(q->world, l->value.string);
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


static librdf_statement*
rasqal_triple_to_redland_statement(rasqal_query *q, rasqal_triple* t)
{
  librdf_node* nodes[3];

  /* ASSUMPTION: all the parts of the triple are not variables */
  /* FIXME: and no error checks */
  nodes[0]=rasqal_expression_to_redland_node(q, t->subject);
  nodes[1]=rasqal_expression_to_redland_node(q, t->predicate);
  nodes[2]=rasqal_expression_to_redland_node(q, t->object);

  return librdf_new_statement_from_nodes(q->world, nodes[0], nodes[1], nodes[2]);
}

static int
rasqal_triple_present(rasqal_query *query, rasqal_triple *t) 
{
  librdf_statement *s=rasqal_triple_to_redland_statement(query, t);
  
  int rc=!librdf_model_contains_statement(query->model, s);
  librdf_free_statement(s);
  return rc;
}


static librdf_statement*
rasqal_redland_get_match(struct rasqal_triples_match_s* rtm,
                         void *user_data) 
{
  rasqal_triple_meta *m=(rasqal_triple_meta *)user_data;
  
  librdf_statement* statement=librdf_stream_get_object(m->stream);
#ifdef RASQAL_DEBUG
  RASQAL_DEBUG1("  matched statement ");
  librdf_statement_print(statement, stdout);
  fputc('\n', stdout);
#endif

  return statement;
}


static void
rasqal_redland_next_match(struct rasqal_triples_match_s* rtm,
                          void *user_data)
{
  rasqal_triple_meta *m=(rasqal_triple_meta *)user_data;
  
  librdf_stream_next(m->stream);
}

static int
rasqal_redland_is_end(struct rasqal_triples_match_s* rtm,
                      void *user_data)
{
  rasqal_triple_meta *m=(rasqal_triple_meta *)user_data;
  
  return librdf_stream_end(m->stream);
}


static void
rasqal_redland_finish_triples_match(struct rasqal_triples_match_s* rtm,
                                    void *user_data) {
  rasqal_triple_meta *m=(rasqal_triple_meta *)user_data;
  
  /* FIXME leak nodes[0..2]? */
  if(m->stream) {
    librdf_free_stream(m->stream);
    m->stream=NULL;
  }
  librdf_free_statement(m->qstatement);
}



static rasqal_triples_match*
rasqal_new_triples_match(rasqal_query *query, void *user_data,
                         rasqal_triple_meta *m, rasqal_triple *t) {
  rasqal_triples_match *rtm;
  rasqal_variable* var;

  rtm=(rasqal_triples_match *)RASQAL_CALLOC(rasqal_triples_match, sizeof(rasqal_triples_match), 1);
  rtm->get_match=rasqal_redland_get_match;
  rtm->next_match=rasqal_redland_next_match;
  rtm->is_end=rasqal_redland_is_end;
  rtm->finish=rasqal_redland_finish_triples_match;
  rtm->user_data=user_data;

  /* at least one of the triple terms is a variable and we need to
   * do a triplesMatching() aka librdf_model_find_statements
   *
   * redland find_statements will do the right thing and internally
   * pick the most efficient, indexed way to get the answer.
   */

  if((var=rasqal_expression_as_variable(t->subject))) {
    if(var->value)
      m->nodes[0]=rasqal_expression_to_redland_node(query, var->value);
    else
      m->nodes[0]=NULL;
  } else
    m->nodes[0]=rasqal_expression_to_redland_node(query, t->subject);

  m->bindings[0]=var;
  

  if((var=rasqal_expression_as_variable(t->predicate))) {
    if(var->value)
      m->nodes[1]=rasqal_expression_to_redland_node(query, var->value);
    else
      m->nodes[1]=NULL;
  } else
    m->nodes[1]=rasqal_expression_to_redland_node(query, t->predicate);

  m->bindings[1]=var;
  

  if((var=rasqal_expression_as_variable(t->object))) {
    if(var->value)
      m->nodes[2]=rasqal_expression_to_redland_node(query, var->value);
    else
      m->nodes[2]=NULL;
  } else
    m->nodes[2]=rasqal_expression_to_redland_node(query, t->object);

  m->bindings[2]=var;
  

  m->qstatement=librdf_new_statement_from_nodes(query->world, 
                                                m->nodes[0],
                                                m->nodes[1], 
                                                m->nodes[2]);
  if(!m->qstatement)
    return NULL;

#ifdef RASQAL_DEBUG
  RASQAL_DEBUG1("query statement: ");
  librdf_statement_print(m->qstatement, stdout);
  fputc('\n', stdout);
#endif
  
  m->stream=librdf_model_find_statements(query->model, m->qstatement);

  RASQAL_DEBUG1("rasqal_new_triples_match done\n");

  return rtm;
}


static void
rasqal_free_triples_match(rasqal_triples_match* rtm) {
  rtm->finish(rtm, rtm->user_data);
  RASQAL_FREE(rasqal_triples_match, rtm);
}


/* methods */
static librdf_statement*
rasqal_triples_match_get_match(struct rasqal_triples_match_s* rtm) {
  return rtm->get_match(rtm,  rtm->user_data);
}

static void
rasqal_triples_match_next_match(struct rasqal_triples_match_s* rtm) {
  rtm->next_match(rtm,  rtm->user_data);
}

static int
rasqal_triples_match_is_end(struct rasqal_triples_match_s* rtm) {
  return rtm->is_end(rtm,  rtm->user_data);
}


int
rasqal_engine_run(rasqal_query *query) {
  int i;
  int triples_size=rasqal_sequence_size(query->triples);
  int column=0;
  int rc=0;
  
  query->triple_meta=(rasqal_triple_meta*)RASQAL_CALLOC(rasqal_triple_meta, sizeof(rasqal_triple_meta), triples_size);
  if(!query->triple_meta)
    return 1;
  
  for(i=0; i<triples_size; i++) {
    rasqal_triple *t=rasqal_sequence_get_at(query->triples, i);

    query->triple_meta[i].is_exact=
      !(rasqal_expression_as_variable(t->predicate) ||
       rasqal_expression_as_variable(t->subject) ||
       rasqal_expression_as_variable(t->object));
  }


  while(column >= 0) {
    librdf_statement* statement;
    rasqal_triple_meta *m=&query->triple_meta[column];
    rasqal_triple *t=rasqal_sequence_get_at(query->triples, column);

    RASQAL_DEBUG2("column %d\n", column);

    if(query->abort)
      break;

    if (m->is_exact) {
      /* exact triple match wanted */
      if(!rasqal_triple_present(query, t))
        /* failed */
        column--;
      RASQAL_DEBUG2("exact match OK for column %d\n", column);
    } else if(!m->triples_match) {
      /* Column has no triplesMatch so create a new query */
      m->triples_match=rasqal_new_triples_match(query, m, m, t);
      if(!m->triples_match) {
        RASQAL_DEBUG2("failed to make new triplesMatch for column %d\n", column);
        /* failed to match */
        column--;
        break;
      }
      RASQAL_DEBUG2("made new triplesMatch for column %d\n", column);
    }


    if(m->triples_match) {
      if(rasqal_triples_match_is_end(m->triples_match)) {
        RASQAL_DEBUG2("end of triplesMatch for column %d\n", column);

        if(m->bindings[0]) 
          rasqal_variable_set_value(m->bindings[0],  NULL);
        if(m->bindings[1]) 
          rasqal_variable_set_value(m->bindings[1],  NULL);
        if(m->bindings[2]) 
          rasqal_variable_set_value(m->bindings[2],  NULL);

        rasqal_free_triples_match(m->triples_match);
        
        column--;
        continue;
      }

      statement=rasqal_triples_match_get_match(m->triples_match);

      /* set 1 or 2 variable values from the fields of statement */
      if(m->bindings[0]) {
        RASQAL_DEBUG2("column %d: binding subject to variable\n", column);
        rasqal_variable_set_value(m->bindings[0], 
                                  redland_node_to_rasqal_expression(librdf_statement_get_subject(statement)));
      }
      if(m->bindings[1])  {
        RASQAL_DEBUG2("column %d: binding predicate to variable\n", column);
        rasqal_variable_set_value(m->bindings[1], 
                                  redland_node_to_rasqal_expression((librdf_statement_get_predicate(statement))));
      }
      if(m->bindings[2])  {
        RASQAL_DEBUG2("column %d: binding object to variable\n", column);
        rasqal_variable_set_value(m->bindings[2], 
                                  redland_node_to_rasqal_expression((librdf_statement_get_object(statement))));
      }

      rasqal_triples_match_next_match(m->triples_match);
    }
    
    if(column == (triples_size-1)) {
      /* Done all conjunctions, so print out the variable bindings */ 
#ifdef RASQAL_DEBUG
      RASQAL_DEBUG1("result variable bindings:\n  ");
      rasqal_sequence_print(query->selects, stdout);
      fputc('\n', stdout);
#endif
      /* exact match, so column must have ended */
      if(m->is_exact)
        column--;
    } else
      column++;

  }
  
  RASQAL_FREE(rasqal_triple_meta, query->triple_meta);
  query->triple_meta=NULL;

  return rc;
}

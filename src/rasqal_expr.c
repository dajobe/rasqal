/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_expr.c - Rasqal general expression support
 *
 * $Id$
 *
 * Copyright (C) 2003-2004 David Beckett - http://purl.org/net/dajobe/
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

#ifdef RASQAL_REGEX_PCRE
#include <pcre.h>
#endif

#ifdef RASQAL_REGEX_POSIX
#include <sys/types.h>
#include <regex.h>
#endif

#include "rasqal.h"
#include "rasqal_internal.h"



inline int rasqal_literal_as_integer(rasqal_literal* l);
inline int rasqal_literal_compare(rasqal_literal* l1, rasqal_literal *l2, int *error);

inline int rasqal_variable_as_boolean(rasqal_variable* v);
inline int rasqal_variable_as_integer(rasqal_variable* v);
inline int rasqal_variable_compare(rasqal_variable* v1, rasqal_variable* v2, int *error);

inline int rasqal_expression_as_boolean(rasqal_expression* e);
inline int rasqal_expression_as_integer(rasqal_expression* e);
inline int rasqal_expression_compare(rasqal_expression* e1, rasqal_expression* e2, int *error);


rasqal_literal*
rasqal_new_integer_literal(rasqal_literal_type type, int integer) {
  rasqal_literal* l=(rasqal_literal*)calloc(sizeof(rasqal_literal), 1);

  l->type=type;
  l->value.integer=integer;
  l->usage=1;
  return l;
}


rasqal_literal*
rasqal_new_floating_literal(float floating) {
  rasqal_literal* l=(rasqal_literal*)calloc(sizeof(rasqal_literal), 1);

  l->type=RASQAL_LITERAL_FLOATING;
  l->value.floating=floating;
  l->usage=1;
  return l;
}


rasqal_literal*
rasqal_new_uri_literal(raptor_uri *uri) {
  rasqal_literal* l=(rasqal_literal*)calloc(sizeof(rasqal_literal), 1);

  l->type=RASQAL_LITERAL_URI;
  l->value.uri=uri;
  l->usage=1;
  return l;
}


rasqal_literal*
rasqal_new_pattern_literal(char *pattern, char *flags) {
  rasqal_literal* l=(rasqal_literal*)calloc(sizeof(rasqal_literal), 1);

  l->type=RASQAL_LITERAL_PATTERN;
  l->value.string=pattern;
  l->flags=flags;
  l->usage=1;
  return l;
}


rasqal_literal*
rasqal_new_string_literal(char *string, char *language,
                          raptor_uri *datatype, char *datatype_qname) {
  rasqal_literal* l=(rasqal_literal*)calloc(sizeof(rasqal_literal), 1);

  l->type=RASQAL_LITERAL_STRING;
  l->value.string=string;
  l->datatype=datatype;
  l->flags=datatype_qname;
  l->usage=1;
  return l;
}


/* used for BLANK and QNAME */
rasqal_literal*
rasqal_new_simple_literal(rasqal_literal_type type, char *string) {
  rasqal_literal* l=(rasqal_literal*)calloc(sizeof(rasqal_literal), 1);

  l->type=type;
  l->value.string=string;
  l->usage=1;
  return l;
}


rasqal_literal*
rasqal_new_boolean_literal(int value) {
  rasqal_literal* l=(rasqal_literal*)calloc(sizeof(rasqal_literal), 1);

  l->type=RASQAL_LITERAL_BOOLEAN;
  l->value.integer=value;
  l->usage=1;
  return l;
}


rasqal_literal*
rasqal_new_variable_literal(rasqal_variable *variable)
{
  rasqal_literal* l=(rasqal_literal*)calloc(sizeof(rasqal_literal), 1);
  l->type=RASQAL_LITERAL_VARIABLE;
  l->value.variable=variable;
  l->usage=1;
  return l;
}


rasqal_literal*
rasqal_new_literal_from_literal(rasqal_literal* l) {
  l->usage++;
  return l;
}


void
rasqal_free_literal(rasqal_literal* l) {
  if(--l->usage)
    return;
  
  switch(l->type) {
    case RASQAL_LITERAL_URI:
      if(l->value.uri)
        raptor_free_uri(l->value.uri);
      break;
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
      if(l->value.string)
        free(l->value.string);
      if(l->language)
        free(l->language);
      if(l->datatype)
        raptor_free_uri(l->datatype);
      break;
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_FLOATING:
      break;
    case RASQAL_LITERAL_VARIABLE:
      /* It is correct that this is not called here
       * since all variables are shared and owned by
       * the rasqal_query sequence variables_sequence */

      /* rasqal_free_variable(l->value.variable); */
      break;

    default:
      abort();
  }
  free(l);
}


static const char* rasqal_literal_type_labels[]={
  "UNKNOWN",
  "uri",
  "qname",
  "string",
  "blank",
  "pattern",
  "boolean",
  "integer",
  "floating",
  "variable"
};


static void
rasqal_literal_print_type(rasqal_literal* literal, FILE* fh)
{
  rasqal_literal_type type;

  if(!literal) {
    fputs("null", fh);
    return;
  }
  
  type=literal->type;
  if(type > RASQAL_LITERAL_LAST)
    type=RASQAL_LITERAL_UNKNOWN;
  fputs(rasqal_literal_type_labels[(int)type], fh);
}


void
rasqal_literal_print(rasqal_literal* l, FILE* fh)
{
  if(!l) {
    fputs("null", fh);
    return;
  }

  if(l->type != RASQAL_LITERAL_VARIABLE)
    rasqal_literal_print_type(l, fh);

  switch(l->type) {
    case RASQAL_LITERAL_URI:
      fprintf(fh, "<%s>", raptor_uri_as_string(l->value.uri));
      break;
    case RASQAL_LITERAL_BLANK:
      fprintf(fh, " %s", l->value.string);
      break;
    case RASQAL_LITERAL_PATTERN:
      fprintf(fh, "/%s/%s", l->value.string, l->flags ? l->flags : "");
      break;
    case RASQAL_LITERAL_STRING:
      fprintf(fh, "(\"%s\"", l->value.string);
      if(l->language)
        fprintf(fh, "@%s", l->language);
      if(l->datatype)
        fprintf(fh, "^^<%s>", raptor_uri_as_string(l->datatype));
      fputc(')', fh);
      break;
    case RASQAL_LITERAL_QNAME:
      fprintf(fh, "(%s)", l->value.string);
      break;
    case RASQAL_LITERAL_INTEGER:
      fprintf(fh, " %d", l->value.integer);
      break;
    case RASQAL_LITERAL_BOOLEAN:
      if(l->value.integer)
        fputs("boolean(true)", fh);
      else
        fputs("boolean(false)", fh);
      break;
    case RASQAL_LITERAL_FLOATING:
      fprintf(fh, " %f", l->value.floating);
      break;
    case RASQAL_LITERAL_VARIABLE:
      rasqal_variable_print(l->value.variable, fh);
      break;
    default:
      abort();
  }
}



int
rasqal_literal_as_boolean(rasqal_literal* l)
{
  if(!l)
    return 0;
  
  switch(l->type) {
    case RASQAL_LITERAL_URI:
      return (l->value.uri) != NULL;
      break;
      
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
      return (l->value.string) != NULL;
      break;

    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_BOOLEAN:
      return l->value.integer != 0;
      break;

    case RASQAL_LITERAL_FLOATING:
      return l->value.floating != 0.0;
      break;

    case RASQAL_LITERAL_VARIABLE:
      return rasqal_variable_as_boolean(l->value.variable);
      break;

    default:
      abort();
  }
}

inline int
rasqal_literal_as_integer(rasqal_literal* l)
{
  if(!l)
    return 0;
  
  switch(l->type) {
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_BOOLEAN:
      return l->value.integer != 0;
      break;
    case RASQAL_LITERAL_FLOATING:
      return (int)l->value.floating;
      break;

    case RASQAL_LITERAL_STRING:
      return (int)atoi(l->value.string);
      break;

    case RASQAL_LITERAL_VARIABLE:
      return rasqal_variable_as_integer(l->value.variable);
      break;

    default:
      abort();
  }
}


char*
rasqal_literal_as_string(rasqal_literal* l)
{
  static char buf[32]; /* fixme */

  if(!l)
    return NULL;
  
  switch(l->type) {
    case RASQAL_LITERAL_INTEGER:
      sprintf(buf, "%d", l->value.integer);
      return buf;
      
    case RASQAL_LITERAL_BOOLEAN:
      return l->value.integer ? "true" :"false";

    case RASQAL_LITERAL_FLOATING:
      sprintf(buf, "%f", l->value.floating);
      return buf;

    case RASQAL_LITERAL_STRING:
      return l->value.string;

    case RASQAL_LITERAL_URI:
      return raptor_uri_as_string(l->value.uri);

    case RASQAL_LITERAL_VARIABLE:
      return rasqal_literal_as_string(l->value.variable->value);

    default:
      abort();
  }
}


inline rasqal_variable*
rasqal_literal_as_variable(rasqal_literal* l) {
  return (l->type == RASQAL_LITERAL_VARIABLE) ? l->value.variable : NULL;
}


inline int
rasqal_literal_compare(rasqal_literal* l1, rasqal_literal* l2, int *error)
{
  *error=0;

  /* null literals */
  if(!l1 || !l2) {
    /* if either is not null, the comparison fails */
    if(l1 || l2)
      *error=1;
    return 0;
  }

  if(l1->type == RASQAL_LITERAL_VARIABLE)
    l1=l1->value.variable->value;
    
  if(l2->type == RASQAL_LITERAL_VARIABLE)
    l2=l2->value.variable->value;
    
  if(l1->type != l2->type) {
    if(!l1->type != RASQAL_LITERAL_INTEGER &&
       l2->type == RASQAL_LITERAL_INTEGER)
      return l2->value.integer - rasqal_literal_as_integer(l1);

    if(!l2->type != RASQAL_LITERAL_INTEGER &&
       l1->type == RASQAL_LITERAL_INTEGER)
      return rasqal_literal_as_integer(l2) - l1->value.integer;

    *error=1;
    return 0;
  }

  switch(l1->type) {
    case RASQAL_LITERAL_URI:
      return !raptor_uri_equals(l1->value.uri,l2->value.uri);

    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
      return strcmp(l1->value.string,l2->value.string);

    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_BOOLEAN:
      return l2->value.integer - l1->value.integer;
      break;

    case RASQAL_LITERAL_FLOATING:
      return l2->value.floating - l1->value.floating;
      break;

    default:
      abort();
  }
}


rasqal_variable*
rasqal_new_variable(rasqal_query* rq,
                    const char *name, rasqal_literal *value) 
{
  int i;
  rasqal_variable* v;
  
  for(i=0; i< raptor_sequence_size(rq->variables_sequence); i++) {
    v=(rasqal_variable*)raptor_sequence_get_at(rq->variables_sequence, i);
    if(!strcmp(v->name, name)) {
      /* name already present, do not need a copy */
      RASQAL_FREE(cstring, name);
      return v;
    }
  }
    
  v=(rasqal_variable*)calloc(sizeof(rasqal_variable), 1);

  v->name=name;
  v->value=value;
  v->offset=rq->variables_count++;

  raptor_sequence_push(rq->variables_sequence, v);
  
  return v;
}


void
rasqal_free_variable(rasqal_variable* v) {
  if(v->name)
    RASQAL_FREE(cstring, v->name);
  if(v->value)
    rasqal_free_literal(v->value);
  free(v);
}


void
rasqal_variable_print(rasqal_variable* v, FILE* fh)
{
  fprintf(fh, "variable(%s", v->name);
  if(v->value) {
    fputc('=', fh);
    rasqal_literal_print(v->value, fh);
  }
  fputc(')', fh);
}

inline int
rasqal_variable_as_boolean(rasqal_variable* v)
{
  return rasqal_literal_as_boolean(v->value);
}

inline int
rasqal_variable_as_integer(rasqal_variable* v)
{
  return rasqal_literal_as_integer(v->value);
}


inline int
rasqal_variable_compare(rasqal_variable* v1, rasqal_variable* v2, int *error)
{
  *error=0;
  return rasqal_literal_compare(v1->value, v2->value, error);
}

void
rasqal_variable_set_value(rasqal_variable* v, rasqal_literal *e)
{
  if(v->value)
    rasqal_free_literal(v->value);
  v->value=e;
#ifdef RASQAL_DEBUG
  RASQAL_DEBUG2("setting variable %s to value ", v->name);
  if(v->value)
    rasqal_literal_print(v->value, stderr);
  else
    fputs("(NULL)", stderr);
  fputc('\n', stderr);
#endif
}


static inline rasqal_literal*
rasqal_variable_get_value(rasqal_variable* v) {
  return v->value;
}



rasqal_prefix*
rasqal_new_prefix(const char *prefix, raptor_uri* uri) 
{
  rasqal_prefix* p=(rasqal_prefix*)RASQAL_CALLOC(rasqal_prefix,
                                                 sizeof(rasqal_prefix), 1);

  p->prefix=prefix;
  p->uri=uri;

  return p;
}


void
rasqal_free_prefix(rasqal_prefix* p) {
  if(p->prefix)
    RASQAL_FREE(cstring, p->prefix);
  raptor_free_uri(p->uri);
  RASQAL_FREE(rasqal_prefix, p);
}


void
rasqal_prefix_print(rasqal_prefix* p, FILE* fh)
{
  fprintf(fh, "prefix(%s as %s)", p->prefix, raptor_uri_as_string(p->uri));
}



rasqal_triple*
rasqal_new_triple(rasqal_literal* subject, rasqal_literal* predicate, rasqal_literal* object)
{
  rasqal_triple* t=(rasqal_triple*)calloc(sizeof(rasqal_triple), 1);

  t->subject=subject;
  t->predicate=predicate;
  t->object=object;

  return t;
}

void
rasqal_free_triple(rasqal_triple* t)
{
  rasqal_free_literal(t->subject);
  rasqal_free_literal(t->predicate);
  rasqal_free_literal(t->object);
  free(t);
}


void
rasqal_triple_print(rasqal_triple* t, FILE* fh)
{
  fputs("triple(", fh);
  rasqal_literal_print(t->subject, fh);
  fputs(", ", fh);
  rasqal_literal_print(t->predicate, fh);
  fputs(", ", fh);
  rasqal_literal_print(t->object, fh);
  fputc(')', fh);
}


rasqal_expression*
rasqal_new_1op_expression(rasqal_op op, rasqal_expression* arg)
{
  rasqal_expression* e=(rasqal_expression*)calloc(sizeof(rasqal_expression), 1);
  e->op=op;
  e->arg1=arg;
  return e;
}

rasqal_expression*
rasqal_new_2op_expression(rasqal_op op,
                          rasqal_expression* arg1, 
                          rasqal_expression* arg2)
{
  rasqal_expression* e=(rasqal_expression*)calloc(sizeof(rasqal_expression), 1);
  e->op=op;
  e->arg1=arg1;
  e->arg2=arg2;
  return e;
}

rasqal_expression*
rasqal_new_string_op_expression(rasqal_op op,
                                rasqal_expression* arg1,
                                rasqal_literal* literal)
{
  rasqal_expression* e=(rasqal_expression*)calloc(sizeof(rasqal_expression), 1);
  e->op=op;
  e->arg1=arg1;
  e->literal=literal;
  return e;
}

rasqal_expression*
rasqal_new_literal_expression(rasqal_literal *literal)
{
  rasqal_expression* e=(rasqal_expression*)calloc(sizeof(rasqal_expression), 1);
  e->op=RASQAL_EXPR_LITERAL;
  e->literal=literal;
  return e;
}


void
rasqal_free_expression(rasqal_expression* e) {
  switch(e->op) {
    case RASQAL_EXPR_EXPR:
      rasqal_free_expression(e->arg1);
      break;
    case RASQAL_EXPR_AND:
    case RASQAL_EXPR_OR:
    case RASQAL_EXPR_EQ:
    case RASQAL_EXPR_NEQ:
    case RASQAL_EXPR_LT:
    case RASQAL_EXPR_GT:
    case RASQAL_EXPR_LE:
    case RASQAL_EXPR_GE:
    case RASQAL_EXPR_PLUS:
    case RASQAL_EXPR_MINUS:
    case RASQAL_EXPR_STAR:
    case RASQAL_EXPR_SLASH:
    case RASQAL_EXPR_REM:
    case RASQAL_EXPR_STR_EQ:
    case RASQAL_EXPR_STR_NEQ:
      rasqal_free_expression(e->arg1);
      rasqal_free_expression(e->arg2);
      break;
    case RASQAL_EXPR_TILDE:
    case RASQAL_EXPR_BANG:
      rasqal_free_expression(e->arg1);
      break;
    case RASQAL_EXPR_STR_MATCH:
    case RASQAL_EXPR_STR_NMATCH:
      rasqal_free_expression(e->arg1);
      /* FALLTHROUGH */
    case RASQAL_EXPR_LITERAL:
      rasqal_free_literal(e->literal);
      break;
    default:
      abort();
  }
  free(e);
}


int
rasqal_expression_foreach(rasqal_expression* e, 
                          rasqal_expression_foreach_fn fn,
                          void *user_data) {
  switch(e->op) {
    case RASQAL_EXPR_EXPR:
      return rasqal_expression_foreach(e->arg1, fn, user_data);
      break;
    case RASQAL_EXPR_AND:
    case RASQAL_EXPR_OR:
    case RASQAL_EXPR_EQ:
    case RASQAL_EXPR_NEQ:
    case RASQAL_EXPR_LT:
    case RASQAL_EXPR_GT:
    case RASQAL_EXPR_LE:
    case RASQAL_EXPR_GE:
    case RASQAL_EXPR_PLUS:
    case RASQAL_EXPR_MINUS:
    case RASQAL_EXPR_STAR:
    case RASQAL_EXPR_SLASH:
    case RASQAL_EXPR_REM:
    case RASQAL_EXPR_STR_EQ:
    case RASQAL_EXPR_STR_NEQ:
      return fn(user_data, e) ||
        rasqal_expression_foreach(e->arg1, fn, user_data) ||
        rasqal_expression_foreach(e->arg2, fn, user_data);
      break;
    case RASQAL_EXPR_TILDE:
    case RASQAL_EXPR_BANG:
      return fn(user_data, e) ||
        rasqal_expression_foreach(e->arg1, fn, user_data);
      break;
    case RASQAL_EXPR_STR_MATCH:
    case RASQAL_EXPR_STR_NMATCH:
    case RASQAL_EXPR_LITERAL:
      return fn(user_data, e);
      break;
    default:
      abort();
  }
  free(e);
}


inline int
rasqal_expression_as_boolean(rasqal_expression* e) {
  switch(e->op) {
    case RASQAL_EXPR_EXPR:
      return rasqal_expression_as_boolean(e->arg1);

    case RASQAL_EXPR_LITERAL:
      return rasqal_literal_as_boolean(e->literal);
      break;

    default:
      abort();
  }
}


int
rasqal_expression_as_integer(rasqal_expression* e) {
  switch(e->op) {
    case RASQAL_EXPR_EXPR:
      return rasqal_expression_as_integer(e->arg1);

    case RASQAL_EXPR_LITERAL:
      return rasqal_literal_as_integer(e->literal);
      break;

    default:
      abort();
  }
}


int
rasqal_expression_compare(rasqal_expression* e1, rasqal_expression* e2,
                          int *error) {
  rasqal_literal *l1, *l2;
  *error=0;
  
  if(e1->op == RASQAL_EXPR_EXPR)
    return rasqal_expression_compare(e1->arg1, e2, error);
  if(e2->op == RASQAL_EXPR_EXPR)
    return rasqal_expression_compare(e1, e2->arg1, error);

  if(e1->op == RASQAL_EXPR_LITERAL && e1->op == e2->op)
    return rasqal_literal_compare(e1->literal, e2->literal, error);


  switch(e1->op) {
    case RASQAL_EXPR_LITERAL:
      l1=e1->literal;
      break;

    default:
      RASQAL_FATAL2("Unexpected e1 op %d\n", e1->op);
  }

  switch(e2->op) {
    case RASQAL_EXPR_LITERAL:
      l2=e2->literal;
      break;
    default:
      RASQAL_FATAL2("Unexpected e2 op %d\n", e2->op);
  }

  return rasqal_literal_compare(l1, l2, error);
}


int
rasqal_expression_is_variable(rasqal_expression* e) {
  return (e->op == RASQAL_EXPR_LITERAL &&
          e->literal->type == RASQAL_LITERAL_VARIABLE);
}


rasqal_literal*
rasqal_expression_evaluate(rasqal_query *query, rasqal_expression* e) {
  int error=0;
  
  switch(e->op) {
    case RASQAL_EXPR_EXPR:
      return rasqal_expression_evaluate(query, e->arg1);

    case RASQAL_EXPR_AND:
      {
        rasqal_literal *l;
        int b;
        
        l=rasqal_expression_evaluate(query, e->arg1);
        if(!l)
          return NULL;
        b=rasqal_literal_as_boolean(l);
        rasqal_free_literal(l);

        if(b) {
          l=rasqal_expression_evaluate(query, e->arg2);
          if(!l)
            return NULL;
          b=rasqal_literal_as_boolean(l);
          rasqal_free_literal(l);
        }
        return rasqal_new_boolean_literal(b);
      }
      
    case RASQAL_EXPR_OR:
      {
        rasqal_literal *l;
        int b;
        
        l=rasqal_expression_evaluate(query, e->arg1);
        if(!l)
          return NULL;
        b=rasqal_literal_as_boolean(l);
        rasqal_free_literal(l);

        if(!b) {
          l=rasqal_expression_evaluate(query, e->arg2);
          if(!l)
            return NULL;
          b=rasqal_literal_as_boolean(l);
          rasqal_free_literal(l);
        }
        return rasqal_new_boolean_literal(b);
      }

    case RASQAL_EXPR_EQ:
      {
        rasqal_literal *l1, *l2;
        int b;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          return NULL;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          return NULL;
        }

        b=(rasqal_literal_compare(l1, l2, &error) == 0);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error)
          return NULL;
        return rasqal_new_boolean_literal(b);
      }

    case RASQAL_EXPR_NEQ:
      {
        rasqal_literal *l1, *l2;
        int b;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          return NULL;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          return NULL;
        }

        b=(rasqal_literal_compare(l1, l2, &error) != 0);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error)
          return NULL;
        return rasqal_new_boolean_literal(b);
      }

    case RASQAL_EXPR_LT:
      {
        rasqal_literal *l1, *l2;
        int b;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          return NULL;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          return NULL;
        }

        b=(rasqal_literal_compare(l1, l2, &error) < 0);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error)
          return NULL;
        return rasqal_new_boolean_literal(b);
      }

    case RASQAL_EXPR_GT:
      {
        rasqal_literal *l1, *l2;
        int b;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          return NULL;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          return NULL;
        }

        b=(rasqal_literal_compare(l1, l2, &error) > 0);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error)
          return NULL;
        return rasqal_new_boolean_literal(b);
      }

    case RASQAL_EXPR_LE:
      {
        rasqal_literal *l1, *l2;
        int b;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          return NULL;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          return NULL;
        }

        b=(rasqal_literal_compare(l1, l2, &error) <= 0);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error)
          return NULL;
        return rasqal_new_boolean_literal(b);
      }

    case RASQAL_EXPR_GE:
      {
        rasqal_literal *l1, *l2;
        int b;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          return NULL;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          return NULL;
        }

        b=(rasqal_literal_compare(l1, l2, &error) >= 0);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error)
          return NULL;
        return rasqal_new_boolean_literal(b);
      }

    case RASQAL_EXPR_PLUS:
      {
        rasqal_literal *l1, *l2;
        int i;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          return NULL;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          return NULL;
        }

        i=rasqal_literal_as_integer(l1) + rasqal_literal_as_integer(l2);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error)
          return NULL;
        return rasqal_new_integer_literal(RASQAL_LITERAL_INTEGER, i);
      }
      
    case RASQAL_EXPR_MINUS:
      {
        rasqal_literal *l1, *l2;
        int i;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          return NULL;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          return NULL;
        }

        i=rasqal_literal_as_integer(l1) - rasqal_literal_as_integer(l2);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error)
          return NULL;
        return rasqal_new_integer_literal(RASQAL_LITERAL_INTEGER, i);
      }
      
    case RASQAL_EXPR_STAR:
      {
        rasqal_literal *l1, *l2;
        int i;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          return NULL;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          return NULL;
        }

        i=rasqal_literal_as_integer(l1) * rasqal_literal_as_integer(l2);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error)
          return NULL;
        return rasqal_new_integer_literal(RASQAL_LITERAL_INTEGER, i);
      }
      
    case RASQAL_EXPR_SLASH:
      {
        rasqal_literal *l1, *l2;
        int i;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          return NULL;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          return NULL;
        }

        i=rasqal_literal_as_integer(l1) / rasqal_literal_as_integer(l2);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error)
          return NULL;
        return rasqal_new_integer_literal(RASQAL_LITERAL_INTEGER, i);
      }
      
    case RASQAL_EXPR_REM:
      {
        rasqal_literal *l1, *l2;
        int i;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          return NULL;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          return NULL;
        }

        i=rasqal_literal_as_integer(l1) % rasqal_literal_as_integer(l2);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error)
          return NULL;
        return rasqal_new_integer_literal(RASQAL_LITERAL_INTEGER, i);
      }
      
    case RASQAL_EXPR_STR_EQ:
      {
        rasqal_literal *l1, *l2;
        int b;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          return NULL;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          return NULL;
        }

        b=(rasqal_literal_compare(l1, l2, &error) == 0);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error)
          return NULL;
        return rasqal_new_boolean_literal(b);
      }
      
    case RASQAL_EXPR_STR_NEQ:
      {
        rasqal_literal *l1, *l2;
        int b;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          return NULL;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          return NULL;
        }

        b=(rasqal_literal_compare(l1, l2, &error) != 0);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error)
          return NULL;
        return rasqal_new_boolean_literal(b);
      }

    case RASQAL_EXPR_TILDE:
      {
        rasqal_literal *l=rasqal_expression_evaluate(query, e->arg1);
        int i= ~ rasqal_literal_as_integer(l);
        rasqal_free_literal(l);
        return rasqal_new_integer_literal(RASQAL_LITERAL_INTEGER, i);
      }

    case RASQAL_EXPR_BANG:
      {
        rasqal_literal *l=rasqal_expression_evaluate(query, e->arg1);
        int b= ! rasqal_literal_as_boolean(l);
        rasqal_free_literal(l);
        return rasqal_new_boolean_literal(b);
      }

    case RASQAL_EXPR_STR_MATCH:
    case RASQAL_EXPR_STR_NMATCH: 
      {
        int b=0;
        int flag_i=0; /* flags contains i */
        char *p;
        char *match_string;
        char *pattern;
        rasqal_literal *l1, *l2;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          return NULL;
        match_string=rasqal_literal_as_string(l1);
        if(!match_string)
          return NULL;
        
        l2=e->literal;
        pattern=l2->value.string;
        
        for(p=l2->flags; p && *p; p++)
          if(*p == 'i')
            flag_i++;
          
#ifdef RASQAL_REGEX_PCRE
        pcre* re;
        int options=0;
        const char *re_error=NULL;
        int erroffset=0;
        
        if(flag_i)
          options |= PCRE_CASELESS;
        
        re=pcre_compile(pattern, options, &re_error, &erroffset, NULL);
        if(!re)
          rasqal_query_error(query, "Regex compile of '%s' failed - %s",
                             pattern, re_error);
        else {
          int rc=pcre_exec(re, 
                           NULL, /* no study */
                           match_string, strlen(match_string),
                           0 /* startoffset */,
                           0 /* options */,
                           NULL, 0 /* ovector, ovecsize - no matches wanted */
                           );
          if(rc >= 0)
            b=1;
          else if(rc != PCRE_ERROR_NOMATCH)
            rasqal_query_error(query, "Regex match failed - returned code %d", rc);
        }
        
#endif
#ifdef RASQAL_REGEX_POSIX
        regex_t reg;
        int rc;
        int options=REG_EXTENDED | REG_NOSUB;
        
        if(flag_i)
          options |=REG_ICASE;
        
        rc=regcomp(&reg, pattern, options);
        if(rc)
          rasqal_query_error(query, "Regex compile of '%s' failed", pattern);
        else {
          rc=regexec(&reg, match_string, 
                     0, NULL, /* nmatch, regmatch_t pmatch[] - no matches wanted */
                     0 /* eflags */
                     );
          if(!rc)
            b=1;
          else if (rc != REG_NOMATCH)
            rasqal_query_error(query, "Regex match failed - returned code %d", rc);
        }
        regfree(&reg);
#endif
#ifdef RASQAL_REGEX_NONE
        rasqal_query_warning(query, "Regex support missing, cannot compare '%s' to '%s'", match_string, pattern);
        b=1;
#endif

        RASQAL_DEBUG5("regex match returned %s for '%s' against '%s' (flags=%s)\n", b ? "true " : "false", match_string, pattern, l2->flags ? l2->flags : "");
        
        if(e->op == RASQAL_EXPR_STR_NMATCH)
          b=1-b;

        return rasqal_new_boolean_literal(b);
      }

    case RASQAL_EXPR_LITERAL:
      return rasqal_new_literal_from_literal(e->literal);
      break;

    default:
      abort();
  }

  return NULL;
}


static const char* rasqal_op_labels[RASQAL_EXPR_LAST+1]={
  "UNKNOWN",
  "expr",
  "and",
  "or",
  "eq",
  "neq",
  "lt",
  "gt",
  "le",
  "ge",
  "plus",
  "minus",
  "star",
  "slash",
  "rem",
  "str_eq",
  "str_ne",
  "str_match",
  "str_nmatch",
  "tilde",
  "bang",
  "literal",
  "pattern",
};

void
rasqal_expression_print_op(rasqal_expression* expression, FILE* fh)
{
  rasqal_op op=expression->op;
  if(op > RASQAL_EXPR_LAST)
    op=RASQAL_EXPR_UNKNOWN;
  fputs(rasqal_op_labels[(int)op], fh);
}


void
rasqal_expression_print(rasqal_expression* e, FILE* fh)
{
  fputs("expr(", fh);
  switch(e->op) {
    case RASQAL_EXPR_EXPR:
      rasqal_expression_print(e->arg1, fh);
      break;
    case RASQAL_EXPR_AND:
    case RASQAL_EXPR_OR:
    case RASQAL_EXPR_EQ:
    case RASQAL_EXPR_NEQ:
    case RASQAL_EXPR_LT:
    case RASQAL_EXPR_GT:
    case RASQAL_EXPR_LE:
    case RASQAL_EXPR_GE:
    case RASQAL_EXPR_PLUS:
    case RASQAL_EXPR_MINUS:
    case RASQAL_EXPR_STAR:
    case RASQAL_EXPR_SLASH:
    case RASQAL_EXPR_REM:
    case RASQAL_EXPR_STR_EQ:
    case RASQAL_EXPR_STR_NEQ:
      fputs("op ", fh);
      rasqal_expression_print_op(e, fh);
      fputc('(', fh);
      rasqal_expression_print(e->arg1, fh);
      fputs(", ", fh);
      rasqal_expression_print(e->arg2, fh);
      fputc(')', fh);
      break;
    case RASQAL_EXPR_STR_MATCH:
    case RASQAL_EXPR_STR_NMATCH:
      fputs("op ", fh);
      rasqal_expression_print_op(e, fh);
      fputc('(', fh);
      rasqal_expression_print(e->arg1, fh);
      fputs(", ", fh);
      rasqal_literal_print(e->literal, fh);
      fputc(')', fh);
      break;
    case RASQAL_EXPR_TILDE:
    case RASQAL_EXPR_BANG:
      fputs("op ", fh);
      rasqal_expression_print_op(e, fh);
      fputc('(', fh);
      rasqal_expression_print(e->arg1, fh);
      fputc(')', fh);
      break;
    case RASQAL_EXPR_LITERAL:
      rasqal_literal_print(e->literal, fh);
      break;
    case RASQAL_EXPR_PATTERN:
      fprintf(fh, "expr_pattern(%s)", (char*)e->value);
      break;
    default:
      abort();
  }
  fputc(')', fh);
}


#ifdef STANDALONE
#include <stdio.h>

int main(int argc, char *argv[]);


#define assert_match(function, result, string) do { if(strcmp(result, string)) { fprintf(stderr, #function " failed - returned %s, expected %s\n", result, string); exit(1); } } while(0)

char *program;

int
main(int argc, char *argv[]) 
{
  rasqal_literal *lit1, *lit2;
  rasqal_expression *expr1, *expr2;
  rasqal_expression* expr;
  rasqal_literal* result;
  int err;

  lit1=rasqal_new_integer_literal(RASQAL_LITERAL_INTEGER, 1);
  expr1=rasqal_new_literal_expression(lit1);
  lit2=rasqal_new_integer_literal(RASQAL_LITERAL_INTEGER, 1);
  expr2=rasqal_new_literal_expression(lit2);
  expr=rasqal_new_2op_expression(RASQAL_EXPR_PLUS, expr1, expr2);

  program=argv[0];

  fprintf(stderr, "%s: expression: ", program);
  rasqal_expression_print(expr, stderr);
  fputc('\n', stderr);

  result=rasqal_expression_evaluate(NULL, expr);

  if(result) {
    int bresult;
    
    fprintf(stderr, "%s: expression result: \n", program);
    rasqal_literal_print(result, stderr);
    fputc('\n', stderr);
    bresult=rasqal_literal_as_boolean(result);
    fprintf(stderr, "%s: boolean expression result: %d\n", program, bresult);

  } else
    fprintf(stderr, "%s: expression failed with error %d\n", program, err);

  rasqal_free_expression(expr);

  if(result)
    rasqal_free_literal(result);

  return (result == NULL);
}
#endif

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

#include "rasqal.h"
#include "rasqal_internal.h"



inline int rasqal_literal_as_boolean(rasqal_literal* literal);
inline int rasqal_literal_as_integer(rasqal_literal* l);
inline int rasqal_literal_compare(rasqal_literal* l1, rasqal_literal *l2, int *error);

inline int rasqal_variable_as_boolean(rasqal_variable* v);
inline int rasqal_variable_as_integer(rasqal_variable* v);
inline int rasqal_variable_compare(rasqal_variable* v1, rasqal_variable* v2, int *error);

inline int rasqal_expression_as_boolean(rasqal_expression* e);
inline int rasqal_expression_as_integer(rasqal_expression* e);
inline int rasqal_expression_compare(rasqal_expression* e1, rasqal_expression* e2, int *error);
rasqal_literal* rasqal_expression_evaluate(rasqal_expression* e);


rasqal_literal*
rasqal_new_literal(rasqal_literal_type type, int integer, float floating,
                   char *string, raptor_uri *uri)
{
  rasqal_literal* l=(rasqal_literal*)calloc(sizeof(rasqal_literal), 1);

  l->type=type;
  switch(type) {
    case RASQAL_LITERAL_URI:
      l->value.uri=uri;
      break;
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
      l->value.string=string;
      break;
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_NULL:
      l->value.integer=integer;
      break;
    case RASQAL_LITERAL_FLOATING:
      l->value.floating=floating;
      break;
    default:
      abort();
  }
  
  return l;
}


void
rasqal_free_literal(rasqal_literal* l) {
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
      break;
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_NULL:
    case RASQAL_LITERAL_FLOATING:
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
  "null",
  "integer",
  "floating"
};


static void
rasqal_literal_print_type(rasqal_literal* literal, FILE* fh)
{
  rasqal_literal_type type=literal->type;
  if(type > RASQAL_LITERAL_LAST)
    type=RASQAL_LITERAL_UNKNOWN;
  fputs(rasqal_literal_type_labels[(int)type], fh);
}


static void
rasqal_literal_print(rasqal_literal* l, FILE* fh)
{
  /*  fputs("literal_", fh); */
  rasqal_literal_print_type(l, fh);
  switch(l->type) {
    case RASQAL_LITERAL_URI:
      fprintf(fh, "<%s>", raptor_uri_as_string(l->value.uri));
      break;
    case RASQAL_LITERAL_BLANK:
      fprintf(fh, " %s", l->value.string);
      break;
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_PATTERN:
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
    case RASQAL_LITERAL_NULL:
      fputs("null", fh);
      break;
    case RASQAL_LITERAL_FLOATING:
      fprintf(fh, " %f", l->value.floating);
      break;
    default:
      abort();
  }
}



inline int
rasqal_literal_as_boolean(rasqal_literal* l)
{
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
    case RASQAL_LITERAL_NULL:
      return l->value.integer != 0;
      break;
    case RASQAL_LITERAL_FLOATING:
      return l->value.floating != 0.0;
      break;

    default:
      abort();
  }
}

inline int
rasqal_literal_as_integer(rasqal_literal* l)
{
  switch(l->type) {
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_NULL:
      return l->value.integer != 0;
      break;
    case RASQAL_LITERAL_FLOATING:
      return (int)l->value.floating;
      break;

    default:
      abort();
  }
}


inline int
rasqal_literal_compare(rasqal_literal* l1, rasqal_literal* l2, int *error)
{
  *error=0;
  
  if(l1->type != l2->type) {
    *error=1;
    return 0;
  }

  switch(l1->type) {
    case RASQAL_LITERAL_URI:
      return raptor_uri_equals(l1->value.uri,l2->value.uri);

    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
      return strcmp(l1->value.string,l2->value.string);

    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_NULL:
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
                    const char *name, rasqal_expression *value) 
{
  int i;
  rasqal_variable* v;
  
  for(i=0; i< rasqal_sequence_size(rq->variables_sequence); i++) {
    v=(rasqal_variable*)rasqal_sequence_get_at(rq->variables_sequence, i);
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

  rasqal_sequence_push(rq->variables_sequence, v);
  
  return v;
}


void
rasqal_free_variable(rasqal_variable* v) {
  if(v->name)
    RASQAL_FREE(cstring, v->name);
  if(v->value)
    rasqal_free_expression(v->value);
  free(v);
}


void
rasqal_variable_print(rasqal_variable* v, FILE* fh)
{
  fprintf(fh, "variable(%s", v->name);
  if(v->value) {
    fputc('=', fh);
    rasqal_expression_print(v->value, fh);
  }
  fputc(')', fh);
}

inline int
rasqal_variable_as_boolean(rasqal_variable* v)
{
  return rasqal_expression_as_boolean(v->value);
}

inline int
rasqal_variable_as_integer(rasqal_variable* v)
{
  return rasqal_expression_as_integer(v->value);
}


inline int
rasqal_variable_compare(rasqal_variable* v1, rasqal_variable* v2, int *error)
{
  *error=0;
  return rasqal_expression_compare(v1->value, v2->value, error);
}

void
rasqal_variable_set_value(rasqal_variable* v, rasqal_expression *e)
{
  if(v->value)
    rasqal_free_expression(v->value);
  v->value=e;
#ifdef RASQAL_DEBUG
  RASQAL_DEBUG2("setting variable %s to value ", v->name);
  if(v->value)
    rasqal_expression_print(v->value, stderr);
  else
    fputs("(NULL)", stderr);
  fputc('\n', stderr);
#endif
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
rasqal_new_triple(rasqal_expression* subject, rasqal_expression* predicate, rasqal_expression* object)
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
  rasqal_free_expression(t->subject);
  rasqal_free_expression(t->predicate);
  rasqal_free_expression(t->object);
  free(t);
}


void
rasqal_triple_print(rasqal_triple* t, FILE* fh)
{
  fputs("triple(", fh);
  rasqal_expression_print(t->subject, fh);
  fputs(", ", fh);
  rasqal_expression_print(t->predicate, fh);
  fputs(", ", fh);
  rasqal_expression_print(t->object, fh);
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


rasqal_expression*
rasqal_new_variable_expression(rasqal_variable *variable)
{
  rasqal_expression* e=(rasqal_expression*)calloc(sizeof(rasqal_expression), 1);
  e->op=RASQAL_EXPR_VARIABLE;
  e->variable=variable;
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
    case RASQAL_EXPR_VARIABLE:
      /* It is correct that this is not called here
       * since all variables are shared and owned by
       * the rasqal_query sequence variables_sequence */

      /* rasqal_free_variable(e->variable); */
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

    case RASQAL_EXPR_VARIABLE:
      return rasqal_variable_as_boolean(e->variable);
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

    case RASQAL_EXPR_VARIABLE:
      return rasqal_variable_as_integer(e->variable);
      break;

    default:
      abort();
  }
}


rasqal_variable*
rasqal_expression_as_variable(rasqal_expression* e) {
  switch(e->op) {
    case RASQAL_EXPR_EXPR:
      return rasqal_expression_as_variable(e->arg1);
      break;

    case RASQAL_EXPR_VARIABLE:
      return e->variable;
      break;

    default:
      break;

  }

  return NULL;
}


int
rasqal_expression_compare(rasqal_expression* e1, rasqal_expression* e2,
                          int *error) {
  *error=0;
  
  if(e1->op == RASQAL_EXPR_EXPR)
    return rasqal_expression_compare(e1->arg1, e2, error);
  if(e2->op == RASQAL_EXPR_EXPR)
    return rasqal_expression_compare(e1, e2->arg1, error);

  if(e1->op != e2->op) {
    *error=1;
    return 0;
  }
  
  switch(e1->op) {
    case RASQAL_EXPR_LITERAL:
      return rasqal_literal_compare(e1->literal, e1->literal, error);

    case RASQAL_EXPR_VARIABLE:
      return rasqal_variable_compare(e1->variable, e2->variable, error);

    default:
      abort();
  }
}


int
rasqal_expression_is_variable(rasqal_expression* e) {
  return (e->op == RASQAL_EXPR_VARIABLE);
}


rasqal_literal*
rasqal_expression_evaluate(rasqal_expression* e) {
  int error=0;
  
  switch(e->op) {
    case RASQAL_EXPR_EXPR:
      return rasqal_expression_evaluate(e->arg1);

    case RASQAL_EXPR_AND:
      {
        int b=rasqal_expression_as_boolean(e->arg1) &&
              rasqal_expression_as_boolean(e->arg2);
        return rasqal_new_literal(RASQAL_LITERAL_BOOLEAN, b, 0.0, NULL, NULL);
      }
      
    case RASQAL_EXPR_OR:
      {
        int b=rasqal_expression_as_boolean(e->arg1) ||
              rasqal_expression_as_boolean(e->arg2);
        return rasqal_new_literal(RASQAL_LITERAL_BOOLEAN, b, 0.0, NULL, NULL);
      }

    case RASQAL_EXPR_EQ:
      {
        int b=(rasqal_expression_compare(e->arg1, e->arg2, &error) == 0);
        if(error)
          return NULL;
        return rasqal_new_literal(RASQAL_LITERAL_BOOLEAN, b, 0.0, NULL, NULL);
      }

    case RASQAL_EXPR_NEQ:
      {
        int b=(rasqal_expression_compare(e->arg1, e->arg2, &error) != 0);
        if(error)
          return NULL;
        return rasqal_new_literal(RASQAL_LITERAL_BOOLEAN, b, 0.0, NULL, NULL);
      }

    case RASQAL_EXPR_LT:
      {
        int b=(rasqal_expression_compare(e->arg1, e->arg2, &error) < 0);
        if(error)
          return NULL;
        return rasqal_new_literal(RASQAL_LITERAL_BOOLEAN, b, 0.0, NULL, NULL);
      }

    case RASQAL_EXPR_GT:
      {
        int b=(rasqal_expression_compare(e->arg1, e->arg2, &error) > 0);
        if(error)
          return NULL;
        return rasqal_new_literal(RASQAL_LITERAL_BOOLEAN, b, 0.0, NULL, NULL);
      }

    case RASQAL_EXPR_LE:
      {
        int b=(rasqal_expression_compare(e->arg1, e->arg2, &error) <= 0);
        if(error)
          return NULL;
        return rasqal_new_literal(RASQAL_LITERAL_BOOLEAN, b, 0.0, NULL, NULL);
      }

    case RASQAL_EXPR_GE:
      {
        int b=(rasqal_expression_compare(e->arg1, e->arg2, &error) >= 0);
        if(error)
          return NULL;
        return rasqal_new_literal(RASQAL_LITERAL_BOOLEAN, b, 0.0, NULL, NULL);
      }

    case RASQAL_EXPR_PLUS:
      {
        int i=rasqal_expression_as_integer(e->arg1) +
              rasqal_expression_as_integer(e->arg2);
        return rasqal_new_literal(RASQAL_LITERAL_INTEGER, i, 0.0, NULL, NULL);
      }
      
    case RASQAL_EXPR_MINUS:
      {
        int i=rasqal_expression_as_integer(e->arg1) -
              rasqal_expression_as_integer(e->arg2);
        return rasqal_new_literal(RASQAL_LITERAL_INTEGER, i, 0.0, NULL, NULL);
      }
      
    case RASQAL_EXPR_STAR:
      {
        int i=rasqal_expression_as_integer(e->arg1) *
              rasqal_expression_as_integer(e->arg2);
        return rasqal_new_literal(RASQAL_LITERAL_INTEGER, i, 0.0, NULL, NULL);
      }
      
    case RASQAL_EXPR_SLASH:
      {
        int i=rasqal_expression_as_integer(e->arg1) /
              rasqal_expression_as_integer(e->arg2);
        return rasqal_new_literal(RASQAL_LITERAL_INTEGER, i, 0.0, NULL, NULL);
      }
      
    case RASQAL_EXPR_REM:
      {
        int i=rasqal_expression_as_integer(e->arg1) %
              rasqal_expression_as_integer(e->arg2);
        return rasqal_new_literal(RASQAL_LITERAL_INTEGER, i, 0.0, NULL, NULL);
      }
      
    case RASQAL_EXPR_STR_EQ:
      {
        int b=(rasqal_expression_compare(e->arg1, e->arg2, &error) == 0);
        if(error)
          return NULL;
        return rasqal_new_literal(RASQAL_LITERAL_BOOLEAN, b, 0.0, NULL, NULL);
      }
      
    case RASQAL_EXPR_STR_NEQ:
      {
        int b=(rasqal_expression_compare(e->arg1, e->arg2, &error) != 0);
        if(error)
          return NULL;
        return rasqal_new_literal(RASQAL_LITERAL_BOOLEAN, b, 0.0, NULL, NULL);
      }

    case RASQAL_EXPR_TILDE:
      {
        int i= ~ rasqal_expression_as_integer(e->arg1);
        return rasqal_new_literal(RASQAL_LITERAL_INTEGER, i, 0.0, NULL, NULL);
      }

    case RASQAL_EXPR_BANG:
      {
        int b=!rasqal_expression_as_boolean(e->arg1);
        return rasqal_new_literal(RASQAL_LITERAL_BOOLEAN, b, 0.0, NULL, NULL);
      }

    case RASQAL_EXPR_STR_MATCH:
    case RASQAL_EXPR_STR_NMATCH:
      /* FIXME */
      break;

    case RASQAL_EXPR_LITERAL:
      {
        int i=rasqal_literal_as_integer(e->literal);
        return rasqal_new_literal(RASQAL_LITERAL_INTEGER, i, 0.0, NULL, NULL);
      }

    case RASQAL_EXPR_VARIABLE:
      {
        int i=rasqal_variable_as_integer(e->variable);
        return rasqal_new_literal(RASQAL_LITERAL_INTEGER, i, 0.0, NULL, NULL);
      }

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
  "variable",
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
    case RASQAL_EXPR_VARIABLE:
      rasqal_variable_print(e->variable, fh);
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

  lit1=rasqal_new_literal(RASQAL_LITERAL_INTEGER, 1, 0.0, NULL, NULL);
  expr1=rasqal_new_literal_expression(lit1);
  lit2=rasqal_new_literal(RASQAL_LITERAL_INTEGER, 1, 0.0, NULL, NULL);
  expr2=rasqal_new_literal_expression(lit2);
  expr=rasqal_new_2op_expression(RASQAL_EXPR_PLUS, expr1, expr2);

  program=argv[0];

  fprintf(stderr, "%s: expression: ", program);
  rasqal_expression_print(expr, stderr);
  fputc('\n', stderr);

  result=rasqal_expression_evaluate(expr);

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

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
inline int rasqal_literal_equals(rasqal_literal* l1, rasqal_literal *l2);

inline int rasqal_variable_as_boolean(rasqal_variable* v);
inline int rasqal_variable_as_integer(rasqal_variable* v);
inline int rasqal_variable_equals(rasqal_variable* v1, rasqal_variable* v2);

int rasqal_expression_as_boolean(rasqal_expression* e);
int rasqal_expression_as_integer(rasqal_expression* e);
int rasqal_expression_equals(rasqal_expression* e1, rasqal_expression* e2);
rasqal_expression* rasqal_evaluate_expression(rasqal_expression* e);


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

void
rasqal_print_literal_type(rasqal_literal* literal, FILE* fh)
{
  rasqal_literal_type type=literal->type;
  if(type > RASQAL_LITERAL_LAST)
    type=RASQAL_LITERAL_UNKNOWN;
  fputs(rasqal_literal_type_labels[(int)type], fh);
}


void
rasqal_print_literal(rasqal_literal* l, FILE* fh)
{
  /*  fputs("literal_", fh); */
  rasqal_print_literal_type(l, fh);
  switch(l->type) {
    case RASQAL_LITERAL_URI:
      fprintf(fh, "<%s>", raptor_uri_as_string(l->value.uri));
      break;
    case RASQAL_LITERAL_BLANK:
      fprintf(fh, "_:%s", l->value.string);
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
rasqal_literal_equals(rasqal_literal* l1, rasqal_literal* l2)
{
  if(l1->type != l2->type)
    return 1;

  switch(l1->type) {
    case RASQAL_LITERAL_URI:
      return raptor_uri_equals(l1->value.uri,l2->value.uri);

    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
      return !strcmp(l1->value.string,l2->value.string);

    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_NULL:
      return l1->value.integer == l2->value.integer;
      break;

    case RASQAL_LITERAL_FLOATING:
      return l1->value.floating == l2->value.floating;
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
    if(!strcmp(v->name, name))
      return v;
  }
    
  v=(rasqal_variable*)calloc(sizeof(rasqal_variable), 1);

  v->name=name;
  v->value=value;
  v->offset=rq->variables_count++;

  rasqal_sequence_push(rq->variables_sequence, v);
  
  return v;
}


void
rasqal_free_variable(rasqal_variable* variable) {
  free(variable);
}


void
rasqal_print_variable(rasqal_variable* v, FILE* fh)
{
  fprintf(fh, "variable(%s", v->name);
  if(v->value) {
    fputc('=', fh);
    rasqal_print_expression(v->value, fh);
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
rasqal_variable_equals(rasqal_variable* v1, rasqal_variable* v2)
{
  return rasqal_expression_equals(v1->value, v2->value);
}

void
rasqal_variable_set_value(rasqal_variable* v, rasqal_expression *e)
{
  if(v->value)
    return rasqal_free_expression(v->value);
  v->value=e;
}



rasqal_prefix*
rasqal_new_prefix(const char *prefix, raptor_uri* uri) 
{
  rasqal_prefix* p=(rasqal_prefix*)calloc(sizeof(rasqal_prefix), 1);

  p->prefix=prefix;
  p->uri=uri;

  return p;
}


void
rasqal_free_prefix(rasqal_prefix* prefix) {
  free(prefix);
}


void
rasqal_print_prefix(rasqal_prefix* p, FILE* fh)
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
rasqal_print_triple(rasqal_triple* t, FILE* fh)
{
  fputs("triple(", fh);
  rasqal_print_expression(t->subject, fh);
  fputs(", ", fh);
  rasqal_print_expression(t->predicate, fh);
  fputs(", ", fh);
  rasqal_print_expression(t->object, fh);
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
    case RASQAL_EXPR_BIT_AND:
    case RASQAL_EXPR_BIT_OR:
    case RASQAL_EXPR_BIT_XOR:
    case RASQAL_EXPR_LSHIFT:
    case RASQAL_EXPR_RSIGNEDSHIFT:
    case RASQAL_EXPR_RUNSIGNEDSHIFT:
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
      rasqal_free_variable(e->variable);
      break;
    default:
      abort();
  }
  free(e);
}

int
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
rasqal_expression_equals(rasqal_expression* e1, rasqal_expression* e2) {
  if(e1->op == RASQAL_EXPR_EXPR)
    return rasqal_expression_equals(e1->arg1, e2);
  if(e2->op == RASQAL_EXPR_EXPR)
    return rasqal_expression_equals(e1, e2->arg1);

  if(e1->op != e2->op)
    return 1;
  
  switch(e1->op) {
    case RASQAL_EXPR_LITERAL:
      return rasqal_literal_equals(e1->literal, e1->literal);

    case RASQAL_EXPR_VARIABLE:
      return rasqal_variable_equals(e1->variable, e2->variable);

    default:
      abort();
  }
}


int
rasqal_expression_is_variable(rasqal_expression* e) {
  return (e->op == RASQAL_EXPR_VARIABLE);
}


rasqal_expression*
rasqal_evaluate_expression(rasqal_expression* e) {
  switch(e->op) {
    case RASQAL_EXPR_EXPR:
      return rasqal_evaluate_expression(e->arg1);
      break;

    case RASQAL_EXPR_AND:
      {
        int b=rasqal_expression_as_boolean(e->arg1) &&
              rasqal_expression_as_boolean(e->arg2);
        rasqal_literal *l=rasqal_new_literal(RASQAL_LITERAL_BOOLEAN, b, 0.0, NULL, NULL);
        return rasqal_new_literal_expression(l);
      }
      
    case RASQAL_EXPR_OR:
      {
        int b=rasqal_expression_as_boolean(e->arg1) ||
              rasqal_expression_as_boolean(e->arg2);
        rasqal_literal *l=rasqal_new_literal(RASQAL_LITERAL_BOOLEAN, b, 0.0, NULL, NULL);
        return rasqal_new_literal_expression(l);
      }

    case RASQAL_EXPR_BIT_AND:
      {
        int i=rasqal_expression_as_integer(e->arg1) &
              rasqal_expression_as_integer(e->arg2);
        rasqal_literal *l=rasqal_new_literal(RASQAL_LITERAL_INTEGER, i, 0.0, NULL, NULL);
        return rasqal_new_literal_expression(l);
      }

    case RASQAL_EXPR_BIT_OR:
      {
        int i=rasqal_expression_as_integer(e->arg1) |
              rasqal_expression_as_integer(e->arg2);
        rasqal_literal *l=rasqal_new_literal(RASQAL_LITERAL_INTEGER, i, 0.0, NULL, NULL);
        return rasqal_new_literal_expression(l);
      }

    case RASQAL_EXPR_BIT_XOR:
     {
       int i=rasqal_expression_as_integer(e->arg1) ^
             rasqal_expression_as_integer(e->arg2);
       rasqal_literal *l=rasqal_new_literal(RASQAL_LITERAL_INTEGER, i, 0.0, NULL, NULL);
       return rasqal_new_literal_expression(l);
     }

    case RASQAL_EXPR_LSHIFT:
     {
       int i=rasqal_expression_as_integer(e->arg1) <<
             rasqal_expression_as_integer(e->arg2);
       rasqal_literal *l=rasqal_new_literal(RASQAL_LITERAL_INTEGER, i, 0.0, NULL, NULL);
       return rasqal_new_literal_expression(l);
     }

    case RASQAL_EXPR_RSIGNEDSHIFT:
     {
       int i=rasqal_expression_as_integer(e->arg1) >>
             rasqal_expression_as_integer(e->arg2);
       rasqal_literal *l=rasqal_new_literal(RASQAL_LITERAL_INTEGER, i, 0.0, NULL, NULL);
       return rasqal_new_literal_expression(l);
     }

    case RASQAL_EXPR_RUNSIGNEDSHIFT:
     {
       int i=rasqal_expression_as_integer(e->arg1) >>
             rasqal_expression_as_integer(e->arg2);
       rasqal_literal *l=rasqal_new_literal(RASQAL_LITERAL_INTEGER, i, 0.0, NULL, NULL);
       return rasqal_new_literal_expression(l);
     }

    case RASQAL_EXPR_EQ:
      {
        int b=rasqal_expression_equals(e->arg1, e->arg2);
        rasqal_literal *l=rasqal_new_literal(RASQAL_LITERAL_BOOLEAN, b, 0.0, NULL, NULL);
        return rasqal_new_literal_expression(l);
      }

    case RASQAL_EXPR_NEQ:
      {
        int b=!rasqal_expression_equals(e->arg1, e->arg2);
        rasqal_literal *l=rasqal_new_literal(RASQAL_LITERAL_BOOLEAN, b, 0.0, NULL, NULL);
        return rasqal_new_literal_expression(l);
      }

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
      break;
    case RASQAL_EXPR_TILDE:
    case RASQAL_EXPR_BANG:
      break;
    case RASQAL_EXPR_STR_MATCH:
    case RASQAL_EXPR_STR_NMATCH:
      break;
    case RASQAL_EXPR_LITERAL:
      break;
    case RASQAL_EXPR_VARIABLE:
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
  "bit_and",
  "bit_or",
  "bit_xor",
  "lshift",
  "rsignedshift",
  "runsignedshift",
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
rasqal_print_expression_op(rasqal_expression* expression, FILE* fh)
{
  rasqal_op op=expression->op;
  if(op > RASQAL_EXPR_LAST)
    op=RASQAL_EXPR_UNKNOWN;
  fputs(rasqal_op_labels[(int)op], fh);
}


void
rasqal_print_expression(rasqal_expression* e, FILE* fh)
{
  fputs("expr(", fh);
  switch(e->op) {
    case RASQAL_EXPR_EXPR:
      rasqal_print_expression(e->arg1, fh);
      break;
    case RASQAL_EXPR_AND:
    case RASQAL_EXPR_OR:
    case RASQAL_EXPR_BIT_AND:
    case RASQAL_EXPR_BIT_OR:
    case RASQAL_EXPR_BIT_XOR:
    case RASQAL_EXPR_LSHIFT:
    case RASQAL_EXPR_RSIGNEDSHIFT:
    case RASQAL_EXPR_RUNSIGNEDSHIFT:
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
      rasqal_print_expression_op(e, fh);
      fputc('(', fh);
      rasqal_print_expression(e->arg1, fh);
      fputs(", ", fh);
      rasqal_print_expression(e->arg2, fh);
      fputc(')', fh);
      break;
    case RASQAL_EXPR_STR_MATCH:
    case RASQAL_EXPR_STR_NMATCH:
      fputs("op ", fh);
      rasqal_print_expression_op(e, fh);
      fputc('(', fh);
      rasqal_print_expression(e->arg1, fh);
      fputs(", ", fh);
      rasqal_print_literal(e->literal, fh);
      fputc(')', fh);
      break;
    case RASQAL_EXPR_TILDE:
    case RASQAL_EXPR_BANG:
      fputs("op ", fh);
      rasqal_print_expression_op(e, fh);
      fputc('(', fh);
      rasqal_print_expression(e->arg1, fh);
      fputc(')', fh);
      break;
    case RASQAL_EXPR_LITERAL:
      rasqal_print_literal(e->literal, fh);
      break;
    case RASQAL_EXPR_VARIABLE:
      rasqal_print_variable(e->variable, fh);
      break;
    case RASQAL_EXPR_PATTERN:
      fprintf(fh, "expr_pattern(%s)", (char*)e->value);
      break;
    default:
      abort();
  }
  fputc(')', fh);
}

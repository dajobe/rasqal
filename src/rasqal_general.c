/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_general.c - Rasqal general support
 *
 * $Id$
 *
 * Copyright (C) 2003 David Beckett - http://purl.org/net/dajobe/
 * Institute for Learning and Research Technology - http://www.ilrt.org/
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
#include <stdlib.h>
#include <string.h>

#include <rasqal.h>
#include <rasqal_internal.h>




rasqal_literal*
rasqal_new_literal(rasqal_literal_type type, int integer, float floating,
                   char *string)
{
  rasqal_literal* l=(rasqal_literal*)calloc(sizeof(rasqal_literal), 1);

  l->type=type;
  switch(type) {
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_PATTERN:
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
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_PATTERN:
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
  "string",
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
  fputs("literal_", fh);
  rasqal_print_literal_type(l, fh);
  switch(l->type) {
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_PATTERN:
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



rasqal_variable*
rasqal_new_variable(const char *name, const char *value) 
{
  rasqal_variable* v=(rasqal_variable*)calloc(sizeof(rasqal_variable), 1);

  v->name=name;
  v->value=value;

  return v;
}


void
rasqal_free_variable(rasqal_variable* variable) {
  free(variable);
}


void
rasqal_print_variable(rasqal_variable* v, FILE* fh)
{
  if(v->value)
    fprintf(fh, "variable(%s=%s)", v->name, v->value);
  else
    fprintf(fh, "variable(%s)", v->name);
}


rasqal_prefix*
rasqal_new_prefix(const char *prefix, const char *uri) 
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
  fprintf(fh, "prefix(%s as %s)", p->prefix, p->uri);
}


rasqal_term*
rasqal_new_term(rasqal_term_type type, void *value) 
{
  rasqal_term* t=(rasqal_term*)calloc(sizeof(rasqal_term), 1);

  t->type=type;
  t->value=value;

  return t;
}

void
rasqal_free_term(rasqal_term* term) {
  free(term);
}


void
rasqal_print_term(rasqal_term* t, FILE* fh)
{
  switch(t->type) {
    case RASQAL_TERM_VAR:
      rasqal_print_variable((rasqal_variable*)t->value, fh);
      break;
    case RASQAL_TERM_URI:
      fprintf(fh, "term_uri(%s)", (char*)t->value);
      break;
    case RASQAL_TERM_PATTERN:
      fprintf(fh, "term_pattern(%s)", (char*)t->value);
      break;
    case RASQAL_TERM_LITERAL:
      rasqal_print_literal((rasqal_literal*)t->value, fh);
      break;
    default:
      abort();
  }
}


rasqal_triple*
rasqal_new_triple(rasqal_term* subject, rasqal_term* predicate, rasqal_term* object)
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
  rasqal_free_term(t->subject);
  rasqal_free_term(t->predicate);
  rasqal_free_term(t->object);
  free(t);
}


void
rasqal_print_triple(rasqal_triple* t, FILE* fh)
{
  fputs("triple(", fh);
  rasqal_print_term(t->subject, fh);
  fputs(", ", fh);
  rasqal_print_term(t->predicate, fh);
  fputs(", ", fh);
  rasqal_print_term(t->object, fh);
  fputc(')', fh);
}


rasqal_expression*
rasqal_new_expression(rasqal_op op,
                      rasqal_expression* arg1, 
                      rasqal_expression* arg2,
                      rasqal_literal *literal,
                      rasqal_variable *variable)
{
  rasqal_expression* e=(rasqal_expression*)calloc(sizeof(rasqal_expression), 1);

  e->op=op;
  e->arg1=arg1;
  e->arg2=arg2;
  e->literal=literal;
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
    default:
      abort();
  }
  fputc(')', fh);
}




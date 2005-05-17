/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_expr.c - Rasqal general expression support
 *
 * $Id$
 *
 * Copyright (C) 2003-2005, David Beckett http://purl.org/net/dajobe/
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

#ifdef RASQAL_REGEX_PCRE
#include <pcre.h>
#endif

#ifdef RASQAL_REGEX_POSIX
#include <sys/types.h>
#include <regex.h>
#endif

#include "rasqal.h"
#include "rasqal_internal.h"


#ifndef STANDALONE

static RASQAL_INLINE int rasqal_expression_as_boolean(rasqal_expression* e, int *error);
static RASQAL_INLINE int rasqal_expression_as_integer(rasqal_expression* e, int *error);
static RASQAL_INLINE int rasqal_expression_compare(rasqal_expression* e1, rasqal_expression* e2, int flags, int *error);



rasqal_data_graph*
rasqal_new_data_graph(raptor_uri* uri, raptor_uri* name_uri, int flags)
{
  rasqal_data_graph* dg=(rasqal_data_graph*)RASQAL_CALLOC(rasqal_data_graph,
                                                      sizeof(rasqal_data_graph), 1);
  dg->uri=raptor_uri_copy(uri);
  if(name_uri)
    dg->name_uri=raptor_uri_copy(name_uri);
  dg->flags=flags;

  return dg;
}


void
rasqal_free_data_graph(rasqal_data_graph* dg)
{
  if(dg->uri)
    raptor_free_uri(dg->uri);
  if(dg->name_uri)
    raptor_free_uri(dg->name_uri);
  RASQAL_FREE(rasqal_data_graph, dg);
}


void
rasqal_data_graph_print(rasqal_data_graph* dg, FILE* fh)
{
  if(dg->name_uri)
    fprintf(fh, "data graph(%s named as %s flags %d)", 
            raptor_uri_as_string(dg->uri),
            raptor_uri_as_string(dg->name_uri),
            dg->flags);
  else
    fprintf(fh, "data graph(%s, flags %d)", 
            raptor_uri_as_string(dg->uri), dg->flags);
}



/**
 * rasqal_new_variable_typed - Constructor - Create a new typed Rasqal variable
 * @rq: &rasqal_query to associate the variable with
 * @type: variable type defined by enumeration rasqal_variable_type
 * @name: variable name
 * @value: variable &rasqal_literal value (or NULL)
 * 
 * The variable must be associated with a query, since variable
 * names are only significant with a single query.
 * 
 * The @name and @value become owned by the rasqal_variable structure
 *
 * Return value: a new &rasqal_variable or NULL on failure.
 **/
rasqal_variable*
rasqal_new_variable_typed(rasqal_query* rq,
                          rasqal_variable_type type, 
                          unsigned char *name, rasqal_literal *value)
{
  int i;
  rasqal_variable* v;
  raptor_sequence* seq;
  int* count_p;

  switch(type) {
    case RASQAL_VARIABLE_TYPE_ANONYMOUS:
      seq=rq->anon_variables_sequence;
      count_p=&rq->anon_variables_count;
      break;
    case RASQAL_VARIABLE_TYPE_NORMAL:
      seq=rq->variables_sequence;
      count_p=&rq->variables_count;
      break;

    case RASQAL_VARIABLE_TYPE_UNKNOWN:
    default:
      RASQAL_DEBUG2("Unknown variable type %d", type);
      return NULL;
  }
  
  for(i=0; i< raptor_sequence_size(seq); i++) {
    v=(rasqal_variable*)raptor_sequence_get_at(seq, i);
    if(!strcmp((const char*)v->name, (const char*)name)) {
      /* name already present, do not need a copy */
      RASQAL_FREE(cstring, name);
      return v;
    }
  }
    
  v=(rasqal_variable*)RASQAL_CALLOC(rasqal_variable, sizeof(rasqal_variable), 1);

  v->type= type;
  v->name= name;
  v->value= value;
  v->offset= (*count_p)++;

  raptor_sequence_push(seq, v);
  
  return v;
}


/**
 * rasqal_new_variable - Constructor - Create a new Rasqal normal variable
 * @rq: &rasqal_query to associate the variable with
 * @name: variable name
 * @value: variable &rasqal_literal value (or NULL)
 * 
 * The variable must be associated with a query, since variable
 * names are only significant with a single query.
 *
 * This creates a regular variable that can be returned of type
 * RASQAL_VARIABLE_TYPE_NORMAL.  Use rasqal_new_variable_typed
 * to create other variables.
 * 
 * The @name and @value become owned by the rasqal_variable structure
 *
 * Return value: a new &rasqal_variable or NULL on failure.
 **/
rasqal_variable*
rasqal_new_variable(rasqal_query* rq,
                    unsigned char *name, rasqal_literal *value) 
{
  return rasqal_new_variable_typed(rq, RASQAL_VARIABLE_TYPE_NORMAL, name, value);
}


/**
 * rasqal_free_variable - Destructor - Destroy a Rasqal variable object
 * @v: &rasqal_variable object
 *
 **/
void
rasqal_free_variable(rasqal_variable* v)
{
  if(v->name)
    RASQAL_FREE(cstring, (void*)v->name);
  if(v->value)
    rasqal_free_literal(v->value);
  RASQAL_FREE(rasqal_variable, v);
}


/**
 * rasqal_variable_print - Print a Rasqal variable in a debug format
 * @v: the &rasqal_variable object
 * @fh: the &FILE* handle to print to
 * 
 * The print debug format may change in any release.
 * 
 **/
void
rasqal_variable_print(rasqal_variable* v, FILE* fh)
{
  if(v->type == RASQAL_VARIABLE_TYPE_ANONYMOUS)
    fprintf(fh, "anon-variable(%s", v->name);
  else
    fprintf(fh, "variable(%s", v->name);
  if(v->value) {
    fputc('=', fh);
    rasqal_literal_print(v->value, fh);
  }
  fputc(')', fh);
}


/**
 * rasqal_variable_set_value - Set the value of a Rasqal variable
 * @v: the &rasqal_variable object
 * @e: the &rasqal_literal value to set (or NULL)
 * 
 * The variable value is an input parameter and is copied in, not shared.
 * If the variable value is NULL, any existing value is deleted.
 * 
 **/
void
rasqal_variable_set_value(rasqal_variable* v, rasqal_literal *e)
{
  if(v->value)
    rasqal_free_literal(v->value);
  v->value=e;
#ifdef RASQAL_DEBUG
  if(!v->name)
    RASQAL_FATAL1("variable has no name");
  RASQAL_DEBUG2("setting variable %s to value ", v->name);
  if(v->value)
    rasqal_literal_print(v->value, stderr);
  else
    fputs("(NULL)", stderr);
  fputc('\n', stderr);
#endif
}


rasqal_prefix*
rasqal_new_prefix(const unsigned char *prefix, raptor_uri* uri) 
{
  rasqal_prefix* p=(rasqal_prefix*)RASQAL_CALLOC(rasqal_prefix,
                                                 sizeof(rasqal_prefix), 1);

  p->prefix=prefix;
  p->uri=uri;

  return p;
}


void
rasqal_free_prefix(rasqal_prefix* p)
{
  if(p->prefix)
    RASQAL_FREE(cstring, (void*)p->prefix);
  raptor_free_uri(p->uri);
  RASQAL_FREE(rasqal_prefix, p);
}


void
rasqal_prefix_print(rasqal_prefix* p, FILE* fh)
{
  fprintf(fh, "prefix(%s as %s)", (p->prefix ? (const char*)p->prefix : "(default)"), raptor_uri_as_string(p->uri));
}



rasqal_triple*
rasqal_new_triple(rasqal_literal* subject, rasqal_literal* predicate, rasqal_literal* object)
{
  rasqal_triple* t=(rasqal_triple*)RASQAL_CALLOC(rasqal_triple,sizeof(rasqal_triple), 1);

  t->subject=subject;
  t->predicate=predicate;
  t->object=object;

  return t;
}

rasqal_triple*
rasqal_new_triple_from_triple(rasqal_triple* t) {
  rasqal_triple* newt=(rasqal_triple*)RASQAL_CALLOC(rasqal_triple,sizeof(rasqal_triple), 1);

  newt->subject=rasqal_new_literal_from_literal(t->subject);
  newt->predicate=rasqal_new_literal_from_literal(t->predicate);
  newt->object=rasqal_new_literal_from_literal(t->object);

  return newt;
}

void
rasqal_free_triple(rasqal_triple* t)
{
  if(t->subject)
    rasqal_free_literal(t->subject);
  if(t->predicate)
    rasqal_free_literal(t->predicate);
  if(t->object)
    rasqal_free_literal(t->object);
  RASQAL_FREE(rasqal_triple, t);
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
  if(t->origin) {
    fputs(" with origin(", fh);
    rasqal_literal_print(t->origin, fh);
    fputc(')', fh);
  }
}


void
rasqal_triple_set_origin(rasqal_triple* t, rasqal_literal* l)
{
  t->origin=l;
}


rasqal_literal*
rasqal_triple_get_origin(rasqal_triple* t)
{
  return t->origin;
}


void
rasqal_triple_set_flags(rasqal_triple* t, unsigned int flags)
{
  RASQAL_DEPRECATED_MESSAGE("rasqal_triple flags are going away");
  t->flags=flags;
}


unsigned int
rasqal_triple_get_flags(rasqal_triple* t)
{
  RASQAL_DEPRECATED_MESSAGE("rasqal_triple flags are going away");
  return t->flags;
}


rasqal_expression*
rasqal_new_1op_expression(rasqal_op op, rasqal_expression* arg)
{
  rasqal_expression* e=(rasqal_expression*)RASQAL_CALLOC(rasqal_expression, sizeof(rasqal_expression), 1);
  e->op=op;
  e->arg1=arg;
  return e;
}

rasqal_expression*
rasqal_new_2op_expression(rasqal_op op,
                          rasqal_expression* arg1, 
                          rasqal_expression* arg2)
{
  rasqal_expression* e=(rasqal_expression*)RASQAL_CALLOC(rasqal_expression, sizeof(rasqal_expression), 1);
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
  rasqal_expression* e=(rasqal_expression*)RASQAL_CALLOC(rasqal_expression, sizeof(rasqal_expression), 1);
  e->op=op;
  e->arg1=arg1;
  e->literal=literal;
  return e;
}

rasqal_expression*
rasqal_new_literal_expression(rasqal_literal *literal)
{
  rasqal_expression* e=(rasqal_expression*)RASQAL_CALLOC(rasqal_expression, sizeof(rasqal_expression), 1);
  e->op=RASQAL_EXPR_LITERAL;
  e->literal=literal;
  return e;
}


rasqal_expression*
rasqal_new_function_expression(raptor_uri* name,
                               raptor_sequence* args)
{
  rasqal_expression* e=(rasqal_expression*)RASQAL_CALLOC(rasqal_expression, sizeof(rasqal_expression), 1);
  e->op=RASQAL_EXPR_FUNCTION;
  e->name=name;
  e->args=args;
  return e;
}


rasqal_expression*
rasqal_new_cast_expression(raptor_uri* name, rasqal_expression *value) 
{
  rasqal_expression* e=(rasqal_expression*)RASQAL_CALLOC(rasqal_expression, sizeof(rasqal_expression), 1);
  e->op=RASQAL_EXPR_CAST;
  e->name=name;
  e->arg1=value;
  return e;
}


void
rasqal_free_expression(rasqal_expression* e)
{
  switch(e->op) {
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
    case RASQAL_EXPR_UMINUS:
    case RASQAL_EXPR_BOUND:
    case RASQAL_EXPR_STR:
    case RASQAL_EXPR_LANG:
    case RASQAL_EXPR_DATATYPE:
    case RASQAL_EXPR_ISURI:
    case RASQAL_EXPR_ISBLANK:
    case RASQAL_EXPR_ISLITERAL:
    case RASQAL_EXPR_ORDER_COND_ASC:
    case RASQAL_EXPR_ORDER_COND_DESC:
    case RASQAL_EXPR_ORDER_COND_NONE:
      rasqal_free_expression(e->arg1);
      break;
    case RASQAL_EXPR_STR_MATCH:
    case RASQAL_EXPR_STR_NMATCH:
      rasqal_free_expression(e->arg1);
      /* FALLTHROUGH */
    case RASQAL_EXPR_LITERAL:
      rasqal_free_literal(e->literal);
      break;
    case RASQAL_EXPR_FUNCTION:
      raptor_free_uri(e->name);
      raptor_free_sequence(e->args);
      break;
    case RASQAL_EXPR_CAST:
      raptor_free_uri(e->name);
      rasqal_free_expression(e->arg1);
      break;
      
    case RASQAL_EXPR_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown operation %d", e->op);
  }
  RASQAL_FREE(rasqal_expression, e);
}


int
rasqal_expression_foreach(rasqal_expression* e, 
                          rasqal_expression_foreach_fn fn,
                          void *user_data)
{
  switch(e->op) {
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
    case RASQAL_EXPR_UMINUS:
    case RASQAL_EXPR_BOUND:
    case RASQAL_EXPR_STR:
    case RASQAL_EXPR_LANG:
    case RASQAL_EXPR_DATATYPE:
    case RASQAL_EXPR_ISURI:
    case RASQAL_EXPR_ISBLANK:
    case RASQAL_EXPR_ISLITERAL:
    case RASQAL_EXPR_CAST:
    case RASQAL_EXPR_ORDER_COND_ASC:
    case RASQAL_EXPR_ORDER_COND_DESC:
    case RASQAL_EXPR_ORDER_COND_NONE:
      return fn(user_data, e) ||
        rasqal_expression_foreach(e->arg1, fn, user_data);
      break;
    case RASQAL_EXPR_STR_MATCH:
    case RASQAL_EXPR_STR_NMATCH:
    case RASQAL_EXPR_LITERAL:
    case RASQAL_EXPR_FUNCTION:
      return fn(user_data, e);
      break;

    case RASQAL_EXPR_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown operation %d", e->op);
  }
}


static RASQAL_INLINE int
rasqal_expression_as_boolean(rasqal_expression* e, int *error)
{
  if(e->op == RASQAL_EXPR_LITERAL)
    return rasqal_literal_as_boolean(e->literal, error);

  abort();
}


static RASQAL_INLINE int
rasqal_expression_as_integer(rasqal_expression* e, int *error)
{
  if(e->op == RASQAL_EXPR_LITERAL)
    return rasqal_literal_as_integer(e->literal, error);

  abort();
}


static RASQAL_INLINE int
rasqal_expression_compare(rasqal_expression* e1, rasqal_expression* e2,
                          int flags, int *error)
{
  *error=0;
  
  if(e1->op == RASQAL_EXPR_LITERAL && e1->op == e2->op)
    return rasqal_literal_compare(e1->literal, e2->literal, flags, error);

  if(e1->op !=RASQAL_EXPR_LITERAL)
    RASQAL_FATAL2("Unexpected e1 op %d\n", e1->op);
  else
    RASQAL_FATAL2("Unexpected e2 op %d\n", e2->op);
}


rasqal_literal*
rasqal_expression_evaluate(rasqal_query *query, rasqal_expression* e)
{
  int error=0;
  rasqal_literal* result=NULL;
  
#ifdef RASQAL_DEBUG
  RASQAL_DEBUG2("evaluating expression %p: ", e);
  rasqal_expression_print(e, stderr);
  fprintf(stderr, "\n");
#endif
  
  switch(e->op) {
    case RASQAL_EXPR_AND:
      {
        rasqal_literal *l;
        int b;
        
        l=rasqal_expression_evaluate(query, e->arg1);
        if(!l)
          goto failed;
        b=rasqal_literal_as_boolean(l, &error);
        rasqal_free_literal(l);
        if(error)
          goto failed;

        if(b) {
          l=rasqal_expression_evaluate(query, e->arg2);
          if(!l)
            goto failed;
          b=rasqal_literal_as_boolean(l, &error);
          rasqal_free_literal(l);
          if(error)
            goto failed;
        }
        result=rasqal_new_boolean_literal(b);
        break;
      }
      
    case RASQAL_EXPR_OR:
      {
        rasqal_literal *l;
        int b;
        
        l=rasqal_expression_evaluate(query, e->arg1);
        if(!l)
          goto failed;
        b=rasqal_literal_as_boolean(l, &error);
        rasqal_free_literal(l);
        if(error)
          goto failed;

        if(!b) {
          l=rasqal_expression_evaluate(query, e->arg2);
          if(!l)
            goto failed;
          b=rasqal_literal_as_boolean(l, &error);
          rasqal_free_literal(l);
          if(error)
            goto failed;
        }
        result=rasqal_new_boolean_literal(b);
        break;
      }

    case RASQAL_EXPR_EQ:
      {
        rasqal_literal *l1, *l2;
        int b;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          goto failed;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          goto failed;
        }

        b=(rasqal_literal_compare(l1, l2, 0, &error) == 0);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error)
          goto failed;
        result=rasqal_new_boolean_literal(b);
        break;
      }

    case RASQAL_EXPR_NEQ:
      {
        rasqal_literal *l1, *l2;
        int b;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          goto failed;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          goto failed;
        }

        b=(rasqal_literal_compare(l1, l2, 0, &error) != 0);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error)
          goto failed;
        result=rasqal_new_boolean_literal(b);
        break;
      }

    case RASQAL_EXPR_LT:
      {
        rasqal_literal *l1, *l2;
        int b;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          goto failed;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          goto failed;
        }

        b=(rasqal_literal_compare(l1, l2, 0, &error) > 0);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error)
          goto failed;
        result=rasqal_new_boolean_literal(b);
        break;
      }

    case RASQAL_EXPR_GT:
      {
        rasqal_literal *l1, *l2;
        int b;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          goto failed;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          goto failed;
        }

        b=(rasqal_literal_compare(l1, l2, 0, &error) < 0);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error)
          goto failed;
        result=rasqal_new_boolean_literal(b);
        break;
      }

    case RASQAL_EXPR_LE:
      {
        rasqal_literal *l1, *l2;
        int b;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          goto failed;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          goto failed;
        }

        b=(rasqal_literal_compare(l1, l2, 0, &error) >= 0);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error)
          goto failed;
        result=rasqal_new_boolean_literal(b);
        break;        
      }

    case RASQAL_EXPR_GE:
      {
        rasqal_literal *l1, *l2;
        int b;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          goto failed;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          goto failed;
        }

        b=(rasqal_literal_compare(l1, l2, 0, &error) <= 0);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error)
          goto failed;
        result=rasqal_new_boolean_literal(b);
        break;
      }

    case RASQAL_EXPR_UMINUS:
      {
        rasqal_literal *l=rasqal_expression_evaluate(query, e->arg1);
        double d= -rasqal_literal_as_floating(l, &error);
        rasqal_free_literal(l);
        if(error)
          goto failed;
        result=rasqal_new_floating_literal(d);
        break;
      }

    case RASQAL_EXPR_BOUND:
      {
        rasqal_literal *l;
        rasqal_variable* v;
        int b;

        l=rasqal_expression_evaluate(query, e->arg1);
        if(!l)
          goto failed;
        
        v=rasqal_literal_as_variable(l);
        if(!v) {
          rasqal_free_literal(l);
          goto failed;
        }

        b=(v->value != NULL);

        rasqal_free_literal(l);
        result=rasqal_new_boolean_literal(b);
        break;
      }

    case RASQAL_EXPR_STR:
      {
        rasqal_literal* l;
        const unsigned char* s;
        unsigned char* new_s;
        size_t len;
        
        l=rasqal_expression_evaluate(query, e->arg1);
        if(!l)
          goto failed;

        s=rasqal_literal_as_string(l);
        if(!s) {
          rasqal_free_literal(l);
          goto failed;
        }
        
        len=strlen((const char*)s);

        new_s=(unsigned char *)RASQAL_MALLOC(cstring, len+1);
        strncpy((char*)new_s, (const char*)s, len+1);

        result=rasqal_new_string_literal(new_s, NULL, NULL, NULL);
        rasqal_free_literal(l);

        break;
      }
      
    case RASQAL_EXPR_LANG:
      {
        rasqal_literal* l;
        rasqal_variable* v;
        unsigned char* new_language;
        int free_literal=1;
        
        l=rasqal_expression_evaluate(query, e->arg1);
        if(!l)
          goto failed;

        v=rasqal_literal_as_variable(l);
        if(v) {
          rasqal_free_literal(l);
          l=v->value;
          free_literal=0;
        }

        if(l->type != RASQAL_LITERAL_STRING) {
          if(free_literal)
            rasqal_free_literal(l);
          goto failed;
        }
        
        if(l->language) {
          new_language=(unsigned char*)RASQAL_MALLOC(cstring, strlen(l->language)+1);
          strcpy((char*)new_language, l->language);
        } else {
          new_language=(unsigned char*)RASQAL_MALLOC(cstring, 2);
          new_language[0]='-';
          new_language[1]='\0';
        }
        
        result=rasqal_new_string_literal(new_language, NULL, NULL, NULL);

        if(free_literal)
          rasqal_free_literal(l);

        break;
      }

    case RASQAL_EXPR_DATATYPE:
      {
        rasqal_literal* l;
        rasqal_variable* v;
        int free_literal=1;
        
        l=rasqal_expression_evaluate(query, e->arg1);
        if(!l)
          goto failed;

        v=rasqal_literal_as_variable(l);
        if(v) {
          rasqal_free_literal(l);
          l=v->value;
          free_literal=0;
          if(!l)
            goto failed;
        }

        if(l->type != RASQAL_LITERAL_STRING) {
          if(free_literal)
            rasqal_free_literal(l);
          goto failed;
        }
        
        if(l->datatype)
          result=rasqal_new_uri_literal(raptor_uri_copy(l->datatype));
        else
          result=NULL;

        if(free_literal)
          rasqal_free_literal(l);

        break;
      }

    case RASQAL_EXPR_ISURI:
      {
        rasqal_literal *l;
        rasqal_variable* v;
        int free_literal=1;
        int b;
        
        l=rasqal_expression_evaluate(query, e->arg1);
        if(!l)
          goto failed;
        
        v=rasqal_literal_as_variable(l);
        if(v) {
          rasqal_free_literal(l);
          l=v->value;
          free_literal=0;
          if(!l)
            goto failed;
        }

        b=(l->type == RASQAL_LITERAL_URI);
        
        if(free_literal)
          rasqal_free_literal(l);

        result=rasqal_new_boolean_literal(b);
        break;
      }

    case RASQAL_EXPR_ISBLANK:
      {
        rasqal_literal *l;
        rasqal_variable* v;
        int free_literal=1;
        int b;
        
        l=rasqal_expression_evaluate(query, e->arg1);
        if(!l)
          goto failed;
        
        v=rasqal_literal_as_variable(l);
        if(v) {
          rasqal_free_literal(l);
          l=v->value;
          free_literal=0;
          if(!l)
            goto failed;
        }

        b=(l->type == RASQAL_LITERAL_BLANK);

        if(free_literal)
          rasqal_free_literal(l);

        result=rasqal_new_boolean_literal(b);
        break;
      }

    case RASQAL_EXPR_ISLITERAL:
      {
        rasqal_literal *l;
        rasqal_variable* v;
        int free_literal=1;
        int b;
        
        l=rasqal_expression_evaluate(query, e->arg1);
        if(!l)
          goto failed;
        
        v=rasqal_literal_as_variable(l);
        if(v) {
          rasqal_free_literal(l);
          l=v->value;
          free_literal=0;
          if(!l)
            goto failed;
        }

        b=(l->type == RASQAL_LITERAL_STRING);

        if(free_literal)
          rasqal_free_literal(l);

        result=rasqal_new_boolean_literal(b);
        break;
      }
      
    case RASQAL_EXPR_PLUS:
      {
        rasqal_literal *l1, *l2;
        double d;
        int error1=0;
        int error2=0;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          goto failed;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          goto failed;
        }

        d=rasqal_literal_as_floating(l1, &error1) + 
          rasqal_literal_as_floating(l2, &error2);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error || error1 || error2)
          goto failed;

        result=rasqal_new_floating_literal(d);
        break;
      }
      
    case RASQAL_EXPR_MINUS:
      {
        rasqal_literal *l1, *l2;
        double d;
        int error1=0;
        int error2=0;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          goto failed;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          goto failed;
        }

        d=rasqal_literal_as_floating(l1, &error1) -
          rasqal_literal_as_floating(l2, &error2);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error || error1 || error2)
          goto failed;

        result=rasqal_new_floating_literal(d);
        break;
      }
      
    case RASQAL_EXPR_STAR:
      {
        rasqal_literal *l1, *l2;
        double d;
        int error1=0;
        int error2=0;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          goto failed;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          goto failed;
        }

        d=rasqal_literal_as_floating(l1, &error1) *
          rasqal_literal_as_floating(l2, &error2);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error || error1 || error2)
          goto failed;

        result=rasqal_new_floating_literal(d);
        break;
      }
      
    case RASQAL_EXPR_SLASH:
      {
        rasqal_literal *l1, *l2;
        double d;
        int error1=0;
        int error2=0;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          goto failed;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          goto failed;
        }

        d=rasqal_literal_as_floating(l1, &error1) /
          rasqal_literal_as_floating(l2, &error2);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error || error1 || error2)
          goto failed;
        result=rasqal_new_floating_literal(d);
        break;
      }
      
    case RASQAL_EXPR_REM:
      {
        rasqal_literal *l1, *l2;
        int i;
        int error1=0;
        int error2=0;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          goto failed;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          goto failed;
        }

        i=rasqal_literal_as_integer(l1, &error1) %
          rasqal_literal_as_integer(l2, &error2);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error || error1 || error2)
          goto failed;

        result=rasqal_new_integer_literal(RASQAL_LITERAL_INTEGER, i);
        break;
      }
      
    case RASQAL_EXPR_STR_EQ:
      {
        rasqal_literal *l1, *l2;
        int b;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          goto failed;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          goto failed;
        }

        b=(rasqal_literal_compare(l1, l2, RASQAL_COMPARE_NOCASE, &error) == 0);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error)
          goto failed;

        result=rasqal_new_boolean_literal(b);
        break;
      }
      
    case RASQAL_EXPR_STR_NEQ:
      {
        rasqal_literal *l1, *l2;
        int b;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          goto failed;

        l2=rasqal_expression_evaluate(query, e->arg2);
        if(!l2) {
          rasqal_free_literal(l1);
          goto failed;
        }

        b=(rasqal_literal_compare(l1, l2, RASQAL_COMPARE_NOCASE, &error) != 0);
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        if(error)
          goto failed;

        result=rasqal_new_boolean_literal(b);
        break;
      }

    case RASQAL_EXPR_TILDE:
      {
        rasqal_literal *l=rasqal_expression_evaluate(query, e->arg1);
        int i= ~ rasqal_literal_as_integer(l, &error);
        rasqal_free_literal(l);
        if(error)
          goto failed;

        result=rasqal_new_integer_literal(RASQAL_LITERAL_INTEGER, i);
        break;
      }

    case RASQAL_EXPR_BANG:
      {
        rasqal_literal *l=rasqal_expression_evaluate(query, e->arg1);
        int b= ! rasqal_literal_as_boolean(l, &error);
        rasqal_free_literal(l);
        if(error)
          goto failed;

        result=rasqal_new_boolean_literal(b);
        break;
      }

    case RASQAL_EXPR_STR_MATCH:
    case RASQAL_EXPR_STR_NMATCH: 
      {
        int b=0;
        int flag_i=0; /* flags contains i */
        const unsigned char *p;
        const unsigned char *match_string;
        const unsigned char *pattern;
        rasqal_literal *l1, *l2;
        int rc=0;
#ifdef RASQAL_REGEX_PCRE
        pcre* re;
        int options=0;
        const char *re_error=NULL;
        int erroffset=0;
#endif
#ifdef RASQAL_REGEX_POSIX
        regex_t reg;
        int options=REG_EXTENDED | REG_NOSUB;
#endif
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          goto failed;
        match_string=rasqal_literal_as_string(l1);
        if(!match_string) {
          rasqal_free_literal(l1);
          goto failed;
        }
        
        l2=e->literal;
        pattern=l2->string;
        
        for(p=l2->flags; p && *p; p++)
          if(*p == 'i')
            flag_i++;
          
#ifdef RASQAL_REGEX_PCRE
        if(flag_i)
          options |= PCRE_CASELESS;
        
        re=pcre_compile((const char*)pattern, options, 
                        &re_error, &erroffset, NULL);
        if(!re)
          rasqal_query_error(query, "Regex compile of '%s' failed - %s",
                             pattern, re_error);
        else {
          rc=pcre_exec(re, 
                       NULL, /* no study */
                       (const char*)match_string, strlen((const char*)match_string),
                       0 /* startoffset */,
                       0 /* options */,
                       NULL, 0 /* ovector, ovecsize - no matches wanted */
                       );
          if(rc >= 0)
            b=1;
          else if(rc != PCRE_ERROR_NOMATCH) {
            rasqal_query_error(query, "Regex match failed - returned code %d", rc);
            rc= -1;
          } else
            rc=0;
        }
        
#endif
        
#ifdef RASQAL_REGEX_POSIX
        if(flag_i)
          options |=REG_ICASE;
        
        rc=regcomp(&reg, (const char*)pattern, options);
        if(rc) {
          rasqal_query_error(query, "Regex compile of '%s' failed", pattern);
          rc= -1;
        } else {
          rc=regexec(&reg, (const char*)match_string, 
                     0, NULL, /* nmatch, regmatch_t pmatch[] - no matches wanted */
                     0 /* eflags */
                     );
          if(!rc)
            b=1;
          else if (rc != REG_NOMATCH) {
            rasqal_query_error(query, "Regex match failed - returned code %d", rc);
            rc= -1;
          } else
            rc= 0;
        }
        regfree(&reg);
#endif
#ifdef RASQAL_REGEX_NONE
        rasqal_query_warning(query, "Regex support missing, cannot compare '%s' to '%s'", match_string, pattern);
        b=1;
        rc= -1;
#endif

        RASQAL_DEBUG5("regex match returned %s for '%s' against '%s' (flags=%s)\n", b ? "true" : "false", match_string, pattern, l2->flags ? (char*)l2->flags : "");
        
        if(e->op == RASQAL_EXPR_STR_NMATCH)
          b=1-b;

        rasqal_free_literal(l1);

        if(rc<0)
          goto failed;
        
        result=rasqal_new_boolean_literal(b);
        break;
      }

    case RASQAL_EXPR_LITERAL:
      result=rasqal_new_literal_from_literal(e->literal);
      break;

    case RASQAL_EXPR_FUNCTION:
      rasqal_query_warning(query, "No function expressions support at present.  Returning false.");
      result=rasqal_new_boolean_literal(0);
      break;
      
    case RASQAL_EXPR_CAST:
      {
        rasqal_literal *l1;
        const unsigned char *string;
        unsigned char *new_string;
        raptor_uri *uri;
        
        l1=rasqal_expression_evaluate(query, e->arg1);
        if(!l1)
          goto failed;

        string=rasqal_literal_as_string(l1);
        new_string=(unsigned char*)RASQAL_MALLOC(string, strlen((const char*)string)+1);
        strcpy((char*)new_string, (const char*)string);
        uri=raptor_uri_copy(e->name);

        rasqal_free_literal(l1);

        result=rasqal_new_string_literal(new_string, NULL, uri, NULL);
        break;
      }

  case RASQAL_EXPR_ORDER_COND_ASC:
  case RASQAL_EXPR_ORDER_COND_DESC:
  case RASQAL_EXPR_ORDER_COND_NONE:
      RASQAL_FATAL1("No order conditions yet");
      break;

    case RASQAL_EXPR_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown operation %d", e->op);
  }

  failed:

#ifdef RASQAL_DEBUG
  RASQAL_DEBUG2("result of %p: ", e);
  if(result)
    rasqal_literal_print(result, stderr);
  else
    fputs("(NULL)",stderr);
  fputc('\n', stderr);
#endif
  
  return result;
}


static const char* rasqal_op_labels[RASQAL_EXPR_LAST+1]={
  "UNKNOWN",
  "and",
  "or",
  "eq",
  "neq",
  "lt",
  "gt",
  "le",
  "ge",
  "uminus",
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
  "function",
  "bound",
  "str",
  "lang",
  "datatype",
  "isUri",
  "isBlank",
  "isLiteral",
  "cast",
  "order asc",
  "order desc",
  "order none"
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
    case RASQAL_EXPR_UMINUS:
    case RASQAL_EXPR_BOUND:
    case RASQAL_EXPR_STR:
    case RASQAL_EXPR_LANG:
    case RASQAL_EXPR_DATATYPE:
    case RASQAL_EXPR_ISURI:
    case RASQAL_EXPR_ISBLANK:
    case RASQAL_EXPR_ISLITERAL:
    case RASQAL_EXPR_ORDER_COND_ASC:
    case RASQAL_EXPR_ORDER_COND_DESC:
    case RASQAL_EXPR_ORDER_COND_NONE:
      fputs("op ", fh);
      rasqal_expression_print_op(e, fh);
      fputc('(', fh);
      rasqal_expression_print(e->arg1, fh);
      fputc(')', fh);
      break;

    case RASQAL_EXPR_LITERAL:
      rasqal_literal_print(e->literal, fh);
      break;

    case RASQAL_EXPR_FUNCTION:
      fputs("function(uri=", fh);
      raptor_uri_print(e->name, fh);
      fputs(", args=", fh);
      raptor_sequence_print(e->args, fh);
      fputc(')', fh);
      break;

    case RASQAL_EXPR_CAST:
      fputs("cast(type=", fh);
      raptor_uri_print(e->name, fh);
      fputs(", value=", fh);
      rasqal_expression_print(e->arg1, fh);
      fputc(')', fh);
      break;

    case RASQAL_EXPR_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown operation %d", e->op);
  }
  fputc(')', fh);
}


/* for use with rasqal_expression_foreach and user_data=rasqal_query */
int
rasqal_expression_has_qname(void *user_data, rasqal_expression *e)
{
  if(e->op == RASQAL_EXPR_LITERAL)
    return rasqal_literal_has_qname(e->literal);

  return 0;
}

/* for use with rasqal_expression_foreach and user_data=rasqal_query */
int
rasqal_expression_expand_qname(void *user_data, rasqal_expression *e)
{
  if(e->op == RASQAL_EXPR_LITERAL)
    return rasqal_literal_expand_qname(user_data, e->literal);

  return 0;
}

#endif /* not STANDALONE */




#ifdef STANDALONE
#include <stdio.h>

int main(int argc, char *argv[]);


#define assert_match(function, result, string) do { if(strcmp(result, string)) { fprintf(stderr, #function " failed - returned %s, expected %s\n", result, string); exit(1); } } while(0)


int
main(int argc, char *argv[]) 
{
  const char *program=rasqal_basename(argv[0]);
  rasqal_literal *lit1, *lit2;
  rasqal_expression *expr1, *expr2;
  rasqal_expression* expr;
  rasqal_literal* result;
  int error=0;

  raptor_init();

  rasqal_uri_init();
  
  lit1=rasqal_new_integer_literal(RASQAL_LITERAL_INTEGER, 1);
  expr1=rasqal_new_literal_expression(lit1);
  lit2=rasqal_new_integer_literal(RASQAL_LITERAL_INTEGER, 1);
  expr2=rasqal_new_literal_expression(lit2);
  expr=rasqal_new_2op_expression(RASQAL_EXPR_PLUS, expr1, expr2);

  fprintf(stderr, "%s: expression: ", program);
  rasqal_expression_print(expr, stderr);
  fputc('\n', stderr);

  result=rasqal_expression_evaluate(NULL, expr);

  if(result) {
    int bresult;
    
    fprintf(stderr, "%s: expression result: \n", program);
    rasqal_literal_print(result, stderr);
    fputc('\n', stderr);
    bresult=rasqal_literal_as_boolean(result, &error);
    if(error) {
      fprintf(stderr, "%s: boolean expression FAILED\n", program);
    } else
      fprintf(stderr, "%s: boolean expression result: %d\n", program, bresult);


  } else
    fprintf(stderr, "%s: expression evaluation FAILED with error\n", program);

  rasqal_free_expression(expr);

  if(result)
    rasqal_free_literal(result);

  rasqal_uri_finish();
  
  raptor_finish();

  return error;
}
#endif

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_expr.c - Rasqal general expression support
 *
 * Copyright (C) 2003-2010, David Beckett http://www.dajobe.org/
 * Copyright (C) 2003-2005, University of Bristol, UK http://www.bristol.ac.uk/
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
#include <ctype.h>
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


/**
 * rasqal_new_0op_expression:
 * @world: rasqal_world object
 * @op: Expression operator
 * 
 * Constructor - create a new 0-operand (constant) expression.
 *
 * The operators are:
 * @RASQAL_EXPR_VARSTAR
 *
 * The only operator here is the '*' in COUNT(*) as used by LAQRS.
 * 
 * Return value: a new #rasqal_expression object or NULL on failure
 **/
rasqal_expression*
rasqal_new_0op_expression(rasqal_world* world, rasqal_op op)
{
  rasqal_expression* e;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);

  e = (rasqal_expression*)RASQAL_CALLOC(rasqal_expression, 1, sizeof(*e));
  if(e) {
    e->usage = 1;
    e->world = world;
    e->op = op;
  }
  return e;
}


/**
 * rasqal_new_1op_expression:
 * @world: rasqal_world object
 * @op: Expression operator
 * @arg: Operand 1 
 * 
 * Constructor - create a new 1-operand expression.
 * Takes ownership of the operand expression.
 *
 * The operators are:
 * @RASQAL_EXPR_TILDE @RASQAL_EXPR_BANG @RASQAL_EXPR_UMINUS
 * @RASQAL_EXPR_BOUND @RASQAL_EXPR_STR @RASQAL_EXPR_LANG
 * @RASQAL_EXPR_LANGMATCHES
 * @RASQAL_EXPR_DATATYPE @RASQAL_EXPR_ISURI @RASQAL_EXPR_ISBLANK
 * @RASQAL_EXPR_ISLITERAL @RASQAL_EXPR_ORDER_COND_ASC
 * @RASQAL_EXPR_ORDER_COND_DESC @RASQAL_EXPR_GROUP_COND_ASC
 * @RASQAL_EXPR_GROUP_COND_DESC @RASQAL_EXPR_COUNT @RASQAL_EXPR_SUM
 * @RASQAL_EXPR_AVG @RASQAL_EXPR_MIN @RASQAL_EXPR_MAX
 *
 * @RASQAL_EXPR_BANG and @RASQAL_EXPR_UMINUS are used by RDQL and
 * SPARQL.  @RASQAL_EXPR_TILDE by RDQL only.  The rest by SPARQL
 * only.
 * 
 * Return value: a new #rasqal_expression object or NULL on failure
 **/
rasqal_expression*
rasqal_new_1op_expression(rasqal_world* world, rasqal_op op,
                          rasqal_expression* arg)
{
  rasqal_expression* e = NULL;

  if(!world || !arg)
    goto tidy;
  
  e = (rasqal_expression*)RASQAL_CALLOC(rasqal_expression, 1, sizeof(*e));
  if(e) {
    e->usage = 1;
    e->world = world;
    e->op = op;
    e->arg1 = arg; arg = NULL;
  }
  
  tidy:
  if(arg)
    rasqal_free_expression(arg);

  return e;
}


/**
 * rasqal_new_2op_expression:
 * @world: rasqal_world object
 * @op: Expression operator
 * @arg1: Operand 1 
 * @arg2: Operand 2
 * 
 * Constructor - create a new 2-operand expression.
 * Takes ownership of the operand expressions.
 * 
 * The operators are:
 * @RASQAL_EXPR_AND @RASQAL_EXPR_OR @RASQAL_EXPR_EQ
 * @RASQAL_EXPR_NEQ @RASQAL_EXPR_LT @RASQAL_EXPR_GT @RASQAL_EXPR_LE
 * @RASQAL_EXPR_GE @RASQAL_EXPR_PLUS @RASQAL_EXPR_MINUS
 * @RASQAL_EXPR_STAR @RASQAL_EXPR_SLASH @RASQAL_EXPR_REM
 * @RASQAL_EXPR_STR_EQ @RASQAL_EXPR_STR_NEQ
 *
 * @RASQAL_EXPR_REM @RASQAL_EXPR_STR_EQ and @RASQAL_EXPR_STR_NEQ are
 * not used by SPARQL. @RASQAL_EXPR_REM is used by RDQL.
 * 
 * Return value: a new #rasqal_expression object or NULL on failure
 **/
rasqal_expression*
rasqal_new_2op_expression(rasqal_world* world,
                          rasqal_op op,
                          rasqal_expression* arg1, 
                          rasqal_expression* arg2)
{
  rasqal_expression* e = NULL;

  if(!world || !arg1 || !arg2)
    goto tidy;
  
  e = (rasqal_expression*)RASQAL_CALLOC(rasqal_expression, 1, sizeof(*e));
  if(e) {
    e->usage = 1;
    e->world = world;
    e->op = op;
    e->arg1 = arg1; arg1 = NULL;
    e->arg2 = arg2; arg2 = NULL;
  }
  
tidy:
  if(arg1)
    rasqal_free_expression(arg1);
  if(arg2)
    rasqal_free_expression(arg2);

  return e;
}


/**
 * rasqal_new_3op_expression:
 * @world: rasqal_world object
 * @op: Expression operator
 * @arg1: Operand 1 
 * @arg2: Operand 2
 * @arg3: Operand 3 (may be NULL)
 * 
 * Constructor - create a new 3-operand expression.
 * Takes ownership of the operands.
 * 
 * The only operator is:
 * @RASQAL_EXPR_REGEX
 *
 * Return value: a new #rasqal_expression object or NULL on failure
 **/
rasqal_expression*
rasqal_new_3op_expression(rasqal_world* world,
                          rasqal_op op,
                          rasqal_expression* arg1, 
                          rasqal_expression* arg2,
                          rasqal_expression* arg3)
{
  rasqal_expression* e = NULL;

  if(!world || !arg1 || !arg2) /* arg3 may be NULL */
    goto tidy;

  e = (rasqal_expression*)RASQAL_CALLOC(rasqal_expression, 1, sizeof(*e));
  if(e) {
    e->usage = 1;
    e->world = world;
    e->op = op;
    e->arg1 = arg1; arg1 = NULL;
    e->arg2 = arg2; arg2 = NULL;
    e->arg3 = arg3; arg3 = NULL;
  }

  tidy:
  if(arg1)
    rasqal_free_expression(arg1);
  if(arg2)
    rasqal_free_expression(arg2);
  if(arg3)
    rasqal_free_expression(arg3);

  return e;
}


/**
 * rasqal_new_string_op_expression:
 * @world: rasqal_world object
 * @op: Expression operator
 * @arg1: Operand 1 
 * @literal: Literal operand 2
 * 
 * Constructor - create a new expression with one expression and one string operand.
 * Takes ownership of the operands.
 *
 * The operators are:
 * @RASQAL_EXPR_STR_MATCH (RDQL, SPARQL) and
 * @RASQAL_EXPR_STR_NMATCH (RDQL)
 *
 * Return value: a new #rasqal_expression object or NULL on failure
 **/
rasqal_expression*
rasqal_new_string_op_expression(rasqal_world* world,
                                rasqal_op op,
                                rasqal_expression* arg1,
                                rasqal_literal* literal)
{
  rasqal_expression* e = NULL;

  if(!world || !arg1 || !literal)
    goto tidy;
  
  e = (rasqal_expression*)RASQAL_CALLOC(rasqal_expression, 1, sizeof(*e));
  if(e) {
    e->usage = 1;
    e->world = world;
    e->op = op;
    e->arg1 = arg1; arg1 = NULL;
    e->literal = literal; literal = NULL;
  }

  tidy:
  if(arg1)
    rasqal_free_expression(arg1);
  if(literal)
    rasqal_free_literal(literal);

  return e;
}


/**
 * rasqal_new_literal_expression:
 * @world: rasqal_world object
 * @literal: Literal operand 1
 * 
 * Constructor - create a new expression for a #rasqal_literal
 * Takes ownership of the operand literal.
 * 
 * Return value: a new #rasqal_expression object or NULL on failure
 **/
rasqal_expression*
rasqal_new_literal_expression(rasqal_world* world, rasqal_literal *literal)
{
  rasqal_expression* e;

  if(!world || !literal)
    return NULL;
  
  e = (rasqal_expression*)RASQAL_CALLOC(rasqal_expression, 1, sizeof(*e));
  if(e) {  
    e->usage = 1;
    e->world = world;
    e->op = RASQAL_EXPR_LITERAL;
    e->literal = literal;
  } else {
    rasqal_free_literal(literal);
  }
  return e;
}


/**
 * rasqal_new_function_expression2:
 * @world: rasqal_world object
 * @name: function name
 * @args: sequence of #rasqal_expression function arguments
 * @params: sequence of #rasqal_expression function parameters (or NULL)
 * @flags: extension function bitflags
 * 
 * Constructor - create a new expression for a function with expression arguments.
 * Takes ownership of the function uri and arguments.
 * 
 * Return value: a new #rasqal_expression object or NULL on failure
 **/
rasqal_expression*
rasqal_new_function_expression2(rasqal_world* world,
                                raptor_uri* name,
                                raptor_sequence* args,
                                raptor_sequence* params,
                                unsigned int flags)
{
  rasqal_expression* e = NULL;

  if(!world || !name || !args)
    goto tidy;
  
  e = (rasqal_expression*)RASQAL_CALLOC(rasqal_expression, 1, sizeof(*e));
  if(e) {
    e->usage = 1;
    e->world = world;
    e->op = RASQAL_EXPR_FUNCTION;
    e->name = name; name = NULL;
    e->args = args; args = NULL;
    e->params = params; params = NULL;
    e->flags = flags;
  }
  
  tidy:
  if(name)
    raptor_free_uri(name);
  if(args)
    raptor_free_sequence(args);
  if(params)
    raptor_free_sequence(params);

  return e;
}


/**
 * rasqal_new_function_expression:
 * @world: rasqal_world object
 * @name: function name
 * 
 * Constructor - create a new expression for a function with expression arguments.
 * Takes ownership of the function uri and arguments.
 * 
 * @Deprecated: use rasqal_new_function_expression() with extra args
 *
 * Return value: a new #rasqal_expression object or NULL on failure
 **/
rasqal_expression*
rasqal_new_function_expression(rasqal_world* world,
                               raptor_uri* name,
                               raptor_sequence* args)
{
  return rasqal_new_function_expression2(world, name, args, NULL, 0);
}



/**
 * rasqal_new_cast_expression:
 * @world: rasqal_world object
 * @name: cast datatype URI
 * @value: expression value to cast to @datatype type
 * 
 * Constructor - create a new expression for casting and expression to a datatype.
 * Takes ownership of the datatype uri and expression value.
 * 
 * Return value: a new #rasqal_expression object or NULL on failure
 **/
rasqal_expression*
rasqal_new_cast_expression(rasqal_world* world, raptor_uri* name,
                           rasqal_expression *value) 
{
  rasqal_expression* e = NULL;

  if(!world || !name || !value)
    goto tidy;
  
  e = (rasqal_expression*)RASQAL_CALLOC(rasqal_expression, 1, sizeof(*e));
  if(e) {
    e->usage = 1;
    e->world = world;
    e->op = RASQAL_EXPR_CAST;
    e->name = name; name = NULL;
    e->arg1 = value; value = NULL;
  }

  tidy:
  if(name)
    raptor_free_uri(name);
  if(value)
    rasqal_free_expression(value);

  return e;
}


/**
 * rasqal_new_coalesce_expression:
 * @world: rasqal_world object
 * @args: sequence of #rasqal_expression coalesce arguments
 * 
 * Constructor - create a new COALESCE() with expression arguments.
 *
 * Takes ownership of the @args
 * 
 * Return value: a new #rasqal_expression object or NULL on failure
 **/
rasqal_expression*
rasqal_new_coalesce_expression(rasqal_world* world, raptor_sequence* args)
{
  rasqal_expression* e = NULL;

  if(!world || !args)
    goto tidy;
  
  e = (rasqal_expression*)RASQAL_CALLOC(rasqal_expression, 1, sizeof(*e));
  if(e) {
    e->usage = 1;
    e->world = world;
    e->op = RASQAL_EXPR_COALESCE;
    e->args = args; args = NULL;
  }
  
  tidy:
  if(args)
    raptor_free_sequence(args);

  return e;
}


/**
 * rasqal_expression_clear:
 * @e: expression
 * 
 * Empty an expression of contained content.
 *
 * Intended to be used to deallocate resources from a statically
 * declared #rasqal_expression such as on a stack.
 **/
void
rasqal_expression_clear(rasqal_expression* e)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(e, rasqal_expression);

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
    case RASQAL_EXPR_LANGMATCHES:
    case RASQAL_EXPR_SAMETERM:
    case RASQAL_EXPR_STRLANG:
    case RASQAL_EXPR_STRDT:
      rasqal_free_expression(e->arg1);
      rasqal_free_expression(e->arg2);
      break;
    case RASQAL_EXPR_REGEX:
    case RASQAL_EXPR_IF:
      rasqal_free_expression(e->arg1);
      rasqal_free_expression(e->arg2);
      if(e->arg3)
        rasqal_free_expression(e->arg3);
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
    case RASQAL_EXPR_GROUP_COND_ASC:
    case RASQAL_EXPR_GROUP_COND_DESC:
    case RASQAL_EXPR_COUNT:
    case RASQAL_EXPR_SUM:
    case RASQAL_EXPR_AVG:
    case RASQAL_EXPR_MIN:
    case RASQAL_EXPR_MAX:
    case RASQAL_EXPR_URI:
    case RASQAL_EXPR_IRI:
    case RASQAL_EXPR_BNODE:
    case RASQAL_EXPR_SAMPLE:
      /* arg1 is optional for RASQAL_EXPR_BNODE */
      if(e->arg1)
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
    case RASQAL_EXPR_GROUP_CONCAT:
      raptor_free_uri(e->name);
      raptor_free_sequence(e->args);
      break;
    case RASQAL_EXPR_CAST:
      raptor_free_uri(e->name);
      rasqal_free_expression(e->arg1);
      break;

    case RASQAL_EXPR_VARSTAR:
      /* constants */
      break;
      
    case RASQAL_EXPR_COALESCE:
      raptor_free_sequence(e->args);
      break;

    case RASQAL_EXPR_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown operation %d", e->op);
  }
}


/**
 * rasqal_new_expression_from_expression:
 * @e: #rasqal_expression object to copy or NULL
 *
 * Copy Constructor - create a new #rasqal_expression object from an existing rasqal_expression object.
 * 
 * Return value: a new #rasqal_expression object or NULL if @e is NULL
 **/
rasqal_expression*
rasqal_new_expression_from_expression(rasqal_expression* e)
{
  if(!e)
    return NULL;

  e->usage++;
  return e;
}


/**
 * rasqal_free_expression:
 * @e: #rasqal_expression object
 * 
 * Destructor - destroy a #rasqal_expression object.
 *
 **/
void
rasqal_free_expression(rasqal_expression* e)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(e, rasqal_expression);
  
  if(--e->usage)
    return;

  rasqal_expression_clear(e);
  RASQAL_FREE(rasqal_expression, e);
}


/**
 * rasqal_expression_visit:
 * @e:  #rasqal_expression to visit
 * @fn: visit function
 * @user_data: user data to pass to visit function
 * 
 * Visit a user function over a #rasqal_expression
 * 
 * If the user function @fn returns non-0, the visit is truncated
 * and the value is returned.
 *
 * Return value: non-0 if the visit was truncated.
 **/
int
rasqal_expression_visit(rasqal_expression* e, 
                        rasqal_expression_visit_fn fn,
                        void *user_data)
{
  int i;
  int result = 0;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(e, rasqal_expression, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(fn, rasqal_expression_visit_fn, 1);

  /* This ordering allows fn to potentially edit 'e' in-place */
  result = fn(user_data, e);
  if(result)
    return result;

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
    case RASQAL_EXPR_LANGMATCHES:
    case RASQAL_EXPR_SAMETERM:
    case RASQAL_EXPR_STRLANG:
    case RASQAL_EXPR_STRDT:
      return rasqal_expression_visit(e->arg1, fn, user_data) ||
             rasqal_expression_visit(e->arg2, fn, user_data);
      break;
    case RASQAL_EXPR_REGEX:
    case RASQAL_EXPR_IF:
      return rasqal_expression_visit(e->arg1, fn, user_data) ||
             rasqal_expression_visit(e->arg2, fn, user_data) ||
             (e->arg3 && rasqal_expression_visit(e->arg3, fn, user_data));
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
    case RASQAL_EXPR_GROUP_COND_ASC:
    case RASQAL_EXPR_GROUP_COND_DESC:
    case RASQAL_EXPR_COUNT:
    case RASQAL_EXPR_SUM:
    case RASQAL_EXPR_AVG:
    case RASQAL_EXPR_MIN:
    case RASQAL_EXPR_MAX:
    case RASQAL_EXPR_URI:
    case RASQAL_EXPR_IRI:
    case RASQAL_EXPR_BNODE:
    case RASQAL_EXPR_SAMPLE:
      /* arg1 is optional for RASQAL_EXPR_BNODE */
      return (e->arg1) ? rasqal_expression_visit(e->arg1, fn, user_data) : 1;
      break;
    case RASQAL_EXPR_STR_MATCH:
    case RASQAL_EXPR_STR_NMATCH:
      return fn(user_data, e->arg1);
      break;
    case RASQAL_EXPR_LITERAL:
      return 0;
    case RASQAL_EXPR_FUNCTION:
    case RASQAL_EXPR_COALESCE:
    case RASQAL_EXPR_GROUP_CONCAT:
      for(i = 0; i < raptor_sequence_size(e->args); i++) {
        rasqal_expression* e2;
        e2 = (rasqal_expression*)raptor_sequence_get_at(e->args, i);
        if(!rasqal_expression_visit(e2, fn, user_data)) {
          result = 0;
          break;
        }
      }
      return result;
      break;

    case RASQAL_EXPR_VARSTAR:
      /* constants */
      return 0;
      break;
      
    case RASQAL_EXPR_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown operation %d", e->op);
      return -1; /* keep some compilers happy */
  }
}


/* 
 * rasqal_language_matches:
 * @lang_tag: language tag such as "en" or "en-US" or "ab-cd-ef"
 * @lang_range: language range such as "*" (SPARQL) or "en" or "ab-cd"
 *
 * INTERNAL - Match a language tag against a language range
 *
 * Returns true if @lang_range matches @lang_tag per
 *   Matching of Language Tags [RFC4647] section 2.1
 * RFC4647 defines a case-insensitive, hierarchical matching
 * algorithm which operates on ISO-defined subtags for language and
 * country codes, and user defined subtags.
 *
 * (Note: RFC3066 section 2.5 matching is identical to
 * RFC4647 section 3.3.1 Basic Filtering )
 * 
 * In SPARQL, a language-range of "*" matches any non-empty @lang_tag string.
 * See http://www.w3.org/TR/2007/WD-rdf-sparql-query-20070326/#func-langMatches
 *
 * Return value: non-0 if true
 */
static int
rasqal_language_matches(const unsigned char* lang_tag,
                        const unsigned char* lang_range) 
{
  int b= 0;

  if(!(lang_tag && lang_range && *lang_tag && *lang_range)) {
    /* One of the arguments is NULL or the empty string */
    return 0;
  }

  /* Now have two non-empty arguments */

  /* Simple range string "*" matches anything excluding NULL/empty
   * lang_tag (checked above)
   */
  if(lang_range[0] == '*') {
    if(!lang_range[1])
      b = 1;
    return b;
  }
  
  while (1) {
    char tag_c   = tolower(*lang_tag++);
    char range_c = tolower(*lang_range++);
    if ((!tag_c && !range_c) || (!range_c && tag_c == '-')) {
      /* EITHER
       *   The end of both strings (thus everything previous matched
       *   such as e.g. tag "fr-CA" matching range "fr-ca")
       * OR
       *   The end of the range and end of the tag prefix (e.g. tag
       *   "en-US" matching range "en")
       * means a match
       */
      b = 1;
      break;
    } 
    if (range_c != tag_c) {
      /* If a difference was found - including one of the
       * strings being shorter than the other, it means no match
       * (b is set to 0 above)
       */
      break;
    }
  }

  return b;
}


/* 
 * rasqal_expression_evaluate_strmatch:
 * @world: #rasqal_world
 * @locator: error locator object
 * @e: The expression to evaluate.
 * @flags: Compare flags
 *
 * INTERNAL - Evaluate RASQAL_EXPR_STR_MATCH, RASQAL_EXPR_STR_NMATCH and
 * RASQAL_EXPR_REGEX expressions.
 *
 * Return value: A #rasqal_literal value or NULL on failure.
 */
static rasqal_literal*
rasqal_expression_evaluate_strmatch(rasqal_world *world,
                                    raptor_locator *locator,
                                    rasqal_expression *e,
                                    int flags)
{
  int b=0;
  int flag_i=0; /* flags contains i */
  const unsigned char *p;
  const unsigned char *match_string;
  const unsigned char *pattern;
  const unsigned char *regex_flags;
  rasqal_literal *l1, *l2, *l3;
  int error=0;
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
    
  l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
  if(!l1)
    goto failed;

  match_string=rasqal_literal_as_string_flags(l1, flags, &error);
  if(error || !match_string) {
    rasqal_free_literal(l1);
    goto failed;
  }
    
  l3=NULL;
  regex_flags=NULL;
  if(e->op == RASQAL_EXPR_REGEX) {
    l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
    if(!l2) {
      rasqal_free_literal(l1);
      goto failed;
    }

    if(e->arg3) {
      l3 = rasqal_expression_evaluate(world, locator, e->arg3, flags);
      if(!l3) {
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        goto failed;
      }
      regex_flags=l3->string;
    }
      
  } else {
    l2=e->literal;
    regex_flags=l2->flags;
  }
  pattern=l2->string;
    
  for(p=regex_flags; p && *p; p++)
    if(*p == 'i')
      flag_i++;
      
#ifdef RASQAL_REGEX_PCRE
  if(flag_i)
    options |= PCRE_CASELESS;
    
  re=pcre_compile((const char*)pattern, options, 
                  &re_error, &erroffset, NULL);
  if(!re) {
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, locator,
                            "Regex compile of '%s' failed - %s", pattern, re_error);
    rc= -1;
  } else {
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
      rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, locator,
                              "Regex match failed - returned code %d", rc);
      rc= -1;
    } else
      rc=0;
  }
  pcre_free(re);
  
#endif
    
#ifdef RASQAL_REGEX_POSIX
  if(flag_i)
    options |=REG_ICASE;
    
  rc=regcomp(&reg, (const char*)pattern, options);
  if(rc) {
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, locator,
                            "Regex compile of '%s' failed", pattern);
    rc= -1;
  } else {
    rc=regexec(&reg, (const char*)match_string, 
               0, NULL, /* nmatch, regmatch_t pmatch[] - no matches wanted */
               0 /* eflags */
               );
    if(!rc)
      b=1;
    else if (rc != REG_NOMATCH) {
      rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, locator,
                              "Regex match failed - returned code %d", rc);
      rc= -1;
    } else
      rc= 0;
  }
  regfree(&reg);
#endif

#ifdef RASQAL_REGEX_NONE
  rasqal_log_error_simple(world,
                          RAPTOR_LOG_LEVEL_WARN,
                          locator,
                          "Regex support missing, cannot compare '%s' to '%s'", match_string, pattern);
  b=1;
  rc= -1;
#endif

  RASQAL_DEBUG5("regex match returned %s for '%s' against '%s' (flags=%s)\n", b ? "true" : "false", match_string, pattern, l2->flags ? (char*)l2->flags : "");
  
  if(e->op == RASQAL_EXPR_STR_NMATCH)
    b=1-b;

  rasqal_free_literal(l1);
  if(e->op == RASQAL_EXPR_REGEX) {
    rasqal_free_literal(l2);
    if(l3)
      rasqal_free_literal(l3);
  }
    
  if(rc<0)
    goto failed;
    
  return rasqal_new_boolean_literal(world, b);

  failed:
  return NULL;
}


/**
 * rasqal_expression_evaluate:
 * @world: #rasqal_world
 * @locator: error locator (or NULL)
 * @e: The expression to evaluate.
 * @flags: Flags for rasqal_literal_compare() and RASQAL_COMPARE_NOCASE for string matches.
 * 
 * Evaluate a #rasqal_expression tree to give a #rasqal_literal result
 * or error.
 * 
 * Return value: a #rasqal_literal value or NULL on failure.
 **/
rasqal_literal*
rasqal_expression_evaluate(rasqal_world *world, raptor_locator *locator,
                           rasqal_expression* e, int flags)
{
  rasqal_literal* result = NULL;
  rasqal_literal *l1;
  rasqal_literal *l2;
  const unsigned char *s;

  /* pack vars from different switch cases in unions to save some stack space */
  union {
    struct { int e1; int e2; } errs;
    struct { int dummy_do_not_mask_e; int free_literal; } flags;
    int e;
  } errs;
  union {
    struct { int b1; int b2; } bools;
    int b;
    int i;
    raptor_uri *dt_uri;
    const unsigned char *s;
    unsigned char *new_s;
    rasqal_variable *v;
  } vars;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(e, rasqal_expression, NULL);

  errs.e = 0;

#ifdef RASQAL_DEBUG
  RASQAL_DEBUG2("evaluating expression %p: ", e);
  rasqal_expression_print(e, stderr);
  fprintf(stderr, "\n");
#endif
  
  switch(e->op) {
    case RASQAL_EXPR_AND:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1) {
        errs.errs.e1=1;
        vars.bools.b1=0;
      } else {
        errs.errs.e1=0;
        vars.bools.b1=rasqal_literal_as_boolean(l1, &errs.errs.e1);
        rasqal_free_literal(l1);
      }

      l1 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l1) {
        errs.errs.e2=1;
        vars.bools.b2=0;
      } else {
        errs.errs.e2=0;
        vars.bools.b2=rasqal_literal_as_boolean(l1, &errs.errs.e2);
        rasqal_free_literal(l1);
      }

      /* See http://www.w3.org/TR/2005/WD-rdf-sparql-query-20051123/#truthTable */
      if(!errs.errs.e1 && !errs.errs.e2) {
        /* No type error, answer is A && B */
        vars.b = vars.bools.b1 && vars.bools.b2; /* don't need b1,b2 anymore */
      } else {
        if((!vars.bools.b1 && errs.errs.e2) || (errs.errs.e1 && vars.bools.b2))
          /* F && E => F.   E && F => F. */
          vars.b=0;
        else
          /* Otherwise E */
          goto failed;
      }
      result=rasqal_new_boolean_literal(world, vars.b);
      break;
      
    case RASQAL_EXPR_OR:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1) {
        errs.errs.e1=1;
        vars.bools.b1=0;
      } else {
        errs.errs.e1=0;
        vars.bools.b1=rasqal_literal_as_boolean(l1, &errs.errs.e1);
        rasqal_free_literal(l1);
      }

      l1 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l1) {
        errs.errs.e2=1;
        vars.bools.b2=0;
      } else {
        errs.errs.e2=0;
        vars.bools.b2=rasqal_literal_as_boolean(l1, &errs.errs.e2);
        rasqal_free_literal(l1);
      }

      /* See http://www.w3.org/TR/2005/WD-rdf-sparql-query-20051123/#truthTable */
      if(!errs.errs.e1 && !errs.errs.e2) {
        /* No type error, answer is A || B */
        vars.b = vars.bools.b1 || vars.bools.b2; /* don't need b1,b2 anymore */
      } else {
        if((vars.bools.b1 && errs.errs.e2) || (errs.errs.e1 && vars.bools.b2))
          /* T || E => T.   E || T => T */
          vars.b=1;
        else
          /* Otherwise E */
          goto failed;
      }
      result=rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_EQ:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      /* FIXME - this should probably be checked at literal creation
       * time
       */
      if(!rasqal_xsd_datatype_check(l1->type, l1->string, flags) ||
         !rasqal_xsd_datatype_check(l2->type, l2->string, flags)) {
        RASQAL_DEBUG1("One of the literals was invalid\n");
        goto failed;
      }

      vars.b=(rasqal_literal_equals_flags(l1, l2, flags, &errs.e) != 0);
#if RASQAL_DEBUG > 1
      if(errs.e)
        RASQAL_DEBUG1("rasqal_literal_equals_flags returned: FAILURE\n");
      else
        RASQAL_DEBUG2("rasqal_literal_equals_flags returned: %d\n", vars.b);
#endif
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;
      result=rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_NEQ:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.b=(rasqal_literal_not_equals_flags(l1, l2, flags, &errs.e) != 0);
#if RASQAL_DEBUG > 1
      if(errs.e)
        RASQAL_DEBUG1("rasqal_literal_not_equals_flags returned: FAILURE\n");
      else
        RASQAL_DEBUG2("rasqal_literal_not_equals_flags returned: %d\n", vars.b);
#endif
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;
      result=rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_LT:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.b=(rasqal_literal_compare(l1, l2, flags, &errs.e) < 0);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;
      result=rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_GT:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.b=(rasqal_literal_compare(l1, l2, flags, &errs.e) > 0);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;
      result=rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_LE:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.b=(rasqal_literal_compare(l1, l2, flags, &errs.e) <= 0);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;
      result=rasqal_new_boolean_literal(world, vars.b);
      break;        

    case RASQAL_EXPR_GE:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.b=(rasqal_literal_compare(l1, l2, flags, &errs.e) >= 0);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;
      result=rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_UMINUS:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      result=rasqal_literal_negate(l1, &errs.e);
      rasqal_free_literal(l1);
      if(errs.e)
        goto failed;
      break;

    case RASQAL_EXPR_BOUND:
      /* Do not use rasqal_expression_evaluate() here since
       * we need to check the argument is a variable, and
       * that function will flatten such thing to literals
       * as early as possible. See (FLATTEN_LITERAL) below
       */
      if(!e->arg1 || e->arg1->op != RASQAL_EXPR_LITERAL)
        goto failed;

      l1=e->arg1->literal;
      if(!l1 || l1->type != RASQAL_LITERAL_VARIABLE)
        goto failed;

      vars.v=rasqal_literal_as_variable(l1);
      if(!vars.v)
        goto failed;

      result=rasqal_new_boolean_literal(world, (vars.v->value != NULL));
      break;

    case RASQAL_EXPR_STR:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      /* Note: flags removes RASQAL_COMPARE_XQUERY as this is the
       * explicit stringify operation
       */
      s=rasqal_literal_as_string_flags(l1, (flags & ~RASQAL_COMPARE_XQUERY),
                                       &errs.e);
      if(!s || errs.e) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.new_s=(unsigned char *)RASQAL_MALLOC(cstring, strlen((const char*)s)+1);
      if(!vars.new_s) {
        rasqal_free_literal(l1);
        goto failed;
      }
      strcpy((char*)vars.new_s, (const char*)s);

      result=rasqal_new_string_literal(world, vars.new_s, NULL, NULL, NULL);
      rasqal_free_literal(l1);

      break;
      
    case RASQAL_EXPR_LANG:
      errs.flags.free_literal=1;
      
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      vars.v=rasqal_literal_as_variable(l1);
      if(vars.v) {
        rasqal_free_literal(l1);
        l1=vars.v->value; /* don't need vars.v after this */
        errs.flags.free_literal=0;
        if(!l1)
          goto failed;
      }

      if(rasqal_literal_get_rdf_term_type(l1) != RASQAL_LITERAL_STRING) {
        if(errs.flags.free_literal)
          rasqal_free_literal(l1);
        goto failed;
      }

      if(l1->language) {
        vars.new_s=(unsigned char*)RASQAL_MALLOC(cstring,
                                                 strlen(l1->language)+1);
        if(!vars.new_s) {
          if(errs.flags.free_literal)
            rasqal_free_literal(l1);
          goto failed;
        }
        strcpy((char*)vars.new_s, l1->language);
      } else  {
        vars.new_s=(unsigned char*)RASQAL_MALLOC(cstring, 1);
        if(!vars.new_s) {
          if(errs.flags.free_literal)
            rasqal_free_literal(l1);
          goto failed;
        }
        *vars.new_s='\0';
      }
      result=rasqal_new_string_literal(world, vars.new_s, NULL, NULL, NULL);
      
      if(errs.flags.free_literal)
        rasqal_free_literal(l1);

      break;

    case RASQAL_EXPR_LANGMATCHES:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      s=rasqal_literal_as_string_flags(l1, flags, &errs.e);
      vars.s=rasqal_literal_as_string_flags(l2, flags, &errs.e);

      if(errs.e)
        vars.b=0;
      else
        vars.b=rasqal_language_matches(s, vars.s); /* don't need s anymore */
      
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);

      result=rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_DATATYPE:
      errs.flags.free_literal=1;
      vars.dt_uri=NULL;
      
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      vars.v=rasqal_literal_as_variable(l1);
      if(vars.v) {
        rasqal_free_literal(l1);
        l1=vars.v->value; /* don't need vars.v after this */
        errs.flags.free_literal=0;
        if(!l1)
          goto failed;
      }

      if(rasqal_literal_get_rdf_term_type(l1) != RASQAL_LITERAL_STRING) {
        if(errs.flags.free_literal)
          rasqal_free_literal(l1);
        goto failed;
      }

      if(l1->language) {
        if(errs.flags.free_literal)
          rasqal_free_literal(l1);
        goto failed;
      }

      /* The datatype of a plain literal is xsd:string */
      vars.dt_uri=l1->datatype;
      if(!vars.dt_uri && l1->type == RASQAL_LITERAL_STRING)
        vars.dt_uri=rasqal_xsd_datatype_type_to_uri(l1->world,
                                                    RASQAL_LITERAL_XSD_STRING);

      if(!vars.dt_uri) {
        if(errs.flags.free_literal)
          rasqal_free_literal(l1);
        goto failed;
      }
      
      result = rasqal_new_uri_literal(world, raptor_uri_copy(vars.dt_uri));

      if(errs.flags.free_literal)
        rasqal_free_literal(l1);

      break;

    case RASQAL_EXPR_ISURI:
      errs.flags.free_literal=1;
      
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;
      
      vars.v=rasqal_literal_as_variable(l1);
      if(vars.v) {
        rasqal_free_literal(l1);
        l1=vars.v->value; /* don't need vars.v after this */
        errs.flags.free_literal=0;
        if(!l1)
          goto failed;
      }

      vars.b=(l1->type == RASQAL_LITERAL_URI);
      
      if(errs.flags.free_literal)
        rasqal_free_literal(l1);

      result=rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_ISBLANK:
      errs.flags.free_literal=1;
      
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;
      
      vars.v=rasqal_literal_as_variable(l1);
      if(vars.v) {
        rasqal_free_literal(l1);
        l1=vars.v->value; /* don't need vars.v after this */
        errs.flags.free_literal=0;
        if(!l1)
          goto failed;
      }

      vars.b=(l1->type == RASQAL_LITERAL_BLANK);

      if(errs.flags.free_literal)
        rasqal_free_literal(l1);

      result=rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_ISLITERAL:
      errs.flags.free_literal=1;
      
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;
      
      vars.v=rasqal_literal_as_variable(l1);
      if(vars.v) {
        rasqal_free_literal(l1);
        l1=vars.v->value; /* don't need vars.v after this */
        errs.flags.free_literal=0;
        if(!l1)
          goto failed;
      }

      vars.b=(rasqal_literal_get_rdf_term_type(l1) == RASQAL_LITERAL_STRING);

      if(errs.flags.free_literal)
        rasqal_free_literal(l1);

      result=rasqal_new_boolean_literal(world, vars.b);
      break;
      
    case RASQAL_EXPR_PLUS:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      result=rasqal_literal_add(l1, l2, &errs.e);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;
      
      break;
        
    case RASQAL_EXPR_MINUS:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      result=rasqal_literal_subtract(l1, l2, &errs.e);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;
      
      break;
      
    case RASQAL_EXPR_STAR:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      result=rasqal_literal_multiply(l1, l2, &errs.e);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;
      
      break;
      
    case RASQAL_EXPR_SLASH:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      result=rasqal_literal_divide(l1, l2, &errs.e);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;
      
      break;
      
    case RASQAL_EXPR_REM:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.i=rasqal_literal_as_integer(l2, &errs.errs.e2);
      /* error if divisor is zero */
      if(!vars.i)
        errs.errs.e2=1;
      else
        vars.i=rasqal_literal_as_integer(l1, &errs.errs.e1) % vars.i;

      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.errs.e1 || errs.errs.e2)
        goto failed;

      result=rasqal_new_integer_literal(world, RASQAL_LITERAL_INTEGER, vars.i);
      break;
      
    case RASQAL_EXPR_STR_EQ:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.b=(rasqal_literal_compare(l1, l2, flags | RASQAL_COMPARE_NOCASE,
                                     &errs.e) == 0);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;

      result=rasqal_new_boolean_literal(world, vars.b);
      break;
      
    case RASQAL_EXPR_STR_NEQ:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.b=(rasqal_literal_compare(l1, l2, flags | RASQAL_COMPARE_NOCASE, 
                                     &errs.e) != 0);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;

      result=rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_TILDE:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      vars.i= ~ rasqal_literal_as_integer(l1, &errs.e);
      rasqal_free_literal(l1);
      if(errs.e)
        goto failed;

      result=rasqal_new_integer_literal(world, RASQAL_LITERAL_INTEGER, vars.i);
      break;

    case RASQAL_EXPR_BANG:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      vars.b= ! rasqal_literal_as_boolean(l1, &errs.e);
      rasqal_free_literal(l1);
      if(errs.e)
        goto failed;

      result=rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_STR_MATCH:
    case RASQAL_EXPR_STR_NMATCH:
    case RASQAL_EXPR_REGEX:
      result=rasqal_expression_evaluate_strmatch(world, locator, e, flags);
      break;

    case RASQAL_EXPR_LITERAL:
      /* flatten any literal to a value as soon as possible - this
       * removes variables from expressions the first time they are seen.
       * (FLATTEN_LITERAL)
       */
      result=rasqal_new_literal_from_literal(rasqal_literal_value(e->literal));
      break;

    case RASQAL_EXPR_FUNCTION:
      rasqal_log_error_simple(world,
                              RAPTOR_LOG_LEVEL_WARN,
                              locator,
                              "No function expressions support at present.  Returning false.");
      result=rasqal_new_boolean_literal(world, 0);
      break;
      
    case RASQAL_EXPR_CAST:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      result=rasqal_literal_cast(l1, e->name, flags, &errs.e);

      rasqal_free_literal(l1);
      if(errs.e)
        goto failed;

      break;

    case RASQAL_EXPR_ORDER_COND_ASC:
    case RASQAL_EXPR_ORDER_COND_DESC:
    case RASQAL_EXPR_GROUP_COND_ASC:
    case RASQAL_EXPR_GROUP_COND_DESC:
    case RASQAL_EXPR_COUNT:
    case RASQAL_EXPR_SUM:
    case RASQAL_EXPR_AVG:
    case RASQAL_EXPR_MIN:
    case RASQAL_EXPR_MAX:
      result = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      break;

    case RASQAL_EXPR_VARSTAR:
      /* constants */
      break;
      
    case RASQAL_EXPR_SAMETERM:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;
      
      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.b = rasqal_literal_same_term(l1, l2);
#if RASQAL_DEBUG > 1
      RASQAL_DEBUG2("rasqal_literal_same_term returned: %d\n", vars.b);
#endif
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;

      result=rasqal_new_boolean_literal(world, vars.b);
      break;
      
    case RASQAL_EXPR_COALESCE:
      for(vars.i = 0; vars.i < raptor_sequence_size(e->args); vars.i++) {
        rasqal_expression* e2;
        e2 = (rasqal_expression*)raptor_sequence_get_at(e->args, vars.i);
        result = rasqal_expression_evaluate(world, locator, e2, flags);
        if(result)
          break;
      }
      break;

    case RASQAL_EXPR_IF:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      /* IF condition */
      vars.b = rasqal_literal_as_boolean(l1, &errs.e);
      rasqal_free_literal(l1);

      if(errs.e)
        goto failed;

      /* condition is true: evaluate arg2 or false: evaluate arg3 */
      result = rasqal_expression_evaluate(world, locator,
                                          vars.b ? e->arg2 : e->arg3,
                                          flags);
      break;

    case RASQAL_EXPR_URI:
    case RASQAL_EXPR_IRI:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      s = rasqal_literal_as_string_flags(l1, flags, &errs.e);
      if(errs.e) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.dt_uri = raptor_new_uri(world->raptor_world_ptr, s);
      rasqal_free_literal(l1);
      if(!vars.dt_uri)
        goto failed;
      
      result = rasqal_new_uri_literal(world, vars.dt_uri);
      /* vars.dt_uri becomes owned by the result literal */
      break;

    case RASQAL_EXPR_STRLANG:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;
      
      s = rasqal_literal_as_string_flags(l1, flags, &errs.e);
      if(errs.e) {
        rasqal_free_literal(l1);
        goto failed;
      }

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.s = rasqal_literal_as_string_flags(l1, flags, &errs.e);
      if(errs.e) {
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        goto failed;
      }

      result = rasqal_new_string_literal(world, s, (const char*)vars.s,
                                         /*datatype */ NULL, /* qname */ NULL);

      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      break;

    case RASQAL_EXPR_STRDT:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;
      
      s = rasqal_literal_as_string_flags(l1, flags, &errs.e);
      if(errs.e) {
        rasqal_free_literal(l1);
        goto failed;
      }

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.dt_uri = rasqal_literal_as_uri(l2);
      if(vars.dt_uri) {
        vars.dt_uri = raptor_uri_copy(vars.dt_uri);
      } else {
        const unsigned char *uri_string;
        uri_string = rasqal_literal_as_string_flags(l2, flags, &errs.e);
        if(errs.e) {
          rasqal_free_literal(l1);
          rasqal_free_literal(l2);
          goto failed;
        }
        vars.dt_uri = raptor_new_uri(world->raptor_world_ptr, uri_string);
        if(!vars.dt_uri) {
          rasqal_free_literal(l1);
          rasqal_free_literal(l2);
          goto failed;
        }
      }
      
      result = rasqal_new_string_literal(world, s, /* language */ NULL,
                                         vars.dt_uri, /* qname */ NULL);

      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      break;

    case RASQAL_EXPR_BNODE:
      if(e->arg1) {
        l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
        if(!l1)
          goto failed;

        s = rasqal_literal_as_string_flags(l1, flags, &errs.e);
        if(errs.e) {
          rasqal_free_literal(l1);
          goto failed;
        }
      } else {
        s = rasqal_world_generate_bnodeid(world, NULL);
        if(!s)
          goto failed;
      }

      result = rasqal_new_simple_literal(world, RASQAL_LITERAL_BLANK, s);
      break;

    case RASQAL_EXPR_SAMPLE:
      RASQAL_FATAL1("SAMPLE() not implemented");
      break;
      
    case RASQAL_EXPR_GROUP_CONCAT:
      RASQAL_FATAL1("GROUP_CONCAT() not implemented");
      break;

    case RASQAL_EXPR_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown operation %d", e->op);
  }

  got_result:

#ifdef RASQAL_DEBUG
  RASQAL_DEBUG2("result of %p: ", e);
  rasqal_expression_print(e, stderr);
  fputs( ": ", stderr);
  if(result)
    rasqal_literal_print(result, stderr);
  else
    fputs("FAILURE",stderr);
  fputc('\n', stderr);
#endif
  
  return result;

  failed:

  if(result) {
    rasqal_free_literal(result);
    result=NULL;
  }
  goto got_result;
}


static const char* const rasqal_op_labels[RASQAL_EXPR_LAST+1]={
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
  "langMatches",
  "regex",
  "group asc",
  "group desc",
  "count",
  "varstar",
  "sameTerm",
  "sum",
  "avg",
  "min",
  "max",
  "coalesce"
  "if",
  "uri",
  "iri",
  "strlang",
  "strdt",
  "bnode",
  "group_concat",
  "sample"
};


/**
 * rasqal_expression_write_op:
 * @e: the #rasqal_expression object
 * @iostr: the #raptor_iostream to write to
 * 
 * Write a rasqal expression operator to an iostream in a debug format.
 *
 * The print debug format may change in any release.
 **/
void
rasqal_expression_write_op(rasqal_expression* e, raptor_iostream* iostr)
{
  rasqal_op op;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN(e, rasqal_expression);

  op = e->op;
  if(op > RASQAL_EXPR_LAST)
    op = RASQAL_EXPR_UNKNOWN;
  raptor_iostream_string_write(rasqal_op_labels[(int)op], iostr);
}


/**
 * rasqal_expression_print_op:
 * @e: the #rasqal_expression object
 * @fh: the FILE* handle to print to
 * 
 * Print a rasqal expression operator in a debug format.
 *
 * The print debug format may change in any release.
 **/
void
rasqal_expression_print_op(rasqal_expression* e, FILE* fh)
{
  rasqal_op op;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN(e, rasqal_expression);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(fh, FILE*);

  op = e->op;
  if(op > RASQAL_EXPR_LAST)
    op = RASQAL_EXPR_UNKNOWN;
  fputs(rasqal_op_labels[(int)op], fh);
}


/**
 * rasqal_expression_write:
 * @e: #rasqal_expression object.
 * @iostr: The #raptor_iostream to write to.
 * 
 * Write a Rasqal expression to an iostream in a debug format.
 * 
 * The print debug format may change in any release.
 **/
void
rasqal_expression_write(rasqal_expression* e, raptor_iostream* iostr)
{
  int i;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(e, rasqal_expression);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(iostr, raptor_iostr);

  raptor_iostream_counted_string_write("expr(", 5, iostr);
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
    case RASQAL_EXPR_LANGMATCHES:
    case RASQAL_EXPR_REGEX:
    case RASQAL_EXPR_SAMETERM:
    case RASQAL_EXPR_IF:
    case RASQAL_EXPR_STRLANG:
    case RASQAL_EXPR_STRDT:
      raptor_iostream_counted_string_write("op ", 3, iostr);
      rasqal_expression_write_op(e, iostr);
      raptor_iostream_write_byte('(', iostr);
      rasqal_expression_write(e->arg1, iostr);
      raptor_iostream_counted_string_write(", ", 2, iostr);
      rasqal_expression_write(e->arg2, iostr);
      /* There are two 3-op expressions - both handled here */
      if((e->op == RASQAL_EXPR_REGEX || e->op == RASQAL_EXPR_IF) && e->arg3) {
        raptor_iostream_counted_string_write(", ", 2, iostr);
        rasqal_expression_write(e->arg3, iostr);
      }
      raptor_iostream_write_byte(')', iostr);
      break;
    case RASQAL_EXPR_STR_MATCH:
    case RASQAL_EXPR_STR_NMATCH:
      raptor_iostream_counted_string_write("op ", 3, iostr);
      rasqal_expression_write_op(e, iostr);
      raptor_iostream_write_byte('(', iostr);
      rasqal_expression_write(e->arg1, iostr);
      raptor_iostream_counted_string_write(", ", 2, iostr);
      rasqal_literal_write(e->literal, iostr);
      raptor_iostream_write_byte(')', iostr);
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
    case RASQAL_EXPR_GROUP_COND_ASC:
    case RASQAL_EXPR_GROUP_COND_DESC:
    case RASQAL_EXPR_COUNT:
    case RASQAL_EXPR_SUM:
    case RASQAL_EXPR_AVG:
    case RASQAL_EXPR_MIN:
    case RASQAL_EXPR_MAX:
    case RASQAL_EXPR_URI:
    case RASQAL_EXPR_IRI:
    case RASQAL_EXPR_BNODE:
    case RASQAL_EXPR_SAMPLE:
      raptor_iostream_counted_string_write("op ", 3, iostr);
      rasqal_expression_write_op(e, iostr);
      raptor_iostream_write_byte('(', iostr);
      /* arg1 is optional for RASQAL_EXPR_BNODE */
      if(e->arg1)
        rasqal_expression_write(e->arg1, iostr);
      raptor_iostream_write_byte(')', iostr);
      break;

    case RASQAL_EXPR_LITERAL:
      rasqal_literal_write(e->literal, iostr);
      break;

    case RASQAL_EXPR_FUNCTION:
      raptor_iostream_counted_string_write("function(uri=", 13, iostr);
      raptor_uri_write(e->name, iostr);
      raptor_iostream_counted_string_write(", args=", 7, iostr);
      for(i=0; i<raptor_sequence_size(e->args); i++) {
        rasqal_expression* e2;
        if(i>0)
          raptor_iostream_counted_string_write(", ", 2, iostr);
        e2=(rasqal_expression*)raptor_sequence_get_at(e->args, i);
        rasqal_expression_write(e2, iostr);
      }
      raptor_iostream_write_byte(')', iostr);
      break;

    case RASQAL_EXPR_CAST:
      raptor_iostream_counted_string_write("cast(type=", 10, iostr);
      raptor_uri_write(e->name, iostr);
      raptor_iostream_counted_string_write(", value=", 8, iostr);
      rasqal_expression_write(e->arg1, iostr);
      raptor_iostream_write_byte(')', iostr);
      break;

    case RASQAL_EXPR_VARSTAR:
      raptor_iostream_counted_string_write("varstar", 7, iostr);
      break;
      
    case RASQAL_EXPR_COALESCE:
      raptor_iostream_counted_string_write("coalesce(", 9, iostr);
      for(i = 0; i < raptor_sequence_size(e->args); i++) {
        rasqal_expression* e2;
        if(i > 0)
          raptor_iostream_counted_string_write(", ", 2, iostr);
        e2 = (rasqal_expression*)raptor_sequence_get_at(e->args, i);
        rasqal_expression_write(e2, iostr);
      }
      raptor_iostream_write_byte(')', iostr);
      break;

    case RASQAL_EXPR_GROUP_CONCAT:
      raptor_iostream_counted_string_write("group_concat(args=", 18, iostr);
      for(i = 0; i<raptor_sequence_size(e->args); i++) {
        rasqal_expression* e2;
        if(i > 0)
          raptor_iostream_counted_string_write(", ", 2, iostr);
        e2 = (rasqal_expression*)raptor_sequence_get_at(e->args, i);
        rasqal_expression_write(e2, iostr);
      }
      raptor_iostream_write_byte(')', iostr);
      break;

    case RASQAL_EXPR_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown operation %d", e->op);
  }
  raptor_iostream_write_byte(')', iostr);
}


/**
 * rasqal_expression_print:
 * @e: #rasqal_expression object.
 * @fh: The FILE* handle to print to.
 * 
 * Print a Rasqal expression in a debug format.
 * 
 * The print debug format may change in any release.
 *
 * Return value: non-0 on failure
 **/
int
rasqal_expression_print(rasqal_expression* e, FILE* fh)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(e, rasqal_expression, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(fh, FILE*, 1);

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
    case RASQAL_EXPR_LANGMATCHES:
    case RASQAL_EXPR_REGEX:
    case RASQAL_EXPR_SAMETERM:
    case RASQAL_EXPR_IF:
    case RASQAL_EXPR_STRLANG:
    case RASQAL_EXPR_STRDT:
      fputs("op ", fh);
      rasqal_expression_print_op(e, fh);
      fputc('(', fh);
      rasqal_expression_print(e->arg1, fh);
      fputs(", ", fh);
      rasqal_expression_print(e->arg2, fh);
      /* There are two 3-op expressions - both handled here */
      if((e->op == RASQAL_EXPR_REGEX || e->op == RASQAL_EXPR_IF) && e->arg3) {
        fputs(", ", fh);
        rasqal_expression_print(e->arg3, fh);
      }
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
    case RASQAL_EXPR_GROUP_COND_ASC:
    case RASQAL_EXPR_GROUP_COND_DESC:
    case RASQAL_EXPR_COUNT:
    case RASQAL_EXPR_SUM:
    case RASQAL_EXPR_AVG:
    case RASQAL_EXPR_MIN:
    case RASQAL_EXPR_MAX:
    case RASQAL_EXPR_URI:
    case RASQAL_EXPR_IRI:
    case RASQAL_EXPR_BNODE:
    case RASQAL_EXPR_SAMPLE:
      fputs("op ", fh);
      rasqal_expression_print_op(e, fh);
      fputc('(', fh);
      /* arg1 is optional for RASQAL_EXPR_BNODE */
      if(e->arg1)
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

    case RASQAL_EXPR_VARSTAR:
      fputs("varstar", fh);
      break;
      
    case RASQAL_EXPR_COALESCE:
      fputs("coalesce(", fh);
      raptor_sequence_print(e->args, fh);
      fputc(')', fh);
      break;

    case RASQAL_EXPR_GROUP_CONCAT:
      fputs("group_concat(args=", fh);
      raptor_sequence_print(e->args, fh);
      fputc(')', fh);
      break;

    case RASQAL_EXPR_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown operation %d", e->op);
  }
  fputc(')', fh);

  return 0;
}


/* for use with rasqal_expression_visit and user_data=rasqal_query */
int
rasqal_expression_has_qname(void *user_data, rasqal_expression *e)
{
  if(e->op == RASQAL_EXPR_LITERAL)
    return rasqal_literal_has_qname(e->literal);

  return 0;
}


/* for use with rasqal_expression_visit and user_data=rasqal_query */
int
rasqal_expression_expand_qname(void *user_data, rasqal_expression *e)
{
  if(e->op == RASQAL_EXPR_LITERAL)
    return rasqal_literal_expand_qname(user_data, e->literal);

  return 0;
}


int
rasqal_expression_is_constant(rasqal_expression* e) 
{
  int i;
  int result = 0;
  
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
    case RASQAL_EXPR_LANGMATCHES:
    case RASQAL_EXPR_SAMETERM:
    case RASQAL_EXPR_STRLANG:
    case RASQAL_EXPR_STRDT:
      result = rasqal_expression_is_constant(e->arg1) &&
               rasqal_expression_is_constant(e->arg2);
      break;
    case RASQAL_EXPR_REGEX:
    case RASQAL_EXPR_IF:
      result = rasqal_expression_is_constant(e->arg1) &&
               rasqal_expression_is_constant(e->arg2) &&
               (e->arg3 && rasqal_expression_is_constant(e->arg3));
      break;
    case RASQAL_EXPR_STR_MATCH:
    case RASQAL_EXPR_STR_NMATCH:
      result = rasqal_expression_is_constant(e->arg1) &&
               rasqal_literal_is_constant(e->literal);
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
    case RASQAL_EXPR_GROUP_COND_ASC:
    case RASQAL_EXPR_GROUP_COND_DESC:
    case RASQAL_EXPR_COUNT:
    case RASQAL_EXPR_SUM:
    case RASQAL_EXPR_AVG:
    case RASQAL_EXPR_MIN:
    case RASQAL_EXPR_MAX:
    case RASQAL_EXPR_URI:
    case RASQAL_EXPR_IRI:
    case RASQAL_EXPR_BNODE:
    case RASQAL_EXPR_SAMPLE:
      /* arg1 is optional for RASQAL_EXPR_BNODE and result is always constant */
      result = (e->arg1) ? rasqal_expression_is_constant(e->arg1) : 1;
      break;

    case RASQAL_EXPR_LITERAL:
      result=rasqal_literal_is_constant(e->literal);
      break;

    case RASQAL_EXPR_FUNCTION:
    case RASQAL_EXPR_COALESCE:
    case RASQAL_EXPR_GROUP_CONCAT:
      result = 1;
      for(i = 0; i < raptor_sequence_size(e->args); i++) {
        rasqal_expression* e2;
        e2 = (rasqal_expression*)raptor_sequence_get_at(e->args, i);
        if(!rasqal_expression_is_constant(e2)) {
          result = 0;
          break;
        }
      }
      break;

    case RASQAL_EXPR_CAST:
      result=rasqal_expression_is_constant(e->arg1);
      break;

    case RASQAL_EXPR_VARSTAR:
      result=0;
      break;
      
    case RASQAL_EXPR_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown operation %d", e->op);
  }
  
  return result;
}


void
rasqal_expression_convert_to_literal(rasqal_expression* e, rasqal_literal* l)
{
  int usage=e->usage;

  /* update expression 'e' in place */
  rasqal_expression_clear(e);

  memset(e, 0, sizeof(rasqal_expression));
  e->usage=usage;
  e->op=RASQAL_EXPR_LITERAL;
  e->literal=l;
}

  


/* for use with rasqal_expression_visit and user_data=rasqal_query */
static int
rasqal_expression_has_variable(void *user_data, rasqal_expression *e)
{
  rasqal_variable* v;
  const unsigned char* name=((rasqal_variable*)user_data)->name;

  if(e->op != RASQAL_EXPR_LITERAL)
    return 0;
  
  v=rasqal_literal_as_variable(e->literal);
  if(!v)
    return 0;
  
  if(!strcmp((const char*)v->name, (const char*)name))
    return 1;

  return 0;
}


int
rasqal_expression_mentions_variable(rasqal_expression* e, rasqal_variable* v)
{
  return rasqal_expression_visit(e, rasqal_expression_has_variable, v);
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
  rasqal_world *world;

#ifdef RAPTOR_V2_AVAILABLE
  raptor_world* raptor_world_ptr;
  raptor_world_ptr = raptor_new_world();
  if(!raptor_world_ptr || raptor_world_open(raptor_world_ptr))
    exit(1);
#else
  raptor_init();
#endif

  world = rasqal_new_world();
#ifdef RAPTOR_V2_AVAILABLE
  rasqal_world_set_raptor(world, raptor_world_ptr);
#endif
  /* no rasqal_world_open() */
  
  rasqal_uri_init(world);

  rasqal_xsd_init(world);
  
  lit1=rasqal_new_integer_literal(world, RASQAL_LITERAL_INTEGER, 1);
  expr1=rasqal_new_literal_expression(world, lit1);
  lit2=rasqal_new_integer_literal(world, RASQAL_LITERAL_INTEGER, 1);
  expr2=rasqal_new_literal_expression(world, lit2);
  expr=rasqal_new_2op_expression(world, RASQAL_EXPR_PLUS, expr1, expr2);

  fprintf(stderr, "%s: expression: ", program);
  rasqal_expression_print(expr, stderr);
  fputc('\n', stderr);

  result = rasqal_expression_evaluate(world, NULL, expr, 0);

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

  rasqal_xsd_finish(world);

  rasqal_uri_finish(world);
  
  RASQAL_FREE(rasqal_world, world);

#ifdef RAPTOR_V2_AVAILABLE
  raptor_free_world(raptor_world_ptr);
#else
  raptor_finish();
#endif

  return error;
}
#endif

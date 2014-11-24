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

#include "rasqal.h"
#include "rasqal_internal.h"


#define DEBUG_FH stderr


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

  e = RASQAL_CALLOC(rasqal_expression*, 1, sizeof(*e));
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
 * @RASQAL_EXPR_ORDER_COND_DESC @RASQAL_EXPR_COUNT @RASQAL_EXPR_SUM
 * @RASQAL_EXPR_AVG @RASQAL_EXPR_MIN @RASQAL_EXPR_MAX
 *
 * The operator @RASQAL_EXPR_TILDE is not used by SPARQL (formerly RDQL).
 * 
 * Return value: a new #rasqal_expression object or NULL on failure
 **/
rasqal_expression*
rasqal_new_1op_expression(rasqal_world* world, rasqal_op op,
                          rasqal_expression* arg)
{
  rasqal_expression* e = NULL;

  if(op == RASQAL_EXPR_BNODE) {
    if(!world)
      goto tidy;
  } else {
    if(!world || !arg)
      goto tidy;
  }
  
  e = RASQAL_CALLOC(rasqal_expression*, 1, sizeof(*e));
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
 * @RASQAL_EXPR_REM @RASQAL_EXPR_STR_EQ, @RASQAL_EXPR_STR_NEQ and
 * @RASQAL_EXPR_REM are unused (formerly RDQL).
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
  
  e = RASQAL_CALLOC(rasqal_expression*, 1, sizeof(*e));
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
 * The operators are:
 * #RASQAL_EXPR_REGEX #RASQAL_EXPR_IF #RASQAL_EXPR_SUBSTR
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

  e = RASQAL_CALLOC(rasqal_expression*, 1, sizeof(*e));
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
 * rasqal_new_4op_expression:
 * @world: rasqal_world object
 * @op: Expression operator
 * @arg1: Operand 1 
 * @arg2: Operand 2
 * @arg3: Operand 3
 * @arg4: Operand 4 (may be NULL)
 * 
 * Constructor - create a new 4-operand expression.
 * Takes ownership of the operands.
 * 
 * The operators are:
 * #RASQAL_EXPR_REPLACE
 *
 * Return value: a new #rasqal_expression object or NULL on failure
 **/
rasqal_expression*
rasqal_new_4op_expression(rasqal_world* world,
                          rasqal_op op,
                          rasqal_expression* arg1, 
                          rasqal_expression* arg2,
                          rasqal_expression* arg3,
                          rasqal_expression* arg4)
{
  rasqal_expression* e = NULL;

  if(!world || !arg1 || !arg2 || !arg3) /* arg4 may be NULL */
    goto tidy;

  e = RASQAL_CALLOC(rasqal_expression*, 1, sizeof(*e));
  if(e) {
    e->usage = 1;
    e->world = world;
    e->op = op;
    e->arg1 = arg1; arg1 = NULL;
    e->arg2 = arg2; arg2 = NULL;
    e->arg3 = arg3; arg3 = NULL;
    e->arg4 = arg4; arg4 = NULL;
  }

  tidy:
  if(arg1)
    rasqal_free_expression(arg1);
  if(arg2)
    rasqal_free_expression(arg2);
  if(arg3)
    rasqal_free_expression(arg3);
  if(arg4)
    rasqal_free_expression(arg4);

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
 * @RASQAL_EXPR_STR_MATCH and
 * @RASQAL_EXPR_STR_NMATCH (unused: formerly for RDQL)
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
  
  e = RASQAL_CALLOC(rasqal_expression*, 1, sizeof(*e));
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
  
  e = RASQAL_CALLOC(rasqal_expression*, 1, sizeof(*e));
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


static rasqal_expression*
rasqal_new_function_expression_common(rasqal_world* world,
                                      rasqal_op op,
                                      raptor_uri* name,
                                      rasqal_expression* arg1,
                                      raptor_sequence* args,
                                      raptor_sequence* params,
                                      unsigned int flags)
{
  rasqal_expression* e = NULL;

  if(!world || (arg1 && args) || (name && !args)|| (!name && args))
    goto tidy;
  
  e = RASQAL_CALLOC(rasqal_expression*, 1, sizeof(*e));
  if(e) {
    e->usage = 1;
    e->world = world;
    e->op = op;
    e->name = name; name = NULL;
    e->arg1 = arg1; arg1 = NULL;
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
 * @args: sequence of #rasqal_expression function arguments
 * @params: sequence of #rasqal_expression function parameters (or NULL)
 * @flags: extension function bitflags
 * 
 * Constructor - create a new expression for a URI-named function with arguments and optional parameters.
 *
 * Takes ownership of the @name, @args and @params arguments.
 * 
 * Return value: a new #rasqal_expression object or NULL on failure
 **/
rasqal_expression*
rasqal_new_function_expression(rasqal_world* world,
                               raptor_uri* name,
                               raptor_sequence* args,
                               raptor_sequence* params,
                               unsigned int flags)
{
  return rasqal_new_function_expression_common(world, RASQAL_EXPR_FUNCTION,
                                               name,
                                               NULL /* expr */, args,
                                               params,
                                               flags);
}


/**
 * rasqal_new_aggregate_function_expression:
 * @world: rasqal_world object
 * @op:  built-in aggregate function expression operator
 * @arg1: #rasqal_expression argument to aggregate function
 * @params: sequence of #rasqal_expression function parameters (or NULL)
 * @flags: extension function bitflags
 * 
 * Constructor - create a new 1-arg aggregate function expression for a builtin aggregate function
 *
 * Takes ownership of the @args and @params
 * 
 * Return value: a new #rasqal_expression object or NULL on failure
 **/
rasqal_expression*
rasqal_new_aggregate_function_expression(rasqal_world* world,
                                         rasqal_op op,
                                         rasqal_expression* arg1,
                                         raptor_sequence* params,
                                         unsigned int flags)
{
  return rasqal_new_function_expression_common(world, op,
                                               NULL /* name */,
                                               arg1, NULL /* args */,
                                               params,
                                               flags | RASQAL_EXPR_FLAG_AGGREGATE);
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
  
  e = RASQAL_CALLOC(rasqal_expression*, 1, sizeof(*e));
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
 * rasqal_new_expr_seq_expression:
 * @world: rasqal_world object
 * @op: expression operation
 * @args: sequence of #rasqal_expression arguments
 * 
 * Constructor - create a new expression with a sequence of expression arguments.
 *
 * Takes ownership of the @args
 * 
 * Return value: a new #rasqal_expression object or NULL on failure
 **/
rasqal_expression*
rasqal_new_expr_seq_expression(rasqal_world* world,
                               rasqal_op op,
                               raptor_sequence* args)
{
  rasqal_expression* e = NULL;

  if(!world || !args)
    goto tidy;
  
  e = RASQAL_CALLOC(rasqal_expression*, 1, sizeof(*e));
  if(e) {
    e->usage = 1;
    e->world = world;
    e->op = op;
    e->args = args; args = NULL;
  }
  
  tidy:
  if(args)
    raptor_free_sequence(args);

  return e;
}


/**
 * rasqal_new_set_expression:
 * @world: rasqal_world object
 * @op: list operation
 * @arg1: expression to look for in list
 * @args: sequence of #rasqal_expression list arguments
 * 
 * Constructor - create a new set IN/NOT IN operation with expression arguments.
 *
 * Takes ownership of the @arg1 and @args
 * 
 * Return value: a new #rasqal_expression object or NULL on failure
 **/
rasqal_expression*
rasqal_new_set_expression(rasqal_world* world, rasqal_op op,
                          rasqal_expression* arg1,
                          raptor_sequence* args)
{
  rasqal_expression* e = NULL;

  if(!world || !arg1 || !args)
    goto tidy;
  
  e = RASQAL_CALLOC(rasqal_expression*, 1, sizeof(*e));
  if(e) {
    e->usage = 1;
    e->world = world;
    e->op = op;
    e->arg1 = arg1; arg1 = NULL;
    e->args = args; args = NULL;
  }
  
  tidy:
  if(arg1)
    rasqal_free_expression(arg1);
  if(args)
    raptor_free_sequence(args);

  return e;
}


/**
 * rasqal_new_group_concat_expression:
 * @world: rasqal_world object
 * @flags: bitset of flags.  Only #RASQAL_EXPR_FLAG_DISTINCT is defined
 * @args: sequence of #rasqal_expression list arguments
 * @separator: SEPARATOR string literal or NULL
 * 
 * Constructor - create a new SPARQL group concat expression
 *
 * Takes an optional distinct flag, a list of expressions and an optional separator string.
 *
 * Takes ownership of the @args and @separator
 * 
 * Return value: a new #rasqal_expression object or NULL on failure
 **/
rasqal_expression*
rasqal_new_group_concat_expression(rasqal_world* world, 
                                   unsigned int flags,
                                   raptor_sequence* args,
                                   rasqal_literal* separator)
{
  rasqal_expression* e = NULL;

  if(!world || !args)
    goto tidy;
  
  e = RASQAL_CALLOC(rasqal_expression*, 1, sizeof(*e));
  if(e) {
    e->usage = 1;
    e->world = world;
    /* Discard any flags except RASQAL_EXPR_FLAG_DISTINCT */
    e->flags = (flags & RASQAL_EXPR_FLAG_DISTINCT) | RASQAL_EXPR_FLAG_AGGREGATE;
    e->op = RASQAL_EXPR_GROUP_CONCAT;
    e->args = args; args = NULL;
    e->literal = separator; separator = NULL;
  }
  
  tidy:
  if(args)
    raptor_free_sequence(args);
  if(separator)
    rasqal_free_literal(separator);

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
    case RASQAL_EXPR_CURRENT_DATETIME:
    case RASQAL_EXPR_NOW:
    case RASQAL_EXPR_RAND:
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
    case RASQAL_EXPR_LANGMATCHES:
    case RASQAL_EXPR_SAMETERM:
    case RASQAL_EXPR_STRLANG:
    case RASQAL_EXPR_STRDT:
    case RASQAL_EXPR_STRBEFORE:
    case RASQAL_EXPR_STRAFTER:
      rasqal_free_expression(e->arg1);
      rasqal_free_expression(e->arg2);
      break;

    case RASQAL_EXPR_REGEX:
    case RASQAL_EXPR_IF:
    case RASQAL_EXPR_STRSTARTS:
    case RASQAL_EXPR_STRENDS:
    case RASQAL_EXPR_CONTAINS:
    case RASQAL_EXPR_SUBSTR:
    case RASQAL_EXPR_REPLACE:
      rasqal_free_expression(e->arg1);
      rasqal_free_expression(e->arg2);
      if(e->arg3)
        rasqal_free_expression(e->arg3);
      if(e->arg4)
        rasqal_free_expression(e->arg4);
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
    case RASQAL_EXPR_ISNUMERIC:
    case RASQAL_EXPR_YEAR:
    case RASQAL_EXPR_MONTH:
    case RASQAL_EXPR_DAY:
    case RASQAL_EXPR_HOURS:
    case RASQAL_EXPR_MINUTES:
    case RASQAL_EXPR_SECONDS:
    case RASQAL_EXPR_TIMEZONE:
    case RASQAL_EXPR_FROM_UNIXTIME:
    case RASQAL_EXPR_TO_UNIXTIME:
    case RASQAL_EXPR_STRLEN:
    case RASQAL_EXPR_UCASE:
    case RASQAL_EXPR_LCASE:
    case RASQAL_EXPR_ENCODE_FOR_URI:
    case RASQAL_EXPR_TZ:
    case RASQAL_EXPR_ABS:
    case RASQAL_EXPR_ROUND:
    case RASQAL_EXPR_CEIL:
    case RASQAL_EXPR_FLOOR:
    case RASQAL_EXPR_MD5:
    case RASQAL_EXPR_SHA1:
    case RASQAL_EXPR_SHA224:
    case RASQAL_EXPR_SHA256:
    case RASQAL_EXPR_SHA384:
    case RASQAL_EXPR_SHA512:
    case RASQAL_EXPR_UUID:
    case RASQAL_EXPR_STRUUID:
      /* arg1 is optional for RASQAL_EXPR_BNODE */
      if(e->arg1)
        rasqal_free_expression(e->arg1);
      break;

    case RASQAL_EXPR_STR_MATCH:
    case RASQAL_EXPR_STR_NMATCH:
      rasqal_free_expression(e->arg1);
      rasqal_free_literal(e->literal);
      break;

    case RASQAL_EXPR_LITERAL:
      rasqal_free_literal(e->literal);
      break;

    case RASQAL_EXPR_FUNCTION:
    case RASQAL_EXPR_GROUP_CONCAT:
      /* FUNCTION name */
      if(e->name)
        raptor_free_uri(e->name);
      raptor_free_sequence(e->args);
      if(e->literal) /* GROUP_CONCAT() SEPARATOR */
        rasqal_free_literal(e->literal);
      break;

    case RASQAL_EXPR_CAST:
      raptor_free_uri(e->name);
      rasqal_free_expression(e->arg1);
      break;

    case RASQAL_EXPR_VARSTAR:
      /* constants */
      break;
      
    case RASQAL_EXPR_COALESCE:
    case RASQAL_EXPR_CONCAT:
      raptor_free_sequence(e->args);
      break;

    case RASQAL_EXPR_IN:
    case RASQAL_EXPR_NOT_IN:
      rasqal_free_expression(e->arg1);
      raptor_free_sequence(e->args);
      break;

    case RASQAL_EXPR_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown operation %u", e->op);
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
  if(!e)
    return;
  
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
    case RASQAL_EXPR_CURRENT_DATETIME:
    case RASQAL_EXPR_NOW:
    case RASQAL_EXPR_RAND:
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
    case RASQAL_EXPR_LANGMATCHES:
    case RASQAL_EXPR_SAMETERM:
    case RASQAL_EXPR_STRLANG:
    case RASQAL_EXPR_STRDT:
    case RASQAL_EXPR_STRSTARTS:
    case RASQAL_EXPR_STRENDS:
    case RASQAL_EXPR_CONTAINS:
    case RASQAL_EXPR_STRBEFORE:
    case RASQAL_EXPR_STRAFTER:
      result = rasqal_expression_visit(e->arg1, fn, user_data) ||
               rasqal_expression_visit(e->arg2, fn, user_data);
      break;

    case RASQAL_EXPR_REGEX:
    case RASQAL_EXPR_IF:
    case RASQAL_EXPR_SUBSTR:
      result = rasqal_expression_visit(e->arg1, fn, user_data) ||
               rasqal_expression_visit(e->arg2, fn, user_data) ||
               (e->arg3 && rasqal_expression_visit(e->arg3, fn, user_data));
      break;

    case RASQAL_EXPR_REPLACE:
      result = rasqal_expression_visit(e->arg1, fn, user_data) ||
               rasqal_expression_visit(e->arg2, fn, user_data) ||
               rasqal_expression_visit(e->arg3, fn, user_data) ||
               (e->arg4 && rasqal_expression_visit(e->arg4, fn, user_data));
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
    case RASQAL_EXPR_ISNUMERIC:
    case RASQAL_EXPR_YEAR:
    case RASQAL_EXPR_MONTH:
    case RASQAL_EXPR_DAY:
    case RASQAL_EXPR_HOURS:
    case RASQAL_EXPR_MINUTES:
    case RASQAL_EXPR_SECONDS:
    case RASQAL_EXPR_TIMEZONE:
    case RASQAL_EXPR_FROM_UNIXTIME:
    case RASQAL_EXPR_TO_UNIXTIME:
    case RASQAL_EXPR_STRLEN:
    case RASQAL_EXPR_UCASE:
    case RASQAL_EXPR_LCASE:
    case RASQAL_EXPR_ENCODE_FOR_URI:
    case RASQAL_EXPR_TZ:
    case RASQAL_EXPR_ABS:
    case RASQAL_EXPR_ROUND:
    case RASQAL_EXPR_CEIL:
    case RASQAL_EXPR_FLOOR:
    case RASQAL_EXPR_MD5:
    case RASQAL_EXPR_SHA1:
    case RASQAL_EXPR_SHA224:
    case RASQAL_EXPR_SHA256:
    case RASQAL_EXPR_SHA384:
    case RASQAL_EXPR_SHA512:
    case RASQAL_EXPR_UUID:
    case RASQAL_EXPR_STRUUID:
      /* arg1 is optional for RASQAL_EXPR_BNODE */
      result = (e->arg1) ? rasqal_expression_visit(e->arg1, fn, user_data) : 0;
      break;

    case RASQAL_EXPR_STR_MATCH:
    case RASQAL_EXPR_STR_NMATCH:
      result = fn(user_data, e->arg1);
      break;

    case RASQAL_EXPR_LITERAL:
      break;

    case RASQAL_EXPR_FUNCTION:
    case RASQAL_EXPR_COALESCE:
    case RASQAL_EXPR_GROUP_CONCAT:
    case RASQAL_EXPR_CONCAT:
      for(i = 0; i < raptor_sequence_size(e->args); i++) {
        rasqal_expression* e2;
        e2 = (rasqal_expression*)raptor_sequence_get_at(e->args, i);
        result = rasqal_expression_visit(e2, fn, user_data);
        if(result)
          break;
      }
      break;

    case RASQAL_EXPR_VARSTAR:
      /* constants */
      break;
      
    case RASQAL_EXPR_IN:
    case RASQAL_EXPR_NOT_IN:
      result = rasqal_expression_visit(e->arg1, fn, user_data);
      if(!result) {
        for(i = 0; i < raptor_sequence_size(e->args); i++) {
          rasqal_expression* e2;
          e2 = (rasqal_expression*)raptor_sequence_get_at(e->args, i);
          result = rasqal_expression_visit(e2, fn, user_data);
          if(result)
            break;
        }
      }
      break;

    case RASQAL_EXPR_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown operation %u", e->op);
      result= -1; /* keep some compilers happy */
      break;
  }

  return result;
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
  "coalesce",
  "if",
  "uri",
  "iri",
  "strlang",
  "strdt",
  "bnode",
  "group_concat",
  "sample",
  "in",
  "not in",
  "isnumeric",
  "year",
  "month",
  "day",
  "hours",
  "minutes",
  "seconds",
  "timezone",
  "current_datetime",
  "now",
  "from_unixtime",
  "to_unixtime",
  "concat",
  "strlen",
  "substr",
  "ucase",
  "lcase",
  "strstarts",
  "strends",
  "contains",
  "encode_for_uri",
  "tz",
  "rand",
  "abs",
  "round",
  "ceil",
  "floor",
  "md5",
  "sha1",
  "sha224",
  "sha256",
  "sha384",
  "sha512",
  "strbefore",
  "strafter",
  "replace",
  "uuid",
  "struuid"
};


/**
 * rasqal_expression_op_label:
 * @op: the #rasqal_expression_op object
 * 
 * Get a label for the rasqal expression operator
 *
 * Return value: the label (shared string) or NULL if op is out of range or unknown
 **/
const char*
rasqal_expression_op_label(rasqal_op op)
{
  if(op > RASQAL_EXPR_LAST)
    op = RASQAL_EXPR_UNKNOWN;

  return rasqal_op_labels[RASQAL_GOOD_CAST(int, op)];
}


/**
 * rasqal_expression_write_op:
 * @e: the #rasqal_expression object
 * @iostr: the #raptor_iostream to write to
 * 
 * INTERNAL - Write a rasqal expression operator to an iostream in a debug format.
 *
 * The print debug format may change in any release.
 **/
void
rasqal_expression_write_op(rasqal_expression* e, raptor_iostream* iostr)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(e, rasqal_expression);

  raptor_iostream_string_write(rasqal_expression_op_label(e->op), iostr);
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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(e, rasqal_expression);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(fh, FILE*);

  fputs(rasqal_expression_op_label(e->op), fh);
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
    case RASQAL_EXPR_CURRENT_DATETIME:
    case RASQAL_EXPR_NOW:
    case RASQAL_EXPR_RAND:
      raptor_iostream_counted_string_write("op ", 3, iostr);
      rasqal_expression_write_op(e, iostr);
      raptor_iostream_counted_string_write("()", 2, iostr);
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
    case RASQAL_EXPR_LANGMATCHES:
    case RASQAL_EXPR_REGEX:
    case RASQAL_EXPR_SAMETERM:
    case RASQAL_EXPR_IF:
    case RASQAL_EXPR_STRLANG:
    case RASQAL_EXPR_STRDT:
    case RASQAL_EXPR_STRSTARTS:
    case RASQAL_EXPR_STRENDS:
    case RASQAL_EXPR_SUBSTR:
    case RASQAL_EXPR_CONTAINS:
    case RASQAL_EXPR_STRBEFORE:
    case RASQAL_EXPR_STRAFTER:
    case RASQAL_EXPR_REPLACE:
      raptor_iostream_counted_string_write("op ", 3, iostr);
      rasqal_expression_write_op(e, iostr);
      raptor_iostream_write_byte('(', iostr);
      rasqal_expression_write(e->arg1, iostr);
      raptor_iostream_counted_string_write(", ", 2, iostr);
      rasqal_expression_write(e->arg2, iostr);

      /* There are four 3+ arg expressions - all handled here */
      if((e->op == RASQAL_EXPR_REGEX || e->op == RASQAL_EXPR_IF ||
          e->op == RASQAL_EXPR_SUBSTR || e->op == RASQAL_EXPR_REPLACE)
         && e->arg3) {
        raptor_iostream_counted_string_write(", ", 2, iostr);
        rasqal_expression_write(e->arg3, iostr);
      }
      /* One 3 or 4 arg expression */
      if((e->op == RASQAL_EXPR_REPLACE) && e->arg4) {
        raptor_iostream_counted_string_write(", ", 2, iostr);
        rasqal_expression_write(e->arg4, iostr);
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
    case RASQAL_EXPR_ISNUMERIC:
    case RASQAL_EXPR_YEAR:
    case RASQAL_EXPR_MONTH:
    case RASQAL_EXPR_DAY:
    case RASQAL_EXPR_HOURS:
    case RASQAL_EXPR_MINUTES:
    case RASQAL_EXPR_SECONDS:
    case RASQAL_EXPR_TIMEZONE:
    case RASQAL_EXPR_FROM_UNIXTIME:
    case RASQAL_EXPR_TO_UNIXTIME:
    case RASQAL_EXPR_STRLEN:
    case RASQAL_EXPR_UCASE:
    case RASQAL_EXPR_LCASE:
    case RASQAL_EXPR_ENCODE_FOR_URI:
    case RASQAL_EXPR_TZ:
    case RASQAL_EXPR_ABS:
    case RASQAL_EXPR_ROUND:
    case RASQAL_EXPR_CEIL:
    case RASQAL_EXPR_FLOOR:
    case RASQAL_EXPR_MD5:
    case RASQAL_EXPR_SHA1:
    case RASQAL_EXPR_SHA224:
    case RASQAL_EXPR_SHA256:
    case RASQAL_EXPR_SHA384:
    case RASQAL_EXPR_SHA512:
    case RASQAL_EXPR_UUID:
    case RASQAL_EXPR_STRUUID:
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
    case RASQAL_EXPR_CONCAT:
      rasqal_expression_write_op(e, iostr);
      raptor_iostream_write_byte('(', iostr);
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
      raptor_iostream_counted_string_write("group_concat(", 13, iostr);
      if(e->flags & RASQAL_EXPR_FLAG_DISTINCT)
        raptor_iostream_counted_string_write("distinct,", 9, iostr);
      raptor_iostream_counted_string_write("args=", 5, iostr);
      for(i = 0; i < raptor_sequence_size(e->args); i++) {
        rasqal_expression* e2;
        if(i > 0)
          raptor_iostream_counted_string_write(", ", 2, iostr);
        e2 = (rasqal_expression*)raptor_sequence_get_at(e->args, i);
        rasqal_expression_write(e2, iostr);
      }
      if(e->literal) {
        raptor_iostream_counted_string_write(",separator=", 11, iostr);
        rasqal_literal_write(e->literal, iostr);
      }
      raptor_iostream_write_byte(')', iostr);
      break;

    case RASQAL_EXPR_IN:
    case RASQAL_EXPR_NOT_IN:
      raptor_iostream_counted_string_write("op ", 3, iostr);
      rasqal_expression_write_op(e, iostr);
      raptor_iostream_counted_string_write("(expr=", 6, iostr);
      rasqal_expression_write(e->arg1, iostr);
      raptor_iostream_counted_string_write(", args=", 7, iostr);
      for(i = 0; i < raptor_sequence_size(e->args); i++) {
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
      RASQAL_FATAL2("Unknown operation %u", e->op);
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
    case RASQAL_EXPR_STRSTARTS:
    case RASQAL_EXPR_STRENDS:
    case RASQAL_EXPR_CONTAINS:
    case RASQAL_EXPR_SUBSTR:
    case RASQAL_EXPR_STRBEFORE:
    case RASQAL_EXPR_STRAFTER:
    case RASQAL_EXPR_REPLACE:
      fputs("op ", fh);
      rasqal_expression_print_op(e, fh);
      fputc('(', fh);
      rasqal_expression_print(e->arg1, fh);
      fputs(", ", fh);
      rasqal_expression_print(e->arg2, fh);

      /* There are four 3+ arg expressions - all handled here */
      if((e->op == RASQAL_EXPR_REGEX || e->op == RASQAL_EXPR_IF ||
          e->op == RASQAL_EXPR_SUBSTR || e->op == RASQAL_EXPR_REPLACE)
         && e->arg3) {
        fputs(", ", fh);
        rasqal_expression_print(e->arg3, fh);
      }
      /* One 3 or 4 arg expression */
      if((e->op == RASQAL_EXPR_REPLACE) && e->arg4) {
        fputs(", ", fh);
        rasqal_expression_print(e->arg4, fh);
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
    case RASQAL_EXPR_ISNUMERIC:
    case RASQAL_EXPR_YEAR:
    case RASQAL_EXPR_MONTH:
    case RASQAL_EXPR_DAY:
    case RASQAL_EXPR_HOURS:
    case RASQAL_EXPR_MINUTES:
    case RASQAL_EXPR_SECONDS:
    case RASQAL_EXPR_TIMEZONE:
    case RASQAL_EXPR_CURRENT_DATETIME:
    case RASQAL_EXPR_NOW:
    case RASQAL_EXPR_FROM_UNIXTIME:
    case RASQAL_EXPR_TO_UNIXTIME:
    case RASQAL_EXPR_STRLEN:
    case RASQAL_EXPR_UCASE:
    case RASQAL_EXPR_LCASE:
    case RASQAL_EXPR_ENCODE_FOR_URI:
    case RASQAL_EXPR_TZ:
    case RASQAL_EXPR_RAND:
    case RASQAL_EXPR_ABS:
    case RASQAL_EXPR_ROUND:
    case RASQAL_EXPR_CEIL:
    case RASQAL_EXPR_FLOOR:
    case RASQAL_EXPR_MD5:
    case RASQAL_EXPR_SHA1:
    case RASQAL_EXPR_SHA224:
    case RASQAL_EXPR_SHA256:
    case RASQAL_EXPR_SHA384:
    case RASQAL_EXPR_SHA512:
    case RASQAL_EXPR_UUID:
    case RASQAL_EXPR_STRUUID:
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
    case RASQAL_EXPR_CONCAT:
      rasqal_expression_print_op(e, fh);
      fputc('(', fh);
      raptor_sequence_print(e->args, fh);
      fputc(')', fh);
      break;

    case RASQAL_EXPR_GROUP_CONCAT:
      fputs("group_concat(", fh);
      if(e->flags & RASQAL_EXPR_FLAG_DISTINCT)
        fputs("distinct,", fh);
      fputs("args=", fh);
      raptor_sequence_print(e->args, fh);
      if(e->literal) {
        fputs(",separator=", fh);
        rasqal_literal_print(e->literal, fh);
      }
      fputc(')', fh);
      break;

    case RASQAL_EXPR_IN:
    case RASQAL_EXPR_NOT_IN:
      fputs("op ", fh);
      rasqal_expression_print_op(e, fh);
      fputs("(expr=", fh);
      rasqal_expression_print(e->arg1, fh);
      fputs(", args=", fh);
      raptor_sequence_print(e->args, fh);
      fputc(')', fh);
      break;

    case RASQAL_EXPR_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown operation %u", e->op);
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
    case RASQAL_EXPR_CURRENT_DATETIME:
    case RASQAL_EXPR_NOW:
      /* Constant - set once at the first execution of the expression in
       * a query execution after rasqal_world_reset_now() removes any
       * existing value.
       */
      result = 1;
      break;
      
    case RASQAL_EXPR_RAND:
      /* Never a constant */
      result = 0;
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
    case RASQAL_EXPR_LANGMATCHES:
    case RASQAL_EXPR_SAMETERM:
    case RASQAL_EXPR_STRLANG:
    case RASQAL_EXPR_STRDT:
    case RASQAL_EXPR_STRSTARTS:
    case RASQAL_EXPR_STRENDS:
    case RASQAL_EXPR_CONTAINS:
    case RASQAL_EXPR_STRBEFORE:
    case RASQAL_EXPR_STRAFTER:
      result = rasqal_expression_is_constant(e->arg1) &&
               rasqal_expression_is_constant(e->arg2);
      break;

    case RASQAL_EXPR_REGEX:
    case RASQAL_EXPR_IF:
    case RASQAL_EXPR_SUBSTR:
      result = rasqal_expression_is_constant(e->arg1) &&
               rasqal_expression_is_constant(e->arg2) &&
               (e->arg3 && rasqal_expression_is_constant(e->arg3));
      break;

    case RASQAL_EXPR_REPLACE:
      result = rasqal_expression_is_constant(e->arg1) &&
               rasqal_expression_is_constant(e->arg2) &&
               rasqal_expression_is_constant(e->arg3) &&
               (e->arg4 && rasqal_expression_is_constant(e->arg4));
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
    case RASQAL_EXPR_ISNUMERIC:
    case RASQAL_EXPR_YEAR:
    case RASQAL_EXPR_MONTH:
    case RASQAL_EXPR_DAY:
    case RASQAL_EXPR_HOURS:
    case RASQAL_EXPR_MINUTES:
    case RASQAL_EXPR_SECONDS:
    case RASQAL_EXPR_TIMEZONE:
    case RASQAL_EXPR_FROM_UNIXTIME:
    case RASQAL_EXPR_TO_UNIXTIME:
    case RASQAL_EXPR_STRLEN:
    case RASQAL_EXPR_UCASE:
    case RASQAL_EXPR_LCASE:
    case RASQAL_EXPR_ENCODE_FOR_URI:
    case RASQAL_EXPR_TZ:
    case RASQAL_EXPR_ABS:
    case RASQAL_EXPR_ROUND:
    case RASQAL_EXPR_CEIL:
    case RASQAL_EXPR_FLOOR:
    case RASQAL_EXPR_MD5:
    case RASQAL_EXPR_SHA1:
    case RASQAL_EXPR_SHA224:
    case RASQAL_EXPR_SHA256:
    case RASQAL_EXPR_SHA384:
    case RASQAL_EXPR_SHA512:
    case RASQAL_EXPR_UUID:
    case RASQAL_EXPR_STRUUID:
      /* arg1 is optional for RASQAL_EXPR_BNODE and result is always constant */
      result = (e->arg1) ? rasqal_expression_is_constant(e->arg1) : 1;
      break;

    case RASQAL_EXPR_LITERAL:
      result=rasqal_literal_is_constant(e->literal);
      break;

    case RASQAL_EXPR_FUNCTION:
    case RASQAL_EXPR_COALESCE:
    case RASQAL_EXPR_GROUP_CONCAT:
    case RASQAL_EXPR_CONCAT:
      result = 1;
      for(i = 0; i < raptor_sequence_size(e->args); i++) {
        rasqal_expression* e2;
        e2 = (rasqal_expression*)raptor_sequence_get_at(e->args, i);
        if(!rasqal_expression_is_constant(e2)) {
          result = 0;
          break;
        }
      }
      /* e->literal is always a string constant - do not need to check */
      break;

    case RASQAL_EXPR_CAST:
      result=rasqal_expression_is_constant(e->arg1);
      break;

    case RASQAL_EXPR_VARSTAR:
      result=0;
      break;
      
    case RASQAL_EXPR_IN:
    case RASQAL_EXPR_NOT_IN:
      result = rasqal_expression_is_constant(e->arg1);
      if(!result)
        break;
      
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
      
    case RASQAL_EXPR_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown operation %u", e->op);
  }

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG2("expression %p: ", e);
  rasqal_expression_print(e, DEBUG_FH);
  fprintf(DEBUG_FH, " %s constant\n", (result ? "is" : "is not"));
#endif
  
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
  
  if(!strcmp(RASQAL_GOOD_CAST(const char*, v->name), RASQAL_GOOD_CAST(const char*, name)))
    return 1;

  return 0;
}


int
rasqal_expression_mentions_variable(rasqal_expression* e, rasqal_variable* v)
{
  return rasqal_expression_visit(e, rasqal_expression_has_variable, v);
}


/*
 * Deep copy a sequence of rasqal_expression to a new one.
 */
raptor_sequence*
rasqal_expression_copy_expression_sequence(raptor_sequence* exprs_seq) 
{
  raptor_sequence* nexprs_seq = NULL;
  int size;
  int i;
  
  if(!exprs_seq)
    return NULL;
  
  nexprs_seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_expression,
                                   (raptor_data_print_handler)rasqal_expression_print);
  if(!nexprs_seq)
    return NULL;

  size = raptor_sequence_size(exprs_seq);
  for(i = 0; i < size; i++) {
    rasqal_expression* e;
    e = (rasqal_expression*)raptor_sequence_get_at(exprs_seq, i);
    if(e) {
      e = rasqal_new_expression_from_expression(e);
      if(e)
        raptor_sequence_set_at(nexprs_seq, i, e);
    }
  }
  
  return nexprs_seq;
}


/**
 * rasqal_expression_sequence_evaluate:
 * @query: query
 * @exprs_seq: sequence of #rasqal_expression to evaluate
 * @ignore_errors: non-0 to ignore errors in evaluation
 * @error_p: OUT: pointer to error flag (or NULL)
 *
 * INTERNAL - evaluate a sequence of expressions into a sequence of literals
 *
 * Intended to implement SPARQL 1.1 Algebra ListEval defined:
 *   ListEval(ExprList, mu) returns a list E, where Ei = mu(ExprListi).
 *
 * The result is a new sequence with #rasqal_literal values evaluated
 * from the sequence of expressions @exprs_seq.  If @ignore_errors is
 * non-0, errors returned by a expressions are ignored (this
 * corresponds to SPARQL 1.1 Algebra ListEvalE )
 *
 * Return value: sequence of literals or NULL on failure
 */
raptor_sequence*
rasqal_expression_sequence_evaluate(rasqal_query* query,
                                    raptor_sequence* exprs_seq,
                                    int ignore_errors,
                                    int* error_p)
{
  int size;
  int i;
  raptor_sequence* literal_seq = NULL;
  
  if(!query || !exprs_seq) {
    if(error_p)
      *error_p = 1;
    return NULL;
  }
  
  size = raptor_sequence_size(exprs_seq);
  if(!size) {
    if(error_p)
      *error_p = 1;
    return NULL;
  }

  literal_seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_literal,
                                    (raptor_data_print_handler)rasqal_literal_print);

  for(i = 0; i < size; i++) {
    rasqal_expression* e;
    rasqal_literal *l;
    int error = 0;
    
    e = (rasqal_expression*)raptor_sequence_get_at(exprs_seq, i);
    l = rasqal_expression_evaluate2(e, query->eval_context, &error);
    if(error) {
      if(ignore_errors)
        continue;
      
      if(error_p)
        *error_p = error;

      return NULL;
    }

    /* l becomes owned by the sequence after this */
    raptor_sequence_set_at(literal_seq, i, l);
  }
  
  return literal_seq;
}


/**
 * rasqal_expression_compare:
 * @e1: #rasqal_expression first expression
 * @e2: #rasqal_expression second expression
 * @flags: comparison flags: see rasqal_literal_compare()
 * @error_p: pointer to error
 * 
 * Compare two expressions
 * 
 * The two literals are compared.  The comparison returned is as for
 * strcmp, first before second returns <0.  equal returns 0, and
 * first after second returns >0.  For URIs, the string value is used
 * for the comparsion.
 *
 * See rasqal_literal_compare() for comparison flags.
 * 
 * If @error is not NULL, *error is set to non-0 on error
 *
 * Return value: <0, 0, or >0 as described above.
 **/
int
rasqal_expression_compare(rasqal_expression* e1, rasqal_expression* e2,
                          int flags, int* error_p)
{
  int rc = 0;
  int i;
  int diff;
  
  if(error_p)
    *error_p = 0;

  /* sort NULLs earlier */
  if(!e1 || !e2) {
    if(!e1 && !e2)
      return 0;
    if(!e1)
      return -1;
    else
      return 1;
  }
  

  if(e1->op != e2->op) {
    if(e1->op == RASQAL_EXPR_UNKNOWN || e2->op == RASQAL_EXPR_UNKNOWN)
      return 1;

    return RASQAL_GOOD_CAST(int, e2->op) - RASQAL_GOOD_CAST(int, e1->op);
  }
    
  switch(e1->op) {
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
    case RASQAL_EXPR_STRSTARTS:
    case RASQAL_EXPR_STRENDS:
    case RASQAL_EXPR_CONTAINS:
    case RASQAL_EXPR_SUBSTR:
    case RASQAL_EXPR_STRBEFORE:
    case RASQAL_EXPR_STRAFTER:
      rc = rasqal_expression_compare(e1->arg1, e2->arg1, flags, error_p);
      if(rc)
        return rc;
      
      rc = rasqal_expression_compare(e1->arg2, e2->arg2, flags, error_p);
      if(rc)
        return rc;
      
      /* There are three 3-op expressions - both handled here */
      if(e1->op == RASQAL_EXPR_REGEX || e1->op == RASQAL_EXPR_IF ||
         e1->op == RASQAL_EXPR_SUBSTR)
        rc = rasqal_expression_compare(e1->arg3, e2->arg3, flags, error_p);

      break;

    case RASQAL_EXPR_REPLACE:
      /* 3 or 4 args */
      rc = rasqal_expression_compare(e1->arg1, e2->arg1, flags, error_p);
      if(rc)
        return rc;
      
      rc = rasqal_expression_compare(e1->arg2, e2->arg2, flags, error_p);
      if(rc)
        return rc;
      
      rc = rasqal_expression_compare(e1->arg3, e2->arg3, flags, error_p);
      if(rc)
        return rc;

      rc = rasqal_expression_compare(e1->arg4, e2->arg4, flags, error_p);
      break;

    case RASQAL_EXPR_STR_MATCH:
    case RASQAL_EXPR_STR_NMATCH:
      rc = rasqal_expression_compare(e1->arg1, e2->arg1, flags, error_p);
      if(rc)
        return rc;
      
      rc = rasqal_literal_compare(e1->literal, e2->literal, flags, error_p);
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
    case RASQAL_EXPR_ISNUMERIC:
    case RASQAL_EXPR_YEAR:
    case RASQAL_EXPR_MONTH:
    case RASQAL_EXPR_DAY:
    case RASQAL_EXPR_HOURS:
    case RASQAL_EXPR_MINUTES:
    case RASQAL_EXPR_SECONDS:
    case RASQAL_EXPR_TIMEZONE:
    case RASQAL_EXPR_FROM_UNIXTIME:
    case RASQAL_EXPR_TO_UNIXTIME:
    case RASQAL_EXPR_STRLEN:
    case RASQAL_EXPR_UCASE:
    case RASQAL_EXPR_LCASE:
    case RASQAL_EXPR_ENCODE_FOR_URI:
    case RASQAL_EXPR_TZ:
    case RASQAL_EXPR_ABS:
    case RASQAL_EXPR_ROUND:
    case RASQAL_EXPR_CEIL:
    case RASQAL_EXPR_FLOOR:
    case RASQAL_EXPR_MD5:
    case RASQAL_EXPR_SHA1:
    case RASQAL_EXPR_SHA224:
    case RASQAL_EXPR_SHA256:
    case RASQAL_EXPR_SHA384:
    case RASQAL_EXPR_SHA512:
    case RASQAL_EXPR_UUID:
    case RASQAL_EXPR_STRUUID:
      /* arg1 is optional for RASQAL_EXPR_BNODE */
      rc = rasqal_expression_compare(e1->arg1, e2->arg1, flags, error_p);
      break;

    case RASQAL_EXPR_LITERAL:
      rc = rasqal_literal_compare(e1->literal, e2->literal, flags, error_p);
      break;

    case RASQAL_EXPR_FUNCTION:
    case RASQAL_EXPR_COALESCE:
    case RASQAL_EXPR_CONCAT:
      diff = raptor_sequence_size(e2->args) - raptor_sequence_size(e1->args);
      if(diff)
        return diff;
      
      for(i = 0; i < raptor_sequence_size(e1->args); i++) {
        rasqal_expression* e1_f;
        rasqal_expression* e2_f;

        e1_f = (rasqal_expression*)raptor_sequence_get_at(e1->args, i);
        e2_f = (rasqal_expression*)raptor_sequence_get_at(e2->args, i);

        rc = rasqal_expression_compare(e1_f, e2_f, flags, error_p);
        if(rc)
          break;
      }
      break;

    case RASQAL_EXPR_CAST:
      rc = raptor_uri_compare(e1->name, e2->name);
      if(rc)
        break;
      
      rc = rasqal_expression_compare(e1->arg1, e2->arg1, flags, error_p);
      break;

    case RASQAL_EXPR_VARSTAR:
    case RASQAL_EXPR_CURRENT_DATETIME:
    case RASQAL_EXPR_NOW:
      /* 0-args: always equal */
      rc = 0;
      break;
      
    case RASQAL_EXPR_RAND:
      /* 0-args: always different */
      rc = 1;
      break;

    case RASQAL_EXPR_GROUP_CONCAT:
      rc = (RASQAL_GOOD_CAST(int, e2->flags) - RASQAL_GOOD_CAST(int, e1->flags));
      if(rc)
        break;

      diff = raptor_sequence_size(e2->args) - raptor_sequence_size(e1->args);
      if(diff)
        return diff;
      
      for(i = 0; i < raptor_sequence_size(e1->args); i++) {
        rasqal_expression* e1_f;
        rasqal_expression* e2_f;

        e1_f = (rasqal_expression*)raptor_sequence_get_at(e1->args, i);
        e2_f = (rasqal_expression*)raptor_sequence_get_at(e2->args, i);

        rc = rasqal_expression_compare(e1_f, e2_f, flags, error_p);
        if(rc)
          break;
      }
      if(rc)
        break;

      rc = rasqal_literal_compare(e1->literal, e2->literal, flags, error_p);
      break;

    case RASQAL_EXPR_IN:
    case RASQAL_EXPR_NOT_IN:
      rc = rasqal_expression_compare(e1->arg1, e2->arg1, flags, error_p);
      if(rc)
        return rc;
      
      diff = raptor_sequence_size(e2->args) - raptor_sequence_size(e1->args);
      if(diff)
        return diff;
      
      for(i = 0; i < raptor_sequence_size(e1->args); i++) {
        rasqal_expression* e1_f;
        rasqal_expression* e2_f;

        e1_f = (rasqal_expression*)raptor_sequence_get_at(e1->args, i);
        e2_f = (rasqal_expression*)raptor_sequence_get_at(e2->args, i);

        rc = rasqal_expression_compare(e1_f, e2_f, flags, error_p);
        if(rc)
          break;
      }
      break;

    case RASQAL_EXPR_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown operation %u", e1->op);
  }

  return rc;
}


/**
 * rasqal_expression_is_aggregate:
 * @e: expression
 *
 * INTERNAL - determine if expression is an aggregate expression (at the top; not recursively)
 *
 * Return value: non-0 if is aggreate
 */
int
rasqal_expression_is_aggregate(rasqal_expression* e)
{
  if(e->op == RASQAL_EXPR_COUNT ||
     e->op == RASQAL_EXPR_SUM ||
     e->op == RASQAL_EXPR_AVG ||
     e->op == RASQAL_EXPR_MIN ||
     e->op == RASQAL_EXPR_MAX ||
     e->op == RASQAL_EXPR_SAMPLE ||
     e->op == RASQAL_EXPR_GROUP_CONCAT)
    return 1;

  if(e->op != RASQAL_EXPR_FUNCTION)
    return 0;

  return (e->flags & RASQAL_EXPR_FLAG_AGGREGATE) != 0;
}


static int
rasqal_expression_mentions_aggregate_visitor(void *user_data,
                                             rasqal_expression *e)
{
  return rasqal_expression_is_aggregate(e);
}


/*
 * Return non-0 if the expression tree mentions an aggregate expression
 */
int
rasqal_expression_mentions_aggregate(rasqal_expression* e)
{
  return rasqal_expression_visit(e,
                                 rasqal_expression_mentions_aggregate_visitor,
                                 NULL);
}



/*
 * rasqal_expression_convert_aggregate_to_variable:
 * @e_in: Input aggregate expression
 * @v: Input variable
 * @e_out: Output expression (or NULL)
 *
 * INTERNAL - Turn aggregate expression @e_in into a
 * #RASQAL_EXPR_LITERAL type one pointing at #rasqal_variable @v. If
 * field @e_out is not NULL, it returns in that variable a new
 * aggregate expression with the old expression fields.
 *
 * Takes ownership of @v
 *
 * Return value: non-0 on failure
 */
int
rasqal_expression_convert_aggregate_to_variable(rasqal_expression* e_in,
                                                rasqal_variable* v,
                                                rasqal_expression** e_out)
{
  rasqal_world *world;
  rasqal_literal* l;
  
  if(!e_in || !v)
    goto tidy;

  world = e_in->world;
  
  if(e_out) {
    *e_out = RASQAL_MALLOC(rasqal_expression*, sizeof(**e_out));
    if(!*e_out)
      goto tidy;
  }
  
  l = rasqal_new_variable_literal(world, v);
  if(!l)
    goto tidy;
  
  if(e_out) {
    /* if e_out is not NULL, copy entire contents to new expression */
    memcpy(*e_out, e_in, sizeof(**e_out));

    /* ... and zero out old expression */
    memset(e_in, 0, sizeof(*e_in));
  } else {
    /* Otherwise just destroy the old aggregate fields */
    rasqal_expression_clear(e_in);
  }
  

  e_in->usage = 1;
  e_in->world = world;
  e_in->op = RASQAL_EXPR_LITERAL;
  e_in->literal = l;

  return 0;

  tidy:
  if(e_out) {
    RASQAL_FREE(rasqal_expression*, *e_out);
    *e_out = NULL;
  }

  return 1;
}



/**
 * rasqal_new_evaluation_context:
 * @world: rasqal world
 * @locator: locator or NULL
 * @flags: expression comparison flags
 *
 * Constructor - create a #rasqal_evaluation_context for use with
 * rasqal_expression_evaluate2()
 *
 * Return value: non-0 on failure
 */
rasqal_evaluation_context*
rasqal_new_evaluation_context(rasqal_world* world, 
                              raptor_locator* locator,
                              int flags)
{
  rasqal_evaluation_context* eval_context;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);

  eval_context = RASQAL_CALLOC(rasqal_evaluation_context*, 1, sizeof(*eval_context));
  if(!eval_context)
    return NULL;
  
  eval_context->world = world;
  eval_context->locator = locator;
  eval_context->flags = flags;

  eval_context->random = rasqal_new_random(world);
  if(!eval_context->random) {
    RASQAL_FREE(rasqal_evaluation_context*, eval_context);
    eval_context = NULL;
  }

  return eval_context;
}


/**
 * rasqal_free_evaluation_context:
 * @eval_context: #rasqal_evaluation_context object
 * 
 * Destructor - destroy a #rasqal_evaluation_context object.
 *
 **/
void
rasqal_free_evaluation_context(rasqal_evaluation_context* eval_context)
{
  if(!eval_context)
    return;

  if(eval_context->base_uri)
    raptor_free_uri(eval_context->base_uri);

  if(eval_context->random)
    rasqal_free_random(eval_context->random);

  RASQAL_FREE(rasqal_evaluation_context*, eval_context);
}


/**
 * rasqal_evaluation_context_set_base_uri:
 * @eval_context: #rasqal_evaluation_context object
 * @base_uri: base URI
 *
 * Set the URI for a #rasqal_evaluation_context
 *
 * Return value: non-0 on failure
 */
int
rasqal_evaluation_context_set_base_uri(rasqal_evaluation_context* eval_context,
                                       raptor_uri *base_uri)
{
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(eval_context, rasqal_evaluation_context, 1);

  if(eval_context->base_uri)
    raptor_free_uri(eval_context->base_uri);

  eval_context->base_uri = raptor_uri_copy(base_uri);

  return 0;
}


/**
 * rasqal_evaluation_context_set_rand_seed:
 * @eval_context: #rasqal_evaluation_context object
 * @seed: random seed
 *
 * Set the random seed for a #rasqal_evaluation_context
 *
 * Return value: non-0 on failure
 */
int
rasqal_evaluation_context_set_rand_seed(rasqal_evaluation_context* eval_context,
                                        unsigned int seed)
{
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(eval_context, rasqal_evaluation_context, 1);

  return rasqal_random_seed(eval_context->random, seed);
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
  rasqal_evaluation_context *eval_context = NULL;

  raptor_world* raptor_world_ptr;
  raptor_world_ptr = raptor_new_world();
  if(!raptor_world_ptr || raptor_world_open(raptor_world_ptr))
    exit(1);

  world = rasqal_new_world();
  rasqal_world_set_raptor(world, raptor_world_ptr);
  /* no rasqal_world_open() */
  
  rasqal_uri_init(world);

  rasqal_xsd_init(world);
  
  eval_context = rasqal_new_evaluation_context(world, NULL /* locator */, 0);

  lit1=rasqal_new_integer_literal(world, RASQAL_LITERAL_INTEGER, 1);
  expr1=rasqal_new_literal_expression(world, lit1);
  lit2=rasqal_new_integer_literal(world, RASQAL_LITERAL_INTEGER, 1);
  expr2=rasqal_new_literal_expression(world, lit2);
  expr=rasqal_new_2op_expression(world, RASQAL_EXPR_PLUS, expr1, expr2);

  fprintf(stderr, "%s: expression: ", program);
  rasqal_expression_print(expr, stderr);
  fputc('\n', stderr);

  result = rasqal_expression_evaluate2(expr, eval_context, &error);

  if(error) {
    fprintf(stderr, "%s: expression evaluation FAILED with error\n", program);
  } else {
    int bresult;
    
    fprintf(stderr, "%s: expression result: \n", program);
    rasqal_literal_print(result, stderr);
    fputc('\n', stderr);
    bresult=rasqal_literal_as_boolean(result, &error);
    if(error) {
      fprintf(stderr, "%s: boolean expression FAILED\n", program);
    } else
      fprintf(stderr, "%s: boolean expression result: %d\n", program, bresult);


  }

  rasqal_free_expression(expr);

  if(result)
    rasqal_free_literal(result);

  rasqal_xsd_finish(world);

  rasqal_uri_finish(world);
  
  rasqal_free_evaluation_context(eval_context);

  RASQAL_FREE(rasqal_world, world);

  raptor_free_world(raptor_world_ptr);

  return error;
}
#endif /* STANDALONE */

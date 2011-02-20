/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_expr_numerics.c - Rasqal expression evaluation
 *
 * Copyright (C) 2011, David Beckett http://www.dajobe.org/
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


/* 
 * rasqal_expression_evaluate_abs:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate SPARQL 1.1 RASQAL_EXPR_ABS (numeric) expression.
 *
 * Return value: A #rasqal_literal value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_abs(rasqal_expression *e,
                               rasqal_evaluation_context *eval_context,
                               int *error_p)
{
  rasqal_literal* l1;
  rasqal_literal* result = NULL;

  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if(*error_p || !l1)
    goto failed;
  
  if(!rasqal_literal_is_numeric(l1))
    goto failed;

  result = rasqal_literal_abs(l1, error_p);
  rasqal_free_literal(l1);
  l1 = NULL;
  
  if(*error_p)
    goto failed;

  return result;

  failed:
  if(error_p)
    *error_p = 1;
  
  if(l1)
    rasqal_free_literal(l1);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_round:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate SPARQL 1.1 RASQAL_EXPR_ROUND (numeric) expression.
 *
 * Return value: A #rasqal_literal value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_round(rasqal_expression *e,
                                 rasqal_evaluation_context *eval_context,
                                 int *error_p)
{
  rasqal_literal* l1;
  rasqal_literal* result = NULL;

  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if(*error_p || !l1)
    goto failed;
  
  if(!rasqal_literal_is_numeric(l1))
    goto failed;

  result = rasqal_literal_round(l1, error_p);
  rasqal_free_literal(l1);
  l1 = NULL;
  
  if(*error_p)
    goto failed;

  return result;

  failed:
  if(error_p)
    *error_p = 1;
  
  if(l1)
    rasqal_free_literal(l1);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_ceil:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate SPARQL 1.1 RASQAL_EXPR_CEIL (numeric) expression.
 *
 * Return value: A #rasqal_literal value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_ceil(rasqal_expression *e,
                                rasqal_evaluation_context *eval_context,
                                int *error_p)
{
  rasqal_literal* l1;
  rasqal_literal* result = NULL;

  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if(*error_p || !l1)
    goto failed;
  
  if(!rasqal_literal_is_numeric(l1))
    goto failed;

  result = rasqal_literal_ceil(l1, error_p);
  rasqal_free_literal(l1);
  l1 = NULL;
  
  if(*error_p)
    goto failed;

  return result;

  failed:
  if(error_p)
    *error_p = 1;
  
  if(l1)
    rasqal_free_literal(l1);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_floor:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate SPARQL 1.1 RASQAL_EXPR_FLOOR (numeric) expression.
 *
 * Return value: A #rasqal_literal value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_floor(rasqal_expression *e,
                                rasqal_evaluation_context *eval_context,
                                int *error_p)
{
  rasqal_literal* l1;
  rasqal_literal* result = NULL;

  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if(*error_p || !l1)
    goto failed;
  
  if(!rasqal_literal_is_numeric(l1))
    goto failed;

  result = rasqal_literal_floor(l1, error_p);
  rasqal_free_literal(l1);
  l1 = NULL;
  
  if(*error_p)
    goto failed;

  return result;

  failed:
  if(error_p)
    *error_p = 1;
  
  if(l1)
    rasqal_free_literal(l1);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_rand:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate SPARQL 1.1 RASQAL_EXPR_RAND (integer expr) expression.
 *
 * Return value: A #rasqal_literal xsd:double value in range [0, 1) or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_rand(rasqal_expression *e,
                                rasqal_evaluation_context *eval_context,
                                int *error_p)
{
  rasqal_world* world = eval_context->world;
  double d;
  
#ifdef HAVE_RAND_R
  d = ((double)rand_r(&eval_context->seed)) / (double)(RAND_MAX + 1.0);
#else
  d = ((double)rand())  / (double)(RAND_MAX + 1.0);
#endif

  return rasqal_new_double_literal(world, d);
}

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
 * rasqal_expression_evaluate_rand:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate SPARQL 1.1 RASQAL_EXPR_RAND (integer expr) expression.
 *
 * Return value: A #rasqal_literal integer value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_rand(rasqal_expression *e,
                                rasqal_evaluation_context *eval_context,
                                int *error_p)
{
  rasqal_world* world = eval_context->world;
  int r;
  
#ifdef HAVE_RAND_R
  r = rand_r(&eval_context->seed);
#else
  r = rand();
#endif
  return rasqal_new_integer_literal(world, RASQAL_LITERAL_INTEGER, r);
}

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_expr_datetimes.c - Rasqal date and time expression functions
 *
 * Copyright (C) 2010, David Beckett http://www.dajobe.org/
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
 * rasqal_expression_evaluate_now:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate RASQAL_EXPR_NOW, RASQAL_EXPR_DATETIME expressions.
 *
 * Return value: A #rasqal_literal value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_now(rasqal_expression *e,
                               rasqal_evaluation_context *eval_context,
                               int *error_p)
{
  rasqal_world* world = eval_context->world;
  struct timeval *tv;
  rasqal_xsd_datetime* dt;

  tv = rasqal_world_get_now_timeval(world);
  if(!tv)
    goto failed;
  
  dt = rasqal_new_xsd_datetime_from_timeval(world, tv);
  if(!dt)
    goto failed;
  
  return rasqal_new_datetime_literal_from_datetime(world, dt);
  
  failed:
  if(error_p)
    *error_p = 1;
  
  return NULL;
}


/* 
 * rasqal_expression_evaluate_to_unixtime:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate LAQRS RASQAL_EXPR_TO_UNIXTIME (datetime) expression.
 *
 * Return value: A #rasqal_literal integer value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_to_unixtime(rasqal_expression *e,
                                       rasqal_evaluation_context *eval_context,
                                       int *error_p)
{
  rasqal_world* world = eval_context->world;
  rasqal_literal* l;
  time_t unixtime = 0;
  
  l = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if((error_p && *error_p) || !l)
    goto failed;

  if(l->type != RASQAL_LITERAL_DATETIME)
    goto failed;

  unixtime = rasqal_xsd_datetime_get_as_unixtime(l->value.datetime);
  rasqal_free_literal(l); l = NULL;
  if(!unixtime)
    goto failed;

  return rasqal_new_numeric_literal_from_long(world, RASQAL_LITERAL_INTEGER,
                                              unixtime);

  failed:
  if(error_p)
    *error_p = 1;
  
  if(l)
    rasqal_free_literal(l);
  return NULL;

}


/* 
 * rasqal_expression_evaluate_from_unixtime:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate LAQRS RASQAL_EXPR_FROM_UNIXTIME (integer expr) expression.
 *
 * Return value: A #rasqal_literal datetime value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_from_unixtime(rasqal_expression *e,
                                         rasqal_evaluation_context *eval_context,
	int *error_p)
{
  rasqal_world* world = eval_context->world;
  rasqal_literal* l;
  int unixtime = 0;
  rasqal_xsd_datetime* dt;
  
  l = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if((error_p && *error_p) || !l)
    goto failed;

  unixtime = rasqal_literal_as_integer(l, error_p);
  rasqal_free_literal(l); l = NULL;
  if(error_p && *error_p)
    goto failed;

  dt = rasqal_new_xsd_datetime_from_unixtime(world, unixtime);
  if(!dt)
    goto failed;

  return rasqal_new_datetime_literal_from_datetime(world, dt);

  failed:
  if(error_p)
    *error_p = 1;
  
  if(l)
    rasqal_free_literal(l);
  return NULL;

}


/* 
 * rasqal_expression_evaluate_datetime_part:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate SPARQL 1.1 RASQAL_EXPR_YEAR, RASQAL_EXPR_MONTH,
 *  RASQAL_EXPR_DAY, RASQAL_EXPR_HOURS, RASQAL_EXPR_MINUTES,
 *  RASQAL_EXPR_SECONDS (datetime) expressions.
 *
 * Return value: A #rasqal_literal integer value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_datetime_part(rasqal_expression *e,
                                         rasqal_evaluation_context *eval_context,
	int *error_p)
{
  rasqal_world* world = eval_context->world;
  rasqal_literal* l;
  int i;

  l = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if((error_p && *error_p) || !l)
    goto failed;
  
  if(l->type != RASQAL_LITERAL_DATETIME)
    goto failed;
  
  /* SECONDS accessor has decimal results and includes microseconds */
  if(e->op == RASQAL_EXPR_SECONDS) {
    rasqal_xsd_decimal* dec;
    rasqal_literal* result = NULL;

    dec = rasqal_xsd_datetime_get_seconds_as_decimal(world,
                                                     l->value.datetime);
    rasqal_free_literal(l);
    if(dec) {
      result = rasqal_new_decimal_literal_from_decimal(world, NULL, dec);
      if(!result)
        rasqal_free_xsd_decimal(dec);
    }
    
    if(!result)
      goto failed;
    
    return result;
  }
  
  /* The remaining accessors have xsd:integer results */
  if(e->op == RASQAL_EXPR_YEAR)
    i = l->value.datetime->year;
  else if(e->op == RASQAL_EXPR_MONTH)
    i = l->value.datetime->month;
  else if(e->op == RASQAL_EXPR_DAY)
    i = l->value.datetime->day;
  else if(e->op == RASQAL_EXPR_HOURS)
    i = l->value.datetime->hour;
  else if(e->op == RASQAL_EXPR_MINUTES)
    i = l->value.datetime->minute;
  else if(e->op == RASQAL_EXPR_SECONDS)
    i = l->value.datetime->second;
  else
    i = 0;
  
  rasqal_free_literal(l);

  return rasqal_new_integer_literal(world, RASQAL_LITERAL_INTEGER, i);

  
  failed:
  if(error_p)
    *error_p = 1;
  
  return NULL;
}


/* 
 * rasqal_expression_evaluate_datetime_timezone:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate SPARQL 1.1 RASQAL_EXPR_TIMEZONE (datetime) expression.
 *
 * Return value: A #rasqal_literal xsd:dayTimeDuration value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_datetime_timezone(rasqal_expression *e,
                                             rasqal_evaluation_context *eval_context,
	int *error_p)
{
  rasqal_world* world = eval_context->world;
  rasqal_literal* l;
  const unsigned char* s = NULL;
  raptor_uri* dt_uri;
  
  l = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if((error_p && *error_p) || !l)
    goto failed;
  
  if(l->type != RASQAL_LITERAL_DATETIME)
    goto failed;
  
  s = RASQAL_GOOD_CAST(const unsigned char*, rasqal_xsd_datetime_get_timezone_as_counted_string(l->value.datetime, NULL));
  if(!s)
    goto failed;

  dt_uri = raptor_new_uri_from_uri_local_name(world->raptor_world_ptr,
                                              world->xsd_namespace_uri, 
                                              RASQAL_GOOD_CAST(unsigned char*, "dayTimeDuration"));
  if(!dt_uri)
    goto failed;
  
  rasqal_free_literal(l);

  /* after this s and dt_uri are owned by the result literal */
  return rasqal_new_string_literal(world, s, NULL, dt_uri, NULL);

  failed:
  if(error_p)
    *error_p = 1;
  
  if(s)
    RASQAL_FREE(char*, s);
  
  if(l)
    rasqal_free_literal(l);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_datetime_tz:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate SPARQL 1.1 RASQAL_EXPR_TZ (datetime) expression.
 *
 * Return value: A #rasqal_literal string value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_datetime_tz(rasqal_expression *e,
                                       rasqal_evaluation_context *eval_context,
                                       int *error_p)
{
  rasqal_world* world = eval_context->world;
  rasqal_literal* l;
  const unsigned char* s = NULL;
  
  l = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if((error_p && *error_p) || !l)
    goto failed;
  
  if(l->type != RASQAL_LITERAL_DATETIME)
    goto failed;
  
#define TIMEZONE_STRING_LEN 7

  s = RASQAL_GOOD_CAST(const unsigned char*, rasqal_xsd_datetime_get_tz_as_counted_string(l->value.datetime, NULL));
  if(!s)
    goto failed;
  
  rasqal_free_literal(l);

  /* after this s is owned by the result literal */
  return rasqal_new_string_literal(world, s, NULL, NULL, NULL);

  failed:
  if(error_p)
    *error_p = 1;
  
  if(l)
    rasqal_free_literal(l);

  return NULL;
}

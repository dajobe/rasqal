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
 * @world: #rasqal_world
 * @locator: error locator object
 * @e: The expression to evaluate.
 * @flags: Compare flags
 *
 * INTERNAL - Evaluate RASQAL_EXPR_NOW, RASQAL_EXPR_DATETIME expressions.
 *
 * Return value: A #rasqal_literal value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_now(rasqal_world *world,
                               raptor_locator *locator,
                               rasqal_expression *e,
                               int flags)
{
  struct timeval *tv;
  rasqal_xsd_datetime* dt;

  tv = rasqal_world_get_now_timeval(world);
  if(!tv)
    return NULL;
  
  dt = rasqal_new_xsd_datetime_from_timeval(world, tv);
  if(!dt)
    return NULL;
  
  return rasqal_new_datetime_literal_from_datetime(world, dt);
}


/* 
 * rasqal_expression_evaluate_to_unixtime:
 * @world: #rasqal_world
 * @locator: error locator object
 * @e: The expression to evaluate.
 * @flags: Compare flags
 *
 * INTERNAL - Evaluate LAQRS RASQAL_EXPR_TO_UNIXTIME (datetime) expression.
 *
 * Return value: A #rasqal_literal integer value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_to_unixtime(rasqal_world *world,
                                       raptor_locator *locator,
                                       rasqal_expression *e,
                                       int flags)
{
  rasqal_literal* l;
  int unixtime = 0;
  
  l = rasqal_expression_evaluate(world, locator, e->arg1, flags);
  if(!l)
    return NULL;

  if(l->type != RASQAL_LITERAL_DATETIME)
    goto failed;

  unixtime = rasqal_xsd_datetime_get_as_unixtime(l->value.datetime);
  rasqal_free_literal(l); l = NULL;
  if(!unixtime)
    goto failed;

  return rasqal_new_integer_literal(world, RASQAL_LITERAL_INTEGER, unixtime);

  failed:
  if(l)
    rasqal_free_literal(l);
  return NULL;

}


/* 
 * rasqal_expression_evaluate_from_unixtime:
 * @world: #rasqal_world
 * @locator: error locator object
 * @e: The expression to evaluate.
 * @flags: Compare flags
 *
 * INTERNAL - Evaluate LAQRS RASQAL_EXPR_FROM_UNIXTIME (integer expr) expression.
 *
 * Return value: A #rasqal_literal datetime value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_from_unixtime(rasqal_world *world,
                                         raptor_locator *locator,
                                         rasqal_expression *e,
                                         int flags)
{
  rasqal_literal* l;
  int error = 0;
  int unixtime = 0;
  rasqal_xsd_datetime* dt;
  
  l = rasqal_expression_evaluate(world, locator, e->arg1, flags);
  if(!l)
    goto failed;

  unixtime = rasqal_literal_as_integer(l, &error);
  rasqal_free_literal(l); l = NULL;
  if(error)
    goto failed;

  dt = rasqal_new_xsd_datetime_from_unixtime(world, unixtime);
  if(!dt)
    goto failed;

  return rasqal_new_datetime_literal_from_datetime(world, dt);

  failed:
  if(l)
    rasqal_free_literal(l);
  return NULL;

}


/* 
 * rasqal_expression_evaluate_datetime_part:
 * @world: #rasqal_world
 * @locator: error locator object
 * @e: The expression to evaluate.
 * @flags: Compare flags
 *
 * INTERNAL - Evaluate SPARQL 1.1 RASQAL_EXPR_YEAR, RASQAL_EXPR_MONTH,
 *  RASQAL_EXPR_DAY, RASQAL_EXPR_HOURS, RASQAL_EXPR_MINUTES,
 *  RASQAL_EXPR_SECONDS (datetime) expressions.
 *
 * Return value: A #rasqal_literal integer value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_datetime_part(rasqal_world *world,
                                         raptor_locator *locator,
                                         rasqal_expression *e,
                                         int flags)
{
  rasqal_literal* l;
  rasqal_literal* result = NULL;
  int i;

  l = rasqal_expression_evaluate(world, locator, e->arg1, flags);
  if(!l)
    goto failed;
  
  if(l->type != RASQAL_LITERAL_DATETIME)
    goto failed;
  
  /* SECONDS accessor has decimal results and includes microseconds */
  if(e->op == RASQAL_EXPR_SECONDS) {
    rasqal_xsd_decimal* dec;
    
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
  if(result) {
    rasqal_free_literal(result);
    result = NULL;
  }

  return NULL;
}


/* 
 * rasqal_expression_evaluate_datetime_timezone:
 * @world: #rasqal_world
 * @locator: error locator object
 * @e: The expression to evaluate.
 * @flags: Compare flags
 *
 * INTERNAL - Evaluate SPARQL 1.1 RASQAL_EXPR_TIMEZONE (datetime) expressions.
 *
 * Return value: A #rasqal_literal integer value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_datetime_timezone(rasqal_world *world,
                                             raptor_locator *locator,
                                             rasqal_expression *e,
                                             int flags)
{
  rasqal_literal* l;
  const unsigned char* s;

  l = rasqal_expression_evaluate(world, locator, e->arg1, flags);
  if(!l)
    goto failed;
  
  if(l->type != RASQAL_LITERAL_DATETIME)
    goto failed;
  

  s = (const unsigned char*)rasqal_xsd_datetime_get_timezone_as_counted_string(l->value.datetime,
                                                                               NULL);
  if(!s)
    return NULL;

  /* after this s is owned by the result literal */
  return rasqal_new_string_literal(world, s, NULL, NULL, NULL);

  failed:
  if(l)
    rasqal_free_literal(l);

  return NULL;
}

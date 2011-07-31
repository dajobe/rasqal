/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_solution_modifier.c - Rasqal solution modifier class
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
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdarg.h>

#include "rasqal.h"
#include "rasqal_internal.h"


/**
 * rasqal_new_solution_modifier:
 * @query: rasqal query
 * @order_conditions: sequence of order condition expressions (or NULL)
 * @group_conditions: sequence of group by condition expressions (or NULL)
 * @having_conditions: sequence of (group by ...) having condition expressions (or NULL)
 * @limit: result limit LIMIT (>=0) or <0 if not given
 * @offset: result offset OFFSET (>=0) or <0 if not given
 *
 * INTERNAL - Create a new solution modifier object.
 * 
 * Return value: a new #rasqal_solution_modifier object or NULL on failure
 **/
rasqal_solution_modifier*
rasqal_new_solution_modifier(rasqal_query* query,
                             raptor_sequence* order_conditions,
                             raptor_sequence* group_conditions,
                             raptor_sequence* having_conditions,
                             int limit,
                             int offset)
{
  rasqal_solution_modifier* sm;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  sm = RASQAL_CALLOC(rasqal_solution_modifier*, 1, sizeof(*sm));
  if(!sm)
    return NULL;

  sm->query = query;
  sm->order_conditions = order_conditions;
  sm->group_conditions = group_conditions;
  sm->having_conditions = having_conditions;
  sm->limit = limit;
  sm->offset = offset;

  return sm;
}


  
/*
 * rasqal_free_solution_modifier:
 * @sm: #rasqal_solution_modifier object
 *
 * INTERNAL - Free a solution modifier object.
 * 
 **/
void
rasqal_free_solution_modifier(rasqal_solution_modifier* sm)
{
  if(!sm)
    return;
  
  if(sm->order_conditions)
    raptor_free_sequence(sm->order_conditions);
  
  if(sm->group_conditions)
    raptor_free_sequence(sm->group_conditions);

  if(sm->having_conditions)
    raptor_free_sequence(sm->having_conditions);
  
  RASQAL_FREE(rasqal_solution_modifier, sm);
}

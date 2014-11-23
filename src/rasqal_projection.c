/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_projection.c - Rasqal (SELECT) projection class
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
 * rasqal_new_projection:
 * @query: rasqal query
 * @variables: sequence of order condition expressions (or NULL).  Takes ownership of this sequence if present.
 * @wildcard: non-0 if @variables was '*'
 * @distinct: 1 if distinct, 2 if reduced, otherwise neither
 *
 * INTERNAL - Create a new projection object.
 * 
 * Return value: a new #rasqal_projection object or NULL on failure
 **/
rasqal_projection*
rasqal_new_projection(rasqal_query* query,
                      raptor_sequence* variables,
                      int wildcard, int distinct)
{
  rasqal_projection* projection;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  projection = RASQAL_CALLOC(rasqal_projection*, 1, sizeof(*projection));
  if(!projection)
    return NULL;

  projection->query = query;
  projection->variables = variables;
  projection->wildcard = wildcard ? 1 : 0;
  projection->distinct = distinct;

  return projection;
}


  
/*
 * rasqal_free_projection:
 * @projection: #rasqal_projection object
 *
 * INTERNAL - Free a projection object.
 * 
 **/
void
rasqal_free_projection(rasqal_projection* projection)
{
  if(!projection)
    return;
  
  if(projection->variables)
    raptor_free_sequence(projection->variables);
  
  RASQAL_FREE(rasqal_projection, projection);
}


/*
 * rasqal_projection_get_variables_sequence:
 * @projection: #rasqal_projection object
 *
 * INTERNAL - Get variables inside a projection
 * 
 **/
raptor_sequence*
rasqal_projection_get_variables_sequence(rasqal_projection* projection)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(projection, rasqal_projection, NULL);
  
  return projection->variables;
}


/*
 * rasqal_projection_add_variable:
 * @projection: #rasqal_projection object
 * @var: #rasqal_variable variable
 *
 * INTERNAL - add a binding variable to the projection.
 *
 * Return value: non-0 on failure
 **/
int
rasqal_projection_add_variable(rasqal_projection* projection,
                               rasqal_variable* var)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(projection, rasqal_projection, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(var, rasqal_variable, 1);

  if(!projection->variables) {
    projection->variables = raptor_new_sequence((raptor_data_free_handler)rasqal_free_variable,
                                                (raptor_data_print_handler)rasqal_variable_print);
    if(!projection->variables)
      return 1;
  }

  var = rasqal_new_variable_from_variable(var);

  return raptor_sequence_push(projection->variables, (void*)var);
}

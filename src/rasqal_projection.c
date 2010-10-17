/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_solution_modifier.c - Rasqal (SELECT) projection class
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
 * @variables: sequence of order condition expressions (or NULL)
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

  projection = (rasqal_projection*)RASQAL_CALLOC(rasqal_projection,
                                                 1, sizeof(*projection));
  if(!projection)
    return NULL;

  projection->query = query;
  projection->variables = variables;
  projection->wildcard = wildcard;
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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(projection, rasqal_projection);
  
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

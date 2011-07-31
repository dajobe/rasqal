/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_bindings.c - Rasqal result bindings class
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
 * rasqal_new_bindings:
 * @query: rasqal query
 * @variables: sequence of variables
 * @rows: sequence of #rasqal_row (or NULL)
 *
 * INTERNAL - Create a new bindings object.
 * 
 * The @variables and @rows become owned by the bindings object.
 *
 * Return value: a new #rasqal_bindings object or NULL on failure
 **/
rasqal_bindings*
rasqal_new_bindings(rasqal_query* query,
                    raptor_sequence* variables,
                    raptor_sequence* rows)
{
  rasqal_bindings* bindings;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(variables, raptor_sequence, NULL);

  bindings = RASQAL_CALLOC(rasqal_bindings*, 1, sizeof(*bindings));
  if(!bindings)
    return NULL;

  bindings->query = query;
  bindings->variables = variables;
  bindings->rows = rows;

  return bindings;
}


  
/*
 * rasqal_free_bindings:
 * @bindings: #rasqal_bindings object
 *
 * INTERNAL - Free a bindings object.
 * 
 **/
void
rasqal_free_bindings(rasqal_bindings* bindings)
{
  if(!bindings)
    return;
  
  raptor_free_sequence(bindings->variables);
  if(bindings->rows)
    raptor_free_sequence(bindings->rows);
  
  RASQAL_FREE(rasqal_bindings, bindings);
}


/*
 * rasqal_bindings_print:
 * @gp: the #rasqal_bindings object
 * @fh: the FILE* handle to print to
 *
 * INTERNAL - Print a #rasqal_bindings in a debug format.
 * 
 * The print debug format may change in any release.
 * 
 * Return value: non-0 on failure
 **/
int
rasqal_bindings_print(rasqal_bindings* bindings, FILE* fh)
{
  int i;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(bindings, rasqal_bindings, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(fh, FILE*, 1);

  fputs("\n  variables: ", fh);
  raptor_sequence_print(bindings->variables, fh);
  fputs("\n  rows: [\n    ", fh);

  if(bindings->rows) {
    for(i = 0; i < raptor_sequence_size(bindings->rows); i++) {
      rasqal_row* row;
      row = (rasqal_row*)raptor_sequence_get_at(bindings->rows, i);
      if(i > 0)
        fputs("\n    ", fh);
      rasqal_row_print(row, fh);
    }
  }
  fputs("\n  ]\n", fh);

  return 0;
}

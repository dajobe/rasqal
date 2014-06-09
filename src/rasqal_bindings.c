/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_bindings.c - Rasqal result bindings class
 *
 * Copyright (C) 2010-2013, David Beckett http://www.dajobe.org/
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


/*
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

  bindings->usage = 1;
  bindings->query = query;
  bindings->variables = variables;
  bindings->rows = rows;

  return bindings;
}


  
/*
 * rasqal_new_bindings_from_bindings:
 * @bindings: #rasqal_bindings to copy
 *
 * INTERNAL - Copy Constructor - Create a new Rasqal bindings from an existing one
 *
 * This adds a new reference to the bindings, it does not do a deep copy
 *
 * Return value: a new #rasqal_bindings or NULL on failure.
 **/
rasqal_bindings*
rasqal_new_bindings_from_bindings(rasqal_bindings* bindings)
{
  if(!bindings)
    return NULL;
  
  bindings->usage++;

  return bindings;
}


/*
 * rasqal_new_bindings_from_var_values:
 * @query: rasqal query
 * @var: variable
 * @values: sequence of #rasqal_value (or NULL)
 *
 * INTERNAL - Create a new bindings object for one variable with multiple bindings
 *
 * The @var and @values become owned by the bindings object.
 *
 * Return value: a new #rasqal_bindings object or NULL on failure
 **/
rasqal_bindings*
rasqal_new_bindings_from_var_values(rasqal_query* query,
                                    rasqal_variable* var,
                                    raptor_sequence* values)
{
  rasqal_bindings* bindings = NULL;
  raptor_sequence* varlist = NULL;
  rasqal_row* row = NULL;
  raptor_sequence* rowlist = NULL;
  int size = 0;
  int i;


  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(var, rasqal_variable, NULL);

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
  RASQAL_DEBUG1("binding ");
  rasqal_variable_print(var, stderr);
  fputs(" and row values ", stderr);
  raptor_sequence_print(values, stderr);
  fputc('\n', stderr);
#endif

  varlist = raptor_new_sequence((raptor_data_free_handler)rasqal_free_variable,
                                (raptor_data_print_handler)rasqal_variable_print);
  if(!varlist) {
    RASQAL_DEBUG1("Cannot create varlist sequence");
    goto tidy;
  }

  if(raptor_sequence_push(varlist, var)) {
    RASQAL_DEBUG1("varlist sequence push failed");
    goto tidy;
  }
  var = NULL;

  if(values)
    size = raptor_sequence_size(values);

  row = rasqal_new_row_for_size(query->world, size);
  if(!row) {
    RASQAL_DEBUG1("cannot create row");
    goto tidy;
  }

  for(i = 0; i < size; i++) {
    rasqal_literal* value = (rasqal_literal*)raptor_sequence_get_at(values, i);
    rasqal_row_set_value_at(row, i, value);
  }

  rowlist = raptor_new_sequence((raptor_data_free_handler)rasqal_free_row,
                                (raptor_data_print_handler)rasqal_row_print);
  if(!rowlist) {
    RASQAL_DEBUG1("cannot create rowlist sequence");
    goto tidy;
  }

  if(raptor_sequence_push(rowlist, row)) {
    RASQAL_DEBUG1("rowlist sequence push failed");
    goto tidy;
  }
  row = NULL;

  bindings = rasqal_new_bindings(query, varlist, rowlist);
  varlist = NULL; rowlist = NULL;

tidy:
  if(row)
    rasqal_free_row(row);
  if(rowlist)
    raptor_free_sequence(rowlist);
  if(varlist)
    raptor_free_sequence(varlist);
  if(var)
    rasqal_free_variable(var);
  if(values)
    raptor_free_sequence(values);

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
  
  if(--bindings->usage)
    return;
  
  raptor_free_sequence(bindings->variables);
  if(bindings->rows)
    raptor_free_sequence(bindings->rows);
  
  RASQAL_FREE(rasqal_bindings, bindings);
}


/*
 * rasqal_bindings_print:
 * @bindings: the #rasqal_bindings object
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

/*
 * rasqal_bindings_get_row:
 * @bindings: the #rasqal_bindings object
 * @offset: row offset into bindings (0+)
 *
 * INTERNAL - get a row from a binding at the given offset
 *
 * Return value: new row or NULL if out of range
 */
rasqal_row*
rasqal_bindings_get_row(rasqal_bindings* bindings, int offset)
{
  rasqal_row* row = NULL;

  if(bindings->rows) {
    row = (rasqal_row*)raptor_sequence_get_at(bindings->rows, offset);
    if(row)
      row = rasqal_new_row_from_row(row);
  }

  return row;
}

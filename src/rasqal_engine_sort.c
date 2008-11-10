/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_engine_sort.c - Rasqal query engine row sorting routines
 *
 * Copyright (C) 2004-2008, David Beckett http://www.dajobe.org/
 * Copyright (C) 2004-2005, University of Bristol, UK http://www.bristol.ac.uk/
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


#define DEBUG_FH stderr


/**
 * rasqal_engine_rowsort_compare_literals_sequence:
 * @compare_flags: comparison flags for rasqal_literal_compare()
 * @values_a: first array of literals
 * @values_b: second array of literals
 * @expr_sequence: array of expressions
 * @size: size of arrays
 *
 * INTERNAL - compare two arrays of literals evaluated in an array of expressions
 *
 * Return value: <0, 0 or >1 comparison
 */
static int
rasqal_engine_rowsort_compare_literals_sequence(int compare_flags,
                                                rasqal_literal** values_a,
                                                rasqal_literal** values_b,
                                                raptor_sequence* expr_sequence,
                                                int size)
{
  int result = 0;
  int i;

  for(i = 0; i < size; i++) {
    rasqal_expression* e = NULL;
    int error = 0;
    rasqal_literal* literal_a = values_a[i];
    rasqal_literal* literal_b = values_b[i];
    
    if(expr_sequence)
      e = (rasqal_expression*)raptor_sequence_get_at(expr_sequence, i);

#ifdef RASQAL_DEBUG
    RASQAL_DEBUG1("Comparing ");
    rasqal_literal_print(literal_a, DEBUG_FH);
    fputs(" to ", DEBUG_FH);
    rasqal_literal_print(literal_b, DEBUG_FH);
    fputs("\n", DEBUG_FH);
#endif

    if(!literal_a || !literal_b) {
      if(!literal_a && !literal_b)
        result = 0;
      else {
        result = literal_a ? 1 : -1;
#ifdef RASQAL_DEBUG
        RASQAL_DEBUG2("Got one NULL literal comparison, returning %d\n", result);
#endif
        break;
      }
    }
    
    result = rasqal_literal_compare(literal_a, literal_b,
                                    compare_flags | RASQAL_COMPARE_URI,
                                    &error);

    if(error) {
#ifdef RASQAL_DEBUG
      RASQAL_DEBUG2("Got literal comparison error at expression %d, returning 0\n", i);
#endif
      result = 0;
      break;
    }
        
    if(!result)
      continue;

    if(e && e->op == RASQAL_EXPR_ORDER_COND_DESC)
      result= -result;
    /* else Order condition is RASQAL_EXPR_ORDER_COND_ASC so nothing to do */
    
#ifdef RASQAL_DEBUG
    RASQAL_DEBUG3("Returning comparison result %d at expression %d\n", result, i);
#endif
    break;
  }

  return result;
}


/**
 * rasqal_engine_rowsort_literal_sequence_equals:
 * @values_a: first array of literals
 * @values_b: second array of literals
 * @size: size of arrays
 *
 * INTERNAL - compare two arrays of literals for equality
 *
 * Return value: non-0 if equal
 */
static int
rasqal_engine_rowsort_literal_sequence_equals(rasqal_literal** values_a,
                                              rasqal_literal** values_b,
                                              int size)
{
  int result = 1; /* equal */
  int i;
  int error = 0;

  for(i = 0; i < size; i++) {
    rasqal_literal* literal_a = values_a[i];
    rasqal_literal* literal_b = values_b[i];
    
    result = rasqal_literal_equals_flags(literal_a, literal_b,
                                         RASQAL_COMPARE_RDF, &error);
#ifdef RASQAL_DEBUG
    RASQAL_DEBUG1("Comparing ");
    rasqal_literal_print(literal_a, DEBUG_FH);
    fputs(" to ", DEBUG_FH);
    rasqal_literal_print(literal_b, DEBUG_FH);
    fprintf(DEBUG_FH, " gave %s\n", (result ? "equality" : "not equal"));
#endif

    if(error)
      result = 0;
    
    /* if different, end */
    if(!result)
      break;
  }

  return result;
}


typedef struct 
{ 
  int is_distinct;
  int compare_flags;
  raptor_sequence* order_conditions_sequence;
} rowsort_compare_data;


static void
rasqal_engine_rowsort_free_compare_data(const void* user_data)
{
  rowsort_compare_data* rcd = (rowsort_compare_data*)user_data;

  RASQAL_FREE(rowsort_compare_data,  rcd);
}


/**
 * rasqal_engine_rowsort_row_compare:
 * @user_data: comparison user data pointer
 * @a: pointer to address of first #row
 * @b: pointer to address of second #row
 *
 * INTERNAL - compare two pointers to #row objects
 *
 * Suitable for use as a compare function in qsort_r() or similar.
 *
 * Return value: <0, 0 or >1 comparison
 */
static int
rasqal_engine_rowsort_row_compare(void* user_data, const void *a, const void *b)
{
  rasqal_row* row_a;
  rasqal_row* row_b;
  rowsort_compare_data* rcd;
  int result = 0;
  rcd = (rowsort_compare_data*)user_data;
  row_a = *(rasqal_row**)a;
  row_b = *(rasqal_row**)b;

  if(rcd->is_distinct) {
    result = !rasqal_engine_rowsort_literal_sequence_equals(row_a->values,
                                                            row_b->values,
                                                            row_a->size);
    
    if(!result)
      /* duplicate, so return that */
      return 0;
  }
  
  /* now order it */
  result = rasqal_engine_rowsort_compare_literals_sequence(rcd->compare_flags,
                                                           row_a->order_values,
                                                           row_b->order_values,
                                                           rcd->order_conditions_sequence,
                                                           row_a->order_size);
  
  /* still equal?  make sort stable by using the original order */
  if(!result) {
    result = row_a->offset - row_b->offset;
    RASQAL_DEBUG2("Got equality result so using offsets, returning %d\n",
                  result);
  }
  
  return result;
}


static void
rasqal_engine_rowsort_map_free_row(const void *key, const void *value)
{
  if(key)
    rasqal_free_row((rasqal_row*)key);
  if(value)
    rasqal_free_row((rasqal_row*)value);
}


static void
rasqal_engine_rowsort_map_print_row(void *object, FILE *fh)
{
  if(object)
    rasqal_row_print((rasqal_row*)object, fh);
  else
    fputs("NULL", fh);
}


/**
 * rasqal_engine_new_rowsort_map:
 * @flags: 1: do distinct
 *
 * INTERNAL - create a new map for sorting rows
 *
 */
rasqal_map*
rasqal_engine_new_rowsort_map(int is_distinct, int compare_flags,
                              raptor_sequence* order_conditions_sequence)
{
  rowsort_compare_data* rcd;

  rcd = (rowsort_compare_data*)RASQAL_MALLOC(rowsort_compare_data,
                                             sizeof(rowsort_compare_data));
  if(!rcd)
    return NULL;
  
  rcd->is_distinct = is_distinct;
  rcd->compare_flags = compare_flags;
  rcd->order_conditions_sequence = order_conditions_sequence;
  
  return rasqal_new_map(rasqal_engine_rowsort_row_compare, rcd,
                        rasqal_engine_rowsort_free_compare_data,
                        rasqal_engine_rowsort_map_free_row,
                        rasqal_engine_rowsort_map_print_row,
                        NULL,
                        0);
}


/**
 * rasqal_engine_rowsort_map_add_row:
 * @map: row map
 * @row: row to add
 *
 * INTERNAL - Add a row o a rowsort_map for sorting
 *
 * return value: non-0 if the row was a duplicate (and not added)
 */
int
rasqal_engine_rowsort_map_add_row(rasqal_map* map, rasqal_row* row)
{
  /* map. after this, row is owned by map */
  if(!rasqal_map_add_kv(map, row, NULL))
    return 0;

  /* duplicate, and not added so delete it */
#ifdef RASQAL_DEBUG
  RASQAL_DEBUG1("Got duplicate row ");
  rasqal_row_print(row, DEBUG_FH);
  fputc('\n', DEBUG_FH);
#endif
  rasqal_free_row(row);

  return 1;
}


static void
rasqal_engine_rowsort_map_add_to_sequence(void *key, void *value,
                                          void *user_data)
{
  rasqal_row* row;
  row = rasqal_new_row_from_row((rasqal_row*)key);
  raptor_sequence_push((raptor_sequence*)user_data, row);
}


raptor_sequence*
rasqal_engine_rowsort_map_to_sequence(rasqal_map* map, raptor_sequence* seq)
{

  /* do sort/distinct: walk map in order, adding rows to seq */
  rasqal_map_visit(map, rasqal_engine_rowsort_map_add_to_sequence, (void*)seq);

  return seq;
}


/**
 * rasqal_engine_rowsort_calculate_order_values:
 * @query: query object
 * @row: row
 *
 * INTERNAL - Calculate the order condition values for a row
 *
 * Return value: non-0 on failure 
 */
int
rasqal_engine_rowsort_calculate_order_values(rasqal_query* query,
                                             rasqal_row* row)
{
  int i;
  
  if(!row->order_size)
    return 1;
  
  for(i = 0; i < row->order_size; i++) {
    rasqal_expression* e;
    rasqal_literal *l;
    
    e = (rasqal_expression*)raptor_sequence_get_at(query->order_conditions_sequence, i);
    l = rasqal_expression_evaluate_v2(query->world, &query->locator,
                                      e, query->compare_flags);
    if(row->order_values[i])
      rasqal_free_literal(row->order_values[i]);
    if(l) {
      row->order_values[i] = rasqal_new_literal_from_literal(rasqal_literal_value(l));
      rasqal_free_literal(l);
    } else
      row->order_values[i] = NULL;
  }
  
  return 0;
}

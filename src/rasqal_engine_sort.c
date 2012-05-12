/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_engine_sort.c - Rasqal query engine row sorting routines
 *
 * Copyright (C) 2004-2009, David Beckett http://www.dajobe.org/
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

  RASQAL_FREE(rowsort_compare_data, rcd);
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
  row_a = (rasqal_row*)a;
  row_b = (rasqal_row*)b;

  if(rcd->is_distinct) {
    result = !rasqal_literal_array_equals(row_a->values, row_b->values,
                                          row_a->size);
    
    if(!result)
      /* duplicate, so return that */
      return 0;
  }
  
  /* now order it */
  if(rcd->order_conditions_sequence)
    result = rasqal_literal_array_compare(row_a->order_values,
                                          row_b->order_values,
                                          rcd->order_conditions_sequence,
                                          row_a->order_size,
                                          rcd->compare_flags);


  /* still equal?  make sort stable by using the original order */
  if(!result) {
    result = row_a->offset - row_b->offset;
    RASQAL_DEBUG2("Got equality result so using offsets, returning %d\n",
                  result);
  }
  
  return result;
}


static int
rasqal_engine_rowsort_map_print_row(void *object, FILE *fh)
{
  if(object)
    rasqal_row_print((rasqal_row*)object, fh);
  else
    fputs("NULL", fh);
  return 0;
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

  rcd = RASQAL_MALLOC(rowsort_compare_data*, sizeof(*rcd));
  if(!rcd)
    return NULL;
  
  rcd->is_distinct = is_distinct;
  if(is_distinct) {
    compare_flags &= ~RASQAL_COMPARE_XQUERY;
    compare_flags |= RASQAL_COMPARE_RDF;
  }
  rcd->compare_flags = compare_flags;
  rcd->order_conditions_sequence = order_conditions_sequence;
  
  return rasqal_new_map(rasqal_engine_rowsort_row_compare, rcd,
                        (raptor_data_free_handler)rasqal_engine_rowsort_free_compare_data,
                        (raptor_data_free_handler)rasqal_free_row,
                        (raptor_data_free_handler)rasqal_free_row,
                        rasqal_engine_rowsort_map_print_row,
                        NULL,
                        0);
}


/**
 * rasqal_engine_rowsort_map_add_row:
 * @map: row map
 * @row: row to add
 *
 * INTERNAL - Add a row to a rowsort_map for sorting.  The row
 * becomes owned by the map
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
 * @order_seq: order conditions sequence
 * @row: row
 *
 * INTERNAL - Calculate the order condition values for a row
 *
 * Return value: non-0 on failure 
 */
int
rasqal_engine_rowsort_calculate_order_values(rasqal_query* query,
                                             raptor_sequence* order_seq,
                                             rasqal_row* row)
{
  int i;
  
  if(!row->order_size)
    return 1;
  
  for(i = 0; i < row->order_size; i++) {
    rasqal_expression* e;
    rasqal_literal *l;
    int error = 0;
    
    e = (rasqal_expression*)raptor_sequence_get_at(order_seq, i);
    l = rasqal_expression_evaluate2(e, query->eval_context, &error);

    if(row->order_values[i])
      rasqal_free_literal(row->order_values[i]);

    if(error)
      row->order_values[i] = NULL;
    else {
      row->order_values[i] = rasqal_new_literal_from_literal(rasqal_literal_value(l));
      rasqal_free_literal(l);
    }
  }
  
  return 0;
}

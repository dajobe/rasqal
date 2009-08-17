/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_row_compatible.c - Rasqal Class for checking if two Query Result Rows are compatible
 *
 * Copyright (C) 2009, David Beckett http://www.dajobe.org/
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
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <string.h>

#include "rasqal.h"
#include "rasqal_internal.h"


rasqal_row_compatible*
rasqal_new_row_compatible(rasqal_variables_table* vt,
                          rasqal_rowsource *first_rowsource,
                          rasqal_rowsource *second_rowsource)
{
  rasqal_row_compatible* map = NULL;
  int count = rasqal_variables_table_get_total_variables_count(vt);
  int i;
  
  map = (rasqal_row_compatible*)RASQAL_CALLOC(rasqal_row_compatible, 1, sizeof(rasqal_row_compatible));
  map->variables_table = vt;
  map->first_rowsource = first_rowsource;
  map->second_rowsource = second_rowsource;
  map->variables_count = count;
  map->defined_in_map = (int*)RASQAL_CALLOC(intarray, 2*count, sizeof(int));

  for(i = 0; i < count; i++) {
    rasqal_variable *v;
    int offset1;
    int offset2;

    v = rasqal_variables_table_get(vt, i);
    offset1 = rasqal_rowsource_get_variable_offset_by_name(first_rowsource,
                                                           v->name);
    offset2 = rasqal_rowsource_get_variable_offset_by_name(second_rowsource,
                                                           v->name);
    map->defined_in_map[i<<1] = offset1;
    map->defined_in_map[1 + (i<<1)] = offset2;
    if(offset1 >= 0 && offset2 >= 0)
      map->variables_in_both_rows_count++;
  }
  
  return map;
}


void
rasqal_free_row_compatible(rasqal_row_compatible* map)
{
  RASQAL_FREE(intarray, map->defined_in_map);
  RASQAL_FREE(rasqal_row_compatible, map);
}


/**
 * rasqal_row_compatible_check:
 * @map: row compatible map object
 * @first_row: first row
 * @second_row: second row
 *
 * Test if two rows have SPARQL Algebra "Compatible Mappings"
 * 
 *   "Two solution mappings μ1 and μ2 are compatible if, for every
 *   variable v in dom(μ1) and in dom(μ2), μ1(v) = μ2(v)."
 *  -- SPARQL Query Language 20080115, 12.3 Basic Graph Patterns
 *
 * interpretation:
 *  for all variables in both rows
 *    the values for both rows must either
 *      a) be the same defined value
 *      b) both be undefined
 */
int
rasqal_row_compatible_check(rasqal_row_compatible* map,
                            rasqal_row *first_row, rasqal_row *second_row)
{
  int i;
  int count = map->variables_count;
  int compatible = 1;

  /* If no variables in common, always compatible */
  if(!map->variables_in_both_rows_count)
    return 1;
  
  for(i = 0; i < count; i++) {
    rasqal_literal *first_value = NULL;
    rasqal_literal *second_value = NULL;

    int offset1 = map->defined_in_map[i<<1];
    int offset2 = map->defined_in_map[1 + (i<<1)];

    if(offset1 >= 0)
      first_value = first_row->values[offset1];

    if(offset2 >= 0)
      second_value = second_row->values[offset2];

    /* do not test if both are NULL */
    if(!first_value && !second_value)
      continue;

    if(!first_value || !second_value) {
      RASQAL_DEBUG2("row variable %d has (one NULL, one value)\n", i);
      /* incompatible if one is NULL and the other is not */
      compatible = 0;
      break;
    }

    if(!rasqal_literal_equals(first_value, second_value)) {
      RASQAL_DEBUG2("row variable %d has different values\n", i);
      /* incompatible if not equal values */
      compatible = 0;
      break;
    }

  }

  return compatible;
}


#ifdef STANDALONE


static void
rasqal_print_row_compatible(FILE *handle,
                            rasqal_row_compatible* map)
{
  int count = map->variables_count;
  rasqal_variables_table* vt = map->variables_table;
  int i;

  fprintf(handle, "Row compatible map: total variables: %d  shared variables: %d\n",
          count, map->variables_in_both_rows_count);
  for(i = 0; i < count; i++) {
    rasqal_variable *v = rasqal_variables_table_get(vt, i);
    int offset1 = map->defined_in_map[i<<1];
    int offset2 = map->defined_in_map[1 + (i<<1)];
    
    fprintf(handle, "  Variable %2s   offsets RS 1: %3d  RS 2: %3d  %s\n",
            v->name, offset1, offset2,
            ((offset1 >=0 && offset2 >= 0) ? "SHARED" : ""));
  }
}


/* one more prototype */
int main(int argc, char *argv[]);

const char* const join_1_data_2x3_rows[] =
{
  /* 2 variable names and 3 rows */
  "a",   NULL, "b",   NULL,
  /* row 1 data */
  "foo", NULL, "bar", NULL,
  /* row 2 data */
  "baz", NULL, "fez", NULL,
  /* row 3 data (joinable on b) */
  "bob", NULL, "sue", NULL,
  /* end of data */
  NULL, NULL
};
  

const char* const join_2_data_3x5_rows[] =
{
  /* 3 variable names and 5 rows */
  "b",     NULL, "c",      NULL, "d",      NULL,
  /* row 1 data */
  "red",   NULL, "orange", NULL, "yellow", NULL,
  /* row 2 data */
  "blue",  NULL, "indigo", NULL, "violet", NULL,
  /* row 3 data */
  "black", NULL, "silver", NULL, "gold",   NULL,
  /* row 4 data */
  "green", NULL, "tope",   NULL, "bronze", NULL,
  /* row 5 data (joinable on b) */
  "sue",   NULL, "blue",   NULL, "black", NULL,
  /* end of data */
  NULL, NULL
};


int
main(int argc, char *argv[]) 
{
  const char *program = rasqal_basename(argv[0]);
  rasqal_rowsource *rowsource = NULL;
  rasqal_rowsource *left_rs = NULL;
  rasqal_rowsource *right_rs = NULL;
  rasqal_world* world = NULL;
  rasqal_query* query = NULL;
  raptor_sequence* seq = NULL;
  int failures = 0;
  int vars_count;
  rasqal_variables_table* vt;
  raptor_sequence* vars_seq = NULL;
  rasqal_row_compatible* rc_map;
  
  world = rasqal_new_world(); rasqal_world_open(world);
  
  query = rasqal_new_query(world, "sparql", NULL);
  
  vt = query->vars_table;

  /* 2 variables and 3 rows */
  vars_count = 2;
  seq = rasqal_new_row_sequence(world, vt, join_1_data_2x3_rows, vars_count,
                                &vars_seq);
  if(!seq) {
    fprintf(stderr,
            "%s: failed to create left sequence of %d vars\n", program,
            vars_count);
    failures++;
    goto tidy;
  }

  left_rs = rasqal_new_rowsequence_rowsource(world, query, vt, seq, vars_seq);
  if(!left_rs) {
    fprintf(stderr, "%s: failed to create left rowsource\n", program);
    failures++;
    goto tidy;
  }
  /* vars_seq and seq are now owned by left_rs */
  vars_seq = seq = NULL;
  
  /* 3 variables and 5 rows */
  vars_count = 3;
  seq = rasqal_new_row_sequence(world, vt, join_2_data_3x5_rows, vars_count,
                                &vars_seq);
  if(!seq) {
    fprintf(stderr,
            "%s: failed to create right sequence of %d rows\n", program,
            vars_count);
    failures++;
    goto tidy;
  }

  right_rs = rasqal_new_rowsequence_rowsource(world, query, vt, seq, vars_seq);
  if(!right_rs) {
    fprintf(stderr, "%s: failed to create right rowsource\n", program);
    failures++;
    goto tidy;
  }
  /* vars_seq and seq are now owned by right_rs */
  vars_seq = seq = NULL;

  rc_map = rasqal_new_row_compatible(vt, left_rs, right_rs);
  if(!rc_map) {
    fprintf(stderr, "%s: failed to create row compatible\n", program);
    failures++;
    goto tidy;
  }

  rasqal_print_row_compatible(stderr, rc_map);
  
  tidy:
  if(seq)
    raptor_free_sequence(seq);
  if(left_rs)
    rasqal_free_rowsource(left_rs);
  if(right_rs)
    rasqal_free_rowsource(right_rs);
  if(rowsource)
    rasqal_free_rowsource(rowsource);
  if(query)
    rasqal_free_query(query);
  if(world)
    rasqal_free_world(world);

  return failures;
}

#endif

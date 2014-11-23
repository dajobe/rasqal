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


#ifndef STANDALONE

rasqal_row_compatible*
rasqal_new_row_compatible(rasqal_variables_table* vt,
                          rasqal_rowsource *first_rowsource,
                          rasqal_rowsource *second_rowsource)
{
  rasqal_row_compatible* map = NULL;
  int count = rasqal_variables_table_get_total_variables_count(vt);
  int i;
  
  map = RASQAL_CALLOC(rasqal_row_compatible*, 1, sizeof(*map));
  if(!map)
    return NULL;
  map->variables_table = vt;
  map->first_rowsource = first_rowsource;
  map->second_rowsource = second_rowsource;
  map->variables_count = count;
  map->defined_in_map = RASQAL_CALLOC(int*, RASQAL_GOOD_CAST(size_t, 2 * count), sizeof(int));
  if(!map->defined_in_map) {
    RASQAL_FREE(rasqal_row_compatible, map);
    return NULL;
  }

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
  if(!map)
    return;
  
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
#ifdef RASQAL_DEBUG
    rasqal_variable *v = rasqal_variables_table_get(map->variables_table, i);
    const unsigned char *name = v->name;
#endif
    rasqal_literal *first_value = NULL;
    rasqal_literal *second_value = NULL;

    int offset1 = map->defined_in_map[i<<1];
    int offset2 = map->defined_in_map[1 + (i<<1)];

    if(offset1 >= 0)
      first_value = first_row->values[offset1];

    if(offset2 >= 0)
      second_value = second_row->values[offset2];

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG5("row variable #%d - %s has first row offset #%d  second row offset #%d\n", i, name, offset1, offset2);
#endif

    /* do not test if both are NULL */
    if(!first_value && !second_value)
      continue;

    if(!first_value || !second_value) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      RASQAL_DEBUG3("row variable #%d - %s has (one NULL, one value)\n", i,
                    name);
#endif
      /* compatible if one is NULL and the other is not */
      continue;
    }

    if(!rasqal_literal_equals(first_value, second_value)) {
      RASQAL_DEBUG3("row variable #%d - %s has different values\n", i, name);
      /* incompatible if not equal values */
      compatible = 0;
      break;
    } else {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      RASQAL_DEBUG3("row variable #%d - %s has same values\n", i, name);
#endif
    }
  }

  return compatible;
}


void
rasqal_print_row_compatible(FILE *handle, rasqal_row_compatible* map)
{
  int count = map->variables_count;
  rasqal_variables_table* vt = map->variables_table;
  int i;
  char left_rs[4];
  char right_rs[4];

  fprintf(handle,
          "Row compatible map: total variables: %d  shared variables: %d\n",
          count, map->variables_in_both_rows_count);
  for(i = 0; i < count; i++) {
    rasqal_variable *v = rasqal_variables_table_get(vt, i);
    int offset1 = map->defined_in_map[i<<1];
    int offset2 = map->defined_in_map[1 + (i<<1)];
    
    if(offset1 < 0)
      *left_rs='\0';
    else
      sprintf(left_rs, "%2d", offset1);
    
    if(offset2 < 0)
      *right_rs='\0';
    else
      sprintf(right_rs, "%2d", offset2);
    
    fprintf(handle, 
            "  Variable %10s   offsets left RS: %-3s  right RS: %-3s  %s\n",
            v->name, left_rs, right_rs,
            ((offset1 >=0 && offset2 >= 0) ? "SHARED" : ""));
  }
}

#endif /* not STANDALONE */



#ifdef STANDALONE

/* one more prototype */
int main(int argc, char *argv[]);

#define EXPECTED_ROWS_COUNT 4

const char* const compatible_data_abc_rows[] =
{
  /* 3 variable names and 4 rows */
  "a", NULL,       "b",  NULL,    "c",   NULL,
  /* row 1 data - match on b, c : COMPATIBLE */
  "purple", NULL,  "blue", NULL,  "red", NULL,
  /* row 2 data - match on b, not c: INCOMPATIBLE */
  "purple", NULL,  "blue", NULL,  "red", NULL,
  /* row 3 data - match on b, NULL c: COMPATIBLE */
  "purple", NULL,  "red", NULL,  NULL, NULL,
  /* row 4 data - NULL b, NULL c: COMPATIBLE */
  "purple", NULL,  NULL, NULL,    NULL, NULL,
  /* end of data */
  NULL, NULL,  NULL, NULL,    NULL, NULL,
};
  

const char* const compatible_data_abcd_rows[] =
{
  /* 3 variable names and 4 rows */
   "b", NULL,    "c", NULL,     "d", NULL,
  /* row 1 data - match on b,c: COMPATIBLE */
  "blue", NULL, "red", NULL,   "yellow", NULL,
  /* row 2 data - match on b, not on c: INCOMPATIBLE */
  "red", NULL,  "green", NULL, "yellow", NULL,
  /* row 3 data - match on b, NULL c: COMPATIBLE */
   "red", NULL,  NULL, NULL,    "yellow", NULL,
  /* row 4 data - NULL b, NULL c: COMPATIBLE */
  NULL, NULL,   NULL, NULL,    "yellow", NULL,
  /* end of data */
  NULL, NULL,   NULL, NULL,    NULL, NULL
};

const int expected_compatible_results[EXPECTED_ROWS_COUNT]=
{
  1,
  0,
  1,
  1
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
  rasqal_row_compatible* rc_map = NULL;
  int i;
  
  world = rasqal_new_world(); rasqal_world_open(world);
  
  query = rasqal_new_query(world, "sparql", NULL);
  
  vt = query->vars_table;

  /* 3 variables and 4 rows */
  vars_count = 3;
  seq = rasqal_new_row_sequence(world, vt, compatible_data_abc_rows, vars_count,
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
  
  /* 3 variables and 4 rows */
  vars_count = 3;
  seq = rasqal_new_row_sequence(world, vt, compatible_data_abcd_rows,
                                vars_count, &vars_seq);
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

#ifdef RASQAL_DEBUG
  fputs("\n", stderr);
#endif

  for(i = 0; i < EXPECTED_ROWS_COUNT; i++) {
    rasqal_row *left_row = rasqal_rowsource_read_row(left_rs);
    rasqal_row *right_row = rasqal_rowsource_read_row(right_rs);
    int expected = expected_compatible_results[i];
    int compatible;

    if(!left_row) {
      fprintf(stderr, "%s: FAILED left rowsource ended early at row #%d\n", program, i);
      failures++;
      goto tidy;
    }
    if(!right_row) {
      fprintf(stderr, "%s: FAILED right rowsource ended early at row #%d\n", program, i);
      failures++;
      goto tidy;
    }

    compatible = rasqal_row_compatible_check(rc_map, left_row, right_row);
    RASQAL_DEBUG4("%s: compatible check for row #%d returned %d\n",
                  program, i, compatible);
    if(compatible != expected) {
      fprintf(stderr, 
              "%s: FAILED compatible check for row #%d returned %d  expected %d\n",
              program, i, compatible, expected);
      failures++;
    }

#ifdef RASQAL_DEBUG
    fputs("\n", stderr);
#endif

    if(left_row)
      rasqal_free_row(left_row);
    if(right_row)
      rasqal_free_row(right_row);
  }
  
  tidy:
  if(rc_map)
    rasqal_free_row_compatible(rc_map);
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

#endif /* STANDALONE */

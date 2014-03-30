/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_results_compatible.c - Rasqal Class for checking if two Query Results are compatible
 *
 * Copyright (C) 2014, David Beckett http://www.dajobe.org/
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

/* FIXME */
rasqal_results_compatible* rasqal_new_results_compatible(rasqal_world* world, rasqal_query_results *first_qr, rasqal_query_results *second_qr);
void rasqal_free_results_compatible(rasqal_results_compatible* rrc);
rasqal_variable* rasqal_results_compatible_get_variable_by_offset(rasqal_results_compatible* rrc, int idx);
int rasqal_results_compatible_get_variable_offset_for_result(rasqal_results_compatible* rrc, int idx, int qr_index);
void rasqal_print_results_compatible(FILE *handle, rasqal_results_compatible* rrc);
int rasqal_results_compatible_equal(rasqal_results_compatible* rrc);


#ifndef STANDALONE

rasqal_results_compatible*
rasqal_new_results_compatible(rasqal_world* world,
                              rasqal_query_results *first_qr,
                              rasqal_query_results *second_qr)
{
  rasqal_results_compatible* rrc = NULL;
  rasqal_variables_table* first_vt;
  rasqal_variables_table* second_vt;
  int i;

  first_vt = rasqal_query_results_get_variables_table(first_qr);
  second_vt = rasqal_query_results_get_variables_table(second_qr);

  rrc = RASQAL_CALLOC(rasqal_results_compatible*, 1, sizeof(*rrc));
  if(!rrc)
    return NULL;

  rrc->first_count = rasqal_variables_table_get_total_variables_count(first_vt);
  rrc->second_count = rasqal_variables_table_get_total_variables_count(second_vt);
  rrc->variables_count = 0;
  rrc->defined_in_map = RASQAL_CALLOC(int*,
                                      rrc->first_count + rrc->second_count,
                                      sizeof(int));
  if(!rrc->defined_in_map) {
    RASQAL_FREE(rasqal_results_compatible, rrc);
    return NULL;
  }
  rrc->vt = rasqal_new_variables_table(world);
  if(!rrc->vt) {
    RASQAL_FREE(int*, rrc->defined_in_map);
    RASQAL_FREE(rasqal_results_compatible, rrc);
    return NULL;
  }

  first_vt = rasqal_query_results_get_variables_table(first_qr);
  for(i = 0; i < rrc->first_count; i++) {
    rasqal_variable *v;
    rasqal_variable *v2;

    v = rasqal_variables_table_get(first_vt, i);
    v2 = rasqal_variables_table_add2(rrc->vt, v->type, v->name, 0, NULL);
    rrc->defined_in_map[(v2->offset)<<1] = i;
  }

  second_vt = rasqal_query_results_get_variables_table(second_qr);
  for(i = 0; i < rrc->second_count; i++) {
    rasqal_variable *v;
    rasqal_variable *v2;

    v = rasqal_variables_table_get(second_vt, i);
    v2 = rasqal_variables_table_get_by_name(rrc->vt, v->type, v->name);
    if(!v2)
      v2 = rasqal_variables_table_add2(rrc->vt, v->type, v->name, 0, NULL);
    rrc->defined_in_map[1 + ((v2->offset)<<1)] = i;
  }

  rrc->variables_count = rasqal_variables_table_get_total_variables_count(rrc->vt);

  for(i = 0; i < rrc->variables_count; i++) {
    if(rrc->defined_in_map[(i<<1)] >= 0 && rrc->defined_in_map[1 + (i<<1)] >= 0)
      rrc->variables_in_both_results_count++;
  }

  return rrc;
}


void
rasqal_free_results_compatible(rasqal_results_compatible* rrc)
{
  if(!rrc)
    return;

  RASQAL_FREE(int*, rrc->defined_in_map);
  RASQAL_FREE(rasqal_variable**, rrc->defined_in_map);
  RASQAL_FREE(rasqal_results_compatible, rrc);
}


/**
 * rasqal_results_compatible_equal:
 * @map: results compatible map object
 *
 * Test if two results have equal sets of variables
 *
 * Return value: non-0 if the results have the same sets of variables
 */
int
rasqal_results_compatible_equal(rasqal_results_compatible* rrc)
{
  int i;
  int count = rrc->variables_count;

  /* If no variables in common, not equal */
  if(!rrc->variables_in_both_results_count)
    return 0;

  /* If variables count are different, not equal */
  if(rrc->first_count != rrc->second_count)
    return 0;

  for(i = 0; i < count; i++) {
    /* If any variable is not in both, not equal */
    if(!rrc->defined_in_map[i<<1] ||
       !rrc->defined_in_map[1 + (i<<1)])
      return 0;
  }

  return 1;
}


/**
 * rasqal_results_compatible_get_variable_by_offset:
 * @map: results comparible
 * @idx: variable index
 *
 * Get variable by index
 *
 * Return value: pointer to shared #rasqal_variable or NULL if out of range
 */
rasqal_variable*
rasqal_results_compatible_get_variable_by_offset(rasqal_results_compatible* rrc, int idx)
{
  return rasqal_variables_table_get(rrc->vt, idx);
}


/**
 * rasqal_results_compatible_get_variable_offset_for_result:
 * @map: results comparible
 * @idx: variable index
 * @qr_index: results index 0 (first) or 1 (second)
 *
 * Get variable index in a query results by variable index
 *
 * Return value: index into query result list of variables or <0 if @idx or @qr_index is out of range
 */
int
rasqal_results_compatible_get_variable_offset_for_result(rasqal_results_compatible* rrc,
                                                         int idx, int qr_index)
{
  if(qr_index < 0 || qr_index > 1)
    return -1;

  if(!rasqal_results_compatible_get_variable_by_offset(rrc, idx))
    return -1;

  return rrc->defined_in_map[qr_index + (idx<<1)];
}


void
rasqal_print_results_compatible(FILE *handle, rasqal_results_compatible* rrc)
{
  int count = rrc->variables_count;
  rasqal_variables_table* vt = rrc->variables_table;
  int i;
  char first_qr[4];
  char second_qr[4];

  fprintf(handle,
          "Results compatible map: total variables: %d  shared variables: %d\n",
          count, rrc->variables_in_both_results_count);
  for(i = 0; i < count; i++) {
    rasqal_variable *v = rasqal_variables_table_get(vt, i);
    int offset1 = rrc->defined_in_map[i<<1];
    int offset2 = rrc->defined_in_map[1 + (i<<1)];

    if(offset1 < 0)
      *first_qr = '\0';
    else
      sprintf(first_qr, "%2d", offset1);

    if(offset2 < 0)
      *second_qr = '\0';
    else
      sprintf(second_qr, "%2d", offset2);

    fprintf(handle,
            "  Variable %10s   offsets first: %-3s  second: %-3s  %s\n",
            v->name, first_qr, second_qr,
            ((offset1 >= 0 && offset2 >= 0) ? "SHARED" : ""));
  }
}

#endif /* not STANDALONE */



#ifdef STANDALONE

/* one more prototype */
int main(int argc, char *argv[]);

#define NTESTS 2

/* FIXME - results test data needed */

const int expected_equal_results[NTESTS]=
{
  1,
  0
};


int
main(int argc, char *argv[])
{
  const char *program = rasqal_basename(argv[0]);
  rasqal_world* world = NULL;
  int failures = 0;
  int i;

  world = rasqal_new_world(); rasqal_world_open(world);

  for(i = 1; i < NTESTS; i++) {
    rasqal_query_results *first_qr = NULL;
    rasqal_query_results *second_qr = NULL;
    int expected = expected_equal_results[i];
    rasqal_results_compatible* rrc;
    int equal;

    /* FIXME - initialise first_qr and second_qr */

    rrc = rasqal_new_results_compatible(world, first_qr, second_qr);
    if(!rrc) {
      fprintf(stderr, "%s: failed to create results compatible\n", program);
      failures++;
    } else {
      rasqal_print_results_compatible(stderr, rrc);

      equal = rasqal_results_compatible_equal(rrc);
      RASQAL_DEBUG4("%s: equal results %d returned %d\n", program, i, equal);
      if(equal != expected) {
        fprintf(stderr,
                "%s: FAILED equal results check %d returned %d  expected %d\n",
                program, i, equal, expected);
        failures++;
      }
    }

    if(first_qr)
      rasqal_free_query_results(first_qr);
    if(second_qr)
      rasqal_free_query_results(second_qr);
    if(rrc)
      rasqal_free_results_compatible(rrc);
  }

  if(world)
    rasqal_free_world(world);

  return failures;
}

#endif /* STANDALONE */

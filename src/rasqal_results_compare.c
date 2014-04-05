/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_results_compare.c - Rasqal Class for comparing Query Results
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


#ifndef STANDALONE

rasqal_results_compare*
rasqal_new_results_compare(rasqal_world* world,
                              rasqal_query_results *first_qr,
                              rasqal_query_results *second_qr)
{
  rasqal_results_compare* rrc = NULL;
  rasqal_variables_table* first_vt;
  rasqal_variables_table* second_vt;
  unsigned int i;
  unsigned int size;

  first_vt = rasqal_query_results_get_variables_table(first_qr);
  second_vt = rasqal_query_results_get_variables_table(second_qr);

  rrc = RASQAL_CALLOC(rasqal_results_compare*, 1, sizeof(*rrc));
  if(!rrc)
    return NULL;

  rrc->first_count = rasqal_variables_table_get_total_variables_count(first_vt);
  rrc->second_count = rasqal_variables_table_get_total_variables_count(second_vt);
  rrc->variables_count = 0;
  size = rrc->first_count + rrc->second_count;
  rrc->defined_in_map = RASQAL_CALLOC(int*, size, sizeof(int));
  if(!rrc->defined_in_map) {
    RASQAL_FREE(rasqal_results_compare, rrc);
    return NULL;
  }
  for(i = 0; i < size; i++)
    rrc->defined_in_map[i] = -1;

  rrc->vt = rasqal_new_variables_table(world);
  if(!rrc->vt) {
    RASQAL_FREE(int*, rrc->defined_in_map);
    RASQAL_FREE(rasqal_results_compare, rrc);
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
      rrc->variables_in_both_count++;
  }

  return rrc;
}


void
rasqal_free_results_compare(rasqal_results_compare* rrc)
{
  if(!rrc)
    return;

  if(rrc->defined_in_map)
    RASQAL_FREE(rasqal_variable**, rrc->defined_in_map);
  if(rrc->vt)
    rasqal_free_variables_table(rrc->vt);
  RASQAL_FREE(rasqal_results_compare, rrc);
}


/**
 * rasqal_results_compare_equal:
 * @map: results compatible map object
 *
 * Test if two results have equal sets of variables
 *
 * Return value: non-0 if the results have the same sets of variables
 */
int
rasqal_results_compare_equal(rasqal_results_compare* rrc)
{
  int i;
  int count = rrc->variables_count;

  /* If no variables in common, not equal */
  if(!rrc->variables_in_both_count)
    return 0;

  /* If variables count are different, not equal */
  if(rrc->first_count != rrc->second_count)
    return 0;

  for(i = 0; i < count; i++) {
    /* If any variable is not in both, not equal */
    if(rrc->defined_in_map[i<<1] < 0 ||
       rrc->defined_in_map[1 + (i<<1)] < 0 )
      return 0;
  }

  return 1;
}


/**
 * rasqal_results_compare_get_variable_by_offset:
 * @map: results comparible
 * @idx: variable index
 *
 * Get variable by index
 *
 * Return value: pointer to shared #rasqal_variable or NULL if out of range
 */
rasqal_variable*
rasqal_results_compare_get_variable_by_offset(rasqal_results_compare* rrc, int idx)
{
  return rasqal_variables_table_get(rrc->vt, idx);
}


/**
 * rasqal_results_compare_get_variable_offset_for_result:
 * @map: results comparible
 * @var_idx: variable index
 * @qr_index: results index 0 (first) or 1 (second)
 *
 * Get variable index in a query results by variable index
 *
 * Return value: index into query result list of variables or <0 if @var_idx or @qr_index is out of range
 */
int
rasqal_results_compare_get_variable_offset_for_result(rasqal_results_compare* rrc,
                                                      int var_idx, int qr_index)
{
  if(qr_index < 0 || qr_index > 1)
    return -1;

  if(!rasqal_results_compare_get_variable_by_offset(rrc, var_idx))
    return -1;

  return rrc->defined_in_map[qr_index + (var_idx << 1)];
}


#endif /* not STANDALONE */



#ifdef STANDALONE

/* some more prototypes */
int main(int argc, char *argv[]);

#define NTESTS 2

const struct {
  const char* first_qr_string;
  const char* second_qr_string;
  int expected_vars_count;
  int expected_rows_count;
  int expected_equality;
} expected_data[NTESTS] = {
  {
    "a\tb\tc\td\te\tf\n\"a\"\t\"b\"\t\"c\"\t\"d\"\t\"e\"\t\"f\"\n",
    "a\tb\tc\td\te\tf\n\"a\"\t\"b\"\t\"c\"\t\"d\"\t\"e\"\t\"f\"\n",
    6, 1, 1
  },
  {
    "a\tb\tc\td\te\tf\n\"a\"\t\"b\"\t\"c\"\t\"d\"\t\"e\"\t\"f\"\n",
    "d\tf\tc\ta\te\tb\n\"d\"\t\"f\"\t\"c\"\t\"a\"\t\"e\"\t\"b\"\n",
    6, 1, 1
  }
};


static void
rasqal_print_results_compare(FILE *handle, rasqal_results_compare* rrc)
{
  int count = rrc->variables_count;
  rasqal_variables_table* vt = rrc->vt;
  int i;
  char first_qr[4];
  char second_qr[4];

  fprintf(handle,
          "Results compatible map: total variables: %d  shared variables: %d\n",
          count, rrc->variables_in_both_count);
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


#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
static void
print_bindings_results_simple(rasqal_query_results *results, FILE* output)
{
  while(!rasqal_query_results_finished(results)) {
    int i;

    fputs("row: [", output);
    for(i = 0; i < rasqal_query_results_get_bindings_count(results); i++) {
      const unsigned char *name;
      rasqal_literal *value;

      name = rasqal_query_results_get_binding_name(results, i);
      value = rasqal_query_results_get_binding_value(results, i);

      if(i > 0)
        fputs(", ", output);

      fprintf(output, "%s=", name);
      rasqal_literal_print(value, output);
    }
    fputs("]\n", output);

    rasqal_query_results_next(results);
  }
}
#endif


int
main(int argc, char *argv[])
{
  const char *program = rasqal_basename(argv[0]);
  rasqal_world* world = NULL;
  raptor_world* raptor_world_ptr;
  int failures = 0;
  int i;
  rasqal_query_results_type type = RASQAL_QUERY_RESULTS_BINDINGS;

  world = rasqal_new_world(); rasqal_world_open(world);

  raptor_world_ptr = rasqal_world_get_raptor(world);

  for(i = 0; i < NTESTS; i++) {
    raptor_uri* base_uri = raptor_new_uri(raptor_world_ptr,
                                          (const unsigned char*)"http://example.org/");
    rasqal_query_results *first_qr;
    rasqal_query_results *second_qr = NULL;
    int expected_equality = expected_data[i].expected_equality;
    rasqal_results_compare* rrc;
    int equal;

    first_qr = rasqal_new_query_results_from_string(world,
                                                    type,
                                                    base_uri,
                                                    expected_data[i].first_qr_string,
                                                    0);
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("First query result from string");
    print_bindings_results_simple(first_qr, stderr);
    rasqal_query_results_rewind(first_qr);
#endif

    second_qr = rasqal_new_query_results_from_string(world,
                                                     type,
                                                     base_uri,
                                                     expected_data[i].second_qr_string,
                                                     0);
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("Second query result from string");
    print_bindings_results_simple(second_qr, stderr);
    rasqal_query_results_rewind(second_qr);
#endif

    raptor_free_uri(base_uri);

    rrc = rasqal_new_results_compare(world, first_qr, second_qr);
    if(!rrc) {
      fprintf(stderr, "%s: failed to create results compatible\n", program);
      failures++;
    } else {
      rasqal_print_results_compare(stderr, rrc);

      equal = rasqal_results_compare_equal(rrc);
      RASQAL_DEBUG4("%s: equal results test %d returned %d\n", program, i, equal);
      if(equal != expected_equality) {
        fprintf(stderr,
                "%s: FAILED equal results test %d returned %d  expected %d\n",
                program, i, equal, expected_equality);
        failures++;
      }
    }

    if(first_qr)
      rasqal_free_query_results(first_qr);
    if(second_qr)
      rasqal_free_query_results(second_qr);
    if(rrc)
      rasqal_free_results_compare(rrc);
  }

  if(world)
    rasqal_free_world(world);

  return failures;
}

#endif /* STANDALONE */

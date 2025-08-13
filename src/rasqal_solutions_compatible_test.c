/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_solutions_compatible_test.c - Test solution compatibility for SPARQL MINUS
 *
 * Copyright (C) 2025, David Beckett http://www.dajobe.org/
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

static void
print_test_result(const char* test_name, int result)
{
  printf("%s: %s\n", test_name, result ? "PASS" : "FAIL");
}

/*
 * Test SPARQL MINUS solution compatibility according to SPARQL 1.1 spec:
 *
 * "Two solution mappings μ1 and μ2 are compatible if, for every variable v
 * in dom(μ1) and in dom(μ2), μ1(v) = μ2(v)."
 *
 * Key test cases for MINUS:
 * 1. Compatible solutions (same values for shared variables)
 * 2. Incompatible solutions (different values for shared variables)
 * 3. "Vacuously compatible" solutions (no shared variables)
 * 4. Mixed bound/unbound variables in shared domain
 */

static int
test_basic_compatibility(rasqal_world* world)
{
  rasqal_query* query = NULL;
  rasqal_row_compatible* rc_map = NULL;
  rasqal_rowsource* left_rs = NULL;
  rasqal_rowsource* right_rs = NULL;
  raptor_sequence* left_seq = NULL;
  raptor_sequence* right_seq = NULL;
  raptor_sequence* left_vars_seq = NULL;
  raptor_sequence* right_vars_seq = NULL;
  rasqal_variables_table* vt = NULL;
  rasqal_row* row1 = NULL;
  rasqal_row* row2 = NULL;
  int result = 0;
  int vars_count = 2;

  /* Test data: two compatible solutions with same values for shared variables */
  const char* const left_data[] = {
    /* variable names */
    "a", NULL, "b", NULL,
    /* row 1 data */
    "\"value1\"", NULL, "\"value2\"", NULL,
    /* end */
    NULL, NULL, NULL, NULL
  };

  const char* const right_data[] = {
    /* variable names */
    "a", NULL, "b", NULL,
    /* row 1 data - same values, should be compatible */
    "\"value1\"", NULL, "\"value2\"", NULL,
    /* end */
    NULL, NULL, NULL, NULL
  };

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query)
    goto cleanup;

  vt = query->vars_table;

  /* Create rowsources with test data */
  left_seq = rasqal_new_row_sequence(world, vt, left_data, vars_count, &left_vars_seq);
  if(!left_seq)
    goto cleanup;

  left_rs = rasqal_new_rowsequence_rowsource(world, query, vt, left_seq, left_vars_seq);
  if(!left_rs)
    goto cleanup;
  /* left_seq and left_vars_seq are now owned by left_rs */
  left_seq = left_vars_seq = NULL;

  right_seq = rasqal_new_row_sequence(world, vt, right_data, vars_count, &right_vars_seq);
  if(!right_seq)
    goto cleanup;

  right_rs = rasqal_new_rowsequence_rowsource(world, query, vt, right_seq, right_vars_seq);
  if(!right_rs)
    goto cleanup;
  /* right_seq and right_vars_seq are now owned by right_rs */
  right_seq = right_vars_seq = NULL;

  /* Create compatibility map */
  rc_map = rasqal_new_row_compatible(vt, left_rs, right_rs);
  if(!rc_map)
    goto cleanup;

  /* Get rows from rowsources */
  row1 = rasqal_rowsource_read_row(left_rs);
  row2 = rasqal_rowsource_read_row(right_rs);
  if(!row1 || !row2)
    goto cleanup;

  /* Test compatibility - should be compatible since values are the same */
  result = rasqal_row_compatible_check(rc_map, row1, row2);

cleanup:
  if(row2)
    rasqal_free_row(row2);
  if(row1)
    rasqal_free_row(row1);
  if(rc_map)
    rasqal_free_row_compatible(rc_map);
  if(right_rs)
    rasqal_free_rowsource(right_rs);
  if(left_rs)
    rasqal_free_rowsource(left_rs);
  if(right_seq)
    raptor_free_sequence(right_seq);
  if(left_seq)
    raptor_free_sequence(left_seq);
  if(right_vars_seq)
    raptor_free_sequence(right_vars_seq);
  if(left_vars_seq)
    raptor_free_sequence(left_vars_seq);
  if(query)
    rasqal_free_query(query);

  return result;
}

static int
test_incompatible_solutions(rasqal_world* world)
{
  rasqal_query* query = NULL;
  rasqal_row_compatible* rc_map = NULL;
  rasqal_rowsource* left_rs = NULL;
  rasqal_rowsource* right_rs = NULL;
  raptor_sequence* left_seq = NULL;
  raptor_sequence* right_seq = NULL;
  raptor_sequence* left_vars_seq = NULL;
  raptor_sequence* right_vars_seq = NULL;
  rasqal_variables_table* vt = NULL;
  rasqal_row* row1 = NULL;
  rasqal_row* row2 = NULL;
  int result = 0;
  int vars_count = 1;

  /* Test data: two incompatible solutions with different values for shared variable */
  const char* const left_data[] = {
    /* variable names */
    "a", NULL,
    /* row 1 data */
    "\"value1\"", NULL,
    /* end */
    NULL, NULL
  };

  const char* const right_data[] = {
    /* variable names */
    "a", NULL,
    /* row 1 data - different value, should be incompatible */
    "\"value2\"", NULL,
    /* end */
    NULL, NULL
  };

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query)
    goto cleanup;

  vt = query->vars_table;

  /* Create rowsources with test data */
  left_seq = rasqal_new_row_sequence(world, vt, left_data, vars_count, &left_vars_seq);
  if(!left_seq)
    goto cleanup;

  left_rs = rasqal_new_rowsequence_rowsource(world, query, vt, left_seq, left_vars_seq);
  if(!left_rs)
    goto cleanup;
  left_seq = left_vars_seq = NULL;

  right_seq = rasqal_new_row_sequence(world, vt, right_data, vars_count, &right_vars_seq);
  if(!right_seq)
    goto cleanup;

  right_rs = rasqal_new_rowsequence_rowsource(world, query, vt, right_seq, right_vars_seq);
  if(!right_rs)
    goto cleanup;
  right_seq = right_vars_seq = NULL;

  /* Create compatibility map */
  rc_map = rasqal_new_row_compatible(vt, left_rs, right_rs);
  if(!rc_map)
    goto cleanup;

  /* Get rows from rowsources */
  row1 = rasqal_rowsource_read_row(left_rs);
  row2 = rasqal_rowsource_read_row(right_rs);
  if(!row1 || !row2)
    goto cleanup;

  /* Test compatibility - should be incompatible, so we expect 0 */
  result = !rasqal_row_compatible_check(rc_map, row1, row2);

cleanup:
  if(row2)
    rasqal_free_row(row2);
  if(row1)
    rasqal_free_row(row1);
  if(rc_map)
    rasqal_free_row_compatible(rc_map);
  if(right_rs)
    rasqal_free_rowsource(right_rs);
  if(left_rs)
    rasqal_free_rowsource(left_rs);
  if(right_seq)
    raptor_free_sequence(right_seq);
  if(left_seq)
    raptor_free_sequence(left_seq);
  if(right_vars_seq)
    raptor_free_sequence(right_vars_seq);
  if(left_vars_seq)
    raptor_free_sequence(left_vars_seq);
  if(query)
    rasqal_free_query(query);

  return result;
}

static int
test_vacuous_compatibility(rasqal_world* world)
{
  rasqal_query* query = NULL;
  rasqal_row_compatible* rc_map = NULL;
  rasqal_rowsource* left_rs = NULL;
  rasqal_rowsource* right_rs = NULL;
  raptor_sequence* left_seq = NULL;
  raptor_sequence* right_seq = NULL;
  raptor_sequence* left_vars_seq = NULL;
  raptor_sequence* right_vars_seq = NULL;
  rasqal_variables_table* vt = NULL;
  rasqal_row* row1 = NULL;
  rasqal_row* row2 = NULL;
  int result = 0;
  int vars_count = 1;

  /* Test data: no shared variables - should be vacuously compatible */
  const char* const left_data[] = {
    /* variable names */
    "a", NULL,
    /* row 1 data */
    "\"value1\"", NULL,
    /* end */
    NULL, NULL
  };

  const char* const right_data[] = {
    /* variable names - different variable name */
    "b", NULL,
    /* row 1 data */
    "\"value2\"", NULL,
    /* end */
    NULL, NULL
  };

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query)
    goto cleanup;

  vt = query->vars_table;

  /* Create rowsources with test data */
  left_seq = rasqal_new_row_sequence(world, vt, left_data, vars_count, &left_vars_seq);
  if(!left_seq)
    goto cleanup;

  left_rs = rasqal_new_rowsequence_rowsource(world, query, vt, left_seq, left_vars_seq);
  if(!left_rs)
    goto cleanup;
  left_seq = left_vars_seq = NULL;

  right_seq = rasqal_new_row_sequence(world, vt, right_data, vars_count, &right_vars_seq);
  if(!right_seq)
    goto cleanup;

  right_rs = rasqal_new_rowsequence_rowsource(world, query, vt, right_seq, right_vars_seq);
  if(!right_rs)
    goto cleanup;
  right_seq = right_vars_seq = NULL;

  /* Create compatibility map */
  rc_map = rasqal_new_row_compatible(vt, left_rs, right_rs);
  if(!rc_map)
    goto cleanup;

  /* Get rows from rowsources */
  row1 = rasqal_rowsource_read_row(left_rs);
  row2 = rasqal_rowsource_read_row(right_rs);
  if(!row1 || !row2)
    goto cleanup;

  /* Test compatibility - should be vacuously compatible (no shared variables) */
  result = rasqal_row_compatible_check(rc_map, row1, row2);

cleanup:
  if(row2)
    rasqal_free_row(row2);
  if(row1)
    rasqal_free_row(row1);
  if(rc_map)
    rasqal_free_row_compatible(rc_map);
  if(right_rs)
    rasqal_free_rowsource(right_rs);
  if(left_rs)
    rasqal_free_rowsource(left_rs);
  if(right_seq)
    raptor_free_sequence(right_seq);
  if(left_seq)
    raptor_free_sequence(left_seq);
  if(right_vars_seq)
    raptor_free_sequence(right_vars_seq);
  if(left_vars_seq)
    raptor_free_sequence(left_vars_seq);
  if(query)
    rasqal_free_query(query);

  return result;
}

static int
test_unbound_variable_compatibility(rasqal_world* world)
{
  rasqal_query* query = NULL;
  rasqal_row_compatible* rc_map = NULL;
  rasqal_rowsource* left_rs = NULL;
  rasqal_rowsource* right_rs = NULL;
  raptor_sequence* left_seq = NULL;
  raptor_sequence* right_seq = NULL;
  raptor_sequence* left_vars_seq = NULL;
  raptor_sequence* right_vars_seq = NULL;
  rasqal_variables_table* vt = NULL;
  rasqal_row* row1 = NULL;
  rasqal_row* row2 = NULL;
  int result = 0;
  int vars_count = 2;

  /* Test data: shared variable with same value, one unbound - should be compatible */
  const char* const left_data[] = {
    /* variable names */
    "a", NULL, "b", NULL,
    /* row 1 data - both variables bound */
    "\"value1\"", NULL, "\"value2\"", NULL,
    /* end */
    NULL, NULL, NULL, NULL
  };

  const char* const right_data[] = {
    /* variable names */
    "a", NULL, "b", NULL,
    /* row 1 data - variable a bound (same value), variable b unbound (NULL) */
    "\"value1\"", NULL, NULL, NULL,
    /* end */
    NULL, NULL, NULL, NULL
  };

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query)
    goto cleanup;

  vt = query->vars_table;

  /* Create rowsources with test data */
  left_seq = rasqal_new_row_sequence(world, vt, left_data, vars_count, &left_vars_seq);
  if(!left_seq)
    goto cleanup;

  left_rs = rasqal_new_rowsequence_rowsource(world, query, vt, left_seq, left_vars_seq);
  if(!left_rs)
    goto cleanup;
  left_seq = left_vars_seq = NULL;

  right_seq = rasqal_new_row_sequence(world, vt, right_data, vars_count, &right_vars_seq);
  if(!right_seq)
    goto cleanup;

  right_rs = rasqal_new_rowsequence_rowsource(world, query, vt, right_seq, right_vars_seq);
  if(!right_rs)
    goto cleanup;
  right_seq = right_vars_seq = NULL;

  /* Create compatibility map */
  rc_map = rasqal_new_row_compatible(vt, left_rs, right_rs);
  if(!rc_map)
    goto cleanup;

  /* Get rows from rowsources */
  row1 = rasqal_rowsource_read_row(left_rs);
  row2 = rasqal_rowsource_read_row(right_rs);
  if(!row1 || !row2)
    goto cleanup;

  /* Test compatibility - should be compatible (shared variable 'a' has same value, unbound variable 'b' doesn't affect compatibility) */
  result = rasqal_row_compatible_check(rc_map, row1, row2);

cleanup:
  if(row2)
    rasqal_free_row(row2);
  if(row1)
    rasqal_free_row(row1);
  if(rc_map)
    rasqal_free_row_compatible(rc_map);
  if(right_rs)
    rasqal_free_rowsource(right_rs);
  if(left_rs)
    rasqal_free_rowsource(left_rs);
  if(right_seq)
    raptor_free_sequence(right_seq);
  if(left_seq)
    raptor_free_sequence(left_seq);
  if(right_vars_seq)
    raptor_free_sequence(right_vars_seq);
  if(left_vars_seq)
    raptor_free_sequence(left_vars_seq);
  if(query)
    rasqal_free_query(query);

  return result;
}

int
main(int argc, char *argv[])
{
  rasqal_world* world = NULL;
  int failures = 0;
  int result;

  printf("Testing SPARQL solution compatibility for MINUS operations...\n\n");

  world = rasqal_new_world();
  if(!world) {
    printf("Failed to create rasqal world\n");
    return 1;
  }

  if(rasqal_world_open(world)) {
    printf("Failed to open rasqal world\n");
    rasqal_free_world(world);
    return 1;
  }

  result = test_basic_compatibility(world);
  failures += !result;
  print_test_result("Basic compatibility (same values)", result);

  result = test_incompatible_solutions(world);
  failures += !result;
  print_test_result("Incompatible solutions (different values)", result);

  result = test_vacuous_compatibility(world);
  failures += !result;
  print_test_result("Vacuous compatibility (no shared variables)", result);

  result = test_unbound_variable_compatibility(world);
  failures += !result;
  print_test_result("Unbound variable compatibility", result);

  printf("\nTotal failures: %d\n", failures);

  rasqal_free_world(world);

  return failures;
}

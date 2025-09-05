/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_minus.c - Rasqal MINUS rowsource class
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
#include <string.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <raptor.h>

#include "rasqal.h"
#include "rasqal_internal.h"



#ifndef STANDALONE

typedef struct 
{
  rasqal_rowsource* lhs_rowsource;
  rasqal_rowsource* rhs_rowsource;

  /* Current LHS row being processed */
  rasqal_row* current_lhs_row;

  /* RHS rows cache for current LHS row */
  raptor_sequence* rhs_rows;
  int rhs_rows_read;

  /* Row compatibility map for MINUS compatibility checking */
  rasqal_row_compatible* rc_map;

  /* Failed flag */
  int failed;

  /* row offset for read_row() */
  int offset;
} rasqal_minus_rowsource_context;


static int
rasqal_minus_rowsource_init(rasqal_rowsource* rowsource, void *user_data) 
{
  rasqal_minus_rowsource_context* con;

  con = (rasqal_minus_rowsource_context*)user_data;
  
  if(!con->lhs_rowsource || !con->rhs_rowsource) {
    con->failed = 1;
    return 1;
  }
  
  con->failed = 0;
  con->current_lhs_row = NULL;
  con->rhs_rows = NULL;
  con->rhs_rows_read = 0;

  /* Create compatibility map for LHS and RHS rowsources */
  con->rc_map = rasqal_new_row_compatible(rowsource->query->vars_table, 
                                          con->lhs_rowsource, 
                                          con->rhs_rowsource);
  if(!con->rc_map) {
    con->failed = 1;
    return 1;
  }

  /* Set up reset requirements for both rowsources */
  rasqal_rowsource_set_requirements(con->lhs_rowsource,
                                    RASQAL_ROWSOURCE_REQUIRE_RESET);
  rasqal_rowsource_set_requirements(con->rhs_rowsource,
                                    RASQAL_ROWSOURCE_REQUIRE_RESET);

  return 0;
}


static int
rasqal_minus_rowsource_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_minus_rowsource_context* con;
  con = (rasqal_minus_rowsource_context*)user_data;
  
  if(con->lhs_rowsource)
    rasqal_free_rowsource(con->lhs_rowsource);

  if(con->rhs_rowsource)
    rasqal_free_rowsource(con->rhs_rowsource);

  if(con->current_lhs_row)
    rasqal_free_row(con->current_lhs_row);

  if(con->rhs_rows)
    raptor_free_sequence(con->rhs_rows);

  if(con->rc_map)
    rasqal_free_row_compatible(con->rc_map);
  
  RASQAL_FREE(rasqal_minus_rowsource_context, con);
  
  return 0;
}


static int
rasqal_minus_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                        void *user_data)
{
  rasqal_minus_rowsource_context* con;
  int rc = 0;
  
  con = (rasqal_minus_rowsource_context*)user_data;
  
  /* Ensure LHS variables are available */
  rc = rasqal_rowsource_ensure_variables(con->lhs_rowsource);
  if(rc)
    return rc;
  
  /* Copy variables from LHS rowsource */
  rc = rasqal_rowsource_copy_variables(rowsource, con->lhs_rowsource);
  if(rc)
    return rc;
  
  /* RHS variables are not needed in output - MINUS only returns LHS variables */
  
  return 0;
}


/**
 * rasqal_minus_compatible_check:
 * @map: row compatible map object
 * @first_row: LHS row
 * @second_row: RHS row
 *
 * SPARQL 1.1 MINUS-specific compatibility check
 * 
 * Fixes the SPARQL 1.1 disjoint domain bug where the standard 
 * rasqal_row_compatible_check() incorrectly treats disjoint domains
 * as always compatible. For MINUS operations, disjoint domains
 * should be INCOMPATIBLE.
 */
static int
rasqal_minus_compatible_check(rasqal_row_compatible* map,
                              rasqal_row* lhs_row, rasqal_row* rhs_row)
{
  int i;
  int count = map->variables_count;
  int is_compatible = 1;
  int variables_checked = 0;

  /* Incompatible if there are no shared variables */
  if(map->variables_in_both_rows_count == 0)
    return 0;
  
  /* Check compatibility for shared variables using SPARQL 1.1 semantics */
  for(i = 0; i < count && is_compatible; i++) {
    rasqal_literal* lhs_value = NULL;
    rasqal_literal* rhs_value = NULL;
    
    int offset1 = map->defined_in_map[i<<1];
    int offset2 = map->defined_in_map[1 + (i<<1)];
    
    /* Skip variables not in both rowsources */
    if(offset1 < 0 || offset2 < 0)
      continue;
      
    if(offset1 >= 0 && lhs_row->values)
      lhs_value = lhs_row->values[offset1];
      
    if(offset2 >= 0 && rhs_row->values)
      rhs_value = rhs_row->values[offset2];
    
    /* SPARQL 1.1 semantics: only check variables that are bound in
     * BOTH solutions.  If either is NULL (unbound), skip this
     * variable for compatibility checking
     */
    if(!lhs_value || !rhs_value)
      continue;
      
    variables_checked++;
    
    /* Both bound: must be equal to be compatible */
    if(!rasqal_literal_equals(lhs_value, rhs_value)) {
      is_compatible = 0;
      break;
    }
  }
  
  /* If no variables were actually checked (all were NULL), treat as
   * incompatible */
  if(!variables_checked)
    is_compatible = 0;
  
  return is_compatible;
}


static rasqal_row*
rasqal_minus_rowsource_read_row(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_minus_rowsource_context* con;
  rasqal_row* lhs_row = NULL;
  int compatible_found = 0;
  int k;

  con = (rasqal_minus_rowsource_context*)user_data;
  
  if(con->failed)
    return NULL;

  /* Read all RHS rows once if we haven't done so yet */
  if(!con->rhs_rows_read) {
    /* Manual row reading (works correctly, unlike read_all_rows) */
    con->rhs_rows = raptor_new_sequence((raptor_data_free_handler)rasqal_free_row, 
                                        (raptor_data_print_handler)rasqal_row_print);
    if(con->rhs_rows) {
      rasqal_row* rhs_row;
      while((rhs_row = rasqal_rowsource_read_row(con->rhs_rowsource)) != NULL) {
        raptor_sequence_push(con->rhs_rows, rhs_row);
      }
    }
    con->rhs_rows_read = 1;
  }

  while(1) {
    /* Get next LHS row if we don't have one cached */
    if(!con->current_lhs_row) {
      con->current_lhs_row = rasqal_rowsource_read_row(con->lhs_rowsource);
      if(!con->current_lhs_row)
        return NULL; /* No more LHS rows */
    }

    lhs_row = con->current_lhs_row;
    compatible_found = 0;

    /* Check if current LHS row is compatible with any RHS row */
    if(con->rhs_rows) {
      int rhs_count = raptor_sequence_size(con->rhs_rows);
     
      for(k = 0; k < rhs_count; k++) {
        rasqal_row* rhs_row = (rasqal_row*)raptor_sequence_get_at(con->rhs_rows,
                                                                  k);
        if(rhs_row && rasqal_minus_compatible_check(con->rc_map, lhs_row,
                                                    rhs_row)) {
          compatible_found = 1;
          break;
        }
      }
    }

    if(!compatible_found) {
      /* No compatible RHS row found - return this LHS row */
      rasqal_row* new_row = rasqal_new_row_from_row_deep(lhs_row);
      
      /* Clear current LHS row now that we've copied it */
      rasqal_free_row(con->current_lhs_row);
      con->current_lhs_row = NULL;
      
      if(new_row) {
        rasqal_row_set_rowsource(new_row, rowsource);
        new_row->offset = con->offset++;
        return new_row;
      } else {
        /* Failed to create new row */
        return NULL;
      }
    }

    /* Compatible row found - clear current LHS row and try the next one */
    rasqal_free_row(con->current_lhs_row);
    con->current_lhs_row = NULL;
  }

  return NULL;
}


static raptor_sequence*
rasqal_minus_rowsource_read_all_rows(rasqal_rowsource* rowsource,
                                     void *user_data)
{
  rasqal_minus_rowsource_context* con;
  raptor_sequence* rows = NULL;
  rasqal_row* row = NULL;

  con = (rasqal_minus_rowsource_context*)user_data;
  
  if(con->failed)
    return NULL;

  rows = raptor_new_sequence((raptor_data_free_handler)rasqal_free_row,
                             (raptor_data_print_handler)rasqal_row_print);
  if(!rows)
    return NULL;

  while(1) {
    row = rasqal_minus_rowsource_read_row(rowsource, user_data);
    if(!row)
      break;
    
    raptor_sequence_push(rows, row);
  }

  return rows;
}


static int
rasqal_minus_rowsource_reset(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_minus_rowsource_context* con;
  
  con = (rasqal_minus_rowsource_context*)user_data;
  
  rasqal_rowsource_reset(con->lhs_rowsource);
  rasqal_rowsource_reset(con->rhs_rowsource);

  /* Clear cached state */
  if(con->current_lhs_row) {
    rasqal_free_row(con->current_lhs_row);
    con->current_lhs_row = NULL;
  }

  if(con->rhs_rows) {
    raptor_free_sequence(con->rhs_rows);
    con->rhs_rows = NULL;
  }

  con->rhs_rows_read = 0;
  con->offset = 0;
  
  return 0;
}


static int
rasqal_minus_rowsource_set_requirements(rasqal_rowsource* rowsource,
                                        void *user_data, 
                                        unsigned int requirements)
{
  rasqal_minus_rowsource_context* con;
  
  con = (rasqal_minus_rowsource_context*)user_data;
  
  if(con->lhs_rowsource)
    rasqal_rowsource_set_requirements(con->lhs_rowsource, requirements);
  
  return 0;
}


static rasqal_rowsource*
rasqal_minus_rowsource_get_inner_rowsource(rasqal_rowsource* rowsource,
                                           void *user_data, 
                                           int offset)
{
  rasqal_minus_rowsource_context* con;
  
  con = (rasqal_minus_rowsource_context*)user_data;
  
  if(offset == 0)
    return con->lhs_rowsource;
  else
    return NULL;
}


static const rasqal_rowsource_handler rasqal_minus_rowsource_handler = {
  /* .version = */ 1,
  "minus",
  /* .init = */ rasqal_minus_rowsource_init,
  /* .finish = */ rasqal_minus_rowsource_finish,
  /* .ensure_variables = */ rasqal_minus_rowsource_ensure_variables,
  /* .read_row = */ rasqal_minus_rowsource_read_row,
  /* .read_all_rows = */ rasqal_minus_rowsource_read_all_rows,
  /* .reset = */ rasqal_minus_rowsource_reset,
  /* .set_requirements = */ rasqal_minus_rowsource_set_requirements,
  /* .get_inner_rowsource = */ rasqal_minus_rowsource_get_inner_rowsource,
  /* .set_origin = */ NULL,
};


/**
 * rasqal_new_minus_rowsource:
 * @world: world object
 * @query: query object
 * @lhs_rowsource: left-hand side rowsource
 * @rhs_rowsource: right-hand side rowsource
 *
 * INTERNAL - create a new MINUS (set difference) over LHS and RHS rowsources
 *
 * This creates a rowsource that returns solutions from the LHS that are
 * not compatible with any solution from the RHS. Two solutions are compatible
 * if they have the same values for all shared variables.
 *
 * Both @lhs_rowsource and @rhs_rowsource become owned by the new rowsource.
 *
 * Return value: new rowsource or NULL on failure
 */
rasqal_rowsource*
rasqal_new_minus_rowsource(rasqal_world *world,
                           rasqal_query* query,
                           rasqal_rowsource* lhs_rowsource,
                           rasqal_rowsource* rhs_rowsource)
{
  rasqal_minus_rowsource_context* con;
  int flags = 0;

  if(!world || !query || !lhs_rowsource || !rhs_rowsource)
    goto fail;
  
  con = RASQAL_CALLOC(rasqal_minus_rowsource_context*, 1, sizeof(*con));
  if(!con)
    goto fail;

  con->lhs_rowsource = lhs_rowsource;
  con->rhs_rowsource = rhs_rowsource;
  con->current_lhs_row = NULL;
  con->rhs_rows = NULL;
  con->rhs_rows_read = 0;
  con->offset = 0;

  return rasqal_new_rowsource_from_handler(world, query,
                                           con,
                                           &rasqal_minus_rowsource_handler,
                                           query->vars_table,
                                           flags);

 fail:
  if(lhs_rowsource)
    rasqal_free_rowsource(lhs_rowsource);
  if(rhs_rowsource)
    rasqal_free_rowsource(rhs_rowsource);
  return NULL;
}


#endif /* not STANDALONE */



#ifdef STANDALONE

/* one more prototype */
int main(int argc, char *argv[]);


const char* const minus_1_data_lhs_2x3_rows[] =
{
  /* 2 variable names and 3 rows */
  "a",   NULL, "b",   NULL,
  /* row 1 data */
  "foo", NULL, "bar", NULL,
  /* row 2 data */
  "baz", NULL, "fez", NULL,
  /* row 3 data */
  "bob", NULL, "sue", NULL,
  /* end of data */
  NULL, NULL, NULL, NULL
};

const char* const minus_1_data_rhs_2x2_rows[] =
{
  /* 2 variable names and 2 rows */
  "a",   NULL, "b",   NULL,
  /* row 1 data */
  "foo", NULL, "bar", NULL,
  /* row 2 data */
  "baz", NULL, "fez", NULL,
  /* end of data */
  NULL, NULL, NULL, NULL
};

const char* const minus_1_expected_1x1_rows[] =
{
  /* 2 variable names and 1 row */
  "a",   NULL, "b",   NULL,
  /* row 1 data */
  "bob", NULL, "sue", NULL,
  /* end of data */
  NULL, NULL, NULL, NULL
};


const char* const minus_2_data_lhs_3x2_rows[] =
{
  /* 3 variable names and 2 rows */
  "a",   NULL, "b",   NULL, "c",   NULL,
  /* row 1 data */
  "foo", NULL, "bar", NULL, "baz", NULL,
  /* row 2 data */
  "bob", NULL, "sue", NULL, "sam", NULL,
  /* end of data */
  NULL, NULL, NULL, NULL, NULL, NULL
};

const char* const minus_2_data_rhs_2x1_rows[] =
{
  /* 2 variable names and 1 row */
  "a",   NULL, "b",   NULL,
  /* row 1 data */
  "foo", NULL, "bar", NULL,
  /* end of data */
  NULL, NULL, NULL, NULL
};

const char* const minus_2_expected_3x1_rows[] =
{
  /* 3 variable names and 1 row */
  "a",   NULL, "b",   NULL, "c",   NULL,
  /* row 1 data */
  "bob", NULL, "sue", NULL, "sam", NULL,
  /* end of data */
  NULL, NULL, NULL, NULL, NULL, NULL
};


const char* const minus_3_data_lhs_2x2_rows[] =
{
  /* 2 variable names and 2 rows */
  "a",   NULL, "b",   NULL,
  /* row 1 data */
  "foo", NULL, "bar", NULL,
  /* row 2 data */
  "baz", NULL, "fez", NULL,
  /* end of data */
  NULL, NULL, NULL, NULL
};

const char* const minus_3_data_rhs_1x1_rows[] =
{
  /* 1 variable name and 1 row */
  "a",   NULL,
  /* row 1 data */
  "foo", NULL,
  /* end of data */
  NULL, NULL
};

const char* const minus_3_expected_2x1_rows[] =
{
  /* 2 variable names and 1 row */
  "a",   NULL, "b",   NULL,
  /* row 1 data */
  "baz", NULL, "fez", NULL,
  /* end of data */
  NULL, NULL, NULL, NULL
};


int
main(int argc, char *argv[]) 
{
  const char *program = rasqal_basename((const char*)argv[0]);
  rasqal_world* world = NULL;
  rasqal_query* query = NULL;
  rasqal_rowsource* lhs_rs = NULL;
  rasqal_rowsource* rhs_rs = NULL;
  rasqal_rowsource* minus_rs = NULL;
  rasqal_variables_table* vt = NULL;
  int failures = 0;
  int test_count = 0;
  int vars_count;
  raptor_sequence* seq = NULL;
  raptor_sequence* vars_seq = NULL;
  int count;



  world = rasqal_new_world();
  rasqal_world_open(world);

  query = rasqal_new_query(world, "sparql", NULL);
  vt = query->vars_table;

  fprintf(RASQAL_DEBUG_FH, "%s: Testing MINUS rowsource\n", program);

  /* Test 1: Basic MINUS with same variables */
  test_count++;
  fprintf(RASQAL_DEBUG_FH, "%s: Test %d: Basic MINUS with same variables\n", program,
          test_count);
  
  /* Create LHS rowsource with 2 variables and 3 rows */
  vars_count = 2;
  seq = rasqal_new_row_sequence(world, vt, minus_1_data_lhs_2x3_rows,
                                vars_count, &vars_seq);
  if(!seq) {
    fprintf(RASQAL_DEBUG_FH, "%s: failed to create LHS sequence of %d vars\n",
            program, vars_count);
    failures++;
    goto test1_fail;
  }
  
  lhs_rs = rasqal_new_rowsequence_rowsource(world, query, vt, seq, vars_seq);
  if(!lhs_rs) {
    fprintf(RASQAL_DEBUG_FH, "%s: failed to create LHS rowsource\n", program);
    failures++;
    goto test1_fail;
  }
  /* vars_seq and seq are now owned by lhs_rs */
  vars_seq = seq = NULL;
  
  /* Create RHS rowsource with 2 variables and 2 rows */
  vars_count = 2;
  seq = rasqal_new_row_sequence(world, vt, minus_1_data_rhs_2x2_rows,
                                vars_count, &vars_seq);
  if(!seq) {
    fprintf(RASQAL_DEBUG_FH, "%s: failed to create RHS sequence of %d vars\n",
            program, vars_count);
    failures++;
    goto test1_fail;
  }
  
  rhs_rs = rasqal_new_rowsequence_rowsource(world, query, vt, seq, vars_seq);
  if(!rhs_rs) {
    fprintf(RASQAL_DEBUG_FH, "%s: failed to create RHS rowsource\n", program);
    failures++;
    goto test1_fail;
  }
  /* vars_seq and seq are now owned by rhs_rs */
  vars_seq = seq = NULL;
  

  
  minus_rs = rasqal_new_minus_rowsource(world, query, lhs_rs, rhs_rs);
  if(!minus_rs) {
    fprintf(RASQAL_DEBUG_FH, "%s: failed to create MINUS rowsource\n", program);
    failures++;
    goto test1_fail;
  }
  /* lhs_rs and rhs_rs are now owned by minus_rs */
  lhs_rs = rhs_rs = NULL;
  
  /* Read all rows and check count */
  seq = rasqal_rowsource_read_all_rows(minus_rs);
  if(!seq) {
    fprintf(RASQAL_DEBUG_FH, "%s: read_rows returned a NULL seq for MINUS rowsource\n",
            program);
    failures++;
    goto test1_fail;
  }
  
  count = raptor_sequence_size(seq);
  if(count != 1) {
    fprintf(RASQAL_DEBUG_FH,
            "%s: read_rows returned %d rows for MINUS rowsource, expected 1\n",
            program, count);
    failures++;
    goto test1_fail;
  }
  
  fprintf(RASQAL_DEBUG_FH, "%s: Test %d passed\n", program, test_count);

  test1_fail:
  if(seq)
    raptor_free_sequence(seq);
  if(minus_rs)
    rasqal_free_rowsource(minus_rs);
  if(lhs_rs)
    rasqal_free_rowsource(lhs_rs);
  if(rhs_rs)
    rasqal_free_rowsource(rhs_rs);

  /* Test 2: MINUS with different variable sets */
  test_count++;
  fprintf(RASQAL_DEBUG_FH, "%s: Test %d: MINUS with different variable sets\n",
          program, test_count);
  
  /* Create LHS rowsource with 3 variables and 2 rows */
  vars_count = 3;
  seq = rasqal_new_row_sequence(world, vt, minus_2_data_lhs_3x2_rows,
                                vars_count, &vars_seq);
  if(!seq) {
    fprintf(RASQAL_DEBUG_FH, "%s: failed to create LHS sequence of %d vars\n", program,
            vars_count);
    failures++;
    goto test2_fail;
  }
  
  lhs_rs = rasqal_new_rowsequence_rowsource(world, query, vt, seq, vars_seq);
  if(!lhs_rs) {
    fprintf(RASQAL_DEBUG_FH, "%s: failed to create LHS rowsource\n", program);
    failures++;
    goto test2_fail;
  }
  /* vars_seq and seq are now owned by lhs_rs */
  vars_seq = seq = NULL;
  
  /* Create RHS rowsource with 2 variables and 1 row */
  vars_count = 2;
  seq = rasqal_new_row_sequence(world, vt, minus_2_data_rhs_2x1_rows,
                                vars_count, &vars_seq);
  if(!seq) {
    fprintf(RASQAL_DEBUG_FH, "%s: failed to create RHS sequence of %d vars\n",
            program, vars_count);
    failures++;
    goto test2_fail;
  }
  
  rhs_rs = rasqal_new_rowsequence_rowsource(world, query, vt, seq, vars_seq);
  if(!rhs_rs) {
    fprintf(RASQAL_DEBUG_FH, "%s: failed to create RHS rowsource\n", program);
    failures++;
    goto test2_fail;
  }
  /* vars_seq and seq are now owned by rhs_rs */
  vars_seq = seq = NULL;
  

  
  minus_rs = rasqal_new_minus_rowsource(world, query, lhs_rs, rhs_rs);
  if(!minus_rs) {
    fprintf(RASQAL_DEBUG_FH, "%s: failed to create MINUS rowsource\n", program);
    failures++;
    goto test2_fail;
  }
  /* lhs_rs and rhs_rs are now owned by minus_rs */
  lhs_rs = rhs_rs = NULL;
  
  /* Read all rows and check count */
  seq = rasqal_rowsource_read_all_rows(minus_rs);
  if(!seq) {
    fprintf(RASQAL_DEBUG_FH,
            "%s: read_rows returned a NULL seq for MINUS rowsource\n", program);
    failures++;
    goto test2_fail;
  }
  
  count = raptor_sequence_size(seq);
  if(count != 1) {
    fprintf(RASQAL_DEBUG_FH,
            "%s: read_rows returned %d rows for MINUS rowsource, expected 1\n",
            program, count);
    failures++;
    goto test2_fail;
  }
  
  fprintf(RASQAL_DEBUG_FH, "%s: Test %d passed\n", program, test_count);

  test2_fail:
  if(seq)
    raptor_free_sequence(seq);
  if(minus_rs)
    rasqal_free_rowsource(minus_rs);
  if(lhs_rs)
    rasqal_free_rowsource(lhs_rs);
  if(rhs_rs)
    rasqal_free_rowsource(rhs_rs);

  /* Test 3: MINUS with partial variable match */
  test_count++;
  fprintf(RASQAL_DEBUG_FH, "%s: Test %d: MINUS with partial variable match\n",
          program, test_count);
  
  /* Create LHS rowsource with 2 variables and 2 rows */
  vars_count = 2;
  seq = rasqal_new_row_sequence(world, vt, minus_3_data_lhs_2x2_rows,
                                vars_count, &vars_seq);
  if(!seq) {
    fprintf(RASQAL_DEBUG_FH, "%s: failed to create LHS sequence of %d vars\n",
            program, vars_count);
    failures++;
    goto test3_fail;
  }
  
  lhs_rs = rasqal_new_rowsequence_rowsource(world, query, vt, seq, vars_seq);
  if(!lhs_rs) {
    fprintf(RASQAL_DEBUG_FH, "%s: failed to create LHS rowsource\n", program);
    failures++;
    goto test3_fail;
  }
  /* vars_seq and seq are now owned by lhs_rs */
  vars_seq = seq = NULL;
  
  /* Create RHS rowsource with 1 variable and 1 row */
  vars_count = 1;
  seq = rasqal_new_row_sequence(world, vt, minus_3_data_rhs_1x1_rows,
                                vars_count, &vars_seq);
  if(!seq) {
    fprintf(RASQAL_DEBUG_FH, "%s: failed to create RHS sequence of %d vars\n",
            program, vars_count);
    failures++;
    goto test3_fail;
  }
  
  rhs_rs = rasqal_new_rowsequence_rowsource(world, query, vt, seq, vars_seq);
  if(!rhs_rs) {
    fprintf(RASQAL_DEBUG_FH, "%s: failed to create RHS rowsource\n", program);
    failures++;
    goto test3_fail;
  }
  /* vars_seq and seq are now owned by rhs_rs */
  vars_seq = seq = NULL;
  

  
  minus_rs = rasqal_new_minus_rowsource(world, query, lhs_rs, rhs_rs);
  if(!minus_rs) {
    fprintf(RASQAL_DEBUG_FH, "%s: failed to create MINUS rowsource\n", program);
    failures++;
    goto test3_fail;
  }
  /* lhs_rs and rhs_rs are now owned by minus_rs */
  lhs_rs = rhs_rs = NULL;
  
  /* Read all rows and check count */
  seq = rasqal_rowsource_read_all_rows(minus_rs);
  if(!seq) {
    fprintf(RASQAL_DEBUG_FH,
            "%s: read_rows returned a NULL seq for MINUS rowsource\n", program);
    failures++;
    goto test3_fail;
  }
  
  count = raptor_sequence_size(seq);
  if(count != 1) {
    fprintf(RASQAL_DEBUG_FH,
            "%s: read_rows returned %d rows for MINUS rowsource, expected 1\n",
            program, count);
    failures++;
    goto test3_fail;
  }
  
  fprintf(RASQAL_DEBUG_FH, "%s: Test %d passed\n", program, test_count);

  test3_fail:
  if(seq)
    raptor_free_sequence(seq);
  if(minus_rs)
    rasqal_free_rowsource(minus_rs);
  if(lhs_rs)
    rasqal_free_rowsource(lhs_rs);
  if(rhs_rs)
    rasqal_free_rowsource(rhs_rs);

  rasqal_free_query(query);
  rasqal_free_world(world);

  fprintf(RASQAL_DEBUG_FH, "%s: Completed %d tests with %d failures\n", program,
          test_count, failures);

  return failures;
}

#endif /* STANDALONE */

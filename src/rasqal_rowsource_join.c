/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_join.c - Rasqal join rowsource class
 *
 * Copyright (C) 2008-2012, David Beckett http://www.dajobe.org/
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


#define DEBUG_FH stderr

#ifndef STANDALONE

typedef enum {
  JS_START,
  JS_INIT_RIGHT,
  JS_READ_RIGHT,
  JS_FINISHED
} rasqal_join_state;

typedef struct 
{
  rasqal_rowsource* left;

  rasqal_rowsource* right;

  /* current left row */
  rasqal_row *left_row;
  
  /* array to map right variables into output rows */
  int* right_map;

  /* 0 = reading from left rs, 1 = reading from right rs, 2 = finished */
  rasqal_join_state state;

  int failed;

  /* row offset for read_row() */
  int offset;

  /* row join type */
  rasqal_join_type join_type;
  
  /* join expression */
  rasqal_expression *expr;

  /* map for checking compatibility of rows */
  rasqal_row_compatible* rc_map;

  /* number of right rows joined per-left */
  int right_rows_joined_count;

  /* join expression constant boolean value or < 0 if not valid */
  int constant_join_condition;
} rasqal_join_rowsource_context;


static int
rasqal_join_rowsource_init(rasqal_rowsource* rowsource, void *user_data) 
{
  rasqal_join_rowsource_context* con;
  rasqal_variables_table* vars_table; 

  con = (rasqal_join_rowsource_context*)user_data;

  con->failed = 0;
  con->state = JS_START;
  con->constant_join_condition = -1;

  /* If join condition is a constant - optimize it away */
  if(con->expr && rasqal_expression_is_constant(con->expr)) {
    rasqal_query *query = rowsource->query;
    rasqal_literal* result;
    int bresult;
    int error = 0;
    
    result = rasqal_expression_evaluate2(con->expr, query->eval_context,
                                         &error);

#ifdef RASQAL_DEBUG
    RASQAL_DEBUG1("join expression condition is constant: ");
    if(error)
      fputs("type error", DEBUG_FH);
    else
      rasqal_literal_print(result, DEBUG_FH);
    fputc('\n', DEBUG_FH);
#endif

    if(error) {
      bresult = 0;
    } else {
      error = 0;
      bresult = rasqal_literal_as_boolean(result, &error);
#ifdef RASQAL_DEBUG
      if(error)
        RASQAL_DEBUG1("join boolean expression returned error\n");
      else
        RASQAL_DEBUG2("join boolean expression result: %d\n", bresult);
#endif
      rasqal_free_literal(result);
    }

    /* free expression always */
    rasqal_free_expression(con->expr); con->expr = NULL;

    if(con->join_type == RASQAL_JOIN_TYPE_NATURAL) {
      if(!bresult) {
        /* Constraint is always false so row source is finished */
        con->state = JS_FINISHED;
      }
      /* otherwise always true so no need to evaluate on each row
       * and deleting con->expr will handle that
       */
    }

    con->constant_join_condition = bresult;
  }

  rasqal_rowsource_set_requirements(con->left, RASQAL_ROWSOURCE_REQUIRE_RESET);
  rasqal_rowsource_set_requirements(con->right, RASQAL_ROWSOURCE_REQUIRE_RESET);
  
  vars_table = con->left->vars_table;
  con->rc_map = rasqal_new_row_compatible(vars_table, con->left, con->right);
  if(!con->rc_map)
    return -1;

#ifdef RASQAL_DEBUG
  RASQAL_DEBUG2("rowsource %p ", rowsource);
  rasqal_print_row_compatible(stderr, con->rc_map);
#endif

  return 0;
}


static int
rasqal_join_rowsource_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_join_rowsource_context* con;
  con = (rasqal_join_rowsource_context*)user_data;

  if(con->left_row)
    rasqal_free_row(con->left_row);
  
  if(con->left)
    rasqal_free_rowsource(con->left);
  
  if(con->right)
    rasqal_free_rowsource(con->right);
  
  if(con->right_map)
    RASQAL_FREE(int, con->right_map);
  
  if(con->expr)
    rasqal_free_expression(con->expr);
  
  if(con->rc_map)
    rasqal_free_row_compatible(con->rc_map);
  
  RASQAL_FREE(rasqal_join_rowsource_context, con);

  return 0;
}


static int
rasqal_join_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                        void *user_data)
{
  rasqal_join_rowsource_context* con;
  int map_size;
  int i;
  
  con = (rasqal_join_rowsource_context*)user_data;

  if(rasqal_rowsource_ensure_variables(con->left))
    return 1;

  if(rasqal_rowsource_ensure_variables(con->right))
    return 1;

  map_size = rasqal_rowsource_get_size(con->right);
  con->right_map = RASQAL_MALLOC(int*, RASQAL_GOOD_CAST(size_t,
                                                        sizeof(int) * RASQAL_GOOD_CAST(size_t, map_size)));
  if(!con->right_map)
    return 1;

  rowsource->size = 0;

  /* copy in variables from left rowsource */
  if(rasqal_rowsource_copy_variables(rowsource, con->left))
    return 1;
  
  /* add any new variables not already seen from right rowsource */
  for(i = 0; i < map_size; i++) {
    rasqal_variable* v;
    int offset;
    
    v = rasqal_rowsource_get_variable_by_offset(con->right, i);
    if(!v)
      break;
    offset = rasqal_rowsource_add_variable(rowsource, v);
    if(offset < 0)
      return 1;

    con->right_map[i] = offset;
  }

  return 0;
}


static rasqal_row*
rasqal_join_rowsource_build_merged_row(rasqal_rowsource* rowsource,
                                       rasqal_join_rowsource_context* con,
                                       rasqal_row *right_row)
{
  rasqal_row *row;
  int i;

  row = rasqal_new_row_for_size(rowsource->world, rowsource->size);
  if(!row) {
    if(right_row)
      rasqal_free_row(right_row);
    return NULL;
  }

  rasqal_row_set_rowsource(row, rowsource);
  row->offset = con->offset;

#ifdef RASQAL_DEBUG
  RASQAL_DEBUG1("merge\n  left row   : ");
  rasqal_row_print(con->left_row, stderr);
  fputs("\n  right row  : ", stderr);
  if(right_row)
    rasqal_row_print(right_row, stderr);
  else
    fputs("NONE", stderr);
  fputs("\n", stderr);
#endif

  for(i = 0; i < con->left_row->size; i++) {
    rasqal_literal *l = con->left_row->values[i];
    row->values[i] = rasqal_new_literal_from_literal(l);
  }

  if(right_row) {
    for(i = 0; i < right_row->size; i++) {
      rasqal_literal *l = right_row->values[i];
      int dest_i = con->right_map[i];
      if(!row->values[dest_i])
        row->values[dest_i] = rasqal_new_literal_from_literal(l);
    }

    rasqal_free_row(right_row);
  }
  
#ifdef RASQAL_DEBUG
  fputs("  result row : ", stderr);
  if(row)
    rasqal_row_print(row, stderr);
  else
    fputs("NONE", stderr);
  fputs("\n", stderr);
#endif

  return row;
}


static rasqal_row*
rasqal_join_rowsource_read_row(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_join_rowsource_context* con;
  rasqal_row* row = NULL;
  rasqal_query *query = rowsource->query;

  con = (rasqal_join_rowsource_context*)user_data;

  if(con->failed || con->state == JS_FINISHED)
    return NULL;

  while(1) {
    rasqal_row *right_row;
    int bresult = 1;
    int compatible = 1;

    if(con->state == JS_START) {
      /* start / re-start left */
      if(con->left_row)
        rasqal_free_row(con->left_row);

      con->left_row  = rasqal_rowsource_read_row(con->left);
#ifdef RASQAL_DEBUG
      RASQAL_DEBUG2("rowsource %p read left row : ", rowsource);
      if(con->left_row)
        rasqal_row_print(con->left_row, stderr);
      else
        fputs("NONE", stderr);
      fputs("\n", stderr);
#endif
      con->state = JS_INIT_RIGHT;
    }

    if(con->state == JS_INIT_RIGHT) {
      /* start right */

      if(!con->left_row) {
        con->state = JS_FINISHED;
        return NULL;
      }

      con->right_rows_joined_count = 0;

      rasqal_rowsource_reset(con->right);
    }


    right_row = rasqal_rowsource_read_row(con->right);
#ifdef RASQAL_DEBUG
    RASQAL_DEBUG2("rowsource %p read right row : ", rowsource);
    if(right_row)
      rasqal_row_print(right_row, stderr);
    else
      fputs("NONE", stderr);
    fputs("\n", stderr);
#endif

    if(!right_row && con->state == JS_READ_RIGHT) {
      /* right has finished */

      /* restart left */
      con->state = JS_START;

      /* if all right table returned no bindings, return left row */
      if(!con->right_rows_joined_count) {
        /* otherwise return LEFT or RIGHT row only */
        if(con->join_type == RASQAL_JOIN_TYPE_LEFT) {
          /* LEFT JOIN - add left row if expr fails or not compatible */
          if(con->left_row) {
            con->right_rows_joined_count++;
        
            row = rasqal_join_rowsource_build_merged_row(rowsource, con, NULL);
            break;
          }
        }
      }

      /* restart left by continuing the loop */
      continue;
    }


    /* state is always JS_READ_RIGHT at this point */
    con->state = JS_READ_RIGHT;

    /* now may have both left and right rows so compute compatibility */
    if(right_row) {
      compatible = rasqal_row_compatible_check(con->rc_map,
                                               con->left_row, right_row);
      RASQAL_DEBUG2("join rows compatible: %s\n", compatible ? "YES" : "NO");
    }


    if(con->constant_join_condition >= 0) {
      /* Get constant join expression value */
      bresult = con->constant_join_condition;
    } else if(con->expr) {
      /* Check join expression if present */
      rasqal_literal *result;
      int error = 0;
      
      result = rasqal_expression_evaluate2(con->expr, query->eval_context,
                                           &error);
#ifdef RASQAL_DEBUG
      RASQAL_DEBUG1("join expression result: ");
      if(error)
        fputs("type error", DEBUG_FH);
      else
        rasqal_literal_print(result, DEBUG_FH);
      fputc('\n', DEBUG_FH);
#endif

      if(error) {
        bresult = 0;
      } else {
        error = 0;
        bresult = rasqal_literal_as_boolean(result, &error);
#ifdef RASQAL_DEBUG
        if(error)
          RASQAL_DEBUG1("filter boolean expression returned error\n");
        else
          RASQAL_DEBUG2("filter boolean expression result: %d\n", bresult);
#endif
        rasqal_free_literal(result);
      }
    }
    
    if(con->join_type == RASQAL_JOIN_TYPE_NATURAL) {
      /* found a row if compatible and constraint matches */
      if(compatible && bresult && right_row) {
        con->right_rows_joined_count++;

        /* consumes right_row */
        row = rasqal_join_rowsource_build_merged_row(rowsource, con, right_row);
        break;
      }
      
    } else if(con->join_type == RASQAL_JOIN_TYPE_LEFT) {
      /*
       * { merge(mu1, mu2) | mu1 in Omega1 and mu2 in Omega2, and mu1
       *   and mu2 are compatible and expr(merge(mu1, mu2)) is true }
       */
      if(compatible && bresult) {
        con->right_rows_joined_count++;

        /* No constraint OR constraint & compatible so return merged row */

        /* Compute row only now it is known to be needed (consumes right_row) */
        row = rasqal_join_rowsource_build_merged_row(rowsource, con, right_row);
        break;
      }

#if 0    
      /*
       * { mu1 | mu1 in Omega1 and mu2 in Omega2, and mu1 and mu2 are
       * not compatible }
       */
      if(!compatible) {
        /* otherwise return LEFT or RIGHT row only */
        if(con->join_type == RASQAL_JOIN_TYPE_LEFT) {
          /* LEFT JOIN - add left row if expr fails or not compatible */
          if(con->left_row) {
            con->right_rows_joined_count++;

            row = rasqal_join_rowsource_build_merged_row(rowsource, con, NULL);
            if(right_row)
              rasqal_free_row(right_row);
            break;
          }
        }
      }
#endif

      /*
       * { mu1 | mu1 in Omega1 and mu2 in Omega2, and mu1 and mu2 are
       *   compatible and for all mu2, expr(merge(mu1, mu2)) is false }
       */
      
      /* The above is handled using check for
       * !con->right_rows_joined_count earlier, to generate a row
       * once.
       */

    } /* end if LEFT JOIN */

    if(right_row)
      rasqal_free_row(right_row);
      
  } /* end while */

  if(row) {
    rasqal_row_set_rowsource(row, rowsource);
    row->offset = con->offset++;

    rasqal_row_bind_variables(row, rowsource->query->vars_table);
  }
  
  return row;
}


static int
rasqal_join_rowsource_reset(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_join_rowsource_context* con;
  int rc;
  
  con = (rasqal_join_rowsource_context*)user_data;

  con->state = JS_START;
  con->failed = 0;
  
  rc = rasqal_rowsource_reset(con->left);
  if(rc)
    return rc;

  return rasqal_rowsource_reset(con->right);
}


static rasqal_rowsource*
rasqal_join_rowsource_get_inner_rowsource(rasqal_rowsource* rowsource,
                                          void *user_data, int offset)
{
  rasqal_join_rowsource_context *con;
  con = (rasqal_join_rowsource_context*)user_data;

  if(offset == 0)
    return con->left;
  else if(offset == 1)
    return con->right;
  else
    return NULL;
}


static const rasqal_rowsource_handler rasqal_join_rowsource_handler = {
  /* .version = */ 1,
  "join",
  /* .init = */ rasqal_join_rowsource_init,
  /* .finish = */ rasqal_join_rowsource_finish,
  /* .ensure_variables = */ rasqal_join_rowsource_ensure_variables,
  /* .read_row = */ rasqal_join_rowsource_read_row,
  /* .read_all_rows = */ NULL,
  /* .reset = */ rasqal_join_rowsource_reset,
  /* .set_requirements = */ NULL,
  /* .get_inner_rowsource = */ rasqal_join_rowsource_get_inner_rowsource,
  /* .set_origin = */ NULL,
};


/**
 * rasqal_new_join_rowsource:
 * @world: world object
 * @query: query object
 * @left: input left (first) rowsource
 * @right: input right (second) rowsource
 * @join_type: join type
 * @expr: join expression to filter result rows
 *
 * INTERNAL - create a new JOIN over two rowsources
 *
 * This uses the number of variables in @vt to set the rowsource size
 * (order size is always 0) and then checks that all the rows in the
 * sequence are the same.  If not, construction fails and NULL is
 * returned.
 *
 * The @left and @right rowsources become owned by the rowsource.
 *
 * Return value: new rowsource or NULL on failure
 */
rasqal_rowsource*
rasqal_new_join_rowsource(rasqal_world *world,
                          rasqal_query* query,
                          rasqal_rowsource* left,
                          rasqal_rowsource* right,
                          rasqal_join_type join_type,
                          rasqal_expression *expr)
{
  rasqal_join_rowsource_context* con;
  int flags = 0;

  if(!world || !query || !left || !right)
    goto fail;

  /* only left outer join and cross join supported */
  if(join_type != RASQAL_JOIN_TYPE_LEFT &&
     join_type != RASQAL_JOIN_TYPE_NATURAL)
    goto fail;
  
  con = RASQAL_CALLOC(rasqal_join_rowsource_context*, 1, sizeof(*con));
  if(!con)
    goto fail;

  con->left = left;
  con->right = right;
  con->join_type = join_type;
  con->expr = rasqal_new_expression_from_expression(expr);
  
  return rasqal_new_rowsource_from_handler(world, query,
                                           con,
                                           &rasqal_join_rowsource_handler,
                                           query->vars_table,
                                           flags);

  fail:
  if(left)
    rasqal_free_rowsource(left);
  if(right)
    rasqal_free_rowsource(right);
  return NULL;
}


#endif /* not STANDALONE */



#ifdef STANDALONE

/* one more prototype */
int main(int argc, char *argv[]);


const char* const join_1_data_2x3_rows[] =
{
  /* 2 variable names and 3 rows */
  "a",   NULL, "b",   NULL,
  /* row 1 data */
  "foo", NULL, "red", NULL,
  /* row 2 data */
  "baz", NULL, "blue", NULL,
  /* row 3 data */
  "bob", NULL, "green", NULL,
  /* end of data */
  NULL, NULL, NULL, NULL
};
  

/* join on b */

const char* const join_2_data_3x2_rows[] =
{
  /* 3 variable names and 4 rows */
  "b",     NULL, "c",      NULL, "d",      NULL,
  /* row 1 data */
  "red",   NULL, "orange", NULL, "yellow", NULL,
  /* row 2 data */
  "blue",  NULL, "indigo", NULL, "violet", NULL,
  /* end of data */
  NULL, NULL, NULL, NULL, NULL, NULL
};


typedef struct {
  rasqal_join_type join_type;
  int expected;
} join_test_config_type;

#define JOIN_TESTS_COUNT 2
const join_test_config_type join_test_config[JOIN_TESTS_COUNT] = { 
  { RASQAL_JOIN_TYPE_NATURAL, 2 },
  { RASQAL_JOIN_TYPE_LEFT, 3 },
};


/* there is one variable 'b' that is joined on */
#define EXPECTED_COLUMNS_COUNT (2 + 3 - 1)
const char* const join_result_vars[] = { "a" , "b" , "c", "d" };


int
main(int argc, char *argv[]) 
{
  const char *program = rasqal_basename(argv[0]);
  rasqal_rowsource *rowsource = NULL;
  rasqal_rowsource *left_rs = NULL;
  rasqal_rowsource *right_rs = NULL;
  rasqal_world* world = NULL;
  rasqal_query* query = NULL;
  int count;
  raptor_sequence* seq = NULL;
  int failures = 0;
  rasqal_variables_table* vt;
  int size;
  int expected_size = EXPECTED_COLUMNS_COUNT;
  int i;
  raptor_sequence* vars_seq = NULL;
  int test_count;
  
  world = rasqal_new_world(); rasqal_world_open(world);
  
  query = rasqal_new_query(world, "sparql", NULL);
  
  vt = query->vars_table;

  for(test_count = 0; test_count < JOIN_TESTS_COUNT; test_count++) {
    rasqal_join_type join_type = join_test_config[test_count].join_type;
    int expected_count = join_test_config[test_count].expected;
    int vars_count;

    fprintf(stderr, "%s: test #%d  join type %d\n", program, test_count,
            RASQAL_GOOD_CAST(int, join_type));

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

    /* 3 variables and 2 rows */
    vars_count = 3;
    seq = rasqal_new_row_sequence(world, vt, join_2_data_3x2_rows, vars_count,
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

    rowsource = rasqal_new_join_rowsource(world, query, left_rs, right_rs,
                                          join_type, NULL);
    if(!rowsource) {
      fprintf(stderr, "%s: failed to create join rowsource\n", program);
      failures++;
      goto tidy;
    }
    /* left_rs and right_rs are now owned by rowsource */
    left_rs = right_rs = NULL;

    seq = rasqal_rowsource_read_all_rows(rowsource);
    if(!seq) {
      fprintf(stderr,
              "%s: read_rows returned a NULL seq for a join rowsource\n",
              program);
      failures++;
      goto tidy;
    }
    count = raptor_sequence_size(seq);
    if(count != expected_count) {
      fprintf(stderr,
              "%s: read_rows returned %d rows for a join rowsource, expected %d\n",
              program, count, expected_count);
      failures++;
      goto tidy;
    }

    size = rasqal_rowsource_get_size(rowsource);
    if(size != expected_size) {
      fprintf(stderr,
              "%s: read_rows returned %d columns (variables) for a join rowsource, expected %d\n",
              program, size, expected_size);
      failures++;
      goto tidy;
    }
    for(i = 0; i < expected_size; i++) {
      rasqal_variable* v;
      const char* name = NULL;
      const char *expected_name = join_result_vars[i];

      v = rasqal_rowsource_get_variable_by_offset(rowsource, i);
      if(!v) {
        fprintf(stderr,
              "%s: read_rows had NULL column (variable) #%d expected %s\n",
                program, i, expected_name);
        failures++;
        goto tidy;
      }
      name = RASQAL_GOOD_CAST(const char*, v->name);
      if(strcmp(name, expected_name)) {
        fprintf(stderr,
              "%s: read_rows returned column (variable) #%d %s but expected %s\n",
                program, i, name, expected_name);
        failures++;
        goto tidy;
      }
    }

#ifdef RASQAL_DEBUG
    rasqal_rowsource_print_row_sequence(rowsource, seq, DEBUG_FH);
#endif

    raptor_free_sequence(seq); seq = NULL;
    rasqal_free_rowsource(rowsource); rowsource = NULL;
    
    /* end test_count loop */
  }
  
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

#endif /* STANDALONE */

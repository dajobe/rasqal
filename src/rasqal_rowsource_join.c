/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_join.c - Rasqal join rowsource class
 *
 * Copyright (C) 2008, David Beckett http://www.dajobe.org/
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

#define INIT_RIGHT  (0)
#define READ_RIGHT (1)
#define FINISHED   (2)

typedef struct 
{
  rasqal_query* query;

  rasqal_rowsource* left;
  rasqal_rowsource* right;

  /* current left row */
  rasqal_row *left_row;
  
  /* array to map right variables into output rows */
  int* right_map;

  /* 0 = reading from left rs, 1 = reading from right rs, 2 = finished */
  int state;

  int failed;

  /* row offset for read_row() */
  int offset;

  /* join type: 0 = left outer join */
  int join_type;
  
  /* join expression */
  rasqal_expression *expr;
} rasqal_join_rowsource_context;


static int
rasqal_join_rowsource_init(rasqal_rowsource* rowsource, void *user_data) 
{
  rasqal_join_rowsource_context* con;
  con = (rasqal_join_rowsource_context*)user_data;

  con->failed = 0;

  con->left_row  = rasqal_rowsource_read_row(con->left);
  if(!con->left_row) {
    con->state = FINISHED;
    return 1;
  }

  con->state = INIT_RIGHT;

  if(con->expr && rasqal_expression_is_constant(con->expr)) {
    rasqal_query *query;
    rasqal_literal* result;
    int bresult;
    
    query = con->query;
    result = rasqal_expression_evaluate(query, con->expr,
uery->compare_flags);

#ifdef RASQAL_DEBUG
    RASQAL_DEBUG1("join expression condition is constant: ");
    if(!result)
      fputs("type error", DEBUG_FH);
    else
      rasqal_literal_print(result, DEBUG_FH);
    fputc('\n', DEBUG_FH);
#endif
    if(!result) {
      bresult = 0;
    } else {
      int error = 0;
      bresult = rasqal_literal_as_boolean(result, &error);
      if(error)
        RASQAL_DEBUG1("join boolean expression returned error\n");
#ifdef RASQAL_DEBUG
      else
        RASQAL_DEBUG2("join boolean expression result: %d\n",
result);
#endif
      rasqal_free_literal(result);
    }

    /* free expression always */
    rasqal_free_expression(con->expr); con->expr = NULL;

    if(!bresult) {
      /* Constraint is always false so row source is finished */
      con->state = 2;
    }
    /* otherwise always true so no need to evaluate on each row
     * and deleting con->expr will handle that
     */

  }

  return 0;
}


static int
rasqal_join_rowsource_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_join_rowsource_context* con;
  con = (rasqal_join_rowsource_context*)user_data;
  if(con->left)
    rasqal_free_rowsource(con->left);
  
  if(con->right)
    rasqal_free_rowsource(con->right);
  
  if(con->right_map)
    RASQAL_FREE(int, con->right_map);
  
  if(con->expr)
    rasqal_free_expression(con->expr);
  
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
  con->right_map = (int*)RASQAL_MALLOC(int, sizeof(int) * map_size);
  if(!con->right_map)
    return 1;

  rowsource->size = 0;

  /* copy in variables from left rowsource */
  rasqal_rowsource_copy_variables(rowsource, con->left);
  
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

  row = rasqal_new_row_for_size(rowsource->size);
  if(!row)
    return NULL;

  row->rowsource = rowsource;
  row->offset = row->offset;

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
  rasqal_query *query;

  con = (rasqal_join_rowsource_context*)user_data;
  query = con->query;

  if(con->failed || con->state == FINISHED)
    return NULL;

  while(1) {
    rasqal_row *right_row;
    rasqal_literal *result;
    int bresult;
    
    if(con->state == INIT_RIGHT) {
      if(!con->left_row) {
        con->state = FINISHED;
        return NULL;
      }

      rasqal_rowsource_reset(con->right);
      right_row  = rasqal_rowsource_read_row(con->right);
      row = rasqal_join_rowsource_build_merged_row(rowsource, con, right_row);
      if(right_row) {
        con->state = READ_RIGHT;
      } else {
        /* con->state = INIT_RIGHT; */
        if(row)
          con->left_row = rasqal_rowsource_read_row(con->left);
      }
      if(row)
        goto have_row;
    }

    /* else state is READ_RIGHT */

    right_row = rasqal_rowsource_read_row(con->right);
    if(!right_row) {
      /* right table done, restart left, continue looping */
      con->state = INIT_RIGHT;
      con->left_row = rasqal_rowsource_read_row(con->left);
      continue;
    }

    /* con->state = READ_RIGHT; */
    row = rasqal_join_rowsource_build_merged_row(rowsource, con, right_row);

    have_row:
    if(!con->expr)
      break;

    result = rasqal_expression_evaluate(query, con->expr, query->compare_flags);
#ifdef RASQAL_DEBUG
    RASQAL_DEBUG1("join expression result:\n");
    if(!result)
      fputs("type error", DEBUG_FH);
    else
      rasqal_literal_print(result, DEBUG_FH);
    fputc('\n', DEBUG_FH);
#endif
    if(!result) {
      bresult = 0;
    } else {
      int error = 0;
      bresult = rasqal_literal_as_boolean(result, &error);
      if(error)
        RASQAL_DEBUG1("filter boolean expression returned error\n");
#ifdef RASQAL_DEBUG
      else
        RASQAL_DEBUG2("filter boolean expression result: %d\n", bresult);
#endif
      rasqal_free_literal(result);
    }
    if(bresult)
      /* Constraint succeeded so return row */
      break;

    rasqal_free_row(row); row = NULL;
  }

  if(row) {
    row->rowsource = rowsource;
    row->offset = con->offset++;
  }
  
  return row;
}


static rasqal_query*
rasqal_join_rowsource_get_query(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_join_rowsource_context* con;
  con = (rasqal_join_rowsource_context*)user_data;
  return con->query;
}


static int
rasqal_join_rowsource_reset(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_join_rowsource_context* con;
  int rc;
  
  con = (rasqal_join_rowsource_context*)user_data;

  con->state = INIT_RIGHT;
  con->failed = 0;
  
  rc = rasqal_rowsource_reset(con->left);
  if(rc)
    return rc;

  return rasqal_rowsource_reset(con->right);
}


static const rasqal_rowsource_handler rasqal_join_rowsource_handler = {
  /* .version = */ 1,
  "join",
  /* .init = */ rasqal_join_rowsource_init,
  /* .finish = */ rasqal_join_rowsource_finish,
  /* .ensure_variables = */ rasqal_join_rowsource_ensure_variables,
  /* .read_row = */ rasqal_join_rowsource_read_row,
  /* .read_all_rows = */ NULL,
  /* .get_query = */ rasqal_join_rowsource_get_query,
  /* .reset = */ rasqal_join_rowsource_reset
};


/**
 * rasqal_new_join_rowsource:
 * @query: query results object
 * @left: left (first) rowsource
 * @right: right (second) rowsource
 * @join_type: 0 = left outer join
 * @expr: join expression to filter result rows
 *
 * INTERNAL - create a new JOIN over two rowsources
 *
 * This uses the number of variables in @vt to set the rowsource size
 * (order size is always 0) and then checks that all the rows in the
 * sequence are the same.  If not, construction fails and NULL is
 * returned.
 *
 * Return value: new rowsource or NULL on failure
 */
rasqal_rowsource*
rasqal_new_join_rowsource(rasqal_query* query,
                          rasqal_rowsource* left,
                          rasqal_rowsource* right,
                          int join_type,
                          rasqal_expression *expr)
{
  rasqal_join_rowsource_context* con;
  int flags = 0;

  if(!query || !left || !right)
    return NULL;

  /* only left outer join now */
  if(join_type != 0)
    return NULL;
  
  con = (rasqal_join_rowsource_context*)RASQAL_CALLOC(rasqal_join_rowsource_context, 1, sizeof(rasqal_join_rowsource_context));
  if(!con)
    return NULL;

  con->query = query;
  con->left = left;
  con->right = right;
  con->join_type = join_type;
  con->expr = expr;
  
  return rasqal_new_rowsource_from_handler(con,
                                           &rasqal_join_rowsource_handler,
                                           query->vars_table,
                                           flags);
}


#endif



#ifdef STANDALONE

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
  /* row 3 data */
  "bob", NULL, "sue", NULL,
  /* end of data */
  NULL, NULL
};
  

const char* const join_2_data_3x4_rows[] =
{
  /* 3 variable names and 4 rows */
  "b",     NULL, "c",      NULL, "d",      NULL,
  /* row 1 data */
  "red",   NULL, "orange", NULL, "yellow", NULL,
  /* row 2 data */
  "blue",  NULL, "indigo", NULL, "violet", NULL,
  /* row 3 data */
  "black", NULL, "silver", NULL, "gold",   NULL,
  /* row 4 data */
  "green", NULL, "tope",   NULL, "bronze", NULL,
  /* end of data */
  NULL, NULL
};


#define EXPECTED_ROWS_COUNT (3 * 4)

/* there is one duplicate variable 'b' */
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
  int vars_count;
  rasqal_variables_table* vt;
  int size;
  int expected_count = EXPECTED_ROWS_COUNT;
  int expected_size = EXPECTED_COLUMNS_COUNT;
  int i;
  raptor_sequence* vars_seq = NULL;
  
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

  left_rs = rasqal_new_rowsequence_rowsource(query, vt, seq, vars_seq);
  if(!left_rs) {
    fprintf(stderr, "%s: failed to create left rowsource\n", program);
    failures++;
    goto tidy;
  }
  /* vars_seq and seq are now owned by left_rs */
  vars_seq = seq = NULL;
  
  /* 3 variables and 4 rows */
  vars_count = 3;
  seq = rasqal_new_row_sequence(world, vt, join_2_data_3x4_rows, vars_count,
                                &vars_seq);
  if(!seq) {
    fprintf(stderr,
            "%s: failed to create right sequence of %d rows\n", program,
            vars_count);
    failures++;
    goto tidy;
  }

  right_rs = rasqal_new_rowsequence_rowsource(query, vt, seq, vars_seq);
  if(!right_rs) {
    fprintf(stderr, "%s: failed to create right rowsource\n", program);
    failures++;
    goto tidy;
  }
  /* vars_seq and seq are now owned by right_rs */
  vars_seq = seq = NULL;

  rowsource = rasqal_new_join_rowsource(query, left_rs, right_rs, 0, NULL);
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
    name = (const char*)v->name;
    if(strcmp(name, expected_name)) {
      fprintf(stderr,
            "%s: read_rows returned column (variable) #%d %s but expected %s\n",
              program, i, name, expected_name);
      failures++;
      goto tidy;
    }
  }
  
  
  if(rasqal_rowsource_get_query(rowsource) != query) {
    fprintf(stderr,
            "%s: get_query returned a different query for a join rowsurce\n",
            program);
    failures++;
    goto tidy;
  }

#ifdef RASQAL_DEBUG
  rasqal_rowsource_print_row_sequence(rowsource, seq, DEBUG_FH);
#endif

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

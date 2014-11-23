/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_union.c - Rasqal union rowsource class
 *
 * Copyright (C) 2008-2009, David Beckett http://www.dajobe.org/
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

typedef struct 
{
  rasqal_rowsource* left;

  rasqal_rowsource* right;

  /* array of size (number of variables in @right) with this row offset value */
  int* right_map;

  /* array of size (number of variables in @right) holding right row temporarily */
  rasqal_literal** right_tmp_values;

  /* 0 = reading from left rs, 1 = reading from right rs, 2 = finished */
  int state;

  int failed;

  /* row offset for read_row() */
  int offset;
} rasqal_union_rowsource_context;


static int
rasqal_union_rowsource_init(rasqal_rowsource* rowsource, void *user_data) 
{
  rasqal_union_rowsource_context* con;

  con = (rasqal_union_rowsource_context*)user_data;
  con->state = 0;

  con->failed = 0;

  rasqal_rowsource_set_requirements(con->left, RASQAL_ROWSOURCE_REQUIRE_RESET);
  rasqal_rowsource_set_requirements(con->right, RASQAL_ROWSOURCE_REQUIRE_RESET);

  return 0;
}


static int
rasqal_union_rowsource_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_union_rowsource_context* con;
  con = (rasqal_union_rowsource_context*)user_data;
  if(con->left)
    rasqal_free_rowsource(con->left);
  
  if(con->right)
    rasqal_free_rowsource(con->right);
  
  if(con->right_map)
    RASQAL_FREE(int, con->right_map);
  
  if(con->right_tmp_values)
    RASQAL_FREE(ptrarray, con->right_tmp_values);
  
  RASQAL_FREE(rasqal_union_rowsource_context, con);

  return 0;
}


static int
rasqal_union_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                        void *user_data)
{
  rasqal_union_rowsource_context* con;
  int map_size;
  int i;
  
  con = (rasqal_union_rowsource_context*)user_data;

  if(rasqal_rowsource_ensure_variables(con->left))
    return 1;

  if(rasqal_rowsource_ensure_variables(con->right))
    return 1;

  map_size = rasqal_rowsource_get_size(con->right);
  con->right_map = RASQAL_MALLOC(int*, RASQAL_GOOD_CAST(size_t,
                                                        sizeof(int) * RASQAL_GOOD_CAST(size_t, map_size)));
  if(!con->right_map)
    return 1;

  con->right_tmp_values = RASQAL_MALLOC(rasqal_literal**,
                                        sizeof(rasqal_literal*) * RASQAL_GOOD_CAST(size_t, map_size));
  if(!con->right_tmp_values)
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


static void
rasqal_union_rowsource_adjust_right_row(rasqal_rowsource *rowsource,
                                        rasqal_union_rowsource_context* con,
                                        rasqal_row *row)
{
  rasqal_rowsource *right_rowsource = con->right;
  int i;

  /* save right row values */
  for(i = 0; i < right_rowsource->size; i++)
    con->right_tmp_values[i] = row->values[i];

  /* NULL out other pointers */
  for(i = 0; i < rowsource->size; i++)
    row->values[i] = NULL;

  /* map them into correct order in result row */
  for(i = 0; i < right_rowsource->size; i++) {
    int offset = con->right_map[i];
    row->values[offset] = con->right_tmp_values[i];
  }
}


static rasqal_row*
rasqal_union_rowsource_read_row(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_union_rowsource_context* con;
  rasqal_row* row = NULL;

  con = (rasqal_union_rowsource_context*)user_data;
  
  if(con->failed || con->state > 1)
    return NULL;

  if(con->state == 0) {
    row = rasqal_rowsource_read_row(con->left);
#ifdef RASQAL_DEBUG
    RASQAL_DEBUG2("rowsource %p read left row : ", rowsource);
    if(row)
      rasqal_row_print(row, stderr);
    else
      fputs("NONE", stderr);
    fputs("\n", stderr);
#endif

    if(!row)
      con->state = 1;
    else {
      /* otherwise: rows from left are correct order but wrong size */
      if(rasqal_row_expand_size(row, rowsource->size)) {
        rasqal_free_row(row);
        return NULL;
      }
    }
  }
  if(!row && con->state == 1) {
    row = rasqal_rowsource_read_row(con->right);
#ifdef RASQAL_DEBUG
    RASQAL_DEBUG2("rowsource %p read right row : ", rowsource);
    if(row)
      rasqal_row_print(row, stderr);
    else
      fputs("NONE", stderr);
    fputs("\n", stderr);
#endif

    if(!row)
      /* finished */
      con->state = 2;
    else {
      if(rasqal_row_expand_size(row, rowsource->size)) {
        rasqal_free_row(row);
        return NULL;
      }
      /* transform row from right to match new projection */
      rasqal_union_rowsource_adjust_right_row(rowsource, con, row);
    }
  }

  if(row) {
    rasqal_row_set_rowsource(row, rowsource);
    row->offset = con->offset++;
  }
  
  return row;
}


static raptor_sequence*
rasqal_union_rowsource_read_all_rows(rasqal_rowsource* rowsource,
                                     void *user_data)
{
  rasqal_union_rowsource_context* con;
  raptor_sequence* seq1 = NULL;
  raptor_sequence* seq2 = NULL;
  int left_size;
  int right_size;
  int i;
  
  con = (rasqal_union_rowsource_context*)user_data;

  if(con->failed)
    return NULL;
  
  seq1 = rasqal_rowsource_read_all_rows(con->left);
  if(!seq1) {
    con->failed = 1;
    return NULL;
  }

  seq2 = rasqal_rowsource_read_all_rows(con->right);
  if(!seq2) {
    con->failed = 1;
    raptor_free_sequence(seq1);
    return NULL;
  }

#ifdef RASQAL_DEBUG
  fprintf(DEBUG_FH, "left rowsource (%d vars):\n",
          rasqal_rowsource_get_size(con->left));
  rasqal_rowsource_print_row_sequence(con->left, seq1, DEBUG_FH);

  fprintf(DEBUG_FH, "right rowsource (%d vars):\n",
          rasqal_rowsource_get_size(con->right));
  rasqal_rowsource_print_row_sequence(con->right, seq2, DEBUG_FH);
#endif

  /* transform rows from left to match new projection */
  left_size = raptor_sequence_size(seq1);
  for(i = 0; i < left_size; i++) {
    rasqal_row *row = (rasqal_row*)raptor_sequence_get_at(seq1, i);
    /* rows from left are correct order but wrong size */
    rasqal_row_expand_size(row, rowsource->size);
    rasqal_row_set_rowsource(row, rowsource);
  }
  /* transform rows from right to match new projection */
  right_size = raptor_sequence_size(seq2);
  for(i = 0; i < right_size; i++) {
    rasqal_row *row = (rasqal_row*)raptor_sequence_get_at(seq2, i);
    /* rows from right need resizing and adjusting by offset */
    rasqal_row_expand_size(row, rowsource->size);
    rasqal_union_rowsource_adjust_right_row(rowsource, con, row);
    row->offset += left_size;
    rasqal_row_set_rowsource(row, rowsource);
  }

  if(raptor_sequence_join(seq1, seq2)) {
    raptor_free_sequence(seq1);
    seq1 = NULL;
  }
  raptor_free_sequence(seq2);
  
  con->state = 2;
  return seq1;
}


static int
rasqal_union_rowsource_reset(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_union_rowsource_context* con;
  int rc;
  
  con = (rasqal_union_rowsource_context*)user_data;

  con->state = 0;
  con->failed = 0;

  rc = rasqal_rowsource_reset(con->left);
  if(rc)
    return rc;

  return rasqal_rowsource_reset(con->right);
}


static rasqal_rowsource*
rasqal_union_rowsource_get_inner_rowsource(rasqal_rowsource* rowsource,
                                           void *user_data, int offset)
{
  rasqal_union_rowsource_context *con;
  con = (rasqal_union_rowsource_context*)user_data;

  if(offset == 0)
    return con->left;
  else if(offset == 1)
    return con->right;
  else
    return NULL;
}


static const rasqal_rowsource_handler rasqal_union_rowsource_handler = {
  /* .version = */ 1,
  "union",
  /* .init = */ rasqal_union_rowsource_init,
  /* .finish = */ rasqal_union_rowsource_finish,
  /* .ensure_variables = */ rasqal_union_rowsource_ensure_variables,
  /* .read_row = */ rasqal_union_rowsource_read_row,
  /* .read_all_rows = */ rasqal_union_rowsource_read_all_rows,
  /* .reset = */ rasqal_union_rowsource_reset,
  /* .set_requirements = */ NULL,
  /* .get_inner_rowsource = */ rasqal_union_rowsource_get_inner_rowsource,
  /* .set_origin = */ NULL,
};


/**
 * rasqal_new_union_rowsource:
 * @world: world object
 * @query: query object
 * @left: left (first) rowsource
 * @right: right (second) rowsource
 *
 * INTERNAL - create a new UNION over two rowsources
 *
 * This uses the number of variables in @vt to set the rowsource size
 * (order size is always 0) and then checks that all the rows in the
 * sequence are the same.  If not, construction fails and NULL is
 * returned.
 *
 * The @left and @right rowsources become owned by the new rowsource.
 *
 * Return value: new rowsource or NULL on failure
 */
rasqal_rowsource*
rasqal_new_union_rowsource(rasqal_world *world,
                           rasqal_query* query,
                           rasqal_rowsource* left,
                           rasqal_rowsource* right)
{
  rasqal_union_rowsource_context* con;
  int flags = 0;

  if(!world || !query || !left || !right)
    goto fail;
  
  con = RASQAL_CALLOC(rasqal_union_rowsource_context*, 1, sizeof(*con));
  if(!con)
    goto fail;

  con->left = left;
  con->right = right;
  
  return rasqal_new_rowsource_from_handler(world, query,
                                           con,
                                           &rasqal_union_rowsource_handler,
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


const char* const union_1_data_2x3_rows[] =
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
  

const char* const union_2_data_3x4_rows[] =
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
  NULL, NULL, NULL, NULL, NULL, NULL
};


#define EXPECTED_ROWS_COUNT (3 + 4)

/* there is one duplicate variable 'b' */
#define EXPECTED_COLUMNS_COUNT (2 + 3 - 1)
const char* const union_result_vars[] = { "a" , "b" , "c", "d" };


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
  seq = rasqal_new_row_sequence(world, vt, union_1_data_2x3_rows, vars_count,
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
  seq = rasqal_new_row_sequence(world, vt, union_2_data_3x4_rows, vars_count,
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

  rowsource = rasqal_new_union_rowsource(world, query, left_rs, right_rs);
  if(!rowsource) {
    fprintf(stderr, "%s: failed to create union rowsource\n", program);
    failures++;
    goto tidy;
  }
  /* left_rs and right_rs are now owned by rowsource */
  left_rs = right_rs = NULL;

  seq = rasqal_rowsource_read_all_rows(rowsource);
  if(!seq) {
    fprintf(stderr,
            "%s: read_rows returned a NULL seq for a union rowsource\n",
            program);
    failures++;
    goto tidy;
  }
  count = raptor_sequence_size(seq);
  if(count != expected_count) {
    fprintf(stderr,
            "%s: read_rows returned %d rows for a union rowsource, expected %d\n",
            program, count, expected_count);
    failures++;
    goto tidy;
  }
  
  size = rasqal_rowsource_get_size(rowsource);
  if(size != expected_size) {
    fprintf(stderr,
            "%s: read_rows returned %d columns (variables) for a union rowsource, expected %d\n",
            program, size, expected_size);
    failures++;
    goto tidy;
  }
  for(i = 0; i < expected_size; i++) {
    rasqal_variable* v;
    const char* name = NULL;
    const char *expected_name = union_result_vars[i];
    
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

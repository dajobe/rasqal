/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_union.c - Rasqal union rowsource class
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


#ifndef STANDALONE

typedef struct 
{
  rasqal_query* query;

  rasqal_rowsource* left;
  rasqal_rowsource* right;

  /* 0 = reading from left rs, 1 = reading from right rs, 2 = finished */
  int state;

  int failed;
} rasqal_union_rowsource_context;


static int
rasqal_union_rowsource_init(rasqal_rowsource* rowsource, void *user_data) 
{
  rasqal_union_rowsource_context* con;
  con = (rasqal_union_rowsource_context*)user_data;
  con->state = 0;

  con->failed = 0;
  
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
  
  RASQAL_FREE(rasqal_union_rowsource_context, con);

  return 0;
}


static int
rasqal_union_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                        void *user_data)
{
  rasqal_union_rowsource_context* con;

  con = (rasqal_union_rowsource_context*)user_data;

  if(rasqal_rowsource_ensure_variables(con->left))
    return 1;

  if(rasqal_rowsource_ensure_variables(con->right))
    return 1;

  rowsource->size = 0;

  /* copy in variables from left rowsource */
  rasqal_rowsource_copy_variables(rowsource, con->left);
  
  /* add any new ones not already seen from right rowsource */
  rasqal_rowsource_copy_variables(rowsource, con->right);

  return 0;
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
    if(!row)
      con->state = 1;
  }
  if(!row && con->state == 1) {
    row= rasqal_rowsource_read_row(con->right);
    if(!row)
      /* finished */
      con->state = 2;
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

  if(raptor_sequence_join(seq1, seq2)) {
    raptor_free_sequence(seq1);
    seq1 = NULL;
  }
  raptor_free_sequence(seq2);
  
  con->state = 2;
  return seq1;
}


static rasqal_query*
rasqal_union_rowsource_get_query(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_union_rowsource_context* con;
  con = (rasqal_union_rowsource_context*)user_data;
  return con->query;
}


static const rasqal_rowsource_handler rasqal_union_rowsource_handler = {
  /* .version = */ 1,
  "union",
  /* .init = */ rasqal_union_rowsource_init,
  /* .finish = */ rasqal_union_rowsource_finish,
  /* .ensure_variables = */ rasqal_union_rowsource_ensure_variables,
  /* .read_row = */ rasqal_union_rowsource_read_row,
  /* .read_all_rows = */ rasqal_union_rowsource_read_all_rows,
  /* .get_query = */ rasqal_union_rowsource_get_query
};


/**
 * rasqal_new_union_rowsource:
 * @query: query results object
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
 * Return value: new rowsource or NULL on failure
 */
rasqal_rowsource*
rasqal_new_union_rowsource(rasqal_query* query,
                           rasqal_rowsource* left,
                           rasqal_rowsource* right)
{
  rasqal_union_rowsource_context* con;
  int flags = 0;

  if(!query || !left || !right)
    return NULL;
  
  con = (rasqal_union_rowsource_context*)RASQAL_CALLOC(rasqal_union_rowsource_context, 1, sizeof(rasqal_union_rowsource_context));
  if(!con)
    return NULL;

  con->query = query;
  con->left = left;
  con->right = right;
  
  return rasqal_new_rowsource_from_handler(con,
                                           &rasqal_union_rowsource_handler,
                                           query->vars_table,
                                           flags);
}


#endif



#ifdef STANDALONE

/* one more prototype */
int main(int argc, char *argv[]);


const char* const union_1_data_2x2_rows[] =
{
  /* 2 variable names and 2 rows */
  "a",   NULL, "b",   NULL,
  /* row 1 data */
  "foo", NULL, "bar", NULL,
  /* row 2 data */
  "baz", NULL, "fez", NULL,
  /* end of data */
  NULL, NULL
};
  

const char* const union_2_data_3x3_rows[] =
{
  /* 3 variable names and 3 rows */
  "b",     NULL, "c",      NULL, "d",      NULL,
  /* row 1 data */
  "red",   NULL, "orange", NULL, "yellow", NULL,
  /* row 2 data */
  "blue",  NULL, "indigo", NULL, "violet", NULL,
  /* row 3 data */
  "black", NULL, "silver", NULL, "gold",   NULL,
  /* end of data */
  NULL, NULL
};


#define EXPECTED_ROWS_COUNT (2+3)  
/* there is one duplicate variable 'b' */
#define EXPECTED_COLUMNS_COUNT (2+3-1)

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
  int rows_count;
  rasqal_variables_table* vt;
  int size;
  int expected_count = EXPECTED_ROWS_COUNT;
  int expected_size = EXPECTED_COLUMNS_COUNT;
  
  world = rasqal_new_world(); rasqal_world_open(world);
  
  query = rasqal_new_query(world, "sparql", NULL);
  
  vt = query->vars_table;

  /* 2 variables and 2 rows */
  rows_count = 2;
  seq = rasqal_new_row_sequence(world, vt, union_1_data_2x2_rows, rows_count);
  if(!seq) {
    fprintf(stderr,
            "%s: failed to create left sequence of %d rows\n", program,
            rows_count);
    failures++;
    goto tidy;
  }

  left_rs = rasqal_new_rowsequence_rowsource(query, vt, seq);
  if(!left_rs) {
    fprintf(stderr, "%s: failed to create left rowsource\n", program);
    failures++;
    goto tidy;
  }
  /* seq is now owned by left_rs */
  seq = NULL;

  /* 3 variables and 3 rows */
  rows_count = 3;
  seq = rasqal_new_row_sequence(world, vt, union_2_data_3x3_rows, rows_count);
  if(!seq) {
    fprintf(stderr,
            "%s: failed to create right sequence of %d rows\n", program,
            rows_count);
    failures++;
    goto tidy;
  }

  right_rs = rasqal_new_rowsequence_rowsource(query, vt, seq);
  if(!right_rs) {
    fprintf(stderr, "%s: failed to create right rowsource\n", program);
    failures++;
    goto tidy;
  }
  /* seq is now owned by right_rs */
  seq = NULL;

  rowsource = rasqal_new_union_rowsource(query, left_rs, right_rs);
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
  
  if(rasqal_rowsource_get_query(rowsource) != query) {
    fprintf(stderr,
            "%s: get_query returned a different query for a union rowsurce\n",
            program);
    failures++;
    goto tidy;
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

#endif

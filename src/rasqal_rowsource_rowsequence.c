/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_rowsequence.c - Rasqal rowsequence rowsource class
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


#ifndef STANDALONE

typedef struct 
{
  raptor_sequence* seq;

  /* variables for this rowsource */
  raptor_sequence* vars_seq;
  
  /* index into seq or <0 when finished */
  int offset;

  int failed;
} rasqal_rowsequence_rowsource_context;


static int
rasqal_rowsequence_rowsource_init(rasqal_rowsource* rowsource, void *user_data) 
{
  rasqal_rowsequence_rowsource_context* con;
  int rows_count;
  int i;
  
  con = (rasqal_rowsequence_rowsource_context*)user_data;
  con->offset = 0;

  con->failed = 0;
  
  /* adjust offset of every row */
  rows_count = raptor_sequence_size(con->seq);
  for(i = 0; i < rows_count; i++) {
    rasqal_row* row;
    row = (rasqal_row*)raptor_sequence_get_at(con->seq, i);
    
    rasqal_row_set_weak_rowsource(row, rowsource);
    row->offset = i;
    
  }

  return 0;
}


static int
rasqal_rowsequence_rowsource_finish(rasqal_rowsource* rowsource,
                                    void *user_data)
{
  rasqal_rowsequence_rowsource_context* con;

  con = (rasqal_rowsequence_rowsource_context*)user_data;

  if(con->seq) {
    int rows_count;
    int i;

    rows_count = raptor_sequence_size(con->seq);
    for(i = 0; i < rows_count; i++) {
      rasqal_row* row;
      row = (rasqal_row*)raptor_sequence_get_at(con->seq, i);
      rasqal_row_set_weak_rowsource(row, NULL);
    }

    raptor_free_sequence(con->seq);
  }

  if(con->vars_seq)
    raptor_free_sequence(con->vars_seq);
  
  RASQAL_FREE(rasqal_rowsequence_rowsource_context, con);

  return 0;
}


static int
rasqal_rowsequence_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                              void *user_data)
{
  rasqal_rowsequence_rowsource_context* con;
  int i;
  
  con = (rasqal_rowsequence_rowsource_context*)user_data;

  rowsource->size = 0;
  for(i = 0; i < raptor_sequence_size(con->vars_seq); i++) {
    rasqal_variable* v;
    v = (rasqal_variable*)raptor_sequence_get_at(con->vars_seq, i);
    rasqal_rowsource_add_variable(rowsource, v);
  }
  
  raptor_free_sequence(con->vars_seq);
  con->vars_seq = NULL;
  
  return 0;
}


static rasqal_row*
rasqal_rowsequence_rowsource_read_row(rasqal_rowsource* rowsource,
                                      void *user_data)
{
  rasqal_rowsequence_rowsource_context* con;
  rasqal_row* row = NULL;
  
  con = (rasqal_rowsequence_rowsource_context*)user_data;
  if(con->failed || con->offset < 0)
    return NULL;

  row = (rasqal_row*)raptor_sequence_get_at(con->seq, con->offset);
  if(!row) {
    /* finished */
    con->offset = -1;
  } else {
    row = rasqal_new_row_from_row(row);
    con->offset++;
  }

  return row;
}


static raptor_sequence*
rasqal_rowsequence_rowsource_read_all_rows(rasqal_rowsource* rowsource,
                                           void *user_data)
{
  rasqal_rowsequence_rowsource_context* con;
  raptor_sequence* seq;
  
  con = (rasqal_rowsequence_rowsource_context*)user_data;
  if(con->offset < 0)
    return NULL;

  seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_row,
                            (raptor_data_print_handler)rasqal_row_print);
  if(seq) {
    int i;
    int size = raptor_sequence_size(con->seq);
    for(i = 0; i < size; i++) {
      rasqal_row *row = (rasqal_row*)raptor_sequence_get_at(con->seq, i);
      raptor_sequence_push(seq, rasqal_new_row_from_row(row));
    }
  }
  
  return seq;
}


static int
rasqal_rowsequence_rowsource_reset(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_rowsequence_rowsource_context* con;

  con = (rasqal_rowsequence_rowsource_context*)user_data;

  con->offset = 0;

  return 0;
}


static const rasqal_rowsource_handler rasqal_rowsequence_rowsource_handler = {
  /* .version = */ 1,
  "rowsequence",
  /* .init = */ rasqal_rowsequence_rowsource_init,
  /* .finish = */ rasqal_rowsequence_rowsource_finish,
  /* .ensure_variables = */ rasqal_rowsequence_rowsource_ensure_variables,
  /* .read_row = */ rasqal_rowsequence_rowsource_read_row,
  /* .read_all_rows = */ rasqal_rowsequence_rowsource_read_all_rows,
  /* .reset = */ rasqal_rowsequence_rowsource_reset,
  /* .set_requirements = */ NULL,
  /* .get_inner_rowsource = */ NULL,
  /* .set_origin = */ NULL,
};


/**
 * rasqal_new_rowsequence_rowsource:
 * @world: world object
 * @query: query object
 * @vt: variables table
 * @rows_seq: input sequence of #rasqal_row
 * @vars_seq: input sequence of #rasqal_variable for all rows in @rows_seq
 *
 * INTERNAL - create a new rowsource over a sequence of rows with given variables
 *
 * This uses the number of variables in @vt to set the rowsource size
 * (order size is always 0) and then checks that all the rows in the
 * sequence are the same.  If not, construction fails and NULL is
 * returned.
 *
 * The @rows_seq and @vars_seq become owned by the new rowsource.
 *
 * Return value: new rowsource or NULL on failure
 */
rasqal_rowsource*
rasqal_new_rowsequence_rowsource(rasqal_world *world,
                                 rasqal_query* query, 
                                 rasqal_variables_table* vt,
                                 raptor_sequence* rows_seq,
                                 raptor_sequence* vars_seq)
{
  rasqal_rowsequence_rowsource_context* con;
  int flags = 0;
  
  if(!world || !query || !vt || !vars_seq)
    return NULL;

  if(!raptor_sequence_size(vars_seq))
    return NULL;
  
  con = RASQAL_CALLOC(rasqal_rowsequence_rowsource_context*, 1, sizeof(*con));
  if(!con)
    return NULL;

  con->seq = rows_seq;
  con->vars_seq = vars_seq;

  return rasqal_new_rowsource_from_handler(world, query,
                                           con,
                                           &rasqal_rowsequence_rowsource_handler,
                                           vt,
                                           flags);
}


#endif /* not STANDALONE */



#ifdef STANDALONE


const char* const test_1_rows[] =
{
  /* 2 variable names */
  "a",   NULL, "b",   NULL,
  /* row 1 data */
  "foo", NULL, "bar", NULL,
  /* end of data */
  NULL, NULL, NULL, NULL
};
  

const char* const test_3_rows[] =
{
  /* 4 variable names */
  "c1",    NULL, "c2",     NULL, "c3",     NULL, "c4",       NULL,
  /* row 1 data */
  "red",   NULL, "orange", NULL, "yellow", NULL, "green",    NULL,
  /* row 2 data */
  "blue",  NULL, "indigo", NULL, "violet", NULL, "white",    NULL,
  /* row 3 data */
  "black", NULL, "silver", NULL, "gold",   NULL, "platinum", NULL,
  /* end of data */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};
  

/* one more prototype */
int main(int argc, char *argv[]);

int
main(int argc, char *argv[]) 
{
  const char *program = rasqal_basename(argv[0]);
  rasqal_rowsource *rowsource = NULL;
  raptor_sequence *seq = NULL;
  rasqal_world* world = NULL;
  rasqal_query* query = NULL;
  rasqal_row* row = NULL;
  int count;
  int failures = 0;
  rasqal_variables_table* vt;
  int rows_count;
  int i;
  raptor_sequence* vars_seq = NULL;
  
  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    return(1);
  }
  
  query = rasqal_new_query(world, "sparql", NULL);
  
  /* test 1-row rowsource (2 variables) */
  rows_count = 1;
  
#ifdef RASQAL_DEBUG  
  RASQAL_DEBUG2("Testing %d-row rowsource\n", rows_count);
#endif

  vt = rasqal_new_variables_table(world);

  /* add 2 variables to table and 1 row sequence */
  seq = rasqal_new_row_sequence(world, vt, test_1_rows, 2, &vars_seq);
  if(!seq) {
    fprintf(stderr, "%s: failed to create sequence of %d rows\n", program,
            rows_count);
    failures++;
    goto tidy;
  }

  rowsource = rasqal_new_rowsequence_rowsource(world, query, vt, seq, vars_seq);
  if(!rowsource) {
    fprintf(stderr, "%s: failed to create %d-row sequence rowsource\n",
            program, rows_count);
    failures++;
    goto tidy;
  }
  /* vars_seq and seq are now owned by rowsource */
  vars_seq = seq = NULL;

  row = rasqal_rowsource_read_row(rowsource);
  if(!row) {
    fprintf(stderr,
            "%s: read_row returned no row for a %d-row sequence rowsource\n",
            program, rows_count);
    failures++;
    goto tidy;
  }

#ifdef RASQAL_DEBUG  
  RASQAL_DEBUG1("Result Row:\n  ");
  rasqal_row_print(row, stderr);
  fputc('\n', stderr);
#endif

  rasqal_free_row(row); row = NULL;

  count = rasqal_rowsource_get_rows_count(rowsource);
  if(count != rows_count) {
    fprintf(stderr,
            "%s: read_rows returned count %d instead of %d for a %d-row sequence rowsource\n",
            program, count, rows_count, rows_count);
    failures++;
    goto tidy;
  }

  row = rasqal_rowsource_read_row(rowsource);
  if(row) {
    fprintf(stderr,
            "%s: read_row returned > %d rows for a %d-row sequence rowsource\n",
            program, rows_count, rows_count);
    failures++;
    goto tidy;
  }
  

  rasqal_free_rowsource(rowsource); rowsource = NULL;
  rasqal_free_variables_table(vt); vt = NULL;

  /* test 3-row rowsource */
  rows_count = 3;

#ifdef RASQAL_DEBUG  
  RASQAL_DEBUG2("Testing %d-row rowsource\n", rows_count);
#endif

  vt = rasqal_new_variables_table(world);

  /* add 4 variables to table and 3 row sequence */
  seq = rasqal_new_row_sequence(world, vt, test_3_rows, 4, &vars_seq);
  if(!seq) {
    fprintf(stderr, "%s: failed to create sequence of %d rows\n",
            program, rows_count);
    failures++;
    goto tidy;
  }

  rowsource = rasqal_new_rowsequence_rowsource(world, query, vt, seq, vars_seq);
  if(!rowsource) {
    fprintf(stderr, "%s: failed to create %d-row sequence rowsource\n",
            program, rows_count);
    failures++;
    goto tidy;
  }
  /* vars_seq and seq are now owned by rowsource */
  vars_seq = seq = NULL;

  for(i = 0; i < rows_count; i++) {
    row = rasqal_rowsource_read_row(rowsource);
    if(!row) {
      fprintf(stderr,
              "%s: read_row returned no row for row %d in a %d-row sequence rowsource\n",
              program, i, rows_count);
      failures++;
      goto tidy;
    }

  #ifdef RASQAL_DEBUG  
    RASQAL_DEBUG1("Result Row:\n  ");
    rasqal_row_print(row, stderr);
    fputc('\n', stderr);
  #endif

    rasqal_free_row(row); row = NULL;
  }
  
  count = rasqal_rowsource_get_rows_count(rowsource);
  if(count != rows_count) {
    fprintf(stderr,
            "%s: read_rows returned count %d instead of %d for a %d-row sequence rowsource\n",
            program, count, rows_count, rows_count);
    failures++;
    goto tidy;
  }

  row = rasqal_rowsource_read_row(rowsource);
  if(row) {
    fprintf(stderr,
            "%s: read_row returned >%d rows for a %d-row sequence rowsource\n",
            program, rows_count, rows_count);
    failures++;
    goto tidy;
  }
  
  rasqal_free_rowsource(rowsource); rowsource = NULL;
  rasqal_free_variables_table(vt); vt = NULL;


  tidy:
  if(row)
    rasqal_free_row(row);
  if(seq)
    raptor_free_sequence(seq);
  if(rowsource)
    rasqal_free_rowsource(rowsource);
  if(vt)
    rasqal_free_variables_table(vt);
  if(query)
    rasqal_free_query(query);
  if(world)
    rasqal_free_world(world);

  return failures;
}

#endif /* STANDALONE */

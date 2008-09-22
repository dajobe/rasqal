/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_rowsequence.c - Rasqal row sequence rowsource class
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

  raptor_sequence* seq;

  /* index into seq or <0 when finished */
  int offset;

  int failed;
} rasqal_rowsequence_rowsource_context;


static int
rasqal_rowsequence_rowsource_init(rasqal_rowsource* rowsource, void *user_data) 
{
  rasqal_rowsequence_rowsource_context* con;

  con = (rasqal_rowsequence_rowsource_context*)user_data;
  con->offset = 0;

  con->failed = 0;
  
  return 0;
}


static int
rasqal_rowsequence_rowsource_finish(rasqal_rowsource* rowsource,
                                    void *user_data)
{
  rasqal_rowsequence_rowsource_context* con;

  con = (rasqal_rowsequence_rowsource_context*)user_data;
  if(con->seq)
    raptor_free_sequence(con->seq);
  
  RASQAL_FREE(rasqal_rowsequence_rowsource_context, con);

  return 0;
}


static int
rasqal_rowsequence_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                              void *user_data)
{
  /*
  rasqal_rowsequence_rowsource_context* con;
  con = (rasqal_rowsequence_rowsource_context*)user_data; 
  */
  return 0;
}


static rasqal_query_result_row*
rasqal_rowsequence_rowsource_read_row(rasqal_rowsource* rowsource,
                                      void *user_data)
{
  rasqal_rowsequence_rowsource_context* con;
  rasqal_query_result_row* row = NULL;
  
  con = (rasqal_rowsequence_rowsource_context*)user_data;
  if(con->failed || con->offset < 0)
    return NULL;

  row = (rasqal_query_result_row*)raptor_sequence_delete_at(con->seq, con->offset);
  if(!row) {
    /* finished */
    con->offset = -1;
  } else
    con->offset++;

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

  seq = con->seq;
  con->seq = NULL;
  
  return seq;
}


static rasqal_query*
rasqal_rowsequence_rowsource_get_query(rasqal_rowsource* rowsource,
                                       void *user_data)
{
  rasqal_rowsequence_rowsource_context* con;

  con = (rasqal_rowsequence_rowsource_context*)user_data;
  return con->query;
}


static const rasqal_rowsource_handler rasqal_rowsequence_rowsource_handler = {
  /* .version = */ 1,
  /* .init = */ rasqal_rowsequence_rowsource_init,
  /* .finish = */ rasqal_rowsequence_rowsource_finish,
  /* .ensure_variables = */ rasqal_rowsequence_rowsource_ensure_variables,
  /* .read_row = */ rasqal_rowsequence_rowsource_read_row,
  /* .read_all_rows = */ rasqal_rowsequence_rowsource_read_all_rows,
  /* .get_query = */ rasqal_rowsequence_rowsource_get_query
};


/**
 * rasqal_new_rowsequence_rowsource:
 * @query: query object
 * @vt: variables table
 * @seq: sequence of rasqal_query_result_row*
 *
 * INTERNAL - create a new rowsource over a sequence of rows
 *
 * This uses the number of variables in @vt to set the rowsource size
 * (order size is always 0) and then checks that all the rows in the
 * sequence are the same.  If not, construction fails and NULL is
 * returned.
 *
 * Return value: new rowsource or NULL on failure
 */
rasqal_rowsource*
rasqal_new_rowsequence_rowsource(rasqal_query* query, 
                                 rasqal_variables_table* vt,
                                 raptor_sequence* seq)
{
  rasqal_rowsequence_rowsource_context* con;
  int flags = 0;
  rasqal_rowsource* rs;
  int failed = 0;
  
  if(!query || !vt || !seq)
    return NULL;
  
  con = (rasqal_rowsequence_rowsource_context*)RASQAL_CALLOC(rasqal_rowsequence_rowsource_context, 1, sizeof(rasqal_rowsequence_rowsource_context));
  if(!con)
    return NULL;

  con->query = query;
  con->seq = seq;

  rs = rasqal_new_rowsource_from_handler(con,
                                         &rasqal_rowsequence_rowsource_handler,
                                         flags);

  if(rs) {
    int i;
    int rows_count;
    
    for(i = 0; 1; i++) {
      rasqal_variable* v = rasqal_variables_table_get(vt, i);
      if(!v)
        break;
      v = rasqal_new_variable_from_variable(v);
      rasqal_rowsource_add_variable(rs, v);
    }
    rs->size = i;
    rs->order_size = 0;

    rows_count = raptor_sequence_size(seq);
    for(i = 0; i < rows_count; i++) {
      rasqal_query_result_row* row;
      row = (rasqal_query_result_row*)raptor_sequence_get_at(seq, i);

      row->rowsource = rs;
      row->offset = i;
      
      if(row->size != rs->size) {
        RASQAL_DEBUG4("Row %d size %d is different from rowsource size %d",
                      i, row->size, rs->size);
        failed = 1;
        break;
      }
      if(row->order_size != rs->order_size) {
        RASQAL_DEBUG4("Row %d order size %d is different from rowsource order size %d",
                      i, row->order_size, rs->order_size);
        failed = 1;
        break;
      }
    }
  }

  if(failed) {
    rasqal_free_rowsource(rs);
    rs = NULL;
  }
  return rs;
}


#endif



#ifdef STANDALONE


const char* const test_1_rows[] =
{
  /* 2 variable names */
  "a",   NULL, "b",   NULL,
  /* row 1 data */
  "foo", NULL, "bar", NULL,
  /* end of data */
  NULL, NULL
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
  NULL, NULL
};
  

/* one more prototype */
int main(int argc, char *argv[]);

int
main(int argc, char *argv[]) 
{
  const char *program = rasqal_basename(argv[0]);
  rasqal_rowsource *rowsource = NULL;
  raptor_sequence *seq = NULL;
  rasqal_world* fake_world = (rasqal_world*)-1;
  rasqal_query* fake_query = (rasqal_query*)-1;
  rasqal_query_result_row* row = NULL;
  int count;
  int failures = 0;
  rasqal_variables_table* vt;
  int rows_count;
  int i;
  
  /* test 1-row rowsource (2 variables) */
  rows_count = 1;
  
#ifdef RASQAL_DEBUG  
  RASQAL_DEBUG2("Testing %d-row rowsource\n", rows_count);
#endif

  vt = rasqal_new_variables_table(fake_world);

  /* add 2 variables to table and 1 row sequence */
  seq = rasqal_new_query_result_row_sequence(fake_world, vt, test_1_rows, 2);
  if(!seq) {
    fprintf(stderr, "%s: failed to create sequence of %d rows\n", program,
            rows_count);
    failures++;
    goto tidy;
  }

  rowsource = rasqal_new_rowsequence_rowsource(fake_query, vt, seq);
  if(!rowsource) {
    fprintf(stderr, "%s: failed to create %d-row sequence rowsource\n",
            program, rows_count);
    failures++;
    goto tidy;
  }
  /* seq is now owned by rowsource */
  seq = NULL;

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
  rasqal_query_result_row_print(row, stderr);
  fputc('\n', stderr);
#endif

  rasqal_free_query_result_row(row); row = NULL;

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
  
  if(rasqal_rowsource_get_query(rowsource) != fake_query) {
    fprintf(stderr,
            "%s: get_query returned a different query for a %d-row sequence rowsurce\n",
            program, rows_count);
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

  vt = rasqal_new_variables_table(fake_world);

  /* add 4 variables to table and 3 row sequence */
  seq = rasqal_new_query_result_row_sequence(fake_world, vt, test_3_rows, 4);
  if(!seq) {
    fprintf(stderr, "%s: failed to create sequence of %d rows\n",
            program, rows_count);
    failures++;
    goto tidy;
  }

  rowsource = rasqal_new_rowsequence_rowsource(fake_query, vt, seq);
  if(!rowsource) {
    fprintf(stderr, "%s: failed to create %d-row sequence rowsource\n",
            program, rows_count);
    failures++;
    goto tidy;
  }
  /* seq is now owned by rowsource */
  seq = NULL;

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
    rasqal_query_result_row_print(row, stderr);
    fputc('\n', stderr);
  #endif

    rasqal_free_query_result_row(row); row = NULL;
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
  
  if(rasqal_rowsource_get_query(rowsource) != fake_query) {
    fprintf(stderr,
            "%s: get_query returned a different query for a %d-row sequence rowsurce\n",
            program, rows_count);
    failures++;
    goto tidy;
  }

  rasqal_free_rowsource(rowsource); rowsource = NULL;
  rasqal_free_variables_table(vt); vt = NULL;


  tidy:
  if(row)
    rasqal_free_query_result_row(row);
  if(seq)
    raptor_free_sequence(seq);
  if(rowsource)
    rasqal_free_rowsource(rowsource);
  if(vt)
    rasqal_free_variables_table(vt);

  return failures;
}

#endif

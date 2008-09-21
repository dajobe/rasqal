/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_empty.c - Rasqal empty rowsource class
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
} rasqal_empty_rowsource_context;


static rasqal_query_result_row*
rasqal_empty_rowsource_read_row(rasqal_rowsource* rowsource, void *user_data)
{
  /* rasqal_empty_rowsource_context* con=(rasqal_empty_rowsource_context*)user_data; */
  return NULL;
}

static raptor_sequence*
rasqal_empty_rowsource_read_all_rows(rasqal_rowsource* rowsource,
                                     void *user_data)
{
  /* rasqal_empty_rowsource_context* con=(rasqal_empty_rowsource_context*)user_data; */
  return NULL;
}

static rasqal_query*
rasqal_empty_rowsource_get_query(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_empty_rowsource_context* con=(rasqal_empty_rowsource_context*)user_data;
  return con->query;
}


static const rasqal_rowsource_handler rasqal_empty_rowsource_handler={
  /* .version = */ 1,
  /* .init = */ NULL,
  /* .finish = */ NULL,
  /* .ensure_variables = */ NULL,
  /* .read_row = */ rasqal_empty_rowsource_read_row,
  /* .read_all_rows = */ rasqal_empty_rowsource_read_all_rows,
  /* .get_query = */ rasqal_empty_rowsource_get_query
};


rasqal_rowsource*
rasqal_new_empty_rowsource(rasqal_query* query)
{
  rasqal_empty_rowsource_context* con;
  int flags=0;
  
  con=(rasqal_empty_rowsource_context*)RASQAL_CALLOC(rasqal_empty_rowsource_context, 1, sizeof(rasqal_empty_rowsource_context));
  if(!con)
    return NULL;

  con->query=query;
  return rasqal_new_rowsource_from_handler(con,
                                           &rasqal_empty_rowsource_handler,
                                           flags);
}


#endif



#ifdef STANDALONE

/* one more prototype */
int main(int argc, char *argv[]);

int
main(int argc, char *argv[]) 
{
  const char *program=rasqal_basename(argv[0]);
  rasqal_rowsource *rowsource=NULL;
  rasqal_query* fake_query=(rasqal_query*)-1;
  rasqal_query_result_row* row=NULL;
  int count;
  raptor_sequence* seq=NULL;
  int failures=0;
  
  rowsource=rasqal_new_empty_rowsource(fake_query);
  if(!rowsource) {
    fprintf(stderr, "%s: failed to create emtpy rowsource\n", program);
    failures++;
    goto tidy;
  }

  row=rasqal_rowsource_read_row(rowsource);
  if(row) {
    fprintf(stderr, "%s: read_row returned a row for a empty stream\n",
            program);
    failures++;
    goto tidy;
  }
  
  count=rasqal_rowsource_get_rows_count(rowsource);
  if(count) {
    fprintf(stderr, "%s: read_rows returned a row count for a empty stream\n",
            program);
    failures++;
    goto tidy;
  }
  
  seq=rasqal_rowsource_read_all_rows(rowsource);
  if(!seq) {
    fprintf(stderr, "%s: read_rows returned a NULL seq for a empty stream\n",
            program);
    failures++;
    goto tidy;
  }
  if(raptor_sequence_size(seq) != 0) {
    fprintf(stderr, "%s: read_rows returned a non-empty seq for a empty stream\n",
            program);
    failures++;
    goto tidy;
  }

  if(rasqal_rowsource_get_query(rowsource) != fake_query) {
    fprintf(stderr,
            "%s: get_query returned a different query for an empty stream\n",
            program);
    failures++;
    goto tidy;
  }


  tidy:
  if(seq)
    raptor_free_sequence(seq);
  if(rowsource)
    rasqal_free_rowsource(rowsource);

  return failures;
}

#endif

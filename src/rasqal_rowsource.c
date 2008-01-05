/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource.c - Rasqal class for abstracting generating query rows
 *
 * Copyright (C) 2008, David Beckett http://purl.org/net/dajobe/
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
#include <stdarg.h>

#include <raptor.h>

#include "rasqal.h"
#include "rasqal_internal.h"


#ifndef STANDALONE


struct rasqal_rowsource_s
{
  void *user_data;
  const rasqal_rowsource_handler* handler;

  /* non-0 if rowsource has ended */
  int ended;

  /* count of number of rows returned */
  int count;

  /* count of variables_sequence array */
  int variables_count;

  /* array of rasqal_variable* */
  raptor_sequence* variables_sequence;
};




/**
 * rasqal_new_rowsource_from_handler:
 * @user_data: pointer to context information to pass in to calls
 * @handler: pointer to handler methods
 *
 * Create a new rowsource over a user-defined handler.
 *
 * Return value: new #rasqal_rowsource object or NULL on failure
 **/
rasqal_rowsource*
rasqal_new_rowsource_from_handler(void *user_data,
                                  const rasqal_rowsource_handler *handler)
{
  rasqal_rowsource* rowsource;

  if(!handler)
    return NULL;

  if(rowsource->handler->version < 1 || rowsource->handler->version >1)
    return NULL;

  rowsource=(rasqal_rowsource*)RASQAL_CALLOC(rasqal_rowsource, 1, sizeof(rasqal_rowsource));
  if(!rowsource)
    return NULL;

  rowsource->handler=handler;
  rowsource->user_data=(void*)user_data;

  if(rowsource->handler->init && 
     rowsource->handler->init(rowsource, rowsource->user_data)) {
    RASQAL_FREE(rasqal_rowsource, rowsource);
    return NULL;
  }
  return rowsource;
}


/**
 * rasqal_free_rowsource:
 * @rowsource: rowsource object
 *
 * Destructor - destroy an rowsource.
 **/
void
rasqal_free_rowsource(rasqal_rowsource *rowsource)
{
  if(rowsource->handler->finish)
    rowsource->handler->finish(rowsource, rowsource->user_data);

  if(rowsource->variables_sequence)
    raptor_free_sequence(rowsource->variables_sequence);

  RASQAL_FREE(rasqal_rowsource, rowsource);
}



/**
 * rasqal_rowsource_update_variables:
 * @rowsource: rasqal rowsource
 *
 * Set the rowsource metadata
 *
 * Return value: non-0 on failure
 **/
int
rasqal_rowsource_update_variables(rasqal_rowsource *rowsource)
{
  if(rowsource->ended)
    return 1;

  if(rowsource->handler->update_variables)
    return rowsource->handler->update_variables(rowsource->user_data, rowsource);

  return 0;
}


/**
 * rasqal_rowsource_read_row:
 * @rowsource: rasqal rowsource
 *
 * Read a query result row from the rowsource.
 *
 * Return value: row or NULL when no more rows are available
 **/
rasqal_query_result_row*
rasqal_rowsource_read_row(rasqal_rowsource *rowsource)
{
  rasqal_query_result_row* row=NULL;
  
  if(rowsource->ended)
    return NULL;

  if(rowsource->handler->read_row)
    row=rowsource->handler->read_row(rowsource, rowsource->user_data);

  if(!row)
    rowsource->ended=1;
  else
    rowsource->count++;

  return row;
}


/**
 * rasqal_rowsource_get_row_count:
 * @rowsource: rasqal rowsource
 *
 * Get number of rows seen from a rowsource.
 *
 * Return value: row count
 **/
int
rasqal_rowsource_get_rows_count(rasqal_rowsource *rowsource)
{
  return rowsource->count;
}



#endif



#ifdef STANDALONE

/* one more prototype */
int main(int argc, char *argv[]);


#define IN_FILENAME "in.bin"
#define OUT_BYTES_COUNT 14


int
main(int argc, char *argv[]) 
{
  const char *program=rasqal_basename(argv[0]);
#define TEST_ITEMS_COUNT 9
  int i;

  for(i=0; i<4; i++) {
    rasqal_rowsource *rowsource;
    size_t count;

    /* for _from_file */
    FILE *handle=NULL;
    /* for _from_string */
    void *string;
    size_t string_len;

    switch(i) {
      case 0:
#ifdef RASQAL_DEBUG
        fprintf(stderr, "%s: Creating rowsource from afilename '%s'\n", program, OUT_FILENAME);
#endif
        rowsource=rasqal_new_rowsource_from_filename((const char*)IN_FILENAME);
        if(!rowsource) {
          fprintf(stderr, "%s: Failed to create rowsource to filename '%s'\n",
                  program, OUT_FILENAME);
          exit(1);
        }
        break;

      case 1:
#ifdef RASQAL_DEBUG
        fprintf(stderr, "%s: Creating rowsource from file handle\n", program);
#endif
        handle=fopen((const char*)OUT_FILENAME, "wb");
        rowsource=rasqal_new_rowsource_from_file_handle(handle);
        if(!rowsource) {
          fprintf(stderr, "%s: Failed to create rowsource from a file handle\n", program);
          exit(1);
        }
        break;

      case 2:
#ifdef RASQAL_DEBUG
        fprintf(stderr, "%s: Creating rowsource from a string\n", program);
#endif
        rowsource=rasqal_new_rowsource_from_string(&string, &string_len, NULL);
        if(!rowsource) {
          fprintf(stderr, "%s: Failed to create rowsource from a string\n", program);
          exit(1);
        }
        break;

      case 3:
#ifdef RASQAL_DEBUG
        fprintf(stderr, "%s: Creating rowsource from a sink\n", program);
#endif
        rowsource=rasqal_new_rowsource_from_sink();
        if(!rowsource) {
          fprintf(stderr, "%s: Failed to create rowsource from a sink\n", program);
          exit(1);
        }
        break;

      default:
        fprintf(stderr, "%s: Unknown test case %d init\n", program, i);
        exit(1);
    }
    

    count=rasqal_rowsource_get_rows_count(rowsource);
    if(count != OUT_BYTES_COUNT) {
      fprintf(stderr, "%s: I/O stream wrote %d bytes, expected %d\n", program,
              (int)count, (int)OUT_BYTES_COUNT);
      return 1;
    }
    
#ifdef RASQAL_DEBUG
    fprintf(stderr, "%s: Freeing rowsource\n", program);
#endif
    rasqal_free_rowsource(rowsource);

    switch(i) {
      case 0:
        remove(OUT_FILENAME);
        break;

      case 1:
        fclose(handle);
        remove(OUT_FILENAME);
        break;

      case 2:
        if(!string) {
          fprintf(stderr, "%s: I/O stream failed to create a string\n", program);
          return 1;
        }
        if(string_len != count) {
          fprintf(stderr, "%s: I/O stream created a string length %d, expected %d\n", program, (int)string_len, (int)count);
          return 1;
        }
        rasqal_free_memory(string);
        break;

      case 3:
        break;

      default:
        fprintf(stderr, "%s: Unknown test case %d tidy\n", program, i);
        exit(1);
    }
    
  }
  
  /* keep gcc -Wall happy */
  return(0);
}

#endif

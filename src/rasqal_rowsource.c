/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource.c - Rasqal query rowsource (row generator) class
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
#include <stdarg.h>

#include <raptor.h>

#include "rasqal.h"
#include "rasqal_internal.h"


#ifndef STANDALONE

static void rasqal_rowsource_print_header(rasqal_rowsource* rowsource, FILE* fh);

/**
 * rasqal_new_rowsource_from_handler:
 * @query: query object
 * @user_data: pointer to context information to pass in to calls
 * @handler: pointer to handler methods
 * @vars_table: variables table to use for rows (or NULL)
 * @flags: 0 (none defined so far)
 *
 * Create a new rowsource over a user-defined handler.
 *
 * Return value: new #rasqal_rowsource object or NULL on failure
 **/
rasqal_rowsource*
rasqal_new_rowsource_from_handler(rasqal_world* world,
                                  rasqal_query* query,
                                  void *user_data,
                                  const rasqal_rowsource_handler *handler,
                                  rasqal_variables_table* vars_table,
                                  int flags)
{
  rasqal_rowsource* rowsource;

  if(!world || !handler)
    return NULL;

  if(handler->version < 1 || handler->version > 1)
    return NULL;

  rowsource = RASQAL_CALLOC(rasqal_rowsource*, 1, sizeof(*rowsource));
  if(!rowsource) {
    if(handler->finish)
      handler->finish(NULL, user_data);
    return NULL;
  }

  rowsource->usage = 1;
  rowsource->world = world;
  rowsource->query = query;
  rowsource->user_data = (void*)user_data;
  rowsource->handler = handler;
  rowsource->flags = flags;

  rowsource->size = 0;

  rowsource->generate_group = 0;
  
  if(vars_table)
    rowsource->vars_table = rasqal_new_variables_table_from_variables_table(vars_table);
  else
    rowsource->vars_table = NULL;

  rowsource->variables_sequence = raptor_new_sequence((raptor_data_free_handler)rasqal_free_variable,
                                                      (raptor_data_print_handler)rasqal_variable_print);
  if(!rowsource->variables_sequence) {
    rasqal_free_rowsource(rowsource);
    return NULL;
  }
  
  if(rowsource->handler->init && 
     rowsource->handler->init(rowsource, rowsource->user_data)) {
    RASQAL_DEBUG2("rowsource %s init failed\n", rowsource->handler->name);
    rasqal_free_rowsource(rowsource);
    return NULL;
  }

  return rowsource;
}


/**
 * rasqal_new_rowsource_from_rowsource:
 * @row: query result row
 *
 * INTERNAL - Copy a rowsource.
 *
 * Return value: a copy of the rowsource or NULL
 */
rasqal_rowsource*
rasqal_new_rowsource_from_rowsource(rasqal_rowsource* rowsource)
{
  if(!rowsource)
    return NULL;

  rowsource->usage++;
  return rowsource;
}


/**
 * rasqal_free_rowsource:
 * @rowsource: rowsource object
 *
 * INTERNAL - Destructor - destroy an rowsource.
 **/
void
rasqal_free_rowsource(rasqal_rowsource *rowsource)
{
  if(!rowsource)
    return;

  if(--rowsource->usage)
    return;

  if(rowsource->handler->finish)
    rowsource->handler->finish(rowsource, rowsource->user_data);

  if(rowsource->vars_table)
    rasqal_free_variables_table(rowsource->vars_table);

  if(rowsource->variables_sequence)
    raptor_free_sequence(rowsource->variables_sequence);

  if(rowsource->rows_sequence)
    raptor_free_sequence(rowsource->rows_sequence);

  RASQAL_FREE(rasqal_rowsource, rowsource);
}



/**
 * rasqal_rowsource_add_variable:
 * @rowsource: rasqal rowsource
 * @v: variable
 *
 * Add a variable to the rowsource if the variable is not already present
 *
 * Return value: variable offset or < 0 on failure
 **/
int
rasqal_rowsource_add_variable(rasqal_rowsource *rowsource, rasqal_variable* v)
{
  int offset;

  if(!rowsource || !v)
    return -1;

  offset = rasqal_rowsource_get_variable_offset_by_name(rowsource, v->name);
  if(offset >= 0)
    return offset;

  v = rasqal_new_variable_from_variable(v);
  if(raptor_sequence_push(rowsource->variables_sequence, v))
    return -1;

  offset = rowsource->size;
  
  rowsource->size++;

  return offset;
}


/**
 * rasqal_rowsource_ensure_variables:
 * @rowsource: rasqal rowsource
 *
 * INTERNAL - Ensure that the variables in the rowsource are defined
 *
 * Return value: non-0 on failure
 */
int
rasqal_rowsource_ensure_variables(rasqal_rowsource *rowsource)
{
  int rc = 0;
  
  if(rowsource->updated_variables)
    return 0;

  rowsource->updated_variables++;
  
  if(rowsource->handler->ensure_variables)
    rc = rowsource->handler->ensure_variables(rowsource, rowsource->user_data);

#ifdef RASQAL_DEBUG
  if(!rc) {
    RASQAL_DEBUG3("%s rowsource %p header: ", rowsource->handler->name,
                  rowsource);
    rasqal_rowsource_print_header(rowsource, stderr);
  }
#endif

  return rc;
}


/**
 * rasqal_rowsource_read_row:
 * @rowsource: rasqal rowsource
 *
 * Read a query result row from the rowsource.
 *
 * If a row is returned, it is owned by the caller.
 *
 * Return value: row or NULL when no more rows are available
 **/
rasqal_row*
rasqal_rowsource_read_row(rasqal_rowsource *rowsource)
{
  rasqal_row* row = NULL;
  
  if(!rowsource || rowsource->finished)
    return NULL;

  if(rowsource->flags & RASQAL_ROWSOURCE_FLAGS_SAVED_ROWS) {
    /* return row from saved rows sequence at offset */
    row = (rasqal_row*)raptor_sequence_get_at(rowsource->rows_sequence,
                                              rowsource->offset++);
#ifdef RASQAL_DEBUG
    RASQAL_DEBUG3("%s rowsource %p returned saved row:  ",
                  rowsource->handler->name, rowsource);
    if(row)
      rasqal_row_print(row, stderr);
    else
      fputs("NONE", stderr);
    fputs("\n", stderr);
#endif      
    if(row)
      row = rasqal_new_row_from_row(row);
    /* row is owned by us */
  } else {
    if(rasqal_rowsource_ensure_variables(rowsource))
      return NULL;

    if(rowsource->handler->read_row) {
      row = rowsource->handler->read_row(rowsource, rowsource->user_data);
      /* row is owned by us */

      if(row && rowsource->flags & RASQAL_ROWSOURCE_FLAGS_SAVE_ROWS) {
        if(!rowsource->rows_sequence) {
          rowsource->rows_sequence = raptor_new_sequence((raptor_data_free_handler)rasqal_free_row,
                                                         (raptor_data_print_handler)rasqal_row_print);
          rowsource->offset = 0;
        }
        /* copy to save it away */
        row = rasqal_new_row_from_row(row);
        raptor_sequence_push(rowsource->rows_sequence, row);
      }
    } else {
      if(!rowsource->rows_sequence) {
        raptor_sequence* seq;
        
        seq = rasqal_rowsource_read_all_rows(rowsource);
        if(rowsource->rows_sequence)
          raptor_free_sequence(rowsource->rows_sequence);
        /* rows_sequence now owns all rows */
        rowsource->rows_sequence = seq;

        rowsource->offset = 0;
      }
      
      if(rowsource->rows_sequence) {
        /* return row from sequence at offset */
        row = (rasqal_row*)raptor_sequence_get_at(rowsource->rows_sequence,
                                                  rowsource->offset++);
        if(row)
          row = rasqal_new_row_from_row(row);
        /* row is owned by us */
      }
    }
  }
  
  if(!row) {
    rowsource->finished = 1;
    if(rowsource->flags & RASQAL_ROWSOURCE_FLAGS_SAVE_ROWS)
      rowsource->flags |= RASQAL_ROWSOURCE_FLAGS_SAVED_ROWS;
  } else {
    rowsource->count++;

    /* Generate a group around all rows if there are no groups returned */
    if(rowsource->generate_group && row->group_id < 0)
      row->group_id = 0;
  }

#ifdef RASQAL_DEBUG
  RASQAL_DEBUG3("%s rowsource %p returned row:  ", rowsource->handler->name, 
                rowsource);
  if(row)
    rasqal_row_print(row, stderr);
  else
    fputs("NONE", stderr);
  fputs("\n", stderr);
#endif

  return row;
}


/**
 * rasqal_rowsource_get_row_count:
 * @rowsource: rasqal rowsource
 *
 * Get number of rows seen from a rowsource.
 *
 * Return value: row count or < 0 on failure
 **/
int
rasqal_rowsource_get_rows_count(rasqal_rowsource *rowsource)
{
  if(!rowsource)
    return -1;

  return rowsource->count;
}


/**
 * rasqal_rowsource_read_all_rows:
 * @rowsource: rasqal rowsource
 *
 * Read all rows from a rowsource
 *
 * After calling this, the rowsource will be empty of rows and finished
 * and if a sequence is returned, it is owned by the caller.
 *
 * Return value: new sequence of all rows (may be size 0) or NULL on failure
 **/
raptor_sequence*
rasqal_rowsource_read_all_rows(rasqal_rowsource *rowsource)
{
  raptor_sequence* seq;

  if(!rowsource)
    return NULL;

  if(rowsource->flags & RASQAL_ROWSOURCE_FLAGS_SAVED_ROWS) {
    raptor_sequence* new_seq;

    /* Return a complete copy of all previously saved rows */
    seq = rowsource->rows_sequence;
    new_seq = rasqal_row_sequence_copy(seq);
    RASQAL_DEBUG4("%s rowsource %p returning a sequence of %d saved rows\n",
                  rowsource->handler->name, rowsource,
                  raptor_sequence_size(new_seq));
    return new_seq;
  }

  /* Execute */
  if(rasqal_rowsource_ensure_variables(rowsource))
    return NULL;

  if(rowsource->handler->read_all_rows) {
    seq = rowsource->handler->read_all_rows(rowsource, rowsource->user_data);
    if(!seq) {
      seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_row,
                                (raptor_data_print_handler)rasqal_row_print);
    } else if(rowsource->generate_group) {
      int i;
      rasqal_row* row;
      
      /* Set group for all rows if there are no groups returned */

      for(i = 0; (row = (rasqal_row*)raptor_sequence_get_at(seq, i)); i++) {
        /* if first row has a group ID, end */
        if(!i && row->group_id >= 0)
          break;
        
        row->group_id = 0;
      }
    }

    goto done;
  }

  seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_row,
                            (raptor_data_print_handler)rasqal_row_print);
  if(!seq)
    return NULL;

  while(1) {
    rasqal_row* row = rasqal_rowsource_read_row(rowsource);
    if(!row)
      break;

    /* Generate a group around all rows if there are no groups returned */
    if(rowsource->generate_group && row->group_id < 0)
      row->group_id = 0;

    raptor_sequence_push(seq, row);
  }

  done:
  if(seq && rowsource->flags & RASQAL_ROWSOURCE_FLAGS_SAVE_ROWS) {
    raptor_sequence* new_seq;

    /* Save a complete copy of all rows */
    new_seq = rasqal_row_sequence_copy(seq);
    RASQAL_DEBUG4("%s rowsource %p saving a sequence of %d rows\n",
                  rowsource->handler->name, rowsource,
                  raptor_sequence_size(new_seq));
    rowsource->rows_sequence = new_seq;
    rowsource->flags |= RASQAL_ROWSOURCE_FLAGS_SAVED_ROWS;
  }
  
  RASQAL_DEBUG4("%s rowsource %p returning a sequence of %d rows\n",
                rowsource->handler->name, rowsource,
                raptor_sequence_size(seq));
  return seq;
}


/**
 * rasqal_rowsource_get_size:
 * @rowsource: rasqal rowsource
 *
 * Get rowsource row width or <0 on failure
 **/
int
rasqal_rowsource_get_size(rasqal_rowsource *rowsource)
{
  if(!rowsource)
    return -1;

  rasqal_rowsource_ensure_variables(rowsource);

  return rowsource->size;
}


/**
 * rasqal_rowsource_get_variable_by_offset:
 * @rowsource: rasqal rowsource
 * @offset: integer offset into array of variables
 *
 * Get the variable associated with the given offset
 *
 * Return value: pointer to shared #rasqal_variable or NULL if out of range
 **/
rasqal_variable*
rasqal_rowsource_get_variable_by_offset(rasqal_rowsource *rowsource, int offset)
{
  if(!rowsource)
    return NULL;

  rasqal_rowsource_ensure_variables(rowsource);

  if(!rowsource->variables_sequence)
    return NULL;
  
  return (rasqal_variable*)raptor_sequence_get_at(rowsource->variables_sequence,
                                                  offset);
}


/**
 * rasqal_rowsource_get_variable_offset_by_name:
 * @rowsource: rasqal rowsource
 * @name: variable name
 *
 * Get the offset of a variable into the list of variables
 *
 * Return value: offset or <0 if not present or on failure
 **/
int
rasqal_rowsource_get_variable_offset_by_name(rasqal_rowsource *rowsource,
                                             const unsigned char* name)
{
  int offset= -1;
  int i;
  
  if(!rowsource)
    return -1;

  rasqal_rowsource_ensure_variables(rowsource);

  if(!rowsource->variables_sequence)
    return -1;
  
  for(i=0; i < raptor_sequence_size(rowsource->variables_sequence); i++) {
    rasqal_variable* v;
    v = (rasqal_variable*)raptor_sequence_get_at(rowsource->variables_sequence, i);
    if(!strcmp(RASQAL_GOOD_CAST(const char*, v->name),
               RASQAL_GOOD_CAST(const char*, name))) {
      offset = i;
      break;
    }
  }

  return offset;
}


/**
 * rasqal_rowsource_copy_variables:
 * @dest_rowsource: destination rowsource to copy into
 * @src_rowsource: source rowsource to copy from
 *
 * INTERNAL - Copy a variables projection from one rowsource to another
 *
 * This adds new variables from @src_rowsource to the
 * @dest_rowsource, it does not add duplicates.
 * 
 * Return value: 0 on success, non-0 on failure
 **/
int
rasqal_rowsource_copy_variables(rasqal_rowsource *dest_rowsource,
                                rasqal_rowsource *src_rowsource)
{
  int i;
  
  for(i = 0; i < src_rowsource->size; i++) {
    rasqal_variable* v;
    v = rasqal_rowsource_get_variable_by_offset(src_rowsource, i);
    if(!v) {
      RASQAL_DEBUG5("%s src rowsource %p failed to return variable at offset %d, size %d\n",
                    src_rowsource->handler->name, src_rowsource,
                    i, src_rowsource->size);
      return 1;
    }
    if(rasqal_rowsource_add_variable(dest_rowsource, v) < 0)
      return 1;
  }
  
  return 0;
}


static void 
rasqal_rowsource_print_header(rasqal_rowsource* rowsource, FILE* fh)
{
  int i;
  
  fputs("variables: ", fh);
  for(i = 0; i < rowsource->size; i++) {
    rasqal_variable* v;
    const unsigned char *name = NULL;

    v = rasqal_rowsource_get_variable_by_offset(rowsource, i);
    if(v)
      name = v->name;
    if(i > 0)
      fputs(", ", fh);
    if(name)
      fputs(RASQAL_GOOD_CAST(const char*, name), fh);
    else
      fputs("NULL", fh);
  }
  fputs("\n", fh);
}


/**
 * rasqal_rowsource_print_row_sequence:
 * @rowsource: rowsource associated with rows
 * @seq: query result sequence of #rasqal_row
 * @fp: FILE* handle to print to
 *
 * INTERNAL - Print a result set header with row values from a sequence
 */
void 
rasqal_rowsource_print_row_sequence(rasqal_rowsource* rowsource,
                                    raptor_sequence* seq,
                                    FILE* fh)
{
  int size = raptor_sequence_size(seq);
  int i;

  rasqal_rowsource_print_header(rowsource, fh);
  
  for(i = 0; i < size; i++) {
    rasqal_row *row = (rasqal_row*)raptor_sequence_get_at(seq, i);
    rasqal_row_print(row, fh);
    fputs("\n", fh);
  }
}


/**
 * rasqal_rowsource_reset:
 * @rowsource: rasqal rowsource
 *
 * INTERNAL - Reset a rowsource to regenerate the same set of rows
 *
 * Return value: query or NULL
 **/
int
rasqal_rowsource_reset(rasqal_rowsource* rowsource)
{
  rowsource->finished = 0;
  rowsource->count = 0;

  if(rowsource->handler->reset)
    return rowsource->handler->reset(rowsource, rowsource->user_data);

  if(rowsource->flags & RASQAL_ROWSOURCE_FLAGS_SAVED_ROWS) {
    RASQAL_DEBUG3("%s rowsource %p resetting to use saved rows\n", 
                  rowsource->handler->name, rowsource);
    rowsource->offset = 0;
  } else {
    RASQAL_DEBUG3("WARNING: %s rowsource %p has no reset and there are no saved rows\n",
                  rowsource->handler->name, rowsource);
  }
  return 0;
}


rasqal_rowsource*
rasqal_rowsource_get_inner_rowsource(rasqal_rowsource* rowsource, int offset)
{
  if(rowsource->handler->get_inner_rowsource)
    return rowsource->handler->get_inner_rowsource(rowsource,
                                                   rowsource->user_data,
                                                   offset);
  return NULL;
}


/**
 * rasqal_rowsource_visit:
 * @node: #rasqal_rowsource row source
 * @fn: pointer to function to apply that takes user data and rowsource parameters
 * @user_data: user data for applied function 
 * 
 * Visit a user function over a #rasqal_rowsource
 *
 * If the user function @fn returns 0, the visit is truncated.
 *
 * Return value: non-0 if the visit was truncated or on failure.
 **/
int
rasqal_rowsource_visit(rasqal_rowsource* rowsource,
                       rasqal_rowsource_visit_fn fn,
                       void *user_data)
{
  int result;
  int offset;
  rasqal_rowsource* inner_rs;
  
  if(!rowsource || !fn)
    return 1;

  result = fn(rowsource, user_data);
  /* end if failed */
  if(result < 0)
    return result;

  /* Do not recurse if handled */
  if(result > 0)
    return 0;

  for(offset = 0;
      (inner_rs = rasqal_rowsource_get_inner_rowsource(rowsource, offset));
      offset++) {
    result = rasqal_rowsource_visit(inner_rs, fn, user_data);
    /* end if failed */
    if(result < 0)
      return result;
  }

  return 0;
}


static int
rasqal_rowsource_visitor_set_origin(rasqal_rowsource* rowsource,
                                    void *user_data)
{
  rasqal_literal *literal = (rasqal_literal*)user_data;

  if(rowsource->handler->set_origin)
    return rowsource->handler->set_origin(rowsource, rowsource->user_data,
                                          literal);
  return 0;
}


int
rasqal_rowsource_set_origin(rasqal_rowsource* rowsource,
                            rasqal_literal *literal)
{
  rasqal_rowsource_visit(rowsource, 
                         rasqal_rowsource_visitor_set_origin,
                         literal);

  return 0;
}


static int
rasqal_rowsource_visitor_set_requirements(rasqal_rowsource* rowsource,
                                          void *user_data)
{
  unsigned int flags = *(unsigned int*)user_data;

  if(rowsource->handler->set_requirements)
    return rowsource->handler->set_requirements(rowsource, rowsource->user_data,
                                                flags);

  if(flags & RASQAL_ROWSOURCE_REQUIRE_RESET) {
    if(!rowsource->handler->reset) {
      /* If there is no reset handler, it is handled by this module and it is needed
       * it is handled by this module
       */
      RASQAL_DEBUG3("setting %s rowsource %p to save rows\n",
                    rowsource->handler->name, rowsource);
      rowsource->flags |= RASQAL_ROWSOURCE_FLAGS_SAVE_ROWS;
      return 1;
    }
  }

  return 0;
}


int
rasqal_rowsource_set_requirements(rasqal_rowsource* rowsource, unsigned int flags)
{
  return rasqal_rowsource_visit(rowsource, 
                                rasqal_rowsource_visitor_set_requirements,
                                &flags);
}


int
rasqal_rowsource_request_grouping(rasqal_rowsource* rowsource)
{
  rowsource->generate_group = 1;

  return 0;
}


#define SPACES_LENGTH 80
static const char spaces[SPACES_LENGTH+1] = "                                                                                ";

static void
rasqal_rowsource_write_indent(raptor_iostream *iostr, unsigned int indent) 
{
  while(indent > 0) {
    unsigned int sp = (indent > SPACES_LENGTH) ? SPACES_LENGTH : indent;
    raptor_iostream_write_bytes(spaces, sizeof(char), RASQAL_GOOD_CAST(size_t, sp), iostr);
    indent -= sp;
  }
}


static int
rasqal_rowsource_write_internal(rasqal_rowsource *rowsource, 
                                raptor_iostream* iostr, unsigned int indent)
{
  const char* rs_name = rowsource->handler->name;
  int arg_count = 0;
  unsigned int indent_delta;
  int offset;
  rasqal_rowsource* inner_rowsource;
  
  indent_delta = RASQAL_GOOD_CAST(unsigned int, strlen(rs_name));

  raptor_iostream_counted_string_write(rs_name, indent_delta, iostr);
  raptor_iostream_counted_string_write("(\n", 2, iostr);
  indent_delta++;
  
  indent += indent_delta;
  rasqal_rowsource_write_indent(iostr, indent);


  for(offset = 0;
      (inner_rowsource = rasqal_rowsource_get_inner_rowsource(rowsource, offset));
      offset++) {
      if(arg_count) {
        raptor_iostream_counted_string_write(" ,\n", 3, iostr);
        rasqal_rowsource_write_indent(iostr, indent);
      }
      rasqal_rowsource_write_internal(inner_rowsource, iostr, indent);
      arg_count++;
  }

  raptor_iostream_write_byte('\n', iostr);
  indent-= indent_delta;

  rasqal_rowsource_write_indent(iostr, indent);
  raptor_iostream_write_byte(')', iostr);

  return 0;
}


int
rasqal_rowsource_write(rasqal_rowsource *rowsource, raptor_iostream *iostr)
{
  return rasqal_rowsource_write_internal(rowsource, iostr, 0);
}
  

/**
 * rasqal_rowsource_print:
 * @rs: the #rasqal_rowsource object
 * @fh: the FILE* handle to print to
 *
 * Print a #rasqal_rowsource in a debug format.
 * 
 * The print debug format may change in any release.
 * 
 **/
void
rasqal_rowsource_print(rasqal_rowsource *rowsource, FILE* fh)
{
  raptor_iostream *iostr;

  if(!rowsource || !fh)
    return;

  iostr = raptor_new_iostream_to_file_handle(rowsource->world->raptor_world_ptr, fh);
  rasqal_rowsource_write(rowsource, iostr);
  raptor_free_iostream(iostr);
}


/*
 * rasqal_rowsource_remove_all_variables:
 * @rowsource: rasqal rowsource
 *
 * INTERNAL - Remove all variables from a rowsource
 */
void
rasqal_rowsource_remove_all_variables(rasqal_rowsource *rowsource)
{
  while(1) {
    rasqal_variable* v;
    v = (rasqal_variable*)raptor_sequence_pop(rowsource->variables_sequence);
    if(!v)
      break;
    rasqal_free_variable(v);
  }
  rowsource->size = 0;
}

#endif /* not STANDALONE */



#ifdef STANDALONE

/* one more prototype */
int main(int argc, char *argv[]);


#define IN_FILENAME "in.bin"
#define OUT_BYTES_COUNT 14


int
main(int argc, char *argv[]) 
{
  const char *program = rasqal_basename(argv[0]);
#define TEST_ITEMS_COUNT 9
  int i;

  for(i = 0; i < 4; i++) {
    rasqal_rowsource *rowsource;
    int count;

    /* for _from_file */
    FILE *handle = NULL;
    /* for _from_string */
    void *string;
    size_t string_len;

    switch(i) {
      case 0:
#ifdef RASQAL_DEBUG
        fprintf(stderr, "%s: Creating rowsource from a filename '%s'\n",
                program, OUT_FILENAME);
#endif
        rowsource = rasqal_new_rowsource_from_filename(RASQAL_GOOD_CAST(const char*, IN_FILENAME));
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
        handle = fopen(RASQAL_GOOD_CAST(const char*, OUT_FILENAME), "wb");
        rowsource = rasqal_new_rowsource_from_file_handle(handle);
        if(!rowsource) {
          fprintf(stderr, "%s: Failed to create rowsource from a file handle\n", program);
          exit(1);
        }
        break;

      case 2:
#ifdef RASQAL_DEBUG
        fprintf(stderr, "%s: Creating rowsource from a string\n", program);
#endif
        rowsource = rasqal_new_rowsource_from_string(&string, &string_len, NULL);
        if(!rowsource) {
          fprintf(stderr, "%s: Failed to create rowsource from a string\n", program);
          exit(1);
        }
        break;

      case 3:
#ifdef RASQAL_DEBUG
        fprintf(stderr, "%s: Creating rowsource from a sink\n", program);
#endif
        rowsource = rasqal_new_rowsource_from_sink();
        if(!rowsource) {
          fprintf(stderr, "%s: Failed to create rowsource from a sink\n", program);
          exit(1);
        }
        break;

      default:
        fprintf(stderr, "%s: Unknown test case %d init\n", program, i);
        exit(1);
    }
    

    count = rasqal_rowsource_get_rows_count(rowsource);
    if(count != OUT_BYTES_COUNT) {
      fprintf(stderr, "%s: I/O stream wrote %d bytes, expected %d\n", program,
              count, RASQAL_GOOD_CAST(int, OUT_BYTES_COUNT));
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
          fprintf(stderr, "%s: I/O stream created a string length %d, expected %d\n", program, RASQAL_BAD_CAST(int, string_len), count);
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

#endif /* STANDALONE */

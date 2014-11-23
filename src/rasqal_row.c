/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_row.c - Rasqal Query Result Row
 *
 * Copyright (C) 2003-2009, David Beckett http://www.dajobe.org/
 * Copyright (C) 2003-2005, University of Bristol, UK http://www.bristol.ac.uk/
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

#include "rasqal.h"
#include "rasqal_internal.h"




static rasqal_row*
rasqal_new_row_common(rasqal_world* world, int size, int order_size)
{
  rasqal_row* row;
  
  row = RASQAL_CALLOC(rasqal_row*, 1, sizeof(*row));
  if(!row)
    return NULL;

  row->usage = 1;
  row->size = size;
  row->order_size = order_size;

  if(row->size > 0) {
    row->values = RASQAL_CALLOC(rasqal_literal**, RASQAL_GOOD_CAST(size_t, row->size),
                                sizeof(rasqal_literal*));
    if(!row->values) {
      rasqal_free_row(row);
      return NULL;
    }
  }

  if(row->order_size > 0) {
    row->order_values = RASQAL_CALLOC(rasqal_literal**, RASQAL_GOOD_CAST(size_t, row->order_size),
                                      sizeof(rasqal_literal*));
    if(!row->order_values) {
      rasqal_free_row(row);
      return NULL;
    }
  }

  row->group_id = -1;
  
  return row;
}


/**
 * rasqal_new_row:
 * @rowsource: rowsource
 *
 * INTERNAL - Create a new query result row at an offset into the result sequence.
 *
 * Return value: a new query result row or NULL on failure
 */
rasqal_row*
rasqal_new_row(rasqal_rowsource* rowsource)
{
  int size;
  int order_size = -1;
  rasqal_row* row;

  if(!rowsource)
    return NULL;

  rowsource = rasqal_new_rowsource_from_rowsource(rowsource);

  size = rasqal_rowsource_get_size(rowsource);

  row = rasqal_new_row_common(rowsource->world, size, order_size);
  if(row)
    row->rowsource = rowsource;

  return row;
}


/**
 * rasqal_new_row_for_size:
 * @world: rasqal_world
 * @size: width of row
 *
 * Constructor - Create a new query result row of a given size
 *
 * Return value: a new query result row or NULL on failure
 */
rasqal_row*
rasqal_new_row_for_size(rasqal_world* world, int size)
{
  int order_size = 0;

  return rasqal_new_row_common(world, size, order_size);
}


/**
 * rasqal_new_row_from_row:
 * @row: query result row
 * 
 * INTERNAL - Copy a query result row.
 *
 * Return value: a copy of the query result row or NULL
 */
rasqal_row*
rasqal_new_row_from_row(rasqal_row* row)
{
  row->usage++;
  return row;
}


/**
 * rasqal_free_row:
 * @row: query result row
 * 
 * Destructor - Free a query result row object.
 */
void 
rasqal_free_row(rasqal_row* row)
{
  if(!row)
    return;

  if(--row->usage)
    return;
  
  if(row->values) {
    int i; 
    for(i = 0; i < row->size; i++) {
      if(row->values[i])
        rasqal_free_literal(row->values[i]);
    }
    RASQAL_FREE(array, row->values);
  }
  if(row->order_values) {
    int i; 
    for(i = 0; i < row->order_size; i++) {
      if(row->order_values[i])
        rasqal_free_literal(row->order_values[i]);
    }
    RASQAL_FREE(array, row->order_values);
  }

  if(row->rowsource)
    rasqal_free_rowsource(row->rowsource);

  RASQAL_FREE(rasqal_row, row);
}


/**
 * rasqal_row_print:
 * @row: query result row
 * @fp: FILE* handle
 *
 * INTERNAL - Print a query result row.
 */
int
rasqal_row_print(rasqal_row* row, FILE* fh)
{
  rasqal_rowsource* rowsource = row->rowsource;
  int i;
  
  fputs("row[", fh);
  for(i = 0; i < row->size; i++) {
    /* Do not use rasqal_query_results_get_binding_name(row->results, i); 
     * as it does not work for a construct result
     */
    const unsigned char *name = NULL;
    rasqal_literal *value;

    if(rowsource) {
      rasqal_variable* v;
      v = rasqal_rowsource_get_variable_by_offset(rowsource, i);
      if(v)
        name = v->name;
    }
    
    value = row->values[i];
    if(i > 0)
      fputs(", ", fh);
    if(name)
      fprintf(fh, "%s=", name);

    rasqal_literal_print(value, fh);
  }

  if(row->order_size > 0) {
    fputs(" with ordering values [", fh);

    for(i = 0; i < row->order_size; i++) {
      rasqal_literal *value = row->order_values[i];
      
      if(i > 0)
        fputs(", ", fh);
      rasqal_literal_print(value, fh);
    }
    fputs("]", fh);

  }

  if(row->group_id >= 0)
    fprintf(fh, " group %d", row->group_id);

  fprintf(fh, " offset %d]", row->offset);

  return 0;
}


/*
 * rasqal_row_write:
 * @row: query result row
 * @iostr: raptor iostream
 *
 * INTERNAL - Write a query result row to an iostream.
 *
 * Return value: non-0 on failure
 */
int
rasqal_row_write(rasqal_row* row, raptor_iostream* iostr)
{
  rasqal_rowsource* rowsource;
  int i;

  if(!row || !iostr)
    return 1;

  rowsource = row->rowsource;
  raptor_iostream_counted_string_write("row[", 4, iostr);
  for(i = 0; i < row->size; i++) {
    /* Do not use rasqal_query_results_get_binding_name(row->results, i);
     * as it does not work for a construct result
     */
    const unsigned char *name = NULL;
    rasqal_literal *value;

    if(rowsource) {
      rasqal_variable* v;
      v = rasqal_rowsource_get_variable_by_offset(rowsource, i);
      if(v)
        name = v->name;
    }

    value = row->values[i];
    if(i > 0)
      raptor_iostream_counted_string_write(", ", 2, iostr);
    if(name) {
      raptor_iostream_string_write(name, iostr);
      raptor_iostream_counted_string_write("=", 1, iostr);
    }

    rasqal_literal_write(value, iostr);
  }

  if(row->order_size > 0) {
    raptor_iostream_counted_string_write(" with ordering values [", 23, iostr);

    for(i = 0; i < row->order_size; i++) {
      rasqal_literal *value = row->order_values[i];

      if(i > 0)
        raptor_iostream_counted_string_write(", ", 2, iostr);
      rasqal_literal_write(value, iostr);
    }
    raptor_iostream_counted_string_write("]", 1, iostr);

  }

  if(row->group_id >= 0) {
    raptor_iostream_counted_string_write(" group ", 7, iostr);
    raptor_iostream_decimal_write(row->group_id, iostr);
  }

  raptor_iostream_counted_string_write(" offset ", 8, iostr);
  raptor_iostream_decimal_write(row->offset, iostr);
  raptor_iostream_counted_string_write("]", 1, iostr);

  return 0;
}


/**
 * rasqal_row_set_value_at:
 * @row: query result row
 * @offset: offset into row (column number)
 * @value: literal value to set
 *
 * Set the value of a variable in a query result row
 *
 * Any existing row value is freed and the literal @value passed in
 * is copied.
 *
 * Return value: non-0 on failure
 */
int
rasqal_row_set_value_at(rasqal_row* row, int offset, rasqal_literal* value)
{
  if(!row || !value)
    return 1;

  if(offset < 0 || offset >= row->size)
    return 1;
  
  if(row->values[offset])
    rasqal_free_literal(row->values[offset]);
  
  row->values[offset] = rasqal_new_literal_from_literal(value);

  return 0;
}


/**
 * rasqal_new_row_sequence:
 * @world: world object ot use
 * @vt: variables table to use to declare variables
 * @row_data: row data
 * @vars_count: number of variables in row
 * @vars_seq_p: OUT parameter - pointer to place to store sequence of variables (or NULL)
 *
 * INTERNAL - Make a sequence of #rasqal_row* objects
 * with variables defined into the @vt table and values in the sequence
 *
 * The @row_data parameter is an array of strings forming a table of
 * width (vars_count * 2).
 * The first row is a list of variable names at offset 0.
 * The remaining rows are values where offset 0 is a literal and
 * offset 1 is a URI string.
 *
 * The last row is indicated by an entire row of NULLs.
 *
 * Return value: sequence of rows or NULL on failure
 */
raptor_sequence*
rasqal_new_row_sequence(rasqal_world* world,
                        rasqal_variables_table* vt,
                        const char* const row_data[],
                        int vars_count,
                        raptor_sequence** vars_seq_p)
{
  raptor_sequence *seq = NULL;
  raptor_sequence *vars_seq = NULL;
  int row_i;
  int column_i;
  int failed = 0;
  
#define GET_CELL(row, column, offset) \
  row_data[((((row)*vars_count)+(column))<<1)+(offset)]

  seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_row,
                            (raptor_data_print_handler)rasqal_row_print);
  if(!seq)
    return NULL;

  if(vars_seq_p) {
    vars_seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_variable,
                                   (raptor_data_print_handler)rasqal_variable_print);
    if(!vars_seq) {
      raptor_free_sequence(seq);
      return NULL;
    }
  }
  
  /* row 0 is variables */
  row_i = 0;

  for(column_i = 0; column_i < vars_count; column_i++) {
    const char * var_name = GET_CELL(row_i, column_i, 0);
    size_t var_name_len = strlen(var_name);
    rasqal_variable* v;
    
    v = rasqal_variables_table_add2(vt, RASQAL_VARIABLE_TYPE_NORMAL,
                                    RASQAL_GOOD_CAST(const unsigned char*, var_name),
                                    var_name_len, NULL);
    if(!v) {
      failed = 1;
      goto tidy;
    }

    if(vars_seq) {
      raptor_sequence_push(vars_seq, v);
      /* v is now owned by vars_seq */
    }
  }

  for(row_i = 1; 1; row_i++) {
    rasqal_row* row;
    int data_values_seen = 0;

    /* Terminate on an entire row of NULLs */
    for(column_i = 0; column_i < vars_count; column_i++) {
      if(GET_CELL(row_i, column_i, 0) || GET_CELL(row_i, column_i, 1)) {
        data_values_seen++;
        break;
      }
    }
    if(!data_values_seen)
      break;
    
    row = rasqal_new_row_for_size(world, vars_count);
    if(!row) {
      failed = 1;
      goto tidy;
    }

    for(column_i = 0; column_i < vars_count; column_i++) {
      rasqal_literal* l = NULL;

      if(GET_CELL(row_i, column_i, 0)) {
        /* string literal */
        const char* str = GET_CELL(row_i, column_i, 0);
        size_t str_len = strlen(str);
        char *eptr = NULL;
        long number;
        
        number = strtol(RASQAL_GOOD_CAST(const char*, str), &eptr, 10);
        if(!*eptr) {
          /* is an integer */
          l = rasqal_new_numeric_literal_from_long(world,
                                                   RASQAL_LITERAL_INTEGER, 
                                                   number);
        } else {
          unsigned char *val;
          val = RASQAL_MALLOC(unsigned char*, str_len + 1);
          if(val) {
            memcpy(val, str, str_len + 1);

            l = rasqal_new_string_literal_node(world, val, NULL, NULL);
          } else 
            failed = 1;
        }
      } else if(GET_CELL(row_i, column_i, 1)) {
        /* URI */
        const unsigned char* str;
        raptor_uri* u;
        str = RASQAL_GOOD_CAST(const unsigned char*, GET_CELL(row_i, column_i, 1));
        u = raptor_new_uri(world->raptor_world_ptr, str);
        if(u)
          l = rasqal_new_uri_literal(world, u);
        else
          failed = 1;
      } else {
        /* variable is not defined for this row */
        continue;
      }

      if(!l) {
        rasqal_free_row(row);
        failed = 1;
        goto tidy;
      }
      rasqal_row_set_value_at(row, column_i, l);
      /* free our copy of literal, rasqal_row has a reference */
      rasqal_free_literal(l);
    }

    raptor_sequence_push(seq, row);
  }

  tidy:
  if(failed) {
    if(seq) {
      raptor_free_sequence(seq);
      seq = NULL;
    }
    if(vars_seq) {
      raptor_free_sequence(vars_seq);
      vars_seq = NULL;
    }
  } else {
    if(vars_seq)
      *vars_seq_p = vars_seq;
  }
  
  return seq;
}


/**
 * rasqal_row_to_nodes:
 * @row: Result row
 *
 * INTERNAL - Turn the given result row literals into RDF strings, URIs or blank literals.
 * 
 * Return value: non-0 on failure
 */
int
rasqal_row_to_nodes(rasqal_row* row)
{
  int i;

  if(!row)
    return 1;
  
  for(i = 0; i < row->size; i++) {
    if(row->values[i]) {
      rasqal_literal* new_l;
      new_l = rasqal_literal_as_node(row->values[i]);
      if(!new_l)
        return -1;
      rasqal_free_literal(row->values[i]);
      row->values[i] = new_l;
    }
  }
  
  return 0;
}


/**
 * rasqal_row_set_values_from_variables_table:
 * @row: Result row
 * @vars_table: Variables table
 *
 * INTERNAL - Set the values of all variables in the row from the given variables table
 * 
 */
void
rasqal_row_set_values_from_variables_table(rasqal_row* row,
                                           rasqal_variables_table* vars_table)
{
  int i;
  
  for(i = 0; i < row->size; i++) {
    rasqal_literal *l;
    l = rasqal_variables_table_get_value(vars_table, i);
    if(row->values[i])
      rasqal_free_literal(row->values[i]);
    row->values[i] = rasqal_new_literal_from_literal(l);
  }
}


/**
 * rasqal_row_set_order_size:
 * @row: Result row
 * @order_size: number of order conditions
 *
 * INTERNAL - Initialise the row with space to handle @order_size order conditions being evaluated
 *
 * Return value: non-0 on failure 
 */
int
rasqal_row_set_order_size(rasqal_row *row, int order_size)
{
  row->order_size = order_size;
  if(row->order_size > 0) {
    row->order_values = RASQAL_CALLOC(rasqal_literal**, RASQAL_GOOD_CAST(size_t, row->order_size),
                                      sizeof(rasqal_literal*));
    if(!row->order_values) {
      row->order_size = -1;
      return 1;
    }
  }
  
  return 0;
}


/**
 * rasqal_row_expand_size:
 * @row: Result row
 * @size: number of variables
 *
 * INTERNAL - Expand the row to be able to handle @size variables
 *
 * Return value: non-0 on failure 
 */
int
rasqal_row_expand_size(rasqal_row *row, int size)
{
  rasqal_literal** nvalues;

  /* do not allow row size to contract & lose data */
  if(row->size > size)
    return 1;
  
  nvalues = RASQAL_CALLOC(rasqal_literal**, RASQAL_GOOD_CAST(size_t, size), sizeof(rasqal_literal*));
  if(!nvalues)
    return 1;
  memcpy(nvalues, row->values, RASQAL_GOOD_CAST(size_t, sizeof(rasqal_literal*) * RASQAL_GOOD_CAST(size_t, row->size)));
  RASQAL_FREE(array, row->values);
  row->values = nvalues;
  
  row->size = size;
  return 0;
}



/**
 * rasqal_row_bind_variables:
 * @row: Result row
 * @vars_table: Variables table
 *
 * INTERNAL - Bind the variable table vars with values in the row
 *
 * Return value: non-0 on failure
 */
int
rasqal_row_bind_variables(rasqal_row* row,
                          rasqal_variables_table* vars_table)
{
  int i;
  
  for(i = 0; i < row->size; i++) {
    rasqal_variable* v;
    
    v = rasqal_rowsource_get_variable_by_offset(row->rowsource, i);
    if(v) {
      rasqal_literal *value = row->values[i];
      if(value) {
        value = rasqal_new_literal_from_literal(value);
        if(!value)
          return 1;
      }
      
      /* it is OK to bind to NULL */
      rasqal_variable_set_value(v, value);
    }
  }

  return 0;
}


/**
 * rasqal_row_sequence_copy:
 * @row_sequence: sequence of #raptor_row
 *
 * INTERNAL - Deep copy a sequence of rows
 *
 * Return value: new sequence of all rows (may be size 0) or NULL on failure
 **/
raptor_sequence*
rasqal_row_sequence_copy(raptor_sequence *seq)
{
  raptor_sequence* new_seq;
  int i;
  rasqal_row* row;

  new_seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_row,
                                (raptor_data_print_handler)rasqal_row_print);
  if(!new_seq)
    return NULL;
  
  for(i = 0; (row = (rasqal_row*)raptor_sequence_get_at(seq, i)); i++) {
    row = rasqal_new_row_from_row(row);
      
    raptor_sequence_push(new_seq, row);
  }
  
  return new_seq;
}


void
rasqal_row_set_rowsource(rasqal_row* row, rasqal_rowsource* rowsource)
{
  if(!(row->flags & RASQAL_ROW_FLAG_WEAK_ROWSOURCE) && row->rowsource)
    rasqal_free_rowsource(row->rowsource);

  row->rowsource = rasqal_new_rowsource_from_rowsource(rowsource);
  row->flags = RASQAL_GOOD_CAST(unsigned int, RASQAL_GOOD_CAST(int, row->flags) & ~RASQAL_ROW_FLAG_WEAK_ROWSOURCE);
}

/* Set/reset a row's rowsource to a weak reference; one that should
 * NOT be freed.
 *
 *  *DANGEROUS* : should only be used by the rasqal_rowsequence_rowsource class
 */
void
rasqal_row_set_weak_rowsource(rasqal_row* row, rasqal_rowsource* rowsource)
{
  row->rowsource = rowsource;
  row->flags |= RASQAL_ROW_FLAG_WEAK_ROWSOURCE;
}

rasqal_variable*
rasqal_row_get_variable_by_offset(rasqal_row* row, int offset)
{
  if(offset < 0)
    return NULL;

  return rasqal_rowsource_get_variable_by_offset(row->rowsource, offset);
}

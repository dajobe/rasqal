/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_project.c - Rasqal variables projection rowsource class
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
  /* inner rowsource to project */
  rasqal_rowsource *rowsource;

  /* variable names to project input rows to */
  raptor_sequence* projection_variables;

  /* variables projection array: [output row var index]=input row var index */
  int* projection;

} rasqal_project_rowsource_context;


static int
rasqal_project_rowsource_init(rasqal_rowsource* rowsource, void *user_data)
{
  return 0;
}


static int
rasqal_project_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                         void *user_data)
{
  rasqal_project_rowsource_context* con;
  int size;
  int i;
  
  con = (rasqal_project_rowsource_context*)user_data; 

  if(rasqal_rowsource_ensure_variables(con->rowsource))
    return 1;

  rowsource->size = 0;

  size = raptor_sequence_size(con->projection_variables);

  con->projection = RASQAL_MALLOC(int*, RASQAL_GOOD_CAST(size_t,
                                                         sizeof(int) * RASQAL_GOOD_CAST(size_t, size)));
  if(!con->projection)
    return 1;
  
  for(i = 0; i < size; i++) {
    rasqal_variable* v;
    int offset;
    
    v = (rasqal_variable*)raptor_sequence_get_at(con->projection_variables, i);
    if(!v)
      break;
    offset = rasqal_rowsource_get_variable_offset_by_name(con->rowsource, 
                                                          v->name);
#ifdef RASQAL_DEBUG
    if(offset < 0)
      RASQAL_DEBUG2("Variable %s is in projection but not in input rowsource\n",
                    v->name);
#endif

    rasqal_rowsource_add_variable(rowsource, v);
    con->projection[i] = offset;
  }

  return 0;
}


static int
rasqal_project_rowsource_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_project_rowsource_context *con;
  con = (rasqal_project_rowsource_context*)user_data;

  if(con->rowsource)
    rasqal_free_rowsource(con->rowsource);
  
  if(con->projection_variables)
    raptor_free_sequence(con->projection_variables);
  
  if(con->projection)
    RASQAL_FREE(int*, con->projection);
  
  RASQAL_FREE(rasqal_project_rowsource_context, con);

  return 0;
}


static rasqal_row*
rasqal_project_rowsource_read_row(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_project_rowsource_context *con;
  rasqal_row *row = NULL;
  rasqal_row* nrow = NULL;
  
  con = (rasqal_project_rowsource_context*)user_data;

  row = rasqal_rowsource_read_row(con->rowsource);
  if(row) {
    int i;
    
    nrow = rasqal_new_row_for_size(rowsource->world, rowsource->size);
    if(!nrow)
      goto failed;

    rasqal_row_set_rowsource(nrow, rowsource);
    nrow->offset = row->offset;
      
    for(i = 0; i < rowsource->size; i++) {
      int offset = con->projection[i];
      if(offset >= 0)
        nrow->values[i] = rasqal_new_literal_from_literal(row->values[offset]);
      else {
        rasqal_variable* v;
        rasqal_query *query = rowsource->query;
        
        v = (rasqal_variable*)raptor_sequence_get_at(con->projection_variables, i);
        if(v && v->expression) {
          int error = 0;

          if(v->value)
            rasqal_free_literal(v->value);
          
          v->value = rasqal_expression_evaluate2(v->expression,
                                                 query->eval_context,
                                                 &error);
          if(error) {
            /* FIXME: Errors are ignored - check this */
#if 0
            goto failed;
#endif
          } else
            nrow->values[i] = rasqal_new_literal_from_literal(v->value);

        }
      }
    }

    rasqal_free_row(row);
    row = nrow;
  }
  
  return row;

  failed:
  if(row)
    rasqal_free_row(row);
  
  return NULL;
}


static int
rasqal_project_rowsource_reset(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_project_rowsource_context *con;
  con = (rasqal_project_rowsource_context*)user_data;

  return rasqal_rowsource_reset(con->rowsource);
}


static rasqal_rowsource*
rasqal_project_rowsource_get_inner_rowsource(rasqal_rowsource* rowsource,
                                             void *user_data, int offset)
{
  rasqal_project_rowsource_context *con;
  con = (rasqal_project_rowsource_context*)user_data;

  if(offset == 0)
    return con->rowsource;
  return NULL;
}


static const rasqal_rowsource_handler rasqal_project_rowsource_handler = {
  /* .version =          */ 1,
  "project",
  /* .init =             */ rasqal_project_rowsource_init,
  /* .finish =           */ rasqal_project_rowsource_finish,
  /* .ensure_variables = */ rasqal_project_rowsource_ensure_variables,
  /* .read_row =         */ rasqal_project_rowsource_read_row,
  /* .read_all_rows =    */ NULL,
  /* .reset =            */ rasqal_project_rowsource_reset,
  /* .set_requirements = */ NULL,
  /* .get_inner_rowsource = */ rasqal_project_rowsource_get_inner_rowsource,
  /* .set_origin =       */ NULL,
};


/**
 * rasqal_new_project_rowsource:
 * @world: query world
 * @query: query results object
 * @rowsource: input rowsource
 * @projection_variables: input sequence of #rasqal_variable
 *
 * INTERNAL - create a PROJECTion over input rowsource
 *
 * The @rowsource becomes owned by the new rowsource.
 *
 * Return value: new rowsource or NULL on failure
 */
rasqal_rowsource*
rasqal_new_project_rowsource(rasqal_world *world,
                             rasqal_query *query,
                             rasqal_rowsource* rowsource,
                             raptor_sequence* projection_variables)
{
  rasqal_project_rowsource_context *con;
  int flags = 0;
  
  if(!world || !query || !rowsource || !projection_variables)
    goto fail;
  
  con = RASQAL_CALLOC(rasqal_project_rowsource_context*, 1, sizeof(*con));
  if(!con)
    goto fail;

  con->rowsource = rowsource;
  con->projection_variables = rasqal_variable_copy_variable_sequence(projection_variables);

  return rasqal_new_rowsource_from_handler(world, query,
                                           con,
                                           &rasqal_project_rowsource_handler,
                                           query->vars_table,
                                           flags);

  fail:
  if(rowsource)
    rasqal_free_rowsource(rowsource);
  return NULL;
}




#endif /* not STANDALONE */



#ifdef STANDALONE

/* one more prototype */
int main(int argc, char *argv[]);


const char* const project_1_data_4x2_rows[] =
{
  /* 4 variable names and 2 rows */
  "a",   NULL, "b",   NULL, "c",   NULL, "d",   NULL,
  /* row 1 data */
  "foo", NULL, "bar", NULL, "baz", NULL, "fez", NULL,
  /* row 2 data */
  "bob", NULL, "sue", NULL, "kit", NULL, "kat", NULL,
  /* end of data */
  NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};
  
const char* const project_1_var_names[] = { "c", "b" };

#define EXPECTED_ROWS_COUNT 2
#define EXPECTED_COLUMNS_COUNT 2


int
main(int argc, char *argv[]) 
{
  const char *program = rasqal_basename(argv[0]);
  rasqal_rowsource *rowsource = NULL;
  rasqal_rowsource *input_rs = NULL;
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
  raptor_sequence* projection_seq = NULL;
  
  world = rasqal_new_world(); rasqal_world_open(world);
  
  query = rasqal_new_query(world, "sparql", NULL);
  
  vt = query->vars_table;

  /* 4 variables and 2 rows */
  vars_count = 4;
  seq = rasqal_new_row_sequence(world, vt, project_1_data_4x2_rows, vars_count,
                                &vars_seq);
  if(!seq) {
    fprintf(stderr,
            "%s: failed to create left sequence of %d vars\n", program,
            vars_count);
    failures++;
    goto tidy;
  }

  input_rs = rasqal_new_rowsequence_rowsource(world, query, vt, seq, vars_seq);
  if(!input_rs) {
    fprintf(stderr, "%s: failed to create left rowsource\n", program);
    failures++;
    goto tidy;
  }
  /* vars_seq and seq are now owned by input_rs */
  vars_seq = seq = NULL;
  
  projection_seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_variable,
                                       (raptor_data_print_handler)rasqal_variable_print);
  for(i = 0 ; i < EXPECTED_COLUMNS_COUNT; i++) {
    rasqal_variable* v;
    unsigned const char* name = (unsigned const char*)project_1_var_names[i];
    v = rasqal_variables_table_get_by_name(vt, RASQAL_VARIABLE_TYPE_NORMAL,
                                           name);
    /* returns SHARED pointer to variable */
    if(v) {
      v = rasqal_new_variable_from_variable(v);
      raptor_sequence_push(projection_seq, v);
    }
  }

  rowsource = rasqal_new_project_rowsource(world, query, input_rs, projection_seq);
  if(!rowsource) {
    fprintf(stderr, "%s: failed to create project rowsource\n", program);
    failures++;
    goto tidy;
  }
  /* input_rs and right_rs are now owned by rowsource */
  input_rs = NULL;

  seq = rasqal_rowsource_read_all_rows(rowsource);
  if(!seq) {
    fprintf(stderr,
            "%s: read_rows returned a NULL seq for a project rowsource\n",
            program);
    failures++;
    goto tidy;
  }
  count = raptor_sequence_size(seq);
  if(count != expected_count) {
    fprintf(stderr,
            "%s: read_rows returned %d rows for a project rowsource, expected %d\n",
            program, count, expected_count);
    failures++;
    goto tidy;
  }
  
  size = rasqal_rowsource_get_size(rowsource);
  if(size != expected_size) {
    fprintf(stderr,
            "%s: read_rows returned %d columns (variables) for a project rowsource, expected %d\n",
            program, size, expected_size);
    failures++;
    goto tidy;
  }
  for(i = 0; i < expected_size; i++) {
    rasqal_variable* v;
    const char* name = NULL;
    const char *expected_name = project_1_var_names[i];
    
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
  fprintf(DEBUG_FH, "result of projection:\n");
  rasqal_rowsource_print_row_sequence(rowsource, seq, DEBUG_FH);
#endif

  tidy:
  if(seq)
    raptor_free_sequence(seq);
  if(projection_seq)
    raptor_free_sequence(projection_seq);
  if(input_rs)
    rasqal_free_rowsource(input_rs);
  if(rowsource)
    rasqal_free_rowsource(rowsource);
  if(query)
    rasqal_free_query(query);
  if(world)
    rasqal_free_world(world);

  return failures;
}

#endif /* STANDALONE */

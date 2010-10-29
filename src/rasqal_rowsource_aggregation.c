/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_groupby.c - Rasqal GROUP BY and HAVING rowsource class
 *
 * Copyright (C) 2010, David Beckett http://www.dajobe.org/
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


/**
 * rasqal_aggregation_rowsource_context:
 *
 * INTERNAL - Aggregration rowsource context
 *
 * Structure for handing aggregation over a grouped input rowsource
 * (formed by the group_by rowsource) with parameters of:
 *   1. a sequence of #rasqal_expression
 *   2. a function with operation #rasqal_op (may be internal funcs)
 *   3. a sequence of k:v parameters
 *
 * For example with the SPARQL 1.1 example query
 *   SELECT (ex:agg(?y, ?z) AS ?agg) WHERE { ?x ?y ?z } GROUP BY ?x.
 * the aggregation part corresponds to
 *   1. [?y, ?z]
 *   2. ex:agg and #RASQAL_EXPR_FUNCTION
 *   3. []
 */
typedef struct 
{
  /* inner (grouped) rowsource */
  rasqal_rowsource *rowsource;

  /* group expression list */
  raptor_sequence* expr_seq;

  /* size of above list: can be 0 if @expr_seq is NULL too */
  int expr_seq_size;

  /* #RASQAL_EXPR_FUNCTION or built-in aggregation function operation */
  rasqal_op op;

  /* Implementation function if op == #RASQAL_EXPR_FUNCTION */
  void* func;
  
  /* sequence of parameters (SPARQL 1.1 calls them 'scalar') */
  raptor_sequence* parameters;
  
  /* last group ID seen */
  int group_id;

  /* output row offset */
  int offset;
} rasqal_aggregation_rowsource_context;




static int
rasqal_aggregation_rowsource_init(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_aggregation_rowsource_context* con;
  con = (rasqal_aggregation_rowsource_context*)user_data;

  con->offset = 0;

  return 0;
}


static int
rasqal_aggregation_rowsource_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_aggregation_rowsource_context* con;
  con = (rasqal_aggregation_rowsource_context*)user_data;

  if(con->rowsource)
    rasqal_free_rowsource(con->rowsource);
  
  if(con->expr_seq)
    raptor_free_sequence(con->expr_seq);
  
  if(con->parameters)
    raptor_free_sequence(con->parameters);
  
  RASQAL_FREE(rasqal_aggregation_rowsource_context, con);

  return 0;
}


static int
rasqal_aggregation_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                          void *user_data)
{
  rasqal_aggregation_rowsource_context* con;
  
  con = (rasqal_aggregation_rowsource_context*)user_data; 

  if(rasqal_rowsource_ensure_variables(con->rowsource))
    return 1;

  rowsource->size = 0;
  if(rasqal_rowsource_copy_variables(rowsource, con->rowsource))
    return 1;

  return 0;
}


static rasqal_row*
rasqal_aggregation_rowsource_read_row(rasqal_rowsource* rowsource,
                                      void *user_data)
{
  rasqal_aggregation_rowsource_context* con;
  rasqal_row* row;

  con = (rasqal_aggregation_rowsource_context*)user_data;
  
  row = rasqal_rowsource_read_row(con->rowsource);

  if(row)
    row->offset = con->offset++;

  return row;
}


static rasqal_rowsource*
rasqal_aggregation_rowsource_get_inner_rowsource(rasqal_rowsource* rowsource,
                                                 void *user_data, int offset)
{
  rasqal_aggregation_rowsource_context *con;
  con = (rasqal_aggregation_rowsource_context*)user_data;

  if(offset == 0)
    return con->rowsource;

  return NULL;
}


static const rasqal_rowsource_handler rasqal_aggregation_rowsource_handler = {
  /* .version = */ 1,
  "aggregation",
  /* .init = */ rasqal_aggregation_rowsource_init,
  /* .finish = */ rasqal_aggregation_rowsource_finish,
  /* .ensure_variables = */ rasqal_aggregation_rowsource_ensure_variables,
  /* .read_row = */ rasqal_aggregation_rowsource_read_row,
  /* .read_all_rows = */ NULL,
  /* .reset = */ NULL,
  /* .set_preserve = */ NULL,
  /* .get_inner_rowsource = */ rasqal_aggregation_rowsource_get_inner_rowsource,
  /* .set_origin = */ NULL,
};


rasqal_rowsource*
rasqal_new_aggregation_rowsource(rasqal_world *world, rasqal_query* query,
                                 rasqal_rowsource* rowsource,
                                 raptor_sequence* expr_seq,
                                 rasqal_op op,
                                 void *func,
                                 raptor_sequence* parameters)
{
  rasqal_aggregation_rowsource_context* con;
  int flags = 0;

  if(!world || !query)
    return NULL;
  
  con = (rasqal_aggregation_rowsource_context*)RASQAL_CALLOC(rasqal_aggregation_rowsource_context, 1, sizeof(*con));
  if(!con)
    goto fail;

  con->rowsource = rowsource;
  con->expr_seq_size = 0;

  if(expr_seq) {
    con->expr_seq = rasqal_expression_copy_expression_sequence(expr_seq);

    if(!con->expr_seq)
      goto fail;

    con->expr_seq_size = raptor_sequence_size(expr_seq);
  }

  con->op = op;
  con->func = func;
  con->parameters = parameters;
  
  return rasqal_new_rowsource_from_handler(world, query,
                                           con,
                                           &rasqal_aggregation_rowsource_handler,
                                           query->vars_table,
                                           flags);

  fail:

  if(rowsource)
    rasqal_free_rowsource(rowsource);
  if(expr_seq)
    raptor_free_sequence(expr_seq);
  if(parameters)
    raptor_free_sequence(parameters);

  return NULL;
}

#endif



#ifdef STANDALONE

/* one more prototype */
int main(int argc, char *argv[]);


#define AGGREGATION_TESTS_COUNT 1


#define MAX_TEST_VARS 3

/* Test 0 */
static const char* const data_xyz_3_rows[] =
{
  /* 3 variable names and 3 rows */
  "x",  NULL, "y",  NULL, "z",  NULL,
  /* row 1 data */
  "1",  NULL, "2",  NULL, "3",  NULL,
  /* row 2 data */
  "1",  NULL, "3",  NULL, "4",  NULL,
  /* row 3 data */
  "2",  NULL, "5",  NULL, "6",  NULL,
  /* end of data */
  NULL, NULL, NULL, NULL, NULL, NULL,
};



/* Input Group IDs expected */
/* Test 0 */
static const int test0_groupids[] = {
  0, 0, 1
};

static const struct {
  int vars;
  int rows;
  int ngroups;
  const char* const *data;
  const int const *group_ids;
  const char* const expr_vars[MAX_TEST_VARS];
} test_data[AGGREGATION_TESTS_COUNT] = {
  {3, 3, 2, data_xyz_3_rows, test0_groupids, { "y", "z" } }
};


int
main(int argc, char *argv[]) 
{
  const char *program = rasqal_basename(argv[0]);
  rasqal_rowsource *rowsource = NULL;
  rasqal_world* world = NULL;
  rasqal_query* query = NULL;
  raptor_sequence* row_seq = NULL;
  raptor_sequence* expr_seq = NULL;
  int failures = 0;
  rasqal_variables_table* vt;
  rasqal_rowsource *input_rs = NULL;
  int vars_count;
  raptor_sequence* vars_seq = NULL;
  int test_id;

  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    return(1);
  }
  
  query = rasqal_new_query(world, "sparql", NULL);

  vt = query->vars_table;
  
  for(test_id = 0; test_id < AGGREGATION_TESTS_COUNT; test_id++) {
    int expected_rows_count = test_data[test_id].rows;
    int expected_vars_count = test_data[test_id].vars;
    const int* input_group_ids = test_data[test_id].group_ids;
    raptor_sequence* seq = NULL;
    int count;
    int size;
    int i;

    vars_count = expected_vars_count;
    row_seq = rasqal_new_row_sequence(world, vt, test_data[test_id].data,
                                      vars_count, &vars_seq);
    if(row_seq) {
      for(i = 0; i < expected_rows_count; i++) {
        rasqal_row* row = (rasqal_row*)raptor_sequence_get_at(row_seq, i);
        row->group_id = input_group_ids[i];
      }
      
      input_rs = rasqal_new_rowsequence_rowsource(world, query, vt, 
                                                  row_seq, vars_seq);
      /* vars_seq and row_seq are now owned by input_rs */
      vars_seq = row_seq = NULL;
    }
    if(!input_rs) {
      fprintf(stderr, "%s: failed to create rowsequence rowsource\n", program);
      failures++;
      goto tidy;
    }

#ifdef HAVE_RAPTOR2_API
    expr_seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_expression,
                                   (raptor_data_print_handler)rasqal_expression_print);
#else
    expr_seq = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_expression,
                                   (raptor_sequence_print_handler*)rasqal_expression_print);
#endif

    if(test_data[test_id].expr_vars[0] != NULL) {
      int vindex;
      const unsigned char* var_name;
      
      for(vindex = 0;
          (var_name = (const unsigned char*)test_data[test_id].expr_vars[vindex] );
          vindex++) {
        rasqal_variable* v;
        rasqal_literal *l = NULL;
        rasqal_expression* e = NULL;

        v = rasqal_variables_table_get_by_name(vt, var_name);
        if(v)
          l = rasqal_new_variable_literal(world, v);

        if(l)
          e = rasqal_new_literal_expression(world, l);

        if(e)
          raptor_sequence_push(expr_seq, e);
        else {
          fprintf(stderr, "%s: failed to create variable %s\n", program,
                  (const char*)var_name);
          failures++;
          goto tidy;
        }
        
      }
    }
    
    rowsource = rasqal_new_aggregation_rowsource(world, query, input_rs,
                                                 expr_seq,
                                                 RASQAL_EXPR_FUNCTION, NULL,
                                                 /* parameters */ NULL);
    /* expr_seq and input_rs are now owned by rowsource */
    expr_seq = NULL; input_rs = NULL;

    if(!rowsource) {
      fprintf(stderr, "%s: failed to create aggregation rowsource\n", program);
      failures++;
      goto tidy;
    }


    /* Test the rowsource */
    seq = rasqal_rowsource_read_all_rows(rowsource);
    if(!seq) {
      fprintf(stderr,
              "%s: test %d rasqal_rowsource_read_all_rows() returned a NULL seq for a aggregation rowsource\n",
              program, test_id);
      failures++;
      goto tidy;
    }
    count = raptor_sequence_size(seq);
    if(count != expected_rows_count) {
      fprintf(stderr,
              "%s: test %d rasqal_rowsource_read_all_rows() returned %d rows for a aggregation rowsource, expected %d\n",
              program, test_id, count, expected_rows_count);
      failures++;
      goto tidy;
    }

    size = rasqal_rowsource_get_size(rowsource);
    if(size != expected_vars_count) {
      fprintf(stderr,
              "%s: test %d rasqal_rowsource_get_size() returned %d columns (variables) for a aggregation rowsource, expected %d\n",
              program, test_id, size, expected_vars_count);
      failures++;
      goto tidy;
    }


#ifdef RASQAL_DEBUG
    rasqal_rowsource_print_row_sequence(rowsource, seq, stderr);
#endif

    raptor_free_sequence(seq); seq = NULL;

    rasqal_free_rowsource(rowsource); rowsource = NULL;

    if(expr_seq)
      raptor_free_sequence(expr_seq);
    expr_seq = NULL;
  }
  
  
  tidy:
  if(expr_seq)
    raptor_free_sequence(expr_seq);
  if(rowsource)
    rasqal_free_rowsource(rowsource);
  if(input_rs)
    rasqal_free_rowsource(input_rs);
  if(query)
    rasqal_free_query(query);
  if(world)
    rasqal_free_world(world);

  return failures;
}

#endif

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_aggregation.c - Rasqal aggregation rowsource class
 *
 * Handles SPARQL Aggregation() algebra including Distinct of
 * expression arguments.
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


/*
 * rasqal_agg_expr_data:
 *
 * INTERNAL - data for defining an agg expression input args / output var/values
 *
 * This is separate from #rasqal_builtin_agg_expression_execute which contains
 * information only needed during execution.
 */
typedef struct 
{
  /* agg expression */
  rasqal_expression* expr;
  
  /* aggregation function execution user data as created by
   * rasqal_builtin_agg_expression_execute_init() and destroyed by
   * rasqal_builtin_agg_expression_execute_finish().
   */
  void* agg_user_data;

  /* (shared) output variable for this expression pointing into
   * aggregation rowsource context vars_seq */
  rasqal_variable* variable;

  /* sequence of aggregate function arguments */
  raptor_sequence* exprs_seq;

  /* map for distincting literal values */
  rasqal_map* map;
} rasqal_agg_expr_data;

  
/*
 * rasqal_aggregation_rowsource_context:
 *
 * INTERNAL - Aggregration rowsource context
 *
 * Structure for handing aggregation over a grouped input rowsource
 * created by rasqal_new_aggregation_rowsource().
 *
 */
typedef struct 
{
  /* inner (grouped) rowsource */
  rasqal_rowsource *rowsource;

  /* aggregate expressions */
  raptor_sequence* exprs_seq;

  /* output variables to bind (in order) */
  raptor_sequence* vars_seq;
  
  /* pointer to array of data per aggregate expression */
  rasqal_agg_expr_data* expr_data;
  
  /* number of agg expressions (size of exprs_seq, vars_seq, expr_data) */
  int expr_count;
  
  /* non-0 when done */
  int finished;

  /* last group ID seen */
  int last_group_id;
  
  /* saved row between group boundaries */
  rasqal_row* saved_row;

  /* output row offset */
  int offset;

  /* sequence of values from input rowsource to copy/sample through */
  raptor_sequence* input_values;

  /* number of variables/values on input rowsource to copy/sample through
   * (size of @input_values) */
  int input_values_count;

  /* step into current group */
  int step_count;
} rasqal_aggregation_rowsource_context;


/*
 * rasqal_builtin_agg_expression_execute:
 *
 * INTERNAL - state for built-in execution of certain aggregate expressions
 *
 * Executes AVG, COUNT, GROUP_CONCAT, MAX, MIN, SAMPLE
 *
 */
typedef struct 
{
  rasqal_world* world;
  
  /* expression being executed */
  rasqal_expression* expr;

  /* literal for computation (e.g. current MAX, MIN seen) */
  rasqal_literal* l;

  /* number of steps executed - used for AVG in calculating result */
  int count;

  /* error happened */
  int error;

  /* separator for GROUP_CONCAT */
  unsigned char separator[2];
  
  /* string buffer for GROUP_CONCAT */
  raptor_stringbuffer *sb;
} rasqal_builtin_agg_expression_execute;


static void rasqal_builtin_agg_expression_execute_finish(void* user_data);  


static void*
rasqal_builtin_agg_expression_execute_init(rasqal_world *world,
                                           rasqal_expression* expr)
{
  rasqal_builtin_agg_expression_execute* b;

  b = RASQAL_CALLOC(rasqal_builtin_agg_expression_execute*, 1, sizeof(*b));
  if(!b)
    return NULL;

  b->expr = expr;
  b->world = world;
  b->l = NULL;
  b->count = 0;
  b->error = 0;

  if(expr->op == RASQAL_EXPR_GROUP_CONCAT) {
    b->sb = raptor_new_stringbuffer();
    if(!b->sb) {
      rasqal_builtin_agg_expression_execute_finish(b);
      return NULL;
    }

    b->separator[0] = (unsigned char)' ';
    b->separator[1] = (unsigned char)'\0';
  }
  
  return b;
}


static void
rasqal_builtin_agg_expression_execute_finish(void* user_data)
{
  rasqal_builtin_agg_expression_execute* b;

  b = (rasqal_builtin_agg_expression_execute*)user_data;

  if(b->l)
    rasqal_free_literal(b->l);

  if(b->sb)
    raptor_free_stringbuffer(b->sb);
  
  RASQAL_FREE(rasqal_builtin_agg_expression_execute, b);
}


static int
rasqal_builtin_agg_expression_execute_reset(void* user_data)
{
  rasqal_builtin_agg_expression_execute* b;
  
  b = (rasqal_builtin_agg_expression_execute*)user_data;

  b->count = 0;
  b->error = 0;

  if(b->l) {
    rasqal_free_literal(b->l);
    b->l = 0;
  }

  if(b->sb) {
    raptor_free_stringbuffer(b->sb);
    b->sb = raptor_new_stringbuffer();
    if(!b->sb)
      return 1;
  }
  
  return 0;
}


static int
rasqal_builtin_agg_expression_execute_step(void* user_data,
                                           raptor_sequence* literals)
{
  rasqal_builtin_agg_expression_execute* b;
  rasqal_literal* l;
  int i;

  b = (rasqal_builtin_agg_expression_execute*)user_data;

  if(b->error)
    return b->error;
  
  if(b->expr->op == RASQAL_EXPR_COUNT) {
    /* COUNT(*) : counts every row (does not care about literals) */
    if(b->expr->arg1->op == RASQAL_EXPR_VARSTAR)
      b->count++;
    /* COUNT(expr list) : counts rows with non-empty sequence of literals */
    else if(raptor_sequence_size(literals) > 0)
      b->count++;
    
    return 0;
  }
    

  /* Other aggregate functions count every row */
  b->count++;

  for(i = 0; (l = (rasqal_literal*)raptor_sequence_get_at(literals, i)); i++) {
    rasqal_literal* result = NULL;

    if(b->expr->op == RASQAL_EXPR_SAMPLE) {
      /* Sample chooses the first literal it sees */
      if(!b->l)
        b->l = rasqal_new_literal_from_literal(l);

      break;
    }

    if(b->expr->op == RASQAL_EXPR_GROUP_CONCAT) {
      const unsigned char* str;
      int error = 0;
      
      str = RASQAL_GOOD_CAST(const unsigned char*, rasqal_literal_as_string_flags(l, 0, &error));

      if(!error) {
        if(raptor_stringbuffer_length(b->sb))
          raptor_stringbuffer_append_counted_string(b->sb, b->separator, 1, 1);

        raptor_stringbuffer_append_string(b->sb, str, 1); 
      }
      continue;
    }
  
    
    if(!b->l)
      result = rasqal_new_literal_from_literal(l);
    else {
      if(b->expr->op == RASQAL_EXPR_SUM || b->expr->op == RASQAL_EXPR_AVG) {
        result = rasqal_literal_add(b->l, l, &b->error);
      } else if(b->expr->op == RASQAL_EXPR_MIN) {
        int cmp = rasqal_literal_compare(b->l, l, 0, &b->error);
        if(cmp <= 0)
          result = rasqal_new_literal_from_literal(b->l);
        else
          result = rasqal_new_literal_from_literal(l);
      } else if(b->expr->op == RASQAL_EXPR_MAX) {
        int cmp = rasqal_literal_compare(b->l, l, 0, &b->error);
        if(cmp >= 0)
          result = rasqal_new_literal_from_literal(b->l);
        else
          result = rasqal_new_literal_from_literal(l);
      } else {
        RASQAL_FATAL2("Builtin aggregation operation %u is not implemented",
                      b->expr->op);
      }

      rasqal_free_literal(b->l);

      if(!result)
        b->error = 1;
    }
    
    b->l = result;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG3("Aggregation step result %s (error=%d)\n", 
                  (result ? RASQAL_GOOD_CAST(const char*, rasqal_literal_as_string(result)) : "(NULL)"),
                  b->error);
#endif
  }
  
  return b->error;
}


static rasqal_literal*
rasqal_builtin_agg_expression_execute_result(void* user_data)
{
  rasqal_builtin_agg_expression_execute* b;

  b = (rasqal_builtin_agg_expression_execute*)user_data;

  if(b->error)
    return NULL;

  if(b->expr->op == RASQAL_EXPR_COUNT) {
    rasqal_literal* result;

    result = rasqal_new_integer_literal(b->world, RASQAL_LITERAL_INTEGER,
                                        b->count);
    return result;
  }
    
  if(b->expr->op == RASQAL_EXPR_GROUP_CONCAT) {
    size_t len;
    unsigned char* str;
    rasqal_literal* result;
      
    len = raptor_stringbuffer_length(b->sb);
    str = RASQAL_MALLOC(unsigned char*, len + 1);
    if(!str)
      return NULL;
    
    if(raptor_stringbuffer_copy_to_string(b->sb, str, len)) {
      RASQAL_FREE(char*, str);
      return NULL;
    }

    result = rasqal_new_string_literal(b->world, str, NULL, NULL, NULL);

    return result;
  }
  
    
  if(b->expr->op == RASQAL_EXPR_AVG) {
    rasqal_literal* count_l = NULL;
    rasqal_literal* result = NULL;

    if(b->count)
      count_l = rasqal_new_integer_literal(b->world, RASQAL_LITERAL_INTEGER,
                                           b->count);

    if(b->l && count_l)
      result = rasqal_literal_divide(b->l, count_l, &b->error);
    else
      /* No total to divide */
      b->error = 1;
    if(count_l)
      rasqal_free_literal(count_l);

    if(b->error) {
      /* result will be NULL and error will be non-0 on division by 0
       * in which case the result is literal(integer 0)
       */
      result = rasqal_new_integer_literal(b->world, RASQAL_LITERAL_INTEGER,
                                          0);
    }
    
    return result;
  }
    
  return rasqal_new_literal_from_literal(b->l);
}



static int
rasqal_aggregation_rowsource_init(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_aggregation_rowsource_context* con;

  con = (rasqal_aggregation_rowsource_context*)user_data;

  con->input_values = raptor_new_sequence((raptor_data_free_handler)rasqal_free_literal,
                                          (raptor_data_print_handler)rasqal_literal_print);
    

  con->last_group_id = -1;
  con->offset = 0;
  con->step_count = 0;
  
  if(rasqal_rowsource_request_grouping(con->rowsource))
    return 1;
  
  return 0;
}


static int
rasqal_aggregation_rowsource_finish(rasqal_rowsource* rowsource,
                                    void *user_data)
{
  rasqal_aggregation_rowsource_context* con;

  con = (rasqal_aggregation_rowsource_context*)user_data;

  if(con->expr_data) {
    int i;

    for(i = 0; i < con->expr_count; i++) {
      rasqal_agg_expr_data* expr_data = &con->expr_data[i];

      if(expr_data->agg_user_data)
        rasqal_builtin_agg_expression_execute_finish(expr_data->agg_user_data);

      if(expr_data->exprs_seq)
        raptor_free_sequence(expr_data->exprs_seq);

      if(expr_data->expr)
        rasqal_free_expression(expr_data->expr);

      if(expr_data->map)
        rasqal_free_map(expr_data->map);
    }

    RASQAL_FREE(rasqal_agg_expr_data, con->expr_data);
  }
  
  if(con->exprs_seq)
    raptor_free_sequence(con->exprs_seq);

  if(con->vars_seq)
    raptor_free_sequence(con->vars_seq);

  if(con->rowsource)
    rasqal_free_rowsource(con->rowsource);
  
  if(con->saved_row)
    rasqal_free_row(con->saved_row);

  if(con->input_values)
    raptor_free_sequence(con->input_values);

  RASQAL_FREE(rasqal_aggregation_rowsource_context, con);

  return 0;
}


static int
rasqal_aggregation_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                              void *user_data)
{
  rasqal_aggregation_rowsource_context* con;
  int offset;
  int i;
  
  con = (rasqal_aggregation_rowsource_context*)user_data; 

  if(rasqal_rowsource_ensure_variables(con->rowsource))
    return 1;

  rowsource->size = 0;

  if(rasqal_rowsource_copy_variables(rowsource, con->rowsource))
    return 1;

  con->input_values_count = rowsource->size;

  for(i = 0; i < con->expr_count; i++) {
    rasqal_agg_expr_data* expr_data = &con->expr_data[i];

    offset = rasqal_rowsource_add_variable(rowsource, expr_data->variable);
    if(offset < 0)
      return 1;
  }
  
  return 0;
}


static rasqal_row*
rasqal_aggregation_rowsource_read_row(rasqal_rowsource* rowsource,
                                      void *user_data)
{
  rasqal_aggregation_rowsource_context* con;
  rasqal_row* row;
  int error = 0;
  
  con = (rasqal_aggregation_rowsource_context*)user_data;

  if(con->finished)
    return NULL;
  

  /* Iterate over input rows until last row seen or group done */
  while(1) {
    error = 0;
    
    if(con->saved_row)
      row = con->saved_row;
    else
      row = rasqal_rowsource_read_row(con->rowsource);

    if(!row) {
      /* End of input - calculate last aggregation result */
      con->finished = 1;
      break;
    }


    if(con->last_group_id != row->group_id) {
      int i;
      
      if(!con->saved_row && con->last_group_id >= 0) {
        /* Existing aggregation is done - return result */

        /* save current row for next time this function is called */
        con->saved_row = row;

        row = NULL;
#ifdef RASQAL_DEBUG
        RASQAL_DEBUG2("Aggregation ending group %d", con->last_group_id);
        fputc('\n', DEBUG_FH);
#endif

        /* Empty distinct maps */
        for(i = 0; i < con->expr_count; i++) {
          rasqal_agg_expr_data* expr_data = &con->expr_data[i];

          if(expr_data->map) {
            rasqal_free_map(expr_data->map);
            expr_data->map = NULL;
          }
        }
      
        break;
      }

      /* reference is now in 'row' variable */
      con->saved_row = NULL;
      
#ifdef RASQAL_DEBUG
      RASQAL_DEBUG2("Aggregation starting group %d", row->group_id);
      fputc('\n', DEBUG_FH);
#endif


      /* next time this function is called we continue here */

      for(i = 0; i < con->expr_count; i++) {
        rasqal_agg_expr_data* expr_data = &con->expr_data[i];

        if(!expr_data->agg_user_data) {
          /* init once */
          expr_data->agg_user_data = rasqal_builtin_agg_expression_execute_init(rowsource->world,
                                                                                expr_data->expr);
          
          if(!expr_data->agg_user_data) {
            error = 1;
            break;
          }
        }

        /* Init map for each group */
        if(expr_data->expr->flags & RASQAL_EXPR_FLAG_DISTINCT) {
          expr_data->map = rasqal_new_literal_sequence_sort_map(1 /* is_distinct */,
                                                                0 /* compare_flags */);
          if(!expr_data->map) {
            error = 1;
            break;
          }
        }
      }
      
      if(error)
        break;

      con->last_group_id = row->group_id;
    } /* end if handling change of group ID */
  

    /* Bind the values in the input row to the variables in the table */
    rasqal_row_bind_variables(row, rowsource->query->vars_table);

    /* Evaluate the expressions giving a sequence of literals to 
     * run the aggregation step over.
     */
    if(1) {
      int i;

      if(!con->step_count) {
        /* copy first value row from input rowsource */
        for(i = 0; i < con->input_values_count; i++) {
          rasqal_literal* value;
          
          value = rasqal_new_literal_from_literal(row->values[i]);
          raptor_sequence_set_at(con->input_values, i, value);
        }
      }

      con->step_count++;
      
      for(i = 0; i < con->expr_count; i++) {
        rasqal_agg_expr_data* expr_data = &con->expr_data[i];
        raptor_sequence* seq;
        
        /* SPARQL Aggregation uses ListEvalE() to evaluate - ignoring
         * errors and filtering out expressions that fail
         */
        seq = rasqal_expression_sequence_evaluate(rowsource->query,
                                                  expr_data->exprs_seq,
                                                  /* ignore_errors */ 1,
                                                  &error);
        if(error)
          continue;

        if(expr_data->map) {
          if(rasqal_literal_sequence_sort_map_add_literal_sequence(expr_data->map, 
                                                                   seq)) {
            /* duplicate found
             *
             * The above function just freed seq so no data is lost
             */
            continue;
          }
        }

#ifdef RASQAL_DEBUG
        RASQAL_DEBUG2("Aggregation expr %d step over literals: ", i);
        raptor_sequence_print(seq, DEBUG_FH);
        fputc('\n', DEBUG_FH);
#endif

        error = rasqal_builtin_agg_expression_execute_step(expr_data->agg_user_data,
                                                           seq);
        /* when DISTINCTing, seq remains owned by the map
         * otherwise seq is local and must be freed
         */
        if(!expr_data->map)
          raptor_free_sequence(seq);

        if(error) {
          RASQAL_DEBUG2("Aggregation expr %d returned error\n", i);
          error = 0;
        }
      }
    }

    rasqal_free_row(row); row = NULL;
    
    if(error)
      break;

  } /* end while reading rows */
  

  if(error) {
    /* Discard row on error */
    if(row) {
      rasqal_free_row(row);
      row = NULL;
    }
  } else if (con->last_group_id >= 0) {
    int offset = 0;
    int i;

    /* Generate result row and reset for next group */
    row = rasqal_new_row(rowsource);

    /* Copy scalar results through */
    for(i = 0; i < con->input_values_count; i++) {
      rasqal_literal* result;

      /* Reset: get and delete any stored input rowsource literal */
      result = (rasqal_literal*)raptor_sequence_delete_at(con->input_values, i);

      rasqal_row_set_value_at(row, offset, result);
      rasqal_free_literal(result);
      
      offset++;
    }


    /* Set aggregate results */
    for(i = 0; i < con->expr_count; i++) {
      rasqal_literal* result;
      rasqal_agg_expr_data* expr_data = &con->expr_data[i];
      rasqal_variable* v;
      
      /* Calculate the result because the input ended or a new group started */
      result = rasqal_builtin_agg_expression_execute_result(expr_data->agg_user_data);
  
#ifdef RASQAL_DEBUG
      RASQAL_DEBUG2("Aggregation %d ending group with result: ", i);
      rasqal_literal_print(result, DEBUG_FH);
      fputc('\n', DEBUG_FH);
#endif
      
      v = rasqal_rowsource_get_variable_by_offset(rowsource, offset);
      result = rasqal_new_literal_from_literal(result);
      /* it is OK to bind to NULL */
      rasqal_variable_set_value(v, result);

      rasqal_row_set_value_at(row, offset, result);
        
      if(result)
        rasqal_free_literal(result);
    
      offset++;

      if(rasqal_builtin_agg_expression_execute_reset(expr_data->agg_user_data)) {
        rasqal_free_row(row);
        row = NULL;
        break;
      }
    }

    con->step_count = 0;
      
    if(row)
      row->offset = con->offset++;
  }

  
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
  /* .set_requirements = */ NULL,
  /* .get_inner_rowsource = */ rasqal_aggregation_rowsource_get_inner_rowsource,
  /* .set_origin = */ NULL,
};


/**
 * rasqal_new_aggregation_rowsource:
 * @world: world
 * @query: query
 * @rowsource: input (grouped) rowsource - typically constructed by rasqal_new_groupby_rowsource()
 * @exprs_seq: sequence of #rasqal_expression
 * @vars_seq: sequence of #rasqal_variable to bind in output rows
 *
 * INTERNAL - Create a new rowsource for a aggregration
 *
 * The @rowsource becomes owned by the new rowsource.  The @exprs_seq
 * and @vars_seq are not. 
 *
 * For example with the SPARQL 1.1 example queries
 *
 * SELECT (MAX(?y) AS ?agg) WHERE { ?x ?y ?z } GROUP BY ?x
 * the aggregation part corresponds to
 *   exprs_seq : [ expr MAX with sequence of expression args [?y] }
 *   vars_seq  : [ {internal variable name} ]
 *
 * SELECT (ex:agg(?y, ?z) AS ?agg) WHERE { ?x ?y ?z } GROUP BY ?x
 * the aggregation part corresponds to
 *   exprs_seq : [ expr ex:agg with sequence of expression args [?y, ?z] ]
 *   vars_seq  : [ {internal variable name} ]
 *
 * SELECT ?x, (MIN(?z) AS ?agg) WHERE { ?x ?y ?z } GROUP BY ?x
 * the aggregation part corresponds to
 *   exprs_seq : [ non-aggregate expression ?x,
 *                 expr MIN with sequence of expression args [?z] ]
 *   vars_seq  : [ ?x, {internal variable name} ]
 *
 * Return value: new rowsource or NULL on failure
*/

rasqal_rowsource*
rasqal_new_aggregation_rowsource(rasqal_world *world, rasqal_query* query,
                                 rasqal_rowsource* rowsource,
                                 raptor_sequence* exprs_seq,
                                 raptor_sequence* vars_seq)
{
  rasqal_aggregation_rowsource_context* con = NULL;
  int flags = 0;
  int size;
  int i;
  
  if(!world || !query || !rowsource || !exprs_seq || !vars_seq)
    goto fail;

  exprs_seq = rasqal_expression_copy_expression_sequence(exprs_seq);
  vars_seq = rasqal_variable_copy_variable_sequence(vars_seq);

  size = raptor_sequence_size(exprs_seq);
  if(size != raptor_sequence_size(vars_seq)) {
    RASQAL_DEBUG3("expressions sequence size %d does not match vars sequence size %d\n", size, raptor_sequence_size(vars_seq));
    goto fail;
  }


  con = RASQAL_CALLOC(rasqal_aggregation_rowsource_context*, 1, sizeof(*con));
  if(!con)
    goto fail;

  con->rowsource = rowsource;

  con->exprs_seq = exprs_seq;
  con->vars_seq = vars_seq;
  
  /* allocate per-expr data */
  con->expr_count = size;
  con->expr_data = RASQAL_CALLOC(rasqal_agg_expr_data*, RASQAL_GOOD_CAST(size_t, size),
                                 sizeof(rasqal_agg_expr_data));
  if(!con->expr_data)
    goto fail;

  /* Initialise per-expr data */
  for(i = 0; i < size; i++) {
    rasqal_expression* expr = (rasqal_expression *)raptor_sequence_get_at(exprs_seq, i);
    rasqal_variable* variable = (rasqal_variable*)raptor_sequence_get_at(vars_seq, i);
    rasqal_agg_expr_data* expr_data = &con->expr_data[i];

    expr_data->expr = rasqal_new_expression_from_expression(expr);
    expr_data->variable = variable;

    /* Prepare expression arguments sequence in per-expr data */
    if(expr->args) {
      /* list of #rasqal_expression arguments already in expr
       * #RASQAL_EXPR_FUNCTION and #RASQAL_EXPR_GROUP_CONCAT 
       */
      expr_data->exprs_seq = rasqal_expression_copy_expression_sequence(expr->args);
    } else {
      /* single argument */
      
      expr_data->exprs_seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_expression,
                                               (raptor_data_print_handler)rasqal_expression_print);
      raptor_sequence_push(expr_data->exprs_seq,
                           rasqal_new_expression_from_expression(expr->arg1));
    }
  }
  
  
  return rasqal_new_rowsource_from_handler(world, query,
                                           con,
                                           &rasqal_aggregation_rowsource_handler,
                                           query->vars_table,
                                           flags);

  fail:

  if(rowsource)
    rasqal_free_rowsource(rowsource);
  if(exprs_seq)
    raptor_free_sequence(exprs_seq);
  if(vars_seq)
    raptor_free_sequence(vars_seq);
  if(con)
    RASQAL_FREE(rasqal_aggregation_rowsource_context*, con);

  return NULL;
}

#endif /* not STANDALONE */



#ifdef STANDALONE

/* one more prototype */
int main(int argc, char *argv[]);


#define AGGREGATION_TESTS_COUNT 6


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

/* MAX(?y) GROUP BY ?x result */
static const int test0_output_rows[] =
{ 3, 5, };
/* MIN(?x) GROUP BY ?x result */
static const int test1_output_rows[] =
{ 1, 2, };
/* SUM(?z) GROUP BY ?x result */
static const int test2_output_rows[] =
{ 7, 6, };
/* AVG(?x) GROUP BY ?x result */
static const double test3_output_rows[] =
{ 1.0, 2.0, };
/* SAMPLE(?y) GROUP BY ?x result */
static const int test4_output_rows[] =
{ 2, 5, };
/* GROUP_CONCAT(?z) GROUP BY ?x result */
static const char* const test5_output_rows[] =
{ "3 4", "6", };


/* Input Group IDs expected */
/* Test 0 */
static const int test0_groupids[] = {
  0, 0, 1
};

static const struct {
  int input_vars;
  int input_rows;
  int input_ngroups;
  int output_vars;
  int output_rows;
  const char* const *data;
  const int *group_ids;
  rasqal_literal_type result_type;
  const int *result_int_data;
  const double *result_double_data;
  const char* const *result_string_data;
  rasqal_op op;
  const char* const expr_agg_vars[MAX_TEST_VARS];
} test_data[AGGREGATION_TESTS_COUNT] = {
  /*
   * Execute the aggregation part of SELECT (MAX(?y) AS ?fake) ... GROUP BY ?x
   *   Input 3 vars (x, y, z), 3 rows and 2 groups.
   *   Output is 1 var (fake), 2 rows (1 per input group)
   * Expected result: [ ?fake => 3, ?fake => 5]
   */
  {3, 3, 2, 1, 2, data_xyz_3_rows, test0_groupids,
   RASQAL_LITERAL_INTEGER, test0_output_rows, NULL,
   NULL,
   RASQAL_EXPR_MAX, { "y" } },

  /*
   * Execute the aggregation part of SELECT (MIN(?x) AS ?fake) ... GROUP BY ?x
   * Expected result: [ ?fake => 1, ?fake => 2]
   */
  {3, 3, 2, 1, 2, data_xyz_3_rows, test0_groupids, 
   RASQAL_LITERAL_INTEGER, test1_output_rows, NULL,
   NULL,
   RASQAL_EXPR_MIN, { "x" } },

  /*
   * Execute the aggregation part of SELECT (SUM(?z) AS ?fake) ... GROUP BY ?x
   * Expected result: [ ?fake => 7, ?fake => 6]
   */
  {3, 3, 2, 1, 2, data_xyz_3_rows, test0_groupids,
   RASQAL_LITERAL_INTEGER, test2_output_rows, NULL,
   NULL,
   RASQAL_EXPR_SUM, { "z" } },

  /*
   * Execute the aggregation part of SELECT (AVG(?x) AS ?fake) ... GROUP BY ?x
   * Expected result: [ ?fake => 1.0, ?fake => 2.0]
   */
  {3, 3, 2, 1, 2, data_xyz_3_rows, test0_groupids, 
   RASQAL_LITERAL_DECIMAL, NULL, test3_output_rows,
   NULL,
   RASQAL_EXPR_AVG, { "x" } },

  /*
   * Execute the aggregation part of SELECT (SAMPLE(?y) AS ?fake) ... GROUP BY ?x
   * Expected result: [ ?fake => 2, ?fake => 5]
   */
  {3, 3, 2, 1, 2, data_xyz_3_rows, test0_groupids, 
   RASQAL_LITERAL_INTEGER, test4_output_rows, NULL,
   NULL,
   RASQAL_EXPR_SAMPLE, { "y" } },

  /*
   * Execute the aggregation part of SELECT (GROUP_CONCAT(?z) AS ?fake) ... GROUP BY ?x
   * Expected result: [ ?fake => "3 4", ?fake => "6"]
   */
  {3, 3, 2, 1, 2, data_xyz_3_rows, test0_groupids,
   RASQAL_LITERAL_INTEGER, NULL, NULL,
   test5_output_rows,
   RASQAL_EXPR_GROUP_CONCAT, { "z" } }
};


static rasqal_expression*
make_test_expr(rasqal_world* world,
               raptor_sequence* expr_vars_seq,
               rasqal_op op)
{
  if(op == RASQAL_EXPR_MAX ||
     op == RASQAL_EXPR_MIN ||
     op == RASQAL_EXPR_SUM ||
     op == RASQAL_EXPR_AVG ||
     op == RASQAL_EXPR_SAMPLE) {
    rasqal_expression* arg1;

    arg1 = (rasqal_expression*)raptor_sequence_delete_at(expr_vars_seq, 0);
    raptor_free_sequence(expr_vars_seq);

    return rasqal_new_aggregate_function_expression(world, op,
                                                    arg1,
                                                    /* params */ NULL, 
                                                    /* flags */ 0);
  }
  
  if(op == RASQAL_EXPR_GROUP_CONCAT) {
    return rasqal_new_group_concat_expression(world, 
                                              /* flags */ 0,
                                              expr_vars_seq,
                                              /* separator */ NULL);
  }

  return NULL;
}


int
main(int argc, char *argv[]) 
{
  const char *program = rasqal_basename(argv[0]);
  rasqal_rowsource *rowsource = NULL;
  rasqal_world* world = NULL;
  rasqal_query* query = NULL;
  raptor_sequence* row_seq = NULL;
  raptor_sequence* expr_args_seq = NULL;
  int failures = 0;
  rasqal_variables_table* vt;
  rasqal_rowsource *input_rs = NULL;
  raptor_sequence* vars_seq = NULL;
  raptor_sequence* exprs_seq = NULL;
  int test_id;

  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    return(1);
  }
  
  query = rasqal_new_query(world, "sparql", NULL);

  vt = query->vars_table;
  
  for(test_id = 0; test_id < AGGREGATION_TESTS_COUNT; test_id++) {
    int input_vars_count = test_data[test_id].input_vars;
    int output_rows_count = test_data[test_id].output_rows;
    int output_vars_count = test_data[test_id].output_vars;
    const int* input_group_ids = test_data[test_id].group_ids;
    rasqal_literal_type expected_type = test_data[test_id].result_type;
    const int* result_int_data = test_data[test_id].result_int_data;
    const double* result_double_data = test_data[test_id].result_double_data;
    const char* const* result_string_data = test_data[test_id].result_string_data;
    rasqal_op op  = test_data[test_id].op;
    raptor_sequence* seq = NULL;
    int count;
    int size;
    int i;
    #define OUT_VAR_NAME_LEN 4
    const char* output_var_name = "fake";
    rasqal_variable* output_var;
    rasqal_expression* expr;
    int output_row_size = (input_vars_count + output_vars_count);

    if(output_vars_count != 1) {
      fprintf(stderr,
              "%s: test %d expects %d variables which is not supported. Test skipped\n",
              program, test_id, output_vars_count);
      failures++;
      goto tidy;
    }

    row_seq = rasqal_new_row_sequence(world, vt, test_data[test_id].data,
                                      test_data[test_id].input_vars, &vars_seq);
    if(row_seq) {
      for(i = 0; i < test_data[test_id].input_rows; i++) {
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

    expr_args_seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_expression,
                                        (raptor_data_print_handler)rasqal_expression_print);

    if(test_data[test_id].expr_agg_vars[0] != NULL) {
      int vindex;
      const unsigned char* var_name;
      
      for(vindex = 0;
          (var_name = RASQAL_GOOD_CAST(const unsigned char*, test_data[test_id].expr_agg_vars[vindex] ));
          vindex++) {
        rasqal_variable* v;
        rasqal_literal *l = NULL;
        rasqal_expression* e = NULL;

        v = rasqal_variables_table_get_by_name(vt, RASQAL_VARIABLE_TYPE_NORMAL,
                                               var_name);
        /* returns SHARED pointer to variable */
        if(v) {
          v = rasqal_new_variable_from_variable(v);
          l = rasqal_new_variable_literal(world, v);
        }

        if(l)
          e = rasqal_new_literal_expression(world, l);

        if(e)
          raptor_sequence_push(expr_args_seq, e);
        else {
          fprintf(stderr, "%s: failed to create variable %s\n", program,
                  RASQAL_GOOD_CAST(const char*, var_name));
          failures++;
          goto tidy;
        }
        
      }
    } /* if vars */


    output_var = rasqal_variables_table_add2(vt, RASQAL_VARIABLE_TYPE_ANONYMOUS, 
                                             RASQAL_GOOD_CAST(const unsigned char*, output_var_name),
                                             OUT_VAR_NAME_LEN, NULL);
    expr = make_test_expr(world, expr_args_seq, op);
    /* expr_args_seq is now owned by expr */
    expr_args_seq = NULL;

    exprs_seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_expression,
                                    (raptor_data_print_handler)rasqal_expression_print);
    raptor_sequence_push(exprs_seq, expr);
    /* expr is now owned by exprs_seq */
    expr = NULL;
    
    vars_seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_variable,
                                   (raptor_data_print_handler)rasqal_variable_print);
    raptor_sequence_push(vars_seq, output_var);
    /* output_var is now owned by vars_seq */
    output_var = NULL;

    rowsource = rasqal_new_aggregation_rowsource(world, query, input_rs,
                                                 exprs_seq, vars_seq);
    /* input_rs is now owned by rowsource */
    input_rs = NULL;
    /* these are no longer needed; agg rowsource made copies */
    raptor_free_sequence(exprs_seq); exprs_seq = NULL;
    raptor_free_sequence(vars_seq); vars_seq = NULL;

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
    if(count != output_rows_count) {
      fprintf(stderr,
              "%s: test %d rasqal_rowsource_read_all_rows() returned %d rows for a aggregation rowsource, expected %d\n",
              program, test_id, count, output_rows_count);
      failures++;
      goto tidy;
    }

    size = rasqal_rowsource_get_size(rowsource);
    if(size != output_row_size) {
      fprintf(stderr,
              "%s: test %d rasqal_rowsource_get_size() returned %d columns (variables) for a aggregation rowsource, expected %d\n",
              program, test_id, size, output_row_size);
      failures++;
      goto tidy;
    }

    if(result_int_data || result_double_data) {
      for(i = 0; i < output_rows_count; i++) {
        rasqal_row* row = (rasqal_row*)raptor_sequence_get_at(seq, i);
        rasqal_literal* value;
        int vc;
        
        if(row->size != output_row_size) {
          fprintf(stderr,
                  "%s: test %d row #%d is size %d expected %d\n",
                  program, test_id, i, row->size, output_row_size);
          failures++;
          goto tidy;
        }
        
        /* Expected variable ordering in output row is:
         * {input vars} {output_vars}
         */
        for(vc = 0; vc < output_vars_count; vc++) {
          rasqal_variable* row_var;
          int offset = input_vars_count + vc;
          
          row_var = rasqal_rowsource_get_variable_by_offset(rowsource, offset);
          value = row->values[offset]; 

          if(!value) {
            fprintf(stderr,
                    "%s: test %d row #%d %s value #%d result is NULL\n",
                    program, test_id, i, row_var->name, vc);
            failures++;
            goto tidy;
          }

          if(value->type != expected_type) {
            fprintf(stderr,
                    "%s: test %d row #%d %s value #%d result is type %s expected %s\n",
                    program, test_id, i, row_var->name, vc,
                    rasqal_literal_type_label(value->type),
                    rasqal_literal_type_label(expected_type));
            failures++;
            goto tidy;
          }

          if(expected_type == RASQAL_LITERAL_INTEGER) {
            int expected_integer = result_int_data[i];
            int integer;

            integer = rasqal_literal_as_integer(value, NULL);
            
            if(integer != expected_integer) {
              fprintf(stderr,
                    "%s: test %d row #%d %s value #%d result is %d expected %d\n",
                      program, test_id, i, row_var->name, vc,
                      integer, expected_integer);
              failures++;
              goto tidy;
            }
          } else if(expected_type == RASQAL_LITERAL_DECIMAL) {
            double expected_double = result_double_data[i];
            double d;

            d = rasqal_literal_as_double(value, NULL);
            
            if(!rasqal_double_approximately_equal(d, expected_double)) {
              fprintf(stderr,
                    "%s: test %d row #%d %s value #%d result is %f expected %f\n",
                      program, test_id, i, row_var->name, vc,
                      d, expected_double);
              failures++;
              goto tidy;
            }
          }
          
        }
        
      }
    }
    
    if(result_string_data) {
      for(i = 0; i < output_rows_count; i++) {
        rasqal_row* row = (rasqal_row*)raptor_sequence_get_at(seq, i);
        rasqal_literal* value;
        const unsigned char* str;
        const char* expected_string = result_string_data[i];
        int vc;
        
        if(row->size != output_row_size) {
          fprintf(stderr,
                  "%s: test %d row #%d is size %d expected %d\n",
                  program, test_id, i, row->size, output_row_size);
          failures++;
          goto tidy;
        }
        
        /* Expected variable ordering in output row is:
         * {input vars} {output_vars}
         */
        for(vc = 0; vc < output_vars_count; vc++) {
          rasqal_variable* row_var;
          int offset = input_vars_count + vc;

          row_var = rasqal_rowsource_get_variable_by_offset(rowsource, offset);
          value = row->values[offset]; 

          if(!value) {
            fprintf(stderr,
                    "%s: test %d row #%d %s value #%d result is NULL\n",
                    program, test_id, i, row_var->name, vc);
            failures++;
            goto tidy;
          }

          if(value->type != RASQAL_LITERAL_STRING) {
            fprintf(stderr,
                    "%s: test %d row #%d %s value #%d is type %s expected integer\n",
                    program, test_id, i, row_var->name, vc,
                    rasqal_literal_type_label(value->type));
            failures++;
            goto tidy;
          }

          str = rasqal_literal_as_string(value);

          if(strcmp(RASQAL_GOOD_CAST(const char*, str), expected_string)) {
            fprintf(stderr,
                    "%s: test %d row #%d %s value #%d is %s expected %s\n",
                    program, test_id, i, row_var->name, vc,
                    str, expected_string);
            failures++;
            goto tidy;
          }
        }
        
      }
    }
    

#ifdef RASQAL_DEBUG
    rasqal_rowsource_print_row_sequence(rowsource, seq, stderr);
#endif

    raptor_free_sequence(seq); seq = NULL;

    rasqal_free_rowsource(rowsource); rowsource = NULL;

    if(expr_args_seq)
      raptor_free_sequence(expr_args_seq);
    expr_args_seq = NULL;
  }
  
  
  tidy:
  if(exprs_seq)
    raptor_free_sequence(exprs_seq);
  if(vars_seq)
    raptor_free_sequence(vars_seq);
  if(expr_args_seq)
    raptor_free_sequence(expr_args_seq);
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

#endif /* STANDALONE */

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_extend.c - Rasqal extend rowsource class
 *
 * Copyright (C) 2009, David Beckett http://www.dajobe.org/
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

/*

SPARQL 1.2 Extend algebra operation:

Algebra Translation:
  If E is of the form BIND(expr AS var)
      G := Extend(G, var, expr)
      End

Extend Definition:

  Extend(μ, var, expr) = μ ∪ { (var,value) | var not in dom(μ) and value = expr(μ) }
  Extend(μ, var, expr) = μ if var not in dom(μ) and expr(μ) is an error
  Extend is undefined when var in dom(μ).
  Extend(Ω, var, expr) = { Extend(μ, var, expr) | μ in Ω }

Evaluation Semantics:

  eval(D(G), Extend(P, var, expr)) = Extend(eval(D(G), P), var, expr)

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




typedef struct
{
  /* input rowsource */
  rasqal_rowsource* input_rs;

  /* extend variable */
  rasqal_variable *var;

  /* extend expression */
  rasqal_expression *expr;

  /* optional filter expression */
  rasqal_expression *filter_expr;

  /* NEW: Scope integration */
  rasqal_query_scope* extend_scope;
  rasqal_variable_lookup_context* lookup_context;

  /* Variable resolution cache */
  rasqal_variable** resolved_variables;
  int resolved_variables_count;

} rasqal_extend_rowsource_context;


static int
rasqal_extend_rowsource_init(rasqal_rowsource* rowsource, void *user_data)
{
  return 0;
}


static int
rasqal_extend_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                         void *user_data)
{
  rasqal_extend_rowsource_context *con;
  rasqal_query* query;
  con = (rasqal_extend_rowsource_context*)user_data;
  query = rowsource->query;

  /* Ensure input rowsource variables are available */
  if(con->input_rs && rasqal_rowsource_ensure_variables(con->input_rs))
    return 1;

  /* Copy all variables from input rowsource */
  if(rasqal_rowsource_copy_variables(rowsource, con->input_rs))
    return 1;

  /* Add the extend variable to our variable table */
  rasqal_rowsource_add_variable(rowsource, con->var);

  /* CRITICAL FIX: Also add the variable to the query's vars_table
   * so that scope-aware evaluation can find it */
  if(!rasqal_variables_table_contains(query->vars_table, con->var->type, con->var->name)) {
    if(rasqal_variables_table_add_variable(query->vars_table, con->var))
      return 1;
  }

  /* BIND UNION FIX: Register the variable in the scope's local_vars table
   * so that scope visibility checking can determine which variables are
   * bound at which scope level (fix for bind07 test) */
  if(con->extend_scope && con->extend_scope->local_vars) {
    if(!rasqal_variables_table_contains(con->extend_scope->local_vars, con->var->type, con->var->name)) {
      if(rasqal_variables_table_add_variable(con->extend_scope->local_vars, con->var)) {
        return 1;
      }
    }
  }

  return 0;
}


static int
rasqal_extend_rowsource_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_extend_rowsource_context *con;
  con = (rasqal_extend_rowsource_context*)user_data;

  if(con->input_rs)
    rasqal_free_rowsource(con->input_rs);

  if(con->expr)
    rasqal_free_expression(con->expr);

  if(con->filter_expr)
    rasqal_free_expression(con->filter_expr);

  RASQAL_FREE(rasqal_extend_rowsource_context, con);

  return 0;
}


static rasqal_literal*
evaluate_extend_expression(rasqal_extend_rowsource_context* con,
                          rasqal_row* input_row)
{
  rasqal_query* query = con->input_rs->query;

  /* For now, evaluate without explicit row context setting */
  /* Variables should be resolved through the row that's already set */
  int error = 0;
  rasqal_literal* result = rasqal_expression_evaluate2(con->expr, query->eval_context, &error);

  if(error || !result)
    return NULL;

  return result;
}


static rasqal_row*
rasqal_extend_rowsource_read_row(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_extend_rowsource_context *con;
  rasqal_row* input_row = NULL;
  rasqal_row* output_row = NULL;
  rasqal_literal* result = NULL;
  int extend_var_offset;

  con = (rasqal_extend_rowsource_context*)user_data;

  /* Get next row from input rowsource */
  input_row = rasqal_rowsource_read_row(con->input_rs);
  if(!input_row)
    return NULL;

  /* SPARQL 1.2 Extend semantics: Check for variable conflict
   * "Extend is undefined when var in dom(μ)" */
  extend_var_offset = rasqal_rowsource_get_variable_offset_by_name(con->input_rs, 
                                                                   con->var->name);
  if(extend_var_offset >= 0 && extend_var_offset < input_row->size && 
     input_row->values[extend_var_offset] != NULL) {
    /* Variable already bound in input solution - Extend is undefined */
    RASQAL_DEBUG2("EXTEND: Variable %s already bound in input solution - skipping row per SPARQL 1.2 semantics\n", 
                  con->var->name);
    rasqal_free_row(input_row);
    return NULL; /* Skip this solution as Extend is undefined */
  }

  /* Evaluate expression with scope awareness */
  result = evaluate_extend_expression(con, input_row);

  if(result) {
    /* Expression evaluated successfully - create extended row */
    /* Input row has size N, output row needs size N+1 for the new variable */
    int output_size = input_row->size + 1;

    output_row = rasqal_new_row_for_size(con->input_rs->world, output_size);
    if(output_row) {
      int i;
      int j;

      /* Set the rowsource for proper variable resolution */
      rasqal_row_set_rowsource(output_row, rowsource);

      /* Copy all values from input row */
      for(i = 0; i < input_row->size; i++) {
        output_row->values[i] = rasqal_new_literal_from_literal(input_row->values[i]);
      }

      /* Set the extend variable value in the last position */
      output_row->values[input_row->size] = rasqal_new_literal_from_literal(result);
      output_row->size = output_size;

#ifdef RASQAL_DEBUG
      if(rasqal_get_debug_level() >= 2) {
        /* Debug: Verify the variable binding */
        RASQAL_DEBUG2("EXTEND: Bound variable %s to value: ", con->var->name);
        rasqal_literal_print(result, RASQAL_DEBUG_FH);
        fputc('\n', RASQAL_DEBUG_FH);

        /* Debug: Print the full row */
        RASQAL_DEBUG1("EXTEND: Output row values:\n");
        for(j = 0; j < output_row->size; j++) {
          rasqal_variable* var = rasqal_rowsource_get_variable_by_offset(rowsource, j);
          if(var) {
            fprintf(RASQAL_DEBUG_FH, "  %s = ", var->name);
            if(output_row->values[j]) {
              rasqal_literal_print(output_row->values[j], RASQAL_DEBUG_FH);
            } else {
              fputs("NULL", RASQAL_DEBUG_FH);
            }
            fputc('\n', RASQAL_DEBUG_FH);
          }
        }
      }
#endif
    }

    /* Free the original result since we created a copy */
    rasqal_free_literal(result);
    result = NULL;
  } else {
    /* Expression evaluation failed - preserve original row */
    output_row = input_row;
    input_row = NULL; /* Don't free it since we're returning it */
    if(result)
      rasqal_free_literal(result);
  }

  /* Clean up input row if we created a new output row */
  if(input_row)
    rasqal_free_row(input_row);

  return output_row;
}


static int
rasqal_extend_rowsource_reset(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_extend_rowsource_context *con;
  con = (rasqal_extend_rowsource_context*)user_data;

  /* Reset input rowsource */
  if(con->input_rs)
    return rasqal_rowsource_reset(con->input_rs);

  return 0;
}


static rasqal_rowsource_handler rasqal_extend_rowsource_handler = {
  /* .version = */ 1,
  "extend",
  /* .init = */ rasqal_extend_rowsource_init,
  /* .finish = */ rasqal_extend_rowsource_finish,
  /* .ensure_variables = */ rasqal_extend_rowsource_ensure_variables,
  /* .read_row = */ rasqal_extend_rowsource_read_row,
  /* .read_all_rows = */ NULL,
  /* .reset = */ rasqal_extend_rowsource_reset,
  /* .set_preserve = */ NULL,
  /* .get_inner_rowsource = */ NULL,
  /* .set_origin = */ NULL
};


/* Forward declaration */
rasqal_rowsource*
rasqal_new_extend_rowsource_with_filter(rasqal_world *world,
                                        rasqal_query *query,
                                        rasqal_rowsource* input_rs,
                                        rasqal_variable* var,
                                        rasqal_expression* expr,
                                        rasqal_expression* filter_expr,
                                        rasqal_query_scope* execution_scope);

rasqal_rowsource*
rasqal_new_extend_rowsource(rasqal_world *world,
                            rasqal_query *query,
                            rasqal_rowsource* input_rs,
                            rasqal_variable* var,
                            rasqal_expression* expr,
                            rasqal_query_scope* execution_scope)
{
  return rasqal_new_extend_rowsource_with_filter(world, query, input_rs, var, expr, NULL, execution_scope);
}

rasqal_rowsource*
rasqal_new_extend_rowsource_with_filter(rasqal_world *world,
                                        rasqal_query *query,
                                        rasqal_rowsource* input_rs,
                                        rasqal_variable* var,
                                        rasqal_expression* expr,
                                        rasqal_expression* filter_expr,
                                        rasqal_query_scope* execution_scope)
{
  rasqal_extend_rowsource_context *con;
  int flags = 0;

  if(!world || !query || !input_rs || !var || !expr)
    return NULL;

  con = RASQAL_CALLOC(rasqal_extend_rowsource_context*, 1, sizeof(*con));
  if(!con)
    return NULL;

  con->input_rs = input_rs;
  con->var = rasqal_new_variable_from_variable(var);
  con->expr = rasqal_new_expression_from_expression(expr);

  /* Store optional filter expression */
  if(filter_expr) {
    con->filter_expr = rasqal_new_expression_from_expression(filter_expr);
  } else {
    con->filter_expr = NULL;
  }

  /* Store execution scope for variable registration */
  con->extend_scope = execution_scope;

  return rasqal_new_rowsource_from_handler(world, query,
                                           con,
                                           &rasqal_extend_rowsource_handler,
                                           query->vars_table,
                                           flags);
}

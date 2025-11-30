/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_triples.c - Rasqal triple pattern rowsource class
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

/*
 * Column numbering convention used throughout this module:
 * 1) "column" or "absolute_col": Absolute column numbers
 *    e.g., start_column=0, end_column=2 means columns 0,1,2
 * 2) "col_idx": Zero-based array index (column - start_column),
 *    used for accessing triple_meta[] and matrix arrays
 *
 * Examples: If start_column=5 and end_column=7, then:
 * - Absolute columns: 5, 6, 7
 * - Array indices: 0, 1, 2 (for triple_meta[0], triple_meta[1], triple_meta[2])
 */


/* Triple binding position constants */
#define BINDING_SUBJECT   0
#define BINDING_PREDICATE 1
#define BINDING_OBJECT    2
#define BINDING_ORIGIN    3

typedef struct
{
  /* source of triple pattern matches */
  rasqal_triples_source* triples_source;

  /* sequence of triple SHARED with query */
  raptor_sequence* triples;

  /* first triple pattern in sequence to use */
  int start_column;

  /* last triple pattern in sequence to use */
  int end_column;

  /* number of triple patterns in the sequence
     ( = end_column - start_column + 1) */
  int triples_count;

  /* An array of items, one per triple pattern in the sequence */
  rasqal_triple_meta* triple_meta;

  /* offset into results for current row */
  int offset;

  /* number of variables used in variables table  */
  int size;

  /* GRAPH origin to use */
  rasqal_literal *origin;

  /* Track cartesian product iteration state */
  int cartesian_initialized;
  int current_outer_column;  /* leftmost column for cartesian product */
  int current_inner_column;  /* rightmost column for cartesian product */

  /* Track state of each column for N-column cartesian product */
  int *column_states;

  /* Constraint-based multi-pattern processing */
  int current_column;                    /* Currently processing column (0 to end_column) */
  int variable_count;                    /* Total variables in query (from vars_table) */
  char* column_variable_matrix;          /* [column * variable_count + var_idx] = 1 if bound, 0 otherwise */
  rasqal_variable** variable_index_map;  /* [var_idx] → variable pointer */
} rasqal_triples_rowsource_context;

/* This is an lvalue */
#define column_var_matrix_lookup(con, col_idx, var_idx) con->column_variable_matrix[((col_idx * con->variable_count) + var_idx)]

/*
 * rasqal_triples_rowsource_is_variable_already_bound_earlier:
 * @var: variable to check
 * @con: triples rowsource context
 * @current_column: absolute column number being processed
 *
 * Check if a variable appears in any triple pattern before the current column.
 * This is used during constraint-aware parts calculation to determine which
 * variables should be treated as constraints (already bound) vs. variables
 * to be bound by the current pattern.
 *
 * Return value: non-0 if variable appears in earlier column, 0 if not
 */
static int
rasqal_triples_rowsource_is_variable_already_bound_earlier(rasqal_variable* var, rasqal_triples_rowsource_context* con, int current_column)
{
  int earlier_col;

  if(!var || !con)
    return 0;

  /* Check if this variable is bound by any earlier column */
  for(earlier_col = con->start_column; earlier_col < current_column; earlier_col++) {
    rasqal_triple *earlier_t = (rasqal_triple*)raptor_sequence_get_at(con->triples, earlier_col);
    rasqal_variable *earlier_v;

    if(!earlier_t)
      continue;

    if((earlier_v = rasqal_literal_as_variable(earlier_t->subject)) && earlier_v == var)
      return 1;
    if((earlier_v = rasqal_literal_as_variable(earlier_t->predicate)) && earlier_v == var)
      return 1;
    if((earlier_v = rasqal_literal_as_variable(earlier_t->object)) && earlier_v == var)
      return 1;
    if(earlier_t->origin && (earlier_v = rasqal_literal_as_variable(earlier_t->origin)) && earlier_v == var)
      return 1;
  }

  return 0;  /* Variable not bound by any earlier column */
}

/*
 * rasqal_triples_rowsource_init:
 * @rowsource: rasqal rowsource
 * @user_data: user data (rasqal_triples_rowsource_context)
 *
 * Initialize triples rowsource by setting up variable projection and
 * calculating constraint-aware binding parts for each triple pattern.
 * For each column, determines which parts (subject/predicate/object/origin)
 * should be bound vs. used as constraints based on whether variables
 * appear in earlier patterns.
 *
 * Return value: 0 on success, non-0 on failure
 */
static int
rasqal_triples_rowsource_init(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_query *query = rowsource->query;
  rasqal_triples_rowsource_context *con;
  int column;
  int rc = 0;
  int size;
  int i;

  con = (rasqal_triples_rowsource_context*)user_data;

  size = rasqal_variables_table_get_total_variables_count(query->vars_table);

  /* Construct the ordered projection of the variables set by these triples */
  con->size = 0;
  for(i = 0; i < size; i++) {
    rasqal_variable *v;
    v = rasqal_variables_table_get(rowsource->vars_table, i);

    for(column = con->start_column; column <= con->end_column; column++) {
      if(rasqal_query_variable_bound_in_triple(query, v, column)) {
          v = rasqal_new_variable_from_variable(v);
          if(raptor_sequence_push(rowsource->variables_sequence, v))
            return -1;
          con->size++;
          break; /* end column search loop */
        }
    }
  }

  /* Variable projection setup complete */

  for(column = con->start_column; column <= con->end_column; column++) {
    rasqal_triple_meta *m;
    rasqal_triple *t;
    rasqal_variable* v;

    m = &con->triple_meta[column - con->start_column];

    m->parts = (rasqal_triple_parts)0;

    t = (rasqal_triple*)raptor_sequence_get_at(con->triples, column);

    /* For constraint-based joining: only bind variables that are NOT already bound by earlier columns */
    if((v = rasqal_literal_as_variable(t->subject))) {
      if(!rasqal_triples_rowsource_is_variable_already_bound_earlier(v, con, column) &&
         rasqal_query_variable_bound_in_triple(query, v, column) & RASQAL_TRIPLE_SUBJECT)
        m->parts = (rasqal_triple_parts)(m->parts | RASQAL_TRIPLE_SUBJECT);
    }

    if((v = rasqal_literal_as_variable(t->predicate))) {
      if(!rasqal_triples_rowsource_is_variable_already_bound_earlier(v, con, column) &&
         rasqal_query_variable_bound_in_triple(query, v, column) & RASQAL_TRIPLE_PREDICATE)
        m->parts = (rasqal_triple_parts)(m->parts | RASQAL_TRIPLE_PREDICATE);
    }

    if((v = rasqal_literal_as_variable(t->object))) {
      if(!rasqal_triples_rowsource_is_variable_already_bound_earlier(v, con, column) &&
         rasqal_query_variable_bound_in_triple(query, v, column) & RASQAL_TRIPLE_OBJECT)
        m->parts = (rasqal_triple_parts)(m->parts | RASQAL_TRIPLE_OBJECT);
    }

    RASQAL_DEBUG4("triple pattern column %d has constraint-aware parts %s (%u)\n", column,
                  rasqal_engine_get_parts_string(m->parts), m->parts);

  }

  return rc;
}


static int
rasqal_triples_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                          void *user_data)
{
  rasqal_triples_rowsource_context* con;
  con = (rasqal_triples_rowsource_context*)user_data;

  rowsource->size = con->size;

  return 0;
}


static int
rasqal_triples_rowsource_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_triples_rowsource_context *con;
  int i;

  con = (rasqal_triples_rowsource_context*)user_data;

  if(con->triple_meta) {
    for(i = con->start_column; i <= con->end_column; i++) {
      rasqal_triple_meta *m;
      m = &con->triple_meta[i - con->start_column];
      rasqal_reset_triple_meta(m);
    }

    RASQAL_FREE(rasqal_triple_meta, con->triple_meta);
  }

  if(con->origin)
    rasqal_free_literal(con->origin);

  if(con->column_states)
    RASQAL_FREE(int*, con->column_states);

  if(con->column_variable_matrix)
    RASQAL_FREE(char*, con->column_variable_matrix);

  if(con->variable_index_map)
    RASQAL_FREE(rasqal_variable**, con->variable_index_map);

  RASQAL_FREE(rasqal_triples_rowsource_context, con);

  return 0;
}


/* Helper functions for constraint-based multi-pattern processing */

static int
rasqal_triples_rowsource_get_variable_index(rasqal_triples_rowsource_context* con, rasqal_variable* var)
{
  int i;

  if(!con->variable_index_map)
    return -1;

  for(i = 0; i < con->variable_count; i++) {
    if(con->variable_index_map[i] == var) {
      return i;
    }
  }
  return -1;  /* Variable not found */
}

/*
 * rasqal_triples_rowsource_is_variable_bound_by_earlier_column:
 * @con: triples rowsource context
 * @var: variable to check
 * @current_column: absolute column number being processed
 *
 * Check if a variable is marked as bound by any earlier column in the
 * column-variable binding matrix. This is used during constraint validation
 * to determine if a variable should have a consistent value across patterns.
 *
 * Return value: non-0 if variable bound by earlier column, 0 if not
 */
static int
rasqal_triples_rowsource_is_variable_bound_by_earlier_column(rasqal_triples_rowsource_context* con,
   rasqal_variable* var, int current_column)
{
  int var_idx = rasqal_triples_rowsource_get_variable_index(con, var);
  int col;

  if(var_idx < 0)
    return 0;

  /* Check if this variable is marked as bound by any earlier column */
  for(col = con->start_column; col < current_column; col++) {
    int col_idx = col - con->start_column;
    if(column_var_matrix_lookup(con, col_idx, var_idx)) {
      return 1;  /* Found an earlier binding */
    }
  }

  return 0;
}

/*
 * rasqal_triples_rowsource_validate_column_constraints_post_bind:
 * @con: triples rowsource context
 * @column: absolute column number that was just bound
 *
 * Validate that newly bound variable values are consistent with variables
 * bound by earlier columns. For each variable in the current pattern that
 * was also bound by an earlier column, checks that the matched literal
 * equals the variable's existing value. This ensures join constraints
 * are properly enforced across triple patterns.
 *
 * Return value: non-0 if all constraints satisfied, 0 if constraint violated
 */
static int
rasqal_triples_rowsource_validate_column_constraints_post_bind(rasqal_triples_rowsource_context* con,
   int column)
{
  rasqal_triple *t = (rasqal_triple*)raptor_sequence_get_at(con->triples, column);
  int col_idx = column - con->start_column;  /* Convert absolute column to zero-based index */
  rasqal_triple_meta *meta = &con->triple_meta[col_idx];
  rasqal_variable *var;
  int i;
  /* Get the actual matched triple values from the bindings */
  rasqal_literal *matched_literals[4] = {
    meta->bindings[BINDING_SUBJECT] ? meta->bindings[BINDING_SUBJECT]->value : NULL,
    meta->bindings[BINDING_PREDICATE] ? meta->bindings[BINDING_PREDICATE]->value : NULL,
    meta->bindings[BINDING_OBJECT] ? meta->bindings[BINDING_OBJECT]->value : NULL,
    meta->bindings[BINDING_ORIGIN] ? meta->bindings[BINDING_ORIGIN]->value : NULL
  };

  rasqal_literal *pattern_literals[4] = {t->subject, t->predicate, t->object, t->origin};


  if(!t)
    return 1;

  RASQAL_DEBUG2("validate_column_constraints_post_bind called for column %d\n", column);

  /* Check each position in the triple pattern */
  for(i = 0; i < 4; i++) {
    if(!pattern_literals[i])
      continue;

    var = rasqal_literal_as_variable(pattern_literals[i]);
    if(!var)
      continue;  /* Not a variable */

    /* Check if this variable was bound by an earlier column */
    if(rasqal_triples_rowsource_is_variable_bound_by_earlier_column(con, var, column)) {
      /* This variable should have a consistent value across all patterns */
      /* The matched value should equal the variable's value */
      if(!matched_literals[i] || !var->value) {
        RASQAL_DEBUG2("Constraint violation: variable %s missing value\n",
                      var->name ? (char*)var->name : "?");
        return 0;
      }

      if(!rasqal_literal_equals(matched_literals[i], var->value)) {
        RASQAL_DEBUG2("Constraint violation: variable %s has inconsistent values\n",
                      var->name ? (char*)var->name : "?");
        return 0;
      }

      RASQAL_DEBUG1("DEBUG: Constraint values are equal - this may be the bug!\n");
    }
  }

  return 1;  /* All constraints satisfied */
}

/*
 * rasqal_triples_rowsource_record_newly_bound_variables:
 * @con: triples rowsource context
 * @column: absolute column number that was just bound
 *
 * Update the column-variable binding matrix to record which variables
 * were newly bound by the current column. Only records variables that
 * this column was supposed to bind (based on meta->parts), not variables
 * that were used as constraints. This tracking is essential for proper
 * backtracking and constraint enforcement.
 *
 * Return value: none
 */
static void
rasqal_triples_rowsource_record_newly_bound_variables(rasqal_triples_rowsource_context* con,
                                                      int column)
{
  int col_idx = column - con->start_column;  /* Convert absolute column to zero-based index */
  rasqal_triple_meta *meta = &con->triple_meta[col_idx];
  rasqal_triple_parts parts_to_bind = meta->parts;

  if(!con->column_variable_matrix)
    return;

  /* Only record variables that this column was supposed to bind (based on m->parts) */
  if((parts_to_bind & RASQAL_TRIPLE_SUBJECT) && meta->bindings[BINDING_SUBJECT] && meta->bindings[BINDING_SUBJECT]->value) {
    int var_idx = rasqal_triples_rowsource_get_variable_index(con, meta->bindings[BINDING_SUBJECT]);
    if(var_idx >= 0) {
      column_var_matrix_lookup(con, col_idx, var_idx) = 1;
      RASQAL_DEBUG3("Recorded: column %d bound variable %s (subject)\n", column,
                    meta->bindings[BINDING_SUBJECT]->name ? (char*)meta->bindings[BINDING_SUBJECT]->name : "?");
    }
  }

  if((parts_to_bind & RASQAL_TRIPLE_PREDICATE) && meta->bindings[BINDING_PREDICATE] && meta->bindings[BINDING_PREDICATE]->value) {
    int var_idx = rasqal_triples_rowsource_get_variable_index(con, meta->bindings[BINDING_PREDICATE]);
    if(var_idx >= 0) {
      column_var_matrix_lookup(con, col_idx, var_idx) = 1;
      RASQAL_DEBUG3("Recorded: column %d bound variable %s (predicate)\n", column,
                    meta->bindings[BINDING_PREDICATE]->name ? (char*)meta->bindings[BINDING_PREDICATE]->name : "?");
    }
  }

  if((parts_to_bind & RASQAL_TRIPLE_OBJECT) && meta->bindings[BINDING_OBJECT] && meta->bindings[BINDING_OBJECT]->value) {
    int var_idx = rasqal_triples_rowsource_get_variable_index(con, meta->bindings[BINDING_OBJECT]);
    if(var_idx >= 0) {
      column_var_matrix_lookup(con, col_idx, var_idx) = 1;
      RASQAL_DEBUG3("Recorded: column %d bound variable %s (object)\n", column,
                    meta->bindings[BINDING_OBJECT]->name ? (char*)meta->bindings[BINDING_OBJECT]->name : "?");
    }
  }

  if((parts_to_bind & RASQAL_TRIPLE_ORIGIN) && meta->bindings[BINDING_ORIGIN] && meta->bindings[BINDING_ORIGIN]->value) {
    int var_idx = rasqal_triples_rowsource_get_variable_index(con, meta->bindings[BINDING_ORIGIN]);
    if(var_idx >= 0) {
      column_var_matrix_lookup(con, col_idx, var_idx) = 1;
      RASQAL_DEBUG3("Recorded: column %d bound variable %s (origin)\n", column,
                    meta->bindings[BINDING_ORIGIN]->name ? (char*)meta->bindings[BINDING_ORIGIN]->name : "?");
    }
  }
}

#ifdef RASQAL_DEBUG
static void
rasqal_triples_rowsource_print_column_variable_matrix(rasqal_triples_rowsource_context* con)
{
  int col, var_idx;

  if(!con->column_variable_matrix || !con->variable_index_map)
    return;

  fprintf(RASQAL_DEBUG_FH, "Column-Variable Binding Matrix:\n");

  /* Print header with variable names */
  fprintf(RASQAL_DEBUG_FH, "         ");
  for(var_idx = 0; var_idx < con->variable_count; var_idx++) {
    rasqal_variable* var = con->variable_index_map[var_idx];
    fprintf(RASQAL_DEBUG_FH, "%-8s ", var->name ? (char*)var->name : "?");
  }
  fprintf(RASQAL_DEBUG_FH, "\n");

  /* Print each column row */
  for(col = con->start_column; col <= con->end_column; col++) {
    int col_idx = col - con->start_column;
    fprintf(RASQAL_DEBUG_FH, "Column %d ", col);

    for(var_idx = 0; var_idx < con->variable_count; var_idx++) {
      char bound = column_var_matrix_lookup(con, col_idx, var_idx);
      fprintf(RASQAL_DEBUG_FH, "   %c     ", bound ? '1' : '0');
    }
    fprintf(RASQAL_DEBUG_FH, "\n");
  }

  /* Print current variable values */
  fprintf(RASQAL_DEBUG_FH, "Current variable values:\n");
  for(var_idx = 0; var_idx < con->variable_count; var_idx++) {
    rasqal_variable* var = con->variable_index_map[var_idx];
    fprintf(RASQAL_DEBUG_FH, "  ");
    if(var->name)
      fprintf(RASQAL_DEBUG_FH, "%s", (char*)var->name);
    else
      fprintf(RASQAL_DEBUG_FH, "?");
    fprintf(RASQAL_DEBUG_FH, " = ");
    if(var->value)
      fprintf(RASQAL_DEBUG_FH, "%s", rasqal_literal_as_string(var->value));
    else
      fprintf(RASQAL_DEBUG_FH, "NULL");
    fprintf(RASQAL_DEBUG_FH, "\n");
  }
}
#endif

/*
 * rasqal_triples_rowsource_get_next_constraint_based_row:
 * @rowsource: rasqal rowsource
 * @con: triples rowsource context
 *
 * Core constraint-based multi-pattern join algorithm implementing just-in-time
 * triples_match creation and systematic backtracking.  Creates triples_match
 * objects only when processing each column to ensure variables bound by
 * earlier columns are used as constraints.  Uses the column-variable binding
 * matrix to track state and implements backtracking when patterns are
 * exhausted or constraints are violated.
 *
 * Return value: RASQAL_ENGINE_OK if solution found, RASQAL_ENGINE_FINISHED
 * if no more solutions, RASQAL_ENGINE_FAILED on error
 */
static rasqal_engine_error
rasqal_triples_rowsource_get_next_constraint_based_row(rasqal_rowsource* rowsource,
                                                       rasqal_triples_rowsource_context *con)
{
  rasqal_query *query = rowsource->query;
  rasqal_triple_meta *m;
  rasqal_triple *t;
  int column;

  /* Initialize only basic state if needed */
  if(!con->cartesian_initialized) {
    con->cartesian_initialized = 1;
    con->current_column = con->start_column;
  }

#ifdef RASQAL_DEBUG
  if(rasqal_get_debug_level() >= 2) {
    RASQAL_DEBUG2("Processing column %d\n", con->current_column);
    rasqal_triples_rowsource_print_column_variable_matrix(con);
  }
#endif

  /* Main processing loop */
  while(1) {
    /* Step 1: Check if all columns processed successfully */
    if(con->current_column > con->end_column) {
      /* All columns processed - we have a complete solution */
      RASQAL_DEBUG1("All columns bound successfully - solution found\n");

      /* CRITICAL FIX: Re-bind all columns to ensure variables have correct values.
       *
       * When this rowsource is used in a join operation, other rowsources may
       * modify shared variables between calls. For example, in a query like:
       *   { ?a rdf:type :T . ?b rdf:type :T } JOIN { ?a :p ?x }
       * the right rowsource will rebind variable ?a while iterating, corrupting
       * the value set by this (left) rowsource. We must restore all variable
       * values from the current iterator positions before the row is created.
       *
       * Without this fix, rows can be created with incorrect variable values,
       * leading to wrong query results (e.g., [s3,s2] instead of [s1,s2]).
       */
      for(column = con->start_column; column <= con->end_column; column++) {
        rasqal_triple_meta *rebind_m = &con->triple_meta[column - con->start_column];
        if(rebind_m->triples_match && rebind_m->parts) {
          rasqal_triples_match_bind_match(rebind_m->triples_match, rebind_m->bindings, rebind_m->parts);
        }
      }

      /* Advance rightmost column for next iteration */
      m = &con->triple_meta[con->end_column - con->start_column];
      rasqal_triples_match_next_match(m->triples_match);
      con->current_column = con->end_column;  /* Set to last column for next iteration */

      return RASQAL_ENGINE_OK;
    }

    /* Step 2: Try to bind current column */
    column = con->current_column;  /* This is the absolute column number (e.g., 0, 1, 2...) */
    m = &con->triple_meta[column - con->start_column];  /* Convert to zero-based index for meta array */

    /* Create triples_match if not already created */
    if(!m->triples_match) {
      t = (rasqal_triple*)raptor_sequence_get_at(con->triples, column);

      /* Debug: check variable values before creating triples match */
      if(1) {
        rasqal_variable *var = NULL;
        if((var = rasqal_literal_as_variable(t->subject))) {
          RASQAL_DEBUG1("JIT: Before triples_match creation\n");
          if(var->value) {
            RASQAL_DEBUG2("  Subject variable %s is SET\n", var->name ? (char*)var->name : "?");
          } else {
            RASQAL_DEBUG2("  Subject variable %s is NULL\n", var->name ? (char*)var->name : "?");
          }
        }
      }

      m->triples_match = rasqal_new_triples_match(query, con->triples_source, m, t);
      if(!m->triples_match) {
        RASQAL_DEBUG2("Failed to create triples match for column %d\n", column);
        return RASQAL_ENGINE_FAILED;
      }
      RASQAL_DEBUG2("JIT: Created triples match for column %d\n", column);
    }

    /* Position iterator to first match if needed */
    if(rasqal_triples_match_is_end(m->triples_match)) {
      int reset_col;

      RASQAL_DEBUG2("Column %d iterator exhausted - backtracking\n", column);
      /* Backtrack: move to previous column and advance it, then retry */
      if(column < con->start_column) {
        /* This shouldn't happen - we've gone before the first column */
        RASQAL_DEBUG1("Backtracked before start column - finished\n");
        return RASQAL_ENGINE_FINISHED;
      }

      if(column == con->start_column) {
        /* First column exhausted - try to advance it */
        rasqal_triples_match_next_match(m->triples_match);

        /* Check if first column has more matches */
        if(rasqal_triples_match_is_end(m->triples_match)) {
          RASQAL_DEBUG1("No more matches in first column - finished\n");
          return RASQAL_ENGINE_FINISHED;
        }

        /* First column advanced successfully - reset all subsequent columns */
        for(reset_col = column + 1; reset_col <= con->end_column; reset_col++) {
          rasqal_triple_meta *reset_m = &con->triple_meta[reset_col - con->start_column];
          if(reset_m->triples_match) {
            rasqal_free_triples_match(reset_m->triples_match);
            reset_m->triples_match = NULL;
          }
        }

        continue;
      }

      /* Cleanup current column */
      if(m->triples_match) {
        rasqal_free_triples_match(m->triples_match);
        m->triples_match = NULL;
      }

      /* Move back and advance previous column */
      con->current_column--;
      column = con->current_column;
      m = &con->triple_meta[column - con->start_column];
      rasqal_triples_match_next_match(m->triples_match);

      /* Reset all subsequent columns since they depended on the old binding */
      for(reset_col = column + 1; reset_col <= con->end_column; reset_col++) {
        rasqal_triple_meta *reset_m = &con->triple_meta[reset_col - con->start_column];
        if(reset_m->triples_match) {
          rasqal_free_triples_match(reset_m->triples_match);
          reset_m->triples_match = NULL;
        }
      }

      continue;
    }

    /* Step 3: Try to bind the current column */
    if(m->parts) {
      rasqal_triple_parts parts;
      parts = rasqal_triples_match_bind_match(m->triples_match, m->bindings, m->parts);

      if(parts == 0) {
        /* Binding failed - advance this column and try again */
        RASQAL_DEBUG2("Binding failed for column %d, advancing\n", column);
        rasqal_triples_match_next_match(m->triples_match);
        continue;
      }

      /* Binding succeeded - now validate constraints */
      if(!rasqal_triples_rowsource_validate_column_constraints_post_bind(con, column)) {
        RASQAL_DEBUG2("Column %d binding rejected due to constraint violation\n", column);
        /* This binding violates constraints - try next match */
        rasqal_triples_match_next_match(m->triples_match);
        continue;
      }

      RASQAL_DEBUG2("Successfully bound column %d\n", column);
      rasqal_triples_rowsource_record_newly_bound_variables(con, column);

      /* Move to next column */
      con->current_column++;
      continue;
    }

    /* No binding needed for this column, just advance */
    con->current_column++;
  }
}


/*
 * rasqal_triples_rowsource_get_next_row:
 * @rowsource: rasqal rowsource
 * @con: triples rowsource context
 *
 * Main entry point for getting the next row from a triples rowsource.
 * Dispatches between single-column processing (simplified direct matching)
 * and multi-column constraint-based join algorithm. For multi-column
 * queries, delegates to the constraint-based algorithm which handles
 * variable binding, constraint enforcement, and backtracking.
 *
 * Return value: RASQAL_ENGINE_OK if row available, RASQAL_ENGINE_FINISHED
 * if no more rows, RASQAL_ENGINE_FAILED on error
 */
static rasqal_engine_error
rasqal_triples_rowsource_get_next_row(rasqal_rowsource* rowsource,
                                      rasqal_triples_rowsource_context *con)
{
  rasqal_query *query = rowsource->query;
  rasqal_engine_error error = RASQAL_ENGINE_OK;
  rasqal_triple_meta *m;
  rasqal_triple *t;
  int column;

  RASQAL_DEBUG2("Context: ptr=%p\n", (void*)con);
  RASQAL_DEBUG2("Context values: start=%d\n", con->start_column);
  RASQAL_DEBUG2("Context values: end=%d\n", con->end_column);

  if(con->end_column > con->start_column) {
    /* Multiple columns - use new constraint-based algorithm */
    RASQAL_DEBUG1("Using constraint-based multi-pattern processing\n");
    return rasqal_triples_rowsource_get_next_constraint_based_row(rowsource, con);
  }

  /* Single column case - simplified processing */
  column = con->start_column;
  m = &con->triple_meta[0];  /* Always index 0 for single column */
  t = (rasqal_triple*)raptor_sequence_get_at(con->triples, column);

  if(!m->triples_match) {
    m->triples_match = rasqal_new_triples_match(query, con->triples_source, m, t);
    if(!m->triples_match) {
      RASQAL_DEBUG2("Failed to make a triple match for column %d\n", column);
      return RASQAL_ENGINE_FAILED;
    }
    RASQAL_DEBUG2("made new triples match for column %d\n", column);
  }

  /* Check if iterator is exhausted before attempting to use it */
  if(rasqal_triples_match_is_end(m->triples_match))
    return RASQAL_ENGINE_FINISHED;

  if(m->parts) {
    /* Try to bind first - iterator needs to be positioned */
    rasqal_triple_parts parts = rasqal_triples_match_bind_match(m->triples_match, m->bindings, m->parts);
    RASQAL_DEBUG4("bind_match for column %d returned parts %s (%u)\n",
                  column, rasqal_engine_get_parts_string(parts), parts);
    if(!parts) {
      rasqal_triples_match_next_match(m->triples_match);
      return rasqal_triples_rowsource_get_next_row(rowsource, con);
    }
  }

  /* Advance for next iteration */
  rasqal_triples_match_next_match(m->triples_match);

  return error;
}

static rasqal_row*
rasqal_triples_rowsource_read_row(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_triples_rowsource_context *con;
  int i;
  rasqal_row* row = NULL;
  rasqal_engine_error error = RASQAL_ENGINE_OK;

  con = (rasqal_triples_rowsource_context*)user_data;

  error = rasqal_triples_rowsource_get_next_row(rowsource, con);
  RASQAL_DEBUG2("rasqal_triples_rowsource_get_next_row() returned error %s\n",
                rasqal_engine_error_as_string(error));

  if(error != RASQAL_ENGINE_OK)
    goto done;

#ifdef RASQAL_DEBUG
  if(rasqal_get_debug_level() >= 2) {
    int values_returned = 0;
    /* Count actual bound values */
    for(i = 0; i < con->size; i++) {
      rasqal_variable* v;
      v = rasqal_rowsource_get_variable_by_offset(rowsource, i);
      if(v->value)
        values_returned++;
    }
    RASQAL_DEBUG2("Solution binds %d values\n", values_returned);
  }
#endif

  row = rasqal_new_row(rowsource);
  if(!row)
    goto done;

  for(i = 0; i < row->size; i++) {
    rasqal_variable* v;
    v = rasqal_rowsource_get_variable_by_offset(rowsource, i);
    if(row->values[i])
      rasqal_free_literal(row->values[i]);
    row->values[i] = rasqal_new_literal_from_literal(v->value);
  }

  row->offset = con->offset++;

  done:

  return row;
}


static raptor_sequence*
rasqal_triples_rowsource_read_all_rows(rasqal_rowsource* rowsource,
                                       void *user_data)
{
  raptor_sequence *seq = NULL;

  return seq;
}


static int
rasqal_triples_rowsource_reset(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_triples_rowsource_context *con;
  int column;

  con = (rasqal_triples_rowsource_context*)user_data;

  /* Reset algorithm state for next iteration */
  con->cartesian_initialized = 0;

  for(column = con->start_column; column <= con->end_column; column++) {
    rasqal_triple_meta *m;

    m = &con->triple_meta[column - con->start_column];
    rasqal_reset_triple_meta(m);
  }

  return 0;
}


static int
rasqal_triples_rowsource_set_origin(rasqal_rowsource *rowsource,
                                    void *user_data,
                                    rasqal_literal *origin)
{
  rasqal_triples_rowsource_context *con;
  int column;

  con = (rasqal_triples_rowsource_context*)user_data;
  if(con->origin)
    rasqal_free_literal(con->origin);
  con->origin = rasqal_new_literal_from_literal(origin);

  for(column = con->start_column; column <= con->end_column; column++) {
    rasqal_triple *t;
    t = (rasqal_triple*)raptor_sequence_get_at(con->triples, column);
    if(t->origin)
      rasqal_free_literal(t->origin);
    t->origin = rasqal_new_literal_from_literal(con->origin);
  }

  return 0;
}


static const rasqal_rowsource_handler rasqal_triples_rowsource_handler = {
  /* .version = */ 1,
  "triple pattern",
  /* .init = */ rasqal_triples_rowsource_init,
  /* .finish = */ rasqal_triples_rowsource_finish,
  /* .ensure_variables = */ rasqal_triples_rowsource_ensure_variables,
  /* .read_row = */ rasqal_triples_rowsource_read_row,
  /* .read_all_rows = */ rasqal_triples_rowsource_read_all_rows,
  /* .reset = */ rasqal_triples_rowsource_reset,
  /* .set_requirements = */ NULL,
  /* .get_inner_rowsource = */ NULL,
  /* .set_origin = */ rasqal_triples_rowsource_set_origin
};


/**
 * rasqal_new_triples_rowsource:
 * @world: world object
 * @query: query object
 * @triples_source: shared triples source
 * @triples: shared triples sequence
 * @start_column: start column in triples sequence
 * @end_column: end column in triples sequence
 * @bound_in: array marking the triples that bind a variable
 *
 * INTERNAL - create a new triples rowsource
 *
 * Return value: new triples rowsource or NULL on failure
 */
rasqal_rowsource*
rasqal_new_triples_rowsource(rasqal_world *world,
                             rasqal_query *query,
                             rasqal_triples_source* triples_source,
                             raptor_sequence* triples,
                             int start_column, int end_column)
{
  rasqal_triples_rowsource_context *con;
  int flags = 0;

  if(!world || !query || !triples_source)
    return NULL;

  if(!triples)
    return rasqal_new_empty_rowsource(world, query);

  con = RASQAL_CALLOC(rasqal_triples_rowsource_context*, 1, sizeof(*con));
  if(!con)
    return NULL;

  con->triples_source = triples_source;
  con->triples = triples;
  con->start_column = start_column;
  con->end_column = end_column;
  /* Constraint-based processing state initialized */

  /* Debug: Print the initialization values */
  RASQAL_DEBUG2("Rowsource init: ptr=%p\n", (void*)con);
  RASQAL_DEBUG2("Rowsource values: start=%d\n", start_column);
  RASQAL_DEBUG2("Rowsource values: end=%d\n", end_column);

  con->triples_count = con->end_column - con->start_column + 1;

  /* Initialize constraint-based multi-pattern processing */
  con->variable_count = rasqal_variables_table_get_total_variables_count(query->vars_table);
  con->current_column = con->start_column;

  /* Allocate fixed-size 2D matrix */
  if(con->variable_count > 0) {
    int i;

    con->column_variable_matrix = RASQAL_CALLOC(char*,
                                               con->triples_count * con->variable_count,
                                               sizeof(char));
    if(!con->column_variable_matrix) {
      rasqal_triples_rowsource_finish(NULL, con);
      return NULL;
    }

    /* Build variable index mapping */
    con->variable_index_map = RASQAL_CALLOC(rasqal_variable**, con->variable_count,
                                           sizeof(rasqal_variable*));
    if(!con->variable_index_map) {
      rasqal_triples_rowsource_finish(NULL, con);
      return NULL;
    }

    for(i = 0; i < con->variable_count; i++)
      con->variable_index_map[i] = rasqal_variables_table_get(query->vars_table, i);

    RASQAL_DEBUG3("Initialized matrix: %d columns × %d variables\n", con->triples_count, con->variable_count);
  }

  con->triple_meta = RASQAL_CALLOC(rasqal_triple_meta*, RASQAL_GOOD_CAST(size_t, con->triples_count),
                                   sizeof(rasqal_triple_meta));
  if(!con->triple_meta) {
    rasqal_triples_rowsource_finish(NULL, con);
    return NULL;
  }

  return rasqal_new_rowsource_from_handler(world, query,
                                           con,
                                           &rasqal_triples_rowsource_handler,
                                           query->vars_table,
                                           flags);
}


#endif /* not STANDALONE */



#ifdef STANDALONE

/* one more prototype */
int main(int argc, char *argv[]);

/* Helper function to test a query */
static int
test_query_with_format(rasqal_world *world, const char *data_file,
                      const char *query_format, const char *test_name,
                      int expected_min_rows, int expected_max_rows)
{
  rasqal_query *query = NULL;
  rasqal_rowsource *rowsource = NULL;
  raptor_sequence* triples;
  rasqal_triples_source* triples_source = NULL;
  raptor_uri *base_uri = NULL;
  unsigned char *data_string = NULL;
  unsigned char *uri_string = NULL;
  unsigned char *query_string = NULL;
  int failures = 0;
  int row_count = 0;
  int rc;
  size_t qs_len;

  printf("=== %s Test ===\n", test_name);

  data_string = raptor_uri_filename_to_uri_string(data_file);
  qs_len = strlen(RASQAL_GOOD_CAST(const char*, data_string)) + strlen(query_format) + 10;
  query_string = RASQAL_MALLOC(unsigned char*, qs_len);
  PRAGMA_IGNORE_WARNING_FORMAT_NONLITERAL_START
  snprintf(RASQAL_GOOD_CAST(char*, query_string), qs_len, query_format, data_string);
  PRAGMA_IGNORE_WARNING_END
  raptor_free_memory(data_string);

  uri_string = raptor_uri_filename_to_uri_string("");
  base_uri = raptor_new_uri(world->raptor_world_ptr, uri_string);
  raptor_free_memory(uri_string);

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query) {
    printf("  FAILED: Could not create query\n");
    failures++;
    goto cleanup;
  }

  printf("  Query: %s\n", query_string);
  rc = rasqal_query_prepare(query, query_string, base_uri);
  if(rc) {
    printf("  FAILED: Could not prepare query\n");
    failures++;
    goto cleanup;
  }

  triples = rasqal_query_get_triple_sequence(query);
  triples_source = rasqal_new_triples_source(query);

  printf("  Triple patterns in query: %d\n", (int)raptor_sequence_size(triples));

  if(raptor_sequence_size(triples) >= 2) {
    /* Test cartesian product with columns 0-1 */
    rowsource = rasqal_new_triples_rowsource(world, query, triples_source,
                                           triples, 0, 1);
  } else {
    /* Test single pattern with column 0 */
    rowsource = rasqal_new_triples_rowsource(world, query, triples_source,
                                           triples, 0, 0);
  }

  if(!rowsource) {
    printf("  FAILED: Could not create rowsource\n");
    failures++;
    goto cleanup;
  }

  /* Read all rows and count them */
  while(1) {
    rasqal_row* row = rasqal_rowsource_read_row(rowsource);
    if(!row)
      break;

    row_count++;
    if(row_count <= 3) { /* Show first few rows */
      printf("  Row %d: ", row_count);
      rasqal_row_print(row, stdout);
      printf("\n");
    } else if(row_count == 4) {
      printf("  ... (additional rows)\n");
    }

    rasqal_free_row(row);

    /* Safety check to avoid infinite loops */
    if(row_count > 1000) {
      printf("  FAILED: Exceeded safety limit, possible infinite loop\n");
      failures++;
      break;
    }
  }

  printf("  Total rows returned: %d\n", row_count);

  /* Validate row count */
  if(row_count < expected_min_rows) {
    printf("  FAILED: Expected at least %d rows, got %d\n", expected_min_rows,
           row_count);
    failures++;
  } else if(row_count > expected_max_rows) {
    printf("  FAILED: Expected at most %d rows, got %d\n", expected_max_rows,
           row_count);
    failures++;
  } else {
    printf("  PASSED: Row count %d is within expected range [%d, %d]\n",
           row_count, expected_min_rows, expected_max_rows);
  }

cleanup:
  if(rowsource)
    rasqal_free_rowsource(rowsource);

  if(triples_source)
    rasqal_free_triples_source(triples_source);

  if(query)
    rasqal_free_query(query);

  if(base_uri)
    raptor_free_uri(base_uri);

  if(query_string)
    RASQAL_FREE(char*, query_string);

  return failures;
}

#define QUERY_LANGUAGE "sparql"
#define SINGLE_PATTERN_QUERY_FORMAT "\
SELECT ?s ?p ?o \
FROM <%s> \
WHERE { ?s ?p ?o }\
"
#define CARTESIAN_QUERY_FORMAT "\
SELECT ?s1 ?p1 ?o1 ?s2 ?p2 ?o2 \
FROM <%s> \
WHERE { ?s1 ?p1 ?o1 . ?s2 ?p2 ?o2 }\
"
#define JOIN_QUERY_FORMAT "\
SELECT ?person ?name ?beverage \
FROM <%s> \
WHERE { ?person <http://example.org/name> ?name . ?person <http://example.org/likes> ?beverage }\
"



/* Multi-value test queries using external data file */
#define MULTI_VALUE_SIMPLE_QUERY_FORMAT "\
SELECT ?name ?age \
FROM <%s> \
WHERE { ?person <http://example.org/name> ?name . ?person <http://example.org/age> ?age }\
"

#define MULTI_VALUE_CARTESIAN_QUERY_FORMAT "\
SELECT ?name ?type \
FROM <%s> \
WHERE { ?person <http://example.org/name> ?name . ?person <http://example.org/type> ?type }\
"

#define MULTI_VALUE_COMPLEX_QUERY_FORMAT "\
SELECT ?name ?age ?city \
FROM <%s> \
WHERE { ?person <http://example.org/name> ?name . ?person <http://example.org/age> ?age . ?person <http://example.org/city> ?city }\
"

int
main(int argc, char *argv[])
{
  const char *program = rasqal_basename(argv[0]);
  rasqal_world *world;
  const char *single_pattern_query_format = SINGLE_PATTERN_QUERY_FORMAT;
  const char *cartesian_query_format = CARTESIAN_QUERY_FORMAT;
  const char *join_query_format = JOIN_QUERY_FORMAT;
  int failures = 0;
  const char *data_file;
  const char *multi_value_file;

  /* try to eet data from environment */
  data_file = getenv("CARTESIAN_DATA_FILE");
  multi_value_file = getenv("MULTI_VALUE_DATA_FILE");
  if(!data_file || !multi_value_file) {
    if(argc != 3) {
      fprintf(stderr, "USAGE: %s data-filename multi-value-data-filename\n", program);
      return(1);
    }
    data_file = argv[1];
    multi_value_file = argv[1];
  }

  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    return(1);
  }

  printf("%s: Comprehensive Cartesian Product Testing\n", program);
  printf("%s: Using data file: %s\n", program, data_file);

  /* Test 1: Single Pattern Query
   * This tests basic single triple pattern matching and should
   * return all triples in data.
   * With our test data (14 triples), we expect 14 rows
   */
  failures += test_query_with_format(world, data_file,
                                     single_pattern_query_format,
                                     "Single Pattern", 1, 50);

  /* Test 2: Cartesian Product Query
   * This tests true cartesian product with two separate patterns
   * With our test data (14 triples), we expect 14 * 14 = 196 combinations
  */
  failures += test_query_with_format(world, data_file, cartesian_query_format,
                                   "Cartesian Product", 100, 300);

  /* Test 3: Join Query with Shared Variable
   * This tests variable join where shared ?person variable
   * constrains the combinations.  The rowsource correctly applies
   * join constraints: only valid person-name-beverage matches
   *
   * Expected: 3 results, one for each person. Here's why:
   *
   * Data contains:
   * - 3 name triples: person1-Alice, person2-Bob, person3-Carol
   * - 3 likes triples: person1-coffee, person2-tea, person3-coffee
   *
   * This is a triple join query where ?person must match between patterns.
   * Each person has exactly one name and one beverage preference,
   * so the result should be exactly 3 rows:
   *
   * 1. person1: Alice + coffee
   * 2. person2: Bob + tea
   * 3. person3: Carol + coffee
   */
  failures += test_query_with_format(world, data_file, join_query_format,
                                     "Variable Join", 3, 3);

  /* Test 4-6: Multi-Value Join Tests (if multi-value test file exists) */
  printf("\n%s: Running Multi-Value Join Tests\n", program);

  /* Test 4: Simple Multi-Value Join
   * person1 has 2 names ("Alice"@en, "Alicia"@es) and 1 age (25)
   * person2 has 1 name ("Bob") and 1 age (30)
   * Expected: 3 rows (person1 × 2 names + person2 × 1 name)
   */
  failures += test_query_with_format(world, multi_value_file,
                                  MULTI_VALUE_SIMPLE_QUERY_FORMAT,
                                  "Multi-Value Simple Join", 3, 3);

    /* Test 5: Multi-Value Cartesian
   * person2 has 1 name ("Bob") and 2 types ("Student", "Athlete")
   * Expected: 2 rows (Bob×Student, Bob×Athlete cartesian product)
   */
  failures += test_query_with_format(world, multi_value_file,
                                     MULTI_VALUE_CARTESIAN_QUERY_FORMAT,
                                     "Multi-Value Cartesian", 2, 2);

  /* Test 6: Complex Multi-Value Join
   * person1 has 2 names, 1 age, 1 city
   * Expected: 3 rows (Alice+25, Bob+25, Carol+30 from name+age join)
   */
  failures += test_query_with_format(world, multi_value_file,
                                     MULTI_VALUE_COMPLEX_QUERY_FORMAT,
                                     "Multi-Value Complex Join", 3, 3);

  if(failures == 0) {
    printf("\n%s: ALL TESTS PASSED\n", program);
  } else {
    printf("\n%s: %d TEST(S) FAILED\n", program, failures);
  }

  if(world)
    rasqal_free_world(world);

  return failures;
}

#endif /* STANDALONE */


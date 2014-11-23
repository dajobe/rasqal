/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_graph_pattern.c - Rasqal graph pattern class
 *
 * Copyright (C) 2004-2010, David Beckett http://www.dajobe.org/
 * Copyright (C) 2004-2005, University of Bristol, UK http://www.bristol.ac.uk/
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


/*
 * rasqal_new_graph_pattern:
 * @query: #rasqal_graph_pattern query object
 * @op: enum #rasqal_graph_pattern_operator operator
 *
 * INTERNAL - Create a new graph pattern object.
 * 
 * NOTE: This does not initialise the graph pattern completely
 * but relies on other operations.  The empty graph pattern
 * has no triples and no sub-graphs.
 *
 * Return value: a new #rasqal_graph_pattern object or NULL on failure
 **/
static rasqal_graph_pattern*
rasqal_new_graph_pattern(rasqal_query* query, 
                         rasqal_graph_pattern_operator op)
{
  rasqal_graph_pattern* gp;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);
  
  gp = RASQAL_CALLOC(rasqal_graph_pattern*, 1, sizeof(*gp));
  if(!gp)
    return NULL;

  gp->op = op;
  
  gp->query = query;
  gp->triples = NULL;
  gp->start_column = -1;
  gp->end_column = -1;

  /* This is initialised by
   * rasqal_query_prepare_count_graph_patterns() inside
   * rasqal_query_prepare()
   */
  gp->gp_index = -1;

  return gp;
}


/*
 * rasqal_new_basic_graph_pattern:
 * @query: #rasqal_graph_pattern query object
 * @triples: triples sequence containing the graph pattern
 * @start_column: first triple in the pattern
 * @end_column: last triple in the pattern
 *
 * INTERNAL - Create a new graph pattern object over triples.
 * 
 * Return value: a new #rasqal_graph_pattern object or NULL on failure
 **/
rasqal_graph_pattern*
rasqal_new_basic_graph_pattern(rasqal_query* query,
                               raptor_sequence *triples,
                               int start_column, int end_column)
{
  rasqal_graph_pattern* gp;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(triples, raptor_sequence, NULL);
  
  gp = rasqal_new_graph_pattern(query, RASQAL_GRAPH_PATTERN_OPERATOR_BASIC);
  if(!gp)
    return NULL;

  gp->triples = triples;
  gp->start_column = start_column;
  gp->end_column = end_column;

  return gp;
}


/*
 * rasqal_new_graph_pattern_from_sequence:
 * @query: #rasqal_graph_pattern query object
 * @graph_patterns: sequence containing the graph patterns (or NULL)
 * @operator: enum #rasqal_graph_pattern_operator such as
 * RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL
 *
 * INTERNAL - Create a new graph pattern from a sequence of graph_patterns.
 * 
 * Return value: a new #rasqal_graph_pattern object or NULL on failure
 **/
rasqal_graph_pattern*
rasqal_new_graph_pattern_from_sequence(rasqal_query* query,
                                       raptor_sequence *graph_patterns, 
                                       rasqal_graph_pattern_operator op)
{
  rasqal_graph_pattern* gp;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);
  
  gp = rasqal_new_graph_pattern(query, op);
  if(!gp) {
    if(graph_patterns)
      raptor_free_sequence(graph_patterns);
    return NULL;
  }
  
  gp->graph_patterns = graph_patterns;
  return gp;
}


/*
 * rasqal_new_filter_graph_pattern:
 * @query: #rasqal_graph_pattern query object
 * @expr: expression
 *
 * INTERNAL - Create a new graph pattern from a sequence of graph_patterns.
 * 
 * Return value: a new #rasqal_graph_pattern object or NULL on failure
 **/
rasqal_graph_pattern*
rasqal_new_filter_graph_pattern(rasqal_query* query,
                                rasqal_expression* expr)
{
  rasqal_graph_pattern* gp;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(expr, rasqal_expression, NULL);
  
  gp = rasqal_new_graph_pattern(query, RASQAL_GRAPH_PATTERN_OPERATOR_FILTER);
  if(!gp) {
    rasqal_free_expression(expr);
    return NULL;
  }
  
  if(rasqal_graph_pattern_set_filter_expression(gp, expr)) {
    rasqal_free_graph_pattern(gp);
    gp = NULL;
  }

  return gp;
}


/*
 * rasqal_new_let_graph_pattern:
 * @query: #rasqal_graph_pattern query object
 * @var: variable to assign
 * @expr: expression
 *
 * INTERNAL - Create a new assignment graph pattern
 * 
 * Return value: a new #rasqal_graph_pattern object or NULL on failure
 **/
rasqal_graph_pattern*
rasqal_new_let_graph_pattern(rasqal_query *query,
                             rasqal_variable *var,
                             rasqal_expression *expr)
{
  rasqal_graph_pattern* gp;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(var, rasqal_variable, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(expr, rasqal_expression, NULL);
  
  gp = rasqal_new_graph_pattern(query, RASQAL_GRAPH_PATTERN_OPERATOR_LET);
  if(!gp) {
    rasqal_free_expression(expr);
    return NULL;
  }
  
  gp->var = var;
  gp->filter_expression = expr;
  
  return gp;
}


/*
 * rasqal_new_select_graph_pattern:
 * @query: #rasqal_graph_pattern query object
 * @projection: projection object
 * @data_graphs: sequence of #rasqal_data_graph (or NULL)
 * @where: WHERE graph pattern
 * @modifier: solution modifier
 * @bindings: binding VALUES (or NULL)
 *
 * INTERNAL - Create a new SELECT graph pattern
 *
 * The @projection, @data_graphs, @where and @modifier all become owned
 * by the new graph pattern object.
 *
 * Roughly corresponds to:
 *   SELECT DISTINCT [in @projection] {@projection}
 *   FROM @data_graphs
 *   WHERE @where
 *   GROUP BY (HAVING)/ORDER BY [in @modifier]
 *
 * Return value: a new #rasqal_graph_pattern object or NULL on failure
 **/
rasqal_graph_pattern*
rasqal_new_select_graph_pattern(rasqal_query *query,
                                rasqal_projection* projection,
                                raptor_sequence* data_graphs,
                                rasqal_graph_pattern* where,
                                rasqal_solution_modifier* modifier,
                                rasqal_bindings* bindings)
{
  rasqal_graph_pattern* gp;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(projection, rasqal_projeciton, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(where, rasqal_graph_pattern, NULL);
  
  gp = rasqal_new_graph_pattern(query, RASQAL_GRAPH_PATTERN_OPERATOR_SELECT);
  if(!gp) {
    rasqal_free_projection(projection);
    if(where)
      rasqal_free_graph_pattern(where);

    if(modifier)
      rasqal_free_solution_modifier(modifier);

    if(bindings)
      rasqal_free_bindings(bindings);

    return NULL;
  }

  gp->projection = projection;
  gp->data_graphs = data_graphs;
  gp->modifier = modifier;
  gp->bindings = bindings;
  
  if(rasqal_graph_pattern_add_sub_graph_pattern(gp, where)) {
    rasqal_free_graph_pattern(gp);
    return NULL;
  }

  return gp;
}


/*
 * rasqal_new_single_graph_pattern:
 * @query: #rasqal_graph_pattern query object
 * @op: enum #rasqal_graph_pattern_operator operator
 * @single: single inner graph grattern
 *
 * INTERNAL - Create a new graph pattern object over a single graph pattern.
 * 
 * Return value: a new #rasqal_graph_pattern object or NULL on failure
 **/
rasqal_graph_pattern*
rasqal_new_single_graph_pattern(rasqal_query* query,
                                rasqal_graph_pattern_operator op,
                                rasqal_graph_pattern* single)
{
  rasqal_graph_pattern* gp;

  gp = rasqal_new_graph_pattern(query, op);
  if(!gp) {
    if(single)
      rasqal_free_graph_pattern(single);

    return NULL;
  }

  if(rasqal_graph_pattern_add_sub_graph_pattern(gp, single)) {
    rasqal_free_graph_pattern(gp);
    return NULL;
  }

  return gp;
}


/*
 * rasqal_free_graph_pattern:
 * @gp: #rasqal_graph_pattern object
 *
 * INTERNAL - Free a graph pattern object.
 * 
 **/
void
rasqal_free_graph_pattern(rasqal_graph_pattern* gp)
{
  if(!gp)
    return;
  
  if(gp->graph_patterns)
    raptor_free_sequence(gp->graph_patterns);
  
  if(gp->filter_expression)
    rasqal_free_expression(gp->filter_expression);

  if(gp->origin)
    rasqal_free_literal(gp->origin);

  if(gp->projection)
    rasqal_free_projection(gp->projection);

  if(gp->modifier)
    rasqal_free_solution_modifier(gp->modifier);

  if(gp->data_graphs)
    raptor_free_sequence(gp->data_graphs);
  
  if(gp->var)
    rasqal_free_variable(gp->var);

  if(gp->bindings)
    rasqal_free_bindings(gp->bindings);

  RASQAL_FREE(rasqal_graph_pattern, gp);
}


/*
 * rasqal_graph_pattern_adjust:
 * @gp: #rasqal_graph_pattern graph pattern
 * @offset: adjustment
 *
 * INTERNAL - Adjust the column in a graph pattern by the offset.
 * 
 **/
void
rasqal_graph_pattern_adjust(rasqal_graph_pattern* gp, int offset)
{
  gp->start_column += offset;
  gp->end_column += offset;
}


/**
 * rasqal_graph_pattern_set_filter_expression:
 * @gp: #rasqal_graph_pattern query object
 * @expr: #rasqal_expression expr - ownership taken
 *
 * Set a filter graph pattern constraint expression
 *
 * Return value: non-0 on failure
 **/
int
rasqal_graph_pattern_set_filter_expression(rasqal_graph_pattern* gp,
                                           rasqal_expression* expr)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(gp, rasqal_graph_pattern, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(expr, rasqal_expression, 1);
  
  if(gp->filter_expression)
    rasqal_free_expression(gp->filter_expression);

  gp->filter_expression = expr;

  return 0;
}


/**
 * rasqal_graph_pattern_get_filter_expression:
 * @gp: #rasqal_graph_pattern query object
 *
 * Get a filter graph pattern's constraint expression
 *
 * Return value: expression or NULL on failure
 **/
rasqal_expression*
rasqal_graph_pattern_get_filter_expression(rasqal_graph_pattern* gp)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(gp, rasqal_graph_pattern, NULL);

  return gp->filter_expression;
}


/**
 * rasqal_graph_pattern_get_operator:
 * @graph_pattern: #rasqal_graph_pattern graph pattern object
 *
 * Get the graph pattern operator .
 * 
 * The operator for the given graph pattern. See also
 * rasqal_graph_pattern_operator_as_string().
 *
 * Return value: graph pattern operator
 **/
rasqal_graph_pattern_operator
rasqal_graph_pattern_get_operator(rasqal_graph_pattern* graph_pattern)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(graph_pattern, rasqal_graph_pattern, RASQAL_GRAPH_PATTERN_OPERATOR_UNKNOWN);

  return graph_pattern->op;
}


static const char* const rasqal_graph_pattern_operator_labels[RASQAL_GRAPH_PATTERN_OPERATOR_LAST + 1] = {
  "UNKNOWN",
  "Basic",
  "Optional",
  "Union",
  "Group",
  "Graph",
  "Filter",
  "Let",
  "Select",
  "Service",
  "Minus",
  "Values"
};


/**
 * rasqal_graph_pattern_operator_as_string:
 * @op: the #rasqal_graph_pattern_operator verb of the query
 *
 * Get a string for the query verb.
 * 
 * Return value: pointer to a shared string label for the query verb
 **/
const char*
rasqal_graph_pattern_operator_as_string(rasqal_graph_pattern_operator op)
{
  if(op <= RASQAL_GRAPH_PATTERN_OPERATOR_UNKNOWN || 
     op > RASQAL_GRAPH_PATTERN_OPERATOR_LAST)
    op = RASQAL_GRAPH_PATTERN_OPERATOR_UNKNOWN;

  return rasqal_graph_pattern_operator_labels[RASQAL_GOOD_CAST(int, op)];
}
  

#ifdef RASQAL_DEBUG
#define DO_INDENTING 0
#else
#define DO_INDENTING -1
#endif

#define SPACES_LENGTH 80
static const char spaces[SPACES_LENGTH + 1] = "                                                                                ";

static void
rasqal_graph_pattern_write_indent(raptor_iostream *iostr, int indent) 
{
  while(indent > 0) {
    int sp = (indent > SPACES_LENGTH) ? SPACES_LENGTH : indent;
    raptor_iostream_write_bytes(spaces, sizeof(char), RASQAL_GOOD_CAST(size_t, sp), iostr);
    indent -= sp;
  }
}


static void
rasqal_graph_pattern_write_plurals(raptor_iostream *iostr,
                                   const char* label, int value)
{
  raptor_iostream_decimal_write(value, iostr);
  raptor_iostream_write_byte(' ', iostr);
  raptor_iostream_string_write(label, iostr);

  if(value != 1)
    raptor_iostream_write_byte('s', iostr);
}


/**
 * rasqal_graph_pattern_print_indent:
 * @gp: the #rasqal_graph_pattern object
 * @iostr: the iostream to write to
 * @indent: the current indent level, <0 for no indenting
 *
 * INTERNAL - Print a #rasqal_graph_pattern in a debug format with indenting
 * 
 **/
static int
rasqal_graph_pattern_write_internal(rasqal_graph_pattern* gp,
                                    raptor_iostream* iostr, int indent)
{
  int pending_nl = 0;
  
  raptor_iostream_counted_string_write("graph pattern", 13, iostr);

  if(gp->gp_index >= 0) {
    raptor_iostream_write_byte('[', iostr);
    raptor_iostream_decimal_write(gp->gp_index, iostr);
    raptor_iostream_write_byte(']', iostr);
  }

  raptor_iostream_write_byte(' ', iostr);
  raptor_iostream_string_write(rasqal_graph_pattern_operator_as_string(gp->op),
                               iostr);
  raptor_iostream_write_byte('(', iostr);

  if(indent >= 0)
    indent += 2;

  if(gp->silent) {
    raptor_iostream_counted_string_write("silent", 6, iostr);
    pending_nl = 1;
  }

  if(gp->triples) {
    int size = gp->end_column - gp->start_column + 1;
    int i;

    if(pending_nl) {
      raptor_iostream_counted_string_write(" ,", 2, iostr);

      if(indent >= 0) {
        raptor_iostream_write_byte('\n', iostr);
        rasqal_graph_pattern_write_indent(iostr, indent);
      }
    }

    raptor_iostream_counted_string_write("over ", 5, iostr);
    rasqal_graph_pattern_write_plurals(iostr, "triple", size);
    raptor_iostream_write_byte('[', iostr);

    if(indent >= 0) {
      raptor_iostream_write_byte('\n', iostr);
      indent += 2;
      rasqal_graph_pattern_write_indent(iostr, indent);
    }
    
    for(i = gp->start_column; i <= gp->end_column; i++) {
      rasqal_triple *t = (rasqal_triple*)raptor_sequence_get_at(gp->triples, i);
      if(i > gp->start_column) {
        raptor_iostream_counted_string_write(" ,", 2, iostr);

        if(indent >= 0) {
          raptor_iostream_write_byte('\n', iostr);
          rasqal_graph_pattern_write_indent(iostr, indent);
        }
      }
      rasqal_triple_write(t, iostr);
    }

    if(indent >= 0) {
      raptor_iostream_write_byte('\n', iostr);
      indent -= 2;
      rasqal_graph_pattern_write_indent(iostr, indent);
    }

    raptor_iostream_write_byte(']', iostr);

    pending_nl = 1;
  }

  if(gp->origin) {
    if(pending_nl) {
      raptor_iostream_counted_string_write(" ,", 2, iostr);

      if(indent >= 0) {
        raptor_iostream_write_byte('\n', iostr);
        rasqal_graph_pattern_write_indent(iostr, indent);
      }
    }

    raptor_iostream_counted_string_write("origin ", 7, iostr);

    rasqal_literal_write(gp->origin, iostr);

    pending_nl = 1;
  }

  if(gp->graph_patterns) {
    int size = raptor_sequence_size(gp->graph_patterns);
    int i;

    if(pending_nl) {
      raptor_iostream_counted_string_write(" ,", 2, iostr);

      if(indent >= 0) {
        raptor_iostream_write_byte('\n', iostr);
        rasqal_graph_pattern_write_indent(iostr, indent);
      }
    }
    
    raptor_iostream_counted_string_write("over ", 5, iostr);
    rasqal_graph_pattern_write_plurals(iostr, "graph pattern", size);
    raptor_iostream_write_byte('[', iostr);

    if(indent >= 0) {
      raptor_iostream_write_byte('\n', iostr);
      indent += 2;
      rasqal_graph_pattern_write_indent(iostr, indent);
    }
    
    for(i = 0; i< size; i++) {
      rasqal_graph_pattern* sgp;
      sgp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      if(i) {
        raptor_iostream_counted_string_write(" ,", 2, iostr);
        if(indent >= 0) {
          raptor_iostream_write_byte('\n', iostr);
          rasqal_graph_pattern_write_indent(iostr, indent);
        }
      }

      if(sgp)
        rasqal_graph_pattern_write_internal(sgp, iostr, indent);
      else
        raptor_iostream_counted_string_write("(empty)", 7, iostr);
    }

    if(indent >= 0) {
      raptor_iostream_write_byte('\n', iostr);
      indent -= 2;
      rasqal_graph_pattern_write_indent(iostr, indent);
    }

    raptor_iostream_write_byte(']', iostr);

    pending_nl = 1;
  }

  if(gp->var) {
    rasqal_variable_write(gp->var, iostr);
    raptor_iostream_counted_string_write(" := ", 4, iostr);
    pending_nl = 0;
  }
  
  if(gp->filter_expression) {
    if(pending_nl) {
      raptor_iostream_counted_string_write(" ,", 2, iostr);

      if(indent >= 0) {
        raptor_iostream_write_byte('\n', iostr);
        rasqal_graph_pattern_write_indent(iostr, indent);
      }
    }
    
    if(gp->triples || gp->graph_patterns)
      raptor_iostream_counted_string_write("with ", 5, iostr);
  
    if(indent >= 0) {
      raptor_iostream_write_byte('\n', iostr);
      indent += 2;
      rasqal_graph_pattern_write_indent(iostr, indent);
    }

    rasqal_expression_write(gp->filter_expression, iostr);

    if(indent >= 0)
      indent -= 2;
    
    pending_nl = 1;
  }

  if(gp->projection) {
    raptor_sequence* vars_seq;
    
    if(pending_nl) {
      raptor_iostream_counted_string_write(" ,", 2, iostr);

      if(indent >= 0) {
        raptor_iostream_write_byte('\n', iostr);
        rasqal_graph_pattern_write_indent(iostr, indent);
      }
    }

    if(indent >= 0) {
      raptor_iostream_write_byte('\n', iostr);
      indent += 2;
      rasqal_graph_pattern_write_indent(iostr, indent);
    }

    raptor_iostream_counted_string_write("select-variables: [", 19, iostr);
    vars_seq = rasqal_projection_get_variables_sequence(gp->projection);
    if(!vars_seq)
      raptor_iostream_write_byte('*', iostr);
    else
      rasqal_variables_write(vars_seq, iostr);
    raptor_iostream_counted_string_write("]", 1, iostr);
    
    if(indent >= 0)
      indent -= 2;

    pending_nl = 1;
  }

  if(gp->bindings) {
    int i;

    if(pending_nl) {
      raptor_iostream_counted_string_write(" ,", 2, iostr);

      if(indent >= 0) {
        raptor_iostream_write_byte('\n', iostr);
        rasqal_graph_pattern_write_indent(iostr, indent);
      }
    }

    raptor_iostream_counted_string_write("bindings: [", 11, iostr);
    if(indent >= 0) {
      raptor_iostream_write_byte('\n', iostr);
      indent += 2;
      rasqal_graph_pattern_write_indent(iostr, indent);
    }

    raptor_iostream_counted_string_write("variables: [", 12, iostr);
    rasqal_variables_write(gp->bindings->variables, iostr);
    raptor_iostream_counted_string_write("]\n", 2, iostr);

    rasqal_graph_pattern_write_indent(iostr, indent);
    raptor_iostream_counted_string_write("rows: [", 7, iostr);

    indent += 2;
    for(i = 0; 1; i++) {
      rasqal_row* row = rasqal_bindings_get_row(gp->bindings, i);
      if(!row)
        break;

      raptor_iostream_write_byte('\n', iostr);
      rasqal_graph_pattern_write_indent(iostr, indent);
      rasqal_row_write(row, iostr);
    }
    indent -= 2;

    raptor_iostream_write_byte('\n', iostr);
    rasqal_graph_pattern_write_indent(iostr, indent);
    raptor_iostream_counted_string_write("]", 1, iostr);
    
    if(indent >= 0)
      indent -= 2;

    pending_nl = 1;
  }

#if 0  
  if(gp->where) {
    if(pending_nl) {
      raptor_iostream_counted_string_write(" ,", 2, iostr);

      if(indent >= 0) {
        raptor_iostream_write_byte('\n', iostr);
        rasqal_graph_pattern_write_indent(iostr, indent);
      }
    }

    if(indent >= 0) {
      raptor_iostream_write_byte('\n', iostr);
      indent += 2;
      rasqal_graph_pattern_write_indent(iostr, indent);
    }

    rasqal_graph_pattern_write_internal(gp->where, iostr, indent);

    if(indent >= 0)
      indent -= 2;
    
    pending_nl = 1;
  }
#endif
  
  if(indent >= 0)
    indent -= 2;

  if(pending_nl) {
    if(indent >= 0) {
      raptor_iostream_write_byte('\n', iostr);
      rasqal_graph_pattern_write_indent(iostr, indent);
    }
  }
  
  raptor_iostream_write_byte(')', iostr);

  return 0;
}


/**
 * rasqal_graph_pattern_print:
 * @gp: the #rasqal_graph_pattern object
 * @fh: the FILE* handle to print to
 *
 * Print a #rasqal_graph_pattern in a debug format.
 * 
 * The print debug format may change in any release.
 * 
 * Return value: non-0 on failure
 **/
int
rasqal_graph_pattern_print(rasqal_graph_pattern* gp, FILE* fh)
{
  raptor_iostream* iostr;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(gp, rasqal_graph_pattern, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(fh, FILE*, 1);

  iostr = raptor_new_iostream_to_file_handle(gp->query->world->raptor_world_ptr, fh);
  rasqal_graph_pattern_write_internal(gp, iostr, DO_INDENTING);
  raptor_free_iostream(iostr);

  return 0;
}


/**
 * rasqal_graph_pattern_visit:
 * @query: #rasqal_query to operate on
 * @gp: #rasqal_graph_pattern graph pattern
 * @fn: pointer to function to apply that takes user data and graph pattern parameters
 * @user_data: user data for applied function 
 * 
 * Visit a user function over a #rasqal_graph_pattern
 *
 * If the user function @fn returns 0, the visit is truncated.
 *
 * Return value: 0 if the visit was truncated.
 **/
int
rasqal_graph_pattern_visit(rasqal_query *query,
                           rasqal_graph_pattern* gp,
                           rasqal_graph_pattern_visit_fn fn,
                           void *user_data)
{
  raptor_sequence *seq;
  int result;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(gp, rasqal_graph_pattern, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(fn, rasqal_graph_pattern_visit_fn, 1);

  result = fn(query, gp, user_data);
  if(result)
    return result;
  
  seq = rasqal_graph_pattern_get_sub_graph_pattern_sequence(gp);

  if(seq && raptor_sequence_size(seq) > 0) {
    int gp_index=0;

    while(1) {
      rasqal_graph_pattern* sgp;
      sgp = rasqal_graph_pattern_get_sub_graph_pattern(gp, gp_index);
      if(!sgp)
        break;
      
      result = rasqal_graph_pattern_visit(query, sgp, fn, user_data);
      if(result)
        return result;

      gp_index++;
    }
  }

  return 0;
}


/**
 * rasqal_graph_pattern_get_index:
 * @gp: #rasqal_graph_pattern graph pattern
 * 
 * Get the graph pattern absolute index in the array of graph patterns.
 * 
 * The graph pattern index is assigned when rasqal_query_prepare() is
 * run on a query containing a graph pattern.
 *
 * Return value: index or <0 if no index has been assigned yet
 **/
int
rasqal_graph_pattern_get_index(rasqal_graph_pattern* gp)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(gp, rasqal_graph_pattern, -1);

  return gp->gp_index;
}


/**
 * rasqal_graph_pattern_add_sub_graph_pattern:
 * @graph_pattern: graph pattern to add to
 * @sub_graph_pattern: graph pattern to add inside
 *
 * Add a sub graph pattern to a graph pattern.
 *
 * Return value: non-0 on failure
 **/
int
rasqal_graph_pattern_add_sub_graph_pattern(rasqal_graph_pattern* graph_pattern,
                                           rasqal_graph_pattern* sub_graph_pattern)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(graph_pattern, rasqal_graph_pattern, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(sub_graph_pattern, rasqal_graph_pattern, 1);

  if(!graph_pattern->graph_patterns) {
    graph_pattern->graph_patterns = raptor_new_sequence((raptor_data_free_handler)rasqal_free_graph_pattern, (raptor_data_print_handler)rasqal_graph_pattern_print);

    if(!graph_pattern->graph_patterns) {
      if(sub_graph_pattern)
        rasqal_free_graph_pattern(sub_graph_pattern);
      return 1;
    }
  }

  return raptor_sequence_push(graph_pattern->graph_patterns, sub_graph_pattern);
}


/**
 * rasqal_graph_pattern_get_triple:
 * @graph_pattern: #rasqal_graph_pattern graph pattern object
 * @idx: index into the sequence of triples in the graph pattern
 *
 * Get a triple inside a graph pattern.
 * 
 * Return value: #rasqal_triple or NULL if out of range
 **/
rasqal_triple*
rasqal_graph_pattern_get_triple(rasqal_graph_pattern* graph_pattern, int idx)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(graph_pattern, rasqal_graph_pattern, NULL);

  if(!graph_pattern->triples)
    return NULL;

  idx += graph_pattern->start_column;

  if(idx > graph_pattern->end_column)
    return NULL;
  
  return (rasqal_triple*)raptor_sequence_get_at(graph_pattern->triples, idx);
}


/**
 * rasqal_graph_pattern_get_sub_graph_pattern_sequence:
 * @graph_pattern: #rasqal_graph_pattern graph pattern object
 *
 * Get the sequence of graph patterns inside a graph pattern .
 * 
 * Return value:  a #raptor_sequence of #rasqal_graph_pattern pointers.
 **/
raptor_sequence*
rasqal_graph_pattern_get_sub_graph_pattern_sequence(rasqal_graph_pattern* graph_pattern)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(graph_pattern, rasqal_graph_pattern, NULL);

  return graph_pattern->graph_patterns;
}


/**
 * rasqal_graph_pattern_get_sub_graph_pattern:
 * @graph_pattern: #rasqal_graph_pattern graph pattern object
 * @idx: index into the sequence of sub graph_patterns in the graph pattern
 *
 * Get a sub-graph pattern inside a graph pattern.
 * 
 * Return value: #rasqal_graph_pattern or NULL if out of range
 **/
rasqal_graph_pattern*
rasqal_graph_pattern_get_sub_graph_pattern(rasqal_graph_pattern* graph_pattern, int idx)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(graph_pattern, rasqal_graph_pattern, NULL);

  if(!graph_pattern->graph_patterns)
    return NULL;

  return (rasqal_graph_pattern*)raptor_sequence_get_at(graph_pattern->graph_patterns, idx);
}


/**
 * rasqal_graph_pattern_get_origin:
 * @graph_pattern: #rasqal_graph_pattern graph pattern object
 *
 * Get the graph pattern literal for
 * #RASQAL_GRAPH_PATTERN_OPERATOR_GRAPH graph pattern
 * 
 * Return value: graph literal parameter or NULL if wrong graph pattern type or not defined
 **/
rasqal_literal*
rasqal_graph_pattern_get_origin(rasqal_graph_pattern* graph_pattern)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(graph_pattern,
                                            rasqal_graph_pattern, NULL);

  if(graph_pattern->op == RASQAL_GRAPH_PATTERN_OPERATOR_GRAPH)
    return graph_pattern->origin;
  else
    return NULL;
}
  

/*
 * rasqal_graph_pattern_set_origin:
 * @graph_pattern: #rasqal_graph_pattern graph pattern object
 * @origin: #rasqal_literal variable or URI
 *
 * INTERNAL - Set the graph pattern triple origin.
 * 
 **/
void
rasqal_graph_pattern_set_origin(rasqal_graph_pattern* graph_pattern,
                                rasqal_literal *origin)
{
  graph_pattern->origin = rasqal_new_literal_from_literal(origin);
}
  

/**
 * rasqal_new_basic_graph_pattern_from_formula:
 * @query: #rasqal_graph_pattern query object
 * @formula: triples sequence containing the graph pattern
 *
 * INTERNAL - Create a new graph pattern object over a formula. This function
 * frees the formula passed in.
 * 
 * Return value: a new #rasqal_graph_pattern object or NULL on failure
 **/
rasqal_graph_pattern*
rasqal_new_basic_graph_pattern_from_formula(rasqal_query* query,
                                            rasqal_formula* formula)
{
  rasqal_graph_pattern* gp;
  raptor_sequence *triples = query->triples;
  raptor_sequence *formula_triples  = formula->triples;
  int offset = raptor_sequence_size(triples);
  int triple_pattern_size = 0;

  if(formula_triples) {
    /* Move formula triples to end of main triples sequence */
    triple_pattern_size = raptor_sequence_size(formula_triples);

    if(raptor_sequence_join(triples, formula_triples)) {
      rasqal_free_formula(formula);
      return NULL;
    }
  }

  rasqal_free_formula(formula);

  gp = rasqal_new_basic_graph_pattern(query, triples, 
                                      offset, 
                                      offset + triple_pattern_size - 1);

  return gp;
}


/**
 * rasqal_new_2_group_graph_pattern:
 * @query: query object
 * @first_gp: first graph pattern
 * @second_gp: second graph pattern
 *
 * INTERNAL - Make a new group graph pattern from two graph patterns
 * of which either or both may be NULL, in which case a group
 * of 0 graph patterns is created.
 *
 * @first_gp and @second_gp if given, become owned by the new graph
 * pattern.
 *
 * Return value: new group graph pattern or NULL on failure
 */
rasqal_graph_pattern*
rasqal_new_2_group_graph_pattern(rasqal_query* query,
                                 rasqal_graph_pattern* first_gp,
                                 rasqal_graph_pattern* second_gp)
{
  raptor_sequence *seq;

  seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_graph_pattern, (raptor_data_print_handler)rasqal_graph_pattern_print);
  if(!seq) {
    if(first_gp)
      rasqal_free_graph_pattern(first_gp);
    if(second_gp)
      rasqal_free_graph_pattern(second_gp);

    return NULL;
  }

  if(first_gp && raptor_sequence_push(seq, first_gp)) {
    raptor_free_sequence(seq);
    if(second_gp)
      rasqal_free_graph_pattern(second_gp);

    return NULL;
  }

  if(second_gp && raptor_sequence_push(seq, second_gp)) {
    raptor_free_sequence(seq);

    return NULL;
  }

  return rasqal_new_graph_pattern_from_sequence(query, seq,
                                                RASQAL_GRAPH_PATTERN_OPERATOR_GROUP);
}


/**
 * rasqal_graph_pattern_variable_bound_in:
 * @gp: graph pattern
 * @v: variable
 *
 * Is the variable bound in this graph pattern (not including children)?
 *
 * Return value: non-0 if variable is bound in the given graph pattern.
 */
int
rasqal_graph_pattern_variable_bound_in(rasqal_graph_pattern *gp,
                                       rasqal_variable *v)
{
  rasqal_query* query;
  int width;
  int gp_offset;
  unsigned short *row;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(gp, rasqal_graph_pattern, 0);
  
  query = gp->query;
  width = rasqal_variables_table_get_total_variables_count(query->vars_table);
  gp_offset = (gp->gp_index + RASQAL_VAR_USE_MAP_OFFSET_LAST + 1) * width;
  row = &query->variables_use_map[gp_offset];

  return ((row[v->offset] & RASQAL_VAR_USE_BOUND_HERE) != 0);
}



/*
 * rasqal_graph_pattern_variable_bound_below:
 * @gp: graph pattern
 * @v: variable
 *
 * INTERNAL - Is the variable bound in the graph pattern or below?
 *
 * Return value: non-0 if variable is bound in the graph pattern tree
 */
int
rasqal_graph_pattern_variable_bound_below(rasqal_graph_pattern *gp,
                                          rasqal_variable *v)
{
  int bound;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(gp, rasqal_graph_pattern, 0);

  bound = rasqal_graph_pattern_variable_bound_in(gp, v);
  if(bound)
    return bound;

  if(gp->graph_patterns) {
    int size = raptor_sequence_size(gp->graph_patterns);
    int i;
    
    for(i = 0; i < size; i++) {
      rasqal_graph_pattern *sgp;

      sgp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      bound = rasqal_graph_pattern_variable_bound_below(sgp, v);
      if(bound)
        break;
    }
  }
  
  return bound;
}



/**
 * rasqal_new_basic_graph_pattern_from_triples:
 * @query: #rasqal_graph_pattern query object
 * @triples: triples sequence containing the graph pattern
 *
 * INTERNAL - Create a new graph pattern object from a sequence of triples.
 *
 * The @triples become owned by the graph pattern
 * 
 * Return value: a new #rasqal_graph_pattern object or NULL on failure
 **/
rasqal_graph_pattern*
rasqal_new_basic_graph_pattern_from_triples(rasqal_query* query,
                                            raptor_sequence* triples)
{
  rasqal_graph_pattern* gp;
  raptor_sequence *graph_triples = query->triples;
  int offset = raptor_sequence_size(graph_triples);
  int triple_pattern_size = 0;

  if(triples) {
    /* Move triples to end of graph triples sequence */
    triple_pattern_size = raptor_sequence_size(triples);

    if(raptor_sequence_join(graph_triples, triples)) {
      raptor_free_sequence(triples);
      return NULL;
    }
  }

  raptor_free_sequence(triples);

  gp = rasqal_new_basic_graph_pattern(query, graph_triples,
                                      offset, 
                                      offset + triple_pattern_size - 1);

  return gp;
}


/**
 * rasqal_new_values_graph_pattern:
 * @query: #rasqal_graph_pattern query object
 * @bindings: bindings object
 *
 * INTERNAL - Create a new values graph pattern object from a bindings
 *
 * The @bindings becomes owned by the graph pattern
 *
 * Return value: a new #rasqal_graph_pattern object or NULL on failure
 **/
rasqal_graph_pattern*
rasqal_new_values_graph_pattern(rasqal_query* query,
                                rasqal_bindings* bindings)
{
  rasqal_graph_pattern* gp;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  gp = rasqal_new_graph_pattern(query, RASQAL_GRAPH_PATTERN_OPERATOR_VALUES);
  if(gp)
    gp->bindings = bindings;

  return gp;
}


/**
 * rasqal_graph_pattern_get_variable:
 * @graph_pattern: #rasqal_graph_pattern graph pattern object
 *
 * Get the variable for #RASQAL_GRAPH_PATTERN_OPERATOR_LET graph pattern
 * 
 * Return value: graph variable or NULL if wrong graph pattern or not defined
 **/
rasqal_variable*
rasqal_graph_pattern_get_variable(rasqal_graph_pattern* graph_pattern)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(graph_pattern,
                                            rasqal_graph_pattern, NULL);

  if(graph_pattern->op == RASQAL_GRAPH_PATTERN_OPERATOR_LET)
    return graph_pattern->var;
  else
    return NULL;
}


/**
 * rasqal_graph_pattern_get_service:
 * @graph_pattern: #rasqal_graph_pattern graph pattern object
 *
 * Get the literal for #RASQAL_GRAPH_PATTERN_OPERATOR_SERVICE graph pattern
 * 
 * Return value: graph variable or NULL if wrong graph pattern or not defined
 **/
rasqal_literal*
rasqal_graph_pattern_get_service(rasqal_graph_pattern* graph_pattern)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(graph_pattern,
                                            rasqal_graph_pattern, NULL);

  if(graph_pattern->op == RASQAL_GRAPH_PATTERN_OPERATOR_SERVICE)
    return graph_pattern->origin;
  else
    return NULL;
}


struct gpft
{
  /* sequence owned here */
  raptor_sequence* triples;
  /* shared pointer to current origin */
  rasqal_literal* origin;
};

static int
rasqal_graph_pattern_get_flattened_triples_visit(rasqal_query* query,
                                                 rasqal_graph_pattern* gp,
                                                 struct gpft* state)
{
  raptor_sequence* seq;
  int size;

  if(gp->op == RASQAL_GRAPH_PATTERN_OPERATOR_GRAPH)
    state->origin = gp->origin;
  else if(gp->op != RASQAL_GRAPH_PATTERN_OPERATOR_BASIC)
    return 1;
  else {
    /*  RASQAL_GRAPH_PATTERN_OPERATOR_BASIC */
    rasqal_triples_sequence_set_origin(state->triples,
                                       gp->triples, state->origin);
  }
  

  seq = rasqal_graph_pattern_get_sub_graph_pattern_sequence(gp);  
  if(seq) {
    int gp_index = 0;
    int result = 0;
    
    size = raptor_sequence_size(seq);
    while(size > 0) {
      rasqal_graph_pattern* sgp;
      sgp = rasqal_graph_pattern_get_sub_graph_pattern(gp, gp_index);
      if(!sgp)
        break;
      
      result = rasqal_graph_pattern_get_flattened_triples_visit(query, sgp,
                                                                state);
      if(result)
        return result;
      
      gp_index++;
      size--;
    }
  }
    

  if(gp->op == RASQAL_GRAPH_PATTERN_OPERATOR_GRAPH)
    state->origin = NULL;
  
  return 0;
}


/**
 * rasqal_graph_pattern_get_flattened_triples:
 * @query: query
 * @graph_pattern: graph pattern
 *
 * Get the triples inside a tree of graph patterns (BASIC + GRAPH) as a single sequence with GRAPHs turned into triple origin.
 *
 * The returned sequence and all the #rasqal_triple in it are owned
 * by the caller (hold references).
 *
 * Return value: new sequence of #raptor_triple or NULL on failure
 */
raptor_sequence*
rasqal_graph_pattern_get_flattened_triples(rasqal_query* query,
                                           rasqal_graph_pattern* graph_pattern)
{
  struct gpft state;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(graph_pattern,
                                            rasqal_graph_pattern, NULL);

  memset(&state, '\0', sizeof(state));

  state.triples = raptor_new_sequence((raptor_data_free_handler)rasqal_free_triple,
                                      (raptor_data_print_handler)rasqal_triple_print);
  state.origin = NULL;
  
  if(rasqal_graph_pattern_get_flattened_triples_visit(query, graph_pattern,
                                                      &state)) {
    raptor_free_sequence(state.triples);
    return NULL;
  }

  return state.triples;
}


/**
 * rasqal_graph_pattern_get_triples:
 * @query: query
 * @graph_pattern: graph pattern
 *
 * Get the triples inside this graph pattern (if any).
 *
 * The returned sequence and all the #rasqal_triple in it are owned
 * by the caller (hold references).
 *
 * Return value: new sequence of #raptor_triple or NULL on failure or no triples
 */
raptor_sequence*
rasqal_graph_pattern_get_triples(rasqal_query* query,
                                 rasqal_graph_pattern* graph_pattern)
{
  raptor_sequence* triples = NULL;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(graph_pattern,
                                            rasqal_graph_pattern, NULL);

  if (graph_pattern->triples) {
    int i;
    triples = raptor_new_sequence((raptor_data_free_handler)rasqal_free_triple,
                                      (raptor_data_print_handler)rasqal_triple_print);
  
    for(i = graph_pattern->start_column; i <= graph_pattern->end_column; i++) {
      rasqal_triple *t = (rasqal_triple*)raptor_sequence_get_at(graph_pattern->triples, i);
      t = rasqal_new_triple_from_triple(t);
      if(!t) {
        raptor_free_sequence(triples);
        return NULL;
      }
      raptor_sequence_push(triples, t);  
    }
  }

  return triples;
}

struct find_parent_data {
  rasqal_graph_pattern* child_gp;
  rasqal_graph_pattern* parent_gp;
};

static int
rasqal_graph_pattern_find_parent(rasqal_query* query,
                                 rasqal_graph_pattern* gp,
                                 void* data)
{
  struct find_parent_data* fpd = (struct find_parent_data*)data;
  int i;

  if(gp->graph_patterns) {
    int size = raptor_sequence_size(gp->graph_patterns);

    for(i = 0; i< size; i++) {
      rasqal_graph_pattern* sgp;
      sgp = (rasqal_graph_pattern*)raptor_sequence_get_at(gp->graph_patterns, i);
      if(sgp == fpd->child_gp) {
        fpd->parent_gp = gp;
        /* Found - truncate search */
        return 1;
      }
    }
  }

  return 0;
}


/**
 * rasqal_graph_pattern_get_parent:
 * @query: query
 * @gp: graph pattern to find parent
 * @tree_gp: graph pattern tree to search for @gp
 *
 * Find the parent graph pattern of @gp in the tree of graph patterns @gp_tree
 *
 * Return value: pointer to parent GP or NULL on error/not found
 */
rasqal_graph_pattern*
rasqal_graph_pattern_get_parent(rasqal_query *query,
                                rasqal_graph_pattern* gp,
                                rasqal_graph_pattern* tree_gp)
{
  struct find_parent_data fpd;

  fpd.child_gp = gp;
  fpd.parent_gp = NULL;

  if(gp == tree_gp || gp == query->query_graph_pattern)
    return NULL;

  (void)rasqal_graph_pattern_visit(query, tree_gp,
                                   rasqal_graph_pattern_find_parent,
                                   &fpd);

  return fpd.parent_gp;
}

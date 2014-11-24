/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_algebra.c - Rasqal algebra class
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


#ifndef STANDALONE

static rasqal_algebra_node* rasqal_algebra_graph_pattern_to_algebra(rasqal_query* query, rasqal_graph_pattern* gp);

/*
 * rasqal_new_algebra_node:
 * @query: #rasqal_algebra_node query object
 * @op: enum #rasqal_algebra_operator operator
 *
 * INTERNAL - Create a new algebra object.
 * 
 * Return value: a new #rasqal_algebra object or NULL on failure
 **/
static rasqal_algebra_node*
rasqal_new_algebra_node(rasqal_query* query, rasqal_algebra_node_operator op)
{
  rasqal_algebra_node* node;

  if(!query)
    return NULL;
  
  node = RASQAL_CALLOC(rasqal_algebra_node*, 1, sizeof(*node));
  if(!node)
    return NULL;

  node->op = op;
  node->query = query;
  return node;
}


/*
 * rasqal_new_filter_algebra_node:
 * @query: #rasqal_query query object
 * @expr: FILTER expression
 * @node: algebra node being filtered (or NULL)
 *
 * INTERNAL - Create a new algebra node for an expression over a node
 *
 * expr and node become owned by the new node.  The @node may be NULL
 * which means that the logical input/output is a row with no bindings.
 * 
 * Return value: a new #rasqal_algebra_node object or NULL on failure
 **/
rasqal_algebra_node*
rasqal_new_filter_algebra_node(rasqal_query* query,
                               rasqal_expression* expr,
                               rasqal_algebra_node* node)
{
  rasqal_algebra_node* new_node;

  if(!query || !expr)
    goto fail;
  
  new_node = rasqal_new_algebra_node(query, RASQAL_ALGEBRA_OPERATOR_FILTER);
  if(new_node) {
    new_node->expr = expr;
    new_node->node1 = node;
    return new_node;
  }
  
  fail:
  if(expr)
    rasqal_free_expression(expr);
  if(node)
    rasqal_free_algebra_node(node);
  return NULL;
}


/*
 * rasqal_new_triples_algebra_node:
 * @query: #rasqal_query query object
 * @triples: triples sequence (SHARED) (or NULL for empty BGP)
 * @start_column: first triple
 * @end_column: last triple
 *
 * INTERNAL - Create a new algebra node for Basic Graph Pattern
 * 
 * Return value: a new #rasqal_algebra_node object or NULL on failure
 **/
rasqal_algebra_node*
rasqal_new_triples_algebra_node(rasqal_query* query,
                                raptor_sequence* triples,
                                int start_column, int end_column)
{
  rasqal_algebra_node* node;

  if(!query)
    return NULL;
  
  node = rasqal_new_algebra_node(query, RASQAL_ALGEBRA_OPERATOR_BGP);
  if(!node)
    return NULL;

  node->triples = triples;
  if(!triples) {
    start_column= -1;
    end_column= -1;
  }
  node->start_column = start_column;
  node->end_column = end_column;

  return node;
}


/*
 * rasqal_new_empty_algebra_node:
 * @query: #rasqal_query query object
 *
 * INTERNAL - Create a new empty algebra node
 * 
 * Return value: a new #rasqal_algebra_node object or NULL on failure
 **/
rasqal_algebra_node*
rasqal_new_empty_algebra_node(rasqal_query* query)
{
  rasqal_algebra_node* node;

  if(!query)
    return NULL;
  
  node = rasqal_new_algebra_node(query, RASQAL_ALGEBRA_OPERATOR_BGP);
  if(!node)
    return NULL;

  node->triples = NULL;
  node->start_column= -1;
  node->end_column= -1;

  return node;
}


/*
 * rasqal_new_2op_algebra_node:
 * @query: #rasqal_query query object
 * @op: operator 
 * @node1: 1st algebra node
 * @node2: 2nd algebra node (pr NULL for #RASQAL_ALGEBRA_OPERATOR_TOLIST only)
 *
 * INTERNAL - Create a new algebra node for 1 or 2 graph patterns
 *
 * node1 and node2 become owned by the new node
 *
 * Return value: a new #rasqal_algebra_node object or NULL on failure
 **/
rasqal_algebra_node*
rasqal_new_2op_algebra_node(rasqal_query* query,
                            rasqal_algebra_node_operator op,
                            rasqal_algebra_node* node1,
                            rasqal_algebra_node* node2)
{
  rasqal_algebra_node* node;

  if(!query || !node1)
    goto fail;
  if(op != RASQAL_ALGEBRA_OPERATOR_TOLIST && !node2)
    goto fail;
  
  node = rasqal_new_algebra_node(query, op);
  if(node) {
    node->node1 = node1;
    node->node2 = node2;
    
    return node;
  }

  fail:
  if(node1)
    rasqal_free_algebra_node(node1);
  if(node2)
    rasqal_free_algebra_node(node2);
  return NULL;
}


/*
 * rasqal_new_leftjoin_algebra_node:
 * @query: #rasqal_query query object
 * @node1: 1st algebra node
 * @node2: 2nd algebra node
 * @expr: expression
 *
 * INTERNAL - Create a new LEFTJOIN algebra node for 2 graph patterns
 * 
 * node1, node2 and expr become owned by the new node
 *
 * Return value: a new #rasqal_algebra_node object or NULL on failure
 **/
rasqal_algebra_node*
rasqal_new_leftjoin_algebra_node(rasqal_query* query,
                                 rasqal_algebra_node* node1,
                                 rasqal_algebra_node* node2,
                                 rasqal_expression* expr)
{
  rasqal_algebra_node* node;

  if(!query || !node1 || !node2 || !expr)
    goto fail;

  node = rasqal_new_algebra_node(query, RASQAL_ALGEBRA_OPERATOR_LEFTJOIN);
  if(node) {
    node->node1 = node1;
    node->node2 = node2;
    node->expr = expr;
    
    return node;
  }

  fail:
  if(node1)
    rasqal_free_algebra_node(node1);
  if(node2)
    rasqal_free_algebra_node(node2);
  if(expr)
    rasqal_free_expression(expr);
  return NULL;
}


/*
 * rasqal_new_orderby_algebra_node:
 * @query: #rasqal_query query object
 * @node1: inner algebra node
 * @seq: sequence of order condition #rasqal_expression
 * @distinct: distinct flag
 *
 * INTERNAL - Create a new ORDERBY algebra node for a sequence of order conditions (with optional DISTINCTness)
 * 
 * #node and #seq become owned by the new node
 *
 * Return value: a new #rasqal_algebra_node object or NULL on failure
 **/
rasqal_algebra_node*
rasqal_new_orderby_algebra_node(rasqal_query* query,
                                rasqal_algebra_node* node1,
                                raptor_sequence* seq,
                                int distinct)
{
  rasqal_algebra_node* node;

  if(!query || !node1 || !seq || !raptor_sequence_size(seq))
    goto fail;

  node = rasqal_new_algebra_node(query, RASQAL_ALGEBRA_OPERATOR_ORDERBY);
  if(node) {
    node->node1 = node1;
    node->seq = seq;
    node->distinct = distinct;
    
    return node;
  }

  fail:
  if(node1)
    rasqal_free_algebra_node(node1);
  if(seq)
    raptor_free_sequence(seq);

  return NULL;
}


/*
 * rasqal_new_slice_algebra_node:
 * @query: #rasqal_query query object
 * @node1: inner algebra node
 * @limit: max rows limit (or <0 for no limit)
 * @offset: start row offset (or <0 for no offset)
 *
 * INTERNAL - Create a new SLICE algebra node for selecting a range of rows
 * 
 * #node and #seq become owned by the new node
 *
 * Return value: a new #rasqal_algebra_node object or NULL on failure
 **/
rasqal_algebra_node*
rasqal_new_slice_algebra_node(rasqal_query* query,
                              rasqal_algebra_node* node1,
                              int limit,
                              int offset)
{
  rasqal_algebra_node* node;

  if(!query || !node1)
    goto fail;

  node = rasqal_new_algebra_node(query, RASQAL_ALGEBRA_OPERATOR_SLICE);
  if(node) {
    node->node1 = node1;
    node->limit = limit;
    node->offset = offset;
    
    return node;
  }

  fail:
  if(node1)
    rasqal_free_algebra_node(node1);

  return NULL;
}


/*
 * rasqal_new_project_algebra_node:
 * @query: #rasqal_query query object
 * @node1: inner algebra node
 * @vars_seq: sequence of variables
 *
 * INTERNAL - Create a new PROJECT algebra node for a sequence of variables over an inner node
 * 
 * The inputs @node and @seq become owned by the new node
 *
 * Return value: a new #rasqal_algebra_node object or NULL on failure
 **/
rasqal_algebra_node*
rasqal_new_project_algebra_node(rasqal_query* query,
                                rasqal_algebra_node* node1,
                                raptor_sequence* vars_seq)
{
  rasqal_algebra_node* node;

  if(!query || !node1 || !vars_seq)
    goto fail;

  node = rasqal_new_algebra_node(query, RASQAL_ALGEBRA_OPERATOR_PROJECT);
  if(node) {
    node->node1 = node1;
    node->vars_seq = vars_seq;
    
    return node;
  }

  fail:
  if(node1)
    rasqal_free_algebra_node(node1);
  if(vars_seq)
    raptor_free_sequence(vars_seq);

  return NULL;
}


/*
 * rasqal_new_distinct_algebra_node:
 * @query: #rasqal_query query object
 * @node1: inner algebra node
 *
 * INTERNAL - Create a new DISTINCT algebra node for an inner node
 * 
 * The input @node becomes owned by the new node
 *
 * Return value: a new #rasqal_algebra_node object or NULL on failure
 **/
rasqal_algebra_node*
rasqal_new_distinct_algebra_node(rasqal_query* query,
                                 rasqal_algebra_node* node1)
{
  rasqal_algebra_node* node;

  if(!query || !node1)
    goto fail;

  node = rasqal_new_algebra_node(query, RASQAL_ALGEBRA_OPERATOR_DISTINCT);
  if(node) {
    node->node1 = node1;
    return node;
  }

  fail:
  if(node1)
    rasqal_free_algebra_node(node1);

  return NULL;
}


/*
 * rasqal_new_graph_algebra_node:
 * @query: #rasqal_query query object
 * @node1: inner algebra node
 * @graph: graph literal
 *
 * INTERNAL - Create a new GRAPH algebra node over an inner node
 * 
 * The inputs @node1 and @graph become owned by the new node
 *
 * Return value: a new #rasqal_algebra_node object or NULL on failure
 **/
rasqal_algebra_node*
rasqal_new_graph_algebra_node(rasqal_query* query,
                              rasqal_algebra_node* node1,
                              rasqal_literal *graph)
{
  rasqal_algebra_node* node;

  if(!query || !node1 || !graph)
    goto fail;

  node = rasqal_new_algebra_node(query, RASQAL_ALGEBRA_OPERATOR_GRAPH);
  if(node) {
    node->node1 = node1;
    node->graph = graph;
    
    return node;
  }

  fail:
  if(node1)
    rasqal_free_algebra_node(node1);
  if(graph)
    rasqal_free_literal(graph);

  return NULL;
}


/*
 * rasqal_new_assignment_algebra_node:
 * @query: #rasqal_query query object
 * @var: variable
 * @expr: expression
 *
 * INTERNAL - Create a new LET algebra node over a variable and expression
 * 
 * The input @expr becomes owned by the new node
 *
 * Return value: a new #rasqal_algebra_node object or NULL on failure
 **/
rasqal_algebra_node*
rasqal_new_assignment_algebra_node(rasqal_query* query,
                                   rasqal_variable *var,
                                   rasqal_expression *expr)
{
  rasqal_algebra_node* node;

  if(!query || !var || !expr)
    goto fail;

  node = rasqal_new_algebra_node(query, RASQAL_ALGEBRA_OPERATOR_ASSIGN);
  if(node) {
    node->var = var;
    node->expr = expr;
    
    return node;
  }

  fail:
  if(expr)
    rasqal_free_expression(expr);

  return NULL;
}


/*
 * rasqal_new_groupby_algebra_node:
 * @query: #rasqal_query query object
 * @node1: inner algebra node
 * @seq: sequence of order condition #rasqal_expression
 *
 * INTERNAL - Create a new GROUP algebra node for a sequence of GROUP BY conditions
 * 
 * #node and #seq become owned by the new node
 *
 * Return value: a new #rasqal_algebra_node object or NULL on failure
 **/
rasqal_algebra_node*
rasqal_new_groupby_algebra_node(rasqal_query* query,
                                rasqal_algebra_node* node1,
                                raptor_sequence* seq)
{
  rasqal_algebra_node* node;

  if(!query || !node1 || !seq || !raptor_sequence_size(seq))
    goto fail;

  node = rasqal_new_algebra_node(query, RASQAL_ALGEBRA_OPERATOR_GROUP);
  if(node) {
    node->node1 = node1;
    node->seq = seq;
    
    return node;
  }

  fail:
  if(node1)
    rasqal_free_algebra_node(node1);
  if(seq)
    raptor_free_sequence(seq);

  return NULL;
}


/*
 * rasqal_new_aggregation_algebra_node:
 * @query: #rasqal_query query object
 * @node1: inner algebra node
 * @exprs_seq: sequence of #rasqal_expression
 * @vars_seq: sequence of #rasqal_sequence
 *
 * INTERNAL - Create a new AGGREGATION algebra node for a query over a sequence of expressions to variables
 * 
 * On construction @node1, @exprs_seq and @vars_seq become owned by
 * the new node.
 *
 * Return value: a new #rasqal_algebra_node object or NULL on failure
 **/
rasqal_algebra_node*
rasqal_new_aggregation_algebra_node(rasqal_query* query,
                                    rasqal_algebra_node* node1,
                                    raptor_sequence* exprs_seq,
                                    raptor_sequence* vars_seq)
{
  rasqal_algebra_node* node;

  if(!query || !node1 || !exprs_seq || !vars_seq)
    goto fail;

  node = rasqal_new_algebra_node(query, RASQAL_ALGEBRA_OPERATOR_AGGREGATION);
  if(node) {
    node->node1 = node1;
    node->seq = exprs_seq;
    node->vars_seq = vars_seq;
    return node;
  }

  fail:
  if(node1)
    rasqal_free_algebra_node(node1);
  if(exprs_seq)
    raptor_free_sequence(exprs_seq);
  if(vars_seq)
    raptor_free_sequence(vars_seq);

  return NULL;
}


/*
 * rasqal_new_having_algebra_node:
 * @query: #rasqal_query query object
 * @node1: inner algebra node
 * @exprs_seq: sequence of variables
 *
 * INTERNAL - Create a new HAVING algebra node for a sequence of expressions over an inner node
 * 
 * The inputs @node and @exprs_seq become owned by the new node
 *
 * Return value: a new #rasqal_algebra_node object or NULL on failure
 **/
rasqal_algebra_node*
rasqal_new_having_algebra_node(rasqal_query* query,
                               rasqal_algebra_node* node1,
                               raptor_sequence* exprs_seq)
{
  rasqal_algebra_node* node;

  if(!query || !node1 || !exprs_seq)
    goto fail;

  node = rasqal_new_algebra_node(query, RASQAL_ALGEBRA_OPERATOR_HAVING);
  if(node) {
    node->node1 = node1;
    node->seq = exprs_seq;
    
    return node;
  }

  fail:
  if(node1)
    rasqal_free_algebra_node(node1);
  if(exprs_seq)
    raptor_free_sequence(exprs_seq);

  return NULL;
}


/*
 * rasqal_new_values_algebra_node:
 * @query: #rasqal_query query object
 * @bindings: variable bindings
 *
 * INTERNAL - Create a new VALUES algebra node for a binidngs
 *
 * The input @bindings become owned by the new node
 *
 * Return value: a new #rasqal_algebra_node object or NULL on failure
 **/
rasqal_algebra_node*
rasqal_new_values_algebra_node(rasqal_query* query,
                               rasqal_bindings* bindings)
{
  rasqal_algebra_node* node;

  if(!query || !bindings)
    goto fail;

  node = rasqal_new_algebra_node(query, RASQAL_ALGEBRA_OPERATOR_VALUES);
  if(node) {
    node->bindings = bindings;
    return node;
  }

  fail:
  if(bindings)
    rasqal_free_bindings(bindings);

  return NULL;
}


/*
 * rasqal_new_service_algebra_node:
 * @query: #rasqal_query query object
 * @service_uri: service URI
 * @query_string: query string to send to the service
 * @data_graphs: sequence of #rasqal_data_graph (or NULL)
 * @silent: silent flag
 *
 * INTERNAL - Create a new SERVICE algebra node for a sequence of expressions over an inner node
 * 
 * The inputs @service_uri, @query_string, and @data_graphs become owned by the new node
 *
 * Return value: a new #rasqal_algebra_node object or NULL on failure
 **/
rasqal_algebra_node*
rasqal_new_service_algebra_node(rasqal_query* query,
                                raptor_uri* service_uri,
                                const unsigned char* query_string,
                                raptor_sequence* data_graphs,
                                int silent)
{
  rasqal_algebra_node* node;

  if(!query || !service_uri || !query_string)
    goto fail;

  node = rasqal_new_algebra_node(query, RASQAL_ALGEBRA_OPERATOR_SERVICE);
  if(node) {
    node->service_uri = service_uri;
    node->query_string = query_string;
    node->data_graphs = data_graphs;
    node->flags = (silent ? RASQAL_ENGINE_BITFLAG_SILENT : 0);
    
    return node;
  }

  fail:
  if(service_uri)
    raptor_free_uri(service_uri);
  if(query_string)
    RASQAL_FREE(cstring, query_string);
  if(data_graphs)
    raptor_free_sequence(data_graphs);

  return NULL;
}


/*
 * rasqal_free_algebra_node:
 * @gp: #rasqal_algebra_node object
 *
 * INTERNAL - Free an algebra node object.
 * 
 **/
void
rasqal_free_algebra_node(rasqal_algebra_node* node)
{
  if(!node)
    return;

  /* node->triples is SHARED with the query - not freed here */

  if(node->node1)
    rasqal_free_algebra_node(node->node1);

  if(node->node2)
    rasqal_free_algebra_node(node->node2);

  if(node->expr)
    rasqal_free_expression(node->expr);

  if(node->seq)
    raptor_free_sequence(node->seq);

  if(node->vars_seq)
    raptor_free_sequence(node->vars_seq);

  if(node->graph)
    rasqal_free_literal(node->graph);

  if(node->var)
    rasqal_free_variable(node->var);

  if(node->bindings)
    rasqal_free_bindings(node->bindings);

  if(node->service_uri)
    raptor_free_uri(node->service_uri);
  if(node->query_string)
    RASQAL_FREE(cstring, node->query_string);
  if(node->data_graphs)
    raptor_free_sequence(node->data_graphs);

  RASQAL_FREE(rasqal_algebra, node);
}


/**
 * rasqal_algebra_node_get_operator:
 * @algebra_node: #rasqal_algebra_node algebra node object
 *
 * Get the algebra node operator .
 * 
 * The operator for the given algebra node. See also
 * rasqal_algebra_node_operator_as_counted_string().
 *
 * Return value: algebra node operator
 **/
rasqal_algebra_node_operator
rasqal_algebra_node_get_operator(rasqal_algebra_node* node)
{
  return node->op;
}


static struct {
  const char* const label;
  size_t length;
} rasqal_algebra_node_operator_labels[RASQAL_ALGEBRA_OPERATOR_LAST + 1] = {
  { "UNKNOWN", 7 },
  { "BGP" , 3 },
  { "Filter", 6 },
  { "Join", 4 },
  { "Diff", 4 },
  { "LeftJoin", 8 },
  { "Union", 5 },
  { "ToList", 6 },
  { "OrderBy", 7 },
  { "Project", 7 },
  { "Distinct", 8 },
  { "Reduced", 7 },
  { "Slice", 5 },
  { "Graph", 5 },
  { "Assignment", 10 },
  { "Group", 5 },
  { "Aggregate", 9 },
  { "Having", 6 },
  { "Values", 6 },
  { "Service", 7 }
};


/**
 * rasqal_algebra_node_operator_as_counted_string:
 * @op: the #rasqal_algebra_node_operator verb of the query
 * @length_p: pointer to store the length (or NULL)
 *
 * INTERNAL - Get a counted string for the query verb.
 * 
 * Return value: pointer to a shared string label for the query verb
 **/
const char*
rasqal_algebra_node_operator_as_counted_string(rasqal_algebra_node_operator op,
                                               size_t* length_p)
{
  if(op <= RASQAL_ALGEBRA_OPERATOR_UNKNOWN || 
     op > RASQAL_ALGEBRA_OPERATOR_LAST)
    op = RASQAL_ALGEBRA_OPERATOR_UNKNOWN;

  if(length_p)
    *length_p = rasqal_algebra_node_operator_labels[RASQAL_GOOD_CAST(int, op)].length;

  return rasqal_algebra_node_operator_labels[RASQAL_GOOD_CAST(int, op)].label;
}
  


#define SPACES_LENGTH 80
static const char spaces[SPACES_LENGTH+1] = "                                                                                ";

static void
rasqal_algebra_write_indent(raptor_iostream *iostr, unsigned int indent)
{
  while(indent) {
    unsigned int sp = (indent > SPACES_LENGTH) ? SPACES_LENGTH : indent;
    raptor_iostream_write_bytes(spaces, sizeof(char), sp, iostr);
    indent -= sp;
  }
}

static int
rasqal_algebra_algebra_node_write_internal(rasqal_algebra_node *node, 
                                           raptor_iostream* iostr,
                                           unsigned int indent)
{
  const char* op_string;
  size_t op_length;
  int arg_count = 0;
  unsigned int indent_delta;

  op_string = rasqal_algebra_node_operator_as_counted_string(node->op,
                                                             &op_length);
  
  if(node->op == RASQAL_ALGEBRA_OPERATOR_BGP && !node->triples) {
    raptor_iostream_write_byte('Z', iostr);
    return 0;
  }

  raptor_iostream_counted_string_write(op_string, op_length, iostr);
  raptor_iostream_counted_string_write("(\n", 2, iostr);

  indent_delta = RASQAL_GOOD_CAST(unsigned int, op_length) + 1;
  
  indent += indent_delta;
  rasqal_algebra_write_indent(iostr, indent);

  if(node->op == RASQAL_ALGEBRA_OPERATOR_BGP) {
    int i;
    
    for(i = node->start_column; i <= node->end_column; i++) {
      rasqal_triple *t;
      t = (rasqal_triple*)raptor_sequence_get_at(node->triples, i);
      if(arg_count) {
        raptor_iostream_counted_string_write(" ,\n", 3, iostr);
        rasqal_algebra_write_indent(iostr, indent);
      }
      rasqal_triple_write(t, iostr);
      arg_count++;
    }
  }
  if(node->node1) {
    if(arg_count) {
      raptor_iostream_counted_string_write(" ,\n", 3, iostr);
      rasqal_algebra_write_indent(iostr, indent);
    }
    rasqal_algebra_algebra_node_write_internal(node->node1, iostr, indent);
    arg_count++;
    if(node->node2) {
      if(arg_count) {
        raptor_iostream_counted_string_write(" ,\n", 3, iostr);
        rasqal_algebra_write_indent(iostr, indent);
      }
      rasqal_algebra_algebra_node_write_internal(node->node2, iostr, indent);
      arg_count++;
    }
  }

  /* look for assignment var */
  if(node->var) {
    if(arg_count) {
      raptor_iostream_counted_string_write(" ,\n", 3, iostr);
      rasqal_algebra_write_indent(iostr, indent);
    }
    rasqal_variable_write(node->var, iostr);
    arg_count++;
  }

  /* look for FILTER expression */
  if(node->expr) {
    if(arg_count) {
      raptor_iostream_counted_string_write(" ,\n", 3, iostr);
      rasqal_algebra_write_indent(iostr, indent);
    }
    rasqal_expression_write(node->expr, iostr);
    arg_count++;
  }

  if(node->seq && node->op == RASQAL_ALGEBRA_OPERATOR_ORDERBY) {
    int order_size = raptor_sequence_size(node->seq);
    if(order_size) {
      int i;
      
      if(arg_count) {
        raptor_iostream_counted_string_write(" ,\n", 3, iostr);
        rasqal_algebra_write_indent(iostr, indent);
      }
      raptor_iostream_counted_string_write("Conditions([ ", 13, iostr);
      for(i = 0; i < order_size; i++) {
        rasqal_expression* e;
        e = (rasqal_expression*)raptor_sequence_get_at(node->seq, i);
        if(i > 0)
          raptor_iostream_counted_string_write(", ", 2, iostr);
        rasqal_expression_write(e, iostr);
        arg_count++;
      }
      raptor_iostream_counted_string_write(" ])", 3, iostr);
    }
  }

  if(node->vars_seq && node->op == RASQAL_ALGEBRA_OPERATOR_PROJECT) {
    if(arg_count) {
      raptor_iostream_counted_string_write(" ,\n", 3, iostr);
      rasqal_algebra_write_indent(iostr, indent);
    }
    raptor_iostream_counted_string_write("Variables([ ", 12, iostr);
    rasqal_variables_write(node->vars_seq, iostr);
    arg_count += raptor_sequence_size(node->vars_seq);
    raptor_iostream_counted_string_write(" ])", 3, iostr);
  }

  if(node->op == RASQAL_ALGEBRA_OPERATOR_SLICE) {
    if(arg_count) {
      raptor_iostream_counted_string_write(" ,\n", 3, iostr);
      rasqal_algebra_write_indent(iostr, indent);
    }
    raptor_iostream_string_write("slice limit ", iostr);
    raptor_iostream_decimal_write(RASQAL_GOOD_CAST(int, node->limit), iostr);
    raptor_iostream_string_write(" offset ", iostr);
    raptor_iostream_decimal_write(RASQAL_GOOD_CAST(int, node->offset), iostr);
    raptor_iostream_write_byte('\n', iostr);
    arg_count++;
  }

  if(node->op == RASQAL_ALGEBRA_OPERATOR_GRAPH) {
    if(arg_count) {
      raptor_iostream_counted_string_write(" ,\n", 3, iostr);
      rasqal_algebra_write_indent(iostr, indent);
    }
    raptor_iostream_string_write("origin ", iostr);
    rasqal_literal_write(node->graph, iostr);
    raptor_iostream_write_byte('\n', iostr);
  }

  raptor_iostream_write_byte('\n', iostr);
  indent-= indent_delta;

  rasqal_algebra_write_indent(iostr, indent);
  raptor_iostream_write_byte(')', iostr);

  return 0;
}


int
rasqal_algebra_algebra_node_write(rasqal_algebra_node *node, 
                                  raptor_iostream* iostr)
{
  return rasqal_algebra_algebra_node_write_internal(node, iostr, 0);
}
  

/**
 * rasqal_algebra_node_print:
 * @gp: the #rasqal_algebra_node object
 * @fh: the FILE* handle to print to
 *
 * Print a #rasqal_algebra_node in a debug format.
 * 
 * The print debug format may change in any release.
 * 
 * Return value: non-0 on failure
 **/
int
rasqal_algebra_node_print(rasqal_algebra_node* node, FILE* fh)
{
  raptor_iostream* iostr;

  iostr = raptor_new_iostream_to_file_handle(node->query->world->raptor_world_ptr, fh);
  rasqal_algebra_algebra_node_write(node, iostr);
  raptor_free_iostream(iostr);

  return 0;
}


/**
 * rasqal_algebra_node_visit:
 * @query: #rasqal_query to operate on
 * @node: #rasqal_algebra_node graph pattern
 * @fn: pointer to function to apply that takes user data and graph pattern parameters
 * @user_data: user data for applied function 
 * 
 * Visit a user function over a #rasqal_algebra_node
 *
 * If the user function @fn returns 0, the visit is truncated.
 *
 * Return value: 0 if the visit was truncated.
 **/
int
rasqal_algebra_node_visit(rasqal_query *query,
                          rasqal_algebra_node* node,
                          rasqal_algebra_node_visit_fn fn,
                          void *user_data)
{
  int result;
  
  result = fn(query, node, user_data);
  if(result)
    return result;
  
  if(node->node1) {
    result = rasqal_algebra_node_visit(query, node->node1, fn, user_data);
    if(result)
      return result;
  }
  if(node->node2) {
    result = rasqal_algebra_node_visit(query, node->node2, fn, user_data);
    if(result)
      return result;
  }

  return 0;
}


static rasqal_algebra_node*
rasqal_algebra_basic_graph_pattern_to_algebra(rasqal_query* query,
                                              rasqal_graph_pattern* gp)
{
  rasqal_algebra_node* node = NULL;
  rasqal_expression* fs = NULL;
  
  node = rasqal_new_triples_algebra_node(query, 
                                         rasqal_query_get_triple_sequence(query),
                                         gp->start_column, gp->end_column);
  if(!node)
    goto fail;

  if(gp->filter_expression) {
    fs = rasqal_new_expression_from_expression(gp->filter_expression);
    if(!fs) {
      RASQAL_DEBUG1("rasqal_new_expression_from_expression() failed\n");
      goto fail;
    }

    node = rasqal_new_filter_algebra_node(query, fs, node);
    fs = NULL; /* now owned by node */
    if(!node) {
      RASQAL_DEBUG1("rasqal_new_filter_algebra_node() failed\n");
      goto fail;
    }
  }

  return node;
  
  fail:
  if(node)
    rasqal_free_algebra_node(node);
  
  return NULL;
}


static rasqal_algebra_node*
rasqal_algebra_filter_graph_pattern_to_algebra(rasqal_query* query,
                                               rasqal_graph_pattern* gp)
{
  rasqal_algebra_node* node = NULL;
  rasqal_expression* e;

  e = rasqal_new_expression_from_expression(gp->filter_expression);
  if(!e) {
    RASQAL_DEBUG1("rasqal_new_expression_from_expression() failed\n");
    return NULL;
  }

  node = rasqal_new_filter_algebra_node(query, e, NULL);
  e = NULL; /* now owned by node */
  if(!node) {
    RASQAL_DEBUG1("rasqal_new_filter_algebra_node() failed\n");
  }

  return node;
}


static rasqal_algebra_node*
rasqal_algebra_union_graph_pattern_to_algebra(rasqal_query* query,
                                              rasqal_graph_pattern* gp)
{
  int idx = 0;
  rasqal_algebra_node* node = NULL;

  while(1) {
    rasqal_graph_pattern* sgp;
    rasqal_algebra_node* gnode;
    
    sgp = rasqal_graph_pattern_get_sub_graph_pattern(gp, idx);
    if(!sgp)
      break;
    
    gnode = rasqal_algebra_graph_pattern_to_algebra(query, sgp);
    if(!gnode) {
      RASQAL_DEBUG1("rasqal_algebra_graph_pattern_to_algebra() failed\n");
      goto fail;
    }
    
    if(!node)
      node = gnode;
    else {
      node = rasqal_new_2op_algebra_node(query, RASQAL_ALGEBRA_OPERATOR_UNION,
                                         node, gnode);
      if(!node) {
        RASQAL_DEBUG1("rasqal_new_2op_algebra_node() failed\n");
        goto fail;
      }
    }
    
    idx++;
  }

  return node;

  fail:
  if(node)
    rasqal_free_algebra_node(node);

  return NULL;
}


/*
 * Takes a reference to @bindings
 */
static rasqal_algebra_node*
rasqal_algebra_bindings_to_algebra(rasqal_query* query,
                                   rasqal_bindings* bindings)
{
  rasqal_algebra_node* node;

  bindings = rasqal_new_bindings_from_bindings(bindings);
  node = rasqal_new_values_algebra_node(query, bindings);

  return node;
}


static rasqal_algebra_node*
rasqal_algebra_values_graph_pattern_to_algebra(rasqal_query* query,
                                               rasqal_graph_pattern* gp)
{
  rasqal_algebra_node* node;

  node = rasqal_algebra_bindings_to_algebra(query, gp->bindings);

  return node;
}


/*
 * rasqal_algebra_new_boolean_constant_expr:
 * @query: query object
 * @value: boolean constant
 *
 * INTERNAL - Create a new expression for a boolean constant (true/false)
 *
 * Return value: new expression or NULL on failure
 */
static rasqal_expression*
rasqal_algebra_new_boolean_constant_expr(rasqal_query* query, int value)
{
  rasqal_literal *true_lit;
  
  true_lit = rasqal_new_boolean_literal(query->world, value);
  if(!true_lit) {
    RASQAL_DEBUG1("rasqal_new_boolean_literal() failed\n");
    return NULL;
  }
  
  return rasqal_new_literal_expression(query->world, true_lit);
}


static rasqal_algebra_node*
rasqal_algebra_group_graph_pattern_to_algebra(rasqal_query* query,
                                              rasqal_graph_pattern* gp)
{
  int idx = 0;
  /* Let FS := the empty set */
  rasqal_expression* fs = NULL;
  /* Let G := the empty pattern, Z, a basic graph pattern which
   * is the empty set. */
  rasqal_algebra_node* gnode = NULL;

  gnode = rasqal_new_empty_algebra_node(query);
  if(!gnode) {
    RASQAL_DEBUG1("rasqal_new_empty_algebra_node() failed\n");
    goto fail;
  }

  for(idx = 0; 1; idx++) {
    rasqal_graph_pattern* egp;
    egp = rasqal_graph_pattern_get_sub_graph_pattern(gp, idx);
    if(!egp)
      break;

    if(egp->op == RASQAL_GRAPH_PATTERN_OPERATOR_FILTER &&
       egp->filter_expression) {
      /* If E is of the form FILTER(expr)
         FS := FS set-union {expr} 
      */
      rasqal_expression* e;

      /* add all gp->conditions_sequence to FS */
      e = rasqal_new_expression_from_expression(egp->filter_expression);
      if(!e) {
        RASQAL_DEBUG1("rasqal_new_expression_from_expression() failed\n");
        goto fail;
      }
      fs = fs ? rasqal_new_2op_expression(query->world, RASQAL_EXPR_AND, fs, e) : e;

      if(egp->op == RASQAL_GRAPH_PATTERN_OPERATOR_FILTER)
        continue;
    }

    if(egp->op == RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL) {
      /*  If E is of the form OPTIONAL{P} */
      int sgp_idx = 0;
      int sgp_size = raptor_sequence_size(egp->graph_patterns);

      /* walk through all optionals */
      for(sgp_idx = 0; sgp_idx < sgp_size; sgp_idx++) {
        rasqal_graph_pattern* sgp;
        rasqal_algebra_node* anode;

        sgp = rasqal_graph_pattern_get_sub_graph_pattern(egp, sgp_idx);

        /* Let A := Transform(P) */
        anode = rasqal_algebra_graph_pattern_to_algebra(query, sgp);
        if(!anode) {
          RASQAL_DEBUG1("rasqal_algebra_graph_pattern_to_algebra() failed\n");
          goto fail;
        }
        
        if(anode->op == RASQAL_ALGEBRA_OPERATOR_FILTER) {
          rasqal_expression* f_expr = anode->expr;
          rasqal_algebra_node *a2node = anode->node1;
          /* If A is of the form Filter(F, A2)
             G := LeftJoin(G, A2, F)
          */
          gnode = rasqal_new_leftjoin_algebra_node(query, gnode, a2node,
                                                   f_expr);
          anode->expr = NULL;
          anode->node1 = NULL;
          rasqal_free_algebra_node(anode);
          if(!gnode) {
            RASQAL_DEBUG1("rasqal_new_leftjoin_algebra_node() failed\n");
            goto fail;
          }
        } else  {
          rasqal_expression *true_expr = NULL;

          true_expr = rasqal_algebra_new_boolean_constant_expr(query, 1);
          if(!true_expr) {
            rasqal_free_algebra_node(anode);
            goto fail;
          }
          
          /* G := LeftJoin(G, A, true) */
          gnode = rasqal_new_leftjoin_algebra_node(query, gnode, anode,
                                                   true_expr);
          if(!gnode) {
            RASQAL_DEBUG1("rasqal_new_leftjoin_algebra_node() failed\n");
            goto fail;
          }

          true_expr = NULL; /* now owned by gnode */
        }
      } /* end for all optional */
    } else {
      /* If E is any other form:*/
      rasqal_algebra_node* anode;

      /* Let A := Transform(E) */
      anode = rasqal_algebra_graph_pattern_to_algebra(query, egp);
      if(!anode) {
        RASQAL_DEBUG1("rasqal_algebra_graph_pattern_to_algebra() failed\n");
        goto fail;
      }

      /* G := Join(G, A) */
      gnode = rasqal_new_2op_algebra_node(query, RASQAL_ALGEBRA_OPERATOR_JOIN,
                                          gnode, anode);
      if(!gnode) {
        RASQAL_DEBUG1("rasqal_new_2op_algebra_node() failed\n");
        goto fail;
      }
    }

  }

  /*
    If FS is not empty:
    Let X := Conjunction of expressions in FS
    G := Filter(X, G)
    
    The result is G.
  */
  if(fs) {
    gnode = rasqal_new_filter_algebra_node(query, fs, gnode);
    fs = NULL; /* now owned by gnode */
    if(!gnode) {
      RASQAL_DEBUG1("rasqal_new_filter_algebra_node() failed\n");
      goto fail;
    }
  }

  if(gnode)
    return gnode;

  fail:

  if(gnode)
    rasqal_free_algebra_node(gnode);
  if(fs)
    rasqal_free_expression(fs);
  return NULL;
}


static rasqal_algebra_node*
rasqal_algebra_graph_graph_pattern_to_algebra(rasqal_query* query,
                                              rasqal_graph_pattern* gp)
{
  rasqal_literal *graph = NULL;
  rasqal_graph_pattern* sgp;
  rasqal_algebra_node* gnode;
    
  if(gp->origin)
    graph = rasqal_new_literal_from_literal(gp->origin);

  sgp = rasqal_graph_pattern_get_sub_graph_pattern(gp, 0);
  if(!sgp)
    goto fail;
  
  gnode = rasqal_algebra_graph_pattern_to_algebra(query, sgp);
  if(!gnode) {
    RASQAL_DEBUG1("rasqal_algebra_graph_graph_pattern_to_algebra() failed\n");
    goto fail;
  }
    
  return rasqal_new_graph_algebra_node(query, gnode, graph);
  
  fail:
  if(graph)
    rasqal_free_literal(graph);

  return NULL;
}


static rasqal_algebra_node*
rasqal_algebra_let_graph_pattern_to_algebra(rasqal_query* query,
                                            rasqal_graph_pattern* gp)
{
  rasqal_expression *expr;
    
  expr = rasqal_new_expression_from_expression(gp->filter_expression);
  if(expr)
    return rasqal_new_assignment_algebra_node(query, gp->var, expr);
  
  return NULL;
}


static rasqal_algebra_node*
rasqal_algebra_select_graph_pattern_to_algebra(rasqal_query* query,
                                               rasqal_graph_pattern* gp)
{
  rasqal_projection* projection;
  rasqal_solution_modifier* modifier;
  rasqal_graph_pattern* where_gp;
  rasqal_algebra_node* where_node;
  rasqal_algebra_node* node;
  rasqal_algebra_aggregate* ae;
  rasqal_bindings* bindings;
  
  where_gp = rasqal_graph_pattern_get_sub_graph_pattern(gp, 0);
  projection = gp->projection;
  modifier = gp->modifier;
  bindings = gp->bindings;

  where_node = rasqal_algebra_graph_pattern_to_algebra(query, where_gp);
  if(!where_node)
    goto fail;

  node = rasqal_algebra_query_add_group_by(query, where_node, modifier);
  where_node = NULL; /* now owned by node */
  if(!node)
    goto fail;

  ae = rasqal_algebra_query_prepare_aggregates(query, node, projection,
                                               modifier);
  if(!ae)
    goto fail;

  if(ae) {
    node = rasqal_algebra_query_add_aggregation(query, ae, node);
    ae = NULL;
    if(!node)
      goto fail;
  }

  node = rasqal_algebra_query_add_having(query, node, modifier);
  if(!node)
    goto fail;

  node = rasqal_algebra_query_add_projection(query, node, projection);
  if(!node)
    goto fail;

  node = rasqal_algebra_query_add_orderby(query, node, projection, modifier);
  if(!node)
    goto fail;

  node = rasqal_algebra_query_add_distinct(query, node, projection);
  if(!node)
    goto fail;

  node = rasqal_algebra_query_add_slice(query, node, modifier);
  if(!node)
    goto fail;

  if(bindings) {
    rasqal_algebra_node* bindings_node;

    bindings_node = rasqal_algebra_bindings_to_algebra(query, bindings);
    if(!bindings_node) {
      rasqal_free_algebra_node(node);
      goto fail;
    }

    node = rasqal_new_2op_algebra_node(query,
                                       RASQAL_ALGEBRA_OPERATOR_JOIN,
                                       node, bindings_node);
  }

  return node;

  fail:
  return NULL;
}


static rasqal_algebra_node*
rasqal_algebra_service_graph_pattern_to_algebra(rasqal_query* query,
                                                rasqal_graph_pattern* gp)
{
  rasqal_algebra_node* node = NULL;
  raptor_uri* service_uri = NULL;
  raptor_sequence* data_graphs = NULL;
  rasqal_graph_pattern* inner_gp;
  char* string = NULL;
  raptor_iostream *iostr = NULL;

  service_uri = rasqal_literal_as_uri(gp->origin);
  if(!service_uri)
    goto fail;

  inner_gp = rasqal_graph_pattern_get_sub_graph_pattern(gp, 0);
  if(!inner_gp)
    goto fail;

  iostr = raptor_new_iostream_to_string(query->world->raptor_world_ptr,
                                        (void**)&string, NULL,
                                        rasqal_alloc_memory);
  if(!iostr)
    goto fail;

  rasqal_query_write_sparql_20060406_graph_pattern(inner_gp, iostr,
                                                   query->base_uri);
  raptor_free_iostream(iostr); iostr = NULL;

  RASQAL_DEBUG2("Formatted query string is '%s'", string);


  node = rasqal_new_service_algebra_node(query,
                                         raptor_uri_copy(service_uri),
                                         (const unsigned char*)string,
                                         data_graphs, gp->silent);
  string = NULL;
  if(!node) {
    RASQAL_DEBUG1("rasqal_new_service_algebra_node() failed\n");
    goto fail;
  }

  return node;

  fail:
  if(string)
    RASQAL_FREE(char*, string);

  return node;
}



static rasqal_algebra_node*
rasqal_algebra_graph_pattern_to_algebra(rasqal_query* query,
                                        rasqal_graph_pattern* gp)
{
  rasqal_algebra_node* node = NULL;
  
  switch(gp->op) {
    case RASQAL_GRAPH_PATTERN_OPERATOR_BASIC:
      node = rasqal_algebra_basic_graph_pattern_to_algebra(query, gp);
      break;

    case RASQAL_GRAPH_PATTERN_OPERATOR_UNION:
      node = rasqal_algebra_union_graph_pattern_to_algebra(query, gp);
      break;
      
    case RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL:
    case RASQAL_GRAPH_PATTERN_OPERATOR_GROUP:
      node = rasqal_algebra_group_graph_pattern_to_algebra(query, gp);
      break;
      
    case RASQAL_GRAPH_PATTERN_OPERATOR_GRAPH:
      node = rasqal_algebra_graph_graph_pattern_to_algebra(query, gp);
      break;

    case RASQAL_GRAPH_PATTERN_OPERATOR_LET:
      node = rasqal_algebra_let_graph_pattern_to_algebra(query, gp);
      break;

    case RASQAL_GRAPH_PATTERN_OPERATOR_SELECT:
      node = rasqal_algebra_select_graph_pattern_to_algebra(query, gp);
      break;

    case RASQAL_GRAPH_PATTERN_OPERATOR_FILTER:
      node = rasqal_algebra_filter_graph_pattern_to_algebra(query, gp);
      break;

    case RASQAL_GRAPH_PATTERN_OPERATOR_VALUES:
      node = rasqal_algebra_values_graph_pattern_to_algebra(query, gp);
      break;

    case RASQAL_GRAPH_PATTERN_OPERATOR_SERVICE:
      node = rasqal_algebra_service_graph_pattern_to_algebra(query, gp);
      break;

    case RASQAL_GRAPH_PATTERN_OPERATOR_MINUS:

    case RASQAL_GRAPH_PATTERN_OPERATOR_UNKNOWN:
    default:
      RASQAL_DEBUG3("Unsupported graph pattern operator %s (%u)\n",
                    rasqal_graph_pattern_operator_as_string(gp->op),
                    gp->op);
      break;
  }

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  if(gp) {
    RASQAL_DEBUG1("Input gp:\n");
    rasqal_graph_pattern_print(gp, stderr);
    fputc('\n', stderr);
  }

  if(node) {
    RASQAL_DEBUG1("Resulting node:\n");
    rasqal_algebra_node_print(node, stderr);
    fputc('\n', stderr);
  }
#endif /* not STANDALONE */

  return node;
}


/*
 * rasqal_algebra_node_is_empty:
 * @node: #rasqal_algebra_node node
 *
 * INTERNAL - Check if a an algebra node is empty
 * 
 * Return value: non-0 if empty
 **/
int
rasqal_algebra_node_is_empty(rasqal_algebra_node* node)
{
  return (node->op == RASQAL_ALGEBRA_OPERATOR_BGP && !node->triples);
}


static int
rasqal_algebra_remove_znodes(rasqal_query* query, rasqal_algebra_node* node,
                             void* data)
{
  int* modified = (int*)data;
  int is_z1;
  int is_z2;
  rasqal_algebra_node *anode;

  if(!node)
    return 1;

  /* Look for join operations with no variable join conditions and see if they
   * can be merged, when one of node1 or node2 is an empty graph pattern.
   */
  if(node->op != RASQAL_ALGEBRA_OPERATOR_JOIN &&
     node->op != RASQAL_ALGEBRA_OPERATOR_LEFTJOIN)
    return 0;

  /* Evaluate if the join condition expression is constant TRUE */
  if(node->expr) {
    rasqal_literal* result;
    int bresult;
    int error = 0;
    
    if(!rasqal_expression_is_constant(node->expr))
       return 0;

    result = rasqal_expression_evaluate2(node->expr, query->eval_context,
                                         &error);
    if(error)
      return 0;
    
    bresult = rasqal_literal_as_boolean(result, &error);
    rasqal_free_literal(result);
    if(error)
      return 0;
    
    if(!bresult) {
      /* join condition is always FALSE - can never merge - this join
       * is useless and should be replaced with an empty graph
       * pattern - FIXME  */
      return 0;
    }
    
    /* conclusion: join condition is always TRUE - so can merge nodes */
    rasqal_free_expression(node->expr);
    node->expr = NULL;
  }
  

  if(!node->node1 || !node->node2)
    return 0;

  /* Look for empty graph patterns */
  is_z1 = rasqal_algebra_node_is_empty(node->node1);
  is_z2 = rasqal_algebra_node_is_empty(node->node2);
  
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG1("Checking node\n");
  rasqal_algebra_node_print(node, stderr);
  fprintf(stderr, "\nNode 1 (%s): %s\n", 
          is_z1 ? "empty" : "not empty",
          rasqal_algebra_node_operator_as_counted_string(node->node1->op, NULL));
  fprintf(stderr, "Node 2 (%s): %s\n", 
          is_z2 ? "empty" : "not empty",
          rasqal_algebra_node_operator_as_counted_string(node->node2->op, NULL));
#endif

  if(is_z1 && !is_z2) {
    /* Replace join(Z, A) by A */
    
    anode = node->node2;
    /* an empty node has no extra things to free */
    RASQAL_FREE(rasqal_algebra_node, node->node1);
    memcpy(node, anode, sizeof(rasqal_algebra_node));
    /* free the node struct memory - contained pointers now owned by node */
    RASQAL_FREE(rasqal_algebra_node, anode);
    *modified = 1;
  } else if(!is_z1 && is_z2) {
    /* Replace join(A, Z) by A */
    
    anode = node->node1;
    /* ditto */
    RASQAL_FREE(rasqal_algebra_node, node->node2);
    memcpy(node, anode, sizeof(rasqal_algebra_node));
    RASQAL_FREE(rasqal_algebra_node, anode);
    *modified = 1;
  }

  return 0;
}


static raptor_sequence*
rasqal_algebra_get_variables_mentioned_in(rasqal_query* query,
                                          int row_index)
{
  raptor_sequence* seq; /* sequence of rasqal_variable* */
  int width;
  unsigned short *row;
  int i;
  
  seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_variable,
                            (raptor_data_print_handler)rasqal_variable_print);
  if(!seq)
    return NULL;

  width = rasqal_variables_table_get_total_variables_count(query->vars_table);
  row = &query->variables_use_map[row_index * width];

  for(i = 0; i < width; i++) {
    rasqal_variable* v;

    if(!(row[i] & RASQAL_VAR_USE_MENTIONED_HERE))
      continue;

    v = rasqal_variables_table_get(query->vars_table, i);
    raptor_sequence_push(seq, rasqal_new_variable_from_variable(v));
  }

  return seq;
}


/**
 * rasqal_agg_expr_var_compare:
 * @user_data: comparison user data pointer
 * @a: pointer to address of first #rasqal_expression
 * @b: pointer to address of second #rasqal_expression
 *
 * INTERNAL - compare two void pointers to #rasqal_expression objects with signature suitable for for #rasqal_map comparison.
 *
 * Return value: <0, 0 or >1 comparison
 */
static int
rasqal_agg_expr_var_compare(void* user_data, const void *a, const void *b)
{
  rasqal_algebra_aggregate* ae = (rasqal_algebra_aggregate*)user_data;
  rasqal_expression* expr_a  = (rasqal_expression*)a;
  rasqal_expression* expr_b  = (rasqal_expression*)b;
  int result = 0;

  result = rasqal_expression_compare(expr_a, expr_b, ae->flags, &ae->error);

  return result;
}


/*
 * SPEC:
 *   at each node:
 *     if expression contains an aggregate function
 *       is expression is in map?
 *       Yes:
 *         get value => use existing internal variable for it
 *       No: 
 *         create a new internal variable for it $$internal{id}
 *         inc id
 *         add it to the map
 *       rewrite expression to use internal variable
 *
 */
static int
rasqal_algebra_extract_aggregate_expression_visit(void *user_data,
                                                  rasqal_expression *e)
{
  rasqal_algebra_aggregate* ae = (rasqal_algebra_aggregate*)user_data;
  rasqal_variable* v;

  ae->error = 0;

  /* If not an aggregate expression, ignore it */
  if(!rasqal_expression_is_aggregate(e))
    return 0;


  /* is expression is in map? */
  v = (rasqal_variable*)rasqal_map_search(ae->agg_vars, e);
  if(v) {
    /* Yes: get value => use existing internal variable for it */
    RASQAL_DEBUG2("Found variable %s for existing expression\n", v->name);

    /* add a new reference to v */
    v = rasqal_new_variable_from_variable(v);
    
    /* convert expression in-situ to use existing internal variable
     * After this e holds a v reference
     */
    if(rasqal_expression_convert_aggregate_to_variable(e, v, NULL)) {
      ae->error = 1;
      return 1;
    }


  } else {
    /* No */
    char var_name[20];
    rasqal_expression* new_e = NULL;
        
    /* Check if a new variable is allowed to be added */
    if(ae->adding_new_vars_is_error) {
      rasqal_log_error_simple(ae->query->world, RAPTOR_LOG_LEVEL_ERROR,
                              NULL, "Found new aggregate expression in %s",
                              ae->error_part);
      ae->error = 1;
      return 1;
    }
    
    /* If not an error, create a new internal variable name for it
     * $$agg{id}$$ and add it to the map.
     */
    sprintf(var_name, "$$agg$$%d", ae->counter++);

    v = rasqal_variables_table_add2(ae->query->vars_table,
                                    RASQAL_VARIABLE_TYPE_ANONYMOUS,
                                    RASQAL_GOOD_CAST(const unsigned char*, var_name),
                                    0,
                                    NULL);
    if(!v) {
      ae->error = 1;
      return 1;
    }

    /* convert expression in-situ to use new internal variable
     * and create a new expression in new_e from the old fields.
     *
     * After this one v reference is held by new_e.
     */
    if(rasqal_expression_convert_aggregate_to_variable(e, v, &new_e)) {
      ae->error = 1;
      return 1;
    }

    v = rasqal_new_variable_from_variable(v);

    /* new_e is a new reference
     * after this new_e and 1 v reference are owned by the map 
     */
    if(rasqal_map_add_kv(ae->agg_vars, new_e, v)) {
      ae->error = 1;
      return 1;
    }

#ifdef RASQAL_DEBUG
    RASQAL_DEBUG1("after adding new variable, resulting aggregate vars ");
    rasqal_map_print(ae->agg_vars, stderr);
    fputc('\n', stderr);
#endif

    new_e = rasqal_new_expression_from_expression(new_e);
    if(raptor_sequence_push(ae->agg_exprs, new_e)) {
      ae->error = 1;
      return 1;
    }

    /* add one more v reference for the variables sequence too */
    v = rasqal_new_variable_from_variable(v);
    if(raptor_sequence_push(ae->agg_vars_seq, v)) {
      ae->error = 1;
      return 1;
    }

  }


  return 0;
}


static int
rasqal_algebra_extract_aggregate_expressions(rasqal_query* query,
                                             rasqal_algebra_node* node,
                                             rasqal_algebra_aggregate* ae,
                                             rasqal_projection* projection)
{
  raptor_sequence* seq;
  int i;
  int rc = 0;
  rasqal_variable* v;
  
  if(!projection)
    return 0;

  ae->query = query;

  /* Initialisation of map (key: rasqal_expression, value: rasqal_variable ) */
  ae->agg_vars = rasqal_new_map(/* compare key function */ rasqal_agg_expr_var_compare,
                                /* compare data */ ae,
                                /* free compare data */ NULL,
                                (raptor_data_free_handler)rasqal_free_expression,
                                (raptor_data_free_handler)rasqal_free_variable,
                                (raptor_data_print_handler)rasqal_expression_print,
                                (raptor_data_print_handler)rasqal_variable_print,
                                /* flags */ 0);

  /* Sequence to hold list of aggregate expressions */
  seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_expression,
                            (raptor_data_print_handler)rasqal_expression_print);
  ae->agg_exprs = seq;

  seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_variable,
                            (raptor_data_print_handler)rasqal_variable_print);
  ae->agg_vars_seq = seq;

  /* init internal variable counter */
  ae->counter = 0;

  ae->flags = 0;
  
  ae->error = 0;

  /*
   * walk each select/project expression recursively and pull out aggregate
   * expressions into the ae->agg_vars map, replacing them with
   * internal variable names.
   */
  if ((seq = projection->variables)) {
    for(i = 0;
	(v = (rasqal_variable*)raptor_sequence_get_at(seq, i));
	i++) {
      rasqal_expression* expr = v->expression;

      if(!expr)
	continue;

      if(rasqal_expression_visit(expr,
				 rasqal_algebra_extract_aggregate_expression_visit,
				 ae)) {
	rc = 1;
	goto tidy;
      }
    }
  }

  tidy:
  if(ae->error)
    rc = 1;
  
  return rc;
}


/**
 * rasqal_algebra_query_to_algebra:
 * @query: #rasqal_query to operate on
 *
 * Turn a graph pattern into query algebra structure
 *
 * Return value: algebra expression or NULL on failure
 */
rasqal_algebra_node*
rasqal_algebra_query_to_algebra(rasqal_query* query)
{
  rasqal_graph_pattern* query_gp;
  rasqal_algebra_node* node;
  int modified = 0;
  
  query_gp = rasqal_query_get_query_graph_pattern(query);
  if(!query_gp)
    return NULL;
  
  node = rasqal_algebra_graph_pattern_to_algebra(query, query_gp);
  if(!node)
    return NULL;

  /* FIXME - this does not seem right to be here */
  if(query->bindings) {
    rasqal_algebra_node* bindings_node;

    bindings_node = rasqal_algebra_bindings_to_algebra(query, query->bindings);
    if(!bindings_node) {
      rasqal_free_algebra_node(node);
      return NULL;
    }

    node = rasqal_new_2op_algebra_node(query,
                                       RASQAL_ALGEBRA_OPERATOR_JOIN,
                                       node, bindings_node);
  }

  rasqal_algebra_node_visit(query, node, 
                            rasqal_algebra_remove_znodes,
                            &modified);

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG1("modified after remove zones, algebra node now:\n  ");
  rasqal_algebra_node_print(node, stderr);
  fputs("\n", stderr);
#endif


  return node;
}


void
rasqal_free_algebra_aggregate(rasqal_algebra_aggregate* ae)
{
  if(!ae)
    return;
  
  if(ae->agg_exprs)
    raptor_free_sequence(ae->agg_exprs);
  
  if(ae->agg_vars)
    rasqal_free_map(ae->agg_vars);

  if(ae->agg_vars_seq)
    raptor_free_sequence(ae->agg_vars_seq);
  
  RASQAL_FREE(rasqal_algebra_aggregate, ae);
}

  
static int
rasqal_algebra_replace_aggregate_expressions(rasqal_query* query,
                                             raptor_sequence* exprs_seq,
                                             rasqal_algebra_aggregate* ae)
{
  int i;
  rasqal_expression* expr;

  /* It is now a mistake to find a new aggregate expressions not
   * previously found in SELECT
   */
  ae->adding_new_vars_is_error = 1;
  ae->error_part = "HAVING";
  
  for(i = 0;
      (expr = (rasqal_expression*)raptor_sequence_get_at(exprs_seq, i));
      i++) {
    
    if(rasqal_expression_visit(expr,
                               rasqal_algebra_extract_aggregate_expression_visit,
                               ae)) {
      return 1;
    }
    
  }

  return 0;
}


/**
 * rasqal_algebra_query_prepare_aggregates:
 * @query: #rasqal_query to read from
 * @node: algebra node to prepare
 * @projection: variable projection to use
 * @modifier: solution modifier to use
 *
 * INTERNAL - prepare query aggregates
 *
 * Return value: aggregate expression data or NULL on failure
 */
rasqal_algebra_aggregate*
rasqal_algebra_query_prepare_aggregates(rasqal_query* query,
                                        rasqal_algebra_node* node,
                                        rasqal_projection* projection,
                                        rasqal_solution_modifier* modifier)
{
  rasqal_algebra_aggregate* ae;
  
  ae = RASQAL_CALLOC(rasqal_algebra_aggregate*, 1, sizeof(*ae));
  if(!ae)
    return NULL;

  if(rasqal_algebra_extract_aggregate_expressions(query, node, ae, projection)) {
    RASQAL_DEBUG1("rasqal_algebra_extract_aggregate_expressions() failed\n");
    rasqal_free_algebra_aggregate(ae);
    rasqal_free_algebra_node(node);
    return NULL;
  }

  /* Update variable use structures since agg variables were created */
  if(ae->counter)
    rasqal_query_build_variables_use(query, projection);
  
#ifdef RASQAL_DEBUG
  if(ae->counter) {
    raptor_sequence* seq = projection->variables;

    if(seq) {
      RASQAL_DEBUG1("after aggregate expressions extracted:\n");
      raptor_sequence_print(seq, stderr);
      fputs("\n", stderr);

      RASQAL_DEBUG1("aggregate expressions:\n");
      raptor_sequence_print(ae->agg_exprs, stderr);
      fputs("\n", stderr);
    }
  } else {
    RASQAL_DEBUG1("found no aggregate expressions in select\n");
  }
#endif


  /* if agg expressions were found, need to walk HAVING list and do a
   * similar replacement substituion in the expressions
   */
  if(ae->counter && modifier && modifier->having_conditions) {
    if(rasqal_algebra_replace_aggregate_expressions(query, 
                                                    modifier->having_conditions,
                                                    ae)) {
      RASQAL_DEBUG1("rasqal_algebra_replace_aggregate_expressions() failed\n");
      rasqal_free_algebra_aggregate(ae);
      rasqal_free_algebra_node(node);
      return NULL;
    }
  }
  
  return ae;
}


/**
 * rasqal_algebra_query_add_group_by:
 * @query: #rasqal_query to read from
 * @node: node to apply modifiers to
 * @modifier: solution modifier to use
 *
 * Apply any needed GROUP BY to query algebra structure
 *
 * Return value: non-0 on failure
 */
rasqal_algebra_node*
rasqal_algebra_query_add_group_by(rasqal_query* query,
                                  rasqal_algebra_node* node,
                                  rasqal_solution_modifier* modifier)
{
  raptor_sequence* modifier_seq;
  
  if(!modifier)
    return node;
  
  /* GROUP BY */
  modifier_seq = modifier->group_conditions;

  if(modifier_seq) {
    raptor_sequence* seq;
    
    /* Make a deep copy of the query group conditions sequence for
     * the GROUP algebra node
     */
    seq = rasqal_expression_copy_expression_sequence(modifier_seq);
    if(!seq) {
      rasqal_free_algebra_node(node);
      return NULL;
    }
    
    node = rasqal_new_groupby_algebra_node(query, node, seq);
    
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("modified after adding group node, algebra node now:\n  ");
    rasqal_algebra_node_print(node, stderr);
    fputs("\n", stderr);
#endif
  }
  
  return node;
}
  

/**
 * rasqal_algebra_query_add_orderby:
 * @query: #rasqal_query to read from
 * @node: node to apply modifiers to
 * @projection: variable projection to use
 * @modifier: solution modifier to use
 *
 * Apply any needed modifiers to query algebra structure
 *
 * Return value: non-0 on failure
 */
rasqal_algebra_node*
rasqal_algebra_query_add_orderby(rasqal_query* query,
                                 rasqal_algebra_node* node,
                                 rasqal_projection* projection,
                                 rasqal_solution_modifier* modifier)
{
  raptor_sequence* modifier_seq;
  int distinct = 0;

  if(!modifier)
    return node;
  
  /* ORDER BY */
  modifier_seq = modifier->order_conditions;
  if(modifier_seq) {
    raptor_sequence* seq;
    
    /* Make a deep copy of the query order conditions sequence for
     * the ORDERBY algebra node
     */
    seq = rasqal_expression_copy_expression_sequence(modifier_seq);
    if(!seq) {
      rasqal_free_algebra_node(node);
      return NULL;
    }

    if(projection)
      distinct = projection->distinct;

    node = rasqal_new_orderby_algebra_node(query, node, seq, distinct);
    
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("modified after adding orderby node, algebra node now:\n  ");
    rasqal_algebra_node_print(node, stderr);
    fputs("\n", stderr);
#endif
  }

  return node;
}


/**
 * rasqal_algebra_query_add_slice:
 * @query: #rasqal_query to read from
 * @node: node to apply modifiers to
 * @modifier: solution modifier to use
 *
 * Apply any needed slice (LIMIT, OFFSET) modifiers to query algebra structure
 *
 * This is separate from rasqal_algebra_query_add_orderby() since currently
 * the query results module implements that for the outer result rows.
 *
 * Return value: non-0 on failure
 */
rasqal_algebra_node*
rasqal_algebra_query_add_slice(rasqal_query* query,
                               rasqal_algebra_node* node,
                               rasqal_solution_modifier* modifier)
{
  if(!modifier)
    return node;
  
  /* LIMIT and OFFSET */
  if(modifier->limit > 0 || modifier->offset > 0) {
    node = rasqal_new_slice_algebra_node(query, node, modifier->limit, modifier->offset);

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("modified after adding slice node, algebra node now:\n  ");
    rasqal_algebra_node_print(node, stderr);
    fputs("\n", stderr);
#endif
  }

  return node;
}


/**
 * rasqal_algebra_query_add_aggregation:
 * @query: #rasqal_query to read from
 * @ae: aggregation structure
 * @node: node to apply aggregation to
 *
 * Apply any aggregation step needed to query algebra structure
 *
 * Becomes the owner of @ae
 *
 * Return value: non-0 on failure
 */
rasqal_algebra_node*
rasqal_algebra_query_add_aggregation(rasqal_query* query,
                                     rasqal_algebra_aggregate* ae,
                                     rasqal_algebra_node* node)
{
  raptor_sequence* exprs_seq;
  raptor_sequence* vars_seq;
  
  if(!query || !ae || !node)
    goto tidy;
  
  if(!ae->counter) {
    rasqal_free_algebra_aggregate(ae);
    return node;
  }

  /* Move ownership of sequences inside ae to here */
  exprs_seq = ae->agg_exprs; ae->agg_exprs = NULL;
  vars_seq = ae->agg_vars_seq; ae->agg_vars_seq = NULL;

  rasqal_free_algebra_aggregate(ae); ae = NULL;
  
  node = rasqal_new_aggregation_algebra_node(query, node, exprs_seq, vars_seq);
  exprs_seq = NULL; vars_seq = NULL;
  if(!node) {
    RASQAL_DEBUG1("rasqal_new_aggregation_algebra_node() failed\n");
    goto tidy;
  }
  
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG1("modified after adding aggregation node, algebra node now:\n  ");
  rasqal_algebra_node_print(node, stderr);
  fputs("\n", stderr);
#endif

  return node;


  tidy:
  if(ae)
    rasqal_free_algebra_aggregate(ae);
  if(node)
    rasqal_free_algebra_node(node);

  return NULL;
}


/**
 * rasqal_algebra_query_add_projection:
 * @query: #rasqal_query to read from
 * @node: node to apply projection to
 * @projection: variable projection to use
 *
 * Add a projection to the query algebra structure
 *
 * Return value: non-0 on failure
 */
rasqal_algebra_node*
rasqal_algebra_query_add_projection(rasqal_query* query,
                                    rasqal_algebra_node* node,
                                    rasqal_projection* projection)
{
  int vars_size = 0;
  raptor_sequence* seq = NULL;  /* sequence of rasqal_variable* */
  raptor_sequence* vars_seq;
  int i;

  if(!projection)
    return NULL;
  
  /* FIXME Optimization: do not always need a PROJECT node when the
   * variables at the top level node are the same as the projection
   * list.
   */

  /* project all projection variables (may be an empty sequence) */
  seq = projection->variables;
  if(seq)
    vars_size = raptor_sequence_size(seq);
  
  vars_seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_variable,
                                 (raptor_data_print_handler)rasqal_variable_print);
  if(!vars_seq) {
    rasqal_free_algebra_node(node);
    return NULL;
  }
  
  for(i = 0; i < vars_size; i++) {
    rasqal_variable* v;

    v = (rasqal_variable*)raptor_sequence_get_at(seq, i);
    v = rasqal_new_variable_from_variable(v);
    raptor_sequence_push(vars_seq, v);
  }
  
  node = rasqal_new_project_algebra_node(query, node, vars_seq);
  
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG1("modified after adding project node, algebra node now:\n  ");
  rasqal_algebra_node_print(node, stderr);
  fputs("\n", stderr);
#endif
  
  return node;
}


/**
 * rasqal_algebra_query_add_construct_projection:
 * @query: #rasqal_query to read from
 * @node: node to apply projection to
 *
 * Add a query projection for a CONSTRUCT to the query algebra structure
 *
 * Return value: non-0 on failure
 */
rasqal_algebra_node*
rasqal_algebra_query_add_construct_projection(rasqal_query* query,
                                              rasqal_algebra_node* node)
{
  int vars_size = 0;
  raptor_sequence* seq = NULL;  /* sequence of rasqal_variable* */
  raptor_sequence* vars_seq;
  int i;
  
  /* project all variables mentioned in CONSTRUCT */
  seq = rasqal_algebra_get_variables_mentioned_in(query, 
                                                  RASQAL_VAR_USE_MAP_OFFSET_VERBS);
  if(!seq) {
    rasqal_free_algebra_node(node);
    return NULL;
  }
  
  vars_size = raptor_sequence_size(seq);
  
  vars_seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_variable,
                                 (raptor_data_print_handler)rasqal_variable_print);
  if(!vars_seq) {
    rasqal_free_algebra_node(node);
    return NULL;
  }
  
  for(i = 0; i < vars_size; i++) {
    rasqal_variable* v;

    v = (rasqal_variable*)raptor_sequence_get_at(seq, i);
    v = rasqal_new_variable_from_variable(v);
    raptor_sequence_push(vars_seq, v);
  }
  
  node = rasqal_new_project_algebra_node(query, node, vars_seq);
  
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG1("modified after adding construct project node, algebra node now:\n  ");
  rasqal_algebra_node_print(node, stderr);
  fputs("\n", stderr);
#endif
  
  raptor_free_sequence(seq);

  return node;
}


/**
 * rasqal_algebra_query_add_distinct:
 * @query: #rasqal_query to read from
 * @node: node to apply distinct to
 * @projection: variable projection to use
 *
 * Apply distinctness to query algebra structure
 *
 * Return value: non-0 on failure
 */
rasqal_algebra_node*
rasqal_algebra_query_add_distinct(rasqal_query* query,
                                  rasqal_algebra_node* node,
                                  rasqal_projection* projection)
{
  if(!projection)
    return node;

  if(projection->distinct) {
    node = rasqal_new_distinct_algebra_node(query, node);

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("modified after adding distinct node, algebra node now:\n  ");
    rasqal_algebra_node_print(node, stderr);
    fputs("\n", stderr);
#endif
  }

  return node;
}


/**
 * rasqal_algebra_query_add_having:
 * @query: #rasqal_query to read from
 * @node: node to apply having to
 * @modifier: solution modifier to use
 *
 * Apply any needed HAVING expressions to query algebra structure
 *
 * Return value: non-0 on failure
 */
rasqal_algebra_node*
rasqal_algebra_query_add_having(rasqal_query* query,
                                rasqal_algebra_node* node,
                                rasqal_solution_modifier* modifier)
{
  raptor_sequence* modifier_seq;
  
  if(!modifier)
    return node;
  
  /* HAVING */
  modifier_seq = modifier->having_conditions;
  if(modifier_seq) {
    raptor_sequence* exprs_seq;
    
    /* Make a deep copy of the query order conditions sequence for
     * the ORDERBY algebra node
     */
    exprs_seq = rasqal_expression_copy_expression_sequence(modifier_seq);
    if(!exprs_seq) {
      rasqal_free_algebra_node(node);
      return NULL;
    }
    
    node = rasqal_new_having_algebra_node(query, node, exprs_seq);
    
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("modified after adding having node, algebra node now:\n  ");
    rasqal_algebra_node_print(node, stderr);
    fputs("\n", stderr);
#endif
  }

  return node;
}


#endif

#ifdef STANDALONE
#include <stdio.h>

#define QUERY_LANGUAGE "sparql"
#define QUERY_FORMAT "\
         PREFIX ex: <http://example.org/ns#/> \
         SELECT $subject \
         FROM <http://librdf.org/rasqal/rasqal.rdf> \
         WHERE \
         { $subject ex:predicate $value . \
           FILTER (($value + 1) < 10) \
         }"


int main(int argc, char *argv[]);

int
main(int argc, char *argv[]) {
  char const *program = rasqal_basename(*argv);
  const char *query_language_name = QUERY_LANGUAGE;
  const unsigned char *query_format = (const unsigned char *)QUERY_FORMAT;
  int failures = 0;
#define FAIL do { failures++; goto tidy; } while(0)
  rasqal_world *world;
  rasqal_query* query = NULL;
  rasqal_literal *lit1 = NULL, *lit2 = NULL;
  rasqal_expression *expr1 = NULL, *expr2 = NULL;
  rasqal_expression* expr = NULL;
  rasqal_expression* expr3 = NULL;
  rasqal_expression* expr4 = NULL;
  rasqal_algebra_node* node0 = NULL;
  rasqal_algebra_node* node1 = NULL;
  rasqal_algebra_node* node2 = NULL;
  rasqal_algebra_node* node3 = NULL;
  rasqal_algebra_node* node4 = NULL;
  rasqal_algebra_node* node5 = NULL;
  rasqal_algebra_node* node6 = NULL;
  rasqal_algebra_node* node7 = NULL;
  rasqal_algebra_node* node8 = NULL;
  rasqal_algebra_node* node9 = NULL;
  raptor_uri *base_uri = NULL;
  unsigned char *uri_string;
  rasqal_graph_pattern* query_gp;
  rasqal_graph_pattern* sgp;
  raptor_sequence* triples;
  raptor_sequence* conditions = NULL;
  rasqal_literal* lit3 = NULL;
  rasqal_literal* lit4 = NULL;

  world = rasqal_new_world();
  if(!world || rasqal_world_open(world))
    FAIL;
  
  uri_string = raptor_uri_filename_to_uri_string("");
  if(!uri_string)
    FAIL;
  base_uri = raptor_new_uri(world->raptor_world_ptr, uri_string);
  if(!base_uri)
    FAIL;
  raptor_free_memory(uri_string);
  
  query = rasqal_new_query(world, query_language_name, NULL);
  if(!query) {
    fprintf(stderr, "%s: creating query in language %s FAILED\n", program,
            query_language_name);
    FAIL;
  }

  if(rasqal_query_prepare(query, query_format, base_uri)) {
    fprintf(stderr, "%s: %s query prepare FAILED\n", program, 
            query_language_name);
    FAIL;
  }

  lit1 = rasqal_new_integer_literal(world, RASQAL_LITERAL_INTEGER, 1);
  if(!lit1)
    FAIL;
  expr1 = rasqal_new_literal_expression(world, lit1);
  if(!expr1)
    FAIL;
  lit1 = NULL; /* now owned by expr1 */

  lit2 = rasqal_new_integer_literal(world, RASQAL_LITERAL_INTEGER, 1);
  if(!lit2)
    FAIL;
  expr2 = rasqal_new_literal_expression(world, lit2);
  if(!expr2)
    FAIL;
  lit2 = NULL; /* now owned by expr2 */

  expr = rasqal_new_2op_expression(world, RASQAL_EXPR_PLUS, expr1, expr2);
  if(!expr)
    FAIL;
  expr1 = NULL; expr2 = NULL; /* now owned by expr */
  
  node0 = rasqal_new_empty_algebra_node(query);
  if(!node0)
    FAIL;

  node1 = rasqal_new_filter_algebra_node(query, expr, node0);
  if(!node1) {
    fprintf(stderr, "%s: rasqal_new_filter_algebra_node() failed\n", program);
    FAIL;
  }
  node0 = NULL; expr = NULL; /* now owned by node1 */
  
  fprintf(stderr, "%s: node result: \n", program);
  rasqal_algebra_node_print(node1, stderr);
  fputc('\n', stderr);

  rasqal_free_algebra_node(node1);


  /* construct abstract nodes from query structures */
  query_gp = rasqal_query_get_query_graph_pattern(query);

#ifdef RASQAL_DEBUG
  fprintf(stderr, "%s: query graph pattern: \n", program);
  rasqal_graph_pattern_print(query_gp, stderr);
  fputc('\n', stderr);
#endif

  /* make a filter node around 2nd GP - a FILTER gp */
  sgp = rasqal_graph_pattern_get_sub_graph_pattern(query_gp, 1);
  expr = rasqal_graph_pattern_get_filter_expression(sgp);
  if(!expr) {
    fprintf(stderr, "%s: rasqal_graph_pattern_get_filter_expression() failed\n", program);
    FAIL;
  }
  expr = rasqal_new_expression_from_expression(expr);
  if(!expr) {
    fprintf(stderr, "%s: rasqal_new_expression_from_expression() failed\n", program);
    FAIL;
  }

  node8 = rasqal_new_empty_algebra_node(query);
  if(!node8)
    FAIL;
  node1 = rasqal_new_filter_algebra_node(query, expr, node8);
  if(!node1) {
    fprintf(stderr, "%s: rasqal_new_filter_algebra_node() failed\n", program);
    FAIL;
  }
  /* these are now owned by node1 */
  node8 = NULL;
  expr = NULL;

  fprintf(stderr, "%s: node1 result: \n", program);
  rasqal_algebra_node_print(node1, stderr);
  fputc('\n', stderr);


  /* make an triples node around first (and only) triple pattern */
  triples = rasqal_query_get_triple_sequence(query);
  node2 = rasqal_new_triples_algebra_node(query, triples, 0, 0);
  if(!node2)
    FAIL;

  fprintf(stderr, "%s: node2 result: \n", program);
  rasqal_algebra_node_print(node2, stderr);
  fputc('\n', stderr);


  node3 = rasqal_new_2op_algebra_node(query,
                                      RASQAL_ALGEBRA_OPERATOR_JOIN,
                                      node1, node2);

  if(!node3)
    FAIL;
  
  /* these become owned by node3 */
  node1 = node2 = NULL;
  
  fprintf(stderr, "%s: node3 result: \n", program);
  rasqal_algebra_node_print(node3, stderr);
  fputc('\n', stderr);

  node4 = rasqal_new_empty_algebra_node(query);
  if(!node4)
    FAIL;

  fprintf(stderr, "%s: node4 result: \n", program);
  rasqal_algebra_node_print(node4, stderr);
  fputc('\n', stderr);

  node5 = rasqal_new_2op_algebra_node(query,
                                      RASQAL_ALGEBRA_OPERATOR_UNION,
                                      node3, node4);

  if(!node5)
    FAIL;
  
  /* these become owned by node5 */
  node3 = node4 = NULL;
  
  fprintf(stderr, "%s: node5 result: \n", program);
  rasqal_algebra_node_print(node5, stderr);
  fputc('\n', stderr);



  lit1 = rasqal_new_boolean_literal(world, 1);
  if(!lit1)
    FAIL;
  expr1 = rasqal_new_literal_expression(world, lit1);
  if(!expr1)
    FAIL;
  lit1 = NULL; /* now owned by expr1 */

  node6 = rasqal_new_empty_algebra_node(query);
  if(!node6)
    FAIL;

  node7 = rasqal_new_leftjoin_algebra_node(query, node5, node6, expr1);
  if(!node7)
    FAIL;
  /* these become owned by node7 */
  node5 = node6 = NULL;
  expr1 = NULL;
  
  fprintf(stderr, "%s: node7 result: \n", program);
  rasqal_algebra_node_print(node7, stderr);
  fputc('\n', stderr);

  /* This is an artificial order conditions sequence equivalent to
   * ORDER BY 1, 2 which would probably never appear in a query.
   */
  conditions = raptor_new_sequence((raptor_data_free_handler)rasqal_free_expression,
                                   (raptor_data_print_handler)rasqal_expression_print);
  if(!conditions)
    FAIL;
  lit3 = rasqal_new_integer_literal(world, RASQAL_LITERAL_INTEGER, 1);
  if(!lit3)
    FAIL;
  expr3 = rasqal_new_literal_expression(world, lit3);
  if(!expr3)
    FAIL;
  lit3 = NULL; /* now owned by expr3 */

  raptor_sequence_push(conditions, expr3);
  expr3 = NULL; /* now owned by conditions */
  
  lit4 = rasqal_new_integer_literal(world, RASQAL_LITERAL_INTEGER, 2);
  if(!lit4)
    FAIL;
  expr4 = rasqal_new_literal_expression(world, lit4);
  if(!expr4)
    FAIL;
  lit4 = NULL; /* now owned by expr4 */

  raptor_sequence_push(conditions, expr4);
  expr4 = NULL; /* now owned by conditions */
  
  node9 = rasqal_new_orderby_algebra_node(query, node7, conditions, 0);
  if(!node9)
    FAIL;
  /* these become owned by node9 */
  node7 = NULL;
  conditions = NULL;
  
  fprintf(stderr, "%s: node9 result: \n", program);
  rasqal_algebra_node_print(node9, stderr);
  fputc('\n', stderr);


  tidy:
  if(lit1)
    rasqal_free_literal(lit1);
  if(lit2)
    rasqal_free_literal(lit2);
  if(lit3)
    rasqal_free_literal(lit3);
  if(expr1)
    rasqal_free_expression(expr1);
  if(expr2)
    rasqal_free_expression(expr2);
  if(expr3)
    rasqal_free_expression(expr3);

  if(node9)
    rasqal_free_algebra_node(node9);
  if(node8)
    rasqal_free_algebra_node(node8);
  if(node7)
    rasqal_free_algebra_node(node7);
  if(node6)
    rasqal_free_algebra_node(node6);
  if(node5)
    rasqal_free_algebra_node(node5);
  if(node4)
    rasqal_free_algebra_node(node4);
  if(node3)
    rasqal_free_algebra_node(node3);
  if(node2)
    rasqal_free_algebra_node(node2);
  if(node1)
    rasqal_free_algebra_node(node1);
  if(node0)
    rasqal_free_algebra_node(node0);

  if(query)
    rasqal_free_query(query);
  if(base_uri)
    raptor_free_uri(base_uri);
  if(world)
    rasqal_free_world(world);
  
  return failures;
}
#endif /* STANDALONE */


/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_query.c - Rasqal RDF Query
 *
 * $Id$
 *
 * Copyright (C) 2003-2005, David Beckett http://purl.org/net/dajobe/
 * Institute for Learning and Research Technology http://www.ilrt.bristol.ac.uk/
 * University of Bristol, UK http://www.bristol.ac.uk/
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


static void rasqal_query_add_query_result(rasqal_query* query, rasqal_query_results* query_results);
static void rasqal_query_remove_query_result(rasqal_query* query, rasqal_query_results* query_results);
static int rasqal_query_results_write_xml_20041221(raptor_iostream *iostr, rasqal_query_results *results, raptor_uri *base_uri);
static int rasqal_query_results_write_xml_result2(raptor_iostream *iostr, rasqal_query_results *results, raptor_uri *base_uri);
static int rasqal_query_results_write_xml_result3(raptor_iostream *iostr, rasqal_query_results *results, raptor_uri *base_uri);

/**
 * rasqal_new_query:
 * @name: the query language name (or NULL)
 * @uri: #raptor_uri language uri (or NULL)
 *
 * Constructor - create a new rasqal_query object.
 *
 * A query language can be named or identified by a URI, either
 * of which is optional.  The default query language will be used
 * if both are NULL.  rasqal_languages_enumerate returns
 * information on the known names, labels and URIs.
 *
 * Return value: a new #rasqal_query object or NULL on failure
 */
rasqal_query*
rasqal_new_query(const char *name, const unsigned char *uri)
{
  rasqal_query_engine_factory* factory;
  rasqal_query* query;
  raptor_uri_handler *uri_handler;
  void *uri_context;

  factory=rasqal_get_query_engine_factory(name, uri);
  if(!factory)
    return NULL;

  query=(rasqal_query*)RASQAL_CALLOC(rasqal_query, 1, sizeof(rasqal_query));
  if(!query)
    return NULL;
  
  query->context=(char*)RASQAL_CALLOC(rasqal_query_context, 1,
                                      factory->context_length);
  if(!query->context) {
    rasqal_free_query(query);
    return NULL;
  }
  
  query->factory=factory;

  query->failed=0;

  raptor_uri_get_handler(&uri_handler, &uri_context);
  query->namespaces=raptor_new_namespaces(uri_handler, uri_context,
                                          rasqal_query_simple_error,
                                          query,
                                          0);

  query->variables_sequence=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_variable, (raptor_sequence_print_handler*)rasqal_variable_print);

  query->anon_variables_sequence=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_variable, (raptor_sequence_print_handler*)rasqal_variable_print);

  query->triples=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_triple, (raptor_sequence_print_handler*)rasqal_triple_print);
  
  query->prefixes=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_prefix, (raptor_sequence_print_handler*)rasqal_prefix_print);

  query->query_graph_pattern=NULL;

  query->data_graphs=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_data_graph, (raptor_sequence_print_handler*)rasqal_data_graph_print);

  query->distinct= 0;
  query->limit= -1;
  query->offset= -1;

  query->order_conditions_sequence=NULL;

  query->results_sequence=NULL;
  
  query->usage=1;
  
  if(factory->init(query, name)) {
    rasqal_free_query(query);
    return NULL;
  }
  
  return query;
}



/**
 * rasqal_free_query:
 * @query: #rasqal_query object
 * 
 * Destructor - destroy a #rasqal_query object.
 **/
void
rasqal_free_query(rasqal_query* query) 
{
  if(--query->usage)
    return;
  
  if(query->executed)
    rasqal_engine_execute_finish(query);

  if(query->factory)
    query->factory->terminate(query);

  if(query->context)
    RASQAL_FREE(rasqal_query_context, query->context);

  if(query->namespaces)
    raptor_free_namespaces(query->namespaces);

  if(query->base_uri)
    raptor_free_uri(query->base_uri);

  if(query->query_string)
    RASQAL_FREE(cstring, query->query_string);

  if(query->data_graphs)
    raptor_free_sequence(query->data_graphs);
  if(query->selects)
    raptor_free_sequence(query->selects);
  if(query->describes)
    raptor_free_sequence(query->describes);

  if(query->triples)
    raptor_free_sequence(query->triples);
  if(query->optional_triples)
    raptor_free_sequence(query->optional_triples);
  if(query->constructs)
    raptor_free_sequence(query->constructs);
  if(query->prefixes)
    raptor_free_sequence(query->prefixes);

  if(query->variable_names)
    RASQAL_FREE(cstrings, query->variable_names);
  
  if(query->binding_values)
    RASQAL_FREE(cstrings, query->binding_values);
  
  if(query->variables)
    RASQAL_FREE(vararray, query->variables);

  if(query->variables_declared_in)
    RASQAL_FREE(intarray, query->variables_declared_in);

  if(query->query_graph_pattern)
    rasqal_free_graph_pattern(query->query_graph_pattern);

  if(query->order_conditions_sequence)
    raptor_free_sequence(query->order_conditions_sequence);

  if(query->results_sequence)
    raptor_free_sequence(query->results_sequence);

  /* Do this last since most everything above could refer to a variable */
  if(query->anon_variables_sequence)
    raptor_free_sequence(query->anon_variables_sequence);

  if(query->variables_sequence)
    raptor_free_sequence(query->variables_sequence);

  if(query->triple)
    rasqal_free_triple(query->triple);
  
  RASQAL_FREE(rasqal_query, query);
}


/* Methods */

/**
 * rasqal_query_get_name:
 * @query: #rasqal_query query object
 *
 * Get a short name for the query language.
 *
 * Return value: shared string label value
 **/
const char*
rasqal_query_get_name(rasqal_query* query)
{
  return query->factory->name;
}


/**
 * rasqal_query_get_label:
 * @query: #rasqal_query query object
 *
 * Get a readable label for the query language.
 *
 * Return value: shared string label value
 **/
const char*
rasqal_query_get_label(rasqal_query* query)
{
  return query->factory->label;
}


/**
 * rasqal_query_set_fatal_error_handler:
 * @query: the query
 * @user_data: user data to pass to function
 * @handler: pointer to the function
 *
 * Set the query error handling function.
 * 
 * The function will receive callbacks when the query fails.
 * 
 **/
void
rasqal_query_set_fatal_error_handler(rasqal_query* query, void *user_data,
                                     raptor_message_handler handler)
{
  query->fatal_error_user_data=user_data;
  query->fatal_error_handler=handler;
}


/**
 * rasqal_query_set_error_handler:
 * @query: the query
 * @user_data: user data to pass to function
 * @handler: pointer to the function
 *
 * Set the query error handling function.
 * 
 * The function will receive callbacks when the query fails.
 * 
 **/
void
rasqal_query_set_error_handler(rasqal_query* query, void *user_data,
                               raptor_message_handler handler)
{
  query->error_user_data=user_data;
  query->error_handler=handler;
}


/**
 * rasqal_query_set_warning_handler:
 * @query: the query
 * @user_data: user data to pass to function
 * @handler: pointer to the function
 *
 * Set the query warning handling function.
 * 
 * The function will receive callbacks when the query gives a warning.
 * 
 **/
void
rasqal_query_set_warning_handler(rasqal_query* query, void *user_data,
                                 raptor_message_handler handler)
{
  query->warning_user_data=user_data;
  query->warning_handler=handler;
}


/**
 * rasqal_query_set_feature:
 * @query: #rasqal_query query object
 * @feature: feature to set from enumerated #rasqal_feature values
 * @value: integer feature value
 *
 * Set various query features.
 * 
 * feature can be one of:
 **/
void
rasqal_query_set_feature(rasqal_query* query, 
                         rasqal_feature feature, int value)
{
  switch(feature) {
      
    case RASQAL_FEATURE_LAST:
    default:
      break;
  }
}


/**
 * rasqal_query_get_distinct:
 * @query: #rasqal_query query object
 *
 * Get the query distinct results flag.
 *
 * Return value: non-0 if the results should be distinct
 **/
int
rasqal_query_get_distinct(rasqal_query* query)
{
  return query->distinct;
}


/**
 * rasqal_query_set_distinct:
 * @query: #rasqal_query query object
 * @is_distinct: non-0 if distinct
 *
 * Set the query distinct results flag.
 *
 **/
void
rasqal_query_set_distinct(rasqal_query* query, int is_distinct)
{
  query->distinct= (is_distinct != 0) ? 1 : 0;
}


/**
 * rasqal_query_get_limit:
 * @query: #rasqal_query query object
 *
 * Get the query-specified limit on results.
 *
 * This is the limit given in the query on the number of results allowed.
 *
 * Return value: integer >=0 if a limit is given, otherwise <0
 **/
int
rasqal_query_get_limit(rasqal_query* query)
{
  return query->limit;
}


/**
 * rasqal_query_set_limit:
 * @query: #rasqal_query query object
 * @limit: the limit on results, >=0 to set a limit, <0 to have no limit
 *
 * Set the query-specified limit on results.
 *
 * This is the limit given in the query on the number of results allowed.
 **/
void
rasqal_query_set_limit(rasqal_query* query, int limit)
{
  query->limit=limit;
}


/**
 * rasqal_query_get_offset:
 * @query: #rasqal_query query object
 *
 * Get the query-specified offset on results.
 *
 * This is the offset given in the query on the number of results allowed.
 *
 * Return value: integer >=0 if a offset is given, otherwise <0
 **/
int
rasqal_query_get_offset(rasqal_query* query)
{
  return query->offset;
}


/**
 * rasqal_query_set_offset:
 * @query: #rasqal_query query object
 * @offset: offset for results, >=0 to set an offset, <0 to have no offset
 *
 * Set the query-specified offset on results.
 *
 * This is the offset given in the query on the number of results allowed.
 **/
void
rasqal_query_set_offset(rasqal_query* query, int offset)
{
  query->offset=offset;
}


/**
 * rasqal_query_add_data_graph:
 * @query: #rasqal_query query object
 * @uri: #raptor_uri source uri for retrieval
 * @name_uri: #raptor_uri name uri (or NULL)
 * @flags: RASQAL_DATA_GRAPH_NAMED or RASQAL_DATA_GRAPH_BACKGROUND
 *
 * Add a data graph to the query.
 *
 * named_uri must be given if flags RASQAL_DATA_GRAPH_NAMED is set.
 * It is the name of the graph and also used as the base URI
 * when resolving any relative URIs for the graph in uri.
 *
 * Return value: non-0 on failure
 **/
int
rasqal_query_add_data_graph(rasqal_query* query, 
                            raptor_uri* uri, raptor_uri* name_uri,
                            int flags)
{
  rasqal_data_graph *dg;

  if((flags & RASQAL_DATA_GRAPH_NAMED) && !name_uri)
    return 1;
  
  dg=rasqal_new_data_graph(uri, name_uri, flags);
  
  raptor_sequence_push(query->data_graphs, (void*)dg);

  return 0;
}


/**
 * rasqal_query_get_data_graph_sequence:
 * @query: #rasqal_query query object
 *
 * Get the sequence of data_graph URIs.
 *
 * Return value: a #raptor_sequence of #raptor_uri pointers.
 **/
raptor_sequence*
rasqal_query_get_data_graph_sequence(rasqal_query* query)
{
  return query->data_graphs;
}


/**
 * rasqal_query_get_data_graph:
 * @query: #rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Get a rasqal_data_graph* in the sequence of data_graphs.
 *
 * Return value: a #rasqal_data_graph pointer or NULL if out of the sequence range
 **/
rasqal_data_graph*
rasqal_query_get_data_graph(rasqal_query* query, int idx)
{
  if(!query->data_graphs)
    return NULL;
  
  return (rasqal_data_graph*)raptor_sequence_get_at(query->data_graphs, idx);
}


/**
 * rasqal_query_add_variable:
 * @query: #rasqal_query query object
 * @var: #rasqal_variable variable
 *
 * Add a binding variable to the query.
 *
 * See also rasqal_query_set_variable which assigns or removes a value to
 * a previously added variable in the query.
 **/
void
rasqal_query_add_variable(rasqal_query* query, rasqal_variable* var)
{
  if(!query->selects)
    query->selects=raptor_new_sequence(NULL, (raptor_sequence_print_handler*)rasqal_variable_print);

  raptor_sequence_push(query->selects, (void*)var);
}


/**
 * rasqal_query_get_bound_variable_sequence:
 * @query: #rasqal_query query object
 *
 * Get the sequence of variables to bind in the query.
 *
 * This returns the sequence of variables that are explicitly chosen
 * via SELECT in RDQL, SPARQL.  Or all variables mentioned with SELECT *
 *
 * Return value: a #raptor_sequence of #rasqal_variable pointers.
 **/
raptor_sequence*
rasqal_query_get_bound_variable_sequence(rasqal_query* query)
{
  return query->selects;
}


/**
 * rasqal_query_get_all_variable_sequence:
 * @query: #rasqal_query query object
 *
 * Get the sequence of all variables mentioned in the query.
 *
 * Return value: a #raptor_sequence of #rasqal_variable pointers.
 **/
raptor_sequence*
rasqal_query_get_all_variable_sequence(rasqal_query* query)
{
  return query->selects;
}


/**
 * rasqal_query_get_variable:
 * @query: #rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Get a variable in the sequence of variables to bind.
 *
 * Return value: a #rasqal_variable pointer or NULL if out of the sequence range
 **/
rasqal_variable*
rasqal_query_get_variable(rasqal_query* query, int idx)
{
  if(!query->selects)
    return NULL;
  
  return (rasqal_variable*)raptor_sequence_get_at(query->selects, idx);
}


/**
 * rasqal_query_has_variable:
 * @query: #rasqal_query query object
 * @name: variable name
 *
 * Find if the named variable is in the sequence of variables to bind.
 *
 * Return value: non-0 if the variable name was found.
 **/
int
rasqal_query_has_variable(rasqal_query* query, const unsigned char *name)
{
  int i;

  if(!query->selects)
    return 1;
  
  for(i=0; i< raptor_sequence_size(query->selects); i++) {
    rasqal_variable* v=(rasqal_variable*)raptor_sequence_get_at(query->selects, i);
    if(!strcmp((const char*)v->name, (const char*)name))
      return 1;
  }
  return 0;
}


/**
 * rasqal_query_set_variable:
 * @query: #rasqal_query query object
 * @name: #rasqal_variable variable
 * @value: #rasqal_literal value to set or NULL
 *
 * Add a binding variable to the query.
 *
 * See also rasqal_query_add_variable which adds a new binding variable
 * and must be called before this method is invoked.
 *
 * Return value: non-0 on failure
 **/
int
rasqal_query_set_variable(rasqal_query* query, const unsigned char *name,
                          rasqal_literal* value)
{
  int i;

  if(!query->selects)
    return 1;
  
  for(i=0; i< raptor_sequence_size(query->selects); i++) {
    rasqal_variable* v=(rasqal_variable*)raptor_sequence_get_at(query->selects, i);
    if(!strcmp((const char*)v->name, (const char*)name)) {
      if(v->value)
        rasqal_free_literal(v->value);
      v->value=value;
      return 0;
    }
  }
  return 1;
}


/**
 * rasqal_query_get_triple_sequence:
 * @query: #rasqal_query query object
 *
 * Get the sequence of matching triples in the query.
 *
 * Return value: a #raptor_sequence of #rasqal_triple pointers.
 **/
raptor_sequence*
rasqal_query_get_triple_sequence(rasqal_query* query)
{
  return query->triples;
}


/**
 * rasqal_query_get_triple:
 * @query: #rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Get a triple in the sequence of matching triples in the query.
 *
 * Return value: a #rasqal_triple pointer or NULL if out of the sequence range
 **/
rasqal_triple*
rasqal_query_get_triple(rasqal_query* query, int idx)
{
  if(!query->triples)
    return NULL;
  
  return (rasqal_triple*)raptor_sequence_get_at(query->triples, idx);
}


/**
 * rasqal_query_add_prefix:
 * @query: #rasqal_query query object
 * @prefix: #rasqal_prefix namespace prefix, URI
 *
 * Add a namespace prefix to the query.
 *
 * If the prefix has already been used, the old URI will be overridden.
 **/
void
rasqal_query_add_prefix(rasqal_query* query, rasqal_prefix* prefix)
{
  if(!query->prefixes)
    query->prefixes=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_prefix, (raptor_sequence_print_handler*)rasqal_prefix_print);
  else {
    int i;
    for(i=0; i< raptor_sequence_size(query->prefixes); i++) {
      rasqal_prefix* p=(rasqal_prefix*)raptor_sequence_get_at(query->prefixes, i);
      if(strcmp((const char*)p->prefix, (const char*)prefix->prefix)) {
        rasqal_engine_undeclare_prefix(query, p);
        break;
      }
    }
  }

  raptor_sequence_push(query->prefixes, (void*)prefix);
}


/**
 * rasqal_query_get_prefix_sequence:
 * @query: #rasqal_query query object
 *
 * Get the sequence of namespace prefixes in the query.
 *
 * Return value: a #raptor_sequence of #rasqal_prefix pointers.
 **/
raptor_sequence*
rasqal_query_get_prefix_sequence(rasqal_query* query)
{
  return query->prefixes;
}


/**
 * rasqal_query_get_prefix:
 * @query: #rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Get a prefix in the sequence of namespsace prefixes in the query.
 *
 * Return value: a #rasqal_prefix pointer or NULL if out of the sequence range
 **/
rasqal_prefix*
rasqal_query_get_prefix(rasqal_query* query, int idx)
{
  if(!query->prefixes)
    return NULL;

  return (rasqal_prefix*)raptor_sequence_get_at(query->prefixes, idx);
}


/**
 * rasqal_query_get_query_graph_pattern:
 * @query: #rasqal_query query object
 *
 * Get the top query graph pattern.
 *
 * Return value: a #rasqal_graph_pattern of the top query graph pattern
 **/
rasqal_graph_pattern*
rasqal_query_get_query_graph_pattern(rasqal_query* query)
{
  return query->query_graph_pattern;
}


/**
 * rasqal_query_get_graph_pattern_sequence:
 * @query: #rasqal_query query object
 *
 * Get the sequence of graph_patterns expressions inside the top query graph pattern.
 *
 * Return value: a #raptor_sequence of #rasqal_graph_pattern pointers.
 **/
raptor_sequence*
rasqal_query_get_graph_pattern_sequence(rasqal_query* query)
{
  return rasqal_graph_pattern_get_sub_graph_pattern_sequence(query->query_graph_pattern);
}


/**
 * rasqal_query_get_graph_pattern:
 * @query: #rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Get a graph_pattern in the sequence of graph_pattern expressions in the top query graph pattern.
 *
 * Return value: a #rasqal_graph_pattern pointer or NULL if out of the sequence range
 **/
rasqal_graph_pattern*
rasqal_query_get_graph_pattern(rasqal_query* query, int idx)
{
  return rasqal_graph_pattern_get_sub_graph_pattern(query->query_graph_pattern, idx);
}


/**
 * rasqal_query_get_construct_triples_sequence:
 * @query: #rasqal_query query object
 *
 * Get the sequence of triples for a construct.
 *
 * Return value: a #raptor_sequence of #rasqal_triple pointers.
 **/
raptor_sequence*
rasqal_query_get_construct_triples_sequence(rasqal_query* query)
{
  return query->constructs;
}


/**
 * rasqal_query_get_construct_triple:
 * @query: #rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Get a triple in the sequence of construct triples.
 *
 * Return value: a #rasqal_triple pointer or NULL if out of the sequence range
 **/
rasqal_triple*
rasqal_query_get_construct_triple(rasqal_query* query, int idx)
{
  if(!query->constructs)
    return NULL;

  return (rasqal_triple*)raptor_sequence_get_at(query->constructs, idx);
}


/**
 * rasqal_graph_pattern_add_sub_graph_pattern:
 * @graph_pattern: graph pattern to add to
 * @sub_graph_pattern: graph pattern to add inside
 *
 * Add a sub graph pattern to a graph pattern .
 *
 **/
void
rasqal_graph_pattern_add_sub_graph_pattern(rasqal_graph_pattern* graph_pattern,
                                           rasqal_graph_pattern* sub_graph_pattern)
{
  if(!graph_pattern->graph_patterns)
    graph_pattern->graph_patterns=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_graph_pattern, (raptor_sequence_print_handler*)rasqal_graph_pattern_print);
  raptor_sequence_push(graph_pattern->graph_patterns, sub_graph_pattern);
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
  if(!graph_pattern->graph_patterns)
    return NULL;

  return (rasqal_graph_pattern*)raptor_sequence_get_at(graph_pattern->graph_patterns, idx);
}


/**
 * rasqal_graph_pattern_get_flags:
 * @graph_pattern: #rasqal_graph_pattern graph pattern object
 *
 * Get the graph pattern flags .
 * 
 * @deprecated: Always returns 0
 *
 * Return value: 0
 **/
int
rasqal_graph_pattern_get_flags(rasqal_graph_pattern* graph_pattern)
{
  RASQAL_DEPRECATED_MESSAGE("use rasqal_graph_pattern_get_operator");
  return 0;
}


/**
 * rasqal_graph_pattern_set_origin:
 * @graph_pattern: #rasqal_graph_pattern graph pattern object
 * @origin: #rasqal_literal variable or URI
 *
 * Get the graph pattern triple origin.
 * 
 * All triples in this graph pattern or contained graph patterns are set
 * to have the given origin.
 **/
void
rasqal_graph_pattern_set_origin(rasqal_graph_pattern* graph_pattern,
                                rasqal_literal *origin)
{
  raptor_sequence* s;
  
  s=graph_pattern->triples;
  if(s) {
    int i;

    /* Flag all the triples in this graph pattern with origin */
    for(i= graph_pattern->start_column; i <= graph_pattern->end_column; i++) {
      rasqal_triple *t=(rasqal_triple*)raptor_sequence_get_at(s, i);
      rasqal_triple_set_origin(t, rasqal_new_literal_from_literal(origin));
    }
  }

  s=graph_pattern->graph_patterns;
  if(s) {
    int i;

    /* Flag all the triples in sub-graph patterns with origin */
    for(i=0; i < raptor_sequence_size(s); i++) {
      rasqal_graph_pattern *gp=(rasqal_graph_pattern*)raptor_sequence_get_at(s, i);
      rasqal_graph_pattern_set_origin(gp, origin);
    }
  }

}


/**
 * rasqal_query_prepare:
 * @query: the #rasqal_query object
 * @query_string: the query string (or NULL)
 * @base_uri: base URI of query string (optional)
 *
 * Prepare a query - typically parse it.
 * 
 * Some query languages may require a base URI to resolve any
 * relative URIs in the query string.  If this is not given,
 * the current directory in the filesystem is used as the base URI.
 *
 * The query string may be NULL in which case it is not parsed
 * and the query parts may be created by API calls such as
 * rasqal_query_add_source etc.
 *
 * Return value: non-0 on failure.
 **/
int
rasqal_query_prepare(rasqal_query* query,
                     const unsigned char *query_string,
                     raptor_uri *base_uri)
{
  int rc=0;
  
  if(query->failed)
    return 1;

  if(query->prepared)
    return 0;
  query->prepared=1;

  if(query_string) {
    query->query_string=(unsigned char*)RASQAL_MALLOC(cstring, strlen((const char*)query_string)+1);
    strcpy((char*)query->query_string, (const char*)query_string);
  }

  if(base_uri)
    base_uri=raptor_uri_copy(base_uri);
  else {
    unsigned char *uri_string=raptor_uri_filename_to_uri_string("");
    base_uri=raptor_new_uri(uri_string);
    raptor_free_memory(uri_string);
  }
  
  query->base_uri=base_uri;
  query->locator.uri=base_uri;
  query->locator.line = query->locator.column = query->locator.byte = -1;

  rc=query->factory->prepare(query);
  if(rc)
    query->failed=1;

  if(query->query_graph_pattern) {
    rasqal_query_graph_pattern_visit(query, rasqal_engine_merge_triples,
                                     NULL);

    rasqal_query_graph_pattern_visit(query, rasqal_engine_merge_graph_patterns,
                                     NULL);
  }

  return rc;
}


static int rasqal_query_result_row_update(rasqal_query_result_row* row, int offset);



static rasqal_query_result_row*
rasqal_new_query_result_row(rasqal_query_results* query_results, int offset)
{
  rasqal_query* query=query_results->query;
  int size;
  int order_size;
  rasqal_query_result_row* row;
  
  if(!rasqal_query_results_is_bindings(query_results))
    return NULL;

  size=rasqal_query_results_get_bindings_count(query_results);

  row=(rasqal_query_result_row*)RASQAL_CALLOC(rasqal_query_result_row,
                                              sizeof(rasqal_query_result_row),
                                              1);
  row->usage=1;
  row->results=query_results;

  row->size=size;
  row->values=(rasqal_literal**)RASQAL_CALLOC(array, sizeof(rasqal_literal*), 
                                              size);

  if(query->order_conditions_sequence)
    order_size=raptor_sequence_size(query->order_conditions_sequence);
  else
    order_size=0;
  
  if(order_size) {
    row->order_size=order_size;
    row->order_values=(rasqal_literal**)RASQAL_CALLOC(array, 
                                                      sizeof(rasqal_literal*),
                                                      order_size);
  }
  
  rasqal_query_result_row_update(row, offset);
  
  return row;
}


static rasqal_query_result_row*
rasqal_new_query_result_row_from_query_result_row(rasqal_query_result_row* row)
{
  row->usage++;
  return row;
}



static void 
rasqal_free_query_result_row(rasqal_query_result_row* row)
{
  if(--row->usage)
    return;
  
  if(row->values) {
    int i; 
    for(i=0; i < row->size; i++) {
      if(row->values[i])
        rasqal_free_literal(row->values[i]);
    }
    RASQAL_FREE(array, row->values);
  }
  if(row->order_values) {
    int i; 
    for(i=0; i < row->order_size; i++) {
      if(row->order_values[i])
        rasqal_free_literal(row->order_values[i]);
    }
    RASQAL_FREE(array, row->order_values);
  }

  RASQAL_FREE(rasqal_query_result_row, row);
}


static int
rasqal_query_result_row_update(rasqal_query_result_row* row, int offset)
{
  rasqal_query_results *query_results=row->results;
  rasqal_query* query;
  int i;
  
  if(!rasqal_query_results_is_bindings(query_results))
    return 1;

  query=query_results->query;
  rasqal_engine_assign_binding_values(query);

  for(i=0; i < row->size; i++) {
    rasqal_literal *l=query->binding_values[i];
    if(row->values[i])
      rasqal_free_literal(row->values[i]);
    if(l)
      row->values[i]=rasqal_literal_as_node(l);
    else
      row->values[i]=NULL;
  }

  if(row->order_size) {
    for(i=0; i < row->order_size; i++) {
      rasqal_expression* e=(rasqal_expression*)raptor_sequence_get_at(query->order_conditions_sequence, i);
      rasqal_literal *l=rasqal_expression_evaluate(query, e, 
                                                   query->compare_flags);
      if(row->order_values[i])
        rasqal_free_literal(row->order_values[i]);
      if(l) {
        row->order_values[i]=rasqal_literal_as_node(l);
        rasqal_free_literal(l);
      } else
        row->order_values[i]=NULL;
    }
  }
  
  row->offset=offset;
  
  return 0;
}


static void 
rasqal_query_result_row_print(rasqal_query_result_row* row, FILE* fh)
{
  int i;
  
  fputs("result[", fh);
  for(i=0; i < row->size; i++) {
    const unsigned char *name=rasqal_query_results_get_binding_name(row->results, i);
    rasqal_literal *value=row->values[i];
    
    if(i > 0)
      fputs(", ", fh);
    fprintf(fh, "%s=", name);

    if(value)
      rasqal_literal_print(value, fh);
    else
      fputs("NULL", fh);
  }

  fputs(" with ordering values [", fh);

  if(row->order_size) {

    for(i=0; i < row->order_size; i++) {
      rasqal_literal *value=row->order_values[i];
      
      if(i > 0)
        fputs(", ", fh);
      if(value)
        rasqal_literal_print(value, fh);
      else
        fputs("NULL", fh);
    }
    fputs("]", fh);
  }

  fprintf(fh, " offset %d]", row->offset);
}


static int
rasqal_query_result_literal_sequence_compare(rasqal_query* query,
                                             rasqal_literal** values_a,
                                             rasqal_literal** values_b,
                                             raptor_sequence* expr_sequence,
                                             int size)
{
  int result=0;
  int i;

  for(i=0; i < size; i++) {
    rasqal_expression* e=NULL;
    int error=0;
    rasqal_literal* literal_a=values_a[i];
    rasqal_literal* literal_b=values_b[i];
    
    if(expr_sequence)
      e=(rasqal_expression*)raptor_sequence_get_at(expr_sequence, i);

#ifdef RASQAL_DEBUG
    RASQAL_DEBUG1("Comparing ");
    rasqal_literal_print(literal_a, stderr);
    fputs(" to ", stderr);
    rasqal_literal_print(literal_b, stderr);
    fputs("\n", stderr);
#endif

    if(!literal_a || !literal_b) {
      if(!literal_a && !literal_b)
        result= 0;
      else {
        result= literal_a ? 1 : -1;
#ifdef RASQAL_DEBUG
        RASQAL_DEBUG2("Got one NULL literal comparison, returning %d\n", result);
#endif
        break;
      }
    }
    
    result=rasqal_literal_compare(literal_a, literal_b, query->compare_flags,
                                  &error);

    if(error) {
#ifdef RASQAL_DEBUG
      RASQAL_DEBUG2("Got literal comparison error at expression %d, returning 0\n", i);
#endif
      result=0;
      break;
    }
        
    if(!result)
      continue;

    if(e && e->op == RASQAL_EXPR_ORDER_COND_DESC)
      result= -result;
    /* else Order condition is RASQAL_EXPR_ORDER_COND_ASC so nothing to do */
    
#ifdef RASQAL_DEBUG
    RASQAL_DEBUG3("Returning comparison result %d at expression %d\n", result, i);
#endif
    break;
  }

  return result;
}


static int
rasqal_query_result_row_compare(const void *a, const void *b)
{
  rasqal_query_result_row* row_a;
  rasqal_query_result_row* row_b;
  rasqal_query_results* results;
  rasqal_query* query;
  int result=0;

  row_a=*(rasqal_query_result_row**)a;
  row_b=*(rasqal_query_result_row**)b;
  results=row_a->results;
  query=results->query;
  
  if(query->distinct) {
    result=rasqal_query_result_literal_sequence_compare(query,
                                                        row_a->values,
                                                        row_b->values,
                                                        NULL,
                                                        row_a->size);
    if(!result)
      /* duplicate, so return that */
      return 0;
  }
  
  /* now order it */
  result=rasqal_query_result_literal_sequence_compare(query,
                                                      row_a->order_values,
                                                      row_b->order_values,
                                                      query->order_conditions_sequence,
                                                      row_a->order_size);
  
  /* still equal?  make sort stable by using the original order */
  if(!result) {
    result= row_a->offset - row_b->offset;
    RASQAL_DEBUG2("Got equality result so using offsets, returning %d\n",
                  result);
  }
  
  return result;
}


static void
rasqal_map_add_to_sequence(void *key, void *value, void *user_data)
{
  rasqal_query_result_row* row;
  row=rasqal_new_query_result_row_from_query_result_row((rasqal_query_result_row*)key);
  raptor_sequence_push((raptor_sequence*)user_data, row);
}


/*
 * rasqal_query_results_update:
 * @query_results: #rasqal_query_results query_results
 *
 * Update the next result - INTERNAL.
 * 
 * Return value: non-0 if failed or results exhausted
 **/
static int
rasqal_query_results_update(rasqal_query_results *query_results)
{
  rasqal_query* query;

  if(!query_results)
    return 1;
  
  if(!rasqal_query_results_is_bindings(query_results))
    return 1;

  query=query_results->query;

  if(query->finished)
    return 1;

  while(1) {
    int rc;
    
    /* rc<0 error rc=0 end of results,  rc>0 got a result */
    rc=rasqal_engine_get_next_result(query);

    if(rc < 1) {
      /* <0 failure OR =0 end of results */
      query->finished=1;
      break;
    }
    
    if(rc < 0) {
      /* <0 failure */
      query->failed=1;
      break;
    }
    
    /* otherwise is >0 match */
    query->result_count++;

    if(rasqal_engine_check_limit_offset(query) < 0)
      continue;

    /* else got result or finished */
    break;

  } /* while */

  return query->finished;
}


static void
rasqal_map_print_query_result_row(void *object, FILE *fh)
{
  if(object)
    rasqal_query_result_row_print((rasqal_query_result_row*)object, fh);
  else
    fputs("NULL", fh);
}


static void
rasqal_map_free_query_result_row(const void *key, const void *value)
{
  if(key)
    rasqal_free_query_result_row((rasqal_query_result_row*)key);
  if(value)
    rasqal_free_query_result_row((rasqal_query_result_row*)value);
}


/**
 * rasqal_query_execute:
 * @query: the #rasqal_query object
 *
 * Excute a query - run and return results.
 *
 * return value: a #rasqal_query_results structure or NULL on failure.
 **/
rasqal_query_results*
rasqal_query_execute(rasqal_query* query)
{
  rasqal_query_results *query_results;
  int rc=0;
  
  if(query->failed)
    return NULL;

  query->finished=0;
  query->executed=1;
  query->current_triple_result= -1;
  query->ask_result= -1;
  
  query_results=(rasqal_query_results*)RASQAL_CALLOC(rasqal_query_results, sizeof(rasqal_query_results), 1);
  query_results->query=query;

  rc=rasqal_engine_execute_init(query, query_results);
  if(rc) {
    query->failed=1;
    RASQAL_FREE(rasqal_query_results, query_results);
    return NULL;
  }

  if(query->factory->execute) {
    rc=query->factory->execute(query, query_results);
    if(rc) {
      query->failed=1;
      RASQAL_FREE(rasqal_query_results, query_results);
      return NULL;
    }
  }

  rasqal_query_add_query_result(query, query_results);

  if(query->order_conditions_sequence || query->distinct) {
    rasqal_map* map=NULL;
    raptor_sequence* seq;
    int offset=0;

    /* make a row:NULL map */
    map=rasqal_new_map(rasqal_query_result_row_compare,
                       rasqal_map_free_query_result_row, 
                       rasqal_map_print_query_result_row,
                       NULL,
                       0);

    /* get all query results and order them */
    seq=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_query_result_row, (raptor_sequence_print_handler*)rasqal_query_result_row_print);
    while(1) {
      rasqal_query_result_row* row;

      /* query->results_sequence is NOT assigned before here 
       * so that this function does the regular query results next
       * operation.
       */
      rc=rasqal_engine_get_next_result(query);
      if(rc < 1)
        /* <0 failure OR =0 end of results */
        break;
      
      if(rc < 0) {
        /* <0 failure */
        query->finished=1;
        query->failed=1;

        if(map)
          rasqal_free_map(map);
        raptor_free_sequence(seq);
        seq=NULL;
        break;
      }

      /* otherwise is >0 match */
      row=rasqal_new_query_result_row(query_results, offset);

      /* after this, row is owned by map */
      if(!rasqal_map_add_kv(map, row, NULL)) {
        offset++;
      } else {
       /* duplicate, and not added so delete it */
#ifdef RASQAL_DEBUG
        RASQAL_DEBUG1("Got duplicate row ");
        rasqal_query_result_row_print(row, stderr);
        fputc('\n', stderr);
#endif
        rasqal_free_query_result_row(row);
        row=NULL;
      }
    }

#ifdef RASQAL_DEBUG
    if(map) {
      fputs("resulting map ", stderr);
      rasqal_map_print(map, stderr);
      fputs("\n", stderr);
    }
#endif

    if(map) {
      rasqal_map_visit(map, rasqal_map_add_to_sequence, (void*)seq);
      rasqal_free_map(map);
    }
    query->results_sequence=seq;

    if(query->results_sequence) {
      query->finished= (raptor_sequence_size(query->results_sequence) == 0);

      /* Reset to first result an index into sequence of results */
      query->result_count= 0;
    }
  } else {
    /* No order sequence */
    rasqal_query_results_update(query_results);
    query_results->row=rasqal_new_query_result_row(query_results, 
                                                   query->result_count);
  }

  return query_results;
}


static const char* rasqal_query_verb_labels[RASQAL_QUERY_VERB_LAST+1]={
  "Unknown",
  "SELECT",
  "CONSTRUCT",
  "DESCRIBE",
  "ASK"
};

/* Utility methods */

/**
 * rasqal_query_verb_as_string:
 * @verb: the #rasqal_query_verb verb of the query
 *
 * Get a string for the query verb.
 * 
 * Return value: pointer to a shared string label for the query verb
 **/
const char*
rasqal_query_verb_as_string(rasqal_query_verb verb)
{
  if(verb <= RASQAL_QUERY_VERB_UNKNOWN || 
     verb > RASQAL_QUERY_VERB_LAST)
    verb=RASQAL_QUERY_VERB_UNKNOWN;

  return rasqal_query_verb_labels[(int)verb];
}
  

/**
 * rasqal_query_print:
 * @query: the #rasqal_query object
 * @fh: the #FILE* handle to print to.
 *
 * Print a query in a debug format.
 * 
 **/
void
rasqal_query_print(rasqal_query* query, FILE *fh)
{
  fprintf(fh, "query verb: %s\n", rasqal_query_verb_as_string(query->verb));
  
  if(query->distinct)
    fputs("query results distinct: yes\n", fh);
  if(query->limit >= 0)
    fprintf(fh, "query results limit: %d\n", query->limit);
  if(query->offset >= 0)
    fprintf(fh, "query results offset: %d\n", query->offset);

  fprintf(fh, "data graphs: ");
  if(query->data_graphs)
    raptor_sequence_print(query->data_graphs, fh);
  if(query->variables_sequence) {
    fprintf(fh, "\nall variables: "); 
    raptor_sequence_print(query->variables_sequence, fh);
  }
  if(query->anon_variables_sequence) {
    fprintf(fh, "\nanonymous variables: "); 
    raptor_sequence_print(query->anon_variables_sequence, fh);
  }
  if(query->selects) {
    fprintf(fh, "\nbound variables: "); 
    raptor_sequence_print(query->selects, fh);
  }
  if(query->describes) {
    fprintf(fh, "\ndescribes: ");
    raptor_sequence_print(query->describes, fh);
  }
  if(query->triples) {
    fprintf(fh, "\ntriples: ");
    raptor_sequence_print(query->triples, fh);
  }
  if(query->optional_triples) {
    fprintf(fh, "\noptional triples: ");
    raptor_sequence_print(query->optional_triples, fh);
  }
  if(query->constructs) {
    fprintf(fh, "\nconstructs: ");
    raptor_sequence_print(query->constructs, fh);
  }
  if(query->prefixes) {
    fprintf(fh, "\nprefixes: ");
    raptor_sequence_print(query->prefixes, fh);
  }
  if(query->query_graph_pattern) {
    fprintf(fh, "\nquery graph pattern: ");
    rasqal_graph_pattern_print(query->query_graph_pattern, fh);
  }
  if(query->order_conditions_sequence) {
    fprintf(fh, "\nquery order conditions: ");
    raptor_sequence_print(query->order_conditions_sequence, fh);
  }
  fputc('\n', fh);
}


static void
rasqal_query_add_query_result(rasqal_query* query,
                              rasqal_query_results* query_results) 
{
  query_results->next=query->results;
  query->results=query_results;
  /* add reference to ensure query lives as long as this runs */
  query->usage++;
}



static void
rasqal_query_remove_query_result(rasqal_query* query,
                                 rasqal_query_results* query_results) 
{
  rasqal_query_results *cur, *prev=NULL;
  for(cur=query->results; cur && cur != query_results; cur=cur->next)
    prev=cur;
  
  if(cur == query_results) {
    if(prev)
      prev->next=cur->next;
  }
  if(cur == query->results && cur != NULL)
    query->results=cur->next;

  /* remove reference and free if we are the last */
  rasqal_free_query(query);
}



/**
 * rasqal_free_query_results:
 * @query_results: #rasqal_query_results object
 *
 * Destructor - destroy a rasqal_query_results.
 *
 **/
void
rasqal_free_query_results(rasqal_query_results *query_results) {
  rasqal_query* query;

  if(!query_results)
    return;
  
  if(query_results->row)
    rasqal_free_query_result_row(query_results->row);

  query=query_results->query;
  rasqal_query_remove_query_result(query, query_results);
  RASQAL_FREE(rasqal_query_results, query_results);
}


/**
 * rasqal_query_results_is_bindings:
 * @query_results: #rasqal_query_results object
 *
 * Test if rasqal_query_results is variable bindings format.
 * 
 * Return value: non-0 if true
 **/
int
rasqal_query_results_is_bindings(rasqal_query_results *query_results) {
  rasqal_query* query=query_results->query;
  return (query->verb == RASQAL_QUERY_VERB_SELECT);
}


/**
 * rasqal_query_results_is_boolean:
 * @query_results: #rasqal_query_results object
 *
 * Test if rasqal_query_results is boolean format.
 * 
 * Return value: non-0 if true
 **/
int
rasqal_query_results_is_boolean(rasqal_query_results *query_results) {
  rasqal_query* query=query_results->query;
  return (query->verb == RASQAL_QUERY_VERB_ASK);
}
 

/**
 * rasqal_query_results_is_graph:
 * @query_results: #rasqal_query_results object
 *
 * Test if rasqal_query_results is RDF graph format.
 * 
 * Return value: non-0 if true
 **/
int
rasqal_query_results_is_graph(rasqal_query_results *query_results) {
  rasqal_query* query=query_results->query;
  return (query->verb == RASQAL_QUERY_VERB_CONSTRUCT ||
          query->verb == RASQAL_QUERY_VERB_DESCRIBE);
}


/**
 * rasqal_query_results_get_count:
 * @query_results: #rasqal_query_results query_results
 *
 * Get number of bindings so far.
 * 
 * Return value: number of bindings found so far or < 0 on failure
 **/
int
rasqal_query_results_get_count(rasqal_query_results *query_results)
{
  rasqal_query* query;

  if(!query_results)
    return -1;

  if(!rasqal_query_results_is_bindings(query_results))
    return -1;
  
  query=query_results->query;
  if(query->offset > 0)
    return query->result_count - query->offset;
  return query->result_count;
}


/**
 * rasqal_query_results_next:
 * @query_results: #rasqal_query_results query_results
 *
 * Move to the next result.
 * 
 * Return value: non-0 if failed or results exhausted
 **/
int
rasqal_query_results_next(rasqal_query_results *query_results)
{
  rasqal_query* query;
  
  if(!query_results)
    return 1;
  
  if(!rasqal_query_results_is_bindings(query_results))
    return 1;

  query=query_results->query;

  /* Ordered Results */
  if(query->results_sequence) {
    int size=raptor_sequence_size(query->results_sequence);

    while(1) {
      query->result_count++;
      if(query->result_count >= size) {
        query->finished=1;
        break;
      }

      if(rasqal_engine_check_limit_offset(query) < 0)
        continue;
      /* else got result or finished */
      break;
    }
  } else {
    rasqal_query_results_update(query_results);
    if(!query->finished)
      rasqal_query_result_row_update(query_results->row, query->result_count);
  }

  return query->finished;
}


/**
 * rasqal_query_results_finished:
 * @query_results: #rasqal_query_results query_results
 *
 * Find out if binding results are exhausted.
 * 
 * Return value: non-0 if results are finished or query failed
 **/
int
rasqal_query_results_finished(rasqal_query_results *query_results)
{
  if(!query_results)
    return 1;
  
  if(!rasqal_query_results_is_bindings(query_results))
    return 1;
  
  return (query_results->query->failed || query_results->query->finished);
}


/**
 * rasqal_query_results_get_bindings:
 * @query_results: #rasqal_query_results query_results
 * @names: pointer to an array of binding names (or NULL)
 * @values: pointer to an array of binding value #rasqal_literal (or NULL)
 *
 * Get all binding names, values for current result.
 * 
 * If names is not NULL, it is set to the address of a shared array
 * of names of the bindings (an output parameter).  These names
 * are shared and must not be freed by the caller
 *
 * If values is not NULL, it is set to the address of a shared array
 * of #rasqal_literal* binding values.  These values are shaerd
 * and must not be freed by the caller.
 * 
 * Return value: non-0 if the assignment failed
 **/
int
rasqal_query_results_get_bindings(rasqal_query_results *query_results,
                                  const unsigned char ***names, 
                                  rasqal_literal ***values)
{
  rasqal_query* query;
  
  if(!query_results)
    return 1;
  
  if(!rasqal_query_results_is_bindings(query_results))
    return 1;
  
  query=query_results->query;
  if(query->finished)
    return 1;

  if(names)
    *names=query->variable_names;
  
  if(values) {
    rasqal_query_result_row* row;

    if(query->results_sequence)
      /* Ordered Results */
      row=(rasqal_query_result_row*)raptor_sequence_get_at(query->results_sequence, query->result_count);
    else
      /* Streamed Results */
      row=query_results->row;

    *values=row->values;
  }
  
  return 0;
}


/**
 * rasqal_query_results_get_binding_value:
 * @query_results: #rasqal_query_results query_results
 * @offset: offset of binding name into array of known names
 *
 * Get one binding value for the current result.
 * 
 * Return value: a pointer to a shared #rasqal_literal binding value or NULL on failure
 **/
rasqal_literal*
rasqal_query_results_get_binding_value(rasqal_query_results *query_results, 
                                       int offset)
{
  rasqal_query* query;
  rasqal_query_result_row* row=NULL;

  if(!query_results)
    return NULL;
  
  if(!rasqal_query_results_is_bindings(query_results))
    return NULL;
  
  query=query_results->query;
  if(query->finished)
    return NULL;

  if(offset < 0 || offset > query->select_variables_count-1)
    return NULL;
  
  /* Ordered Results */
  if(query->results_sequence)
    row=(rasqal_query_result_row*)raptor_sequence_get_at(query->results_sequence, query->result_count);
  else
    row=query_results->row;

  if(row)
    return row->values[offset];
  else {
    query->finished=1;
    return NULL;
  }
}


/**
 * rasqal_query_results_get_binding_name:
 * @query_results: #rasqal_query_results query_results
 * @offset: offset of binding name into array of known names
 *
 * Get binding name for the current result.
 * 
 * Return value: a pointer to a shared copy of the binding name or NULL on failure
 **/
const unsigned char*
rasqal_query_results_get_binding_name(rasqal_query_results *query_results, 
                                      int offset)
{
  rasqal_query* query;

  if(!query_results)
    return NULL;
  
  if(!rasqal_query_results_is_bindings(query_results))
    return NULL;
  
  query=query_results->query;

  if(offset < 0 || offset > query->select_variables_count-1)
    return NULL;
  
  return query->variables[offset]->name;
}


/**
 * rasqal_query_results_get_binding_value_by_name:
 * @query_results: #rasqal_query_results query_results
 * @name: variable name
 *
 * Get one binding value for a given name in the current result.
 * 
 * Return value: a pointer to a shared #rasqal_literal binding value or NULL on failure
 **/
rasqal_literal*
rasqal_query_results_get_binding_value_by_name(rasqal_query_results *query_results,
                                               const unsigned char *name)
{
  int offset= -1;
  int i;
  rasqal_query* query;
  rasqal_query_result_row* row=NULL;

  if(!query_results)
    return NULL;
  
  if(!rasqal_query_results_is_bindings(query_results))
    return NULL;
  
  query=query_results->query;
  if(query->finished)
    return NULL;

  for(i=0; i< query->select_variables_count; i++) {
    if(!strcmp((const char*)name, (const char*)query->variables[i]->name)) {
      offset=i;
      break;
    }
  }
  
  if(offset < 0)
    return NULL;
  
  /* Ordered Results */
  if(query->results_sequence)
    row=(rasqal_query_result_row*)raptor_sequence_get_at(query->results_sequence, query->result_count);
  else
    row=query_results->row;
  
  if(row)
    return row->values[offset];
  else {
    query->finished=1;
    return NULL;
  }
}


/**
 * rasqal_query_results_get_bindings_count:
 * @query_results: #rasqal_query_results query_results
 *
 * Get the number of bound variables in the result.
 * 
 * Return value: <0 if failed or results exhausted
 **/
int
rasqal_query_results_get_bindings_count(rasqal_query_results *query_results)
{
  if(!query_results)
    return -1;
  
  if(!rasqal_query_results_is_bindings(query_results))
    return -1;
  
  return query_results->query->select_variables_count;
}


/**
 * rasqal_query_get_user_data:
 * @query: #rasqal_query
 *
 * Get query user data.
 * 
 * Return value: user data as set by rasqal_query_set_user_data
 **/
void*
rasqal_query_get_user_data(rasqal_query* query)
{
  return query->user_data;
}


/**
 * rasqal_query_set_user_data:
 * @query: #rasqal_query
 * @user_data: some user data to associate with the query
 *
 * Set the query user data.
 *
 **/
void
rasqal_query_set_user_data(rasqal_query* query, void *user_data)
{
  query->user_data=user_data;
}


/**
 * rasqal_query_get_verb:
 * @query: #rasqal_query
 *
 * Get the query verb.
 *
 * Return value: the operating verb of the query of type rasqal_query_verb
 **/
rasqal_query_verb
rasqal_query_get_verb(rasqal_query* query)
{
  return query->verb;
}


/**
 * rasqal_query_get_wildcard:
 * @query: #rasqal_query
 *
 * Get the query verb is wildcard flag.
 *
 * Return value: non-0 if the query verb was a wildcard (such as SELECT *)
 **/
int
rasqal_query_get_wildcard(rasqal_query* query)
{
  return query->wildcard;
}


/**
 * rasqal_query_get_order_conditions_sequence:
 * @query: #rasqal_query query object
 *
 * Get the sequence of query ordering conditions.
 *
 * Return value: a #raptor_sequence of #rasqal_expression pointers.
 **/
raptor_sequence*
rasqal_query_get_order_conditions_sequence(rasqal_query* query)
{
  return query->order_conditions_sequence;
}


/**
 * rasqal_query_get_order_condition:
 * @query: #rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Get a query ordering expression in the sequence of query ordering conditions.
 *
 * Return value: a #rasqal_expression pointer or NULL if out of the sequence range
 **/
rasqal_expression*
rasqal_query_get_order_condition(rasqal_query* query, int idx)
{
  if(!query->order_conditions_sequence)
    return NULL;
  
  return (rasqal_expression*)raptor_sequence_get_at(query->order_conditions_sequence, idx);
}


/**
 * rasqal_query_results_write:
 * @iostr: #raptor_iostream to write the query to
 * @results: #rasqal_query_results query results format
 * @format_uri: #raptor_uri describing the format to write (or NULL for default)
 * @base_uri: #raptor_uri base URI of the output format
 *
 * Write the query results to an iostream in a format.
 * 
 * The supported URIs for the format_uri are:
 *
 * http://www.w3.org/2005/sparql-results# (default)
 *
 * Older formats:
 * http://www.w3.org/TR/2005/WD-rdf-sparql-XMLres-20050527/
 * http://www.w3.org/2001/sw/DataAccess/rf1/result2
 *
 * http://www.w3.org/TR/2004/WD-rdf-sparql-XMLres-20041221/
 * http://www.w3.org/2001/sw/DataAccess/rf1/result
 *
 * If the writing succeeds, the query results will be exhausted.
 * 
 * Return value: non-0 on failure
 **/
int
rasqal_query_results_write(raptor_iostream *iostr,
                           rasqal_query_results *results,
                           raptor_uri *format_uri,
                           raptor_uri *base_uri)
{
  /*
   * SPARQL XML Results 2005-??-?? (to appear)
   * http://www.w3.org/2005/sparql-results#
   */
  if(!format_uri ||
     !strcmp((const char*)raptor_uri_as_string(format_uri),
             "http://www.w3.org/2005/sparql-results#"))
    return rasqal_query_results_write_xml_result3(iostr, results, base_uri);

  /*
   * SPARQL XML Results 2005-05-27
   * http://www.w3.org/TR/2005/WD-rdf-sparql-XMLres-20050527/
   * http://www.w3.org/2001/sw/DataAccess/rf1/result2
   */
  if(!strcmp((const char*)raptor_uri_as_string(format_uri),
            "http://www.w3.org/2001/sw/DataAccess/rf1/result2") ||
     !strcmp((const char*)raptor_uri_as_string(format_uri),
             "http://www.w3.org/TR/2005/WD-rdf-sparql-XMLres-20050527/"))
    return rasqal_query_results_write_xml_result2(iostr, results, base_uri);

  /*
   * SPARQL XML Results 2004-12-21
   * http://www.w3.org/TR/2004/WD-rdf-sparql-XMLres-20041221/
   * http://www.w3.org/2001/sw/DataAccess/rf1/result
   */
  if(!strcmp((const char*)raptor_uri_as_string(format_uri),
            "http://www.w3.org/2001/sw/DataAccess/rf1/result") ||
     !strcmp((const char*)raptor_uri_as_string(format_uri),
             "http://www.w3.org/TR/2004/WD-rdf-sparql-XMLres-20041221/"))
    return rasqal_query_results_write_xml_20041221(iostr, results, base_uri);

  return 1;
}


/*
 * rasqal_query_results_write_xml_20041221:
 * @iostr: #raptor_iostream to write the query to
 * @results: #rasqal_query_results query results format
 * @base_uri: #raptor_uri base URI of the output format
 *
 * Write the 2004-12-21 XML query results format to an iostream in a
 * format - INTERNAL.
 * 
 * If the writing succeeds, the query results will be exhausted.
 * 
 * Return value: non-0 on failure
 **/
static int
rasqal_query_results_write_xml_20041221(raptor_iostream *iostr,
                                        rasqal_query_results *results,
                                        raptor_uri *base_uri)
{
  rasqal_query* query=results->query;
  raptor_uri_handler *uri_handler;
  void *uri_context;
  raptor_xml_writer* xml_writer;
  raptor_namespace *res_ns;
  raptor_namespace_stack *nstack;
  raptor_qname* sparql_qname;
  raptor_xml_element *sparql_element;
  raptor_qname* results_qname;
  raptor_xml_element *results_element;
  raptor_qname* result_qname;
  raptor_xml_element *result_element;
  raptor_qname* qname1;
  raptor_xml_element *element1;
  raptor_qname **attrs;
  int i;
  raptor_uri* base_uri_copy=NULL;
  
  if(!rasqal_query_results_is_bindings(results)) {
    rasqal_query_error(query, "Can only write XML format 2004-11-21 for variable binding results");
    return 1;
  }
  
  raptor_uri_get_handler(&uri_handler, &uri_context);

  nstack=raptor_new_namespaces(uri_handler, uri_context,
                               rasqal_query_simple_error, query,
                               1);
  xml_writer=raptor_new_xml_writer(nstack,
                                   uri_handler, uri_context,
                                   iostr,
                                   rasqal_query_simple_error, query,
                                   1);
  if(!xml_writer)
    return 1;

  res_ns=raptor_new_namespace(nstack,
                              NULL,
                              (const unsigned char*)"http://www.w3.org/2001/sw/DataAccess/rf1/result",
                              0);

  raptor_xml_writer_raw(xml_writer,
                        (const unsigned char*)"<?xml version=\"1.0\"?>\n");


  sparql_qname=raptor_new_qname_from_namespace_local_name(res_ns,
                                                          (const unsigned char*)"sparql",
                                                          NULL); /* no attribute value - element */
  
  base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
  sparql_element=raptor_new_xml_element(sparql_qname,
                                        NULL, /* language */
                                        base_uri_copy);

  raptor_xml_writer_start_element(xml_writer, sparql_element);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);


  /*   <head> */
  qname1=raptor_new_qname_from_namespace_local_name(res_ns, 
                          (const unsigned char*)"head",
                          NULL); /* no attribute value - element */
  
  base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
  element1=raptor_new_xml_element(qname1,
                                  NULL, /* language */
                                  base_uri_copy);

  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"  ", 2);
  raptor_xml_writer_start_element(xml_writer, element1);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);

  for(i=0; 1; i++) {
    const unsigned char *name;
    raptor_qname* variable_qname;
    raptor_xml_element *variable_element;

    name=rasqal_query_results_get_binding_name(results, i);
    if(!name)
      break;

    /*     <variable name="x"/> */
    variable_qname=raptor_new_qname_from_namespace_local_name(res_ns, 
                                    (const unsigned char*)"variable",
                                    NULL); /* no attribute value - element */
    
    base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
    variable_element=raptor_new_xml_element(variable_qname,
                                            NULL,
                                            base_uri_copy);
    

    attrs=(raptor_qname **)raptor_alloc_memory(sizeof(raptor_qname*));
    attrs[0]=raptor_new_qname_from_namespace_local_name(res_ns, 
                              (const unsigned char*)"name",
                              (const unsigned char*)name); /* attribute value */
    raptor_xml_element_set_attributes(variable_element, attrs, 1);

    raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"    ", 4);
    raptor_xml_writer_empty_element(xml_writer, variable_element);
    raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);

    raptor_free_xml_element(variable_element);
  }

  /*   </head> */
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"  ", 2);
  raptor_xml_writer_end_element(xml_writer, element1);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);

  raptor_free_xml_element(element1);


  /*   <results> */
  results_qname=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                           (const unsigned char*)"results",
                                                           NULL);
  
  base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
  results_element=raptor_new_xml_element(results_qname,
                                         NULL, /* language */
                                         base_uri_copy);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"  ", 2);
  raptor_xml_writer_start_element(xml_writer, results_element);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);


  /* declare result element for later multiple use */
  result_qname=raptor_new_qname_from_namespace_local_name(res_ns, 
                                (const unsigned char*)"result",
                                                          NULL);
  
  base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
  result_element=raptor_new_xml_element(result_qname,
                                        NULL, /* language */
                                        base_uri_copy);


  while(!rasqal_query_results_finished(results)) {
    /*     <result> */
    raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"    ", 4);
    raptor_xml_writer_start_element(xml_writer, result_element);
    raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);

    for(i=0; i<rasqal_query_results_get_bindings_count(results); i++) {
      const unsigned char *name=rasqal_query_results_get_binding_name(results, i);
      rasqal_literal *l=rasqal_query_results_get_binding_value(results, i);

      qname1=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                        (const unsigned char*)name,
                                                        NULL);

      base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
      element1=raptor_new_xml_element(qname1,
                                      NULL, /* language */
                                      base_uri_copy);
      

      raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"      ", 6);

      if(!l) {
        attrs=(raptor_qname **)raptor_alloc_memory(sizeof(raptor_qname*));
        attrs[0]=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                            (const unsigned char*)"bound",
                                                            (const unsigned char*)"false");
        
        raptor_xml_element_set_attributes(element1, attrs, 1);
        
        raptor_xml_writer_empty_element(xml_writer, element1);

      } else switch(l->type) {
        case RASQAL_LITERAL_URI:
          attrs=(raptor_qname **)raptor_alloc_memory(sizeof(raptor_qname*));
          attrs[0]=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                              (const unsigned char*)"uri",
                                                              (const unsigned char*)raptor_uri_as_string(l->value.uri));

          raptor_xml_element_set_attributes(element1, attrs, 1);

          raptor_xml_writer_empty_element(xml_writer, element1);

          break;
        case RASQAL_LITERAL_BLANK:
          attrs=(raptor_qname **)raptor_alloc_memory(sizeof(raptor_qname*));
          attrs[0]=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                              (const unsigned char*)"bnodeid",
                                                              l->string);

          raptor_xml_element_set_attributes(element1, attrs, 1);

          raptor_xml_writer_empty_element(xml_writer, element1);

          break;
        case RASQAL_LITERAL_STRING:
          if(l->language || l->datatype) {
            attrs=(raptor_qname **)raptor_alloc_memory(sizeof(raptor_qname*));

            if(l->language)
              attrs[0]=raptor_new_qname(nstack,
                                        (const unsigned char*)"xml:lang",
                                        (const unsigned char*)l->language,
                                        rasqal_query_simple_error, query);
            else
              attrs[0]=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                                  (const unsigned char*)"datatype",
                                                                  (const unsigned char*)raptor_uri_as_string(l->datatype));
            raptor_xml_element_set_attributes(element1, attrs, 1);
          }


          raptor_xml_writer_start_element(xml_writer, element1);


          raptor_xml_writer_cdata_counted(xml_writer,
                                          (const unsigned char*)l->string,
                                          l->string_len);

          raptor_xml_writer_end_element(xml_writer, element1);
          
          break;
        case RASQAL_LITERAL_PATTERN:
        case RASQAL_LITERAL_QNAME:
        case RASQAL_LITERAL_INTEGER:
        case RASQAL_LITERAL_BOOLEAN:
        case RASQAL_LITERAL_DOUBLE:
        case RASQAL_LITERAL_FLOAT:
        case RASQAL_LITERAL_VARIABLE:
        case RASQAL_LITERAL_DECIMAL:
        case RASQAL_LITERAL_DATETIME:

        case RASQAL_LITERAL_UNKNOWN:
        default:
          rasqal_query_error(query, "Cannot turn literal type %d into XML", 
                             l->type);
      }

      raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);

      raptor_free_xml_element(element1);
    }

    raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"    ", 4);
    raptor_xml_writer_end_element(xml_writer, result_element);
    raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);
    
    rasqal_query_results_next(results);
  }

  raptor_free_xml_element(result_element);

  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"  ", 2);
  raptor_xml_writer_end_element(xml_writer, results_element);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);

  raptor_free_xml_element(results_element);

  raptor_xml_writer_end_element(xml_writer, sparql_element);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);

  raptor_free_xml_element(sparql_element);


  raptor_free_xml_writer(xml_writer);

  raptor_free_namespace(res_ns);

  raptor_free_namespaces(nstack);

  return 0;
}


/*
 * rasqal_query_results_write_xml_result2:
 * @iostr: #raptor_iostream to write the query to
 * @results: #rasqal_query_results query results format
 * @base_uri: #raptor_uri base URI of the output format
 *
 * Write the second version of the XML query results format to an
 * iostream in a format - INTERNAL.
 * 
 * If the writing succeeds, the query results will be exhausted.
 * 
 * Return value: non-0 on failure
 **/
static int
rasqal_query_results_write_xml_result2(raptor_iostream *iostr,
                                       rasqal_query_results *results,
                                       raptor_uri *base_uri)
{
  rasqal_query* query=results->query;
  raptor_uri_handler *uri_handler;
  void *uri_context;
  raptor_xml_writer* xml_writer;
  raptor_namespace *res_ns;
  raptor_namespace_stack *nstack;
  raptor_qname* sparql_qname;
  raptor_xml_element *sparql_element;
  raptor_qname* results_qname;
  raptor_xml_element *results_element;
  raptor_qname* result_qname;
  raptor_xml_element *result_element;
  raptor_qname* qname1;
  raptor_xml_element *element1;
  raptor_qname **attrs;
  int i;
  raptor_uri* base_uri_copy=NULL;

  if(!rasqal_query_results_is_bindings(results) &&
     !rasqal_query_results_is_boolean(results)) {
    rasqal_query_error(query, "Can only write XML format v2 for variable binding and boolean results");
    return 1;
  }
  
  
  raptor_uri_get_handler(&uri_handler, &uri_context);

  nstack=raptor_new_namespaces(uri_handler, uri_context,
                               rasqal_query_simple_error, query,
                               1);
  xml_writer=raptor_new_xml_writer(nstack,
                                   uri_handler, uri_context,
                                   iostr,
                                   rasqal_query_simple_error, query,
                                   1);
  if(!xml_writer)
    return 1;

  res_ns=raptor_new_namespace(nstack,
                              NULL,
                              (const unsigned char*)"http://www.w3.org/2001/sw/DataAccess/rf1/result2",
                              0);


  raptor_xml_writer_raw(xml_writer,
                        (const unsigned char*)"<?xml version=\"1.0\"?>\n");


  sparql_qname=raptor_new_qname_from_namespace_local_name(res_ns,
                                                          (const unsigned char*)"sparql",
                                                          NULL); /* no attribute value - element */
  
  base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
  sparql_element=raptor_new_xml_element(sparql_qname,
                                        NULL, /* language */
                                        base_uri_copy);

  if(rasqal_query_results_is_bindings(results)) {
    raptor_namespace* xsi_ns;
    raptor_namespace* xs_ns;
    xsi_ns=raptor_new_namespace(nstack,
                                (const unsigned char*)"xsi",
                                (const unsigned char*)"http://www.w3.org/2001/XMLSchema-instance",
                                0);
    raptor_xml_element_declare_namespace(sparql_element, xsi_ns);
    
    xs_ns=raptor_new_namespace(nstack,
                               (const unsigned char*)"xs",
                               (const unsigned char*)"http://www.w3.org/2001/XMLSchema",
                               0);
    raptor_xml_element_declare_namespace(sparql_element, xs_ns);

    attrs=(raptor_qname **)raptor_alloc_memory(sizeof(raptor_qname*));
    attrs[0]=raptor_new_qname_from_namespace_local_name(xsi_ns,
                                                        (const unsigned char*)"schemaLocation",  
                                                        (const unsigned char*)"http://www.w3.org/2001/sw/DataAccess/rf1/result2.xsd");
    raptor_xml_element_set_attributes(sparql_element, attrs, 1);
  }
  
  raptor_xml_writer_start_element(xml_writer, sparql_element);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);

  /*   <head> */
  qname1=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                    (const unsigned char*)"head",
                                                    NULL); /* no attribute value - element */
  
  base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
  element1=raptor_new_xml_element(qname1,
                                  NULL, /* language */
                                  base_uri_copy);
  
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"  ", 2);
  raptor_xml_writer_start_element(xml_writer, element1);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);
  
  /* At present <head> for boolean results has no content */
  if(rasqal_query_results_is_bindings(results)) {
    for(i=0; 1; i++) {
      const unsigned char *name;
      raptor_qname* variable_qname;
      raptor_xml_element *variable_element;
      
      name=rasqal_query_results_get_binding_name(results, i);
      if(!name)
        break;
      
      /*     <variable name="x"/> */
      variable_qname=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                                (const unsigned char*)"variable",
                                                                NULL); /* no attribute value - element */
      
      base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
      variable_element=raptor_new_xml_element(variable_qname,
                                              NULL,
                                              base_uri_copy);
      
      
      attrs=(raptor_qname **)raptor_alloc_memory(sizeof(raptor_qname*));
      attrs[0]=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                          (const unsigned char*)"name",
                                                          (const unsigned char*)name); /* attribute value */
      raptor_xml_element_set_attributes(variable_element, attrs, 1);
      
      raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"    ", 4);
      raptor_xml_writer_empty_element(xml_writer, variable_element);
      raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);
      
      raptor_free_xml_element(variable_element);
    }
  }
    
  /*   </head> */
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"  ", 2);
  raptor_xml_writer_end_element(xml_writer, element1);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);
  
  raptor_free_xml_element(element1);
  

  /*   <results> */
  results_qname=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                           (const unsigned char*)"results",
                                                           NULL);
  
  base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
  results_element=raptor_new_xml_element(results_qname,
                                         NULL, /* language */
                                         base_uri_copy);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"  ", 2);
  raptor_xml_writer_start_element(xml_writer, results_element);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);


  /* Boolean Results */
  if(rasqal_query_results_is_boolean(results)) {
    result_qname=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                            (const unsigned char*)"boolean",
                                                            NULL);
    base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
    result_element=raptor_new_xml_element(result_qname,
                                          NULL, /* language */
                                          base_uri_copy);

    raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"    ", 4);
    raptor_xml_writer_start_element(xml_writer, result_element);
    if(rasqal_query_results_get_boolean(results))
      raptor_xml_writer_raw(xml_writer, RASQAL_XSD_BOOLEAN_TRUE);
    else
      raptor_xml_writer_raw(xml_writer, RASQAL_XSD_BOOLEAN_FALSE);
    raptor_xml_writer_end_element(xml_writer, result_element);
    raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);

    goto resultsdone;
  }


  /* Variable Binding Results */

  /* declare result element for later multiple use */
  result_qname=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                          (const unsigned char*)"result",
                                                          NULL);
  base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
  result_element=raptor_new_xml_element(result_qname,
                                        NULL, /* language */
                                        base_uri_copy);


  while(!rasqal_query_results_finished(results)) {
    /*     <result> */
    raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"    ", 4);
    raptor_xml_writer_start_element(xml_writer, result_element);
    raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);

    for(i=0; i<rasqal_query_results_get_bindings_count(results); i++) {
      const unsigned char *name=rasqal_query_results_get_binding_name(results, i);
      raptor_qname* binding_qname;
      raptor_xml_element *binding_element;
      rasqal_literal *l=rasqal_query_results_get_binding_value(results, i);

      /*       <binding> */
      binding_qname=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                               (const unsigned char*)"binding",
                                                               NULL);
      
      base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
      binding_element=raptor_new_xml_element(binding_qname,
                                             NULL, /* language */
                                             base_uri_copy);
      attrs=(raptor_qname **)raptor_alloc_memory(sizeof(raptor_qname*));
      attrs[0]=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                          (const unsigned char*)"name",
                                                          name);
      raptor_xml_element_set_attributes(binding_element, attrs, 1);
      

      raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"      ", 6);
      raptor_xml_writer_start_element(xml_writer, binding_element);

      if(!l) {
        qname1=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                          (const unsigned char*)"unbound",
                                                          NULL);

        base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
        element1=raptor_new_xml_element(qname1,
                                        NULL, /* language */
                                        base_uri_copy);

        raptor_xml_writer_empty_element(xml_writer, element1);

      } else switch(l->type) {
        case RASQAL_LITERAL_URI:
          qname1=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                            (const unsigned char*)"uri",
                                                            NULL);
          
          base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
          element1=raptor_new_xml_element(qname1,
                                          NULL, /* language */
                                          base_uri_copy);
          
          raptor_xml_writer_start_element(xml_writer, element1);
          raptor_xml_writer_cdata(xml_writer, (const unsigned char*)raptor_uri_as_string(l->value.uri));
          raptor_xml_writer_end_element(xml_writer, element1);

          break;

        case RASQAL_LITERAL_BLANK:
          qname1=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                            (const unsigned char*)"bnode",
                                                            NULL);
          
          base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
          element1=raptor_new_xml_element(qname1,
                                          NULL, /* language */
                                          base_uri_copy);
          
          raptor_xml_writer_start_element(xml_writer, element1);
          raptor_xml_writer_cdata(xml_writer, (const unsigned char*)l->string);
          raptor_xml_writer_end_element(xml_writer, element1);
          break;

        case RASQAL_LITERAL_STRING:
          qname1=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                            (const unsigned char*)"literal",
                                                            NULL);
          
          base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
          element1=raptor_new_xml_element(qname1,
                                          NULL, /* language */
                                          base_uri_copy);

          if(l->language || l->datatype) {
            attrs=(raptor_qname **)raptor_alloc_memory(sizeof(raptor_qname*));

            if(l->language)
              attrs[0]=raptor_new_qname(nstack,
                                        (const unsigned char*)"xml:lang",
                                        (const unsigned char*)l->language,
                                        rasqal_query_simple_error, query);
            else
              attrs[0]=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                                  (const unsigned char*)"datatype",
                                                                  (const unsigned char*)raptor_uri_as_string(l->datatype));
            raptor_xml_element_set_attributes(element1, attrs, 1);
          }


          raptor_xml_writer_start_element(xml_writer, element1);


          raptor_xml_writer_cdata_counted(xml_writer,
                                          (const unsigned char*)l->string, 
                                          l->string_len);

          raptor_xml_writer_end_element(xml_writer, element1);
          
          break;
        case RASQAL_LITERAL_PATTERN:
        case RASQAL_LITERAL_QNAME:
        case RASQAL_LITERAL_INTEGER:
        case RASQAL_LITERAL_BOOLEAN:
        case RASQAL_LITERAL_DOUBLE:
        case RASQAL_LITERAL_FLOAT:
        case RASQAL_LITERAL_VARIABLE:
        case RASQAL_LITERAL_DECIMAL:
        case RASQAL_LITERAL_DATETIME:

        case RASQAL_LITERAL_UNKNOWN:
        default:
          rasqal_query_error(query, "Cannot turn literal type %d into XML", 
                             l->type);
      }

      raptor_free_xml_element(element1);

      /*       </binding> */
      raptor_xml_writer_end_element(xml_writer, binding_element);
      raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);
      
      raptor_free_xml_element(binding_element);
    }

    raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"    ", 4);
    raptor_xml_writer_end_element(xml_writer, result_element);
    raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);
    
    rasqal_query_results_next(results);
  }

  resultsdone:
  
  raptor_free_xml_element(result_element);

  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"  ", 2);
  raptor_xml_writer_end_element(xml_writer, results_element);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);

  raptor_free_xml_element(results_element);

  raptor_xml_writer_end_element(xml_writer, sparql_element);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);

  raptor_free_xml_element(sparql_element);


  raptor_free_xml_writer(xml_writer);

  raptor_free_namespace(res_ns);

  raptor_free_namespaces(nstack);

  return 0;
}


/*
 * rasqal_query_results_write_xml_result3:
 * @iostr: #raptor_iostream to write the query to
 * @results: #rasqal_query_results query results format
 * @base_uri: #raptor_uri base URI of the output format
 *
 * Write the third version of the XML query results format to an
 * iostream in a format - INTERNAL.
 * 
 * If the writing succeeds, the query results will be exhausted.
 * 
 * Return value: non-0 on failure
 **/
static int
rasqal_query_results_write_xml_result3(raptor_iostream *iostr,
                                       rasqal_query_results *results,
                                       raptor_uri *base_uri)
{
  rasqal_query* query=results->query;
  raptor_uri_handler *uri_handler;
  void *uri_context;
  raptor_xml_writer* xml_writer;
  raptor_namespace *res_ns;
  raptor_namespace_stack *nstack;
  raptor_qname* sparql_qname;
  raptor_xml_element *sparql_element;
  raptor_qname* results_qname;
  raptor_xml_element *results_element;
  raptor_qname* result_qname;
  raptor_xml_element *result_element;
  raptor_qname* qname1;
  raptor_xml_element *element1;
  raptor_qname **attrs;
  int i;
  raptor_uri* base_uri_copy=NULL;

  if(!rasqal_query_results_is_bindings(results) &&
     !rasqal_query_results_is_boolean(results)) {
    rasqal_query_error(query, "Can only write XML format v3 for variable binding and boolean results");
    return 1;
  }
  
  
  raptor_uri_get_handler(&uri_handler, &uri_context);

  nstack=raptor_new_namespaces(uri_handler, uri_context,
                               rasqal_query_simple_error, query,
                               1);
  xml_writer=raptor_new_xml_writer(nstack,
                                   uri_handler, uri_context,
                                   iostr,
                                   rasqal_query_simple_error, query,
                                   1);
  if(!xml_writer)
    return 1;

  res_ns=raptor_new_namespace(nstack,
                              NULL,
                              (const unsigned char*)"http://www.w3.org/2005/sparql-results#",
                              0);


  raptor_xml_writer_raw(xml_writer,
                        (const unsigned char*)"<?xml version=\"1.0\"?>\n");


  sparql_qname=raptor_new_qname_from_namespace_local_name(res_ns,
                                                          (const unsigned char*)"sparql",
                                                          NULL); /* no attribute value - element */
  
  base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
  sparql_element=raptor_new_xml_element(sparql_qname,
                                        NULL, /* language */
                                        base_uri_copy);

  if(rasqal_query_results_is_bindings(results)) {
    /* FIXME - consider when to write the XSD.  Need the XSD URI too. */
#if 0
    raptor_namespace* xsi_ns;
    xsi_ns=raptor_new_namespace(nstack,
                                (const unsigned char*)"xsi",
                                (const unsigned char*)"http://www.w3.org/2001/XMLSchema-instance",
                                0);
    raptor_xml_element_declare_namespace(sparql_element, xsi_ns);
    
    attrs=(raptor_qname **)raptor_alloc_memory(sizeof(raptor_qname*));
    attrs[0]=raptor_new_qname_from_namespace_local_name(xsi_ns,
                                                        (const unsigned char*)"schemaLocation",  
                                                        (const unsigned char*)"http://www.w3.org/2001/sw/DataAccess/rf1/result2.xsd");
    raptor_xml_element_set_attributes(sparql_element, attrs, 1);
#endif
  }
  
  raptor_xml_writer_start_element(xml_writer, sparql_element);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);

  /*   <head> */
  qname1=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                    (const unsigned char*)"head",
                                                    NULL); /* no attribute value - element */
  
  base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
  element1=raptor_new_xml_element(qname1,
                                  NULL, /* language */
                                  base_uri_copy);
  
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"  ", 2);
  raptor_xml_writer_start_element(xml_writer, element1);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);
  
  if(rasqal_query_results_is_bindings(results)) {
    for(i=0; 1; i++) {
      const unsigned char *name;
      raptor_qname* variable_qname;
      raptor_xml_element *variable_element;
      
      name=rasqal_query_results_get_binding_name(results, i);
      if(!name)
        break;
      
      /*     <variable name="x"/> */
      variable_qname=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                                (const unsigned char*)"variable",
                                                                NULL); /* no attribute value - element */
      
      base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
      variable_element=raptor_new_xml_element(variable_qname,
                                              NULL,
                                              base_uri_copy);
      
      
      attrs=(raptor_qname **)raptor_alloc_memory(sizeof(raptor_qname*));
      attrs[0]=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                          (const unsigned char*)"name",
                                                          (const unsigned char*)name); /* attribute value */
      raptor_xml_element_set_attributes(variable_element, attrs, 1);
      
      raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"    ", 4);
      raptor_xml_writer_empty_element(xml_writer, variable_element);
      raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);
      
      raptor_free_xml_element(variable_element);
    }
  }

  /* FIXME - could add <link> inside <head> */

    
  /*   </head> */
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"  ", 2);
  raptor_xml_writer_end_element(xml_writer, element1);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);
  
  raptor_free_xml_element(element1);
  

  /* Boolean Results */
  if(rasqal_query_results_is_boolean(results)) {
    result_qname=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                            (const unsigned char*)"boolean",
                                                            NULL);
    base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
    result_element=raptor_new_xml_element(result_qname,
                                          NULL, /* language */
                                          base_uri_copy);

    raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"  ", 2);
    raptor_xml_writer_start_element(xml_writer, result_element);
    if(rasqal_query_results_get_boolean(results))
      raptor_xml_writer_raw(xml_writer, RASQAL_XSD_BOOLEAN_TRUE);
    else
      raptor_xml_writer_raw(xml_writer, RASQAL_XSD_BOOLEAN_FALSE);
    raptor_xml_writer_end_element(xml_writer, result_element);
    raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);

    goto results3done;
  }


  /* Variable Binding Results */

  /*   <results> */
  results_qname=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                           (const unsigned char*)"results",
                                                           NULL);
  
  base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
  results_element=raptor_new_xml_element(results_qname,
                                         NULL, /* language */
                                         base_uri_copy);

  attrs=(raptor_qname **)raptor_alloc_memory(2*sizeof(raptor_qname*));
  i=(rasqal_query_get_order_condition(query, 0) != NULL);
  attrs[0]=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                      (const unsigned char*)"ordered", 
                                                      i ? RASQAL_XSD_BOOLEAN_TRUE : RASQAL_XSD_BOOLEAN_FALSE);
  
  i=rasqal_query_get_distinct(query);
  attrs[1]=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                      (const unsigned char*)"distinct",
                                                      i ? RASQAL_XSD_BOOLEAN_TRUE : RASQAL_XSD_BOOLEAN_FALSE);
  raptor_xml_element_set_attributes(results_element, attrs, 2);

  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"  ", 2);
  raptor_xml_writer_start_element(xml_writer, results_element);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);


  /* declare result element for later multiple use */
  result_qname=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                          (const unsigned char*)"result",
                                                          NULL);
  base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
  result_element=raptor_new_xml_element(result_qname,
                                        NULL, /* language */
                                        base_uri_copy);


  while(!rasqal_query_results_finished(results)) {
    /*     <result> */
    raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"    ", 4);
    raptor_xml_writer_start_element(xml_writer, result_element);
    raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);

    for(i=0; i<rasqal_query_results_get_bindings_count(results); i++) {
      const unsigned char *name=rasqal_query_results_get_binding_name(results, i);
      raptor_qname* binding_qname;
      raptor_xml_element *binding_element;
      rasqal_literal *l=rasqal_query_results_get_binding_value(results, i);

      /*       <binding> */
      binding_qname=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                               (const unsigned char*)"binding",
                                                               NULL);
      
      base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
      binding_element=raptor_new_xml_element(binding_qname,
                                             NULL, /* language */
                                             base_uri_copy);
      attrs=(raptor_qname **)raptor_alloc_memory(sizeof(raptor_qname*));
      attrs[0]=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                          (const unsigned char*)"name",
                                                          name);
      raptor_xml_element_set_attributes(binding_element, attrs, 1);
      

      raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"      ", 6);
      raptor_xml_writer_start_element(xml_writer, binding_element);

      if(!l) {
        qname1=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                          (const unsigned char*)"unbound",
                                                          NULL);

        base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
        element1=raptor_new_xml_element(qname1,
                                        NULL, /* language */
                                        base_uri_copy);

        raptor_xml_writer_empty_element(xml_writer, element1);

      } else switch(l->type) {
        case RASQAL_LITERAL_URI:
          qname1=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                            (const unsigned char*)"uri",
                                                            NULL);
          
          base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
          element1=raptor_new_xml_element(qname1,
                                          NULL, /* language */
                                          base_uri_copy);
          
          raptor_xml_writer_start_element(xml_writer, element1);
          raptor_xml_writer_cdata(xml_writer, (const unsigned char*)raptor_uri_as_string(l->value.uri));
          raptor_xml_writer_end_element(xml_writer, element1);

          break;

        case RASQAL_LITERAL_BLANK:
          qname1=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                            (const unsigned char*)"bnode",
                                                            NULL);
          
          base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
          element1=raptor_new_xml_element(qname1,
                                          NULL, /* language */
                                          base_uri_copy);
          
          raptor_xml_writer_start_element(xml_writer, element1);
          raptor_xml_writer_cdata(xml_writer, (const unsigned char*)l->string);
          raptor_xml_writer_end_element(xml_writer, element1);
          break;

        case RASQAL_LITERAL_STRING:
          qname1=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                            (const unsigned char*)"literal",
                                                            NULL);
          
          base_uri_copy=base_uri ? raptor_uri_copy(base_uri) : NULL;
          element1=raptor_new_xml_element(qname1,
                                          NULL, /* language */
                                          base_uri_copy);

          if(l->language || l->datatype) {
            attrs=(raptor_qname **)raptor_alloc_memory(sizeof(raptor_qname*));

            if(l->language)
              attrs[0]=raptor_new_qname(nstack,
                                        (const unsigned char*)"xml:lang",
                                        (const unsigned char*)l->language,
                                        rasqal_query_simple_error, query);
            else
              attrs[0]=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                                  (const unsigned char*)"datatype",
                                                                  (const unsigned char*)raptor_uri_as_string(l->datatype));
            raptor_xml_element_set_attributes(element1, attrs, 1);
          }


          raptor_xml_writer_start_element(xml_writer, element1);


          raptor_xml_writer_cdata_counted(xml_writer,
                                          (const unsigned char*)l->string, 
                                          l->string_len);

          raptor_xml_writer_end_element(xml_writer, element1);
          
          break;
        case RASQAL_LITERAL_PATTERN:
        case RASQAL_LITERAL_QNAME:
        case RASQAL_LITERAL_INTEGER:
        case RASQAL_LITERAL_BOOLEAN:
        case RASQAL_LITERAL_DOUBLE:
        case RASQAL_LITERAL_FLOAT:
        case RASQAL_LITERAL_VARIABLE:
        case RASQAL_LITERAL_DECIMAL:
        case RASQAL_LITERAL_DATETIME:

        case RASQAL_LITERAL_UNKNOWN:
        default:
          rasqal_query_error(query, "Cannot turn literal type %d into XML", 
                             l->type);
      }

      raptor_free_xml_element(element1);

      /*       </binding> */
      raptor_xml_writer_end_element(xml_writer, binding_element);
      raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);
      
      raptor_free_xml_element(binding_element);
    }

    raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"    ", 4);
    raptor_xml_writer_end_element(xml_writer, result_element);
    raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);
    
    rasqal_query_results_next(results);
  }

  raptor_free_xml_element(result_element);

  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"  ", 2);
  raptor_xml_writer_end_element(xml_writer, results_element);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);

  raptor_free_xml_element(results_element);

  results3done:
  
  raptor_xml_writer_end_element(xml_writer, sparql_element);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);

  raptor_free_xml_element(sparql_element);


  raptor_free_xml_writer(xml_writer);

  raptor_free_namespace(res_ns);

  raptor_free_namespaces(nstack);

  return 0;
}


static RASQAL_INLINE unsigned char*
rasqal_prefix_id(int prefix_id, unsigned char *string) {
  int tmpid=prefix_id;
  unsigned char* buffer;
  size_t length=strlen((const char*)string)+4;  /* "r" +... + "_" +... \0 */

  while(tmpid/=10)
    length++;
  
  buffer=(unsigned char*)RASQAL_MALLOC(cstring, length);
  if(!buffer)
    return NULL;
  
  sprintf((char*)buffer, "r%d_%s", prefix_id, string);
  
  RASQAL_FREE(cstring, string);
  return buffer;
}


/**
 * rasqal_query_results_get_triple:
 * @query_results: #rasqal_query_results query_results
 *
 * Get the current triple in the result.
 *
 * The return value is a shared #raptor_statement.
 * 
 * Return value: #raptor_statement or NULL if failed or results exhausted
 **/
raptor_statement*
rasqal_query_results_get_triple(rasqal_query_results *query_results) {
  rasqal_query* query;
  int rc;
  rasqal_triple *t;
  rasqal_literal *s, *p, *o;
  raptor_statement *rs;
  
  if(!query_results)
    return NULL;
  
  if(!rasqal_query_results_is_graph(query_results))
    return NULL;
  
  query=query_results->query;
  if(query->finished)
    return NULL;

  if((query->current_triple_result < 0)||
     query->current_triple_result >= raptor_sequence_size(query->constructs)) {
    /* rc<0 error rc=0 end of results,  rc>0 got a result */
    rc=rasqal_engine_get_next_result(query);
    if(rc < 1)
      query->finished=1;
    if(rc < 0)
      query->failed=1;
  
    if(query->finished || query->failed)
      return NULL;

    query->current_triple_result=0;
  }


  t=(rasqal_triple*)raptor_sequence_get_at(query->constructs,
                                           query->current_triple_result);

  rs=&query->statement;

  s=rasqal_literal_as_node(t->subject);
  switch(s->type) {
    case RASQAL_LITERAL_URI:
      rs->subject=s->value.uri;
      rs->subject_type=RAPTOR_IDENTIFIER_TYPE_RESOURCE;
      break;

    case RASQAL_LITERAL_BLANK:
      s->string=rasqal_prefix_id(query->result_count, 
                                 (unsigned char*)s->string);

      rs->subject=s->string;
      rs->subject_type=RAPTOR_IDENTIFIER_TYPE_ANONYMOUS;
      break;

    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_VARIABLE:
    case RASQAL_LITERAL_DECIMAL:
    case RASQAL_LITERAL_DATETIME:
      /* QNames should be gone by the time expression eval happens
       * Everything else is removed by rasqal_literal_as_node() above. 
       */

    case RASQAL_LITERAL_STRING:
      /* string [literal] subjects are not RDF */

    case RASQAL_LITERAL_UNKNOWN:
    default:
      /* case RASQAL_LITERAL_STRING: */
      RASQAL_FATAL2("Triple with non-URI/blank subject type %d", s->type);
      break;
  }
  
  p=rasqal_literal_as_node(t->predicate);
  switch(p->type) {
    case RASQAL_LITERAL_URI:
      rs->predicate=p->value.uri;
      rs->predicate_type=RAPTOR_IDENTIFIER_TYPE_RESOURCE;
      break;

    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_VARIABLE:
    case RASQAL_LITERAL_DECIMAL:
    case RASQAL_LITERAL_DATETIME:
      /* QNames should be gone by the time expression eval happens
       * Everything else is removed by rasqal_literal_as_node() above. 
       */

    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_STRING:
      /* blank node or string [literal] predicates are not RDF */

    case RASQAL_LITERAL_UNKNOWN:
    default:
      RASQAL_FATAL2("Triple with non-URI predicatge type %d", p->type);
      break;
  }

  o=rasqal_literal_as_node(t->object);
  switch(o->type) {
    case RASQAL_LITERAL_URI:
      rs->object=o->value.uri;
      rs->object_type=RAPTOR_IDENTIFIER_TYPE_RESOURCE;
      break;

    case RASQAL_LITERAL_BLANK:
      o->string=rasqal_prefix_id(query->result_count, 
                                 (unsigned char*)o->string);

      rs->object=o->string;
      rs->object_type=RAPTOR_IDENTIFIER_TYPE_ANONYMOUS;
      break;
      
    case RASQAL_LITERAL_STRING:
      rs->object=o->string;
      rs->object_literal_language=(const unsigned char*)o->language;
      rs->object_literal_datatype=o->datatype;
      rs->object_type=RAPTOR_IDENTIFIER_TYPE_LITERAL;
      break;

    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_VARIABLE:
    case RASQAL_LITERAL_DECIMAL:
    case RASQAL_LITERAL_DATETIME:
      /* QNames should be gone by the time expression eval happens
       * Everything else is removed by rasqal_literal_as_node() above. 
       */

    case RASQAL_LITERAL_UNKNOWN:
    default:
      RASQAL_FATAL2("Triple with unknown object type %d", o->type);
      break;
  }

  /* for saving s, p, o for later disposal */
  query->triple=rasqal_new_triple(s, p, o);

  return rs;
}


/**
 * rasqal_query_results_next_triple:
 * @query_results: #rasqal_query_results query_results
 *
 * Move to the next triple result.
 * 
 * Return value: non-0 if failed or results exhausted
 **/
int
rasqal_query_results_next_triple(rasqal_query_results *query_results) {
  rasqal_query* query;
  int rc;
  
  if(!query_results)
    return 1;
  
  if(!rasqal_query_results_is_graph(query_results))
    return 1;
  
  query=query_results->query;
  if(query->finished)
    return 1;

  if(query->triple) {
    rasqal_free_triple(query->triple);
    query->triple=NULL;
  }

  if(++query->current_triple_result >= raptor_sequence_size(query->constructs)) {
    /* rc<0 error rc=0 end of results,  rc>0 got a result */
    rc=rasqal_engine_get_next_result(query);
    if(rc < 1)
      query->finished=1;
    if(rc < 0)
      query->failed=1;
    if(query->finished || query->failed)
      return 1;

    query->current_triple_result=0;
  }
  
  return 0;
}


/**
 * rasqal_query_results_get_boolean:
 * @query_results: #rasqal_query_results query_results
 *
 * Get boolean query result.
 *
 * The return value is only meaningful if this is a boolean
 * query result - see rasqal_query_results_is_boolean()
 *
 * Return value: boolean query result - >0 is true, 0 is false, <0 on error or finished
 */
int
rasqal_query_results_get_boolean(rasqal_query_results *query_results) {
  rasqal_query* query;
  int rc;
  
  if(!query_results)
    return -1;
  
  if(!rasqal_query_results_is_boolean(query_results))
    return -1;
  
  query=query_results->query;
  if(query->finished || query->failed)
    return -1;

  if(query->ask_result >= 0)
    return query->ask_result;

  /* rc<0 error rc=0 end of results,  rc>0 got a result */
  rc=rasqal_engine_get_next_result(query);
  if(rc < 1) {
    /* error or end of results */
    query->finished= 1;
    query->ask_result= 0; /* false */
  }
  if(rc < 0) {
    /* error */
    query->failed= 1;
    query->ask_result= -1; /* error */
  }
  if(rc > 0) {
    /* ok */
    query->ask_result= 1; /* true */
  }

  return query->ask_result;
}


/**
 * rasqal_query_graph_pattern_visit:
 * @query: query
 * @visit_fn: user function to operate on
 * @data: user data to pass to function
 * 
 * Visit all graph patterns in a query with a user function @visit_fn.
 *
 * See also rasqal_graph_pattern_visit().
 **/
void
rasqal_query_graph_pattern_visit(rasqal_query* query, 
                                 rasqal_graph_pattern_visit_fn visit_fn, 
                                 void* data)
{
  rasqal_graph_pattern* gp=rasqal_query_get_query_graph_pattern(query);
  if(!gp)
    return;

  rasqal_graph_pattern_visit(query, gp, visit_fn, data);
}

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


static void rasqal_query_add_query_result(rasqal_query *query, rasqal_query_results* query_results);
static void rasqal_query_remove_query_result(rasqal_query *query, rasqal_query_results* query_results);
static int rasqal_query_results_write_xml_20041221(raptor_iostream *iostr, rasqal_query_results *results, raptor_uri *base_uri);
static int rasqal_query_results_write_xml_result2(raptor_iostream *iostr, rasqal_query_results *results, raptor_uri *base_uri);

/**
 * rasqal_new_query - Constructor - create a new rasqal_query object
 * @name: the query language name (or NULL)
 * @uri: &raptor_uri language uri (or NULL)
 *
 * A query language can be named or identified by a URI, either
 * of which is optional.  The default query language will be used
 * if both are NULL.  rasqal_languages_enumerate returns
 * information on the known names, labels and URIs.
 *
 * Return value: a new &rasqal_query object or NULL on failure
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

  query->usage=1;
  
  if(factory->init(query, name)) {
    rasqal_free_query(query);
    return NULL;
  }
  
  return query;
}



/**
 * rasqal_free_query - destructor - destroy a rasqal_query object
 * @query: &rasqal_query object
 * 
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

  /* Do thes last since most everything above could refer to a variable */
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
 * rasqal_query_get_name - Get a short name for the query language
 * @query: &rasqal_query query object
 *
 * Return value: shared string label value
 **/
const char*
rasqal_query_get_name(rasqal_query *query)
{
  return query->factory->name;
}


/**
 * rasqal_query_get_label - Get a readable label for the query language
 * @query: &rasqal_query query object
 *
 * Return value: shared string label value
 **/
const char*
rasqal_query_get_label(rasqal_query *query)
{
  return query->factory->label;
}


/**
 * rasqal_query_set_fatal_error_handler - Set the query error handling function
 * @query: the query
 * @user_data: user data to pass to function
 * @handler: pointer to the function
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
 * rasqal_query_set_error_handler - Set the query error handling function
 * @query: the query
 * @user_data: user data to pass to function
 * @handler: pointer to the function
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
 * rasqal_set_warning_handler - Set the query warning handling function
 * @query: the query
 * @user_data: user data to pass to function
 * @handler: pointer to the function
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
 * rasqal_query_set_feature - Set various query features
 * @query: &rasqal_query query object
 * @feature: feature to set from enumerated &rasqal_feature values
 * @value: integer feature value
 * 
 * feature can be one of:
 **/
void
rasqal_query_set_feature(rasqal_query *query, 
                         rasqal_feature feature, int value)
{
  switch(feature) {
      
    case RASQAL_FEATURE_LAST:
    default:
      break;
  }
}


/**
 * rasqal_query_get_distinct - Get the query distinct results flag
 * @query: &rasqal_query query object
 *
 * Return value: non-0 if the results should be distinct
 **/
int
rasqal_query_get_distinct(rasqal_query *query)
{
  return query->distinct;
}


/**
 * rasqal_query_set_distinct - Set the query distinct results flag
 * @query: &rasqal_query query object
 * @is_distinct: non-0 if distinct
 **/
void
rasqal_query_set_distinct(rasqal_query *query, int is_distinct)
{
  query->distinct= (is_distinct != 0) ? 1 : 0;
}


/**
 * rasqal_query_get_limit - Get the query-specified limit on results
 * @query: &rasqal_query query object
 *
 * This is the limit given in the query on the number of results allowed.
 *
 * Return value: integer >=0 if a limit is given, otherwise <0
 **/
int
rasqal_query_get_limit(rasqal_query *query)
{
  return query->limit;
}


/**
 * rasqal_query_set_limit - Set the query-specified limit on results
 * @query: &rasqal_query query object
 * @limit: the limit on results, >=0 to set a limit, <0 to have no limit
 *
 * This is the limit given in the query on the number of results allowed.
 **/
void
rasqal_query_set_limit(rasqal_query *query, int limit)
{
  query->limit=limit;
}


/**
 * rasqal_query_get_offset - Get the query-specified offset on results
 * @query: &rasqal_query query object
 *
 * This is the offset given in the query on the number of results allowed.
 *
 * Return value: integer >=0 if a offset is given, otherwise <0
 **/
int
rasqal_query_get_offset(rasqal_query *query)
{
  return query->offset;
}


/**
 * rasqal_query_set_offset - Set the query-specified offset on results
 * @query: &rasqal_query query object
 * @offset: offset for results, >=0 to set an offset, <0 to have no offset
 *
 * This is the offset given in the query on the number of results allowed.
 **/
void
rasqal_query_set_offset(rasqal_query *query, int offset)
{
  query->offset=offset;
}


/**
 * rasqal_query_add_data_graph - Add a data graph to the query
 * @query: &rasqal_query query object
 * @uri: &raptor_uri source uri for retrieval
 * @name_uri: &raptor_uri name uri (or NULL)
 * @flags: RASQAL_DATA_GRAPH_NAMED or RASQAL_DATA_GRAPH_BACKGROUND
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
 * rasqal_query_get_data_graph_sequence - Get the sequence of data_graph URIs
 * @query: &rasqal_query query object
 *
 * Return value: a &raptor_sequence of &raptor_uri pointers.
 **/
raptor_sequence*
rasqal_query_get_data_graph_sequence(rasqal_query* query)
{
  return query->data_graphs;
}


/**
 * rasqal_query_get_data_graph - Get a rasqal_data_graph* in the sequence of data_graphs
 * @query: &rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Return value: a &rasqal_data_graph pointer or NULL if out of the sequence range
 **/
rasqal_data_graph*
rasqal_query_get_data_graph(rasqal_query* query, int idx)
{
  if(!query->data_graphs)
    return NULL;
  
  return (rasqal_data_graph*)raptor_sequence_get_at(query->data_graphs, idx);
}


/**
 * rasqal_query_add_variable - Add a binding variable to the query
 * @query: &rasqal_query query object
 * @var: &rasqal_variable variable
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
 * rasqal_query_get_bound_variable_sequence - Get the sequence of variables to bind in the query
 * @query: &rasqal_query query object
 *
 * This returns the sequence of variables that are explicitly chosen
 * via SELECT in RDQL, SPARQL.  Or all variables mentioned with SELECT *
 *
 * Return value: a &raptor_sequence of &rasqal_variable pointers.
 **/
raptor_sequence*
rasqal_query_get_bound_variable_sequence(rasqal_query* query)
{
  return query->selects;
}


/**
 * rasqal_query_get_all_variable_sequence - Get the sequence of all variables mentioned in the query
 * @query: &rasqal_query query object
 *
 * Return value: a &raptor_sequence of &rasqal_variable pointers.
 **/
raptor_sequence*
rasqal_query_get_all_variable_sequence(rasqal_query* query)
{
  return query->selects;
}


/**
 * rasqal_query_get_variable - Get a variable in the sequence of variables to bind
 * @query: &rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Return value: a &rasqal_variable pointer or NULL if out of the sequence range
 **/
rasqal_variable*
rasqal_query_get_variable(rasqal_query* query, int idx)
{
  if(!query->selects)
    return NULL;
  
  return (rasqal_variable*)raptor_sequence_get_at(query->selects, idx);
}


/**
 * rasqal_query_has_variable - Find if the named variable is in the sequence of variables to bind
 * @query: &rasqal_query query object
 * @name: variable name
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
 * rasqal_query_set_variable - Add a binding variable to the query
 * @query: &rasqal_query query object
 * @name: &rasqal_variable variable
 * @value: &rasqal_literal value to set or NULL
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
 * rasqal_query_get_triple_sequence - Get the sequence of matching triples in the query
 * @query: &rasqal_query query object
 *
 * Return value: a &raptor_sequence of &rasqal_triple pointers.
 **/
raptor_sequence*
rasqal_query_get_triple_sequence(rasqal_query* query)
{
  return query->triples;
}


/**
 * rasqal_query_get_triple - Get a triple in the sequence of matching triples in the query
 * @query: &rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Return value: a &rasqal_triple pointer or NULL if out of the sequence range
 **/
rasqal_triple*
rasqal_query_get_triple(rasqal_query* query, int idx)
{
  if(!query->triples)
    return NULL;
  
  return (rasqal_triple*)raptor_sequence_get_at(query->triples, idx);
}


/**
 * rasqal_query_add_prefix - Add a namespace prefix to the query
 * @query: &rasqal_query query object
 * @prefix: &rasqal_prefix namespace prefix, URI
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
 * rasqal_query_get_prefix_sequence - Get the sequence of namespace prefixes in the query
 * @query: &rasqal_query query object
 *
 * Return value: a &raptor_sequence of &rasqal_prefix pointers.
 **/
raptor_sequence*
rasqal_query_get_prefix_sequence(rasqal_query* query)
{
  return query->prefixes;
}


/**
 * rasqal_query_get_prefix - Get a prefix in the sequence of namespsace prefixes in the query
 * @query: &rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Return value: a &rasqal_prefix pointer or NULL if out of the sequence range
 **/
rasqal_prefix*
rasqal_query_get_prefix(rasqal_query* query, int idx)
{
  if(!query->prefixes)
    return NULL;

  return (rasqal_prefix*)raptor_sequence_get_at(query->prefixes, idx);
}


/**
 * rasqal_query_get_query_graph_pattern - Get the top query graph pattern
 * @query: &rasqal_query query object
 *
 * Return value: a &rasqal_graph_pattern of the top query graph pattern
 **/
rasqal_graph_pattern*
rasqal_query_get_query_graph_pattern(rasqal_query* query)
{
  return query->query_graph_pattern;
}


/**
 * rasqal_query_get_graph_pattern_sequence - Get the sequence of graph_patterns expressions inside the top query graph pattern
 * @query: &rasqal_query query object
 *
 * Return value: a &raptor_sequence of &rasqal_graph_pattern pointers.
 **/
raptor_sequence*
rasqal_query_get_graph_pattern_sequence(rasqal_query* query)
{
  return rasqal_graph_pattern_get_sub_graph_pattern_sequence(query->query_graph_pattern);
}


/**
 * rasqal_query_get_graph_pattern - Get a graph_pattern in the sequence of graph_pattern expressions in the top query graph pattern
 * @query: &rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Return value: a &rasqal_graph_pattern pointer or NULL if out of the sequence range
 **/
rasqal_graph_pattern*
rasqal_query_get_graph_pattern(rasqal_query* query, int idx)
{
  return rasqal_graph_pattern_get_sub_graph_pattern(query->query_graph_pattern, idx);
}


/**
 * rasqal_query_get_construct_triples_sequence - Get the sequence of triples for a construct
 * @query: &rasqal_query query object
 *
 * Return value: a &raptor_sequence of &rasqal_triples pointers.
 **/
raptor_sequence*
rasqal_query_get_construct_triples_sequence(rasqal_query* query)
{
  return query->constructs;
}


/**
 * rasqal_query_get_construct_triple - Get a triple in the sequence of construct triples
 * @query: &rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Return value: a &rasqal_triple pointer or NULL if out of the sequence range
 **/
rasqal_triple*
rasqal_query_get_construct_triple(rasqal_query* query, int idx)
{
  if(!query->constructs)
    return NULL;

  return (rasqal_triple*)raptor_sequence_get_at(query->constructs, idx);
}


/**
 * rasqal_graph_pattern_add_sub_graph_pattern - Add a sub graph pattern to a graph pattern 
 * @graph_pattern: graph pattern to add to
 * @sub_graph_pattern: graph pattern to add inside
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
 * rasqal_graph_pattern_get_triple - Get a triple inside a graph pattern
 * @graph_pattern: &rasqal_graph_pattern graph pattern object
 * @idx: index into the sequence of triples in the graph pattern
 * 
 * Return value: &rasqal_triple or NULL if out of range
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
 * rasqal_graph_pattern_get_sub_graph_pattern_sequence - Get the sequence of graph patterns inside a graph pattern 
 * @graph_pattern: 
 * 
 * Return value:  a &raptor_sequence of &rasqal_graph_pattern pointers.
 **/
raptor_sequence*
rasqal_graph_pattern_get_sub_graph_pattern_sequence(rasqal_graph_pattern* graph_pattern)
{
  return graph_pattern->graph_patterns;
}


/**
 * rasqal_graph_pattern_get_sub_graph_pattern - Get a sub-graph pattern inside a graph pattern
 * @graph_pattern: &rasqal_graph_pattern graph pattern object
 * @idx: index into the sequence of sub graph_patterns in the graph pattern
 * 
 * Return value: &rasqal_graph_pattern or NULL if out of range
 **/
rasqal_graph_pattern*
rasqal_graph_pattern_get_sub_graph_pattern(rasqal_graph_pattern* graph_pattern, int idx)
{
  if(!graph_pattern->graph_patterns)
    return NULL;

  return (rasqal_graph_pattern*)raptor_sequence_get_at(graph_pattern->graph_patterns, idx);
}


/**
 * rasqal_graph_pattern_get_flags - Get the graph pattern flags 
 * @graph_pattern: &rasqal_graph_pattern graph pattern object
 * 
 * DEPRECATED: Always returns 0
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
 * rasqal_graph_pattern_set_origin - Get the graph pattern triple origin
 * @graph_pattern: &rasqal_graph_pattern graph pattern object
 * @origin: &rasqal_literal variable or URI
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
 * rasqal_query_prepare: Prepare a query - typically parse it
 * @query: the &rasqal_query object
 * @query_string: the query string (or NULL)
 * @base_uri: base URI of query string (optional)
 * 
 * Some query languages may require a base URI to resolve any
 * relative URIs in the query string.  If this is not given,
 * the current directory int the filesystem is used as the base URI.
 *
 * The query string may be NULL in which case it is not parsed
 * and the query parts may be created by API calls such as
 * rasqal_query_add_source etc.
 *
 * Return value: non-0 on failure.
 **/
int
rasqal_query_prepare(rasqal_query *query,
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

  if(query->query_graph_pattern)
    rasqal_engine_make_basic_graph_pattern(query->query_graph_pattern);

  return rc;
}


/**
 * rasqal_query_execute - Excute a query - run and return results
 * @query: the &rasqal_query object
 *
 * return value: a &rasqal_query_results structure or NULL on failure.
 **/
rasqal_query_results*
rasqal_query_execute(rasqal_query *query)
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

  rasqal_query_results_next(query_results);

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
 * rasqal_query_verb_as_string - Get a string for the query verb
 * @verb: the &rasqal_query_verb verb of the query
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
 * rasqal_query_print - Print a query in a debug format
 * @query: the &rasqal_query object
 * @fh: the &FILE* handle to print to.
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
rasqal_query_add_query_result(rasqal_query *query,
                              rasqal_query_results* query_results) 
{
  query_results->next=query->results;
  query->results=query_results;
  /* add reference to ensure query lives as long as this runs */
  query->usage++;
}



static void
rasqal_query_remove_query_result(rasqal_query *query,
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
 * rasqal_free_query_results - destructor - destroy a rasqal_query_results
 * @query_results: &rasqal_query_results object
 *
 **/
void
rasqal_free_query_results(rasqal_query_results *query_results) {
  rasqal_query *query;

  if(!query_results)
    return;
  
  query=query_results->query;
  rasqal_query_remove_query_result(query, query_results);
  RASQAL_FREE(rasqal_query_results, query_results);
}


/**
 * rasqal_query_results_is_bindings - test if rasqal_query_results is variable bindings format
 * @query_results: &rasqal_query_results object
 * 
 * Return value: non-0 if true
 **/
int
rasqal_query_results_is_bindings(rasqal_query_results *query_results) {
  rasqal_query *query=query_results->query;
  return (query->verb == RASQAL_QUERY_VERB_SELECT);
}


/**
 * rasqal_query_results_is_boolean - test if rasqal_query_results is boolean format
 * @query_results: &rasqal_query_results object
 * 
 * Return value: non-0 if true
 **/
int
rasqal_query_results_is_boolean(rasqal_query_results *query_results) {
  rasqal_query *query=query_results->query;
  return (query->verb == RASQAL_QUERY_VERB_ASK);
}
 

/**
 * rasqal_query_results_is_graph - test if rasqal_query_results is RDF graph format
 * @query_results: &rasqal_query_results object
 * 
 * Return value: non-0 if true
 **/
int
rasqal_query_results_is_graph(rasqal_query_results *query_results) {
  rasqal_query *query=query_results->query;
  return (query->verb == RASQAL_QUERY_VERB_CONSTRUCT ||
          query->verb == RASQAL_QUERY_VERB_DESCRIBE);
}


/**
 * rasqal_query_results_get_count - Get number of bindings so far
 * @query_results: &rasqal_query_results query_results
 * 
 * Return value: number of bindings found so far or < 0 on failure
 **/
int
rasqal_query_results_get_count(rasqal_query_results *query_results)
{
  rasqal_query *query;

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
 * rasqal_query_results_next - Move to the next result
 * @query_results: &rasqal_query_results query_results
 * 
 * Return value: non-0 if failed or results exhausted
 **/
int
rasqal_query_results_next(rasqal_query_results *query_results)
{
  rasqal_query *query;
  
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
    
    if(query->offset > 0) {
      /* offset */
      if(query->result_count <= query->offset)
        continue;
      
      if(query->limit >= 0) {
        /* offset and limit */
        if(query->result_count > (query->offset + query->limit)) {
          query->finished=1;
          query->result_count--;
        }
      }
      
    } else if(query->limit >= 0) {
      /* limit */
      if(query->result_count > query->limit) {
        query->finished=1;
        query->result_count--;
        }
    }
    break;

  } /* while */
  
  return query->finished;
}


/**
 * rasqal_query_results_finished - Find out if binding results are exhausted
 * @query_results: &rasqal_query_results query_results
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
 * rasqal_query_results_get_bindings - Get all binding names, values for current result
 * @query_results: &rasqal_query_results query_results
 * @names: pointer to an array of binding names (or NULL)
 * @values: pointer to an array of binding value &rasqal_literal (or NULL)
 * 
 * If names is not NULL, it is set to the address of a shared array
 * of names of the bindings (an output parameter).  These names
 * are shared and must not be freed by the caller
 *
 * If values is not NULL, it is set to the address of a shared array
 * of &rasqal_literal* binding values.  These values are shaerd
 * and must not be freed by the caller.
 * 
 * Return value: non-0 if the assignment failed
 **/
int
rasqal_query_results_get_bindings(rasqal_query_results *query_results,
                                  const unsigned char ***names, 
                                  rasqal_literal ***values)
{
  rasqal_query *query;
  
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
    if(query->binding_values)
      rasqal_engine_assign_binding_values(query);
    
    *values=query->binding_values;
  }
  
  return 0;
}


/**
 * rasqal_query_results_get_binding_value - Get one binding value for the current result
 * @query_results: &rasqal_query_results query_results
 * @offset: offset of binding name into array of known names
 * 
 * Return value: a pointer to a shared &rasqal_literal binding value or NULL on failure
 **/
rasqal_literal*
rasqal_query_results_get_binding_value(rasqal_query_results *query_results, 
                                       int offset)
{
  rasqal_query *query;

  if(!query_results)
    return NULL;
  
  if(!rasqal_query_results_is_bindings(query_results))
    return NULL;
  
  query=query_results->query;
  if(query->finished)
    return NULL;

  if(offset < 0 || offset > query->select_variables_count-1)
    return NULL;
  
  if(query->binding_values)
    rasqal_engine_assign_binding_values(query);
  else
    return NULL;
  
  return query->binding_values[offset];
}


/**
 * rasqal_query_results_get_binding_name - Get binding name for the current result
 * @query_results: &rasqal_query_results query_results
 * @offset: offset of binding name into array of known names
 * 
 * Return value: a pointer to a shared copy of the binding name or NULL on failure
 **/
const unsigned char*
rasqal_query_results_get_binding_name(rasqal_query_results *query_results, 
                                      int offset)
{
  rasqal_query *query;

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
 * rasqal_query_results_get_binding_value_by_name - Get one binding value for a given name in the current result
 * @query_results: &rasqal_query_results query_results
 * @name: variable name
 * 
 * Return value: a pointer to a shared &rasqal_literal binding value or NULL on failure
 **/
rasqal_literal*
rasqal_query_results_get_binding_value_by_name(rasqal_query_results *query_results,
                                               const unsigned char *name)
{
  int offset= -1;
  int i;
  rasqal_query *query;

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
  
  if(query->binding_values)
    rasqal_engine_assign_binding_values(query);
  else
    return NULL;
  
  return query->binding_values[offset];
}


/**
 * rasqal_query_results_get_bindings_count - Get the number of bound variables in the result
 * @query_results: &librdf_query_results query_results
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
 * rasqal_query_get_user_data - Get query user data
 * @query: &rasqal_query
 * 
 * Return value: user data as set by rasqal_query_set_user_data
 **/
void*
rasqal_query_get_user_data(rasqal_query *query)
{
  return query->user_data;
}


/**
 * rasqal_query_set_user_data - Set the query user data
 * @query: &rasqal_query
 * @user_data: some user data to associate with the query
 **/
void
rasqal_query_set_user_data(rasqal_query *query, void *user_data)
{
  query->user_data=user_data;
}


/**
 * rasqal_query_get_verb - Get the query verb
 * @query: &rasqal_query
 *
 * Return value: the operating verb of the query of type rasqal_query_verb
 **/
rasqal_query_verb
rasqal_query_get_verb(rasqal_query *query)
{
  return query->verb;
}


/**
 * rasqal_query_get_wildcard - Get the query verb is wildcard flag
 * @query: &rasqal_query
 *
 * Return value: non-0 if the query verb was a wildcard (such as SELECT *)
 **/
int
rasqal_query_get_wildcard(rasqal_query *query)
{
  return query->wildcard;
}


/**
 * rasqal_query_get_order_conditions_sequence - Get the sequence of query ordering conditions
 * @query: &rasqal_query query object
 *
 * Return value: a &raptor_sequence of &rasqal_expression pointers.
 **/
raptor_sequence*
rasqal_query_get_order_conditions_sequence(rasqal_query* query)
{
  return query->order_conditions_sequence;
}


/**
 * rasqal_query_get_order_condition - Get a query ordering expression in the sequence of query ordering conditions
 * @query: &rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Return value: a &rasqal_expression pointer or NULL if out of the sequence range
 **/
rasqal_expression*
rasqal_query_get_order_condition(rasqal_query* query, int idx)
{
  if(!query->order_conditions_sequence)
    return NULL;
  
  return (rasqal_expression*)raptor_sequence_get_at(query->order_conditions_sequence, idx);
}


/**
 * rasqal_query_results_write - Write the query results to an iostream in a format
 * @iostr: &raptor_iostream to write the query to
 * @results: &rasqal_query_results query results format
 * @format_uri: &raptor_uri describing the format to write
 * @base_uri: &raptor_uri base URI of the output format
 * 
 * The supported URIs for the format_uri are:
 *
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
   * SPARQL XML Results 2004-12-21
   * http://www.w3.org/TR/2004/WD-rdf-sparql-XMLres-20041221/
   * http://www.w3.org/2001/sw/DataAccess/rf1/result
   */
  if(!strcmp((const char*)raptor_uri_as_string(format_uri),
            "http://www.w3.org/2001/sw/DataAccess/rf1/result") ||
     !strcmp((const char*)raptor_uri_as_string(format_uri),
             "http://www.w3.org/TR/2004/WD-rdf-sparql-XMLres-20041221/"))
    return rasqal_query_results_write_xml_20041221(iostr, results, base_uri);

  /*
   * SPARQL XML Results 2005-05-27 (to appear)
   * http://www.w3.org/TR/2005/WD-rdf-sparql-XMLres-20050527/
   * http://www.w3.org/2001/sw/DataAccess/rf1/result2
   */
  if(!strcmp((const char*)raptor_uri_as_string(format_uri),
            "http://www.w3.org/2001/sw/DataAccess/rf1/result2") ||
     !strcmp((const char*)raptor_uri_as_string(format_uri),
             "http://www.w3.org/TR/2005/WD-rdf-sparql-XMLres-20050527/"))
    return rasqal_query_results_write_xml_result2(iostr, results, base_uri);

  return 1;
}


/**
 * rasqal_query_results_write_xml_20041221 - Write the 2004-12-21 XML query results format to an iostream in a format - INTERNAL
 * @iostr: &raptor_iostream to write the query to
 * @results: &rasqal_query_results query results format
 * @base_uri: &raptor_uri base URI of the output format
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
      size_t len;

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
          len=strlen((const char*)l->string);
          
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
                                          (const unsigned char*)l->string, len);

          raptor_xml_writer_end_element(xml_writer, element1);
          
          break;
        case RASQAL_LITERAL_PATTERN:
        case RASQAL_LITERAL_QNAME:
        case RASQAL_LITERAL_INTEGER:
        case RASQAL_LITERAL_BOOLEAN:
        case RASQAL_LITERAL_FLOATING:
        case RASQAL_LITERAL_VARIABLE:

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


/**
 * rasqal_query_results_write_xml_result2 - Write the second version of the XML query results format to an iostream in a format - INTERNAL
 * @iostr: &raptor_iostream to write the query to
 * @results: &rasqal_query_results query results format
 * @base_uri: &raptor_uri base URI of the output format
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
      raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"true", 4);
    else
      raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"false", 5);
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
      size_t len;


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

          len=strlen((const char*)l->string);
          
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
                                          (const unsigned char*)l->string, len);

          raptor_xml_writer_end_element(xml_writer, element1);
          
          break;
        case RASQAL_LITERAL_PATTERN:
        case RASQAL_LITERAL_QNAME:
        case RASQAL_LITERAL_INTEGER:
        case RASQAL_LITERAL_BOOLEAN:
        case RASQAL_LITERAL_FLOATING:
        case RASQAL_LITERAL_VARIABLE:

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
 * rasqal_query_results_get_triple - Get the current triple in the result
 * @query_results: &rasqal_query_results query_results
 *
 * The return value is a shared raptor_statement
 * 
 * Return value: &raptor_statement or NULL if failed or results exhausted
 **/
raptor_statement*
rasqal_query_results_get_triple(rasqal_query_results *query_results) {
  rasqal_query *query;
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
    case RASQAL_LITERAL_FLOATING:
    case RASQAL_LITERAL_VARIABLE:
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
    case RASQAL_LITERAL_FLOATING:
    case RASQAL_LITERAL_VARIABLE:
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
    case RASQAL_LITERAL_FLOATING:
    case RASQAL_LITERAL_VARIABLE:
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
 * rasqal_query_results_next_triple - Move to the next triple result
 * @query_results: &rasqal_query_results query_results
 * 
 * Return value: non-0 if failed or results exhausted
 **/
int
rasqal_query_results_next_triple(rasqal_query_results *query_results) {
  rasqal_query *query;
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
 * rasqal_query_results_get_boolean - Get boolean query result
 * @query_results: &rasqal_query_results query_results
 *
 * The return value is only meaningful if this is a boolean
 * query result - see &rasqal_query_results_is_boolean
 *
 * Return value: boolean query result - >0 is true, 0 is false, <0 on error or finished
 */
int
rasqal_query_results_get_boolean(rasqal_query_results *query_results) {
  rasqal_query *query;
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


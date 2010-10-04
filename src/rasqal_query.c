/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_query.c - Rasqal RDF Query
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


/*
 *
 * Query Class Internals
 *
 * This is the main Rasqal class for constructing RDF graph queries
 * from a syntax or by API, preparing them for execution with a query
 * execution and executing them to return a result set.
 *
 * Queries are constructed from a syntax in some query language
 * syntax and build an RDF query API structure based on triple
 * patterns, filter expressions, graph patterns above them operating
 * over a set of graphs.
 *
 * This class does not deal with manipulating result sets which are
 * handled by the #rasqal_query_results and methods on it although
 * rasqal_query_execute() does return a newly constructed result
 * object.
 *
 * It also does not deal with executing a query which is handled by
 * #rasqal_query_execution_factory instances that have their own
 * simpler API.
 *
 */

#define DEBUG_FH stderr

#if 0
#undef RASQAL_NO_GP_MERGE
#else
#define RASQAL_NO_GP_MERGE 1
#endif


static int rasqal_query_add_query_result(rasqal_query* query, rasqal_query_results* query_results);


/**
 * rasqal_new_query:
 * @world: rasqal_world object
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
rasqal_new_query(rasqal_world *world, const char *name,
                 const unsigned char *uri)
{
  rasqal_query_language_factory* factory;
  rasqal_query* query;
#ifndef HAVE_RAPTOR2_API
  const raptor_uri_handler *uri_handler;
  void *uri_context;
#endif

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);

  /* for compatibility with older binaries that do not call it */
  rasqal_world_open(world);

  factory = rasqal_get_query_language_factory(world, name, uri);
  if(!factory)
    return NULL;

  query = (rasqal_query*)RASQAL_CALLOC(rasqal_query, 1, sizeof(rasqal_query));
  if(!query)
    return NULL;
  
  /* set usage first to 1 so we can clean up with rasqal_free_query() on error */
  query->usage = 1;

  query->world = world;
  
  query->factory = factory;

  query->limit = -1;
  query->offset = -1;

  query->genid_counter = 1;

  query->context = (char*)RASQAL_CALLOC(rasqal_query_context, 1,
                                        factory->context_length);
  if(!query->context)
    goto tidy;
  
#ifdef HAVE_RAPTOR2_API
  query->namespaces = raptor_new_namespaces(world->raptor_world_ptr, 0);
#else
  raptor_uri_get_handler(&uri_handler, &uri_context);
  query->namespaces = raptor_new_namespaces(uri_handler, uri_context,
                                            (raptor_simple_message_handler)rasqal_query_simple_error,
                                            query,
                                            0);
#endif
  if(!query->namespaces)
    goto tidy;

  query->vars_table = rasqal_new_variables_table(query->world);
  if(!query->vars_table)
    goto tidy;

#ifdef HAVE_RAPTOR2_API
  query->triples = raptor_new_sequence((raptor_data_free_handler)rasqal_free_triple, (raptor_data_print_handler)rasqal_triple_print);
#else
  query->triples = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_triple, (raptor_sequence_print_handler*)rasqal_triple_print);
#endif
  if(!query->triples)
    goto tidy;

#ifdef HAVE_RAPTOR2_API
  query->prefixes = raptor_new_sequence((raptor_data_free_handler)rasqal_free_prefix, (raptor_data_print_handler)rasqal_prefix_print);
#else
  query->prefixes = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_prefix, (raptor_sequence_print_handler*)rasqal_prefix_print);
#endif
  if(!query->prefixes)
    goto tidy;

#ifdef HAVE_RAPTOR2_API
  query->data_graphs = raptor_new_sequence((raptor_data_free_handler)rasqal_free_data_graph, (raptor_data_print_handler)rasqal_data_graph_print);
#else
  query->data_graphs = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_data_graph, (raptor_sequence_print_handler*)rasqal_data_graph_print);
#endif
  if(!query->data_graphs)
    goto tidy;

#ifdef HAVE_RAPTOR2_API
  query->results = raptor_new_sequence((raptor_data_free_handler)rasqal_query_results_remove_query_reference, NULL);
#else
  query->results = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_query_results_remove_query_reference, NULL);
#endif
  if(!query->results)
    goto tidy;

  if(factory->init(query, name))
    goto tidy;
  
  return query;

  tidy:
  rasqal_free_query(query);
  return NULL;
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
  if(!query)
    return;
  
  if(--query->usage)
    return;
  
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
  if(query->results)
    raptor_free_sequence(query->results);

  if(query->variables_bound_in)
    RASQAL_FREE(intarray, query->variables_bound_in);

  if(query->variables_use_map)
    RASQAL_FREE(intarray, query->variables_use_map);

  if(query->query_graph_pattern)
    rasqal_free_graph_pattern(query->query_graph_pattern);

  if(query->order_conditions_sequence)
    raptor_free_sequence(query->order_conditions_sequence);

  if(query->group_conditions_sequence)
    raptor_free_sequence(query->group_conditions_sequence);

  if(query->having_conditions_sequence)
    raptor_free_sequence(query->having_conditions_sequence);

  if(query->graph_patterns_sequence)
    raptor_free_sequence(query->graph_patterns_sequence);

  if(query->query_results_formatter_name)
    RASQAL_FREE(cstring, query->query_results_formatter_name);

  /* Do this last since most everything above could refer to a variable */
  if(query->vars_table)
    rasqal_free_variables_table(query->vars_table);

  if(query->updates)
    raptor_free_sequence(query->updates);
  
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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  return query->factory->desc.names[0];
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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  return query->factory->desc.label;
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
#ifdef HAVE_RAPTOR2_API
#else
  raptor_error_handlers* error_handlers;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN(query, rasqal_query);

  if(!query->world)
    return;

  error_handlers = &query->world->error_handlers;
  
  error_handlers->handlers[RAPTOR_LOG_LEVEL_FATAL].user_data = user_data;
  error_handlers->handlers[RAPTOR_LOG_LEVEL_FATAL].handler   = handler;
#endif
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
#ifdef HAVE_RAPTOR2_API
#else
  raptor_error_handlers* error_handlers;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN(query, rasqal_query);

  if(!query->world)
    return;

  error_handlers = &query->world->error_handlers;
  
  error_handlers->handlers[RAPTOR_LOG_LEVEL_ERROR].user_data = user_data;
  error_handlers->handlers[RAPTOR_LOG_LEVEL_ERROR].handler   = handler;
#endif
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
#ifdef HAVE_RAPTOR2_API
#else
  raptor_error_handlers* error_handlers;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN(query, rasqal_query);

  if(!query->world)
    return;

  error_handlers = &query->world->error_handlers;
  
  error_handlers->handlers[RAPTOR_LOG_LEVEL_WARN].user_data = user_data;
  error_handlers->handlers[RAPTOR_LOG_LEVEL_WARN].handler   = handler;
#endif
}


/**
 * rasqal_query_set_feature:
 * @query: #rasqal_query query object
 * @feature: feature to set from enumerated #rasqal_feature values
 * @value: integer feature value
 *
 * Set various query features.
 * 
 * The allowed features are available via rasqal_features_enumerate().
 *
 * Return value: non 0 on failure or if the feature is unknown
 **/
int
rasqal_query_set_feature(rasqal_query* query, rasqal_feature feature, int value)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);

  switch(feature) {
    case RASQAL_FEATURE_NO_NET:
      query->features[(int)feature] = value;
      break;
      
    default:
      break;
  }

  return 0;
}


/**
 * rasqal_query_set_feature_string:
 * @query: #rasqal_query query object
 * @feature: feature to set from enumerated #rasqal_feature values
 * @value: feature value
 *
 * Set query features with string values.
 * 
 * The allowed features are available via rasqal_features_enumerate().
 * If the feature type is integer, the value is interpreted as an integer.
 *
 * Return value: non 0 on failure or if the feature is unknown
 **/
int
rasqal_query_set_feature_string(rasqal_query *query, 
                                rasqal_feature feature, 
                                const unsigned char *value)
{
  int value_is_string = (rasqal_feature_value_type(feature) == 1);

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);

  if(!value_is_string)
    return rasqal_query_set_feature(query, feature, atoi((const char*)value));

  return -1;
}


/**
 * rasqal_query_get_feature:
 * @query: #rasqal_query query object
 * @feature: feature to get value
 *
 * Get various query features.
 * 
 * The allowed features are available via rasqal_features_enumerate().
 *
 * Note: no feature value is negative
 *
 * Return value: feature value or < 0 for an illegal feature
 **/
int
rasqal_query_get_feature(rasqal_query *query, rasqal_feature feature)
{
  int result= -1;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);

  switch(feature) {
    case RASQAL_FEATURE_NO_NET:
      result = (query->features[(int)feature] != 0);
      break;

    default:
      break;
  }
  
  return result;
}


/**
 * rasqal_query_get_feature_string:
 * @query: #rasqal_query query object
 * @feature: feature to get value
 *
 * Get query features with string values.
 * 
 * The allowed features are available via rasqal_features_enumerate().
 * If a string is returned, it must be freed by the caller.
 *
 * Return value: feature value or NULL for an illegal feature or no value
 **/
const unsigned char *
rasqal_query_get_feature_string(rasqal_query *query, 
                                rasqal_feature feature)
{
  int value_is_string = (rasqal_feature_value_type(feature) == 1);

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  if(!value_is_string)
    return NULL;
  
  return NULL;
}


/**
 * rasqal_query_get_distinct:
 * @query: #rasqal_query query object
 *
 * Get the query distinct mode
 *
 * See rasqal_query_set_distinct() for the distinct modes.
 *
 * Return value: non-0 if the results should be distinct
 **/
int
rasqal_query_get_distinct(rasqal_query* query)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 0);

  return query->distinct;
}


/**
 * rasqal_query_set_distinct:
 * @query: #rasqal_query query object
 * @distinct_mode: distinct mode
 *
 * Set the query distinct results mode.
 *
 * The allowed @distinct_mode values are:
 * 0 if not given
 * 1 if DISTINCT: ensure solutions are unique
 * 2 if SPARQL REDUCED: permit elimination of some non-unique solutions 
 *
 **/
void
rasqal_query_set_distinct(rasqal_query* query, int distinct_mode)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(query, rasqal_query);

  if(distinct_mode >= 0 && distinct_mode <= 2)
    query->distinct= distinct_mode;
  else
    query->distinct= 0;
}


/**
 * rasqal_query_get_explain:
 * @query: #rasqal_query query object
 *
 * Get the query explain results flag.
 *
 * Return value: non-0 if the results should be explain
 **/
int
rasqal_query_get_explain(rasqal_query* query)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 0);

  return query->explain;
}


/**
 * rasqal_query_set_explain:
 * @query: #rasqal_query query object
 * @is_explain: non-0 if explain
 *
 * Set the query explain results flag.
 *
 **/
void
rasqal_query_set_explain(rasqal_query* query, int is_explain)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(query, rasqal_query);

  query->explain = (is_explain != 0) ? 1 : 0;
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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 0);

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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(query, rasqal_query);

  query->limit = limit;
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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 0);

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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(query, rasqal_query);

  query->offset = offset;
}


/**
 * rasqal_query_add_data_graph2:
 * @query: #rasqal_query query object
 * @uri: #raptor_uri source uri for retrieval
 * @name_uri: #raptor_uri name uri (or NULL)
 * @flags: RASQAL_DATA_GRAPH_NAMED or RASQAL_DATA_GRAPH_BACKGROUND
 * @format_mime_type: MIME Type of data format at @uri (or NULL)
 * @format_name: Raptor parser Name of data format at @uri (or NULL)
 * @format_uri: URI of data format at @uri (or NULL)
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
rasqal_query_add_data_graph2(rasqal_query* query, 
                             raptor_uri* uri, raptor_uri* name_uri,
                             int flags, const char* format_type,
                             const char* format_name,
                             raptor_uri* format_uri)
{
  rasqal_data_graph *dg;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(uri, raptor_uri, 1);

  if((flags & RASQAL_DATA_GRAPH_NAMED) && !name_uri)
    return 1;
  
  dg = rasqal_new_data_graph2(query->world, uri, name_uri, flags,
                              format_type, format_name, format_uri);
  if(!dg)
    return 1;
  if(raptor_sequence_push(query->data_graphs, (void*)dg))
    return 1;
  return 0;
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
 * @Deprecated: Replaced by rasqal_query_add_data_graph2() with extra
 * format argumetns.
 *
 * Return value: non-0 on failure
 **/
int
rasqal_query_add_data_graph(rasqal_query* query, 
                            raptor_uri* uri, raptor_uri* name_uri,
                            int flags)
{
  return rasqal_query_add_data_graph2(query, uri, name_uri, flags,
                                      NULL, NULL, NULL);
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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  if(!query->data_graphs)
    return NULL;
  
  return (rasqal_data_graph*)raptor_sequence_get_at(query->data_graphs, idx);
}


/**
 * rasqal_query_dataset_contains_named_graph:
 * @query: #rasqal_query query object
 * @graph_uri: query URI
 *
 * Test if the query dataset contains a named graph
 *
 * Return value: non-0 if the dataset contains a named graph
 */
int
rasqal_query_dataset_contains_named_graph(rasqal_query* query,
                                          raptor_uri *graph_uri)
{
  rasqal_data_graph *dg;
  int idx;
  int found = 0;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(graph_uri, raptor_uri, 1);

  for(idx = 0; (dg = rasqal_query_get_data_graph(query, idx)); idx++) {
    if(dg->name_uri && raptor_uri_equals(dg->name_uri, graph_uri)) {
      /* graph_uri is a graph name in the dataset */
      found = 1;
      break;
    }
  }
  return found;
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
 *
 * Return value: non-0 on failure
 **/
int
rasqal_query_add_variable(rasqal_query* query, rasqal_variable* var)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(var, rasqal_variable, 1);

  if(!query->selects) {
#ifdef HAVE_RAPTOR2_API
    query->selects = raptor_new_sequence(NULL, (raptor_data_print_handler)rasqal_variable_print);
#else
    query->selects = raptor_new_sequence(NULL, (raptor_sequence_print_handler*)rasqal_variable_print);
#endif
    if(!query->selects)
      return 1;
  }

  return raptor_sequence_push(query->selects, (void*)var);
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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  return query->selects;
}


/**
 * rasqal_query_get_describe_sequence:
 * @query: #rasqal_query query object
 *
 * Get the sequence of literals described in the query.
 *
 * This returns the sequence of literals (constants or variables) that are
 * explicitly chosen via DESCRIBE in SPARQL.
 *
 * Return value: a #raptor_sequence of #rasqal_literal pointers.
 **/
raptor_sequence*
rasqal_query_get_describe_sequence(rasqal_query* query)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  return query->describes;
}


/**
 * rasqal_query_get_anonymous_variable_sequence:
 * @query: #rasqal_query query object
 *
 * Get the sequence of anonymous variables mentioned in the query.
 *
 * Return value: a #raptor_sequence of #rasqal_variable pointers.
 **/
raptor_sequence*
rasqal_query_get_anonymous_variable_sequence(rasqal_query* query)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  return rasqal_variables_table_get_anonymous_variables_sequence(query->vars_table);
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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  return rasqal_variables_table_get_named_variables_sequence(query->vars_table);
}


/**
 * rasqal_query_get_variable:
 * @query: #rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Get a variable in the sequence of variables to bind.
 *
 * Return value: a #rasqal_variable pointer or NULL if out of range
 **/
rasqal_variable*
rasqal_query_get_variable(rasqal_query* query, int idx)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  if(!query->selects || idx < 0 || idx >= query->select_variables_count)
    return NULL;
  
  return rasqal_variables_table_get(query->vars_table, idx);
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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 0);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(name, char*, 0);

  return rasqal_variables_table_has(query->vars_table, name);
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

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(name, char*, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(value, rasqal_literal, 1);

  if(!query->selects)
    return 1;
  
  for(i = 0; i< raptor_sequence_size(query->selects); i++) {
    rasqal_variable* v;
    v = (rasqal_variable*)raptor_sequence_get_at(query->selects, i);
    if(!strcmp((const char*)v->name, (const char*)name)) {
      if(v->value)
        rasqal_free_literal(v->value);
      v->value = value;
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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  if(!query->triples)
    return NULL;
  
  return (rasqal_triple*)raptor_sequence_get_at(query->triples, idx);
}


int
rasqal_query_declare_prefix(rasqal_query *rq, rasqal_prefix *p)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(rq, rasqal_query, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(p, rasqal_prefix, 1);

  if(p->declared)
    return 0;

  if(raptor_namespaces_start_namespace_full(rq->namespaces, 
                                            p->prefix, 
                                            raptor_uri_as_string(p->uri),
                                            rq->prefix_depth))
    return 1;
  p->declared = 1;
  rq->prefix_depth++;
  return 0;
}


static int
rasqal_query_undeclare_prefix(rasqal_query *rq, rasqal_prefix *prefix)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(rq, rasqal_query, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(prefix, rasqal_prefix, 1);

  if(!prefix->declared) {
    prefix->declared = 1;
    return 0;
  }
  
  raptor_namespaces_end_for_depth(rq->namespaces, prefix->depth);
  return 0;
}


int
rasqal_query_declare_prefixes(rasqal_query *rq) 
{
  int i;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(rq, rasqal_query, 1);

  if(!rq->prefixes)
    return 0;
  
  for(i = 0; i< raptor_sequence_size(rq->prefixes); i++) {
    rasqal_prefix* p = (rasqal_prefix*)raptor_sequence_get_at(rq->prefixes, i);
    if(rasqal_query_declare_prefix(rq, p))
      return 1;
  }

  return 0;
}


/**
 * rasqal_query_add_prefix:
 * @query: #rasqal_query query object
 * @prefix: #rasqal_prefix namespace prefix, URI
 *
 * Add a namespace prefix to the query.
 *
 * If the prefix has already been used, the old URI will be overridden.
 *
 * Return value: non-0 on failure
 **/
int
rasqal_query_add_prefix(rasqal_query* query, rasqal_prefix* prefix)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(prefix, rasqal_prefix, 1);

  if(!query->prefixes) {
#ifdef HAVE_RAPTOR2_API
    query->prefixes = raptor_new_sequence((raptor_data_free_handler)rasqal_free_prefix, (raptor_data_print_handler)rasqal_prefix_print);
#else
    query->prefixes = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_prefix, (raptor_sequence_print_handler*)rasqal_prefix_print);
#endif
    if(!query->prefixes)
      return 1;
  } else {
    int i;
    for(i = 0; i < raptor_sequence_size(query->prefixes); i++) {
      rasqal_prefix* p;
      p = (rasqal_prefix*)raptor_sequence_get_at(query->prefixes, i);

      if((!p->prefix && !prefix->prefix) ||
         ((p->prefix && prefix->prefix &&
          !strcmp((const char*)p->prefix, (const char*)prefix->prefix)))
        ) {
        rasqal_query_undeclare_prefix(query, p);
        break;
      }
    }
  }

  return raptor_sequence_push(query->prefixes, (void*)prefix);
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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  if(!query->constructs)
    return NULL;

  return (rasqal_triple*)raptor_sequence_get_at(query->constructs, idx);
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
  int rc = 0;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);

  if(query->failed)
    return 1;

  if(query->prepared)
    return 0;
  query->prepared = 1;

  if(query_string) {
    /* flex lexers require two NULs at the end of the lexed buffer.
     * Add them here instead of parser to allow resource cleanup on error.
     *  
     * flex manual:
     *
     * Function: YY_BUFFER_STATE yy_scan_buffer (char *base, yy_size_t size)
     * which scans in place the buffer starting at `base', consisting of
     * `size' bytes, the last two bytes of which _must_ be
     * `YY_END_OF_BUFFER_CHAR' (ASCII NUL).  These last two bytes are not
     * scanned; thus, scanning consists of `base[0]' through
     * `base[size-2]', inclusive.
     */
    int len = strlen((const char*)query_string)+3; /* +3 for " \0\0" */
    unsigned char *query_string_copy = (unsigned char*)RASQAL_MALLOC(cstring, len);
    if(!query_string_copy) {
      query->failed = 1;
      return 1;
    }
    strcpy((char*)query_string_copy, (const char*)query_string);
    query_string_copy[len-3] = ' ';
    query_string_copy[len-2] = query_string_copy[len-1] = '\0';
    query->query_string = query_string_copy;
    query->query_string_length = len;
  }

  if(base_uri)
    base_uri = raptor_uri_copy(base_uri);
  else {
    unsigned char *uri_string = raptor_uri_filename_to_uri_string("");
    base_uri = raptor_new_uri(query->world->raptor_world_ptr, uri_string);
    if(uri_string)
      raptor_free_memory(uri_string);
  }

  rasqal_query_set_base_uri(query, base_uri);
  query->locator.line = query->locator.column = query->locator.byte = -1;

  rc = query->factory->prepare(query);
  if(rc) {
    query->failed = 1;
    rc = 1;
  } else if(rasqal_query_prepare_common(query)) {
    query->failed = 1;
    rc = 1;
  }

  return rc;
}


/**
 * rasqal_query_get_engine_by_name:
 * @name: query engine name
 *
 * INTERNAL - Get a query engine by name
 *
 * If @name is NULL or the name is unknown, the default factory is returned
 *
 * return value: pointer to factory
 **/
const rasqal_query_execution_factory* 
rasqal_query_get_engine_by_name(const char* name)
{
  const rasqal_query_execution_factory* engine;

#if RASQAL_QUERY_ENGINE_VERSION == 1
  engine = &rasqal_query_engine_1;
#else
  engine = &rasqal_query_engine_algebra;
#endif

#ifdef RASQAL_DEBUG
  if(1) {
    char* n = getenv("RASQAL_DEBUG_ENGINE");
    if(n)
      name = n;
  }
#endif

  if(name) {
    if(!strcmp(name, "1") || !strcmp(name, "original"))
      engine = &rasqal_query_engine_1;
    else if(!strcmp(name, "2") || !strcmp(name, "algebra"))
      engine = &rasqal_query_engine_algebra;
  }

  return engine;
}


/**
 * rasqal_query_execute_with_engine:
 * @query: the #rasqal_query object
 * @engine: execution engine factory (or NULL)
 *
 * INTERNAL - Excecute a query with a given factory and return results.
 *
 * return value: a #rasqal_query_results structure or NULL on failure.
 **/
rasqal_query_results*
rasqal_query_execute_with_engine(rasqal_query* query,
                                 const rasqal_query_execution_factory* engine)
{
  rasqal_query_results *query_results = NULL;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  if(query->failed)
    return NULL;

  if(!engine)
    engine = rasqal_query_get_engine_by_name(NULL);

  query_results = rasqal_query_results_execute_with_engine(query, engine);
  if(query_results && rasqal_query_add_query_result(query, query_results)) {
    rasqal_free_query_results(query_results);
    query_results = NULL;      
  }

  return query_results;
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
  return rasqal_query_execute_with_engine(query, NULL);
}


static const char* const rasqal_query_verb_labels[RASQAL_QUERY_VERB_LAST+1] = {
  "Unknown",
  "SELECT",
  "CONSTRUCT",
  "DESCRIBE",
  "ASK",
  "DELETE",
  "INSERT",
  "UPDATE"
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
    verb = RASQAL_QUERY_VERB_UNKNOWN;

  return rasqal_query_verb_labels[(int)verb];
}
  

/**
 * rasqal_query_print:
 * @query: the #rasqal_query object
 * @fh: the FILE* handle to print to.
 *
 * Print a query in a debug format.
 * 
 * Return value: non-0 on failure
 **/
int
rasqal_query_print(rasqal_query* query, FILE *fh)
{
  rasqal_variables_table* vars_table = query->vars_table;
  raptor_sequence* seq;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(fh, FILE*, 1);

  fprintf(fh, "query verb: %s\n", rasqal_query_verb_as_string(query->verb));
  
  if(query->distinct)
    fprintf(fh, "query results distinct mode: %s\n",
            (query->distinct == 1 ? "distinct" : "reduced"));
  if(query->explain)
    fputs("query results explain: yes\n", fh);
  if(query->limit >= 0)
    fprintf(fh, "query results limit: %d\n", query->limit);
  if(query->offset >= 0)
    fprintf(fh, "query results offset: %d\n", query->offset);

  fputs("data graphs: ", fh);
  if(query->data_graphs)
    raptor_sequence_print(query->data_graphs, fh);
  seq = rasqal_variables_table_get_named_variables_sequence(vars_table);
  if(seq) {
    fputs("\nnamed variables: ", fh); 
    raptor_sequence_print(seq, fh);
  }
  seq = rasqal_variables_table_get_anonymous_variables_sequence(vars_table);
  if(seq) {
    fputs("\nanonymous variables: ", fh); 
    raptor_sequence_print(seq, fh);
  }
  if(query->selects) {
    fputs("\nbound variables: ", fh); 
    raptor_sequence_print(query->selects, fh);
  }
  if(query->describes) {
    fputs("\ndescribes: ", fh);
    raptor_sequence_print(query->describes, fh);
  }
  if(query->triples) {
    fputs("\ntriples: ", fh);
    raptor_sequence_print(query->triples, fh);
  }
  if(query->optional_triples) {
    fputs("\noptional triples: ", fh);
    raptor_sequence_print(query->optional_triples, fh);
  }
  if(query->constructs) {
    fputs("\nconstructs: ", fh);
    raptor_sequence_print(query->constructs, fh);
  }
  if(query->prefixes) {
    fputs("\nprefixes: ", fh);
    raptor_sequence_print(query->prefixes, fh);
  }
  if(query->query_graph_pattern) {
    fputs("\nquery graph pattern: ", fh);
    rasqal_graph_pattern_print(query->query_graph_pattern, fh);
  }
  if(query->order_conditions_sequence) {
    fputs("\nquery order conditions: ", fh);
    raptor_sequence_print(query->order_conditions_sequence, fh);
  }
  if(query->group_conditions_sequence) {
    fputs("\nquery group conditions: ", fh);
    raptor_sequence_print(query->group_conditions_sequence, fh);
  }
  if(query->having_conditions_sequence) {
    fputs("\nquery having conditions: ", fh);
    raptor_sequence_print(query->having_conditions_sequence, fh);
  }
  if(query->updates) {
    fputs("\nupdate operations: ", fh);
    raptor_sequence_print(query->updates, fh);
  }
  fputc('\n', fh);

  return 0;
}


static int
rasqal_query_add_query_result(rasqal_query* query,
                              rasqal_query_results* query_results) 
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, 1);

  /* add reference to ensure query lives as long as this runs */

  /* query->results sequence has rasqal_query_results_remove_query_reference()
     as the free handler which calls rasqal_free_query() decrementing
     query->usage */
  
  query->usage++;

  return raptor_sequence_push(query->results, query_results);
}



int
rasqal_query_remove_query_result(rasqal_query* query,
                                 rasqal_query_results* query_results) 
{
  int i;
  int size;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, 1);

  size = raptor_sequence_size(query->results);
  for(i = 0 ; i < size; i++) {
    rasqal_query_results *result;
    result = (rasqal_query_results*)raptor_sequence_get_at(query->results, i);

    if(result == query_results) {
      raptor_sequence_set_at(query->results, i, NULL);
      break;
    }
  }

  return 0;
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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(query, rasqal_query);

  query->user_data = user_data;
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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, RASQAL_QUERY_VERB_UNKNOWN);

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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 0);

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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  if(!query->order_conditions_sequence)
    return NULL;
  
  return (rasqal_expression*)raptor_sequence_get_at(query->order_conditions_sequence, idx);
}


/**
 * rasqal_query_get_group_conditions_sequence:
 * @query: #rasqal_query query object
 *
 * Get the sequence of query grouping conditions.
 *
 * Return value: a #raptor_sequence of #rasqal_expression pointers.
 **/
raptor_sequence*
rasqal_query_get_group_conditions_sequence(rasqal_query* query)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  return query->group_conditions_sequence;
}


/**
 * rasqal_query_get_group_condition:
 * @query: #rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Get a query grouping expression in the sequence of query grouping conditions.
 *
 * Return value: a #rasqal_expression pointer or NULL if out of the sequence range
 **/
rasqal_expression*
rasqal_query_get_group_condition(rasqal_query* query, int idx)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  if(!query->group_conditions_sequence)
    return NULL;
  
  return (rasqal_expression*)raptor_sequence_get_at(query->group_conditions_sequence, idx);
}


/**
 * rasqal_query_get_having_conditions_sequence:
 * @query: #rasqal_query query object
 *
 * Get the sequence of query having conditions.
 *
 * Return value: a #raptor_sequence of #rasqal_expression pointers.
 **/
raptor_sequence*
rasqal_query_get_having_conditions_sequence(rasqal_query* query)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  return query->having_conditions_sequence;
}


/**
 * rasqal_query_get_having_condition:
 * @query: #rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Get a query having expression in the sequence of query havinging conditions.
 *
 * Return value: a #rasqal_expression pointer or NULL if out of the sequence range
 **/
rasqal_expression*
rasqal_query_get_having_condition(rasqal_query* query, int idx)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  if(!query->having_conditions_sequence)
    return NULL;
  
  return (rasqal_expression*)raptor_sequence_get_at(query->having_conditions_sequence, idx);
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
  rasqal_graph_pattern* gp;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN(query, rasqal_query);

  gp = rasqal_query_get_query_graph_pattern(query);
  if(!gp)
    return;

  rasqal_graph_pattern_visit(query, gp, visit_fn, data);
}


/**
 * rasqal_query_write:
 * @iostr: #raptor_iostream to write the query to
 * @query: #rasqal_query pointer.
 * @format_uri: #raptor_uri describing the format to write (or NULL for default)
 * @base_uri: #raptor_uri base URI of the output format
 *
 * Write a query to an iostream in a specified format.
 * 
 * The supported URIs for the format_uri are:
 *
 * Default: SPARQL Query Language 2006-04-06
 * http://www.w3.org/TR/2006/CR-rdf-sparql-query-20060406/
 *
 * Return value: non-0 on failure
 **/
int
rasqal_query_write(raptor_iostream* iostr, rasqal_query* query,
                   raptor_uri* format_uri, raptor_uri* base_uri)
{
  const char *format_uri_str = NULL;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(iostr, raptor_iostream, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);

  if(format_uri)
    format_uri_str = (const char*)raptor_uri_as_string(format_uri);

  if(!format_uri ||
     !strcmp(format_uri_str,
             "http://www.w3.org/TR/rdf-sparql-query/") ||
     !strcmp(format_uri_str,
             "http://www.w3.org/TR/2006/WD-rdf-sparql-query-20060220/") ||
     !strcmp(format_uri_str,
             "http://www.w3.org/TR/2006/CR-rdf-sparql-query-20060406/"))
    return rasqal_query_write_sparql_20060406(iostr, query, base_uri);

  return 1;
}


/**
 * rasqal_query_iostream_write_escaped_counted_string:
 * @query: #rasqal_query object
 * @iostr: #raptor_iostream to write the escaped string to
 * @string: string to escape
 * @len: Length of string to escape
 * 
 * Write a string to an iostream in escaped form suitable for the query string.
 * 
 * Return value: non-0 on failure
 **/
int
rasqal_query_iostream_write_escaped_counted_string(rasqal_query* query,
                                                   raptor_iostream* iostr,
                                                   const unsigned char* string,
                                                   size_t len)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(iostr, raptor_iostream, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(string, char*, 1);

  if(query->factory->iostream_write_escaped_counted_string)
    return query->factory->iostream_write_escaped_counted_string(query, iostr, 
                                                                 string, len);
  else
    return 1;
}


/**
 * rasqal_query_escape_counted_string:
 * @query: #rasqal_query object
 * @string: string to escape
 * @len: Length of string to escape
 * @output_len_p: Pointer to store length of output string (or NULL)
 * 
 * Convert a string into an escaped form suitable for the query string.
 * 
 * The returned string must be freed by the caller with
 * rasqal_free_memory()
 *
 * Return value: the escaped string or NULL on failure.
 **/
unsigned char*
rasqal_query_escape_counted_string(rasqal_query* query,
                                   const unsigned char* string, 
                                   size_t len,
                                   size_t* output_len_p)
{
  raptor_iostream* iostr;
  void* output_string = NULL;
  int rc;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(string, char*, NULL);

#ifdef HAVE_RAPTOR2_API
  iostr = raptor_new_iostream_to_string(query->world->raptor_world_ptr,
                                        &output_string, output_len_p,
                                        rasqal_alloc_memory);
#else
  iostr = raptor_new_iostream_to_string(&output_string, output_len_p,
                                        rasqal_alloc_memory);
#endif
  if(!iostr)
    return NULL;
  rc = rasqal_query_iostream_write_escaped_counted_string(query, iostr,
                                                          string, len);
  raptor_free_iostream(iostr);
  if(rc && output_string) {
    rasqal_free_memory(output_string);
    output_string = NULL;
  }
  
  return (unsigned char *)output_string;
}


unsigned char*
rasqal_query_get_genid(rasqal_query* query, const unsigned char* base, 
                       int counter)
{
  int tmpcounter;
  int length;
  unsigned char *buffer;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  /* This is read-only and thread safe */
  if(counter < 0)
    counter= query->genid_counter++;
  
  length = strlen((const char*)base)+2;  /* base + (int) + "\0" */
  tmpcounter = counter;
  while(tmpcounter /= 10)
    length++;
  
  buffer = (unsigned char*)RASQAL_MALLOC(cstring, length);
  if(!buffer)
    return NULL;

  sprintf((char*)buffer, "%s%d", base, counter);
  return buffer;
}


void
rasqal_query_set_base_uri(rasqal_query* query, raptor_uri* base_uri)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(query, rasqal_query);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(base_uri, raptor_uri);

  if(query->base_uri)
    raptor_free_uri(query->base_uri);
  query->base_uri = base_uri;
  query->locator.uri = base_uri;
}


void
rasqal_query_set_store_results(rasqal_query* query, int store_results)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(query, rasqal_query);

  query->store_results = store_results;
}


rasqal_variable* 
rasqal_query_get_variable_by_offset(rasqal_query* query, int idx)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  return rasqal_variables_table_get(query->vars_table, idx);
}


int
rasqal_query_variable_bound_in_triple(rasqal_query *query,
                                      rasqal_variable *v,
                                      int column) 
{
  int v_column;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 0);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(v, rasqal_variable, 0);

  v_column = query->variables_bound_in[v->offset];
  if(v_column < 0)
    return 0;

  return v_column == column;
}


/*
 * rasqal_query_add_update_operation:
 * @query: #rasqal_query query object
 * @update: #rasqal_update_operation to add
 *
 * Add a update operation to the query.
 *
 * INTERNAL - The @update object becomes owned by the query.
 *
 * Return value: non-0 on failure
 **/
int
rasqal_query_add_update_operation(rasqal_query* query, 
                                  rasqal_update_operation *update)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);

  if(!update)
    return 1;

  if(!query->updates) {
#ifdef HAVE_RAPTOR2_API
    query->updates = raptor_new_sequence((raptor_data_free_handler)rasqal_free_update_operation, (raptor_data_print_handler)rasqal_update_operation_print);
#else
    query->updates = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_update_operation, (raptor_sequence_print_handler*)rasqal_update_operation_print);
#endif
    if(!query->updates) {
      rasqal_free_update_operation(update);
      return 1;
    }
  }

  if(raptor_sequence_push(query->updates, (void*)update))
    return 1;
  return 0;
}


/**
 * rasqal_query_get_update_operations_sequence:
 * @query: #rasqal_query query object
 *
 * Get the sequence of update operations
 *
 * Return value: a #raptor_sequence of #rasqal_update_operation pointers.
 **/
raptor_sequence*
rasqal_query_get_update_operations_sequence(rasqal_query* query)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  return query->updates;
}


/**
 * rasqal_query_get_update_operation:
 * @query: #rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Get a query update operation in the sequence of update operations
 *
 * Return value: a #rasqal_update_operation pointer or NULL if out of the sequence range
 **/
rasqal_update_operation*
rasqal_query_get_update_operation(rasqal_query* query, int idx)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  if(!query->updates)
    return NULL;
  
  return (rasqal_update_operation*)raptor_sequence_get_at(query->updates, idx);
}


/**
 * rasqal_query_set_default_generate_bnodeid_parameters:
 * @rdf_query: #rasqal_query object
 * @prefix: prefix string
 * @base: integer base identifier
 *
 * Set default bnodeid generation parameters
 *
 * Sets the parameters for the default algorithm used to generate
 * blank node IDs.  The default algorithm uses both @prefix and @base
 * to generate a new identifier.  The exact identifier generated is
 * not guaranteed to be a strict concatenation of @prefix and @base
 * but will use both parts.
 *
 * For finer control of the generated identifiers, use
 * rasqal_world_set_generate_bnodeid_handler() on the world class (or
 * the deprecated rasqal_query_set_generate_bnodeid_handler() on the
 * query class).
 *
 * If prefix is NULL, the default prefix is used (currently "bnodeid")
 * If base is less than 1, it is initialised to 1.
 * 
 * This calls rasqal_world_set_default_generate_bnodeid_parameters()
 * method on the world class; there is no separate configuration per
 * query object.
 *  
 * @Deprecated: Use
 * rasqal_world_set_default_generate_bnodeid_parameters() on the
 * world class.
 *
 **/
void
rasqal_query_set_default_generate_bnodeid_parameters(rasqal_query* rdf_query, 
                                                     char *prefix, int base)
{

  RASQAL_ASSERT_OBJECT_POINTER_RETURN(rdf_query, rasqal_query);

  rasqal_world_set_default_generate_bnodeid_parameters(rdf_query->world, 
                                                       prefix, base);
}


/**
 * rasqal_query_set_generate_bnodeid_handler:
 * @query: #rasqal_query query object
 * @user_data: user data pointer for callback
 * @handler: generate blank ID callback function
 *
 * Set the generate blank node ID handler function for the query.
 *
 * Sets the function to generate blank node IDs for the query.
 * The handler is called with a pointer to the rasqal_query, the
 * @user_data pointer and a user_bnodeid which is the value of
 * a user-provided blank node identifier (may be NULL).
 * It can either be returned directly as the generated value when present or
 * modified.  The passed in value must be free()d if it is not used.
 *
 * If handler is NULL, the default method is used
 *
 * Any hander set by rasqal_world_set_generate_bnodeid_handler()
 * overrides this.
 *  
 * @Deprecated: Use
 * rasqal_world_set_generate_bnodeid_handler() on the world class.
 * 
 **/
void
rasqal_query_set_generate_bnodeid_handler(rasqal_query* query,
                                          void *user_data,
                                          rasqal_generate_bnodeid_handler handler)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(query, rasqal_query);

  query->generate_bnodeid_handler_user_data = user_data;
  query->generate_bnodeid_handler = handler;
}


/*
 * rasqal_query_generate_bnodeid - Default generate id - internal
 */
unsigned char*
rasqal_query_generate_bnodeid(rasqal_query* rdf_query,
                              unsigned char *user_bnodeid)
{
  /* prefer world generate handler/data */
  if(rdf_query->world->generate_bnodeid_handler)
    return rasqal_world_generate_bnodeid(rdf_query->world, user_bnodeid);

  if(rdf_query->generate_bnodeid_handler)
    return rdf_query->generate_bnodeid_handler(rdf_query, 
                                               rdf_query->generate_bnodeid_handler_user_data, user_bnodeid);
  else
    return rasqal_world_default_generate_bnodeid_handler(rdf_query->world,
                                                         user_bnodeid);
}

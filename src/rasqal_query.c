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
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
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
 * if both are NULL.  rasqal_world_get_query_language_description returns
 * the description of the known names, labels, MIME types and URIs.
 *
 * Return value: a new #rasqal_query object or NULL on failure
 */
rasqal_query*
rasqal_new_query(rasqal_world *world, const char *name,
                 const unsigned char *uri)
{
  rasqal_query_language_factory* factory;
  rasqal_query* query;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);

  /* for compatibility with older binaries that do not call it */
  rasqal_world_open(world);

  factory = rasqal_get_query_language_factory(world, name, uri);
  if(!factory)
    return NULL;

  query = RASQAL_CALLOC(rasqal_query*, 1, sizeof(*query));
  if(!query)
    return NULL;
  
  /* set usage first to 1 so we can clean up with rasqal_free_query() on error */
  query->usage = 1;

  query->world = world;
  
  query->factory = factory;

  query->context = RASQAL_CALLOC(void*, 1, factory->context_length);
  if(!query->context)
    goto tidy;
  
  query->namespaces = raptor_new_namespaces(world->raptor_world_ptr, 0);
  if(!query->namespaces)
    goto tidy;

  query->vars_table = rasqal_new_variables_table(query->world);
  if(!query->vars_table)
    goto tidy;

  query->triples = raptor_new_sequence((raptor_data_free_handler)rasqal_free_triple, (raptor_data_print_handler)rasqal_triple_print);
  if(!query->triples)
    goto tidy;

  query->prefixes = raptor_new_sequence((raptor_data_free_handler)rasqal_free_prefix, (raptor_data_print_handler)rasqal_prefix_print);
  if(!query->prefixes)
    goto tidy;

  query->data_graphs = raptor_new_sequence((raptor_data_free_handler)rasqal_free_data_graph, (raptor_data_print_handler)rasqal_data_graph_print);
  if(!query->data_graphs)
    goto tidy;

  query->results = raptor_new_sequence((raptor_data_free_handler)rasqal_query_results_remove_query_reference, NULL);
  if(!query->results)
    goto tidy;

  query->eval_context = rasqal_new_evaluation_context(query->world,
                                                      &query->locator,
                                                      query->compare_flags);
  if(!query->eval_context)
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

  if(query->eval_context)
    rasqal_free_evaluation_context(query->eval_context);

  if(query->context)
    RASQAL_FREE(rasqal_query_context, query->context);

  if(query->namespaces)
    raptor_free_namespaces(query->namespaces);

  if(query->base_uri)
    raptor_free_uri(query->base_uri);

  if(query->query_string)
    RASQAL_FREE(char*, query->query_string);

  if(query->data_graphs)
    raptor_free_sequence(query->data_graphs);

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

  if(query->triples_use_map)
    RASQAL_FREE(shortarray, query->triples_use_map);

  if(query->variables_use_map)
    RASQAL_FREE(shortarray, query->variables_use_map);

  if(query->query_graph_pattern)
    rasqal_free_graph_pattern(query->query_graph_pattern);

  if(query->graph_patterns_sequence)
    raptor_free_sequence(query->graph_patterns_sequence);

  if(query->query_results_formatter_name)
    RASQAL_FREE(char*, query->query_results_formatter_name);

  /* Do this last since most everything above could refer to a variable */
  if(query->vars_table)
    rasqal_free_variables_table(query->vars_table);

  if(query->updates)
    raptor_free_sequence(query->updates);

  if(query->modifier)
    rasqal_free_solution_modifier(query->modifier);
  
  if(query->bindings)
    rasqal_free_bindings(query->bindings);
  
  if(query->projection)
    rasqal_free_projection(query->projection);
  
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
    case RASQAL_FEATURE_RAND_SEED:

      if(feature == RASQAL_FEATURE_RAND_SEED)
        query->user_set_rand = 1;
      
      query->features[RASQAL_GOOD_CAST(int, feature)] = value;
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
    return rasqal_query_set_feature(query, feature,
                                    atoi(RASQAL_GOOD_CAST(const char*, value)));

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
    case RASQAL_FEATURE_RAND_SEED:
      result = (query->features[RASQAL_GOOD_CAST(int, feature)] != 0);
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

  if(!query->projection)
    return 0;
  
  return query->projection->distinct;
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

  if(distinct_mode < 0 || distinct_mode > 2)
    distinct_mode = 0;

  if(!query->projection) {
    query->projection = rasqal_new_projection(query,
                                              /* variables */ NULL,
                                              /* wildcard */ 0,
                                              /* distinct */ 0);
    if(!query->projection)
      return;
  }
  query->projection->distinct = distinct_mode;
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

  if(query->modifier)
    return query->modifier->limit;
  else
    return -1;
}


/**
 * rasqal_query_set_limit:
 * @query: #rasqal_query query object
 * @limit: the limit on results, >=0 to set a limit, <0 to have no limit
 *
 * Set the query-specified limit on results.
 *
 * This is the limit given in the query on the number of results
 * allowed.  It is only guaranteed to work after the query is
 * prepared and before it is executed.
 **/
void
rasqal_query_set_limit(rasqal_query* query, int limit)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(query, rasqal_query);

  if(query->modifier)
    query->modifier->limit = limit;
}


/**
 * rasqal_query_get_offset:
 * @query: #rasqal_query query object
 *
 * Get the query-specified offset on results.
 *
 * This is the offset given in the query on the number of results
 * allowed.  It is only guaranteed to work after the query is
 * prepared and before it is executed.
 *
 * Return value: integer >=0 if a offset is given, otherwise <0
 **/
int
rasqal_query_get_offset(rasqal_query* query)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 0);

  if(query->modifier)
    return query->modifier->offset;
  else
    return -1;
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

  if(query->modifier)
    query->modifier->offset = offset;
}


/**
 * rasqal_query_add_data_graph:
 * @query: #rasqal_query query object
 * @data_graph: data graph
 *
 * Add a data graph to the query.
 *
 * Return value: non-0 on failure
 **/
int
rasqal_query_add_data_graph(rasqal_query* query, rasqal_data_graph* data_graph)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(data_graph, rasqal_data_graph, 1);

  if(raptor_sequence_push(query->data_graphs, (void*)data_graph))
    return 1;
  return 0;
}


/**
 * rasqal_query_add_data_graphs:
 * @query: #rasqal_query query object
 * @data_graphs: sequence of #rasqal_data_graph
 *
 * Add a set of data graphs to the query.
 *
 * The objects in the passed-in @data_graphs sequence becomes owne by the query.
 * The @data_graphs sequence itself is freed and must not be used after this call.
 *
 * Return value: non-0 on failure
 **/
int
rasqal_query_add_data_graphs(rasqal_query* query,
                             raptor_sequence* data_graphs)
{
  int rc;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(data_graphs, raptor_sequence, 1);

  rc = raptor_sequence_join(query->data_graphs, data_graphs);
  raptor_free_sequence(data_graphs);

  return rc;
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
 * Add a projected (named) variable to the query.
 *
 * See also rasqal_query_set_variable() which assigns or removes a value to
 * a previously added variable in the query.
 *
 * Return value: non-0 on failure
 **/
int
rasqal_query_add_variable(rasqal_query* query, rasqal_variable* var)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(var, rasqal_variable, 1);

  if(!rasqal_variables_table_contains(query->vars_table, var->type, var->name)) {
    if(rasqal_variables_table_add_variable(query->vars_table, var))
      return 1;
  }

  if(!query->projection) {
    query->projection = rasqal_new_projection(query,
                                              /* variables */ NULL,
                                              /* wildcard */ 0,
                                              /* distinct */ 0);
    if(!query->projection)
      return 1;
  }
  return rasqal_projection_add_variable(query->projection, var);
}


/**
 * rasqal_query_get_bound_variable_sequence:
 * @query: #rasqal_query query object
 *
 * Get the sequence of projected variables in the query.
 *
 * This returns the sequence of variables that are explicitly chosen
 * via SELECT in SPARQL.  Or all variables mentioned with SELECT *
 *
 * Return value: a #raptor_sequence of #rasqal_variable pointers.
 **/
raptor_sequence*
rasqal_query_get_bound_variable_sequence(rasqal_query* query)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  if(!query->projection)
    return NULL;
  
  return rasqal_projection_get_variables_sequence(query->projection);
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
 * Get a variable in the query
 *
 * Return value: pointer to shared #rasqal_variable or NULL if out of range
 **/
rasqal_variable*
rasqal_query_get_variable(rasqal_query* query, int idx)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  return rasqal_variables_table_get(query->vars_table, idx);
}


/**
 * rasqal_query_has_variable2:
 * @query: #rasqal_query query object
 * @type: the variable type to match or #RASQAL_VARIABLE_TYPE_UNKNOWN for any.
 * @name: variable name
 *
 * Find if the named variable of the given type is in the query
 *
 * Note that looking up for any type #RASQAL_VARIABLE_TYPE_UNKNOWN
 * may a name match but for any type so in cases where the query has
 * both a named and anonymous (extensional) variable, an arbitrary one
 * will be returned.
 *
 * Return value: non-0 if the variable name was found.
 **/
int
rasqal_query_has_variable2(rasqal_query* query,
                           rasqal_variable_type type,
                           const unsigned char *name)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 0);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(name, char*, 0);

  return rasqal_variables_table_contains(query->vars_table, type, name);
}


#ifndef RASQAL_DISABLE_DEPRECATED
/**
 * rasqal_query_has_variable:
 * @query: #rasqal_query query object
 * @name: variable name
 *
 * Find if the named variable is in the query (of any type)
 *
 * @Deprecated: Use rasqal_query_has_variable2() with the variable type arg
 *
 * Return value: non-0 if the variable name was found.
 **/
int
rasqal_query_has_variable(rasqal_query* query, const unsigned char *name)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 0);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(name, char*, 0);

  return rasqal_query_has_variable2(query, RASQAL_VARIABLE_TYPE_UNKNOWN, name);
}
#endif

/**
 * rasqal_query_set_variable2:
 * @query: #rasqal_query query object
 * @type: the variable type to match or #RASQAL_VARIABLE_TYPE_UNKNOWN for any.
 * @name: #rasqal_variable variable
 * @value: #rasqal_literal value to set or NULL
 *
 * Bind an existing typed variable to a value to the query.
 *
 * See also rasqal_query_add_variable() which adds a new binding variable
 * and must be called before this method is invoked.
 *
 * Return value: non-0 on failure
 **/
int
rasqal_query_set_variable2(rasqal_query* query,
                           rasqal_variable_type type,
                           const unsigned char *name,
                           rasqal_literal* value)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(name, char*, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(value, rasqal_literal, 1);

  return rasqal_variables_table_set(query->vars_table, type, name, value);
}


#ifndef RASQAL_DISABLE_DEPRECATED
/**
 * rasqal_query_set_variable:
 * @query: #rasqal_query query object
 * @name: #rasqal_variable variable
 * @value: #rasqal_literal value to set or NULL
 *
 * Bind an existing named (selected) variable to a value to the query.
 *
 * @Deprecated for rasqal_query_set_variable2() that includes a type
 * arg.  This function only sets named variables of type
 * #RASQAL_VARIABLE_TYPE_NORMAL
 *
 * Return value: non-0 on failure
 **/
int
rasqal_query_set_variable(rasqal_query* query, const unsigned char *name,
                          rasqal_literal* value)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(name, char*, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(value, rasqal_literal, 1);

  return rasqal_query_set_variable2(query, RASQAL_VARIABLE_TYPE_NORMAL,
                                    name, value);
}
#endif

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
    query->prefixes = raptor_new_sequence((raptor_data_free_handler)rasqal_free_prefix, (raptor_data_print_handler)rasqal_prefix_print);
    if(!query->prefixes)
      return 1;
  } else {
    int i;
    for(i = 0; i < raptor_sequence_size(query->prefixes); i++) {
      rasqal_prefix* p;
      p = (rasqal_prefix*)raptor_sequence_get_at(query->prefixes, i);

      if((!p->prefix && !prefix->prefix) ||
         ((p->prefix && prefix->prefix &&
           !strcmp(RASQAL_GOOD_CAST(const char*, p->prefix),
                   RASQAL_GOOD_CAST(const char*, prefix->prefix))))
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

  query->store_results = 0;

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
    size_t len = strlen(RASQAL_GOOD_CAST(const char*, query_string)) + 3; /* +3 for " \0\0" */
    unsigned char *query_string_copy = RASQAL_MALLOC(unsigned char*, len);
    if(!query_string_copy) {
      query->failed = 1;
      return 1;
    }
    memcpy(query_string_copy, query_string, len - 3);
    query_string_copy[len - 3] = ' ';
    query_string_copy[len - 2] = query_string_copy[len - 1] = '\0';
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

  /* set evaluaton context with latest copies of query fields */
  query->eval_context->flags = query->compare_flags;
  rasqal_evaluation_context_set_base_uri(query->eval_context, query->base_uri);

  /* set random seed */
  if(1) {
    unsigned int seed;

    /* get seed either from user or system sources */
    if(query->user_set_rand)
      /* it is ok to truncate here for the purposes of getting a seed */
      seed = RASQAL_GOOD_CAST(unsigned int, query->features[RASQAL_GOOD_CAST(int, RASQAL_FEATURE_RAND_SEED)]);
    else
      seed = rasqal_random_get_system_seed(query->world);
    
    rasqal_evaluation_context_set_rand_seed(query->eval_context, seed);
  }
  

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

  /* default */
  engine = &rasqal_query_engine_algebra;

  if(name) {
    if(!strcmp(name, "2") || !strcmp(name, "algebra"))
      engine = &rasqal_query_engine_algebra;
    else
      engine = NULL;
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
  rasqal_query_results_type type;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  if(query->failed)
    return NULL;

  type = rasqal_query_get_result_type(query);
  if(type == RASQAL_QUERY_RESULTS_UNKNOWN)
    return NULL;
  
  query_results = rasqal_new_query_results2(query->world, query, type);
  if(!query_results)
    return NULL;

  if(!engine)
    engine = rasqal_query_get_engine_by_name(NULL);

  if(rasqal_query_results_execute_with_engine(query_results, engine,
                                              query->store_results)) {
    rasqal_free_query_results(query_results);
    query_results = NULL;      
  }
    
      
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

  return rasqal_query_verb_labels[RASQAL_GOOD_CAST(int, verb)];
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
  int distinct_mode;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(fh, FILE*, 1);

  fprintf(fh, "query verb: %s\n", rasqal_query_verb_as_string(query->verb));
  
  distinct_mode = rasqal_query_get_distinct(query);
  if(distinct_mode)
    fprintf(fh, "query results distinct mode: %s\n",
            (distinct_mode == 1 ? "distinct" : "reduced"));
  if(query->explain)
    fputs("query results explain: yes\n", fh);

  if(query->modifier) {
    if(query->modifier->limit > 0)
      fprintf(fh, "query results limit: %d\n", query->modifier->limit);
    if(query->modifier->offset > 0)
      fprintf(fh, "query results offset: %d\n", query->modifier->offset);
  }
  
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
  seq = rasqal_query_get_bound_variable_sequence(query);
  if(seq) {
    int i;
    
    fputs("\nprojected variable names: ", fh);
    for(i = 0; 1; i++) {
      rasqal_variable* v = (rasqal_variable*)raptor_sequence_get_at(seq, i);
      if(!v)
        break;
      if(i > 0)
        fputs(", ", fh);

      fputs((const char*)v->name, fh);
    }
    fputc('\n', fh);
      
    fputs("\nbound variables: ", fh); 
    raptor_sequence_print(seq, fh);
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

  if(query->modifier) {
    if(query->modifier->order_conditions) {
      fputs("\nquery order conditions: ", fh);
      raptor_sequence_print(query->modifier->order_conditions, fh);
    }
    if(query->modifier->group_conditions) {
      fputs("\nquery group conditions: ", fh);
      raptor_sequence_print(query->modifier->group_conditions, fh);
    }
    if(query->modifier->having_conditions) {
      fputs("\nquery having conditions: ", fh);
      raptor_sequence_print(query->modifier->having_conditions, fh);
    }
  }
  
  if(query->updates) {
    fputs("\nupdate operations: ", fh);
    raptor_sequence_print(query->updates, fh);
  }
  if(query->bindings) {
    fputs("\nbindings: ", fh);
    rasqal_bindings_print(query->bindings, fh);
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
 * Return value: user data as set by rasqal_query_set_user_data()
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

  if(!query->projection)
    return 0;
  
  return query->projection->wildcard;
}


/**
 * rasqal_query_set_wildcard:
 * @query: #rasqal_query query object
 * @wildcard: wildcard
 *
 * Set the query projection wildcard flag
 *
 **/
void
rasqal_query_set_wildcard(rasqal_query* query, int wildcard)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(query, rasqal_query);

  if(!query->projection) {
    query->projection = rasqal_new_projection(query,
                                              /* variables */ NULL,
                                              /* wildcard */ 0,
                                              /* wildcard */ 0);
    if(!query->projection)
      return;
  }
  query->projection->wildcard = wildcard ? 1 : 0;
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

  if(query->modifier)
    return query->modifier->order_conditions;
  else
    return NULL;
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

  if(!query->modifier || !query->modifier->order_conditions)
    return NULL;
  
  return (rasqal_expression*)raptor_sequence_get_at(query->modifier->order_conditions, idx);
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

  if(query->modifier)
    return query->modifier->group_conditions;
  else
    return NULL;
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

   if(!query->modifier || !query->modifier->group_conditions)
    return NULL;
  
  return (rasqal_expression*)raptor_sequence_get_at(query->modifier->group_conditions, idx);
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

  if(query->modifier)
    return query->modifier->having_conditions;
  else
    return NULL;
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

   if(!query->modifier || !query->modifier->having_conditions)
    return NULL;
  
  return (rasqal_expression*)raptor_sequence_get_at(query->modifier->having_conditions, idx);
}


/**
 * rasqal_query_graph_pattern_visit2:
 * @query: query
 * @visit_fn: user function to operate on
 * @data: user data to pass to function
 * 
 * Visit all graph patterns in a query with a user function @visit_fn.
 *
 * See also rasqal_graph_pattern_visit().
 *
 * Return value: result from visit function @visit_fn if it returns non-0
 **/
int
rasqal_query_graph_pattern_visit2(rasqal_query* query, 
                                  rasqal_graph_pattern_visit_fn visit_fn, 
                                  void* data)
{
  rasqal_graph_pattern* gp;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);

  gp = rasqal_query_get_query_graph_pattern(query);
  if(!gp)
    return 1;

  return rasqal_graph_pattern_visit(query, gp, visit_fn, data);
}


#ifndef RASQAL_DISABLE_DEPRECATED
/**
 * rasqal_query_graph_pattern_visit:
 * @query: query
 * @visit_fn: user function to operate on
 * @data: user data to pass to function
 * 
 * Visit all graph patterns in a query with a user function @visit_fn.
 *
 * @Deprecated: use rasqal_query_graph_pattern_visit2() that returns the @visit_fn status code.
 *
 * See also rasqal_graph_pattern_visit().
 **/
void
rasqal_query_graph_pattern_visit(rasqal_query* query, 
                                 rasqal_graph_pattern_visit_fn visit_fn, 
                                 void* data)
{
  (void)rasqal_query_graph_pattern_visit2(query, visit_fn, data);
}
#endif


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
    format_uri_str = RASQAL_GOOD_CAST(const char*, raptor_uri_as_string(format_uri));

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

  iostr = raptor_new_iostream_to_string(query->world->raptor_world_ptr,
                                        &output_string, output_len_p,
                                        rasqal_alloc_memory);
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


/**
 * rasqal_query_set_store_results:
 * @query: the #rasqal_query object
 * @store_results: store results flag
 *
 * Request that query results are stored during execution
 * 
 * When called after a rasqal_query_prepare(), this tells
 * rasqal_query_execute() to execute the entire query immediately
 * rather than generate them lazily, and store all the results in
 * memory.  The results will then be available for reading multiple
 * times using rasqal_query_results_rewind() to move back to the
 * start of the result object.  If called after preparation, returns
 * failure.
 *
 * Return value: non-0 on failure.
 **/
int
rasqal_query_set_store_results(rasqal_query* query, int store_results)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);

  if(!query->prepared)
    return 1;

  query->store_results = store_results;

  return 0;
}


rasqal_variable* 
rasqal_query_get_variable_by_offset(rasqal_query* query, int idx)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  return rasqal_variables_table_get(query->vars_table, idx);
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
    query->updates = raptor_new_sequence((raptor_data_free_handler)rasqal_free_update_operation, (raptor_data_print_handler)rasqal_update_operation_print);
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

    return rasqal_world_default_generate_bnodeid_handler(rdf_query->world,
                                                         user_bnodeid);
}


/**
 * rasqal_query_get_bindings_variables_sequence:
 * @query: #rasqal_query query object
 *
 * Get the sequence of BINDINGS block variables
 *
 * Return value: a #raptor_sequence of #raptor_variable pointers
 **/
raptor_sequence*
rasqal_query_get_bindings_variables_sequence(rasqal_query* query)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  if(query->bindings)
    return query->bindings->variables;
  else
    return NULL;
  
}


/**
 * rasqal_query_get_bindings_variable:
 * @query: #rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Get a rasqal_variable* in the sequence of BINDINGS block variables
 *
 * Return value: a #raptor_sequence of #raptor_variable pointers
 **/
rasqal_variable*
rasqal_query_get_bindings_variable(rasqal_query* query, int idx)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  if(!query->bindings || !query->bindings->variables)
    return NULL;
  
  return (rasqal_variable*)raptor_sequence_get_at(query->bindings->variables, idx);
}


/**
 * rasqal_query_get_bindings_rows_sequence:
 * @query: #rasqal_query query object
 *
 * Get the sequence of BINDINGS block result rows
 *
 * Return value: a #raptor_sequence of #raptor_row pointers
 **/
raptor_sequence*
rasqal_query_get_bindings_rows_sequence(rasqal_query* query)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  if(query->bindings)
    return query->bindings->rows;
  else
    return NULL;
  
}


/**
 * rasqal_query_get_bindings_row:
 * @query: #rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Get a rasqal_row* in the sequence of BINDINGS block result rows
 *
 * Return value: a #raptor_sequence of #raptor_row pointers
 **/
rasqal_row*
rasqal_query_get_bindings_row(rasqal_query* query, int idx)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  if(!query->bindings || !query->bindings->rows)
    return NULL;
  
  return (rasqal_row*)raptor_sequence_get_at(query->bindings->rows, idx);
}


/*
 * rasqal_query_variable_bound_in_triple:
 * @query: #rasqal_query query object
 * @variable: variable
 * @column: triple column
 * 
 * INTERNAL - Test if variable is bound in given triple
 *
 * Return value: part of triple the variable is bound in
 */
rasqal_triple_parts
rasqal_query_variable_bound_in_triple(rasqal_query* query,
                                      rasqal_variable* v,
                                      int column)
{
  int width;
  unsigned short *triple_row;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, (rasqal_triple_parts)0);
  
  width = rasqal_variables_table_get_total_variables_count(query->vars_table);
  triple_row = &query->triples_use_map[column * width];

  return (rasqal_triple_parts)((triple_row[v->offset] & RASQAL_TRIPLES_BOUND_MASK) >> 4);
}


/**
 * rasqal_query_get_result_type:
 * @query: #rasqal_query query object
 *
 * Get the result type expected from executing the query.
 *
 * This function is only valid after rasqal_query_prepare() has been
 * run on the query and will return #RASQAL_QUERY_RESULTS_UNKNOWN if
 * called before preparation.
 *
 * Return value: result type or #RASQAL_QUERY_RESULTS_UNKNOWN if not known or on error
 **/
rasqal_query_results_type
rasqal_query_get_result_type(rasqal_query* query)
{
  rasqal_query_results_type type = RASQAL_QUERY_RESULTS_BINDINGS;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, RASQAL_QUERY_RESULTS_UNKNOWN);

  if(!query->prepared)
    return RASQAL_QUERY_RESULTS_UNKNOWN;

  if(query->query_results_formatter_name)
    type = RASQAL_QUERY_RESULTS_SYNTAX;
  else
    switch(query->verb) {
      case RASQAL_QUERY_VERB_SELECT:
        type = RASQAL_QUERY_RESULTS_BINDINGS;
        break;

      case RASQAL_QUERY_VERB_ASK:
        type = RASQAL_QUERY_RESULTS_BOOLEAN;
        break;

      case RASQAL_QUERY_VERB_CONSTRUCT:
      case RASQAL_QUERY_VERB_DESCRIBE:
        type = RASQAL_QUERY_RESULTS_GRAPH;
        break;
        
      case RASQAL_QUERY_VERB_UNKNOWN:
      case RASQAL_QUERY_VERB_DELETE:
      case RASQAL_QUERY_VERB_INSERT:
      case RASQAL_QUERY_VERB_UPDATE:
      default:
        type = RASQAL_QUERY_RESULTS_UNKNOWN;
    }

  return type;
}


/*
 * rasqal_query_store_select_query:
 * @query: query
 * @gp: SELECT graph pattern
 *
 * INTERNAL - store a select query
 *
 * The query object owns the @projection, @data_graphs, @where_gp and
 * the @modifier after this call.
 *
 * Return value: non-0 on failure
 */
int
rasqal_query_store_select_query(rasqal_query* query,
                                rasqal_projection* projection,
                                raptor_sequence* data_graphs,
                                rasqal_graph_pattern* where_gp,
                                rasqal_solution_modifier* modifier)
{
  if(!projection || !where_gp || !modifier)
    return 1;

  query->verb = RASQAL_QUERY_VERB_SELECT;

  rasqal_query_set_projection(query, projection);

  query->query_graph_pattern = where_gp;
  
  if(data_graphs)
    rasqal_query_add_data_graphs(query, data_graphs);

  rasqal_query_set_modifier(query, modifier);

  return 0;
}


int
rasqal_query_reset_select_query(rasqal_query* query)
{
  rasqal_query_set_projection(query, NULL);
  rasqal_query_set_modifier(query, NULL);

  if(query->data_graphs) {
    while(1) {
      rasqal_data_graph* dg;
      dg = (rasqal_data_graph*)raptor_sequence_pop(query->data_graphs);
      if(!dg)
        break;
      rasqal_free_data_graph(dg);
    }
  }

  return 0;
}


/*
 * rasqal_query_get_projection:
 * @query: #rasqal_query
 *
 * INTERNAL - Get the query variable projection
 *
 * This may be NULL if the query does not project any variables such
 * as for CONSTRUCT or ASK queries.
 *
 * Return value: projection or NULL.
 **/
rasqal_projection*
rasqal_query_get_projection(rasqal_query* query)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, NULL);

  return query->projection;
}


/*
 * rasqal_query_set_projection:
 * @query: #rasqal_query
 * @projection: variable projection and flags
 *
 * INTERNAL - Set the query projection
 *
 * Return value: non-0 on failure
 **/
int
rasqal_query_set_projection(rasqal_query* query, rasqal_projection* projection)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);

  if(query->projection)
    rasqal_free_projection(query->projection);

  query->projection = projection;
  
  return 0;
}


/*
 * rasqal_query_set_modifier:
 * @query: #rasqal_query
 * @modifier: variable modifier and flags
 *
 * INTERNAL - Set the query modifier
 *
 * Return value: non-0 on failure
 **/
int
rasqal_query_set_modifier(rasqal_query* query,
                          rasqal_solution_modifier* modifier)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query, rasqal_query, 1);

  if(query->modifier)
    rasqal_free_solution_modifier(query->modifier);

  query->modifier = modifier;
  
  return 0;
}

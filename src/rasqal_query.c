/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_query.c - Rasqal RDF Query
 *
 * $Id$
 *
 * Copyright (C) 2003-2004, David Beckett http://purl.org/net/dajobe/
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

  query->triples=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_triple, (raptor_sequence_print_handler*)rasqal_triple_print);
  
  query->prefixes=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_prefix, (raptor_sequence_print_handler*)rasqal_prefix_print);

  query->graph_patterns=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_graph_pattern, (raptor_sequence_print_handler*)rasqal_graph_pattern_print);

  query->optional_graph_pattern= -1;

  query->sources=raptor_new_sequence((raptor_sequence_free_handler*)raptor_free_uri, (raptor_sequence_print_handler*)raptor_sequence_print_uri);

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

  if(query->sources)
    raptor_free_sequence(query->sources);
  if(query->selects)
    raptor_free_sequence(query->selects);
  if(query->describes)
    raptor_free_sequence(query->describes);
  if(query->ordered_triples)
    raptor_free_sequence(query->ordered_triples);
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
    RASQAL_FREE(graphpatternarray, query->variables_declared_in);

  if(query->graph_patterns)
    raptor_free_sequence(query->graph_patterns);

  if(query->constraints_expression) {
    rasqal_free_expression(query->constraints_expression);
    if(query->constraints)
      raptor_free_sequence(query->constraints);
  } else if(query->constraints) {
    int i;
    
    /* free rasqal_expressions that are normally assembled into an
     * expression tree pointed at query->constraints_expression
     * when query construction succeeds.
     */
    for(i=0; i< raptor_sequence_size(query->constraints); i++) {
      rasqal_expression* e=(rasqal_expression*)raptor_sequence_get_at(query->constraints, i);
      rasqal_free_expression(e);
    }
    raptor_free_sequence(query->constraints);
  }

  /* Do this last since most everything above could refer to a variable */
  if(query->variables_sequence)
    raptor_free_sequence(query->variables_sequence);
  
  RASQAL_FREE(rasqal_query, query);
}


/* Methods */

/**
 * rasqal_query_get_name - Return the short name for the query
 * @query: &rasqal_query query object
 **/
const char*
rasqal_query_get_name(rasqal_query *query)
{
  return query->factory->name;
}


/**
 * rasqal_query_get_label - Return a readable label for the query
 * @query: &rasqal_query query object
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
      
    default:
      break;
  }
}


/**
 * rasqal_query_add_source - Add a source URI to the query
 * @query: &rasqal_query query object
 * @uri: &raptor_uri uri
 **/
void
rasqal_query_add_source(rasqal_query* query, raptor_uri* uri)
{
  raptor_sequence_shift(query->sources, (void*)uri);
}


/**
 * rasqal_query_get_source_sequence - Get the sequence of source URIs
 * @query: &rasqal_query query object
 *
 * Return value: a &raptor_sequence of &raptor_uri pointers.
 **/
raptor_sequence*
rasqal_query_get_source_sequence(rasqal_query* query)
{
  return query->sources;
}


/**
 * rasqal_query_get_source - Get a source URI in the sequence of sources
 * @query: &rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Return value: a &raptor_uri pointer or NULL if out of the sequence range
 **/
raptor_uri*
rasqal_query_get_source(rasqal_query* query, int idx)
{
  if(!query->sources)
    return NULL;
  
  return (raptor_uri*)raptor_sequence_get_at(query->sources, idx);
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

  raptor_sequence_shift(query->selects, (void*)var);
}


/**
 * rasqal_query_get_variable_sequence - Get the sequence of variables to bind in the query
 * @query: &rasqal_query query object
 *
 * Return value: a &raptor_sequence of &rasqal_variable pointers.
 **/
raptor_sequence*
rasqal_query_get_variable_sequence(rasqal_query* query)
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
 * rasqal_query_add_triple -  Add a matching triple to the query
 * @query: &rasqal_query query object
 * @triple: &rasqal_triple triple
 *
 **/
void
rasqal_query_add_triple(rasqal_query* query, rasqal_triple* triple)
{
  if(!query->triples)
    query->triples=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_triple, (raptor_sequence_print_handler*)rasqal_triple_print);
  
  raptor_sequence_shift(query->triples, (void*)triple);
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
 * rasqal_query_add_constraint - Add a constraint expression to the query
 * @query: &rasqal_query query object
 * @expr: &rasqal_expression expr
 *
 **/
void
rasqal_query_add_constraint(rasqal_query* query, rasqal_expression* expr)
{
  if(!query->constraints)
    query->constraints=raptor_new_sequence(NULL, (raptor_sequence_print_handler*)rasqal_expression_print);
  
  raptor_sequence_shift(query->constraints, (void*)expr);
}


/**
 * rasqal_query_get_constraint_sequence - Get the sequence of constraints expressions in the query
 * @query: &rasqal_query query object
 *
 * Return value: a &raptor_sequence of &rasqal_expression pointers.
 **/
raptor_sequence*
rasqal_query_get_constraint_sequence(rasqal_query* query)
{
  return query->constraints;
}


/**
 * rasqal_query_get_constraint - Get a constraint in the sequence of constraint expressions in the query
 * @query: &rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Return value: a &rasqal_expression pointer or NULL if out of the sequence range
 **/
rasqal_expression*
rasqal_query_get_constraint(rasqal_query* query, int idx)
{
  if(!query->constraints)
    return NULL;

  return (rasqal_expression*)raptor_sequence_get_at(query->constraints, idx);
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

  raptor_sequence_shift(query->prefixes, (void*)prefix);
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
  query->locator.line= query->locator.column = 0;

  rc=query->factory->prepare(query);
  if(rc)
    query->failed=1;
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
  
  rc=rasqal_engine_execute_init(query);
  if(rc) {
    query->failed=1;
    return NULL;
  }

  if(query->factory->execute) {
    rc=query->factory->execute(query);
    if(rc) {
      query->failed=1;
      return NULL;
    }
  }

  query_results=(rasqal_query_results*)RASQAL_CALLOC(rasqal_query_results, sizeof(rasqal_query_results), 1);
  query_results->query=query;

  rasqal_query_add_query_result(query, query_results);

  rasqal_query_results_next(query_results);

  return query_results;
}


/* Utility methods */

/**
 * rasqal_query_print - Print a query in a debug format
 * @query: the &rasqal_query object
 * @fh: the &FILE* handle to print to.
 * 
 **/
void
rasqal_query_print(rasqal_query* query, FILE *fh)
{
  fprintf(fh, "sources: ");
  if(query->sources)
    raptor_sequence_print(query->sources, fh);
  if(query->selects) {
    fprintf(fh, "\nselects: "); 
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
  if(query->ordered_triples) {
    fprintf(fh, "\nordered triples: ");
    raptor_sequence_print(query->ordered_triples, fh);
  }
  if(query->optional_triples) {
    fprintf(fh, "\noptional triples: ");
    raptor_sequence_print(query->optional_triples, fh);
  }
  if(query->constructs) {
    fprintf(fh, "\nconstructs: ");
    raptor_sequence_print(query->constructs, fh);
  }
  if(query->constraints) {
    fprintf(fh, "\nconstraints: ");
    raptor_sequence_print(query->constraints, fh);
  }
  if(query->prefixes) {
    fprintf(fh, "\nprefixes: ");
    raptor_sequence_print(query->prefixes, fh);
  }
  if(query->graph_patterns) {
    fprintf(fh, "\ngraph patterns: ");
    raptor_sequence_print(query->graph_patterns, fh);
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



void
rasqal_free_query_results(rasqal_query_results *query_results) 
{
  rasqal_query *query;

  if(!query_results)
    return;
  
  query=query_results->query;
  rasqal_query_remove_query_result(query, query_results);
  RASQAL_FREE(rasqal_query_results, query_results);
}


/**
 * rasqal_query_results_get_count - Get number of bindings so far
 * @query_results: &rasqal_query_results query_results
 * 
 * Return value: number of bindings found so far
 **/
int
rasqal_query_results_get_count(rasqal_query_results *query_results)
{
  if(!query_results)
    return -1;

  return query_results->query->result_count;
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
  int rc;
  
  if(!query_results)
    return 1;
  
  query=query_results->query;
  if(query->finished)
    return 1;

  /* rc<0 error rc=0 end of results,  rc>0 got a result */
  rc=rasqal_engine_get_next_result(query);
  if(rc<1)
    query->finished=1;
  if(rc<0)
    query->failed=1;
  
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
  
  query=query_results->query;
  if(query->finished)
    return NULL;

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
 * rasqal_query_results_write - Write the query results to an iostream in a format
 * @iostr: &raptor_iostream to write the query to
 * @results: &rasqal_query_results query results format
 * @format_uri: &raptor_uri describing the format to write
 * @base_uri: &raptor_uri base URI of the output format
 * 
 * The only supported URI for the format_uri is
 * http://www.w3.org/TR/2004/WD-rdf-sparql-XMLres-20041221/
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
  
  /*
   * SPARQL XML Results 2004-12-21
   * http://www.w3.org/TR/2004/WD-rdf-sparql-XMLres-20041221/
   */
  if(strcmp((const char*)raptor_uri_as_string(format_uri),
            "http://www.w3.org/TR/2004/WD-rdf-sparql-XMLres-20041221/"))
    return 1;

  if(!base_uri)
    return 1;

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
  
  sparql_element=raptor_new_xml_element(sparql_qname,
                                        NULL, /* language */
                                        raptor_uri_copy(base_uri));

  raptor_xml_writer_start_element(xml_writer, sparql_element);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);


  /*   <head> */
  qname1=raptor_new_qname_from_namespace_local_name(res_ns, 
                          (const unsigned char*)"head",
                          NULL); /* no attribute value - element */
  
  element1=raptor_new_xml_element(qname1,
                                  NULL, /* language */
                                  raptor_uri_copy(base_uri));

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
    
    variable_element=raptor_new_xml_element(variable_qname,
                                            NULL,
                                            raptor_uri_copy(base_uri));
    

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
  
  results_element=raptor_new_xml_element(results_qname,
                                         NULL, /* language */
                                         raptor_uri_copy(base_uri));
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"  ", 2);
  raptor_xml_writer_start_element(xml_writer, results_element);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);


  /* declare result element for later multiple use */
  result_qname=raptor_new_qname_from_namespace_local_name(res_ns, 
                                (const unsigned char*)"result",
                                                          NULL);
  
  result_element=raptor_new_xml_element(result_qname,
                                        NULL, /* language */
                                        raptor_uri_copy(base_uri));


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
      element1=raptor_new_xml_element(qname1,
                                      NULL, /* language */
                                      raptor_uri_copy(base_uri));
      

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
        case RASQAL_LITERAL_BLANK:
        case RASQAL_LITERAL_PATTERN:
        case RASQAL_LITERAL_QNAME:
        case RASQAL_LITERAL_INTEGER:
        case RASQAL_LITERAL_BOOLEAN:
        case RASQAL_LITERAL_FLOATING:
        case RASQAL_LITERAL_VARIABLE:
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



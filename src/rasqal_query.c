/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_query.c - Rasqal RDF Query
 *
 * $Id$
 *
 * Copyright (C) 2003-2004 David Beckett - http://purl.org/net/dajobe/
 * Institute for Learning and Research Technology - http://www.ilrt.bris.ac.uk/
 * University of Bristol - http://www.bristol.ac.uk/
 * 
 * This package is Free Software or Open Source available under the
 * following licenses (these are alternatives):
 *   1. GNU Lesser General Public License (LGPL)
 *   2. GNU General Public License (GPL)
 *   3. Mozilla Public License (MPL)
 * 
 * See LICENSE.html or LICENSE.txt at the top of this package for the
 * full license terms.
 * 
 * 
 */

#ifdef HAVE_CONFIG_H
#include <rasqal_config.h>
#endif

#ifdef WIN32
#include <win32_config.h>
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
 * rasqal_new_query - Constructor - create a new rasqal_query object
 * @name: the query name
 *
 * Return value: a new &rasqal_query object or NULL on failure
 */
rasqal_query*
rasqal_new_query(const char *name, const unsigned char *uri) {
  rasqal_query_engine_factory* factory;
  rasqal_query* query;
  raptor_uri_handler *uri_handler;
  void *uri_context;

  factory=rasqal_get_query_engine_factory(name, uri);
  if(!factory)
    return NULL;

  query=(rasqal_query*)RASQAL_CALLOC(rasqal_query, 1, 
                                         sizeof(rasqal_query));
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

  if(query->selects)
    raptor_free_sequence(query->selects);
  if(query->sources)
    raptor_free_sequence(query->sources);
  if(query->triples)
    raptor_free_sequence(query->triples);
  if(query->constraints)
    raptor_free_sequence(query->constraints);
  if(query->prefixes)
    raptor_free_sequence(query->prefixes);
  if(query->ordered_triples)
    raptor_free_sequence(query->ordered_triples);

  if(query->variable_names)
    RASQAL_FREE(cstrings, query->variable_names);
  
  if(query->binding_values)
    RASQAL_FREE(cstrings, query->binding_values);
  
  if(query->variables)
    RASQAL_FREE(vararray, query->variables);

  if(query->variables_sequence)
    raptor_free_sequence(query->variables_sequence);

  if(query->constraints_expression)
    rasqal_free_expression(query->constraints_expression);


  RASQAL_FREE(rasqal_query, query);
}


/* Methods */

/**
 * rasqal_get_name: Return the short name for the query
 * @query: &rasqal_query query object
 **/
const char*
rasqal_get_name(rasqal_query *query)
{
  return query->factory->name;
}


/**
 * rasqal_get_label: Return a readable label for the query
 * @query: &rasqal_query query object
 **/
const char*
rasqal_get_label(rasqal_query *query)
{
  return query->factory->label;
}


/**
 * rasqal_query_add_source: Add a source URI to the query
 * @query: &rasqal_query query object
 * @uri: &raptor_uri uri
 **/
void
rasqal_query_add_source(rasqal_query* query, raptor_uri* uri) {
  raptor_sequence_shift(query->sources, (void*)uri);
}


/**
 * rasqal_query_get_source_sequence: Get the sequence of source URIs
 * @query: &rasqal_query query object
 *
 * Return value: a &raptor_sequence of &raptor_uri pointers.
 **/
raptor_sequence*
rasqal_query_get_source_sequence(rasqal_query* query) {
  return query->sources;
}


/**
 * rasqal_query_get_source: Get a source URI in the sequence of sources
 * @query: &rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Return value: a &raptor_uri pointer or NULL if out of the sequence range
 **/
raptor_uri*
rasqal_query_get_source(rasqal_query* query, int idx) {
  return (raptor_uri*)raptor_sequence_get_at(query->sources, idx);
}


/**
 * rasqal_query_add_variable: Add a binding variable to the query
 * @query: &rasqal_query query object
 * @var: &rasqal_variable variable
 *
 * See also rasqal_query_set_variable which assigns or removes a value to
 * a previously added variable in the query.
 **/
void
rasqal_query_add_variable(rasqal_query* query, rasqal_variable* var) {
  raptor_sequence_shift(query->selects, (void*)var);
}


/**
 * rasqal_query_get_variable_sequence: Get the sequence of variables to bind in the query
 * @query: &rasqal_query query object
 *
 * Return value: a &raptor_sequence of &rasqal_variable pointers.
 **/
raptor_sequence*
rasqal_query_get_variable_sequence(rasqal_query* query) {
  return query->selects;
}


/**
 * rasqal_query_get_variable: Get a variable in the sequence of variables to bind
 * @query: &rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Return value: a &rasqal_variable pointer or NULL if out of the sequence range
 **/
rasqal_variable*
rasqal_query_get_variable(rasqal_query* query, int idx) {
  return (rasqal_variable*)raptor_sequence_get_at(query->selects, idx);
}


/**
 * rasqal_query_has_variable: Find if the named variable is in the sequence of variables to bind
 * @query: &rasqal_query query object
 * @name: variable name
 *
 * Return value: non-0 if the variable name was found.
 **/
int
rasqal_query_has_variable(rasqal_query* query, const char *name) {
  int i;

  for(i=0; i< raptor_sequence_size(query->selects); i++) {
    rasqal_variable* v=raptor_sequence_get_at(query->selects, i);
    if(!strcmp(v->name, name))
      return 1;
  }
  return 0;
}


/**
 * rasqal_query_set_variable: Add a binding variable to the query
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
rasqal_query_set_variable(rasqal_query* query, const char *name,
                          rasqal_literal* value) {
  int i;

  for(i=0; i< raptor_sequence_size(query->selects); i++) {
    rasqal_variable* v=raptor_sequence_get_at(query->selects, i);
    if(!strcmp(v->name, name)) {
      if(v->value)
        rasqal_free_literal(v->value);
      v->value=value;
      return 0;
    }
  }
  return 1;
}


/**
 * rasqal_query_add_triple: Add a matching triple to the query
 * @query: &rasqal_query query object
 * @triple: &rasqal_triple triple
 *
 **/
void
rasqal_query_add_triple(rasqal_query* query, rasqal_triple* triple) {
  raptor_sequence_shift(query->triples, (void*)triple);
}


/**
 * rasqal_query_get_triple_sequence: Get the sequence of matching triples in the query
 * @query: &rasqal_query query object
 *
 * Return value: a &raptor_sequence of &rasqal_triple pointers.
 **/
raptor_sequence*
rasqal_query_get_triple_sequence(rasqal_query* query) {
  return query->triples;
}


/**
 * rasqal_query_get_triple: Get a triple in the sequence of matching triples in the query
 * @query: &rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Return value: a &rasqal_triple pointer or NULL if out of the sequence range
 **/
rasqal_triple*
rasqal_query_get_triple(rasqal_query* query, int idx) {
  return (rasqal_triple*)raptor_sequence_get_at(query->triples, idx);
}


/**
 * rasqal_query_add_constraint: Add a constraint expression to the query
 * @query: &rasqal_query query object
 * @expr: &rasqal_expression expr
 *
 **/
void
rasqal_query_add_constraint(rasqal_query* query, rasqal_expression* expr) {
  raptor_sequence_shift(query->constraints, (void*)expr);
}


/**
 * rasqal_query_get_constraint_sequence: Get the sequence of constraints expressions in the query
 * @query: &rasqal_query query object
 *
 * Return value: a &raptor_sequence of &rasqal_expression pointers.
 **/
raptor_sequence*
rasqal_query_get_constraint_sequence(rasqal_query* query) {
  return query->constraints;
}


/**
 * rasqal_query_get_constraint: Get a constraint in the sequence of constraint expressions in the query
 * @query: &rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Return value: a &rasqal_expression pointer or NULL if out of the sequence range
 **/
rasqal_expression*
rasqal_query_get_constraint(rasqal_query* query, int idx) {
  return (rasqal_expression*)raptor_sequence_get_at(query->constraints, idx);
}


/**
 * rasqal_query_add_prefix: Add a namespace prefix to the query
 * @query: &rasqal_query query object
 * @prefix: &rasqal_prefix namespace prefix, URI
 *
 **/
void
rasqal_query_add_prefix(rasqal_query* query, rasqal_prefix* prefix) {
  raptor_sequence_shift(query->prefixes, (void*)prefix);
}


/**
 * rasqal_query_get_prefix_sequence: Get the sequence of namespace prefixes in the query
 * @query: &rasqal_query query object
 *
 * Return value: a &raptor_sequence of &rasqal_prefix pointers.
 **/
raptor_sequence*
rasqal_query_get_prefix_sequence(rasqal_query* query) {
  return query->prefixes;
}


/**
 * rasqal_query_get_prefix: Get a prefix in the sequence of namespsace prefixes in the query
 * @query: &rasqal_query query object
 * @idx: index into the sequence (0 or larger)
 *
 * Return value: a &rasqal_prefix pointer or NULL if out of the sequence range
 **/
rasqal_prefix*
rasqal_query_get_prefix(rasqal_query* query, int idx) {
  return (rasqal_prefix*)raptor_sequence_get_at(query->prefixes, idx);
}


/**
 * rasqal_query_prepare: Prepare a query - typically parse it
 * @query: the &rasqal_query object
 * @query_string: the query string
 * @base_uri: base URI of query string (optional)
 * 
 * Some query languages may require a base URI to resolve any
 * relative URIs in the query string.  If this is not given,
 * the current directory int the filesystem is used as the base URI.
 *
 * Return value: non-0 on failure.
 **/
int
rasqal_query_prepare(rasqal_query *query,
                     const unsigned char *query_string,
                     raptor_uri *base_uri)
{
  int rc=0;
  
  if(query->failed || query->finished)
    return 1;

  if(query->prepared)
    return 1;
  query->prepared=1;
  
  query->query_string=(char*)RASQAL_MALLOC(cstring, strlen(query_string)+1);
  strcpy((char*)query->query_string, (const char*)query_string);

  if(base_uri)
    base_uri=raptor_uri_copy(base_uri);
  else {
    const char *uri_string=raptor_uri_filename_to_uri_string("");
    base_uri=raptor_new_uri(uri_string);
    SYSTEM_FREE((void*)uri_string);
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
 * rasqal_query_execute: excute a query - run and return results
 * @query: the &rasqal_query object
 *
 * return value: non-0 on failure.
 **/
int
rasqal_query_execute(rasqal_query *query)
{
  int rc=0;
  
  if(query->failed || query->finished)
    return 1;

  if(query->executed)
    return 1;
  query->executed=1;
  
  rc=rasqal_engine_execute_init(query);
  if(rc) {
    query->failed=1;
    return rc;
  }

  if(query->factory->execute) {
    rc=query->factory->execute(query);
    if(rc) {
      query->failed=1;
      return rc;
    }
  }

  rasqal_query_next_result(query);

  return query->failed;
}


/* Utility methods */
void
rasqal_query_print(rasqal_query* query, FILE *fh) {
  fprintf(fh, "selects: ");
  raptor_sequence_print(query->selects, fh);
  fprintf(fh, "\nsources: ");
  raptor_sequence_print(query->sources, fh);
  fprintf(fh, "\ntriples: ");
  raptor_sequence_print(query->triples, fh);
  if(query->ordered_triples) {
    fprintf(fh, "\nordered triples: ");
    raptor_sequence_print(query->ordered_triples, fh);
  }
  fprintf(fh, "\nconstraints: ");
  raptor_sequence_print(query->constraints, fh);
  fprintf(fh, "\nprefixes: ");
  raptor_sequence_print(query->prefixes, fh);
  fputc('\n', fh);
}


/**
 * rasqal_query_get_result_count: Get number of bindings so far
 * @query: &rasqal_query query
 * 
 * Return value: number of bindings found so far
 **/
int
rasqal_query_get_result_count(rasqal_query *query) {
  return query->result_count;
}


/**
 * rasqal_query_results_finished: Find out if binding results are exhausted
 * @query: &rasqal_query query
 * 
 * Return value: non-0 if results are finished or query failed
 **/
int
rasqal_query_results_finished(rasqal_query *query) {
  return (query->failed || query->finished);
}


/**
 * rasqal_query_get_result_bindings: Get all binding names, values for current result
 * @query: &rasqal_query query
 * @names: pointer to an array of binding names (or NULL)
 * @values: pointer to an array of binding value &rasqal_literal (or NULL)
 * 
 * If either of the pointers is not NULL, pointers to shared copies
 * of the binding names or values are returned.
 * 
 * Return value: non-0 if the assignment failed
 **/
int
rasqal_query_get_result_bindings(rasqal_query *query,
                                 const char ***names, 
                                 rasqal_literal ***values) {
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
 * rasqal_query_get_result_binding_value: Get one binding value for the current result
 * @query: &rasqal_query query
 * @offset: offset of binding name into array of known names
 * 
 * Return value: a pointer to a shared &rasqal_literal binding value or NULL on failure
 **/
rasqal_literal*
rasqal_query_get_result_binding_value(rasqal_query *query, int offset) {
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
 * rasqal_query_get_result_binding_name: Get binding name for the current result
 * @query: &rasqal_query query
 * @offset: offset of binding name into array of known names
 * 
 * Return value: a pointer to a shared copy of the binding name or NULL on failure
 **/
const char*
rasqal_query_get_result_binding_name(rasqal_query *query, int offset) {
  if(query->finished)
    return NULL;

  if(offset < 0 || offset > query->select_variables_count-1)
    return NULL;
  
  return query->variables[offset]->name;
}


/**
 * rasqal_query_get_result_binding_by_name: Get one binding value for a given name in the current result
 * @query: &rasqal_query query
 * @name: variable name
 * 
 * Return value: a pointer to a shared &rasqal_literal binding value or NULL on failure
 **/
rasqal_literal*
rasqal_query_get_result_binding_by_name(rasqal_query *query,
                                        const char *name) {
  int offset= -1;
  int i;
  
  if(query->finished)
    return NULL;

  for(i=0; i< query->select_variables_count; i++) {
    if(!strcmp(name, query->variables[i]->name)) {
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
 * rasqal_query_next_result: Move to the next result
 * @query: &rasqal_query query
 * 
 * Return value: non-0 if failed or results exhausted
 **/
int
rasqal_query_next_result(rasqal_query *query) {
  int rc;
  
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


void*
rasqal_query_get_user_data(rasqal_query *query) {
  return query->user_data;
}


void
rasqal_query_set_user_data(rasqal_query *query, void *user_data) {
  query->user_data=user_data;
}

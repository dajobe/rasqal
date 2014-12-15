/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_query_results.c - Rasqal RDF Query Results
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

#ifndef STANDALONE

/*
 *
 * Query Results Class Internals
 *
 * This class provides the abstraction for query results in different
 * forms.  The forms can be either a sequence of variable bindings,
 * set of RDF triples, boolean value or a syntax.
 *
 * Query results can be created as a result of a #rasqal_query
 * execution using rasqal_query_execute() or as an independent result
 * set constructed from a query results syntax such as the SPARQL XML
 * results format via the #rasqal_query_results_formatter class.
 *
 * The query results constructor rasqal_new_query_results() takes
 * a world to use, an optional query, the type of result as well
 * as a variable table to operate on.  If the query is given, then
 * that is used to handle limit, offset and triple construction,
 * otherwise the result set is standalone and not associated with
 * a query.
 *
 * The variables table is used for the variables that will appear in
 * the result rows in the result set.  The query results module does
 * not own any variable information, all API calls are delegated to
 * the variables table.
 * 
 * If the rasqal_new_query_results_from_query_execution() is used to
 * make a query results from a query structure via executing the
 * query, it initialises a execution engine via the
 * #rasqal_query_execution_factory 'execute_init' factory method.
 * This method also determines whether the entire results need to be
 * (or a requested to be) obtained in one go, and if so, they are
 * done during construction.
 *
 * The user API to getting query results is primarily to get variable
 * bindings - a sequence of variable:value (also called #rasqal_row
 * internally), RDF triples, a boolean value or a syntax.  
 *
 * The variable bindings are generated from the execution engine by
 * retrieving #rasqal_row either one-by-one using the get_row method
 * or getting the entire result at once with the get_all_rows method.
 *
 * In the case of getting the entire result the rows are stored as a
 * sqeuence inside the #rasqal_query_results and returned one-by-one
 * from there, respecting any limit and offset.
 *
 * The RDF triples and boolean value results are generated from the
 * variable bindings (#rasqal_row) inside this class.  The underlying
 * execution engine only knows about rows.
 *
 * The class also handles several other results-specific methods such
 * as getting variable binding names, values by name, counts of
 * number of results, writing a query results as a syntax (in a
 * simple fashion), read a query results from a syntax.
 */

static int rasqal_query_results_execute_and_store_results(rasqal_query_results* query_results);
static void rasqal_query_results_update_query_bindings(rasqal_query_results* query_results, rasqal_query *query);


/*
 * A query result for some query
 */
struct rasqal_query_results_s {
  rasqal_world* world;

  /* type of query result (bindings, boolean, graph or syntax) */
  rasqal_query_results_type type;
  
  /* non-0 if have read all (variable binding) results */
  int finished;

  /* non-0 if query has been executed */
  int executed;

  /* non 0 if query had fatal error and cannot return results */
  int failed;

  /* query that this was executed over */
  rasqal_query* query;

  /* how many (variable bindings) results found so far */
  int result_count;

  /* execution data for execution engine. owned by this object */
  void* execution_data;

  /* current row of results */
  rasqal_row* row;

  /* boolean ASK result >0 true, 0 false or -1 uninitialised */
  int ask_result;

  /* boolean: non-0 to store query results rather than lazy eval */
  int store_results;

  /* current triple in the sequence of triples 'constructs' or -1 */
  int current_triple_result;

  /* constructed triple result - shared and updated for each triple */
  raptor_statement result_triple;

  /* sequence of stored results */
  raptor_sequence* results_sequence;

  /* size of result row fields:
   * row->results, row->values
   */
  int size;

  /* Execution engine used here */
  const rasqal_query_execution_factory* execution_factory;

  /* Variables table for variables in result rows */
  rasqal_variables_table* vars_table;

  /* non-0 if @vars_table has been initialized from first row */
  int vars_table_init;
};
    

int
rasqal_init_query_results(void)
{
  return 0;
}


void
rasqal_finish_query_results(void)
{
}


/**
 * rasqal_new_query_results2:
 * @world: rasqal world object
 * @query: query object (or NULL)
 * @type: query results (expected) type
 * 
 * Constructor - create a new query results set
 *
 * The @query may be NULL for result set objects that are standalone
 * and not attached to any particular query
 *
 * Return value: a new query result object or NULL on failure
 **/
rasqal_query_results*  
rasqal_new_query_results2(rasqal_world* world,
                          rasqal_query* query,
                          rasqal_query_results_type type)
{
  rasqal_query_results* query_results;
    
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);

  query_results = RASQAL_CALLOC(rasqal_query_results*, 1, sizeof(*query_results));
  if(!query_results)
    return NULL;
  
  query_results->vars_table = rasqal_new_variables_table(world);
  if(!query_results->vars_table) {
    RASQAL_FREE(rasqal_query_results, query_results);
    return NULL;
  }

  query_results->world = world;
  query_results->type = type;
  query_results->finished = 0;
  query_results->executed = 0;
  query_results->failed = 0;
  query_results->query = query;
  query_results->result_count = 0;
  query_results->execution_data = NULL;
  query_results->row = NULL;
  query_results->ask_result = -1; 
  query_results->store_results = 0; 
  query_results->current_triple_result = -1;

  /* initialize static query_results->result_triple */
  raptor_statement_init(&query_results->result_triple, world->raptor_world_ptr);

  query_results->results_sequence = NULL;
  query_results->size = 0;

  return query_results;
}


/**
 * rasqal_new_query_results:
 * @world: rasqal world object
 * @query: query object (or NULL)
 * @type: query results (expected) type
 * @vars_table: This parameter is *IGNORED*
 *
 * Constructor - create a new query results set
 *
 * @Deprecated for rasqal_new_query_results2() that loses the unused argument.
 *
 * The @query may be NULL for result set objects that are standalone
 * and not attached to any particular query
 *
 * Return value: a new query result object or NULL on failure
 **/
rasqal_query_results*
rasqal_new_query_results(rasqal_world* world,
                         rasqal_query* query,
                         rasqal_query_results_type type,
                         rasqal_variables_table* vars_table)
{
  return rasqal_new_query_results2(world, query, type);
}


/**
 * rasqal_new_query_results_from_string:
 * @world: rasqal world object
 * @type: query results (expected) type; typically #RASQAL_QUERY_RESULTS_BINDINGS
 * @base_uri: base URI of query results format (or NULL)
 * @string: query results string
 * @string_len: length of @string (or 0 to calculate it here)
 *
 * Constructor - create a new query results set from a results format string
 *
 * Return value: a new query result object or NULL on failure
 **/
rasqal_query_results*
rasqal_new_query_results_from_string(rasqal_world* world,
                                     rasqal_query_results_type type,
                                     raptor_uri* base_uri,
                                     const char* string,
                                     size_t string_len)
{
  int rc;
  raptor_iostream* iostr = NULL;
  rasqal_query_results_formatter* formatter = NULL;
  rasqal_query_results* results = NULL;
  raptor_world *raptor_world_ptr;
  const char* formatter_name;
  const unsigned char* id = NULL;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(string, const char*, NULL);

  if(!string_len)
    string_len = strlen(string);

  raptor_world_ptr = rasqal_world_get_raptor(world);

  results = rasqal_new_query_results2(world, NULL, type);
  if(!results)
    goto failed;

  iostr = raptor_new_iostream_from_string(raptor_world_ptr,
                                          RASQAL_GOOD_CAST(void*, string),
                                          string_len);
  if(!iostr)
    goto failed;

  if(base_uri)
    id = raptor_uri_as_string(base_uri);

  formatter_name =
    rasqal_world_guess_query_results_format_name(world,
                                                 base_uri,
                                                 NULL /* mime_type */,
                                                 RASQAL_GOOD_CAST(const unsigned char*, string),
                                                 string_len,
                                                 id);
  
  formatter = rasqal_new_query_results_formatter(world,
                                                 formatter_name,
                                                 NULL /* mime type */,
                                                 NULL /* uri */);
  if(!formatter)
    goto failed;

  rc = rasqal_query_results_formatter_read(world, iostr, formatter,
                                           results, base_uri);
  if(rc)
    goto failed;

  /* success */
  goto tidy;

  failed:
  if(results) {
    rasqal_free_query_results(results);
    results = NULL;
  }

  tidy:
  if(formatter)
    rasqal_free_query_results_formatter(formatter);

  if(iostr)
    raptor_free_iostream(iostr);

  return results;
}


/**
 * rasqal_query_results_execute_with_engine:
 * @query_results: the #rasqal_query_results object
 * @engine: execution factory
 * @store_results: non-0 to store query results
 *
 * INTERNAL - Create a new query results set executing a prepared query with the given execution engine
 *
 * return value: non-0 on failure
 **/
int
rasqal_query_results_execute_with_engine(rasqal_query_results* query_results,
                                         const rasqal_query_execution_factory* engine,
                                         int store_results)
{
  int rc = 0;
  size_t ex_data_size;
  rasqal_query* query;
  

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, 1);
  
  query = query_results->query;
  
  if(query->failed)
    return 1;

  query_results->execution_factory = engine;
  
  /* set executed flag early to enable cleanup on error */
  query_results->executed = 1;

  /* ensure stored results are present if ordering or distincting are being done */
  query_results->store_results = (store_results ||
                                  rasqal_query_get_order_conditions_sequence(query) ||
                                  rasqal_query_get_distinct(query));
  
  ex_data_size = query_results->execution_factory->execution_data_size;
  if(ex_data_size > 0) {
    query_results->execution_data = RASQAL_CALLOC(void*, 1, ex_data_size);

    if(!query_results->execution_data)
      return 1;
  } else
    query_results->execution_data = NULL;

  /* Update the current datetime once per query execution */
  rasqal_world_reset_now(query->world);
  
  if(query_results->execution_factory->execute_init) {
    rasqal_engine_error execution_error = RASQAL_ENGINE_OK;
    int execution_flags = 0;

    if(query_results->store_results)
      execution_flags |= 1;

    rc = query_results->execution_factory->execute_init(query_results->execution_data, query, query_results, execution_flags, &execution_error);

    if(rc || execution_error != RASQAL_ENGINE_OK) {
      query_results->failed = 1;
      return 1;
    }
  }

#ifdef RASQAL_DEBUG
  RASQAL_DEBUG1("After execute_init, query is now:\n");
  rasqal_query_print(query, stderr);
#endif

  /* Choose either to execute all now and store OR do it on demand (lazy) */
  if(query_results->store_results)
    rc = rasqal_query_results_execute_and_store_results(query_results);

  return rc;
}


/**
 * rasqal_free_query_results:
 * @query_results: #rasqal_query_results object
 *
 * Destructor - destroy a rasqal_query_results.
 *
 **/
void
rasqal_free_query_results(rasqal_query_results* query_results)
{
  rasqal_query* query;

  if(!query_results)
    return;

  query = query_results->query;

  if(query_results->executed) {
    if(query_results->execution_factory->execute_finish) {
      rasqal_engine_error execution_error = RASQAL_ENGINE_OK;

      query_results->execution_factory->execute_finish(query_results->execution_data, &execution_error);
      /* ignoring failure of execute_finish */
    }
  }

  if(query_results->execution_data)
    RASQAL_FREE(rasqal_engine_execution_data, query_results->execution_data);

  if(query_results->row)
    rasqal_free_row(query_results->row);

  if(query_results->results_sequence)
    raptor_free_sequence(query_results->results_sequence);

  /* free terms owned by static query_results->result_triple */
  raptor_free_statement(&query_results->result_triple);

  if(query_results->vars_table)
    rasqal_free_variables_table(query_results->vars_table);

  if(query)
    rasqal_query_remove_query_result(query, query_results);

  RASQAL_FREE(rasqal_query_results, query_results);
}


/**
 * rasqal_query_results_get_query:
 * @query_results: #rasqal_query_results object
 *
 * Get thq query associated with this query result
 * 
 * Return value: shared pointer to query object
 **/
rasqal_query*
rasqal_query_results_get_query(rasqal_query_results* query_results)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, NULL);

  return query_results->query;
}


/**
 * rasqal_query_results_get_type:
 * @query_results: #rasqal_query_results object
 *
 * Get query results type
 * 
 * Return value: non-0 if true
 **/
rasqal_query_results_type
rasqal_query_results_get_type(rasqal_query_results* query_results)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, (rasqal_query_results_type)0);

  return query_results->type;
}


static const char* const rasqal_query_results_type_labels[RASQAL_QUERY_RESULTS_LAST + 1] = {
  "Bindings",
  "Boolean",
  "Graph",
  "Syntax",
  "Unknown"
};



/**
 * rasqal_query_results_type_label:
 * @type: #rasqal_query_results_type type
 *
 * Get a label for a query results type
 *
 * Return value: label or NULL on failure (invalid type)
 **/
const char*
rasqal_query_results_type_label(rasqal_query_results_type type)
{
  if(type > RASQAL_QUERY_RESULTS_LAST)
    type = RASQAL_QUERY_RESULTS_UNKNOWN;

  return rasqal_query_results_type_labels[RASQAL_GOOD_CAST(int, type)];
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
rasqal_query_results_is_bindings(rasqal_query_results* query_results)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, 0);

  return (query_results->type == RASQAL_QUERY_RESULTS_BINDINGS);
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
rasqal_query_results_is_boolean(rasqal_query_results* query_results)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, 0);

  return (query_results->type == RASQAL_QUERY_RESULTS_BOOLEAN);
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
rasqal_query_results_is_graph(rasqal_query_results* query_results)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, 0);

  return (query_results->type == RASQAL_QUERY_RESULTS_GRAPH);
}


/**
 * rasqal_query_results_is_syntax:
 * @query_results: #rasqal_query_results object
 *
 * Test if the rasqal_query_results is a syntax.
 *
 * Many of the query results may be formatted as a syntax using the
 * #rasqal_query_formatter class however this function returns true
 * if a syntax result was specifically requested.
 * 
 * Return value: non-0 if true
 **/
int
rasqal_query_results_is_syntax(rasqal_query_results* query_results)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, 0);

  return (query_results->type == RASQAL_QUERY_RESULTS_SYNTAX);
}


/**
 * rasqal_query_check_limit_offset_core:
 * @result_offset: offset to check
 * @limit: limit
 * @offset: offset
 *
 * INTERNAL - Check the result_offset is in the limit and offset range if any.
 *
 * Return value: before range -1, in range 0, after range 1
 */
int
rasqal_query_check_limit_offset_core(int result_offset,
                                     int limit,
                                     int offset)
{
  if(result_offset < 0)
    return -1;

  if(offset > 0) {
    /* offset */
    if(result_offset <= offset)
      return -1;
    
    if(limit >= 0) {
      /* offset and limit */
      if(result_offset > (offset + limit)) {
        return 1;
      }
    }
    
  } else if(limit >= 0) {
    /* limit */
    if(result_offset > limit) {
      return 1;
    }
  }

  return 0;
}


/**
 * rasqal_query_check_limit_offset:
 * @query_results: query results object
 * @result_offset: offset to check
 *
 * INTERNAL - Check the query result count is in the limit and offset range if any.
 *
 * Return value: before range -1, in range 0, after range 1
 */
int
rasqal_query_check_limit_offset(rasqal_query* query,
                                int result_offset)
{
  int limit;
  int offset;
  
  if(!query)
    return 0;

  if(result_offset < 0)
    return -1;

  limit = rasqal_query_get_limit(query);

  /* Ensure ASK queries never do more than one result */
  if(query->verb == RASQAL_QUERY_VERB_ASK)
    limit = 1;

  offset = rasqal_query_get_offset(query);
  
  return rasqal_query_check_limit_offset_core(result_offset, limit, offset);
}


/**
 * rasqal_query_results_get_row_from_saved:
 * @query_results: Query results to execute
 *
 * INTERNAL - Get next result row from a stored query result sequence
 *
 * Return value: result row or NULL if finished or failed
 */
static rasqal_row*
rasqal_query_results_get_row_from_saved(rasqal_query_results* query_results)
{
  rasqal_query* query = query_results->query;
  int size;
  rasqal_row* row = NULL;
  
  size = raptor_sequence_size(query_results->results_sequence);
  
  while(1) {
    int check;
    
    if(query_results->result_count >= size) {
      query_results->finished = 1;
      break;
    }
    
    query_results->result_count++;
    
    check = rasqal_query_check_limit_offset(query, query_results->result_count);
    
    /* finished if beyond result range */
    if(check > 0) {
      query_results->finished = 1;
      query_results->result_count--;
      break;
    }
    
    /* continue if before start of result range */
    if(check < 0)
      continue;
    
    /* else got result */
    row = (rasqal_row*)raptor_sequence_get_at(query_results->results_sequence,
                                              query_results->result_count - 1);
    
    if(row) {
      row = rasqal_new_row_from_row(row);
      
      /* stored results may not be canonicalized yet - do it lazily */
      rasqal_row_to_nodes(row);

      query_results->row = row;
      
      if(query && query->constructs)
        rasqal_query_results_update_query_bindings(query_results, query);
    }
    break;
  }
  
  return row;
}


/**
 * rasqal_query_results_ensure_have_row_internal:
 * @query_results: #rasqal_query_results query_results
 *
 * INTERNAL - Ensure there is a row in the query results by getting it from the generator/stored list
 *
 * If one already is held, nothing is done.  It is assumed
 * that @query_results is not NULL and the query is neither finished
 * nor failed.
 *
 * Return value: non-0 if failed or results exhausted
 **/
static int
rasqal_query_results_ensure_have_row_internal(rasqal_query_results* query_results)
{
  /* already have row */
  if(query_results->row)
    return 0;
  
  if(query_results->results_sequence) {
    query_results->row = rasqal_query_results_get_row_from_saved(query_results);
  } else if(query_results->execution_factory &&
            query_results->execution_factory->get_row) {
    rasqal_engine_error execution_error = RASQAL_ENGINE_OK;

    /* handle limit/offset for incremental get_row() */
    while(1) {
      int check;
      
      query_results->row = query_results->execution_factory->get_row(query_results->execution_data, &execution_error);
      if(execution_error == RASQAL_ENGINE_FAILED) {
        query_results->failed = 1;
        break;
      }

      if(execution_error != RASQAL_ENGINE_OK)
        break;

      query_results->result_count++;
      
      check = rasqal_query_check_limit_offset(query_results->query,
                                              query_results->result_count);
      
      /* finished if beyond result range */
      if(check > 0) {
        query_results->finished = 1;
        query_results->result_count--;

        /* empty row to trigger finished */
        rasqal_free_row(query_results->row); query_results->row = NULL;
        break;
      }
      
      /* continue if before start of result range */
      if(check < 0) {
        /* empty row because continuing */
        rasqal_free_row(query_results->row); query_results->row = NULL;
        continue;
      }

      /* got a row */
      break;

    } /* end while */
    
  }
  
  if(query_results->row) {
    rasqal_row_to_nodes(query_results->row);
    query_results->size = query_results->row->size;
  } else
    query_results->finished = 1;

  if(query_results->row && !query_results->vars_table_init) {
    /* build variables table once from first row seen */
    int i;

    query_results->vars_table_init = 1;

    for(i = 0; 1; i++) {
      rasqal_variable* v;

      v = rasqal_row_get_variable_by_offset(query_results->row, i);
      if(!v)
        break;

      v = rasqal_variables_table_add2(query_results->vars_table,
                                      v->type,
                                      v->name, /* name len */ 0,
                                      /* value */ NULL);
      rasqal_free_variable(v);
    }
  }

  return (query_results->row == NULL);
}


/**
 * rasqal_query_results_get_current_row:
 * @query_results: query results object
 *
 * INTERNAL - Get the current query result as a row of values
 *
 * The returned row is shared and owned by query_results
 *
 * Return value: result row or NULL on failure
 */
rasqal_row*
rasqal_query_results_get_current_row(rasqal_query_results* query_results)
{
  if(!query_results || query_results->failed || query_results->finished)
    return NULL;
  
  if(!rasqal_query_results_is_bindings(query_results))
    return NULL;

  /* ensure we have a row */
  rasqal_query_results_ensure_have_row_internal(query_results);

  return query_results->row;
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
rasqal_query_results_get_count(rasqal_query_results* query_results)
{
  rasqal_query* query;
  int offset = -1;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, -1);

  if(query_results->failed)
    return -1;

  if(!rasqal_query_results_is_bindings(query_results))
    return -1;

  query = query_results->query;

  if(query)
    offset = rasqal_query_get_offset(query);

  if(query && offset > 0)
    return query_results->result_count - offset;
  else
    return query_results->result_count;
}


/*
 * rasqal_query_results_next_internal:
 * @query_results: #rasqal_query_results query_results
 *
 * INTERNAL - Move to the next result without query verb type checking
 * 
 * Return value: non-0 if failed or results exhausted
 **/
static int
rasqal_query_results_next_internal(rasqal_query_results* query_results)
{
  if(query_results->failed || query_results->finished)
    return 1;
  
  /* Remove any current row */
  if(query_results->row) {
    rasqal_free_row(query_results->row);
    query_results->row = NULL;
  }

  /* Now try to get a new one */
  return rasqal_query_results_ensure_have_row_internal(query_results);
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
rasqal_query_results_next(rasqal_query_results* query_results)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, 1);

  if(!rasqal_query_results_is_bindings(query_results))
    return 1;

  return rasqal_query_results_next_internal(query_results);
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
rasqal_query_results_finished(rasqal_query_results* query_results)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, 1);

  if(query_results->failed || query_results->finished)
    return 1;
  
  if(!rasqal_query_results_is_bindings(query_results))
    return 1;

  /* need to have at least tried to get a row once */
  if(!query_results->failed && !query_results->finished)
    rasqal_query_results_ensure_have_row_internal(query_results);
  
  return (query_results->failed || query_results->finished);
}


/**
 * rasqal_query_results_rewind:
 * @query_results: #rasqal_query_results query_results
 *
 * Rewind stored query results to start
 *
 * This requires rasqal_query_set_store_results() to be called before
 * query execution.
 * 
 * Return value: non-0 if rewinding is not available when results are not stored
 **/
int
rasqal_query_results_rewind(rasqal_query_results* query_results)
{
  int size;
  int limit = -1;
  int offset = -1;
  rasqal_query* query;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, 1);

  if(!query_results->results_sequence)
    return 1;

  size = raptor_sequence_size(query_results->results_sequence);

  /* This may be NULL for a static query result */
  query = query_results->query;

  if(query) {
    /* If the query failed, it remains failed */
    if(query->failed)
      return 1;
    
    limit = rasqal_query_get_limit(query);
    offset = rasqal_query_get_offset(query);
  }
  
  /* reset to first result */
  query_results->finished = (size == 0);
  
  if(query && !limit)
    query_results->finished = 1;
  
  if(!query_results->finished) {
    /* Reset to first result, index-1 into sequence of results */
    query_results->result_count = 0;
    
    /* skip past any OFFSET */
    if(query && offset > 0) {
      query_results->result_count += offset;
      
      if(query_results->result_count >= size)
        query_results->finished = 1;
    }
    
  }

    
  if(query_results->finished)
    query_results->result_count = 0;
  else {
    if(query && query->constructs)
      rasqal_query_results_update_query_bindings(query_results, query);
  }

  return 0;
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
rasqal_query_results_get_bindings(rasqal_query_results* query_results,
                                  const unsigned char ***names, 
                                  rasqal_literal ***values)
{
  rasqal_row* row;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, 1);

  if(!rasqal_query_results_is_bindings(query_results))
    return 1;
  
  row = rasqal_query_results_get_current_row(query_results);

  if(!row) {
      query_results->finished = 1;
      return 0;
  }

  if(names)
    *names = rasqal_variables_table_get_names(query_results->vars_table);
  
  if(values)
    *values = row->values;
    
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
rasqal_query_results_get_binding_value(rasqal_query_results* query_results, 
                                       int offset)
{
  rasqal_row* row;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, NULL);
  
  if(!rasqal_query_results_is_bindings(query_results))
    return NULL;
  
  if(offset < 0 || offset > query_results->size-1)
    return NULL;

  row = rasqal_query_results_get_current_row(query_results);
  if(row)
    return row->values[offset];

  query_results->finished = 1;
  return NULL;
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
rasqal_query_results_get_binding_name(rasqal_query_results* query_results, 
                                      int offset)
{
  rasqal_row* row;
  rasqal_variable* v;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, NULL);
  
  if(!rasqal_query_results_is_bindings(query_results)) 
    return NULL;
  
  row = rasqal_query_results_get_current_row(query_results);
  if(!row)
    return NULL;
  
  v = rasqal_variables_table_get(query_results->vars_table, offset);
  if(!v)
    return NULL;
  
  return v->name;
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
rasqal_query_results_get_binding_value_by_name(rasqal_query_results* query_results,
                                               const unsigned char *name)
{
  rasqal_row* row;
  rasqal_variable* v;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(name, char*, NULL);
  
  if(!rasqal_query_results_is_bindings(query_results))
    return NULL;
  
  row = rasqal_query_results_get_current_row(query_results);
  if(!row)
    return NULL;
  
  v = rasqal_variables_table_get_by_name(query_results->vars_table, 
                                         RASQAL_VARIABLE_TYPE_NORMAL, name);
  if(!v)
    return NULL;

  return row->values[v->offset];
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
rasqal_query_results_get_bindings_count(rasqal_query_results* query_results)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, -1);

  if(query_results->failed)
    return -1;
  
  if(!rasqal_query_results_is_bindings(query_results))
    return -1;

  /* ensures an attempt is made to get at least 1 row */
  rasqal_query_results_ensure_have_row_internal(query_results);
  
  return query_results->size;
}


static unsigned char*
rasqal_prefix_id(int prefix_id, const unsigned char *string)
{
  int tmpid = prefix_id;
  unsigned char* buffer;
  size_t length = strlen(RASQAL_GOOD_CAST(const char*, string)) + 4;  /* "r" +... + "q" +... \0 */

  while(tmpid /= 10)
    length++;
  
  buffer = RASQAL_MALLOC(unsigned char*, length);
  if(!buffer)
    return NULL;
  
  sprintf(RASQAL_GOOD_CAST(char*, buffer), "r%dq%s", prefix_id, string);
  
  return buffer;
}


static raptor_term*
rasqal_literal_to_result_term(rasqal_query_results* query_results,
                              rasqal_literal* l)
{
  rasqal_literal* nodel;
  raptor_term* t = NULL;
  unsigned char *nodeid;
  
  nodel = rasqal_literal_as_node(l);
  if(!nodel)
    return NULL;

  switch(nodel->type) {
    case RASQAL_LITERAL_URI:
      t = raptor_new_term_from_uri(query_results->world->raptor_world_ptr,
                                   nodel->value.uri);
      break;
      
    case RASQAL_LITERAL_BLANK:
      if(l->type == RASQAL_LITERAL_BLANK) {
        /* original was a genuine blank node not a variable with a
         * blank node value so make a new one every result, not every triple
         */
        nodeid = rasqal_prefix_id(query_results->result_count,
                                  nodel->string);
      } else {
        nodeid = RASQAL_MALLOC(unsigned char*, nodel->string_len + 1);
        if(nodeid)
          memcpy(nodeid, nodel->string, nodel->string_len + 1);
      }

      if(nodeid)
        l = rasqal_new_simple_literal(query_results->world,
                                      RASQAL_LITERAL_BLANK,
                                      nodeid);

      if(!nodeid || !l)
        goto done;
      
      t = raptor_new_term_from_blank(query_results->world->raptor_world_ptr,
                                     nodeid);
      rasqal_free_literal(l);
      break;
      
    case RASQAL_LITERAL_STRING:
      t = raptor_new_term_from_literal(query_results->world->raptor_world_ptr,
                                       nodel->string,
                                       nodel->datatype,
                                       RASQAL_GOOD_CAST(const unsigned char*, nodel->language));
      break;
      
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_VARIABLE:
    case RASQAL_LITERAL_DECIMAL:
    case RASQAL_LITERAL_DATE:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_UDT:
    case RASQAL_LITERAL_INTEGER_SUBTYPE:
      /* QNames should be gone by the time expression eval happens
       * Everything else is removed by rasqal_literal_as_node() above. 
       */
      
    case RASQAL_LITERAL_UNKNOWN:
    default:
      break;
  }
  
  
  done:
  if(nodel)
    rasqal_free_literal(nodel);

  return t;
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
rasqal_query_results_get_triple(rasqal_query_results* query_results)
{
  rasqal_query* query;
  rasqal_triple *t;
  raptor_statement *rs = NULL;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, NULL);

 if(query_results->failed || query_results->finished)
    return NULL;
  
  if(!rasqal_query_results_is_graph(query_results))
    return NULL;
  
  query = query_results->query;
  if(!query)
    return NULL;
  
  if(query->verb == RASQAL_QUERY_VERB_DESCRIBE)
    return NULL;

 
  /* ensure we have a row to work on */
  if(rasqal_query_results_ensure_have_row_internal(query_results))
    return NULL;

  while(1) {
    int skip = 0;

    if(query_results->current_triple_result < 0)
      query_results->current_triple_result = 0;

    t = (rasqal_triple*)raptor_sequence_get_at(query->constructs,
                                               query_results->current_triple_result);

    rs = &query_results->result_triple;

    raptor_statement_clear(rs);

    rs->subject = rasqal_literal_to_result_term(query_results, t->subject);
    if(!rs->subject || rs->subject->type == RAPTOR_TERM_TYPE_LITERAL) {
      rasqal_log_warning_simple(query_results->world,
                                RASQAL_WARNING_LEVEL_BAD_TRIPLE,
                                &query->locator,
                                "Triple with non-RDF subject term skipped");
      skip = 1;
    } else {
      rs->predicate = rasqal_literal_to_result_term(query_results, t->predicate);
      if(!rs->predicate || rs->predicate->type != RAPTOR_TERM_TYPE_URI) {
        rasqal_log_warning_simple(query_results->world,
                                  RASQAL_WARNING_LEVEL_BAD_TRIPLE,
                                  &query->locator,
                                  "Triple with non-RDF predicate term skipped");
        skip = 1;
      } else {
        rs->object = rasqal_literal_to_result_term(query_results, t->object);
        if(!rs->object) {
          rasqal_log_warning_simple(query_results->world,
                                    RASQAL_WARNING_LEVEL_BAD_TRIPLE,
                                    &query->locator,
                                    "Triple with non-RDF object term skipped");
          skip = 1;
        } 
      }
    }

    if(!skip)
      /* got triple, return it */
      break;

    /* Have to move to next triple internally */
    if(rasqal_query_results_next_triple(query_results)) {
      /* end of results or failed */
      rs = NULL;
      break;
    }
  }
  
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
rasqal_query_results_next_triple(rasqal_query_results* query_results)
{
  rasqal_query* query;
  int rc = 0;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, 1);

  if(query_results->failed || query_results->finished)
    return 1;
  
  if(!rasqal_query_results_is_graph(query_results))
    return 1;
  
  query = query_results->query;
  if(!query)
    return 1;

  if(query->verb == RASQAL_QUERY_VERB_DESCRIBE)
    return 1;
  
  if(++query_results->current_triple_result >= raptor_sequence_size(query->constructs)) {
    if(rasqal_query_results_next_internal(query_results))
      return 1;
    
    query_results->current_triple_result = -1;
  }

  return rc;
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
 * Return value: boolean query result - >0 is true, 0 is false, <0 on error
 */
int
rasqal_query_results_get_boolean(rasqal_query_results* query_results)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, -1);

  if(query_results->failed)
    return -1;
  
  if(!rasqal_query_results_is_boolean(query_results))
    return -1;
  
  if(query_results->ask_result >= 0)
    return query_results->ask_result;

  rasqal_query_results_ensure_have_row_internal(query_results);
  
  query_results->ask_result = (query_results->result_count > 0) ? 1 : 0;
  query_results->finished = 1;
  
  return query_results->ask_result;
}


/**
 * rasqal_query_results_set_boolean:
 * @query_results: #rasqal_query_results query_results
 * @value: boolean value
 *
 * INTERNAL - Set boolean query result value.
 *
 * Return value: boolean query result - >0 is true, 0 is false, <0 on error
 */
int
rasqal_query_results_set_boolean(rasqal_query_results* query_results, int value)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, -1);

  if(query_results->failed)
    return -1;

  if(!rasqal_query_results_is_boolean(query_results))
    return -1;

  query_results->finished = 1;
  query_results->ask_result = value;
  return 0;
}


/**
 * rasqal_query_results_write:
 * @iostr: #raptor_iostream to write the query to
 * @results: #rasqal_query_results query results format
 * @name: format name (or NULL)
 * @mime_type: format mime type (or NULL)
 * @format_uri: #raptor_uri describing the format to write (or NULL for default)
 * @base_uri: #raptor_uri base URI of the output format
 *
 * Write the query results to an iostream in a format.
 * 
 * This uses the #rasqal_query_results_formatter class and the
 * rasqal_query_results_formatter_write() method to perform the
 * formatting.
 *
 * Note that after calling this method, the query results will be
 * empty and rasqal_query_results_finished() will return true (non-0)
 *
 * See rasqal_world_get_query_results_format_description() for obtaining the
 * supported format names, mime_types and URIs at run time.
 *
 * Return value: non-0 on failure
 **/
int
rasqal_query_results_write(raptor_iostream *iostr,
                           rasqal_query_results* results,
                           const char *name,
                           const char *mime_type,
                           raptor_uri *format_uri,
                           raptor_uri *base_uri)
{
  rasqal_query_results_formatter *formatter;
  int status;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(iostr, raptor_iostream, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(results, rasqal_query_results, 1);

  if(results->failed)
    return 1;

  formatter = rasqal_new_query_results_formatter(results->world, 
                                                 name, mime_type,
                                                 format_uri);
  if(!formatter)
    return 1;

  status = rasqal_query_results_formatter_write(iostr, formatter,
                                                results, base_uri);

  rasqal_free_query_results_formatter(formatter);
  return status;
}


/**
 * rasqal_query_results_read:
 * @iostr: #raptor_iostream to read the query from
 * @results: #rasqal_query_results query results format
 * @name: format name (or NULL)
 * @mime_type: format mime type (or NULL)
 * @format_uri: #raptor_uri describing the format to read (or NULL for default)
 * @base_uri: #raptor_uri base URI of the input format
 *
 * Read the query results from an iostream in a format.
 * 
 * This uses the #rasqal_query_results_formatter class
 * and the rasqal_query_results_formatter_read() method
 * to perform the formatting. 
 *
 * See rasqal_world_get_query_results_format_description() for
 * obtaining the supported format URIs at run time.
 *
 * Return value: non-0 on failure
 **/
int
rasqal_query_results_read(raptor_iostream *iostr,
                          rasqal_query_results* results,
                          const char *name,
                          const char *mime_type,
                          raptor_uri *format_uri,
                          raptor_uri *base_uri)
{
  rasqal_query_results_formatter *formatter;
  int status;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(iostr, raptor_iostream, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(results, rasqal_query_results, 1);

  if(results->failed)
    return 1;

  formatter = rasqal_new_query_results_formatter(results->world,
                                                 name, mime_type,
                                                 format_uri);
  if(!formatter)
    return 1;

  status = rasqal_query_results_formatter_read(results->world, iostr, formatter,
                                               results, base_uri);

  rasqal_free_query_results_formatter(formatter);
  return status;
}


/**
 * rasqal_query_results_add_row:
 * @query_results: query results object
 * @row: query result row
 *
 * Add a query result row to the sequence of result rows
 *
 * Return value: non-0 on failure
 */
int
rasqal_query_results_add_row(rasqal_query_results* query_results,
                             rasqal_row* row)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(row, rasqal_row, 1);

  if(!query_results->results_sequence) {
    query_results->results_sequence = raptor_new_sequence((raptor_data_free_handler)rasqal_free_row, (raptor_data_print_handler)rasqal_row_print);
    if(!query_results->results_sequence)
      return 1;
    
    query_results->result_count = 0;
  }

  row->offset = raptor_sequence_size(query_results->results_sequence);

  return raptor_sequence_push(query_results->results_sequence, row);
}


/**
 * rasqal_query_results_execute_and_store_results:
 * @query_results: query results object
 *
 * INTERNAL - Store all query result (rows) immediately
 *
 * Return value: non-0 on failure
 */
static int
rasqal_query_results_execute_and_store_results(rasqal_query_results* query_results)
{
  raptor_sequence* seq = NULL;

  if(query_results->results_sequence)
     raptor_free_sequence(query_results->results_sequence);

  if(query_results->execution_factory->get_all_rows) {
    rasqal_engine_error execution_error = RASQAL_ENGINE_OK;
    
    seq = query_results->execution_factory->get_all_rows(query_results->execution_data, &execution_error);
    if(execution_error == RASQAL_ENGINE_FAILED)
      query_results->failed = 1;
  }

  query_results->results_sequence = seq;

  if(!seq) {
    query_results->finished = 1;
  } else
    rasqal_query_results_rewind(query_results);

  return query_results->failed;
}


/*
 * rasqal_query_results_update_query_bindings:
 * @query_results: query results to read from
 * @query: query to set bindings to
 *
 * INTERNAL - bind the query variables to the values from the current query results row
 *
 * Used to handle query CONSTRUCT
 */
static void
rasqal_query_results_update_query_bindings(rasqal_query_results* query_results, rasqal_query* query)
{
  int i;
  int size;
  rasqal_row* row;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN(query_results, rasqal_query_results);

  rasqal_query_results_ensure_have_row_internal(query_results);

  row = query_results->row;
  if(!row) {
    query_results->finished = 1;
    return;
  }

  size = rasqal_variables_table_get_named_variables_count(query_results->vars_table);
  for(i = 0; i < size; i++) {
    rasqal_variable* srcv;
    rasqal_variable* v;
    /* source value is in row */
    rasqal_literal* value = row->values[i];

    /* source variable is in query results */
    srcv = rasqal_variables_table_get(query_results->vars_table, i);

    /* destination variable is in query */
    v = rasqal_variables_table_get_by_name(query->vars_table, srcv->type, srcv->name);
    if(v)
      rasqal_variable_set_value(v, rasqal_new_literal_from_literal(value));
    else {
      RASQAL_DEBUG2("Cannot bind query results variable %s into query", srcv->name);
    }
  }
}


void
rasqal_query_results_remove_query_reference(rasqal_query_results* query_results)
{
  rasqal_query* query;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN(query_results, rasqal_query_results);

  query = query_results->query;
  query_results->query = NULL;

  rasqal_free_query(query);
}


rasqal_variables_table*
rasqal_query_results_get_variables_table(rasqal_query_results* query_results)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, NULL);

  return query_results->vars_table;
}



rasqal_world*
rasqal_query_results_get_world(rasqal_query_results* query_results)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, NULL);

  return query_results->world;
}


/**
 * rasqal_query_results_get_row_by_offset:
 * @query_results: query result
 * @result_offset: index into result rows
 *
 * Get stored result row by an offset
 *
 * The result_offset index is 0-indexed into the subset of results
 * constrained by any query limit and offset.
 *
 * Return value: row or NULL if @result_offset is out of range
 */
rasqal_row*
rasqal_query_results_get_row_by_offset(rasqal_query_results* query_results,
                                       int result_offset)
{
  rasqal_query* query;
  int check;
  rasqal_row* row;
  int offset = 0;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(query_results, rasqal_query_results, NULL);

  if(!query_results->results_sequence)
    return NULL;

  if(result_offset < 0)
    return NULL;

  query = query_results->query;
  if(query)
    offset = rasqal_query_get_offset(query);

  /* Adjust 0-indexed to query results 1-indexed + query result offset */
  result_offset += 1 + offset;

  check = rasqal_query_check_limit_offset(query_results->query,
                                          result_offset);
  /* outside limit/offset range in some way */
  if(check < 0 || check > 0)
    return NULL;

  row = (rasqal_row*)raptor_sequence_get_at(query_results->results_sequence,
                                            result_offset - 1);
  if(row) {
    row = rasqal_new_row_from_row(row);

    /* stored results may not be canonicalized yet - do it lazily */
    rasqal_row_to_nodes(row);
  }
  
  return row;
}


struct rqr_context
{
  rasqal_query_results* results;

  /* size of @order - number of values */
  int size;

  /* Sequence of offsets to variables in lexical order  */
  int* order;
};


/**
 * rasqal_query_results_sort_compare_row:
 * @a: pointer to address of first #row
 * @b: pointer to address of second #row
 * @arg: query results pointer
 *
 * INTERNAL - compare two pointers to #row objects with user data arg
 *
 * Suitable for use as a compare function with raptor_sort_r() or
 * compatible.  Used by rasqal_query_results_sort().
 *
 * Return value: <0, 0 or >0 comparison
 */
static int
rasqal_query_results_sort_compare_row(const void *a, const void *b, void *arg)
{
  rasqal_row* row_a;
  rasqal_row* row_b;
  struct rqr_context* rqr;
  int result = 0;

  row_a = *(rasqal_row**)a;
  row_b = *(rasqal_row**)b;
  rqr = (struct rqr_context*)arg;

  result = rasqal_literal_array_compare_by_order(row_a->values, row_b->values,
                                                 rqr->order, row_a->size, 0);

  /* still equal?  make sort stable by using the original order */
  if(!result) {
    result = row_a->offset - row_b->offset;
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG2("Got equality result so using offsets, returning %d\n",
                  result);
#endif
  }

  return result;
}


int
rasqal_query_results_sort(rasqal_query_results* query_results)
{
  struct rqr_context rqr;

  if(query_results->execution_factory && !query_results->results_sequence) {
    int rc;
    
    rc = rasqal_query_results_execute_and_store_results(query_results);
    if(rc)
      return rc;
  }

  rqr.results = query_results;
  rqr.size = query_results->size;
  rqr.order = rasqal_variables_table_get_order(query_results->vars_table);
  if(!rqr.order)
    return 1;

  if(query_results->results_sequence) {
    int size = raptor_sequence_size(query_results->results_sequence);
    if(size > 1) {
#if RAPTOR_VERSION < 20015
      raptor_sequence *seq;
      void** array;
      size_t i;

      seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_row, (raptor_data_print_handler)rasqal_row_print);
      if(!seq) {
        RASQAL_FREE(int*, rqr.order);
        return 1;
      }

      array = rasqal_sequence_as_sorted(seq,
                                        rasqal_query_results_sort_compare_row,
                                        &rqr);
      if(!array) {
        raptor_free_sequence(seq);
        RASQAL_FREE(int*, rqr.order);
        return 1;
      }

      for(i = 0; i < RASQAL_GOOD_CAST(size_t, size); i++) {
        rasqal_row* row = rasqal_new_row_from_row(RASQAL_GOOD_CAST(rasqal_row*, array[i]));
        raptor_sequence_push(seq, row);
      }
      raptor_free_sequence(query_results->results_sequence);
      query_results->results_sequence = seq;
      RASQAL_FREE(void*, array);
#else
      raptor_sequence_sort_r(query_results->results_sequence,
                             rasqal_query_results_sort_compare_row,
                             &rqr);
#endif
    }
  }
  
  RASQAL_FREE(int*, rqr.order);
  return 0;
}

#endif /* not STANDALONE */



#ifdef STANDALONE

/* one more prototype */
int main(int argc, char *argv[]);

#define NTESTS 2

const struct {
  const char* qr_string;
  int expected_vars_count;
  int expected_rows_count;
  int expected_equality;
} expected_data[NTESTS] = {
  {
    "a\tb\tc\td\n\"a\"\t\"b\"\t\"c\"\t\"d\"\n",
    4, 1, 1
  },
  {
    "a,b,c,d,e,f\n\"a\",\"b\",\"c\",\"d\",\"e\",\"f\"\n",
    6, 1, 1
  }
};


#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
static void
print_bindings_results_simple(rasqal_query_results *results, FILE* output)
{
  while(!rasqal_query_results_finished(results)) {
    int i;

    fputs("row: [", output);
    for(i = 0; i < rasqal_query_results_get_bindings_count(results); i++) {
      const unsigned char *name;
      rasqal_literal *value;

      name = rasqal_query_results_get_binding_name(results, i);
      value = rasqal_query_results_get_binding_value(results, i);

      if(i > 0)
        fputs(", ", output);

      fprintf(output, "%s=", name);
      rasqal_literal_print(value, output);
    }
    fputs("]\n", output);

    rasqal_query_results_next(results);
  }
}
#endif

int
main(int argc, char *argv[])
{
  const char *program = rasqal_basename(argv[0]);
  rasqal_world* world = NULL;
  raptor_world* raptor_world_ptr;
  int failures = 0;
  int i;
  rasqal_query_results_type type = RASQAL_QUERY_RESULTS_BINDINGS;

  world = rasqal_new_world(); rasqal_world_open(world);

  raptor_world_ptr = rasqal_world_get_raptor(world);

  for(i = 0; i < NTESTS; i++) {
    raptor_uri* base_uri = raptor_new_uri(raptor_world_ptr,
                                          (const unsigned char*)"http://example.org/");
    rasqal_query_results *qr;
    int expected_vars_count = expected_data[i].expected_vars_count;
    int vars_count;

    qr = rasqal_new_query_results_from_string(world,
                                              type,
                                              base_uri,
                                              expected_data[i].qr_string,
                                              0);
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("Query result from string:");
    print_bindings_results_simple(first_qr, stderr);
    rasqal_query_results_rewind(first_qr);
#endif

    raptor_free_uri(base_uri);

    if(!qr) {
      fprintf(stderr, "%s: failed to create query results\n", program);
      failures++;
    } else {
      rasqal_variables_table* vt;
      
      vt = rasqal_query_results_get_variables_table(qr);
      vars_count = rasqal_variables_table_get_named_variables_count(vt);
      RASQAL_DEBUG4("%s: query results test %d returned %d vars\n", program, i,
                    vars_count);
      if(vars_count != expected_vars_count) {
        fprintf(stderr,
                "%s: FAILED query results test %d returned %d vars  expected %d vars\n",
                program, i, vars_count, expected_vars_count);
        failures++;
      }
    }

    if(qr)
      rasqal_free_query_results(qr);
  }

  if(world)
    rasqal_free_world(world);

  return failures;
}

#endif /* STANDALONE */

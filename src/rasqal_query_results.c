/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_query.c - Rasqal RDF Query Results
 *
 * $Id$
 *
 * Copyright (C) 2003-2006, David Beckett http://purl.org/net/dajobe/
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


static int rasqal_query_results_write_xml_20041221(raptor_iostream *iostr, rasqal_query_results* results, raptor_uri *base_uri);
static int rasqal_query_results_write_xml_result2(raptor_iostream *iostr, rasqal_query_results* results, raptor_uri *base_uri);
static int rasqal_query_results_write_xml_result3(raptor_iostream *iostr, rasqal_query_results* results, raptor_uri *base_uri);
static int rasqal_query_results_write_json1(raptor_iostream *iostr, rasqal_query_results* results, raptor_uri *base_uri);


void 
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


void 
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


int
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
  
  if(query_results->executed)
    rasqal_engine_execute_finish(query_results);

  if(query_results->row)
    rasqal_free_query_result_row(query_results->row);

  if(query_results->execution_data && query_results->free_execution_data)
    query_results->free_execution_data(query, query_results, query_results->execution_data);
  
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
rasqal_query_results_is_bindings(rasqal_query_results* query_results)
{
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
rasqal_query_results_is_boolean(rasqal_query_results* query_results)
{
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
rasqal_query_results_is_graph(rasqal_query_results* query_results)
{
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
rasqal_query_results_get_count(rasqal_query_results* query_results)
{
  rasqal_query* query;

  if(!query_results || query_results->failed)
    return -1;

  if(!rasqal_query_results_is_bindings(query_results))
    return -1;
  
  query=query_results->query;
  if(query->offset > 0)
    return query_results->result_count - query->offset;
  return query_results->result_count;
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
  rasqal_query* query;
  
  if(!query_results || query_results->failed || query_results->finished)
    return 1;
  
  if(!rasqal_query_results_is_bindings(query_results))
    return 1;

  query=query_results->query;

  /* Ordered Results */
  if(query->results_sequence) {
    int size=raptor_sequence_size(query->results_sequence);

    while(1) {
      if(query_results->result_count >= size) {
        query_results->finished=1;
        break;
      }

      query_results->result_count++;

      /* finished if beyond result range */
      if(rasqal_engine_check_limit_offset(query_results) > 0) {
        query_results->result_count--;
        break;
      }
      
      /* continue if before start of result range */
      if(rasqal_engine_check_limit_offset(query_results) < 0)
        continue;

      /* else got result or finished */
      break;
    }
  } else {
    rasqal_query_results_update(query_results);
    if(!query_results->finished)
      rasqal_query_result_row_update(query_results->row, query_results->result_count);
  }

  return query_results->finished;
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
  if(!query_results)
    return 1;
  
  if(!rasqal_query_results_is_bindings(query_results))
    return 1;
  
  return (query_results->failed || query_results->finished);
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
  rasqal_query* query;
  
  if(!query_results || query_results->failed || query_results->finished)
    return 1;
  
  if(!rasqal_query_results_is_bindings(query_results))
    return 1;
  
  query=query_results->query;
  if(names)
    *names=query->variable_names;
  
  if(values) {
    rasqal_query_result_row* row;

    if(query->results_sequence)
      /* Ordered Results */
      row=(rasqal_query_result_row*)raptor_sequence_get_at(query->results_sequence, query_results->result_count-1);
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
rasqal_query_results_get_binding_value(rasqal_query_results* query_results, 
                                       int offset)
{
  rasqal_query* query;
  rasqal_query_result_row* row=NULL;

  if(!query_results || query_results->failed || query_results->finished)
    return NULL;
  
  if(!rasqal_query_results_is_bindings(query_results))
    return NULL;
  
  query=query_results->query;
  if(offset < 0 || offset > query->select_variables_count-1)
    return NULL;
  
  /* Ordered Results */
  if(query->results_sequence)
    row=(rasqal_query_result_row*)raptor_sequence_get_at(query->results_sequence, query_results->result_count-1);
  else
    row=query_results->row;

  if(row)
    return row->values[offset];
  else {
    query_results->finished=1;
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
rasqal_query_results_get_binding_name(rasqal_query_results* query_results, 
                                      int offset)
{
  rasqal_query* query;

  if(!query_results || query_results->failed || query_results->finished)
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
rasqal_query_results_get_binding_value_by_name(rasqal_query_results* query_results,
                                               const unsigned char *name)
{
  int offset= -1;
  int i;
  rasqal_query* query;
  rasqal_query_result_row* row=NULL;

  if(!query_results || query_results->failed || query_results->finished)
    return NULL;
  
  if(!rasqal_query_results_is_bindings(query_results))
    return NULL;
  
  query=query_results->query;
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
    row=(rasqal_query_result_row*)raptor_sequence_get_at(query->results_sequence, query_results->result_count-1);
  else
    row=query_results->row;
  
  if(row)
    return row->values[offset];
  else {
    query_results->finished=1;
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
rasqal_query_results_get_bindings_count(rasqal_query_results* query_results)
{
  if(!query_results || query_results->failed)
    return -1;
  
  if(!rasqal_query_results_is_bindings(query_results))
    return -1;
  
  return query_results->query->select_variables_count;
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
  int rc;
  rasqal_triple *t;
  rasqal_literal *s, *p, *o;
  raptor_statement *rs;
  int skipped;
  
  if(!query_results || query_results->failed || query_results->finished)
    return NULL;
  
  if(!rasqal_query_results_is_graph(query_results))
    return NULL;
  
  query=query_results->query;
  if(query->verb == RASQAL_QUERY_VERB_DESCRIBE)
    return NULL;

  skipped=0;
  while(1) {
    if(skipped ||
       ((query->current_triple_result < 0)||
        query->current_triple_result >= raptor_sequence_size(query->constructs))) {
      /* rc<0 error rc=0 end of results,  rc>0 got a result */
      rc=rasqal_engine_get_next_result(query_results);
      if(rc < 1)
        query_results->finished=1;
      if(rc < 0)
        query_results->failed=1;

      if(query_results->finished || query_results->failed) {
        rs=NULL;
        break;
      }

      query->current_triple_result=0;
      
      skipped=0;
    }


    t=(rasqal_triple*)raptor_sequence_get_at(query->constructs,
                                             query->current_triple_result);

    rs=&query->statement;

    s=rasqal_literal_as_node(t->subject);
    if(!s) {
      rasqal_query_warning(query, "Triple with unbound subject skipped");
      skipped=1;
      continue;
    }
    switch(s->type) {
      case RASQAL_LITERAL_URI:
        rs->subject=s->value.uri;
        rs->subject_type=RAPTOR_IDENTIFIER_TYPE_RESOURCE;
        break;

      case RASQAL_LITERAL_BLANK:
        s->string=rasqal_prefix_id(query_results->result_count, 
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
        rasqal_query_warning(query, "Triple with non-URI/blank node subject skipped");
        skipped=1;
        break;
    }
    if(skipped) {
      if(s)
        rasqal_free_literal(s);
      continue;
    }
    

    p=rasqal_literal_as_node(t->predicate);
    if(!p) {
      rasqal_query_warning(query, "Triple with unbound predicate skipped");
      rasqal_free_literal(s);
      skipped=1;
      continue;
    }
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
        rasqal_query_warning(query, "Triple with non-URI predicate skipped");
        skipped=1;
        break;
    }
    if(skipped) {
      rasqal_free_literal(s);
      if(p)
        rasqal_free_literal(p);
      continue;
    }

    o=rasqal_literal_as_node(t->object);
    if(!o) {
      rasqal_query_warning(query, "Triple with unbound object skipped");
      rasqal_free_literal(s);
      rasqal_free_literal(p);
      skipped=1;
      continue;
    }
    switch(o->type) {
      case RASQAL_LITERAL_URI:
        rs->object=o->value.uri;
        rs->object_type=RAPTOR_IDENTIFIER_TYPE_RESOURCE;
        break;

      case RASQAL_LITERAL_BLANK:
        o->string=rasqal_prefix_id(query_results->result_count, 
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
        rasqal_query_warning(query, "Triple with unknown object skipped");
        skipped=1;
        break;
    }
    if(skipped) {
      rasqal_free_literal(s);
      rasqal_free_literal(p);
      if(o)
        rasqal_free_literal(o);
      continue;
    }

    /* for saving s, p, o for later disposal */
    query->triple=rasqal_new_triple(s, p, o);

    /* got triple, return it */
    break;
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
  int rc;
  
  if(!query_results || query_results->failed || query_results->finished)
    return 1;
  
  if(!rasqal_query_results_is_graph(query_results))
    return 1;
  
  query=query_results->query;
  if(query->verb == RASQAL_QUERY_VERB_DESCRIBE)
    return 1;
  
  if(query->triple) {
    rasqal_free_triple(query->triple);
    query->triple=NULL;
  }

  if(++query->current_triple_result >= raptor_sequence_size(query->constructs)) {
    /* rc<0 error rc=0 end of results,  rc>0 got a result */
    rc=rasqal_engine_get_next_result(query_results);
    if(rc < 1)
      query_results->finished=1;
    if(rc < 0)
      query_results->failed=1;
    if(query_results->finished || query_results->failed)
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
rasqal_query_results_get_boolean(rasqal_query_results* query_results)
{
  int rc;
  
  if(!query_results || query_results->failed || query_results->finished)
    return -1;
  
  if(!rasqal_query_results_is_boolean(query_results))
    return -1;
  
  if(query_results->ask_result >= 0)
    return query_results->ask_result;

  /* rc<0 error rc=0 end of results,  rc>0 got a result */
  rc=rasqal_engine_get_next_result(query_results);
  if(rc < 1) {
    /* error or end of results */
    query_results->finished= 1;
    query_results->ask_result= 0; /* false */
  }
  if(rc < 0) {
    /* error */
    query_results->failed= 1;
    query_results->ask_result= -1; /* error */
  }
  if(rc > 0) {
    /* ok */
    query_results->ask_result= 1; /* true */
  }

  return query_results->ask_result;
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
 * Default: SPARQL XML Results 2006-01-25
 * http://www.w3.org/TR/2006/WD-rdf-sparql-XMLres-20060125/
 * http://www.w3.org/2005/sparql-results#
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
                           rasqal_query_results* results,
                           raptor_uri *format_uri,
                           raptor_uri *base_uri)
{
  if(!results || results->failed || results->finished)
    return 1;
  
  /*
   * DEFAULT format:
   *
   * SPARQL XML Results 2006-01-25
   * http://www.w3.org/TR/2006/WD-rdf-sparql-XMLres-20060125/
   */
  if(!format_uri ||
     !strcmp((const char*)raptor_uri_as_string(format_uri),
             "http://www.w3.org/2005/sparql-results#") ||
     !strcmp((const char*)raptor_uri_as_string(format_uri),
             "http://www.w3.org/TR/2006/WD-rdf-sparql-XMLres-20060125/"))
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

  /*
   * SPARQL Query Results in JSON (http://json.org/) draft
   * Defined in http://www.w3.org/2001/sw/DataAccess/json-sparql/
   * Version: 1.6 $ of $Date: 2006/04/05 15:55:17
   */
  if(!strcmp((const char*)raptor_uri_as_string(format_uri),
             "http://www.w3.org/2001/sw/DataAccess/json-sparql/") ||
     !strcmp((const char*)raptor_uri_as_string(format_uri),
             "http://www.mindswap.org/%7Ekendall/sparql-results-json/"))
    return rasqal_query_results_write_json1(iostr, results, base_uri);


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
                                        rasqal_query_results* results,
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
                                       rasqal_query_results* results,
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
                                       rasqal_query_results* results,
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


static
void raptor_iostream_write_json_boolean(raptor_iostream* iostr, 
                                        const char* name, int bool)
{
  raptor_iostream_write_string(iostr, name);
  raptor_iostream_write_counted_string(iostr, "\" : ",4);

  if(bool)
    raptor_iostream_write_counted_string(iostr, "true", 4);
  else
    raptor_iostream_write_counted_string(iostr, "false", 5);

}


/*
 * rasqal_query_results_write_json1:
 * @iostr: #raptor_iostream to write the query to
 * @results: #rasqal_query_results query results format
 * @base_uri: #raptor_uri base URI of the output format
 *
 * Write a JSON version of the query results format to an
 * iostream in a format - INTERNAL.
 * 
 * If the writing succeeds, the query results will be exhausted.
 * 
 * Return value: non-0 on failure
 **/
static int
rasqal_query_results_write_json1(raptor_iostream *iostr,
                                 rasqal_query_results* results,
                                 raptor_uri *base_uri)
{
  rasqal_query* query=results->query;
  int i;
  int row_comma;
  int column_comma=0;
  
  if(!rasqal_query_results_is_bindings(results) &&
     !rasqal_query_results_is_boolean(results)) {
    rasqal_query_error(query, "Can only write JSON format for variable binding and boolean results");
    return 1;
  }
  
  
  raptor_iostream_write_counted_string(iostr, "{\n", 2);
  
  /* Header */
  raptor_iostream_write_counted_string(iostr, "  \"head\": {\n", 12);
  
  if(rasqal_query_results_is_bindings(results)) {
    raptor_iostream_write_counted_string(iostr, "    \"vars\": [ ", 14);
    for(i=0; 1; i++) {
      const unsigned char *name;
      
      name=rasqal_query_results_get_binding_name(results, i);
      if(!name)
        break;
      
      /*     'x', */
      if(i > 0)
        raptor_iostream_write_counted_string(iostr, ", ", 2);
      raptor_iostream_write_byte(iostr, '\"');
      raptor_iostream_write_string(iostr, name);
      raptor_iostream_write_byte(iostr, '\"');
    }
    raptor_iostream_write_counted_string(iostr, " ]\n", 3);
  }

  /* FIXME - could add link inside 'head': */
    
  /*   End Header */
  raptor_iostream_write_counted_string(iostr, "  },\n", 5);


  /* Boolean Results */
  if(rasqal_query_results_is_boolean(results)) {
    raptor_iostream_write_counted_string(iostr, "  ", 2);
    raptor_iostream_write_json_boolean(iostr, "boolean", 
                                       rasqal_query_results_get_boolean(results));
    goto results3done;
  }

  /* Variable Binding Results */
  raptor_iostream_write_counted_string(iostr, "  \"results\": {\n", 15);

  raptor_iostream_write_counted_string(iostr, "    \"", 5);
  raptor_iostream_write_json_boolean(iostr, "ordered", 
                                     (rasqal_query_get_order_condition(query, 0) != NULL));
  raptor_iostream_write_counted_string(iostr, ",\n", 2);

  raptor_iostream_write_counted_string(iostr, "    \"", 5);
  raptor_iostream_write_json_boolean(iostr, "distinct", 
                                     rasqal_query_get_distinct(query));
  raptor_iostream_write_counted_string(iostr, ",\n", 2);

  raptor_iostream_write_counted_string(iostr, "    \"bindings\" : [\n", 19);

  row_comma=0;
  while(!rasqal_query_results_finished(results)) {
    if(row_comma)
      raptor_iostream_write_counted_string(iostr, ",\n", 2);

    /* Result row */
    raptor_iostream_write_counted_string(iostr, "      {\n", 8);

    column_comma=0;
    for(i=0; i<rasqal_query_results_get_bindings_count(results); i++) {
      const unsigned char *name=rasqal_query_results_get_binding_name(results, i);
      rasqal_literal *l=rasqal_query_results_get_binding_value(results, i);

      if(column_comma)
        raptor_iostream_write_counted_string(iostr, ",\n", 2);

      /*       <binding> */
      raptor_iostream_write_counted_string(iostr, "        \"", 9);
      raptor_iostream_write_string(iostr, name);
      raptor_iostream_write_counted_string(iostr, "\" : { ", 6);

      if(!l) {
        raptor_iostream_write_string(iostr, "\"type\": \"unbound\", \"value\": null");
      } else switch(l->type) {
        const unsigned char* str;
        size_t len;
        
        case RASQAL_LITERAL_URI:
          raptor_iostream_write_string(iostr, "\"type\": \"uri\", \"value\": \"");
          str=(const unsigned char*)raptor_uri_as_counted_string(l->value.uri, &len);
          raptor_iostream_write_string_ntriples(iostr, str, len, '"');
          raptor_iostream_write_byte(iostr, '"');
          break;

        case RASQAL_LITERAL_BLANK:
          raptor_iostream_write_string(iostr, "\"type\": \"bnode\", \"value\": \"");
          raptor_iostream_write_string_ntriples(iostr, (const unsigned char*)l->string, 
                                                l->string_len, '"');
          raptor_iostream_write_byte(iostr, '"');
          break;

        case RASQAL_LITERAL_STRING:
          raptor_iostream_write_string(iostr, "\"type\": \"literal\", \"value\": \"");
          raptor_iostream_write_string_ntriples(iostr, (const unsigned char*)l->string,
                                                l->string_len, '"');
          raptor_iostream_write_byte(iostr, '"');

          if(l->language) {
            raptor_iostream_write_string(iostr, ",\n      \"xml:lang\" : \"");
            raptor_iostream_write_string(iostr, (const unsigned char*)l->language);
            raptor_iostream_write_byte(iostr, '"');
          }
          
          if(l->datatype) {
            raptor_iostream_write_string(iostr, ",\n      \"datatype\" : \"");
            str=(const unsigned char*)raptor_uri_as_counted_string(l->datatype, &len);
            raptor_iostream_write_string_ntriples(iostr, str, len, '"');
            raptor_iostream_write_byte(iostr, '"');
          }
          
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

      /* End Binding */
      raptor_iostream_write_counted_string(iostr, " }", 2);
      column_comma=1;
    }

    /* End Result Row */
    raptor_iostream_write_counted_string(iostr, "\n      }", 8);
    row_comma=1;
    
    rasqal_query_results_next(results);
  }

  raptor_iostream_write_counted_string(iostr, "\n    ]\n  }", 10);

  results3done:
  
  /* end sparql */
  raptor_iostream_write_counted_string(iostr, "\n}\n", 3);

  return 0;
}

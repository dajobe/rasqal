/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_format_rdf.c - Format results in a serialized RDF graph
 *
 * Copyright (C) 2010, David Beckett http://www.dajobe.org/
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


#include <raptor.h>

/* Rasqal includes */
#include <rasqal.h>
#include <rasqal_internal.h>


#define DEBUG_FH stderr

#ifndef FILE_READ_BUF_SIZE
#ifdef BUFSIZ
#define FILE_READ_BUF_SIZE BUFSIZ
#else
#define FILE_READ_BUF_SIZE 1024
#endif
#endif


#define QUERY_RESULTS_TURTLE_PRETTY 1


typedef struct 
{
  const char* name;

  const char* read_format_name;

  const char* write_format_name;

  /* Namespace URIs */
  raptor_uri* rdf_ns_uri;
  raptor_uri* rs_ns_uri;

  /* Predicate URIs */
  raptor_uri* rdf_type_uri;
  raptor_uri* rs_variable_uri;
  raptor_uri* rs_value_uri;
  raptor_uri* rs_solution_uri;
  raptor_uri* rs_binding_uri;
  raptor_uri* rs_resultVariable_uri;

  /* Class URIs */
  raptor_uri* rs_ResultSet_uri;
} rasqal_query_results_format_rdf;


typedef struct 
{
  rasqal_world* world;
  raptor_world *raptor_world_ptr;

  rasqal_query_results_formatter* formatter;

  rasqal_rowsource* rowsource;

  int failed;

  raptor_uri* rs_uri;

  /* Input fields */
  raptor_uri* base_uri;
  raptor_iostream* iostr;

  /* Parsing fields */
  int parsed;
  rasqal_dataset* ds;
  const char* format_name;
  rasqal_row* row; /* current result row */

  int offset; /* current result row number */
  unsigned char buffer[FILE_READ_BUF_SIZE]; /* iostream read buffer */

  /* Output fields */
  raptor_sequence* results_sequence; /* saved result rows */

  /* Variables table allocated for variables in the result set */
  rasqal_variables_table* vars_table;
  int variables_count;

  unsigned int flags;
} rasqal_rowsource_rdf_context;


static const unsigned char* const rs_namespace_uri_string =
  RASQAL_GOOD_CAST(const unsigned char*, "http://www.w3.org/2001/sw/DataAccess/tests/result-set#");


static int
rasqal_query_results_rdf_init(rasqal_query_results_formatter* formatter,
                              const char* name)
{
  rasqal_query_results_format_rdf* context = (rasqal_query_results_format_rdf*)formatter->context;
  raptor_world* raptor_world_ptr = formatter->factory->world->raptor_world_ptr;
  
  context->name = name;

  if(!strcmp(name, "rdfxml")) {
    context->read_format_name = "rdfxml";
    context->write_format_name = "rdfxml-abbrev";
  } else if(!strcmp(name, "turtle")) {
    context->read_format_name = "turtle";
    context->write_format_name = "turtle";
  } else {
    context->read_format_name = "guess";
    context->write_format_name = NULL;
  }

  /* Namespaces */
  context->rdf_ns_uri = raptor_new_uri(raptor_world_ptr,
                                       raptor_rdf_namespace_uri);
  context->rs_ns_uri = raptor_new_uri(raptor_world_ptr,
                                      rs_namespace_uri_string);


  /* Predicates */
  context->rdf_type_uri = 
    raptor_new_uri_from_uri_local_name(raptor_world_ptr,
                                       context->rdf_ns_uri,
                                       RASQAL_GOOD_CAST(const unsigned char*, "type"));
  context->rs_variable_uri =
    raptor_new_uri_from_uri_local_name(raptor_world_ptr,
                                       context->rs_ns_uri,
                                       RASQAL_GOOD_CAST(const unsigned char*, "variable"));
  context->rs_value_uri =
    raptor_new_uri_from_uri_local_name(raptor_world_ptr,
                                       context->rs_ns_uri,
                                       RASQAL_GOOD_CAST(const unsigned char*, "value"));
  context->rs_solution_uri =
    raptor_new_uri_from_uri_local_name(raptor_world_ptr,
                                       context->rs_ns_uri,
                                       RASQAL_GOOD_CAST(const unsigned char*, "solution"));
  context->rs_binding_uri =
    raptor_new_uri_from_uri_local_name(raptor_world_ptr,
                                       context->rs_ns_uri,
                                       RASQAL_GOOD_CAST(const unsigned char*, "binding"));
  context->rs_resultVariable_uri =
    raptor_new_uri_from_uri_local_name(raptor_world_ptr,
                                       context->rs_ns_uri,
                                       RASQAL_GOOD_CAST(const unsigned char*, "resultVariable"));

  /* Classes */
  context->rs_ResultSet_uri =
    raptor_new_uri_from_uri_local_name(raptor_world_ptr,
                                       context->rs_ns_uri,
                                       RASQAL_GOOD_CAST(const unsigned char*, "ResultSet"));

  return 0;
}


static void
rasqal_query_results_rdf_finish(rasqal_query_results_formatter* formatter)
{
  rasqal_query_results_format_rdf* context;

  context = (rasqal_query_results_format_rdf*)formatter->context;
  
  if(context->rdf_ns_uri)
    raptor_free_uri(context->rdf_ns_uri);
  if(context->rs_ns_uri)
    raptor_free_uri(context->rs_ns_uri);

  if(context->rdf_type_uri)
    raptor_free_uri(context->rdf_type_uri);
  if(context->rs_variable_uri)
    raptor_free_uri(context->rs_variable_uri);
  if(context->rs_value_uri)
    raptor_free_uri(context->rs_value_uri);
  if(context->rs_solution_uri)
    raptor_free_uri(context->rs_solution_uri);
  if(context->rs_binding_uri)
    raptor_free_uri(context->rs_binding_uri);

  if(context->rs_ResultSet_uri)
    raptor_free_uri(context->rs_ResultSet_uri);
  if(context->rs_resultVariable_uri)
    raptor_free_uri(context->rs_resultVariable_uri);
}


/*
 * rasqal_query_results_rdf_write:
 * @iostr: #raptor_iostream to write the query results to
 * @results: #rasqal_query_results query results input
 * @base_uri: #raptor_uri base URI of the output format
 *
 * INTERNAL - Write RDF serialized query results to an iostream in a format.
 * 
 * If the writing succeeds, the query results will be exhausted.
 * 
 * Return value: non-0 on failure
 **/
static int
rasqal_query_results_rdf_write(rasqal_query_results_formatter* formatter,
                               raptor_iostream *iostr,
                               rasqal_query_results* results,
                               raptor_uri *base_uri)
{
  rasqal_world* world = rasqal_query_results_get_world(results);
  raptor_world *raptor_world_ptr;
  rasqal_query_results_format_rdf* formatter_context;
  int i;
  int size;
  raptor_term* resultset_node;
  raptor_serializer* ser;
  raptor_statement statement;
  int rc = 0;
  
  if(!rasqal_query_results_is_bindings(results)) {
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                            "Can only write RDF format for variable binding results");
    return 1;
  }

  raptor_world_ptr = world->raptor_world_ptr;

  formatter_context = (rasqal_query_results_format_rdf*)formatter->context;
  
  if(!formatter_context->write_format_name) {
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                            "Cannot write RDF in format %s",
                            formatter_context->name);
    return 1;
  }
  

  /* Start serializing */
  ser = raptor_new_serializer(raptor_world_ptr,
                              formatter_context->write_format_name);
  if(!ser) {
    rc = 1;
    goto tidy;
  }
  
  raptor_serializer_start_to_iostream(ser, base_uri, iostr);
  
  raptor_serializer_set_namespace(ser, formatter_context->rs_ns_uri,
                                  (const unsigned char *)"rs");
  raptor_serializer_set_namespace(ser, formatter_context->rdf_ns_uri,
                                  (const unsigned char *)"rdf");

  raptor_statement_init(&statement, raptor_world_ptr);

  /* create a result set blank node term */
  resultset_node = raptor_new_term_from_blank(raptor_world_ptr, NULL);

  /* result set triple */
  statement.subject = resultset_node;
  statement.predicate = raptor_new_term_from_uri(raptor_world_ptr,
                                                 formatter_context->rdf_type_uri);
  statement.object = raptor_new_term_from_uri(raptor_world_ptr, 
                                              formatter_context->rs_ResultSet_uri);
  raptor_serializer_serialize_statement(ser, &statement);
  raptor_free_term(statement.predicate); statement.predicate = NULL;
  raptor_free_term(statement.object); statement.object = NULL;


  /* variable name triples
   * all these statements have same predicate 
   */
  /* statement.subject = resultset_node; */
  statement.predicate = raptor_new_term_from_uri(raptor_world_ptr,
                                                 formatter_context->rs_resultVariable_uri);
  for(i = 0; 1; i++) {
    const unsigned char *name;
    
    name = rasqal_query_results_get_binding_name(results, i);
    if(!name)
      break;
      
    statement.object = raptor_new_term_from_literal(raptor_world_ptr, 
                                                    name, NULL, NULL);
    raptor_serializer_serialize_statement(ser, &statement);
    raptor_free_term(statement.object); statement.object = NULL;
  }
  raptor_free_term(statement.predicate); statement.predicate = NULL;


  /* data triples */
  size = rasqal_query_results_get_bindings_count(results);
  while(!rasqal_query_results_finished(results)) {
    raptor_term* row_node = raptor_new_term_from_blank(raptor_world_ptr, NULL);

    /* Result row triples */
    statement.subject = resultset_node;
    statement.predicate = raptor_new_term_from_uri(raptor_world_ptr,
                                                   formatter_context->rs_solution_uri);
    statement.object = row_node;
    raptor_serializer_serialize_statement(ser, &statement);
    raptor_free_term(statement.predicate); statement.predicate = NULL;

    /* Binding triples */
    for(i = 0; i < size; i++) {
      raptor_term* binding_node = raptor_new_term_from_blank(raptor_world_ptr,
                                                             NULL);
      const unsigned char *name;
      rasqal_literal *l;

      name = rasqal_query_results_get_binding_name(results, i);
      l = rasqal_query_results_get_binding_value(results, i);

      /* binding */
      statement.subject = row_node;
      statement.predicate = raptor_new_term_from_uri(raptor_world_ptr,
                                                     formatter_context->rs_binding_uri);
      statement.object = binding_node;
      raptor_serializer_serialize_statement(ser, &statement);
      raptor_free_term(statement.predicate); statement.predicate = NULL;

      /* only emit rs:value and rs:variable triples if there is a value */
      if(l) {
        statement.subject = binding_node;
        statement.predicate = raptor_new_term_from_uri(raptor_world_ptr,
                                                       formatter_context->rs_variable_uri);
        statement.object = raptor_new_term_from_literal(raptor_world_ptr, 
                                                        name, NULL, NULL);
        raptor_serializer_serialize_statement(ser, &statement);
        raptor_free_term(statement.predicate); statement.predicate = NULL;
        raptor_free_term(statement.object); statement.object = NULL;

        /* statement.subject = binding_node; */
        statement.predicate = raptor_new_term_from_uri(raptor_world_ptr,
                                                       formatter_context->rs_value_uri);
        switch(l->type) {
          case RASQAL_LITERAL_URI:
            statement.object = raptor_new_term_from_uri(raptor_world_ptr,
                                                        l->value.uri);
            break;
        case RASQAL_LITERAL_BLANK:
            statement.object = raptor_new_term_from_blank(raptor_world_ptr,
                                                          l->string);
            break;
        case RASQAL_LITERAL_STRING:
        case RASQAL_LITERAL_UDT:
            statement.object = raptor_new_term_from_literal(raptor_world_ptr,
                                                            l->string,
                                                            l->datatype,
                                                            RASQAL_GOOD_CAST(const unsigned char*, l->language));
            break;
        case RASQAL_LITERAL_PATTERN:
        case RASQAL_LITERAL_QNAME:
        case RASQAL_LITERAL_INTEGER:
        case RASQAL_LITERAL_XSD_STRING:
        case RASQAL_LITERAL_BOOLEAN:
        case RASQAL_LITERAL_DOUBLE:
        case RASQAL_LITERAL_FLOAT:
        case RASQAL_LITERAL_VARIABLE:
        case RASQAL_LITERAL_DECIMAL:
        case RASQAL_LITERAL_DATE:
        case RASQAL_LITERAL_DATETIME:
        case RASQAL_LITERAL_INTEGER_SUBTYPE:

        case RASQAL_LITERAL_UNKNOWN:
        default:
          rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR,
                                  NULL,
                                  "Cannot turn literal type %u into RDF",
                                  l->type);
          goto tidy;
        }
        
        raptor_serializer_serialize_statement(ser, &statement);
        raptor_free_term(statement.predicate); statement.predicate = NULL;
        raptor_free_term(statement.object); statement.object = NULL;
      }

      raptor_free_term(binding_node);
    }
    
    rasqal_query_results_next(results);

    raptor_free_term(row_node); row_node = NULL;
  }

  raptor_free_term(resultset_node);

  raptor_serializer_serialize_end(ser);

  raptor_free_serializer(ser);

  tidy:

  return rc;
}


/* Local handlers for turning RDF graph read from an iostream into rows */

static int
rasqal_rowsource_rdf_init(rasqal_rowsource* rowsource, void *user_data) 
{
  rasqal_rowsource_rdf_context* con;

  con = (rasqal_rowsource_rdf_context*)user_data;

  con->rowsource = rowsource;

  con->world = rowsource->world;
  con->raptor_world_ptr = rasqal_world_get_raptor(rowsource->world);

  con->rs_uri = raptor_new_uri(con->raptor_world_ptr,
                               rs_namespace_uri_string);

  return 0;
}


static int
rasqal_rowsource_rdf_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_rowsource_rdf_context* con;

  con = (rasqal_rowsource_rdf_context*)user_data;

  if(con->base_uri)
    raptor_free_uri(con->base_uri);

  if(con->results_sequence)
    raptor_free_sequence(con->results_sequence);

  if(con->vars_table)
    rasqal_free_variables_table(con->vars_table);

  if(con->rs_uri)
    raptor_free_uri(con->rs_uri);

  if(con->ds)
    rasqal_free_dataset(con->ds);

  if(con->flags) {
    if(con->iostr)
      raptor_free_iostream(con->iostr);
  }

  RASQAL_FREE(rasqal_rowsource_rdf_context, con);

  return 0;
}


static int
rasqal_rowsource_rdf_process(rasqal_rowsource_rdf_context* con)
{
  rasqal_query_results_format_rdf* formatter_context;
  raptor_uri* rdf_ns_uri;
  raptor_uri* uri;
  rasqal_literal* predicate_uri_literal;
  rasqal_literal* object_uri_literal;
  rasqal_literal* binding_uri_literal;
  rasqal_literal* resultSet_node;
  rasqal_literal* solution_node;
  rasqal_dataset_term_iterator* solution_iter;
  rasqal_literal* variable_predicate;
  rasqal_literal* value_predicate;

  if(con->parsed)
    return 0;

  formatter_context = (rasqal_query_results_format_rdf*)con->formatter->context;
  
  con->ds = rasqal_new_dataset(con->world);
  
  if(rasqal_dataset_load_graph_iostream(con->ds,
                                        formatter_context->read_format_name,
                                        con->iostr, con->base_uri)) {
    return 1;
  }

  rdf_ns_uri = raptor_new_uri(con->raptor_world_ptr, raptor_rdf_namespace_uri);

  uri = raptor_new_uri_from_uri_local_name(con->raptor_world_ptr,
                                           rdf_ns_uri,
                                           RASQAL_GOOD_CAST(const unsigned char*, "type"));
  predicate_uri_literal = rasqal_new_uri_literal(con->world, uri);

  raptor_free_uri(rdf_ns_uri); rdf_ns_uri = NULL;

  uri = raptor_new_uri_from_uri_local_name(con->raptor_world_ptr,
                                           con->rs_uri,
                                           RASQAL_GOOD_CAST(const unsigned char*, "ResultSet"));
  object_uri_literal = rasqal_new_uri_literal(con->world, uri);

  uri = raptor_new_uri_from_uri_local_name(con->raptor_world_ptr,
                                           con->rs_uri,
                                           RASQAL_GOOD_CAST(const unsigned char*, "variable"));
  variable_predicate = rasqal_new_uri_literal(con->world, uri);
  
  uri = raptor_new_uri_from_uri_local_name(con->raptor_world_ptr,
                                           con->rs_uri,
                                           RASQAL_GOOD_CAST(const unsigned char*, "value"));
  value_predicate = rasqal_new_uri_literal(con->world, uri);
  



  /* find result set node ?rs := getSource(a, rs:ResultSet)  */
  resultSet_node = rasqal_dataset_get_source(con->ds,
                                             predicate_uri_literal,
                                             object_uri_literal);

  rasqal_free_literal(predicate_uri_literal); predicate_uri_literal = NULL;
  rasqal_free_literal(object_uri_literal); object_uri_literal = NULL;
  

  /* if no such triple, expecting empty results */
  if(!resultSet_node) {
    rasqal_free_literal(value_predicate);
    rasqal_free_literal(variable_predicate);
    return 0;
  }
  
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 2
  RASQAL_DEBUG1("Got result set node ");
  rasqal_literal_print(resultSet_node, DEBUG_FH);
  fputc('\n', DEBUG_FH);
#endif    



  /* find variable names from all ?var := getTargets(?rs, rs:resultVariable) */
  uri = raptor_new_uri_from_uri_local_name(con->raptor_world_ptr,
                                           con->rs_uri,
                                           RASQAL_GOOD_CAST(const unsigned char*, "resultVariable"));
  predicate_uri_literal = rasqal_new_uri_literal(con->world, uri);

  solution_iter = rasqal_dataset_get_targets_iterator(con->ds,
                                                      resultSet_node,
                                                      predicate_uri_literal);
  while(1) {
    solution_node = rasqal_dataset_term_iterator_get(solution_iter);
    if(!solution_node)
      break;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 2
    RASQAL_DEBUG1("Got variable node ");
    rasqal_literal_print(solution_node, DEBUG_FH);
    fputc('\n', DEBUG_FH);
#endif
    
    if(1) {
      const unsigned char* name;
      size_t len;

      name = rasqal_literal_as_counted_string(solution_node, &len, 0, NULL);
      if(name) {
        rasqal_variable *v;
        v = rasqal_variables_table_add2(con->vars_table,
                                        RASQAL_VARIABLE_TYPE_NORMAL,
                                        name, len, NULL);
        if(v) {
          rasqal_rowsource_add_variable(con->rowsource, v);
          /* above function takes a reference to v */
          rasqal_free_variable(v);
        }
      }
    }
      
    rasqal_dataset_term_iterator_next(solution_iter);
  }
  rasqal_free_dataset_term_iterator(solution_iter); solution_iter = NULL;

  rasqal_free_literal(predicate_uri_literal); predicate_uri_literal = NULL;


  uri = raptor_new_uri_from_uri_local_name(con->raptor_world_ptr,
                                           con->rs_uri,
                                           RASQAL_GOOD_CAST(const unsigned char*, "binding"));
  binding_uri_literal = rasqal_new_uri_literal(con->world, uri);


  /* for each solution node ?sol := getTargets(?rs, rs:solution) */
  uri = raptor_new_uri_from_uri_local_name(con->raptor_world_ptr,
                                           con->rs_uri,
                                           RASQAL_GOOD_CAST(const unsigned char*, "solution"));
  predicate_uri_literal = rasqal_new_uri_literal(con->world, uri);

  solution_iter = rasqal_dataset_get_targets_iterator(con->ds,
                                                      resultSet_node,
                                                      predicate_uri_literal);
  while(1) {
    rasqal_dataset_term_iterator* binding_iter;
    rasqal_row* row;
    
    solution_node = rasqal_dataset_term_iterator_get(solution_iter);
    if(!solution_node)
      break;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 2
    RASQAL_DEBUG1("Got solution node (row) ");
    rasqal_literal_print(solution_node, DEBUG_FH);
    fputc('\n', DEBUG_FH);
#endif

    row = rasqal_new_row(con->rowsource);
    
    /* for each binding node ?bn := getTargets(?sol, rs:binding) */
    binding_iter = rasqal_dataset_get_targets_iterator(con->ds,
                                                       solution_node,
                                                       binding_uri_literal);
    while(1) {
      rasqal_literal* binding_node;
      rasqal_literal* var_literal;
      rasqal_literal* value_literal;
      
      binding_node = rasqal_dataset_term_iterator_get(binding_iter);
      if(!binding_node)
        break;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 2
      RASQAL_DEBUG1("  Got binding node ");
      rasqal_literal_print(binding_node, DEBUG_FH);
      fputc('\n', DEBUG_FH);
#endif

      /* variable ?var := getTarget(?bn, rs:variable) */
      var_literal = rasqal_dataset_get_target(con->ds,
                                              binding_node,
                                              variable_predicate);

      /* variable ?val := getTarget(?bn, rs:value) */
      value_literal = rasqal_dataset_get_target(con->ds,
                                                binding_node,
                                                value_predicate);

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 2
      RASQAL_DEBUG1("    Variable: ");
      rasqal_literal_print(var_literal, DEBUG_FH);
      fputs(" Value: ", DEBUG_FH);
      rasqal_literal_print(value_literal, DEBUG_FH);
      fputc('\n', DEBUG_FH);
#endif

      /* save row[?var] = ?val */
      if(1) {
        const unsigned char* name;
        int offset;
        name = rasqal_literal_as_string(var_literal);
        offset = rasqal_rowsource_get_variable_offset_by_name(con->rowsource, 
                                                              name);
        rasqal_row_set_value_at(row, offset, value_literal);
      }
      
      /* ?index := getTarget(?sol, rs:index)
       * if ?index exists then that is the integer order of the row
       * in the list of row results */

      rasqal_dataset_term_iterator_next(binding_iter);
    }

    rasqal_free_dataset_term_iterator(binding_iter); binding_iter = NULL;

    /* save row at end of sequence of rows */
    raptor_sequence_push(con->results_sequence, row);

    rasqal_dataset_term_iterator_next(solution_iter);
  }
  rasqal_free_dataset_term_iterator(solution_iter); solution_iter = NULL;

  rasqal_free_literal(value_predicate); value_predicate = NULL;
  rasqal_free_literal(variable_predicate); variable_predicate = NULL;
  rasqal_free_literal(predicate_uri_literal); predicate_uri_literal = NULL;
  rasqal_free_literal(binding_uri_literal); binding_uri_literal = NULL;
  

  if(!raptor_sequence_size(con->results_sequence))
    /* Empty result set with variables defined; so remove vars */
    rasqal_rowsource_remove_all_variables(con->rowsource);

  /* sort sequence of rows by ?index or original order */

  con->parsed = 1;

  return 0;
}


static int
rasqal_rowsource_rdf_ensure_variables(rasqal_rowsource* rowsource,
                                      void *user_data)
{
  rasqal_rowsource_rdf_context* con;

  con = (rasqal_rowsource_rdf_context*)user_data;

  rasqal_rowsource_rdf_process(con);

  return con->failed;
}


static rasqal_row*
rasqal_rowsource_rdf_read_row(rasqal_rowsource* rowsource,
                              void *user_data)
{
  rasqal_rowsource_rdf_context* con;
  rasqal_row* row = NULL;

  con = (rasqal_rowsource_rdf_context*)user_data;

  rasqal_rowsource_rdf_process(con);
  
  if(!con->failed && raptor_sequence_size(con->results_sequence) > 0) {
    RASQAL_DEBUG1("getting row from stored sequence\n");
    row = (rasqal_row*)raptor_sequence_unshift(con->results_sequence);
  }

  return row;
}


static const rasqal_rowsource_handler rasqal_rowsource_rdf_handler={
  /* .version = */ 1,
  "RDF Query Results",
  /* .init = */ rasqal_rowsource_rdf_init,
  /* .finish = */ rasqal_rowsource_rdf_finish,
  /* .ensure_variables = */ rasqal_rowsource_rdf_ensure_variables,
  /* .read_row = */ rasqal_rowsource_rdf_read_row,
  /* .read_all_rows = */ NULL,
  /* .reset = */ NULL,
  /* .set_requirements = */ NULL,
  /* .get_inner_rowsource = */ NULL,
  /* .set_origin = */ NULL,
};



/*
 * rasqal_query_results_getrowsource_rdf:
 * @world: rasqal world object
 * @iostr: #raptor_iostream to read the query results from
 * @base_uri: #raptor_uri base URI of the input format
 *
 * Read the RDF serialized graph of query results from an
 * iostream in a format returning a rwosurce - INTERNAL.
 * 
 * Return value: a new rasqal_rowsource or NULL on failure
 **/
static rasqal_rowsource*
rasqal_query_results_rdf_get_rowsource(rasqal_query_results_formatter* formatter,
                                       rasqal_world *world,
                                       rasqal_variables_table* vars_table,
                                       raptor_iostream *iostr,
                                       raptor_uri *base_uri,
                                       unsigned int flags)
{
  rasqal_rowsource_rdf_context* con;
  
  con = RASQAL_CALLOC(rasqal_rowsource_rdf_context*, 1, sizeof(*con));
  if(!con)
    return NULL;

  con->world = world;
  con->formatter = formatter;
  con->base_uri = base_uri ? raptor_uri_copy(base_uri) : NULL;
  con->iostr = iostr;
  con->flags = flags;

  con->results_sequence = raptor_new_sequence((raptor_data_free_handler)rasqal_free_row, (raptor_data_print_handler)rasqal_row_print);

  con->vars_table = rasqal_new_variables_table_from_variables_table(vars_table);
  
  return rasqal_new_rowsource_from_handler(world, NULL,
                                           con,
                                           &rasqal_rowsource_rdf_handler,
                                           con->vars_table,
                                           0);
}


static void
rasqal_query_results_format_rdf_copy_syntax_description_from_parser(rasqal_query_results_format_factory *factory, const char* name)
{
  raptor_world* raptor_world_ptr = factory->world->raptor_world_ptr;
  const raptor_syntax_description* desc;
  unsigned int i;

  for(i = 0;
      (desc = raptor_world_get_parser_description(raptor_world_ptr, i)); 
      i++) {
    int j;
    for(j = 0; desc->names[j]; j++) {
      if(!strcmp(desc->names[j], name)) {
        factory->desc.mime_types = desc->mime_types;
        return;
      }
    }
  }
}


static int
rasqal_query_results_rdfxml_recognise_syntax(rasqal_query_results_format_factory* factory, 
                                             const unsigned char *buffer, 
                                             size_t len,
                                             const unsigned char *identifier,
                                             const unsigned char *suffix,
const char *mime_type)
{

  if(suffix && !strcmp(RASQAL_GOOD_CAST(const char*, suffix), "rdf"))
    return 5;
  
  return 0;
}


static const char* const rdfxml_names[] = { "rdfxml", NULL};

static const raptor_type_q rdfxml_types[] = {
  { "application/rdf+xml", 19, 10}, 
  { NULL, 0, 0}
};

static int
rasqal_query_results_rdfxml_register_factory(rasqal_query_results_format_factory *factory) 
{
  int rc = 0;

  factory->desc.names = rdfxml_names;
  factory->desc.mime_types = rdfxml_types;

  rasqal_query_results_format_rdf_copy_syntax_description_from_parser(factory,
                                                                      "rdfxml");

  factory->desc.label = "RDF/XML Query Results";
  factory->desc.uri_strings = NULL;

  factory->desc.flags = 0;
  
  factory->context_length = sizeof(rasqal_query_results_format_rdf);
  
  factory->init          = rasqal_query_results_rdf_init;
  factory->finish        = rasqal_query_results_rdf_finish;

  factory->write         = rasqal_query_results_rdf_write;
  factory->get_rowsource = rasqal_query_results_rdf_get_rowsource;
  factory->recognise_syntax = rasqal_query_results_rdfxml_recognise_syntax;
  
  return rc;
}


int
rasqal_init_result_format_rdf(rasqal_world* world)
{
  return !rasqal_world_register_query_results_format_factory(world,
                                                             &rasqal_query_results_rdfxml_register_factory);
}


#ifdef QUERY_RESULTS_TURTLE_PRETTY
/*
 * rasqal_query_results_turtle_write:
 * @iostr: #raptor_iostream to write the query to
 * @results: #rasqal_query_results query results format
 * @base_uri: #raptor_uri base URI of the output format
 *
 * Write a Turtle version of the query results format to an
 * iostream in a format - INTERNAL.
 * 
 * If the writing succeeds, the query results will be exhausted.
 * 
 * Return value: non-0 on failure
 **/
static int
rasqal_query_results_turtle_write(rasqal_query_results_formatter* formatter,
                                  raptor_iostream *iostr,
                                  rasqal_query_results* results,
                                  raptor_uri *base_uri)
{
  rasqal_world* world = rasqal_query_results_get_world(results);
  int i;
  int row_semicolon;
  int column_semicolon = 0;
  int size;
  
  if(!rasqal_query_results_is_bindings(results)) {
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                            "Can only write Turtle format for variable binding results");
    return 1;
  }
  
  
  raptor_iostream_string_write("@prefix xsd:     <http://www.w3.org/2001/XMLSchema#> .\n", iostr);
  raptor_iostream_string_write("@prefix rs:      <http://www.w3.org/2001/sw/DataAccess/tests/result-set#> .\n", iostr);
  raptor_iostream_string_write("@prefix rdf:     <http://www.w3.org/1999/02/22-rdf-syntax-ns#> .\n", iostr);
  raptor_iostream_write_byte('\n', iostr);

  raptor_iostream_counted_string_write("[]    rdf:type      rs:ResultSet ;\n",
                                       35, iostr);


  /* Variable Binding Results */

  for(i = 0; 1; i++) {
    const unsigned char *name;
    
    name = rasqal_query_results_get_binding_name(results, i);
    if(!name)
      break;
      
    raptor_iostream_counted_string_write("      rs:resultVariable  \"",
                                         26, iostr);
    raptor_iostream_string_write(name, iostr);
    raptor_iostream_counted_string_write("\" ;\n", 4, iostr);
  }

  size = rasqal_query_results_get_bindings_count(results);
  row_semicolon = 0;

  while(!rasqal_query_results_finished(results)) {
    if(row_semicolon)
      raptor_iostream_counted_string_write(" ;\n", 3, iostr);

    /* Result row */
    raptor_iostream_counted_string_write("      rs:solution   [ ", 22, iostr);

    column_semicolon = 0;
    for(i = 0; i < size; i++) {
      const unsigned char *name;
      rasqal_literal *l;

      name = rasqal_query_results_get_binding_name(results, i);
      l = rasqal_query_results_get_binding_value(results, i);

      if(column_semicolon)
        raptor_iostream_counted_string_write("; \n                      ",
                                             25, iostr);

      if(l) {
        /* only emit binding rs:value and rs:variable triples if
         * there is a value 
         */

        /* binding */
        raptor_iostream_counted_string_write("rs:binding    [ ", 16, iostr);

        raptor_iostream_counted_string_write("rs:variable   \"", 15, iostr);
        raptor_iostream_string_write(name, iostr);
        raptor_iostream_counted_string_write("\" ;\n                                      rs:value      ", 56, iostr);
        rasqal_literal_write_turtle(l, iostr);

        raptor_iostream_counted_string_write("\n                                    ] ", 39, iostr);
        column_semicolon = 1;
      }

    }

    /* End Result Row */
    raptor_iostream_counted_string_write("\n      ]", 8, iostr);
    row_semicolon = 1;
    
    rasqal_query_results_next(results);
  }

  raptor_iostream_counted_string_write(" .\n", 3, iostr);

  return 0;
}
#endif


static int
rasqal_query_results_turtle_recognise_syntax(rasqal_query_results_format_factory* factory, 
                                             const unsigned char *buffer, 
                                             size_t len,
                                             const unsigned char *identifier,
                                             const unsigned char *suffix,
const char *mime_type)
{
  if(suffix && (!strcmp(RASQAL_GOOD_CAST(const char*, suffix), "ttl") ||
                !strcmp(RASQAL_GOOD_CAST(const char*, suffix), "n3")))
    return 7;
  
  return 0;
}


static const char* const turtle_names[] = { "turtle", NULL};

static const char* const turtle_uri_strings[] = {
  "http://www.w3.org/TeamSubmission/turtle/",
  NULL
};

static const raptor_type_q turtle_types[] = {
  { "application/turtle", 18, 10}, 
  { NULL, 0, 0}
};

static int
rasqal_query_results_turtle_register_factory(rasqal_query_results_format_factory *factory) 
{
  int rc = 0;

  factory->desc.names = turtle_names;

  factory->desc.mime_types = turtle_types;
  rasqal_query_results_format_rdf_copy_syntax_description_from_parser(factory,
                                                                      "turtle");
  
  factory->desc.label = "Turtle Query Results";
  factory->desc.uri_strings = turtle_uri_strings;

  factory->context_length = sizeof(rasqal_query_results_format_rdf);
  
  factory->init          = rasqal_query_results_rdf_init;
  factory->finish        = rasqal_query_results_rdf_finish;
  
#ifdef QUERY_RESULTS_TURTLE_PRETTY
  factory->write         = rasqal_query_results_turtle_write;
#else
  /* This is just not as pretty */
  factory->write         = rasqal_query_results_rdf_write;
#endif

  factory->get_rowsource = rasqal_query_results_rdf_get_rowsource;
  factory->recognise_syntax = rasqal_query_results_turtle_recognise_syntax;

  return rc;
}


int
rasqal_init_result_format_turtle(rasqal_world* world)
{
  return !rasqal_world_register_query_results_format_factory(world,
                                                             &rasqal_query_results_turtle_register_factory);
}

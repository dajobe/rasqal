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


struct rasqal_rdfr_triple_s {
  struct rasqal_rdfr_triple_s *next;
  rasqal_triple *triple;
};

typedef struct rasqal_rdfr_triple_s rasqal_rdfr_triple;


typedef struct 
{
  rasqal_world* world;
  
  rasqal_literal* base_uri_literal;

  rasqal_rdfr_triple *head;
  rasqal_rdfr_triple *tail;
} rasqal_rdfr_triplestore;


typedef struct {
  rasqal_rdfr_triplestore* triplestore;
  
  /* Simple cursor */
  rasqal_triple match;

  rasqal_triple_parts want;
  rasqal_triple_parts parts;
  
  rasqal_rdfr_triple *cursor;
} rasqal_rdfr_term_iterator;


static
rasqal_rdfr_triplestore*
rasqal_new_rdfr_triplestore(rasqal_world* world)
{
  rasqal_rdfr_triplestore* rts;
  
  if(!world)
    return NULL;
  
  rts = (rasqal_rdfr_triplestore*)RASQAL_CALLOC(rasqal_rdfr_triplestore, 1, 
                                                sizeof(*rts));

  rts->world = world;

  return rts;
}


static void
rasqal_rdfr_triplestore_statement_handler(void *user_data,
#ifndef HAVE_RAPTOR2_API
                                          const
#endif
                                          raptor_statement *statement)
{
  rasqal_rdfr_triplestore* rts;
  rasqal_rdfr_triple *triple;
  
  rts = (rasqal_rdfr_triplestore*)user_data;

  triple = (rasqal_rdfr_triple*)RASQAL_MALLOC(rasqal_rdfr_triple,
                                              sizeof(*triple));
  triple->next = NULL;
  triple->triple = raptor_statement_as_rasqal_triple(rts->world,
                                                     statement);

#if 0
  /* this origin URI literal is shared amongst the triples and
   * freed only in rasqal_raptor_free_triples_source
   */
  rasqal_triple_set_origin(triple->triple, rts->base_uri_literal);
#endif

  if(rts->tail)
    rts->tail->next = triple;
  else
    rts->head = triple;

  rts->tail = triple;
}


static int
rasqal_rdfr_triplestore_parse_iostream(rasqal_rdfr_triplestore* rts,
                                       const char* format_name,
                                       raptor_iostream* iostr,
                                       raptor_uri* base_uri)
{
  raptor_parser* parser;

  rts->base_uri_literal = rasqal_new_uri_literal(rts->world,
                                                 raptor_uri_copy(base_uri));

  if(format_name) {
    if(!raptor_world_is_parser_name(rts->world->raptor_world_ptr,
                                    format_name)) {
      rasqal_log_error_simple(rts->world, RAPTOR_LOG_LEVEL_ERROR,
                              /* locator */ NULL,
                              "Invalid format name %s ignored",
                              format_name);
      format_name = NULL;
    }
  }

  if(!format_name)
    format_name = "guess";

  /* parse iostr with parser and base_uri */
#ifdef HAVE_RAPTOR2_API
  parser = raptor_new_parser(rts->world->raptor_world_ptr, format_name);
  raptor_parser_set_statement_handler(parser, rts,
                                      rasqal_rdfr_triplestore_statement_handler);
#else
  parser = raptor_new_parser(format_name);
  raptor_set_statement_handler(parser, rts,
                               rasqal_rdfr_triplestore_statement_handler);
  raptor_set_error_handler(parser, rts->world, rasqal_raptor_error_handler);
#endif
  
  /* parse and store triples */
#ifdef HAVE_RAPTOR2_API
  raptor_parser_parse_iostream(parser, iostr, base_uri);
#else
  rasqal_raptor_parse_iostream(parser, iostr, base_uri);
#endif
    
  raptor_free_parser(parser);

  return 0;
}


static void rasqal_free_rdfr_term_iterator(rasqal_rdfr_term_iterator* iter);
static int rasqal_rdfr_term_iterator_next(rasqal_rdfr_term_iterator* iter);


static rasqal_rdfr_term_iterator*
rasqal_rdfr_triplestore_init_match_internal(rasqal_rdfr_triplestore* rts,
                                            rasqal_literal* subject,
                                            rasqal_literal* predicate,
                                            rasqal_literal* object)
{
  rasqal_rdfr_term_iterator* iter;

  if(!rts)
    return NULL;
  
  iter = (rasqal_rdfr_term_iterator*)RASQAL_CALLOC(rasqal_rdfr_term_iterator,
                                                   1, sizeof(*iter));

  if(!iter)
    return NULL;

  iter->triplestore = rts;
  
  iter->match.subject = subject;
  iter->match.predicate = predicate;
  iter->match.object = object;

  iter->cursor = NULL;

  if(!subject)
    iter->want = RASQAL_TRIPLE_SUBJECT;
  else if(!object)
    iter->want = RASQAL_TRIPLE_OBJECT;
  else
    iter->want = RASQAL_TRIPLE_PREDICATE;
  iter->parts = RASQAL_TRIPLE_SPO ^ iter->want;
  
  if(rasqal_rdfr_term_iterator_next(iter)) {
    rasqal_free_rdfr_term_iterator(iter);
    return NULL;
  }
  
  return iter;
}


static void
rasqal_free_rdfr_term_iterator(rasqal_rdfr_term_iterator* iter)
{
  RASQAL_FREE(rasqal_rdfr_term_iterator, iter);
}


static rasqal_literal*
rasqal_rdfr_term_iterator_get(rasqal_rdfr_term_iterator* iter)
{
  if(!iter)
    return NULL;
  
  if(!iter->cursor)
    return NULL;

  if(iter->want == RASQAL_TRIPLE_SUBJECT)
    return iter->cursor->triple->subject;
  else
    return iter->cursor->triple->object;
}


static int
rasqal_rdfr_term_iterator_next(rasqal_rdfr_term_iterator* iter)
{
  if(!iter)
    return 1;
  
  while(1) { 
    if(iter->cursor)
      iter->cursor = iter->cursor->next;
    else
      iter->cursor = iter->triplestore->head;
    
    if(!iter->cursor)
      break;
    
#if RASQAL_DEBUG > 2
    RASQAL_DEBUG1("Matching against triple: ");
    rasqal_triple_print(iter->cursor->triple, DEBUG_FH);
    fputc('\n', DEBUG_FH);
#endif

    if(rasqal_raptor_triple_match(iter->triplestore->world,
                                  iter->cursor->triple, &iter->match,
                                  iter->parts))
      return 0;
  }
  
  return 1;
}




static rasqal_rdfr_term_iterator*
rasqal_new_rdfr_triplestore_get_sources_iterator(rasqal_rdfr_triplestore* rts,
                                                 rasqal_literal* predicate,
                                                 rasqal_literal* object)
{
  return rasqal_rdfr_triplestore_init_match_internal(rts,
                                                     NULL, predicate, object);
}

static rasqal_rdfr_term_iterator*
rasqal_new_rdfr_triplestore_get_targets_iterator(rasqal_rdfr_triplestore* rts,
                                                 rasqal_literal* subject,
                                                 rasqal_literal* predicate)
{
  return rasqal_rdfr_triplestore_init_match_internal(rts,
                                                     subject, predicate, NULL);
}


static rasqal_literal*
rasqal_new_rdfr_triplestore_get_source(rasqal_rdfr_triplestore* rts,
                                       rasqal_literal* predicate,
                                       rasqal_literal* object)
{
  rasqal_literal *literal;
  
  rasqal_rdfr_term_iterator* iter;
  
  iter = rasqal_new_rdfr_triplestore_get_sources_iterator(rts, predicate,
                                                          object);
  if(!iter)
    return NULL;

  literal = rasqal_rdfr_term_iterator_get(iter);

  rasqal_free_rdfr_term_iterator(iter);
  return literal;
}


static rasqal_literal*
rasqal_new_rdfr_triplestore_get_target(rasqal_rdfr_triplestore* rts,
                                       rasqal_literal* subject,
                                       rasqal_literal* predicate)
{
  rasqal_literal *literal;
  
  rasqal_rdfr_term_iterator* iter;
  
  iter = rasqal_new_rdfr_triplestore_get_targets_iterator(rts, subject,
                                                          predicate);
  if(!iter)
    return NULL;

  literal = rasqal_rdfr_term_iterator_get(iter);

  rasqal_free_rdfr_term_iterator(iter);
  return literal;
}


static void
rasqal_free_rdfr_triplestore(rasqal_rdfr_triplestore* rts) 
{
  rasqal_rdfr_triple *cur;

  if(!rts)
    return;

  cur = rts->head;
  while(cur) {
    rasqal_rdfr_triple *next = cur->next;

#if 0
    rasqal_triple_set_origin(cur->triple, NULL); /* shared URI literal */
#endif
    rasqal_free_triple(cur->triple);
    RASQAL_FREE(rasqal_rdfr_triple, cur);
    cur = next;
  }

  if(rts->base_uri_literal)
    rasqal_free_literal(rts->base_uri_literal);

  RASQAL_FREE(rasqal_rdfr_triplestore, rts);
}


typedef struct 
{
  rasqal_world* world;
#ifdef RAPTOR_V2_AVAILABLE
  raptor_world *raptor_world_ptr;
#endif
  rasqal_rowsource* rowsource;

  int failed;

  raptor_uri* rs_uri;

  /* Input fields */
  raptor_uri* base_uri;
  raptor_iostream* iostr;

  /* Parsing fields */
  int parsed;
  rasqal_rdfr_triplestore* triplestore;
  const char* format_name;
  rasqal_row* row; /* current result row */

  int offset; /* current result row number */
  unsigned char buffer[FILE_READ_BUF_SIZE]; /* iostream read buffer */

  /* Output fields */
  raptor_sequence* results_sequence; /* saved result rows */

  /* Variables table allocated for variables in the result set */
  rasqal_variables_table* vars_table;
  int variables_count;
} rasqal_rowsource_rdf_context;


static const unsigned char* rs_namespace_uri_string =
  (const unsigned char*)"http://www.w3.org/2001/sw/DataAccess/tests/result-set#";


/*
 * rasqal_query_results_write_rdf:
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
rasqal_query_results_write_rdf(raptor_iostream *iostr,
                               rasqal_query_results* results,
                               raptor_uri *base_uri)
{
  return 0;
}



/* Local handlers for turning RDF graph read from an iostream into rows */

static int
rasqal_rowsource_rdf_init(rasqal_rowsource* rowsource, void *user_data) 
{
  rasqal_rowsource_rdf_context* con;

  con = (rasqal_rowsource_rdf_context*)user_data;

  con->rowsource = rowsource;

  con->world = rowsource->world;
#ifdef RAPTOR_V2_AVAILABLE
  con->raptor_world_ptr = rasqal_world_get_raptor(rowsource->world);
#endif

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

  if(con->triplestore)
    rasqal_free_rdfr_triplestore(con->triplestore);

  RASQAL_FREE(rasqal_rowsource_rdf_context, con);

  return 0;
}


static int
rasqal_rowsource_rdf_process(rasqal_rowsource_rdf_context* con)
{
  const char* format_name = "guess";
  raptor_uri* rdf_ns_uri;
  raptor_uri* uri;
  rasqal_literal* predicate_uri_literal;
  rasqal_literal* object_uri_literal;
  rasqal_literal* resultSet_node;
  rasqal_literal* solution_node;
  rasqal_rdfr_term_iterator* solution_iter;
  rasqal_literal* variable_predicate;
  rasqal_literal* value_predicate;

  if(con->parsed)
    return 0;
  
  con->triplestore = rasqal_new_rdfr_triplestore(con->world);
  
  if(rasqal_rdfr_triplestore_parse_iostream(con->triplestore,
                                            format_name,
                                            con->iostr, con->base_uri)) {
    return 1;
  }

  rdf_ns_uri = raptor_new_uri(con->raptor_world_ptr, raptor_rdf_namespace_uri);

  uri = raptor_new_uri_from_uri_local_name(con->raptor_world_ptr,
                                           rdf_ns_uri,
                                           (const unsigned char*)"type");
  predicate_uri_literal = rasqal_new_uri_literal(con->world, uri);

  raptor_free_uri(rdf_ns_uri); rdf_ns_uri = NULL;

  uri = raptor_new_uri_from_uri_local_name(con->raptor_world_ptr,
                                           con->rs_uri,
                                           (const unsigned char*)"ResultSet");
  object_uri_literal = rasqal_new_uri_literal(con->world, uri);

  uri = raptor_new_uri_from_uri_local_name(con->raptor_world_ptr,
                                           con->rs_uri,
                                           (const unsigned char*)"variable");
  variable_predicate = rasqal_new_uri_literal(con->world, uri);
  
  uri = raptor_new_uri_from_uri_local_name(con->raptor_world_ptr,
                                           con->rs_uri,
                                           (const unsigned char*)"value");
  value_predicate = rasqal_new_uri_literal(con->world, uri);
  



  /* find result set node ?rs := getSource(a, rs:ResultSet)  */
  resultSet_node = rasqal_new_rdfr_triplestore_get_source(con->triplestore,
                                                          predicate_uri_literal,
                                                          object_uri_literal);

  rasqal_free_literal(predicate_uri_literal); predicate_uri_literal = NULL;
  rasqal_free_literal(object_uri_literal); object_uri_literal = NULL;
  

  /* if no such triple, expecting empty results */
  if(!resultSet_node)
    return 0;
  
#if RASQAL_DEBUG > 2
  RASQAL_DEBUG1("Got result set node ");
  rasqal_literal_print(resultSet_node, DEBUG_FH);
  fputc('\n', DEBUG_FH);
#endif    



  /* find variable names from all ?var := getTargets(?rs, rs:resultVariable) */
  uri = raptor_new_uri_from_uri_local_name(con->raptor_world_ptr,
                                           con->rs_uri,
                                           (const unsigned char*)"resultVariable");
  predicate_uri_literal = rasqal_new_uri_literal(con->world, uri);

  solution_iter = rasqal_new_rdfr_triplestore_get_targets_iterator(con->triplestore,
                                                                   resultSet_node,
                                                                   predicate_uri_literal);
  while(1) {
    solution_node = rasqal_rdfr_term_iterator_get(solution_iter);
    if(!solution_node)
      break;

#if RASQAL_DEBUG > 2
    RASQAL_DEBUG1("Got variable node ");
    rasqal_literal_print(solution_node, DEBUG_FH);
    fputc('\n', DEBUG_FH);
#endif
    
    if(1) {
      const unsigned char* name;

      name = rasqal_literal_as_string(solution_node);
      if(name) {
        unsigned char* var_name;
        rasqal_variable *v;
        size_t len;
        
        len = strlen((const char*)name);

        var_name = (unsigned char*)RASQAL_MALLOC(cstring, len + 1);
        memcpy(var_name, name, len + 1);

        v = rasqal_variables_table_add(con->vars_table,
                                       RASQAL_VARIABLE_TYPE_NORMAL,
                                       var_name, NULL);
        if(v)
          rasqal_rowsource_add_variable(con->rowsource, v);
      }
    }
      
    rasqal_rdfr_term_iterator_next(solution_iter);
  }
  rasqal_free_rdfr_term_iterator(solution_iter); solution_iter = NULL;



  /* for each solution node ?sol := getTargets(?rs, rs:solution) */
  uri = raptor_new_uri_from_uri_local_name(con->raptor_world_ptr,
                                           con->rs_uri,
                                           (const unsigned char*)"solution");
  predicate_uri_literal = rasqal_new_uri_literal(con->world, uri);

  solution_iter = rasqal_new_rdfr_triplestore_get_targets_iterator(con->triplestore,
                                                                   resultSet_node,
                                                                   predicate_uri_literal);
  while(1) {
    rasqal_rdfr_term_iterator* binding_iter;
    rasqal_row* row;
    
    solution_node = rasqal_rdfr_term_iterator_get(solution_iter);
    if(!solution_node)
      break;

#if RASQAL_DEBUG > 2
    RASQAL_DEBUG1("Got solution node (row) ");
    rasqal_literal_print(solution_node, DEBUG_FH);
    fputc('\n', DEBUG_FH);
#endif

    row = rasqal_new_row(con->rowsource);
    
    uri = raptor_new_uri_from_uri_local_name(con->raptor_world_ptr,
                                             con->rs_uri,
                                             (const unsigned char*)"binding");
    predicate_uri_literal = rasqal_new_uri_literal(con->world, uri);

    /* for each binding node ?bn := getTargets(?sol, rs:binding) */
    binding_iter = rasqal_new_rdfr_triplestore_get_targets_iterator(con->triplestore,
                                                                    solution_node,
                                                                    predicate_uri_literal);
    while(1) {
      rasqal_literal* binding_node;
      rasqal_literal* var_literal;
      rasqal_literal* value_literal;
      
      binding_node = rasqal_rdfr_term_iterator_get(binding_iter);
      if(!binding_node)
        break;

#if RASQAL_DEBUG > 2
      RASQAL_DEBUG1("  Got binding node ");
      rasqal_literal_print(binding_node, DEBUG_FH);
      fputc('\n', DEBUG_FH);
#endif

      /* variable ?var := getTarget(?bn, rs:variable) */
      var_literal = rasqal_new_rdfr_triplestore_get_target(con->triplestore,
                                                           binding_node,
                                                           variable_predicate);

      /* variable ?val := getTarget(?bn, rs:value) */
      value_literal = rasqal_new_rdfr_triplestore_get_target(con->triplestore,
                                                             binding_node,
                                                             value_predicate);

#if RASQAL_DEBUG > 2
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

      rasqal_rdfr_term_iterator_next(binding_iter);
    }

    rasqal_free_rdfr_term_iterator(binding_iter); binding_iter = NULL;

    /* save row at end of sequence of rows */
    raptor_sequence_push(con->results_sequence, row);

    rasqal_rdfr_term_iterator_next(solution_iter);
  }
  rasqal_free_rdfr_term_iterator(solution_iter); solution_iter = NULL;

  rasqal_free_literal(predicate_uri_literal); predicate_uri_literal = NULL;
  

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
  /* .set_preserve = */ NULL,
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
rasqal_query_results_get_rowsource_rdf(rasqal_world *world,
                                       rasqal_variables_table* vars_table,
                                       raptor_iostream *iostr,
                                       raptor_uri *base_uri)
{
  rasqal_rowsource_rdf_context* con;
  
  con = (rasqal_rowsource_rdf_context*)RASQAL_CALLOC(rasqal_rowsource_rdf_context, 1, sizeof(*con));
  if(!con)
    return NULL;

  con->world = world;
  con->base_uri = base_uri ? raptor_uri_copy(base_uri) : NULL;
  con->iostr = iostr;

#ifdef HAVE_RAPTOR2_API
  con->results_sequence = raptor_new_sequence((raptor_data_free_handler)rasqal_free_row, (raptor_data_print_handler)rasqal_row_print);
#else
  con->results_sequence = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_row, (raptor_sequence_print_handler*)rasqal_row_print);
#endif

  con->vars_table = rasqal_new_variables_table_from_variables_table(vars_table);
  
  return rasqal_new_rowsource_from_handler(world, NULL,
                                           con,
                                           &rasqal_rowsource_rdf_handler,
                                           con->vars_table,
                                           0);
}



int
rasqal_init_result_format_rdf(rasqal_world* world)
{
  rasqal_query_results_formatter_func writer_fn = NULL;
  rasqal_query_results_formatter_func reader_fn = NULL;
  rasqal_query_results_get_rowsource_func get_rowsource_fn = NULL;
  int rc = 0;

  writer_fn = &rasqal_query_results_write_rdf;
  reader_fn = NULL,
  get_rowsource_fn = &rasqal_query_results_get_rowsource_rdf;

  rc += rasqal_query_results_format_register_factory(world,
                                                     "rdfxml",
                                                     "RDF/XML Query Results",
                                                     NULL,
                                                     writer_fn, reader_fn, get_rowsource_fn,
                                                     "application/rdf+xml")
                                                     != 0;

  return rc;
}


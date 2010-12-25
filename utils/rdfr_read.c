/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * srxread.c - RDF Results Format reading test program
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
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif


#include <raptor.h>

/* Rasqal includes */
#include <rasqal.h>
#include <rasqal_internal.h>

#ifdef RAPTOR_V2_AVAILABLE
#else
#define raptor_new_uri(world, string) raptor_new_uri(string)
#define raptor_new_iostream_from_filename(world, filename) raptor_new_iostream_from_filename(filename)
#define raptor_new_iostream_to_file_handle(world, fh) raptor_new_iostream_to_file_handle(fh)
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

  /* this origin URI literal is shared amongst the triples and
   * freed only in rasqal_raptor_free_triples_source
   */
  rasqal_triple_set_origin(triple->triple, rts->base_uri_literal);

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
  parser = raptor_new_parser(parser_name);
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


static void
rasqal_free_rdfr_triplestore(rasqal_rdfr_triplestore* rts) 
{
  rasqal_rdfr_triple *cur;

  if(!rts)
    return;

  cur = rts->head;
  while(cur) {
    rasqal_rdfr_triple *next = cur->next;

    rasqal_triple_set_origin(cur->triple, NULL); /* shared URI literal */
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

  const char* format_name;
  
  raptor_uri* rs;

  raptor_uri* base_uri;

  rasqal_rdfr_triplestore* triplestore;
} rasqal_rdfr_context;



static
rasqal_rdfr_context*
rasqal_new_rdfr_context(rasqal_world* world)
{
  rasqal_rdfr_context* rdfrc;
  
  if(!world)
    return NULL;
  
  rdfrc = (rasqal_rdfr_context*)RASQAL_CALLOC(rs_read, 1, sizeof(*rdfrc));

  rdfrc->world = world;
#ifdef RAPTOR_V2_AVAILABLE
  rdfrc->raptor_world_ptr = rasqal_world_get_raptor(world);
#endif

  rdfrc->rs = raptor_new_uri(rdfrc->raptor_world_ptr,
                             (const unsigned char*)"http://www.w3.org/2001/sw/DataAccess/tests/result-set#");

  return rdfrc;
}


static void
rasqal_free_rdfr_context(rasqal_rdfr_context* rdfrc) 
{
  if(!rdfrc)
    return;

  if(rdfrc->rs)
    raptor_free_uri(rdfrc->rs);

  if(rdfrc->triplestore)
    rasqal_free_rdfr_triplestore(rdfrc->triplestore);

  if(rdfrc->base_uri)
    raptor_free_uri(rdfrc->base_uri);

  RASQAL_FREE(rdfr_context, rdfrc);
}


static int
rasqal_rdf_results_read(rasqal_world *world,
                        raptor_iostream *iostr,
                        rasqal_query_results* results,
                        raptor_uri *base_uri)
{
  rasqal_rdfr_context *rdfrc;
  const char* format_name = "guess";
  
  rdfrc = rasqal_new_rdfr_context(world);

  rdfrc->base_uri = raptor_uri_copy(base_uri);

  rdfrc->triplestore = rasqal_new_rdfr_triplestore(world);
  
  if(rasqal_rdfr_triplestore_parse_iostream(rdfrc->triplestore,
                                            format_name,
                                            iostr, rdfrc->base_uri)) {
    return 1;
  }
  

  /* find result set node ?rs := getSource(a, rs:ResultSet)  */
  
  /* if no such triple, expecting empty results */

  /* for each solution node ?sol := getTargets(?rs, rs:solution) */

    /* for each binding node ?bn := getTargets(?sol, rs:binding) */

      /* variable ?var := getTarget(?bn, rs:variable) */

      /* variable ?val := getTarget(?bn, rs:value) */

      /* save row[?var] = ?val */

      /* ?index := getTarget(?sol, rs:index)
       * if ?index exists then that is the integer order of the row
       * in the list of row results */

    /* end for each binding */

    /* save row at end of sequence of rows */

  /* end for each solution */

  /* sort sequence of rows by ?index or original order */

  /* generate rows */

  rasqal_free_rdfr_context(rdfrc);

  return 0;
}


static char *program = NULL;

int main(int argc, char *argv[]);

int
main(int argc, char *argv[]) 
{ 
  int rc = 0;
  const char* rdf_filename = NULL;
  raptor_iostream* iostr = NULL;
  char* p;
  unsigned char* uri_string = NULL;
  raptor_uri* base_uri = NULL;
  rasqal_query_results* results = NULL;
  const char* write_formatter_name = NULL;
  rasqal_query_results_formatter* write_formatter = NULL;
  raptor_iostream *write_iostr = NULL;
  rasqal_world *world;
  rasqal_variables_table* vars_table;
#ifdef RAPTOR_V2_AVAILABLE
  raptor_world *raptor_world_ptr;
#endif
  
  program = argv[0];
  if((p = strrchr(program, '/')))
    program = p + 1;
  else if((p = strrchr(program, '\\')))
    program = p + 1;
  argv[0] = program;
  
  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    return(1);
  }

  if(argc < 2 || argc > 3) {
    fprintf(stderr, "USAGE: %s RDF query results file [write formatter]\n",
            program);

    rc = 1;
    goto tidy;
  }

  rdf_filename = argv[1];
  if(argc > 2) {
    if(strcmp(argv[2], "-"))
      write_formatter_name = argv[2];
  }

#ifdef RAPTOR_V2_AVAILABLE
  raptor_world_ptr = rasqal_world_get_raptor(world);
#endif
  
  uri_string = raptor_uri_filename_to_uri_string((const char*)rdf_filename);
  if(!uri_string)
    goto tidy;
  
  base_uri = raptor_new_uri(raptor_world_ptr, uri_string);
  raptor_free_memory(uri_string);

  vars_table = rasqal_new_variables_table(world);
  results = rasqal_new_query_results(world, NULL,
                                     RASQAL_QUERY_RESULTS_BINDINGS, vars_table);
  rasqal_free_variables_table(vars_table);
  if(!results) {
    fprintf(stderr, "%s: Failed to create query results\n", program);
    rc = 1;
    goto tidy;
  }
  

  iostr = raptor_new_iostream_from_filename(raptor_world_ptr, rdf_filename);
  if(!iostr) {
    fprintf(stderr, "%s: Failed to open iostream to file %s\n", program,
            rdf_filename);
    rc = 1;
    goto tidy;
  }

  rc = rasqal_rdf_results_read(world, iostr, results, base_uri);

  write_formatter = rasqal_new_query_results_formatter2(world, 
                                                        write_formatter_name,
                                                        NULL, NULL);
  if(!write_formatter) {
    fprintf(stderr, "%s: Failed to create query results write formatter '%s'\n",
            program, write_formatter_name);
    rc = 1;
    goto tidy;
  }
  
  write_iostr = raptor_new_iostream_to_file_handle(raptor_world_ptr, stdout);
  if(!write_iostr) {
    fprintf(stderr, "%s: Creating output iostream failed\n", program);
  } else {
    rasqal_query_results_formatter_write(write_iostr, write_formatter,
                                         results, base_uri);
    raptor_free_iostream(write_iostr);
  }


  tidy:
  
  if(write_formatter)
    rasqal_free_query_results_formatter(write_formatter);

  if(iostr)
    raptor_free_iostream(iostr);
  
  if(results)
    rasqal_free_query_results(results);

  if(base_uri)
    raptor_free_uri(base_uri);

  rasqal_free_world(world);

  return (rc);
}

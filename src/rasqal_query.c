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
  rasqal_query* rdf_query;
  raptor_uri_handler *uri_handler;
  void *uri_context;

  factory=rasqal_get_query_engine_factory(name, uri);
  if(!factory)
    return NULL;

  rdf_query=(rasqal_query*)RASQAL_CALLOC(rasqal_query, 1, 
                                         sizeof(rasqal_query));
  if(!rdf_query)
    return NULL;
  
  rdf_query->context=(char*)RASQAL_CALLOC(rasqal_query_context, 1,
                                          factory->context_length);
  if(!rdf_query->context) {
    rasqal_free_query(rdf_query);
    return NULL;
  }
  
  rdf_query->factory=factory;

  rdf_query->failed=0;

  raptor_uri_get_handler(&uri_handler, &uri_context);
  rdf_query->namespaces=raptor_new_namespaces(uri_handler, uri_context,
                                              rasqal_query_simple_error,
                                              rdf_query,
                                              0);


  rdf_query->variables_sequence=rasqal_new_sequence((rasqal_free_handler*)rasqal_free_variable, (rasqal_print_handler*)rasqal_print_variable);


  if(factory->init(rdf_query, name)) {
    rasqal_free_query(rdf_query);
    return NULL;
  }
  
  return rdf_query;
}



/**
 * rasqal_free_query - destructor - destroy a rasqal_query object
 * @query: &rasqal_query object
 * 
 **/
void
rasqal_free_query(rasqal_query* rdf_query) 
{
  if(rdf_query->factory)
    rdf_query->factory->terminate(rdf_query);

  if(rdf_query->context)
    RASQAL_FREE(rasqal_query_context, rdf_query->context);

  if(rdf_query->namespaces)
    raptor_free_namespaces(rdf_query->namespaces);

  if(rdf_query->base_uri)
    raptor_free_uri(rdf_query->base_uri);

  if(rdf_query->query_string)
    RASQAL_FREE(cstring, rdf_query->query_string);

  if(rdf_query->selects)
    rasqal_free_sequence(rdf_query->selects);
  if(rdf_query->sources)
    rasqal_free_sequence(rdf_query->sources);
  if(rdf_query->triples)
    rasqal_free_sequence(rdf_query->triples);
  if(rdf_query->prefixes)
    rasqal_free_sequence(rdf_query->prefixes);
  if(rdf_query->ordered_triples)
    rasqal_free_sequence(rdf_query->ordered_triples);
  if(rdf_query->variables)
    RASQAL_FREE(vararray, rdf_query->variables);
  if(rdf_query->variables_sequence)
    rasqal_free_sequence(rdf_query->variables_sequence);

  RASQAL_FREE(rasqal_query, rdf_query);
}


/* Methods */

/**
 * rasqal_get_name: Return the short name for the query
 * @query: &rasqal_query query object
 **/
const char*
rasqal_get_name(rasqal_query *rdf_query)
{
  return rdf_query->factory->name;
}


/**
 * rasqal_get_label: Return a readable label for the query
 * @query: &rasqal_query query object
 **/
const char*
rasqal_get_label(rasqal_query *rdf_query)
{
  return rdf_query->factory->label;
}


void
rasqal_query_add_source(rasqal_query* query, const unsigned char* uri) {
  rasqal_sequence_shift(query->sources, (void*)uri);
}

rasqal_sequence*
rasqal_query_get_source_sequence(rasqal_query* query) {
  return query->sources;
}

const unsigned char *
rasqal_query_get_source(rasqal_query* query, int idx) {
  return rasqal_sequence_get_at(query->sources, idx);
}


int
rasqal_query_has_variable(rasqal_query* query, const char *name) {
  int i;

  for(i=0; i< rasqal_sequence_size(query->selects); i++) {
    rasqal_variable* v=rasqal_sequence_get_at(query->selects, i);
    if(!strcmp(v->name, name))
      return 1;
  }
  return 0;
}


int
rasqal_query_set_variable(rasqal_query* query, const char *name,
                          rasqal_expression* value) {
  int i;

  for(i=0; i< rasqal_sequence_size(query->selects); i++) {
    rasqal_variable* v=rasqal_sequence_get_at(query->selects, i);
    if(!strcmp(v->name, name)) {
      if(v->value)
        rasqal_free_expression(v->value);
      v->value=value;
      return 0;
    }
  }
  return 1;
}


/**
 * rasqal_query_prepare: Prepare a query - typically parse it
 * @rdf_query: the &rasqal_query object
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
rasqal_query_prepare(rasqal_query *rdf_query,
                     const unsigned char *query_string,
                     raptor_uri *base_uri)
{
  rdf_query->query_string=(char*)RASQAL_MALLOC(cstring, strlen(query_string)+1);
  strcpy((char*)rdf_query->query_string, (const char*)query_string);

  if(base_uri)
    base_uri=raptor_uri_copy(base_uri);
  else {
    const char *uri_string=raptor_uri_filename_to_uri_string("");
    base_uri=raptor_new_uri(uri_string);
    SYSTEM_FREE((void*)uri_string);
  }
  
  rdf_query->base_uri=base_uri;
  rdf_query->locator.uri=base_uri;
  rdf_query->locator.line= rdf_query->locator.column = 0;

  return rdf_query->factory->prepare(rdf_query);
}

librdf_world *world=NULL;


/**
 * rasqal_query_execute: excute a query - run and return results
 * @rdf_query: the &rasqal_query object
 *
 * fixme: not implemented
 *
 * return value: non-0 on failure.
 **/
int
rasqal_query_execute(rasqal_query *rdf_query)
{
  int rc=0;
  librdf_storage *storage;
  librdf_model *model;
  librdf_parser *parser;
  raptor_uri *source0;
  librdf_uri *r_source0;
  
  if(rasqal_query_order_triples(rdf_query))
    return 1;

  source0=(raptor_uri*)rasqal_sequence_get_at(rdf_query->sources, 0);
  //r_source0=librdf_new_uri(world, raptor_uri_as_string(source0));
  r_source0=librdf_new_uri(world, "file:///home/dajobe/rdf/redland/rasqal/jobs-rss.rdf");

  storage = librdf_new_storage(world, NULL, NULL, NULL);
  model = librdf_new_model(world, storage, NULL);

  parser=librdf_new_parser(world, NULL, NULL, NULL);
  librdf_parser_parse_into_model(parser, r_source0, NULL, model);
  librdf_free_parser(parser);
                                 
  rdf_query->world=world;
  rdf_query->model=model;
  rasqal_select_next(rdf_query, 0);

  if(rdf_query->factory->execute)
    rc=rdf_query->factory->execute(rdf_query);
  else
    rc=1;

  librdf_free_uri(r_source0);
  
  librdf_free_model(model);
  librdf_free_storage(storage);

  return rc;
}


/* Utility methods */
void
rasqal_query_print(rasqal_query* query, FILE *fh) {
  fprintf(fh, "selects: ");
  rasqal_sequence_print(query->selects, fh);
  fprintf(fh, "\nsources: ");
  rasqal_sequence_print(query->sources, fh);
  fprintf(fh, "\ntriples: ");
  rasqal_sequence_print(query->triples, fh);
  if(query->ordered_triples) {
    fprintf(fh, "\nordered triples: ");
    rasqal_sequence_print(query->ordered_triples, fh);
  }
  fprintf(fh, "\nconstraints: ");
  rasqal_sequence_print(query->constraints, fh);
  fprintf(fh, "\nprefixes: ");
  rasqal_sequence_print(query->prefixes, fh);
  fputc('\n', fh);
}


#ifdef STANDALONE
#include <stdio.h>
#include <locale.h>



int rdql_parser_error(const char *msg);
int main(int argc, char *argv[]);


extern char *filename;
extern int lineno;
char *program;


int
rdql_parser_error(const char *msg) 
{
  fprintf(stderr, "%s: Query parsing error: %s\n", program, msg);
  return (0);
}


#define RDQL_FILE_BUF_SIZE 2048

int
main(int argc, char *argv[]) 
{ 
  char *query_string=NULL;
  rasqal_query *rq;
  char *ql_name;
  char *ql_uri;
  int rc=0;
  raptor_uri *base_uri=NULL;
  char *p;
  
  program=argv[0];
  if((p=strrchr(program, '/')))
    program=p+1;
  else if((p=strrchr(program, '\\')))
    program=p+1;
  argv[0]=program;
  
  if(argc < 3 || argc > 4) {
    fprintf(stderr, "USAGE: %s QUERY-LANGUAGE|- QUERY-URI|- [QUERY-STRING|-]\n", 
            program);
    exit(1);
  }

  ql_name=!strcmp(argv[1], "-") ? NULL : argv[1];
  ql_uri=!strcmp(argv[1], "-") ? NULL : argv[2];

  rasqal_init();

  world=librdf_new_world();
  librdf_world_open(world);

  if(!argv[2] || !strcmp(argv[2], "-")) {
    query_string=(char*)calloc(RDQL_FILE_BUF_SIZE, 1);
    fread(query_string, RDQL_FILE_BUF_SIZE, 1, stdin);
  } else
    query_string=argv[3];

  rq=rasqal_new_query(ql_name, ql_uri);

  if(rasqal_query_prepare(rq, query_string, base_uri)) {
    fprintf(stderr, "%s: Parsing query '%s' failed\n", program, query_string);
    rc=1;
  }

  fprintf(stdout, "Query:\n");
  rasqal_query_print(rq, stdout);

  if(rasqal_query_execute(rq)) {
    fprintf(stderr, "%s: Query execution failed\n", program);
    rc=1;
  }

  fprintf(stdout, "After execution, Query:\n");
  rasqal_query_print(rq, stdout);


  rasqal_free_query(rq);

  if(!strcmp(argv[2], "-")) {
    free(query_string);
  }

  if(base_uri)
    raptor_free_uri(base_uri);

  rasqal_finish();

  librdf_free_world(world);
  
  return (rc);
}
#endif

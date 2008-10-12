/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_triples.c - Rasqal triple pattern rowsource class
 *
 * Copyright (C) 2008, David Beckett http://www.dajobe.org/
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

#include "rasqal.h"
#include "rasqal_internal.h"


#ifndef STANDALONE

typedef struct 
{
  rasqal_query *query;

  /* source of triple pattern matches */
  rasqal_triples_source* triples_source;

  /* sequence of triple SHARED with query */
  raptor_sequence* triples;

  /* first triple pattern in sequence to use */
  int start_column;

  /* last triple pattern in sequence to use */
  int end_column;

  /* number of triple patterns in the sequence
     ( = end_column - start_column + 1) */
  int triples_count;
  
  /* An array of items, one per triple pattern in the sequence */
  rasqal_triple_meta* triple_meta;

  /* offset into results for current row */
  int offset;
  
  /* number of variables used in variables table  */
  int size;
  
  int new_bindings_count;

} rasqal_triples_rowsource_context;


static int
rasqal_triples_rowsource_init(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_query *query;
  rasqal_triples_rowsource_context *con;
  int column;

  con = (rasqal_triples_rowsource_context*)user_data;
  query = con->query;

  for(column = con->start_column; column <= con->end_column; column++) {
    rasqal_triple_meta *m;
    rasqal_triple *t;
    rasqal_variable* v;

    m = &con->triple_meta[column - con->start_column];
    if(!m)
      return 1;

    m->parts = (rasqal_triple_parts)0;

    t = (rasqal_triple*)raptor_sequence_get_at(con->triples, column);
    
    if((v = rasqal_literal_as_variable(t->subject)) &&
       query->variables_declared_in[v->offset] == column)
      m->parts = (rasqal_triple_parts)(m->parts | RASQAL_TRIPLE_SUBJECT);
    
    if((v = rasqal_literal_as_variable(t->predicate)) &&
       query->variables_declared_in[v->offset] == column)
      m->parts = (rasqal_triple_parts)(m->parts | RASQAL_TRIPLE_PREDICATE);
    
    if((v = rasqal_literal_as_variable(t->object)) &&
       query->variables_declared_in[v->offset] == column)
      m->parts = (rasqal_triple_parts)(m->parts | RASQAL_TRIPLE_OBJECT);

    if(t->origin &&
       (v = rasqal_literal_as_variable(t->origin)) &&
       query->variables_declared_in[v->offset] == column)
      m->parts = (rasqal_triple_parts)(m->parts | RASQAL_TRIPLE_ORIGIN);

    RASQAL_DEBUG3("triple pattern column %d has parts %d\n", column, m->parts);

    /* exact if there are no variables in the triple parts */
    m->is_exact = 1;
    if(rasqal_literal_as_variable(t->predicate) ||
       rasqal_literal_as_variable(t->subject) ||
       rasqal_literal_as_variable(t->object))
      m->is_exact = 0;

  }
  
  return 0;
}


static int
rasqal_triples_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                          void *user_data)
{
  rasqal_triples_rowsource_context* con;
  con = (rasqal_triples_rowsource_context*)user_data; 

  rowsource->size = con->size;
  rowsource->order_size = -1;
  
  return 0;
}


static int
rasqal_triples_rowsource_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_triples_rowsource_context *con;
  con = (rasqal_triples_rowsource_context*)user_data;

  if(con->triple_meta)
    RASQAL_FREE(rasqal_triple_meta, con->triple_meta);

  RASQAL_FREE(rasqal_triples_rowsource_context, con);

  return 0;
}


static rasqal_row*
rasqal_triples_rowsource_read_row(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_query *query;
  rasqal_triples_rowsource_context *con;
  int column;
#ifdef RASQAL_DEBUG
  int values_returned = 0;
#endif
  rasqal_row *row = NULL;
  int i;
  
  con = (rasqal_triples_rowsource_context*)user_data;
  query = con->query;
  
  con->new_bindings_count = 0;

  column = con->start_column;

  while(column >= con->start_column) {
    rasqal_triple_meta *m;
    rasqal_triple *t;

    m = &con->triple_meta[column - con->start_column];
    t = (rasqal_triple*)raptor_sequence_get_at(con->triples, column);

    if(!m) {
      /* error recovery - no match */
      column--;
      break;
    }
    
    if(m->executed) {
      RASQAL_DEBUG2("triplesAtch already executed in column %d\n", column);
      column--;
      continue;
    }
      
    if(m->is_exact) {
      /* exact triple match wanted */
      if(!rasqal_triples_source_triple_present(con->triples_source, t)) {
        /* failed */
        RASQAL_DEBUG2("exact match failed for column %d\n", column);
        column--;
      } else {
        RASQAL_DEBUG2("exact match OK for column %d\n", column);
      }

      RASQAL_DEBUG2("end of exact triple match for column %d\n", column);
      m->executed = 1;
      
    } else {
      /* triple pattern match wanted */
      int parts;

      if(!m->triples_match) {
        /* Column has no triple match so create a new query */
        m->triples_match = rasqal_new_triples_match(query,
                                                    con->triples_source,
                                                    m, t);
        if(!m->triples_match) {
          rasqal_log_error_simple(query->world, RAPTOR_LOG_LEVEL_ERROR,
                                  &query->locator,
                                  "Failed to make a triple match for column%d",
                                  column);
          /* failed to match */
          column--;
          goto done;
        }
        RASQAL_DEBUG2("made new triple match for column %d\n", column);
      }


      if(rasqal_triples_match_is_end(m->triples_match)) {
        int resets = 0;

        RASQAL_DEBUG2("end of pattern triple match for column %d\n", column);
        m->executed = 1;

        resets = rasqal_reset_triple_meta(m);
        con->new_bindings_count -= resets;
        if(con->new_bindings_count < 0)
          con->new_bindings_count = 0;

        column--;
        continue;
      }

      if(m->parts) {
        parts = rasqal_triples_match_bind_match(m->triples_match, m->bindings,
                                                m->parts);
        RASQAL_DEBUG3("bind_match for column %d returned parts %d\n",
                      column, parts);
        if(parts & RASQAL_TRIPLE_SUBJECT)
          con->new_bindings_count++;
        if(parts & RASQAL_TRIPLE_PREDICATE)
          con->new_bindings_count++;
        if(parts & RASQAL_TRIPLE_OBJECT)
          con->new_bindings_count++;
        if(parts & RASQAL_TRIPLE_ORIGIN)
          con->new_bindings_count++;
      } else {
        RASQAL_DEBUG2("Nothing to bind_match for column %d\n", column);
      }

      rasqal_triples_match_next_match(m->triples_match);
      if(!con->new_bindings_count)
        continue;

    }
    
    if(column == con->end_column) {
      /* Done all conjunctions */ 
      
      /* exact match, so column must have ended */
      if(m->is_exact)
        column--;

      /* return with result */
      break;
    } else if(column >= con->start_column)
      column++;

  }

  if(column < con->start_column)
    goto done;
  

#ifdef RASQAL_DEBUG
  /* Count actual bound values */
  for(i=0; i < con->size; i++) {
    rasqal_variable* v = rasqal_variables_table_get(query->vars_table, i);
    if(v->value)
      values_returned++;
  }
  RASQAL_DEBUG2("Solution binds %d values\n", values_returned);
#endif

  row = rasqal_new_row(rowsource);
  if(!row)
    goto done;

  for(i=0; i < row->size; i++) {
    rasqal_literal *l;
    l = rasqal_variables_table_get_value(query->vars_table, i);
    if(row->values[i])
      rasqal_free_literal(row->values[i]);
    row->values[i] = rasqal_new_literal_from_literal(l);
  }
  
  row->offset = con->offset++;

  done:
  return row;
}


static raptor_sequence*
rasqal_triples_rowsource_read_all_rows(rasqal_rowsource* rowsource,
                                       void *user_data)
{
  rasqal_triples_rowsource_context *con;
  raptor_sequence *seq = NULL;
  
  con = (rasqal_triples_rowsource_context*)user_data;

  return seq;
}


static rasqal_query*
rasqal_triples_rowsource_get_query(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_triples_rowsource_context *con;
  con = (rasqal_triples_rowsource_context*)user_data;
  return con->query;
}


static const rasqal_rowsource_handler rasqal_triples_rowsource_handler={
  /* .version = */ 1,
  /* .init = */ rasqal_triples_rowsource_init,
  /* .finish = */ rasqal_triples_rowsource_finish,
  /* .ensure_variables = */ rasqal_triples_rowsource_ensure_variables,
  /* .read_row = */ rasqal_triples_rowsource_read_row,
  /* .read_all_rows = */ rasqal_triples_rowsource_read_all_rows,
  /* .get_query = */ rasqal_triples_rowsource_get_query
};


rasqal_rowsource*
rasqal_new_triples_rowsource(rasqal_query *query,
                             rasqal_triples_source* triples_source,
                             raptor_sequence* triples,
                             int start_column, int end_column)
{
  rasqal_triples_rowsource_context *con;
  int flags = 0;
  
  con = (rasqal_triples_rowsource_context*)RASQAL_CALLOC(rasqal_triples_rowsource_context, 1, sizeof(rasqal_triples_rowsource_context));
  if(!con)
    return NULL;

  con->query = query;
  con->triples_source = triples_source;
  con->triples = triples;
  con->start_column = start_column;
  con->end_column = end_column;

  if(query->constructs)
    con->size = rasqal_variables_table_get_named_variables_count(query->vars_table);
  else
    con->size = query->select_variables_count;

  con->triples_count = con->end_column - con->start_column + 1;

  con->triple_meta = (rasqal_triple_meta*)RASQAL_CALLOC(rasqal_triple_meta,
                                                        con->triples_count,
                                                        sizeof(rasqal_triple_meta));
  if(!con->triple_meta) {
    rasqal_triples_rowsource_finish(NULL, con);
    return NULL;
  }

  return rasqal_new_rowsource_from_handler(con,
                                           &rasqal_triples_rowsource_handler,
                                           flags);
}


#endif



#ifdef STANDALONE

/* one more prototype */
int main(int argc, char *argv[]);

#define QUERY_LANGUAGE "sparql"
#define QUERY_FORMAT "\
SELECT ?s ?p ?o \
FROM <%s> \
WHERE { ?s ?p ?o }\
"

int
main(int argc, char *argv[]) 
{
  const char *program = rasqal_basename(argv[0]);
  rasqal_rowsource *rowsource = NULL;
  rasqal_world *world;
  rasqal_query *query;
  const char *query_language_name = QUERY_LANGUAGE;
  const char *query_format = QUERY_FORMAT;
  unsigned char *query_string;
  int failures = 0;
  int start_column;
  int end_column;
  int rc;
  raptor_sequence* triples;
  rasqal_triples_source* triples_source;
  raptor_uri *base_uri = NULL;
  unsigned char *data_string = NULL;
  unsigned char *uri_string = NULL;
  /* <http://example.org#subject> <http://example.org#predicate> "object" . */
#define SUBJECT_URI_STRING (const unsigned char*)"http://example.org#subject"
#define PREDICATE_URI_STRING (const unsigned char*)"http://example.org#predicate"
#define OBJECT_STRING "object"
  raptor_uri* s_uri = NULL;
  raptor_uri* p_uri = NULL;
  
  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    return(1);
  }
  
  if(argc != 2) {
    fprintf(stderr, "USAGE: %s data-filename\n", program);
    return(1);
  }
    
  data_string=raptor_uri_filename_to_uri_string(argv[1]);
  query_string=(unsigned char*)RASQAL_MALLOC(cstring, strlen((const char*)data_string)+strlen(query_format)+1);
  sprintf((char*)query_string, query_format, data_string);
  raptor_free_memory(data_string);
  
  uri_string=raptor_uri_filename_to_uri_string("");
#ifdef RAPTOR_V2_AVAILABLE
  base_uri = raptor_new_uri_v2(world->raptor_world_ptr, uri_string);  
#else
  base_uri = raptor_new_uri(uri_string);  
#endif
  raptor_free_memory(uri_string);

  query = rasqal_new_query(world, query_language_name, NULL);
  if(!query) {
    fprintf(stderr, "%s: creating query in language %s FAILED\n", program,
            query_language_name);
    failures++;
    goto tidy;
  }

  printf("%s: preparing %s query\n", program, query_language_name);
  rc = rasqal_query_prepare(query, query_string, base_uri);
  if(rc) {
    fprintf(stderr, "%s: failed to prepare query '%s'\n", program,
            query_string);
    failures++;
    goto tidy;
  }
  
  RASQAL_FREE(cstring, query_string);
  query_string = NULL;

  triples = rasqal_query_get_triple_sequence(query);
  start_column = 0;
  end_column = 0;

  triples_source = rasqal_new_triples_source(query);
  
  rowsource = rasqal_new_triples_rowsource(query, triples_source,
                                           triples, start_column, end_column);
  if(!rowsource) {
    fprintf(stderr, "%s: failed to create triples rowsource\n", program);
    failures++;
    goto tidy;
  }

  while(1) {
    rasqal_row* row;
    rasqal_literal *s;
    rasqal_literal *p;
    rasqal_literal *o;

    row = rasqal_rowsource_read_row(rowsource);
    if(!row)
      break;
    
  #ifdef RASQAL_DEBUG  
    RASQAL_DEBUG1("Result Row:\n  ");
    rasqal_row_print(row, stderr);
    fputc('\n', stderr);
  #endif

#ifdef RAPTOR_V2_AVAILABLE
    s_uri = raptor_new_uri_v2(world->raptor_world_ptr, SUBJECT_URI_STRING);
    p_uri = raptor_new_uri_v2(world->raptor_world_ptr, PREDICATE_URI_STRING);
#else
    s_uri = raptor_new_uri(SUBJECT_URI_STRING);
    p_uri = raptor_new_uri(PREDICATE_URI_STRING);
#endif
    
    s = row->values[0];
    if(!s ||
       (s && s->type != RASQAL_LITERAL_URI) ||
       !raptor_uri_equals(s->value.uri, s_uri)) {
      fprintf(stderr, "%s: 's' is bound to %s not URI %s\n", program,
              rasqal_literal_as_string(s), raptor_uri_as_string(s_uri));
      failures++;
    }
    p = row->values[1];
    if(!p ||
       (p && p->type != RASQAL_LITERAL_URI) ||
       !raptor_uri_equals(p->value.uri, p_uri)) {
      fprintf(stderr, "%s: 'p' is bound to %s not URI %s\n", program,
              rasqal_literal_as_string(p), raptor_uri_as_string(p_uri));
      failures++;
    }
    o = row->values[2];
    if(!o ||
       (o && o->type != RASQAL_LITERAL_STRING) ||
       strcmp((const char*)o->string, OBJECT_STRING)) {
      fprintf(stderr, "%s: 'o' is bound to %s not string '%s'\n", program,
              rasqal_literal_as_string(o), OBJECT_STRING);
      failures++;
    }

    rasqal_free_row(row);
    if(failures)
      break;
  }

  tidy:
#ifdef RAPTOR_V2_AVAILABLE
  raptor_free_uri_v2(world->raptor_world_ptr, base_uri);
  if(s_uri)
    raptor_free_uri_v2(world->raptor_world_ptr, s_uri);
  if(p_uri)
    raptor_free_uri_v2(world->raptor_world_ptr, p_uri);
#else
  raptor_free_uri(base_uri);
  if(s_uri)
    raptor_free_uri(s_uri);
  if(p_uri)
    raptor_free_uri(p_uri);
#endif

  if(triples_source)
    rasqal_free_triples_source(triples_source);
  if(rowsource)
    rasqal_free_rowsource(rowsource);
  if(query)
    rasqal_free_query(query);

  return failures;
}

#endif

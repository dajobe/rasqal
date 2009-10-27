/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_query_write.c - Rasqal write queries to a syntax
 *
 * Copyright (C) 2004-2009, David Beckett http://www.dajobe.org/
 * Copyright (C) 2004-2005, University of Bristol, UK http://www.bristol.ac.uk/
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


typedef struct 
{
  rasqal_world* world;
  raptor_uri* type_uri;
  raptor_uri* base_uri;
  raptor_namespace_stack *nstack;
} sparql_writer_context;

static void rasqal_query_write_sparql_expression(sparql_writer_context *wc, raptor_iostream* iostr, rasqal_expression* e);


static void
rasqal_query_write_sparql_variable(sparql_writer_context *wc,
                                   raptor_iostream* iostr, rasqal_variable* v)
{
  if(v->expression) {
    rasqal_query_write_sparql_expression(wc, iostr, v->expression);
    raptor_iostream_write_counted_string(iostr, " AS ", 4);
  }
  if(v->type == RASQAL_VARIABLE_TYPE_ANONYMOUS)
    raptor_iostream_write_counted_string(iostr, "_:", 2);
  else if(!v->expression)
    raptor_iostream_write_byte(iostr, '?');
  raptor_iostream_write_string(iostr, v->name);
}


static void
rasqal_query_write_sparql_uri(sparql_writer_context *wc,
                              raptor_iostream* iostr, raptor_uri* uri)
{
  size_t len;
  unsigned char* string;
  raptor_qname* qname;

  qname = raptor_namespaces_qname_from_uri(wc->nstack, uri, 10);
  if(qname) {
    const raptor_namespace* nspace = raptor_qname_get_namespace(qname);
    if(!raptor_namespace_get_prefix(nspace))
      raptor_iostream_write_byte(iostr, ':');
    raptor_iostream_write_qname(iostr, qname);
    raptor_free_qname(qname);
    return;
  }
  
#ifdef RAPTOR_V2_AVAILABLE
  if(wc->base_uri)
    string = raptor_uri_to_relative_counted_uri_string_v2(wc->world->raptor_world_ptr, wc->base_uri, uri, &len);
  else
    string = raptor_uri_as_counted_string_v2(wc->world->raptor_world_ptr, uri, &len);
#else
  if(wc->base_uri)
    string = raptor_uri_to_relative_counted_uri_string(wc->base_uri, uri, &len);
  else
    string = raptor_uri_as_counted_string(uri, &len);
#endif

  raptor_iostream_write_byte(iostr, '<');
  raptor_iostream_write_string_ntriples(iostr, string, len, '>');
  raptor_iostream_write_byte(iostr, '>');

  if(wc->base_uri)
    raptor_free_memory(string);
}


static void
rasqal_query_write_sparql_literal(sparql_writer_context *wc,
                                  raptor_iostream* iostr, rasqal_literal* l)
{
  if(!l) {
    raptor_iostream_write_counted_string(iostr, "null", 4);
    return;
  }

  switch(l->type) {
    case RASQAL_LITERAL_URI:
      rasqal_query_write_sparql_uri(wc, iostr, l->value.uri);
      break;
    case RASQAL_LITERAL_BLANK:
      raptor_iostream_write_counted_string(iostr, "_:", 2);
      raptor_iostream_write_string(iostr, l->string);
      break;
    case RASQAL_LITERAL_STRING:
      raptor_iostream_write_byte(iostr, '"');
      raptor_iostream_write_string_ntriples(iostr, l->string, l->string_len, '"');
      raptor_iostream_write_byte(iostr, '"');
      if(l->language) {
        raptor_iostream_write_byte(iostr, '@');
        raptor_iostream_write_string(iostr, l->language);
      }
      if(l->datatype) {
        raptor_iostream_write_counted_string(iostr, "^^", 2);
        rasqal_query_write_sparql_uri(wc, iostr, l->datatype);
      }
      break;
    case RASQAL_LITERAL_QNAME:
      raptor_iostream_write_counted_string(iostr, "QNAME(", 6);
      raptor_iostream_write_counted_string(iostr, l->string, l->string_len);
      raptor_iostream_write_byte(iostr, ')');
      break;
    case RASQAL_LITERAL_INTEGER:
      raptor_iostream_write_decimal(iostr, l->value.integer);
      break;
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_DECIMAL:
      raptor_iostream_write_counted_string(iostr, l->string, l->string_len);
      break;
    case RASQAL_LITERAL_VARIABLE:
      rasqal_query_write_sparql_variable(wc, iostr, l->value.variable);
      break;
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_UDT:
      raptor_iostream_write_byte(iostr, '"');
      raptor_iostream_write_string_ntriples(iostr, l->string, l->string_len, '"');
      raptor_iostream_write_counted_string(iostr, "\"^^", 3);
      rasqal_query_write_sparql_uri(wc, iostr,
                                    rasqal_xsd_datatype_type_to_uri(l->world, l->type));
      break;

    case RASQAL_LITERAL_UNKNOWN:
    case RASQAL_LITERAL_PATTERN:
    default:
      RASQAL_FATAL2("Literal type %d cannot be written as a SPARQL literal", l->type);
  }
}


static void
rasqal_query_write_sparql_triple(sparql_writer_context *wc,
                                 raptor_iostream* iostr, rasqal_triple* triple)
{
  rasqal_query_write_sparql_literal(wc, iostr, triple->subject);
  raptor_iostream_write_byte(iostr, ' ');
  if(triple->predicate->type == RASQAL_LITERAL_URI &&
#ifdef RAPTOR_V2_AVAILABLE
     raptor_uri_equals_v2(wc->world->raptor_world_ptr, triple->predicate->value.uri, wc->type_uri)
#else
     raptor_uri_equals(triple->predicate->value.uri, wc->type_uri)
#endif
     )
    raptor_iostream_write_byte(iostr, 'a');
  else
    rasqal_query_write_sparql_literal(wc, iostr, triple->predicate);
  raptor_iostream_write_byte(iostr, ' ');
  rasqal_query_write_sparql_literal(wc, iostr, triple->object);
  raptor_iostream_write_counted_string(iostr, " .", 2);
}


#define SPACES_LENGTH 80
static const char spaces[SPACES_LENGTH+1] = "                                                                                ";

static void
rasqal_query_write_indent(raptor_iostream* iostr, int indent) 
{
  while(indent > 0) {
    int sp = (indent > SPACES_LENGTH) ? SPACES_LENGTH : indent;
    raptor_iostream_write_bytes(iostr, spaces, sizeof(char), sp);
    indent -= sp;
  }
}

  

static const char* const rasqal_sparql_op_labels[RASQAL_EXPR_LAST+1] = {
  NULL, /* UNKNOWN */
  "&&",
  "||",
  "=",
  "!=",
  "<",
  ">",
  "<=",
  ">=",
  "-",
  "+",
  "-",
  "*",
  "/",
  NULL, /* REM */
  NULL, /* STR EQ */
  NULL, /* STR NEQ */
  NULL, /* STR_MATCH */
  NULL, /* STR_NMATCH */
  NULL, /* TILDE */
  "!",
  NULL, /* LITERAL */
  NULL, /* FUNCTION */
  "BOUND",
  "STR",
  "LANG",
  "DATATYPE",
  "isIRI",
  "isBLANK",
  "isLITERAL",
  NULL, /* CAST */
  "ASC",   /* ORDER BY ASC */
  "DESC",  /* ORDER BY DESC */
  "LANGMATCHES",
  "REGEX",
  "ASC",   /* GROUP BY ASC */
  "DESC",  /* GROUP BY DESC */
  "COUNT",
  NULL, /* VARSTAR */
  "sameTerm",
  "SUM",
  "AVG",
  "MIN",
  "MAX"
};



static void
rasqal_query_write_sparql_expression_op(sparql_writer_context *wc,
                                        raptor_iostream* iostr,
                                        rasqal_expression* e)
{
  rasqal_op op = e->op;
  const char* string;
  if(op > RASQAL_EXPR_LAST)
    op = RASQAL_EXPR_UNKNOWN;
  string = rasqal_sparql_op_labels[(int)op];
  
  if(string)
    raptor_iostream_write_string(iostr, string);
  else
    raptor_iostream_write_string(iostr, "NONE");
}


static void
rasqal_query_write_sparql_expression(sparql_writer_context *wc,
                                     raptor_iostream* iostr, 
                                     rasqal_expression* e)
{
  int i;
  int count;

  switch(e->op) {
    case RASQAL_EXPR_AND:
    case RASQAL_EXPR_OR:
    case RASQAL_EXPR_EQ:
    case RASQAL_EXPR_NEQ:
    case RASQAL_EXPR_LT:
    case RASQAL_EXPR_GT:
    case RASQAL_EXPR_LE:
    case RASQAL_EXPR_GE:
    case RASQAL_EXPR_PLUS:
    case RASQAL_EXPR_MINUS:
    case RASQAL_EXPR_STAR:
    case RASQAL_EXPR_SLASH:
    case RASQAL_EXPR_REM:
    case RASQAL_EXPR_STR_EQ:
    case RASQAL_EXPR_STR_NEQ:
      raptor_iostream_write_counted_string(iostr, "( ", 2);
      rasqal_query_write_sparql_expression(wc, iostr, e->arg1);
      raptor_iostream_write_byte(iostr, ' ');
      rasqal_query_write_sparql_expression_op(wc, iostr, e);
      raptor_iostream_write_byte(iostr, ' ');
      rasqal_query_write_sparql_expression(wc, iostr, e->arg2);
      raptor_iostream_write_counted_string(iostr, " )", 2);
      break;

    case RASQAL_EXPR_BOUND:
    case RASQAL_EXPR_STR:
    case RASQAL_EXPR_LANG:
    case RASQAL_EXPR_DATATYPE:
    case RASQAL_EXPR_ISURI:
    case RASQAL_EXPR_ISBLANK:
    case RASQAL_EXPR_ISLITERAL:
    case RASQAL_EXPR_ORDER_COND_ASC:
    case RASQAL_EXPR_ORDER_COND_DESC:
    case RASQAL_EXPR_GROUP_COND_ASC:
    case RASQAL_EXPR_GROUP_COND_DESC:
    case RASQAL_EXPR_COUNT:
    case RASQAL_EXPR_SAMETERM:
    case RASQAL_EXPR_SUM:
    case RASQAL_EXPR_AVG:
    case RASQAL_EXPR_MIN:
    case RASQAL_EXPR_MAX:
      rasqal_query_write_sparql_expression_op(wc, iostr, e);
      raptor_iostream_write_counted_string(iostr, "( ", 2);
      rasqal_query_write_sparql_expression(wc, iostr, e->arg1);
      raptor_iostream_write_counted_string(iostr, " )", 2);
      break;
      
    case RASQAL_EXPR_LANGMATCHES:
    case RASQAL_EXPR_REGEX:
      rasqal_query_write_sparql_expression_op(wc, iostr, e);
      raptor_iostream_write_counted_string(iostr, "( ", 2);
      rasqal_query_write_sparql_expression(wc, iostr, e->arg1);
      raptor_iostream_write_counted_string(iostr, ", ", 2);
      rasqal_query_write_sparql_expression(wc, iostr, e->arg2);
      if(e->op == RASQAL_EXPR_REGEX && e->arg3) {
        raptor_iostream_write_counted_string(iostr, ", ", 2);
        rasqal_query_write_sparql_expression(wc, iostr, e->arg3);
      }
      raptor_iostream_write_counted_string(iostr, " )", 2);
      break;

    case RASQAL_EXPR_TILDE:
    case RASQAL_EXPR_BANG:
    case RASQAL_EXPR_UMINUS:
      rasqal_query_write_sparql_expression_op(wc, iostr, e);
      raptor_iostream_write_counted_string(iostr, "( ", 2);
      rasqal_query_write_sparql_expression(wc, iostr, e->arg1);
      raptor_iostream_write_counted_string(iostr, " )", 2);
      break;

    case RASQAL_EXPR_LITERAL:
      rasqal_query_write_sparql_literal(wc, iostr, e->literal);
      break;

    case RASQAL_EXPR_FUNCTION:
#ifdef RAPTOR_V2_AVAILABLE
      raptor_iostream_write_uri_v2(e->world->raptor_world_ptr, iostr, e->name);
#else
      raptor_iostream_write_uri(iostr, e->name);
#endif
      raptor_iostream_write_counted_string(iostr, "( ", 2);
      count = raptor_sequence_size(e->args);
      for(i = 0; i < count ; i++) {
        rasqal_expression* arg;
        arg = (rasqal_expression*)raptor_sequence_get_at(e->args, i);
        if(i > 0)
          raptor_iostream_write_counted_string(iostr, " ,", 2);
        rasqal_query_write_sparql_expression(wc, iostr, arg);
      }
      raptor_iostream_write_counted_string(iostr, " )", 2);
      break;

    case RASQAL_EXPR_CAST:
#ifdef RAPTOR_V2_AVAILABLE
      raptor_iostream_write_uri_v2(e->world->raptor_world_ptr, iostr, e->name);
#else
      raptor_iostream_write_uri(iostr, e->name);
#endif
      raptor_iostream_write_counted_string(iostr, "( ", 2);
      rasqal_query_write_sparql_expression(wc, iostr, e->arg1);
      raptor_iostream_write_counted_string(iostr, " )", 2);
      break;

    case RASQAL_EXPR_VARSTAR:
      raptor_iostream_write_byte(iostr, '*');
      break;
      
    case RASQAL_EXPR_UNKNOWN:
    case RASQAL_EXPR_STR_MATCH:
    case RASQAL_EXPR_STR_NMATCH:
    default:
      RASQAL_FATAL2("Expression op %d cannot be written as a SPARQL expresson", e->op);
  }
}


static void
rasqal_query_write_sparql_graph_pattern(sparql_writer_context *wc,
                                        raptor_iostream* iostr,
                                        rasqal_graph_pattern *gp, 
                                        int gp_index, int indent)
{
  int triple_index = 0;
  rasqal_graph_pattern_operator op;
  raptor_sequence *seq;
  int filters_count = 0;
  
  op = rasqal_graph_pattern_get_operator(gp);
  
  if(op == RASQAL_GRAPH_PATTERN_OPERATOR_LET) {
    /* LAQRS */
    raptor_iostream_write_counted_string(iostr, "LET (", 5);
    rasqal_query_write_sparql_variable(wc, iostr, gp->var);
    raptor_iostream_write_counted_string(iostr, " := ", 4);
    rasqal_query_write_sparql_expression(wc, iostr, gp->filter_expression);
    raptor_iostream_write_counted_string(iostr, ") .", 3);
    return;
  }
  
  if(op == RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL ||
     op == RASQAL_GRAPH_PATTERN_OPERATOR_GRAPH) {
    /* prefix verbs */
    if(op == RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL) 
      raptor_iostream_write_counted_string(iostr, "OPTIONAL ", 9);
    else {
      rasqal_graph_pattern* sgp;
      rasqal_triple* t;
      sgp = rasqal_graph_pattern_get_sub_graph_pattern(gp, 0);
      t = rasqal_graph_pattern_get_triple(sgp, 0);

      raptor_iostream_write_counted_string(iostr, "GRAPH ", 6);
      rasqal_query_write_sparql_literal(wc, iostr, t->origin);
      raptor_iostream_write_byte(iostr, ' ');
    }
  }
  raptor_iostream_write_counted_string(iostr, "{\n", 2);

  indent+= 2;

  /* look for triples */
  while(1) {
    rasqal_triple* t = rasqal_graph_pattern_get_triple(gp, triple_index);
    if(!t)
      break;
    
    rasqal_query_write_indent(iostr, indent);
    rasqal_query_write_sparql_triple(wc, iostr, t);
    raptor_iostream_write_byte(iostr, '\n');

    triple_index++;
  }


  /* look for sub-graph patterns */
  seq = rasqal_graph_pattern_get_sub_graph_pattern_sequence(gp);
  if(seq && raptor_sequence_size(seq) > 0) {
    for(gp_index = 0; 1; gp_index++) {
      rasqal_graph_pattern* sgp = rasqal_graph_pattern_get_sub_graph_pattern(gp, gp_index);
      if(!sgp)
        break;

      if(sgp->op == RASQAL_GRAPH_PATTERN_OPERATOR_FILTER) {
        filters_count++;
        continue;
      }
      
      if(!gp_index)
        rasqal_query_write_indent(iostr, indent);
      else {
        if(op == RASQAL_GRAPH_PATTERN_OPERATOR_UNION)
          /* infix verb */
          raptor_iostream_write_counted_string(iostr, " UNION ", 7);
        else {
          /* must be prefix verb */
          raptor_iostream_write_byte(iostr, '\n');
          rasqal_query_write_indent(iostr, indent);
        }
      }
      
      rasqal_query_write_sparql_graph_pattern(wc, iostr, sgp, gp_index, indent);
    }
    raptor_iostream_write_byte(iostr, '\n');
  }
  

  /* look for constraints */
  if(filters_count > 0) {
    for(gp_index = 0; 1; gp_index++) {
      rasqal_graph_pattern* sgp;
      rasqal_expression* expr;

      sgp = rasqal_graph_pattern_get_sub_graph_pattern(gp, gp_index);
      if(!sgp)
        break;
      
      if(sgp->op != RASQAL_GRAPH_PATTERN_OPERATOR_FILTER)
        continue;

      expr = rasqal_graph_pattern_get_filter_expression(sgp);

      rasqal_query_write_indent(iostr, indent);
      raptor_iostream_write_counted_string(iostr, "FILTER( ", 8);
      rasqal_query_write_sparql_expression(wc, iostr, expr);
      raptor_iostream_write_counted_string(iostr, " )\n", 3);
    }
  }
  

  indent -= 2;
  
  rasqal_query_write_indent(iostr, indent);
  raptor_iostream_write_byte(iostr, '}');
}


    
int
rasqal_query_write_sparql_20060406(raptor_iostream *iostr, 
                                   rasqal_query* query, raptor_uri *base_uri)
{
  int i;
  sparql_writer_context wc;
#ifndef RAPTOR_V2_AVAILABLE
  const raptor_uri_handler *uri_handler;
  void *uri_context;
#endif

  wc.world = query->world;
  wc.base_uri = NULL;

#ifdef RAPTOR_V2_AVAILABLE
  wc.type_uri = raptor_new_uri_for_rdf_concept_v2(query->world->raptor_world_ptr, "type");
  wc.nstack = raptor_new_namespaces_v2(query->world->raptor_world_ptr,
                                       (raptor_simple_message_handler)rasqal_query_simple_error,
                                       query,
                                       1);
#else
  wc.type_uri = raptor_new_uri_for_rdf_concept("type");
  raptor_uri_get_handler(&uri_handler, &uri_context);
  wc.nstack = raptor_new_namespaces(uri_handler, uri_context,
                                    (raptor_simple_message_handler)rasqal_query_simple_error,
                                    query,
                                    1);
#endif

  if(base_uri) {
    raptor_iostream_write_counted_string(iostr, "BASE ", 5);
    rasqal_query_write_sparql_uri(&wc, iostr, base_uri);
    raptor_iostream_write_byte(iostr, '\n');

    /* from now on all URIs are relative to this */
#ifdef RAPTOR_V2_AVAILABLE
    wc.base_uri = raptor_uri_copy_v2(query->world->raptor_world_ptr, base_uri);
#else
    wc.base_uri = raptor_uri_copy(base_uri);
#endif
  }
  
  
  for(i = 0; 1 ; i++) {
    raptor_namespace *nspace;
    rasqal_prefix* p = rasqal_query_get_prefix(query, i);
    if(!p)
      break;
    
    raptor_iostream_write_counted_string(iostr, "PREFIX ", 7);
    if(p->prefix)
      raptor_iostream_write_string(iostr, p->prefix);
    raptor_iostream_write_counted_string(iostr,": ", 2);
    rasqal_query_write_sparql_uri(&wc, iostr, p->uri);
    raptor_iostream_write_byte(iostr, '\n');

    /* Use this constructor so we copy a URI directly */
    nspace = raptor_new_namespace_from_uri(wc.nstack, p->prefix, p->uri, i);
    raptor_namespaces_start_namespace(wc.nstack, nspace);
  }

  if(query->explain)
    raptor_iostream_write_counted_string(iostr, "EXPLAIN ", 8);

  if(query->verb != RASQAL_QUERY_VERB_CONSTRUCT)
    raptor_iostream_write_string(iostr,
                                 rasqal_query_verb_as_string(query->verb));

  if(query->distinct) {
    if(query->distinct == 1)
      raptor_iostream_write_counted_string(iostr, " DISTINCT", 9);
    else
      raptor_iostream_write_counted_string(iostr, " REDUCED", 8);
  }

  if(query->wildcard)
    raptor_iostream_write_counted_string(iostr, " *", 2);
  else if(query->verb == RASQAL_QUERY_VERB_DESCRIBE) {
    raptor_sequence *lit_seq = query->describes;
    int count = raptor_sequence_size(lit_seq);

    for(i = 0; i < count; i++) {
      rasqal_literal* l = (rasqal_literal*)raptor_sequence_get_at(lit_seq, i);
      raptor_iostream_write_byte(iostr, ' ');
      rasqal_query_write_sparql_literal(&wc, iostr, l);
    }
  } else if(query->verb == RASQAL_QUERY_VERB_SELECT) {
    raptor_sequence *var_seq = query->selects;
    int count = raptor_sequence_size(var_seq);

    for(i = 0; i < count; i++) {
      rasqal_variable* v = (rasqal_variable*)raptor_sequence_get_at(var_seq, i);
      raptor_iostream_write_byte(iostr, ' ');
      rasqal_query_write_sparql_variable(&wc, iostr, v);
    }
  }
  raptor_iostream_write_byte(iostr, '\n');

  if(query->data_graphs) {
    for(i = 0; 1; i++) {
      rasqal_data_graph* dg = rasqal_query_get_data_graph(query, i);
      if(!dg)
        break;
      
      if(dg->flags & RASQAL_DATA_GRAPH_NAMED)
        continue;
      
      raptor_iostream_write_counted_string(iostr, "FROM ", 5);
      rasqal_query_write_sparql_uri(&wc, iostr, dg->uri);
      raptor_iostream_write_counted_string(iostr, "\n", 1);
    }
    
    for(i = 0; 1; i++) {
      rasqal_data_graph* dg = rasqal_query_get_data_graph(query, i);
      if(!dg)
        break;

      if(!(dg->flags & RASQAL_DATA_GRAPH_NAMED))
        continue;
      
      raptor_iostream_write_counted_string(iostr, "FROM NAMED ", 11);
      rasqal_query_write_sparql_uri(&wc, iostr, dg->name_uri);
      raptor_iostream_write_byte(iostr, '\n');
    }
    
  }

  if(query->constructs) {
    raptor_iostream_write_string(iostr, "CONSTRUCT {\n");
    for(i = 0; 1; i++) {
      rasqal_triple* t = rasqal_query_get_construct_triple(query, i);
      if(!t)
        break;

      raptor_iostream_write_counted_string(iostr, "  ", 2);
      rasqal_query_write_sparql_triple(&wc, iostr, t);
      raptor_iostream_write_byte(iostr, '\n');
    }
    raptor_iostream_write_counted_string(iostr, "}\n", 2);
  }
  if(query->query_graph_pattern) {
    raptor_iostream_write_counted_string(iostr, "WHERE ", 6);
    rasqal_query_write_sparql_graph_pattern(&wc, iostr,
                                            query->query_graph_pattern, 
                                            -1, 0);
    raptor_iostream_write_byte(iostr, '\n');
  }

  if(query->group_conditions_sequence) {
    raptor_iostream_write_counted_string(iostr, "GROUP BY ", 9);
    for(i = 0; 1; i++) {
      rasqal_expression* expr = rasqal_query_get_group_condition(query, i);
      if(!expr)
        break;

      if(i > 0)
        raptor_iostream_write_byte(iostr, ' ');
      rasqal_query_write_sparql_expression(&wc, iostr, expr);
    }
    raptor_iostream_write_byte(iostr, '\n');
  }

  if(query->order_conditions_sequence) {
    raptor_iostream_write_counted_string(iostr, "ORDER BY ", 9);
    for(i = 0; 1; i++) {
      rasqal_expression* expr = rasqal_query_get_order_condition(query, i);
      if(!expr)
        break;

      if(i > 0)
        raptor_iostream_write_byte(iostr, ' ');
      rasqal_query_write_sparql_expression(&wc, iostr, expr);
    }
    raptor_iostream_write_byte(iostr, '\n');
  }

  if(query->limit >= 0 || query->offset >= 0) {
    if(query->limit >= 0) {
      raptor_iostream_write_counted_string(iostr, "LIMIT ", 7);
      raptor_iostream_write_decimal(iostr, query->limit);
    }
    if(query->offset >= 0) {
      if(query->limit)
        raptor_iostream_write_byte(iostr, ' ');
      raptor_iostream_write_counted_string(iostr, "OFFSET ", 8);
      raptor_iostream_write_decimal(iostr, query->offset);
    }
    raptor_iostream_write_byte(iostr, '\n');
  }

#ifdef RAPTOR_V2_AVAILABLE
  raptor_free_uri_v2(query->world->raptor_world_ptr, wc.type_uri);
  if(wc.base_uri)
    raptor_free_uri_v2(query->world->raptor_world_ptr, wc.base_uri);
#else
  raptor_free_uri(wc.type_uri);
  if(wc.base_uri)
    raptor_free_uri(wc.base_uri);
#endif
  raptor_free_namespaces(wc.nstack);

  return 0;
}

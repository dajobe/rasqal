/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_query_write.c - Write query data structure as a syntax
 *
 * Copyright (C) 2004-2010, David Beckett http://www.dajobe.org/
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
    raptor_iostream_counted_string_write("( ", 2, iostr);
    rasqal_query_write_sparql_expression(wc, iostr, v->expression);
    raptor_iostream_counted_string_write(" AS ", 4, iostr);
  }
  if(v->type == RASQAL_VARIABLE_TYPE_ANONYMOUS)
    raptor_iostream_counted_string_write("_:", 2, iostr);
  else if(!v->expression)
    raptor_iostream_write_byte('?', iostr);
  raptor_iostream_string_write(v->name, iostr);
  if(v->expression)
    raptor_iostream_counted_string_write(" )", 2, iostr);
}


static void
rasqal_query_write_sparql_uri(sparql_writer_context *wc,
                              raptor_iostream* iostr, raptor_uri* uri)
{
  size_t len;
  unsigned char* string;
  raptor_qname* qname;

  qname = raptor_new_qname_from_namespace_uri(wc->nstack, uri, 10);
  if(qname) {
    const raptor_namespace* nspace = raptor_qname_get_namespace(qname);
    if(!raptor_namespace_get_prefix(nspace))
      raptor_iostream_write_byte(':', iostr);
    raptor_qname_write(qname, iostr);
    raptor_free_qname(qname);
    return;
  }
  
  if(wc->base_uri)
    string = raptor_uri_to_relative_counted_uri_string(wc->base_uri, uri, &len);
  else
    string = raptor_uri_as_counted_string(uri, &len);

  raptor_iostream_write_byte('<', iostr);
  raptor_string_ntriples_write(string, len, '>', iostr);
  raptor_iostream_write_byte('>', iostr);

  if(wc->base_uri)
    raptor_free_memory(string);
}


static void
rasqal_query_write_sparql_literal(sparql_writer_context *wc,
                                  raptor_iostream* iostr, rasqal_literal* l)
{
  if(!l) {
    raptor_iostream_counted_string_write("null", 4, iostr);
    return;
  }

  switch(l->type) {
    case RASQAL_LITERAL_URI:
      rasqal_query_write_sparql_uri(wc, iostr, l->value.uri);
      break;

    case RASQAL_LITERAL_BLANK:
      raptor_iostream_counted_string_write("_:", 2, iostr);
      raptor_iostream_string_write(l->string, iostr);
      break;

    case RASQAL_LITERAL_STRING:
      raptor_iostream_write_byte('"', iostr);
      raptor_string_ntriples_write(l->string, l->string_len, '"', iostr);
      raptor_iostream_write_byte('"', iostr);
      if(l->language) {
        raptor_iostream_write_byte('@', iostr);
        raptor_iostream_string_write(l->language, iostr);
      }
      if(l->datatype) {
        raptor_iostream_counted_string_write("^^", 2, iostr);
        rasqal_query_write_sparql_uri(wc, iostr, l->datatype);
      }
      break;

    case RASQAL_LITERAL_QNAME:
      raptor_iostream_counted_string_write("QNAME(", 6, iostr);
      raptor_iostream_counted_string_write(l->string, l->string_len, iostr);
      raptor_iostream_write_byte(')', iostr);
      break;

    case RASQAL_LITERAL_INTEGER:
      raptor_iostream_decimal_write(l->value.integer, iostr);
      break;

    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_DECIMAL:
      raptor_iostream_counted_string_write(l->string, l->string_len, iostr);
      break;

    case RASQAL_LITERAL_VARIABLE:
      rasqal_query_write_sparql_variable(wc, iostr, l->value.variable);
      break;

    case RASQAL_LITERAL_DATE:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_UDT:
    case RASQAL_LITERAL_INTEGER_SUBTYPE:
      if(1) {
        raptor_uri* dt_uri;
        
        raptor_iostream_write_byte('"', iostr);
        raptor_string_ntriples_write(l->string, l->string_len, '"', iostr);
        raptor_iostream_counted_string_write("\"^^", 3, iostr);
        if(l->type <= RASQAL_LITERAL_LAST_XSD)
          dt_uri = rasqal_xsd_datatype_type_to_uri(l->world, l->type);
        else
          dt_uri = l->datatype;
        rasqal_query_write_sparql_uri(wc, iostr, dt_uri);
      }
      break;

    case RASQAL_LITERAL_UNKNOWN:
    case RASQAL_LITERAL_PATTERN:
    default:
      RASQAL_FATAL2("Literal type %u cannot be written as a SPARQL literal", l->type);
  }
}


static void
rasqal_query_write_sparql_triple(sparql_writer_context *wc,
                                 raptor_iostream* iostr, rasqal_triple* triple)
{
  rasqal_query_write_sparql_literal(wc, iostr, triple->subject);
  raptor_iostream_write_byte(' ', iostr);
  if(triple->predicate->type == RASQAL_LITERAL_URI &&
     raptor_uri_equals(triple->predicate->value.uri, wc->type_uri))
    raptor_iostream_write_byte('a', iostr);
  else
    rasqal_query_write_sparql_literal(wc, iostr, triple->predicate);
  raptor_iostream_write_byte(' ', iostr);
  rasqal_query_write_sparql_literal(wc, iostr, triple->object);
  raptor_iostream_counted_string_write(" .", 2, iostr);
}


#define SPACES_LENGTH 80
static const char spaces[SPACES_LENGTH+1] = "                                                                                ";

static void
rasqal_query_write_indent(raptor_iostream* iostr, unsigned int indent) 
{
  while(indent > 0) {
    unsigned int sp = (indent > SPACES_LENGTH) ? SPACES_LENGTH : indent;
    raptor_iostream_write_bytes(spaces, sizeof(char), RASQAL_GOOD_CAST(size_t, sp), iostr);
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
  "MAX",
  "COALESCE",
  "IF",
  "URI",
  "IRI",
  "STRLANG",
  "STRDT",
  "BNODE",
  "GROUP_CONCAT",
  "SAMPLE",
  "IN",
  "NOT IN",
  "isNUMERIC",
  "YEAR",
  "MONTH",
  "DAY",
  "HOURS",
  "MINUTES",
  "SECONDS",
  "TIMEZONE",
  "CURRENT_DATETIME",
  "NOW",
  "FROM_UNIXTIME",
  "TO_UNIXTIME",
  "CONCAT",
  "STRLEN",
  "SUBSTR",
  "UCASE",
  "LCASE",
  "STRSTARTS",
  "STRENDS",
  "CONTAINS",
  "ENCODE_FOR_URI",
  "TZ",
  "RAND",
  "ABS",
  "ROUND",
  "CEIL",
  "FLOOR",
  "MD5",
  "SHA1",
  "SHA224",
  "SHA256",
  "SHA384",
  "SHA512",
  "STRBEFORE",
  "STRAFTER",
  "REPLACE",
  "UUID",
  "STRUUID"
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
  string = rasqal_sparql_op_labels[RASQAL_GOOD_CAST(int, op)];
  
  if(string)
    raptor_iostream_string_write(string, iostr);
  else
    raptor_iostream_string_write("NONE", iostr);
}


static void
rasqal_query_write_sparql_expression(sparql_writer_context *wc,
                                     raptor_iostream* iostr, 
                                     rasqal_expression* e)
{
  int i;
  int size;

  switch(e->op) {
    case RASQAL_EXPR_CURRENT_DATETIME:
    case RASQAL_EXPR_NOW:
    case RASQAL_EXPR_RAND:
      rasqal_query_write_sparql_expression_op(wc, iostr, e);
      raptor_iostream_counted_string_write("()", 2, iostr);
      break;

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
    case RASQAL_EXPR_STRLANG:
    case RASQAL_EXPR_STRDT:
    case RASQAL_EXPR_STRSTARTS:
    case RASQAL_EXPR_STRENDS:
    case RASQAL_EXPR_CONTAINS:
    case RASQAL_EXPR_STRBEFORE:
    case RASQAL_EXPR_STRAFTER:
      raptor_iostream_counted_string_write("( ", 2, iostr);
      rasqal_query_write_sparql_expression(wc, iostr, e->arg1);
      raptor_iostream_write_byte(' ', iostr);
      rasqal_query_write_sparql_expression_op(wc, iostr, e);
      raptor_iostream_write_byte(' ', iostr);
      rasqal_query_write_sparql_expression(wc, iostr, e->arg2);
      raptor_iostream_counted_string_write(" )", 2, iostr);
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
    case RASQAL_EXPR_URI:
    case RASQAL_EXPR_IRI:
    case RASQAL_EXPR_BNODE:
    case RASQAL_EXPR_SAMPLE:
    case RASQAL_EXPR_ISNUMERIC:
    case RASQAL_EXPR_YEAR:
    case RASQAL_EXPR_MONTH:
    case RASQAL_EXPR_DAY:
    case RASQAL_EXPR_HOURS:
    case RASQAL_EXPR_MINUTES:
    case RASQAL_EXPR_SECONDS:
    case RASQAL_EXPR_TIMEZONE:
    case RASQAL_EXPR_FROM_UNIXTIME:
    case RASQAL_EXPR_TO_UNIXTIME:
    case RASQAL_EXPR_STRLEN:
    case RASQAL_EXPR_UCASE:
    case RASQAL_EXPR_LCASE:
    case RASQAL_EXPR_ENCODE_FOR_URI:
    case RASQAL_EXPR_TZ:
    case RASQAL_EXPR_ABS:
    case RASQAL_EXPR_ROUND:
    case RASQAL_EXPR_CEIL:
    case RASQAL_EXPR_FLOOR:
    case RASQAL_EXPR_MD5:
    case RASQAL_EXPR_SHA1:
    case RASQAL_EXPR_SHA224:
    case RASQAL_EXPR_SHA256:
    case RASQAL_EXPR_SHA384:
    case RASQAL_EXPR_SHA512:
    case RASQAL_EXPR_UUID:
    case RASQAL_EXPR_STRUUID:
      rasqal_query_write_sparql_expression_op(wc, iostr, e);
      raptor_iostream_counted_string_write("( ", 2, iostr);
      rasqal_query_write_sparql_expression(wc, iostr, e->arg1);
      raptor_iostream_counted_string_write(" )", 2, iostr);
      break;
      
    case RASQAL_EXPR_LANGMATCHES:
    case RASQAL_EXPR_REGEX:
    case RASQAL_EXPR_IF:
    case RASQAL_EXPR_SUBSTR:
    case RASQAL_EXPR_REPLACE:
      rasqal_query_write_sparql_expression_op(wc, iostr, e);
      raptor_iostream_counted_string_write("( ", 2, iostr);
      rasqal_query_write_sparql_expression(wc, iostr, e->arg1);
      raptor_iostream_counted_string_write(", ", 2, iostr);
      rasqal_query_write_sparql_expression(wc, iostr, e->arg2);
      if((e->op == RASQAL_EXPR_REGEX || e->op == RASQAL_EXPR_IF ||
          e->op == RASQAL_EXPR_SUBSTR) && e->arg3) {
        raptor_iostream_counted_string_write(", ", 2, iostr);
        rasqal_query_write_sparql_expression(wc, iostr, e->arg3);
      }
      raptor_iostream_counted_string_write(" )", 2, iostr);
      break;

    case RASQAL_EXPR_TILDE:
    case RASQAL_EXPR_BANG:
    case RASQAL_EXPR_UMINUS:
      rasqal_query_write_sparql_expression_op(wc, iostr, e);
      raptor_iostream_counted_string_write("( ", 2, iostr);
      rasqal_query_write_sparql_expression(wc, iostr, e->arg1);
      raptor_iostream_counted_string_write(" )", 2, iostr);
      break;

    case RASQAL_EXPR_LITERAL:
      rasqal_query_write_sparql_literal(wc, iostr, e->literal);
      break;

    case RASQAL_EXPR_FUNCTION:
      raptor_uri_write(e->name, iostr);
      raptor_iostream_counted_string_write("( ", 2, iostr);
      if(e->flags & RASQAL_EXPR_FLAG_DISTINCT)
        raptor_iostream_counted_string_write(" DISTINCT ", 10, iostr);
      size = raptor_sequence_size(e->args);
      for(i = 0; i < size ; i++) {
        rasqal_expression* arg;
        arg = (rasqal_expression*)raptor_sequence_get_at(e->args, i);
        if(i > 0)
          raptor_iostream_counted_string_write(", ", 2, iostr);
        rasqal_query_write_sparql_expression(wc, iostr, arg);
      }
      raptor_iostream_counted_string_write(" )", 2, iostr);
      break;

    case RASQAL_EXPR_CAST:
      raptor_uri_write(e->name, iostr);
      raptor_iostream_counted_string_write("( ", 2, iostr);
      rasqal_query_write_sparql_expression(wc, iostr, e->arg1);
      raptor_iostream_counted_string_write(" )", 2, iostr);
      break;

    case RASQAL_EXPR_VARSTAR:
      raptor_iostream_write_byte('*', iostr);
      break;
      
    case RASQAL_EXPR_COALESCE:
    case RASQAL_EXPR_CONCAT:
      rasqal_query_write_sparql_expression_op(wc, iostr, e);
      raptor_iostream_counted_string_write("( ", 2, iostr);
      size = raptor_sequence_size(e->args);
      for(i = 0; i < size ; i++) {
        rasqal_expression* e2;
        e2 = (rasqal_expression*)raptor_sequence_get_at(e->args, i);
        if(i > 0)
          raptor_iostream_counted_string_write(", ", 2, iostr);
        rasqal_query_write_sparql_expression(wc, iostr, e2);
      }
      raptor_iostream_counted_string_write(" )", 2, iostr);
      break;

    case RASQAL_EXPR_GROUP_CONCAT:
      raptor_iostream_counted_string_write("GROUP_CONCAT( ", 14, iostr);
      if(e->flags & RASQAL_EXPR_FLAG_DISTINCT)
        raptor_iostream_counted_string_write("DISTINCT ", 9, iostr);

      size = raptor_sequence_size(e->args);
      for(i = 0; i < size ; i++) {
        rasqal_expression* arg;
        arg = (rasqal_expression*)raptor_sequence_get_at(e->args, i);
        if(i > 0)
          raptor_iostream_counted_string_write(", ", 2, iostr);
        rasqal_query_write_sparql_expression(wc, iostr, arg);
      }

      if(e->literal) {
        raptor_iostream_counted_string_write(" ; SEPARATOR = ", 15, iostr);
        rasqal_query_write_sparql_literal(wc, iostr, e->literal);
      }
      
      raptor_iostream_counted_string_write(" )", 2, iostr);
      break;

    case RASQAL_EXPR_IN:
    case RASQAL_EXPR_NOT_IN:
      rasqal_query_write_sparql_expression(wc, iostr, e->arg1);
      raptor_iostream_write_byte(' ', iostr);
      rasqal_query_write_sparql_expression_op(wc, iostr, e);
      raptor_iostream_counted_string_write(" (", 2, iostr);
      size = raptor_sequence_size(e->args);
      for(i = 0; i < size ; i++) {
        rasqal_expression* e2;
        e2 = (rasqal_expression*)raptor_sequence_get_at(e->args, i);
        if(i > 0)
          raptor_iostream_counted_string_write(", ", 2, iostr);
        rasqal_query_write_sparql_expression(wc, iostr, e2);
      }
      raptor_iostream_counted_string_write(" )", 2, iostr);
      break;

    case RASQAL_EXPR_UNKNOWN:
    case RASQAL_EXPR_STR_MATCH:
    case RASQAL_EXPR_STR_NMATCH:
    default:
      RASQAL_FATAL2("Expression op %u cannot be written as a SPARQL expresson", e->op);
  }
}


static void
rasqal_query_write_sparql_triple_data(sparql_writer_context *wc,
                                      raptor_iostream* iostr,
                                      raptor_sequence *triples,
                                      unsigned int indent)
{
  int triple_index = 0;
  
  raptor_iostream_counted_string_write("{\n", 2, iostr);
  indent += 2;
  
  /* look for triples */
  while(1) {
    rasqal_triple* t = (rasqal_triple*)raptor_sequence_get_at(triples, triple_index);
    if(!t)
      break;
    
    rasqal_query_write_indent(iostr, indent);
    if(t->origin) {
      raptor_iostream_counted_string_write("GRAPH ", 6, iostr);
      rasqal_query_write_sparql_literal(wc, iostr, t->origin);
      raptor_iostream_counted_string_write(" { ", 3, iostr);
    }
    
    rasqal_query_write_sparql_triple(wc, iostr, t);

    if(t->origin)
      raptor_iostream_counted_string_write(" }", 2, iostr);

    raptor_iostream_write_byte('\n', iostr);
    
    triple_index++;
  }
  
  indent -= 2;
  
  rasqal_query_write_indent(iostr, indent);
  raptor_iostream_write_byte('}', iostr);
  
}


static int
rasqal_query_write_sparql_variables_sequence(sparql_writer_context *wc,
                                             raptor_iostream *iostr, 
                                             raptor_sequence* seq)
{
  int size = raptor_sequence_size(seq);
  int i;
  
  if(!seq)
    return 0;

  for(i = 0; i < size; i++) {
    rasqal_variable* v = (rasqal_variable*)raptor_sequence_get_at(seq, i);
    if(i > 0)
      raptor_iostream_write_byte(' ', iostr);
    rasqal_query_write_sparql_variable(wc, iostr, v);
  }

  return 0;
}


static int
rasqal_query_write_sparql_expression_sequence(sparql_writer_context *wc,
                                              raptor_iostream* iostr,
                                              raptor_sequence* seq)
{
  int size = raptor_sequence_size(seq);
  int i;

  if(!seq)
    return 0;

  for(i = 0; i < size; i++) {
    rasqal_expression* e = (rasqal_expression*)raptor_sequence_get_at(seq, i);
    if(i > 0)
      raptor_iostream_write_byte(' ', iostr);
    rasqal_query_write_sparql_expression(wc, iostr, e);
  }

  return 0;
}

static int
rasqal_query_write_sparql_modifiers(sparql_writer_context *wc,
                                    raptor_iostream* iostr,
                                    rasqal_solution_modifier* modifier)
{
  raptor_sequence* seq;
  int limit, offset;

  seq = modifier->group_conditions;
  if(seq && raptor_sequence_size(seq) > 0) {
    raptor_iostream_counted_string_write("GROUP BY ", 9, iostr);
    rasqal_query_write_sparql_expression_sequence(wc, iostr, seq);
    raptor_iostream_write_byte('\n', iostr);
  }

  seq = modifier->having_conditions;
  if(seq && raptor_sequence_size(seq) > 0) {
    raptor_iostream_counted_string_write("HAVING ", 7, iostr);
    rasqal_query_write_sparql_expression_sequence(wc, iostr, seq);
    raptor_iostream_write_byte('\n', iostr);
  }

  seq = modifier->order_conditions;
  if(seq && raptor_sequence_size(seq) > 0) {
    raptor_iostream_counted_string_write("ORDER BY ", 9, iostr);
    rasqal_query_write_sparql_expression_sequence(wc, iostr, seq);
    raptor_iostream_write_byte('\n', iostr);
  }

  limit = modifier->limit;
  offset = modifier->offset;
  if(limit >= 0 || offset >= 0) {
    if(limit >= 0) {
      raptor_iostream_counted_string_write("LIMIT ", 6, iostr);
      raptor_iostream_decimal_write(RASQAL_GOOD_CAST(int, limit), iostr);
    }
    if(offset >= 0) {
      if(limit)
        raptor_iostream_write_byte(' ', iostr);
      raptor_iostream_counted_string_write("OFFSET ", 7, iostr);
      raptor_iostream_decimal_write(RASQAL_GOOD_CAST(int, offset), iostr);
    }
    raptor_iostream_write_byte('\n', iostr);
  }

  return 0;
}


static int
rasqal_query_write_sparql_row(sparql_writer_context* wc,
                              raptor_iostream* iostr,
                              rasqal_row* row,
                              int write_braces)
{
  int i;

  if(write_braces)
    raptor_iostream_counted_string_write("( ", 2, iostr);
  for(i = 0; i < row->size; i++) {
    rasqal_literal* value = row->values[i];
    if(i > 0)
      raptor_iostream_write_byte(' ', iostr);

    if(value)
      rasqal_query_write_sparql_literal(wc, iostr, value);
    else
      raptor_iostream_counted_string_write("UNDEF", 5, iostr);
  }
  if(write_braces)
    raptor_iostream_counted_string_write(" )", 2, iostr);

  return 0;
}


static int
rasqal_query_write_sparql_values(sparql_writer_context* wc,
                                 raptor_iostream* iostr,
                                 rasqal_bindings* bindings,
                                 unsigned int indent)
{
  int vars_size = -1;
  int rows_size = -1;

  if(!bindings)
    return 0;

  if(bindings->variables)
    vars_size = raptor_sequence_size(bindings->variables);

  raptor_iostream_counted_string_write("VALUES ", 7, iostr);

  if(vars_size > 1)
    raptor_iostream_counted_string_write("( ", 2, iostr);
  rasqal_query_write_sparql_variables_sequence(wc, iostr, bindings->variables);
  raptor_iostream_write_byte(' ', iostr);
  if(vars_size > 1)
    raptor_iostream_counted_string_write(") ", 2, iostr);
  raptor_iostream_counted_string_write("{ ", 2, iostr);

  if(bindings->rows)
    rows_size = raptor_sequence_size(bindings->rows);

  if(rows_size > 0) {
    int i;
    
    if(vars_size > 1)
      raptor_iostream_write_byte('\n', iostr);
    
    indent += 2;
    for(i = 0; i < rows_size; i++) {
      rasqal_row* row;
      row = (rasqal_row*)raptor_sequence_get_at(bindings->rows, i);
      if(vars_size > 1) {
        rasqal_query_write_indent(iostr, indent);
        rasqal_query_write_sparql_row(wc, iostr, row, 1);
        raptor_iostream_write_byte('\n', iostr);
      } else {
        rasqal_query_write_sparql_row(wc, iostr, row, 0);
      }
    }
    indent -= 2;
  }

  if(vars_size > 1)
    rasqal_query_write_indent(iostr, indent);
  else
    raptor_iostream_write_byte(' ', iostr);
  raptor_iostream_counted_string_write("}\n", 2, iostr);

  return 0;
}


static void
rasqal_query_write_sparql_graph_pattern(sparql_writer_context *wc,
                                        raptor_iostream* iostr,
                                        rasqal_graph_pattern *gp, 
                                        int gp_index, unsigned int indent)
{
  int triple_index = 0;
  rasqal_graph_pattern_operator op;
  raptor_sequence *seq;
  int filters_count = 0;
  int want_braces = 1;
  int size = -1;

  op = rasqal_graph_pattern_get_operator(gp);

  if(op == RASQAL_GRAPH_PATTERN_OPERATOR_SELECT) {
    raptor_sequence* vars_seq;
    rasqal_graph_pattern* where_gp;
    
    raptor_iostream_counted_string_write("SELECT ", 7, iostr);
    vars_seq = rasqal_projection_get_variables_sequence(gp->projection);
    rasqal_query_write_sparql_variables_sequence(wc, iostr, vars_seq);
    raptor_iostream_write_byte('\n', iostr);
    rasqal_query_write_indent(iostr, indent);
    raptor_iostream_counted_string_write("WHERE ", 6, iostr);
    where_gp = rasqal_graph_pattern_get_sub_graph_pattern(gp, 0);
    rasqal_query_write_sparql_graph_pattern(wc, iostr, where_gp, 0, indent);

    rasqal_query_write_sparql_modifiers(wc, iostr, gp->modifier);
    if(gp->bindings) {
      rasqal_query_write_indent(iostr, indent);
      rasqal_query_write_sparql_values(wc, iostr, gp->bindings, indent);
    }
    return;
  }

  if(op == RASQAL_GRAPH_PATTERN_OPERATOR_LET) {
    /* LAQRS */
    raptor_iostream_counted_string_write("LET (", 5, iostr);
    rasqal_query_write_sparql_variable(wc, iostr, gp->var);
    raptor_iostream_counted_string_write(" := ", 4, iostr);
    rasqal_query_write_sparql_expression(wc, iostr, gp->filter_expression);
    raptor_iostream_counted_string_write(") .", 3, iostr);
    return;
  }
  
  if(op == RASQAL_GRAPH_PATTERN_OPERATOR_SERVICE) {
    rasqal_graph_pattern* service_gp;

    /* LAQRS */
    raptor_iostream_counted_string_write("SERVICE ", 8, iostr);
    if(gp->silent)
      raptor_iostream_counted_string_write("SILENT ", 7, iostr);
    rasqal_query_write_sparql_literal(wc, iostr, gp->origin);
    raptor_iostream_counted_string_write(" ", 1, iostr);
    service_gp = rasqal_graph_pattern_get_sub_graph_pattern(gp, 0);
    rasqal_query_write_sparql_graph_pattern(wc, iostr, service_gp, 0, indent);
    return;
  }
  
  if(op == RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL ||
     op == RASQAL_GRAPH_PATTERN_OPERATOR_GRAPH) {
    /* prefix verbs */
    if(op == RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL) 
      raptor_iostream_counted_string_write("OPTIONAL ", 9, iostr);
    else {
      raptor_iostream_counted_string_write("GRAPH ", 6, iostr);
      rasqal_query_write_sparql_literal(wc, iostr, gp->origin);
      raptor_iostream_write_byte(' ', iostr);
    }
  }

  if(op == RASQAL_GRAPH_PATTERN_OPERATOR_FILTER)
    want_braces = 0;

  if(op == RASQAL_GRAPH_PATTERN_OPERATOR_VALUES) {
    rasqal_query_write_sparql_values(wc, iostr, gp->bindings, indent);
    want_braces = 0;
  }

  if(want_braces) {
    raptor_iostream_counted_string_write("{\n", 2, iostr);
    indent += 2;
  }

  /* look for triples */
  while(1) {
    rasqal_triple* t = rasqal_graph_pattern_get_triple(gp, triple_index);
    if(!t)
      break;
    
    rasqal_query_write_indent(iostr, indent);
    rasqal_query_write_sparql_triple(wc, iostr, t);
    raptor_iostream_write_byte('\n', iostr);

    triple_index++;
  }


  /* look for sub-graph patterns */
  seq = rasqal_graph_pattern_get_sub_graph_pattern_sequence(gp);
  if(seq)
    size = raptor_sequence_size(seq);
  if(size > 0) {
    for(gp_index = 0; gp_index < size; gp_index++) {
      rasqal_graph_pattern* sgp;

      sgp = rasqal_graph_pattern_get_sub_graph_pattern(gp, gp_index);
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
          raptor_iostream_counted_string_write(" UNION ", 7, iostr);
        else {
          /* must be prefix verb */
          raptor_iostream_write_byte('\n', iostr);
          rasqal_query_write_indent(iostr, indent);
        }
      }
      
      rasqal_query_write_sparql_graph_pattern(wc, iostr, sgp, gp_index, indent);
    }
    if(gp_index < size)
      raptor_iostream_write_byte('\n', iostr);
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
      raptor_iostream_counted_string_write("FILTER( ", 8, iostr);
      rasqal_query_write_sparql_expression(wc, iostr, expr);
      raptor_iostream_counted_string_write(" )\n", 3, iostr);
    }
  }
  

  if(want_braces) {
    indent -= 2;

    rasqal_query_write_indent(iostr, indent);
    raptor_iostream_counted_string_write("}\n", 2, iostr);
  }

}


    
static void
rasqal_query_write_data_format_comment(sparql_writer_context* wc,
                                       raptor_iostream *iostr,
                                       rasqal_data_graph* dg) 
{
  if(dg->format_type || dg->format_name || dg->format_uri) {
    raptor_iostream_counted_string_write("# format ", 9, iostr);
    if(dg->format_type) {
      raptor_iostream_counted_string_write("type ", 5, iostr);
      raptor_iostream_string_write(dg->format_type, iostr);
    }
    if(dg->format_name) {
      raptor_iostream_counted_string_write("name ", 5, iostr);
      raptor_iostream_string_write(dg->format_name, iostr);
    }
    if(dg->format_type) {
      raptor_iostream_counted_string_write("uri ", 4, iostr);
      rasqal_query_write_sparql_uri(wc, iostr, dg->format_uri);
    }
  }
}
  


static int
rasqal_query_write_graphref(sparql_writer_context* wc,
                            raptor_iostream *iostr, 
                            raptor_uri* uri,
                            rasqal_update_graph_applies applies)
{
  switch(applies) {
    case RASQAL_UPDATE_GRAPH_ONE:
      if(uri) {
        raptor_iostream_counted_string_write(" GRAPH ", 7, iostr);
        rasqal_query_write_sparql_uri(wc, iostr, uri);
        break;
      }
      /* FALLTHROUGH */
      
    case RASQAL_UPDATE_GRAPH_DEFAULT:
      raptor_iostream_counted_string_write(" DEFAULT", 8, iostr);
      break;
      
    case RASQAL_UPDATE_GRAPH_NAMED:
      raptor_iostream_counted_string_write(" NAMED", 6, iostr);
      break;
      
    case RASQAL_UPDATE_GRAPH_ALL:
      raptor_iostream_counted_string_write(" ALL", 4, iostr);
      break;
      
    /* default: case not necessary */
  }

  return 0;
}


static int
rasqal_query_write_sparql_projection(sparql_writer_context *wc,
                                     raptor_iostream *iostr, 
                                     rasqal_projection* projection)
{
  if(!projection)
    return 1;

  if(projection->distinct) {
    if(projection->distinct == 1)
      raptor_iostream_counted_string_write(" DISTINCT", 9, iostr);
    else
      raptor_iostream_counted_string_write(" REDUCED", 8, iostr);
  }

  if(projection->wildcard) {
    raptor_iostream_counted_string_write(" *", 2, iostr);
    return 0;
  }

  raptor_iostream_write_byte(' ', iostr);
  return rasqal_query_write_sparql_variables_sequence(wc, iostr,
                                                      projection->variables);
}


int
rasqal_query_write_sparql_20060406_graph_pattern(rasqal_graph_pattern* gp,
                                                 raptor_iostream *iostr,
                                                 raptor_uri* base_uri)
{
  rasqal_world* world = gp->query->world;
  sparql_writer_context wc;

  memset(&wc, '\0', sizeof(wc));
  wc.world = world;
  wc.base_uri = NULL;
  wc.type_uri = raptor_new_uri_for_rdf_concept(world->raptor_world_ptr,
                                               RASQAL_GOOD_CAST(const unsigned char*, "type"));
  wc.nstack = raptor_new_namespaces(world->raptor_world_ptr, 1);

  if(base_uri)
    /* from now on all URIs are relative to this */
    wc.base_uri = raptor_uri_copy(base_uri);

  raptor_iostream_counted_string_write("SELECT *\nWHERE ", 15, iostr);
  rasqal_query_write_sparql_graph_pattern(&wc, iostr, gp,
                                          /* gp_index */ -1,
                                          /* indent */ 0);

  raptor_free_uri(wc.type_uri);
  if(wc.base_uri)
    raptor_free_uri(wc.base_uri);
  raptor_free_namespaces(wc.nstack);

  return 0;
}


int
rasqal_query_write_sparql_20060406(raptor_iostream *iostr,
                                   rasqal_query* query, raptor_uri *base_uri)
{
  int i;
  sparql_writer_context wc;
  rasqal_query_verb verb;
  rasqal_projection* projection;
  
  memset(&wc, '\0', sizeof(wc));
  wc.world = query->world;
  wc.base_uri = NULL;

  wc.type_uri = raptor_new_uri_for_rdf_concept(query->world->raptor_world_ptr,
                                               RASQAL_GOOD_CAST(const unsigned char*, "type"));
  wc.nstack = raptor_new_namespaces(query->world->raptor_world_ptr, 1);

  if(base_uri) {
    raptor_iostream_counted_string_write("BASE ", 5, iostr);
    rasqal_query_write_sparql_uri(&wc, iostr, base_uri);
    raptor_iostream_write_byte('\n', iostr);

    /* from now on all URIs are relative to this */
    wc.base_uri = raptor_uri_copy(base_uri);
  }
  
  
  for(i = 0; 1 ; i++) {
    raptor_namespace *nspace;
    rasqal_prefix* p = rasqal_query_get_prefix(query, i);
    if(!p)
      break;
    
    raptor_iostream_counted_string_write("PREFIX ", 7, iostr);
    if(p->prefix)
      raptor_iostream_string_write(p->prefix, iostr);
    raptor_iostream_counted_string_write(": ", 2, iostr);
    rasqal_query_write_sparql_uri(&wc, iostr, p->uri);
    raptor_iostream_write_byte('\n', iostr);

    /* Use this constructor so we copy a URI directly */
    nspace = raptor_new_namespace_from_uri(wc.nstack, p->prefix, p->uri, i);
    raptor_namespaces_start_namespace(wc.nstack, nspace);
  }

  if(query->explain)
    raptor_iostream_counted_string_write("EXPLAIN ", 8, iostr);

  verb = query->verb;
  
  /* These terms are deprecated */
  if(query->verb == RASQAL_QUERY_VERB_INSERT ||
     query->verb == RASQAL_QUERY_VERB_DELETE) {
    verb = RASQAL_QUERY_VERB_UPDATE;
  }

  
  /* Write SPARQL 1.1 (Draft) Update forms */
  if(verb == RASQAL_QUERY_VERB_UPDATE) {
    rasqal_update_operation* update;
    /* Write SPARQL Update */

    for(i = 0; (update = rasqal_query_get_update_operation(query, i)); i++) {
      int is_always_2_args = (update->type >= RASQAL_UPDATE_TYPE_ADD &&
                              update->type <= RASQAL_UPDATE_TYPE_COPY);
      
      if(update->type == RASQAL_UPDATE_TYPE_UPDATE) {
        /* update operations:
         * WITH ... INSERT { template } DELETE { template } WHERE { template }
         * INSERT/DELETE { template } WHERE { template }
         * INSERT/DELETE DATA { triples } 
         */
        if(update->graph_uri) {
          raptor_iostream_counted_string_write("WITH ", 5, iostr);
          rasqal_query_write_sparql_uri(&wc, iostr, update->graph_uri);
          raptor_iostream_write_byte('\n', iostr);
        }
        if(update->delete_templates) {
          raptor_iostream_counted_string_write("DELETE ", 7, iostr);
          if(update->flags & RASQAL_UPDATE_FLAGS_DATA) 
            raptor_iostream_counted_string_write("DATA ", 5, iostr);
          rasqal_query_write_sparql_triple_data(&wc, iostr,
                                                update->delete_templates,
                                                0);
          raptor_iostream_write_byte('\n', iostr);
        }
        if(update->insert_templates) {
          raptor_iostream_counted_string_write("INSERT ", 7, iostr);
          if(update->flags & RASQAL_UPDATE_FLAGS_DATA) 
            raptor_iostream_counted_string_write("DATA ", 5, iostr);
          rasqal_query_write_sparql_triple_data(&wc, iostr,
                                                update->insert_templates,
                                                0);
          raptor_iostream_write_byte('\n', iostr);
        }
        if(update->where) {
          raptor_iostream_counted_string_write("WHERE ", 6, iostr);
          rasqal_query_write_sparql_graph_pattern(&wc, iostr,
                                                  update->where,
                                                  -1, 0);
          raptor_iostream_write_byte('\n', iostr);
        }
      } else {
        /* admin operations:
         * CLEAR GRAPH graph-uri | DEFAULT | NAMED | ALL
         * CREATE (SILENT) GRAPH graph-uri | DEFAULT | NAMED | ALL
         * DROP (SILENT) GRAPH graph-uri
         * LOAD (SILENT) doc-uri / LOAD (SILENT) doc-uri INTO GRAPH graph-uri
         * ADD (SILENT) GraphOrDefault TO GraphOrDefault
         * MOVE (SILENT) GraphOrDefault TO GraphOrDefault
         * COPY (SILENT) GraphOrDefault TO GraphOrDefault
         */
        raptor_iostream_string_write(rasqal_update_type_label(update->type),
                                     iostr);
        if(update->flags & RASQAL_UPDATE_FLAGS_SILENT)
          raptor_iostream_counted_string_write(" SILENT", 7, iostr);

        if(is_always_2_args) {
          /* ADD, MOVE, COPY are always 2-arg admin operations */
          rasqal_query_write_graphref(&wc, iostr, 
                                      update->graph_uri,
                                      RASQAL_UPDATE_GRAPH_ONE);

          raptor_iostream_counted_string_write(" TO", 3, iostr);
        
          rasqal_query_write_graphref(&wc, iostr, 
                                      update->document_uri,
                                      RASQAL_UPDATE_GRAPH_ONE);

        } else if(update->type == RASQAL_UPDATE_TYPE_LOAD) {
          /* LOAD is 1 or 2 URIs and first one never has a GRAPH prefix */

          raptor_iostream_write_byte(' ', iostr);

          rasqal_query_write_sparql_uri(&wc, iostr, update->document_uri);

          if(update->graph_uri) {
            raptor_iostream_counted_string_write(" INTO", 5, iostr);
            
            rasqal_query_write_graphref(&wc, iostr, 
                                        update->graph_uri,
                                        RASQAL_UPDATE_GRAPH_ONE);
          }
        } else {
          /* everything else is defined by update->applies; only
          * CLEAR and DROP may apply to >1 graph
          */
          rasqal_query_write_graphref(&wc, iostr, 
                                      update->graph_uri,
                                      update->applies);
        }
        
        raptor_iostream_write_byte('\n', iostr);

      }
    }

    goto tidy;
  }


  if(verb != RASQAL_QUERY_VERB_CONSTRUCT)
    raptor_iostream_string_write(rasqal_query_verb_as_string(query->verb),
                                 iostr);

  projection = rasqal_query_get_projection(query);
  if(projection)
    rasqal_query_write_sparql_projection(&wc, iostr, projection);
  
  if(verb == RASQAL_QUERY_VERB_DESCRIBE) {
    raptor_sequence *lit_seq = query->describes;
    int size = raptor_sequence_size(lit_seq);

    for(i = 0; i < size; i++) {
      rasqal_literal* l = (rasqal_literal*)raptor_sequence_get_at(lit_seq, i);
      raptor_iostream_write_byte(' ', iostr);
      rasqal_query_write_sparql_literal(&wc, iostr, l);
    }
  }

  raptor_iostream_write_byte('\n', iostr);

  if(query->data_graphs) {
    for(i = 0; 1; i++) {
      rasqal_data_graph* dg = rasqal_query_get_data_graph(query, i);
      if(!dg)
        break;
      
      if(dg->flags & RASQAL_DATA_GRAPH_NAMED)
        continue;
      
      rasqal_query_write_data_format_comment(&wc, iostr, dg);
      raptor_iostream_counted_string_write("FROM ", 5, iostr);
      rasqal_query_write_sparql_uri(&wc, iostr, dg->uri);
      raptor_iostream_counted_string_write("\n", 1, iostr);
    }
    
    for(i = 0; 1; i++) {
      rasqal_data_graph* dg = rasqal_query_get_data_graph(query, i);
      if(!dg)
        break;

      if(!(dg->flags & RASQAL_DATA_GRAPH_NAMED))
        continue;
      
      rasqal_query_write_data_format_comment(&wc, iostr, dg);
      raptor_iostream_counted_string_write("FROM NAMED ", 11, iostr);
      rasqal_query_write_sparql_uri(&wc, iostr, dg->name_uri);
      raptor_iostream_write_byte('\n', iostr);
    }
    
  }

  if(query->constructs) {
    raptor_iostream_string_write("CONSTRUCT {\n", iostr);
    for(i = 0; 1; i++) {
      rasqal_triple* t = rasqal_query_get_construct_triple(query, i);
      if(!t)
        break;

      raptor_iostream_counted_string_write("  ", 2, iostr);
      rasqal_query_write_sparql_triple(&wc, iostr, t);
      raptor_iostream_write_byte('\n', iostr);
    }
    raptor_iostream_counted_string_write("}\n", 2, iostr);
  }
  if(query->query_graph_pattern) {
    unsigned int indent = 2;
    raptor_iostream_counted_string_write("WHERE {\n", 8, iostr);
    rasqal_query_write_indent(iostr, indent);
    rasqal_query_write_sparql_graph_pattern(&wc, iostr,
                                            query->query_graph_pattern, 
                                            -1, indent);
    raptor_iostream_counted_string_write("}\n", 2, iostr);
  }

  rasqal_query_write_sparql_modifiers(&wc, iostr, query->modifier);
  rasqal_query_write_sparql_values(&wc, iostr, query->bindings, 0);

  tidy:
  raptor_free_uri(wc.type_uri);
  if(wc.base_uri)
    raptor_free_uri(wc.base_uri);
  raptor_free_namespaces(wc.nstack);

  return 0;
}

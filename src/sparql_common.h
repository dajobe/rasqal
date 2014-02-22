/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * sparql_common.h - SPARQL lexer/parser shared internals
 *
 * Copyright (C) 2004-2008, David Beckett http://www.dajobe.org/
 * Copyright (C) 2004-2004, University of Bristol, UK http://www.bristol.ac.uk/
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

#ifndef SPARQL_COMMON_H
#define SPARQL_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif


/* sparql_parser.y */
int sparql_syntax_error(rasqal_query *rq, const char *message, ...) RASQAL_PRINTF_FORMAT(2, 3);
int sparql_syntax_warning(rasqal_query *rq, const char *message, ...) RASQAL_PRINTF_FORMAT(2, 3);

int sparql_query_lex(void);


struct rasqal_sparql_query_language_s {
  /* STATIC lexer */
  yyscan_t scanner;

  int scanner_set;

  /* for error reporting */
  unsigned int lineno;

  /* SPARQL query language language features supported */

  /* querying with SELECT, CONSTRUCT, DESCRIBE, ASK */
  int sparql_scda : 1;

  /* SPARQL 1.1 query language features (e.g. SELECT (expr AS var) ) */
  int sparql11_query : 1;

  /* SPARQL 1.1 property path querying */
  int sparql11_property_paths : 1;

  /* updating in general (LOAD, CLEAR, DROP, ADD, MOVE, COPY, CREATE,
   * INSERT, DELETE, MODIFY ) 
   */
  int sparql11_update : 1;

  /* laqrs */
  int experimental : 1;

  /* count of errors in current query parse */
  int error_count;

  /* for creating long literals */
  raptor_stringbuffer* sb;
};


typedef struct rasqal_sparql_query_language_s rasqal_sparql_query_language;


#ifdef __cplusplus
}
#endif

#endif

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rdql_common.h - RDQL lexer/parser shared internals
 *
 * $Id$
 *
 * Copyright (C) 2003 David Beckett - http://purl.org/net/dajobe/
 * Institute for Learning and Research Technology - http://www.ilrt.org/
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
 */

#ifndef RDQL_COMMON_H
#define RDQL_COMMON_H

#ifdef __cplusplus
extern "C" {
#endif


/* rdql_parser.y */
int rdql_syntax_error(rasqal_query *rq, const char *message, ...);
int rdql_syntax_warning(rasqal_query *rq, const char *message, ...);

int rdql_query_lex(void);


struct rasqal_rdql_query_engine_s {
  /* STATIC lexer */
  yyscan_t scanner;

  int scanner_set;

  /* for error reporting */
  unsigned int lineno;
};


typedef struct rasqal_rdql_query_engine_s rasqal_rdql_query_engine;


#ifdef __cplusplus
}
#endif

#endif

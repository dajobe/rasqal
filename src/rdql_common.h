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


struct rdql_parser_s {
  rasqal_query *query;
  
  /* for lexer to store result in */
  YYSTYPE lval;

  /* STATIC lexer */
  yyscan_t scanner;

  const char *uri_string;

  /* for error reporting */
  unsigned int line;
  unsigned int column;

  unsigned int warnings;
  unsigned int errors;
};


#ifdef __cplusplus
}
#endif

#endif

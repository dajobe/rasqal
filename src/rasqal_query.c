/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_query.c - Rasqal RDF Query
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
 * 
 */

#ifdef HAVE_CONFIG_H
#include <rasqal_config.h>
#endif

#ifdef WIN32
#include <win32_config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rasqal.h>
#include <rasqal_internal.h>


struct rasqal_query_s {
  int foo;
};


/* Constructor */
rasqal_query*
rasqal_new_query(const char *name, const unsigned char *uri) {
  rasqal_query* q=(rasqal_query*)malloc(sizeof(rasqal_query));

  return q;
}

/* Destructor */
void
rasqal_free_query(rasqal_query* query) {
}


/* Methods */
void
rasqal_query_add_source(rasqal_query* query, const unsigned char* uri) {
}

rasqal_sequence*
rasqal_query_get_source_sequence(rasqal_query* query) {
  return NULL;
}

const unsigned char *
rasqal_query_get_source(rasqal_query* query, int idx) {
  return NULL;
}


/* Utility methods */
void
rasqal_query_print(rasqal_query* query, FILE *stream) {
}


/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rdql.h - Rasqal RDF Query library interfaces and definition
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



#ifndef RDQL_H
#define RDQL_H


#ifdef __cplusplus
extern "C" {
#endif

#ifdef WIN32
#  ifdef RASQAL_INTERNAL
#    define RASQAL_API _declspec(dllexport)
#  else
#    define RASQAL_API _declspec(dllimport)
#  endif
#else
#  define RASQAL_API
#endif

/* Public structure */
typedef struct rasqal_query_s rasqal_query;

typedef struct rasqal_sequence_s rasqal_sequence;


/* RASQAL API */

/* Public functions */

RASQAL_API void rasqal_init(void);
RASQAL_API void rasqal_finish(void);


/* Query class */

/* Create */
RASQAL_API rasqal_query* rasqal_new_query(const char *name, const unsigned char *uri);
/* Destroy */
RASQAL_API void rasqal_free_query(rasqal_query* query);

/* Methods */
RASQAL_API void rasqal_query_add_source(rasqal_query* query, const unsigned char* uri);
RASQAL_API rasqal_sequence* rasqal_query_get_source_sequence(rasqal_query* query);
RASQAL_API const unsigned char* rasqal_query_get_source(rasqal_query* query, int idx);

/* Utility methods */
RASQAL_API void rasqal_query_print(rasqal_query*, FILE *stream);


/* Sequence class */

/* Create */
RASQAL_API rasqal_sequence* rasqal_new_sequence(int capacity);
/* Destroy */
RASQAL_API void rasqal_free_sequence(rasqal_sequence* seq);
/* Methods */
RASQAL_API int rasqal_sequence_size(rasqal_sequence* seq);
RASQAL_API int rasqal_sequence_set_at(rasqal_sequence* seq, int idx, void *data);
RASQAL_API int rasqal_sequence_push(rasqal_sequence* seq, void *data);
RASQAL_API int rasqal_sequence_shift(rasqal_sequence* seq, void *data);
RASQAL_API void* rasqal_sequence_get_at(rasqal_sequence* seq, int idx);
RASQAL_API void* rasqal_sequence_pop(rasqal_sequence* seq);
RASQAL_API void* rasqal_sequence_unshift(rasqal_sequence* seq);


#ifdef __cplusplus
}
#endif

#endif

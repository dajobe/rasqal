/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_triple.c - Rasqal triple class
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
#include <ctype.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdarg.h>

#include "rasqal.h"
#include "rasqal_internal.h"


/**
 * rasqal_new_triple:
 * @subject: Triple subject.
 * @predicate: Triple predicate.
 * @object: Triple object.
 * 
 * Constructor - create a new #rasqal_triple triple or triple pattern.
 * Takes ownership of the literals passed in.
 * 
 * The triple origin can be set with rasqal_triple_set_origin().
 *
 * Return value: a new #rasqal_triple or NULL on failure.
 **/
rasqal_triple*
rasqal_new_triple(rasqal_literal* subject, rasqal_literal* predicate,
                  rasqal_literal* object)
{
  rasqal_triple* t;

  t = RASQAL_CALLOC(rasqal_triple*, 1, sizeof(*t));
  if(t) {
    t->subject = subject;
    t->predicate = predicate;
    t->object = object;
  } else {
    if(subject)
      rasqal_free_literal(subject);
    if(predicate)
      rasqal_free_literal(predicate);
    if(object)
      rasqal_free_literal(object);
  }

  return t;
}


/**
 * rasqal_new_triple_from_triple:
 * @t: Triple to copy.
 * 
 * Copy constructor - create a new #rasqal_triple from an existing one.
 * 
 * Return value: a new #rasqal_triple or NULL on failure.
 **/
rasqal_triple*
rasqal_new_triple_from_triple(rasqal_triple* t)
{
  rasqal_triple* newt;

  newt = RASQAL_CALLOC(rasqal_triple*, 1, sizeof(*newt));
  if(newt) {
    newt->subject = rasqal_new_literal_from_literal(t->subject);
    newt->predicate = rasqal_new_literal_from_literal(t->predicate);
    newt->object = rasqal_new_literal_from_literal(t->object);
  }

  return newt;
}


/**
 * rasqal_free_triple:
 * @t: #rasqal_triple object.
 * 
 * Destructor - destroy a #rasqal_triple object.
 **/
void
rasqal_free_triple(rasqal_triple* t)
{
  if(!t)
    return;
  
  if(t->subject)
    rasqal_free_literal(t->subject);
  if(t->predicate)
    rasqal_free_literal(t->predicate);
  if(t->object)
    rasqal_free_literal(t->object);
  if(t->origin)
    rasqal_free_literal(t->origin);
  RASQAL_FREE(rasqal_triple, t);
}


/**
 * rasqal_triple_write:
 * @t: #rasqal_triple object.
 * @iostr: The #raptor_iostream handle to write to.
 * 
 * Write a Rasqal triple to an iostream in a debug format.
 * 
 * The print debug format may change in any release.
 **/
void
rasqal_triple_write(rasqal_triple* t, raptor_iostream* iostr)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(t, rasqal_triple);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(iostr, raptor_iostream);
  
  raptor_iostream_counted_string_write("triple(", 7, iostr);
  rasqal_literal_write(t->subject, iostr);
  raptor_iostream_counted_string_write(", ", 2, iostr);
  rasqal_literal_write(t->predicate, iostr);
  raptor_iostream_counted_string_write(", ", 2, iostr);
  rasqal_literal_write(t->object, iostr);
  raptor_iostream_write_byte(')', iostr);
  if(t->origin) {
    raptor_iostream_counted_string_write(" with origin(", 13, iostr);
    rasqal_literal_write(t->origin, iostr);
    raptor_iostream_write_byte(')', iostr);
  }
}


/**
 * rasqal_triple_print:
 * @t: #rasqal_triple object.
 * @fh: The FILE* handle to print to.
 * 
 * Print a Rasqal triple in a debug format.
 * 
 * The print debug format may change in any release.
 *
 * Return value: non-0 on failure
 **/
int
rasqal_triple_print(rasqal_triple* t, FILE* fh)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(t, rasqal_triple, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(fh, FILE*, 1);
  
  fputs("triple(", fh);
  rasqal_literal_print(t->subject, fh);
  fputs(", ", fh);
  rasqal_literal_print(t->predicate, fh);
  fputs(", ", fh);
  rasqal_literal_print(t->object, fh);
  fputc(')', fh);
  if(t->origin) {
    fputs(" with origin(", fh);
    rasqal_literal_print(t->origin, fh);
    fputc(')', fh);
  }

  return 0;
}


/**
 * rasqal_triple_set_origin:
 * @t: The triple object. 
 * @l: The #rasqal_literal object to set as origin.
 * 
 * Set the origin field of a #rasqal_triple.
 **/
void
rasqal_triple_set_origin(rasqal_triple* t, rasqal_literal* l)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(t, rasqal_triple);

  t->origin = l;
}


/**
 * rasqal_triple_get_origin:
 * @t: The triple object. 
 * 
 * Get the origin field of a #rasqal_triple.
 * 
 * Return value: The triple origin or NULL.
 **/
rasqal_literal*
rasqal_triple_get_origin(rasqal_triple* t)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(t, rasqal_triple, NULL);

  return t->origin;
}


int    
rasqal_triples_sequence_set_origin(raptor_sequence* dest_seq, 
                                   raptor_sequence* src_seq,
                                   rasqal_literal* origin) 
{
  int i;
  int size;
  
  if(!src_seq)
    return 1;
  
  size = raptor_sequence_size(src_seq);
  for(i = 0; i < size; i++) {
    rasqal_triple *t;
    t = (rasqal_triple*)raptor_sequence_get_at(src_seq, i);

    if(dest_seq) {
      /* altern copied triple; this is a deep copy */
      t = rasqal_new_triple_from_triple(t);
      if(!t)
        return 1;
      
      if(t->origin)
        rasqal_free_literal(t->origin);
      t->origin = rasqal_new_literal_from_literal(origin);
      raptor_sequence_push(dest_seq, t);
    } else {
      /* alter sequence in-place */
      if(t->origin)
        rasqal_free_literal(t->origin);

      t->origin = rasqal_new_literal_from_literal(origin);
    }

  }

  return 0;
}

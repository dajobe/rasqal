/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_iostream.c - Rasqal I/O stream utility code
 *
 * Copyright (C) 2011, David Beckett http://www.dajobe.org/
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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdarg.h>

#include "rasqal.h"
#include "rasqal_internal.h"



struct rasqal_read_stringbuffer_iostream_context {
  /* stringbuffer owned by this object */
  raptor_stringbuffer* sb;

  /* input buffer pointer into sb */
  void* string;
  size_t length;

  /* pointer into buffer */
  size_t offset;
};


/* Local handlers for reading from a string */

static void
rasqal_read_stringbuffer_iostream_finish(void *user_data)
{
  struct rasqal_read_stringbuffer_iostream_context* con;

  con = (struct rasqal_read_stringbuffer_iostream_context*)user_data;
  if(con->sb)
    raptor_free_stringbuffer(con->sb);

  RASQAL_FREE(rasqal_read_stringbuffer_iostream_context, con);
  return;
}

static int
rasqal_read_stringbuffer_iostream_read_bytes(void *user_data, void *ptr,
                                             size_t size, size_t nmemb)
{
  struct rasqal_read_stringbuffer_iostream_context* con;
  size_t avail;
  size_t blen;

  if(!ptr || size <= 0 || !nmemb)
    return -1;

  con = (struct rasqal_read_stringbuffer_iostream_context*)user_data;
  if(con->offset >= con->length)
    return 0;

  avail = RASQAL_BAD_CAST(int, ((con->length - con->offset) / size));
  if(avail > nmemb)
    avail = nmemb;

  blen = (avail * size);
  memcpy(ptr, RASQAL_GOOD_CAST(char*, con->string) + con->offset, blen);
  con->offset += blen;

  return RASQAL_BAD_CAST(int, avail);
}

static int
rasqal_read_stringbuffer_iostream_read_eof(void *user_data)
{
  struct rasqal_read_stringbuffer_iostream_context* con;

  con = (struct rasqal_read_stringbuffer_iostream_context*)user_data;
  return (con->offset >= con->length);
}


static const raptor_iostream_handler rasqal_iostream_read_stringbuffer_handler = {
  /* .version     = */ 2,
  /* .init        = */ NULL,
  /* .finish      = */ rasqal_read_stringbuffer_iostream_finish,
  /* .write_byte  = */ NULL,
  /* .write_bytes = */ NULL,
  /* .write_end   = */ NULL,
  /* .read_bytes  = */ rasqal_read_stringbuffer_iostream_read_bytes,
  /* .read_eof    = */ rasqal_read_stringbuffer_iostream_read_eof
};


/*
 * rasqal_new_iostream_from_stringbuffer:
 * @world: raptor world
 * @sb: stringbuffer
 *
 * INTERNAL - create a new iostream reading from a stringbuffer.
 *
 * The stringbuffer @sb becomes owned by the iostream
 *
 * This is intended to be replaced by
 * raptor_new_iostream_from_stringbuffer() in a newer raptor release.
 *
 * Return value: new #raptor_iostream object or NULL on failure
 **/
raptor_iostream*
rasqal_new_iostream_from_stringbuffer(raptor_world *raptor_world_ptr,
                                      raptor_stringbuffer* sb)
{
  struct rasqal_read_stringbuffer_iostream_context* con;
  const raptor_iostream_handler* handler;

  if(!sb)
    return NULL;

  handler = &rasqal_iostream_read_stringbuffer_handler;

  con = RASQAL_CALLOC(struct rasqal_read_stringbuffer_iostream_context*, 1,
                      sizeof(*con));
  if(!con) {
    raptor_free_stringbuffer(sb);
    return NULL;
  }

  con->sb = sb;

  con->string = raptor_stringbuffer_as_string(sb);
  con->length = raptor_stringbuffer_length(sb);

  return raptor_new_iostream_from_handler(raptor_world_ptr, con, handler);
}

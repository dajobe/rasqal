/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_prefix.c - Rasqal prefix class
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
 * rasqal_new_prefix:
 * @world: rasqal_world object
 * @prefix: Short prefix string to stand for URI (or NULL)
 * @uri: Name #raptor_uri.
 * 
 * Constructor - create a new #rasqal_prefix.
 * Takes ownership of prefix and uri.
 * 
 * Return value: a new #rasqal_prefix or NULL on failure.
 **/
rasqal_prefix*
rasqal_new_prefix(rasqal_world* world, const unsigned char *prefix,
                  raptor_uri* uri) 
{
  rasqal_prefix* p;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(uri, raptor_uri, NULL);

  p = RASQAL_CALLOC(rasqal_prefix*, 1, sizeof(*p));
  if(p) {  
    p->world = world;
    p->prefix = prefix;
    p->uri = uri;
  } else {
    RASQAL_FREE(char*, prefix);
    raptor_free_uri(uri);
  }

  return p;
}


/**
 * rasqal_free_prefix:
 * @p: #rasqal_prefix object.
 * 
 * Destructor - destroy a #rasqal_prefix object.
 **/
void
rasqal_free_prefix(rasqal_prefix* p)
{
  if(!p)
    return;
  
  if(p->prefix)
    RASQAL_FREE(char*, p->prefix);
  if(p->uri)
    raptor_free_uri(p->uri);
  RASQAL_FREE(rasqal_prefix, p);
}


/**
 * rasqal_prefix_print:
 * @p: #rasqal_prefix object.
 * @fh: The FILE* handle to print to.
 *
 * Print a Rasqal prefix in a debug format.
 * 
 * The print debug format may change in any release.
 *
 * Return value: non-0 on failure
 **/
int
rasqal_prefix_print(rasqal_prefix* p, FILE* fh)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(p, rasqal_prefix, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(fh, FILE*, 1);

  fprintf(fh, "prefix(%s as %s)",
          (p->prefix ? RASQAL_GOOD_CAST(const char*, p->prefix) : "(default)"),
          raptor_uri_as_string(p->uri));

  return 0;
}

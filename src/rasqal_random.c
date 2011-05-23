/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_random.c - Rasqal RDF Query random functions
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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "rasqal.h"
#include "rasqal_internal.h"



unsigned int
rasqal_random_get_system_seed(rasqal_world* world)
{
  /* Mix seed sources using public domain code from
   * http://www.burtleburtle.net/bob/c/lookup3.c
   */
  uint32_t a = clock();
  uint32_t b = time(NULL);
  uint32_t c;
#ifdef HAVE_UNISTD_H
  c = getpid();
#else
  c = 0;
#endif
  
#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))
  
  /* inlined mix(a, b, c) macro */
  a -= c;  a ^= rot(c, 4);  c += b;
  b -= a;  b ^= rot(a, 6);  a += c;
  c -= b;  c ^= rot(b, 8);  b += a;
  a -= c;  a ^= rot(c,16);  c += b;
  b -= a;  b ^= rot(a,19);  /* a += c; */ /* CLANG: not needed because of below */
  c -= b;  c ^= rot(b, 4);  /* b += a; */ /* CLANG: last calculation not needed */
  
  return (unsigned int)c;
}

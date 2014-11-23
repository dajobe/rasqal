/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * snprintf.c - Rasqal formatted numbers utilities
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
#include <stdarg.h>

#include "rasqal.h"
#include "rasqal_internal.h"


static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";

/**
 * rasqal_format_integer:
 * @buffer: buffer (or NULL)
 * @bufsize: size of above (or 0)
 * @integer: integer value to format
 *
 * INTERNAL - Format an integer as a decimal into a buffer or
 * calculate the size needed.
 *
 * Works Like the C99 snprintf() but just for integers.
 *
 * If @buffer is NULL or the @bufsize is too small, the number of
 * bytes needed (excluding NUL) is returned and no formatting is done.
 *
 * Return value: number of bytes needed or written (excluding NUL) or 0 on failure
 */
size_t
rasqal_format_integer(char* buffer, size_t bufsize, int integer,
                      int width, char padding)
{
  size_t len = 1;
  char *p;
  unsigned int value;
  unsigned int base = 10;

  if(integer < 0) {
    value = RASQAL_GOOD_CAST(unsigned int, -integer);
    len++;
    width++;
  } else
    value = RASQAL_GOOD_CAST(unsigned int, integer);
  while(value /= base)
    len++;

  if(width > 0) {
    size_t width_l = RASQAL_GOOD_CAST(size_t, width);
    if(width_l > len)
      len = width_l;
  }

  if(!buffer || bufsize < (len + 1)) /* +1 for NUL */
    return len;

  if(!padding)
    padding = ' ';

  if(integer < 0)
    value = RASQAL_GOOD_CAST(unsigned int, -integer);
  else
    value = RASQAL_GOOD_CAST(unsigned int, integer);

  p = &buffer[len];
  *p-- = '\0';
  while(value  > 0 && p >= buffer) {
    *p-- = digits[value % base];
    value /= base;
  }
  while(p >= buffer)
    *p-- = padding;
  if(integer < 0)
    *buffer = '-';

  return len;
}

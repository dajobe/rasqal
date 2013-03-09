/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * snprintf.c - Rasqal formatted numbers utilities
 *
 * Copyright (C) 2011-2012, David Beckett http://www.dajobe.org/
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
/* for LONG_MIN and LONG_MAX */
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <float.h>
#define __USE_ISOC99 1
#include <math.h>

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
    return RASQAL_BAD_CAST(int, len);

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


#ifndef HAVE_ROUND
/* round (C99): round x to the nearest integer, away from zero */
#define round(x) (((x) < 0) ? (long)((x)-0.5) : (long)((x)+0.5))
#endif

#ifndef HAVE_TRUNC
/* trunc (C99): round x to the nearest integer, towards zero */
#define trunc(x) (((x) < 0) ? ceil((x)) : floor((x)))
#endif

#ifndef HAVE_LROUND
static long
rasqal_lround(double d)
{
  /* Add +/- 0.5 then then round towards zero.  */
  d = floor(d);

  if(isnan(d) || d > (double)LONG_MAX || d < (double)LONG_MIN) {
    errno = ERANGE;
    /* Undefined behaviour, so we could return anything.  */
    /* return tmp > 0.0 ? LONG_MAX : LONG_MIN;  */
  }
  return (long)d;
}
#define lround(x) rasqal_lround(x)
#endif


static const char* const inf_string = "-INF";

/**
 * rasqal_format_double:
 * @buffer: buffer (or NULL)
 * @bufsize: size of above (or 0)
 * @dvalue: double value to format
 * @min: min width
 * @max: max width
 *
 * INTERNAL - Format a double as an XSD decimal into a buffer or
 * calculate the size needed.
 *
 * Works Like the C99 snprintf() but just for doubles.
 *
 * If @buffer is NULL or the @bufsize is too small, the number of
 * bytes needed (excluding NUL) is returned and no formatting is done.
 *
 * Return value: number of bytes needed or written (excluding NUL) or 0 on failure
 */
size_t
rasqal_format_double(char *buffer, size_t bufsize, double dvalue,
                     unsigned int min, unsigned int max)
{
  double dfvalue;
  long intpart;
  double fracpart = 0.0;
  double frac;
  double frac_delta = 10.0;
  double mod_10;
  size_t exp_len;
  size_t frac_len = 0;
  size_t idx;

  if(isinf(dvalue)) {
    size_t len = (dvalue < 0) ? 5 : 4;

    if(buffer) {
      if(bufsize < len + 1)
        return 0;
      memcpy(buffer, inf_string + (5 - len), len + 1);
    }
    return len;
  }
  
  if(isnan(dvalue)) {
    size_t len = 4;

    if(buffer) {
      if(bufsize < len + 1)
        return 0;
      memcpy(buffer, "NaN", len + 1);
    }
    return len;
  }
  
  if(max < min)
    max = min;

  if(!buffer)
    bufsize = 1000; /* large enough it will never underflow to 0 */

  /* index to the last char */
  idx = bufsize - 1;

  if(buffer)
    buffer[idx] = '\0';
  idx--;
  
  dfvalue = fabs(dvalue);
  intpart = lround(dfvalue);

  /* We "cheat" by converting the fractional part to integer by
   * multiplying by a factor of 10
   */

  frac = (dfvalue - intpart);
  
  for(exp_len = 0; exp_len <= max; ++exp_len) {
    frac *= 10;

    mod_10 = trunc(fmod(trunc(frac), 10));
    
    if(fabs(frac_delta - (fracpart / pow(10, exp_len))) < (DBL_EPSILON * 2.0)) {
      break;
    }
    
    frac_delta = fracpart / pow(10, exp_len);

    /* Only "append" (numerically) if digit is not a zero */
    if(mod_10 > 0 && mod_10 < 10) {
      fracpart = round(frac);
      frac_len = exp_len;
    }
  }
  
  if(frac_len < min) {
    if(buffer)
      buffer[idx] = '0';
    idx--;
  } else {
    /* Convert/write fractional part (right to left) */
    do {
      mod_10 = fmod(trunc(fracpart), 10);
      --frac_len;

      if(buffer)
        buffer[idx] = digits[(unsigned)mod_10];
      idx--;
      fracpart /= 10;

    } while(fracpart > 1 && (frac_len + 1) > 0);
  }

  if(buffer)
    buffer[idx] = '.';
  idx--;

  /* Convert/write integer part (right to left) */
  do {
    if(buffer)
      buffer[idx] = digits[intpart % 10];
    idx--;
    intpart /= 10;
  } while(intpart);
  
  /* Write a sign, if requested */
  if(dvalue < 0) {
    if(buffer)
      buffer[idx] = '-';
    idx--;
  }
  
  return bufsize - idx - 2;
}

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


static const char* const static_str = "NaN-Inf+Inf";

static char et_getdigit(long double *val, int *cnt){
  int digit;
  long double d;
  if( (*cnt)<=0 ) return '0';
  (*cnt)--;
  digit = (int)*val;
  d = digit;
  digit += '0';
  *val = (*val - d)*10.0;
  return (char)digit;
}

/**
 * rasqal_format_double:
 * @buffer: buffer (or NULL)
 * @bufsize: size of above (or 0)
 * @dvalue: double value to format
 * @min: min width - currently unused
 * @max: max width (16 for a 64-bit double)
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
#define MAX_DOUBLE_EXPONENT 350

size_t
rasqal_format_double(char *buffer, size_t bufsize, double dvalue,
                     unsigned int min, unsigned int max)
{
  char prefix = '\0'; /* Prefix character.  "+" or "-" or '\0'. */
  size_t length = 0;
  int exp = 0;
  char *p = buffer;
  int nsd = max; /* Number of significant digits returned */
  long double realvalue = dvalue;
  int c;
  double rounder = 0.5;
  int zc;
  int last_c;

  if( realvalue < 0.0 ) {
    realvalue = -realvalue;
    prefix = '-';
  }

  /* Normalize realvalue to within 10.0 > realvalue >= 1.0 */
  exp = 0;
  if( isnan(realvalue) ) {
    length = 3;
    if(buffer) {
      if(bufsize < length)
        length -= bufsize;
      else
        memcpy(buffer, static_str, length + 1);
    }
    return length;
  }

  /* For non-zero realvalues, find the exponent */
  if( realvalue > 0.0 ) {
    long double scale = 1.0;
    const char* result;

    /* Calculate exponent */
    while( realvalue >= 1e100 * scale && exp <= MAX_DOUBLE_EXPONENT ) {
      scale *= 1e100; exp += 100;
    }
    while( realvalue >= 1e64 * scale && exp <= MAX_DOUBLE_EXPONENT ) {
      scale *= 1e64; exp += 64;
    }
    while( realvalue >= 1e8 * scale && exp <= MAX_DOUBLE_EXPONENT ) {
      scale *= 1e8; exp += 8;
    }
    while( realvalue >= 10.0 * scale && exp <= MAX_DOUBLE_EXPONENT ) {
      scale *= 10.0; exp++;
    }

    /* Adjust double value for chosen exponent until it is just > 1.0 */
    realvalue /= scale;
    while( realvalue < 1e-8 ) {
      realvalue *= 1e8; exp -= 8;
    }
    while( realvalue < 1.0 ) {
      realvalue *= 10.0; exp--;
    }

    if( exp > MAX_DOUBLE_EXPONENT ) {
      if( prefix == '-' ){
        result = static_str + 3;
        length = 4;
      } else if( prefix == '+' ){
        result = static_str + 7;
        length = 4;
      }else{
        result = static_str + 8;
        length = 3;
      }

      if(bufsize < length)
        length -= bufsize;
      else
        memcpy(buffer, result, length + 1);

      return length;
    }
  }

  /* Round floating point value final digit */
  for(c = max; c > 0; c--)
    rounder *= 0.1;

  realvalue += rounder;
  if(realvalue >= 10.0) {
    realvalue *= 0.1; exp++;
  }

  /* The sign in front of the number */
  if(prefix) {
    if(p)
      *p++ = prefix;
    length++;
  }
  /* Digit prior to the decimal point */
  c = et_getdigit(&realvalue, &nsd);
  if(p)
    *p++ = c;
  length++;

  /* The decimal point */
  if(p)
    *p++ = '.';
  length++;

  /* Significant digits after the decimal point */
  for(zc = 0, last_c = '\0'; nsd; last_c = c, length++) {
    c = et_getdigit(&realvalue,&nsd);
    if(p)
      *p++ = c;
    if(c == '0') {
      if(!last_c || last_c == '0')
        zc++;
    } else
      zc = 0;
  }

  /* Remove trailing zeros but always keep 1 digit after . */
  if(zc > 1) {
    zc--;
    length -= zc;
    if(p) {
      p -= zc;
      *p = '0';
    }
  }

  /* Add the "E<NNN>" suffix */
  if(p)
    *p++ = 'E';
  length++;
  if( exp < 0 ) {
    if(p) {
      *p++ = '-'; exp = -exp;
    }
    length++;
  }

  /* Write exponent in decimal */
  if(exp >= 100) {
    if(p)
      *p++ = (char)((exp / 100) + '0');
    length++;
    exp %= 100;
  }
  if(exp >= 10) {
    if(p)
      *p++ = (char)(exp / 10  +'0');
    length++;
  }
  if(p)
    *p++ = (char)(exp % 10 + '0');
  length++;

  if(p) {
    *p = 0;

    //assert(strlen(buffer) == length);
  }
  
  return length;
}

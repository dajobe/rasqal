/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_double.c - Rasqal double utilities
 *
 * Copyright (C) 2012, David Beckett http://www.dajobe.org/
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

/* for frexp(), fabs() and ldexp() - all C99 */
#ifdef HAVE_MATH_H
#include <math.h>
#endif

/* for double and float constants */
#ifdef HAVE_FLOAT_H
#include <float.h>
#endif

#include "rasqal.h"
#include "rasqal_internal.h"


/*
 * rasqal_double_approximately_compare:
 * @a: double
 * @b: double
 *
 * INTERNAL - Compare two doubles approximately
 *
 * Approach from Section 4.2.2 of Seminumerical Algorithms (3rd
 * edition) by D. E. Knuth
 *
 * Return values: <0 if a<b, 0 if 'equal' or >0 if a>b
 */
int
rasqal_double_approximately_compare(double a, double b)
{
  int exponent;
  double delta;
  double difference;

  /* Get larger exponent of a or b into exponent */
  frexp(fabs(a) > fabs(b) ? a : b, &exponent);

  /* Multiply epsilon by 2^exponent to get delta */
  delta = ldexp(RASQAL_DOUBLE_EPSILON, exponent); 
  
  /*
   * Take the difference and evaluate like this:
   *
   * < delta | -delta .... delta | > delta
   * --------------------------------------
   * LESS    | <--- 'EQUAL' ---> | GREATER
   */
  difference = (a - b);
  if(difference > delta)
    return 1;
  else if(difference < -delta) 
    return -1;
  else 
    return 0;
}


/*
 * rasqal_double_approximately_equal:
 * @a: double
 * @b: double
 *
 * INTERNAL - Compare two doubles for approximate equality
 *
 * Return values: non-0 if approximately equal
 */
int
rasqal_double_approximately_equal(double a, double b)
{
  return !rasqal_double_approximately_compare(a, b);
}

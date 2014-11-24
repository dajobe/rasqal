/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_decimal.c - Rasqal XSD Decimal
 *
 * Copyright (C) 2007-2009, David Beckett http://www.dajobe.org/
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

/* for round() prototype */
#define _ISOC99_SOURCE 1

#include <stdio.h>
#include <string.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_MATH_H
#include <math.h>
#endif
#include <stdarg.h>
#ifdef HAVE_FLOAT_H
#include <float.h>
#endif

#include "rasqal.h"
#include "rasqal_internal.h"

#ifndef STANDALONE
/* prototypes */
static void rasqal_xsd_decimal_init(rasqal_xsd_decimal* dec);
static void rasqal_xsd_decimal_clear(rasqal_xsd_decimal* dec);
#endif /* not STANDALONE */

#ifdef RASQAL_DECIMAL_C99
/* C99 Decimal
 * Based on http://www2.hursley.ibm.com/decimal/
 */

#define RASQAL_DECIMAL_RAW _Decimal64
#define RASQAL_DECIMAL_ROUNDING int

#else
#ifdef RASQAL_DECIMAL_MPFR
/* MPFR multiple-precision floating-point computations with correct rounding
 * http://www.mpfr.org/
 */

#ifdef HAVE_MPFR_H
#include <mpfr.h>
#endif
#define RASQAL_DECIMAL_RAW mpfr_t
#define RASQAL_DECIMAL_ROUNDING mp_rnd_t

#else
#ifdef RASQAL_DECIMAL_GMP
/* GNU MP - GNU Multiple Precision Arithmetic Library
 * http://gmplib.org/
 */
#ifdef HAVE_GMP_H
#include <gmp.h>
#endif
#define RASQAL_DECIMAL_RAW mpf_t
#define RASQAL_DECIMAL_ROUNDING int

#else

/* No implementation - use double. */
#define RASQAL_DECIMAL_RAW double
#define RASQAL_DECIMAL_ROUNDING int
#include <float.h>

#endif
#endif
#endif

struct rasqal_xsd_decimal_s {
  unsigned int precision_digits;
  unsigned int precision_bits;
  RASQAL_DECIMAL_RAW raw;
  RASQAL_DECIMAL_ROUNDING rounding;
  char* string;
  size_t string_len;
};


#ifndef STANDALONE

/**
 * rasqal_new_xsd_decimal:
 * @world: rasqal world object
 * 
 * Create a new XSD Decimal object.
 * 
 * Return value: new xsd:decimal object or NULL on failure.
 **/
rasqal_xsd_decimal*
rasqal_new_xsd_decimal(rasqal_world* world)
{
  rasqal_xsd_decimal* dec;

  dec = RASQAL_MALLOC(rasqal_xsd_decimal*, sizeof(*dec));
  if(dec)
    rasqal_xsd_decimal_init(dec);
  return dec;
}


/**
 * rasqal_free_xsd_decimal:
 * @dec: Decimal object
 * 
 * Destroy XSD Decimal object.
 **/
void
rasqal_free_xsd_decimal(rasqal_xsd_decimal* dec)
{
  if(!dec)
    return;
  
  rasqal_xsd_decimal_clear(dec);
  RASQAL_FREE(decimal, dec);
}


static void
rasqal_xsd_decimal_init(rasqal_xsd_decimal* dec)
{
  /* XSD wants min of 18 decimal (base 10) digits 
   * http://www.w3.org/TR/2004/REC-xmlschema-2-20041028/#decimal
   */
#ifdef RASQAL_DECIMAL_NONE
  /* double has a restricted range */
  dec->precision_digits = DBL_DIG;
#else
  dec->precision_digits = 18;
#endif

  /* over-estimate bits since log(10)/log(2) = 3.32192809488736234789 < 4 */
  dec->precision_bits = dec->precision_digits*4;
  
#ifdef RASQAL_DECIMAL_C99
  dec->raw = 0DD;
#endif
#ifdef RASQAL_DECIMAL_MPFR
  mpfr_init2(dec->raw, dec->precision_bits);

  /* GMP_RNDD, GMP_RNDU, GMP_RNDN, GMP_RNDZ */
  dec->rounding = mpfr_get_default_rounding_mode();
#endif
#ifdef RASQAL_DECIMAL_GMP
  mpf_init2(dec->raw, dec->precision_bits);
#endif
#ifdef RASQAL_DECIMAL_NONE
  dec->raw = 0e0;
#endif

  dec->string = NULL;
  dec->string_len = 0;
}


static void
rasqal_xsd_decimal_clear_string(rasqal_xsd_decimal* dec)
{
  if(dec->string) {
    RASQAL_FREE(char*, dec->string);
    dec->string=NULL;
  }
  dec->string_len=0;
}  


static void
rasqal_xsd_decimal_clear(rasqal_xsd_decimal* dec)
{
  rasqal_xsd_decimal_clear_string(dec);
#ifdef RASQAL_DECIMAL_C99
#endif
#ifdef RASQAL_DECIMAL_MPFR
  mpfr_clear(dec->raw);
#endif
#ifdef RASQAL_DECIMAL_GMP
  mpf_clear(dec->raw);
#endif
#ifdef RASQAL_DECIMAL_NONE
  dec->raw= 0e0;
#endif
}  


/**
 * rasqal_xsd_decimal_set_string:
 * @dec: XSD Decimal
 * @string: lexical form
 * 
 * Set an XSD Decimal value from a string lexical form
 * 
 * Return value: non-0 on failure
 **/
int
rasqal_xsd_decimal_set_string(rasqal_xsd_decimal* dec, const char* string)
{
  int rc=0;
  size_t len;
  
  if(!string)
    return 1;

  rasqal_xsd_decimal_clear_string(dec);

  len = strlen(string);
  dec->string = RASQAL_MALLOC(char*, len + 1);
  if(!dec->string)
    return 1;

  memcpy(dec->string, string, len + 1);
  dec->string_len = len;
  
#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  dec->raw = strtod(string, NULL);
#endif
#ifdef RASQAL_DECIMAL_MPFR
  if(*string == '+')
    string++;
  rc = mpfr_set_str(dec->raw, string, 10, dec->rounding);
#endif
#ifdef RASQAL_DECIMAL_GMP
  if(*string == '+')
    string++;
  rc = mpf_set_str(dec->raw, string, 10);
#endif

  return rc;
}


/**
 * rasqal_xsd_decimal_set_long:
 * @dec: XSD Decimal
 * @l: long
 * 
 * Set an XSD Decimal value from a long.
 * 
 * Return value: non-0 on failure
 **/
int
rasqal_xsd_decimal_set_long(rasqal_xsd_decimal* dec, long l)
{
  int rc=0;
  
  rasqal_xsd_decimal_clear_string(dec);

#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  dec->raw=l;
#endif
#ifdef RASQAL_DECIMAL_MPFR
  rc = mpfr_set_si(dec->raw, l, dec->rounding);
#endif
#ifdef RASQAL_DECIMAL_GMP
  mpf_set_si(dec->raw, l);
#endif
  return rc;
}


/**
 * rasqal_xsd_decimal_set_double:
 * @dec: XSD Decimal
 * @d: double
 * 
 * Set an XSD Decimal value from a double.
 * 
 * Return value: non-0 on failure
 **/
int
rasqal_xsd_decimal_set_double(rasqal_xsd_decimal* dec, double d)
{
  int rc=0;
  
  rasqal_xsd_decimal_clear_string(dec);

#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  dec->raw=d;
#endif
#ifdef RASQAL_DECIMAL_MPFR
  mpfr_set_d(dec->raw, d, dec->rounding);
#endif
#ifdef RASQAL_DECIMAL_GMP
  mpf_set_d(dec->raw, d);
#endif
  return rc;
}


/**
 * rasqal_xsd_decimal_get_double:
 * @dec: XSD Decimal
 * 
 * Get an XSD Decimal as a double (may lose precision)
 * 
 * Return value: double value.
 **/
double
rasqal_xsd_decimal_get_double(rasqal_xsd_decimal* dec)
{
  double result=0e0;

#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  result=(double)dec->raw;
#endif
#ifdef RASQAL_DECIMAL_MPFR
  result = mpfr_get_d(dec->raw, dec->rounding);
#endif
#ifdef RASQAL_DECIMAL_GMP
  result=mpf_get_d(dec->raw);
#endif

  return result;
}

/**
 * rasqal_xsd_decimal_get_long:
 * @dec: XSD Decimal
 * @error_p: pointer to error flag
 * 
 * Get an XSD Decimal as a long (may lose precision)
 * 
 * Return value: long value or 0 on failure and *error_p is non-0
 **/
long
rasqal_xsd_decimal_get_long(rasqal_xsd_decimal* dec, int* error_p)
{
  long result = 0;

#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  result=(long)dec->raw;
#endif
#ifdef RASQAL_DECIMAL_MPFR
  if(!mpfr_fits_slong_p(dec->raw, dec->rounding)) {
    if(error_p)
      *error_p = 1;
  } else
    result = mpfr_get_si(dec->raw, dec->rounding);
#endif
#ifdef RASQAL_DECIMAL_GMP
  if(!mpf_fits_slong_p(dec->raw)) {
    if(error_p)
      *error_p = 1;
  } else
    result = mpf_get_si(dec->raw);
#endif

  return result;
}

#ifdef RASQAL_DECIMAL_C99
#define DECIMAL_SNPRINTF snprintf
#define DECIMAL_FMT "%DDf"
#endif
#ifdef RASQAL_DECIMAL_MPFR
#define DECIMAL_SNPRINTF mpfr_snprintf
#define DECIMAL_FMT "%%.%uRf"
#endif
#ifdef RASQAL_DECIMAL_GMP
#define DECIMAL_SNPRINTF gmp_snprintf
#define DECIMAL_FMT "%%.%uFf"
#endif
#ifdef RASQAL_DECIMAL_NONE
#define DECIMAL_SNPRINTF snprintf
#define DECIMAL_FMT "%%.%uf"
#endif


/**
 * rasqal_xsd_decimal_as_string:
 * @dec: XSD Decimal
 * 
 * Get an XSD Decimal as a string lexical form.
 *
 * The returned string is shared and owned by the @dec object and
 * must be copied.
 * 
 * Return value: lexical form string or NULL on failure.
 **/
char*
rasqal_xsd_decimal_as_string(rasqal_xsd_decimal* dec)
{
  char *s=NULL;
  size_t len=0;
#ifdef RASQAL_DECIMAL_C99
#else
  char fmt[18];
  size_t e_index = 0;
  size_t trailing_zero_start = 0;
#endif
  
  if(dec->string)
    return dec->string;
  
#ifdef RASQAL_DECIMAL_C99
  len = dec->precision_digits;
  s = RASQAL_MALLOC(cstring, len + 1);
  if(!s)
    return NULL;

  /* NOTE: Never seen a sprintf that supports _Decimal yet */
  snprintf(s, len, "%DDf", dec->raw);
  len = strlen(s);
#else
  /* construct a format string */
  snprintf(fmt, sizeof(fmt), DECIMAL_FMT, dec->precision_digits);
    
  /* decimal snprintf with no buffer to get buffer length */
  len = RASQAL_GOOD_CAST(size_t, DECIMAL_SNPRINTF(NULL, 0, fmt, dec->raw));

  s = RASQAL_MALLOC(char*, len + 1);
  if(!s)
    return NULL;
  
  /* format into allocated buffer */
  DECIMAL_SNPRINTF(s, len, fmt, dec->raw);

  len = strlen(s);

  /* find trailing zeros */
  for(e_index = len - 1;
      e_index > 0 && s[e_index] == '0';
      e_index--) {
    trailing_zero_start = e_index;
  }

  /* Stop string where the trailing zeros started */
  if(trailing_zero_start) {
    s[trailing_zero_start] = '\0';
    len = trailing_zero_start;
  }
  
  if(s[len - 1] == '.') {
    /* Ends in . so add 0 for canonical format */
    s[len++] = '0';
    s[len] = '\0';
  }
  
#endif

  dec->string = s;
  dec->string_len = len;

  return s;
}


/**
 * rasqal_xsd_decimal_as_counted_string:
 * @dec: XSD Decimal
 * @len_p: pointer to length variable (or NULL)
 * 
 * Get an XSD Decimal as a string lexical form with optional length.
 *
 * The returned string is shared and owned by the @dec object and
 * must be copied.  If @len_p is not NULL, the length of the returned
 * string is stored.
 * 
 * Return value: lexical form string or NULL on failure.
 **/
char*
rasqal_xsd_decimal_as_counted_string(rasqal_xsd_decimal* dec, size_t* len_p)
{
  char* s=rasqal_xsd_decimal_as_string(dec);
  if(s && len_p)
    *len_p=dec->string_len;
  return s;
}


/**
 * rasqal_xsd_decimal_print:
 * @dec: XSD Decimal object
 * @stream: FILE* handle to print to
 * 
 * Print an XSD Decimal to a stream
 * 
 * Return value: non-0 on failure
 **/
int
rasqal_xsd_decimal_print(rasqal_xsd_decimal* dec, FILE* stream)
{
  char* s=NULL;
  size_t len=0;
  
  s=rasqal_xsd_decimal_as_counted_string(dec, &len);
  if(!s)
    return 1;
  
  fwrite(s, 1, len, stream);
  return 0;
}


/**
 * rasqal_xsd_decimal_add:
 * @result: result variable
 * @a: argment decimal 1
 * @b: argument decimal 2
 * 
 * Add two XSD Decimals and store in result XSD Decimal
 * 
 * Return value: non-0 on failure
 **/
int
rasqal_xsd_decimal_add(rasqal_xsd_decimal* result, 
                       rasqal_xsd_decimal* a, rasqal_xsd_decimal* b)
{
  int rc=0;

  rasqal_xsd_decimal_clear_string(result);
  
#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  result->raw = a->raw + b->raw;
#endif
#ifdef RASQAL_DECIMAL_MPFR
  mpfr_add(result->raw, a->raw, b->raw, result->rounding);
#endif
#ifdef RASQAL_DECIMAL_GMP
  mpf_add(result->raw, a->raw, b->raw);
#endif

  return rc;
}


/**
 * rasqal_xsd_decimal_subtract:
 * @result: result variable
 * @a: argment decimal 1
 * @b: argument decimal 2
 * 
 * Subtract two XSD Decimals and store in result XSD Decimal
 * 
 * Return value: non-0 on failure
 **/
int
rasqal_xsd_decimal_subtract(rasqal_xsd_decimal* result, 
                            rasqal_xsd_decimal* a, rasqal_xsd_decimal* b)
{
  int rc=0;
  
  rasqal_xsd_decimal_clear_string(result);
  
#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  result->raw = a->raw - b->raw;
#endif
#ifdef RASQAL_DECIMAL_MPFR
  mpfr_sub(result->raw, a->raw, b->raw, result->rounding);
#endif
#ifdef RASQAL_DECIMAL_GMP
  mpf_sub(result->raw, a->raw, b->raw);
#endif

  return rc;
}


/**
 * rasqal_xsd_decimal_multiply:
 * @result: result variable
 * @a: argment decimal 1
 * @b: argument decimal 2
 * 
 * Multiply two XSD Decimals and store in result XSD Decimal
 * 
 * Return value: non-0 on failure
 **/
int
rasqal_xsd_decimal_multiply(rasqal_xsd_decimal* result, 
                            rasqal_xsd_decimal* a, rasqal_xsd_decimal* b)
{
  int rc=0;
  
  rasqal_xsd_decimal_clear_string(result);
  
#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  result->raw = a->raw * b->raw;
#endif
#ifdef RASQAL_DECIMAL_MPFR
  mpfr_mul(result->raw, a->raw, b->raw, result->rounding);
#endif
#ifdef RASQAL_DECIMAL_GMP
  mpf_mul(result->raw, a->raw, b->raw);
#endif

  return rc;
}


/**
 * rasqal_xsd_decimal_is_zero:
 * @d: decimal
 * 
 * Test if an XSD decimal is zero.
 *
 * Return value: non-0 if decimal is zero
 **/
int
rasqal_xsd_decimal_is_zero(rasqal_xsd_decimal* d)
{
  int rc = 0;

#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  rc = fabs(d->raw) < RASQAL_DOUBLE_EPSILON;
#endif
#ifdef RASQAL_DECIMAL_MPFR
  rc = mpfr_zero_p(d->raw);
#endif
#ifdef RASQAL_DECIMAL_GMP
  rc = !mpf_sgn(d->raw);
#endif

  return rc;
}


/**
 * rasqal_xsd_decimal_divide:
 * @result: result variable
 * @a: argment decimal 1
 * @b: argument decimal 2
 * 
 * Divide two XSD Decimals and store in result XSD Decimal
 *
 * If the divisor @b is 0, failure is returned
 * 
 * Return value: non-0 on failure
 **/
int
rasqal_xsd_decimal_divide(rasqal_xsd_decimal* result, 
                          rasqal_xsd_decimal* a, rasqal_xsd_decimal* b)
{
  int rc=0;
  
  rasqal_xsd_decimal_clear_string(result);

  if(rasqal_xsd_decimal_is_zero(b))
    return 1;
  
#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  result->raw = a->raw / b->raw;
#endif
#ifdef RASQAL_DECIMAL_MPFR
  mpfr_div(result->raw, a->raw, b->raw, result->rounding);
#endif
#ifdef RASQAL_DECIMAL_GMP
  mpf_div(result->raw, a->raw, b->raw);
#endif

  return rc;
}


/**
 * rasqal_xsd_decimal_negate:
 * @result: result variable
 * @a: argment decimal
 * 
 * Negate an XSD Decimal
 *
 * Return value: non-0 on failure
 **/
int
rasqal_xsd_decimal_negate(rasqal_xsd_decimal* result, rasqal_xsd_decimal* a)
{
  int rc=0;
  
  rasqal_xsd_decimal_clear_string(result);
  
#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  result->raw = -a->raw;
#endif
#ifdef RASQAL_DECIMAL_MPFR
  mpfr_neg(result->raw, a->raw, result->rounding);
#endif
#ifdef RASQAL_DECIMAL_GMP
  mpf_neg(result->raw, a->raw);
#endif

  return rc;
}


/**
 * rasqal_xsd_decimal_abs:
 * @result: result variable
 * @a: argment decimal
 * 
 * Return the absolute value of an XSD Decimal
 *
 * Return value: non-0 on failure
 **/
int
rasqal_xsd_decimal_abs(rasqal_xsd_decimal* result, rasqal_xsd_decimal* a)
{
  int rc = 0;
  
  rasqal_xsd_decimal_clear_string(result);
  
#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  result->raw = fabs(a->raw);
#endif
#ifdef RASQAL_DECIMAL_MPFR
  mpfr_abs(result->raw, a->raw, result->rounding);
#endif
#ifdef RASQAL_DECIMAL_GMP
  mpf_abs(result->raw, a->raw);
#endif

  return rc;
}


/**
 * rasqal_xsd_decimal_round:
 * @result: result variable
 * @a: argment decimal
 * 
 * Return the number with no fractional part closes to argument for an XSD Decimal
 *
 * Return value: non-0 on failure
 **/
int
rasqal_xsd_decimal_round(rasqal_xsd_decimal* result, rasqal_xsd_decimal* a)
{
  int rc = 0;
#ifdef RASQAL_DECIMAL_GMP
  mpf_t b;
  mpf_t c;
#endif
  
  rasqal_xsd_decimal_clear_string(result);
  
#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  result->raw = round(a->raw);
#endif
#ifdef RASQAL_DECIMAL_MPFR
  mpfr_round(result->raw, a->raw);
#endif
#ifdef RASQAL_DECIMAL_GMP
  /* GMP has no mpf_round so use result := floor(a + 0.5) */
  mpf_init2(b, a->precision_bits);
  mpf_init2(c, a->precision_bits);

  mpf_set_d(b, 0.5);
  mpf_add(c, a->raw, b);
  mpf_floor(result->raw, c);

  mpf_clear(b);
  mpf_clear(c);
#endif

  return rc;
}


/**
 * rasqal_xsd_decimal_ceil:
 * @result: result variable
 * @a: argment decimal
 * 
 * Return the number with no fractional part closes to argument for an XSD Decimal
 *
 * Return value: non-0 on failure
 **/
int
rasqal_xsd_decimal_ceil(rasqal_xsd_decimal* result, rasqal_xsd_decimal* a)
{
  int rc = 0;
  
  rasqal_xsd_decimal_clear_string(result);
  
#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  result->raw = ceil(a->raw);
#endif
#ifdef RASQAL_DECIMAL_MPFR
  mpfr_ceil(result->raw, a->raw);
#endif
#ifdef RASQAL_DECIMAL_GMP
  mpf_ceil(result->raw, a->raw);
#endif

  return rc;
}


/**
 * rasqal_xsd_decimal_floor:
 * @result: result variable
 * @a: argment decimal
 * 
 * Return the number with no fractional part closes to argument for an XSD Decimal
 *
 * Return value: non-0 on failure
 **/
int
rasqal_xsd_decimal_floor(rasqal_xsd_decimal* result, rasqal_xsd_decimal* a)
{
  int rc = 0;
  
  rasqal_xsd_decimal_clear_string(result);
  
#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  result->raw = floor(a->raw);
#endif
#ifdef RASQAL_DECIMAL_MPFR
  mpfr_floor(result->raw, a->raw);
#endif
#ifdef RASQAL_DECIMAL_GMP
  mpf_floor(result->raw, a->raw);
#endif

  return rc;
}


/**
 * rasqal_xsd_decimal_compare:
 * @a: first XSD decimal
 * @b: second XSD decimal
 * 
 * Compare two XSD Decimals
 * 
 * Return value: <0 if @a is less than @b, 0 if equal, >1 otherwise
 **/
int
rasqal_xsd_decimal_compare(rasqal_xsd_decimal* a, rasqal_xsd_decimal* b)
{
  int rc = 0;
  
#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  rc = rasqal_double_approximately_compare(a->raw, b->raw);
#endif
#ifdef RASQAL_DECIMAL_MPFR
  rc = mpfr_cmp(a->raw, b->raw);
#endif
#ifdef RASQAL_DECIMAL_GMP
  rc=mpf_cmp(a->raw, b->raw);
#endif

  return rc;
}


/**
 * rasqal_xsd_decimal_equals:
 * @a: first XSD Decimal
 * @b: second XSD Decimal
 * 
 * Compare two XSD Decimals for equality.
 * 
 * Return value: non-0 if equal.
 **/
int
rasqal_xsd_decimal_equals(rasqal_xsd_decimal* a, rasqal_xsd_decimal* b)
{
  int rc;
  
#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  rc = rasqal_double_approximately_equal(b->raw, a->raw);
#elif defined(RASQAL_DECIMAL_MPFR)
  rc = mpfr_equal_p(a->raw, b->raw);
#elif defined(RASQAL_DECIMAL_GMP)
  /* NOTE: Not using mpf_eq() but could do, with sufficient bits */
  rc=!mpf_cmp(a->raw, b->raw);
#else
#error RASQAL_DECIMAL flagging error
#endif

  return rc;
}
#endif /* not STANDALONE */


#ifdef STANDALONE
#include <stdio.h>

int main(int argc, char *argv[]);

int
main(int argc, char *argv[]) {
  char const *program=rasqal_basename(*argv);
  int failures=0;
  rasqal_xsd_decimal *a;
  rasqal_xsd_decimal *b;
  rasqal_xsd_decimal *result;
  rasqal_xsd_decimal *result2;
  double result_d;
  char *result_s;
  int result_i;
  const long a_long = 1234567890L;
  const double a_double = 1234567890e0;
  const char* b_string = "123456789012345678";
  const char* expected_a_plus_b = "123456790246913568.0";
  const char* expected_a_plus_b_minus_b = "1234567890.0";
  const char* expected_a_plus_b_minus_b_minus_a = "0.0";
  const char* expected_negative_b = "-123456789012345678.0";
  int expected_a_compare_b= -1;
  int expected_a_equals_b= 0;
  rasqal_world *world;

  world = rasqal_new_world();

#ifdef RASQAL_DECIMAL_MPFR
  fprintf(stderr, "%s: Using MPFR %s\n", program, mpfr_get_version());
#endif
#ifdef RASQAL_DECIMAL_GMP
#ifdef HAVE_GMP_VERSION
  fprintf(stderr, "%s: Using GMP %s\n", program, gmp_version);
#else
  fprintf(stderr, "%s: Using GMP version unknown\n", program);
#endif
#endif
#ifdef RASQAL_DECIMAL_NONE
  fprintf(stderr, "%s: Using double\n", program);
#endif

#ifdef RASQAL_DECIMAL_NONE
#define FAIL_LABEL
#define FAIL failures++
#else
#define FAIL_LABEL tidy:
#define FAIL failures++; goto tidy
#endif

  a = rasqal_new_xsd_decimal(world);
  b = rasqal_new_xsd_decimal(world);

  result = rasqal_new_xsd_decimal(world);
  result2 = rasqal_new_xsd_decimal(world);
  if(!result || !result2) {
    fprintf(stderr, "%s: rasqal_new_xsd_decimal() failed\n", program);
    FAIL;
  }

  rasqal_xsd_decimal_set_long(a, a_long);
  rasqal_xsd_decimal_set_string(b, b_string);

  result_d=rasqal_xsd_decimal_get_double(a);
  if(!rasqal_double_approximately_equal(result_d, a_double)) {
    fprintf(stderr, "FAILED: a=%f expected %f\n", result_d, a_double);
    FAIL;
  }

  result_s=rasqal_xsd_decimal_as_string(b);
  if(strcmp(result_s, b_string)) {
    fprintf(stderr, "FAILED: b=%s expected %s\n", result_s, b_string);
    FAIL;
  }

  /* result = a+b */
  rasqal_xsd_decimal_add(result, a, b);

  result_s=rasqal_xsd_decimal_as_string(result);
  if(strcmp(result_s, expected_a_plus_b)) {
    fprintf(stderr, "FAILED: a+b=%s expected %s\n", result_s, 
            expected_a_plus_b);
    FAIL;
  }
  
  /* result2 = result-b */
  rasqal_xsd_decimal_subtract(result2, result, b);

  result_s=rasqal_xsd_decimal_as_string(result2);
  if(strcmp(result_s, expected_a_plus_b_minus_b)) {
    fprintf(stderr, "FAILED: (a+b)-b=%s expected %s\n", result_s, 
            expected_a_plus_b_minus_b);
    FAIL;
  }

  /* result = result2-a */
  rasqal_xsd_decimal_subtract(result, result2, a);

  result_s=rasqal_xsd_decimal_as_string(result);
  if(strcmp(result_s, expected_a_plus_b_minus_b_minus_a)) {
    fprintf(stderr, "FAILED: (a+b)-b-a=%s expected %s\n", result_s, 
            expected_a_plus_b_minus_b_minus_a);
    FAIL;
  }

  result_i=rasqal_xsd_decimal_compare(a, b);
  if((expected_a_compare_b < 0 && result_i >= 0) ||
     (expected_a_compare_b > 0 && result_i <= 0) ||
     (expected_a_compare_b == 0 && result_i != 0))
  {
    fprintf(stderr, "FAILED: a compare b = %d expected %d\n",
            result_i, expected_a_compare_b);
    FAIL;
  }

  result_i=rasqal_xsd_decimal_equals(a, b);
  if(result_i != expected_a_equals_b) {
    fprintf(stderr, "FAILED: a equals b = %d expected %d\n",
            result_i, expected_a_equals_b);
    FAIL;
  }

  /* result2 = -b */
  rasqal_xsd_decimal_negate(result, b);

  result_s=rasqal_xsd_decimal_as_string(result);
  if(strcmp(result_s, expected_negative_b)) {
    fprintf(stderr, "FAILED: -b=%s expected %s\n", result_s, 
            expected_negative_b);
    FAIL;
  }


  FAIL_LABEL
  if(a)
    rasqal_free_xsd_decimal(a);
  if(b)
    rasqal_free_xsd_decimal(b);
  if(result)
     rasqal_free_xsd_decimal(result);
  if(result2)
     rasqal_free_xsd_decimal(result2);

  rasqal_free_world(world);
  
#ifdef RASQAL_DECIMAL_NONE
  if(failures)
    fprintf(stderr, "%s: ignoring %d failures as RASQAL_DECIMAL_NONE specified\n", program, failures);
  return 0;
#else
  return failures;
#endif
}

#endif /* STANDALONE */

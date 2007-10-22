/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_decimal.c - Rasqal XSD Decimal
 *
 * Copyright (C) 2007, David Beckett http://purl.org/net/dajobe/
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


/* prototypes */
static void rasqal_xsd_decimal_init(rasqal_xsd_decimal* dec);
static void rasqal_xsd_decimal_clear(rasqal_xsd_decimal* dec);


#ifdef RASQAL_DECIMAL_C99
/* C99 Decimal
 * Based on http://www2.hursley.ibm.com/decimal/
 */

#include <float.h>
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


rasqal_xsd_decimal*
rasqal_new_xsd_decimal(void)
{
  rasqal_xsd_decimal* dec;
  dec=(rasqal_xsd_decimal*)RASQAL_MALLOC(decimal, sizeof(rasqal_xsd_decimal));
  if(dec)
    rasqal_xsd_decimal_init(dec);
  return dec;
}


void
rasqal_free_xsd_decimal(rasqal_xsd_decimal* dec)
{
  if(dec) {
    rasqal_xsd_decimal_clear(dec);
    RASQAL_FREE(decimal, dec);
  }
}


static void
rasqal_xsd_decimal_init(rasqal_xsd_decimal* dec)
{
  /* XSD wants min of 18 decimal (base 10) digits 
   * http://www.w3.org/TR/2004/REC-xmlschema-2-20041028/#decimal
   */
  dec->precision_digits= 32;
  /* over-estimate bits since log(10)/log(2) = 3.32192809488736234789 < 4 */
  dec->precision_bits= dec->precision_digits*4;
  
#ifdef RASQAL_DECIMAL_C99
  dec->raw= 0DD;
#endif
#ifdef RASQAL_DECIMAL_MPFR
  mpfr_init2(dec->raw, dec->precision_bits);

  /* GMP_RNDD, GMP_RNDU, GMP_RNDN, GMP_RNDZ */
  dec->rounding=mpfr_get_default_rounding_mode();
#endif
#ifdef RASQAL_DECIMAL_GMP
  mpf_init2(dec->raw, dec->precision_bits);
#endif
#ifdef RASQAL_DECIMAL_NONE
  dec->raw= 0e0;
#endif

  dec->string=NULL;
  dec->string_len=0;
}


static void
rasqal_xsd_decimal_clear_string(rasqal_xsd_decimal* dec)
{
  if(dec->string) {
    RASQAL_FREE(cstring, dec->string);
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


int
rasqal_xsd_decimal_set_string(rasqal_xsd_decimal* dec, const char* string)
{
  int rc=0;
  size_t len;
  
  if(!string)
    return 1;

  rasqal_xsd_decimal_clear_string(dec);

  len=strlen(string);
  dec->string=RASQAL_MALLOC(cstring, len+1);
  if(!dec->string)
    return 1;
  strncpy(dec->string, string, len+1);
  dec->string_len=len;
  
#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  dec->raw=strtod(string);
#endif
#ifdef RASQAL_DECIMAL_MPFR
  rc=mpfr_set_str(dec->raw, string, 10, dec->rounding);
#endif
#ifdef RASQAL_DECIMAL_GMP
  rc=mpf_set_str(dec->raw, string, 10);
#endif

  return rc;
}


int
rasqal_xsd_decimal_set_long(rasqal_xsd_decimal* dec, long l)
{
  int rc=0;
  
  rasqal_xsd_decimal_clear_string(dec);

#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  dec->raw=l;
#endif
#ifdef RASQAL_DECIMAL_MPFR
  rc=mpfr_set_si(dec->raw, l, dec->rounding);
#endif
#ifdef RASQAL_DECIMAL_GMP
  mpf_set_si(dec->raw, l);
#endif
  return rc;
}


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


double
rasqal_xsd_decimal_get_double(rasqal_xsd_decimal* dec)
{
  double result=0e0;

#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  result=(double)dec->raw;
#endif
#ifdef RASQAL_DECIMAL_MPFR
  result=mpfr_get_d(dec->raw, dec->rounding);
#endif
#ifdef RASQAL_DECIMAL_GMP
  result=mpf_get_d(dec->raw);
#endif

  return result;
}


char*
rasqal_xsd_decimal_as_string(rasqal_xsd_decimal* dec)
{
  char *s=NULL;
  size_t len=0;
#if defined(RASQAL_DECIMAL_MPFR) || defined(RASQAL_DECIMAL_GMP)
  mp_exp_t expo;
  char *mpf_s;
#endif
  
  if(dec->string)
    return dec->string;
  
#ifdef RASQAL_DECIMAL_C99
  len=dec->precision_digits;
  s=RASQAL_MALLOC(cstring, len+1);
  if(!s)
    return NULL;
  /* NOTE: Never seen a sprintf that supports _Decimal yet */
  snprintf(s, len, "%DDf", dec->raw);
  len=strlen(s);
#endif
#ifdef RASQAL_DECIMAL_MPFR
  mpf_s=mpfr_get_str(NULL, &expo, 10, 0, dec->raw, dec->rounding);
  if(mpf_s) {
    size_t from_len=strlen(mpf_s);
    char *from_p=mpf_s;
    size_t to_len;

    /* 7=strlen("0.0e0")+1 for sign */
    to_len=!from_len ? 6 : (from_len*2);

    s=RASQAL_MALLOC(cstring, to_len);
    if(!s) {
      mpfr_free_str((char*)mpf_s);
      return NULL;
    }
    /* first digit of mantissa */
    if(!*from_p || *from_p == '0') {
      len=5;
      strncpy(s, "0.0e0", len+1);
    } else {
      char* to_p=s;
      int n;
      
      if(*from_p == '-') {
        *to_p++ = *from_p++;
        from_len--;
      }

      *to_p++ = *from_p++;
      from_len--;
      *to_p++ = '.';
      /* rest of mantissa */
      /* remove trailing 0s */
      while(from_len > 1 && from_p[from_len-1]=='0')
        from_len--;
      strncpy(to_p, from_p, from_len);
      to_p += from_len;
      /* exp */
      n=sprintf(to_p, "e%ld", expo-1);
      len=to_p+n-s;
    }
    mpfr_free_str((char*)mpf_s);
  }
#endif
#ifdef RASQAL_DECIMAL_GMP
  mpf_s=mpf_get_str(NULL, &expo, 10, 0, dec->raw);
  if(mpf_s) {
    size_t from_len=strlen(mpf_s);
    char *from_p=mpf_s;
    size_t to_len;

    /* 7=strlen("0.0e0")+1 for sign */
    to_len=!from_len ? 6 : (from_len*2);

    s=RASQAL_MALLOC(cstring, to_len+1);
    if(!s) {
      free(mpf_s);
      return NULL;
    }
    /* first digit of mantissa */
    if(!*from_p || *from_p == '0') {
      len=5;
      strncpy(s, "0.0e0", len+1);
    } else {
      char *to_p=s;
      int n;
      
      if(*from_p == '-') {
        *to_p++ = *from_p++;
        from_len--;
      }

      *to_p++ = *from_p++;
      from_len--;
      *to_p++ = '.';
      /* rest of mantissa */
      strncpy(to_p, from_p, from_len);
      to_p+= from_len;
      /* exp */
      n=sprintf(to_p, "e%ld", expo-1);
      len=to_p+n-s;
    }
    free(mpf_s);
  }
#endif
#ifdef RASQAL_DECIMAL_NONE
  len=dec->precision_digits;
  s=RASQAL_MALLOC(cstring, len+1);
  if(!s)
    return NULL;
  
  snprintf(s, len, "%f", dec->raw);
  len=strlen(s);
#endif

  dec->string=s;
  dec->string_len=len;
  return s;
}


char*
rasqal_xsd_decimal_as_counted_string(rasqal_xsd_decimal* dec, size_t* len_p)
{
  char* s=rasqal_xsd_decimal_as_string(dec);
  if(s && len_p)
    *len_p=dec->string_len;
  return s;
}


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

int
rasqal_xsd_decimal_add(rasqal_xsd_decimal* result, 
                       rasqal_xsd_decimal* a, rasqal_xsd_decimal* b)
{
  int rc=0;

  rasqal_xsd_decimal_clear_string(result);
  
#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  result->raw = result->a + result->b;
#endif
#ifdef RASQAL_DECIMAL_MPFR
  rc=mpfr_add(result->raw, a->raw, b->raw, result->rounding);
#endif
#ifdef RASQAL_DECIMAL_GMP
  mpf_add(result->raw, a->raw, b->raw);
#endif

  return rc;
}


int
rasqal_xsd_decimal_subtract(rasqal_xsd_decimal* result, 
                            rasqal_xsd_decimal* a, rasqal_xsd_decimal* b)
{
  int rc=0;
  
  rasqal_xsd_decimal_clear_string(result);
  
#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  result->raw = result->a - result->b;
#endif
#ifdef RASQAL_DECIMAL_MPFR
  rc=mpfr_sub(result->raw, a->raw, b->raw, result->rounding);
#endif
#ifdef RASQAL_DECIMAL_GMP
  mpf_sub(result->raw, a->raw, b->raw);
#endif

  return rc;
}


int
rasqal_xsd_decimal_multiply(rasqal_xsd_decimal* result, 
                            rasqal_xsd_decimal* a, rasqal_xsd_decimal* b)
{
  int rc=0;
  
  rasqal_xsd_decimal_clear_string(result);
  
#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  result->raw = result->a * result->b;
#endif
#ifdef RASQAL_DECIMAL_MPFR
  rc=mpfr_mul(result->raw, a->raw, b->raw, result->rounding);
#endif
#ifdef RASQAL_DECIMAL_GMP
  mpf_mul(result->raw, a->raw, b->raw);
#endif

  return rc;
}


int
rasqal_xsd_decimal_divide(rasqal_xsd_decimal* result, 
                          rasqal_xsd_decimal* a, rasqal_xsd_decimal* b)
{
  int rc=0;
  
  rasqal_xsd_decimal_clear_string(result);
  
#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  if(!result->b)
    return 1;
  
  result->raw = result->a / result->b;
#endif
#ifdef RASQAL_DECIMAL_MPFR
  if(mpfr_zero_p(b->raw))
    return 1;
  
  rc=mpfr_div(result->raw, a->raw, b->raw, result->rounding);
#endif
#ifdef RASQAL_DECIMAL_GMP
  if(!mpf_sgn(b->raw))
    return 1;
  
  mpf_div(result->raw, a->raw, b->raw);
#endif

  return rc;
}


int
rasqal_xsd_decimal_compare(rasqal_xsd_decimal* a, rasqal_xsd_decimal* b)
{
  int rc=0;
  
#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  rc= (int)(result->b - result->a);
#endif
#ifdef RASQAL_DECIMAL_MPFR
  rc=mpfr_cmp(a->raw, b->raw);
#endif
#ifdef RASQAL_DECIMAL_GMP
  rc=mpf_cmp(a->raw, b->raw);
#endif

  return rc;
}


int
rasqal_xsd_decimal_equals(rasqal_xsd_decimal* a, rasqal_xsd_decimal* b)
{
  int rc;
  
#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  rc= (result->b == result->a);
#elif defined(RASQAL_DECIMAL_MPFR)
  rc=mpfr_equal_p(a->raw, b->raw);
#elif defined(RASQAL_DECIMAL_GMP)
  /* NOTE: Not using mpf_eq() but could do, with sufficient bits */
  rc=!mpf_cmp(a->raw, b->raw);
#else
#error RASQAL_DECIMAL flagging error
#endif

  return rc;
}


#ifdef STANDALONE
#include <stdio.h>

int main(int argc, char *argv[]);

int
main(int argc, char *argv[]) {
  char const *program=rasqal_basename(*argv);
  int rc=1;
  rasqal_xsd_decimal a;
  rasqal_xsd_decimal b;
  rasqal_xsd_decimal *result;
  rasqal_xsd_decimal *result2;
  double result_d;
  char *result_s;
  int result_i;
  const long a_long=1234567890L;
  const double a_double=1234567890e0;
  const char* b_string="123456789012345678e0";
  const char* expected_a_plus_b="1.23456790246913568e17";
  const char* expected_a_plus_b_minus_b="1.23456789e9";
  const char* expected_a_plus_b_minus_b_minus_a="0.0e0";
  int expected_a_compare_b= -1;
  int expected_a_equals_b= 0;

#ifdef RASQAL_DECIMAL_MPFR
  fprintf(stderr, "%s: Using MPFR %s\n", program, mpfr_get_version());
#endif
#ifdef RASQAL_DECIMAL_GMP
  fprintf(stderr, "%s: Using GMP %s\n", program, gmp_version);
#endif
#ifdef RASQAL_DECIMAL_NONE
  fprintf(stderr, "%s: Using double\n", program);
#endif

  rasqal_xsd_decimal_init(&a);
  rasqal_xsd_decimal_init(&b);

  result=rasqal_new_xsd_decimal();
  result2=rasqal_new_xsd_decimal();
  if(!result || !result2) {
    fprintf(stderr, "%s: rasqal_new_xsd_decimal() failed\n", program);
    goto tidy;
  }

  rasqal_xsd_decimal_set_long(&a, a_long);
  rasqal_xsd_decimal_set_string(&b, b_string);

  result_d=rasqal_xsd_decimal_get_double(&a);
  if(result_d != a_double) {
    fprintf(stderr, "FAILED: a=%lf expected %lf\n", result_d, a_double);
    rc=1;
    goto tidy;
  }

  result_s=rasqal_xsd_decimal_as_string(&b);
  if(strcmp(result_s, b_string)) {
    fprintf(stderr, "FAILED: b=%s expected %s\n", result_s, b_string);
    rc=1;
    goto tidy;
  }

  /* result = a+b */
  rasqal_xsd_decimal_add(result, &a, &b);

  result_s=rasqal_xsd_decimal_as_string(result);
  if(strcmp(result_s, expected_a_plus_b)) {
    fprintf(stderr, "FAILED: a+b=%s expected %s\n", result_s, 
            expected_a_plus_b);
    rc=1;
    goto tidy;
  }
  
  /* result2 = result-b */
  rasqal_xsd_decimal_subtract(result2, result, &b);

  result_s=rasqal_xsd_decimal_as_string(result2);
  if(strcmp(result_s, expected_a_plus_b_minus_b)) {
    fprintf(stderr, "FAILED: (a+b)-b=%s expected %s\n", result_s, 
            expected_a_plus_b_minus_b);
    rc=1;
    goto tidy;
  }

  /* result = result2-a */
  rasqal_xsd_decimal_subtract(result, result2, &a);

  result_s=rasqal_xsd_decimal_as_string(result);
  if(strcmp(result_s, expected_a_plus_b_minus_b_minus_a)) {
    fprintf(stderr, "FAILED: (a+b)-b-a=%s expected %s\n", result_s, 
            expected_a_plus_b_minus_b_minus_a);
    rc=1;
    goto tidy;
  }

  result_i=rasqal_xsd_decimal_compare(&a, &b);
  if(result_i != expected_a_compare_b) {
    fprintf(stderr, "FAILED: a compare b = %d expected %d\n",
            result_i, expected_a_compare_b);
    rc=1;
    goto tidy;
  }

  result_i=rasqal_xsd_decimal_equals(&a, &b);
  if(result_i != expected_a_equals_b) {
    fprintf(stderr, "FAILED: a equals b = %d expected %d\n",
            result_i, expected_a_equals_b);
    rc=1;
    goto tidy;
  }

  rc=0;
  
  tidy:
  rasqal_xsd_decimal_clear(&a);
  rasqal_xsd_decimal_clear(&b);
  if(result)
     rasqal_free_xsd_decimal(result);
  if(result2)
     rasqal_free_xsd_decimal(result2);

  return rc;
}
#endif

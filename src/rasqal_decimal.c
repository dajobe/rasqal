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


/* FOR rasqal_internal.h */
typedef struct rasqal_xsd_decimal_s rasqal_xsd_decimal;

rasqal_xsd_decimal* rasqal_xsd_decimal_new(void);
void rasqal_xsd_decimal_free(rasqal_xsd_decimal* dec);
void rasqal_xsd_decimal_init(rasqal_xsd_decimal* dec);
void rasqal_xsd_decimal_clear(rasqal_xsd_decimal* dec);
int rasqal_xsd_decimal_set_string(rasqal_xsd_decimal* dec, const char* string);
double rasqal_xsd_decimal_get_double(rasqal_xsd_decimal* dec);
char* rasqal_xsd_decimal_as_string(rasqal_xsd_decimal* dec);
char* rasqal_xsd_decimal_as_counted_string(rasqal_xsd_decimal* dec, size_t* len_p);
int rasqal_xsd_decimal_set_long(rasqal_xsd_decimal* dec, long l);
int rasqal_xsd_decimal_set_double(rasqal_xsd_decimal* dec, double d);
int rasqal_xsd_decimal_print(rasqal_xsd_decimal* dec, FILE* stream);
int rasqal_xsd_decimal_add(rasqal_xsd_decimal* result, rasqal_xsd_decimal* a, rasqal_xsd_decimal* b);
int rasqal_xsd_decimal_subtract(rasqal_xsd_decimal* result, rasqal_xsd_decimal* a, rasqal_xsd_decimal* b);
int rasqal_xsd_decimal_multiply(rasqal_xsd_decimal* result, rasqal_xsd_decimal* a, rasqal_xsd_decimal* b);
int rasqal_xsd_decimal_divide(rasqal_xsd_decimal* result, rasqal_xsd_decimal* a, rasqal_xsd_decimal* b);
int rasqal_xsd_decimal_compare(rasqal_xsd_decimal* a, rasqal_xsd_decimal* b);
int rasqal_xsd_decimal_equal(rasqal_xsd_decimal* a, rasqal_xsd_decimal* b);

/* prototypes */


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

/* for MPFR the string can be from library or MPFR allocator */
#define FLAGS_STRING_ALLOCED_HERE 1

struct rasqal_xsd_decimal_s {
  unsigned int precision_digits;
  unsigned int precision_bits;
  RASQAL_DECIMAL_RAW raw;
  RASQAL_DECIMAL_ROUNDING rounding;
  char* string;
  int flags;
  size_t string_len;
};


rasqal_xsd_decimal*
rasqal_xsd_decimal_new(void)
{
  rasqal_xsd_decimal* dec;
  dec=(rasqal_xsd_decimal*)RASQAL_MALLOC(decimal, sizeof(rasqal_xsd_decimal));
  if(dec)
    rasqal_xsd_decimal_init(dec);
  return dec;
}


void
rasqal_xsd_decimal_free(rasqal_xsd_decimal* dec)
{
  if(dec) {
    rasqal_xsd_decimal_clear(dec);
    RASQAL_FREE(decimal, dec);
  }
}


void
rasqal_xsd_decimal_init(rasqal_xsd_decimal* dec)
{
  /* XSD says:
   [[ Note: All ·minimally conforming· processors ·must· support
   decimal numbers with a minimum of 18 decimal digits (i.e., with a
   ·totalDigits· of 18). However, ·minimally conforming· processors
   ·may· set an application-defined limit on the maximum number of
   decimal digits they are prepared to support, in which case that
   application-defined maximum number ·must· be clearly documented.
   ]] -- http://www.w3.org/TR/2004/REC-xmlschema-2-20041028/#decimal
*/

  dec->precision_bits=256;   /* max bits */
  dec->precision_digits=(dec->precision_bits/4); /* "max" base 10 digits */
  
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
  dec->flags=0;
}


static void
rasqal_xsd_decimal_clear_string(rasqal_xsd_decimal* dec)
{
#ifdef RASQAL_DECIMAL_C99
  if(dec->string) {
    RASQAL_FREE(cstring, dec->string);
    dec->string=NULL;
  }
#endif
#ifdef RASQAL_DECIMAL_MPFR
  if(dec->string) {
    if(dec->flags & FLAGS_STRING_ALLOCED_HERE)
      RASQAL_FREE(cstring, dec->string);
    else
      mpfr_free_str((char*)dec->string);
    dec->string=NULL;
  }
#endif
#ifdef RASQAL_DECIMAL_GMP
  if(dec->string) {
    free((char*)dec->string); /* GMP uses free */
    dec->string=NULL;
  }
#endif
  dec->string_len=0;
  dec->flags=0;
}  


void
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
  dec->flags |= FLAGS_STRING_ALLOCED_HERE;
  
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
  if(mpfr_fits_slong_p(dec->raw, dec->rounding)) {
    /* FIXME - buffer size big enough for max LONG */
    len=15;
    s=RASQAL_MALLOC(cstring, len+1);
    if(!s)
      return NULL;
    snprintf(s, len, "%ld", mpfr_get_si(dec->raw, dec->rounding));
    len=strlen(s);
  } else {
    mp_exp_t expo;
    char *mpf_s;
    mpf_s=mpfr_get_str(NULL, &expo, 10, 0, dec->raw, dec->rounding);
    if(mpf_s) {
      size_t from_len=strlen(mpf_s);
      char *from_p=mpf_s;
      char *to_p;
      int is_zero=0;
      
      s=RASQAL_MALLOC(cstring, from_len*2);
      if(!s) {
        free(mpf_s);
        return NULL;
      }
      to_p=s;
      if(*from_p == '-') {
        *to_p++ = *from_p++;
        from_len--;
      }
      /* first digit of mantissa */
      is_zero=(*from_p == '0');
      *to_p++ = *from_p++;
      from_len--;
      *to_p++ = '.';
      /* rest of mantissa */
      /* remove trailing 0s */
      while(from_len > 1 && from_p[from_len-1]=='0')
        from_len--;
      strncpy(to_p, from_p, from_len);
      to_p+= from_len;
      /* exp */
      sprintf(to_p, "e%ld", is_zero ? expo: expo-1);
      len=strlen(s);
      free(mpf_s);
    }
  }
#endif
#ifdef RASQAL_DECIMAL_GMP
  if(mpf_fits_slong_p(dec->raw)) {
    /* FIXME - buffer size big enough for max LONG */
    len=15;
    s=RASQAL_MALLOC(cstring, len+1);
    if(!s)
      return NULL;
    
    snprintf(s, len, "%ld", mpf_get_si(dec->raw));
    len=strlen(s);
  } else {
    mp_exp_t expo;
    char *mpf_s;
    mpf_s=mpf_get_str(NULL, &expo, 10, 0, dec->raw);
    if(mpf_s) {
      size_t from_len=strlen(mpf_s);
      char *from_p=mpf_s;
      char *to_p;
      int is_zero=0;

      s=RASQAL_MALLOC(cstring, from_len*2);
      if(!s) {
        free(mpf_s);
        return NULL;
      }
      to_p=s;
      if(*from_p == '-') {
        *to_p++ = *from_p++;
        from_len--;
      }
      /* first digit of mantissa */
      is_zero=(*from_p == '0');
      *to_p++ = *from_p++;
      from_len--;
      *to_p++ = '.';
      /* rest of mantissa */
      strncpy(to_p, from_p, from_len);
      to_p+= from_len;
      /* exp */
      sprintf(to_p, "e%ld", is_zero ? expo: expo-1);
      len=strlen(s);
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
rasqal_xsd_decimal_equal(rasqal_xsd_decimal* a, rasqal_xsd_decimal* b)
{
  int rc;
  
#if defined(RASQAL_DECIMAL_C99) || defined(RASQAL_DECIMAL_NONE)
  rc= (result->b == result->a);
#endif
#ifdef RASQAL_DECIMAL_MPFR
  rc=mpfr_equal_p(a->raw, b->raw);
#endif
#ifdef RASQAL_DECIMAL_GMP
  /* NOTE: Not using mpf_eq() but could do, with sufficient bits */
  rc=!mpf_cmp(a->raw, b->raw);
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

  result=rasqal_xsd_decimal_new();
  result2=rasqal_xsd_decimal_new();
  if(!result || !result2) {
    fprintf(stderr, "%s: rasqal_xsd_decimal_new() failed\n", program);
    goto tidy;
  }

  rasqal_xsd_decimal_set_long(&a, 1234567890L);
  rasqal_xsd_decimal_set_string(&b, "1234567890123456789012345678901234567890");

  fprintf(stderr, "a=");
  rasqal_xsd_decimal_print(&a, stderr);
  fprintf(stderr, "\n");

  fprintf(stderr, "b=");
  rasqal_xsd_decimal_print(&b, stderr);
  fprintf(stderr, "\n");

  /* result = a-b */
  rasqal_xsd_decimal_add(result, &a, &b);

  fprintf(stderr, "a+b=");
  rasqal_xsd_decimal_print(result, stderr);
  fprintf(stderr, "\n");

  /* result2 = result-b */
  rasqal_xsd_decimal_subtract(result2, result, &b);

  fprintf(stderr, "(a+b)-b=");
  rasqal_xsd_decimal_print(result2, stderr);
  fprintf(stderr, "\n");

  /* result = result2-a */
  rasqal_xsd_decimal_subtract(result, result2, &a);

  fprintf(stderr, "(a+b)-b-a=");
  rasqal_xsd_decimal_print(result, stderr);
  fprintf(stderr, "\n");

  fprintf(stderr, "a compare b = %d\n", rasqal_xsd_decimal_compare(&a, &b));

  fprintf(stderr, "a equal b = %d\n", rasqal_xsd_decimal_equal(&a, &b));


  rc=0;
  
  tidy:
  rasqal_xsd_decimal_clear(&a);
  rasqal_xsd_decimal_clear(&b);
  if(result)
     rasqal_xsd_decimal_free(result);
  if(result2)
     rasqal_xsd_decimal_free(result2);

  return rc;
}
#endif

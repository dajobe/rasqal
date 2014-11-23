/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_expr_numerics.c - Rasqal expression evaluation
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
#include <ctype.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdarg.h>

#include "rasqal.h"
#include "rasqal_internal.h"

#if defined(RASQAL_UUID_OSSP)
#include <uuid.h>
#endif
#if defined(RASQAL_UUID_LIBUUID)
#include <uuid.h>
#endif
#ifdef RASQAL_UUID_LIBC
#include <uuid/uuid.h>
#endif

#define DEBUG_FH stderr


/* 
 * rasqal_expression_evaluate_abs:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate SPARQL 1.1 RASQAL_EXPR_ABS (numeric) expression.
 *
 * Return value: A #rasqal_literal value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_abs(rasqal_expression *e,
                               rasqal_evaluation_context *eval_context,
                               int *error_p)
{
  rasqal_literal* l1;
  rasqal_literal* result = NULL;

  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if((error_p && *error_p) || !l1)
    goto failed;
  
  if(!rasqal_literal_is_numeric(l1))
    goto failed;

  result = rasqal_literal_abs(l1, error_p);
  rasqal_free_literal(l1);
  l1 = NULL;
  
  if(error_p && *error_p)
    goto failed;

  return result;

  failed:
  if(error_p)
    *error_p = 1;
  
  if(l1)
    rasqal_free_literal(l1);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_round:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate SPARQL 1.1 RASQAL_EXPR_ROUND (numeric) expression.
 *
 * Return value: A #rasqal_literal value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_round(rasqal_expression *e,
                                 rasqal_evaluation_context *eval_context,
                                 int *error_p)
{
  rasqal_literal* l1;
  rasqal_literal* result = NULL;

  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if((error_p && *error_p) || !l1)
    goto failed;
  
  if(!rasqal_literal_is_numeric(l1))
    goto failed;

  result = rasqal_literal_round(l1, error_p);
  rasqal_free_literal(l1);
  l1 = NULL;
  
  if(error_p && *error_p)
    goto failed;

  return result;

  failed:
  if(error_p)
    *error_p = 1;
  
  if(l1)
    rasqal_free_literal(l1);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_ceil:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate SPARQL 1.1 RASQAL_EXPR_CEIL (numeric) expression.
 *
 * Return value: A #rasqal_literal value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_ceil(rasqal_expression *e,
                                rasqal_evaluation_context *eval_context,
                                int *error_p)
{
  rasqal_literal* l1;
  rasqal_literal* result = NULL;

  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if((error_p && *error_p) || !l1)
    goto failed;
  
  if(!rasqal_literal_is_numeric(l1))
    goto failed;

  result = rasqal_literal_ceil(l1, error_p);
  rasqal_free_literal(l1);
  l1 = NULL;
  
  if(error_p && *error_p)
    goto failed;

  return result;

  failed:
  if(error_p)
    *error_p = 1;
  
  if(l1)
    rasqal_free_literal(l1);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_floor:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate SPARQL 1.1 RASQAL_EXPR_FLOOR (numeric) expression.
 *
 * Return value: A #rasqal_literal value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_floor(rasqal_expression *e,
                                rasqal_evaluation_context *eval_context,
                                int *error_p)
{
  rasqal_literal* l1;
  rasqal_literal* result = NULL;

  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if((error_p && *error_p) || !l1)
    goto failed;
  
  if(!rasqal_literal_is_numeric(l1))
    goto failed;

  result = rasqal_literal_floor(l1, error_p);
  rasqal_free_literal(l1);
  l1 = NULL;
  
  if(error_p && *error_p)
    goto failed;

  return result;

  failed:
  if(error_p)
    *error_p = 1;
  
  if(l1)
    rasqal_free_literal(l1);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_rand:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate SPARQL 1.1 RASQAL_EXPR_RAND (integer expr) expression.
 *
 * Return value: A #rasqal_literal xsd:double value in range [0, 1) or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_rand(rasqal_expression *e,
                                rasqal_evaluation_context *eval_context,
                                int *error_p)
{
  rasqal_world* world = eval_context->world;
  double d;
  
  d = rasqal_random_drand(eval_context->random);

  return rasqal_new_double_literal(world, d);
}


/* 
 * rasqal_expression_evaluate_digest:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 * @error_p: pointer to error flag or NULL
 *
 * INTERNAL - Evaluate SPARQL 1.1 RASQAL_EXPR_MD5, RASQAL_EXPR_SHA1,
 * RASQAL_EXPR_SHA224, RASQAL_EXPR_SHA256, RASQAL_EXPR_SHA384,
 * RASQAL_EXPR_SHA512 (string) expression.
 *
 * Return value: A #rasqal_literal xsd:string value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_digest(rasqal_expression *e,
                                  rasqal_evaluation_context *eval_context,
                                  int *error_p)
{
  rasqal_world* world = eval_context->world;
  rasqal_digest_type md_type = RASQAL_DIGEST_NONE;
  rasqal_literal* l1 = NULL;
  const unsigned char *s;
  unsigned char *new_s;
  size_t len;
  int output_len;
  unsigned char *output = NULL;
  unsigned int i;
  unsigned char* p;
  
  /* Turn EXPR enum into DIGEST enum - we know they are ordered the same */
  if(e->op >= RASQAL_EXPR_MD5 && e->op <= RASQAL_EXPR_SHA512)
    md_type = RASQAL_GOOD_CAST(rasqal_digest_type, e->op - RASQAL_EXPR_MD5 + RASQAL_DIGEST_MD5);
  else
    goto failed;

  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if((error_p && *error_p) || !l1)
    goto failed;
  
  s = rasqal_literal_as_counted_string(l1, &len, eval_context->flags, error_p);
  if(error_p && *error_p)
    goto failed;

  output_len = rasqal_digest_buffer(md_type, NULL, NULL, 0);
  if(output_len < 0)
    goto failed;

  output = RASQAL_MALLOC(unsigned char*, RASQAL_GOOD_CAST(size_t, output_len));
  if(!output)
    goto failed;
  
  output_len = rasqal_digest_buffer(md_type, output, s, len);
  if(output_len < 0)
    goto failed;

  new_s = RASQAL_MALLOC(unsigned char*, (RASQAL_GOOD_CAST(size_t, output_len) * 2) + 1);
  if(!new_s)
    goto failed;
  
  p = new_s;
  for(i = 0; i < RASQAL_GOOD_CAST(unsigned int, output_len); i++) {
    unsigned short hex;
    unsigned char c = output[i];

    hex = (c & 0xf0) >> 4;
    *p++ = RASQAL_GOOD_CAST(unsigned char, (hex < 10) ? ('0' + hex) : ('a' + hex - 10));
    hex = (c & 0x0f);
    *p++ = RASQAL_GOOD_CAST(unsigned char, (hex < 10) ? ('0' + hex) : ('a' + hex - 10));
  }
  *p = '\0';

  RASQAL_FREE(char, output);
  rasqal_free_literal(l1);

  /* after this new_s becomes owned by result */
  return rasqal_new_string_literal(world, new_s, NULL, NULL, NULL);
  
  failed:
  if(error_p)
    *error_p = 1;

  if(output)
    RASQAL_FREE(char, output);
  if(l1)
    rasqal_free_literal(l1);
  
  return NULL;
}


#define RASQAL_UUID_LEN 16
#define RASQAL_UUID_HEXDIGIT_LEN (RASQAL_UUID_LEN << 1)
/* 4 '-' chars added after 8, 12, 16, 20 output hex digit */
#define RASQAL_UUID_STRING_LEN (RASQAL_UUID_HEXDIGIT_LEN + 4)
#define RASQAL_UUID_URI_PREFIX "urn:uuid:"
#define RASQAL_UUID_URI_PREFIX_LEN 9
  

#ifdef RASQAL_UUID_INTERNAL
typedef union {
  unsigned char b[16];
  int16_t w[8];
} uuid_t;

/*
 * rasqal_uuid_generate:
 * @eval_context: evaluation context
 * @uuid: uuid object
 *
 * INTERNAL - Generate a random UUID based on rasqal random
 *
 * Byte offset
 *  0 1 2 3  4 5  6 7  8 9 101112131415
 * xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx   Hex string
 *
 * where x is any hexadecimal digit and y is one of 8, 9, a, or b.
 */
static void
rasqal_uuid_generate(rasqal_evaluation_context *eval_context, uuid_t ptr)
{
  int16_t* out = ptr.w;
  unsigned char* outc = ptr.b;
  unsigned int i;
  for(i = 0; i < (RASQAL_UUID_LEN / sizeof(int16_t)); i++) {
    *out++ = rasqal_random_irand(eval_context->random);
  }

  outc[6] = (outc[6] & 0x0F) | 0x40;
  outc[8] = (outc[8] & 0x3F) | 0x80;
}
#endif

/* 
 * rasqal_expression_evaluate_uuid:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 * @want_uri: non-0 to return URI otherwise string
 *
 * INTERNAL - Evaluate SPARQL 1.1 RASQAL_EXPR_UUID, RASQAL_EXPR_STRUUID
 *
 * Return value: A #rasqal_literal URI / string value or NULL on failure.
 */
static rasqal_literal*
rasqal_expression_evaluate_uuid(rasqal_expression *e,
                                rasqal_evaluation_context *eval_context,
                                int *error_p,
                                int want_uri)
{
#ifdef RASQAL_UUID_NONE
  return NULL;

#else

  rasqal_world* world = eval_context->world;
#if defined(RASQAL_UUID_OSSP)
  uuid_t* data;
#else
  uuid_t data; /* static */
  int i;
#endif
  size_t output_len = RASQAL_UUID_STRING_LEN;
  unsigned char* output;
  unsigned char* p;

#if defined(RASQAL_UUID_LIBUUID) || defined(RASQAL_UUID_LIBC)
  uuid_generate(data);
#endif
#if defined(RASQAL_UUID_OSSP)
  uuid_create(&data);
  uuid_make(data, UUID_MAKE_V1);
#endif
#ifdef RASQAL_UUID_INTERNAL
  rasqal_uuid_generate(eval_context, data);
#endif

  if(want_uri)
    output_len += RASQAL_UUID_URI_PREFIX_LEN;

  output = RASQAL_MALLOC(unsigned char*, output_len + 1);
  if(!output) {
#if defined(RASQAL_UUID_OSSP)
    uuid_destroy(data);
#endif
    return NULL;
  }

  p = output;
  if(want_uri) {
    memcpy(p, RASQAL_UUID_URI_PREFIX, RASQAL_UUID_URI_PREFIX_LEN);
    p += RASQAL_UUID_URI_PREFIX_LEN;
  }

#if defined(RASQAL_UUID_OSSP)
  uuid_export(data, UUID_FMT_STR, p, /* data_len */ NULL);
  uuid_destroy(data);
#else
  for(i = 0; i < RASQAL_UUID_LEN; i++) {
    unsigned short hex;
#ifdef RASQAL_UUID_INTERNAL
    unsigned char c = data.b[i];
#else
    unsigned char c = data[i];
#endif

    hex = (c & 0xf0) >> 4;
    *p++ = RASQAL_GOOD_CAST(unsigned char, (hex < 10) ? ('0' + hex) : ('a' + hex - 10));
    hex = (c & 0x0f);
    *p++ = RASQAL_GOOD_CAST(unsigned char, (hex < 10) ? ('0' + hex) : ('a' + hex - 10));
    if(i == 3 || i == 5 || i == 7 || i == 9)
      *p++ = '-';
  }
  *p = '\0';
#endif /* end if !RASQAL_UUID_OSSP */

  /* after this output becomes owned by result */
  if(want_uri) {
    raptor_uri* u;
    rasqal_literal* l = NULL;

    u = raptor_new_uri(world->raptor_world_ptr, output);
    if(u)
      l = rasqal_new_uri_literal(world, u);

    RASQAL_FREE(char*, output);
    return l;
  } else {
    return rasqal_new_string_literal(world, output, NULL, NULL, NULL);
  }
#endif
}


/* 
 * rasqal_expression_evaluate_uriuuid:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 * @want_uri: non-0 to return URI otherwise string
 *
 * INTERNAL - Evaluate SPARQL 1.1 RASQAL_EXPR_UUID
 *
 * Return value: A #rasqal_literal URI value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_uriuuid(rasqal_expression *e,
                                   rasqal_evaluation_context *eval_context,
                                   int *error_p)
{
  return rasqal_expression_evaluate_uuid(e, eval_context, error_p, 1);
}

/* 
 * rasqal_expression_evaluate_uuid:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 * @want_uri: non-0 to return URI otherwise string
 *
 * INTERNAL - Evaluate SPARQL 1.1 RASQAL_EXPR_STRUUID
 *
 * Return value: A #rasqal_literal string value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_struuid(rasqal_expression *e,
                                   rasqal_evaluation_context *eval_context,
                                   int *error_p)
{
  return rasqal_expression_evaluate_uuid(e, eval_context, error_p, 0);
}


/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_expr_strings.c - Rasqal string expression functions
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

#ifdef RASQAL_REGEX_PCRE
#include <pcre.h>
#endif

#ifdef RASQAL_REGEX_POSIX
#include <sys/types.h>
#include <regex.h>
#endif

#include "rasqal.h"
#include "rasqal_internal.h"


#define DEBUG_FH stderr


#if RAPTOR_VERSION < 20005

static int
rasqal_unicode_utf8_strlen(const unsigned char *string, size_t length)
{
  size_t unicode_length = 0;
  
  while(length > 0) {
    int unichar_len;
    unichar_len = raptor_unicode_utf8_string_get_char(string, length, NULL);
    if(unichar_len < 0 || unichar_len > (int)length) {
      unicode_length = -1;
      break;
    }
    
    string += unichar_len;
    length -= unichar_len;

    unicode_length++;
  }

  return (int)unicode_length;
}


/*
 * rasqal_unicode_utf8_substr:
 * @dest: destination string buffer to write to (or NULL)
 * @dest_length_p: location to store actual destination length (or NULL)
 * @src: source string
 * @src_length: source length in bytes
 * @startingLoc: starting location offset 0 for first Unicode character
 * @length: number of Unicode characters to copy at offset @startingLoc (or < 0)
 *
 * INTERNAL - Get a unicode (UTF-8) substring of an existing UTF-8 string
 *
 * If @dest is NULL, returns the number of bytes needed to write and
 * does no work.
 * 
 * Return value: number of bytes used in destination string or 0 on failure
 */
static size_t
rasqal_unicode_utf8_substr(unsigned char* dest, size_t* dest_length_p,
                           const unsigned char* src, size_t src_length,
                           int startingLoc, int length)
{
  size_t dest_length = 0; /* destination unicode characters count */
  size_t dest_bytes = 0;  /* destination UTF-8 bytes count */
  int dest_offset = 0; /* destination string unicode characters index */
  unsigned char* p = dest;
  
  if(!src)
    return 0;

  while(src_length > 0) {
    int unichar_len;

    unichar_len = raptor_unicode_utf8_string_get_char(src, src_length, NULL);
    if(unichar_len < 0 || unichar_len > (int)src_length)
      break;

    if(dest_offset >= startingLoc) {
      if(p) {
        /* copy 1 Unicode character to dest */
        memcpy(p, src, unichar_len);
        p += unichar_len;
      }
      dest_bytes += unichar_len;

      dest_length++;
      if(length >= 0 && (int)dest_length == length)
        break;
    }

    src += unichar_len;
    src_length -= unichar_len;

    dest_offset++;
  }

  if(p)
    *p = '\0';

  if(dest_length_p)
    *dest_length_p = dest_length;

  return dest_bytes;
}

#define raptor_unicode_utf8_strlen rasqal_unicode_utf8_strlen
#define raptor_unicode_utf8_substr rasqal_unicode_utf8_substr

#endif


/* 
 * rasqal_expression_evaluate_strlen:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate RASQAL_EXPR_STRLEN(expr) expression.
 *
 * Return value: A #rasqal_literal integer value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_strlen(rasqal_expression *e,
                                  rasqal_evaluation_context *eval_context,
                                  int *error_p)
{
  rasqal_world* world = eval_context->world;
  rasqal_literal* l1;
  rasqal_literal* result = NULL;
  const unsigned char *s;
  int len = 0;
  
  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if(*error_p || !l1)
    goto failed;
  
  s = rasqal_literal_as_string_flags(l1, eval_context->flags, error_p);
  if(*error_p)
    goto failed;

  if(!s)
    len = 0;
  else
    len = raptor_unicode_utf8_strlen(s, strlen((const char*)s));
  

  result = rasqal_new_numeric_literal_from_long(world, RASQAL_LITERAL_INTEGER,
                                                len);
  rasqal_free_literal(l1);
  return result;

  failed:
  if(error_p)
    *error_p = 1;
  
  if(l1)
    rasqal_free_literal(l1);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_substr:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate RASQAL_EXPR_SUBSTR(expr) expression.
 *
 * Return value: A #rasqal_literal integer value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_substr(rasqal_expression *e,
                                  rasqal_evaluation_context *eval_context,
                                  int *error_p)
{
  rasqal_world* world = eval_context->world;
  rasqal_literal* l1 = NULL;
  rasqal_literal* l2 = NULL;
  rasqal_literal* l3 = NULL;
  const unsigned char *s;
  unsigned char* new_s = NULL;
  char* new_lang = NULL;
  raptor_uri* dt_uri = NULL;
  size_t len = 0;
  int startingLoc = 0;
  int length = -1;
  
  /* haystack string */
  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if(*error_p || !l1)
    goto failed;
  
  s = rasqal_literal_as_counted_string(l1, &len, eval_context->flags, error_p);
  if(*error_p)
    goto failed;

  /* integer startingLoc */
  l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
  if(*error_p || !l2)
    goto failed;
  
  startingLoc = rasqal_literal_as_integer(l2, error_p);
  if(*error_p)
    goto failed;

  /* optional integer length */
  if(e->arg3) {
    l3 = rasqal_expression_evaluate2(e->arg3, eval_context, error_p);
    if(!l3)
      goto failed;

    length = rasqal_literal_as_integer(l3, error_p);
    if(*error_p)
      goto failed;

  }
  
  new_s = RASQAL_MALLOC(unsigned char*, len + 1);
  if(!new_s)
    goto failed;

  /* adjust starting index to xsd fn:substring initial offset 1 */
  if(!raptor_unicode_utf8_substr(new_s, /* dest_length_p */ NULL,
                                 s, len, startingLoc - 1, length))
    goto failed;

  if(l1->language) {
    len = strlen((const char*)l1->language);
    new_lang = RASQAL_MALLOC(char*, len + 1);
    if(!new_lang)
      goto failed;

    memcpy(new_lang, l1->language, len + 1);
  }

  dt_uri = l1->datatype;
  if(dt_uri)
    dt_uri = raptor_uri_copy(dt_uri);

  rasqal_free_literal(l1);
  rasqal_free_literal(l2);
  if(l3)
    rasqal_free_literal(l3);

  /* after this new_s, new_lang and dt_uri become owned by result */
  return rasqal_new_string_literal(world, new_s, new_lang, dt_uri,
                                   /* qname */ NULL);
  


  failed:
  if(error_p)
    *error_p = 1;
  
  if(l1)
    rasqal_free_literal(l1);
  if(l2)
    rasqal_free_literal(l2);
  if(l3)
    rasqal_free_literal(l3);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_set_case:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate RASQAL_EXPR_UCASE(expr) or
 * RASQAL_EXPR_LCASE(expr) expressions.
 *
 * Return value: A #rasqal_literal string value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_set_case(rasqal_expression *e,
                                    rasqal_evaluation_context *eval_context,
                                    int *error_p)
{
  rasqal_world* world = eval_context->world;
  rasqal_literal* l1;
  const unsigned char *s;
  unsigned char* new_s = NULL;
  char* new_lang = NULL;
  raptor_uri* dt_uri = NULL;
  size_t len = 0;
  
  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if(*error_p || !l1)
    goto failed;
  
  s = rasqal_literal_as_counted_string(l1, &len, eval_context->flags, error_p);
  if(*error_p)
    goto failed;

  new_s =RASQAL_MALLOC(unsigned char*, len + 1);
  if(!new_s)
    goto failed;

  if(e->op == RASQAL_EXPR_UCASE) {
    unsigned int i;
    
    for(i = 0; i < len; i++) {
      char c = s[i];
      if(islower((int)c))
        c = toupper((int)c);
      new_s[i] = c;
    }
  } else { /* RASQAL_EXPR_LCASE */
    unsigned int i;

    for(i = 0; i < len; i++) {
      char c = s[i];
      if(isupper((int)c))
        c = tolower((int)c);
      new_s[i] = c;
    }
  }
  new_s[len] = '\0';

  if(l1->language) {
    len = strlen((const char*)l1->language);
    new_lang = RASQAL_MALLOC(char*, len + 1);
    if(!new_lang)
      goto failed;

    memcpy(new_lang, l1->language, len + 1);
  }

  dt_uri = l1->datatype;
  if(dt_uri)
    dt_uri = raptor_uri_copy(dt_uri);

  rasqal_free_literal(l1);

  /* after this new_s, new_lang and dt_uri become owned by result */
  return rasqal_new_string_literal(world, new_s, new_lang, dt_uri,
                                   /* qname */ NULL);
  
  
  failed:
  if(error_p)
    *error_p = 1;
  
  if(new_s)
    RASQAL_FREE(char*, new_s);
  if(new_lang)
    RASQAL_FREE(char*, new_lang);
  if(l1)
    rasqal_free_literal(l1);

  return NULL;
}


/*
 * rasqal_literals_sparql11_compatible:
 * @l1: first literal
 * @l2: second  literal
 *
 * INTERNAL - Check if two literals are SPARQL 1.1 compatible such as usable for STRSTARTS()
 *
 * From STRSTARTS(), STRENDS() and CONTAINS() draft definition:
 * 1. pairs of simple literals,
 * 2. pairs of xsd:string typed literals
 * 3. pairs of plain literals with identical language tags
 * 4. pairs of an xsd:string typed literal (arg1 or arg2) and a simple literal (arg2 or arg1)
 * 5. pairs of a plain literal with language tag (arg1) and a simple literal (arg2)
 * 6. pairs of a plain literal with language tag (arg1) and an xsd:string typed literal (arg2)
 *
 * Return value: non-0 if literals are compatible
 */
static int
rasqal_literals_sparql11_compatible(rasqal_literal *l1, rasqal_literal *l2) 
{
  raptor_uri* dt1;
  raptor_uri* dt2;
  const char *lang1;
  const char *lang2;
  raptor_uri* xsd_string_uri;
  
  xsd_string_uri = rasqal_xsd_datatype_type_to_uri(l1->world, 
                                                   RASQAL_LITERAL_XSD_STRING);

  /* Languages */
  lang1 = l1->language;
  lang2 = l2->language;

  /* Turn xsd:string datatypes into plain literals for compatibility
   * purposes 
   */
  dt1 = l1->datatype;
  if(dt1 && raptor_uri_equals(dt1, xsd_string_uri))
    dt1 = NULL;
  
  dt2 = l2->datatype;
  if(dt2 && raptor_uri_equals(dt2, xsd_string_uri))
    dt2 = NULL;
  
  /* If there any datatypes left, the literals are not compatible */
  if(dt1 || dt2)
    return 0;
  
  /* pairs of simple literals (or pairs of xsd:string or mixtures): #1, #2, #4 */
  if(!lang1 && !lang2)
    return 1;

  /* pairs of plain literals with identical language tags #3 */
  if(lang1 && lang2)
    return !strcmp(lang1, lang2);

  /* pairs of a plain literal with language tag (arg1) and a simple
   * literal or xsd:string typed literal [with no language tag] (arg2) #5, #6
   */
  return (lang1 && !lang2);
}


/* 
 * rasqal_expression_evaluate_str_prefix_suffix:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate RASQAL_EXPR_STRSTARTS(lit, lit) and
 * RASQAL_EXPR_STRENDS(lit, lit) expressions.
 *
 * Return value: A #rasqal_literal integer value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_str_prefix_suffix(rasqal_expression *e,
                                             rasqal_evaluation_context *eval_context,
                                             int *error_p)
{
  rasqal_world* world = eval_context->world;
  rasqal_literal *l1 = NULL;
  rasqal_literal *l2 = NULL;
  int b;
  const unsigned char *s1;
  const unsigned char *s2;
  size_t len1 = 0;
  size_t len2 = 0;
  
  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if(*error_p || !l1)
    goto failed;
  
  l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
  if(*error_p || !l2)
    goto failed;

  if(!rasqal_literals_sparql11_compatible(l1, l2))
    goto failed;
  
  s1 = rasqal_literal_as_counted_string(l1, &len1, eval_context->flags, error_p);
  if(*error_p)
    goto failed;
  
  s2 = rasqal_literal_as_counted_string(l2, &len2, eval_context->flags, error_p);
  if(*error_p)
    goto failed;

  if(len1 < len2) {
    /* s1 is shorter than s2 so s2 can never be a prefix, suffix or
     * contain s1 */
    b = 0;
  } else {
    if(e->op == RASQAL_EXPR_STRSTARTS) {
      b = !memcmp(s1, s2, len2);
    } else if(e->op == RASQAL_EXPR_STRENDS) {
      b = !memcmp(s1 + len1 - len2, s2, len2);
    } else { /* RASQAL_EXPR_CONTAINS */
      /* b = (strnstr((const char*)s1, (const char*)s2, len2) != NULL); */
      b = (strstr((const char*)s1, (const char*)s2) != NULL);
    }
  }
  
  
  
  rasqal_free_literal(l1);
  rasqal_free_literal(l2);

  return rasqal_new_boolean_literal(world, b);

  failed:
  if(error_p)
    *error_p = 1;
  
  if(l1)
    rasqal_free_literal(l1);
  if(l2)
    rasqal_free_literal(l2);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_encode_for_uri:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate RASQAL_EXPR_ENCODE_FOR_URI(string) expression.
 *
 * Return value: A #rasqal_literal string value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_encode_for_uri(rasqal_expression *e,
                                          rasqal_evaluation_context *eval_context,
                                          int *error_p)
{
  rasqal_world* world = eval_context->world;
  rasqal_literal* l1;
  raptor_uri* xsd_string_uri;
  const unsigned char *s;
  unsigned char* new_s = NULL;
  raptor_uri* dt_uri = NULL;
  size_t len = 0;
  unsigned int i;
  unsigned char* p;

  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if(*error_p || !l1)
    goto failed;
  
  xsd_string_uri = rasqal_xsd_datatype_type_to_uri(l1->world, 
                                                   RASQAL_LITERAL_XSD_STRING);

  dt_uri = l1->datatype;
  if(dt_uri && !raptor_uri_equals(dt_uri, xsd_string_uri))
    /* datatype and not xsd:string */
    goto failed;

  s = rasqal_literal_as_counted_string(l1, &len, eval_context->flags, error_p);
  if(*error_p)
    goto failed;

  /* pessimistically assume every UTF-8 byte is %XX 3 x len */
  new_s = RASQAL_MALLOC(unsigned char*, (3 * len) + 1);
  if(!new_s)
    goto failed;

  p = new_s;
  for(i = 0; i < len; i++) {
    unsigned char c = s[i];

    /* All characters are escaped except those identified as
     * "unreserved" by [RFC 3986], that is the upper- and lower-case
     * letters A-Z, the digits 0-9, HYPHEN-MINUS ("-"), LOW LINE
     * ("_"), FULL STOP ".", and TILDE "~".
     */
    if((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
       (c >= '0' && c <= '9') || 
       c == '-' || c == '_' || c == '.' || c == '~') {
      *p++ = c;
    } else {
      unsigned short hex;

      *p++ = '%';
      hex = (c & 0xf0) >> 4;
      *p++ = (hex < 10) ? ('0' + hex) : ('A' + hex - 10);
      hex = (c & 0x0f);
      *p++ = (hex < 10) ? ('0' + hex) : ('A' + hex - 10);
    }
  }

  *p = '\0';

  rasqal_free_literal(l1);

  /* after this new_s, new_lang and dt_uri become owned by result */
  return rasqal_new_string_literal(world, new_s, NULL, NULL,
                                   /* qname */ NULL);
  

  failed:
  if(error_p)
    *error_p = 1;
  
  if(new_s)
    RASQAL_FREE(char*, new_s);
  if(l1)
    rasqal_free_literal(l1);

  return NULL;
  
}


/* 
 * rasqal_expression_evaluate_concat:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate RASQAL_EXPR_CONCAT(expr list) expression.
 *
 * Return value: A #rasqal_literal string value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_concat(rasqal_expression *e,
                                  rasqal_evaluation_context *eval_context,
                                  int *error_p)
{
  rasqal_world* world = eval_context->world;
  raptor_stringbuffer* sb = NULL;
  int i;
  size_t len;
  unsigned char* result_str = NULL;
  rasqal_literal* result = NULL;
  int same_dt = 1;
  raptor_uri* dt = NULL;
  
  sb = raptor_new_stringbuffer();
  if(!sb)
    goto failed;
  
  for(i = 0; i < raptor_sequence_size(e->args); i++) {
    rasqal_expression *arg_expr;
    rasqal_literal* arg_literal;
    const unsigned char* s = NULL;
    
    arg_expr = (rasqal_expression*)raptor_sequence_get_at(e->args, i);
    if(!arg_expr)
      break;

    /* FIXME - check what to do with a NULL literal */
    /* FIXME - check what to do with an empty string literal */

    arg_literal = rasqal_expression_evaluate2(arg_expr, eval_context, error_p);
    if(arg_literal) {

      if(!dt)
        /* First datatype URI seen is the result datatype */
        dt = raptor_uri_copy(arg_literal->datatype);
      else {
        /* Otherwise if all same so far, check the datatype URI for
         * this literal is also the same 
         */
        if(same_dt && !raptor_uri_equals(dt, arg_literal->datatype)) {
          /* Seen a difference - tidy up */
          if(dt) {
            raptor_free_uri(dt);
            dt = NULL;
          }
          same_dt = 0;
        }
      }
      
      /* FIXME - check that altering the flags this way to allow
       * concat of URIs is OK 
       */
      s = rasqal_literal_as_string_flags(arg_literal, 
                                         (eval_context->flags & ~RASQAL_COMPARE_XQUERY), 
                                         error_p);
      rasqal_free_literal(arg_literal);
    } else
      *error_p = 1;

    if(!s || *error_p)
      goto failed;
    
    raptor_stringbuffer_append_string(sb, s, 1); 
  }
  
  
  len = raptor_stringbuffer_length(sb);
  result_str = RASQAL_MALLOC(unsigned char*, len + 1);
  if(!result_str)
    goto failed;
  
  if(raptor_stringbuffer_copy_to_string(sb, result_str, len))
    goto failed;
  
  raptor_free_stringbuffer(sb);
  
  /* result_str and dt (if set) becomes owned by result */
  return rasqal_new_string_literal(world, result_str, NULL, dt, NULL);


  failed:
  if(error_p)
    *error_p = 1;
  
  if(dt)
    raptor_free_uri(dt);
  if(result_str)
    RASQAL_FREE(char*, result_str);
  if(sb)
    raptor_free_stringbuffer(sb);
  if(result)
    rasqal_free_literal(result);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_langmatches:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate RASQAL_EXPR_LANGMATCHES(lang tag, lang tag range) expression.
 *
 * Return value: A #rasqal_literal boolean value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_langmatches(rasqal_expression *e,
                                       rasqal_evaluation_context *eval_context,
                                       int *error_p)
{
  rasqal_world* world = eval_context->world;
  rasqal_literal *l1 = NULL;
  rasqal_literal *l2 = NULL;
  const unsigned char *tag;
  const unsigned char *range;
  int b;
  
  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if(*error_p || !l1)
    goto failed;
  
  l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
  if(*error_p || !l2)
    goto failed;
  
  tag = rasqal_literal_as_string_flags(l1, eval_context->flags, error_p);
  if(*error_p)
    goto failed;
  
  range = rasqal_literal_as_string_flags(l2, eval_context->flags, error_p);
  if(*error_p)
    goto failed;
  
  
  b = rasqal_language_matches(tag, range);
  
  rasqal_free_literal(l1);
  rasqal_free_literal(l2);
  
  return rasqal_new_boolean_literal(world, b);

  failed:
  if(error_p)
    *error_p = 1;
  
  if(l1)
    rasqal_free_literal(l1);
  if(l2)
    rasqal_free_literal(l2);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_strmatch:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate RASQAL_EXPR_STR_MATCH, RASQAL_EXPR_STR_NMATCH and
 * RASQAL_EXPR_REGEX expressions.
 *
 * Return value: A #rasqal_literal value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_strmatch(rasqal_expression *e,
                                    rasqal_evaluation_context *eval_context,
                                    int *error_p)
{
  rasqal_world* world = eval_context->world;
  int b=0;
  int flag_i=0; /* flags contains i */
  const unsigned char *p;
  const unsigned char *match_string;
  const unsigned char *pattern;
  const unsigned char *regex_flags;
  rasqal_literal *l1, *l2, *l3;
  int rc=0;
#ifdef RASQAL_REGEX_PCRE
  pcre* re;
  int options = 0;
  const char *re_error = NULL;
  int erroffset = 0;
#endif
#ifdef RASQAL_REGEX_POSIX
  regex_t reg;
  int options = REG_EXTENDED | REG_NOSUB;
#endif
  size_t match_len;
    
  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if(*error_p || !l1)
    goto failed;

  match_string = rasqal_literal_as_counted_string(l1, &match_len, 
                                                  eval_context->flags, error_p);
  if(*error_p || !match_string) {
    rasqal_free_literal(l1);
    goto failed;
  }
    
  l3=NULL;
  regex_flags=NULL;
  if(e->op == RASQAL_EXPR_REGEX) {
    l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
    if(*error_p || !l2) {
      rasqal_free_literal(l1);
      goto failed;
    }

    if(e->arg3) {
      l3 = rasqal_expression_evaluate2(e->arg3, eval_context, error_p);
      if(*error_p || !l3) {
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        goto failed;
      }
      regex_flags=l3->string;
    }
      
  } else {
    l2=e->literal;
    regex_flags=l2->flags;
  }
  pattern=l2->string;
    
  for(p=regex_flags; p && *p; p++)
    if(*p == 'i')
      flag_i++;
      
#ifdef RASQAL_REGEX_PCRE
  if(flag_i)
    options |= PCRE_CASELESS;
    
  re=pcre_compile((const char*)pattern, options, 
                  &re_error, &erroffset, NULL);
  if(!re) {
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, eval_context->locator,
                            "Regex compile of '%s' failed - %s", pattern, re_error);
    rc= -1;
  } else {
    rc = pcre_exec(re, 
                   NULL, /* no study */
                   (const char*)match_string, (int)match_len,
                   0 /* startoffset */,
                   options /* options */,
                   NULL, 0 /* ovector, ovecsize - no matches wanted */
                   );
    if(rc >= 0)
      b=1;
    else if(rc != PCRE_ERROR_NOMATCH) {
      rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, eval_context->locator,
                              "Regex match failed - returned code %d", rc);
      rc= -1;
    } else
      rc=0;
  }
  pcre_free(re);
  
#endif
    
#ifdef RASQAL_REGEX_POSIX
  if(flag_i)
    options |=REG_ICASE;
    
  rc=regcomp(&reg, (const char*)pattern, options);
  if(rc) {
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR,
                            eval_context->locator,
                            "Regex compile of '%s' failed", pattern);
    rc= -1;
  } else {
    rc=regexec(&reg, (const char*)match_string, 
               0, NULL, /* nmatch, regmatch_t pmatch[] - no matches wanted */
               options /* eflags */
               );
    if(!rc)
      b=1;
    else if (rc != REG_NOMATCH) {
      rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR,
                              eval_context->locator,
                              "Regex match failed - returned code %d", rc);
      rc= -1;
    } else
      rc= 0;
  }
  regfree(&reg);
#endif

#ifdef RASQAL_REGEX_NONE
  rasqal_log_warning_simple(world, RASQAL_WARNING_LEVEL_MISSING_SUPPORT,
                            eval_context->locator,
                            "Regex support missing, cannot compare '%s' to '%s'", match_string, pattern);
  b=1;
  rc= -1;
#endif

  RASQAL_DEBUG5("regex match returned %s for '%s' against '%s' (flags=%s)\n", b ? "true" : "false", match_string, pattern, l2->flags ? (char*)l2->flags : "");
  
  if(e->op == RASQAL_EXPR_STR_NMATCH)
    b=1-b;

  rasqal_free_literal(l1);
  if(e->op == RASQAL_EXPR_REGEX) {
    rasqal_free_literal(l2);
    if(l3)
      rasqal_free_literal(l3);
  }
    
  if(rc<0)
    goto failed;
    
  return rasqal_new_boolean_literal(world, b);

  failed:
  if(error_p)
    *error_p = 1;
  
  return NULL;
}


/*
 * rasqal_expression_evaluate_strbefore:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate RASQAL_EXPR_STRBEFORE(string, needle) expression.
 *
 * Return value: A #rasqal_literal string value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_strbefore(rasqal_expression *e,
                                     rasqal_evaluation_context *eval_context,
                                     int *error_p)
{
  rasqal_world* world = eval_context->world;
  rasqal_literal* l1 = NULL;
  rasqal_literal* l2 = NULL;
  const unsigned char *haystack;
  const unsigned char *needle;
  size_t haystack_len;
  size_t needle_len;
  char *ptr;
  unsigned char* result;
  size_t result_len;

  /* haystack string */
  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if(*error_p || !l1)
    goto failed;
  
  /* needle string */
  l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
  if(*error_p || !l2)
    goto failed;


  haystack = rasqal_literal_as_counted_string(l1, &haystack_len, 
                                              eval_context->flags, error_p);
  if(*error_p || !haystack)
    goto failed;
    
  needle = rasqal_literal_as_counted_string(l2, &needle_len, 
                                            eval_context->flags, error_p);
  if(*error_p || !needle)
    goto failed;

  ptr = strstr((const char*)haystack, (const char*)needle);
  if(ptr) {
    result_len = ptr - (const char*)haystack;
  } else {
    result_len = 0;
    haystack = (const unsigned char *)"";
  }

  result = RASQAL_MALLOC(unsigned char*, result_len + 1);
  if(result_len)
    memcpy(result, haystack, result_len);
  result[result_len] = '\0';

  return rasqal_new_string_literal(world, result, 
                                   /* language */ NULL,
                                   /* datatype */ NULL,
                                   /* qname */ NULL);

  failed:
  if(l1)
    rasqal_free_literal(l1);

  if(l2)
    rasqal_free_literal(l2);

  if(error_p)
    *error_p = 1;
  
  return NULL;
}


/*
 * rasqal_expression_evaluate_strafter:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate RASQAL_EXPR_STRAFTER(string, needle) expression.
 *
 * Return value: A #rasqal_literal string value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_strafter(rasqal_expression *e,
                                    rasqal_evaluation_context *eval_context,
                                    int *error_p)
{
  rasqal_world* world = eval_context->world;
  rasqal_literal* l1 = NULL;
  rasqal_literal* l2 = NULL;
  const unsigned char *haystack;
  const unsigned char *needle;
  size_t haystack_len;
  size_t needle_len;
  const char *ptr;
  unsigned char* result;
  size_t result_len;

  /* haystack string */
  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if(*error_p || !l1)
    goto failed;
  
  /* needle string */
  l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
  if(*error_p || !l2)
    goto failed;


  haystack = rasqal_literal_as_counted_string(l1, &haystack_len, 
                                              eval_context->flags, error_p);
  if(*error_p || !haystack)
    goto failed;
    
  needle = rasqal_literal_as_counted_string(l2, &needle_len, 
                                            eval_context->flags, error_p);
  if(*error_p || !needle)
    goto failed;

  ptr = strstr((const char*)haystack, (const char*)needle);
  if(ptr) {
    ptr += needle_len;
    result_len = haystack_len - (ptr - (const char*)haystack);
  } else {
    ptr = (const char *)"";
    result_len = 0;
  }

  result = RASQAL_MALLOC(unsigned char*, result_len + 1);
  if(result_len)
    memcpy(result, ptr, result_len);
  result[result_len] = '\0';

  return rasqal_new_string_literal(world, result, 
                                   /* language */ NULL,
                                   /* datatype */ NULL,
                                   /* qname */ NULL);

  failed:
  if(l1)
    rasqal_free_literal(l1);

  if(l2)
    rasqal_free_literal(l2);

  if(error_p)
    *error_p = 1;
  
  return NULL;
}


/*
 * rasqal_regex_get_ref_number:
 * @str: pointer to pointer to buffer at '$' symbol
 *
 * INTERNAL - Decode a $N or $NN reference at *str and move *str past it
 *
 * Return value: reference number or <0 if none found
 */
static int
rasqal_regex_get_ref_number(const char **str)
{
  const char *p = *str;
  int ref_number = 0;
  
  if(!p[1])
    return -1;
  
  /* skip $ */
  p++;

  if(*p >= '0' && *p <= '9') {
    ref_number = (*p - '0');
    p++;
  } else
    return -1;
  
  if(*p && *p >= '0' && *p <= '9') {
    ref_number = ref_number * 10 + (*p - '0');
    p++;
  }
  
  *str = p;
  return ref_number;	
}


#ifdef RASQAL_REGEX_PCRE
static char*
rasqal_string_replace_pcre(rasqal_world* world, raptor_locator* locator,
                           pcre* re, int options,
                           const char *subject, size_t subject_len,
                           const char *replace, size_t replace_len,
                           size_t *result_len_p)
{
  int capture_count = 0;
  int ovecsize;
  int* ovector;
  int stringcount;
  char* result = NULL;
  
  pcre_fullinfo(re, NULL, PCRE_INFO_CAPTURECOUNT, &capture_count);
  ovecsize = (capture_count + 1) *3;
  ovector = RASQAL_CALLOC(int*, ovecsize, sizeof(int));
  if(!ovector)
    return NULL;

  stringcount = pcre_exec(re, 
                          NULL, /* no study */
                          (const char*)subject, (int)subject_len,
                          0 /* startoffset */,
                          options /* options */,
                          ovector, ovecsize
                          );
  if(stringcount >= 0) {
    const char *r;
    char *result_p;
    size_t len = subject_len + replace_len;
    
    result = RASQAL_MALLOC(char*, len + 1);
    if(!result)
      goto failed;

    r = replace;
    result_p = result;
    while(*r) {
      if (*r == '$') {
        int ref_number;
        if(!*r)
          break;

        ref_number = rasqal_regex_get_ref_number(&r);
        if(ref_number >= 0) {
          size_t copy_len;
          copy_len = pcre_copy_substring(subject, ovector,
                                         stringcount, ref_number,
                                         result_p, (int)len);
          result_p += copy_len; len -= copy_len;
        }
        continue;
      }
      
      if(*r == '\\') {
        if(!*++r)
          break;
      }
      
      *result_p++ = *r++; len--;
    }
    *result_p = '\0';
    
    if(result_len_p)
      *result_len_p = result_p - result;

  } else if(stringcount != PCRE_ERROR_NOMATCH) {
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, locator,
                            "Regex match failed with error code %d", stringcount);
    goto failed;
  }

  return result;

  failed:
  if(result)
    RASQAL_FREE(char*, result);

  if(ovector)
    RASQAL_FREE(int*, ovector);

  return NULL;
}
#endif


#ifdef RASQAL_REGEX_POSIX
static char*
rasqal_string_replace_posix(rasqal_world* world, raptor_locator* locator,
                           regex_t reg, int options,
                           const char *subject, size_t subject_len,
                           const char *replace, size_t replace_len,
                           size_t *result_len_p)
{
  size_t nmatch = reg.re_nsub;
  regmatch_t* pmatch;
  int rc;
  char* result = NULL;

  pmatch = RASQAL_CALLOC(regmatch_t*, nmatch + 1, sizeof(regmatch_t));
  if(!pmatch)
    return NULL;

  rc = regexec(&reg, (const char*)subject,
               nmatch, pmatch,
               options /* eflags */
               );

  if(!rc) {
    const char *r;
    char *result_p;
    size_t len = subject_len + replace_len;
    
    result = RASQAL_MALLOC(char*, len + 1);
    r = replace;
    result_p = result;
    while(*r) {
      if (*r == '$') {
        int ref_number;
        size_t copy_len;
        regmatch_t rm;
        
        if(!*r)
          break;
        
        ref_number = rasqal_regex_get_ref_number(&r);
        if(ref_number >= 0) {
          rm = pmatch[ref_number];
          copy_len = rm.rm_eo - rm.rm_so + 1;
          memcpy(result_p, subject + rm.rm_so, copy_len);
          result_p += copy_len; len -= copy_len;
          continue;
        }
      }
      
      if(*r == '\\') {
        if(!*++r)
          break;
      }
      
      *result_p++ = *r++; len--;
    }
    *result_p = '\0';
    
    if(result_len_p)
      *result_len_p = result_p - result;

  } else if (rc != REG_NOMATCH) {
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, locator,
                            "Regex match failed - returned code %d", rc);
    goto failed;
  }

  RASQAL_FREE(regmatch_t*, pmatch);

  return result;


  failed:
  RASQAL_FREE(regmatch_t*, pmatch);
  
  return NULL;
}
#endif


static char*
rasqal_string_replace(rasqal_world* world, raptor_locator* locator,
                      const char* pattern,
                      const char* regex_flags,
                      const char* subject, size_t subject_len,
                      const char* replace, size_t replace_len,
                      size_t* result_len) 
{
  const char *p;
#ifdef RASQAL_REGEX_PCRE
  pcre* re;
  int options = 0;
  const char *re_error = NULL;
  int erroffset = 0;
#endif
#ifdef RASQAL_REGEX_POSIX
  regex_t reg;
  int options = REG_EXTENDED | REG_NOSUB;
#endif
  int rc = 0;
  char *result_s = NULL;

#ifdef RASQAL_REGEX_PCRE
  for(p = regex_flags; p && *p; p++) {
    if(*p == 'i')
      options |= PCRE_CASELESS;
  }

  re = pcre_compile(pattern, options, 
                    &re_error, &erroffset, NULL);
  if(!re) {
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, locator,
                            "Regex compile of '%s' failed - %s", pattern, re_error);
  } else
    result_s = rasqal_string_replace_pcre(world, locator,
                                          re, options,
                                          subject, subject_len,
                                          replace, replace_len,
                                          result_len);
  pcre_free(re);
#endif
    
#ifdef RASQAL_REGEX_POSIX
  for(p = regex_flags; p && *p; p++) {
    if(*p == 'i')
      options |= REG_ICASE;
  }
    
  rc = regcomp(&reg, pattern, options);
  if(rc) {
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, locator,
                            "Regex compile of '%s' failed", pattern);
  } else
    result_s = rasqal_string_replace_posix(world, locator,
                                           reg, options,
                                           subject, subject_len,
                                           replace, replace_len,
                                           result_len);
  regfree(&reg);
#endif

#ifdef RASQAL_REGEX_NONE
  rasqal_log_warning_simple(world, RASQAL_WARNING_LEVEL_MISSING_SUPPORT,
                            locator,
                            "Regex support missing, cannot replace '%s' from '%s' to '%s'", subject, pattern, replace);
  rc = -1;
#endif

  return result_s;
}


/*
 * rasqal_expression_evaluate_replace:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate RASQAL_EXPR_REPLACE(input, pattern, replacement[, flags]) expression.
 *
 * Return value: A #rasqal_literal string value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_replace(rasqal_expression *e,
                                   rasqal_evaluation_context *eval_context,
                                   int *error_p)
{
  rasqal_world* world = eval_context->world;
  const char *match;
  const char *pattern;
  const char *replace;
  const char *regex_flags = NULL;
  size_t match_len;
  size_t replace_len;
  rasqal_literal* l1 = NULL;
  rasqal_literal* l2 = NULL;
  rasqal_literal* l3 = NULL;
  rasqal_literal* l4 = NULL;
  char* result_s = NULL;
  size_t result_len = 0;
  rasqal_literal* result = NULL;

  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if(*error_p || !l1)
    goto failed;
  match = (const char*)rasqal_literal_as_counted_string(l1, &match_len, 
                                                        eval_context->flags,
                                                        error_p);
  if(*error_p || !match)
    goto failed;

  l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
  if(*error_p || !l2)
    goto failed;
  pattern = (const char*)(l2->string);

  l3 = rasqal_expression_evaluate2(e->arg3, eval_context, error_p);
  if(*error_p || !l3)
    goto failed;
  replace = (const char*)rasqal_literal_as_counted_string(l3, &replace_len, 
                                                          eval_context->flags,
                                                          error_p);
  if(*error_p || !replace)
    goto failed;

  if(e->arg4) {
    l4 = rasqal_expression_evaluate2(e->arg4, eval_context, error_p);
    if(*error_p || !l4)
      goto failed;

    regex_flags = (const char*)(l4->string);
  }

  result_s = rasqal_string_replace(world, eval_context->locator,
                                   pattern,
                                   regex_flags,
                                   match, match_len,
                                   replace, replace_len,
                                   &result_len);
  
  RASQAL_DEBUG6("regex replace returned %s for '%s' from '%s' to '%s' (flags=%s)\n", result_s ? result_s : "NULL", match, pattern, replace, regex_flags ? (char*)regex_flags : "");
  
  if(!result_s)
    goto failed;

  result = rasqal_new_string_literal(world, (const unsigned char*)result_s,
                                     NULL, NULL, NULL);

  rasqal_free_literal(l1);
  rasqal_free_literal(l2);
  rasqal_free_literal(l3);
  if(l4)
    rasqal_free_literal(l4);
    
  return result;


  failed:
  if(l1)
    rasqal_free_literal(l1);

  if(l2)
    rasqal_free_literal(l2);

  if(l3)
    rasqal_free_literal(l3);

  if(l4)
    rasqal_free_literal(l4);

  if(error_p)
    *error_p = 1;
  
  return NULL;
}

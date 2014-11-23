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

#include "rasqal.h"
#include "rasqal_internal.h"


#define DEBUG_FH stderr


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
  if((error_p && *error_p) || !l1)
    goto failed;
  
  s = rasqal_literal_as_string_flags(l1, eval_context->flags, error_p);
  if(error_p && *error_p)
    goto failed;

  if(!s)
    len = 0;
  else
    len = raptor_unicode_utf8_strlen(s, strlen(RASQAL_GOOD_CAST(const char*, s)));
  

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
  if((error_p && *error_p) || !l1)
    goto failed;
  
  s = rasqal_literal_as_counted_string(l1, &len, eval_context->flags, error_p);
  if(error_p && *error_p)
    goto failed;

  /* integer startingLoc */
  l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
  if((error_p && *error_p) || !l2)
    goto failed;
  
  startingLoc = rasqal_literal_as_integer(l2, error_p);
  if(error_p && *error_p)
    goto failed;

  /* optional integer length */
  if(e->arg3) {
    l3 = rasqal_expression_evaluate2(e->arg3, eval_context, error_p);
    if(!l3)
      goto failed;

    length = rasqal_literal_as_integer(l3, error_p);
    if(error_p && *error_p)
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
    len = strlen(RASQAL_GOOD_CAST(const char*, l1->language));
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
  if((error_p && *error_p) || !l1)
    goto failed;
  
  s = rasqal_literal_as_counted_string(l1, &len, eval_context->flags, error_p);
  if(error_p && *error_p)
    goto failed;

  new_s =RASQAL_MALLOC(unsigned char*, len + 1);
  if(!new_s)
    goto failed;

  if(e->op == RASQAL_EXPR_UCASE) {
    unsigned int i;
    
    for(i = 0; i < len; i++) {
      unsigned char c = s[i];
      if(islower(RASQAL_GOOD_CAST(int, c)))
        c = RASQAL_GOOD_CAST(unsigned char, toupper(RASQAL_GOOD_CAST(int, c)));
      new_s[i] = c;
    }
  } else { /* RASQAL_EXPR_LCASE */
    unsigned int i;

    for(i = 0; i < len; i++) {
      unsigned char c = s[i];
      if(isupper(RASQAL_GOOD_CAST(int, c)))
        c = RASQAL_GOOD_CAST(unsigned char, tolower(RASQAL_GOOD_CAST(int, c)));
      new_s[i] = c;
    }
  }
  new_s[len] = '\0';

  if(l1->language) {
    len = strlen(RASQAL_GOOD_CAST(const char*, l1->language));
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
  if((error_p && *error_p) || !l1)
    goto failed;
  
  l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
  if((error_p && *error_p) || !l2)
    goto failed;

  if(!rasqal_literals_sparql11_compatible(l1, l2))
    goto failed;
  
  s1 = rasqal_literal_as_counted_string(l1, &len1, eval_context->flags, error_p);
  if(error_p && *error_p)
    goto failed;
  
  s2 = rasqal_literal_as_counted_string(l2, &len2, eval_context->flags, error_p);
  if(error_p && *error_p)
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
      /* b = (strnstr(RASQAL_GOOD_CAST(const char*, s1), RASQAL_GOOD_CAST(const char*, s2), len2) != NULL); */
      b = (strstr(RASQAL_GOOD_CAST(const char*, s1),
                  RASQAL_GOOD_CAST(const char*, s2)) != NULL);
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
  if((error_p && *error_p) || !l1)
    goto failed;
  
  xsd_string_uri = rasqal_xsd_datatype_type_to_uri(l1->world, 
                                                   RASQAL_LITERAL_XSD_STRING);

  dt_uri = l1->datatype;
  if(dt_uri && !raptor_uri_equals(dt_uri, xsd_string_uri))
    /* datatype and not xsd:string */
    goto failed;

  s = rasqal_literal_as_counted_string(l1, &len, eval_context->flags, error_p);
  if(error_p && *error_p)
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
      *p++ = RASQAL_GOOD_CAST(unsigned char, (hex < 10) ? ('0' + hex) : ('A' + hex - 10));
      hex = (c & 0x0f);
      *p++ = RASQAL_GOOD_CAST(unsigned char, (hex < 10) ? ('0' + hex) : ('A' + hex - 10));
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
 * "If all input literals are typed literals of type xsd:string,
 * then the returned literal is also of type xsd:string, if all input
 * literals are plain literals with identical language tag, then the
 * returned literal is a plain literal with the same language tag, in
 * all other cases, the returned literal is a simple literal."
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
  char* lang_tag = NULL;
  int mode = -1; /* -1: undecided  0: xsd:string  1: simple+lang  2: simple */
  raptor_uri* dt = NULL;
  raptor_uri* xsd_string_uri;
  rasqal_literal *result_l;
  
  xsd_string_uri = rasqal_xsd_datatype_type_to_uri(world,
                                                   RASQAL_LITERAL_XSD_STRING);

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

    arg_literal = rasqal_expression_evaluate2(arg_expr, eval_context, error_p);
    if(!arg_literal) {
      /* FIXME - check what to do with a NULL literal */
#if 0
      if(error_p)
        *error_p = 1;
      goto failed;
#endif
      continue;
    }

    if(arg_literal->type != RASQAL_LITERAL_STRING &&
       arg_literal->type != RASQAL_LITERAL_XSD_STRING) {
      /* result is NULL literal; no error */
      goto null_literal;
    }


#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("Concating literal ");
    rasqal_literal_print(arg_literal, stderr);
    fprintf(stderr, " with existing mode %d  lang=%s\n", mode, lang_tag);
#endif

    if(arg_literal->datatype) {
      /* Datatype */ 
      if(raptor_uri_equals(arg_literal->datatype, xsd_string_uri)) {
        if(mode < 0)
          /* mode -1: expect all xsd:string */
          mode = 0;
        else if(mode != 0) {
          /* mode 1, 2: different datatypes, so result is simple literal */
          if(lang_tag) {
            RASQAL_FREE(char*, lang_tag); lang_tag = NULL;
          }
          mode = 2;
        } else {
          /* mode 0: not xsd:string so result is simple literal */
          mode = 2;
        }
      }
    } else {
      /* No datatype; check language */
      if(arg_literal->language) {
        if(mode < 0) {
          /* mode -1: First literal with language: save it and use it */
          size_t lang_len = strlen(arg_literal->language);

          lang_tag = RASQAL_MALLOC(char*, lang_len + 1);
          if(!lang_tag)
            goto failed;
          memcpy(lang_tag, arg_literal->language, lang_len + 1);
          mode = 1;
        } else if (mode == 1) {
          /* mode 1: Already got a lang tag so check it */
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
          RASQAL_DEBUG3("concat compare lang %s vs %s\n",
                        arg_literal->language, lang_tag);
#endif
          if(strcmp(arg_literal->language, lang_tag)) {
            /* different languages, so result is simple literal */
            RASQAL_FREE(char*, lang_tag); lang_tag = NULL;
            mode = 2;
          }
        } else if (mode == 0) {
          /* mode 0: mixture of xsd:string and language literals,
           * so result is simple literal
           */
          mode = 2;
        } /* otherwise mode 2: No change */
      } else {
        if(lang_tag) {
          /* mode 1: language but this literal has none, so result is
           * simple literal */
          RASQAL_FREE(char*, lang_tag); lang_tag = NULL;
        }
        mode = 2;
      }
    }
    
    /* FIXME - check that altering the flags this way to allow
     * concat of URIs is OK 
     */
    s = rasqal_literal_as_string_flags(arg_literal, 
                                         (eval_context->flags & ~RASQAL_COMPARE_XQUERY), 
                                         error_p);
    rasqal_free_literal(arg_literal);


    if((error_p && *error_p) || !s)
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

  if(mode == 0)
    dt = raptor_uri_copy(xsd_string_uri);

  /* result_str and lang and dt (if set) becomes owned by result */
  result_l = rasqal_new_string_literal(world, result_str, lang_tag, dt, NULL);
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG1("Concat result literal: ");
  rasqal_literal_print(result_l, stderr);
  fprintf(stderr, " with mode %d\n", mode);
#endif

  return result_l;

  failed:
  if(error_p)
    *error_p = 1;

  null_literal:
  if(dt)
    raptor_free_uri(dt);
  if(lang_tag)
    RASQAL_FREE(char*, lang_tag);
  if(result_str)
    RASQAL_FREE(char*, result_str);
  if(sb)
    raptor_free_stringbuffer(sb);

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
  if((error_p && *error_p) || !l1)
    goto failed;
  
  l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
  if((error_p && *error_p) || !l2)
    goto failed;
  
  tag = rasqal_literal_as_string_flags(l1, eval_context->flags, error_p);
  if(error_p && *error_p)
    goto failed;
  
  range = rasqal_literal_as_string_flags(l2, eval_context->flags, error_p);
  if(error_p && *error_p)
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
  int b = 0;
  const unsigned char *l1_str;
  const char *match_string;
  const char *pattern;
  const char *regex_flags;
  rasqal_literal *l1, *l2, *l3;
  int rc = 0;
  size_t match_len;
    
  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if((error_p && *error_p) || !l1)
    goto failed;

  l1_str = rasqal_literal_as_counted_string(l1, &match_len,
                                            eval_context->flags, error_p);
  match_string = RASQAL_GOOD_CAST(const char*, l1_str);
  if((error_p && *error_p) || !match_string) {
    rasqal_free_literal(l1);
    goto failed;
  }
    
  l3 = NULL;
  regex_flags = NULL;
  if(e->op == RASQAL_EXPR_REGEX) {
    l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
    if((error_p && *error_p) || !l2) {
      rasqal_free_literal(l1);
      goto failed;
    }

    if(e->arg3) {
      l3 = rasqal_expression_evaluate2(e->arg3, eval_context, error_p);
      if((error_p && *error_p) || !l3) {
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        goto failed;
      }
      regex_flags = RASQAL_GOOD_CAST(const char*, l3->string);
    }
      
  } else {
    l2 = e->literal;
    regex_flags = RASQAL_GOOD_CAST(const char*, l2->flags);
  }
  pattern = RASQAL_GOOD_CAST(const char*, l2->string);

  rc = rasqal_regex_match(world, eval_context->locator,
                          pattern, regex_flags,
                          match_string, match_len);

#ifdef RASQAL_DEBUG
  if(rc >= 0)
    RASQAL_DEBUG5("regex match returned %s for '%s' against '%s' (flags=%s)\n", rc ? "true" : "false", match_string, pattern, l2->flags ? RASQAL_GOOD_CAST(char*, l2->flags) : "");
  else
    RASQAL_DEBUG4("regex match returned failed for '%s' against '%s' (flags=%s)\n", match_string, pattern, l2->flags ? RASQAL_GOOD_CAST(char*, l2->flags) : "");
#endif
  
  rasqal_free_literal(l1);
  if(e->op == RASQAL_EXPR_REGEX) {
    rasqal_free_literal(l2);
    if(l3)
      rasqal_free_literal(l3);
  }
    
  if(rc < 0)
    goto failed;
    
  b = rc;
  if(e->op == RASQAL_EXPR_STR_NMATCH)
    b = 1 - b;

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
  const char *ptr;
  unsigned char* result;
  size_t result_len;
  char* new_lang = NULL;

  /* haystack string */
  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if((error_p && *error_p) || !l1)
    goto failed;
  
  /* needle string */
  l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
  if((error_p && *error_p) || !l2)
    goto failed;

  if(!rasqal_literal_is_string(l1) || !rasqal_literal_is_string(l2)) {
    /* not strings */
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("Cannot strbefore haystack ");
    rasqal_literal_print(l1, stderr);
    fputs( " to needle ", stderr);
    rasqal_literal_print(l2, stderr);
    fputs(" - both not string", stderr);
#endif
    goto failed;
  }

  if(l2->language && rasqal_literal_string_languages_compare(l1, l2)) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("Cannot strbefore haystack ");
    rasqal_literal_print(l1, stderr);
    fputs( " to language needle ", stderr);
    rasqal_literal_print(l2, stderr);
    fputs(" - languages mismatch", stderr);
#endif
    goto failed;
  }

  haystack = rasqal_literal_as_counted_string(l1, &haystack_len, 
                                              eval_context->flags, error_p);
  if((error_p && *error_p) || !haystack)
    goto failed;
    
  needle = rasqal_literal_as_counted_string(l2, &needle_len, 
                                            eval_context->flags, error_p);
  if((error_p && *error_p) || !needle)
    goto failed;

  ptr = strstr(RASQAL_GOOD_CAST(const char*, haystack), 
               RASQAL_GOOD_CAST(const char*, needle));
  if(ptr) {
    result_len = RASQAL_GOOD_CAST(size_t, ptr - RASQAL_GOOD_CAST(const char*, haystack));

    if(l1->language) {
      size_t len = strlen(RASQAL_GOOD_CAST(const char*, l1->language));
      new_lang = RASQAL_MALLOC(char*, len + 1);
      if(!new_lang)
        goto failed;

      memcpy(new_lang, l1->language, len + 1);
    }
  } else {
    result_len = 0;
    haystack = RASQAL_GOOD_CAST(const unsigned char *, "");
  }

  rasqal_free_literal(l1); l1 = NULL;
  rasqal_free_literal(l2); l2 = NULL;

  result = RASQAL_MALLOC(unsigned char*, result_len + 1);
  if(!result)
    goto failed;

  if(result_len)
    memcpy(result, haystack, result_len);
  result[result_len] = '\0';

  return rasqal_new_string_literal(world, result, 
                                   new_lang,
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
  char* new_lang = NULL;

  /* haystack string */
  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if((error_p && *error_p) || !l1)
    goto failed;
  
  /* needle string */
  l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
  if((error_p && *error_p) || !l2)
    goto failed;

  if(!rasqal_literal_is_string(l1) || !rasqal_literal_is_string(l2)) {
    /* not strings */
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("Cannot strafter haystack ");
    rasqal_literal_print(l1, stderr);
    fputs( " to needle ", stderr);
    rasqal_literal_print(l2, stderr);
    fputs(" - both not string", stderr);
#endif
    goto failed;
  }

  if(l2->language && rasqal_literal_string_languages_compare(l1, l2)) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("Cannot strafter haystack ");
    rasqal_literal_print(l1, stderr);
    fputs( " to language needle ", stderr);
    rasqal_literal_print(l2, stderr);
    fputs(" - languages mismatch", stderr);
#endif
    goto failed;
  }


  haystack = rasqal_literal_as_counted_string(l1, &haystack_len, 
                                              eval_context->flags, error_p);
  if((error_p && *error_p) || !haystack)
    goto failed;
    
  needle = rasqal_literal_as_counted_string(l2, &needle_len, 
                                            eval_context->flags, error_p);
  if((error_p && *error_p) || !needle)
    goto failed;

  ptr = strstr(RASQAL_GOOD_CAST(const char*, haystack),
               RASQAL_GOOD_CAST(const char*, needle));
  if(ptr) {
    ptr += needle_len;
    result_len = haystack_len - RASQAL_GOOD_CAST(size_t, (ptr - RASQAL_GOOD_CAST(const char*, haystack)));

    if(l1->language) {
      size_t len = strlen(RASQAL_GOOD_CAST(const char*, l1->language));
      new_lang = RASQAL_MALLOC(char*, len + 1);
      if(!new_lang)
        goto failed;

      memcpy(new_lang, l1->language, len + 1);
    }
  } else {
    ptr = (const char *)"";
    result_len = 0;
  }

  rasqal_free_literal(l1); l1 = NULL;
  rasqal_free_literal(l2); l2 = NULL;

  result = RASQAL_MALLOC(unsigned char*, result_len + 1);
  if(!result)
    goto failed;

  if(result_len)
    memcpy(result, ptr, result_len);
  result[result_len] = '\0';

  return rasqal_new_string_literal(world, result, 
                                   new_lang,
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
  const unsigned char *tmp_str;
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
  if((error_p && *error_p) || !l1)
    goto failed;
  tmp_str = rasqal_literal_as_counted_string(l1, &match_len, 
                                             eval_context->flags,
                                             error_p);
  match = RASQAL_GOOD_CAST(const char*, tmp_str);
  if((error_p && *error_p) || !match)
    goto failed;

  l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
  if((error_p && *error_p) || !l2)
    goto failed;
  pattern = RASQAL_GOOD_CAST(const char*, l2->string);

  l3 = rasqal_expression_evaluate2(e->arg3, eval_context, error_p);
  if((error_p && *error_p) || !l3)
    goto failed;

  if(l1->type != RASQAL_LITERAL_STRING && l1->type != RASQAL_LITERAL_XSD_STRING)
    /* Not a string so cannot do string operations */
    goto failed;

  tmp_str = rasqal_literal_as_counted_string(l3, &replace_len, 
                                             eval_context->flags,
                                             error_p);
  replace = RASQAL_GOOD_CAST(const char*, tmp_str);
  if((error_p && *error_p) || !replace)
    goto failed;

  if(e->arg4) {
    l4 = rasqal_expression_evaluate2(e->arg4, eval_context, error_p);
    if((error_p && *error_p) || !l4)
      goto failed;

    regex_flags = RASQAL_GOOD_CAST(const char*, l4->string);
  }

  result_s = rasqal_regex_replace(world, eval_context->locator,
                                  pattern,
                                  regex_flags,
                                  match, match_len,
                                  replace, replace_len,
                                  &result_len);
  
  RASQAL_DEBUG6("regex replace returned %s for '%s' from '%s' to '%s' (flags=%s)\n", result_s ? result_s : "NULL", match, pattern, replace, regex_flags ? RASQAL_GOOD_CAST(char*, regex_flags) : "");
  
  if(!result_s)
    goto failed;

  result = rasqal_new_string_literal(world, 
                                     RASQAL_GOOD_CAST(const unsigned char*, result_s),
                                     l1->language, l1->datatype, NULL);
  l1->language = NULL;
  l1->datatype = NULL;

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

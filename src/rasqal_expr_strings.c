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


static size_t
rasqal_unicode_utf8_strlen(const unsigned char *string, size_t length)
{
  size_t unicode_length = 0;
  
  while(length > 0) {
    int unichar_len;
    unichar_len = raptor_unicode_utf8_string_get_char(string, length, NULL);
    if(unichar_len < 0 || unichar_len > (int)length)
      break;
    
    string += unichar_len;
    length -= unichar_len;

    unicode_length++;
  }

  return unicode_length;
}


/* 
 * rasqal_expression_evaluate_strlen:
 * @world: #rasqal_world
 * @locator: error locator object
 * @e: The expression to evaluate.
 * @flags: Compare flags
 *
 * INTERNAL - Evaluate RASQAL_EXPR_STRLEN(expr) expression.
 *
 * Return value: A #rasqal_literal integer value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_strlen(rasqal_world *world,
                                  raptor_locator *locator,
                                  rasqal_expression *e,
                                  int flags)
{
  rasqal_literal* l1;
  rasqal_literal* result = NULL;
  const unsigned char *s;
  size_t len = 0;
  int error = 0;
  
  l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
  if(!l1)
    return NULL;
  
  s = rasqal_literal_as_string_flags(l1, flags, &error);
  if(error)
    goto failed;

  if(!s)
    len = 0;
  else
    len = rasqal_unicode_utf8_strlen(s, strlen((const char*)s));
  

  result = rasqal_new_integer_literal(world, RASQAL_LITERAL_INTEGER, len);
  rasqal_free_literal(l1);
  return result;

  failed:
  if(l1)
    rasqal_free_literal(l1);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_set_case:
 * @world: #rasqal_world
 * @locator: error locator object
 * @e: The expression to evaluate.
 * @flags: Compare flags
 *
 * INTERNAL - Evaluate RASQAL_EXPR_UCASE(expr) or
 * RASQAL_EXPR_LCASE(expr) expressions.
 *
 * Return value: A #rasqal_literal string value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_set_case(rasqal_world *world,
                                    raptor_locator *locator,
                                    rasqal_expression *e,
                                    int flags)
{
  rasqal_literal* l1;
  const unsigned char *s;
  unsigned char* new_s = NULL;
  char* new_lang = NULL;
  raptor_uri* dt_uri = NULL;
  size_t len = 0;
  int error = 0;
  
  l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
  if(!l1)
    return NULL;
  
  s = rasqal_literal_as_counted_string(l1, &len, flags, &error);
  if(error)
    goto failed;

  new_s =(unsigned char*)RASQAL_MALLOC(cstring, len + 1);
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
    new_lang = (char*)RASQAL_MALLOC(cstring, len + 1);
    if(!new_lang)
      goto failed;

    memcpy(new_lang, l1->language, len + 1);
  }

  dt_uri = l1->datatype;
  if(dt_uri)
    dt_uri = raptor_uri_copy(dt_uri);

  /* after this new_s becomes owned by result */
  return rasqal_new_string_literal(world, new_s, new_lang, dt_uri,
                                   /* qname */ NULL);
  
  
  failed:
  if(new_s)
    RASQAL_FREE(cstring, new_s);
  if(new_lang)
    RASQAL_FREE(cstring, new_lang);
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
 * @world: #rasqal_world
 * @locator: error locator object
 * @e: The expression to evaluate.
 * @flags: Compare flags
 *
 * INTERNAL - Evaluate RASQAL_EXPR_STRSTARTS(lit, lit) and
 * RASQAL_EXPR_STRENDS(lit, lit) expressions.
 *
 * Return value: A #rasqal_literal integer value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_str_prefix_suffix(rasqal_world *world,
                                             raptor_locator *locator,
                                             rasqal_expression *e,
                                             int flags)
{
  rasqal_literal *l1 = NULL;
  rasqal_literal *l2 = NULL;
  int b;
  const unsigned char *s1;
  const unsigned char *s2;
  size_t len1 = 0;
  size_t len2 = 0;
  int error = 0;
  
  l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
  if(!l1)
    return NULL;
  
  l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
  if(!l2)
    goto failed;

  if(!rasqal_literals_sparql11_compatible(l1, l2))
    goto failed;
  
  s1 = rasqal_literal_as_counted_string(l1, &len1, flags, &error);
  if(error)
    goto failed;
  
  s2 = rasqal_literal_as_counted_string(l2, &len2, flags, &error);
  if(error)
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
  if(l1)
    rasqal_free_literal(l1);
  if(l2)
    rasqal_free_literal(l2);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_concat:
 * @world: #rasqal_world
 * @locator: error locator object
 * @e: The expression to evaluate.
 * @flags: Compare flags
 *
 * INTERNAL - Evaluate RASQAL_EXPR_CONCAT(expr list) expression.
 *
 * Return value: A #rasqal_literal string value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_concat(rasqal_world *world,
                                  raptor_locator *locator,
                                  rasqal_expression *e,
                                  int flags)
{
  raptor_stringbuffer* sb = NULL;
  int i;
  size_t len;
  unsigned char* result_str = NULL;
  int error = 0;
  rasqal_literal* result = NULL;
  int same_dt = 1;
  raptor_uri* dt = NULL;
  
  sb = raptor_new_stringbuffer();
  
  for(i = 0; i < raptor_sequence_size(e->args); i++) {
    rasqal_expression *arg_expr;
    rasqal_literal* arg_literal;
    const unsigned char* s = NULL;
    
    arg_expr = (rasqal_expression*)raptor_sequence_get_at(e->args, i);
    if(!arg_expr)
      break;

    /* FIXME - check what to do with a NULL literal */
    /* FIXME - check what to do with an empty string literal */

    arg_literal = rasqal_expression_evaluate(world, locator, arg_expr, flags);
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
                                         flags & ~RASQAL_COMPARE_XQUERY, 
                                         &error);
      rasqal_free_literal(arg_literal);
    } else
      error = 1;

    if(!s || error)
      goto failed;
    
    raptor_stringbuffer_append_string(sb, s, 1); 
  }
  
  
  len = raptor_stringbuffer_length(sb);
  result_str = (unsigned char*)RASQAL_MALLOC(cstring, len + 1);
  if(!result_str)
    goto failed;
  
  if(raptor_stringbuffer_copy_to_string(sb, result_str, len))
    goto failed;
  
  raptor_free_stringbuffer(sb);
  
  /* result_str and dt (if set) becomes owned by result */
  return rasqal_new_string_literal(world, result_str, NULL, dt, NULL);


  failed:
  if(dt)
    raptor_free_uri(dt);
  if(result_str)
    RASQAL_FREE(cstring, result_str);
  if(sb)
    raptor_free_stringbuffer(sb);
  if(result)
    rasqal_free_literal(result);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_langmatches:
 * @world: #rasqal_world
 * @locator: error locator object
 * @e: The expression to evaluate.
 * @flags: Compare flags
 *
 * INTERNAL - Evaluate RASQAL_EXPR_LANGMATCHES(lang tag, lang tag range) expression.
 *
 * Return value: A #rasqal_literal boolean value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_langmatches(rasqal_world *world,
                                       raptor_locator *locator,
                                       rasqal_expression *e,
                                       int flags)
{
  rasqal_literal *l1 = NULL;
  rasqal_literal *l2 = NULL;
  const unsigned char *tag;
  const unsigned char *range;
  int b;
  int error = 0;
  
  l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
  if(!l1)
    goto failed;
  
  l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
  if(!l2)
    goto failed;
  
  tag = rasqal_literal_as_string_flags(l1, flags, &error);
  if(error)
    goto failed;
  
  range = rasqal_literal_as_string_flags(l2, flags, &error);
  if(error)
    goto failed;
  
  
  b = rasqal_language_matches(tag, range);
  
  rasqal_free_literal(l1);
  rasqal_free_literal(l2);
  
  return rasqal_new_boolean_literal(world, b);

  failed:
  if(l1)
    rasqal_free_literal(l1);
  if(l2)
    rasqal_free_literal(l2);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_strmatch:
 * @world: #rasqal_world
 * @locator: error locator object
 * @e: The expression to evaluate.
 * @flags: Compare flags
 *
 * INTERNAL - Evaluate RASQAL_EXPR_STR_MATCH, RASQAL_EXPR_STR_NMATCH and
 * RASQAL_EXPR_REGEX expressions.
 *
 * Return value: A #rasqal_literal value or NULL on failure.
 */
rasqal_literal*
rasqal_expression_evaluate_strmatch(rasqal_world *world,
                                    raptor_locator *locator,
                                    rasqal_expression *e,
                                    int flags)
{
  int b=0;
  int flag_i=0; /* flags contains i */
  const unsigned char *p;
  const unsigned char *match_string;
  const unsigned char *pattern;
  const unsigned char *regex_flags;
  rasqal_literal *l1, *l2, *l3;
  int error=0;
  int rc=0;
#ifdef RASQAL_REGEX_PCRE
  pcre* re;
  int options=0;
  const char *re_error=NULL;
  int erroffset=0;
#endif
#ifdef RASQAL_REGEX_POSIX
  regex_t reg;
  int options=REG_EXTENDED | REG_NOSUB;
#endif
  size_t match_len;
    
  l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
  if(!l1)
    goto failed;

  match_string = rasqal_literal_as_counted_string(l1, &match_len, flags, &error);
  if(error || !match_string) {
    rasqal_free_literal(l1);
    goto failed;
  }
    
  l3=NULL;
  regex_flags=NULL;
  if(e->op == RASQAL_EXPR_REGEX) {
    l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
    if(!l2) {
      rasqal_free_literal(l1);
      goto failed;
    }

    if(e->arg3) {
      l3 = rasqal_expression_evaluate(world, locator, e->arg3, flags);
      if(!l3) {
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
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, locator,
                            "Regex compile of '%s' failed - %s", pattern, re_error);
    rc= -1;
  } else {
    rc=pcre_exec(re, 
                 NULL, /* no study */
                 (const char*)match_string, match_len,
                 0 /* startoffset */,
                 0 /* options */,
                 NULL, 0 /* ovector, ovecsize - no matches wanted */
                 );
    if(rc >= 0)
      b=1;
    else if(rc != PCRE_ERROR_NOMATCH) {
      rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, locator,
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
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, locator,
                            "Regex compile of '%s' failed", pattern);
    rc= -1;
  } else {
    rc=regexec(&reg, (const char*)match_string, 
               0, NULL, /* nmatch, regmatch_t pmatch[] - no matches wanted */
               0 /* eflags */
               );
    if(!rc)
      b=1;
    else if (rc != REG_NOMATCH) {
      rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, locator,
                              "Regex match failed - returned code %d", rc);
      rc= -1;
    } else
      rc= 0;
  }
  regfree(&reg);
#endif

#ifdef RASQAL_REGEX_NONE
  rasqal_log_error_simple(world,
                          RAPTOR_LOG_LEVEL_WARN,
                          locator,
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
  return NULL;
}

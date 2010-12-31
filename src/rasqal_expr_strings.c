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
    len = strlen((const char*)s);

  result = rasqal_new_integer_literal(world, RASQAL_LITERAL_INTEGER, len);
  rasqal_free_literal(l1);
  return result;

  failed:
  if(l1)
    rasqal_free_literal(l1);

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
  unsigned char* result_str;
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
    
  l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
  if(!l1)
    goto failed;

  match_string=rasqal_literal_as_string_flags(l1, flags, &error);
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
                 (const char*)match_string, strlen((const char*)match_string),
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

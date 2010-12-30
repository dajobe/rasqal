/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_expr_evaluate.c - Rasqal expression evaluation
 *
 * Copyright (C) 2003-2010, David Beckett http://www.dajobe.org/
 * Copyright (C) 2003-2005, University of Bristol, UK http://www.bristol.ac.uk/
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
 * rasqal_language_matches:
 * @lang_tag: language tag such as "en" or "en-US" or "ab-cd-ef"
 * @lang_range: language range such as "*" (SPARQL) or "en" or "ab-cd"
 *
 * INTERNAL - Match a language tag against a language range
 *
 * Returns true if @lang_range matches @lang_tag per
 *   Matching of Language Tags [RFC4647] section 2.1
 * RFC4647 defines a case-insensitive, hierarchical matching
 * algorithm which operates on ISO-defined subtags for language and
 * country codes, and user defined subtags.
 *
 * (Note: RFC3066 section 2.5 matching is identical to
 * RFC4647 section 3.3.1 Basic Filtering )
 * 
 * In SPARQL, a language-range of "*" matches any non-empty @lang_tag string.
 * See http://www.w3.org/TR/2007/WD-rdf-sparql-query-20070326/#func-langMatches
 *
 * Return value: non-0 if true
 */
static int
rasqal_language_matches(const unsigned char* lang_tag,
                        const unsigned char* lang_range) 
{
  int b= 0;

  if(!(lang_tag && lang_range && *lang_tag && *lang_range)) {
    /* One of the arguments is NULL or the empty string */
    return 0;
  }

  /* Now have two non-empty arguments */

  /* Simple range string "*" matches anything excluding NULL/empty
   * lang_tag (checked above)
   */
  if(lang_range[0] == '*') {
    if(!lang_range[1])
      b = 1;
    return b;
  }
  
  while (1) {
    char tag_c   = tolower(*lang_tag++);
    char range_c = tolower(*lang_range++);
    if ((!tag_c && !range_c) || (!range_c && tag_c == '-')) {
      /* EITHER
       *   The end of both strings (thus everything previous matched
       *   such as e.g. tag "fr-CA" matching range "fr-ca")
       * OR
       *   The end of the range and end of the tag prefix (e.g. tag
       *   "en-US" matching range "en")
       * means a match
       */
      b = 1;
      break;
    } 
    if (range_c != tag_c) {
      /* If a difference was found - including one of the
       * strings being shorter than the other, it means no match
       * (b is set to 0 above)
       */
      break;
    }
  }

  return b;
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
static rasqal_literal*
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


/* 
 * rasqal_expression_evaluate_now:
 * @world: #rasqal_world
 * @locator: error locator object
 * @e: The expression to evaluate.
 * @flags: Compare flags
 *
 * INTERNAL - Evaluate RASQAL_EXPR_NOW, RASQAL_EXPR_DATETIME expressions.
 *
 * Return value: A #rasqal_literal value or NULL on failure.
 */
static rasqal_literal*
rasqal_expression_evaluate_now(rasqal_world *world,
                               raptor_locator *locator,
                               rasqal_expression *e,
                               int flags)
{
  struct timeval *tv;
  rasqal_xsd_datetime* dt;

  tv = rasqal_world_get_now_timeval(world);
  if(!tv)
    return NULL;
  
  dt = rasqal_new_xsd_datetime_from_timeval(world, tv);
  if(!dt)
    return NULL;
  
  return rasqal_new_datetime_literal_from_datetime(world, dt);
}


/**
 * rasqal_expression_evaluate:
 * @world: #rasqal_world
 * @locator: error locator (or NULL)
 * @e: The expression to evaluate.
 * @flags: Flags for rasqal_literal_compare() and RASQAL_COMPARE_NOCASE for string matches.
 * 
 * Evaluate a #rasqal_expression tree to give a #rasqal_literal result
 * or error.
 * 
 * Return value: a #rasqal_literal value or NULL on failure.
 **/
rasqal_literal*
rasqal_expression_evaluate(rasqal_world *world, raptor_locator *locator,
                           rasqal_expression* e, int flags)
{
  rasqal_literal* result = NULL;
  rasqal_literal *l1;
  rasqal_literal *l2;
  const unsigned char *s;

  /* pack vars from different switch cases in unions to save some stack space */
  union {
    struct { int e1; int e2; } errs;
    struct { int dummy_do_not_mask_e; int free_literal; } flags;
    int e;
  } errs;
  union {
    struct { int b1; int b2; } bools;
    int b;
    int i;
    raptor_uri *dt_uri;
    const unsigned char *s;
    unsigned char *new_s;
    rasqal_variable *v;
    rasqal_expression *e;
    struct { void *dummy_do_not_mask; int found; } flags;
    rasqal_xsd_datetime* dt;
    struct timeval *tv;
    raptor_stringbuffer* sb;
  } vars;
  int i; /* for looping */

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(e, rasqal_expression, NULL);

  errs.e = 0;

#ifdef RASQAL_DEBUG
  RASQAL_DEBUG2("evaluating expression %p: ", e);
  rasqal_expression_print(e, stderr);
  fprintf(stderr, "\n");
#endif
  
  switch(e->op) {
    case RASQAL_EXPR_AND:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1) {
        errs.errs.e1=1;
        vars.bools.b1=0;
      } else {
        errs.errs.e1=0;
        vars.bools.b1=rasqal_literal_as_boolean(l1, &errs.errs.e1);
        rasqal_free_literal(l1);
      }

      l1 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l1) {
        errs.errs.e2=1;
        vars.bools.b2=0;
      } else {
        errs.errs.e2=0;
        vars.bools.b2=rasqal_literal_as_boolean(l1, &errs.errs.e2);
        rasqal_free_literal(l1);
      }

      /* See http://www.w3.org/TR/2005/WD-rdf-sparql-query-20051123/#truthTable */
      if(!errs.errs.e1 && !errs.errs.e2) {
        /* No type error, answer is A && B */
        vars.b = vars.bools.b1 && vars.bools.b2; /* don't need b1,b2 anymore */
      } else {
        if((!vars.bools.b1 && errs.errs.e2) || (errs.errs.e1 && vars.bools.b2))
          /* F && E => F.   E && F => F. */
          vars.b=0;
        else
          /* Otherwise E */
          goto failed;
      }
      result=rasqal_new_boolean_literal(world, vars.b);
      break;
      
    case RASQAL_EXPR_OR:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1) {
        errs.errs.e1=1;
        vars.bools.b1=0;
      } else {
        errs.errs.e1=0;
        vars.bools.b1=rasqal_literal_as_boolean(l1, &errs.errs.e1);
        rasqal_free_literal(l1);
      }

      l1 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l1) {
        errs.errs.e2=1;
        vars.bools.b2=0;
      } else {
        errs.errs.e2=0;
        vars.bools.b2=rasqal_literal_as_boolean(l1, &errs.errs.e2);
        rasqal_free_literal(l1);
      }

      /* See http://www.w3.org/TR/2005/WD-rdf-sparql-query-20051123/#truthTable */
      if(!errs.errs.e1 && !errs.errs.e2) {
        /* No type error, answer is A || B */
        vars.b = vars.bools.b1 || vars.bools.b2; /* don't need b1,b2 anymore */
      } else {
        if((vars.bools.b1 && errs.errs.e2) || (errs.errs.e1 && vars.bools.b2))
          /* T || E => T.   E || T => T */
          vars.b=1;
        else
          /* Otherwise E */
          goto failed;
      }
      result=rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_EQ:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      /* FIXME - this should probably be checked at literal creation
       * time
       */
      if(!rasqal_xsd_datatype_check(l1->type, l1->string, flags) ||
         !rasqal_xsd_datatype_check(l2->type, l2->string, flags)) {
        RASQAL_DEBUG1("One of the literals was invalid\n");
        goto failed;
      }

      vars.b=(rasqal_literal_equals_flags(l1, l2, flags, &errs.e) != 0);
#if RASQAL_DEBUG > 1
      if(errs.e)
        RASQAL_DEBUG1("rasqal_literal_equals_flags returned: FAILURE\n");
      else
        RASQAL_DEBUG2("rasqal_literal_equals_flags returned: %d\n", vars.b);
#endif
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;
      result=rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_NEQ:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.b=(rasqal_literal_not_equals_flags(l1, l2, flags, &errs.e) != 0);
#if RASQAL_DEBUG > 1
      if(errs.e)
        RASQAL_DEBUG1("rasqal_literal_not_equals_flags returned: FAILURE\n");
      else
        RASQAL_DEBUG2("rasqal_literal_not_equals_flags returned: %d\n", vars.b);
#endif
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;
      result=rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_LT:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.b=(rasqal_literal_compare(l1, l2, flags, &errs.e) < 0);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;
      result=rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_GT:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.b=(rasqal_literal_compare(l1, l2, flags, &errs.e) > 0);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;
      result=rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_LE:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.b=(rasqal_literal_compare(l1, l2, flags, &errs.e) <= 0);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;
      result=rasqal_new_boolean_literal(world, vars.b);
      break;        

    case RASQAL_EXPR_GE:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.b=(rasqal_literal_compare(l1, l2, flags, &errs.e) >= 0);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;
      result=rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_UMINUS:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      result=rasqal_literal_negate(l1, &errs.e);
      rasqal_free_literal(l1);
      if(errs.e)
        goto failed;
      break;

    case RASQAL_EXPR_BOUND:
      /* Do not use rasqal_expression_evaluate() here since
       * we need to check the argument is a variable, and
       * that function will flatten such thing to literals
       * as early as possible. See (FLATTEN_LITERAL) below
       */
      if(!e->arg1 || e->arg1->op != RASQAL_EXPR_LITERAL)
        goto failed;

      l1=e->arg1->literal;
      if(!l1 || l1->type != RASQAL_LITERAL_VARIABLE)
        goto failed;

      vars.v=rasqal_literal_as_variable(l1);
      if(!vars.v)
        goto failed;

      result=rasqal_new_boolean_literal(world, (vars.v->value != NULL));
      break;

    case RASQAL_EXPR_STR:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      /* Note: flags removes RASQAL_COMPARE_XQUERY as this is the
       * explicit stringify operation
       */
      s=rasqal_literal_as_string_flags(l1, (flags & ~RASQAL_COMPARE_XQUERY),
                                       &errs.e);
      if(!s || errs.e) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.new_s=(unsigned char *)RASQAL_MALLOC(cstring, strlen((const char*)s)+1);
      if(!vars.new_s) {
        rasqal_free_literal(l1);
        goto failed;
      }
      strcpy((char*)vars.new_s, (const char*)s);

      result=rasqal_new_string_literal(world, vars.new_s, NULL, NULL, NULL);
      rasqal_free_literal(l1);

      break;
      
    case RASQAL_EXPR_LANG:
      errs.flags.free_literal=1;
      
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      vars.v=rasqal_literal_as_variable(l1);
      if(vars.v) {
        rasqal_free_literal(l1);
        l1=vars.v->value; /* don't need vars.v after this */
        errs.flags.free_literal=0;
        if(!l1)
          goto failed;
      }

      if(rasqal_literal_get_rdf_term_type(l1) != RASQAL_LITERAL_STRING) {
        if(errs.flags.free_literal)
          rasqal_free_literal(l1);
        goto failed;
      }

      if(l1->language) {
        vars.new_s=(unsigned char*)RASQAL_MALLOC(cstring,
                                                 strlen(l1->language)+1);
        if(!vars.new_s) {
          if(errs.flags.free_literal)
            rasqal_free_literal(l1);
          goto failed;
        }
        strcpy((char*)vars.new_s, l1->language);
      } else  {
        vars.new_s=(unsigned char*)RASQAL_MALLOC(cstring, 1);
        if(!vars.new_s) {
          if(errs.flags.free_literal)
            rasqal_free_literal(l1);
          goto failed;
        }
        *vars.new_s='\0';
      }
      result=rasqal_new_string_literal(world, vars.new_s, NULL, NULL, NULL);
      
      if(errs.flags.free_literal)
        rasqal_free_literal(l1);

      break;

    case RASQAL_EXPR_LANGMATCHES:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      s=rasqal_literal_as_string_flags(l1, flags, &errs.e);
      vars.s=rasqal_literal_as_string_flags(l2, flags, &errs.e);

      if(errs.e)
        vars.b=0;
      else
        vars.b=rasqal_language_matches(s, vars.s); /* don't need s anymore */
      
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);

      result=rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_DATATYPE:
      errs.flags.free_literal=1;
      vars.dt_uri=NULL;
      
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      vars.v=rasqal_literal_as_variable(l1);
      if(vars.v) {
        rasqal_free_literal(l1);
        l1=vars.v->value; /* don't need vars.v after this */
        errs.flags.free_literal=0;
        if(!l1)
          goto failed;
      }

      if(rasqal_literal_get_rdf_term_type(l1) != RASQAL_LITERAL_STRING) {
        if(errs.flags.free_literal)
          rasqal_free_literal(l1);
        goto failed;
      }

      if(l1->language) {
        if(errs.flags.free_literal)
          rasqal_free_literal(l1);
        goto failed;
      }

      /* The datatype of a plain literal is xsd:string */
      vars.dt_uri=l1->datatype;
      if(!vars.dt_uri && l1->type == RASQAL_LITERAL_STRING)
        vars.dt_uri=rasqal_xsd_datatype_type_to_uri(l1->world,
                                                    RASQAL_LITERAL_XSD_STRING);

      if(!vars.dt_uri) {
        if(errs.flags.free_literal)
          rasqal_free_literal(l1);
        goto failed;
      }
      
      result = rasqal_new_uri_literal(world, raptor_uri_copy(vars.dt_uri));

      if(errs.flags.free_literal)
        rasqal_free_literal(l1);

      break;

    case RASQAL_EXPR_ISURI:
      errs.flags.free_literal=1;
      
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;
      
      vars.v=rasqal_literal_as_variable(l1);
      if(vars.v) {
        rasqal_free_literal(l1);
        l1=vars.v->value; /* don't need vars.v after this */
        errs.flags.free_literal=0;
        if(!l1)
          goto failed;
      }

      vars.b=(l1->type == RASQAL_LITERAL_URI);
      
      if(errs.flags.free_literal)
        rasqal_free_literal(l1);

      result=rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_ISBLANK:
      errs.flags.free_literal=1;
      
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;
      
      vars.v=rasqal_literal_as_variable(l1);
      if(vars.v) {
        rasqal_free_literal(l1);
        l1=vars.v->value; /* don't need vars.v after this */
        errs.flags.free_literal=0;
        if(!l1)
          goto failed;
      }

      vars.b=(l1->type == RASQAL_LITERAL_BLANK);

      if(errs.flags.free_literal)
        rasqal_free_literal(l1);

      result=rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_ISLITERAL:
      errs.flags.free_literal=1;
      
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;
      
      vars.v=rasqal_literal_as_variable(l1);
      if(vars.v) {
        rasqal_free_literal(l1);
        l1=vars.v->value; /* don't need vars.v after this */
        errs.flags.free_literal=0;
        if(!l1)
          goto failed;
      }

      vars.b=(rasqal_literal_get_rdf_term_type(l1) == RASQAL_LITERAL_STRING);

      if(errs.flags.free_literal)
        rasqal_free_literal(l1);

      result=rasqal_new_boolean_literal(world, vars.b);
      break;
      
    case RASQAL_EXPR_PLUS:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      result=rasqal_literal_add(l1, l2, &errs.e);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;
      
      break;

    case RASQAL_EXPR_MINUS:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      result=rasqal_literal_subtract(l1, l2, &errs.e);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;
      
      break;
      
    case RASQAL_EXPR_STAR:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      result=rasqal_literal_multiply(l1, l2, &errs.e);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;
      
      break;
      
    case RASQAL_EXPR_SLASH:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      result=rasqal_literal_divide(l1, l2, &errs.e);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;
      
      break;
      
    case RASQAL_EXPR_REM:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.i=rasqal_literal_as_integer(l2, &errs.errs.e2);
      /* error if divisor is zero */
      if(!vars.i)
        errs.errs.e2=1;
      else
        vars.i=rasqal_literal_as_integer(l1, &errs.errs.e1) % vars.i;

      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.errs.e1 || errs.errs.e2)
        goto failed;

      result=rasqal_new_integer_literal(world, RASQAL_LITERAL_INTEGER, vars.i);
      break;
      
    case RASQAL_EXPR_STR_EQ:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.b=(rasqal_literal_compare(l1, l2, flags | RASQAL_COMPARE_NOCASE,
                                     &errs.e) == 0);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;

      result=rasqal_new_boolean_literal(world, vars.b);
      break;
      
    case RASQAL_EXPR_STR_NEQ:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.b=(rasqal_literal_compare(l1, l2, flags | RASQAL_COMPARE_NOCASE, 
                                     &errs.e) != 0);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;

      result=rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_TILDE:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      vars.i= ~ rasqal_literal_as_integer(l1, &errs.e);
      rasqal_free_literal(l1);
      if(errs.e)
        goto failed;

      result=rasqal_new_integer_literal(world, RASQAL_LITERAL_INTEGER, vars.i);
      break;

    case RASQAL_EXPR_BANG:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      vars.b= ! rasqal_literal_as_boolean(l1, &errs.e);
      rasqal_free_literal(l1);
      if(errs.e)
        goto failed;

      result=rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_STR_MATCH:
    case RASQAL_EXPR_STR_NMATCH:
    case RASQAL_EXPR_REGEX:
      result=rasqal_expression_evaluate_strmatch(world, locator, e, flags);
      break;

    case RASQAL_EXPR_LITERAL:
      /* flatten any literal to a value as soon as possible - this
       * removes variables from expressions the first time they are seen.
       * (FLATTEN_LITERAL)
       */
      result=rasqal_new_literal_from_literal(rasqal_literal_value(e->literal));
      break;

    case RASQAL_EXPR_FUNCTION:
      rasqal_log_error_simple(world,
                              RAPTOR_LOG_LEVEL_WARN,
                              locator,
                              "No function expressions support at present.  Returning false.");
      result=rasqal_new_boolean_literal(world, 0);
      break;
      
    case RASQAL_EXPR_CAST:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      result=rasqal_literal_cast(l1, e->name, flags, &errs.e);

      rasqal_free_literal(l1);
      if(errs.e)
        goto failed;

      break;

    case RASQAL_EXPR_ORDER_COND_ASC:
    case RASQAL_EXPR_ORDER_COND_DESC:
    case RASQAL_EXPR_GROUP_COND_ASC:
    case RASQAL_EXPR_GROUP_COND_DESC:
    case RASQAL_EXPR_COUNT:
    case RASQAL_EXPR_SUM:
    case RASQAL_EXPR_AVG:
    case RASQAL_EXPR_MIN:
    case RASQAL_EXPR_MAX:
      result = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      break;

    case RASQAL_EXPR_VARSTAR:
      /* constants */
      break;
      
    case RASQAL_EXPR_SAMETERM:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;
      
      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.b = rasqal_literal_same_term(l1, l2);
#if RASQAL_DEBUG > 1
      RASQAL_DEBUG2("rasqal_literal_same_term returned: %d\n", vars.b);
#endif
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;

      result=rasqal_new_boolean_literal(world, vars.b);
      break;
      
    case RASQAL_EXPR_CONCAT:
      vars.sb = raptor_new_stringbuffer();

      for(i = 0; i < raptor_sequence_size(e->args); i++) {
        rasqal_expression *e2; /* do not use vars.e - unioned with vars.sb */
        e2 = (rasqal_expression*)raptor_sequence_get_at(e->args, i);
        if(!e2)
          break;
        
        l1 = rasqal_expression_evaluate(world, locator, e2, flags);
        s = NULL;
        if(l1)
          /* FIXME - check that altering the flags this way to allow
           * concat of URIs is OK 
           */
          s = rasqal_literal_as_string_flags(l1, 
                                             flags & ~RASQAL_COMPARE_XQUERY, 
                                             &errs.e);

        if(!s || errs.e) {
          raptor_free_stringbuffer(vars.sb);
          goto failed;
        }

        raptor_stringbuffer_append_string(vars.sb, s, 1); 
      }

      if(1) {
        size_t len;
        unsigned char* s2;
        
        len = raptor_stringbuffer_length(vars.sb);
        s2 = (unsigned char*)RASQAL_MALLOC(cstring, len + 1);
        if(!s2) {
          raptor_free_stringbuffer(vars.sb);
          goto failed;
        }
        
        if(raptor_stringbuffer_copy_to_string(vars.sb, s2, len)) {
          RASQAL_FREE(cstring, s2);
          raptor_free_stringbuffer(vars.sb);
          goto failed;
        }
        
        raptor_free_stringbuffer(vars.sb);

        /* s2 becomes owned by result */
        result = rasqal_new_string_literal(world, s2, NULL, NULL, NULL);
      }
      
      break;


    case RASQAL_EXPR_COALESCE:
      for(i = 0; i < raptor_sequence_size(e->args); i++) {
        vars.e = (rasqal_expression*)raptor_sequence_get_at(e->args, i);
        result = rasqal_expression_evaluate(world, locator, vars.e, flags);
        if(result)
          break;
      }
      break;

    case RASQAL_EXPR_IF:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      /* IF condition */
      vars.b = rasqal_literal_as_boolean(l1, &errs.e);
      rasqal_free_literal(l1);

      if(errs.e)
        goto failed;

      /* condition is true: evaluate arg2 or false: evaluate arg3 */
      result = rasqal_expression_evaluate(world, locator,
                                          vars.b ? e->arg2 : e->arg3,
                                          flags);
      break;

    case RASQAL_EXPR_URI:
    case RASQAL_EXPR_IRI:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      s = rasqal_literal_as_string_flags(l1, flags, &errs.e);
      if(errs.e) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.dt_uri = raptor_new_uri(world->raptor_world_ptr, s);
      rasqal_free_literal(l1);
      if(!vars.dt_uri)
        goto failed;
      
      result = rasqal_new_uri_literal(world, vars.dt_uri);
      /* vars.dt_uri becomes owned by the result literal */
      break;

    case RASQAL_EXPR_STRLANG:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;
      
      s = rasqal_literal_as_string_flags(l1, flags, &errs.e);
      if(errs.e) {
        rasqal_free_literal(l1);
        goto failed;
      }

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.s = rasqal_literal_as_string_flags(l1, flags, &errs.e);
      if(errs.e) {
        rasqal_free_literal(l1);
        rasqal_free_literal(l2);
        goto failed;
      }

      result = rasqal_new_string_literal(world, s, (const char*)vars.s,
                                         /*datatype */ NULL, /* qname */ NULL);

      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      break;

    case RASQAL_EXPR_STRDT:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;
      
      s = rasqal_literal_as_string_flags(l1, flags, &errs.e);
      if(errs.e) {
        rasqal_free_literal(l1);
        goto failed;
      }

      l2 = rasqal_expression_evaluate(world, locator, e->arg2, flags);
      if(!l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.dt_uri = rasqal_literal_as_uri(l2);
      if(vars.dt_uri) {
        vars.dt_uri = raptor_uri_copy(vars.dt_uri);
      } else {
        const unsigned char *uri_string;
        uri_string = rasqal_literal_as_string_flags(l2, flags, &errs.e);
        if(errs.e) {
          rasqal_free_literal(l1);
          rasqal_free_literal(l2);
          goto failed;
        }
        vars.dt_uri = raptor_new_uri(world->raptor_world_ptr, uri_string);
        if(!vars.dt_uri) {
          rasqal_free_literal(l1);
          rasqal_free_literal(l2);
          goto failed;
        }
      }
      
      result = rasqal_new_string_literal(world, s, /* language */ NULL,
                                         vars.dt_uri, /* qname */ NULL);

      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      break;

    case RASQAL_EXPR_BNODE:
      if(e->arg1) {
        l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
        if(!l1)
          goto failed;

        s = rasqal_literal_as_string_flags(l1, flags, &errs.e);
        if(errs.e) {
          rasqal_free_literal(l1);
          goto failed;
        }
      } else {
        s = rasqal_world_generate_bnodeid(world, NULL);
        if(!s)
          goto failed;
      }

      result = rasqal_new_simple_literal(world, RASQAL_LITERAL_BLANK, s);
      break;

    case RASQAL_EXPR_SAMPLE:
      rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR,
                              locator,
                              "Evaluation of SPARQL SAMPLE() expression is not implemented yet, returning error.");
      errs.e = 1;
      goto failed;
      break;
      
    case RASQAL_EXPR_GROUP_CONCAT:
      rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR,
                              locator,
                              "Evaluation of SPARQL GROUP_CONCAT() expression is not implemented yet, returning error.");
      errs.e = 1;
      goto failed;
      break;

    case RASQAL_EXPR_IN:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      if(1) {
        vars.flags.found = 0;
        for(i = 0; i < raptor_sequence_size(e->args); i++) {
          vars.e = (rasqal_expression*)raptor_sequence_get_at(e->args, i);
          l2 = rasqal_expression_evaluate(world, locator, vars.e, flags);
          if(!l2) {
            rasqal_free_literal(l1);
            goto failed;
          }

          vars.b = (rasqal_literal_equals_flags(l1, l2, flags, &errs.e) != 0);
#if RASQAL_DEBUG > 1
          if(errs.e)
            RASQAL_DEBUG1("rasqal_literal_equals_flags returned: FAILURE\n");
          else
            RASQAL_DEBUG2("rasqal_literal_equals_flags returned: %d\n", vars.b);
#endif
          rasqal_free_literal(l2);
          if(errs.e) {
            rasqal_free_literal(l1);
            goto failed;
          }
          if(vars.b) {
            /* found - so succeeded */
            vars.flags.found = 1;
            break;
          }
        }
        rasqal_free_literal(l1);

        result = rasqal_new_boolean_literal(world, vars.flags.found);
      }
      break;

    case RASQAL_EXPR_NOT_IN:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      if(1) {
        vars.flags.found = 0;
        for(i = 0; i < raptor_sequence_size(e->args); i++) {
          vars.e = (rasqal_expression*)raptor_sequence_get_at(e->args, i);
          l2 = rasqal_expression_evaluate(world, locator, vars.e, flags);
          if(!l2) {
            rasqal_free_literal(l1);
            goto failed;
          }

          vars.b = (rasqal_literal_equals_flags(l1, l2, flags, &errs.e) != 0);
#if RASQAL_DEBUG > 1
          if(errs.e)
            RASQAL_DEBUG1("rasqal_literal_equals_flags returned: FAILURE\n");
          else
            RASQAL_DEBUG2("rasqal_literal_equals_flags returned: %d\n", vars.b);
#endif
          rasqal_free_literal(l2);
          if(errs.e) {
            rasqal_free_literal(l1);
            goto failed;
          }
          if(vars.b) {
            /* found - so failed */
            vars.flags.found = 1;
            break;
          }
        }
        rasqal_free_literal(l1);
        result = rasqal_new_boolean_literal(world, !vars.flags.found);
      }
      break;

    case RASQAL_EXPR_ISNUMERIC:
      errs.flags.free_literal = 1;
      
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;
      
      vars.v = rasqal_literal_as_variable(l1);
      if(vars.v) {
        rasqal_free_literal(l1);
        l1 = vars.v->value; /* don't need vars.v after this */
        errs.flags.free_literal = 0;
        if(!l1)
          goto failed;
      }

      vars.b = (rasqal_literal_is_numeric(l1));

      if(errs.flags.free_literal)
        rasqal_free_literal(l1);

      result = rasqal_new_boolean_literal(world, vars.b);
      break;
      
    case RASQAL_EXPR_YEAR:
    case RASQAL_EXPR_MONTH:
    case RASQAL_EXPR_DAY:
    case RASQAL_EXPR_HOURS:
    case RASQAL_EXPR_MINUTES:
    case RASQAL_EXPR_SECONDS:
    case RASQAL_EXPR_TIMEZONE:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      if(l1->type != RASQAL_LITERAL_DATETIME)
        goto failed;

      if(e->op == RASQAL_EXPR_TIMEZONE) {
        RASQAL_FATAL1("TIMEZONE() not implemented");
        goto failed;
      }

      /* SECONDS accessor has decimal results and includes microseconds */
      if(e->op == RASQAL_EXPR_SECONDS) {
        rasqal_xsd_decimal* dec;
        
        dec = rasqal_xsd_datetime_get_seconds_as_decimal(world,
                                                         l1->value.datetime);
        if(dec) {
          result = rasqal_new_decimal_literal_from_decimal(world, NULL, dec);
          if(!result)
            rasqal_free_xsd_decimal(dec);
        }
        
        if(!result)
          goto failed;
        
        break;
      }

      /* The remain accessors have xsd:integer results */
      if(e->op == RASQAL_EXPR_YEAR)
        vars.i = l1->value.datetime->year;
      else if(e->op == RASQAL_EXPR_MONTH)
        vars.i = l1->value.datetime->month;
      else if(e->op == RASQAL_EXPR_DAY)
        vars.i = l1->value.datetime->day;
      else if(e->op == RASQAL_EXPR_HOURS)
        vars.i = l1->value.datetime->hour;
      else if(e->op == RASQAL_EXPR_MINUTES)
        vars.i = l1->value.datetime->minute;
      else if(e->op == RASQAL_EXPR_SECONDS)
        vars.i = l1->value.datetime->second;
      else
        vars.i = 0;

      result = rasqal_new_integer_literal(world, RASQAL_LITERAL_INTEGER, vars.i);
      break;

    case RASQAL_EXPR_CURRENT_DATETIME:
    case RASQAL_EXPR_NOW:
      result = rasqal_expression_evaluate_now(world, locator, e, flags);
      break;

    case RASQAL_EXPR_TO_UNIXTIME:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      if(l1->type != RASQAL_LITERAL_DATETIME)
        goto failed;

      vars.i = rasqal_xsd_datetime_get_as_unixtime(l1->value.datetime);
      rasqal_free_literal(l1);
      if(!vars.i)
        goto failed;

      result = rasqal_new_integer_literal(world, RASQAL_LITERAL_INTEGER,
                                          vars.i);
      break;

    case RASQAL_EXPR_FROM_UNIXTIME:
      l1 = rasqal_expression_evaluate(world, locator, e->arg1, flags);
      if(!l1)
        goto failed;

      vars.i = rasqal_literal_as_integer(l1, &errs.e);
      rasqal_free_literal(l1);
      if(errs.e)
        goto failed;

      vars.dt = rasqal_new_xsd_datetime_from_unixtime(world, vars.i);
      if(!vars.dt)
        goto failed;

      result = rasqal_new_datetime_literal_from_datetime(world, vars.dt);
      break;

    case RASQAL_EXPR_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown operation %d", e->op);
  }

  got_result:

#ifdef RASQAL_DEBUG
  RASQAL_DEBUG2("result of %p: ", e);
  rasqal_expression_print(e, stderr);
  fputs( ": ", stderr);
  if(result)
    rasqal_literal_print(result, stderr);
  else
    fputs("FAILURE",stderr);
  fputc('\n', stderr);
#endif
  
  return result;

  failed:

  if(result) {
    rasqal_free_literal(result);
    result=NULL;
  }
  goto got_result;
}

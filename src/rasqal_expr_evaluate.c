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

#include "rasqal.h"
#include "rasqal_internal.h"


#define DEBUG_FH stderr

/* Enable evaluation debugging by defining RASQAL_DEBUG_EVAL */
/* #define RASQAL_DEBUG_EVAL 1 */


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
int
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
    char tag_c   = RASQAL_GOOD_CAST(char, tolower(*lang_tag++));
    char range_c = RASQAL_GOOD_CAST(char, tolower(*lang_range++));
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
 * rasqal_expression_evaluate_strdt:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate RASQAL_EXPR_STRDT(expr) expression.
 *
 * Return value: A #rasqal_literal string value or NULL on failure.
 */
static rasqal_literal*
rasqal_expression_evaluate_strdt(rasqal_expression *e,
                                 rasqal_evaluation_context *eval_context,
                                 int *error_p)
{
  rasqal_world* world = eval_context->world;
  rasqal_literal *l1 = NULL;
  rasqal_literal *l2 = NULL;
  const unsigned char* s = NULL;
  unsigned char* new_s = NULL;
  size_t len;
  raptor_uri* dt_uri = NULL;
  
  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if((error_p && *error_p) || !l1)
    goto failed;

  if(l1->language || l1->datatype) {
    /* not a simple literal so return NULL success */
    rasqal_free_literal(l1);
    return NULL;
  }
  
  s = rasqal_literal_as_counted_string(l1, &len, eval_context->flags, error_p);
  if(error_p && *error_p)
    goto failed;
  
  l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
  if((error_p && *error_p) || !l2)
    goto failed;
  
  dt_uri = rasqal_literal_as_uri(l2);
  if(dt_uri) {
    dt_uri = raptor_uri_copy(dt_uri);
  } else {
    const unsigned char *uri_string;
    
    uri_string = rasqal_literal_as_string_flags(l2, eval_context->flags,
                                                error_p);
    if(error_p && *error_p)
      goto failed;

    dt_uri = raptor_new_uri(world->raptor_world_ptr, uri_string);
    if(!dt_uri)
      goto failed;
  }
  
  new_s =RASQAL_MALLOC(unsigned char*, len + 1);
  if(!new_s)
    goto failed;
  memcpy(new_s, s, len + 1);

  rasqal_free_literal(l1);
  rasqal_free_literal(l2);
  
  /* after this new_s and dt_uri become owned by result */
  return rasqal_new_string_literal(world, new_s, /* language */ NULL,
                                   dt_uri, /* qname */ NULL);
  
  
  failed:
  if(error_p)
    *error_p = 1;
  
  if(new_s)
    RASQAL_FREE(char*, new_s);
  if(l1)
    rasqal_free_literal(l1);
  if(l2)
    rasqal_free_literal(l2);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_strlang:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate RASQAL_EXPR_STRLANG(expr) expression.
 *
 * Return value: A #rasqal_literal string value or NULL on failure.
 */
static rasqal_literal*
rasqal_expression_evaluate_strlang(rasqal_expression *e,
                                   rasqal_evaluation_context *eval_context,
                                   int *error_p)
{
  rasqal_world* world = eval_context->world;
  rasqal_literal *l1 = NULL;
  rasqal_literal *l2 = NULL;
  const unsigned char* s = NULL;
  const unsigned char* lang = NULL;
  unsigned char* new_s = NULL;
  unsigned char* new_lang = NULL;
  size_t len;
  size_t lang_len;

  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if((error_p && *error_p) || !l1)
    goto failed;
  
  if(l1->language || l1->datatype) {
    /* not a simple literal so return NULL success */
    rasqal_free_literal(l1);
    return NULL;
  }
  
  s = rasqal_literal_as_counted_string(l1, &len, eval_context->flags, error_p);
  if(error_p && *error_p)
    goto failed;

  l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
  if((error_p && *error_p) || !l2)
    goto failed;
  
  lang = rasqal_literal_as_counted_string(l2, &lang_len, eval_context->flags,
                                          error_p);
  if(error_p && *error_p)
    goto failed;
  
  new_s = RASQAL_MALLOC(unsigned char*, len + 1);
  if(!new_s)
    goto failed;
  memcpy(new_s, s, len + 1);
  
  new_lang = RASQAL_MALLOC(unsigned char*, lang_len + 1);
  if(!new_lang)
    goto failed;
  memcpy(new_lang, lang, lang_len + 1);
  
  rasqal_free_literal(l1);
  rasqal_free_literal(l2);

  /* after this new_s and new_lang become owned by result */
  return rasqal_new_string_literal(world, new_s,
                                   RASQAL_GOOD_CAST(const char*, new_lang),
                                   /*datatype */ NULL, /* qname */ NULL);
  

  failed:
  if(error_p)
    *error_p = 1;
  
  if(new_s)
    RASQAL_FREE(char*, new_s);
  if(l1)
    rasqal_free_literal(l1);
  if(l2)
    rasqal_free_literal(l2);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_istype:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate RASQAL_EXPR_ISBLANK, RASQAL_EXPR_ISURI,
 * RASQAL_EXPR_ISLITERAL and RASQAL_EXPR_ISNUMERIC (expr)
 * expressions.
 *
 * Return value: A #rasqal_literal boolean value or NULL on failure.
 */
static rasqal_literal*
rasqal_expression_evaluate_istype(rasqal_expression *e,
                                  rasqal_evaluation_context *eval_context,
                                  int *error_p)
{
  rasqal_world* world = eval_context->world;
  rasqal_literal* l1;
  int free_literal = 1;
  rasqal_variable *v;
  int b;
  
  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if((error_p && *error_p) || !l1)
    goto failed;

  v = rasqal_literal_as_variable(l1);
  if(v) {
    rasqal_free_literal(l1);

    l1 = v->value; /* don't need v after this */

    free_literal = 0;
    if(!l1)
      goto failed;
  }
  
  if(e->op == RASQAL_EXPR_ISBLANK)
    b = (l1->type == RASQAL_LITERAL_BLANK);
  else if(e->op == RASQAL_EXPR_ISLITERAL)
    b = (rasqal_literal_get_rdf_term_type(l1) == RASQAL_LITERAL_STRING);
  else if(e->op == RASQAL_EXPR_ISURI)
    b =(l1->type == RASQAL_LITERAL_URI);
  else
    b = (rasqal_literal_is_numeric(l1));
  
  if(free_literal)
    rasqal_free_literal(l1);
  
  return rasqal_new_boolean_literal(world, b);

failed:
  if(error_p)
    *error_p = 1;
  
  if(free_literal && l1)
    rasqal_free_literal(l1);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_bound:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate RASQAL_EXPR_BOUND (variable) expressions.
 *
 * Return value: A #rasqal_literal boolean value or NULL on failure.
 */
static rasqal_literal*
rasqal_expression_evaluate_bound(rasqal_expression *e,
                                 rasqal_evaluation_context *eval_context,
                                 int *error_p)
{
  rasqal_world* world = eval_context->world;
  rasqal_literal* l1;
  rasqal_variable* v;
  
  /* Do not use rasqal_expression_evaluate() here since
   * we need to check the argument is a variable, and
   * that function will flatten such thing to literals
   * as early as possible. See (FLATTEN_LITERAL) below
   */
  if(!e->arg1 || e->arg1->op != RASQAL_EXPR_LITERAL)
    goto failed;
  
  l1 = e->arg1->literal;
  if(!l1 || l1->type != RASQAL_LITERAL_VARIABLE)
    goto failed;
  
  v = rasqal_literal_as_variable(l1);
  if(!v)
    goto failed;
  
  return rasqal_new_boolean_literal(world, (v->value != NULL));

  failed:
  if(error_p)
    *error_p = 1;

  return NULL;
}


/* 
 * rasqal_expression_evaluate_if:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate RASQAL_EXPR_IF (condition, true expr, false expr) expressions.
 *
 * Return value: A #rasqal_literal value or NULL on failure.
 */
static rasqal_literal*
rasqal_expression_evaluate_if(rasqal_expression *e,
                              rasqal_evaluation_context *eval_context,
                              int *error_p)
{
  rasqal_literal* l1;
  int b;
  
  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if((error_p && *error_p) || !l1)
    goto failed;

  /* IF condition */
  b = rasqal_literal_as_boolean(l1, error_p);
  rasqal_free_literal(l1);

  if(error_p && *error_p)
    goto failed;

  /* condition is true: evaluate arg2 or false: evaluate arg3 */
  return rasqal_expression_evaluate2(b ? e->arg2 : e->arg3, eval_context,
                                     error_p);

  failed:
  if(error_p)
    *error_p = 1;
  
  return NULL;
}


/* 
 * rasqal_expression_evaluate_sameterm:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate RASQAL_EXPR_SAMETERM (expr1, expr2) expressions.
 *
 * Return value: A #rasqal_literal boolean value or NULL on failure.
 */
static rasqal_literal*
rasqal_expression_evaluate_sameterm(rasqal_expression *e,
                                    rasqal_evaluation_context *eval_context,
                                    int *error_p)
{
  rasqal_world* world = eval_context->world;
  rasqal_literal* l1;
  rasqal_literal* l2;
  int b;

  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if((error_p && *error_p) || !l1)
    goto failed;
  
  l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
  if((error_p && *error_p) || !l2)
    goto failed;

  b = rasqal_literal_same_term(l1, l2);
#ifdef RASQAL_DEBUG_EVAL
  RASQAL_DEBUG2("rasqal_literal_same_term() returned: %d\n", b);
#endif

  rasqal_free_literal(l1);
  rasqal_free_literal(l2);

  return rasqal_new_boolean_literal(world, b);

  failed:
  if(error_p)
    *error_p = 1;
  
  if(l1)
    rasqal_free_literal(l1);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_in_set:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate RASQAL_EXPR_IN and RASQAL_EXPR_NOT_IN (expr,
 * expr list) expression.
 *
 * Return value: A #rasqal_literal boolean value or NULL on failure.
 */
static rasqal_literal*
rasqal_expression_evaluate_in_set(rasqal_expression *e,
                                  rasqal_evaluation_context *eval_context,
                                  int *error_p)
{
  rasqal_world* world = eval_context->world;
  int size = raptor_sequence_size(e->args);
  int i;
  rasqal_literal* l1;
  int found = 0;

  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if((error_p && *error_p) || !l1)
    goto failed;
  
  for(i = 0; i < size; i++) {
    rasqal_expression* arg_e;
    rasqal_literal* arg_literal;
    
    arg_e = (rasqal_expression*)raptor_sequence_get_at(e->args, i);
    arg_literal = rasqal_expression_evaluate2(arg_e, eval_context, error_p);
    if(!arg_literal)
      goto failed;
    
    found = (rasqal_literal_equals_flags(l1, arg_literal, 
                                         eval_context->flags, error_p) != 0);
#ifdef RASQAL_DEBUG_EVAL
    if(error_p && *error_p)
      RASQAL_DEBUG1("rasqal_literal_equals_flags() returned: FAILURE\n");
    else
      RASQAL_DEBUG2("rasqal_literal_equals_flags() returned: %d\n", found);
#endif
    rasqal_free_literal(arg_literal);

    if(error_p && *error_p)
      goto failed;

    if(found)
      /* found - terminate search */
      break;
  }
  rasqal_free_literal(l1);

  if(e->op == RASQAL_EXPR_NOT_IN)
    found = !found;
  return rasqal_new_boolean_literal(world, found);

  failed:
  if(error_p)
    *error_p = 1;
  
  if(l1)
    rasqal_free_literal(l1);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_coalesce:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate RASQAL_EXPR_COALESCE (expr list) expressions.
 *
 * Return value: A #rasqal_literal value or NULL on failure.
 */
static rasqal_literal*
rasqal_expression_evaluate_coalesce(rasqal_expression *e,
                                    rasqal_evaluation_context *eval_context,
                                    int *error_p)
{
  int size = raptor_sequence_size(e->args);
  int i;
  
  for(i = 0; i < size; i++) {
    rasqal_expression* arg_e;

    arg_e = (rasqal_expression*)raptor_sequence_get_at(e->args, i);
    if(arg_e) {
      int my_error = 0;
      rasqal_literal* result;
      result = rasqal_expression_evaluate2(arg_e, eval_context, &my_error);
      if(!my_error && result)
        return result;
    }
  }
  
  /* No arguments evaluated to an RDF term, return an error (NULL) */
  return NULL;
}


/* 
 * rasqal_expression_evaluate_str:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate RASQAL_EXPR_STR (literal expr) expression.
 *
 * Return value: A #rasqal_literal value or NULL on failure.
 */
static rasqal_literal*
rasqal_expression_evaluate_str(rasqal_expression *e,
                               rasqal_evaluation_context *eval_context,
                               int *error_p)
{
  rasqal_world* world = eval_context->world;
  rasqal_literal* l1 = NULL;
  rasqal_literal* result = NULL;
  const unsigned char *s;
  size_t len;
  unsigned char *new_s;
  
  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if((error_p && *error_p) || !l1)
    goto failed;
  
  /* Note: flags removes RASQAL_COMPARE_XQUERY as this is the
   * explicit stringify operation and we want URIs as strings.
   */
  s = rasqal_literal_as_counted_string(l1, &len,
                                       (eval_context->flags & ~RASQAL_COMPARE_XQUERY),
                                       error_p);
  if((error_p && *error_p) || !s)
    goto failed;
  
  new_s = RASQAL_MALLOC(unsigned char*, len + 1);
  if(!new_s)
    goto failed;

  memcpy(RASQAL_GOOD_CAST(char*, new_s), RASQAL_GOOD_CAST(const char*, s), len + 1);
  
  /* after this new_s is owned by result */
  result = rasqal_new_string_literal(world, new_s, NULL, NULL, NULL);

  if(l1)
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
 * rasqal_expression_evaluate_lang:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate RASQAL_EXPR_LANG (literal expr) expression.
 *
 * Return value: A #rasqal_literal value or NULL on failure.
 */
static rasqal_literal*
rasqal_expression_evaluate_lang(rasqal_expression *e,
                                rasqal_evaluation_context *eval_context,
                                int *error_p)
{
  rasqal_world* world = eval_context->world;
  rasqal_literal* l1;
  int free_literal = 1;
  rasqal_variable* v = NULL;
  unsigned char* new_s;
      
  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if((error_p && *error_p) || !l1)
    goto failed;

  v = rasqal_literal_as_variable(l1);
  if(v) {
    rasqal_free_literal(l1);

    l1 = v->value; /* don't need v after this */

    free_literal = 0;
    if(!l1)
      goto failed;
  }
  
  if(rasqal_literal_get_rdf_term_type(l1) != RASQAL_LITERAL_STRING)
    goto failed;
  
  if(l1->language) {
    size_t len = strlen(l1->language);
    new_s = RASQAL_MALLOC(unsigned char*, len + 1);
    if(!new_s)
      goto failed;

    memcpy(RASQAL_GOOD_CAST(char*, new_s), l1->language, len + 1);
  } else  {
    new_s = RASQAL_MALLOC(unsigned char*, 1);
    if(!new_s)
      goto failed;

    *new_s = '\0';
  }

  if(free_literal)
    rasqal_free_literal(l1);

  /* after this new_s is owned by result */
  return rasqal_new_string_literal(world, new_s, NULL, NULL, NULL);

  failed:
  if(error_p)
    *error_p = 1;
  
  if(free_literal)
    rasqal_free_literal(l1);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_datatype:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate RASQAL_EXPR_DATATYPE (string literal) expression.
 *
 * Return value: A #rasqal_literal URI value or NULL on failure.
 */
static rasqal_literal*
rasqal_expression_evaluate_datatype(rasqal_expression *e,
                                    rasqal_evaluation_context *eval_context,
                                    int *error_p)
{
  rasqal_world* world = eval_context->world;
  rasqal_literal* l1;
  int free_literal = 1;
  rasqal_variable* v = NULL;
  raptor_uri* dt_uri = NULL;
      
  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if((error_p && *error_p) || !l1)
    goto failed;

  v = rasqal_literal_as_variable(l1);
  if(v) {
    rasqal_free_literal(l1);

    l1 = v->value; /* don't need v after this */

    free_literal = 0;
    if(!l1)
      goto failed;
  }
  
  if(rasqal_literal_get_rdf_term_type(l1) != RASQAL_LITERAL_STRING)
    goto failed;
  
  if(l1->language)
    goto failed;
  
  /* The datatype of a plain literal is xsd:string */
  dt_uri = l1->datatype;
  if(!dt_uri && l1->type == RASQAL_LITERAL_STRING)
    dt_uri = rasqal_xsd_datatype_type_to_uri(l1->world,
                                             RASQAL_LITERAL_XSD_STRING);
  
  if(!dt_uri)
    goto failed;
  
  dt_uri = raptor_uri_copy(dt_uri);

  if(free_literal)
    rasqal_free_literal(l1);

  /* after this dt_uri is owned by result */
  return rasqal_new_uri_literal(world, dt_uri);

  failed:
  if(error_p)
    *error_p = 1;
  
  if(free_literal)
    rasqal_free_literal(l1);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_uri_constructor:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate RASQAL_EXPR_URI and RASQAL_EXPR_IRI (string)
 * expressions.
 *
 * Return value: A #rasqal_literal URI value or NULL on failure.
 */
static rasqal_literal*
rasqal_expression_evaluate_uri_constructor(rasqal_expression *e,
                                           rasqal_evaluation_context *eval_context,
                                           int *error_p)
{
  rasqal_world* world = eval_context->world;
  rasqal_literal* l1;
  raptor_uri* dt_uri;
  const unsigned char *s;

  l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
  if((error_p && *error_p) || !l1)
    goto failed;
  
  s = rasqal_literal_as_string_flags(l1, eval_context->flags, error_p);
  if(error_p && *error_p)
    goto failed;
  
  dt_uri = raptor_new_uri_relative_to_base(world->raptor_world_ptr,
                                           eval_context->base_uri,
                                           s);
  rasqal_free_literal(l1); l1 = NULL;
  
  if(!dt_uri)
    goto failed;
  
  /* after this dt_uri is owned by the result literal */
  return rasqal_new_uri_literal(world, dt_uri);

  failed:
  if(error_p)
    *error_p = 1;
  
  if(l1)
    rasqal_free_literal(l1);

  return NULL;
}


/* 
 * rasqal_expression_evaluate_bnode_constructor:
 * @e: The expression to evaluate.
 * @eval_context: Evaluation context
 *
 * INTERNAL - Evaluate RASQAL_EXPR_BNODE (string) expression.
 *
 * Return value: A #rasqal_literal blank node value or NULL on failure.
 */
static rasqal_literal*
rasqal_expression_evaluate_bnode_constructor(rasqal_expression *e,
                                             rasqal_evaluation_context *eval_context,
                                             int *error_p)
{
  rasqal_world* world = eval_context->world;
  rasqal_literal *l1 = NULL;
  unsigned char *new_s = NULL;

  if(e->arg1) {
    const unsigned char *s;
    size_t len;
    
    l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
    if((error_p && *error_p) || !l1)
      goto failed;
    
    s = rasqal_literal_as_counted_string(l1, &len, eval_context->flags,
                                         error_p);
    if(error_p && *error_p)
      goto failed;

    new_s = RASQAL_MALLOC(unsigned char*, len + 1);
    if(!new_s)
      goto failed;

    memcpy(RASQAL_GOOD_CAST(char*, new_s), s, len + 1);

    rasqal_free_literal(l1);
  } else {
    new_s = rasqal_world_generate_bnodeid(world, NULL);
    if(!new_s)
      goto failed;
  }

  /* after this new_s is owned by the result */
  return rasqal_new_simple_literal(world, RASQAL_LITERAL_BLANK, new_s);

  failed:
  if(error_p)
    *error_p = 1;
  
  if(l1)
    rasqal_free_literal(l1);

  return NULL;
}


/**
 * rasqal_expression_evaluate2:
 * @e: The expression to evaluate.
 * @eval_context: expression context
 * @error_p: pointer to error return flag
 * 
 * Evaluate a #rasqal_expression tree in the context of a
 * #rasqal_evaluation_context to give a #rasqal_literal result or error.
 * 
 * Return value: a #rasqal_literal value or NULL (a valid value).  @error_p is set to non-0 on failure.  
 **/
rasqal_literal*
rasqal_expression_evaluate2(rasqal_expression* e,
                            rasqal_evaluation_context* eval_context,
                            int *error_p)
{
  rasqal_world *world;
  int flags;
  rasqal_literal* result = NULL;
  rasqal_literal *l1;
  rasqal_literal *l2;
  
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

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(e, rasqal_expression, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(eval_context, rasqal_evaluation_context, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(error_p, intp, NULL);

  world = eval_context->world;
  flags = eval_context->flags;

  errs.e = 0;

#ifdef RASQAL_DEBUG_EVAL
  RASQAL_DEBUG2("evaluating expression %p: ", e);
  rasqal_expression_print(e, stderr);
  fprintf(stderr, "\n");
#endif
  
  switch(e->op) {
    case RASQAL_EXPR_AND:
      errs.errs.e1 = 0;
      l1 = rasqal_expression_evaluate2(e->arg1, eval_context, &errs.errs.e1);
      if(errs.errs.e1) {
        vars.bools.b1 = 0;
      } else {
        vars.bools.b1 = rasqal_literal_as_boolean(l1, &errs.errs.e1);
        rasqal_free_literal(l1);
      }

      errs.errs.e2 = 0;
      l1 = rasqal_expression_evaluate2(e->arg2, eval_context, &errs.errs.e2);
      if(errs.errs.e2) {
        vars.bools.b2 = 0;
      } else {
        vars.bools.b2 = rasqal_literal_as_boolean(l1, &errs.errs.e2);
        rasqal_free_literal(l1);
      }

      /* See http://www.w3.org/TR/2005/WD-rdf-sparql-query-20051123/#truthTable */
      if(!errs.errs.e1 && !errs.errs.e2) {
        /* No type error, answer is A && B */
        vars.b = vars.bools.b1 && vars.bools.b2; /* don't need b1,b2 anymore */
      } else {
        if((!vars.bools.b1 && errs.errs.e2) || (errs.errs.e1 && vars.bools.b2))
          /* F && E => F.   E && F => F. */
          vars.b = 0;
        else
          /* Otherwise E */
          goto failed;
      }

      result = rasqal_new_boolean_literal(world, vars.b);
      break;
      
    case RASQAL_EXPR_OR:
      errs.errs.e1 = 0;
      l1 = rasqal_expression_evaluate2(e->arg1, eval_context, &errs.errs.e1);
      if(errs.errs.e1) {
        vars.bools.b1 = 0;
      } else {
        vars.bools.b1 = rasqal_literal_as_boolean(l1, &errs.errs.e1);
        rasqal_free_literal(l1);
      }

      errs.errs.e2 = 0;
      l1 = rasqal_expression_evaluate2(e->arg2, eval_context, &errs.errs.e2);
      if(errs.errs.e2) {
        vars.bools.b2 = 0;
      } else {
        vars.bools.b2 = rasqal_literal_as_boolean(l1, &errs.errs.e2);
        rasqal_free_literal(l1);
      }

      /* See http://www.w3.org/TR/2005/WD-rdf-sparql-query-20051123/#truthTable */
      if(!errs.errs.e1 && !errs.errs.e2) {
        /* No type error, answer is A || B */
        vars.b = vars.bools.b1 || vars.bools.b2; /* don't need b1,b2 anymore */
      } else {
        if((vars.bools.b1 && errs.errs.e2) || (errs.errs.e1 && vars.bools.b2))
          /* T || E => T.   E || T => T */
          vars.b = 1;
        else
          /* Otherwise E */
          goto failed;
      }

      result = rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_EQ:
      l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
      if((error_p && *error_p) || !l1)
        goto failed;

      l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
      if((error_p && *error_p) || !l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      /* FIXME - this should probably be checked at literal creation
       * time
       */
      if(!rasqal_xsd_datatype_check(l1->type, l1->string, flags) ||
         !rasqal_xsd_datatype_check(l2->type, l2->string, flags)) {
#ifdef RASQAL_DEBUG_EVAL
        RASQAL_DEBUG1("One of the literals was invalid\n");
#endif
        goto failed;
      }

      vars.b = (rasqal_literal_equals_flags(l1, l2, flags, &errs.e) != 0);
#ifdef RASQAL_DEBUG_EVAL
      if(errs.e)
        RASQAL_DEBUG1("rasqal_literal_equals_flags returned: FAILURE\n");
      else
        RASQAL_DEBUG2("rasqal_literal_equals_flags returned: %d\n", vars.b);
#endif
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;

      result = rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_NEQ:
      l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
      if((error_p && *error_p) || !l1)
        goto failed;

      l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
      if((error_p && *error_p) || !l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.b = (rasqal_literal_not_equals_flags(l1, l2, flags, &errs.e) != 0);
#ifdef RASQAL_DEBUG_EVAL
      if(errs.e)
        RASQAL_DEBUG1("rasqal_literal_not_equals_flags returned: FAILURE\n");
      else
        RASQAL_DEBUG2("rasqal_literal_not_equals_flags returned: %d\n", vars.b);
#endif
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;

      result = rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_LT:
      l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
      if((error_p && *error_p) || !l1)
        goto failed;

      l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
      if((error_p && *error_p) || !l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.b = (rasqal_literal_compare(l1, l2, flags, &errs.e) < 0);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;

      result = rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_GT:
      l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
      if((error_p && *error_p) || !l1)
        goto failed;

      l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
      if((error_p && *error_p) || !l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.b = (rasqal_literal_compare(l1, l2, flags, &errs.e) > 0);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;

      result = rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_LE:
      l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
      if((error_p && *error_p) || !l1)
        goto failed;

      l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
      if((error_p && *error_p) || !l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.b = (rasqal_literal_compare(l1, l2, flags, &errs.e) <= 0);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;
      
      result = rasqal_new_boolean_literal(world, vars.b);
      break;        

    case RASQAL_EXPR_GE:
      l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
      if((error_p && *error_p) || !l1)
        goto failed;

      l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
      if((error_p && *error_p) || !l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.b = (rasqal_literal_compare(l1, l2, flags, &errs.e) >= 0);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;

      result = rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_UMINUS:
      l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
      if((error_p && *error_p) || !l1)
        goto failed;

      result = rasqal_literal_negate(l1, &errs.e);
      rasqal_free_literal(l1);
      if(errs.e)
        goto failed;
      break;

    case RASQAL_EXPR_BOUND:
      result = rasqal_expression_evaluate_bound(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_STR:
      result = rasqal_expression_evaluate_str(e, eval_context, error_p);
      break;
      
    case RASQAL_EXPR_LANG:
      result = rasqal_expression_evaluate_lang(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_LANGMATCHES:
      result = rasqal_expression_evaluate_langmatches(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_DATATYPE:
      result = rasqal_expression_evaluate_datatype(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_ISURI:
    case RASQAL_EXPR_ISBLANK:
    case RASQAL_EXPR_ISLITERAL:
    case RASQAL_EXPR_ISNUMERIC:
      result = rasqal_expression_evaluate_istype(e, eval_context, error_p);
      break;
      
    case RASQAL_EXPR_PLUS:
      l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
      if((error_p && *error_p) || !l1)
        goto failed;

      l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
      if((error_p && *error_p) || !l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      result = rasqal_literal_add(l1, l2, &errs.e);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;
      
      break;

    case RASQAL_EXPR_MINUS:
      l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
      if((error_p && *error_p) || !l1)
        goto failed;

      l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
      if((error_p && *error_p) || !l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      result = rasqal_literal_subtract(l1, l2, &errs.e);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;
      
      break;
      
    case RASQAL_EXPR_STAR:
      l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
      if((error_p && *error_p) || !l1)
        goto failed;

      l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
      if((error_p && *error_p) || !l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      result = rasqal_literal_multiply(l1, l2, &errs.e);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;
      
      break;
      
    case RASQAL_EXPR_SLASH:
      l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
      if((error_p && *error_p) || !l1)
        goto failed;

      l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
      if((error_p && *error_p) || !l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      result = rasqal_literal_divide(l1, l2, &errs.e);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;
      
      break;
      
    case RASQAL_EXPR_REM:
      l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
      if((error_p && *error_p) || !l1)
        goto failed;

      l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
      if((error_p && *error_p) || !l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.i = rasqal_literal_as_integer(l2, &errs.errs.e2);
      /* error if divisor is zero */
      if(!vars.i)
        errs.errs.e2 = 1;
      else
        vars.i = rasqal_literal_as_integer(l1, &errs.errs.e1) % vars.i;

      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.errs.e1 || errs.errs.e2)
        goto failed;

      result = rasqal_new_integer_literal(world, RASQAL_LITERAL_INTEGER, vars.i);
      break;
      
    case RASQAL_EXPR_STR_EQ:
      l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
      if((error_p && *error_p) || !l1)
        goto failed;

      l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
      if((error_p && *error_p) || !l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.b = (rasqal_literal_compare(l1, l2, flags | RASQAL_COMPARE_NOCASE,
                                       &errs.e) == 0);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;

      result = rasqal_new_boolean_literal(world, vars.b);
      break;
      
    case RASQAL_EXPR_STR_NEQ:
      l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
      if((error_p && *error_p) || !l1)
        goto failed;

      l2 = rasqal_expression_evaluate2(e->arg2, eval_context, error_p);
      if((error_p && *error_p) || !l2) {
        rasqal_free_literal(l1);
        goto failed;
      }

      vars.b = (rasqal_literal_compare(l1, l2, flags | RASQAL_COMPARE_NOCASE, 
                                       &errs.e) != 0);
      rasqal_free_literal(l1);
      rasqal_free_literal(l2);
      if(errs.e)
        goto failed;

      result = rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_TILDE:
      l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
      if((error_p && *error_p) || !l1)
        goto failed;

      vars.i= ~ rasqal_literal_as_integer(l1, &errs.e);
      rasqal_free_literal(l1);
      if(errs.e)
        goto failed;

      result = rasqal_new_integer_literal(world, RASQAL_LITERAL_INTEGER, vars.i);
      break;

    case RASQAL_EXPR_BANG:
      l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
      if((error_p && *error_p) || !l1)
        goto failed;

      vars.b = ! rasqal_literal_as_boolean(l1, &errs.e);
      rasqal_free_literal(l1);
      if(errs.e)
        goto failed;

      result = rasqal_new_boolean_literal(world, vars.b);
      break;

    case RASQAL_EXPR_STR_MATCH:
    case RASQAL_EXPR_STR_NMATCH:
    case RASQAL_EXPR_REGEX:
      result = rasqal_expression_evaluate_strmatch(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_LITERAL:
      /* flatten any literal to a value as soon as possible - this
       * removes variables from expressions the first time they are seen.
       * (FLATTEN_LITERAL)
       */
      result = rasqal_new_literal_from_literal(rasqal_literal_value(e->literal));
      break;

    case RASQAL_EXPR_FUNCTION:
      rasqal_log_warning_simple(world, RASQAL_WARNING_LEVEL_NOT_IMPLEMENTED,
                                eval_context->locator,
                                "No function expressions support at present.  Returning false.");
      result = rasqal_new_boolean_literal(world, 0);
      break;
      
    case RASQAL_EXPR_CAST:
      l1 = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
      if((error_p && *error_p) || !l1)
        goto failed;

      result = rasqal_literal_cast(l1, e->name, flags, &errs.e);

      rasqal_free_literal(l1);
      if(errs.e)
        goto failed;

      break;

    case RASQAL_EXPR_ORDER_COND_ASC:
    case RASQAL_EXPR_ORDER_COND_DESC:
    case RASQAL_EXPR_GROUP_COND_ASC:
    case RASQAL_EXPR_GROUP_COND_DESC:
      result = rasqal_expression_evaluate2(e->arg1, eval_context, error_p);
      break;

    case RASQAL_EXPR_COUNT:
    case RASQAL_EXPR_SUM:
    case RASQAL_EXPR_AVG:
    case RASQAL_EXPR_MIN:
    case RASQAL_EXPR_MAX:
    case RASQAL_EXPR_SAMPLE:
    case RASQAL_EXPR_GROUP_CONCAT:
      rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR,
                              eval_context->locator,
                              "Aggregate expressions cannot be evaluated in a general scalar expression.");
      errs.e = 1;
      goto failed;

    case RASQAL_EXPR_VARSTAR:
      /* constants */
      break;
      
    case RASQAL_EXPR_SAMETERM:
      result = rasqal_expression_evaluate_sameterm(e, eval_context, error_p);
      break;
      
    case RASQAL_EXPR_CONCAT:
      result = rasqal_expression_evaluate_concat(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_COALESCE:
      result = rasqal_expression_evaluate_coalesce(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_IF:
      result = rasqal_expression_evaluate_if(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_URI:
    case RASQAL_EXPR_IRI:
      result = rasqal_expression_evaluate_uri_constructor(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_STRLANG:
      result = rasqal_expression_evaluate_strlang(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_STRDT:
      result = rasqal_expression_evaluate_strdt(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_BNODE:
      result = rasqal_expression_evaluate_bnode_constructor(e, eval_context, error_p);
      break;
      
    case RASQAL_EXPR_IN:
      result = rasqal_expression_evaluate_in_set(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_NOT_IN:
      result = rasqal_expression_evaluate_in_set(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_YEAR:
    case RASQAL_EXPR_MONTH:
    case RASQAL_EXPR_DAY:
    case RASQAL_EXPR_HOURS:
    case RASQAL_EXPR_MINUTES:
    case RASQAL_EXPR_SECONDS:
      result = rasqal_expression_evaluate_datetime_part(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_CURRENT_DATETIME:
    case RASQAL_EXPR_NOW:
      result = rasqal_expression_evaluate_now(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_TO_UNIXTIME:
      result = rasqal_expression_evaluate_to_unixtime(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_FROM_UNIXTIME:
      result = rasqal_expression_evaluate_from_unixtime(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_RAND:
      result = rasqal_expression_evaluate_rand(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_STRLEN:
      result = rasqal_expression_evaluate_strlen(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_UCASE:
    case RASQAL_EXPR_LCASE:
      result = rasqal_expression_evaluate_set_case(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_STRSTARTS:
    case RASQAL_EXPR_STRENDS:
    case RASQAL_EXPR_CONTAINS:
      result = rasqal_expression_evaluate_str_prefix_suffix(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_TIMEZONE:
      result = rasqal_expression_evaluate_datetime_timezone(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_TZ:
      result = rasqal_expression_evaluate_datetime_tz(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_ENCODE_FOR_URI:
      result = rasqal_expression_evaluate_encode_for_uri(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_SUBSTR:
      result = rasqal_expression_evaluate_substr(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_STRBEFORE:
      result = rasqal_expression_evaluate_strbefore(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_STRAFTER:
      result = rasqal_expression_evaluate_strafter(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_REPLACE:
      result = rasqal_expression_evaluate_replace(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_ABS:
      result = rasqal_expression_evaluate_abs(e, eval_context, error_p);
      break;
      
    case RASQAL_EXPR_ROUND:
      result = rasqal_expression_evaluate_round(e, eval_context, error_p);
      break;
      
    case RASQAL_EXPR_CEIL:
      result = rasqal_expression_evaluate_ceil(e, eval_context, error_p);
      break;
      
    case RASQAL_EXPR_FLOOR:
      result = rasqal_expression_evaluate_floor(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_MD5:
    case RASQAL_EXPR_SHA1:
    case RASQAL_EXPR_SHA224:
    case RASQAL_EXPR_SHA256:
    case RASQAL_EXPR_SHA384:
    case RASQAL_EXPR_SHA512:
      result = rasqal_expression_evaluate_digest(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_UUID:
      result = rasqal_expression_evaluate_uriuuid(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_STRUUID:
      result = rasqal_expression_evaluate_struuid(e, eval_context, error_p);
      break;

    case RASQAL_EXPR_UNKNOWN:
    default:
      RASQAL_FATAL3("Unknown operation %s (%u)",
                    rasqal_expression_op_label(e->op), e->op);
  }

  got_result:

#ifdef RASQAL_DEBUG_EVAL
  RASQAL_DEBUG2("result of %p: ", e);
  rasqal_expression_print(e, stderr);
  fputs( ": ", stderr);
  if(error_p && *error_p)
    fputs("FAILURE",stderr);
  else
    rasqal_literal_print(result, stderr);
  fputc('\n', stderr);
#endif
  
  return result;

  failed:
  *error_p = 1;
  
  if(result) {
    rasqal_free_literal(result);
    result = NULL;
  }
  goto got_result;
}


#ifndef RASQAL_DISABLE_DEPRECATED
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
 * @Deprecated: use rasqal_expression_evaluate2() using a #rasqal_evaluation_context
 *
 * Return value: a #rasqal_literal value or NULL on failure.
 **/
rasqal_literal*
rasqal_expression_evaluate(rasqal_world *world, raptor_locator *locator,
                           rasqal_expression* e, int flags)
{
  rasqal_evaluation_context context; /* static */
  int error = 0;
  rasqal_literal* l;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(e, rasqal_expression, NULL);

  memset(&context, '\0', sizeof(context));
  context.world = world;
  context.locator = locator;
  context.flags = flags;

  l = rasqal_expression_evaluate2(e, &context, &error);
  if(error)
    return NULL;
  
  return l;
}
#endif

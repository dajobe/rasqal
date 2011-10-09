/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_regex.c - Rasqal regex support
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


#ifndef STANDALONE

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
rasqal_regex_replace_pcre(rasqal_world* world, raptor_locator* locator,
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
rasqal_regex_replace_posix(rasqal_world* world, raptor_locator* locator,
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



/**
 * rasqal_regex_replace:
 * @world: world
 * @locator: locator
 * @options: regex flags string
 * @subject: input string
 * @subject_len: input string length
 * @replace: replacement string
 * @replace_len: Length of replacement string
 * @result_len_p: pointer to store result length (output)
 *
 * INTERNAL - SPARQL 1.1 REPLACE() implementation using PCRE
 *
 * Return value: result string or NULL on failure
 *
 */
char*
rasqal_regex_replace(rasqal_world* world, raptor_locator* locator,
                      const char* pattern,
                      const char* regex_flags,
                      const char* subject, size_t subject_len,
                      const char* replace, size_t replace_len,
                      size_t* result_len_p) 
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
  int options = 0;
  int rc = 0;
#endif
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
    result_s = rasqal_regex_replace_pcre(world, locator,
                                         re, options,
                                         subject, subject_len,
                                         replace, replace_len,
                                         result_len_p);
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
                            "Regex compile of '%s' failed - %d", pattern, rc);
  } else
    result_s = rasqal_regex_replace_posix(world, locator,
                                          reg, options,
                                          subject, subject_len,
                                          replace, replace_len,
                                          result_len_p);
  regfree(&reg);
#endif

#ifdef RASQAL_REGEX_NONE
  rasqal_log_warning_simple(world, RASQAL_WARNING_LEVEL_MISSING_SUPPORT,
                            locator,
                            "Regex support missing, cannot replace '%s' from '%s' to '%s'", subject, pattern, replace);
#endif

  return result_s;
}

#endif


#ifdef STANDALONE
#include <stdio.h>

int main(int argc, char *argv[]);


int
main(int argc, char *argv[])
{
  return 0;
}
#endif

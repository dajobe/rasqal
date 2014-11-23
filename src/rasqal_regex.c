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
 * rasqal_regex_match:
 * @world: world
 * @locator: locator
 * @pattern: regex pattern
 * @regex_flags: regex flags string
 * @subject: input string
 * @subject_len: input string length
 *
 * INTERNAL - Test if a string matches a regex pattern.
 *
 * Intended to be used for executing #RASQAL_EXPR_STR_MATCH and
 * #RASQAL_EXPR_STR_NMATCH operations (unused: formerly RDQL)
 *
 * Return value: <0 on error, 0 for no match, >0 for match
 *
 */
int
rasqal_regex_match(rasqal_world* world, raptor_locator* locator,
                   const char* pattern,
                   const char* regex_flags,
                   const char* subject, size_t subject_len)
{
  int flag_i = 0; /* regex_flags contains i */
  const char *p;
#ifdef RASQAL_REGEX_PCRE
  pcre* re;
  int compile_options = PCRE_UTF8;
  int exec_options = 0;
  const char *re_error = NULL;
  int erroffset = 0;
#endif
#ifdef RASQAL_REGEX_POSIX
  regex_t reg;
  int compile_options = REG_EXTENDED;
  int exec_options = 0;
#endif
  int rc = 0;

  for(p = regex_flags; p && *p; p++)
    if(*p == 'i')
      flag_i++;
      
#ifdef RASQAL_REGEX_PCRE
  if(flag_i)
    compile_options |= PCRE_CASELESS;
    
  re = pcre_compile(RASQAL_GOOD_CAST(const char*, pattern), compile_options,
                    &re_error, &erroffset, NULL);
  if(!re) {
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, locator,
                            "Regex compile of '%s' failed - %s", pattern, re_error);
    rc = -1;
  } else {
    rc = pcre_exec(re, 
                   NULL, /* no study */
                   subject,
                   RASQAL_BAD_CAST(int, subject_len), /* PCRE API is an int */
                   0 /* startoffset */,
                   exec_options /* options */,
                   NULL, 0 /* ovector, ovecsize - no matches wanted */
                   );
    if(rc >= 0)
      rc = 1;
    else if(rc != PCRE_ERROR_NOMATCH) {
      rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, locator,
                              "Regex match failed - returned code %d", rc);
      rc= -1;
    } else
      rc = 0;
  }
  pcre_free(re);
  
#endif
    
#ifdef RASQAL_REGEX_POSIX
  if(flag_i)
    compile_options |= REG_ICASE;
    
  rc = regcomp(&reg, RASQAL_GOOD_CAST(const char*, pattern), compile_options);
  if(rc) {
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR,
                            locator,
                            "Regex compile of '%s' failed", pattern);
    rc = -1;
  } else {
    rc = regexec(&reg, RASQAL_GOOD_CAST(const char*, subject),
                 0, NULL, /* nmatch, regmatch_t pmatch[] - no matches wanted */
                 exec_options /* eflags */
                 );
    if(!rc)
      rc = 1;
    else if (rc != REG_NOMATCH) {
      rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, locator,
                              "Regex match failed - returned code %d", rc);
      rc = -1;
    } else
      rc = 0;
  }
  regfree(&reg);
#endif

#ifdef RASQAL_REGEX_NONE
  rasqal_log_warning_simple(world, RASQAL_WARNING_LEVEL_MISSING_SUPPORT, locator,
                            "Regex support missing, cannot compare '%s' to '%s'",
                            match_string, pattern);
  rc = -1;
#endif

  return rc;
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
rasqal_regex_replace_pcre(rasqal_world* world, raptor_locator* locator,
                          pcre* re, int options,
                          const char *subject, size_t subject_len,
                          const char *replace, size_t replace_len,
                          size_t *result_len_p)
{
  int capture_count;
  int *ovector = NULL;
  int ovecsize;
  size_t startoffset;
  int matched_empty_options;
  char *result = NULL;
  size_t result_size; /* allocated size of result (excluding NUL) */
  size_t result_len; /* used size of result */
  const char *replace_end = replace + replace_len;

  if(pcre_fullinfo(re, NULL, PCRE_INFO_CAPTURECOUNT, &capture_count) < 0)
    goto failed;

  ovecsize = (capture_count + 1) * 3; /* +1 for whole pattern match pair */
  ovector = RASQAL_CALLOC(int *, RASQAL_GOOD_CAST(size_t, ovecsize), sizeof(int));
  if(!ovector)
    goto failed;

  result_size = subject_len << 1;
  result = RASQAL_MALLOC(char*, result_size + 1);
  if(!result)
    goto failed;
  result_len = 0;

  /* Match and replace loop; adjusting startoffset each time */
  startoffset = 0;
  matched_empty_options = 0;
  while(1) {
    int stringcount;
    const char *subject_piece = subject + startoffset;

    stringcount = pcre_exec(re,
                            NULL, /* no study */
                            subject,
                            RASQAL_BAD_CAST(int, subject_len), /* PCRE API is an int */
                            RASQAL_BAD_CAST(int, startoffset),
                            options | matched_empty_options,
                            ovector, ovecsize);

    /* "The value returned by pcre_exec() is one more than the
     * highest numbered pair that has been set. ...  If there are no
     * capturing subpatterns, the return value from a successful
     * match is 1, indicating that just the first pair of offsets has
     * been set." - pcreapi
     */

    if(!stringcount)
      /* ovector was too small - how can this happen?.  Use all
       * the variables available.  Should return an warning? FIXME
       */
      stringcount = ovecsize / 3;
    

    if(stringcount > 0) {
      /* matches have been found */
      const char *subject_match;
      size_t piece_len;
      size_t new_result_len;
      const char *replace_p;
      char last_char;
      char *result_p;

      subject_match = subject + ovector[0];

      /* compute new length of replacement with expanded variables */
      new_result_len = result_len;

      /* compute size of piece before the match */
      piece_len = RASQAL_GOOD_CAST(size_t, subject_match - subject_piece);
      new_result_len += piece_len;

      /* compute size of matched piece */
      replace_p = replace;
      last_char = '\0';
      while(replace_p < replace_end) {
        if(*replace_p == '\\' || *replace_p == '$') {
          int ref_number;

          if(last_char == '\\') {
            /* Allow \\ and \$ */
            replace_p++;
            last_char = '\0';
            continue;
          }

          ref_number = rasqal_regex_get_ref_number(&replace_p);
          if(ref_number >= 0) {
            if(ref_number < stringcount)
              new_result_len = new_result_len + RASQAL_GOOD_CAST(size_t, ovector[(ref_number << 1) + 1] - ovector[ref_number << 1]);
            continue;
          }
        }

        new_result_len++;

        last_char = *replace_p;
        replace_p++;
      }

      /* need to expand result buffer? */
      if(new_result_len > result_size) {
        char* new_result;

        result_size += new_result_len << 1;
        new_result = RASQAL_MALLOC(char*, result_size + 1);
        if(!new_result)
          goto failed;

        memcpy(new_result, result, result_len);
        RASQAL_FREE(char*, result);
        result = new_result;
      }

      /* copy the piece of the input before the match */
      piece_len = RASQAL_GOOD_CAST(size_t, subject_match - subject_piece);
      memcpy(&result[result_len], subject_piece, piece_len);
      result_len += piece_len;

      /* copy replacement into result inserting matched references */
      result_p = result + result_len;
      replace_p = replace;
      last_char = '\0';
      while(replace_p < replace_end) {
        if(*replace_p == '\\' || *replace_p == '$') {
          int ref_number;

          if(last_char == '\\') {
            /* Allow \\ and \$ */
            *(result_p - 1) = *replace_p++;
            last_char = '\0';
            continue;
          }

          ref_number = rasqal_regex_get_ref_number(&replace_p);
          if(ref_number >= 0) {
            if(ref_number < stringcount) {
              size_t match_len;
              int match_start_offset = ovector[ref_number << 1];
              
              match_len = RASQAL_BAD_CAST(size_t, ovector[(ref_number << 1) + 1] - match_start_offset);
              memcpy(result_p, subject + match_start_offset, match_len);
              result_p += match_len;
              result_len += match_len;
            }
            continue;
          }
        }

        *result_p++ = *replace_p;
        result_len++;
        
        last_char = *replace_p;
        replace_p++;
      }
      *result_p = '\0';

      /* continue at offset after all matches */
      startoffset = RASQAL_BAD_CAST(size_t, ovector[1]);
      
      /*
       * "It is possible to emulate Perl's behaviour after matching a
       * null string by first trying the match again at the same
       * offset with PCRE_NOTEMPTY and PCRE_ANCHORED, and then if
       * that fails by advancing the starting offset ... and trying
       * an ordinary match again." - pcreapi
       *
       * The 'and then if' part is implemented by the if() inside
       * the if(stringcount == PCRE_ERROR_NOMATCH) below.
       *
       */
      matched_empty_options = (ovector[0] == ovector[1]) ?
                              (PCRE_NOTEMPTY | PCRE_ANCHORED) : 0;

    } else if(stringcount == PCRE_ERROR_NOMATCH) {
      /* No match */
      size_t piece_len;
      size_t new_result_len;

      if(matched_empty_options && (size_t)startoffset < subject_len) {
        /* If the previous match was an empty string and there is
         * still some input to try, move on one char and continue
         * ordinary matches.
         */
        result[result_len++] = *subject_piece;
        startoffset++;
        matched_empty_options = 0;
        continue;
      }

      /* otherwise we are finished - copy the remaining input */
      piece_len = subject_len - startoffset;
      new_result_len = result_len + piece_len;
      
      if(new_result_len > result_size) {
        char* new_result;
        
        result_size = new_result_len;
        new_result = RASQAL_MALLOC(char*, result_size + 1);
        if(!new_result)
          goto failed;
        
        memcpy(new_result, result, result_len);
        RASQAL_FREE(char*, result);
        result = new_result;
      }
      
      memcpy(&result[result_len], subject_piece, piece_len);
      result_len += piece_len;

      /* NUL terminate the result and end */
      result[result_len] = '\0';
      break;
    } else {
      /* stringcount < 0 : other failures */
      RASQAL_DEBUG2("pcre_exec() failed with code %d\n", stringcount);
      goto failed;
    }
  }

  RASQAL_FREE(int*, ovector);

  if(result_len_p)
    *result_len_p = result_len;

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
  size_t capture_count;
  regmatch_t* pmatch;
  off_t startoffset;
  int matched_empty;
  char *result = NULL;
  size_t result_size; /* allocated size of result (excluding NUL) */
  size_t result_len; /* used size of result */
  const char *replace_end = replace + replace_len;

  capture_count = reg.re_nsub;

  pmatch = RASQAL_CALLOC(regmatch_t*, capture_count + 1, sizeof(regmatch_t));
  if(!pmatch)
    return NULL;

  result_size = subject_len << 1;
  result = RASQAL_MALLOC(char*, result_size + 1);
  if(!result)
    goto failed;
  result_len = 0;

  /* Match and replace loop; adjusting startoffset each time */
  startoffset = 0;
  matched_empty = 0;
  while(1) {
    int rc;
    const char *subject_piece = subject + startoffset;

    rc = regexec(&reg, RASQAL_GOOD_CAST(const char*, subject_piece),
                 capture_count, pmatch,
                 options /* eflags */
                 );

    if(!rc) {
      /* matches have been found */
      const char *subject_match;
      size_t piece_len;
      size_t new_result_len;
      const char *replace_p;
      char last_char;
      char *result_p;

      subject_match = subject_piece + pmatch[0].rm_so;

      /* compute new length of replacement with expanded variables */
      new_result_len = result_len;

      /* compute size of piece before the match */
      piece_len = subject_match - subject_piece;
      new_result_len += piece_len;

      /* compute size of matched piece */
      replace_p = replace;
      last_char = '\0';
      while(replace_p < replace_end) {
        if(*replace_p == '\\' || *replace_p == '$') {
          int ref_number;

          if(last_char == '\\') {
            /* Allow \\ and \$ */
            replace_p++;
            last_char = '\0';
            continue;
          }

          ref_number = rasqal_regex_get_ref_number(&replace_p);
          if(ref_number >= 0) {
            regmatch_t rm;
            size_t copy_len;

            rm = pmatch[ref_number];
            copy_len = rm.rm_eo - rm.rm_so + 1;
            if((size_t)ref_number < capture_count)
              new_result_len += copy_len;
            continue;
          }
        }

        new_result_len++;

        last_char = *replace_p;
        replace_p++;
      }

      /* need to expand result buffer? */
      if(new_result_len > result_size) {
        char* new_result;

        result_size += new_result_len << 1;
        new_result = RASQAL_MALLOC(char*, result_size + 1);
        if(!new_result)
          goto failed;

        memcpy(new_result, result, result_len);
        RASQAL_FREE(char*, result);
        result = new_result;
      }

      /* copy the piece of the input before the match */
      piece_len = subject_match - subject_piece;
      if(piece_len)
        memcpy(&result[result_len], subject_piece, piece_len);
      result_len += piece_len;

      /* copy replacement into result inserting matched references */
      result_p = result + result_len;
      replace_p = replace;
      last_char = '\0';
      while(replace_p < replace_end) {
        if(*replace_p == '\\' || *replace_p == '$') {
          int ref_number;

          if(last_char == '\\') {
            /* Allow \\ and \$ */
            *(result_p - 1) = *replace_p++;
            last_char = '\0';
            continue;
          }

          ref_number = rasqal_regex_get_ref_number(&replace_p);
          if(ref_number >= 0) {
            if((size_t)ref_number < capture_count) {
              regmatch_t rm;
              size_t match_len;

              rm = pmatch[ref_number];
              match_len = rm.rm_eo - rm.rm_so + 1;
              memcpy(result_p, subject + rm.rm_so, match_len);
              result_p += match_len;
              result_len += match_len;
            }
            continue;
          }
        }

        *result_p++ = *replace_p;
        result_len++;
        
        last_char = *replace_p;
        replace_p++;
      }
      *result_p = '\0';

      /* continue at offset after all matches */
      startoffset += pmatch[0].rm_eo;

      matched_empty = (pmatch[0].rm_so == pmatch[0].rm_eo);
    } else if (rc == REG_NOMATCH) {
      /* No match */
      size_t piece_len;
      size_t new_result_len;

      if(matched_empty && (size_t)startoffset < subject_len) {
        /* If the previous match was an empty string and there is
         * still some input to try, move on one char and continue
         * ordinary matches.
         */
        result[result_len++] = *subject_piece;
        startoffset++;
        matched_empty = 0;
        continue;
      }

      /* otherwise we are finished - copy the remaining input */
      piece_len = subject_len - startoffset;
      new_result_len = result_len + piece_len;
      
      if(new_result_len > result_size) {
        char* new_result;
        
        result_size = new_result_len;
        new_result = RASQAL_MALLOC(char*, result_size + 1);
        if(!new_result)
          goto failed;
        
        memcpy(new_result, result, result_len);
        RASQAL_FREE(char*, result);
        result = new_result;
      }
      
      memcpy(&result[result_len], subject_piece, piece_len);
      result_len += piece_len;

      /* NUL terminate the result and end */
      result[result_len] = '\0';
      break;
    } else {
      rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, locator,
                              "Regex match failed - returned code %d", rc);
      goto failed;
    }
  }

  RASQAL_FREE(regmatch_t*, pmatch);

  return result;


  failed:
  if(result)
    RASQAL_FREE(char*, result);

  RASQAL_FREE(regmatch_t*, pmatch);
  
  return NULL;
}
#endif



/**
 * rasqal_regex_replace:
 * @world: world
 * @locator: locator
 * @pattern: regex pattern
 * @regex_flags: regex flags string
 * @subject: input string
 * @subject_len: input string length
 * @replace: replacement string
 * @replace_len: Length of replacement string
 * @result_len_p: pointer to store result length (output)
 *
 * Replace all copies of matches to a pattern with a replacement with subsitution
 *
 * Intended to be used for SPARQL 1.1 REPLACE() implementation.
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
  int compile_options = PCRE_UTF8;
  int exec_options = 0;
  const char *re_error = NULL;
  int erroffset = 0;
#endif
#ifdef RASQAL_REGEX_POSIX
  regex_t reg;
  int compile_options = REG_EXTENDED;
  int exec_options = 0;
  int rc = 0;
  size_t pattern_len;
  char* pattern2;
#endif
  char *result_s = NULL;

#ifdef RASQAL_REGEX_PCRE
  for(p = regex_flags; p && *p; p++) {
    if(*p == 'i')
      exec_options |= PCRE_CASELESS;
  }

  re = pcre_compile(pattern, compile_options,
                    &re_error, &erroffset, NULL);
  if(!re) {
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, locator,
                            "Regex compile of '%s' failed - %s", pattern, re_error);
  } else
    result_s = rasqal_regex_replace_pcre(world, locator,
                                         re, exec_options,
                                         subject, subject_len,
                                         replace, replace_len,
                                         result_len_p);
  pcre_free(re);
#endif
    
#ifdef RASQAL_REGEX_POSIX
  /* Add an outer capture so we can always find what was matched */
  pattern_len = strlen(pattern);
  pattern2 = RASQAL_MALLOC(char*, pattern_len + 3);
  if(!pattern2)
    return NULL;

  pattern2[0] = '(';
  memcpy(pattern2 + 1, pattern, pattern_len);
  pattern2[pattern_len + 1]=')';
  pattern2[pattern_len + 2]='\0';
  
  for(p = regex_flags; p && *p; p++) {
    if(*p == 'i')
      compile_options |= REG_ICASE;
  }
    
  rc = regcomp(&reg, pattern2, compile_options);
  if(rc) {
    RASQAL_FREE(char*, pattern2);
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, locator,
                            "Regex compile of '%s' failed - %d", pattern, rc);
  } else {
    RASQAL_FREE(char*, pattern2);
    result_s = rasqal_regex_replace_posix(world, locator,
                                          reg, exec_options,
                                          subject, subject_len,
                                          replace, replace_len,
                                          result_len_p);
  }

  regfree(&reg);
#endif

#ifdef RASQAL_REGEX_NONE
  rasqal_log_warning_simple(world, RASQAL_WARNING_LEVEL_MISSING_SUPPORT,
                            locator,
                            "Regex support missing, cannot replace '%s' from '%s' to '%s'", subject, pattern, replace);
#endif

  return result_s;
}

#endif /* not STANDALONE */


#ifdef STANDALONE
#include <stdio.h>

int main(int argc, char *argv[]);


#define NTESTS 1

int
main(int argc, char *argv[])
{
  rasqal_world* world;
  const char *program = rasqal_basename(argv[0]);
#ifdef RASQAL_REGEX_PCRE
  raptor_locator* locator = NULL;
  int test = 0;
#endif
  int failures = 0;
  
  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    failures++;
    goto tidy;
  }
    
#if defined(RASQAL_REGEX_POSIX) || defined(RASQAL_REGEX_NONE)
    fprintf(stderr,
            "%s: WARNING: Cannot only run regex tests with PCRE regexes\n",
            program);
#endif

#ifdef RASQAL_REGEX_PCRE
  for(test = 0; test < NTESTS; test++) {
    const char* regex_flags = "";
    const char* subject = "abcd1234-^";
    const char* pattern = "[^a-z0-9]";
    const char* replace = "-";
    const char* expected_result = "abcd1234--";
    size_t subject_len = strlen(RASQAL_GOOD_CAST(const char*, subject));
    size_t replace_len = strlen(RASQAL_GOOD_CAST(const char*, replace));
    char* result;
    size_t result_len = 0;
    
    fprintf(stderr, "%s: Test %d pattern: '%s' subject '%s'\n",
            program, test, pattern, subject);
    
    result = rasqal_regex_replace(world, locator,
                                  pattern, regex_flags,
                                  subject, subject_len,
                                  replace, replace_len,
                                  &result_len);
    
    if(result) {
      if(strcmp(result, expected_result)) {
        fprintf(stderr, "%s: Test %d failed - expected '%s' but got '%s'\n", 
                program, test, expected_result, result);
        failures++;
      }
      RASQAL_FREE(char*, result);
    } else {
      fprintf(stderr, "%s: Test %d failed - result was NULL\n", program, test);
      failures++;
    }
  }
#endif

  tidy:
  rasqal_free_world(world);

  return failures;
}
#endif /* STANDALONE */

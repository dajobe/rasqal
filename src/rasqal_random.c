/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_random.c - Rasqal RDF Query random functions
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

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "rasqal.h"
#include "rasqal_internal.h"


/*
 * rasqal_random_get_system_seed
 * @eval_context: evaluation context
 *
 * INTERNAL - get a 32 bit unsigned integer random seed based on system sources
 *
 * Return value: seed with only lower 32 bits valid
 */
unsigned int
rasqal_random_get_system_seed(rasqal_evaluation_context *eval_context)
{
  /* SOURCE 1: processor clock ticks since process started */
  uint32_t a = (uint32_t)clock();
  /* SOURCE 2: unix time in seconds since epoch */
  uint32_t b = (uint32_t)time(NULL);
  uint32_t c;
#ifdef HAVE_UNISTD_H
  /* SOURCE 3: process ID (on unix) */
  c = getpid();
#else
  c = 0;
#endif
  
  /* Mix seed sources using public domain code from
   * http://www.burtleburtle.net/bob/c/lookup3.c
   */

#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))
  
  /* inlined mix(a, b, c) macro */
  a -= c;  a ^= rot(c, 4);  c += b;
  b -= a;  b ^= rot(a, 6);  a += c;
  c -= b;  c ^= rot(b, 8);  b += a;
  a -= c;  a ^= rot(c,16);  c += b;
  b -= a;  b ^= rot(a,19);  /* a += c; */ /* CLANG: not needed because of below */
  c -= b;  c ^= rot(b, 4);  /* b += a; */ /* CLANG: last calculation not needed */
  
  return (unsigned int)c;
}


/*
 * rasqal_random_init:
 * @eval_context: evaluation context
 *
 * INTERNAL - Allocate state for the random number generator
 *
 * Return value: non-0 on failure
 */
int
rasqal_random_init(rasqal_evaluation_context *eval_context)
{
#ifdef RANDOM_ALGO_RANDOM_R
  eval_context->random_data = RASQAL_CALLOC(struct random_data*,
                                            1, sizeof(struct random_data));
  if(!eval_context->random_data)
    return 1;
#endif  

  return 0;
}


/*
 * rasqal_random_finish:
 * @eval_context: evaluation context
 *
 * INTERNAL - Dealloc state for the random number generator
 */
void
rasqal_random_finish(rasqal_evaluation_context *eval_context)
{
#ifdef RANDOM_ALGO_RANDOM_R
  if(eval_context->random_data)
    RASQAL_FREE(struct random_data*, eval_context->random_data);
#endif  

#ifdef RANDOM_ALGO_RANDOM
  if(eval_context->random_data)
    setstate((char*)eval_context->random_data);
#endif
}


/*
 * rasqal_random_srand:
 * @eval_context: evaluation context
 * @seed: 32 bits of seed
 *
 * INTERNAL - Initialize the random number generator with a seed
 *
 * Return value: non-0 on failure
 */
int
rasqal_random_srand(rasqal_evaluation_context *eval_context, unsigned int seed)
{
  int rc = 0;
  
#ifdef RANDOM_ALGO_RANDOM_R
  rc = initstate_r(seed, eval_context->random_state, RASQAL_RANDOM_STATE_SIZE,
                   (struct random_data*)eval_context->random_data);
#endif

#ifdef RANDOM_ALGO_RANDOM
  eval_context->random_data = (void*)initstate(seed,
                                               eval_context->random_state, 
                                               RASQAL_RANDOM_STATE_SIZE);
#endif

#ifdef RANDOM_ALGO_RAND_R
  eval_context->seed = seed;
#endif

#ifdef RANDOM_ALGO_RAND
  srand(seed);
#endif

  return rc;
}


/*
 * rasqal_random_rand:
 * @eval_context: evaluation context
 *
 * INTERNAL - Get a random 32 bit int from the random number generator
 *
 * Return value: random integer (only lower 32 bits valid)
 */
int
rasqal_random_rand(rasqal_evaluation_context *eval_context)
{
  int r;
#ifdef RANDOM_ALGO_RANDOM_R
  int32_t result;
#endif
#ifdef RANDOM_ALGO_RANDOM
  char *old_state;
#endif

#ifdef RANDOM_ALGO_RANDOM_R
  result = 0;
  random_r((struct random_data*)eval_context->random_data, &result);
  /* This casts a 32 bit integer to an int */
  r = (int)result;
#endif  

#ifdef RANDOM_ALGO_RANDOM
  old_state = setstate(eval_context->random_state);

  /* This casts a long to an int */
  r = (int)random();

  setstate(old_state);
#endif  

#ifdef RANDOM_ALGO_RAND_R
  r = rand_r(&eval_context->seed);
#endif

#ifdef RANDOM_ALGO_RAND
  r = rand();
#endif

  return r;
}

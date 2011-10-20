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
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include "rasqal.h"
#include "rasqal_internal.h"


#ifndef STANDALONE
/*
 * rasqal_random_get_system_seed
 * @random_object: evaluation context
 *
 * INTERNAL - get a 32 bit unsigned integer random seed based on system sources
 *
 * Return value: seed with only lower 32 bits valid
 */
unsigned int
rasqal_random_get_system_seed(rasqal_world *world)
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
 * rasqal_new_random:
 * @world: world object
 *
 * INTERNAL - Constructor - create a new random number generator
 *
 * Return value: new rasqal_random or NULL on failure
 */
rasqal_random*
rasqal_new_random(rasqal_world* world)
{
  rasqal_random* r;
  
  r = RASQAL_CALLOC(rasqal_random*, 1, sizeof(*r));
  if(!r)
    return NULL;
  
  r->world = world;
#ifdef RANDOM_ALGO_RANDOM_R
  random_object->data = RASQAL_CALLOC(struct random_data*,
                                      1, sizeof(struct random_data));
  if(!random_object->data) {
    RASQAL_FREE(rasqal_random*, r);
    return NULL;
  }
#endif  

  return r;
}


/*
 * rasqal_free_random:
 * @random_object: evaluation context
 *
 * INTERNAL - Destructor - Destroy a random number generator
 */
void
rasqal_free_random(rasqal_random *random_object)
{
#ifdef RANDOM_ALGO_RANDOM_R
  if(random_object->data)
    RASQAL_FREE(struct data*, random_object->data);
#endif  

#ifdef RANDOM_ALGO_RANDOM
  if(random_object->data)
    setstate((char*)random_object->data);
#endif
}


/*
 * rasqal_random_srand:
 * @random_object: evaluation context
 * @seed: 32 bits of seed
 *
 * INTERNAL - Initialize the random number generator with a seed
 *
 * Return value: non-0 on failure
 */
int
rasqal_random_srand(rasqal_random *random_object, unsigned int seed)
{
  int rc = 0;
  
#ifdef RANDOM_ALGO_RANDOM_R
  rc = initstate_r(seed, random_object->state, RASQAL_STATE_SIZE,
                   (struct data*)random_object->data);
#endif

#ifdef RANDOM_ALGO_RANDOM
  random_object->data = (void*)initstate(seed,
                                         random_object->state,
                                         RASQAL_STATE_SIZE);
#endif

#ifdef RANDOM_ALGO_RAND_R
  random_object->seed = seed;
#endif

#ifdef RANDOM_ALGO_RAND
  srand(seed);
#endif

  return rc;
}


/*
 * rasqal_random_rand:
 * @random_object: evaluation context
 *
 * INTERNAL - Get a random int from the random number generator
 *
 * Return value: random integer in range 0 to RAND_MAX inclusive
 */
int
rasqal_random_rand(rasqal_random *random_object)
{
  int r;
#ifdef RANDOM_ALGO_RANDOM_R
  int32_t result;
#endif
#ifdef RANDOM_ALGO_RANDOM
  char *old_state;
#endif

  /* results of all these functions is an integer or long in the
   * range 0...RAND_MAX inclusive
   */

#ifdef RANDOM_ALGO_RANDOM_R
  result = 0;
  random_r((struct data*)random_object->data, &result);
  r = (int)result;
#endif  

#ifdef RANDOM_ALGO_RANDOM
  old_state = setstate(random_object->state);
  r = (int)random();
  setstate(old_state);
#endif  

#ifdef RANDOM_ALGO_RAND_R
  r = rand_r(&random_object->seed);
#endif

#ifdef RANDOM_ALGO_RAND
  r = rand();
#endif

  return r;
}

#endif


#ifdef STANDALONE
#include <stdio.h>

int main(int argc, char *argv[]);


#define NTESTS 20

int
main(int argc, char *argv[])
{
  rasqal_world* world;
  const char *program = rasqal_basename(argv[0]);
  int failures = 0;
  rasqal_random* r = NULL;
  int test;

  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    failures++;
    goto tidy;
  }
    
  r = rasqal_new_random(world);
  if(!r) {
    fprintf(stderr, "%s: rasqal_new_random() failed\n", program);
    failures++;
    goto tidy;
  }

  rasqal_random_srand(r, 54321);
    
  for(test = 0; test < NTESTS; test++) {
    int v;
    
    v = rasqal_random_rand(r);
#if RASQAL_DEBUG > 1
    fprintf(stderr, "%s: Test %3d  value: %10d\n", program, test, v);
#endif
  }

  tidy:
  if(r)
    rasqal_free_random(r);

  rasqal_free_world(world);

  return failures;
}
#endif

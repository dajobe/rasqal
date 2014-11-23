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
#ifdef RANDOM_ALGO_GMP_RAND
#ifdef HAVE_GMP_H
#include <gmp.h>
#endif
#define RANDOM_ALGO_BITS 32
#endif
#ifdef RANDOM_ALGO_MTWIST
#include <mtwist_config.h>
#include <mtwist.h>
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
  c = RASQAL_GOOD_CAST(uint32_t, getpid());
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

  /* Good as long as sizeof(unsigned int) >= sizeof(uint32_t) */
  return RASQAL_GOOD_CAST(unsigned int, c);
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
  r->data = RASQAL_CALLOC(struct random_data*,
                          1, sizeof(struct random_data));
  if(!r->data) {
    RASQAL_FREE(rasqal_random*, r);
    return NULL;
  }
#endif  

#ifdef RANDOM_ALGO_GMP_RAND
  r->data = RASQAL_CALLOC(gmp_randstate_t*, 1, sizeof(gmp_randstate_t));
  gmp_randinit_default(*(gmp_randstate_t*)r->data);
#endif

#ifdef RANDOM_ALGO_MTWIST
  r->data = mtwist_new();
#endif

  if(r)
    rasqal_random_seed(r, rasqal_random_get_system_seed(r->world));

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
    RASQAL_FREE(struct random_data*, random_object->data);
#endif  

#ifdef RANDOM_ALGO_RANDOM
  if(random_object->data)
    setstate(RASQAL_GOOD_CAST(char*, random_object->data));
#endif

#ifdef RANDOM_ALGO_GMP_RAND
  if(random_object->data)
    RASQAL_FREE(gmp_randstate_t*, random_object->data);
#endif  

#ifdef RANDOM_ALGO_MTWIST
  mtwist_free((mtwist*)random_object->data);
#endif

  RASQAL_FREE(rasqsal_random*, random_object);
}


/*
 * rasqal_random_seed:
 * @random_object: evaluation context
 * @seed: 32 bits of seed
 *
 * INTERNAL - Initialize the random number generator with a seed
 *
 * Return value: non-0 on failure
 */
int
rasqal_random_seed(rasqal_random *random_object, unsigned int seed)
{
  int rc = 0;
  
#ifdef RANDOM_ALGO_RANDOM_R
  rc = initstate_r(seed, random_object->state, RASQAL_RANDOM_STATE_SIZE,
                   (struct random_data*)random_object->data);
#endif

#ifdef RANDOM_ALGO_RANDOM
  random_object->data = (void*)initstate(seed,
                                         random_object->state,
                                         RASQAL_RANDOM_STATE_SIZE);
#endif

#ifdef RANDOM_ALGO_RAND_R
  random_object->seed = seed;
#endif

#ifdef RANDOM_ALGO_RAND
  srand(seed);
#endif

#ifdef RANDOM_ALGO_GMP_RAND
  gmp_randseed_ui(*(gmp_randstate_t*)random_object->data, (unsigned long)seed);
#endif  

#ifdef RANDOM_ALGO_MTWIST
  mtwist_init((mtwist*)random_object->data, (unsigned long)seed);
#endif

  return rc;
}


/*
 * rasqal_random_irand:
 * @random_object: evaluation context
 *
 * INTERNAL - Get a random int from the random number generator
 *
 * Return value: random integer in the range 0 to RAND_MAX inclusive; [0, RAND_MAX]
 */
int
rasqal_random_irand(rasqal_random *random_object)
{
  int r;
#ifdef RANDOM_ALGO_RANDOM_R
  int32_t result;
#endif
#ifdef RANDOM_ALGO_RANDOM
  char *old_state;
#endif
#ifdef RANDOM_ALGO_GMP_RAND
  mpz_t rand_max_gmp;
  mpz_t iresult;
#endif

  /* results of all these functions is an integer or long in the
   * range 0...RAND_MAX inclusive
   */

#ifdef RANDOM_ALGO_RANDOM_R
  result = 0;
  random_r((struct random_data*)random_object->data, &result);
  /* Good if int is 32 bits or larger */
  r = RASQAL_GOOD_CAST(int, result);
#endif  

#ifdef RANDOM_ALGO_RANDOM
  old_state = setstate(random_object->state);
  r = RASQAL_GOOD_CAST(int, random());
  setstate(old_state);
#endif  

#ifdef RANDOM_ALGO_RAND_R
  r = rand_r(&random_object->seed);
#endif

#ifdef RANDOM_ALGO_RAND
  r = rand();
#endif

#ifdef RANDOM_ALGO_GMP_RAND
  /* This could be init/cleared once in random state */
  mpz_init_set_ui(rand_max_gmp, 1 + (unsigned long)RAND_MAX);

  mpz_init(iresult);
  mpz_urandomm(iresult, *(gmp_randstate_t*)random_object->data, rand_max_gmp);
  /* cast from unsigned long to unsigned int; we know above that the max
   * size is RAND_MAX so it will fit
   */
  r = RASQAL_GOOD_CAST(unsigned int, mpz_get_ui(iresult));
  mpz_clear(iresult);

  mpz_clear(rand_max_gmp);
#endif  

#ifdef RANDOM_ALGO_MTWIST
  /* cast from unsigned long to int but max size is RAND_MAX
   * so it will fit
   */
  r = RASQAL_GOOD_CAST(int, mtwist_u32rand((mtwist*)random_object->data));
#endif

  return r;
}


/*
 * rasqal_random_drand:
 * @random_object: evaluation context
 *
 * INTERNAL - Get a random double from the random number generator
 *
 * Return value: random double in the range 0.0 inclusive to 1.0 exclusive; [0.0, 1.0)
 */
double
rasqal_random_drand(rasqal_random *random_object)
{
#ifdef RANDOM_ALGO_GMP_RAND
  mpf_t fresult;
#else
#ifdef RANDOM_ALGO_MTWIST
#else
  int r;
#endif
#endif
  double d;
  
#ifdef RANDOM_ALGO_GMP_RAND
  mpf_init(fresult);
  mpf_urandomb(fresult, *(gmp_randstate_t*)random_object->data, 
               RANDOM_ALGO_BITS);
  d = mpf_get_d(fresult);
  mpf_clear(fresult);
#else
#ifdef RANDOM_ALGO_MTWIST
  d = mtwist_drand((mtwist*)random_object->data);
#else
  r = rasqal_random_irand(random_object);
  d = r / (double)(RAND_MAX + 1.0);
#endif
#endif

  return d;
}


#endif /* not STANDALONE */


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

  rasqal_random_seed(r, 54321);
    
  for(test = 0; test < NTESTS; test++) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    int v;
    
    v = rasqal_random_irand(r);
    fprintf(stderr, "%s: Test %3d  value: %10d\n", program, test, v);
#else
    (void)rasqal_random_irand(r);
#endif
  }

  for(test = 0; test < NTESTS; test++) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    double d;
    
    d = rasqal_random_drand(r);
    fprintf(stderr, "%s: Test %3d  value: %10f\n", program, test, d);
#else
    (void)rasqal_random_drand(r);
#endif
  }

  tidy:
  if(r)
    rasqal_free_random(r);

  rasqal_free_world(world);

  return failures;
}
#endif /* STANDALONE */

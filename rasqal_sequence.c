/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_sequence.c - Rasqal sequence support
 *
 * $Id$
 *
 * Copyright (C) 2003-2004 David Beckett - http://purl.org/net/dajobe/
 * Institute for Learning and Research Technology - http://www.ilrt.bris.ac.uk/
 * University of Bristol - http://www.bristol.ac.uk/
 * 
 * This package is Free Software or Open Source available under the
 * following licenses (these are alternatives):
 *   1. GNU Lesser General Public License (LGPL)
 *   2. GNU General Public License (GPL)
 *   3. Mozilla Public License (MPL)
 * 
 * See LICENSE.html or LICENSE.txt at the top of this package for the
 * full license terms.
 * 
 * 
 */

#ifdef HAVE_CONFIG_H
#include <rasqal_config.h>
#endif

#ifdef WIN32
#include <win32_config.h>
#endif

#include <stdio.h>
#include <string.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdarg.h>

#include "rasqal.h"
#include "rasqal_internal.h"


struct rasqal_sequence_s {
  int size;
  int capacity;
  void **sequence;
  rasqal_free_handler *free_handler;
  rasqal_print_handler *print_handler;
};


static int rasqal_sequence_ensure(rasqal_sequence *seq, int capacity);
static int rasqal_sequence_grow(rasqal_sequence *seq);

/* Constructor */
rasqal_sequence*
rasqal_new_sequence(rasqal_free_handler *free_handler,
                    rasqal_print_handler *print_handler) {
  rasqal_sequence* seq=(rasqal_sequence*)malloc(sizeof(rasqal_sequence));
  if(!seq)
    return NULL;
  seq->size=0;
  seq->capacity=0;
  seq->free_handler=free_handler;
  seq->print_handler=print_handler;
  
  return seq;
}

/* Destructor*/

void
rasqal_free_sequence(rasqal_sequence* seq) {
  int i;

  if(seq->free_handler)
    for(i=0; i< seq->size; i++)
      if(seq->sequence[i])
        seq->free_handler(seq->sequence[i]);

  if(seq->sequence)
    free(seq->sequence);

  RASQAL_FREE(rasqal_sequence, seq);
}


static int
rasqal_sequence_ensure(rasqal_sequence *seq, int capacity) {
  void *new_sequence;
  if(seq->capacity > capacity)
    return 0;

  /* POLICY - minimum size */
  if(capacity < 8)
    capacity=8;

  new_sequence=calloc(capacity, sizeof(void*));
  if(!new_sequence)
    return 1;

  if(seq->size) {
    memcpy(new_sequence, seq->sequence, sizeof(void*)*seq->size);
    free(seq->sequence);
  }

  seq->sequence=new_sequence;
  seq->capacity=capacity;
  return 0;
}


static int
rasqal_sequence_grow(rasqal_sequence *seq) 
{
  return rasqal_sequence_ensure(seq, seq->capacity*2);
}



/* Methods */
int
rasqal_sequence_size(rasqal_sequence* seq) {
  return seq->size;
}


/* Store methods */
int
rasqal_sequence_set_at(rasqal_sequence* seq, int idx, void *data) {
  if(idx+1 > seq->capacity) {
    if(rasqal_sequence_ensure(seq, idx+1))
      return 1;
  }
    
  if(seq->sequence[idx] && seq->free_handler)
    seq->free_handler(seq->sequence[idx]);
  
  seq->sequence[idx]=data;
  if(idx+1 > seq->size)
    seq->size=idx+1;
  return 0;
}


/* add to end of sequence */
int
rasqal_sequence_push(rasqal_sequence* seq, void *data) {
  if(seq->size == seq->capacity) {
    if(rasqal_sequence_grow(seq))
      return 1;
  }

  seq->sequence[seq->size]=data;
  seq->size++;
  return 0;
}


/* add to start of sequence */
int
rasqal_sequence_shift(rasqal_sequence* seq, void *data) {
  int i;

  if(seq->size == seq->capacity) {
    if(rasqal_sequence_grow(seq))
      return 1;
  }

  for(i=seq->size; i>0; i--)
    seq->sequence[i]=seq->sequence[i-1];
  
  seq->sequence[0]=data;
  seq->size++;
  return 0;
}


/* Retrieval methods */
void*
rasqal_sequence_get_at(rasqal_sequence* seq, int idx) {
  if(idx > seq->size)
    return NULL;
  return seq->sequence[idx];
}

/* remove from end of sequence */
void*
rasqal_sequence_pop(rasqal_sequence* seq) {
  void *data;

  if(!seq->size)
    return NULL;

  seq->size--;
  data=seq->sequence[seq->size];
  seq->sequence[seq->size]=NULL;

  return data;
}

/* remove from start of sequence */
void*
rasqal_sequence_unshift(rasqal_sequence* seq) {
  void *data;
  int i;

  if(!seq->size)
    return NULL;
  
  data=seq->sequence[0];
  seq->size--;
  for(i=0; i<seq->size; i++)
    seq->sequence[i]=seq->sequence[i+1];
  seq->sequence[i]=NULL;
  
  return data;
}


int
rasqal_compare_strings(const void *a, const void *b) 
{
  return strcmp(*(char**)a, *(char**)b);
}



/* sort sequence */
void
rasqal_sequence_sort(rasqal_sequence* seq, 
                     int(*compare)(const void *, const void *))
{
  if(seq->size > 1)
    qsort(seq->sequence, seq->size, sizeof(void*), compare);
}



void
rasqal_sequence_print_string(char *data, FILE *fh) 
{
  fputs(data, fh);
}


void
rasqal_sequence_print_uri(char *data, FILE *fh) 
{
  raptor_uri* uri=(raptor_uri*)data;
  fputs(raptor_uri_as_string(uri), fh);
}


/* print sequence */
void
rasqal_sequence_print(rasqal_sequence* seq, FILE* fh)
{
  int i;

  if(!seq)
    return;
  
  fputc('[', fh);
  for(i=0; i<seq->size; i++) {
    if(i)
      fputs(", ", fh);
    if(seq->sequence[i])
      seq->print_handler(seq->sequence[i], fh);
    else
      fputs("(empty)", fh);
  }
  fputc(']', fh);
}


#ifdef STANDALONE
#include <stdio.h>

int main(int argc, char *argv[]);


#define assert_match(function, result, string) do { if(strcmp(result, string)) { fprintf(stderr, #function " failed - returned %s, expected %s\n", result, string); exit(1); } } while(0)

char *program;

int
main(int argc, char *argv[]) 
{
  rasqal_sequence* seq=rasqal_new_sequence(NULL, (rasqal_print_handler*)rasqal_sequence_print_string);
  char *s;

  program=argv[0];
  

  rasqal_sequence_set_at(seq, 0, "first");

  rasqal_sequence_push(seq, "third");

  rasqal_sequence_shift(seq, "second");

  s=(char*)rasqal_sequence_get_at(seq, 0);
  assert_match(rasqal_sequence_get_at, s, "second");

  s=(char*)rasqal_sequence_get_at(seq, 1);
  assert_match(rasqal_sequence_get_at, s, "first");
  
  s=(char*)rasqal_sequence_get_at(seq, 2);
  assert_match(rasqal_sequence_get_at, s, "third");
  
  if(rasqal_sequence_size(seq) !=3)
    exit(1);

  fprintf(stderr, "%s: sequence after additions: ", program);
  rasqal_sequence_print(seq, stderr);
  fputc('\n', stderr);

  /* now made alphabetical i.e. first, second, third */
  rasqal_sequence_sort(seq, rasqal_compare_strings);

  fprintf(stderr, "%s: sequence after sort: ", program);
  rasqal_sequence_print(seq, stderr);
  fputc('\n', stderr);

  s=(char*)rasqal_sequence_pop(seq);
  assert_match(rasqal_sequence_get_at, s, "third");

  if(rasqal_sequence_size(seq) !=2)
    exit(1);

  fprintf(stderr, "%s: sequence after pop: ", program);
  rasqal_sequence_print(seq, stderr);
  fputc('\n', stderr);

  s=(char*)rasqal_sequence_unshift(seq);
  assert_match(rasqal_sequence_get_at, s, "first");

  if(rasqal_sequence_size(seq) !=1)
    exit(1);

  fprintf(stderr, "%s: sequence after unshift: ", program);
  rasqal_sequence_print(seq, stderr);
  fputc('\n', stderr);

  s=(char*)rasqal_sequence_get_at(seq, 0);
  assert_match(rasqal_sequence_get_at, s, "second");
  
  rasqal_free_sequence(seq);

  return (0);
}
#endif

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_sequence.c - Rasqal sequence support
 *
 * $Id$
 *
 * Copyright (C) 2003 David Beckett - http://purl.org/net/dajobe/
 * Institute for Learning and Research Technology - http://www.ilrt.org/
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
#include <stdlib.h>
#include <string.h>

#include <rasqal.h>
#include <rasqal_internal.h>


struct rasqal_sequence_s {
  int size;
  int capacity;
  void **sequence;
};


static int rasqal_sequence_ensure(rasqal_sequence *seq, int capacity);
static int rasqal_sequence_grow(rasqal_sequence *seq);

/* Constructor */
rasqal_sequence*
rasqal_new_sequence(int capacity) {
  rasqal_sequence* seq=(rasqal_sequence*)malloc(sizeof(rasqal_sequence));
  if(!seq)
    return NULL;
  seq->size=0;
  seq->capacity=0;
  
  if(capacity>0 && rasqal_sequence_ensure(seq, capacity)) {
    free(seq);
    return NULL;
  }
  
  return seq;
}

/* Destructor*/

void
rasqal_free_sequence(rasqal_sequence* seq) {
  int i;
  for(i=0; i< seq->size; i++)
    free(seq->sequence[i]);

  if(seq->sequence)
    free(seq->sequence);
}


static int
rasqal_sequence_ensure(rasqal_sequence *seq, int capacity) {
  void *new_sequence;
  if(seq->capacity > capacity)
    return 0;

  new_sequence=calloc(capacity, sizeof(void*));
  if(!new_sequence)
    return 1;

  if(seq->size) {
    memcpy(new_sequence, seq->sequence, sizeof(void*)*seq->size);
    free(seq->sequence);
  }

  seq->sequence=new_sequence;
  return 0;
}


static int
rasqal_sequence_grow(rasqal_sequence *seq) 
{
  if(!seq->capacity)
    return rasqal_sequence_ensure(seq, 5);
  else
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
  if(idx > seq->capacity) {
    if(rasqal_sequence_ensure(seq, idx))
      return 1;
  }
    
  if(seq->sequence[idx])
    free(seq->sequence[idx]);
  
  seq->sequence[idx]=data;
  return 0;
}


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


int
rasqal_sequence_shift(rasqal_sequence* seq, void *data) {
  int i;

  if(seq->size == seq->capacity) {
    if(rasqal_sequence_grow(seq))
      return 1;
  }

  for(i=seq->size-1; i>0; i--)
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

  return data;
}

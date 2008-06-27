/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_skiplist.c - Skip list
 *
 * Copyright (C) 2005-2008, David Beckett http://www.dajobe.org/
 * Copyright (C) 2005-2005, University of Bristol, UK http://www.bristol.ac.uk/
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
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdarg.h>

#ifndef HAVE_SRANDOMDEV
/* for time() as a random seed */
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#endif

/* Rasqal includes */
#include "rasqal.h"
#include "rasqal_internal.h"


typedef struct rasqal_skiplist_node_s rasqal_skiplist_node;

struct rasqal_skiplist_s {
  /* list header */
  rasqal_skiplist_node* head;

  /* current level of list */
  int level;

  /* number of entries in list */
  int size;

  /* flags from bitor of enum rasqal_skiplist_flags values */
  int flags;

  /* random bits management for choosing level */
  unsigned int random_bits;
  unsigned int random_bits_left;

  /* item comparison function returning for A<B: <0, A=B: 0, A>B: >0 */
  rasqal_compare_fn* compare_fn;

  /* item free item (key, value pair) function */
  rasqal_kv_free_fn* free_fn;

  /* print (key,value) functions */
  raptor_sequence_print_handler* print_key_fn;
  raptor_sequence_print_handler* print_value_fn;
};


/* levels range from (0 .. RASQAL_SKIPLIST_MAX_LEVEL) */
#define RASQAL_SKIPLIST_MAX_LEVEL 15

#define RASQAL_SKIPLIST_NIL NULL

#define BITS_IN_RANDOM 31


struct rasqal_skiplist_node_s {
  void* key;
  void* value;
  /* skip list forward array.
   * Note: This MUST remain at the end of the struct as it is variable size 
   */
  struct rasqal_skiplist_node_s* forward[1];
};


static int rasqal_skiplist_get_random_level(rasqal_skiplist *list);
static void rasqal_skiplist_node_print(rasqal_skiplist* list, rasqal_skiplist_node* node, FILE *fh);


#ifndef STANDALONE_NOTYET


void
rasqal_skiplist_init_with_seed(unsigned long seed) 
{
  srandom(seed);
}


void
rasqal_skiplist_init(void)
{
#ifdef HAVE_SRANDOMDEV
  srandomdev();
#else
  srandom((unsigned long)time(NULL));
#endif
}


void
rasqal_skiplist_finish(void)
{
  /* NOP */
}


/**
 * rasqal_new_skiplist:
 * @compare_fn: function to compare two keys
 * @free_fn: function to delete a (key, value) item pair (or NULL)
 * @print_key_fn: function to print a key function
 * @print_value_fn: function to print a key function
 * @flags: set to RASQAL_SKIPLIST_FLAG_DUPLICATES to allow duplicates
 *
 * Constructor - Create a new Rasqal skiplist.
 *
 * Return value: a new #rasqal_skiplist or NULL on failure.
 */
rasqal_skiplist*
rasqal_new_skiplist(rasqal_compare_fn* compare_fn,
                    rasqal_kv_free_fn* free_fn,
                    raptor_sequence_print_handler* print_key_fn,
                    raptor_sequence_print_handler* print_value_fn,
                    int flags)
{
  int i;
  rasqal_skiplist* list;
  
  list=(rasqal_skiplist*)RASQAL_CALLOC(rasqal_skiplist, 1, sizeof(rasqal_skiplist));
  if(!list)
    return NULL;
  
  list->compare_fn=compare_fn;
  list->print_key_fn=print_key_fn;
  list->print_value_fn=print_value_fn;
  
  list->head=(rasqal_skiplist_node*)RASQAL_MALLOC(rasqal_skiplist_node,
                                                  sizeof(rasqal_skiplist_node) + RASQAL_SKIPLIST_MAX_LEVEL*sizeof(rasqal_skiplist_node* ));
  if(!list->head) {
    RASQAL_FREE(rasqal_skiplist, list);
    return NULL;
  }

  for(i = 0; i <= RASQAL_SKIPLIST_MAX_LEVEL; i++)
    list->head->forward[i] = RASQAL_SKIPLIST_NIL;
  list->level = 0;

  list->size = 0;

  list->random_bits=random();
  list->random_bits_left= BITS_IN_RANDOM;

  list->flags=flags;

  return list;
}


static void
rasqal_free_skiplist_node(rasqal_skiplist* list, rasqal_skiplist_node* node)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(list, rasqal_skiplist);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(node, rasqal_skiplist_node);

  if(list->free_fn)
    list->free_fn(node->key, node->value);
  RASQAL_FREE(rasqal_skiplist_node, node);
}


/**
 * rasqal_free_skiplist - Destructor - Destroy a Rasqal skiplist object
 * @list: #rasqal_skiplist object
 *
 */
void
rasqal_free_skiplist(rasqal_skiplist* list)
{
  rasqal_skiplist_node* node;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN(list, rasqal_skiplist);

  node=list->head;
  for(node = node->forward[0]; node != RASQAL_SKIPLIST_NIL; ) {
    rasqal_skiplist_node* next = node->forward[0];
    rasqal_free_skiplist_node(list, node);
    node=next;
  }

  if(list->head) 
    rasqal_free_skiplist_node(list, list->head);

  RASQAL_FREE(rasqal_skiplist, list);
}


static int
rasqal_skiplist_get_random_level(rasqal_skiplist *list)
{
  int level = 0;
  int b;
  do {
    b = list->random_bits & 3;
    if(!b) 
      level++;
    list->random_bits>>=2;
    if(!--list->random_bits_left) {
      list->random_bits = random();
      list->random_bits_left = BITS_IN_RANDOM >>1;
    };
  } while (!b);

  return (level > RASQAL_SKIPLIST_MAX_LEVEL ? RASQAL_SKIPLIST_MAX_LEVEL : level);
};


/**
 * rasqal_skiplist_insert - Add a (key, value) pair to a skiplist
 * @list: #rasqal_skiplist object
 * @key: pointer to key
 * @value: pointer to value
 * 
 * If duplicates are not allowed, adding a duplicate (key, value)
 * pair will fail.
 *
 * Return value: non-0 on failure
 */
int
rasqal_skiplist_insert(rasqal_skiplist* list, 
                       void* key, void* value)
{
  int i;
  int new_level;
  rasqal_skiplist_node* update[RASQAL_SKIPLIST_MAX_LEVEL+1];
  rasqal_skiplist_node* node;
  
  node=list->head;
  for(i = list->level; i>=0; i--) {
    while(node->forward[i] != RASQAL_SKIPLIST_NIL &&
          list->compare_fn(node->forward[i]->key, key) < 0)
      node=node->forward[i];
    update[i] = node;
  }
  node=node->forward[0];

  if(!(list->flags & RASQAL_SKIPLIST_FLAG_DUPLICATES)) {
    if(node != RASQAL_SKIPLIST_NIL && !list->compare_fn(node->key, key)) 
      /* found duplicate key and they are not allowed */
      return 1;
  }
  
  new_level = rasqal_skiplist_get_random_level(list);
  
  if(new_level > list->level) {
    for(i = list->level + 1; i <= new_level; i++)
      update[i] = list->head;
    list->level = new_level;
  }
  
  node=(rasqal_skiplist_node*)RASQAL_MALLOC(rasqal_skiplist_node, 
                                            sizeof(rasqal_skiplist_node) + 
                                            new_level * sizeof(rasqal_skiplist_node*));
  if(!node)
    return 1;
  node->key = key;
  node->value = value;
  
  for(i = 0; i <= new_level; i++) {
    node->forward[i] = update[i]->forward[i];
    update[i]->forward[i] = node;
  }

  list->size++;

  return 0;
}


/**
 * rasqal_skiplist_delete - Delete a (key, value) pair from a skiplist
 * @list: #rasqal_skiplist object
 * @key: pointer to key
 * @value: pointer to value
 * 
 * Return value: non-0 on failure
 */
int
rasqal_skiplist_delete(rasqal_skiplist* list, void* key)
{
  int i;
  rasqal_skiplist_node* update[RASQAL_SKIPLIST_MAX_LEVEL+1];
  rasqal_skiplist_node* node;

  node=list->head;
  for(i = list->level; i>=0; i--) {
    while(node->forward[i] != RASQAL_SKIPLIST_NIL &&
          list->compare_fn(node->forward[i]->key, key) < 0)
      node=node->forward[i];
    update[i] = node;
  }
  node=node->forward[0];

  if(node == RASQAL_SKIPLIST_NIL || list->compare_fn(node->key, key))
    /* failed to find key */
    return 1;
  
  for(i = 0; i <= list->level; i++) {
    if(update[i]->forward[i] != node) 
      break;
    update[i]->forward[i] = node->forward[i];
  }
  
  rasqal_free_skiplist_node(list, node);
  
  while((list->level > 0) && 
        (list->head->forward[list->level] == RASQAL_SKIPLIST_NIL))
    list->level--;
  
  list->size--;

  return 0;
}


/**
 * rasqal_skiplist_find - Find a value in a skiplist for a given key
 * @list: #rasqal_skiplist object
 * @key: pointer to key
 * 
 * Return value: value pointer or NULL if not found
 */
void*
rasqal_skiplist_find(rasqal_skiplist* list, void* key)
{
  int i;
  rasqal_skiplist_node* node=list->head;
  
  for(i = list->level; i>=0; i--) {
    while(node->forward[i] != RASQAL_SKIPLIST_NIL &&
          list->compare_fn(node->forward[i]->key, key) < 0)
      node=node->forward[i];
  }

  node=node->forward[0];
  if(node != RASQAL_SKIPLIST_NIL && !list->compare_fn(node->key, key)) {
    return node->value;
  }

  /* failed to find key */
  return NULL;
}


static void
rasqal_skiplist_node_print(rasqal_skiplist* list,
                           rasqal_skiplist_node* node, FILE *fh)
{
  fputs("{", fh);
  if(!node->key)
    fputs("NULL", fh);
  else if(list->print_key_fn)
    list->print_key_fn(node->key, fh);
  else
    fprintf(fh, "key %p", node->key);

  fputs(" : ", fh);

  if(!node->value)
    fputs("NULL", fh);
  else if(list->print_value_fn)
    list->print_value_fn(node->value, fh);
  else
    fprintf(fh, "data %p", node->value);

  fputs("}", fh);
}


/**
 * rasqal_skiplist_print - Print a Rasqal skiplist in a debug format
 * @list: the #rasqal_skiplist object
 * @fh: the #FILE* handle to print to
 * 
 * The print debug format may change in any release.
 * 
 */
void
rasqal_skiplist_print(rasqal_skiplist* list, FILE* fh)
{
  rasqal_skiplist_node* node;
  int first=1;
  
  fprintf(fh, "skiplist(size=%d, duplicates=%s) [[ ", list->size,
          (list->flags & RASQAL_SKIPLIST_FLAG_DUPLICATES) ? "yes" : "no");
  node=list->head;
  for(node = node->forward[0]; node != RASQAL_SKIPLIST_NIL; ) {
    if(!first)
      fputs(", ", fh);
    rasqal_skiplist_node_print(list, node, fh);
    first=0;
    node=node->forward[0];
  }
  
  fputs(" ]]", fh);
}


void
rasqal_skiplist_dump(rasqal_skiplist* list, FILE *fh)
{
  rasqal_skiplist_node* node;
  int i;
  int first=1;
  
  fprintf(fh, "skiplist(size=%d, duplicates=%s) [[ ", list->size,
          (list->flags & RASQAL_SKIPLIST_FLAG_DUPLICATES) ? "yes" : "no");
  node=list->head;
  for(i = list->level; i>=0; i--) {
    int count=0;
    while(node->forward[i] != RASQAL_SKIPLIST_NIL) {
      count++;
      node=node->forward[i];
    }
    if(!first)
      fputs(", ", fh);
    fprintf(fh, "L%d: %d node%s", i, count, ((count == 1) ? "" : "s"));
    first=0;
  }
  fputs(" ]]", fh);
}


/**
 * rasqal_skiplist_get_size - Get the number of items in a skiplist
 * @list: #rasqal_skiplist object
 * 
 * Return value: the number of items in the skiplist (0 or more)
 */
unsigned int
rasqal_skiplist_get_size(rasqal_skiplist* list)
{
  return list->size;
}


#endif



#ifdef STANDALONE

static int
rasqal_skiplist_int_compare(const void *a, const void *b)
{
  int* int_a=(int*)a;
  int* int_b=(int*)b;
  
  return *int_a - *int_b;
}

static void
rasqal_skiplist_int_print(void *int_p, FILE *fh)
{
  fprintf(fh,"%d", *(int*)int_p);
}


/* one more prototype */
int main(int argc, char *argv[]);

#define DEFAULT_TEST_SIZE 100

int
main(int argc, char *argv[]) 
{
  const char *program=rasqal_basename(argv[0]);
  int size;
  int* keys;
  int* values;
  rasqal_skiplist* list;
  int i;
  int result_size;

  rasqal_skiplist_init_with_seed(1234567890);
    
  if(argc < 1 || argc > 2) {
    fprintf(stderr, "%s: USAGE: %s [<size>]\n", program, program);
    exit(1);
  }
  
  if(argc == 2)
    size = atoi(argv[1]);
  else
    size = DEFAULT_TEST_SIZE;
  
  list=rasqal_new_skiplist(rasqal_skiplist_int_compare,
                           NULL,
                           rasqal_skiplist_int_print,
                           rasqal_skiplist_int_print,
                           0);
  if(!list) {
    fprintf(stderr, "%s: Creating new skiplist failed\n", program);
    exit(1);
  }
  
  keys = (int*)RASQAL_CALLOC(intarray, size, sizeof(int));
  values = (int*)RASQAL_CALLOC(intarray, size, sizeof(int));
  if(!keys || !values) {
    fprintf(stderr, "%s: Out of memory\n", program);
    exit(1);
  }
  
  for(i = 0; i < size; i++) {
    keys[i]   = random();
    values[i] = i;
  }
  fprintf(stdout, "%s: Testing with %d random-keyed items\n", program, size);
  
  for(i = 0; i < size; i++) {
    if(rasqal_skiplist_insert(list, &keys[i], &values[i])) {
      fprintf(stderr, "%s: insert failed for %d:%d ", program,
              keys[i], values[i]);
      fputs("\n", stderr);
    }
  }

  if(rasqal_skiplist_get_size(list) != size) {
    fprintf(stderr, "%s: skiplist has %d items, expected %d\n", program,
            rasqal_skiplist_get_size(list), size);
    exit(1);
  }
  
  for(i=0; i < size; i++) {
    int* result=(int*)rasqal_skiplist_find(list, &keys[i]);
    if(!result) {
      fprintf(stderr, "%s: find failed to find key %d\n",
              program, keys[i]);
      exit(1);
    }
  }
  
  result_size=size;
  for(i=0; i < size; i++) {
    if(rasqal_skiplist_delete(list, &keys[i])) {
      fprintf(stderr, "%s: delete failed with key '%d'\n",
              program, keys[i]);
      exit(1);
    }

    result_size--;
    
    if(rasqal_skiplist_get_size(list) != result_size) {
      fprintf(stderr, "%s: after deleting, skiplist has %d items, expected %d\n", 
              program, rasqal_skiplist_get_size(list), result_size);
      exit(1);
    }
  }

  rasqal_free_skiplist(list);

  RASQAL_FREE(intarray, values);
  RASQAL_FREE(intarray, keys);

  rasqal_skiplist_finish();
  
  /* keep gcc -Wall happy */
  return 0;
}

#endif

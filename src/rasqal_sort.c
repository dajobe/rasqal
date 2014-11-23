/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_sort.c - Sorting utility functions
 *
 * Copyright (C) 2014, David Beckett http://www.dajobe.org/
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
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <raptor.h>

/* Rasqal includes */
#include <rasqal.h>
#include <rasqal_internal.h>

#include "ssort.h"


/*
 * rasqal_sequence_as_sorted:
 * @seq: sequence to sort
 * @compare: comparison function (a, b, user_data)
 * @user_data: user_data for @compare
 *
 * INTERNAL - Sort the entries in a sequence and return the sort
 * order as an array of pointers (NULL terminated)
 *
 * Return value: array or NULL on failure (or sequence is empty)
 */
void**
rasqal_sequence_as_sorted(raptor_sequence* seq,
                          raptor_data_compare_arg_handler compare,
                          void* user_data)
{
  void** array;
  size_t size;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(seq, raptor_sequence, NULL);

  size = RASQAL_BAD_CAST(size_t, raptor_sequence_size(seq));

  array = RASQAL_CALLOC(void**, size + 1, sizeof(void*));
  if(!array)
    return NULL;

  if(size) {
    size_t i;

    for(i = 0; i < size; i++)
      array[i] = raptor_sequence_get_at(seq, RASQAL_GOOD_CAST(int, i));

    rasqal_ssort_r(array, size, sizeof(void*), compare, user_data);
  }

  return array;
}

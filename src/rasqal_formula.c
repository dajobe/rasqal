/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_formula.c - Rasqal formula class
 *
 * Copyright (C) 2010, David Beckett http://www.dajobe.org/
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
#include <stdarg.h>

#include "rasqal.h"
#include "rasqal_internal.h"


rasqal_formula*
rasqal_new_formula(rasqal_world* world) 
{
  rasqal_formula* f;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);

  f = RASQAL_CALLOC(rasqal_formula*, 1, sizeof(*f));
  if(!f)
    return NULL;
  
  f->world = world;
  return f;
}

void
rasqal_free_formula(rasqal_formula* formula)
{
  if(!formula)
    return;
  
  if(formula->triples)
    raptor_free_sequence(formula->triples);
  if(formula->value)
    rasqal_free_literal(formula->value);
  RASQAL_FREE(rasqal_formula, formula);
}
  

int
rasqal_formula_print(rasqal_formula* formula, FILE *stream)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(formula, rasqal_formula, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(stream, FILE*, 1);

  fputs("formula(triples=", stream);
  if(formula->triples)
    raptor_sequence_print(formula->triples, stream);
  else
    fputs("[]", stream);
  fputs(", value=", stream);
  rasqal_literal_print(formula->value, stream);
  fputc(')', stream);

  return 0;
}


rasqal_formula*
rasqal_formula_join(rasqal_formula* first_formula, 
                    rasqal_formula* second_formula)
{
  if(!first_formula && !second_formula)
    return NULL;

  if(!first_formula)
    return second_formula;
  
  if(!second_formula)
    return first_formula;
  
  if(first_formula->triples || second_formula->triples) {
    if(!first_formula->triples) {
      first_formula->triples=second_formula->triples;
      second_formula->triples=NULL;
    } else if(second_formula->triples)
      if(raptor_sequence_join(first_formula->triples, second_formula->triples)) {
        rasqal_free_formula(first_formula);
        first_formula=NULL;
      }
  }
  rasqal_free_formula(second_formula);

  return first_formula;
}

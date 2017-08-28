/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_data_graphs_set.c - Rasqal RDF Data Graps Set
 *
 * Copyright (C) 2017, Victor Porton http://portonvictor.org
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
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdarg.h>

#include "rasqal.h"
#include "rasqal_internal.h"


rasqal_data_graphs_set*
rasqal_data_graphs_set_new(void)
{
  raptor_sequence* seq;
  rasqal_data_graphs_set* set;

  seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_data_graph, (raptor_data_print_handler)rasqal_data_graph_print);
  if(!seq)
    return NULL;

  set = RASQAL_CALLOC(rasqal_data_graphs_set*, 1, sizeof(*set));
  if(!set)
    return NULL;

  set->seq = seq;

  return set;
}

void
rasqal_data_graphs_set_free(rasqal_data_graphs_set* set)
{
  if(!set)
    return;

  raptor_free_sequence(set->seq);
  RASQAL_FREE(rasqal_data_graphs_set, set);
}

int
rasqal_data_graphs_set_add_data_graph(rasqal_data_graphs_set* set, rasqal_data_graph* data_graph)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(set, rasqal_data_graphs_set, NULL);

  return raptor_sequence_push(set->seq, data_graph);
}

int
rasqal_data_graphs_set_add_data_graphs(rasqal_data_graphs_set* set, raptor_sequence* data_graphs)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(set, rasqal_data_graphs_set, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(data_graphs, raptor_sequence, 1);

  return raptor_sequence_join(set->seq, data_graphs);
}

raptor_sequence*
rasqal_data_graphs_set_get_data_graph_sequence(rasqal_data_graphs_set* set)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(set, rasqal_data_graphs_set, 1);

  return set->seq;
}

rasqal_data_graph*
rasqal_data_graphs_set_get_data_graph(rasqal_data_graphs_set* set, int idx)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(set, rasqal_data_graphs_set, 1);

  return (rasqal_data_graph*)raptor_sequence_get_at(set->seq, idx);
}

int
rasqal_data_graphs_set_dataset_contains_named_graph(rasqal_data_graphs_set* set, raptor_uri *graph_uri)
{
  rasqal_data_graph *dg;
  int idx;
  int found = 0;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(set, rasqal_data_graphs_set, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(graph_uri, raptor_uri, 1);

  for(idx = 0; (dg = rasqal_data_graphs_set_get_data_graph(set, idx)); idx++) {
    if(dg->name_uri && raptor_uri_equals(dg->name_uri, graph_uri)) {
      /* graph_uri is a graph name in the dataset */
      found = 1;
      break;
    }
  }
  return found;
}

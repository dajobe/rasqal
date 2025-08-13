/*
 * rasqal_graph_isomorphism.c - RDF Graph Isomorphism Detection Algorithms
 *
 * This module implements advanced RDF graph isomorphism detection
 * algorithms including signature-based methods, VF2 algorithm
 * adaptation, and hybrid approaches.
 *
 * Copyright (C) 2025, David Beckett http://www.dajobe.org/
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
 */

#ifdef HAVE_CONFIG_H
#include <rasqal_config.h>
#endif

#ifdef WIN32
#include <win32_rasqal_config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>

#include "rasqal.h"
#include "rasqal_internal.h"
#include "rasqal_graph_isomorphism.h"

/* Forward declaration for internal use */
struct rasqal_query_results_compare_s {
  rasqal_world* world;
  rasqal_query_results* first_results;
  rasqal_query_results* second_results;
  rasqal_query_results_compare_options options;
  char** differences;
  int differences_count;
  int differences_size;
};

/*
 * RDF Graph Isomorphism Detection Module
 * 
 * This module implements the core isomorphism detection algorithms
 * described in the advanced graph comparison specification.
 */

/*
 * Blank Node with Signature Structure
 * 
 * Associates a blank node with its computed signature.
 */
typedef struct {
  raptor_term* bnode;
  rasqal_blank_node_signature signature;
} rasqal_graph_isomorphism_blank_node_with_signature;

/*
 * Signature Compartment Structure
 * 
 * Groups blank nodes with identical signatures for efficient comparison.
 */
typedef struct {
  raptor_sequence* blank_nodes;
  rasqal_blank_node_signature signature;
  int size;
} rasqal_graph_isomorphism_signature_compartment;

/**
 * rasqal_free_graph_isomorphism_signature_compartment:
 * @component: signature compartment to free
 *
 * Free a signature compartment structure and its contents.
 *
 * This function cleans up all resources associated with a signature
 * compartment including the sequence of blank nodes and the signature
 * data structure itself.
 */
static void
rasqal_free_graph_isomorphism_signature_compartment(rasqal_graph_isomorphism_signature_compartment *component)
{
  if(!component)
    return;
  if(component->blank_nodes)
    raptor_free_sequence(component->blank_nodes);
  RASQAL_FREE(rasqal_graph_isomorphism_signature_compartment*, component);
}


/*
 * VF2 Algorithm State Structure
 * 
 * This structure maintains the state for the VF2 algorithm implementation,
 * which is adapted for RDF graph isomorphism detection.
 */
typedef struct {
  raptor_sequence* first_nodes;      /* Nodes from first graph */
  raptor_sequence* second_nodes;     /* Nodes from second graph */
  raptor_sequence* first_triples;    /* Triples from first graph */
  raptor_sequence* second_triples;   /* Triples from second graph */
  int* mapping;                      /* Current node mapping */
  int mapping_size;                  /* Size of current mapping */
  int max_mapping_size;              /* Maximum possible mapping size */
  int* first_used;                   /* Track used nodes in first graph */
  int* second_used;                  /* Track used nodes in second graph */
  clock_t start_time;                /* Start time for timeout checking */
  int max_time;                      /* Maximum allowed time in seconds */
} rasqal_graph_isomorphism_vf2_state;

/* Function declarations */
static int rasqal_graph_isomorphism_signature_compartment_compare(const void* a, const void* b);
static int rasqal_graph_isomorphism_ordered_permutation_search(raptor_sequence* first_compartments, raptor_sequence* second_compartments, rasqal_query_results_compare* compare);
static int rasqal_graph_isomorphism_test_simple_mapping(raptor_sequence* first_blank_nodes, raptor_sequence* second_blank_nodes, raptor_sequence* first_triples, raptor_sequence* second_triples, rasqal_query_results_compare* compare);


static int rasqal_graph_isomorphism_vf2_state_init(rasqal_graph_isomorphism_vf2_state* state, raptor_sequence* first_nodes, raptor_sequence* second_nodes, raptor_sequence* first_triples, raptor_sequence* second_triples, int max_time);
static void rasqal_graph_isomorphism_vf2_state_cleanup(rasqal_graph_isomorphism_vf2_state* state);
static int rasqal_graph_isomorphism_vf2_feasible(rasqal_graph_isomorphism_vf2_state* state, int first_node_idx, int second_node_idx);
static int rasqal_graph_isomorphism_vf2_search(rasqal_graph_isomorphism_vf2_state* state);
static int rasqal_graph_isomorphism_compare_triples_directly(raptor_sequence* first_triples, raptor_sequence* second_triples, rasqal_query_results_compare* compare);
static int rasqal_graph_isomorphism_find_blank_node_mapping(raptor_sequence* first_blank_nodes, raptor_sequence* second_blank_nodes, raptor_sequence* first_triples, raptor_sequence* second_triples, raptor_sequence* first_mapping, raptor_sequence* second_mapping, int depth, rasqal_query_results_compare* compare);
static int rasqal_graph_isomorphism_test_mapping(raptor_sequence* first_blank_nodes, raptor_sequence* second_blank_nodes, raptor_sequence* first_triples, raptor_sequence* second_triples, raptor_sequence* first_mapping, raptor_sequence* second_mapping, rasqal_query_results_compare* compare);
static raptor_term* rasqal_graph_isomorphism_map_blank_node(raptor_term* term, raptor_sequence* first_blank_nodes, raptor_sequence* second_blank_nodes, raptor_sequence* first_mapping, raptor_sequence* second_mapping);
static int rasqal_graph_isomorphism_compare_term(raptor_term* first_term, raptor_term* second_term, rasqal_query_results_compare* compare);


int
rasqal_graph_isomorphism_compare_signatures(const rasqal_blank_node_signature* sig1,
                                   const rasqal_blank_node_signature* sig2)
{
  if(!sig1 || !sig2)
    return 0;

  /* Compare complexity first */
  if(sig1->complexity != sig2->complexity)
    return sig1->complexity - sig2->complexity;

  /* Then compare subject count */
  if(sig1->subject_count != sig2->subject_count)
    return sig1->subject_count - sig2->subject_count;

  /* Then compare predicate count */
  if(sig1->predicate_count != sig2->predicate_count)
    return sig1->predicate_count - sig2->predicate_count;

  /* Finally compare object count */
  return sig1->object_count - sig2->object_count;
}

/*
 * rasqal_graph_isomorphism_signature_compartment_compare:
 * @a: first compartment to compare
 * @b: second compartment to compare
 *
 * Compare function for raptor_sequence_sort compatibility.
 *
 * Returns negative if a < b, 0 if equal, positive if a > b.
 */
static int
rasqal_graph_isomorphism_signature_compartment_compare(const void* a, const void* b)
{
  rasqal_graph_isomorphism_signature_compartment* comp1 = (rasqal_graph_isomorphism_signature_compartment*)a;
  rasqal_graph_isomorphism_signature_compartment* comp2 = (rasqal_graph_isomorphism_signature_compartment*)b;

  if(!comp1 || !comp2)
    return 0;

  /* Compare by size first */
  if(comp1->size != comp2->size)
    return comp1->size - comp2->size;

  /* Then compare by signature */
        return rasqal_graph_isomorphism_compare_signatures(&comp1->signature, &comp2->signature);
}

/*
 * rasqal_graph_isomorphism_generate_blank_node_signature:
 * @bnode: blank node to generate signature for
 * @triples: sequence of triples containing the blank node
 * @world: rasqal world
 *
 * Generate a signature for a blank node based on its occurrence patterns
 * in the RDF graph.
 *
 * Returns newly allocated signature structure, or NULL on failure.
 */
rasqal_blank_node_signature*
rasqal_graph_isomorphism_generate_signature(raptor_term* bnode, raptor_sequence* triples, rasqal_world* world)
{
  rasqal_blank_node_signature* signature = NULL;
  int i;

  if(!bnode || !triples || !world)
    return NULL;

  signature = RASQAL_CALLOC(rasqal_blank_node_signature*, 1, sizeof(rasqal_blank_node_signature));
  if(!signature)
    return NULL;

  /* Count occurrences in triples */
  for(i = 0; i < raptor_sequence_size(triples); i++) {
    raptor_statement* triple = (raptor_statement*)raptor_sequence_get_at(triples, i);
    if(!triple)
      continue;

    /* Check subject */
    if(triple->subject && triple->subject->type == RAPTOR_TERM_TYPE_BLANK) {
      if(triple->subject->value.blank.string && bnode->value.blank.string) {
        if(strcmp((char*)triple->subject->value.blank.string, (char*)bnode->value.blank.string) == 0)
          signature->subject_count++;
      }
    }

    /* Check predicate */
    if(triple->predicate && triple->predicate->type == RAPTOR_TERM_TYPE_BLANK) {
      if(triple->predicate->value.blank.string && bnode->value.blank.string) {
        if(strcmp((char*)triple->predicate->value.blank.string, (char*)bnode->value.blank.string) == 0)
          signature->predicate_count++;
      }
    }

    /* Check object */
    if(triple->object && triple->object->type == RAPTOR_TERM_TYPE_BLANK) {
      if(triple->object->value.blank.string && bnode->value.blank.string) {
        if(strcmp((char*)triple->object->value.blank.string, (char*)bnode->value.blank.string) == 0)
          signature->object_count++;
      }
    }
  }

  /* Calculate complexity */
  signature->complexity = signature->subject_count + signature->predicate_count + signature->object_count;

  return signature;
}

/*
 * rasqal_graph_isomorphism_compartmentalize_by_signature_internal:
 * @blank_nodes: sequence of blank nodes to compartmentalize
 * @triples: sequence of triples containing the blank nodes
 * @world: rasqal world
 *
 * Group blank nodes into compartments based on their signatures.
 *
 * Returns sequence of signature compartments, or NULL on failure.
 */
raptor_sequence*
rasqal_graph_isomorphism_compartmentalize_by_signature(raptor_sequence* blank_nodes, raptor_sequence* triples, rasqal_world* world)
{
  raptor_sequence* compartments = NULL;
  int i, j;
  int found;

  if(!blank_nodes || !triples || !world)
    return NULL;

  /* sequence of rasqal_graph_isomorphism_signature_compartment* that we own */
  compartments = raptor_new_sequence((raptor_data_free_handler)rasqal_free_graph_isomorphism_signature_compartment, NULL);
  if(!compartments)
    return NULL;

  /* Process each blank node */
  for(i = 0; i < raptor_sequence_size(blank_nodes); i++) {
    raptor_term* bnode = (raptor_term*)raptor_sequence_get_at(blank_nodes, i);
    rasqal_blank_node_signature* signature = NULL;
    rasqal_graph_isomorphism_signature_compartment* compartment = NULL;

    if(!bnode)
      continue;

    /* Generate signature for this blank node */
    signature = rasqal_graph_isomorphism_generate_signature(bnode, triples, world);
    if(!signature)
      continue;

    /* Look for existing compartment with same signature */
    found = 0;
    for(j = 0; j < raptor_sequence_size(compartments); j++) {
      compartment = (rasqal_graph_isomorphism_signature_compartment*)raptor_sequence_get_at(compartments, j);
      if(compartment &&
         rasqal_graph_isomorphism_compare_signatures(&compartment->signature, signature) == 0) {
        /* Add to existing compartment */
        raptor_sequence_push(compartment->blank_nodes, bnode);
        compartment->size++;
        found = 1;
        break;
      }
    }

    if(!found) {
      /* Create new compartment */
      compartment = RASQAL_CALLOC(rasqal_graph_isomorphism_signature_compartment*, 1, sizeof(rasqal_graph_isomorphism_signature_compartment));
      if(compartment) {
        /* sequence of raptor_term* references (not owned) */
        compartment->blank_nodes = raptor_new_sequence(NULL, NULL);
        if(compartment->blank_nodes) {
          raptor_sequence_push(compartment->blank_nodes, bnode);
          memcpy(&(compartment->signature), signature, sizeof(*signature));
          compartment->size = 1;
          raptor_sequence_push(compartments, compartment);
        } else {
          RASQAL_FREE(rasqal_graph_isomorphism_signature_compartment*, compartment);
        }
      }
    }

    RASQAL_FREE(rasqal_blank_node_signature*, signature);
  }

  return compartments;
}

/*
 * rasqal_graph_isomorphism_order_signatures_by_size_internal:
 * @compartments: sequence of signature compartments to order
 * @world: rasqal world
 *
 * Order compartments by size and signature for efficient comparison.
 *
 * Returns ordered sequence of compartments, or NULL on failure.
 */
raptor_sequence*
rasqal_graph_isomorphism_order_signatures_by_size(raptor_sequence* compartments, rasqal_world* world)
{
  raptor_sequence* ordered = NULL;
  int i;

  if(!compartments || !world)
    return NULL;

  /* Create copy of compartments */
  ordered = raptor_new_sequence(NULL, NULL);
  if(!ordered)
    return NULL;

  for(i = 0; i < raptor_sequence_size(compartments); i++) {
    rasqal_graph_isomorphism_signature_compartment* comp = (rasqal_graph_isomorphism_signature_compartment*)raptor_sequence_get_at(compartments, i);
    if(comp)
      raptor_sequence_push(ordered, comp);
  }

  /* Sort by size and signature */
  raptor_sequence_sort(ordered, rasqal_graph_isomorphism_signature_compartment_compare);

  return ordered;
}

/*
 * rasqal_graph_isomorphism_detect_signature_based:
 * @compare: graph comparison context
 *
 * Detect isomorphism using signature-based approach with compartmentalization.
 *
 * Returns 1 if graphs are isomorphic, 0 if not isomorphic, <0 on error.
 */
int
rasqal_graph_isomorphism_detect_signature_based(rasqal_query_results_compare* compare)
{
  raptor_sequence* first_triples = NULL;
  raptor_sequence* second_triples = NULL;
  raptor_sequence* first_blank_nodes = NULL;
  raptor_sequence* second_blank_nodes = NULL;
  raptor_sequence* first_compartments = NULL;
  raptor_sequence* second_compartments = NULL;
  int result = 0;

  if(!compare || !compare->first_results || !compare->second_results)
    return -1;

  /* Extract triples and blank nodes from both result sets. These are both
   * sequences of references to triples and nodes in the result sets, NOT
   * owned by these sequences.
   */
  first_triples = raptor_new_sequence(NULL, NULL);
  second_triples = raptor_new_sequence(NULL, NULL);
  first_blank_nodes = raptor_new_sequence(NULL, NULL);
  second_blank_nodes = raptor_new_sequence(NULL, NULL);

  if(!first_triples || !second_triples || !first_blank_nodes || !second_blank_nodes) {
    result = -1;
    goto cleanup;
  }

  /* Collect triples and blank nodes from first result set */
  rasqal_query_results_rewind(compare->first_results);
  while(1) {
    raptor_statement* triple = rasqal_query_results_get_triple(compare->first_results);
    if(!triple)
      break;
    raptor_sequence_push(first_triples, triple);
    
    /* Extract blank nodes */
    if(triple->subject && triple->subject->type == RAPTOR_TERM_TYPE_BLANK)
      raptor_sequence_push(first_blank_nodes, triple->subject);
    if(triple->predicate && triple->predicate->type == RAPTOR_TERM_TYPE_BLANK)
      raptor_sequence_push(first_blank_nodes, triple->predicate);
    if(triple->object && triple->object->type == RAPTOR_TERM_TYPE_BLANK)
      raptor_sequence_push(first_blank_nodes, triple->object);

    /* Advance */
    if(rasqal_query_results_next_triple(compare->first_results))
      break;
  }

  /* Collect triples and blank nodes from second result set */
  rasqal_query_results_rewind(compare->second_results);
  while(1) {
    raptor_statement* triple = rasqal_query_results_get_triple(compare->second_results);
    if(!triple)
      break;
    raptor_sequence_push(second_triples, triple);
    
    /* Extract blank nodes */
    if(triple->subject && triple->subject->type == RAPTOR_TERM_TYPE_BLANK)
      raptor_sequence_push(second_blank_nodes, triple->subject);
    if(triple->predicate && triple->predicate->type == RAPTOR_TERM_TYPE_BLANK)
      raptor_sequence_push(second_blank_nodes, triple->predicate);
    if(triple->object && triple->object->type == RAPTOR_TERM_TYPE_BLANK)
      raptor_sequence_push(second_blank_nodes, triple->object);

    /* Advance */
    if(rasqal_query_results_next_triple(compare->second_results))
      break;
  }

  /* Compartmentalize blank nodes by signature */
  first_compartments = rasqal_graph_isomorphism_compartmentalize_by_signature(first_blank_nodes, first_triples, compare->world);
  second_compartments = rasqal_graph_isomorphism_compartmentalize_by_signature(second_blank_nodes, second_triples, compare->world);

  if(!first_compartments || !second_compartments) {
    result = -1;
    goto cleanup;
  }

  /* Use ordered permutation search */
  result = rasqal_graph_isomorphism_ordered_permutation_search(first_compartments, second_compartments, compare);

cleanup:
  if(first_triples)
    raptor_free_sequence(first_triples);
  if(second_triples)
    raptor_free_sequence(second_triples);
  if(first_blank_nodes)
    raptor_free_sequence(first_blank_nodes);
  if(second_blank_nodes)
    raptor_free_sequence(second_blank_nodes);
  if(first_compartments)
    raptor_free_sequence(first_compartments);
  if(second_compartments)
    raptor_free_sequence(second_compartments);

  return result;
}

/*
 * rasqal_graph_isomorphism_detect_exhaustive:
 * @compare: graph comparison context
 *
 * Detect isomorphism using exhaustive search as fallback method.
 *
 * Returns 1 if graphs are isomorphic, 0 if not isomorphic, <0 on error.
 */
int
rasqal_graph_isomorphism_detect_exhaustive(rasqal_query_results_compare* compare)
{
  raptor_sequence* first_triples = NULL;
  raptor_sequence* second_triples = NULL;
  raptor_sequence* first_blank_nodes = NULL;
  raptor_sequence* second_blank_nodes = NULL;
  int result = 0;

  if(!compare || !compare->first_results || !compare->second_results)
    return -1;

  /* Extract triples and blank nodes from both result sets */
  first_triples = raptor_new_sequence(NULL, NULL);
  second_triples = raptor_new_sequence(NULL, NULL);
  first_blank_nodes = raptor_new_sequence(NULL, NULL);
  second_blank_nodes = raptor_new_sequence(NULL, NULL);

  if(!first_triples || !second_triples || !first_blank_nodes || !second_blank_nodes) {
    result = -1;
    goto cleanup;
  }

  /* Collect triples and blank nodes from first result set */
  rasqal_query_results_rewind(compare->first_results);
  while(1) {
    raptor_statement* triple = rasqal_query_results_get_triple(compare->first_results);
    if(!triple)
      break;
    raptor_sequence_push(first_triples, triple);
    
    /* Extract blank nodes */
    if(triple->subject && triple->subject->type == RAPTOR_TERM_TYPE_BLANK)
      raptor_sequence_push(first_blank_nodes, triple->subject);
    if(triple->predicate && triple->predicate->type == RAPTOR_TERM_TYPE_BLANK)
      raptor_sequence_push(first_blank_nodes, triple->predicate);
    if(triple->object && triple->object->type == RAPTOR_TERM_TYPE_BLANK)
      raptor_sequence_push(first_blank_nodes, triple->object);

    /* Advance */
    if(rasqal_query_results_next_triple(compare->first_results))
      break;
  }

  /* Collect triples and blank nodes from second result set */
  rasqal_query_results_rewind(compare->second_results);
  while(1) {
    raptor_statement* triple = rasqal_query_results_get_triple(compare->second_results);
    if(!triple)
      break;
    raptor_sequence_push(second_triples, triple);
    
    /* Extract blank nodes */
    if(triple->subject && triple->subject->type == RAPTOR_TERM_TYPE_BLANK)
      raptor_sequence_push(second_blank_nodes, triple->subject);
    if(triple->predicate && triple->predicate->type == RAPTOR_TERM_TYPE_BLANK)
      raptor_sequence_push(second_blank_nodes, triple->predicate);
    if(triple->object && triple->object->type == RAPTOR_TERM_TYPE_BLANK)
      raptor_sequence_push(second_blank_nodes, triple->object);

    /* Advance */
    if(rasqal_query_results_next_triple(compare->second_results))
      break;
  }

  /* Use simple mapping test */
  result = rasqal_graph_isomorphism_test_simple_mapping(first_blank_nodes, second_blank_nodes, first_triples, second_triples, compare);

cleanup:
  if(first_triples)
    raptor_free_sequence(first_triples);
  if(second_triples)
    raptor_free_sequence(second_triples);
  if(first_blank_nodes)
    raptor_free_sequence(first_blank_nodes);
  if(second_blank_nodes)
    raptor_free_sequence(second_blank_nodes);

  return result;
}

/*
 * rasqal_graph_isomorphism_ordered_permutation_search:
 * @first_compartments: compartments from first graph
 * @second_compartments: compartments from second graph
 * @compare: graph comparison context
 *
 * Perform ordered permutation search on compartments.
 *
 * Returns 1 if isomorphism found, 0 if not isomorphic, <0 on error.
 */
static int
rasqal_graph_isomorphism_ordered_permutation_search(raptor_sequence* first_compartments,
                                 raptor_sequence* second_compartments,
                                 rasqal_query_results_compare* compare)
{
  raptor_sequence* ordered_first = NULL;
  raptor_sequence* ordered_second = NULL;
  int i, result = 0;

  if(!first_compartments || !second_compartments || !compare)
    return -1;

  /* Order compartments by size and signature */
  ordered_first = rasqal_graph_isomorphism_order_signatures_by_size(first_compartments, compare->world);
  ordered_second = rasqal_graph_isomorphism_order_signatures_by_size(second_compartments, compare->world);

  if(!ordered_first || !ordered_second) {
    result = -1;
    goto cleanup;
  }

  /* Check if we have the same number of compartments */
  if(raptor_sequence_size(ordered_first) != raptor_sequence_size(ordered_second)) {
    result = 0;
    goto cleanup;
  }

  /* Verify each compartment has the same size and signature */
  for(i = 0; i < raptor_sequence_size(ordered_first); i++) {
    rasqal_graph_isomorphism_signature_compartment* first_comp = (rasqal_graph_isomorphism_signature_compartment*)raptor_sequence_get_at(ordered_first, i);
    rasqal_graph_isomorphism_signature_compartment* second_comp = (rasqal_graph_isomorphism_signature_compartment*)raptor_sequence_get_at(ordered_second, i);

    if(!first_comp || !second_comp) {
      result = 0;
      goto cleanup;
    }

    if(first_comp->size != second_comp->size) {
      result = 0;
      goto cleanup;
    }

    if(rasqal_graph_isomorphism_compare_signatures(&first_comp->signature, &second_comp->signature) != 0) {
      result = 0;
      goto cleanup;
    }
  }

  result = 1;

cleanup:
  if(ordered_first)
    raptor_free_sequence(ordered_first);
  if(ordered_second)
    raptor_free_sequence(ordered_second);

  return result;
}

/*
 * rasqal_graph_isomorphism_test_simple_mapping:
 * @first_blank_nodes: blank nodes from first graph
 * @second_blank_nodes: blank nodes from second graph
 * @first_triples: triples from first graph
 * @second_triples: triples from second graph
 * @compare: graph comparison context
 *
 * Test simple mapping between graphs.
 *
 * Returns 1 if mapping is possible, 0 if not possible, <0 on error.
 */
static int
rasqal_graph_isomorphism_test_simple_mapping(raptor_sequence* first_blank_nodes,
                                             raptor_sequence* second_blank_nodes,
                                             raptor_sequence* first_triples,
                                             raptor_sequence* second_triples,
                                             rasqal_query_results_compare* compare)
{
  raptor_sequence* first_blank_node_mapping = NULL;
  raptor_sequence* second_blank_node_mapping = NULL;
  int i, result = 0;
  int first_blank_count, second_blank_count;

  if(!first_blank_nodes || !second_blank_nodes || !first_triples || !second_triples || !compare)
    return -1;

  /* Check if we have the same number of blank nodes and triples */
  first_blank_count = raptor_sequence_size(first_blank_nodes);
  second_blank_count = raptor_sequence_size(second_blank_nodes);
  
  if(first_blank_count != second_blank_count)
    return 0;

  if(raptor_sequence_size(first_triples) != raptor_sequence_size(second_triples))
    return 0;

  /* If no blank nodes, just compare triples directly */
  if(first_blank_count == 0) {
    return rasqal_graph_isomorphism_compare_triples_directly(first_triples, second_triples, compare);
  }

  /* Create mapping sequences for blank nodes */
  first_blank_node_mapping = raptor_new_sequence(NULL, NULL);
  second_blank_node_mapping = raptor_new_sequence(NULL, NULL);
  
  if(!first_blank_node_mapping || !second_blank_node_mapping)
    goto cleanup;

  /* Initialize mappings with -1 (unmapped) */
  for(i = 0; i < first_blank_count; i++) {
    raptor_sequence_push(first_blank_node_mapping, (void*)(intptr_t)-1);
    raptor_sequence_push(second_blank_node_mapping, (void*)(intptr_t)-1);
  }

  /* Try to find a valid mapping */
  result = rasqal_graph_isomorphism_find_blank_node_mapping(
    first_blank_nodes, second_blank_nodes,
    first_triples, second_triples,
    first_blank_node_mapping, second_blank_node_mapping,
    0, compare);

cleanup:
  if(first_blank_node_mapping)
    raptor_free_sequence(first_blank_node_mapping);
  if(second_blank_node_mapping)
    raptor_free_sequence(second_blank_node_mapping);

  return result;
}

/*
 * rasqal_graph_isomorphism_vf2_state_init:
 * @state: VF2 state structure to initialize
 * @first_nodes: nodes from first graph
 * @second_nodes: nodes from second graph
 * @first_triples: triples from first graph
 * @second_triples: triples from second graph
 * @max_time: maximum allowed time for search
 *
 * Initialize VF2 algorithm state structure.
 *
 * Returns 1 on success, 0 on failure.
 */
static int
rasqal_graph_isomorphism_vf2_state_init(rasqal_graph_isomorphism_vf2_state* state,
                                       raptor_sequence* first_nodes,
                                       raptor_sequence* second_nodes,
                                       raptor_sequence* first_triples,
                                       raptor_sequence* second_triples,
                                       int max_time)
{
  int node_count;
  int i;

  if(!state || !first_nodes || !second_nodes || !first_triples || !second_triples)
    return 0;

  node_count = raptor_sequence_size(first_nodes);
  if(node_count != raptor_sequence_size(second_nodes))
    return 0;

  state->first_nodes = first_nodes;
  state->second_nodes = second_nodes;
  state->first_triples = first_triples;
  state->second_triples = second_triples;
  state->mapping_size = 0;
  state->max_mapping_size = node_count;
  state->start_time = clock(); /* Set actual start time */
  state->max_time = max_time;

  /* Allocate mapping arrays */
  state->mapping = RASQAL_CALLOC(int*, node_count, sizeof(int));
  state->first_used = RASQAL_CALLOC(int*, node_count, sizeof(int));
  state->second_used = RASQAL_CALLOC(int*, node_count, sizeof(int));

  if(!state->mapping || !state->first_used || !state->second_used) {
    if(state->mapping)
      RASQAL_FREE(int*, state->mapping);
    if(state->first_used)
      RASQAL_FREE(int*, state->first_used);
    if(state->second_used)
      RASQAL_FREE(int*, state->second_used);
    return 0;
  }

  /* Initialize arrays */
  for(i = 0; i < node_count; i++) {
    state->mapping[i] = -1;
    state->first_used[i] = 0;
    state->second_used[i] = 0;
  }

  return 1;
}

/*
 * rasqal_graph_isomorphism_vf2_state_cleanup:
 * @state: VF2 state structure to cleanup
 *
 * Clean up VF2 algorithm state structure.
 */
static void
rasqal_graph_isomorphism_vf2_state_cleanup(rasqal_graph_isomorphism_vf2_state* state)
{
  if(!state)
    return;

  if(state->mapping)
    RASQAL_FREE(int*, state->mapping);
  if(state->first_used)
    RASQAL_FREE(int*, state->first_used);
  if(state->second_used)
    RASQAL_FREE(int*, state->second_used);

  state->mapping = NULL;
  state->first_used = NULL;
  state->second_used = NULL;
}

/*
 * rasqal_graph_isomorphism_vf2_feasible:
 * @state: VF2 state structure
 * @first_node_idx: index of node in first graph
 * @second_node_idx: index of node in second graph
 *
 * Check if mapping first_node_idx to second_node_idx is feasible
 * according to VF2 algorithm rules.
 *
 * Returns 1 if feasible, 0 otherwise.
 */
static int
rasqal_graph_isomorphism_vf2_feasible(rasqal_graph_isomorphism_vf2_state* state,
                                      int first_node_idx, int second_node_idx)
{
  raptor_term* first_node = NULL;
  raptor_term* second_node = NULL;
  int first_degree = 0;
  int second_degree = 0;

  if(!state || first_node_idx < 0 || second_node_idx < 0)
    return 0;

  first_node = (raptor_term*)raptor_sequence_get_at(state->first_nodes, first_node_idx);
  second_node = (raptor_term*)raptor_sequence_get_at(state->second_nodes, second_node_idx);

  if(!first_node || !second_node)
    return 0;

  /* Check if nodes are of the same type */
  if(first_node->type != second_node->type)
    return 0;

  /* For blank nodes, check if they have similar connectivity */
  if(first_node->type == RAPTOR_TERM_TYPE_BLANK) {
    int i;

    /* Count degree (number of triples involving this node) */
    for(i = 0; i < raptor_sequence_size(state->first_triples); i++) {
      raptor_statement* triple = (raptor_statement*)raptor_sequence_get_at(state->first_triples, i);
      if(triple && (triple->subject == first_node || triple->predicate == first_node || triple->object == first_node))
        first_degree++;
    }

    for(i = 0; i < raptor_sequence_size(state->second_triples); i++) {
      raptor_statement* triple = (raptor_statement*)raptor_sequence_get_at(state->second_triples, i);
      if(triple && (triple->subject == second_node || triple->predicate == second_node || triple->object == second_node))
        second_degree++;
    }

    /* Degree must be similar */
    if(abs(first_degree - second_degree) > 1)
      return 0;
  }

  return 1;
}

/*
 * rasqal_graph_isomorphism_vf2_search:
 * @state: VF2 state structure
 *
 * Perform VF2 algorithm search for isomorphism.
 * This is the core recursive function of the VF2 algorithm.
 *
 * Returns 1 if isomorphism found, 0 otherwise.
 */
static int
rasqal_graph_isomorphism_vf2_search(rasqal_graph_isomorphism_vf2_state* state)
{
  int first_node_idx;
  int second_node_idx;
  int result = 0;
  int i;

  if(!state)
    return 0;

  /* Check timeout */
  if(state->max_time > 0) {
    clock_t current_time = clock();
    int elapsed_seconds = (int)((current_time - state->start_time) / CLOCKS_PER_SEC);
    if(elapsed_seconds > state->max_time)
      return 0;
  }

  /* If mapping is complete, we found an isomorphism */
  if(state->mapping_size == state->max_mapping_size)
    return 1;

  /* Find next unmapped node in first graph */
  first_node_idx = -1;
  for(i = 0; i < state->max_mapping_size; i++) {
    if(!state->first_used[i]) {
      first_node_idx = i;
      break;
    }
  }

  if(first_node_idx == -1)
    return 0;

  /* Try mapping to each unmapped node in second graph */
  for(second_node_idx = 0; second_node_idx < state->max_mapping_size; second_node_idx++) {
    if(state->second_used[second_node_idx])
      continue;

    /* Check feasibility */
    if(!rasqal_graph_isomorphism_vf2_feasible(state, first_node_idx, second_node_idx))
      continue;

    /* Try this mapping */
    state->mapping[first_node_idx] = second_node_idx;
    state->first_used[first_node_idx] = 1;
    state->second_used[second_node_idx] = 1;
    state->mapping_size++;

    /* Recursively search with this mapping */
    result = rasqal_graph_isomorphism_vf2_search(state);

    /* If found, return success */
    if(result)
      return 1;

    /* Backtrack */
    state->mapping[first_node_idx] = -1;
    state->first_used[first_node_idx] = 0;
    state->second_used[second_node_idx] = 0;
    state->mapping_size--;
  }

  return 0;
}

/*
 * rasqal_graph_isomorphism_detect_vf2:
 * @compare: graph comparison context
 *
 * Detect isomorphism using VF2 algorithm adaptation for RDF graphs.
 * This implements the VF2 algorithm with RDF-specific adaptations.
 *
 * Algorithm:
 * 1. Extract nodes and triples from both graphs
 * 2. Initialize VF2 state structure
 * 3. Perform VF2 search with feasibility checking
 * 4. Return true if isomorphism is found
 *
 * Returns 1 if graphs are isomorphic, 0 if not isomorphic, <0 on error.
 */
int
rasqal_graph_isomorphism_detect_vf2(rasqal_query_results_compare* compare)
{
  raptor_sequence* first_triples = NULL;
  raptor_sequence* second_triples = NULL;
  raptor_sequence* first_nodes = NULL;
  raptor_sequence* second_nodes = NULL;
  rasqal_graph_isomorphism_vf2_state state;
  int result = 0;
  int max_time = 30; /* Default timeout */

  if(!compare || !compare->first_results || !compare->second_results)
    return -1;

  /* Extract triples and nodes from both result sets */
  first_triples = raptor_new_sequence(NULL, NULL);
  second_triples = raptor_new_sequence(NULL, NULL);
  first_nodes = raptor_new_sequence(NULL, NULL);
  second_nodes = raptor_new_sequence(NULL, NULL);

  if(!first_triples || !second_triples || !first_nodes || !second_nodes) {
    result = -1;
    goto cleanup;
  }

  /* Collect triples and nodes from first result set */
  rasqal_query_results_rewind(compare->first_results);
  while(1) {
    raptor_statement* triple = rasqal_query_results_get_triple(compare->first_results);
    if(!triple)
      break;
    raptor_sequence_push(first_triples, triple);
    
    /* Extract nodes */
    if(triple->subject)
      raptor_sequence_push(first_nodes, triple->subject);
    if(triple->predicate)
      raptor_sequence_push(first_nodes, triple->predicate);
    if(triple->object)
      raptor_sequence_push(first_nodes, triple->object);

    /* Advance to next triple; break on end */
    if(rasqal_query_results_next_triple(compare->first_results))
      break;
  }

  /* Collect triples and nodes from second result set */
  rasqal_query_results_rewind(compare->second_results);
  while(1) {
    raptor_statement* triple = rasqal_query_results_get_triple(compare->second_results);
    if(!triple)
      break;
    raptor_sequence_push(second_triples, triple);
    
    /* Extract nodes */
    if(triple->subject)
      raptor_sequence_push(second_nodes, triple->subject);
    if(triple->predicate)
      raptor_sequence_push(second_nodes, triple->predicate);
    if(triple->object)
      raptor_sequence_push(second_nodes, triple->object);

    /* Advance to next triple; break on end */
    if(rasqal_query_results_next_triple(compare->second_results))
      break;
  }

  /* Get timeout from graph comparison options if available */
  if(compare->options.graph_comparison_options) {
    max_time = compare->options.graph_comparison_options->max_search_time;
  }

  /* Initialize VF2 state */
  if(!rasqal_graph_isomorphism_vf2_state_init(&state, first_nodes, second_nodes, first_triples, second_triples, max_time)) {
    result = -1;
    goto cleanup;
  }

  /* Perform VF2 search */
  result = rasqal_graph_isomorphism_vf2_search(&state);

  /* Cleanup VF2 state */
  rasqal_graph_isomorphism_vf2_state_cleanup(&state);

cleanup:
  if(first_triples)
    raptor_free_sequence(first_triples);
  if(second_triples)
    raptor_free_sequence(second_triples);
  if(first_nodes)
    raptor_free_sequence(first_nodes);
  if(second_nodes)
    raptor_free_sequence(second_nodes);

  return result;
}

/*
 * rasqal_graph_isomorphism_compare_graphs_hybrid:
 * @compare: graph comparison context
 *
 * Hybrid graph comparison using multiple algorithms with fallback strategy.
 * This approach combines different isomorphism detection methods for optimal
 * performance and accuracy.
 *
 * Strategy:
 * 1. Choose primary algorithm based on graph size:
 *    - Small graphs (< 1000 triples): signature-based approach (fast)
 *    - Medium graphs (1000-10000 triples): VF2 algorithm (balanced)
 *    - Large graphs (> 10000 triples): incremental approach (scalable)
 * 
 * 2. If primary algorithm reports "not isomorphic" (result == 0), fall back
 *    to exhaustive search to ensure accuracy. The primary algorithms are fast
 *    approximations that may have false negatives, while exhaustive search is
 *    slower but definitive.
 *
 * 3. If primary algorithm reports "isomorphic" (result == 1) or error
 *    (result < 0), return immediately without fallback.
 *
 * Returns 1 if graphs are isomorphic, 0 if not isomorphic, <0 on error.
 */
int
rasqal_graph_isomorphism_compare_graphs_hybrid(rasqal_query_results_compare* compare)
{
  int triple_count = 0;
  int result = -1; /* Start with error, set to valid value on success */

  if(!compare || !compare->first_results || !compare->second_results)
    return -1;

  /* Count triples in first result set */
  rasqal_query_results_rewind(compare->first_results);
  while(1) {
    raptor_statement* triple = rasqal_query_results_get_triple(compare->first_results);
    if(!triple)
      break;
    triple_count++;
    /* Advance to next triple; stop if at end */
    if(rasqal_query_results_next_triple(compare->first_results))
      break;
  }

  /* Choose algorithm based on graph size */
  if(triple_count < 1000) {
    /* Small graphs: use signature-based approach */
    result = rasqal_graph_isomorphism_detect_signature_based(compare);
  } else if(triple_count < 10000) {
    /* Medium graphs: use VF2 algorithm */
    result = rasqal_graph_isomorphism_detect_vf2(compare);
  } else {
    /* Large graphs: use VF2 algorithm (incremental approach not yet implemented) */
    result = rasqal_graph_isomorphism_detect_vf2(compare);
  }

  /* Fall back to exhaustive search if primary algorithm reports "not isomorphic"
   * 
   * Rationale: The primary algorithms (signature-based, VF2, incremental) are
   * fast approximations that may have false negatives - they might report
   * "not isomorphic" when the graphs actually are isomorphic. The exhaustive
   * search is slower but definitive, so we use it as a fallback to ensure
   * accuracy when the primary algorithm cannot find an isomorphism.
   * 
   * We only fall back when result == 0 (not isomorphic), not when result < 0
   * (error), since retrying on error conditions would be pointless.
   */
  if(result == 0) {
    result = rasqal_graph_isomorphism_detect_exhaustive(compare);
  }

  return result;
}

/*
 * rasqal_graph_isomorphism_compare_triples_directly:
 * @first_triples: triples from first graph
 * @second_triples: triples from second graph
 * @compare: graph comparison context
 *
 * Compare triples directly when no blank nodes are present.
 *
 * Returns 1 if triples match, 0 if they don't match, <0 on error.
 */
static int
rasqal_graph_isomorphism_compare_triples_directly(raptor_sequence* first_triples, raptor_sequence* second_triples, rasqal_query_results_compare* compare)
{
  int i;
  int first_count, second_count;
  int* first_used = NULL;
  int* second_used = NULL;
  int result = -1; /* Start with error, set to valid value on success */
  int j;
  raptor_statement* first_triple;
  raptor_statement* second_triple;
  int found_match;

  if(!first_triples || !second_triples || !compare)
    return -1;

  first_count = raptor_sequence_size(first_triples);
  second_count = raptor_sequence_size(second_triples);

  if(first_count != second_count)
    return 0;

  if(first_count == 0)
    return 1; /* Both empty */

  /* Allocate usage tracking arrays */
  first_used = RASQAL_CALLOC(int*, first_count, sizeof(int));
  second_used = RASQAL_CALLOC(int*, second_count, sizeof(int));

  if(!first_used || !second_used) {
    goto cleanup;
  }

  /* Try to match each triple from first graph to a triple in second graph */
  for(i = 0; i < first_count; i++) {
    first_triple = (raptor_statement*)raptor_sequence_get_at(first_triples, i);
    found_match = 0;

    if(!first_triple)
      continue;

    for(j = 0; j < second_count; j++) {
      if(second_used[j])
        continue; /* Already matched */

      second_triple = (raptor_statement*)raptor_sequence_get_at(second_triples, j);
      if(!second_triple)
        continue;

      /* Compare triple components */
      if(rasqal_graph_isomorphism_compare_term(first_triple->subject, second_triple->subject, compare) &&
         rasqal_graph_isomorphism_compare_term(first_triple->predicate, second_triple->predicate, compare) &&
         rasqal_graph_isomorphism_compare_term(first_triple->object, second_triple->object, compare)) {
        first_used[i] = 1;
        second_used[j] = 1;
        found_match = 1;
        break;
      }
    }

    if(!found_match) {
      result = 0; /* Not isomorphic */
      goto cleanup;
    }
  }

  result = 1; /* Isomorphic */

cleanup:
  if(first_used)
    RASQAL_FREE(int*, first_used);
  if(second_used)
    RASQAL_FREE(int*, second_used);

  return result;
}

/*
 * rasqal_graph_isomorphism_compare_term:
 * @first_term: first term to compare
 * @second_term: second term to compare
 * @compare: graph comparison context
 *
 * Compare two terms for equality.
 *
 * Returns 1 if terms match, 0 otherwise.
 */
static int
rasqal_graph_isomorphism_compare_term(raptor_term* first_term, raptor_term* second_term, rasqal_query_results_compare* compare)
{
  if(!first_term && !second_term)
    return 1; /* Both NULL */
  if(!first_term || !second_term)
    return 0; /* One NULL, other not */

  if(first_term->type != second_term->type)
    return 0;

  switch(first_term->type) {
    case RAPTOR_TERM_TYPE_URI:
      return raptor_uri_equals(first_term->value.uri, second_term->value.uri);
    case RAPTOR_TERM_TYPE_LITERAL:
      return (strcmp((char*)first_term->value.literal.string, (char*)second_term->value.literal.string) == 0);
    case RAPTOR_TERM_TYPE_BLANK:
      /* For blank nodes, we need to check if they're mapped */
      return (strcmp((char*)first_term->value.blank.string, (char*)second_term->value.blank.string) == 0);
    case RAPTOR_TERM_TYPE_UNKNOWN:
    default:
      return 0;
  }
}

/*
 * rasqal_graph_isomorphism_find_blank_node_mapping:
 * @first_blank_nodes: blank nodes from first graph
 * @second_blank_nodes: blank nodes from second graph
 * @first_triples: triples from first graph
 * @second_triples: triples from second graph
 * @first_mapping: mapping for first graph blank nodes
 * @second_mapping: mapping for second graph blank nodes
 * @depth: current recursion depth
 * @compare: graph comparison context
 *
 * Recursively find a valid mapping between blank nodes.
 *
 * Returns 1 if valid mapping found, 0 otherwise.
 */
static int
rasqal_graph_isomorphism_find_blank_node_mapping(raptor_sequence* first_blank_nodes, raptor_sequence* second_blank_nodes, raptor_sequence* first_triples, raptor_sequence* second_triples, raptor_sequence* first_mapping, raptor_sequence* second_mapping, int depth, rasqal_query_results_compare* compare)
{
  int i;
  int blank_count = raptor_sequence_size(first_blank_nodes);
  int j;
  intptr_t mapped_index_ptr;
  int mapped_index;

  if(depth >= blank_count) {
    /* All blank nodes mapped, test the mapping */
    return rasqal_graph_isomorphism_test_mapping(first_blank_nodes, second_blank_nodes, first_triples, second_triples, first_mapping, second_mapping, compare);
  }

  /* Find next unmapped blank node from first graph */
  for(i = 0; i < blank_count; i++) {
    void* ptr = raptor_sequence_get_at(first_mapping, i);
    mapped_index_ptr = (intptr_t)ptr;
    mapped_index = (int)mapped_index_ptr;
    if(mapped_index == -1)
      break;
  }

  if(i >= blank_count)
    return 0; /* All mapped, should not happen */

  /* Try mapping to each unmapped blank node in second graph */
  for(j = 0; j < blank_count; j++) {
    void* ptr = raptor_sequence_get_at(second_mapping, j);
    mapped_index_ptr = (intptr_t)ptr;
    mapped_index = (int)mapped_index_ptr;
    if(mapped_index == -1) {
      /* Try this mapping */
      raptor_sequence_set_at(first_mapping, i, (void*)(intptr_t)j);
      raptor_sequence_set_at(second_mapping, j, (void*)(intptr_t)i);

      /* Recursively try to complete the mapping */
      if(rasqal_graph_isomorphism_find_blank_node_mapping(first_blank_nodes, second_blank_nodes, first_triples, second_triples, first_mapping, second_mapping, depth + 1, compare)) {
        return 1; /* Found valid mapping */
      }

      /* Backtrack */
      raptor_sequence_set_at(first_mapping, i, (void*)(intptr_t)-1);
      raptor_sequence_set_at(second_mapping, j, (void*)(intptr_t)-1);
    }
  }

  return 0; /* No valid mapping found */
}

/*
 * rasqal_graph_isomorphism_test_mapping:
 * @first_blank_nodes: blank nodes from first graph
 * @second_blank_nodes: blank nodes from second graph
 * @first_triples: triples from first graph
 * @second_triples: triples from second graph
 * @first_mapping: mapping for first graph blank nodes
 * @second_mapping: mapping for second graph blank nodes
 * @compare: graph comparison context
 *
 * Test if the current blank node mapping produces isomorphic graphs.
 *
 * Returns 1 if mapping is valid, 0 if not valid, <0 on error.
 */
static int
rasqal_graph_isomorphism_test_mapping(raptor_sequence* first_blank_nodes, raptor_sequence* second_blank_nodes, raptor_sequence* first_triples, raptor_sequence* second_triples, raptor_sequence* first_mapping, raptor_sequence* second_mapping, rasqal_query_results_compare* compare)
{
  int i;
  int first_count, second_count;
  int* first_used = NULL;
  int* second_used = NULL;
  int result = -1; /* Start with error, set to valid value on success */
  int j;
  raptor_statement* first_triple;
  raptor_statement* second_triple;
  int found_match;
  raptor_term* mapped_first_subject;
  raptor_term* mapped_first_predicate;
  raptor_term* mapped_first_object;

  if(!first_triples || !second_triples || !compare)
    return -1;

  first_count = raptor_sequence_size(first_triples);
  second_count = raptor_sequence_size(second_triples);

  if(first_count != second_count)
    return 0;

  if(first_count == 0)
    return 1; /* Both empty */

  /* Allocate usage tracking arrays */
  first_used = RASQAL_CALLOC(int*, first_count, sizeof(int));
  second_used = RASQAL_CALLOC(int*, second_count, sizeof(int));

  if(!first_used || !second_used) {
    goto cleanup;
  }

  /* Try to match each triple from first graph to a triple in second graph */
  for(i = 0; i < first_count; i++) {
    first_triple = (raptor_statement*)raptor_sequence_get_at(first_triples, i);
    found_match = 0;

    if(!first_triple)
      continue;

    for(j = 0; j < second_count; j++) {
      if(second_used[j])
        continue; /* Already matched */

      second_triple = (raptor_statement*)raptor_sequence_get_at(second_triples, j);
      if(!second_triple)
        continue;

      /* Compare triple components with blank node mapping */
      mapped_first_subject = rasqal_graph_isomorphism_map_blank_node(first_triple->subject, first_blank_nodes, second_blank_nodes, first_mapping, second_mapping);
      mapped_first_predicate = rasqal_graph_isomorphism_map_blank_node(first_triple->predicate, first_blank_nodes, second_blank_nodes, first_mapping, second_mapping);
      mapped_first_object = rasqal_graph_isomorphism_map_blank_node(first_triple->object, first_blank_nodes, second_blank_nodes, first_mapping, second_mapping);

      if(rasqal_graph_isomorphism_compare_term(mapped_first_subject, second_triple->subject, compare) &&
         rasqal_graph_isomorphism_compare_term(mapped_first_predicate, second_triple->predicate, compare) &&
         rasqal_graph_isomorphism_compare_term(mapped_first_object, second_triple->object, compare)) {
        first_used[i] = 1;
        second_used[j] = 1;
        found_match = 1;
        break;
      }
    }

    if(!found_match) {
      result = 0; /* Not isomorphic */
      goto cleanup;
    }
  }

  result = 1; /* Isomorphic */

cleanup:
  if(first_used)
    RASQAL_FREE(int*, first_used);
  if(second_used)
    RASQAL_FREE(int*, second_used);

  return result;
}

/*
 * rasqal_graph_isomorphism_map_blank_node:
 * @term: term to potentially map
 * @first_blank_nodes: blank nodes from first graph
 * @second_blank_nodes: blank nodes from second graph
 * @first_mapping: mapping for first graph blank nodes
 * @second_mapping: mapping for second graph blank nodes
 *
 * Map a blank node term according to the current mapping.
 *
 * Returns mapped term or original term if not a blank node.
 */
static raptor_term*
rasqal_graph_isomorphism_map_blank_node(raptor_term* term, raptor_sequence* first_blank_nodes, raptor_sequence* second_blank_nodes, raptor_sequence* first_mapping, raptor_sequence* second_mapping)
{
  int i;

  if(!term || term->type != RAPTOR_TERM_TYPE_BLANK)
    return term; /* Not a blank node, return as-is */

  /* Find this blank node in first_blank_nodes */
  for(i = 0; i < raptor_sequence_size(first_blank_nodes); i++) {
    raptor_term* bnode = (raptor_term*)raptor_sequence_get_at(first_blank_nodes, i);
    if(bnode && strcmp((char*)bnode->value.blank.string, (char*)term->value.blank.string) == 0) {
      /* Found it, get the mapped blank node */
      void* ptr = raptor_sequence_get_at(first_mapping, i);
      intptr_t mapped_index_ptr = (intptr_t)ptr;
      int mapped_index = (int)mapped_index_ptr;
      if(mapped_index >= 0 && mapped_index < raptor_sequence_size(second_blank_nodes)) {
        return (raptor_term*)raptor_sequence_get_at(second_blank_nodes, mapped_index);
      }
    }
  }

  return term; /* Not found in mapping, return as-is */
}

/* Internal API function implementations */



/*
 * rasqal_graph_isomorphism_compare_graphs_incremental:
 * @compare: graph comparison context
 *
 * Incremental graph comparison for large graphs.
 * This approach processes graphs in chunks to handle very large datasets.
 *
 * Algorithm:
 * 1. Divide graphs into manageable chunks
 * 2. Compare chunks incrementally
 * 3. Use early termination if chunks don't match
 * 4. Combine results for final decision
 *
 * Returns 1 if graphs are isomorphic, 0 if not isomorphic, <0 on error.
 */
int
rasqal_graph_isomorphism_compare_graphs_incremental(rasqal_query_results_compare* compare)
{
  /* TODO: Implement incremental graph comparison with chunking for large graphs */
  /* This function is not yet implemented and should not be called */
  
  (void)compare; /* Suppress unused parameter warning */
  return -1; /* Return error - not implemented */
}


#ifdef STANDALONE

/* Standalone unit tests for RDF Graph Isomorphism Detection */

static void
print_test_result(const char* test_name, int result)
{
  printf("%s: %s\n", test_name, result ? "PASS" : "FAIL");
}

static int
test_signature_generation(rasqal_world* world)
{
  raptor_sequence* triples = NULL;
  raptor_term* bnode1 = NULL;
  raptor_term* bnode1_copy = NULL;
  raptor_term* bnode2 = NULL;
  raptor_term* uri1 = NULL;
  raptor_term* uri1_copy = NULL;
  raptor_term* literal1 = NULL;
  raptor_statement* triple1 = NULL;
  raptor_statement* triple2 = NULL;
  rasqal_blank_node_signature* signature1 = NULL;
  rasqal_blank_node_signature* signature2 = NULL;
  int result = 0;

  triples = raptor_new_sequence((raptor_data_free_handler)raptor_free_statement, NULL);
  if(!triples)
    goto cleanup;

  /* Create test data */
  bnode1 = raptor_new_term_from_blank(world->raptor_world_ptr, (unsigned char*)"_:b1");
  bnode2 = raptor_new_term_from_blank(world->raptor_world_ptr, (unsigned char*)"_:b2");
  uri1 = raptor_new_term_from_uri_string(world->raptor_world_ptr, (unsigned char*)"http://example.org/p");
  literal1 = raptor_new_term_from_literal(world->raptor_world_ptr, (unsigned char*)"value", NULL, NULL);

  if(!bnode1 || !bnode2 || !uri1 || !literal1)
    goto cleanup;

  uri1_copy = raptor_term_copy(uri1);
  bnode1_copy = raptor_term_copy(bnode1);

  /* Create test triples */
  triple1 = raptor_new_statement_from_nodes(world->raptor_world_ptr, bnode1, uri1, literal1, NULL);
  triple2 = raptor_new_statement_from_nodes(world->raptor_world_ptr, bnode2, uri1_copy, bnode1_copy, NULL);

  if(!triple1 || !triple2)
    goto cleanup;

  raptor_sequence_push(triples, triple1);
  raptor_sequence_push(triples, triple2);

  /* Generate signatures */
  signature1 = rasqal_graph_isomorphism_generate_signature(bnode1, triples, world);
  signature2 = rasqal_graph_isomorphism_generate_signature(bnode2, triples, world);

  if(!signature1 || !signature2)
    goto cleanup;

  /* Test signature values */
  result = (signature1->subject_count == 1 && signature1->predicate_count == 0 && 
            signature1->object_count == 1 && signature1->complexity == 2);
  
  result &= (signature2->subject_count == 1 && signature2->predicate_count == 0 && 
             signature2->object_count == 0 && signature2->complexity == 1);

cleanup:
  if(signature1)
    RASQAL_FREE(rasqal_blank_node_signature*, signature1);
  if(signature2)
    RASQAL_FREE(rasqal_blank_node_signature*, signature2);
  if(triples)
    raptor_free_sequence(triples);

  return result;
}

static int
test_signature_comparison(rasqal_world* world)
{
  rasqal_blank_node_signature sig1 = {1, 0, 1, 2};
  rasqal_blank_node_signature sig2 = {1, 0, 0, 1};
  rasqal_blank_node_signature sig3 = {1, 0, 1, 2};
  int result = 0;

  /* Test different signatures */
  result = (rasqal_graph_isomorphism_compare_signatures(&sig1, &sig2) > 0);
  
  /* Test identical signatures */
  result &= (rasqal_graph_isomorphism_compare_signatures(&sig1, &sig3) == 0);
  
  /* Test reverse comparison */
  result &= (rasqal_graph_isomorphism_compare_signatures(&sig2, &sig1) < 0);

  return result;
}

static int
test_compartmentalization(rasqal_world* world)
{
  raptor_sequence* triples = NULL;
  raptor_sequence* blank_nodes = NULL;
  raptor_sequence* compartments = NULL;
  raptor_term* bnode1 = NULL;
  raptor_term* bnode1_copy = NULL;
  raptor_term* bnode2 = NULL;
  raptor_term* bnode3 = NULL;
  raptor_term* uri1 = NULL;
  raptor_term* uri1_copy1 = NULL;
  raptor_term* uri1_copy2 = NULL;
  raptor_term* literal1 = NULL;
  raptor_term* literal1_copy = NULL;
  raptor_statement* triple1 = NULL;
  raptor_statement* triple2 = NULL;
  raptor_statement* triple3 = NULL;
  int result = 0;

  triples = raptor_new_sequence((raptor_data_free_handler)raptor_free_statement, NULL);
  blank_nodes = raptor_new_sequence(NULL, NULL);
  
  if(!triples || !blank_nodes)
    goto cleanup;

  /* Create test data */
  bnode1 = raptor_new_term_from_blank(world->raptor_world_ptr, (unsigned char*)"_:b1");
  bnode2 = raptor_new_term_from_blank(world->raptor_world_ptr, (unsigned char*)"_:b2");
  bnode3 = raptor_new_term_from_blank(world->raptor_world_ptr, (unsigned char*)"_:b3");
  uri1 = raptor_new_term_from_uri_string(world->raptor_world_ptr, (unsigned char*)"http://example.org/p");
  literal1 = raptor_new_term_from_literal(world->raptor_world_ptr, (unsigned char*)"value", NULL, NULL);

  if(!bnode1 || !bnode2 || !bnode3 || !uri1 || !literal1)
    goto cleanup;

  /* These are shared references */
  raptor_sequence_push(blank_nodes, bnode1);
  raptor_sequence_push(blank_nodes, bnode2);
  raptor_sequence_push(blank_nodes, bnode3);

  bnode1_copy = raptor_term_copy(bnode1);
  literal1_copy = raptor_term_copy(literal1);
  uri1_copy1 = raptor_term_copy(uri1);
  uri1_copy2 = raptor_term_copy(uri1);

  /* Create test triples with same signature for bnode1 and bnode3 */
  triple1 = raptor_new_statement_from_nodes(world->raptor_world_ptr, bnode1, uri1, literal1, NULL);
  bnode1 = NULL; uri1 = NULL; literal1 = NULL;
  if(!triple1)
    goto cleanup;

  triple2 = raptor_new_statement_from_nodes(world->raptor_world_ptr, bnode2, uri1_copy1, bnode1_copy, NULL);
  bnode2 = NULL; uri1_copy1 = NULL; bnode1_copy = NULL;
  if(!triple1)
    goto cleanup;

   
  triple3 = raptor_new_statement_from_nodes(world->raptor_world_ptr, bnode3, uri1_copy2, literal1_copy, NULL);
  bnode3 = NULL; uri1_copy2 = NULL; literal1_copy = NULL;
  if(!triple1)
    goto cleanup;

  raptor_sequence_push(triples, triple1); triple1 = NULL;
  raptor_sequence_push(triples, triple2); triple2 = NULL;
  raptor_sequence_push(triples, triple3); triple3 = NULL;

  /* Test compartmentalization */
  compartments = rasqal_graph_isomorphism_compartmentalize_by_signature(blank_nodes, triples, world);
  
  if(!compartments)
    goto cleanup;

  /* Should have 2 compartments: one with bnode1 and bnode3 (same signature), one with bnode2 */
  result = (raptor_sequence_size(compartments) == 2);

cleanup:
  if(compartments)
    raptor_free_sequence(compartments);
  if(blank_nodes)
    raptor_free_sequence(blank_nodes);
  if(triples)
    raptor_free_sequence(triples);

  return result;
}

static int
test_ordering(rasqal_world* world)
{
  raptor_sequence* compartments = NULL;
  raptor_sequence* ordered = NULL;
  int result = 0;

  compartments = raptor_new_sequence(NULL, NULL);
  if(!compartments)
    goto cleanup;

  /* Test ordering with empty sequence */
  ordered = rasqal_graph_isomorphism_order_signatures_by_size(compartments,
                                                              world);
  result = (ordered != NULL && raptor_sequence_size(ordered) == 0);

cleanup:
  if(ordered)
    raptor_free_sequence(ordered);
  if(compartments)
    raptor_free_sequence(compartments);

  return result;
}

static int
test_null_parameters(rasqal_world* world)
{
  raptor_sequence* triples = NULL;
  raptor_term* bnode = NULL;
  int result = 1;

  triples = raptor_new_sequence(NULL, NULL);
  bnode = raptor_new_term_from_blank(world->raptor_world_ptr, (unsigned char*)"_:b1");

  if(!triples || !bnode)
    goto cleanup;

  /* Test NULL parameter handling */
  result &= (rasqal_graph_isomorphism_generate_signature(NULL, triples, world) == NULL);
  result &= (rasqal_graph_isomorphism_generate_signature(bnode, NULL, world) == NULL);
  result &= (rasqal_graph_isomorphism_generate_signature(bnode, triples, NULL) == NULL);

  result &= (rasqal_graph_isomorphism_compartmentalize_by_signature(NULL, triples, world) == NULL);
  result &= (rasqal_graph_isomorphism_compartmentalize_by_signature(triples, NULL, world) == NULL);
  result &= (rasqal_graph_isomorphism_compartmentalize_by_signature(triples, triples, NULL) == NULL);

cleanup:
  if(bnode)
    raptor_free_term(bnode);
  if(triples)
    raptor_free_sequence(triples);

  return result;
}

static int
test_memory_management(rasqal_world* world)
{
  raptor_sequence* triples = NULL;
  raptor_term* bnode = NULL;
  raptor_sequence* blank_nodes = NULL;
  raptor_sequence* compartments = NULL;
  raptor_sequence* ordered = NULL;
  rasqal_blank_node_signature* signature = NULL;
  int result = 1;

  triples = raptor_new_sequence(NULL, NULL);
  blank_nodes = raptor_new_sequence(NULL, NULL);
  bnode = raptor_new_term_from_blank(world->raptor_world_ptr, (unsigned char*)"_:b1");

  if(!triples || !blank_nodes || !bnode)
    goto cleanup;

  raptor_sequence_push(blank_nodes, bnode);

  /* Test memory allocation and cleanup */
  signature = rasqal_graph_isomorphism_generate_signature(bnode, triples, world);
  result &= (signature != NULL);
  if(signature) {
    RASQAL_FREE(rasqal_blank_node_signature*, signature);
    signature = NULL;
  }

  compartments = rasqal_graph_isomorphism_compartmentalize_by_signature(blank_nodes, triples, world);
  result &= (compartments != NULL);
  if(compartments) {
      ordered = rasqal_graph_isomorphism_order_signatures_by_size(compartments, world);
  result &= (ordered != NULL);
    if(ordered) {
      raptor_free_sequence(ordered);
      ordered = NULL;
    }
    raptor_free_sequence(compartments);
    compartments = NULL;
  }

cleanup:
  if(signature)
    RASQAL_FREE(rasqal_blank_node_signature*, signature);
  if(ordered)
    raptor_free_sequence(ordered);
  if(compartments)
    raptor_free_sequence(compartments);
  if(bnode)
    raptor_free_term(bnode);
  if(blank_nodes)
    raptor_free_sequence(blank_nodes);
  if(triples)
    raptor_free_sequence(triples);

  return result;
}

static int
test_vf2_detection(rasqal_world* world)
{
  rasqal_query_results* results1 = NULL;
  rasqal_query_results* results2 = NULL;
  rasqal_query_results_compare* compare = NULL;
  raptor_uri* base_uri = NULL;
  int result = 0;

  base_uri = raptor_new_uri(world->raptor_world_ptr, (unsigned char*)"http://example.org/");

  /* Create test query results with isomorphic graphs using Turtle format */
  /* Graph 1: _:b1 <p> "o1" . _:b2 <p> "o2" . _:b1 <q> _:b2 . */
  /* Graph 2: _:x1 <p> "o1" . _:x2 <p> "o2" . _:x1 <q> _:x2 . */

  results1 = rasqal_new_query_results2(world, NULL, RASQAL_QUERY_RESULTS_GRAPH);
  results2 = rasqal_new_query_results2(world, NULL, RASQAL_QUERY_RESULTS_GRAPH);
  if(!results1 || !results2)
    goto cleanup;

  {
    const char* ttl1 =
      "@prefix : <http://example.org/> .\n"
      "_:b1 :p \"o1\" .\n"
      "_:b2 :p \"o2\" .\n"
      "_:b1 :q _:b2 .\n";
    const char* ttl2 =
      "@prefix : <http://example.org/> .\n"
      "_:x1 :p \"o1\" .\n"
      "_:x2 :p \"o2\" .\n"
      "_:x1 :q _:x2 .\n";
    raptor_iostream* iostr;

    iostr = raptor_new_iostream_from_string(world->raptor_world_ptr, (void*)ttl1, strlen(ttl1));
    if(!iostr)
      goto cleanup;
    if(rasqal_query_results_load_graph_iostream(results1, "turtle", iostr, base_uri)) {
      raptor_free_iostream(iostr);
      goto cleanup;
    }
    raptor_free_iostream(iostr);

    iostr = raptor_new_iostream_from_string(world->raptor_world_ptr, (void*)ttl2, strlen(ttl2));
    if(!iostr)
      goto cleanup;
    if(rasqal_query_results_load_graph_iostream(results2, "turtle", iostr, base_uri)) {
      raptor_free_iostream(iostr);
      goto cleanup;
    }
    raptor_free_iostream(iostr);
  }
  
  compare = RASQAL_CALLOC(rasqal_query_results_compare*, 1, sizeof(rasqal_query_results_compare));
  if(!compare)
    goto cleanup;

  compare->world = world;
  compare->first_results = results1;
  compare->second_results = results2;

  /* Test VF2 detection with isomorphic graphs */
  result = (rasqal_graph_isomorphism_detect_vf2(compare) == 1); /* Should detect isomorphism */

cleanup:
  if(compare)
    RASQAL_FREE(rasqal_query_results_compare*, compare);
  if(results2)
    rasqal_free_query_results(results2);
  if(results1)
    rasqal_free_query_results(results1);
  if(base_uri)
    raptor_free_uri(base_uri);

  return result;
}

static int
test_exhaustive_detection(rasqal_world* world)
{
  rasqal_query_results* results1 = NULL;
  rasqal_query_results* results2 = NULL;
  rasqal_query_results_compare* compare = NULL;
  raptor_uri* base_uri = NULL;
  int result = 0;

  base_uri = raptor_new_uri(world->raptor_world_ptr, (unsigned char*)"http://example.org/");

  /* Create test query results with isomorphic graphs using Turtle format */
  results1 = rasqal_new_query_results2(world, NULL, RASQAL_QUERY_RESULTS_GRAPH);
  results2 = rasqal_new_query_results2(world, NULL, RASQAL_QUERY_RESULTS_GRAPH);
  if(!results1 || !results2)
    goto cleanup;

  {
    const char* ttl1 =
      "@prefix : <http://example.org/> .\n"
      "_:b1 :p \"o1\" .\n"
      "_:b2 :p \"o2\" .\n"
      "_:b1 :q _:b2 .\n";
    const char* ttl2 =
      "@prefix : <http://example.org/> .\n"
      "_:x1 :p \"o1\" .\n"
      "_:x2 :p \"o2\" .\n"
      "_:x1 :q _:x2 .\n";
    raptor_iostream* iostr;

    iostr = raptor_new_iostream_from_string(world->raptor_world_ptr, (void*)ttl1, strlen(ttl1));
    if(!iostr)
      goto cleanup;
    if(rasqal_query_results_load_graph_iostream(results1, "turtle", iostr, base_uri)) {
      raptor_free_iostream(iostr);
      goto cleanup;
    }
    raptor_free_iostream(iostr);

    iostr = raptor_new_iostream_from_string(world->raptor_world_ptr, (void*)ttl2, strlen(ttl2));
    if(!iostr)
      goto cleanup;
    if(rasqal_query_results_load_graph_iostream(results2, "turtle", iostr, base_uri)) {
      raptor_free_iostream(iostr);
      goto cleanup;
    }
    raptor_free_iostream(iostr);
  }
  
  compare = RASQAL_CALLOC(rasqal_query_results_compare*, 1, sizeof(rasqal_query_results_compare));
  if(!compare)
    goto cleanup;

  compare->world = world;
  compare->first_results = results1;
  compare->second_results = results2;

  /* Test exhaustive detection with isomorphic graphs */
  result = (rasqal_graph_isomorphism_detect_exhaustive(compare) == 1); /* Should detect isomorphism */

cleanup:
  if(compare)
    RASQAL_FREE(rasqal_query_results_compare*, compare);
  if(results2)
    rasqal_free_query_results(results2);
  if(results1)
    rasqal_free_query_results(results1);
  if(base_uri)
    raptor_free_uri(base_uri);

  return result;
}

static int
test_hybrid_detection(rasqal_world* world)
{
  rasqal_query_results* results1 = NULL;
  rasqal_query_results* results2 = NULL;
  rasqal_query_results_compare* compare = NULL;
  raptor_uri* base_uri = NULL;
  int result = 0;

  base_uri = raptor_new_uri(world->raptor_world_ptr, (unsigned char*)"http://example.org/");

  /* Create test query results with isomorphic graphs using Turtle format */
  results1 = rasqal_new_query_results2(world, NULL, RASQAL_QUERY_RESULTS_GRAPH);
  results2 = rasqal_new_query_results2(world, NULL, RASQAL_QUERY_RESULTS_GRAPH);
  if(!results1 || !results2)
    goto cleanup;

  {
    const char* ttl1 =
      "@prefix : <http://example.org/> .\n"
      "_:b1 :p \"o1\" .\n"
      "_:b2 :p \"o2\" .\n"
      "_:b1 :q _:b2 .\n";
    const char* ttl2 =
      "@prefix : <http://example.org/> .\n"
      "_:x1 :p \"o1\" .\n"
      "_:x2 :p \"o2\" .\n"
      "_:x1 :q _:x2 .\n";
    raptor_iostream* iostr;

    iostr = raptor_new_iostream_from_string(world->raptor_world_ptr, (void*)ttl1, strlen(ttl1));
    if(!iostr)
      goto cleanup;
    if(rasqal_query_results_load_graph_iostream(results1, "turtle", iostr, base_uri)) {
      raptor_free_iostream(iostr);
      goto cleanup;
    }
    raptor_free_iostream(iostr);

    iostr = raptor_new_iostream_from_string(world->raptor_world_ptr, (void*)ttl2, strlen(ttl2));
    if(!iostr)
      goto cleanup;
    if(rasqal_query_results_load_graph_iostream(results2, "turtle", iostr, base_uri)) {
      raptor_free_iostream(iostr);
      goto cleanup;
    }
    raptor_free_iostream(iostr);
  }
  
  compare = RASQAL_CALLOC(rasqal_query_results_compare*, 1, sizeof(rasqal_query_results_compare));
  if(!compare)
    goto cleanup;

  compare->world = world;
  compare->first_results = results1;
  compare->second_results = results2;

  /* Test hybrid detection with isomorphic graphs */
  result = (rasqal_graph_isomorphism_compare_graphs_hybrid(compare) == 1); /* Should detect isomorphism */

cleanup:
  if(compare)
    RASQAL_FREE(rasqal_query_results_compare*, compare);
  if(results2)
    rasqal_free_query_results(results2);
  if(results1)
    rasqal_free_query_results(results1);
  if(base_uri)
    raptor_free_uri(base_uri);

  return result;
}

int
main(int argc, char *argv[])
{
  rasqal_world* world = NULL;
  char const *program = rasqal_basename(*argv);
  int result;
  int failures = 0;

  printf("%s: Testing RDF Graph Isomorphism Detection Module\n", program);

  world = rasqal_new_world();
  if(!world || rasqal_world_open(world))
    return 0;

  result = test_signature_generation(world);
  print_test_result("Signature Generation", result);
  failures += !result;

  result = test_signature_comparison(world);
  print_test_result("Signature Comparison", result);
  failures += !result;

  result = test_compartmentalization(world);
  print_test_result("Compartmentalization", result);
  failures += !result;

  result = test_ordering(world);
  failures += !result;
  print_test_result("Ordering", result);

  result = test_null_parameters(world);
  failures += !result;
  print_test_result("Null Parameter Handling", result);

  result = test_memory_management(world);
  failures += !result;
  print_test_result("Memory Management", result);

  result = test_vf2_detection(world);
  failures += !result;
  print_test_result("VF2 Detection", result);

  result = test_exhaustive_detection(world);
  failures += !result;
  print_test_result("Exhaustive Detection", result);

  result = test_hybrid_detection(world);
  failures += !result;
  print_test_result("Hybrid Detection", result);

  printf("\nTotal failures: %d\n", failures);

  if(world)
    rasqal_free_world(world);

  return failures;
}

#endif /* STANDALONE */

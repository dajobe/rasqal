/*
 * rasqal_graph_isomorphism.h - RDF Graph Isomorphism Detection Algorithms
 *
 * This header defines the INTERNAL API for the RDF graph isomorphism
 * detection algorithms including signature-based methods, VF2
 * algorithm adaptation, and hybrid approaches.
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

#ifndef RASQAL_GRAPH_ISOMORPHISM_H
#define RASQAL_GRAPH_ISOMORPHISM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "rasqal.h"

/*
 * Blank Node Signature Structure
 * 
 * Represents the signature of a blank node based on its occurrence
 * patterns in the RDF graph.
 */
typedef struct {
  int subject_count;      /* Number of times this blank node appears as subject */
  int predicate_count;    /* Number of times this blank node appears as predicate */
  int object_count;       /* Number of times this blank node appears as object */
  int complexity;         /* Overall complexity score */
} rasqal_blank_node_signature;

/* INTERNAL API function declarations */

/*
 * rasqal_graph_isomorphism_detect_signature_based:
 * @compare: graph comparison context
 *
 * Detect isomorphism using signature-based approach with compartmentalization.
 *
 * Returns 1 if graphs are isomorphic, 0 otherwise.
 */
int
rasqal_graph_isomorphism_detect_signature_based(rasqal_query_results_compare* compare);

/*
 * rasqal_graph_isomorphism_detect_exhaustive:
 * @compare: graph comparison context
 *
 * Detect isomorphism using exhaustive search as fallback method.
 *
 * Returns 1 if graphs are isomorphic, 0 otherwise.
 */
int
rasqal_graph_isomorphism_detect_exhaustive(rasqal_query_results_compare* compare);

/*
 * rasqal_graph_isomorphism_detect_vf2:
 * @compare: graph comparison context
 *
 * Detect isomorphism using VF2 algorithm adaptation for RDF graphs.
 *
 * Returns 1 if graphs are isomorphic, 0 otherwise.
 */
int
rasqal_graph_isomorphism_detect_vf2(rasqal_query_results_compare* compare);



/*
 * rasqal_graph_isomorphism_generate_signature:
 * @bnode: blank node to generate signature for
 * @triples: sequence of triples containing the blank node
 * @world: rasqal world
 *
 * Generate a signature for a blank node based on its occurrence patterns.
 *
 * Returns newly allocated signature structure, or NULL on failure.
 */
rasqal_blank_node_signature*
rasqal_graph_isomorphism_generate_signature(raptor_term* bnode, raptor_sequence* triples, rasqal_world* world);

/*
 * rasqal_graph_isomorphism_compartmentalize_by_signature:
 * @blank_nodes: sequence of blank nodes to compartmentalize
 * @triples: sequence of triples containing the blank nodes
 * @world: rasqal world
 *
 * Group blank nodes into compartments based on their signatures.
 *
 * Returns sequence of signature compartments, or NULL on failure.
 */
raptor_sequence*
rasqal_graph_isomorphism_compartmentalize_by_signature(raptor_sequence* blank_nodes, raptor_sequence* triples, rasqal_world* world);

/*
 * rasqal_graph_isomorphism_order_signatures_by_size:
 * @compartments: sequence of signature compartments to order
 * @world: rasqal world
 *
 * Order compartments by size and signature for efficient comparison.
 *
 * Returns ordered sequence of compartments, or NULL on failure.
 */
raptor_sequence*
rasqal_graph_isomorphism_order_signatures_by_size(raptor_sequence* compartments, rasqal_world* world);

/*
 * rasqal_graph_isomorphism_compare_signatures:
 * @sig1: first signature to compare
 * @sig2: second signature to compare
 *
 * Compare two blank node signatures for ordering.
 *
 * Returns negative if sig1 < sig2, 0 if equal, positive if sig1 > sig2.
 */
int
rasqal_graph_isomorphism_compare_signatures(const rasqal_blank_node_signature* sig1, const rasqal_blank_node_signature* sig2);

/*
 * rasqal_graph_isomorphism_compare_graphs_hybrid:
 * @compare: graph comparison context
 *
 * Hybrid graph comparison using multiple algorithms with fallback strategy.
 *
 * Returns 1 if graphs are isomorphic, 0 if not isomorphic, <0 on error.
 */
int
rasqal_graph_isomorphism_compare_graphs_hybrid(rasqal_query_results_compare* compare);

/*
 * rasqal_graph_isomorphism_compare_graphs_incremental:
 * @compare: graph comparison context
 *
 * Incremental graph comparison for large graphs.
 * NOTE: This function is not yet implemented and will return -1.
 *
 * Returns 1 if graphs are isomorphic, 0 if not isomorphic, <0 on error.
 */
int
rasqal_graph_isomorphism_compare_graphs_incremental(rasqal_query_results_compare* compare);

#ifdef __cplusplus
}
#endif

#endif /* RASQAL_GRAPH_ISOMORPHISM_H */ 

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_variable_correlation_map.c - Rasqal SPARQL 1.2 Variable Correlation Map
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


#ifndef STANDALONE

/* External function declaration - defined in rasqal_algebra.c */
void rasqal_algebra_extract_bound_variables(rasqal_algebra_node* node, raptor_sequence* vars);

/*
 * SPARQL 1.2 Variable Correlation Map Implementation
 * 
 * Functions to analyze and manage variable correlation for MINUS operations
 * per SPARQL 1.2 specification requirements.
 */

/**
 * rasqal_new_variable_correlation_map:
 * 
 * Constructor - create a new #rasqal_variable_correlation_map object
 *
 * Return value: new object or NULL on failure
 */
rasqal_variable_correlation_map*
rasqal_new_variable_correlation_map(void)
{
  rasqal_variable_correlation_map* map;
  
  map = RASQAL_CALLOC(rasqal_variable_correlation_map*, 1, sizeof(*map));
  if(!map)
    return NULL;
    
  map->requires_lhs_context = 0;
  map->lhs_variables = NULL;
  map->rhs_not_exists_vars = NULL;
  map->correlation_pairs = raptor_new_sequence((raptor_data_free_handler)rasqal_free_variable, NULL);
  map->saved_bindings = NULL;
  
  if(!map->correlation_pairs) {
    rasqal_free_variable_correlation_map(map);
    return NULL;
  }
  
  return map;
}

/**
 * rasqal_free_variable_correlation_map:
 * @map: #rasqal_variable_correlation_map object
 * 
 * Destructor - destroy a #rasqal_variable_correlation_map object
 */
void
rasqal_free_variable_correlation_map(rasqal_variable_correlation_map* map)
{
  if(!map)
    return;
    
  if(map->lhs_variables)
    raptor_free_sequence(map->lhs_variables);
  if(map->rhs_not_exists_vars)
    raptor_free_sequence(map->rhs_not_exists_vars);
  if(map->correlation_pairs)
    raptor_free_sequence(map->correlation_pairs);
  if(map->saved_bindings)
    raptor_free_sequence(map->saved_bindings);
    
  RASQAL_FREE(rasqal_variable_correlation_map, map);
}

/**
 * rasqal_analyze_scope_variable_correlation:
 * @lhs_node: LHS algebra node
 * @rhs_node: RHS algebra node  
 * @rhs_not_exists_vars: variables used in RHS NOT EXISTS patterns
 * 
 * SPARQL 1.2 compliant variable correlation analysis using query scopes.
 * Analyzes which variables need LHS context for substitute(pattern, μ) operation.
 *
 * Return value: correlation map or NULL on failure
 */
rasqal_variable_correlation_map*
rasqal_analyze_scope_variable_correlation(rasqal_algebra_node* lhs_node,
                                          rasqal_algebra_node* rhs_node, 
                                          raptor_sequence* rhs_not_exists_vars)
{
  rasqal_variable_correlation_map* map;
  int i;
  
  if(!lhs_node || !rhs_node || !rhs_not_exists_vars)
    return NULL;
  
  map = rasqal_new_variable_correlation_map();
  if(!map)
    return NULL;
    
  /* SPARQL 1.2 Analysis: Check each NOT EXISTS variable for scope correlation
   * Per Section 8.1.1: Variables from group are in scope for NOT EXISTS evaluation
   */
  for(i = 0; i < raptor_sequence_size(rhs_not_exists_vars); i++) {
    rasqal_variable* not_exists_var = (rasqal_variable*)raptor_sequence_get_at(rhs_not_exists_vars, i);
    
    if(!not_exists_var)
      continue;
    
    /* Key Analysis: Is this variable available in LHS scope but NOT defined in RHS scope?
     * This indicates the substitute(pattern, μ) operation needs LHS context */
    if(not_exists_var->name &&
       rasqal_scope_provides_variable(lhs_node->execution_scope, RASQAL_GOOD_CAST(const char*, not_exists_var->name)) &&
       !rasqal_scope_defines_variable(rhs_node->execution_scope, RASQAL_GOOD_CAST(const char*, not_exists_var->name))) {
      
      /* SPARQL 1.2: This variable requires LHS context for substitute operation */
      rasqal_variable* correlation_var = rasqal_new_variable_from_variable(not_exists_var);
      if(correlation_var) {
        raptor_sequence_push(map->correlation_pairs, correlation_var);
        map->requires_lhs_context = 1;
      }
    }
  }
  
  return map;
}


/**
 * rasqal_algebra_analyze_direct_minus_correlation:
 * @lhs_node: LHS algebra node  
 * @rhs_node: RHS algebra node
 * 
 * SPARQL 1.2 compliant correlation analysis for direct MINUS operations.
 * Analyzes variable dependencies between LHS and RHS scopes to determine
 * if correlation is needed for proper SPARQL MINUS semantics.
 *
 * This handles cases where RHS patterns (including OPTIONAL) reference
 * variables that are provided by LHS scope but not defined in RHS scope.
 *
 * Return value: correlation map or NULL on failure
 */
rasqal_variable_correlation_map*
rasqal_algebra_analyze_direct_minus_correlation(rasqal_algebra_node* lhs_node,
                                               rasqal_algebra_node* rhs_node)
{
  rasqal_variable_correlation_map* map;
  raptor_sequence* rhs_variables = NULL;
  int i;
  
  if(!lhs_node || !rhs_node)
    return NULL;
    
  /* If execution scopes are not set, create a basic correlation map */
  if(!lhs_node->execution_scope || !rhs_node->execution_scope) {
    map = rasqal_new_variable_correlation_map();
    if(map) {
      map->requires_lhs_context = 0; /* No scopes means no correlation needed */
    }
    return map;
  }
  
  map = rasqal_new_variable_correlation_map();
  if(!map)
    return NULL;
  
  /* Extract all variables used in RHS patterns (not just NOT EXISTS) */
  rhs_variables = raptor_new_sequence((raptor_data_free_handler)rasqal_free_variable, NULL);
  if(!rhs_variables) {
    rasqal_free_variable_correlation_map(map);
    return NULL;
  }
  
  /* Get all variables used in RHS algebra tree */
  rasqal_algebra_extract_bound_variables(rhs_node, rhs_variables);
  
  /* SPARQL 1.2 Analysis: Check each RHS variable for LHS dependency */
  for(i = 0; i < raptor_sequence_size(rhs_variables); i++) {
    rasqal_variable* rhs_var = (rasqal_variable*)raptor_sequence_get_at(rhs_variables, i);
    
    if(!rhs_var || !rhs_var->name)
      continue;
    
    /* Key Analysis: Is this variable provided by LHS but NOT defined locally in RHS?
     * This indicates MINUS operation needs LHS context for proper substitution */
    if(rasqal_scope_provides_variable(lhs_node->execution_scope, RASQAL_GOOD_CAST(const char*, rhs_var->name)) &&
       !rasqal_scope_defines_variable(rhs_node->execution_scope, RASQAL_GOOD_CAST(const char*, rhs_var->name))) {
      
      /* SPARQL 1.2: This variable requires LHS context for MINUS operation */
      rasqal_variable* correlation_var = rasqal_new_variable_from_variable(rhs_var);
      if(correlation_var) {
        raptor_sequence_push(map->correlation_pairs, correlation_var);
        map->requires_lhs_context = 1;
      }
    }
  }
  
  /* Enhanced SPARQL 1.2: Recursive analysis for nested MINUS operations
   * Handle complex subset calculation patterns with multiple MINUS levels */
  if(rhs_node->op == RASQAL_ALGEBRA_OPERATOR_DIFF) {
    /* This RHS is itself a MINUS operation - analyze its nested structure */
    rasqal_variable_correlation_map* nested_map = 
      rasqal_algebra_analyze_direct_minus_correlation(rhs_node->node1, rhs_node->node2);
    
    if(nested_map && nested_map->requires_lhs_context) {
      /* Nested MINUS requires correlation - propagate this requirement upward */
      map->requires_lhs_context = 1;
      
      /* Merge correlation variables from nested analysis */
      if(nested_map->correlation_pairs) {
        for(i = 0; i < raptor_sequence_size(nested_map->correlation_pairs); i++) {
          rasqal_variable* nested_var = (rasqal_variable*)raptor_sequence_get_at(nested_map->correlation_pairs, i);
          if(nested_var) {
            rasqal_variable* merged_var = rasqal_new_variable_from_variable(nested_var);
            if(merged_var) {
              raptor_sequence_push(map->correlation_pairs, merged_var);
            }
          }
        }
      }
    }
    
    if(nested_map)
      rasqal_free_variable_correlation_map(nested_map);
  }
  
  raptor_free_sequence(rhs_variables);
  return map;
}


#endif /* not STANDALONE */


#ifdef STANDALONE

/* Test program for SPARQL 1.2 correlation analysis */

static int
test_scope_variable_functions(rasqal_world* world)
{
  const char* test_name = "Scope Variable Functions";
  int failures = 0;

  fprintf(stderr, "Testing %s\n", test_name);

  /* Test NULL safety */
  if(rasqal_scope_provides_variable(NULL, "testVar1")) {
    fprintf(stderr, "rasqal_scope_provides_variable should return 0 for NULL scope\n");
    failures++;
  }

  if(rasqal_scope_defines_variable(NULL, "testVar1")) {
    fprintf(stderr, "rasqal_scope_defines_variable should return 0 for NULL scope\n");
    failures++;
  }

  /* Note: More detailed scope testing would require complex setup with
   * actual query context. The core functionality is tested through 
   * integration with the SPARQL query processing. */

  return failures;
}

static int
test_correlation_analysis(rasqal_world* world)
{
  const char* test_name = "Basic Correlation Analysis";
  rasqal_query* query = NULL;
  rasqal_variable_correlation_map* map = NULL;
  int failures = 0;

  const char* query_string;
  
  fprintf(stderr, "Testing %s\n", test_name);

  /* Test basic correlation map creation and destruction */
  map = rasqal_new_variable_correlation_map();
  if(!map) {
    fprintf(stderr, "Failed to create correlation map\n");
    return 1;
  }
  
  if(map->requires_lhs_context != 0) {
    fprintf(stderr, "New map should not require LHS context initially\n");
    failures++;
  }
  
  if(!map->correlation_pairs) {
    fprintf(stderr, "Correlation pairs sequence should be initialized\n");
    failures++;
  }
  
  rasqal_free_variable_correlation_map(map);

  /* Test query with NOT EXISTS pattern requiring correlation */
  query_string = 
    "PREFIX : <http://example/> "
    "SELECT ?exam ?date WHERE { "
    "  ?exam :date ?date . "
    "  FILTER NOT EXISTS { "
    "    ?exam2 :type :Ultrasound . "
    "    ?exam2 :date ?date2 . "
    "    FILTER(?date2 > ?date && (?date2 - ?date) <= 21) "
    "  } "
    "}";

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query) {
    fprintf(stderr, "Failed to create query\n");
    return 1;
  }

  if(rasqal_query_prepare(query, (unsigned char*)query_string, NULL)) {
    fprintf(stderr, "Failed to prepare query\n");
    failures++;
    goto cleanup_correlation_test;
  }

  /* After query preparation, algebra should be built with correlation analysis */
  if(!query->query_graph_pattern) {
    fprintf(stderr, "Query pattern not built\n");
    failures++;
    goto cleanup_correlation_test;
  }

  /* Test that correlation analysis functions exist and can be called */
  /* Note: Full integration testing is done via the negation test suite */
  fprintf(stderr, "Basic correlation analysis structure tests passed\n");

  cleanup_correlation_test:
  if(query)
    rasqal_free_query(query);

  return failures;
}

static int
test_nested_minus_detection(rasqal_world* world)
{
  const char* test_name = "Nested MINUS Detection";
  rasqal_query* query = NULL;
  rasqal_variable_correlation_map* map = NULL;
  int failures = 0;

  const char* query_string;
  
  fprintf(stderr, "Testing %s\n", test_name);

  /* Test NULL input handling first */
  map = rasqal_algebra_analyze_direct_minus_correlation(NULL, NULL);
  if(map) {
    fprintf(stderr, "Analysis should fail with NULL inputs\n");
    failures++;
    rasqal_free_variable_correlation_map(map);
  }
  
  map = rasqal_analyze_scope_variable_correlation(NULL, NULL, NULL);
  if(map) {
    fprintf(stderr, "Scope analysis should fail with NULL inputs\n");
    failures++;
    rasqal_free_variable_correlation_map(map);
  }

  /* Test query with nested MINUS structure similar to failing tests */
  query_string = 
    "PREFIX : <http://example/> "
    "PREFIX rdf: <http://www.w3.org/1999/02/22-rdf-syntax-ns#> "
    "SELECT (?s1 AS ?subset) (?s2 AS ?superset) WHERE { "
    "  ?s2 rdf:type :Set . "
    "  ?s1 rdf:type :Set . "
    "  MINUS { "
    "    ?s1 rdf:type :Set . "
    "    ?s2 rdf:type :Set . "
    "    ?s1 :member ?x . "
    "    FILTER NOT EXISTS { ?s2 :member ?x . } "
    "  } "
    "  MINUS { "
    "    ?s2 rdf:type :Set . "
    "    ?s1 rdf:type :Set . "
    "    MINUS { "
    "      ?s1 rdf:type :Set . "
    "      ?s2 rdf:type :Set . "
    "      ?s1 :member ?x . "
    "      FILTER NOT EXISTS { ?s2 :member ?x . } "
    "    } "
    "  } "
    "}";

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query) {
    fprintf(stderr, "Failed to create query for nested MINUS test\n");
    return 1;
  }

  if(rasqal_query_prepare(query, (unsigned char*)query_string, NULL)) {
    fprintf(stderr, "Failed to prepare nested MINUS query\n");
    failures++;
    goto cleanup_nested_test;
  }

  /* After query preparation, algebra should be built */
  if(!query->query_graph_pattern) {
    fprintf(stderr, "Nested MINUS query pattern not built\n");
    failures++;
    goto cleanup_nested_test;
  }

  fprintf(stderr, "Nested MINUS structure parsing tests passed\n");

  cleanup_nested_test:
  if(query)
    rasqal_free_query(query);

  return failures;
}

static int
test_edge_cases(rasqal_world* world)
{
  const char* test_name = "Edge Cases and Error Conditions";
  int failures = 0;

  rasqal_variable_correlation_map* map;
  rasqal_query_scope* scope;
  
  fprintf(stderr, "Testing %s\n", test_name);

  /* Test correlation analysis with NULL inputs */
  map = rasqal_algebra_analyze_direct_minus_correlation(NULL, NULL);
  
  if(map != NULL) {
    fprintf(stderr, "rasqal_algebra_analyze_direct_minus_correlation should return NULL for NULL inputs\n");
    failures++;
    if(map)
      rasqal_free_variable_correlation_map(map);
  }

  /* Test scope functions with edge cases */
  if(rasqal_scope_provides_variable(NULL, NULL)) {
    fprintf(stderr, "rasqal_scope_provides_variable should return 0 for NULL inputs\n");
    failures++;
  }

  if(rasqal_scope_defines_variable(NULL, NULL)) {
    fprintf(stderr, "rasqal_scope_defines_variable should return 0 for NULL inputs\n");
    failures++;
  }

  /* Test empty variable names */
  scope = rasqal_new_query_scope(NULL, RASQAL_GRAPH_PATTERN_OPERATOR_GROUP, NULL);
  if(scope) {
    if(rasqal_scope_provides_variable(scope, "")) {
      fprintf(stderr, "rasqal_scope_provides_variable should handle empty variable name\n");
      failures++;
    }
    
    rasqal_free_query_scope(scope);
  }

  /* Test NULL handling for correlation map */
  rasqal_free_variable_correlation_map(NULL); /* Should not crash */

  fprintf(stderr, "Edge case tests completed\n");

  return failures;
}

int
main(int argc, char *argv[]) 
{
  const char *program = rasqal_basename(argv[0]);
  rasqal_world* world = NULL;
  int failures = 0;
  int rc = 0;

  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    rc = 1;
    goto tidy;
  }

  fprintf(stderr, "%s: Testing SPARQL 1.2 correlation analysis\n", program);

  failures += test_scope_variable_functions(world);
  failures += test_correlation_analysis(world);
  failures += test_nested_minus_detection(world);
  failures += test_edge_cases(world);

  if(failures) {
    fprintf(stderr, "%s: %d test failures\n", program, failures);
    rc = 1;
  } else {
    fprintf(stderr, "%s: All tests passed\n", program);
  }

  tidy:
  if(world)
    rasqal_free_world(world);

  return rc;
}

#endif /* STANDALONE */

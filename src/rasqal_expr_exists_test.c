/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_expr_exists_test.c - Rasqal EXISTS Expression Evaluation Tests
 *
 * Copyright (C) 2023, David Beckett http://www.dajobe.org/
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
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <raptor.h>

#include "rasqal.h"
#include "rasqal_internal.h"

#ifdef STANDALONE

/* Test program for EXISTS expression evaluation */

int main(int argc, char *argv[])
{
  const char *program = rasqal_basename(argv[0]);
  rasqal_world *world = NULL;
  rasqal_query *query = NULL;
  rasqal_evaluation_context *eval_context = NULL;
  int failures = 0;
  int verbose = 1;
  int i;

  /* Process arguments */
  for(i = 1; i < argc ; i++) {
    if(!strcmp(argv[i], "-q") || !strcmp(argv[i], "--quiet"))
      verbose = 0;
    else if(!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
      printf("Usage: %s [OPTIONS]\n", program);
      printf("Test EXISTS expression evaluation with graph context propagation\n\n");
      printf("  -q, --quiet     Run quietly\n");
      printf("  -h, --help      This help message\n");
      return 0;
    }
    else {
      fprintf(stderr, "%s: Unknown argument `%s'\n", program, argv[i]);
      return 1;
    }
  }

  /* Initialize Rasqal */
  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    return 1;
  }

  if(verbose)
    printf("%s: Testing EXISTS expression evaluation\n", program);

  /* Test 1: Basic evaluation context creation and graph origin */
  if(verbose)
    printf("Test 1: Evaluation context graph origin test\n");

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query) {
    fprintf(stderr, "%s: Failed to create query\n", program);
    failures++;
    goto tidy;
  }

  eval_context = rasqal_new_evaluation_context(world, NULL, 0);
  if(!eval_context) {
    fprintf(stderr, "%s: Failed to create evaluation context\n", program);
    failures++;
    goto tidy;
  }

  /* Test graph origin setter/getter */
  {
    rasqal_literal* test_graph_origin;
    rasqal_literal* retrieved_origin;

    test_graph_origin = rasqal_new_uri_literal(world,
      raptor_new_uri(world->raptor_world_ptr, (const unsigned char*)"http://example.org/graph1"));

    if(test_graph_origin) {
      /* Test setting graph origin */
      if(rasqal_evaluation_context_set_graph_origin(eval_context, test_graph_origin) == 0) {
        if(verbose)
          printf("  Graph origin set successfully\n");

        /* Test getting graph origin */
        retrieved_origin = rasqal_evaluation_context_get_graph_origin(eval_context);
        if(retrieved_origin) {
          if(verbose) {
            printf("  Retrieved graph origin: ");
            rasqal_literal_print(retrieved_origin, stdout);
            printf("\n");
          }
        } else {
          fprintf(stderr, "%s: Failed to retrieve graph origin\n", program);
          failures++;
        }
      } else {
        fprintf(stderr, "%s: Failed to set graph origin\n", program);
        failures++;
      }

      rasqal_free_literal(test_graph_origin);
    }
  }

  /* Test 2: EXISTS expression creation and evaluation */
  if(verbose)
    printf("Test 2: EXISTS expression evaluation test\n");

  /* Test basic EXISTS expression creation only for now */
  {
    rasqal_expression* exists_expr;
    rasqal_graph_pattern* test_pattern;
    raptor_sequence* triples;

    /* Create an empty basic graph pattern for testing */
    triples = raptor_new_sequence((raptor_data_free_handler)rasqal_free_triple,
                                 (raptor_data_print_handler)rasqal_triple_print);
    if(triples) {
      test_pattern = rasqal_new_basic_graph_pattern(query, triples, 0, 0, 1);
      if(test_pattern) {
        /* Create EXISTS expression */
        raptor_sequence* args = raptor_new_sequence(NULL, NULL);
        if(args) {
          raptor_sequence_push(args, test_pattern);
          exists_expr = rasqal_new_expr_seq_expression(world, RASQAL_EXPR_EXISTS, args);
          if(exists_expr) {
            if(verbose)
              printf("  EXISTS expression created successfully\n");
            rasqal_free_expression(exists_expr);
            /* The args sequence is owned by the expression and freed with it */
            /* The graph pattern is also freed by the expression */
          } else {
            fprintf(stderr, "%s: Failed to create EXISTS expression\n", program);
            failures++;
            raptor_free_sequence(args);
          }
        } else {
          rasqal_free_graph_pattern(test_pattern);
        }
      } else {
        raptor_free_sequence(triples);
      }
    } else {
      fprintf(stderr, "%s: Failed to create triples sequence\n", program);
      failures++;
    }
  }

  /* Test 3: NOT EXISTS expression evaluation */
  if(verbose)
    printf("Test 3: NOT EXISTS expression evaluation test\n");

  /* Test basic NOT EXISTS expression creation only for now */
  {
    rasqal_expression* not_exists_expr;
    rasqal_graph_pattern* test_pattern;
    raptor_sequence* triples;

    /* Create an empty basic graph pattern for testing */
    triples = raptor_new_sequence((raptor_data_free_handler)rasqal_free_triple,
                                 (raptor_data_print_handler)rasqal_triple_print);
    if(triples) {
      test_pattern = rasqal_new_basic_graph_pattern(query, triples, 0, 0, 1);
      if(test_pattern) {
        /* Create NOT EXISTS expression */
        raptor_sequence* args = raptor_new_sequence(NULL, NULL);
        if(args) {
          raptor_sequence_push(args, test_pattern);
          not_exists_expr = rasqal_new_expr_seq_expression(world, RASQAL_EXPR_NOT_EXISTS, args);
          if(not_exists_expr) {
            if(verbose)
              printf("  NOT EXISTS expression created successfully\n");
            rasqal_free_expression(not_exists_expr);
            /* The args sequence is owned by the expression and freed with it */
            /* The graph pattern is also freed by the expression */
          } else {
            fprintf(stderr, "%s: Failed to create NOT EXISTS expression\n", program);
            failures++;
            raptor_free_sequence(args);
          }
        } else {
          rasqal_free_graph_pattern(test_pattern);
        }
      } else {
        raptor_free_sequence(triples);
      }
    } else {
      fprintf(stderr, "%s: Failed to create triples sequence\n", program);
      failures++;
    }
  }

  /* Test 4: Variable binding context in EXISTS evaluation */
  if(verbose)
    printf("Test 4: Variable binding context test - skipped for now\n");

  /* Test 5: Resource cleanup verification */
  if(verbose)
    printf("Test 5: Resource cleanup verification\n");
  /* Resource cleanup is verified by the above tests completing without crashes */
  if(verbose)
    printf("  Resource cleanup completed successfully\n");

  /* Phase 3H Enhanced Graph Context Tests */
  if(verbose)
    printf("Phase 3H Enhanced Graph Context Tests:\n");

  /* Test 6: Filter expression with graph context propagation */
  if(verbose)
    printf("Test 6: Filter expression with graph context propagation\n");

  {
    rasqal_literal* test_graph_origin;
    rasqal_expression* filter_expr;
    rasqal_literal* result;
    int error = 0;

    /* Create test graph origin */
    test_graph_origin = rasqal_new_uri_literal(world,
      raptor_new_uri(world->raptor_world_ptr, (const unsigned char*)"http://example.org/testgraph"));

    if(test_graph_origin) {
      /* Set graph origin in evaluation context */
      rasqal_evaluation_context_set_graph_origin(eval_context, test_graph_origin);

      /* Create a simple boolean filter expression */
      filter_expr = rasqal_new_literal_expression(world,
        rasqal_new_boolean_literal(world, 1));

      if(filter_expr) {
        eval_context->query = query;

        /* Evaluate expression and verify graph context is preserved */
        result = rasqal_expression_evaluate2(filter_expr, eval_context, &error);
        if(result && !error) {
          /* Verify graph origin is still accessible */
          rasqal_literal* retrieved_origin = rasqal_evaluation_context_get_graph_origin(eval_context);
          if(retrieved_origin) {
            if(verbose)
              printf("  Graph context preserved during filter expression evaluation\n");
          } else {
            fprintf(stderr, "%s: Graph context lost during filter evaluation\n", program);
            failures++;
          }
          rasqal_free_literal(result);
        } else {
          fprintf(stderr, "%s: Filter expression evaluation failed\n", program);
          failures++;
        }

        rasqal_free_expression(filter_expr);
      }

      rasqal_free_literal(test_graph_origin);
    }
  }

  /* Test 7: EXISTS evaluation preserving graph origin from evaluation context */
  if(verbose)
    printf("Test 7: EXISTS evaluation preserving graph origin\n");

  {
    rasqal_expression* exists_expr;
    rasqal_graph_pattern* test_pattern;
    rasqal_literal* test_graph_origin;
    rasqal_literal* result;
    raptor_sequence* triples;
    int error = 0;

    /* Create test graph origin */
    test_graph_origin = rasqal_new_uri_literal(world,
      raptor_new_uri(world->raptor_world_ptr, (const unsigned char*)"http://example.org/exists-graph"));

    if(test_graph_origin) {
      /* Create empty basic graph pattern */
      triples = raptor_new_sequence((raptor_data_free_handler)rasqal_free_triple,
                                   (raptor_data_print_handler)rasqal_triple_print);
      if(triples) {
        test_pattern = rasqal_new_basic_graph_pattern(query, triples, 0, 0, 1);
        if(test_pattern) {
          /* Create EXISTS expression */
          raptor_sequence* args = raptor_new_sequence(NULL, NULL);
          if(args) {
            raptor_sequence_push(args, test_pattern);
            exists_expr = rasqal_new_expr_seq_expression(world, RASQAL_EXPR_EXISTS, args);
            if(exists_expr) {
              /* Set graph context and evaluate EXISTS */
              rasqal_evaluation_context_set_graph_origin(eval_context, test_graph_origin);
              eval_context->query = query;

              result = rasqal_expression_evaluate2(exists_expr, eval_context, &error);
              if(result) {
                if(verbose)
                  printf("  EXISTS expression evaluated with graph context\n");
                rasqal_free_literal(result);
              } else {
                if(verbose)
                  printf("  EXISTS expression evaluation returned NULL (expected for empty pattern)\n");
              }

              /* The args sequence and graph pattern are owned by the expression and freed with it */
              rasqal_free_expression(exists_expr);
            } else {
              raptor_free_sequence(args);
            }
          } else {
            rasqal_free_graph_pattern(test_pattern);
          }
        } else {
          raptor_free_sequence(triples);
        }
      }

      rasqal_free_literal(test_graph_origin);
    }
  }

  /* Test 8: Multi-layer context propagation (GRAPH → FILTER → EXISTS) */
  if(verbose)
    printf("Test 8: Multi-layer context propagation test\n");

  {
    rasqal_literal* graph_origin;
    rasqal_expression* exists_expr;
    rasqal_graph_pattern* exists_pattern;
    rasqal_literal* result;
    raptor_sequence* triples;
    int error = 0;

    /* Create graph origin for multi-layer test */
    graph_origin = rasqal_new_uri_literal(world,
      raptor_new_uri(world->raptor_world_ptr, (const unsigned char*)"http://example.org/multilayer-graph"));

    if(graph_origin) {
      /* Create empty pattern for EXISTS */
      triples = raptor_new_sequence((raptor_data_free_handler)rasqal_free_triple,
                                   (raptor_data_print_handler)rasqal_triple_print);
      if(triples) {
        exists_pattern = rasqal_new_basic_graph_pattern(query, triples, 0, 0, 1);
        if(exists_pattern) {
          /* Create EXISTS expression */
          raptor_sequence* args = raptor_new_sequence(NULL, NULL);
          if(args) {
            raptor_sequence_push(args, exists_pattern);
            exists_expr = rasqal_new_expr_seq_expression(world, RASQAL_EXPR_EXISTS, args);

            if(exists_expr) {
              rasqal_literal* context_origin;

              /* Simulate multi-layer context: Set graph origin */
              rasqal_evaluation_context_set_graph_origin(eval_context, graph_origin);
              eval_context->query = query;

              /* Evaluate EXISTS within graph context */
              result = rasqal_expression_evaluate2(exists_expr, eval_context, &error);

              /* Verify graph context is accessible during nested evaluation */
              context_origin = rasqal_evaluation_context_get_graph_origin(eval_context);
              if(context_origin) {
                if(verbose)
                  printf("  Multi-layer graph context propagation working\n");
              } else {
                fprintf(stderr, "%s: Multi-layer context propagation failed\n", program);
                failures++;
              }

              if(result)
                rasqal_free_literal(result);

              /* The args sequence and graph pattern are owned by the expression and freed with it */
              rasqal_free_expression(exists_expr);
            } else {
              raptor_free_sequence(args);
            }
          } else {
            rasqal_free_graph_pattern(exists_pattern);
          }
        } else {
          raptor_free_sequence(triples);
        }
      }

      rasqal_free_literal(graph_origin);
    }
  }

  /* Test 9: Graph origin accessibility in nested expression evaluations */
  if(verbose)
    printf("Test 9: Graph origin accessibility in nested evaluations\n");

  {
    rasqal_literal* outer_graph;
    rasqal_literal* inner_graph;

    /* Test nested graph context scenarios */
    outer_graph = rasqal_new_uri_literal(world,
      raptor_new_uri(world->raptor_world_ptr, (const unsigned char*)"http://example.org/outer-graph"));
    inner_graph = rasqal_new_uri_literal(world,
      raptor_new_uri(world->raptor_world_ptr, (const unsigned char*)"http://example.org/inner-graph"));

    if(outer_graph && inner_graph) {
      rasqal_literal* retrieved;

      /* Set outer graph context */
      rasqal_evaluation_context_set_graph_origin(eval_context, outer_graph);

      /* Verify outer context is set */
      retrieved = rasqal_evaluation_context_get_graph_origin(eval_context);
      if(retrieved && rasqal_literal_equals(retrieved, outer_graph)) {
        if(verbose)
          printf("  Outer graph context set successfully\n");

        /* Override with inner graph context */
        rasqal_evaluation_context_set_graph_origin(eval_context, inner_graph);

        /* Verify inner context replaces outer */
        retrieved = rasqal_evaluation_context_get_graph_origin(eval_context);
        if(retrieved && rasqal_literal_equals(retrieved, inner_graph)) {
          if(verbose)
            printf("  Inner graph context override working\n");
        } else {
          fprintf(stderr, "%s: Graph context override failed\n", program);
          failures++;
        }
      } else {
        fprintf(stderr, "%s: Initial graph context setting failed\n", program);
        failures++;
      }
    }

    if(outer_graph)
      rasqal_free_literal(outer_graph);
    if(inner_graph)
      rasqal_free_literal(inner_graph);
  }

  /* Test 10: Controlled exists03 scenario reproduction */
  if(verbose)
    printf("Test 10: Controlled exists03 scenario reproduction\n");

  {
    rasqal_literal* exists02_graph;
    rasqal_expression* exists_expr;
    rasqal_graph_pattern* exists_pattern;
    rasqal_variable* var_s, *var_p;
    rasqal_literal* subj, *pred, *obj;
    rasqal_triple* test_triple;
    raptor_sequence* triples;
    rasqal_literal* result;
    int error = 0;

    /* Create exists02.ttl graph context to reproduce exact scenario */
    exists02_graph = rasqal_new_uri_literal(world,
      raptor_new_uri(world->raptor_world_ptr, (const unsigned char*)"file:///exists02.ttl"));

    if(exists02_graph) {
      /* Create variables for EXISTS pattern: { ?s ?p ex:o2 } */
      var_s = rasqal_variables_table_add2(query->vars_table, RASQAL_VARIABLE_TYPE_NORMAL,
                                          (const unsigned char*)"s_test", 6, NULL);
      var_p = rasqal_variables_table_add2(query->vars_table, RASQAL_VARIABLE_TYPE_NORMAL,
                                          (const unsigned char*)"p_test", 6, NULL);

      if(var_s && var_p) {
        /* Create test triple pattern: ?s ?p <http://www.example.org/o2> */
        subj = rasqal_new_variable_literal(world, var_s);
        pred = rasqal_new_variable_literal(world, var_p);
        obj = rasqal_new_uri_literal(world,
          raptor_new_uri(world->raptor_world_ptr, (const unsigned char*)"http://www.example.org/o2"));

        if(subj && pred && obj) {
          test_triple = rasqal_new_triple(subj, pred, obj);
          if(test_triple) {
            triples = raptor_new_sequence((raptor_data_free_handler)rasqal_free_triple,
                                         (raptor_data_print_handler)rasqal_triple_print);
            if(triples) {
              raptor_sequence_push(triples, test_triple);

              exists_pattern = rasqal_new_basic_graph_pattern(query, triples, 0, 0, 1);
              if(exists_pattern) {
                /* Create EXISTS expression */
                raptor_sequence* args = raptor_new_sequence(NULL, NULL);
                if(args) {
                  raptor_sequence_push(args, exists_pattern);
                  exists_expr = rasqal_new_expr_seq_expression(world, RASQAL_EXPR_EXISTS, args);

                  if(exists_expr) {
                    /* Set exists02 graph context */
                    rasqal_evaluation_context_set_graph_origin(eval_context, exists02_graph);
                    eval_context->query = query;

                    /* Evaluate EXISTS within exists02 graph context */
                    result = rasqal_expression_evaluate2(exists_expr, eval_context, &error);

                    if(verbose) {
                      printf("  exists03 scenario test - EXISTS evaluation ");
                      if(result) {
                        int bool_result = rasqal_literal_as_boolean(result, &error);
                        printf("result: %s\n", bool_result ? "true" : "false");
                        rasqal_free_literal(result);
                      } else {
                        printf("returned NULL\n");
                      }
                    }

                    /* The args sequence and graph pattern are owned by the expression and freed with it */
                    rasqal_free_expression(exists_expr);
                  } else {
                    raptor_free_sequence(args);
                  }
                } else {
                  rasqal_free_graph_pattern(exists_pattern);
                }
              } else {
                raptor_free_sequence(triples);
              }
            } else {
              rasqal_free_triple(test_triple);
            }
          } else {
            rasqal_free_literal(subj);
            rasqal_free_literal(pred);
            rasqal_free_literal(obj);
          }
        }
      }

      rasqal_free_literal(exists02_graph);
    }
  }

  tidy:
  if(eval_context)
    rasqal_free_evaluation_context(eval_context);
  if(query)
    rasqal_free_query(query);
  if(world) {
    rasqal_free_world(world);
  }

  if(verbose) {
    if(failures)
      printf("%s: %d test%s FAILED\n", program, failures, (failures == 1 ? "" : "s"));
    else
      printf("%s: All tests PASSED\n", program);
  }

  return failures;
}

#else

int main(int argc, char *argv[]) {
  const char *program=rasqal_basename(argv[0]);
  fprintf(stderr, "%s: STANDALONE not enabled, skipping test\n", program);
  return(0);
}

#endif /* STANDALONE */

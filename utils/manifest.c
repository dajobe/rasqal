/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * manifest.c - Decode tests and testsuites from manifests
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
 */

#include <stdio.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <rasqal_config.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#include <rasqal.h>
#include <rasqal_internal.h>

#include "manifest.h"
#include "rasqalcmdline.h"


static const unsigned int indent_step = 2;
static const unsigned int linewrap = 78;
static const unsigned int banner_width = 68 /* linewrap - 10 */;


static const char manifest_test_state_chars[STATE_LAST + 1] = {
  '.', 'F', '*', '!', '-'
};
static const char* manifest_test_state_labels[STATE_LAST + 1] = {
  "pass",
  "FAIL",
  "XFAIL",
  "UXPASS",
  "SKIP"
};


#define DEFAULT_RESULT_FORMAT_NAME "guess"


/* prototypes */
static void manifest_free_test(manifest_test* t);
static void manifest_free_testsuite(manifest_testsuite* ts);
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 0
static void manifest_test_print(FILE*fh, manifest_test* t, int indent);
#endif

static void
manifest_indent(FILE* fh, unsigned int indent)
{
  while(indent--)
    fputc(' ', fh);
}


static void
manifest_indent_multiline(FILE* fh, const char* str, unsigned int indent,
                          int max_lines_count)
{
  int lines_count = 0;
  char c;

  while((c = *str++)) {
    fputc(c, fh);
    if(c == '\n') {
      lines_count++;
      if(max_lines_count >=0 && lines_count > max_lines_count)
        break;
      manifest_indent(fh, indent);
    }
  }
  if(lines_count > max_lines_count) {
    manifest_indent(fh, indent);
    fputs("...\n", fh);
  }
}


static void
manifest_banner(FILE* fh, unsigned int width, char banner)
{
  while(width--)
    fputc(banner, fh);
  fputc('\n', fh);
}


static char
manifest_test_state_char(manifest_test_state state)
{
  if(state > STATE_LAST)
    return '\0';

  return manifest_test_state_chars[(unsigned int)state];
}


static const char*
manifest_test_state_label(manifest_test_state state)
{
  if(state > STATE_LAST)
    return NULL;

  return manifest_test_state_labels[(unsigned int)state];
}


manifest_world*
manifest_new_world(rasqal_world* world)
{
  manifest_world* mw;
  raptor_world* raptor_world_ptr = rasqal_world_get_raptor(world);
  
  mw = (manifest_world*)calloc(sizeof(*mw), 1);
  if(!mw)
    return NULL;
  
  mw->world = world;
  mw->raptor_world_ptr = raptor_world_ptr;

  /* Create Namespace URIs, concept URIs and rasqal literal concepts  */
  mw->rdfs_namespace_uri = raptor_new_uri(raptor_world_ptr, raptor_rdf_schema_namespace_uri);
  mw->mf_namespace_uri = raptor_new_uri(raptor_world_ptr, (const unsigned char*)"http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#");
  mw->t_namespace_uri = raptor_new_uri(raptor_world_ptr, (const unsigned char*)"http://ns.librdf.org/2009/test-manifest#");
  mw->qt_namespace_uri = raptor_new_uri(raptor_world_ptr, (const unsigned char*)"http://www.w3.org/2001/sw/DataAccess/tests/test-query#");
  mw->dawgt_namespace_uri = raptor_new_uri(raptor_world_ptr, (const unsigned char*)"http://www.w3.org/2001/sw/DataAccess/tests/test-dawg#");
  mw->sd_namespace_uri = raptor_new_uri(raptor_world_ptr, (const unsigned char*)"http://www.w3.org/ns/sparql-service-description#");

  mw->mf_Manifest_uri = raptor_new_uri_from_uri_local_name(raptor_world_ptr, mw->mf_namespace_uri, (const unsigned char*)"Manifest");
  mw->mf_entries_uri = raptor_new_uri_from_uri_local_name(raptor_world_ptr, mw->mf_namespace_uri, (const unsigned char*)"entries");
  mw->mf_name_uri = raptor_new_uri_from_uri_local_name(raptor_world_ptr, mw->mf_namespace_uri, (const unsigned char*)"name");
  mw->mf_action_uri = raptor_new_uri_from_uri_local_name(raptor_world_ptr, mw->mf_namespace_uri, (const unsigned char*)"action");
  mw->mf_result_uri = raptor_new_uri_from_uri_local_name(raptor_world_ptr, mw->mf_namespace_uri, (const unsigned char*)"result");
  mw->mf_resultCardinality_uri = raptor_new_uri_from_uri_local_name(raptor_world_ptr, mw->mf_namespace_uri, (const unsigned char*)"resultCardinality");
  mw->rdf_type_uri = raptor_new_uri_for_rdf_concept(raptor_world_ptr, (const unsigned char*)"type");
  mw->rdf_first_uri = raptor_new_uri_for_rdf_concept(raptor_world_ptr, (const unsigned char*)"first");
  mw->rdf_rest_uri = raptor_new_uri_for_rdf_concept(raptor_world_ptr, (const unsigned char*)"rest");
  mw->rdf_nil_uri = raptor_new_uri_for_rdf_concept(raptor_world_ptr, (const unsigned char*)"nil");
  mw->rdfs_comment_uri = raptor_new_uri_from_uri_local_name(raptor_world_ptr, mw->rdfs_namespace_uri, (const unsigned char*)"comment");
  mw->t_path_uri = raptor_new_uri_from_uri_local_name(raptor_world_ptr, mw->t_namespace_uri, (const unsigned char*)"path");
  mw->qt_data_uri = raptor_new_uri_from_uri_local_name(raptor_world_ptr, mw->qt_namespace_uri, (const unsigned char*)"data");
  mw->qt_graphData_uri = raptor_new_uri_from_uri_local_name(raptor_world_ptr, mw->qt_namespace_uri, (const unsigned char*)"graphData");
  mw->qt_query_uri = raptor_new_uri_from_uri_local_name(raptor_world_ptr, mw->qt_namespace_uri, (const unsigned char*)"query");
  mw->dawgt_approval_uri = raptor_new_uri_from_uri_local_name(raptor_world_ptr, mw->dawgt_namespace_uri, (const unsigned char*)"approval");
  mw->sd_entailmentRegime_uri = raptor_new_uri_from_uri_local_name(raptor_world_ptr, mw->sd_namespace_uri, (const unsigned char*)"entailmentRegime");

  mw->mf_Manifest_literal = rasqal_new_uri_literal(world, raptor_uri_copy(mw->mf_Manifest_uri));
  mw->mf_entries_literal = rasqal_new_uri_literal(world, raptor_uri_copy(mw->mf_entries_uri));
  mw->mf_name_literal = rasqal_new_uri_literal(world, raptor_uri_copy(mw->mf_name_uri));
  mw->mf_action_literal = rasqal_new_uri_literal(world, raptor_uri_copy(mw->mf_action_uri));
  mw->mf_result_literal = rasqal_new_uri_literal(world, raptor_uri_copy(mw->mf_result_uri));
  mw->mf_resultCardinality_literal = rasqal_new_uri_literal(world, raptor_uri_copy(mw->mf_resultCardinality_uri));
  mw->rdf_type_literal = rasqal_new_uri_literal(world, raptor_uri_copy(mw->rdf_type_uri));
  mw->rdf_first_literal = rasqal_new_uri_literal(world, raptor_uri_copy(mw->rdf_first_uri));
  mw->rdf_rest_literal = rasqal_new_uri_literal(world, raptor_uri_copy(mw->rdf_rest_uri));
  mw->rdfs_comment_literal = rasqal_new_uri_literal(world, raptor_uri_copy(mw->rdfs_comment_uri));
  mw->t_path_literal = rasqal_new_uri_literal(world, raptor_uri_copy(mw->t_path_uri));
  mw->qt_data_literal = rasqal_new_uri_literal(world, raptor_uri_copy(mw->qt_data_uri));
  mw->qt_graphData_literal = rasqal_new_uri_literal(world, raptor_uri_copy(mw->qt_graphData_uri));
  mw->qt_query_literal = rasqal_new_uri_literal(world, raptor_uri_copy(mw->qt_query_uri));
  mw->dawgt_approval_literal = rasqal_new_uri_literal(world, raptor_uri_copy(mw->dawgt_approval_uri));
  mw->sd_entailmentRegime_literal = rasqal_new_uri_literal(world, raptor_uri_copy(mw->sd_entailmentRegime_uri));

  return mw;
}


void
manifest_free_world(manifest_world* mw)
{
  if(!mw)
    return;
  
  if(mw->rdfs_namespace_uri)
    raptor_free_uri(mw->rdfs_namespace_uri);
  if(mw->mf_namespace_uri)
    raptor_free_uri(mw->mf_namespace_uri);
  if(mw->t_namespace_uri)
    raptor_free_uri(mw->t_namespace_uri);
  if(mw->qt_namespace_uri)
    raptor_free_uri(mw->qt_namespace_uri);
  if(mw->dawgt_namespace_uri)
    raptor_free_uri(mw->dawgt_namespace_uri);
  if(mw->sd_namespace_uri)
    raptor_free_uri(mw->sd_namespace_uri);

  if(mw->mf_Manifest_uri)
    raptor_free_uri(mw->mf_Manifest_uri);
  if(mw->mf_entries_uri)
    raptor_free_uri(mw->mf_entries_uri);
  if(mw->mf_name_uri)
    raptor_free_uri(mw->mf_name_uri);
  if(mw->mf_action_uri)
    raptor_free_uri(mw->mf_action_uri);
  if(mw->mf_result_uri)
    raptor_free_uri(mw->mf_result_uri);
  if(mw->mf_resultCardinality_uri)
    raptor_free_uri(mw->mf_resultCardinality_uri);
  if(mw->rdf_type_uri)
    raptor_free_uri(mw->rdf_type_uri);
  if(mw->rdf_first_uri)
    raptor_free_uri(mw->rdf_first_uri);
  if(mw->rdf_rest_uri)
    raptor_free_uri(mw->rdf_rest_uri);
  if(mw->rdf_nil_uri)
    raptor_free_uri(mw->rdf_nil_uri);
  if(mw->rdfs_comment_uri)
    raptor_free_uri(mw->rdfs_comment_uri);
  if(mw->t_path_uri)
    raptor_free_uri(mw->t_path_uri);
  if(mw->qt_data_uri)
    raptor_free_uri(mw->qt_data_uri);
  if(mw->qt_graphData_uri)
    raptor_free_uri(mw->qt_graphData_uri);
  if(mw->qt_query_uri)
    raptor_free_uri(mw->qt_query_uri);
  if(mw->dawgt_approval_uri)
    raptor_free_uri(mw->dawgt_approval_uri);
  if(mw->sd_entailmentRegime_uri)
    raptor_free_uri(mw->sd_entailmentRegime_uri);

  if(mw->mf_Manifest_literal)
    rasqal_free_literal(mw->mf_Manifest_literal);
  if(mw->mf_entries_literal)
    rasqal_free_literal(mw->mf_entries_literal);
  if(mw->mf_name_literal)
    rasqal_free_literal(mw->mf_name_literal);
  if(mw->mf_action_literal)
    rasqal_free_literal(mw->mf_action_literal);
  if(mw->mf_result_literal)
    rasqal_free_literal(mw->mf_result_literal);
  if(mw->mf_resultCardinality_literal)
    rasqal_free_literal(mw->mf_resultCardinality_literal);
  if(mw->rdf_type_literal)
    rasqal_free_literal(mw->rdf_type_literal);
  if(mw->rdf_first_literal)
    rasqal_free_literal(mw->rdf_first_literal);
  if(mw->rdf_rest_literal)
    rasqal_free_literal(mw->rdf_rest_literal);
  if(mw->rdfs_comment_literal)
    rasqal_free_literal(mw->rdfs_comment_literal);
  if(mw->t_path_literal)
    rasqal_free_literal(mw->t_path_literal);
  if(mw->qt_data_literal)
    rasqal_free_literal(mw->qt_data_literal);
  if(mw->qt_graphData_literal)
    rasqal_free_literal(mw->qt_graphData_literal);
  if(mw->qt_query_literal)
    rasqal_free_literal(mw->qt_query_literal);
  if(mw->dawgt_approval_literal)
    rasqal_free_literal(mw->dawgt_approval_literal);
  if(mw->sd_entailmentRegime_literal)
    rasqal_free_literal(mw->sd_entailmentRegime_literal);

  free(mw);
}



static manifest_test_result*
manifest_new_test_result(manifest_test_state state)
{
  manifest_test_result* result;
  int i;

  result = (manifest_test_result*)calloc(sizeof(*result), 1);
  if(!result)
    return NULL;

  result->state = state;
  /* total_result->details = NULL; */
  for(i = 0; i <= STATE_LAST; i++)
    result->states[i] = raptor_new_sequence((raptor_data_free_handler)manifest_free_test,
                                            NULL);
  return result;
}


void
manifest_free_test_result(manifest_test_result* result)
{
  int i;

  if(!result)
    return;

  if(result->details)
    free(result->details);

  if(result->log)
    free(result->log);

  for(i = 0; i <= STATE_LAST; i++) {
    if(result->states[i])
      raptor_free_sequence(result->states[i]);
  }

  free(result);
}


static int
manifest_testsuite_result_format(FILE* fh,
                                 manifest_test_result* result,
                                 const char* ts_name,
                                 unsigned indent,
                                 int verbose)
{
  raptor_sequence* seq;
  int i;

  seq = result->states[STATE_FAIL];
  if(seq && raptor_sequence_size(seq)) {
    manifest_test* t;

    manifest_indent(fh, indent);
    fputs("Failed tests:\n", fh);
    for(i = 0;
        (t = RASQAL_GOOD_CAST(manifest_test*, raptor_sequence_get_at(seq, i)));
        i++) {
      manifest_indent(fh, indent + indent_step);

      if(verbose) {
        manifest_banner(fh, banner_width, '=');
        manifest_indent(fh, indent + indent_step);
        fprintf(fh, "%s in suite %s\n", t->name, ts_name);
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 0
        manifest_test_print(fh, t, indent + indent_step);
#endif
      } else {
        fputs(t->name, fh);
        fputc('\n', fh);
      }

      if(verbose) {
        if(t->result) {
          if(t->result->details) {
            manifest_indent(fh, indent + indent_step);
            fputs(t->result->details, fh);
            fputc('\n', fh);
          }

          if(t->result->log) {
            manifest_indent_multiline(fh, t->result->log,
                                      indent + indent_step * 2,
                                      15);
          }
        }

        manifest_indent(fh, indent + indent_step);
        manifest_banner(fh, banner_width, '=');
      }
    }
  }

  seq = result->states[STATE_UXPASS];
  if(seq && raptor_sequence_size(seq)) {
    manifest_test* t;

    manifest_indent(fh, indent);
    fputs("Unexpected passed tests:\n", fh);
    for(i = 0;
        (t = RASQAL_GOOD_CAST(manifest_test*, raptor_sequence_get_at(seq, i)));
        i++) {
      manifest_indent(fh, indent + indent_step);

      if(verbose) {
        manifest_banner(fh, banner_width, '=');
        manifest_indent(fh, indent + indent_step);
        fprintf(fh, "%s in suite %s\n", t->name, ts_name);
      } else {
        fputs(t->name, fh);
        fputc('\n', fh);
      }

      if(verbose) {
        if(t->result) {
          if(t->result->details) {
            manifest_indent(fh, indent + indent_step);
            fputs(t->result->details, fh);
            fputc('\n', fh);
          }

        }
      }
    }

  }

  manifest_indent(fh, indent);

  for(i = 0; i <= STATE_LAST; i++) {
    int count = 0;
    if(i == STATE_XFAIL || i == STATE_UXPASS)
      continue;
    
    seq = result->states[i];
    if(seq)
      count = raptor_sequence_size(seq);
    fprintf(fh, "%s: %3d ",
            manifest_test_state_label(RASQAL_GOOD_CAST(manifest_test_state, i)),
            count);
  }
  fputc('\n', fh);

  return 0;
}




static unsigned int
manifest_decode_test_type(raptor_uri* test_type)
{
  unsigned int flags = 0;
  const char* str;

  if(!test_type)
    return flags;

  str = (const char*)raptor_uri_as_string(test_type);

  if(strstr(str, "UpdateEvaluationTest"))
    return FLAG_IS_UPDATE;

  if(strstr(str, "ProtocolTest"))
    return FLAG_IS_PROTOCOL;

  if(strstr(str, "Syntax")) {
    flags |= FLAG_IS_SYNTAX;

    if(strstr(str, "Negative") || strstr(str, "TestBadSyntax")) {
      flags |= FLAG_MUST_FAIL;
    }
  }

  if(strstr(str, "Test11"))
    flags |= FLAG_LANG_SPARQL_11;

  return flags;
}


/**
 * manifest_new_test:
 * @mw: manifest world
 * @ds: dataset to read from
 * @entry_node: test identifier node
 * @dir: directory
 *
 * Create a new test from parameters
 *
 * These are all input parameters and become owned by this object.
 *
 */
static manifest_test*
manifest_new_test(manifest_world* mw,
                  rasqal_dataset* ds, rasqal_literal* entry_node, char* dir)
{
  rasqal_literal* node = NULL;
  const unsigned char* str = NULL;
  raptor_uri *uri;
  size_t size;
  manifest_test* t;
  char* test_name = NULL;
  char* test_desc = NULL;
  rasqal_literal* action_node;
  raptor_uri* test_query_uri = NULL;
  raptor_sequence* test_data_graphs = NULL;
  raptor_uri* test_result_uri = NULL;
  raptor_uri* test_type = NULL;
  unsigned int test_flags;
  rasqal_dataset_term_iterator* iter = NULL;

  /* Get test fields */
  node = rasqal_dataset_get_target(ds,
                                   entry_node,
                                   mw->mf_name_literal);
  if(node) {
    str = rasqal_literal_as_counted_string(node, &size, 0, NULL);
    if(str) {
      test_name = (char*)malloc(size + 1);
      memcpy(test_name, str, size + 1);
      
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
        fprintf(stderr, "  Test name: '%s'\n", test_name);
#endif
    }
  }

  node = rasqal_dataset_get_target(ds,
                                   entry_node,
                                   mw->rdfs_comment_literal);
  if(node) {
    str = rasqal_literal_as_counted_string(node, &size, 0, NULL);
    if(str) {
      test_desc = (char*)malloc(size + 1);
      memcpy(test_desc, str, size + 1);
      
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
        fprintf(stderr, "  Test desc: '%s'\n", test_desc);
#endif
    }
  }
  
  action_node = rasqal_dataset_get_target(ds,
                                          entry_node,
                                          mw->mf_action_literal);
  if(action_node) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      fputs("  Action node is: ", stderr);
      rasqal_literal_print(action_node, stderr);
      fputc('\n', stderr);
#endif
    if(action_node->type == RASQAL_LITERAL_URI) {
      node = action_node;
    } else {
      node = rasqal_dataset_get_target(ds,
                                       action_node,
                                       mw->qt_query_literal);
    }
    if(node && node->type == RASQAL_LITERAL_URI) {
      uri = rasqal_literal_as_uri(node);
      if(uri) {
        test_query_uri = raptor_uri_copy(uri);
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
          fprintf(stderr, "  Test query URI: '%s'\n",
                  raptor_uri_as_string(test_query_uri));
#endif
      }
    }

    test_data_graphs = raptor_new_sequence((raptor_data_free_handler)rasqal_free_data_graph,
                                           (raptor_data_print_handler)rasqal_data_graph_print);

    node = rasqal_dataset_get_target(ds,
                                     action_node,
                                     mw->qt_data_literal);
    if(node && node->type == RASQAL_LITERAL_URI) {
      uri = rasqal_literal_as_uri(node);
      if(uri) {
        rasqal_data_graph* dg;
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
        fprintf(stderr, "  Test data URI: '%s'\n",
                raptor_uri_as_string(uri));
#endif

        dg = rasqal_new_data_graph_from_uri(mw->world,
                                            uri,
                                            NULL /* graph name URI */,
                                            RASQAL_DATA_GRAPH_BACKGROUND,
                                            NULL /* format mime type */,
                                            NULL /* format/parser name */,
                                            NULL /* format URI */);
        raptor_sequence_push(test_data_graphs, dg);
      }
    }
    
    iter = rasqal_dataset_get_targets_iterator(ds,
                                               action_node,
                                               mw->qt_graphData_literal);
    if(iter) {
      while(1) {
        node = rasqal_dataset_term_iterator_get(iter);
        if(!node)
          break;
        if(node->type == RASQAL_LITERAL_URI) {
          /* FIXME: seen qt:graphData [ qt:graph <uri>; rdfs:label "string" ] */
          uri = rasqal_literal_as_uri(node);
          if(uri) {
            rasqal_data_graph* dg;
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
          fprintf(stderr, "  Test graph data URI: '%s'\n",
                  raptor_uri_as_string(uri));
#endif
            dg = rasqal_new_data_graph_from_uri(mw->world,
                                                uri,
                                                uri,
                                                RASQAL_DATA_GRAPH_NAMED,
                                                NULL /* format mime type */,
                                                NULL /* format/parser name */,
                                                NULL /* format URI */);
            raptor_sequence_push(test_data_graphs, dg);
          }
        }

        if(rasqal_dataset_term_iterator_next(iter))
          break;
      }
      rasqal_free_dataset_term_iterator(iter);
    } /* end if graphData iter */

    
  } /* end if action node */

  node = rasqal_dataset_get_target(ds,
                                   entry_node,
                                   mw->mf_result_literal);
  if(node && node->type == RASQAL_LITERAL_URI) {
    uri = rasqal_literal_as_uri(node);
    if(uri) {
      test_result_uri = raptor_uri_copy(uri);
      
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
        fprintf(stderr, "  Test result URI: '%s'\n",
                raptor_uri_as_string(test_result_uri));
#endif
    }
  }

  node = rasqal_dataset_get_target(ds,
                                   entry_node,
                                   mw->rdf_type_literal);
  if(node && node->type == RASQAL_LITERAL_URI) {
    test_type = rasqal_literal_as_uri(node);

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      fprintf(stderr, "  Test type: '%s'\n",
              raptor_uri_as_string(test_type));
#endif
  }

  test_flags = manifest_decode_test_type(test_type);
  if(!(test_flags & (FLAG_IS_QUERY | FLAG_IS_UPDATE | FLAG_IS_PROTOCOL | FLAG_IS_SYNTAX) )) {
    test_flags |= FLAG_IS_QUERY;
  }

  /* Get a few more flags from other nodes */
  node = rasqal_dataset_get_target(ds,
                                   entry_node,
                                   mw->mf_resultCardinality_literal);
  if(node && node->type == RASQAL_LITERAL_URI) {
    uri = rasqal_literal_as_uri(node);
    if(uri) {
      int is_lax;

      str = raptor_uri_as_string(uri);
      is_lax = (strstr((const char*)str, "LaxCardinality") != NULL);
      
      if(is_lax)
        test_flags |= FLAG_RESULT_CARDINALITY_LAX;
    }
  }

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    fprintf(stderr, "  Test result cardinality: %s\n",
            (test_flags & FLAG_RESULT_CARDINALITY_LAX) ? "lax" : "strict");
#endif

  node = rasqal_dataset_get_target(ds,
                                   entry_node,
                                   mw->dawgt_approval_literal);
  if(node && node->type == RASQAL_LITERAL_URI) {
    uri = rasqal_literal_as_uri(node);
    if(uri) {
      int is_approved;
      int is_withdrawn;
      
      str = raptor_uri_as_string(uri);
      is_approved = (strstr((const char*)str, "Approved") != NULL && 
                     strstr((const char*)str, "NotApproved") == NULL);
      is_withdrawn = (strstr((const char*)str, "Withdrawn") != NULL);
      
      if(is_approved)
        test_flags |= FLAG_TEST_APPROVED;
      if(is_withdrawn)
        test_flags |= FLAG_TEST_WITHDRAWN;
    }
  }

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    fprintf(stderr, "  Test approved: %s\n",
            (test_flags & FLAG_TEST_APPROVED) ? "yes" : "no");
    fprintf(stderr, "  Test withdrawn: %s\n",
            (test_flags & FLAG_TEST_WITHDRAWN) ? "yes" : "no");
#endif

  node = rasqal_dataset_get_target(ds,
                                   action_node,
                                   mw->sd_entailmentRegime_literal);
  if(node)
    test_flags |= FLAG_ENTAILMENT;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    fprintf(stderr, "  Test entailment: %s\n",
            (test_flags & FLAG_ENTAILMENT) ? "yes" : "no");
#endif


  t = (manifest_test*)calloc(sizeof(*t), 1);
  if(!t)
    return NULL;

  t->mw = mw;
  t->name = test_name;
  t->desc = test_desc;
  t->expect = (test_flags & FLAG_MUST_FAIL) ? STATE_FAIL : STATE_PASS;
  t->dir = dir;
  t->test_node = rasqal_new_literal_from_literal(entry_node);
  t->query = test_query_uri;
  t->data_graphs = test_data_graphs;
  t->expected_result = test_result_uri;
  t->flags = test_flags;

  t->usage = 1;

  return t;
}


static manifest_test*
manifest_new_test_from_test(manifest_test* t) {
  t->usage++;
  return t;
}


static void
manifest_free_test(manifest_test* t)
{
  if(!t)
    return;

  if(--t->usage)
    return;

  if(t->name)
    free(t->name);
  if(t->desc)
    free(t->desc);
  if(t->dir)
    free(t->dir);
  if(t->test_node)
    rasqal_free_literal(t->test_node);
  if(t->query)
    raptor_free_uri(t->query);
  if(t->data_graphs)
    raptor_free_sequence(t->data_graphs);
  if(t->expected_result)
    raptor_free_uri(t->expected_result);
  if(t->result)
    manifest_free_test_result(t->result);

  free(t);
}


const char*
manifest_test_get_query_language(manifest_test* t)
{
  const char* language = "sparql";
  if(t->flags & FLAG_IS_UPDATE)
    language = "sparql-update";
  if(t->flags & FLAG_LANG_SPARQL_11)
    language = "sparql11";
  return language;
}


/**
 * manifest_new_testsuite:
 * @world: rasqal world
 * @name: testsuite name
 * @dir: directory containing testsuite
 * @uri: manifest URI
 * @base_uri: manifest base URI
 *
 * Create a new testsuite from a manifest
 */
static manifest_testsuite*
manifest_new_testsuite(manifest_world* mw,
                       char *name, char* dir,
                       raptor_uri* uri, raptor_uri* base_uri)
{
  manifest_testsuite *ts;
  rasqal_dataset* ds = NULL;
  rasqal_literal* manifest_node = NULL;
  rasqal_literal* entries_node = NULL;
  rasqal_literal* list_node = NULL;
  rasqal_literal* node = NULL;
  const unsigned char* str = NULL;
  size_t size;
  raptor_sequence* tests = NULL;

  /* Initialize base */
  ts = (manifest_testsuite*)calloc(sizeof(*ts), 1);
  if(!ts)
    return NULL;

  ts->mw = mw;
  ts->state = STATE_PASS;
  ts->name = strdup(name);
  /* ts->desc = NULL; */
  ts->dir = dir ? strdup(dir) : NULL;
  /* ts->path = NULL; */
  /* ts->tests = NULL; */
  /* ts->details = NULL; */

  /* Make an RDF graph (dataset) to query */
  ds = rasqal_new_dataset(mw->world);
  if(!ds) {
    RASQAL_DEBUG1("Failed to create dataset");
    manifest_free_testsuite(ts); ts = NULL;
    goto tidy;
  }

  if(rasqal_dataset_load_graph_uri(ds, /* graph name */ NULL,
                                   uri, base_uri)) {
    RASQAL_DEBUG2("Failed to load graph %s into dataset",
                  raptor_uri_as_string(uri));
    manifest_free_testsuite(ts); ts = NULL;
    goto tidy;
  }


  manifest_node = rasqal_dataset_get_source(ds,
                                            mw->rdf_type_literal,
                                            mw->mf_Manifest_literal);
  if(!manifest_node) {
    fprintf(stderr, "No manifest found in graph\n");
    manifest_free_testsuite(ts); ts = NULL;
    goto tidy;
  }

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 2
    fputs("Manifest node is: ", stderr);
    rasqal_literal_print(manifest_node, stderr);
    fputc('\n', stderr);
#endif

  entries_node = rasqal_dataset_get_target(ds,
                                           manifest_node,
                                           mw->mf_entries_literal);
  if(!entries_node) {
    fprintf(stderr, "No tests found in manifest graph\n");
    manifest_free_testsuite(ts); ts = NULL;
    goto tidy;
  }

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 2
    fputs("Entries node is: ", stderr);
    rasqal_literal_print(entries_node, stderr);
    fputc('\n', stderr);
#endif

  /* Get test suite fields */
  node = rasqal_dataset_get_target(ds,
                                   manifest_node,
                                   mw->rdfs_comment_literal);
  if(node) {
    str = rasqal_literal_as_counted_string(node, &size, 0, NULL);
    if(str) {
      ts->desc = (char*)malloc(size + 1);
      memcpy(ts->desc, str, size + 1);

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      fprintf(stderr, "Testsuite Description is: '%s'\n", ts->desc);
#endif
    }
  }

  node = rasqal_dataset_get_target(ds,
                                   manifest_node,
                                   mw->t_path_literal);
  if(node) {
    str = rasqal_literal_as_counted_string(node, &size, 0, NULL);
    if(str) {
      ts->path = (char*)malloc(size + 1);
      memcpy(ts->path, str, size + 1);

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
        fprintf(stderr, "Testsuite PATH is: '%s'\n", ts->path);
#endif
    }
  }


  tests = raptor_new_sequence((raptor_data_free_handler)manifest_free_test,
                              NULL);
  for(list_node = entries_node; list_node; ) {
    rasqal_literal* entry_node;
    manifest_test* t;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      fputs("List node is: ", stderr);
      rasqal_literal_print(list_node, stderr);
      fputc('\n', stderr);
#endif

    entry_node = rasqal_dataset_get_target(ds,
                                           list_node,
                                           mw->rdf_first_literal);
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      fputs("Test resource is: ", stderr);
      rasqal_literal_print(entry_node, stderr);
      fputc('\n', stderr);
#endif

    t = manifest_new_test(mw, ds, entry_node, dir);

    if(t)
      raptor_sequence_push(tests, t);

    list_node = rasqal_dataset_get_target(ds,
                                          list_node,
                                          mw->rdf_rest_literal);
    if(!list_node)
      break;

    if(list_node->type == RASQAL_LITERAL_URI) {
      uri = rasqal_literal_as_uri(list_node);
      if(uri && raptor_uri_equals(uri, mw->rdf_nil_uri))
        break;
    }
  } /* end for list_node */

  ts->tests = tests; tests = NULL;
  ts->state = STATE_PASS;
  ts->details = NULL;

  tidy:
  if(ds)
    rasqal_free_dataset(ds);

  if(tests)
    raptor_free_sequence(tests);

  return ts;
}


static void
manifest_free_testsuite(manifest_testsuite* ts)
{
  if(!ts)
    return;

  if(ts->name)
    free(ts->name);
  if(ts->desc)
    free(ts->desc);
  if(ts->dir)
    free(ts->dir);
  if(ts->path)
    free(ts->path);
  if(ts->tests)
    raptor_free_sequence(ts->tests);
  if(ts->details)
    free(ts->details);
  free(ts);
}


static void
manifest_test_run_log_handler(void* user_data, raptor_log_message *message)
{
  manifest_test* t = (manifest_test*)user_data;

  /* Only interested in errors and more severe */
  if(message->level < RAPTOR_LOG_LEVEL_ERROR)
    return;

  fprintf(stderr, "%s\n", message->text);
  t->error_count++;
}

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 0
static void
manifest_test_print(FILE* fh, manifest_test* t, int indent)
{
  manifest_indent(fh, indent);
  if(t->desc)
    fprintf(fh, "Test %s : \"%s\"\n", t->name, t->desc);
  else
    fprintf(fh, "Test %s\n", t->name);
  indent += indent_step;

  manifest_indent(fh, indent);
  fprintf(fh, "SPARQL version: %s\n",
          (t->flags & FLAG_LANG_SPARQL_11) ? "1.1" : "1.0");
  manifest_indent(fh, indent);
  fprintf(fh, "Expect: %s\n",
          (t->flags & FLAG_MUST_FAIL) ? "fail" : "pass");
  manifest_indent(fh, indent);
  fputs("Flags: ", fh);
  if(t->flags & FLAG_IS_QUERY)
    fputs("Query ", fh);
    
  if(t->flags & FLAG_IS_UPDATE)
    fputs("Update ", fh);
    
  if(t->flags & FLAG_IS_PROTOCOL)
    fputs("Protocol ", fh);
    
  if(t->flags & FLAG_IS_SYNTAX)
    fputs("Syntax ", fh);
    
  if(t->flags & FLAG_TEST_APPROVED)
    fputs("Approved ", fh);
    
  if(t->flags & FLAG_TEST_WITHDRAWN)
    fputs("Withdrawn ", fh);
    
  if(t->flags & FLAG_RESULT_CARDINALITY_LAX)
    fputs("LaxCardinality", fh);

  if(t->flags & FLAG_ENTAILMENT)
    fputs("Entailment ", fh);
  fprintf(fh, "(0x%04X)\n", t->flags);

  if(t->query) {
    manifest_indent(fh, indent);
    fprintf(fh, "Query URI: '%s'\n", raptor_uri_as_string(t->query));
  }
  if(t->data_graphs && raptor_sequence_size(t->data_graphs) > 0) {
    manifest_indent(fh, indent);
    fputs("Data URIs: ", fh);
    raptor_sequence_print(t->data_graphs, fh);
    fputc('\n', fh);
  }
  if(t->expected_result) {
    manifest_indent(fh, indent);
    fprintf(fh, "Result URI: '%s'\n",
            raptor_uri_as_string(t->expected_result));
  }
}
#endif


/**
 * manifest_test_run:
 * @t: test
 * @path: env PATH
 *
 * Run a test
 *
 * Return value: a test result or NULL on failure
 */
static manifest_test_result*
manifest_test_run(manifest_test* t, const char* path)
{
  rasqal_world* world = t->mw->world;
  raptor_world* raptor_world_ptr = t->mw->raptor_world_ptr;
  manifest_test_result* result;
  manifest_test_state state = STATE_FAIL;
  unsigned char* query_string = NULL;
  const unsigned char* query_uri_string;
  const char* ql_name;
  rasqal_query* rq = NULL;
  raptor_uri* base_uri = NULL;
  rasqal_query_results_type results_type;
  rasqal_query_results* expected_results = NULL;
  char* result_filename = NULL;
  raptor_iostream* result_iostr = NULL;
  rasqal_query_results *actual_results = NULL;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 0
  RASQAL_DEBUG1("Running ");
  manifest_test_print(stderr, t, 0);
#endif

  if(t && t->flags & (FLAG_IS_UPDATE | FLAG_IS_PROTOCOL)) {
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_WARN, NULL,
                            "Ignoring test %s type UPDATE / PROTOCOL - not supported\n",
                            rasqal_literal_as_string(t->test_node));
    return NULL;
  }

  result = manifest_new_test_result(STATE_FAIL);
  if(!result)
    goto tidy;

  if(!t->query) {
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_WARN, NULL,
                            "Ignoring test %s with no query - not supported\n",
                            rasqal_literal_as_string(t->test_node));
    return NULL;
  }

  /* Read query from file into a string */
  query_uri_string = raptor_uri_as_string(t->query);
  if(raptor_uri_uri_string_is_file_uri(query_uri_string)) {
    char* query_filename = raptor_uri_uri_string_to_filename(query_uri_string);
    query_string = rasqal_cmdline_read_file_string(world, query_filename,
                                                   "query file", NULL);
    raptor_free_memory(query_filename);
  } else {
    raptor_www *www;

    www = raptor_new_www(raptor_world_ptr);
    if(www) {
      raptor_www_fetch_to_string(www, t->query, (void**)&query_string, NULL,
                                 rasqal_alloc_memory);
      raptor_free_www(www);
    }
  }

  if(!query_string) {
    manifest_free_test_result(result);
    result = NULL;
    goto tidy;
  }

  ql_name = manifest_test_get_query_language(t);

  RASQAL_DEBUG4("Read %lu bytes '%s' query string from %s\n",
                strlen((const char*)query_string), ql_name,
                query_uri_string);

  /* Parse and prepare query */
  rq = rasqal_new_query(world, ql_name, NULL);
  if(!rq) {
    RASQAL_DEBUG2("Failed to create query in language %s\n", ql_name);
    manifest_free_test_result(result);
    result = NULL;
    goto tidy;
  }

  if(rasqal_query_prepare(rq, (const unsigned char*)query_string, base_uri))
    state = STATE_FAIL;
  else
    state = STATE_PASS;

  /* Query prepared / parsed OK so for a syntax test, we are done */
  if(t->flags & FLAG_IS_SYNTAX)
    goto returnresult;
  else if(state == STATE_FAIL) {
    /* Otherwise for non-syntax test, stop at a failure */
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                            "Parsing %s query '%s' failed", ql_name, query_string);
    goto setreturnresult;
  }

  /* Default to failure so we just need to set passes */
  state = STATE_FAIL;

  /* Add any data graphs */
  if(t->data_graphs) {
    rasqal_data_graph* dg;

    while((dg = (rasqal_data_graph*)raptor_sequence_pop(t->data_graphs))) {
      if(rasqal_query_add_data_graph(rq, dg)) {
        rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                                "Failed to add data graph %s to query",
                                raptor_uri_as_string(dg->uri));
        manifest_free_test_result(result);
        result = NULL;
        goto tidy;
      }
    }
  }


  /* we now know the query details such as result type */

  /* Read expected results */
  results_type = rasqal_query_get_result_type(rq);
  RASQAL_DEBUG2("Expecting result type %s\n",
                rasqal_query_results_type_label(results_type));

  if(t->expected_result) {
    unsigned char* expected_result_uri_string;

    /* Read result file */
    expected_result_uri_string = raptor_uri_as_string(t->expected_result);
    if(raptor_uri_uri_string_is_file_uri(expected_result_uri_string)) {
      result_filename = raptor_uri_uri_string_to_filename(expected_result_uri_string);

      result_iostr = raptor_new_iostream_from_filename(raptor_world_ptr,
                                                       result_filename);
      if(!result_iostr)
        rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                                "Result file '%s' open failed - %s",
                                result_filename, strerror(errno));
    } else
      rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                              "Result '%s' is not a local file",
                              expected_result_uri_string);

    if(!result_iostr) {
      manifest_free_test_result(result);
      result = NULL;
      goto tidy;
    }

    RASQAL_DEBUG3("Reading result type %s from file %s\n",
                  rasqal_query_results_type_label(results_type),
                  result_filename);

    switch(results_type) {
      case RASQAL_QUERY_RESULTS_BINDINGS:
      case RASQAL_QUERY_RESULTS_BOOLEAN:
        /* read results via rasqal query results format */
        expected_results = rasqal_cmdline_read_results(world,
                                                       raptor_world_ptr,
                                                       results_type,
                                                       result_iostr,
                                                       result_filename,
                                                       /* format name */ NULL);
        raptor_free_iostream(result_iostr); result_iostr = NULL;
        if(!expected_results) {
          RASQAL_DEBUG1("Failed to create query results\n");
          manifest_free_test_result(result);
          result = NULL;
          goto tidy;
        }
        
#if defined(RASQAL_DEBUG)
        if(results_type == RASQAL_QUERY_RESULTS_BINDINGS) {
          RASQAL_DEBUG1("Expected bindings results:\n");
          if(!expected_results)
            fprintf(stderr, "NO RESULTS\n");
          else {
            rasqal_cmdline_print_bindings_results_simple("fake", expected_results,
                                                         stderr, 1, 0);
            rasqal_query_results_rewind(expected_results);
          }
        } else {
          int expected_boolean = rasqal_query_results_get_boolean(expected_results);
          RASQAL_DEBUG2("Expected boolean result: %d\n", expected_boolean);
        }
#endif

        break;

      case RASQAL_QUERY_RESULTS_GRAPH:
        /* read results via raptor parser */

        if(1) {
          const char* format_name = DEFAULT_RESULT_FORMAT_NAME;
          rasqal_dataset* ds;

          ds = rasqal_new_dataset(world);
          if(!ds) {
            RASQAL_DEBUG1("Failed to create dataset\n");
            manifest_free_test_result(result);
            result = NULL;
            goto tidy;
          }

          if(rasqal_dataset_load_graph_iostream(ds, format_name,
                                                result_iostr,
                                                t->expected_result)) {
            RASQAL_DEBUG1("Failed to load graph into dataset\n");
            manifest_free_test_result(result);
            result = NULL;
            goto tidy;
          }

          raptor_free_iostream(result_iostr); result_iostr = NULL;

#ifdef RASQAL_DEBUG
          rasqal_dataset_print(ds, stderr);
#endif

          /* FIXME
           *
           * The code at this point should do something with triples
           * in the dataset; save them for later to compare them to
           * the expected triples.  that requires a triples compare
           * OR a true RDF graph compare.
           *
           * Deleting the dataset here frees the triples just loaded.
           */
          RASQAL_DEBUG1("No support for comparing RDF graph results\n");
          rasqal_free_dataset(ds); ds = NULL;
        }
        break;

      case RASQAL_QUERY_RESULTS_SYNTAX:
      case RASQAL_QUERY_RESULTS_UNKNOWN:
        /* failure */
        rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                                "Reading %s query results format is not supported",
                                rasqal_query_results_type_label(results_type));
        manifest_free_test_result(result);
        result = NULL;
        goto tidy;
    }
  } /* end if results expected */


  /* save results for query execution so we can print and rewind */
  rasqal_query_set_store_results(rq, 1);

  actual_results = rasqal_query_execute(rq);

#if defined(RASQAL_DEBUG)
  /* Debug print what we got */
  if(actual_results) {
    switch(results_type) {
      case RASQAL_QUERY_RESULTS_BINDINGS:
        RASQAL_DEBUG1("Actual bindings results:\n");
        rasqal_cmdline_print_bindings_results_simple("fake", actual_results,
                                                     stderr, 1, 0);
        rasqal_query_results_rewind(actual_results);

        break;

      case RASQAL_QUERY_RESULTS_BOOLEAN:
        if(1) {
          int actual_boolean = rasqal_query_results_get_boolean(actual_results);
          RASQAL_DEBUG2("Actual boolean result: %d\n", actual_boolean);
        }
        break;

      case RASQAL_QUERY_RESULTS_GRAPH:
        RASQAL_DEBUG1("Actual RDF graph result: (cannot be printed yet)\n");
        break;

      case RASQAL_QUERY_RESULTS_SYNTAX:
      case RASQAL_QUERY_RESULTS_UNKNOWN:
        /* failure */
        rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                                "Query result format %s (%d) cannot be printed.",
                                rasqal_query_results_type_label(results_type),
                                results_type);
        state = STATE_FAIL;
        goto tidy;
    }
  }
#endif

  /* Check actual vs expected */ 
  if(actual_results) {
    if(!expected_results)
      state = STATE_PASS;
    else
      switch(results_type) {
        case RASQAL_QUERY_RESULTS_BINDINGS:
          if(1) {
            int rc;
            rasqal_results_compare* rrc;

            /* FIXME: should NOT do this if results are expected to be ordered */
            rasqal_query_results_sort(expected_results);
            rasqal_query_results_sort(actual_results);
            
            rrc = rasqal_new_results_compare(world,
                                             expected_results, "expected",
                                             actual_results, "actual");
            t->error_count = 0;
            rasqal_results_compare_set_log_handler(rrc, t,
                                                   manifest_test_run_log_handler);
            rc = rasqal_results_compare_compare(rrc);
            RASQAL_DEBUG3("rasqal_results_compare_compare returned %d - %s\n", 
                          rc, (rc ? "equal" : "different"));
            rasqal_free_results_compare(rrc); rrc = NULL;

            if(rc && !t->error_count)
              state = STATE_PASS;
          }

          break;

        case RASQAL_QUERY_RESULTS_BOOLEAN:
          if(1) {
            int rc;
            int expected_boolean = rasqal_query_results_get_boolean(expected_results);
            int actual_boolean = rasqal_query_results_get_boolean(actual_results);
            rc = !(expected_boolean == actual_boolean);

            if(!rc)
              state = STATE_PASS;
          }
          break;

        case RASQAL_QUERY_RESULTS_GRAPH:
        case RASQAL_QUERY_RESULTS_SYNTAX:
        case RASQAL_QUERY_RESULTS_UNKNOWN:
          /* failure */
          rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                                  "Query result format %s (%d) cannot be checked.",
                                  rasqal_query_results_type_label(results_type),
                                  results_type);
          state = STATE_FAIL;
          goto tidy;
      }
  } else { /* no actual results */
    if(expected_results)
      /* FAIL: expected results but got none */
      state = STATE_FAIL;
  }  

  returnresult:
  RASQAL_DEBUG3("Test result state %s  expected %s\n", 
                manifest_test_state_label(state),
                manifest_test_state_label(t->expect));

  if(t->expect == STATE_FAIL) {
    if(state == STATE_FAIL) {
      state = STATE_PASS;
      result->details = strdup("Test failed as expected");
    } else {
      state = STATE_FAIL;
      result->details = strdup("Test passed but expected to fail");
    }
  }

  setreturnresult:
  if(result)
    result->state = state;

  RASQAL_DEBUG2("Test result: %s\n", manifest_test_state_label(state));

  tidy:
  if(result_iostr)
    raptor_free_iostream(result_iostr);
  if(actual_results)
    rasqal_free_query_results(actual_results);
  if(expected_results)
    rasqal_free_query_results(expected_results);
  if(result_filename)
    raptor_free_memory(result_filename);
  if(rq)
    rasqal_free_query(rq);
  if(query_string)
    rasqal_free_memory(query_string);

  return result;
}



static int
manifest_test_matches_string(manifest_test* t, const char* test_string)
{
  int found = 0;
  const char* s = RASQAL_GOOD_CAST(const char*, rasqal_literal_as_string(t->test_node));

  found = (t->name && !strcmp(t->name, test_string)) ||
          (s && !strcmp(s, test_string));

  return found;
}


static int
manifest_testsuite_select_tests_by_string(manifest_testsuite* ts,
                                          const char* string)
{
  raptor_sequence* seq;

  seq = raptor_new_sequence((raptor_data_free_handler)manifest_free_test,
                            NULL);
  if(!seq)
    return -1;

  if(string) {
    manifest_test* t;

    while((t = (manifest_test*)raptor_sequence_pop(ts->tests))) {
      if(manifest_test_matches_string(t, string)) {
        raptor_sequence_push(seq, t);
        break;
      } else
        manifest_free_test(t);
    }
  }

  raptor_free_sequence(ts->tests);
  ts->tests = seq;

  return raptor_sequence_size(ts->tests);
}


manifest_test_result*
manifest_testsuite_run_suite(manifest_testsuite* ts,
                             unsigned int indent,
                             int dryrun, int verbose,
                             int approved)
{
  rasqal_world* world = ts->mw->world;
  char* name = ts->name;
  char* desc = ts->desc ? ts->desc : name;
  int i;
  unsigned int expected_failures_count = 0;
  manifest_test* t = NULL;
  unsigned int column;
  manifest_test_result* result;
  manifest_test_state state;
  unsigned int failed_count;
  
  /* Initialize */
  result = manifest_new_test_result(STATE_FAIL);

  /* Run testsuite */
  manifest_indent(stdout, indent);
  fprintf(stdout, "Running testsuite %s: %s\n", name, desc);

  column = indent;
  for(i = 0; (t = (manifest_test*)raptor_sequence_get_at(ts->tests, i)); i++) {
    if(t->flags & (FLAG_IS_UPDATE | FLAG_IS_PROTOCOL)) {
      rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_WARN, NULL,
                              "Ignoring test %s type UPDATE / PROTOCOL - not supported\n",
                              rasqal_literal_as_string(t->test_node));
      t->result = manifest_new_test_result(STATE_SKIP);
    } else if(approved && !(t->flags & (FLAG_TEST_APPROVED))) {
      rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_WARN, NULL,
                              "Ignoring test %s - unapproved\n",
                              rasqal_literal_as_string(t->test_node));
      t->result = manifest_new_test_result(STATE_SKIP);
    } else if(dryrun) {
      t->result = manifest_new_test_result(STATE_SKIP);
    } else {
      t->result = manifest_test_run(t, ts->path);
    }

    if(t->expect == STATE_FAIL)
      expected_failures_count++;

    if(t->result)
      state = t->result->state;
    else {
      RASQAL_DEBUG2("Test %s returned no result - failing\n",
                    rasqal_literal_as_string(t->test_node));
      state = STATE_FAIL;
    }

    if(!verbose)
      fputc(manifest_test_state_char(state), stdout);
    raptor_sequence_push(result->states[(unsigned int)state], manifest_new_test_from_test(t));

    column++;
    if(!verbose && column > linewrap) {
      fputc('\n', stdout);
      manifest_indent(stdout, indent);
      column = indent;
    }

    if(verbose) {
      const char* label = manifest_test_state_label(state);
      unsigned int my_indent = indent + indent_step;
      manifest_indent(stdout, my_indent);
      fputs(t->name, stdout);
      fputs(": ", stdout);
      fputs(label, stdout);
      if(t->result) {
        if(t->result->details) {
          fputs(" - ", stdout);
          fputs(t->result->details, stdout);
        }
      }
      fputc('\n', stdout);
      if(verbose > 1) {
	if(state == STATE_FAIL && t->result && t->result->log) {
          manifest_indent_multiline(stdout, t->result->log, indent,
                                    -1);
	}
      }
    }

  }

  if(!verbose)
    fputc('\n', stderr);

  failed_count = raptor_sequence_size(result->states[STATE_FAIL]);

  result->state = !failed_count ? STATE_PASS : STATE_FAIL;

  return result;
}


/**
 * manifest_manifests_run:
 * @world: world
 * @manifest_uris: sequence of #raptor_uri manifest URIs
 * @base_uri: base URI for manifest
 * @test_string: string for running just one test (by name or URI)
 * @indent: indent size
 * @dryrun: dryrun
 * @verbose: verbose
 * @approved: approved
 *
 * Run the given manifest testsuites returning a test result
 *
 * Return value: test result or NULL on failure
 */
manifest_test_result*
manifest_manifests_run(manifest_world* mw,
                       raptor_sequence* manifest_uris,
                       raptor_uri* base_uri,
                       const char* test_string,
                       unsigned int indent,
                       int dryrun, int verbose, int approved)
{
  manifest_test_state total_state = STATE_PASS;
  manifest_test_result* total_result = NULL;
  raptor_uri* uri;
  int i = 0;

  total_result = manifest_new_test_result(STATE_PASS);
  if(!total_result)
    return NULL;

  for(i = 0; (uri = (raptor_uri*)raptor_sequence_get_at(manifest_uris, i)); i++) {
    int j;
    manifest_testsuite *ts;
    manifest_test_result* result = NULL;
    char* testsuite_name = (char*)raptor_uri_as_string(uri);

    ts = manifest_new_testsuite(mw,
                                /* name */ testsuite_name,
                                /* dir */ NULL,
                                uri, base_uri);

    if(!ts) {
      RASQAL_DEBUG2("Failed to create test suite %s\n", testsuite_name);
      total_state = STATE_FAIL;
      break;
    }

    if(test_string)
      manifest_testsuite_select_tests_by_string(ts, test_string);

    result = manifest_testsuite_run_suite(ts, indent, dryrun, verbose,
                                          approved);

    if(result) {
      manifest_testsuite_result_format(stdout, result, ts->name,
                                       indent + indent_step, verbose);
      for(j = 0; j <= STATE_LAST; j++)
        raptor_sequence_join(total_result->states[j], result->states[j]);

      if(result->state == STATE_FAIL) {
        RASQAL_DEBUG2("Testsuite %s returned fail\n", ts->name);
        total_state = STATE_FAIL;
      }

      manifest_free_test_result(result);
    } else {
      RASQAL_DEBUG2("Testsuite %s failed to return result object\n", ts->name);
      total_state = STATE_FAIL;
    }

    if(i > 1)
      fputc('\n', stdout);

    if(ts)
      manifest_free_testsuite(ts);
  }

  total_result->state = total_state;

  manifest_indent(stdout, indent);
  fputs("Testsuites summary:\n", stdout);

  manifest_testsuite_result_format(stdout, total_result, "total",
                                   indent + indent_step, verbose);

  if(verbose) {
    manifest_indent(stdout, indent);
    fprintf(stdout, "Result status: %d\n", total_state);
  }

  return total_result;
}

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * manifest.c - Run tests from SPARQL query test manifests
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
 * USAGE:
 *   manifest MANIFEST-FILE [BASE-URI]
 *
 * Run the tests in MANIFEST-FILE
 *
 * NOTE: This is not a supported utility.  It is only used for testing
 * invoked by 'improve' and 'check-sparql' and may be replaced.
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

#include <rasqal.h>

#include <rasqal_internal.h>


int main(int argc, char *argv[]);


static const char *program = "manifest";

static int verbose = 1;

static int error_count = 0;
static int warning_count = 0;


static void
manifest_log_handler(void *data, raptor_log_message *message)
{
  raptor_parser *parser = (raptor_parser*)data;

  switch(message->level) {
    case RAPTOR_LOG_LEVEL_FATAL:
    case RAPTOR_LOG_LEVEL_ERROR:
      fprintf(stderr, "%s: Error - ", program);
      raptor_locator_print(message->locator, stderr);
      fprintf(stderr, " - %s\n", message->text);

      raptor_parser_parse_abort(parser);
      error_count++;
      break;

    case RAPTOR_LOG_LEVEL_WARN:
      fprintf(stderr, "%s: Warning - ", program);
      raptor_locator_print(message->locator, stderr);
      fprintf(stderr, " - %s\n", message->text);

      warning_count++;
      break;

    case RAPTOR_LOG_LEVEL_NONE:
    case RAPTOR_LOG_LEVEL_TRACE:
    case RAPTOR_LOG_LEVEL_DEBUG:
    case RAPTOR_LOG_LEVEL_INFO:

      fprintf(stderr, "%s: Unexpected %s message - ", program,
              raptor_log_level_get_label(message->level));
      raptor_locator_print(message->locator, stderr);
      fprintf(stderr, " - %s\n", message->text);
      break;
  }
}

typedef enum
{
  STATE_PASS,
  STATE_FAIL
} manifest_test_state;

typedef struct 
{
  char* dir;
  raptor_uri* test_uri; /* the test uri */
  char* name; /* <test-uri> mf:name ?value */
  char* description; /* <test-uri> rdfs:comment ?value */
  manifest_test_state expect; /* derived from <test-uri> rdf:type ?value */
  char* action; /* <test-uri> mf:action ?value */
} manifest_test;


typedef struct 
{
  rasqal_world* world;
  manifest_test_state state;
  char* name; /* short name */
  char* desc; /* description from ?manifest rdfs:comment ?value */
  char* dir; /* directory */
  char* path; /* for envariable PATH */
  raptor_sequence* tests; /* sequence of manifest_test */
  char* details; /* error details */
} manifest_testsuite;


typedef struct 
{
  manifest_test_state state;
  char* details;
} manifest_test_result;


static void
manifest_free_test_result(manifest_test_result* result)
{
  if(!result)
    return;

  if(result->details)
    free(result->details);
  free(result);
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
manifest_new_testsuite(rasqal_world* world,
                       char *name, char* dir,
                       raptor_uri* uri, raptor_uri* base_uri)
{
  manifest_testsuite *ts;
  rasqal_dataset* ds = NULL;
  int rc = 0;
  raptor_world* raptor_world_ptr = rasqal_world_get_raptor(world);
  rasqal_literal* manifest_node = NULL;
  rasqal_literal* entries_node = NULL;
  rasqal_literal* node = NULL;
  const unsigned char* str = NULL;
  size_t size;

  /* Initialize base */
  ts = (manifest_testsuite*)malloc(sizeof(*ts));
  if(!ts)
    return NULL;

  ts->world = world;
  ts->name = strdup(name);
  ts->dir = dir ? strdup(dir) : NULL;
  ts->state = STATE_PASS;

  /* Create Namespace URIs, concept URIs and rasqal literal concepts  */
  raptor_uri* rdfs_namespace_uri = raptor_new_uri(raptor_world_ptr, raptor_rdf_schema_namespace_uri);
  raptor_uri* mf_namespace_uri = raptor_new_uri(raptor_world_ptr, (const unsigned char*)"http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#");
  raptor_uri* t_namespace_uri = raptor_new_uri(raptor_world_ptr, (const unsigned char*)"http://ns.librdf.org/2009/test-manifest#");

  raptor_uri* mf_Manifest_uri = raptor_new_uri_from_uri_local_name(raptor_world_ptr, mf_namespace_uri, (const unsigned char*)"Manifest");
  raptor_uri* mf_entries_uri = raptor_new_uri_from_uri_local_name(raptor_world_ptr, mf_namespace_uri, (const unsigned char*)"entries");
  raptor_uri* type_uri = raptor_new_uri_for_rdf_concept(raptor_world_ptr, (const unsigned char*)"type");
  raptor_uri* rdfs_comment_uri = raptor_new_uri_from_uri_local_name(raptor_world_ptr, rdfs_namespace_uri, (const unsigned char*)"comment");
  raptor_uri* t_path_uri = raptor_new_uri_from_uri_local_name(raptor_world_ptr, t_namespace_uri, (const unsigned char*)"path");

  rasqal_literal* mf_Manifest_literal = rasqal_new_uri_literal(world, raptor_uri_copy(mf_Manifest_uri));
  rasqal_literal* mf_entries_literal = rasqal_new_uri_literal(world, raptor_uri_copy(mf_entries_uri));
  rasqal_literal* type_literal = rasqal_new_uri_literal(world, raptor_uri_copy(type_uri));
  rasqal_literal* rdfs_comment_literal = rasqal_new_uri_literal(world, raptor_uri_copy(rdfs_comment_uri));
  rasqal_literal* t_path_literal = rasqal_new_uri_literal(world, raptor_uri_copy(t_path_uri));


  /* Make an RDF graph (dataset) to query */
  ds = rasqal_new_dataset(world);
  if(!ds) {
    fprintf(stderr, "%s: Failed to create dataset", program);
    rc = 1;
    goto tidy;
  }
  
  if(rasqal_dataset_load_graph_uri(ds, /* graph name */ NULL,
                                   uri, base_uri)) {
    fprintf(stderr, "%s: Failed to load graph into dataset", program);
    rc = 1;
    goto tidy;
  }


  manifest_node = rasqal_dataset_get_source(ds,
                                            type_literal,
                                            mf_Manifest_literal);
  if(!manifest_node) {
    fprintf(stderr, "No manifest found in graph\n");
    rc = 1;
    goto tidy;
  }

  fputs("Manifest node is: ", stderr);
  rasqal_literal_print(manifest_node, stderr);
  fputc('\n', stderr);


  entries_node = rasqal_dataset_get_target(ds,
                                           manifest_node,
                                           mf_entries_literal);
  if(!entries_node) {
    fprintf(stderr, "No tests found in manifest graph\n");
    rc = 0;
    goto tidy;
  }

  fputs("Entries node is: ", stderr);
  rasqal_literal_print(entries_node, stderr);
  fputc('\n', stderr);
  

  /* Get some text fields */
  node = rasqal_dataset_get_target(ds,
                                   manifest_node,
                                   rdfs_comment_literal);
  if(node) {
    str = rasqal_literal_as_counted_string(node, &size, 0, NULL);
    if(str) {
      ts->desc = (char*)malloc(size + 1);
      memcpy(ts->desc, str, size + 1);
      
      fprintf(stderr, "Description is: '%s'\n", ts->desc);
    }
  }
  
  node = rasqal_dataset_get_target(ds,
                                   manifest_node,
                                   t_path_literal);
  if(node) {
    str = rasqal_literal_as_counted_string(node, &size, 0, NULL);
    if(str) {
      ts->path = (char*)malloc(size + 1);
      memcpy(ts->path, str, size + 1);
      
      fprintf(stderr, "Path is: '%s'\n", ts->path);
    }
  }

/*
  my $list_node=$entries_node;

  my(@tests);
  while($list_node) {
    warn "List node is '$list_node'\n"
      if $debug > 2;

    my $entry_node=$triples{$list_node}->{"<${rdf}first>"}->[0];
    warn "Entry node is '$entry_node'\n"
      if $debug > 2;

    my $name=$triples{$entry_node}->{"<${mf}name>"}->[0] | '';
    $name = decode_literal($name);
    warn "Entry name=$name\n"
      if $debug > 1;

    my $comment=$triples{$entry_node}->{"<${rdfs}comment>"}->[0] || '';
    $comment = decode_literal($comment);
    warn "Entry comment=$comment\n"
      if $debug > 1;

    my $action=$triples{$entry_node}->{"<${mf}action>"}->[0] || '';
    $action = decode_literal($action);
    warn "Entry action $action\n"
       if $debug > 1;

    my $entry_type=$triples{$entry_node}->{"<${rdf}type>"}->[0] || '';
    warn "Entry type is ".($entry_type ? $entry_type : "NONE")."\n"
      if $debug > 1;

    my $expect='pass';
    my $execute=1;

    $expect='fail' if
      $entry_type eq "<${t}NegativeTest>" ||
      $entry_type eq "<${t}XFailTest>";

    my $test_uri=$entry_node; $test_uri =~ s/^<(.+)>$/$1/;
    warn "Test uri $test_uri\n"
       if $debug > 1;

    push(@tests, {name => $name,
		  comment => $comment,
		  dir => $dir,
		  expect => $expect,
		  test_uri => $test_uri,
		  action => $action
	   } );

  next_list_node:
    $list_node=$triples{$list_node}->{"<${rdf}rest>"}->[0];
    last if $list_node eq "<${rdf}nil>";
  }

  $testsuite->{tests}=\@tests;

  return {status => 'pass', details => ''};
}
*/


  tidy:
  if(ds)
    rasqal_free_dataset(ds);

  if(rdfs_namespace_uri)
    raptor_free_uri(rdfs_namespace_uri);
  if(mf_namespace_uri)
    raptor_free_uri(mf_namespace_uri);
  if(t_namespace_uri)
    raptor_free_uri(t_namespace_uri);
  if(mf_Manifest_uri)
    raptor_free_uri(mf_Manifest_uri);
  if(mf_entries_uri)
    raptor_free_uri(mf_entries_uri);
  if(type_uri)
    raptor_free_uri(type_uri);
  if(rdfs_comment_uri)
    raptor_free_uri(rdfs_comment_uri);
  if(t_path_uri)
    raptor_free_uri(t_path_uri);

  if(mf_Manifest_literal)
    rasqal_free_literal(mf_Manifest_literal);
  if(mf_entries_literal)
    rasqal_free_literal(mf_entries_literal);
  if(type_literal)
    rasqal_free_literal(type_literal);
  if(rdfs_comment_literal)
    rasqal_free_literal(rdfs_comment_literal);
  if(t_path_literal)
    rasqal_free_literal(t_path_literal);

  return rc;
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
  /* FIXME: free tests */
  if(ts->details)
    free(ts->details);
  free(ts);
}


/**
 * manifest_test_manifests:
 * @world: world
 * @manifest_uris: array of manifest URIs
 * @base_uri: base URI for manifest
 * @indent: indent size
 *
 * Run the given manifest testsuites returning a test result
 *
 * Return value: test result or NULL on failure
 */
static manifest_test_result*
manifest_test_manifests(rasqal_world* world,
                        raptor_uri** manifest_uris,
                        raptor_uri* base_uri,
                        unsigned int indent)
{
  manifest_test_state total_state = STATE_PASS;
  manifest_test_result* total_result = NULL;
  int rc = 0;
  raptor_uri* uri;
  int i = 0;

  total_result = (manifest_test_result*)malloc(sizeof(*total_result));
  total_result->state = STATE_FAIL;
  total_result->details = NULL;

  for(i = 0; (uri = manifest_uris[i]); i++) {
    manifest_testsuite *ts;
    manifest_test_result* result = NULL;

    ts = manifest_new_testsuite(world, 
                                /* name */ (char*)raptor_uri_as_string(uri),
                                /* dir */ NULL,
                                uri, base_uri);
    
    if(rc) {
      fprintf(stderr, "%s: Suite %s failed preparation - %s\n",
              program, ts->name, ts->details);
      manifest_free_testsuite(ts);
      break;
    }

#if 0
    result = manifest_run_testsuite(ts, indent);
    format_testsuite_result(stdout, result, indent+1);

    for my $counter (@counters) {
      push(@{$total_result->{$counter}}, @{$result->{$counter}});
    }

#endif
    if(result) {
      if(result->state == STATE_FAIL)
        total_state = STATE_FAIL;
    }

    if(i > 1)
      fputc(stdout, '\n');

    if(ts)
      manifest_free_testsuite(ts);
  }
  
  total_result->state = total_state;

#if 0
  printf $indent."Testsuites summary%s:\n", ($verbose ? " for dir $dir" : '');
  format_testsuite_result(*STDOUT, $total_result, $indent.$INDENT, $verbose);

  print_indent(stderr, indent);
#endif
  if(verbose)
    fprintf(stderr, "Result status: %d\n", total_state);

  return total_result;
}


int
main(int argc, char *argv[])
{
  rasqal_world *world = NULL;
  raptor_world* raptor_world_ptr = NULL;
  unsigned char *uri_string;
  raptor_uri *uri;
  raptor_uri *base_uri;
  int rc = 0;
  int free_uri_string = 0;

  if(argc < 2 || argc > 3) {
    fprintf(stderr, "USAGE: %s MANIFEST-FILE [BASE-URI]\n", program);
    rc = 1;
    goto tidy;
  }

  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    rc = 1;
    goto tidy;
  }
  
  raptor_world_ptr = rasqal_world_get_raptor(world);
  rasqal_world_set_log_handler(world, world, manifest_log_handler);

  uri_string = (unsigned char*)argv[1];
  if(!access((const char*)uri_string, R_OK)) {
    uri_string = raptor_uri_filename_to_uri_string((char*)uri_string);
    uri = raptor_new_uri(raptor_world_ptr, uri_string);
    free_uri_string = 1;
  } else {
    uri = raptor_new_uri(raptor_world_ptr, (const unsigned char*)uri_string);
  }

  if(argc == 3) {
    char* base_uri_string = argv[2];
    base_uri = raptor_new_uri(raptor_world_ptr, (unsigned char*)(base_uri_string));
  } else {
    base_uri = raptor_uri_copy(uri);
  }

  raptor_uri* manifest_uris[2] = { uri, NULL };

  manifest_test_result* result;
  result = manifest_test_manifests(world, manifest_uris, base_uri, 0);

  if(result)
    manifest_free_test_result(result);

  raptor_free_uri(base_uri);
  raptor_free_uri(uri);
  if(free_uri_string)
    raptor_free_memory(uri_string);

  rasqal_free_world(world);

  tidy:
  if(warning_count)
    rc = 2;
  else if(error_count)
    rc = 1;

  return rc;
}

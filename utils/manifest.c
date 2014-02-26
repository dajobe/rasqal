/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * manifest.c - Read a query manifest
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
 *   to_ntriples RDF-FILE [BASE-URI]
 *
 * To parse an RDF syntax in RDF-FILE using the 'guess' parser,
 * emitting the result as N-Triples with optional BASE-URI.
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

#include <rasqal.h>

#include <rasqal_internal.h>


int main(int argc, char *argv[]);


static const char *program = "manifest";

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
  STATUS_PASS,
  STATUS_FAIL
} manifest_test_state;

typedef struct 
{
  const char* dir;
  raptor_uri* test_uri; /* the test uri */
  const char* name; /* <test-uri> mf:name ?value */
  const char* description; /* <test-uri> rdfs:comment ?value */
  manifest_test_state expect; /* derived from <test-uri> rdf:type ?value */
  const char* action; /* <test-uri> mf:action ?value */
} manifest_test;


typedef struct 
{
  const char* name; /* short name */
  const char* desc; /* description from ?manifest rdfs:comment ?value */
  const char* dir; /* directory */
  const char* path; /* for envariable PATH */
  raptor_sequence* tests; /* sequence of manifest_test */
} manifest_testsuite;


static int
manifest_read_plan(rasqal_world* world,
                   raptor_uri* uri, raptor_uri* base_uri)
{
  rasqal_dataset* ds = NULL;
  int rc = 0;
  raptor_world* raptor_world_ptr = rasqal_world_get_raptor(world);

  ds = rasqal_new_dataset(world);
  if(!ds) {
    fprintf(stderr, "%s: Failed to create dataset", program);
    rc = 1;
    goto tidy;
  }
  
  if(rasqal_dataset_load_graph_uri(ds, NULL, uri, NULL)) {
    fprintf(stderr, "%s: Failed to load graph into dataset", program);
    rc = 1;
    goto tidy;
  }


  raptor_uri* rdfs_namespace_uri = raptor_new_uri(raptor_world_ptr, raptor_rdf_schema_namespace_uri);
  raptor_uri* mf_namespace_uri = raptor_new_uri(raptor_world_ptr, (const unsigned char*)"http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#");
  raptor_uri* t_namespace_uri = raptor_new_uri(raptor_world_ptr, (const unsigned char*)"http://ns.librdf.org/2009/test-manifest#");

  raptor_uri* mf_Manifest_uri = raptor_new_uri_from_uri_local_name(raptor_world_ptr, mf_namespace_uri, (const unsigned char*)"Manifest");
  raptor_uri* mf_entries_uri = raptor_new_uri_from_uri_local_name(raptor_world_ptr, mf_namespace_uri, (const unsigned char*)"entries");
  rasqal_literal* mf_Manifest_literal = rasqal_new_uri_literal(world, mf_Manifest_uri);
  rasqal_literal* mf_entries_literal = rasqal_new_uri_literal(world, mf_entries_uri);
  raptor_uri* type_uri = raptor_new_uri_for_rdf_concept(raptor_world_ptr, (const unsigned char*)"type");
  rasqal_literal* type_literal = rasqal_new_uri_literal(world, type_uri);
  raptor_uri* rdfs_comment_uri = raptor_new_uri_from_uri_local_name(raptor_world_ptr, rdfs_namespace_uri, (const unsigned char*)"comment");
  rasqal_literal* rdfs_comment_literal = rasqal_new_uri_literal(world, rdfs_comment_uri);
  raptor_uri* t_path_uri = raptor_new_uri_from_uri_local_name(raptor_world_ptr, t_namespace_uri, (const unsigned char*)"path");
  rasqal_literal* t_path_literal = rasqal_new_uri_literal(world, t_path_uri);

  rasqal_literal* manifest_node;
  manifest_node = rasqal_dataset_get_source(ds,
                                            type_literal,
                                            mf_Manifest_literal);

  fputs("Manifest node is: ", stderr);
  rasqal_literal_print(manifest_node, stderr);
  fputc('\n', stderr);

  if(!manifest_node) {
    rc = 1;
    goto tidy;
  }

  rasqal_literal* desc_node;
  desc_node = rasqal_dataset_get_target(ds,
                                        manifest_node,
                                        rdfs_comment_literal);
  fputs("Description is: ", stderr);
  rasqal_literal_print(desc_node, stderr);
  fputc('\n', stderr);

  rasqal_literal* path_node;
  path_node = rasqal_dataset_get_target(ds,
                                        manifest_node,
                                        t_path_literal);
  fputs("Path is: ", stderr);
  rasqal_literal_print(path_node, stderr);
  fputc('\n', stderr);

  rasqal_literal* entries_node;
  entries_node = rasqal_dataset_get_target(ds,
                                           manifest_node,
                                           mf_entries_literal);

  fputs("Entries node is: ", stderr);
  rasqal_literal_print(entries_node, stderr);
  fputc('\n', stderr);
  

/*
our $mf='http://www.w3.org/2001/sw/DataAccess/tests/test-manifest#';
our $rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#';
our $rdfs='http://www.w3.org/2000/01/rdf-schema#';
our $t='http://ns.librdf.org/2009/test-manifest#';

sub read_plan($$) {
  my($testsuite, $plan_file)=@_;

  my $dir = $testsuite->{dir};

  my(%triples);
  my $manifest_node;
  my $entries_node;

  my $to_ntriples_error='to_ntriples.err';
  my $cmd="$TO_NTRIPLES $plan_file 2> $to_ntriples_error";
  print STDERR "$program: Running pipe from $cmd\n"
    if $debug > 1;
  open(MF, "$cmd |") 
    or die "Cannot open pipe from '$cmd' - $!\n";
  while(<MF>) {
    chomp;
    s/\s+\.$//;
    my($s,$p,$o)=split(/ /,$_,3);
    die "no p in '$_'\n" unless defined $p;
    die "no o in '$_'\n" unless defined $o;
    push(@{$triples{$s}->{$p}}, $o);
    $manifest_node=$s if $p eq "<${rdf}type>" && $o eq "<${mf}Manifest>";
    $entries_node=$o if $p eq "<${mf}entries>";
  }
  close(MF);
  if(!-z $to_ntriples_error) {
    my $status = {status => 'fail', details => `cat $to_ntriples_error` };
    unlink $to_ntriples_error;
    return $status;
  }
  unlink $to_ntriples_error;


  warn "Manifest node is '$manifest_node'\n"
    if $debug > 1;
  if($manifest_node) {
    my $desc=$triples{$manifest_node}->{"<${rdfs}comment>"}->[0];
    if($desc) {
      $testsuite->{desc}=decode_literal($desc);
    }
    my $path=$triples{$manifest_node}->{"<${t}path>"}->[0];
    if($path) {
      $testsuite->{path}=decode_literal($path);
    }
  }

  warn "Entries node is '$entries_node'\n"
    if $debug > 1;

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
  rasqal_free_dataset(ds); ds = NULL;

  return rc;
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

  rc = manifest_read_plan(world, uri, base_uri);

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

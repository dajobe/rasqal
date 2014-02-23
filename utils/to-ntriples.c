/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * to-ntriples.c - Test support: parse RDF into N-Triples using raptor
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

#include <raptor2.h>


int main(int argc, char *argv[]);


static const char *program = "to-ntriples";

static raptor_serializer* rdf_serializer;
static int error_count = 0;
static int warning_count = 0;


static void
to_ntriples_write_triple(void* user_data, raptor_statement* triple)
{
  raptor_serializer_serialize_statement(rdf_serializer, triple);
}


static void
to_ntriples_log_handler(void *data, raptor_log_message *message)
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


int
main(int argc, char *argv[])
{
  raptor_world *world = NULL;
  raptor_parser* rdf_parser = NULL;
  unsigned char *uri_string;
  raptor_uri *uri;
  raptor_uri *base_uri;
  int rc = 0;
  int free_uri_string = 0;

  rdf_serializer = NULL;

  if(argc < 2 || argc > 3) {
    fprintf(stderr, "USAGE: %s RDF-FILE [BASE-URI]\n", program);
    rc = 1;
    goto tidy;
  }

  world = raptor_new_world();

  uri_string = (unsigned char*)argv[1];
  if(!access((const char*)uri_string, R_OK)) {
    uri_string = raptor_uri_filename_to_uri_string((char*)uri_string);
    uri = raptor_new_uri(world, uri_string);
    free_uri_string = 1;
  } else {
    uri = raptor_new_uri(world, (const unsigned char*)uri_string);
  }

  if(argc == 3) {
    char* base_uri_string = argv[2];
    base_uri = raptor_new_uri(world, (unsigned char*)(base_uri_string));
  } else {
    base_uri = raptor_uri_copy(uri);
  }

  rdf_parser = raptor_new_parser(world, "guess");

  raptor_world_set_log_handler(world, rdf_parser, to_ntriples_log_handler);

  raptor_parser_set_statement_handler(rdf_parser, NULL,
                                      to_ntriples_write_triple);

  rdf_serializer = raptor_new_serializer(world, "ntriples");

  raptor_serializer_start_to_file_handle(rdf_serializer, base_uri, stdout);
  raptor_parser_parse_file(rdf_parser, uri, base_uri);
  raptor_serializer_serialize_end(rdf_serializer);

  raptor_free_serializer(rdf_serializer);
  raptor_free_parser(rdf_parser);

  raptor_free_uri(base_uri);
  raptor_free_uri(uri);
  if(free_uri_string)
    raptor_free_memory(uri_string);

  raptor_free_world(world);

  tidy:
  if(warning_count)
    rc = 2;
  else if(error_count)
    rc = 1;

  return rc;
}

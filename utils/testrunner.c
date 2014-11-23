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
 *   testrunner MANIFEST-FILE [BASE-URI]
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
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#ifndef HAVE_GETOPT
#include <rasqal_getopt.h>
#endif

/* Rasqal includes */
#include <rasqal.h>
#include <rasqal_internal.h>

#include <raptor2.h>

#include "manifest.h"


#include "rasqalcmdline.h"

#ifdef NEED_OPTIND_DECLARATION
extern int optind;
extern char *optarg;
#endif

int main(int argc, char *argv[]);


static char *program = NULL;

#ifdef HAVE_GETOPT_LONG
#define HELP_TEXT(short, long, description) "  -" short ", --" long "  " description
#define HELP_TEXT_LONG(long, description) "      --" long "  " description
#define HELP_ARG(short, long) "--" #long
#define HELP_PAD "\n                          "
#else
#define HELP_TEXT(short, long, description) "  -" short "  " description
#define HELP_TEXT_LONG(long, description)
#define HELP_ARG(short, long) "-" #short
#define HELP_PAD "\n      "
#endif


#define GETOPT_STRING "ahnqt:v"

#ifdef HAVE_GETOPT_LONG

static struct option long_options[] =
{
  /* name, has_arg, flag, val */
  {"approved", 0, 0, 'a'},
  {"help", 0, 0, 'h'},
  {"dryrun", 0, 0, 'n'},
  {"quiet", 0, 0, 'q'},
  {"test", 1, 0, 't'},
  {"version", 0, 0, 'v'},
  {NULL, 0, 0, 0}
};
#endif


static int error_count = 0;
static int warning_count = 0;

static const char *title_string = "Rasqal RDF test runner utility";


static void
testrunner_log_handler(void *data, raptor_log_message *message)
{
  /* rasqal_world *world = (rasqal_world*)data; */

  switch(message->level) {
    case RAPTOR_LOG_LEVEL_FATAL:
    case RAPTOR_LOG_LEVEL_ERROR:
      fprintf(stderr, "%s: Error - ", program);
      raptor_locator_print(message->locator, stderr);
      fprintf(stderr, " - %s\n", message->text);

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


static void
print_help(rasqal_world* world, raptor_world* raptor_world_ptr)
{
  puts(title_string); puts(rasqal_version_string); putchar('\n');
  puts("Run an RDF query test suite.");
  printf("Usage: %s [OPTIONS] <manifest URI> [base URI]\n", program);
  
  fputs(rasqal_copyright_string, stdout);
  fputs("\nLicense: ", stdout);
  puts(rasqal_license_string);
  fputs("Rasqal home page: ", stdout);
  puts(rasqal_home_url_string);
  
  puts("\nOptions:");
  puts(HELP_TEXT("a", "approved        ", "Run only approved tests"));
  puts(HELP_TEXT("h", "help            ", "Print this help, then exit"));
  puts(HELP_TEXT("n", "dryrun          ", "Prepare but do not run the query"));
  puts(HELP_TEXT("q", "quiet           ", "No extra information messages"));
  puts(HELP_TEXT("t TEST", "test TEST  ", "Run just one TEST"));
  puts(HELP_TEXT("v", "version         ", "Print the Rasqal version"));
  puts("\nReport bugs to http://bugs.librdf.org/");
}


int
main(int argc, char *argv[])
{
  char *p;
  rasqal_world *world = NULL;
  raptor_world* raptor_world_ptr = NULL;
  unsigned char *uri_string;
  raptor_uri *uri;
  raptor_uri *base_uri;
  int rc = 0;
  int free_uri_string = 0;
  int usage = 0;
  int help = 0;
  int quiet = 0;
  int dryrun = 0;
  int approved = 0;
  manifest_world* mw;
  raptor_sequence* seq;
  manifest_test_result* result;
  char* test_string = NULL;

  program = argv[0];
  if((p = strrchr(program, '/')))
    program = p + 1;
  else if((p = strrchr(program, '\\')))
    program = p + 1;
  argv[0] = program;

  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    rc = 1;
    goto tidy;
  }

  raptor_world_ptr = rasqal_world_get_raptor(world);
  rasqal_world_set_log_handler(world, world, testrunner_log_handler);

  while (!usage && !help)
  {
    int c;
    
#ifdef HAVE_GETOPT_LONG
    int option_index = 0;

    c = getopt_long (argc, argv, GETOPT_STRING, long_options, &option_index);
#else
    c = getopt (argc, argv, GETOPT_STRING);
#endif
    if (c == -1)
      break;

    switch (c) {
      case 0:
      case '?': /* getopt() - unknown option */
        usage = 1;
        break;
        
      case 'a':
        approved = 1;
        break;

      case 'h':
        help = 1;
        break;

      case 'n':
        dryrun = 1;
        break;

      case 't':
        if(optarg)
          test_string = optarg;
        break;

      case 'q':
        quiet = 1;
        break;

      case 'v':
        fputs(rasqal_version_string, stdout);
        fputc('\n', stdout);
        rasqal_free_world(world);
        exit(0);

    }
    
  }

  if(!help && !usage) {
    if(optind != argc-1 && optind != argc-2)
      usage = 2; /* Title and usage */
  }

  
  if(usage) {
    if(usage > 1) {
      fputs(title_string, stderr); fputs(rasqal_version_string, stderr); putc('\n', stderr);
      fputs("Rasqal home page: ", stderr);
      fputs(rasqal_home_url_string, stderr);
      fputc('\n', stderr);
      fputs(rasqal_copyright_string, stderr);
      fputs("\nLicense: ", stderr);
      fputs(rasqal_license_string, stderr);
      fputs("\n\n", stderr);
    }
    fprintf(stderr, "Try `%s " HELP_ARG(h, help) "' for more information.\n",
                    program);
    rasqal_free_world(world);

    exit(1);
  }

  if(help) {
    print_help(world, raptor_world_ptr);
    rasqal_free_world(world);

    exit(0);
  }

  uri_string = (unsigned char*)argv[optind];
  if(!access((const char*)uri_string, R_OK)) {
    uri_string = raptor_uri_filename_to_uri_string((char*)uri_string);
    uri = raptor_new_uri(raptor_world_ptr, uri_string);
    free_uri_string = 1;
  } else {
    uri = raptor_new_uri(raptor_world_ptr, (const unsigned char*)uri_string);
  }

  if(optind == argc-1) {
    char* base_uri_string = argv[optind+1];
    base_uri = raptor_new_uri(raptor_world_ptr, (unsigned char*)(base_uri_string));
  } else {
    base_uri = raptor_uri_copy(uri);
  }

  mw = manifest_new_world(world);
  if(!mw) {
    fprintf(stderr, "%s: manifest_new_world() failed\n", program);
    rc = 1;
    goto tidy;
  }

  /* A sequence of owned URIs */
  seq = raptor_new_sequence((raptor_data_free_handler)raptor_free_uri, NULL);
  raptor_sequence_push(seq, raptor_uri_copy(uri));

  result = manifest_manifests_run(mw, seq, base_uri,
                                  test_string,
                                  /* indent */ 0,
                                  dryrun, !quiet, approved);
  raptor_free_sequence(seq);

  if(result) {
    rc = result->state == STATE_PASS ? 0 : 1;
    manifest_free_test_result(result);
  } else
    rc = 1;

  raptor_free_uri(base_uri);
  raptor_free_uri(uri);
  if(free_uri_string)
    raptor_free_memory(uri_string);

  manifest_free_world(mw);

  tidy:
  if(world)
    rasqal_free_world(world);

  return rc;
}

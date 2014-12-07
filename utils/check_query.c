/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * check_query.c - Rasqal RDF query test utility
 *
 * Copyright (C) 2011, David Beckett http://www.dajobe.org/
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
#include <stdarg.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
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
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* Rasqal includes */
#include <rasqal.h>

/* FIXME
 * This code uses internal structs
 *   rasqal_dataset
 * and macros
 *   RASQAL_CALLOC()
 *   RASQAL_FREE()
 * and APIs:
 *   rasqal_new_dataset()
 *   rasqal_dataset_load_graph_iostream()
 *   rasqal_free_dataset()
 *   rasqal_literal_equals_flags()
 *   rasqal_query_results_sort()
 *
 * The dataset API calls are used to read RDF graphs but nothing is
 * done with the data at present.
 *  
 */
#include <rasqal_internal.h>


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
#define HELP_PAD "\n                            "
#else
#define HELP_TEXT(short, long, description) "  -" short "  " description
#define HELP_TEXT_LONG(long, description)
#define HELP_ARG(short, long) "-" #short
#define HELP_PAD "\n      "
#endif

#define GETOPT_STRING "dF:g:hl:n:q:Q:r:R:v"

#ifdef HAVE_GETOPT_LONG

static struct option long_options[] =
{
  /* name, has_arg, flag, val */
  {"debug", 0, 0, 'd'},
  {"data-format", 1, 0, 'F'},
  {"default-graph", 1, 0, 'g'},
  {"help", 0, 0, 'h'},
  {"language", 1, 0, 'l'},
  {"named-graph", 1, 0, 'n'},
  {"query", 1, 0, 'q'},
  {"query-base-uri", 1, 0, 'Q'},
  {"result", 1, 0, 'r'},
  {"result-format", 1, 0, 'R'},
  {"version", 0, 0, 'v'},
  {NULL, 0, 0, 0}
};
#endif


static int error_count = 0;

static int verbose = 0;

static const char *title_string = "Rasqal RDF query test utility";




static void
check_query_log_handler(void* user_data, raptor_log_message *message)
{
  /* Only interested in errors and more severe */
  if(message->level < RAPTOR_LOG_LEVEL_ERROR)
    return;

  fprintf(stderr, "%s: Error: ", program);
  if(message->locator) {
    raptor_locator_print(message->locator, stderr);
    fputs(" : ", stderr);
  }
  fprintf(stderr, "%s\n", message->text);

  error_count++;
}


static rasqal_query*
check_query_init_query(rasqal_world *world, 
                       const char* ql_name,
                       const unsigned char* query_string,
                       raptor_uri* base_uri,
                       raptor_sequence* data_graphs)
{
  rasqal_query* rq;

  rq = rasqal_new_query(world, (const char*)ql_name, NULL);
  if(!rq) {
    fprintf(stderr, "%s: Failed to create query in language %s\n",
            program, ql_name);
    goto tidy_query;
  }
  

  if(rasqal_query_prepare(rq, (const unsigned char*)query_string, base_uri)) {
    fprintf(stderr, "%s: Parsing query failed\n", program);

    rasqal_free_query(rq); rq = NULL;
    goto tidy_query;
  }

  if(data_graphs) {
    rasqal_data_graph* dg;
    
    while((dg = (rasqal_data_graph*)raptor_sequence_pop(data_graphs))) {
      if(rasqal_query_add_data_graph(rq, dg)) {
        fprintf(stderr, "%s: Failed to add data graph to query\n",
                program);
        goto tidy_query;
      }
    }
  }

  tidy_query:
  return rq;
}


#define DEFAULT_QUERY_LANGUAGE "sparql"
#define DEFAULT_DATA_FORMAT_NAME_GRAPH "guess"
#define DEFAULT_RESULT_FORMAT_NAME "xml"


int
main(int argc, char *argv[]) 
{ 
  rasqal_world *world;
  raptor_world* raptor_world_ptr = NULL;
  int rc = 0;
  int usage = 0;
  int help = 0;
  const char* query_language = DEFAULT_QUERY_LANGUAGE;
  const char* query_filename = NULL;
  const char* data_format_name = NULL;
  const char* result_filename = NULL;
  const char* result_format_name = NULL;
  unsigned char* query_string = NULL;
  size_t query_len = 0;
  unsigned char *query_base_uri_string = NULL;
  int free_query_base_uri_string = 0;
  raptor_uri* query_base_uri = NULL;
  rasqal_query* rq = NULL;
  rasqal_query_results* results = NULL;
  rasqal_query_results* expected_results = NULL;
  raptor_sequence* data_graphs = NULL;
  raptor_iostream* result_iostr = NULL;
  rasqal_dataset* ds = NULL;
  rasqal_query_results_type results_type;

  /* Set globals */
  if(1) {
    char *p;
    
    program = argv[0];
    if((p = strrchr(program, '/')))
      program = p + 1;
    else if((p = strrchr(program, '\\')))
      program = p + 1;
    argv[0] = program;
  }
  
  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    return(1);
  }
  
  raptor_world_ptr = rasqal_world_get_raptor(world);
  rasqal_world_set_log_handler(world, world, check_query_log_handler);

  /* Option parsing */
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
        
      case 'd':
        verbose++;
        break;

      case 'F':
        if(optarg) {
          data_format_name = optarg;
        }
        break;
        
      case 'h':
        help = 1;
        break;

      case 'l':
        if(optarg) {
          query_language = optarg;
        }
        break;

      case 'q':
        if(optarg) {
          query_filename = optarg;
        }
        break;

      case 'Q':
        if(optarg) {
          query_base_uri_string = (unsigned char*)optarg;
        }
        break;

      case 'r':
        if(optarg) {
          result_filename = optarg;
        }
        break;
        
      case 'R':
        if(optarg) {
          result_format_name = optarg;
        }
        break;
        
      case 'v':
        fputs(rasqal_version_string, stdout);
        fputc('\n', stdout);
        rasqal_free_world(world);
        exit(0);

      case 'g':
      case 'n':
        if(optarg) {
          rasqal_data_graph *dg = NULL;
          rasqal_data_graph_flags type;

          type = (c == 'n') ? RASQAL_DATA_GRAPH_NAMED : 
                              RASQAL_DATA_GRAPH_BACKGROUND;
          dg = rasqal_cmdline_read_data_graph(world, type, (const char*)optarg,
                                              data_format_name);
          if(!dg) {
            fprintf(stderr, "%s: Failed to create data graph for `%s'\n",
                    program, optarg);
            return(1);
          }
          
          if(!data_graphs) {
            data_graphs = raptor_new_sequence((raptor_data_free_handler)rasqal_free_data_graph,
                                              NULL);

            if(!data_graphs) {
              fprintf(stderr, "%s: Failed to create data graphs sequence\n",
                      program);
              return(1);
            }
          }

          raptor_sequence_push(data_graphs, dg);
        }
        break;

    }
    
  } /* end while option */


  if(!help && !usage) {
    if(optind != argc) {
      fprintf(stderr, "%s: Extra arguments.\n", program);
      usage = 1;
    } else if(!result_filename) {
      usage = 2; /* Title and usage */
    } else if(!query_filename) {
      usage = 2; /* Title and usage */
    }
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
    int i;

    puts(title_string); puts(rasqal_version_string); putchar('\n');
    puts("Run an RDF query and check it against a known result.");
    printf("Usage: %s [OPTIONS] -g DATA -q QUERY-FILE -r RESULT-FILE\n\n", program);

    fputs(rasqal_copyright_string, stdout);
    fputs("\nLicense: ", stdout);
    puts(rasqal_license_string);
    fputs("Rasqal home page: ", stdout);
    puts(rasqal_home_url_string);

    puts("\nNormal operation is to execute the query in the QUERY-FILE and\ncompare to the query results in RESULT-FILE.");
    puts("\nMain options:");
    puts(HELP_TEXT("g URI", "default-graph URI", "Use URI as the default graph in the dataset"));
    puts(HELP_TEXT("l", "language LANGUAGE    ", "Set query language name to one of:"));
    for(i = 0; 1; i++) {
      const raptor_syntax_description* desc;

      desc = rasqal_world_get_query_language_description(world, RASQAL_GOOD_CAST(unsigned int, i));
      if(!desc)
         break;

      printf("    %-15s              %s", desc->names[0], desc->label);
      if(!i)
        puts(" (default)");
      else
        putchar('\n');
    }
    puts(HELP_TEXT("n URI", "named-graph URI  ", "Add named graph URI to dataset"));
    puts(HELP_TEXT("q FILE", "query QUERY-FILE", "Execute query in file QUERY-FILE"));
    puts(HELP_TEXT("r FILE", "result FILE     ", "Compare to result in file RESULTS-FILE"));


    puts("\nAdditional options:");
    puts(HELP_TEXT("d", "debug                ", "Increase debug message level"));
    puts(HELP_TEXT("F", "data-format NAME     ", "Set the data source format NAME (default: " DEFAULT_DATA_FORMAT_NAME_GRAPH ")"));
    puts(HELP_TEXT("h", "help                 ", "Print this help, then exit"));
    puts(HELP_TEXT("Q URI", "query-base-uri URI", "Set the base URI for the query"));
    puts(HELP_TEXT("R", "result-format NAME   ", "Set the result format NAME (default: " DEFAULT_RESULT_FORMAT_NAME ")"));
    puts("    For variable bindings and boolean results:");

    for(i = 0; 1; i++) {
      const raptor_syntax_description* desc;
 
      desc = rasqal_world_get_query_results_format_description(world, RASQAL_GOOD_CAST(unsigned int, i));
      if(!desc)
         break;
 
      if(desc->flags & RASQAL_QUERY_RESULTS_FORMAT_FLAG_READER) {
        printf("      %-10s     %s", desc->names[0], desc->label);
        if(!strcmp(desc->names[0], DEFAULT_RESULT_FORMAT_NAME))
          puts(" (default)");
        else
          putchar('\n');
      }
    }

    puts("    For RDF graph results:");

    for(i = 0; 1; i++) {
      const raptor_syntax_description *desc;
      desc = raptor_world_get_parser_description(raptor_world_ptr, RASQAL_GOOD_CAST(unsigned int, i));
      if(!desc)
        break;

      printf("      %-15s%s", desc->names[0], desc->label);
      if(!strcmp(desc->names[0], DEFAULT_DATA_FORMAT_NAME_GRAPH))
        puts(" (default)");
      else
        putchar('\n');
    }
    puts(HELP_TEXT("v", "version              ", "Print the Rasqal version"));

    puts("\nReport bugs to http://bugs.librdf.org/");

    rasqal_free_world(world);
    
    exit(0);
  }


  /* Compute query base URI from filename or passed in -Q QUERY-BASE-URI */
  if(!query_base_uri_string) {
    query_base_uri_string = raptor_uri_filename_to_uri_string(query_filename);
    free_query_base_uri_string = 1;
  }
  
  query_base_uri = raptor_new_uri(raptor_world_ptr, query_base_uri_string);
  if(!query_base_uri) {
    fprintf(stderr, "%s: Failed to create URI for %s\n",
            program, query_base_uri_string);
    return(1);
  }

  /* Read query from file into a string */
  query_string = rasqal_cmdline_read_file_string(world, query_filename,
                                                 "query file", &query_len);
  if(!query_string) {
    rc = 1;
    goto tidy_setup;
  }


  /* Report information */
  if(verbose) {
    fprintf(stderr, "%s: Reading query in language %s from file %s  URI %s:\n", 
            program, query_language, query_filename,
            raptor_uri_as_string(query_base_uri));
    if(verbose > 1)
      fprintf(stderr, "%s\n", (const char*)query_string);
    fprintf(stderr, "%s: Reading results from file '%s'\n", 
            program, result_filename);
  }


  /* Parse and prepare query */
  rq = check_query_init_query(world, query_language, query_string,
                              query_base_uri, data_graphs);
  rasqal_free_memory(query_string);
  if(!rq) {
    fprintf(stderr, "%s: Parsing query in %s failed\n", program,
            query_filename);
    goto tidy_query;
  }

  /* Query prepared OK - we now know the query details such as result type */


  /* Read expected results */
  if(1) {
    results_type = rasqal_query_get_result_type(rq);
    fprintf(stderr, "%s: Expecting result type %u\n", program, results_type);

    /* Read result file */
    result_iostr = raptor_new_iostream_from_filename(raptor_world_ptr,
                                                     result_filename);
    if(!result_iostr) {
      fprintf(stderr, "%s: result file '%s' open failed - %s\n", 
              program, result_filename, strerror(errno));
      rc = 1;
      goto tidy_setup;
    }


    switch(results_type) {
      case RASQAL_QUERY_RESULTS_BINDINGS:
      case RASQAL_QUERY_RESULTS_BOOLEAN:
        /* read results via rasqal query results format */
        expected_results = rasqal_cmdline_read_results(world,
                                                       raptor_world_ptr,
                                                       results_type,
                                                       result_iostr,
                                                       result_filename,
                                                       result_format_name);
        raptor_free_iostream(result_iostr); result_iostr = NULL;
        if(!expected_results) {
          fprintf(stderr, "%s: Failed to create query results\n", program);
          rc = 1;
          goto tidy_setup;
        }

        break;

      case RASQAL_QUERY_RESULTS_GRAPH:
        /* read results via raptor parser */

        if(1) {
          const char* format_name = NULL;

          if(result_format_name) {
            if(!raptor_world_is_parser_name(raptor_world_ptr,
                                            result_format_name)) {
              fprintf(stderr,
                      "%s: invalid parser name `%s' for `" HELP_ARG(R, result-format) "'\n\n",
                      program, result_format_name);
            } else
              format_name = result_format_name;
          }

          if(!format_name)
            format_name = DEFAULT_RESULT_FORMAT_NAME;

          ds = rasqal_new_dataset(world);
          if(!ds) {
            fprintf(stderr, "%s: Failed to create dataset", program);
            rc = 1;
            goto tidy_setup;
          }

          if(rasqal_dataset_load_graph_iostream(ds, format_name, 
                                                result_iostr, NULL)) {
            fprintf(stderr, "%s: Failed to load graph into dataset", program);
            rc = 1;
            goto tidy_setup;
          }

          raptor_free_iostream(result_iostr); result_iostr = NULL;

          /* FIXME
           *
           * The code at this point should do something with triples
           * in the dataset; save them for later to compare them to
           * the expected triples.  that requires a triples compare
           * OR a true RDF graph compare.
           *
           * Deleting the dataset here frees the triples just loaded.
           */
          rasqal_free_dataset(ds); ds = NULL;
        }
        break;
        
      case RASQAL_QUERY_RESULTS_SYNTAX:
      case RASQAL_QUERY_RESULTS_UNKNOWN:
        /* failure */
        fprintf(stderr,
                "%s: Reading %s query results format is not supported",
                program, rasqal_query_results_type_label(results_type));
        rc = 1;
        goto tidy_setup;
    }
    
  }


  /* save results for query execution so we can print and rewind */
  rasqal_query_set_store_results(rq, 1);

  results = rasqal_query_execute(rq);
  if(results) {

    switch(results_type) {
      case RASQAL_QUERY_RESULTS_BINDINGS:
        fprintf(stderr, "%s: Expected bindings results:\n", program);
        rasqal_cmdline_print_bindings_results_simple(program, expected_results,
                                                     stderr, 1, 0);
        
        fprintf(stderr, "%s: Actual bindings results:\n", program);
        rasqal_cmdline_print_bindings_results_simple(program, results,
                                                     stderr, 1, 0);

        rasqal_query_results_rewind(expected_results);
        rasqal_query_results_rewind(results);

        /* FIXME: should NOT do this if results are expected to be ordered */
        rasqal_query_results_sort(expected_results);
        rasqal_query_results_sort(results);

        if(1) {
          rasqal_results_compare* rrc;
          rrc = rasqal_new_results_compare(world,
                                          expected_results, "expected",
                                          results, "actual");
          rasqal_results_compare_set_log_handler(rrc, world,
                                                 check_query_log_handler);
          rc = !rasqal_results_compare_compare(rrc);
          rasqal_free_results_compare(rrc); rrc = NULL;
        }
        
        break;
        
      case RASQAL_QUERY_RESULTS_BOOLEAN:
        if(1) {
          int expected_boolean = rasqal_query_results_get_boolean(expected_results);
          int actual_boolean = rasqal_query_results_get_boolean(results);
          RASQAL_DEBUG2("Expected boolean result: %d\n", expected_boolean);
          RASQAL_DEBUG2("Actual boolean result: %d\n", actual_boolean);

          rc = (expected_boolean != actual_boolean);
        }
        break;

      case RASQAL_QUERY_RESULTS_GRAPH:
      case RASQAL_QUERY_RESULTS_SYNTAX:
      case RASQAL_QUERY_RESULTS_UNKNOWN:
        /* failure */
        fprintf(stderr, "%s: Query result format %u cannot be tested.", 
                program, results_type);
        rc = 1;
        goto tidy_setup;
    }
  } else
    rc = 1;


  if(verbose) {
    fprintf(stdout, "%s: Result: %s\n", program, rc ? "FAILURE" : "success");
  }

  if(results) {
    rasqal_free_query_results(results); results = NULL;
  }


  tidy_query:
  if(rq)
    rasqal_free_query(rq);


  tidy_setup:
  if(expected_results)
    rasqal_free_query_results(expected_results);

  if(results)
    rasqal_free_query_results(results);

  if(result_iostr)
    raptor_free_iostream(result_iostr);
  
  if(ds)
    rasqal_free_dataset(ds);

  if(free_query_base_uri_string)
    raptor_free_memory(query_base_uri_string);

  if(query_base_uri)
    raptor_free_uri(query_base_uri);

  if(data_graphs)
    raptor_free_sequence(data_graphs);

  rasqal_free_world(world);
  
  return (rc);
}

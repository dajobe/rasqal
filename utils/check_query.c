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
/* for access() and R_OK */
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#ifndef HAVE_GETOPT
#include <rasqal_getopt.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* Rasqal includes */
#include <rasqal.h>

/* FIXME - this uses internal APIs */
#include <rasqal_internal.h>


#ifdef BUFSIZ
#define FILE_READ_BUF_SIZE BUFSIZ
#else
#define FILE_READ_BUF_SIZE 1024
#endif


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

#define GETOPT_STRING "dD:F:hl:N:q:r:v"

#ifdef HAVE_GETOPT_LONG

static struct option long_options[] =
{
  /* name, has_arg, flag, val */
  {"debug", 0, 0, 'd'},
  {"default-graph", 1, 0, 'D'},
  {"format", 1, 0, 'F'},
  {"help", 0, 0, 'h'},
  {"language", 0, 0, 'l'},
  {"named-graph", 1, 0, 'N'},
  {"query", 0, 0, 'q'},
  {"query-base-uri", 0, 0, 'Q'},
  {"result", 0, 0, 'r'},
  {"version", 0, 0, 'v'},
  {NULL, 0, 0, 0}
};
#endif


static int error_count = 0;

static int verbose = 0;

static const char *title_format_string = "Rasqal RDF query test utility %s\n";


#ifdef HAVE_RAPTOR2_API
static void
check_query_log_handler(void* user_data, raptor_log_message *message)
{
  /* Only interested in errors and more severe */
  if(message->level < RAPTOR_LOG_LEVEL_ERROR)
    return;

  fprintf(stderr, "%s: Error - ", program);
  raptor_locator_print(message->locator, stderr);
  fprintf(stderr, " - %s\n", message->text);

  error_count++;
}
#else
static void
check_query_error_handler(void *user_data, 
                          raptor_locator* locator, const char *message) 
{
  fprintf(stderr, "%s: Error - ", program);
  raptor_print_locator(stderr, locator);
  fprintf(stderr, " - %s\n", message);

  error_count++;
}
#endif


static unsigned char*
check_query_read_file_string(const char* filename, 
                             const char* label,
                             size_t* len_p)
{
  raptor_stringbuffer *sb;
  size_t len;
  FILE *fh = NULL;
  unsigned char* string = NULL;

  sb = raptor_new_stringbuffer();
  if(!sb)
    return NULL;

  fh = fopen(filename, "r");
  if(!fh) {
    fprintf(stderr, "%s: %s '%s' open failed - %s", 
            program, label, filename, strerror(errno));
    goto tidy;
  }
    
  while(!feof(fh)) {
    unsigned char buffer[FILE_READ_BUF_SIZE];
    size_t read_len;
    
    read_len = fread((char*)buffer, 1, FILE_READ_BUF_SIZE, fh);
    if(read_len > 0)
      raptor_stringbuffer_append_counted_string(sb, buffer, read_len, 1);
    
    if(read_len < FILE_READ_BUF_SIZE) {
      if(ferror(fh)) {
        fprintf(stderr, "%s: file '%s' read failed - %s\n",
                program, filename, strerror(errno));
        goto tidy;
      }
      
      break;
    }
  }
  fclose(fh); fh = NULL;
  
  len = raptor_stringbuffer_length(sb);
  
  string = (unsigned char*)malloc(len + 1);
  if(string) {
    raptor_stringbuffer_copy_to_string(sb, string, len);
    if(len_p)
      *len_p = len;
  }
  
  tidy:
  if(fh)
    fclose(fh);

  if(sb)
    raptor_free_stringbuffer(sb);

  return string;
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
  
#ifdef HAVE_RAPTOR2_API
#else
  rasqal_query_set_error_handler(rq, world, check_query_error_handler);
  rasqal_query_set_fatal_error_handler(rq, world, check_query_error_handler);
#endif

  if(data_graphs) {
    rasqal_data_graph* dg;
    
    while((dg = (rasqal_data_graph*)raptor_sequence_pop(data_graphs))) {
      if(rasqal_query_add_data_graph2(rq, dg)) {
        fprintf(stderr, "%s: Failed to add data graph to query\n",
                program);
        goto tidy_query;
      }
    }
  }

  if(rasqal_query_prepare(rq, (const unsigned char*)query_string, base_uri)) {
    fprintf(stderr, "%s: Parsing query failed\n", program);

    rasqal_free_query(rq); rq = NULL;
    goto tidy_query;
  }

  tidy_query:
  return rq;
}



#define DEFAULT_QUERY_LANGUAGE "sparql"


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
  const char* result_filename = NULL;
  unsigned char* query_string = NULL;
  size_t query_len = 0;
  unsigned char *query_base_uri_string = NULL;
  int free_query_base_uri_string = 0;
  raptor_uri* query_base_uri = NULL;
  rasqal_query* rq = NULL;
  rasqal_query_results* results = NULL;
  raptor_sequence* data_graphs = NULL;
  char* data_graph_parser_name = NULL;
  raptor_iostream* result_iostr = NULL;
  rasqal_dataset* ds;

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
#ifdef HAVE_RAPTOR2_API
  rasqal_world_set_log_handler(world, world, check_query_log_handler);
#endif
  

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
          if(!raptor_world_is_parser_name(raptor_world_ptr, optarg)) {
              fprintf(stderr,
                      "%s: invalid parser name `%s' for `" HELP_ARG(F, format) "'\n\n",
                      program, optarg);
              usage = 1;
          } else {
            data_graph_parser_name = optarg;
          }
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
        
      case 'v':
        fputs(rasqal_version_string, stdout);
        fputc('\n', stdout);
        rasqal_free_world(world);
        exit(0);

      case 'D':
      case 'N':
        if(optarg) {
          rasqal_data_graph *dg = NULL;
          rasqal_data_graph_flags type;

          type = (c == 'N') ? RASQAL_DATA_GRAPH_NAMED : 
                              RASQAL_DATA_GRAPH_BACKGROUND;

          if(!access((const char*)optarg, R_OK)) {
            /* file: use URI */
            unsigned char* source_uri_string;
            raptor_uri* source_uri;
            raptor_uri* graph_name = NULL;

            source_uri_string = raptor_uri_filename_to_uri_string((const char*)optarg);
            source_uri = raptor_new_uri(raptor_world_ptr, source_uri_string);
            raptor_free_memory(source_uri_string);

            if(type == RASQAL_DATA_GRAPH_NAMED) 
              graph_name = source_uri;
            
            if(source_uri)
              dg = rasqal_new_data_graph_from_uri(world,
                                                  source_uri,
                                                  graph_name,
                                                  type,
                                                  NULL, data_graph_parser_name,
                                                  NULL);

            if(source_uri)
              raptor_free_uri(source_uri);
          } else {
            raptor_uri* source_uri;
            raptor_uri* graph_name = NULL;

            /* URI: use URI */
            source_uri = raptor_new_uri(raptor_world_ptr,
                                        (const unsigned char*)optarg);
            if(type == RASQAL_DATA_GRAPH_NAMED) 
              graph_name = source_uri;
            
            if(source_uri)
              dg = rasqal_new_data_graph_from_uri(world,
                                                  source_uri,
                                                  graph_name,
                                                  type,
                                                  NULL, data_graph_parser_name,
                                                  NULL);

            if(source_uri)
              raptor_free_uri(source_uri);
          }
          
          if(!dg) {
            fprintf(stderr, "%s: Failed to create data graph for `%s'\n",
                    program, optarg);
            return(1);
          }
          
          if(!data_graphs) {
#ifdef HAVE_RAPTOR2_API
            data_graphs = raptor_new_sequence((raptor_data_free_handler)rasqal_free_data_graph,
                                              NULL);

#else
            data_graphs = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_data_graph,
                                              NULL);
#endif
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
      fprintf(stderr, title_format_string, rasqal_version_string);
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
    printf(title_format_string, rasqal_version_string);
    puts("Run an RDF query and check it against a known result.");
    printf("Usage: %s [OPTIONS] -q QUERY-FILE -r RESULT-FILE\n\n", program);

    fputs(rasqal_copyright_string, stdout);
    fputs("\nLicense: ", stdout);
    puts(rasqal_license_string);
    fputs("Rasqal home page: ", stdout);
    puts(rasqal_home_url_string);

    puts("\nNormal operation is to execute the query in the QUERY-FILE and\ncompare to the query results in RESULT-FILE.");
    puts("\nMain options:");
    puts(HELP_TEXT("d", "debug              ", "Increase debug message level"));
    puts(HELP_TEXT("q", "query QUERY-FILE   ", "Execute query in file QUERY-FILE"));
    puts(HELP_TEXT("Q", "query-base-uri URI ", "Set the base URI for the query"));
    puts(HELP_TEXT("r", "result RESULTS-FILE", "Compare to result in file RESULTS-FILE"));
    puts(HELP_TEXT("h", "help               ", "Print this help, then exit"));
    puts(HELP_TEXT("v", "version            ", "Print the Rasqal version"));

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
  query_string = check_query_read_file_string(query_filename,
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
  if(check_query_init_query(world, query_language, query_string,
                            query_base_uri, data_graphs)) {
    fprintf(stderr, "%s: Parsing query in %s failed\n", program,
            query_filename);
    goto tidy_query;
  }

  /* Query prepared OK - we now know the query details such as result type */


  /* Read expected results */
  if(1) {
    rasqal_query_results_type type;
    
    type = rasqal_query_get_result_type(rq);
    fprintf(stderr, "%s: Expecting result type %d\n", program, type);

    /* Read result file */
    result_iostr = raptor_new_iostream_from_filename(raptor_world_ptr,
                                                     result_filename);
    if(!result_iostr) {
      fprintf(stderr, "%s: result file '%s' open failed - %s", 
              program, result_filename, strerror(errno));
      rc = 1;
      goto tidy_setup;
    }


    switch(type) {
      case RASQAL_QUERY_RESULTS_BINDINGS:
      case RASQAL_QUERY_RESULTS_BOOLEAN:
        /* read results via rasqal query results format */
        break;

      case RASQAL_QUERY_RESULTS_GRAPH:
        /* read results via raptor parser */

        if(1) {
          const char* format_name = "xml";
          
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

          rasqal_free_dataset(ds); ds = NULL;
        }
        break;
        
      case RASQAL_QUERY_RESULTS_SYNTAX:
        fprintf(stderr, 
                "%s: Reading query results format 'syntax' is not supported", 
                program);
        rc = 1;
        goto tidy_setup;
        break;

      case RASQAL_QUERY_RESULTS_UNKNOWN:
        /* failure */
        fprintf(stderr, "%s: Unknown query result format cannot be tested.", 
                program);
        rc = 1;
        goto tidy_setup;
        break;
    }
    
  }


  results = rasqal_query_execute(rq);
  if(results) {
    rasqal_query_results_type type;

    /* success in executing query */
    type = rasqal_query_results_get_type(results);
  } else
    rc = 1;

  if(verbose) {
    fprintf(stdout, "%s: Result: %s\n", program, rc ? "FAILURE" : "success");
  }

  if(results)
    rasqal_free_query_results(results);


  tidy_query:
  if(rq)
    rasqal_free_query(rq);


  tidy_setup:
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

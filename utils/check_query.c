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
 *   rasqal_literal_write()
 *   rasqal_query_results_sort()
 *  
 */
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

static const char *title_format_string = "Rasqal RDF query test utility %s\n";




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


static unsigned char*
check_query_read_file_string(const char* filename, 
                             const char* label,
                             size_t* len_p)
{
  raptor_stringbuffer *sb;
  size_t len;
  FILE *fh = NULL;
  unsigned char* string = NULL;
  unsigned char* buffer = NULL;

  sb = raptor_new_stringbuffer();
  if(!sb)
    return NULL;

  fh = fopen(filename, "r");
  if(!fh) {
    fprintf(stderr, "%s: %s '%s' open failed - %s", 
            program, label, filename, strerror(errno));
    goto tidy;
  }
    
  buffer = (unsigned char*)malloc(FILE_READ_BUF_SIZE);
  if(!buffer)
    goto tidy;

  while(!feof(fh)) {
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
  len = raptor_stringbuffer_length(sb);
  
  string = (unsigned char*)malloc(len + 1);
  if(string) {
    raptor_stringbuffer_copy_to_string(sb, string, len);
    if(len_p)
      *len_p = len;
  }
  
  tidy:
  if(buffer)
    free(buffer);

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


static void
print_bindings_result_simple(rasqal_query_results *results, FILE* output,
                             int quiet)
{
  while(!rasqal_query_results_finished(results)) {
    int i;
    
    fputs("result: [", output);
    for(i = 0; i < rasqal_query_results_get_bindings_count(results); i++) {
      const unsigned char *name;
      rasqal_literal *value;
      
      name = rasqal_query_results_get_binding_name(results, i);
      value = rasqal_query_results_get_binding_value(results, i);
      
      if(i > 0)
        fputs(", ", output);
      
      fprintf(output, "%s=", name);
      
      if(value)
        rasqal_literal_print(value, output);
      else
        fputs("NULL", output);
    }
    fputs("]\n", output);
    
    rasqal_query_results_next(results);
  }

  if(!quiet)
    fprintf(stderr, "%s: Query returned %d results\n", program, 
            rasqal_query_results_get_count(results));
}


static rasqal_query_results*
check_query_read_results(rasqal_world* world,
                         raptor_world* raptor_world_ptr,
                         rasqal_query_results_type results_type,
                         raptor_iostream* result_iostr,
                         const char* result_filename,
                         const char* result_format_name)
{
  rasqal_variables_table* vars_table = NULL;
  const char* format_name = NULL;
  rasqal_query_results_formatter* qrf = NULL;
  unsigned char *query_results_base_uri_string = NULL;
  raptor_uri* query_results_base_uri = NULL;
  rasqal_query_results* results = NULL;
  
  query_results_base_uri_string = raptor_uri_filename_to_uri_string(result_filename);
  
  query_results_base_uri = raptor_new_uri(raptor_world_ptr,
                                          query_results_base_uri_string);
  
  vars_table = rasqal_new_variables_table(world);
  results = rasqal_new_query_results(world, NULL, results_type, vars_table);
  rasqal_free_variables_table(vars_table); vars_table = NULL;
  
  if(!results) {
    fprintf(stderr, "%s: Failed to create query results\n", program);
    goto tidy_fail;
  }
  
  if(result_format_name) {
    /* FIXME validate result format name is legal query
     * results formatter name 
     */
    format_name = result_format_name;
  }
  
  if(!format_name)
    format_name = rasqal_world_guess_query_results_format_name(world,
                                                               NULL /* uri */,
                                                               NULL /* mime_type */,
                                                               NULL /*buffer */,
                                                               0,
                                                               (const unsigned char*)result_filename);
  
  qrf = rasqal_new_query_results_formatter(world, 
                                           format_name, 
                                           NULL /* mime type */,
                                           NULL /* uri */);
  if(!qrf)
    goto tidy_fail;
  
  if(rasqal_query_results_formatter_read(world, result_iostr, 
                                         qrf, results,
                                         query_results_base_uri))
  {
    fprintf(stderr, "%s: Failed to read query results from %s with format %s",
            program, result_filename, format_name);
    goto tidy_fail;
  }
  
  rasqal_free_query_results_formatter(qrf); qrf = NULL;
  
  return results;

  tidy_fail:
  if(vars_table)
    rasqal_free_variables_table(vars_table);
  if(results)
    rasqal_free_query_results(results);
  if(query_results_base_uri)
    raptor_free_uri(query_results_base_uri);

  return NULL;
}


typedef struct 
{
  rasqal_world* world;
  
  rasqal_query_results* qr1;
  const char* qr1_label;
  rasqal_query_results* qr2;
  const char* qr2_label;
  
  void* log_user_data;
  raptor_log_handler log_handler;
  raptor_log_message message;
} compare_query_results;


static compare_query_results*
new_compare_query_results(rasqal_world* world,
                          rasqal_query_results* qr1, const char* qr1_label,
                          rasqal_query_results* qr2, const char* qr2_label) 
{
  compare_query_results* cqr;
  
  cqr = RASQAL_CALLOC(compare_query_results*, 1, sizeof(*cqr));
  if(!cqr)
    return NULL;

  cqr->world = world;
  
  cqr->qr1 = qr1;
  cqr->qr1_label = qr1_label;
  cqr->qr2 = qr2;
  cqr->qr2_label = qr2_label;

  cqr->message.code = -1;
  cqr->message.domain = RAPTOR_DOMAIN_NONE;
  cqr->message.level = RAPTOR_LOG_LEVEL_NONE;
  cqr->message.locator = NULL;
  cqr->message.text = NULL;
  
  return cqr;
}


static void
free_compare_query_results(compare_query_results* cqr)
{
  if(!cqr)
    return;
  
  RASQAL_FREE(compare_query_results, cqr);
}


static void
compare_query_results_set_log_handler(compare_query_results* cqr,
                                      void* log_user_data,
                                      raptor_log_handler log_handler)
{
  cqr->log_user_data = log_user_data;
  cqr->log_handler = log_handler;
}


/*
 * Return value: non-0 if equal
 */
static int
compare_query_results_compare(compare_query_results* cqr)
{
  int differences = 0;
  int i;
  int rowi;
  int size1;
  int size2;
  int row_differences_count = 0;
  
  size1 = rasqal_query_results_get_bindings_count(cqr->qr1);
  size2 = rasqal_query_results_get_bindings_count(cqr->qr2);
  
  if(size1 != size2) {
    cqr->message.level = RAPTOR_LOG_LEVEL_ERROR;
    cqr->message.text = "Results have different numbers of bindings";
    if(cqr->log_handler)
      cqr->log_handler(cqr->log_user_data, &cqr->message);

    differences++;
    goto done;
  }
  
  
  /* check variables in each results project the same variables */
  for(i = 0; 1; i++) {
    const unsigned char* v1;
    const unsigned char* v2;
    
    v1 = rasqal_query_results_get_binding_name(cqr->qr1, i);
    v2 = rasqal_query_results_get_binding_name(cqr->qr2, i);
    if(!v1 && !v2)
      break;

    if(v1 && v2) {
      if(strcmp((const char*)v1, (const char*)v2)) {
        /* different names */
        differences++;
      }
    } else
      /* one is NULL, the other is a name */
      differences++;
  }

  if(differences) {
    cqr->message.level = RAPTOR_LOG_LEVEL_ERROR;
    cqr->message.text = "Results have different binding names";
    if(cqr->log_handler)
      cqr->log_handler(cqr->log_user_data, &cqr->message);

    goto done;
  }
  

  /* set results to be stored? */

  /* sort rows by something ?  As long as the sort is the same it
   * probably does not matter what the method is. */

  /* what to do about blank nodes? */

  /* for each row */
  for(rowi = 0; 1; rowi++) {
    int bindingi;
    rasqal_row* row1 = rasqal_query_results_get_row_by_offset(cqr->qr1, rowi);
    rasqal_row* row2 = rasqal_query_results_get_row_by_offset(cqr->qr2, rowi);
    int this_row_different = 0;
    
    if(!row1 && !row2)
      break;
    
    /* for each variable in row1 (== same variables in row2) */
    for(bindingi = 0; bindingi < size1; bindingi++) {
      /* we know the binding names are the same */
      const unsigned char* name;
      rasqal_literal *value1;
      rasqal_literal *value2;
      int error = 0;

      name = rasqal_query_results_get_binding_name(cqr->qr1, bindingi);

      value1 = rasqal_query_results_get_binding_value(cqr->qr1, bindingi);
      value2 = rasqal_query_results_get_binding_value(cqr->qr2, bindingi);

      /* should have compare as native flag? 
       * RASQAL_COMPARE_XQUERY doesn't compare all values
       */
      if(!rasqal_literal_equals_flags(value1, value2, RASQAL_COMPARE_XQUERY,
                                      &error)) {
        /* if different report it */
        raptor_world* raptor_world_ptr;
        void *string;
        size_t length;
        raptor_iostream* string_iostr;

        raptor_world_ptr = rasqal_world_get_raptor(cqr->world);

        string_iostr = raptor_new_iostream_to_string(raptor_world_ptr, 
                                                     &string, &length,
                                                     (raptor_data_malloc_handler)malloc);

        raptor_iostream_counted_string_write("Difference in row ", 18,
                                             string_iostr);
        raptor_iostream_decimal_write(rowi + 1,
                                      string_iostr);
        raptor_iostream_counted_string_write(" binding '", 10, 
                                             string_iostr);
        raptor_iostream_string_write(name,
                                     string_iostr);
        raptor_iostream_counted_string_write("' ", 2, 
                                             string_iostr);
        raptor_iostream_string_write(cqr->qr1_label, string_iostr);
        raptor_iostream_counted_string_write(" value ", 7,
                                             string_iostr);
        rasqal_literal_write(value1,
                             string_iostr);
        raptor_iostream_write_byte(' ',
                                   string_iostr);
        raptor_iostream_string_write(cqr->qr2_label,
                                     string_iostr);
        raptor_iostream_counted_string_write(" value ", 7,
                                             string_iostr);
        rasqal_literal_write(value2,
                             string_iostr);
        raptor_iostream_write_byte(' ',
                                   string_iostr);

        /* this allocates and copies result into 'string' */
        raptor_free_iostream(string_iostr);

        cqr->message.level = RAPTOR_LOG_LEVEL_ERROR;
        cqr->message.text = (const char*)string;
        if(cqr->log_handler)
          cqr->log_handler(cqr->log_user_data, &cqr->message);

        free(string);
        
        differences++;
        this_row_different = 1;
      }
    } /* end for each var */

    if(this_row_different)
      row_differences_count++;

    rasqal_query_results_next(cqr->qr1);
    rasqal_query_results_next(cqr->qr2);
  } /* end for each row */

  if(row_differences_count) {
    cqr->message.level = RAPTOR_LOG_LEVEL_ERROR;
    cqr->message.text = "Results have different values";
    if(cqr->log_handler)
      cqr->log_handler(cqr->log_user_data, &cqr->message);
  }

  done:
  return (differences == 0);
}



#define DEFAULT_QUERY_LANGUAGE "sparql"
#define DEFAULT_DATA_FORMAT_NAME_GRAPH "guess"
#define DEFAULT_RESULT_FORMAT_NAME_GRAPH "xml"


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
                                                  NULL, data_format_name,
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
                                                  NULL, data_format_name,
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
    int i;

    printf(title_format_string, rasqal_version_string);
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

      desc = rasqal_world_get_query_language_description(world, i);
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
    puts(HELP_TEXT("R", "result-format NAME   ", "Set the result format NAME (default: " DEFAULT_RESULT_FORMAT_NAME_GRAPH ")"));
    puts("    For variable bindings and boolean results:");

    for(i = 0; 1; i++) {
      const raptor_syntax_description* desc;
 
      desc = rasqal_world_get_query_results_format_description(world, i);
      if(!desc)
         break;
 
      if(desc->flags & RASQAL_QUERY_RESULTS_FORMAT_FLAG_READER) {
        printf("      %-10s     %s", desc->names[0], desc->label);
        if(!strcmp(desc->names[0], DEFAULT_RESULT_FORMAT_NAME_GRAPH))
          puts(" (default)");
        else
          putchar('\n');
      }
    }

    puts("    For RDF graph results:");

    for(i = 0; 1; i++) {
      const raptor_syntax_description *desc;
      desc = raptor_world_get_parser_description(raptor_world_ptr, i);
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
  rq = check_query_init_query(world, query_language, query_string,
                              query_base_uri, data_graphs);
  if(!rq) {
    fprintf(stderr, "%s: Parsing query in %s failed\n", program,
            query_filename);
    goto tidy_query;
  }

  /* Query prepared OK - we now know the query details such as result type */


  /* Read expected results */
  if(1) {
    results_type = rasqal_query_get_result_type(rq);
    fprintf(stderr, "%s: Expecting result type %d\n", program, results_type);

    /* Read result file */
    result_iostr = raptor_new_iostream_from_filename(raptor_world_ptr,
                                                     result_filename);
    if(!result_iostr) {
      fprintf(stderr, "%s: result file '%s' open failed - %s", 
              program, result_filename, strerror(errno));
      rc = 1;
      goto tidy_setup;
    }


    switch(results_type) {
      case RASQAL_QUERY_RESULTS_BINDINGS:
      case RASQAL_QUERY_RESULTS_BOOLEAN:
        /* read results via rasqal query results format */
        expected_results = check_query_read_results(world,
                                                    raptor_world_ptr,
                                                    results_type,
                                                    result_iostr,
                                                    result_filename,
                                                    result_format_name);
        raptor_free_iostream(result_iostr); result_iostr = NULL;

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
            format_name = DEFAULT_RESULT_FORMAT_NAME_GRAPH;

          
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


  /* save results for query execution so we can print and rewind */
  rasqal_query_set_store_results(rq, 1);

  results = rasqal_query_execute(rq);
  if(results) {

    switch(results_type) {
      case RASQAL_QUERY_RESULTS_BINDINGS:
        fprintf(stderr, "%s: Expected bindings results:\n", program);
        print_bindings_result_simple(expected_results, stderr, 1);
        
        fprintf(stderr, "%s: Actual bindings results:\n", program);
        print_bindings_result_simple(results, stderr, 1);

        rasqal_query_results_rewind(expected_results);
        rasqal_query_results_rewind(results);

        /* FIXME: should NOT do this if results are expected to be ordered */
        rasqal_query_results_sort(expected_results, rasqal_row_compare);
        rasqal_query_results_sort(results, rasqal_row_compare);

        if(1) {
          compare_query_results* cqr;
          cqr = new_compare_query_results(world,
                                          expected_results, "expected",
                                          results, "actual");
          compare_query_results_set_log_handler(cqr, world,
                                                check_query_log_handler);
          rc = !compare_query_results_compare(cqr);
          free_compare_query_results(cqr); cqr = NULL;
        }
        
        break;
        
      case RASQAL_QUERY_RESULTS_BOOLEAN:
      case RASQAL_QUERY_RESULTS_GRAPH:
      case RASQAL_QUERY_RESULTS_SYNTAX:
      case RASQAL_QUERY_RESULTS_UNKNOWN:
        /* failure */
        fprintf(stderr, "%s: Query result format %d cannot be tested.", 
                program, results_type);
        rc = 1;
        goto tidy_setup;
        break;
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

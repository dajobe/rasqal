/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * roqet.c - Rasqal RDF Query utility
 *
 * Copyright (C) 2004-2013, David Beckett http://www.dajobe.org/
 * Copyright (C) 2004-2005, University of Bristol, UK http://www.bristol.ac.uk/
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
#ifdef RASQAL_INTERNAL
#include <rasqal_internal.h>
#endif

#include "rasqalcmdline.h"

#ifdef NEED_OPTIND_DECLARATION
extern int optind;
extern char *optarg;
#endif

int main(int argc, char *argv[]);


static char *program=NULL;


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

#ifdef RASQAL_INTERNAL
/* add 'g:' */
#define GETOPT_STRING "cd:D:e:Ef:F:g:G:hi:np:qr:R:s:t:vW:"
#else
#define GETOPT_STRING "cd:D:e:Ef:F:G:hi:np:qr:R:s:t:vW:"
#endif

#ifdef HAVE_GETOPT_LONG

#ifdef RASQAL_INTERNAL
#define STORE_RESULTS_FLAG 0x100
#endif

static struct option long_options[] =
{
  /* name, has_arg, flag, val */
  {"count", 0, 0, 'c'},
  {"dump-query", 1, 0, 'd'},
  {"data", 1, 0, 'D'},
  {"exec", 1, 0, 'e'},
  {"ignore-errors", 0, 0, 'E'},
  {"feature", 1, 0, 'f'},
  {"format", 1, 0, 'F'},
  {"named", 1, 0, 'G'},
  {"help", 0, 0, 'h'},
  {"input", 1, 0, 'i'},
  {"dryrun", 0, 0, 'n'},
  {"protocol", 0, 0, 'p'},
  {"quiet", 0, 0, 'q'},
  {"results", 1, 0, 'r'},
  {"results-input-format", 1, 0, 'R'},
  {"source", 1, 0, 's'},
  {"results-input", 1, 0, 't'},
  {"version", 0, 0, 'v'},
  {"warnings", 1, 0, 'W'},
#ifdef STORE_RESULTS_FLAG
  {"store-results", 1, 0, STORE_RESULTS_FLAG},
#endif
  {NULL, 0, 0, 0}
};
#endif


static int error_count = 0;
static int warning_count = 0;

static int warning_level = -1;
static int ignore_errors = 0;

static const char *title_string = "Rasqal RDF query utility ";

#define MAX_QUERY_ERROR_REPORT_LEN 512


static void
roqet_log_handler(void *data, raptor_log_message *message)
{
  switch(message->level) {
    case RAPTOR_LOG_LEVEL_FATAL:
    case RAPTOR_LOG_LEVEL_ERROR:
      if(!ignore_errors) {
        fprintf(stderr, "%s: Error - ", program);
        raptor_locator_print(message->locator, stderr);
        fprintf(stderr, " - %s\n", message->text);
      }

      error_count++;
      break;

    case RAPTOR_LOG_LEVEL_WARN:
      if(warning_level > 0) {
        fprintf(stderr, "%s: Warning - ", program);
        raptor_locator_print(message->locator, stderr);
        fprintf(stderr, " - %s\n", message->text);
      }

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

#define SPACES_LENGTH 80
static const char spaces[SPACES_LENGTH + 1] = "                                                                                ";

static void
roqet_write_indent(FILE *fh, unsigned int indent) 
{
  while(indent > 0) {
    unsigned int sp = (indent > SPACES_LENGTH) ? SPACES_LENGTH : indent;

    (void)fwrite(spaces, sizeof(char), sp, fh);
    indent -= sp;
  }
}

  

static void
roqet_graph_pattern_walk(rasqal_graph_pattern *gp, int gp_index,
                         FILE *fh, unsigned int indent)
{
  int triple_index = 0;
  rasqal_graph_pattern_operator op;
  int seen;
  raptor_sequence *seq;
  int idx;
  rasqal_expression* expr;
  rasqal_variable* var;
  rasqal_literal* literal;
  
  op = rasqal_graph_pattern_get_operator(gp);
  
  roqet_write_indent(fh, indent);
  fprintf(fh, "%s graph pattern", 
          rasqal_graph_pattern_operator_as_string(op));
  idx = rasqal_graph_pattern_get_index(gp);

  if(idx >= 0)
    fprintf(fh, "[%d]", idx);

  if(gp_index >= 0)
    fprintf(fh, " #%d", gp_index);
  fputs(" {\n", fh);
  
  indent += 2;

  /* look for LET variable and value */
  var = rasqal_graph_pattern_get_variable(gp);
  if(var) {
    roqet_write_indent(fh, indent);
    fprintf(fh, "%s := ", var->name);
    rasqal_expression_print(var->expression, fh);
  }

  /* look for GRAPH literal */
  literal = rasqal_graph_pattern_get_origin(gp);
  if(literal) {
    roqet_write_indent(fh, indent);
    fputs("origin ", fh);
    rasqal_literal_print(literal, fh);
    fputc('\n', fh);
  }
  
  /* look for SERVICE literal */
  literal = rasqal_graph_pattern_get_service(gp);
  if(literal) {
    roqet_write_indent(fh, indent);
    rasqal_literal_print(literal, fh);
    fputc('\n', fh);
  }
  

  /* look for triples */
  seen = 0;
  while(1) {
    rasqal_triple* t;

    t = rasqal_graph_pattern_get_triple(gp, triple_index);
    if(!t)
      break;
    
    if(!seen) {
      roqet_write_indent(fh, indent);
      fputs("triples {\n", fh);
      seen = 1;
    }

    roqet_write_indent(fh, indent + 2);
    fprintf(fh, "triple #%d { ", triple_index);
    rasqal_triple_print(t, fh);
    fputs(" }\n", fh);

    triple_index++;
  }
  if(seen) {
    roqet_write_indent(fh, indent);
    fputs("}\n", fh);
  }


  /* look for sub-graph patterns */
  seq = rasqal_graph_pattern_get_sub_graph_pattern_sequence(gp);
  if(seq && raptor_sequence_size(seq) > 0) {
    roqet_write_indent(fh, indent);
    fprintf(fh, "sub-graph patterns (%d) {\n", raptor_sequence_size(seq));

    gp_index = 0;
    while(1) {
      rasqal_graph_pattern* sgp;
      sgp = rasqal_graph_pattern_get_sub_graph_pattern(gp, gp_index);
      if(!sgp)
        break;
      
      roqet_graph_pattern_walk(sgp, gp_index, fh, indent + 2);
      gp_index++;
    }

    roqet_write_indent(fh, indent);
    fputs("}\n", fh);
  }
  

  /* look for filter */
  expr = rasqal_graph_pattern_get_filter_expression(gp);
  if(expr) {
    roqet_write_indent(fh, indent);
    fputs("filter { ", fh);
    rasqal_expression_print(expr, fh);
    fputs("}\n", fh);
  }
  

  indent -= 2;
  
  roqet_write_indent(fh, indent);
  fputs("}\n", fh);
}


    
static void
roqet_query_write_variable(FILE* fh, rasqal_variable* v)
{
  fputs((const char*)v->name, fh);
  if(v->expression) {
    fputc('=', fh);
    rasqal_expression_print(v->expression, fh);
  }
}


static void
roqet_query_walk(rasqal_query *rq, FILE *fh, unsigned int indent)
{
  rasqal_query_verb verb;
  int i;
  rasqal_graph_pattern* gp;
  raptor_sequence *seq;

  verb = rasqal_query_get_verb(rq);
  roqet_write_indent(fh, indent);
  fprintf(fh, "query verb: %s\n", rasqal_query_verb_as_string(verb));

  i = rasqal_query_get_distinct(rq);
  if(i != 0) {
    roqet_write_indent(fh, indent);
    fprintf(fh, "query asks for distinct results\n");
  }
  
  i = rasqal_query_get_limit(rq);
  if(i >= 0) {
    roqet_write_indent(fh, indent);
    fprintf(fh, "query asks for result limits %d\n", i);
  }
  
  i = rasqal_query_get_offset(rq);
  if(i >= 0) {
    roqet_write_indent(fh, indent);
    fprintf(fh, "query asks for result offset %d\n", i);
  }
  
  seq = rasqal_query_get_bound_variable_sequence(rq);
  if(seq && raptor_sequence_size(seq) > 0) {
    int size = raptor_sequence_size(seq);
    fprintf(fh, "query projected variable names (%d): ", size);
    i = 0;
    while(1) {
      rasqal_variable* v = (rasqal_variable*)raptor_sequence_get_at(seq, i);
      if(!v)
        break;

      if(i > 0)
        fputs(", ", fh);

      fputs((const char*)v->name, fh);
      i++;
    }
    fputc('\n', fh);

    fprintf(fh, "query bound variables (%d): ", size);
    i = 0;
    while(1) {
      rasqal_variable* v = (rasqal_variable*)raptor_sequence_get_at(seq, i);
      if(!v)
        break;

      if(i > 0)
        fputs(", ", fh);

      roqet_query_write_variable(fh, v);
      i++;
    }
    fputc('\n', fh);
  }

  gp = rasqal_query_get_query_graph_pattern(rq);
  if(!gp)
    return;


  seq = rasqal_query_get_construct_triples_sequence(rq);
  if(seq && raptor_sequence_size(seq) > 0) {
    roqet_write_indent(fh, indent);
    fprintf(fh, "query construct triples (%d) {\n", 
            raptor_sequence_size(seq));
    i = 0;
    while(1) {
      rasqal_triple* t = rasqal_query_get_construct_triple(rq, i);
      if(!t)
        break;
    
      roqet_write_indent(fh, indent + 2);
      fprintf(fh, "triple #%d { ", i);
      rasqal_triple_print(t, fh);
      fputs(" }\n", fh);

      i++;
    }
    roqet_write_indent(fh, indent);
    fputs("}\n", fh);
  }

  /* look for binding rows */
  seq = rasqal_query_get_bindings_variables_sequence(rq);
  if(seq) {
    roqet_write_indent(fh, indent);
    fprintf(fh, "bindings variables (%d): ",  raptor_sequence_size(seq));
    
    i = 0;
    while(1) {
      rasqal_variable* v = rasqal_query_get_bindings_variable(rq, i);
      if(!v)
        break;

      if(i > 0)
        fputs(", ", fh);

      roqet_query_write_variable(fh, v);
      i++;
    }
    fputc('\n', fh);
    
    seq = rasqal_query_get_bindings_rows_sequence(rq);

    fprintf(fh, "bindings rows (%d) {\n", raptor_sequence_size(seq));
    i = 0;
    while(1) {
      rasqal_row* row;
      
      row = rasqal_query_get_bindings_row(rq, i);
      if(!row)
        break;
      
      roqet_write_indent(fh, indent + 2);
      fprintf(fh, "row #%d { ", i);
      rasqal_row_print(row, fh);
      fputs("}\n", fh);
      
      i++;
    }
  }


  fputs("query ", fh);
  roqet_graph_pattern_walk(gp, -1, fh, indent);
}


typedef enum {
  QUERY_OUTPUT_UNKNOWN,
  QUERY_OUTPUT_NONE,
  QUERY_OUTPUT_DEBUG,
  QUERY_OUTPUT_STRUCTURE,
  QUERY_OUTPUT_SPARQL,
  QUERY_OUTPUT_LAST = QUERY_OUTPUT_SPARQL
} query_output_format;

const char* query_output_format_labels[QUERY_OUTPUT_LAST + 1][2] = {
  { NULL, NULL },
  { "none", "No debug data" },
  { "debug", "Debug query dump (output format may change)" },
  { "structure", "Query structure walk (output format may change)" },
  { "sparql", "SPARQL" }
};



static void
print_boolean_result_simple(rasqal_query_results *results,
                            FILE* output, int quiet)
{
  fprintf(stderr, "%s: Query has a boolean result: %s\n", program,
          rasqal_query_results_get_boolean(results) ? "true" : "false");
}


static int
print_graph_result(rasqal_query* rq,
                   rasqal_query_results *results,
                   raptor_world* raptor_world_ptr,
                   FILE* output,
                   const char* serializer_syntax_name, raptor_uri* base_uri,
                   int quiet)
{
  int triple_count = 0;
  rasqal_prefix* prefix;
  int i;
  raptor_serializer* serializer = NULL;
  
  if(!quiet)
    fprintf(stderr, "%s: Query has a graph result:\n", program);
  
  if(!raptor_world_is_serializer_name(raptor_world_ptr, serializer_syntax_name)) {
    fprintf(stderr, 
            "%s: invalid query result serializer name `%s' for `" HELP_ARG(r, results) "'\n",
            program, serializer_syntax_name);
    return 1;
  }

  serializer = raptor_new_serializer(raptor_world_ptr,
                                     serializer_syntax_name);
  if(!serializer) {
    fprintf(stderr, "%s: Failed to create raptor serializer type %s\n",
            program, serializer_syntax_name);
    return(1);
  }
  
  /* Declare any query namespaces in the output serializer */
  for(i = 0; (prefix = rasqal_query_get_prefix(rq, i)); i++)
    raptor_serializer_set_namespace(serializer, prefix->uri, prefix->prefix);
  raptor_serializer_start_to_file_handle(serializer, base_uri, output);
  
  while(1) {
    raptor_statement *rs = rasqal_query_results_get_triple(results);
    if(!rs)
      break;

    raptor_serializer_serialize_statement(serializer, rs);
    triple_count++;
    
    if(rasqal_query_results_next_triple(results))
      break;
  }
  
  raptor_serializer_serialize_end(serializer);
  raptor_free_serializer(serializer);
  
  if(!quiet)
    fprintf(stderr, "%s: Total %d triples\n", program, triple_count);

  return 0;
}


static int
print_formatted_query_results(rasqal_world* world,
                              rasqal_query_results* results,
                              raptor_world* raptor_world_ptr,
                              FILE* output,
                              const char* result_format_name,
                              raptor_uri* base_uri,
                              int quiet)
{
  raptor_iostream *iostr;
  rasqal_query_results_formatter* results_formatter;
  int rc = 0;
  
  results_formatter = rasqal_new_query_results_formatter(world,
                                                         result_format_name,
                                                         NULL, NULL);
  if(!results_formatter) {
    fprintf(stderr, "%s: Invalid bindings result format `%s'\n",
            program, result_format_name);
    rc = 1;
    goto tidy;
  }
  

  iostr = raptor_new_iostream_to_file_handle(raptor_world_ptr, output);
  if(!iostr) {
    rasqal_free_query_results_formatter(results_formatter);
    rc = 1;
    goto tidy;
  }
  
  rc = rasqal_query_results_formatter_write(iostr, results_formatter,
                                            results, base_uri);
  raptor_free_iostream(iostr);
  rasqal_free_query_results_formatter(results_formatter);

  tidy:
  if(rc)
    fprintf(stderr, "%s: Formatting query results failed\n", program);

  return rc;
}



static rasqal_query_results*
roqet_call_sparql_service(rasqal_world* world, raptor_uri* service_uri,
                          const unsigned char* query_string,
                          raptor_sequence* data_graphs,
                          const char* format)
{
  rasqal_service* svc;
  rasqal_query_results* results;

  svc = rasqal_new_service(world, service_uri, query_string,
                           data_graphs);
  if(!svc) {
    fprintf(stderr, "%s: Failed to create service object\n", program);
    return NULL;
  }

  rasqal_service_set_format(svc, format);

  results = rasqal_service_execute(svc);

  rasqal_free_service(svc);

  return results;
}



static rasqal_query*
roqet_init_query(rasqal_world *world, 
                 const char* ql_name,
                 const char* ql_uri, const unsigned char* query_string,
                 raptor_uri* base_uri,
                 rasqal_feature query_feature, int query_feature_value,
                 const unsigned char* query_feature_string_value,
                 int store_results,
                 raptor_sequence* data_graphs)
{
  rasqal_query* rq;

  rq = rasqal_new_query(world, (const char*)ql_name,
                        (const unsigned char*)ql_uri);
  if(!rq) {
    fprintf(stderr, "%s: Failed to create query name %s\n",
            program, ql_name);
    goto tidy_query;
  }
  

  if(query_feature_value >= 0)
    rasqal_query_set_feature(rq, query_feature, query_feature_value);
  if(query_feature_string_value)
    rasqal_query_set_feature_string(rq, query_feature,
                                    query_feature_string_value);

#ifdef STORE_RESULTS_FLAG
  if(store_results >= 0)
    rasqal_query_set_store_results(rq, store_results);
#endif
  
  if(rasqal_query_prepare(rq, query_string, base_uri)) {
    size_t len = strlen((const char*)query_string);
    
    fprintf(stderr, "%s: Parsing query '", program);
    if(len > MAX_QUERY_ERROR_REPORT_LEN) {
      (void)fwrite(query_string,
                   RASQAL_GOOD_CAST(size_t, MAX_QUERY_ERROR_REPORT_LEN),
                   sizeof(char), stderr);
      fprintf(stderr, "...' (%d bytes) failed\n", RASQAL_BAD_CAST(int, len));
    } else {
      (void)fwrite(query_string, len, sizeof(char), stderr);
      fputs("' failed\n", stderr);
    }

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


static
void roqet_print_query(rasqal_query* rq, 
                       raptor_world* raptor_world_ptr,
                       query_output_format output_format,
                       raptor_uri* base_uri)
{
  fprintf(stderr, "Query:\n");
  
  switch(output_format) {
    case QUERY_OUTPUT_NONE:
      break;

    case QUERY_OUTPUT_DEBUG:
      rasqal_query_print(rq, stdout);
      break;
      
    case QUERY_OUTPUT_STRUCTURE:
      roqet_query_walk(rq, stdout, 0);
      break;
      
    case QUERY_OUTPUT_SPARQL:
      if(1) {
        raptor_iostream* output_iostr;
        output_iostr = raptor_new_iostream_to_file_handle(raptor_world_ptr,
                                                          stdout);
        rasqal_query_write(output_iostr, rq, NULL, base_uri);
        raptor_free_iostream(output_iostr);
      }
      break;
      
    case QUERY_OUTPUT_UNKNOWN:
    default:
      fprintf(stderr, "%s: Unknown query output format %u\n", program,
              output_format);
      abort();
  }
}


/* Default parser for input graphs */
#define DEFAULT_DATA_GRAPH_FORMAT "guess"
/* Default serializer for output graphs */
#define DEFAULT_GRAPH_FORMAT "ntriples"
/* Default input result format name */
#define DEFAULT_RESULT_FORMAT_NAME "xml"


static void
print_help(rasqal_world* world, raptor_world* raptor_world_ptr)
{
  unsigned int i;
    
  puts(title_string); puts(rasqal_version_string); putchar('\n');
  puts("Run an RDF query against data into formatted results.");
  printf("Usage: %s [OPTIONS] <query URI> [base URI]\n", program);
  printf("       %s [OPTIONS] -e <query string> [base URI]\n", program);
  printf("       %s [OPTIONS] -p <SPARQL protocol URI> <query URI> [base URI]\n", program);
  printf("       %s [OPTIONS] -p <SPARQL protocol URI> -e <query string> [base URI]\n", program);
  printf("       %s [OPTIONS] -t <query results file> [base URI]\n\n", program);
  
  fputs(rasqal_copyright_string, stdout);
  fputs("\nLicense: ", stdout);
  puts(rasqal_license_string);
  fputs("Rasqal home page: ", stdout);
  puts(rasqal_home_url_string);
  
  puts("\nNormal operation is to execute the query retrieved from URI <query URI>");
  puts("and print the results in a simple text format.");
  puts("\nMain options:");
  puts(HELP_TEXT("e", "exec QUERY      ", "Execute QUERY string instead of <query URI>"));
  puts(HELP_TEXT("p", "protocol URI    ", "Execute QUERY against a SPARQL protocol service URI"));
  puts(HELP_TEXT("i", "input LANGUAGE  ", "Set query language name to one of:"));
  for(i = 0; 1; i++) {
    const raptor_syntax_description* desc;
    
    desc = rasqal_world_get_query_language_description(world, i);
    if(!desc)
      break;
    
    printf("    %-15s         %s", desc->names[0], desc->label);
    if(!i)
      puts(" (default)");
    else
      putchar('\n');
  }
  
  puts(HELP_TEXT("r", "results FORMAT  ", "Set query results output format to one of:"));
  puts("    For variable bindings and boolean results:");
  puts("      simple                A simple text format (default)");
  
  for(i = 0; 1; i++) {
    const raptor_syntax_description* desc;
    
    desc = rasqal_world_get_query_results_format_description(world, i);
    if(!desc)
      break;
    
    if(desc->flags & RASQAL_QUERY_RESULTS_FORMAT_FLAG_WRITER)
      printf("      %-10s            %s\n", desc->names[0], desc->label);
  }
  
  puts("    For RDF graph results:");
  
  for(i = 0; 1; i++) {
    const raptor_syntax_description *desc;
    desc = raptor_world_get_parser_description(raptor_world_ptr, i);
    if(!desc)
      break;
    
    printf("      %-15s       %s", desc->names[0], desc->label);
    if(!i)
      puts(" (default)");
    else
      putchar('\n');
  }
  puts(HELP_TEXT("t", "results FILE    ", "Read query results from a FILE"));
  puts(HELP_TEXT("R", "results-input-format FORMAT", HELP_PAD "Set input query results format to one of:"));
  for(i = 0; 1; i++) {
    const raptor_syntax_description* desc;
    
    desc = rasqal_world_get_query_results_format_description(world, i);
    if(!desc)
      break;
    
    if(desc->flags & RASQAL_QUERY_RESULTS_FORMAT_FLAG_READER) {
      printf("      %-10s            %s", desc->names[0], desc->label);
      if(!strcmp(desc->names[0], DEFAULT_RESULT_FORMAT_NAME))
        puts(" (default)");
      else
        putchar('\n');
    }
  }
  
  puts("\nAdditional options:");
  puts(HELP_TEXT("c", "count           ", "Count triples - no output"));
  puts(HELP_TEXT("d FORMAT", "dump-query FORMAT", HELP_PAD "Print the parsed query out in FORMAT:"));
  for(i = 1; i <= QUERY_OUTPUT_LAST; i++)
    printf("      %-15s       %s\n", query_output_format_labels[i][0],
           query_output_format_labels[i][1]);
  puts(HELP_TEXT("D URI", "data URI    ", "RDF data source URI"));
  puts(HELP_TEXT("E", "ignore-errors   ", "Ignore error messages"));
  puts(HELP_TEXT("f FEATURE(=VALUE)", "feature FEATURE(=VALUE)", HELP_PAD "Set query features" HELP_PAD "  Use `-f help' for a list of valid features"));
  puts(HELP_TEXT("F NAME", "format NAME", "Set data source format name (default: guess)"));
  puts(HELP_TEXT("G URI", "named URI   ", "RDF named graph data source URI"));
  puts(HELP_TEXT("h", "help            ", "Print this help, then exit"));
  puts(HELP_TEXT("n", "dryrun          ", "Prepare but do not run the query"));
  puts(HELP_TEXT("q", "quiet           ", "No extra information messages"));
  puts(HELP_TEXT("s URI", "source URI  ", "Same as `-G URI'"));
  puts(HELP_TEXT("v", "version         ", "Print the Rasqal version"));
  puts(HELP_TEXT("W LEVEL", "warnings LEVEL", HELP_PAD "Set warning message LEVEL from 0: none to 100: all"));
#ifdef STORE_RESULTS_FLAG
  puts("\nDEBUG options:");
  puts(HELP_TEXT_LONG("store-results BOOL", "Set store results yes/no BOOL"));
#endif
  puts("\nReport bugs to http://bugs.librdf.org/");
}


typedef enum {
  MODE_EXEC_UNKNOWN,
  MODE_EXEC_QUERY_STRING,
  MODE_EXEC_QUERY_URI,
  MODE_CALL_PROTOCOL_URI,
  MODE_CALL_PROTOCOL_QUERY_STRING,
  MODE_READ_RESULTS
} roqet_mode;


int
main(int argc, char *argv[]) 
{ 
  int query_from_string = 0;
  unsigned char *query_string = NULL;
  unsigned char *uri_string = NULL;
  int free_uri_string = 0;
  unsigned char *base_uri_string = NULL;
  rasqal_query *rq = NULL;
  rasqal_query_results *results;
  const char *ql_name = "sparql";
  char *ql_uri = NULL;
  int rc = 0;
  raptor_uri *uri = NULL;
  raptor_uri *base_uri = NULL;
  char *filename = NULL;
  char *p;
  int usage = 0;
  int help = 0;
  int quiet = 0;
  int count = 0;
  int dryrun = 0;
  raptor_sequence* data_graphs = NULL;
  const char *result_format_name = NULL;
  query_output_format output_format = QUERY_OUTPUT_NONE;
  rasqal_feature query_feature = (rasqal_feature)-1;
  int query_feature_value= -1;
  unsigned char* query_feature_string_value = NULL;
  rasqal_world *world;
  raptor_world* raptor_world_ptr = NULL;
#ifdef RASQAL_INTERNAL
  int store_results = -1;
#endif
  char* data_graph_parser_name = NULL;
  raptor_iostream* iostr = NULL;
  const unsigned char* service_uri_string = 0;
  raptor_uri* service_uri = NULL;
  const char* result_filename = NULL;
  const char *result_input_format_name = NULL;
  roqet_mode mode = MODE_EXEC_UNKNOWN;
  
  program = argv[0];
  if((p = strrchr(program, '/')))
    program = p + 1;
  else if((p = strrchr(program, '\\')))
    program = p + 1;
  argv[0] = program;

  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    return(1);
  }
  
  raptor_world_ptr = rasqal_world_get_raptor(world);
  rasqal_world_set_log_handler(world, world, roqet_log_handler);
  
#ifdef STORE_RESULTS_FLAG
  /* This is for debugging only */
  if(1) {
    char* sr = getenv("RASQAL_DEBUG_STORE_RESULTS");
    if(sr)
      store_results = atoi(sr);
  }
#endif

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
        
      case 'c':
        count = 1;
        break;

      case 'd':
        output_format = QUERY_OUTPUT_UNKNOWN;
        if(optarg) {
          int i;

          for(i = 1; i <= QUERY_OUTPUT_LAST; i++)
            if(!strcmp(optarg, query_output_format_labels[i][0])) {
              output_format = (query_output_format)i;
              break;
            }
            
        }
        if(output_format == QUERY_OUTPUT_UNKNOWN) {
          int i;
          fprintf(stderr,
                  "%s: invalid argument `%s' for `" HELP_ARG(d, dump-query) "'\n",
                  program, optarg);
          for(i = 1; i <= QUERY_OUTPUT_LAST; i++)
            fprintf(stderr, 
                    "  %-12s for %s\n", query_output_format_labels[i][0],
                   query_output_format_labels[i][1]);
          usage = 1;
        }
        break;
        

      case 'e':
        if(optarg) {
          query_string = (unsigned char*)optarg;
          query_from_string = 1;
        }
        break;

      case 'f':
        if(optarg) {
          if(!strcmp(optarg, "help")) {
            unsigned int i;
            
            fprintf(stderr, "%s: Valid query features are:\n", program);
            for(i = 0; i < rasqal_get_feature_count(); i++) {
              const char *feature_name;
              const char *feature_label;
              if(!rasqal_features_enumerate(world, (rasqal_feature)i,
                                            &feature_name, NULL,
                                            &feature_label)) {
                const char *const feature_type =
                  (rasqal_feature_value_type((rasqal_feature)i) == 0)
                  ? (const char*)"" : (const char*)" (string)";
                fprintf(stderr, "  %-20s  %s%s\n", feature_name, feature_label, 
                       feature_type);
              }
            }
            fputs("Features are set with `" HELP_ARG(f, feature) " FEATURE=VALUE or `-f FEATURE'\nand take a decimal integer VALUE except where noted, defaulting to 1 if omitted.\n", stderr);

            rasqal_free_world(world);
            exit(0);
          } else {
            unsigned int i;
            size_t arg_len = strlen(optarg);
            
            for(i = 0; i < rasqal_get_feature_count(); i++) {
              const char *feature_name;
              size_t len;
              
              if(rasqal_features_enumerate(world, (rasqal_feature)i,
                                           &feature_name, NULL, NULL))
                continue;

              len = strlen(feature_name);

              if(!strncmp(optarg, feature_name, len)) {
                query_feature = (rasqal_feature)i;
                if(rasqal_feature_value_type(query_feature) == 0) {
                  if(len < arg_len && optarg[len] == '=')
                    query_feature_value=atoi(&optarg[len + 1]);
                  else if(len == arg_len)
                    query_feature_value = 1;
                } else {
                  if(len < arg_len && optarg[len] == '=')
                    query_feature_string_value = (unsigned char*)&optarg[len + 1];
                  else if(len == arg_len)
                    query_feature_string_value = (unsigned char*)"";
                }
                break;
              }
            }
            
            if(query_feature_value < 0 && !query_feature_string_value) {
              fprintf(stderr, "%s: invalid argument `%s' for `" HELP_ARG(f, feature) "'\nTry '%s " HELP_ARG(f, feature) " help' for a list of valid features\n",
                      program, optarg, program);
              usage = 1;
            }
          }
        }
        break;

      case 'F':
        if(optarg) {
          if(!raptor_world_is_parser_name(raptor_world_ptr, optarg)) {
              fprintf(stderr,
                      "%s: invalid parser name `%s' for `" HELP_ARG(F, format)  "'\nTry '%s -h' for a list of valid parsers\n",
                      program, optarg, program);
              usage = 1;
          } else {
            data_graph_parser_name = optarg;
          }
        }
        break;
        
      case 'h':
        help = 1;
        break;

      case 'n':
        dryrun = 1;
        break;

      case 'p':
        if(optarg)
          service_uri_string = (const unsigned char*)optarg;
        break;

      case 'r':
        if(optarg) {
          if(!strcmp(optarg, "simple"))
            optarg = NULL;
          else {
            if(!rasqal_query_results_formats_check2(world, optarg,
                                                    NULL /* uri */,
                                                    NULL /* mime type */,
                                                    RASQAL_QUERY_RESULTS_FORMAT_FLAG_READER)) {
              fprintf(stderr,
                      "%s: invalid output result format `%s' for `" HELP_ARG(r, results)  "'\nTry '%s -h' for a list of valid formats\n",
                      program, optarg, program);
              usage = 1;
            } else
              result_format_name = optarg;
          }
        }
        break;

      case 'R':
        if(optarg) {
          if(!rasqal_query_results_formats_check2(world, optarg,
                                                  NULL /* uri */,
                                                  NULL /* mime type */,
                                                  RASQAL_QUERY_RESULTS_FORMAT_FLAG_READER)) {
            fprintf(stderr,
                    "%s: invalid input result format `%s' for `" HELP_ARG(R, results-input-format)  "'\nTry '%s -h' for a list of valid formats\n",
                    program, optarg, program);
            usage = 1;
          } else
            result_input_format_name = optarg;
        }
        break;

      case 'i':
        if(rasqal_language_name_check(world, optarg))
          ql_name = optarg;
        else {
          unsigned int i;

          fprintf(stderr,
                  "%s: invalid query language `%s' for `" HELP_ARG(i, input) "'\n",
                  program, optarg);
          fprintf(stderr, "Valid query languages are:\n");
          for(i = 0; 1; i++) {
            const raptor_syntax_description* desc;
            desc = rasqal_world_get_query_language_description(world, i);
            if(desc == NULL)
              break;

            fprintf(stderr, "  %-18s for %s\n", desc->names[0], desc->label);
          }
          usage = 1;
        }
        break;

      case 'q':
        quiet = 1;
        break;

      case 's':
      case 'D':
      case 'G':
        if(optarg) {
          rasqal_data_graph *dg = NULL;
          rasqal_data_graph_flags type;

          type = (c == 's' || c == 'G') ? RASQAL_DATA_GRAPH_NAMED : 
                                          RASQAL_DATA_GRAPH_BACKGROUND;

          dg = rasqal_cmdline_read_data_graph(world, type, (const char*)optarg,
                                              data_graph_parser_name);
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

      case 'W':
        if(optarg)
          warning_level = atoi(optarg);
        else
          warning_level = 0;
        if(warning_level >= 0)
          rasqal_world_set_warning_level(world, RASQAL_GOOD_CAST(unsigned int, warning_level));
        break;

      case 'E':
        ignore_errors = 1;
        break;

      case 't':
        if(optarg) {
          result_filename = optarg;
        }
        break;

      case 'v':
        fputs(rasqal_version_string, stdout);
        fputc('\n', stdout);
        rasqal_free_world(world);
        exit(0);

#ifdef STORE_RESULTS_FLAG
      case STORE_RESULTS_FLAG:
        if(optarg)
          store_results = (!strcmp(optarg, "yes") || !strcmp(optarg, "YES"));
        break;
#endif

    }
    
  }

  if(!help && !usage) {
    if(service_uri_string) {
      if(optind != argc && optind != argc-1)
        usage = 2; /* Title and usage */
    } else if(query_string) {
      if(optind != argc && optind != argc-1)
        usage = 2; /* Title and usage */
    } else if(result_filename) {
      if(optind != argc && optind != argc-1)
        usage = 2; /* Title and usage */
    } else {
      if(optind != argc-1 && optind != argc-2)
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
    print_help(world, raptor_world_ptr);
    rasqal_free_world(world);

    exit(0);
  }

  if(service_uri_string && query_string) {
    mode = MODE_CALL_PROTOCOL_URI;
    service_uri = raptor_new_uri(raptor_world_ptr, service_uri_string);
    if(optind == argc-1)
      base_uri_string = (unsigned char*)argv[optind];
  } else if(query_string) {
    mode = MODE_EXEC_QUERY_STRING;
    if(optind == argc-1)
      base_uri_string = (unsigned char*)argv[optind];
  } else if(result_filename) {
    mode = MODE_READ_RESULTS;
    if(optind == argc-1)
      base_uri_string = (unsigned char*)argv[optind];
  } else {
    /* read a query from stdin, file or URI */
    if(service_uri_string) {
      mode = MODE_CALL_PROTOCOL_QUERY_STRING;
      service_uri = raptor_new_uri(raptor_world_ptr, service_uri_string);
    } else
      mode = MODE_EXEC_QUERY_URI;

    if(optind == argc-1)
      uri_string = (unsigned char*)argv[optind];
    else {
      uri_string = (unsigned char*)argv[optind++];
      base_uri_string = (unsigned char*)argv[optind];
    }

    /* If uri_string is "path-to-file", turn it into a file: URI */
    if(!strcmp((const char*)uri_string, "-")) {
      if(!base_uri_string) {
        fprintf(stderr,
                "%s: A base URI is required when reading a query from standard input.\n",
                program);
        return(1);
      }

      uri_string = NULL;
    } else if(!access((const char*)uri_string, R_OK)) {
      filename = (char*)uri_string;
      uri_string = raptor_uri_filename_to_uri_string(filename);
      free_uri_string = 1;
    }
    
    if(uri_string) {
      uri = raptor_new_uri(raptor_world_ptr, uri_string);
      if(!uri) {
        fprintf(stderr, "%s: Failed to create URI for %s\n",
                program, uri_string);
        return(1);
      }
    } else
      uri = NULL; /* stdin */

    query_string = rasqal_cmdline_read_uri_file_stdin_contents(world,
                                                               uri, filename,
                                                               NULL);
    if(!query_string) {
      rc = 1;
      goto tidy_setup;
    }
  }


  /* Compute base URI */
  if(!base_uri_string) {
    if(uri)
      base_uri = raptor_uri_copy(uri);
  } else
    base_uri = raptor_new_uri(raptor_world_ptr, base_uri_string);

  if(base_uri_string && !base_uri) {
    fprintf(stderr, "%s: Failed to create URI for %s\n",
            program, base_uri_string);
    return(1);
  }


  switch(mode) {
    case MODE_CALL_PROTOCOL_QUERY_STRING:
    case MODE_CALL_PROTOCOL_URI:
      if(!quiet) {
        fputs(program, stderr);
        fputs(": Calling SPARQL service at URI ", stderr);
        fputs(RASQAL_GOOD_CAST(const char*, service_uri_string), stderr);
        if(mode == MODE_CALL_PROTOCOL_QUERY_STRING) {
          if(query_string)
            fprintf(stderr, " with query '%s'", query_string);
        } else {
          if(filename)
            fprintf(stderr, " with query from file %s", filename);
          else if(uri_string)
            fprintf(stderr, " querying URI %s", uri_string);
        }
        if(base_uri_string)
          fprintf(stderr, " with base URI %s", base_uri_string);
        fputc('\n', stderr);
      }
      
      /* Execute query remotely */
      if(!dryrun)
        results = roqet_call_sparql_service(world, service_uri, query_string,
                                            data_graphs,
                                            /* service_format */ NULL);
    break;
        
    case MODE_EXEC_QUERY_STRING:
    case MODE_EXEC_QUERY_URI:
      if(!quiet) {
        fputs(program, stderr);
        fputs(": Running query", stderr);
        if(mode == MODE_EXEC_QUERY_STRING) {
          fprintf(stderr, " '%s'", query_string);
        } else {
          if(filename) {
            fputs(" from file ", stderr);
            fputs(filename, stderr);
          } else if(uri_string) {
            fputs(" from URI ", stderr);
            fputs(RASQAL_GOOD_CAST(const char*, uri_string), stderr);
          }
        }
        if(base_uri_string) {
          fputs(" with base URI ", stderr);
          fputs(RASQAL_GOOD_CAST(const char*, base_uri_string), stderr);
        }
        fputc('\n', stderr);
      }
      
      /* Execute query in this query engine (from URI or from -e QUERY) */
      rq = roqet_init_query(world,
                            ql_name, ql_uri, query_string,
                            base_uri,
                            query_feature, query_feature_value,
                            query_feature_string_value,
                            store_results,
                            data_graphs);
      
      if(!rq) {
        rc = 1;
        goto tidy_query;
      }
      
      if(output_format != QUERY_OUTPUT_NONE && !quiet)
        roqet_print_query(rq, raptor_world_ptr, output_format, base_uri);
      
      if(!dryrun)
        results = rasqal_query_execute(rq);
      break;
        
    case MODE_READ_RESULTS:
      if(!quiet) {
        if(base_uri_string)
          fprintf(stderr,
                  "%s: Reading results from file %s in format %s with base URI %s\n", program,
                  result_filename, result_input_format_name, base_uri_string);
        else
          fprintf(stderr, "%s: Reading results from file %s\n", program,
                  result_filename);
      }
      
      /* Read result set from filename */
      if(1) {
        raptor_iostream* result_iostr;

        result_iostr = raptor_new_iostream_from_filename(raptor_world_ptr,
                                                         result_filename);
        if(!result_iostr) {
          fprintf(stderr, "%s: results file '%s' open failed - %s\n",
                  program, result_filename, strerror(errno));
          rc = 1;
          goto tidy_setup;
        }

        results = rasqal_cmdline_read_results(world, raptor_world_ptr,
                                              RASQAL_QUERY_RESULTS_BINDINGS,
                                              result_iostr,
                                              result_filename,
                                              result_input_format_name);
        raptor_free_iostream(result_iostr); result_iostr = NULL;
      }

      if(!results) {
        fprintf(stderr, "%s: Failed to read results from '%s'\n", program,
                result_filename);
        rc = 1;
        goto tidy_setup;
      }
      break;
      
      
    case MODE_EXEC_UNKNOWN:
      break;
  }


  /* No results from dryrun */
  if(dryrun)
    goto tidy_query;
  
  if(!results) {
    fprintf(stderr, "%s: Query execution failed\n", program);
    rc = 1;
    goto tidy_query;
  }

  if(rasqal_query_results_is_bindings(results)) {
    if(result_format_name)
      rc = print_formatted_query_results(world, results,
                                         raptor_world_ptr, stdout,
                                         result_format_name, base_uri, quiet);
    else
      rasqal_cmdline_print_bindings_results_simple(program, results,
                                                   stdout, quiet, count);
  } else if(rasqal_query_results_is_boolean(results)) {
    if(result_format_name)
      rc = print_formatted_query_results(world, results,
                                         raptor_world_ptr, stdout,
                                         result_format_name, base_uri, quiet);
    else
      print_boolean_result_simple(results, stdout, quiet);
  } else if(rasqal_query_results_is_graph(results)) {
    if(!result_format_name)
      result_format_name = DEFAULT_GRAPH_FORMAT;
    
    rc = print_graph_result(rq, results, raptor_world_ptr,
                            stdout, result_format_name, base_uri, quiet);
  } else {
    fprintf(stderr, "%s: Query returned unknown result format\n", program);
    rc = 1;
  }

  rasqal_free_query_results(results);
  
 tidy_query:
  if(!query_from_string)
    free(query_string);

  if(rq)
    rasqal_free_query(rq);

 tidy_setup:

  if(data_graphs)
    raptor_free_sequence(data_graphs);
  if(base_uri)
    raptor_free_uri(base_uri);
  if(uri)
    raptor_free_uri(uri);
  if(free_uri_string)
    raptor_free_memory(uri_string);
  if(iostr)
    raptor_free_iostream(iostr);
  if(service_uri)
    raptor_free_uri(service_uri);

  rasqal_free_world(world);

  if(error_count && !ignore_errors)
    return 1;

  if(warning_count && warning_level != 0)
    return 2;
  
  return (rc);
}

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal-compare.c - Rasqal SPARQL Query Results Comparison Utility
 *
 * Copyright (C) 2025, David Beckett http://www.dajobe.org/
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

#define GETOPT_STRING "a:b:c:d:e:F:G:hi:jl:km:o:q:R:s:t:uvVwxS:"

#ifdef HAVE_GETOPT_LONG

static struct option long_options[] =
{
  /* name, has_arg, flag, val */
  {"actual", 1, 0, 'a'},
  {"blank-node-strategy", 1, 0, 'b'},
  {"context", 1, 0, 'c'},
  {"data", 1, 0, 'd'},
  {"expected", 1, 0, 'e'},
  {"format", 1, 0, 'F'},
  {"named-graph", 1, 0, 'G'},
  {"help", 0, 0, 'h'},
  {"input-format", 1, 0, 'i'},
  {"json", 0, 0, 'j'},
  {"max-differences", 1, 0, 'm'},
  {"order-sensitive", 0, 0, 'o'},
  {"query", 1, 0, 'q'},
  {"results-input-format", 1, 0, 'R'},
  {"source", 1, 0, 's'},
  {"timeout", 1, 0, 't'},
  {"unified", 0, 0, 'u'},
  {"verbose", 0, 0, 'v'},
  {"version", 0, 0, 'V'},
  {"warnings", 1, 0, 'w'},
  {"xml", 0, 0, 'x'},
  {"debug", 0, 0, 'k'},
  {"signature-threshold", 1, 0, 'S'},
  {NULL, 0, 0, 0}
};
#endif


static int error_count = 0;
static int verbose = 0;

static const char *title_string = "Rasqal SPARQL Query Results Comparison Utility";


static void
rasqal_compare_log_handler(void* user_data, raptor_log_message *message)
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





static void
print_help(rasqal_world* world, raptor_world* raptor_world_ptr)
{
  unsigned int i;

  puts(title_string); puts(rasqal_version_string); putchar('\n');
  puts("Compare SPARQL query results for equality.");
  printf("Usage: %s [OPTIONS] -e EXPECTED -a ACTUAL\n", program);
  printf("       %s [OPTIONS] -q QUERY -e EXPECTED [DATA-FILES]\n", program);
  printf("       %s [OPTIONS] -q QUERY -a ACTUAL [DATA-FILES]\n\n", program);

  fputs(rasqal_copyright_string, stdout);
  fputs("\nLicense: ", stdout);
  puts(rasqal_license_string);
  fputs("Rasqal home page: ", stdout);
  puts(rasqal_home_url_string);

  puts("\nNormal operation is to compare two SPARQL query result files");
  puts("or execute a query and compare against expected results.");
  puts("\nMain options:");
  puts(HELP_TEXT("q", "query FILE      ", "Execute SPARQL query from FILE"));
  puts(HELP_TEXT("e", "expected FILE   ", "Expected results file"));
  puts(HELP_TEXT("a", "actual FILE     ", "Actual results file (if not executing query)"));
  puts(HELP_TEXT("d", "data FILE       ", "RDF data file for query execution"));
  puts(HELP_TEXT("G", "named-graph FILE", "Named graph file for query execution"));
  puts(HELP_TEXT("F", "format FORMAT   ", "Data source format (default: auto-detect)"));
  puts(HELP_TEXT("R", "results-input-format FORMAT", HELP_PAD "Input results format (default: auto-detect)"));

  puts("\nComparison options:");
  puts(HELP_TEXT("o", "order-sensitive ", "Results must be in same order to be equal"));
  puts(HELP_TEXT("b", "blank-node-strategy STRATEGY", HELP_PAD "Blank node matching strategy:"));
  puts("    any                    Any blank node matches any other (default)");
  puts("    id                     Blank nodes must have same ID to match");
  puts("    structure              Blank nodes match based on structural similarity");
  puts(HELP_TEXT("m", "max-differences N", HELP_PAD "Maximum number of differences to report (default: 10)"));
  puts(HELP_TEXT("t", "timeout SECONDS ", HELP_PAD "Maximum search time for graph comparison (default: 30)"));
  puts(HELP_TEXT("S", "signature-threshold N", HELP_PAD "Signature complexity threshold (default: 1000)"));



  puts("\nDiff output options:");
  puts(HELP_TEXT("u", "unified         ", "Output unified diff format"));
  puts(HELP_TEXT("j", "json            ", "Output JSON diff format"));
  puts(HELP_TEXT("x", "xml             ", "Output XML diff format"));
  puts(HELP_TEXT("k", "debug           ", "Output debug format (similar to roqet -d debug)"));
  puts(HELP_TEXT("c", "context LINES   ", "Number of context lines in diff (default: 3)"));

  puts("\nStandard options:");
  puts(HELP_TEXT("h", "help            ", "Print this help, then exit"));
  puts(HELP_TEXT("v", "verbose         ", "Verbose output"));
  puts(HELP_TEXT("V", "version         ", "Print version"));
  puts(HELP_TEXT("w", "warnings LEVEL  ", "Set warning level (0-100, default: 50)"));

  puts("\nSupported input formats:");
  puts("    For variable bindings and boolean results:");
  for(i = 0; 1; i++) {
    const raptor_syntax_description* desc;

    desc = rasqal_world_get_query_results_format_description(world, i);
    if(!desc)
      break;

    if(desc->flags & RASQAL_QUERY_RESULTS_FORMAT_FLAG_READER) {
      printf("      %-10s            %s", desc->names[0], desc->label);
      if(!strcmp(desc->names[0], "xml"))
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

    printf("      %-15s       %s", desc->names[0], desc->label);
    if(!i)
      puts(" (default)");
    else
      putchar('\n');
  }

  puts("\nExit codes:");
  puts("    0  Results are equal");
  puts("    1  Results are different");
  puts("    2  Error occurred");

  puts("\nReport bugs to http://bugs.librdf.org/");
}


static rasqal_compare_blank_node_strategy
parse_blank_node_strategy(const char* strategy_name)
{
  if(!strcmp(strategy_name, "any"))
    return RASQAL_COMPARE_BLANK_NODE_MATCH_ANY;
  else if(!strcmp(strategy_name, "id"))
    return RASQAL_COMPARE_BLANK_NODE_MATCH_ID;
  else if(!strcmp(strategy_name, "structure"))
    return RASQAL_COMPARE_BLANK_NODE_MATCH_STRUCTURE;
  else
    return RASQAL_COMPARE_BLANK_NODE_MATCH_ANY; /* default */
}


static rasqal_query_results*
load_query_results_from_file(rasqal_world* world, const char* filename,
                            const char* format_name)
{
  raptor_world* raptor_world_ptr = rasqal_world_get_raptor(world);
  raptor_iostream* result_iostr = NULL;
  rasqal_query_results* results = NULL;

  result_iostr = raptor_new_iostream_from_filename(raptor_world_ptr, filename);
  if(!result_iostr) {
    fprintf(stderr, "%s: results file '%s' open failed - %s\n",
            program, filename, strerror(errno));
    return NULL;
  }

  results = rasqal_cmdline_read_results(world, raptor_world_ptr,
                                        RASQAL_QUERY_RESULTS_BINDINGS,
                                        result_iostr, filename, format_name);

  raptor_free_iostream(result_iostr);

  if(!results) {
    fprintf(stderr, "%s: Failed to load query results from %s\n", program, filename);
  }

  return results;
}


static rasqal_query_results*
execute_query(rasqal_world* world, const char* query_filename,
              raptor_sequence* data_graphs, const char* query_language)
{
  rasqal_query* query = NULL;
  rasqal_query_results* results = NULL;
  unsigned char* query_string = NULL;
  size_t query_len = 0;
  raptor_uri* query_base_uri = NULL;
  unsigned char* query_base_uri_string = NULL;
  int free_query_base_uri_string = 0;

  /* Read query from file */
  query_string = rasqal_cmdline_read_file_string(world, query_filename,
                                                 "query file", &query_len);
  if(!query_string) {
    return NULL;
  }

  /* Create base URI from filename */
  query_base_uri_string = raptor_uri_filename_to_uri_string(query_filename);
  if(!query_base_uri_string) {
    fprintf(stderr, "%s: Failed to create base URI for query file %s\n",
            program, query_filename);
    rasqal_free_memory(query_string);
    return NULL;
  }
  free_query_base_uri_string = 1;

  query_base_uri = raptor_new_uri(rasqal_world_get_raptor(world), query_base_uri_string);
  if(!query_base_uri) {
    fprintf(stderr, "%s: Failed to create base URI for query file %s\n",
            program, query_filename);
    rasqal_free_memory(query_string);
    raptor_free_memory(query_base_uri_string);
    return NULL;
  }

  /* Create and prepare query */
  query = rasqal_new_query(world, query_language, (const unsigned char*)query_base_uri_string);
  if(!query) {
    fprintf(stderr, "%s: Failed to create query\n", program);
    goto tidy;
  }

  /* Add data graphs if provided */
  if(data_graphs) {
    int i;
    for(i = 0; i < raptor_sequence_size(data_graphs); i++) {
      rasqal_data_graph* dg = (rasqal_data_graph*)raptor_sequence_get_at(data_graphs, i);
      if(rasqal_query_add_data_graph(query, dg)) {
        fprintf(stderr, "%s: Failed to add data graph\n", program);
        goto tidy;
      }
    }
  }

  /* Prepare query */
  if(rasqal_query_prepare(query, query_string, query_base_uri)) {
    fprintf(stderr, "%s: Failed to prepare query\n", program);
    goto tidy;
  }

  /* Execute query */
  results = rasqal_query_execute(query);
  if(!results) {
    fprintf(stderr, "%s: Failed to execute query\n", program);
    goto tidy;
  }

 tidy:
  if(query)
    rasqal_free_query(query);
  if(query_base_uri)
    raptor_free_uri(query_base_uri);
  if(free_query_base_uri_string && query_base_uri_string)
    raptor_free_memory(query_base_uri_string);
  if(query_string)
    rasqal_free_memory(query_string);

  return results;
}


static void
print_diff_output(rasqal_query_results_compare_result* result,
                  const char* diff_format, int context_lines)
{
  int i;

  if(!result || !result->differences_count)
    return;

  if(!diff_format || !strcmp(diff_format, "readable")) {
    /* Human-readable diff with structured data */
    printf("Found %d differences:\n", result->differences_count + result->triple_differences_count);

    /* String differences */
    for(i = 0; i < result->differences_count; i++) {
      rasqal_query_results_compare_difference* diff = &result->differences[i];
      printf("  %d: %s", i + 1, diff->description);
      if(diff->expected || diff->actual) {
        printf(" (expected: %s, actual: %s)",
               diff->expected ? diff->expected : "missing",
               diff->actual ? diff->actual : "missing");
      }
      printf("\n");
    }

    /* Triple differences */
    for(i = 0; i < result->triple_differences_count; i++) {
      rasqal_query_results_compare_triple_difference* diff = &result->triple_differences[i];
      printf("  %d: %s", result->differences_count + i + 1, diff->description);
      if(diff->expected_triple && diff->actual_triple) {
        unsigned char *subj1, *pred1, *obj1, *subj2, *pred2, *obj2;
        subj1 = raptor_term_to_string(diff->expected_triple->subject);
        pred1 = raptor_term_to_string(diff->expected_triple->predicate);
        obj1 = raptor_term_to_string(diff->expected_triple->object);
        subj2 = raptor_term_to_string(diff->actual_triple->subject);
        pred2 = raptor_term_to_string(diff->actual_triple->predicate);
        obj2 = raptor_term_to_string(diff->actual_triple->object);
        printf(" (expected: <%s> <%s> <%s>, actual: <%s> <%s> <%s>)",
               subj1, pred1, obj1, subj2, pred2, obj2);
        raptor_free_memory(subj1);
        raptor_free_memory(pred1);
        raptor_free_memory(obj1);
        raptor_free_memory(subj2);
        raptor_free_memory(pred2);
        raptor_free_memory(obj2);
      }
    }
  } else if(!strcmp(diff_format, "unified")) {
    /* Enhanced unified diff format with structured data */
    int total_differences = result->differences_count + result->triple_differences_count;

    printf("--- expected\n");
    printf("+++ actual\n");
    printf("@@ Comparison Results @@\n");
    if(total_differences == 0) {
      printf("Results are identical\n");
    } else {
      printf("Found %d differences:\n", total_differences);

      /* String differences */
      for(i = 0; i < result->differences_count; i++) {
        rasqal_query_results_compare_difference* str_diff = &result->differences[i];
        if(str_diff->expected && str_diff->actual) {
          printf("-%s\n", str_diff->expected);
          printf("+%s\n", str_diff->actual);
        } else if(str_diff->expected) {
          printf("-%s\n", str_diff->expected);
          /* No + line - value is missing in actual */
        } else if(str_diff->actual) {
          /* No - line - value is missing in expected */
          printf("+%s\n", str_diff->actual);
        } else {
          printf(" %s\n", str_diff->description);
        }
      }

      /* Triple differences */
      for(i = 0; i < result->triple_differences_count; i++) {
        rasqal_query_results_compare_triple_difference* trip_diff = &result->triple_differences[i];
        if(trip_diff->expected_triple && trip_diff->actual_triple) {
          unsigned char *subj1, *pred1, *obj1, *subj2, *pred2, *obj2;
          subj1 = raptor_term_to_string(trip_diff->expected_triple->subject);
          pred1 = raptor_term_to_string(trip_diff->expected_triple->predicate);
          obj1 = raptor_term_to_string(trip_diff->expected_triple->object);
          subj2 = raptor_term_to_string(trip_diff->actual_triple->subject);
          pred2 = raptor_term_to_string(trip_diff->actual_triple->predicate);
          obj2 = raptor_term_to_string(trip_diff->actual_triple->object);
          printf("-%s %s %s\n", subj1, pred1, obj1);
          printf("+%s %s %s\n", subj2, pred2, obj2);
          raptor_free_memory(subj1);
          raptor_free_memory(pred1);
          raptor_free_memory(obj1);
          raptor_free_memory(subj2);
          raptor_free_memory(pred2);
          raptor_free_memory(obj2);
        } else if(trip_diff->expected_triple) {
          unsigned char *subj, *pred, *obj;
          subj = raptor_term_to_string(trip_diff->expected_triple->subject);
          pred = raptor_term_to_string(trip_diff->expected_triple->predicate);
          obj = raptor_term_to_string(trip_diff->expected_triple->object);
          printf("-%s %s %s\n", subj, pred, obj);
          raptor_free_memory(subj);
          raptor_free_memory(pred);
          raptor_free_memory(obj);
          /* No + line - triple is missing in actual */
        } else if(trip_diff->actual_triple) {
          /* No - line - triple is missing in expected */
          unsigned char *subj, *pred, *obj;
          subj = raptor_term_to_string(trip_diff->actual_triple->subject);
          pred = raptor_term_to_string(trip_diff->actual_triple->predicate);
          obj = raptor_term_to_string(trip_diff->actual_triple->object);
          printf("+%s %s %s\n", subj, pred, obj);
          raptor_free_memory(subj);
          raptor_free_memory(pred);
          raptor_free_memory(obj);
        }
      }
      printf("\nSummary: %d differences found\n", total_differences);
    }
  } else if(!strcmp(diff_format, "json")) {
    /* JSON diff format with structured data */
    printf("{\n");
    printf("  \"equal\": %s,\n", result->equal ? "true" : "false");
    printf("  \"differences_count\": %d,\n", result->differences_count);
    printf("  \"triple_differences_count\": %d,\n", result->triple_differences_count);

    /* String differences */
    printf("  \"differences\": [\n");
    for(i = 0; i < result->differences_count; i++) {
      rasqal_query_results_compare_difference* json_diff = &result->differences[i];
      printf("    {\n");
      printf("      \"description\": \"%s\"", json_diff->description);
      if(json_diff->expected || json_diff->actual) {
        printf(",\n      \"expected\": \"%s\"", json_diff->expected ? json_diff->expected : "missing");
        printf(",\n      \"actual\": \"%s\"", json_diff->actual ? json_diff->actual : "missing");
      }
      printf("\n    }");
      if(i < result->differences_count - 1)
        printf(",");
      printf("\n");
    }
    printf("  ],\n");

    /* Triple differences */
    printf("  \"triple_differences\": [\n");
    for(i = 0; i < result->triple_differences_count; i++) {
      rasqal_query_results_compare_triple_difference* json_trip_diff = &result->triple_differences[i];
      printf("    {\n");
      printf("      \"description\": \"%s\"", json_trip_diff->description);
      if(json_trip_diff->expected_triple) {
        unsigned char *subj, *pred, *obj;
        subj = raptor_term_to_string(json_trip_diff->expected_triple->subject);
        pred = raptor_term_to_string(json_trip_diff->expected_triple->predicate);
        obj = raptor_term_to_string(json_trip_diff->expected_triple->object);
        printf(",\n      \"expected_triple\": {\n");
        printf("        \"subject\": \"%s\",\n", subj);
        printf("        \"predicate\": \"%s\",\n", pred);
        printf("        \"object\": \"%s\"\n", obj);
        printf("      }");
        raptor_free_memory(subj);
        raptor_free_memory(pred);
        raptor_free_memory(obj);
      }
      if(json_trip_diff->actual_triple) {
        unsigned char *subj, *pred, *obj;
        subj = raptor_term_to_string(json_trip_diff->actual_triple->subject);
        pred = raptor_term_to_string(json_trip_diff->actual_triple->predicate);
        obj = raptor_term_to_string(json_trip_diff->actual_triple->object);
        printf(",\n      \"actual_triple\": {\n");
        printf("        \"subject\": \"%s\",\n", subj);
        printf("        \"predicate\": \"%s\",\n", pred);
        printf("        \"object\": \"%s\"\n", obj);
        printf("      }");
        raptor_free_memory(subj);
        raptor_free_memory(pred);
        raptor_free_memory(obj);
      }
      printf("\n    }");
      if(i < result->triple_differences_count - 1)
        printf(",");
      printf("\n");
    }
    printf("  ]\n");
    printf("}\n");
  } else if(!strcmp(diff_format, "xml")) {
    /* XML diff format with structured data */
    printf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    printf("<comparison>\n");
    printf("  <equal>%s</equal>\n", result->equal ? "true" : "false");
    printf("  <differences_count>%d</differences_count>\n", result->differences_count);
    printf("  <triple_differences_count>%d</triple_differences_count>\n", result->triple_differences_count);

    /* String differences */
    printf("  <differences>\n");
    for(i = 0; i < result->differences_count; i++) {
      rasqal_query_results_compare_difference* xml_diff = &result->differences[i];
      printf("    <difference>\n");
      printf("      <description>%s</description>\n", xml_diff->description);
      if(xml_diff->expected || xml_diff->actual) {
        printf("      <expected>%s</expected>\n", xml_diff->expected ? xml_diff->expected : "missing");
        printf("      <actual>%s</actual>\n", xml_diff->actual ? xml_diff->actual : "missing");
      }
      printf("    </difference>\n");
    }
    printf("  </differences>\n");

    /* Triple differences */
    printf("  <triple_differences>\n");
    for(i = 0; i < result->triple_differences_count; i++) {
      rasqal_query_results_compare_triple_difference* xml_trip_diff = &result->triple_differences[i];
      printf("    <triple_difference>\n");
      printf("      <description>%s</description>\n", xml_trip_diff->description);
      if(xml_trip_diff->expected_triple) {
        unsigned char *subj, *pred, *obj;
        subj = raptor_term_to_string(xml_trip_diff->expected_triple->subject);
        pred = raptor_term_to_string(xml_trip_diff->expected_triple->predicate);
        obj = raptor_term_to_string(xml_trip_diff->expected_triple->object);
        printf("      <expected_triple>\n");
        printf("        <subject>%s</subject>\n", subj);
        printf("        <predicate>%s</predicate>\n", pred);
        printf("        <object>%s</object>\n", obj);
        printf("      </expected_triple>\n");
        raptor_free_memory(subj);
        raptor_free_memory(pred);
        raptor_free_memory(obj);
      }
      if(xml_trip_diff->actual_triple) {
        unsigned char *subj, *pred, *obj;
        subj = raptor_term_to_string(xml_trip_diff->actual_triple->subject);
        pred = raptor_term_to_string(xml_trip_diff->actual_triple->predicate);
        obj = raptor_term_to_string(xml_trip_diff->actual_triple->object);
        printf("      <actual_triple>\n");
        printf("        <subject>%s</subject>\n", subj);
        printf("        <predicate>%s</predicate>\n", pred);
        printf("        <object>%s</object>\n", obj);
        printf("      </actual_triple>\n");
        raptor_free_memory(subj);
        raptor_free_memory(pred);
        raptor_free_memory(obj);
      }
      printf("    </triple_difference>\n");
    }
    printf("  </triple_differences>\n");
    printf("</comparison>\n");
  } else if(!strcmp(diff_format, "debug")) {
    /* Debug format with structured data */
    printf("comparison result: %s\n", result->equal ? "equal" : "different");
    printf("differences count: %d\n", result->differences_count);
    printf("triple differences count: %d\n", result->triple_differences_count);

    /* String differences */
    if(result->differences_count > 0) {
      printf("differences:\n");
      for(i = 0; i < result->differences_count; i++) {
        rasqal_query_results_compare_difference* debug_diff = &result->differences[i];
        printf("  %d: %s", i + 1, debug_diff->description);
        if(debug_diff->expected || debug_diff->actual) {
          printf(" (expected: %s, actual: %s)",
                 debug_diff->expected ? debug_diff->expected : "missing",
                 debug_diff->actual ? debug_diff->actual : "missing");
        }
        printf("\n");
      }
    }

    /* Triple differences */
    if(result->triple_differences_count > 0) {
      printf("triple differences:\n");
      for(i = 0; i < result->triple_differences_count; i++) {
        rasqal_query_results_compare_triple_difference* debug_trip_diff = &result->triple_differences[i];
        printf("  %d: %s", i + 1, debug_trip_diff->description);
        if(debug_trip_diff->expected_triple && debug_trip_diff->actual_triple) {
          unsigned char *subj1, *pred1, *obj1, *subj2, *pred2, *obj2;
          subj1 = raptor_term_to_string(debug_trip_diff->expected_triple->subject);
          pred1 = raptor_term_to_string(debug_trip_diff->expected_triple->predicate);
          obj1 = raptor_term_to_string(debug_trip_diff->expected_triple->object);
          subj2 = raptor_term_to_string(debug_trip_diff->actual_triple->subject);
          pred2 = raptor_term_to_string(debug_trip_diff->actual_triple->predicate);
          obj2 = raptor_term_to_string(debug_trip_diff->actual_triple->object);
          printf(" (expected: <%s> <%s> <%s>, actual: <%s> <%s> <%s>)",
                 subj1, pred1, obj1, subj2, pred2, obj2);
          raptor_free_memory(subj1);
          raptor_free_memory(pred1);
          raptor_free_memory(obj1);
          raptor_free_memory(subj2);
          raptor_free_memory(pred2);
          raptor_free_memory(obj2);
        } else if(debug_trip_diff->expected_triple) {
          unsigned char *subj, *pred, *obj;
          subj = raptor_term_to_string(debug_trip_diff->expected_triple->subject);
          pred = raptor_term_to_string(debug_trip_diff->expected_triple->predicate);
          obj = raptor_term_to_string(debug_trip_diff->expected_triple->object);
          printf(" (expected: <%s> <%s> <%s>, actual: missing)",
                 subj, pred, obj);
          raptor_free_memory(subj);
          raptor_free_memory(pred);
          raptor_free_memory(obj);
        } else if(debug_trip_diff->actual_triple) {
          unsigned char *subj, *pred, *obj;
          subj = raptor_term_to_string(debug_trip_diff->actual_triple->subject);
          pred = raptor_term_to_string(debug_trip_diff->actual_triple->predicate);
          obj = raptor_term_to_string(debug_trip_diff->actual_triple->object);
          printf(" (expected: missing, actual: <%s> <%s> <%s>)",
                 subj, pred, obj);
          raptor_free_memory(subj);
          raptor_free_memory(pred);
          raptor_free_memory(obj);
        }
        printf("\n");
      }
    }

    if(result->error_message) {
      printf("error message: %s\n", result->error_message);
    }
  }
}


int
main(int argc, char *argv[])
{
  rasqal_world *world;
  raptor_world* raptor_world_ptr = NULL;
  rasqal_query_results *expected_results = NULL;
  rasqal_query_results *actual_results = NULL;
  rasqal_query_results_compare *compare = NULL;
  rasqal_query_results_compare_result *result = NULL;
  rasqal_query_results_compare_options options;

  /* Command line options */
  char *query_filename = NULL;
  char *expected_filename = NULL;
  char *actual_filename = NULL;
  char *data_filename = NULL;
  char *named_graph_filename = NULL;
  char *data_format_name = NULL;
  char *results_format_name = NULL;
  const char *query_language = "sparql";
  const char *diff_format = "readable";
  int context_lines = 3;
  int warning_level = 50;
  int rc = 0;
  int usage = 0;
  int help = 0;
  raptor_sequence* data_graphs = NULL;

  program = argv[0];
  if(strrchr(program, '/'))
    program = strrchr(program, '/') + 1;
  else if(strrchr(program, '\\'))
    program = strrchr(program, '\\') + 1;

  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    return(2);
  }

  raptor_world_ptr = rasqal_world_get_raptor(world);
  rasqal_world_set_log_handler(world, world, rasqal_compare_log_handler);
  /* Warning handler is not available in public API */

  /* Initialize comparison options with defaults */
  rasqal_query_results_compare_options_init(&options);
  
  /* Initialize graph comparison options */
  if(!options.graph_comparison_options) {
    options.graph_comparison_options = (rasqal_graph_comparison_options*)malloc(sizeof(*options.graph_comparison_options));
    if(options.graph_comparison_options)
      rasqal_graph_comparison_options_init(options.graph_comparison_options);
  }

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
        if(optarg) {
          actual_filename = optarg;
        }
        break;

      case 'b':
        if(optarg) {
          options.blank_node_strategy = parse_blank_node_strategy(optarg);
        }
        break;

      case 'c':
        if(optarg) {
          context_lines = atoi(optarg);
          if(context_lines < 0)
            context_lines = 3;
        }
        break;

      case 'd':
        if(optarg) {
          data_filename = optarg;
        }
        break;

      case 'e':
        if(optarg) {
          expected_filename = optarg;
        }
        break;

      case 'F':
        if(optarg) {
          data_format_name = optarg;
        }
        break;

      case 'G':
        if(optarg) {
          named_graph_filename = optarg;
        }
        break;

      case 'h':
        help = 1;
        break;



      case 'j':
        diff_format = (const char*)"json";
        break;



      case 'm':
        if(optarg) {
          options.max_differences = atoi(optarg);
          if(options.max_differences < 1)
            options.max_differences = 10;
        }
        break;



      case 'o':
        options.order_sensitive = 1;
        break;

      case 'q':
        if(optarg) {
          query_filename = optarg;
        }
        break;

      case 'R':
        if(optarg) {
          results_format_name = optarg;
        }
        break;

      case 's':
      case 'D':
        if(optarg) {
          data_filename = optarg;
        }
        break;

      case 't':
        if(optarg) {
          /* Timeout option - set max_search_time for graph comparison */
          if(options.graph_comparison_options) {
            options.graph_comparison_options->max_search_time = atoi(optarg);
            if(options.graph_comparison_options->max_search_time < 0)
              options.graph_comparison_options->max_search_time = 30; /* Default 30 seconds */
          }
        }
        break;

      case 'u':
        diff_format = (const char*)"unified";
        break;

      case 'v':
        verbose = 1;
        break;

      case 'V':
        fputs(rasqal_version_string, stdout);
        fputc('\n', stdout);
        rasqal_free_world(world);
        exit(0);

      case 'w':
        if(optarg)
          warning_level = atoi(optarg);
        if(warning_level >= 0)
          rasqal_world_set_warning_level(world, (unsigned int)warning_level);
        break;

      case 'x':
        diff_format = (const char*)"xml";
        break;

      case 'k':
        diff_format = (const char*)"debug";
        break;

      case 'S':
        if(optarg) {
          /* Signature threshold option */
          if(options.graph_comparison_options) {
            options.graph_comparison_options->signature_threshold = atoi(optarg);
            if(options.graph_comparison_options->signature_threshold < 0)
              options.graph_comparison_options->signature_threshold = 1000; /* Default */
          }
        }
        break;

    }

  }

  if(!help && !usage) {
    /* Validate required arguments */
    if(!expected_filename) {
      fprintf(stderr, "%s: Expected results file (-e) is required\n", program);
      usage = 1;
    }

    if(!actual_filename && !query_filename) {
      fprintf(stderr, "%s: Either actual results file (-a) or query file (-q) is required\n", program);
      usage = 1;
    }

    if(actual_filename && query_filename) {
      fprintf(stderr, "%s: Cannot specify both actual results file (-a) and query file (-q)\n", program);
      usage = 1;
    }
  }

  if(usage) {
    fprintf(stderr, "Try `%s -h' for more information.\n", program);
    rasqal_free_world(world);
    return(2);
  }

  if(help) {
    print_help(world, raptor_world_ptr);
    rasqal_free_world(world);
    return(0);
  }

  /* Load data graphs if provided */
  if(data_filename || named_graph_filename) {
    data_graphs = raptor_new_sequence((raptor_data_free_handler)rasqal_free_data_graph, NULL);
    if(!data_graphs) {
      fprintf(stderr, "%s: Failed to create data graphs sequence\n", program);
      rc = 2;
      goto tidy;
    }

    if(data_filename) {
      rasqal_data_graph* dg = rasqal_cmdline_read_data_graph(world,
                                                             RASQAL_DATA_GRAPH_BACKGROUND,
                                                             data_filename, data_format_name);
      if(!dg) {
        fprintf(stderr, "%s: Failed to create data graph for %s\n", program, data_filename);
        rc = 2;
        goto tidy;
      }
      raptor_sequence_push(data_graphs, dg);
    }

    if(named_graph_filename) {
      rasqal_data_graph* dg = rasqal_cmdline_read_data_graph(world,
                                                             RASQAL_DATA_GRAPH_NAMED,
                                                             named_graph_filename, data_format_name);
      if(!dg) {
        fprintf(stderr, "%s: Failed to create named graph for %s\n", program, named_graph_filename);
        rc = 2;
        goto tidy;
      }
      raptor_sequence_push(data_graphs, dg);
    }
  }

  /* Load expected results */
  expected_results = load_query_results_from_file(world, expected_filename, results_format_name);
  if(!expected_results) {
    rc = 2;
    goto tidy;
  }

  /* Get actual results */
  if(actual_filename) {
    /* Load from file */
    actual_results = load_query_results_from_file(world, actual_filename, results_format_name);
    if(!actual_results) {
      rc = 2;
      goto tidy;
    }
  } else if(query_filename) {
    /* Execute query */
    actual_results = execute_query(world, query_filename, data_graphs, query_language);
    if(!actual_results) {
      rc = 2;
      goto tidy;
    }
  }

  /* Create comparison context */
  compare = rasqal_new_query_results_compare(world, expected_results, actual_results);
  if(!compare) {
    fprintf(stderr, "%s: Failed to create comparison context\n", program);
    rc = 2;
    goto tidy;
  }

  /* Set comparison options */
  if(rasqal_query_results_compare_set_options(compare, &options)) {
    fprintf(stderr, "%s: Failed to set comparison options\n", program);
    rc = 2;
    goto tidy;
  }

  /* Execute comparison */
  result = rasqal_query_results_compare_execute(compare);
  if(!result) {
    fprintf(stderr, "%s: Failed to execute comparison\n", program);
    rc = 2;
    goto tidy;
  }

  /* Output results */
  if(verbose) {
    printf("Results are %s\n", result->equal ? "equal" : "different");
    if(result->differences_count > 0) {
      printf("Found %d differences\n", result->differences_count);
    }
  }

  /* Print diff output if there are differences */
  if(!result->equal && result->differences_count > 0) {
    print_diff_output(result, diff_format, context_lines);
  }

  /* Set exit code */
  if(error_count) {
    rc = 2;
  } else if(result->equal) {
    rc = 0;  /* Equal */
  } else {
    rc = 1;  /* Different */
  }

 tidy:
  if(result)
    rasqal_free_query_results_compare_result(result);
  if(compare)
    rasqal_free_query_results_compare(compare);
  if(actual_results)
    rasqal_free_query_results(actual_results);
  if(expected_results)
    rasqal_free_query_results(expected_results);
  if(data_graphs)
    raptor_free_sequence(data_graphs);
  if(options.graph_comparison_options)
    free(options.graph_comparison_options);
  rasqal_free_world(world);

  return rc;
}

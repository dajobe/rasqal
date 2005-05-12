/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * roqet.c - Rasqal RDF Query test program
 *
 * $Id$
 *
 * Copyright (C) 2004-2005, David Beckett http://purl.org/net/dajobe/
 * Institute for Learning and Research Technology http://www.ilrt.bristol.ac.uk/
 * University of Bristol, UK http://www.bristol.ac.uk/
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
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
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

#ifdef NEED_OPTIND_DECLARATION
extern int optind;
extern char *optarg;
#endif

int rdql_parser_error(const char *msg);
int main(int argc, char *argv[]);


static char *program=NULL;


static enum {
  RESULTS_FORMAT_SIMPLE,
  RESULTS_FORMAT_XML_V1,
  RESULTS_FORMAT_XML_V2
} results_format = RESULTS_FORMAT_SIMPLE;


int
rdql_parser_error(const char *msg) 
{
  fprintf(stderr, "%s: Query parsing error: %s\n", program, msg);
  return (0);
}


#ifdef HAVE_GETOPT_LONG
#define HELP_TEXT(short, long, description) "  -" short ", --" long "  " description
#define HELP_ARG(short, long) "--" #long
#else
#define HELP_TEXT(short, long, description) "  -" short "  " description
#define HELP_ARG(short, long) "-" #short
#endif


#define GETOPT_STRING "cdf:ho:r:i:nqs:vw"

#ifdef HAVE_GETOPT_LONG
static struct option long_options[] =
{
  /* name, has_arg, flag, val */
  {"count", 0, 0, 'c'},
  {"dump-query", 0, 0, 'd'},
  {"dryrun", 0, 0, 'n'},
  {"help", 0, 0, 'h'},
  {"format", 1, 0, 'f'}, /* deprecated for -r/--results */
  {"output", 1, 0, 'o'}, /* deprecated for -r/--results */
  {"results", 1, 0, 'r'},
  {"input", 1, 0, 'i'},
  {"quiet", 0, 0, 'q'},
  {"source", 1, 0, 's'},
  {"version", 0, 0, 'v'},
  {"walk-query", 0, 0, 'w'},
  {NULL, 0, 0, 0}
};
#endif


static int error_count=0;

static const char *title_format_string="Rasqal RDF query utility %s\n";

#ifdef BUFSIZ
#define FILE_READ_BUF_SIZE BUFSIZ
#else
#define FILE_READ_BUF_SIZE 1024
#endif

#define MAX_QUERY_ERROR_REPORT_LEN 512


static int
roqet_query_results_print_as_xml(rasqal_query_results *results, FILE *fh,
                                 raptor_uri* base_uri,
                                 int format)
{
  raptor_iostream *iostr;
  raptor_uri* uri;
  
  iostr=raptor_new_iostream_to_file_handle(fh);
  if(!iostr)
    return 1;

  if(format == RESULTS_FORMAT_XML_V1)
    uri=raptor_new_uri((const unsigned char*)"http://www.w3.org/TR/2004/WD-rdf-sparql-XMLres-20041221/");
  else /* RESULTS_FORMAT_XML_V2 */
    uri=raptor_new_uri((const unsigned char*)"http://www.w3.org/2001/sw/DataAccess/rf1/result2");

  rasqal_query_results_write(iostr, results, uri, base_uri);

  raptor_free_uri(uri);

  raptor_free_iostream(iostr);
  
  return 0;
}


static void
roqet_error_handler(void *user_data, 
                    raptor_locator* locator, const char *message) 
{
  fprintf(stderr, "%s: Error - ", program);
  raptor_print_locator(stderr, locator);
  fprintf(stderr, " - %s\n", message);

  error_count++;
}


static const char *spaces="                                                                                  ";

static void
roqet_write_indent(FILE *fh, int indent) 
{
  if(indent > 0)
    fwrite(spaces, sizeof(char), indent, fh);
}

  

static void
roqet_graph_pattern_walk(rasqal_graph_pattern *gp, int gp_index,
                         FILE *fh, int indent) {
  int triple_index=0;
  int flags;
  int seen;
  raptor_sequence *seq;
  
  flags=rasqal_graph_pattern_get_flags(gp);
  
  roqet_write_indent(fh, indent);
  fputs("graph pattern", fh);
  if(gp_index >= 0)
    fprintf(fh, " #%d", gp_index);
  if(flags != 0)
    fprintf(fh, "with flags %d", flags);
  fputs(" {\n", fh);
  
  indent+= 2;

  /* look for triples */
  seen=0;
  while(1) {
    rasqal_triple* t=rasqal_graph_pattern_get_triple(gp, triple_index);
    if(!t)
      break;
    
    if(!seen) {
      roqet_write_indent(fh, indent);
      fputs("triples {\n", fh);
      seen=1;
    }
    roqet_write_indent(fh, indent+2);
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
  seq=rasqal_graph_pattern_get_sub_graph_pattern_sequence(gp);
  if(seq && raptor_sequence_size(seq) > 0) {
    roqet_write_indent(fh, indent);
    fprintf(fh, "sub-graph patterns (%d) {\n", raptor_sequence_size(seq));

    gp_index=0;
    while(1) {
      rasqal_graph_pattern* sgp=rasqal_graph_pattern_get_sub_graph_pattern(gp, gp_index);
      if(!sgp)
        break;
      
      roqet_graph_pattern_walk(sgp, gp_index, fh, indent+2);
      gp_index++;
    }

    roqet_write_indent(fh, indent);
    fputs("}\n", fh);
  }
  

  /* look for constraints */
  seq=rasqal_graph_pattern_get_constraint_sequence(gp);
  if(seq && raptor_sequence_size(seq) > 0) {
    roqet_write_indent(fh, indent);
    fprintf(fh, "constraints (%d) {\n", raptor_sequence_size(seq));

    gp_index=0;
    while(1) {
      rasqal_expression* expr=rasqal_graph_pattern_get_constraint(gp, gp_index);
      if(!expr)
        break;
      
      roqet_write_indent(fh, indent+2);
      fprintf(fh, "constraint #%d { ", gp_index);
      rasqal_expression_print(expr, fh);
      fputs("}\n", fh);
      
      gp_index++;
    }

    roqet_write_indent(fh, indent);
    fputs("}\n", fh);
  }
  

  indent-=2;
  
  roqet_write_indent(fh, indent);
  fputs("}\n", fh);
}


    
static void
roqet_query_walk(rasqal_query *rq, FILE *fh, int indent) {
  rasqal_query_verb verb;
  int i;
  rasqal_graph_pattern* gp;
  raptor_sequence *seq;

  verb=rasqal_query_get_verb(rq);
  roqet_write_indent(fh, indent);
  fprintf(fh, "query verb: %s\n", rasqal_query_verb_as_string(verb));

  i=rasqal_query_get_distinct(rq);
  if(i != 0) {
    roqet_write_indent(fh, indent);
    fprintf(fh, "query asks for distinct results\n");
  }
  
  i=rasqal_query_get_limit(rq);
  if(i >= 0) {
    roqet_write_indent(fh, indent);
    fprintf(fh, "query asks for result limits %d\n", i);
  }
  
  i=rasqal_query_get_offset(rq);
  if(i >= 0) {
    roqet_write_indent(fh, indent);
    fprintf(fh, "query asks for result offset %d\n", i);
  }
  
  seq=rasqal_query_get_bound_variable_sequence(rq);
  if(seq && raptor_sequence_size(seq) > 0) {
    fprintf(fh, "query bound variables (%d): ", 
            raptor_sequence_size(seq));
    i=0;
    while(1) {
      rasqal_variable* v=(rasqal_variable*)raptor_sequence_get_at(seq, i);
      if(!v)
        break;

      if(i > 0)
        fputs(", ", fh);
      fputs(v->name, fh);

      i++;
    }
    fputc('\n', fh);
  }

  gp=rasqal_query_get_query_graph_pattern(rq);
  if(!gp)
    return;


  seq=rasqal_query_get_construct_triples_sequence(rq);
  if(seq && raptor_sequence_size(seq) > 0) {
    roqet_write_indent(fh, indent);
    fprintf(fh, "query construct triples (%d) {\n", 
            raptor_sequence_size(seq));
    i=0;
    while(1) {
      rasqal_triple* t=rasqal_query_get_construct_triple(rq, i);
      if(!t)
        break;
    
      roqet_write_indent(fh, indent+2);
      fprintf(fh, "triple #%d { ", i);
      rasqal_triple_print(t, fh);
      fputs(" }\n", fh);

      i++;
    }
    roqet_write_indent(fh, indent);
    fputs("}\n", fh);
  }

  fputs("query ", fh);
  roqet_graph_pattern_walk(gp, -1, fh, indent);
}



int
main(int argc, char *argv[]) 
{ 
  void *query_string=NULL;
  unsigned char *uri_string=NULL;
  int free_uri_string=0;
  unsigned char *base_uri_string=NULL;
  rasqal_query *rq;
  rasqal_query_results *results;
  char *ql_name="rdql";
  char *ql_uri=NULL;
  int rc=0;
  raptor_uri *uri=NULL;
  raptor_uri *base_uri=NULL;
  char *filename=NULL;
  char *p;
  int usage=0;
  int help=0;
  int quiet=0;
  int count=0;
  int dump_query=0;
  int dryrun=0;
  int walk_query=0;
  raptor_sequence* source_uris=NULL;
  raptor_serializer* serializer=NULL;
  const char *serializer_syntax_name="ntriples";

  program=argv[0];
  if((p=strrchr(program, '/')))
    program=p+1;
  else if((p=strrchr(program, '\\')))
    program=p+1;
  argv[0]=program;
  

  rasqal_init();

  while (!usage && !help)
  {
    int c;
    raptor_uri *source_uri;
    
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
        usage=1;
        break;
        
      case 'c':
        count=1;
        break;

      case 'd':
        dump_query=1;
        break;

      case 'h':
        help=1;
        break;

      case 'n':
        dryrun=1;
        break;

      case 'f':
      case 'o':
        fprintf(stderr,
                "%s: %s has been deprecated. Please use %s to set the results format\n",
                program,
                ((c == 'f') ? HELP_ARG(f, format) : HELP_ARG(o, output)), 
                HELP_ARG(r, results));
        /* FALLTHROUGH */

      case 'r':
        if(optarg) {
          if(!strcmp(optarg, "simple"))
            results_format=RESULTS_FORMAT_SIMPLE;
          else if(!strcmp(optarg, "xml"))
            results_format=RESULTS_FORMAT_XML_V2;
          else if(!strcmp(optarg, "xml-v1"))
            results_format=RESULTS_FORMAT_XML_V1;
          else if(raptor_serializer_syntax_name_check(optarg))
            serializer_syntax_name=optarg;
          else {
            fprintf(stderr, 
                    "%s: invalid argument `%s' for `" HELP_ARG(r, results) "'\n",
                    program, optarg);
            usage=1;
            break;
            
          }
        }
        break;

      case 'i':
	if(rasqal_language_name_check(optarg))
          ql_name=optarg;
	else {
          int i;
          fprintf(stderr,
                  "%s: invalid argument `%s' for `" HELP_ARG(i, input) "'\n",
                  program, optarg);
          fprintf(stderr, "Valid arguments are:\n");
          for(i=0; 1; i++) {
            const char *help_name;
            const char *help_label;
            if(rasqal_languages_enumerate(i, &help_name, &help_label, NULL))
              break;
            printf("  %-12s for %s\n", help_name, help_label);
          }
          usage=1;
	}
        break;

      case 'q':
        quiet=1;
        break;

      case 's':
        if(!source_uris) {
          source_uris=raptor_new_sequence((raptor_sequence_free_handler*)raptor_free_uri,
                                          (raptor_sequence_print_handler*)raptor_sequence_print_uri);
          if(!source_uris) {
            fprintf(stderr, "%s: Failed to create source sequence\n", program);
          return(1);
          }
        }
        
        if(!access((const char*)optarg, R_OK)) {
          unsigned char* source_uri_string=raptor_uri_filename_to_uri_string((const char*)optarg);
          source_uri=raptor_new_uri(source_uri_string);
          raptor_free_memory(source_uri_string);
        } else
          source_uri=raptor_new_uri((const unsigned char*)optarg);

        if(!source_uri) {
          fprintf(stderr, "%s: Failed to create source URI for %s\n",
                  program, optarg);
          return(1);
        }
        raptor_sequence_push(source_uris, source_uri);
        break;

      case 'v':
        fputs(rasqal_version_string, stdout);
        fputc('\n', stdout);
        exit(0);

      case 'w':
        walk_query=1;
        break;

    }
    
  }

  if(optind != argc-1 && optind != argc-2 && !help && !usage) {
    usage=2; /* Title and usage */
  }

  
  if(usage) {
    if(usage>1) {
      fprintf(stderr, title_format_string, rasqal_version_string);
      fputs(rasqal_short_copyright_string, stderr);
      fputc('\n', stderr);
    }
    fprintf(stderr, "Try `%s " HELP_ARG(h, help) "' for more information.\n",
                    program);
    exit(1);
  }

  if(help) {
    int i;
    
    printf("Usage: %s [OPTIONS] <query URI> [base URI]\n", program);
    printf(title_format_string, rasqal_version_string);
    puts(rasqal_short_copyright_string);
    puts("Run an RDF query giving variable bindings or RDF triples.");
    puts("\nMain options:");
    puts(HELP_TEXT("h", "help            ", "Print this help, then exit"));
    puts(HELP_TEXT("i", "input LANGUAGE  ", "Set query language name to one of:"));
    for(i=0; 1; i++) {
      const char *help_name;
      const char *help_label;
      if(rasqal_languages_enumerate(i, &help_name, &help_label, NULL))
        break;
      printf("    %-15s         %s", help_name, help_label);
      if(!i)
        puts(" (default)");
      else
        putchar('\n');
    }
    puts(HELP_TEXT("r", "results FORMAT  ", "Set query results format to one of:"));
    puts("    For variable bindings and boolean results:");
    puts("      simple                A simple text format (default)");
    puts("      xml                   SPARQL Query Results XML format V2");
    puts("      xml-v1                SPARQL Query Results XML format V1");
    puts("    For RDF graph results:");
    for(i=0; 1; i++) {
      const char *help_name;
      const char *help_label;
      if(raptor_serializers_enumerate(i, &help_name, &help_label, NULL, NULL))
        break;
      printf("      %-15s       %s", help_name, help_label);
      if(!i)
        puts(" (default)");
      else
        putchar('\n');
    }
    puts("\nAdditional options:");
    puts(HELP_TEXT("c", "count           ", "Count triples - no output"));
    puts(HELP_TEXT("d", "dump-query      ", "Dump the parsed query"));
    puts(HELP_TEXT("n", "dryrun          ", "Prepare but do not run the query"));
    puts(HELP_TEXT("q", "quiet           ", "No extra information messages"));
    puts(HELP_TEXT("s", "source URI      ", "Query against RDF data at source URI"));
    puts(HELP_TEXT("v", "version         ", "Print the Rasqal version"));
    puts(HELP_TEXT("w", "walk-query      ", "Walk the prepared query using the API"));
    puts("\nReport bugs to <redland-dev@lists.librdf.org>.");
    puts("Rasqal home page: http://librdf.org/rasqal/");
    exit(0);
  }


  if(optind == argc-1)
    uri_string=(unsigned char*)argv[optind];
  else {
    uri_string=(unsigned char*)argv[optind++];
    base_uri_string=(unsigned char*)argv[optind];
  }

  /* If uri_string is "path-to-file", turn it into a file: URI */
  if(!strcmp((const char*)uri_string, "-")) {
    if(!base_uri_string) {
      fprintf(stderr, "%s: A Base URI is required when reading from standard input.\n",
              program);
      return(1);
    }
    uri_string=NULL;
  } else if(!access((const char*)uri_string, R_OK)) {
    filename=(char*)uri_string;
    uri_string=raptor_uri_filename_to_uri_string(filename);
    free_uri_string=1;
  }

  if(uri_string) {
    uri=raptor_new_uri(uri_string);
    if(!uri) {
      fprintf(stderr, "%s: Failed to create URI for %s\n",
              program, uri_string);
      return(1);
    }
  } else
    uri=NULL; /* stdin */


  if(!base_uri_string) {
    base_uri=raptor_uri_copy(uri);
  } else {
    base_uri=raptor_new_uri(base_uri_string);
    if(!base_uri) {
      fprintf(stderr, "%s: Failed to create URI for %s\n",
              program, base_uri_string);
      return(1);
    }
  }

  if(!uri_string) {
    query_string=calloc(FILE_READ_BUF_SIZE, 1);
    fread(query_string, FILE_READ_BUF_SIZE, 1, stdin);
  } else if(filename) {
    raptor_stringbuffer *sb=raptor_new_stringbuffer();
    size_t len;
    FILE *fh;

    fh=fopen(filename, "r");
    if(!fh) {
      fprintf(stderr, "%s: file '%s' open failed - %s", 
              program, filename, strerror(errno));
      rc=1;
      goto tidy_setup;
    }
    
    while(!feof(fh)) {
      unsigned char buffer[FILE_READ_BUF_SIZE];
      size_t read_len;
      read_len=fread((char*)buffer, 1, FILE_READ_BUF_SIZE, fh);
      if(read_len > 0)
        raptor_stringbuffer_append_counted_string(sb, buffer, read_len, 1);
      if(read_len < FILE_READ_BUF_SIZE)
        break;
    }
    fclose(fh);

    len=raptor_stringbuffer_length(sb);
    query_string=malloc(len+1);
    raptor_stringbuffer_copy_to_string(sb, (unsigned char*)query_string, len);
    raptor_free_stringbuffer(sb);
  } else {
    raptor_www *www=raptor_www_new();
    if(www) {
      raptor_www_set_error_handler(www, roqet_error_handler, NULL);
      raptor_www_fetch_to_string(www, uri, &query_string, NULL, malloc);
      raptor_www_free(www);
    }
    if(!query_string || error_count) {
      fprintf(stderr, "%s: Retrieving query at URI '%s' failed\n", 
              program, uri_string);
      rc=1;
      goto tidy_setup;
    }
  }


  if(!quiet) {
    if (filename) {
      if(base_uri_string)
        fprintf(stderr, "%s: Querying from file %s with base URI %s\n", program,
                filename, base_uri_string);
      else
        fprintf(stderr, "%s: Querying from file %s\n", program, filename);
    } else {
      if(base_uri_string)
        fprintf(stderr, "%s: Querying URI %s with base URI %s\n", program,
                uri_string, base_uri_string);
      else
        fprintf(stderr, "%s: Querying URI %s\n", program, uri_string);
    }
  }
  
  rq=rasqal_new_query((const char*)ql_name, (const unsigned char*)ql_uri);
  rasqal_query_set_error_handler(rq, NULL, roqet_error_handler);
  rasqal_query_set_fatal_error_handler(rq, NULL, roqet_error_handler);
  
  if(source_uris) {
    while(raptor_sequence_size(source_uris)) {
      raptor_uri* source_uri=(raptor_uri*)raptor_sequence_pop(source_uris);
      if(rasqal_query_add_data_graph(rq, source_uri, source_uri, 
                                     RASQAL_DATA_GRAPH_NAMED)) {
        fprintf(stderr, "%s: Failed to add named graph %s\n", program, 
                raptor_uri_as_string(source_uri));
      }
      raptor_free_uri(source_uri);
    }
  }

  if(rasqal_query_prepare(rq, (const unsigned char*)query_string, base_uri)) {
    size_t len=strlen((const char*)query_string);
    
    fprintf(stderr, "%s: Parsing query '", program);
    if(len > MAX_QUERY_ERROR_REPORT_LEN) {
      fwrite(query_string, MAX_QUERY_ERROR_REPORT_LEN, sizeof(char), stderr);
      fprintf(stderr, "...' (%d bytes) failed\n", (int)len);
    } else {
      fwrite(query_string, len, sizeof(char), stderr);
      fputs("' failed\n", stderr);
    }
    rc=1;
    goto tidy_query;
  }

  if(walk_query) {
    fprintf(stderr, "%s: walking query structure\n", program);
    roqet_query_walk(rq, stdout, 0);
  }
  
  if(dump_query) {
    fprintf(stderr, "Query:\n");
    rasqal_query_print(rq, stdout);
  }

  if(dryrun)
    goto tidy_query;

  if(!(results=rasqal_query_execute(rq))) {
    fprintf(stderr, "%s: Query execution failed\n", program);
    rc=1;
    goto tidy_query;
  }

  if(results_format == RESULTS_FORMAT_XML_V1 ||
     results_format == RESULTS_FORMAT_XML_V2) {
    roqet_query_results_print_as_xml(results, stdout, base_uri, 
                                     results_format);
  } else {
    if(rasqal_query_results_is_bindings(results)) {
      if(!quiet)
        fprintf(stdout, "%s: Query has a variable bindings result\n", program);

      while(!rasqal_query_results_finished(results)) {
        if(!count) {
          int i;
          
          fputs("result: [", stdout);
          for(i=0; i<rasqal_query_results_get_bindings_count(results); i++) {
            const unsigned char *name=rasqal_query_results_get_binding_name(results, i);
            rasqal_literal *value=rasqal_query_results_get_binding_value(results, i);
            
            if(i>0)
              fputs(", ", stdout);
            fprintf(stdout, "%s=", name);
            if(value)
              rasqal_literal_print(value, stdout);
            else
              fputs("NULL", stdout);
          }
          fputs("]\n", stdout);
        }
        
        rasqal_query_results_next(results);
      }

      if(!quiet)
        fprintf(stdout, "%s: Query returned %d results\n", program, 
                rasqal_query_results_get_count(results));

    } else if (rasqal_query_results_is_boolean(results)) {
      fprintf(stdout, "%s: Query has a boolean result: %s\n",
              program,
              rasqal_query_results_get_boolean(results) ? "true" : "false");
    }
    else if (rasqal_query_results_is_graph(results)) {
      int triple_count=0;
      
      if(!quiet)
        fprintf(stdout, "%s: Query has a graph result:\n", program);

      serializer=raptor_new_serializer(serializer_syntax_name);
      if(!serializer) {
        fprintf(stderr, 
                "%s: Failed to create raptor serializer type %s\n", program,
                serializer_syntax_name);
        return(1);
      }

      raptor_serialize_start_to_file_handle(serializer, base_uri, stdout);

      while(1) {
        raptor_statement *rs=rasqal_query_results_get_triple(results);
        if(!rs)
          break;
        raptor_serialize_statement(serializer, rs);
        triple_count++;

        if(rasqal_query_results_next_triple(results))
          break;
      }

      raptor_serialize_end(serializer);
      raptor_free_serializer(serializer);

      if(!quiet)
        fprintf(stdout, "%s: Total %d triples\n", program, triple_count);
    } else {
      fprintf(stdout, "%s: Query returned unknown result format\n", program);
      rc=1;
    }
  }

  rasqal_free_query_results(results);
  
 tidy_query:  
  rasqal_free_query(rq);

  free(query_string);

 tidy_setup:

  if(source_uris)
    raptor_free_sequence(source_uris);
  if(base_uri)
    raptor_free_uri(base_uri);
  if(uri)
    raptor_free_uri(uri);
  if(free_uri_string)
    raptor_free_memory(uri_string);

  rasqal_finish();
  
  return (rc);
}

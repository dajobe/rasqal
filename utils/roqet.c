/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * roqet.c - Rasqal RDF Query test program
 *
 * $Id$
 *
 * Copyright (C) 2004 David Beckett - http://purl.org/net/dajobe/
 * Institute for Learning and Research Technology - http://www.ilrt.bris.ac.uk/
 * University of Bristol - http://www.bristol.ac.uk/
 * 
 * This package is Free Software or Open Source available under the
 * following licenses (these are alternatives):
 *   1. GNU Lesser General Public License (LGPL)
 *   2. GNU General Public License (GPL)
 *   3. Mozilla Public License (MPL)
 * 
 * See LICENSE.html or LICENSE.txt at the top of this package for the
 * full license terms.
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
#include <unistd.h>

/* Rasqal includes */
#include <rasqal.h>

#ifdef NEED_OPTIND_DECLARATION
extern int optind;
extern char *optarg;
#endif

int rdql_parser_error(const char *msg);
int main(int argc, char *argv[]);


static char *program=NULL;


static enum { OUTPUT_FORMAT_SIMPLE } output_format = OUTPUT_FORMAT_SIMPLE;


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


#define GETOPT_STRING "cho:i:qv"

#ifdef HAVE_GETOPT_LONG
static struct option long_options[] =
{
  /* name, has_arg, flag, val */
  {"count", 0, 0, 'c'},
  {"help", 0, 0, 'h'},
  {"output", 1, 0, 'o'},
  {"input", 1, 0, 'i'},
  {"quiet", 0, 0, 'q'},
  {"version", 0, 0, 'v'},
  {NULL, 0, 0, 0}
};
#endif


static const char *title_format_string="Rasqal RDF query utility %s\n";


#define RDQL_FILE_BUF_SIZE 2048

int
main(int argc, char *argv[]) 
{ 
  char *query_string=NULL;
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

      case 'h':
        help=1;
        break;

      case 'o':
        if(optarg) {
          if(!strcmp(optarg, "simple"))
            output_format=OUTPUT_FORMAT_SIMPLE;
          else {
            fprintf(stderr, "%s: invalid argument `%s' for `" HELP_ARG(o, output) "'\n",
                    program, optarg);
            fprintf(stderr, "Valid arguments are:\n  `simple'   for a simple format (default)\n");
            usage=1;
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

      case 'v':
        fputs(rasqal_version_string, stdout);
        fputc('\n', stdout);
        exit(0);
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
      printf("    %-12s            %s", help_name, help_label);
      if(!i)
        puts(" (default)");
      else
        putchar('\n');
    }
    puts(HELP_TEXT("o", "output FORMAT   ", "Set output format to one of:"));
    puts("    'simple'                A simple format (default)");
    puts("\nAdditional options:");
    puts(HELP_TEXT("c", "count           ", "Count triples - no output"));
    puts(HELP_TEXT("q", "quiet           ", "No extra information messages"));
    puts(HELP_TEXT("v", "version         ", "Print the Rasqal version"));
    puts("\nReport bugs to <redland-dev@lists.librdf.org>.");
    puts("Rasqal home page: http://www.redland.opensource.ac.uk/rasqal/");
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


  query_string=(char*)calloc(RDQL_FILE_BUF_SIZE, 1);
  if(!uri_string) {
    fread(query_string, RDQL_FILE_BUF_SIZE, 1, stdin);
  } else if(filename) {
    FILE *fh=fopen(filename, "r");
    if(!fh) {
      fprintf(stderr, "%s: file '%s' open failed - %s", 
              program, filename, strerror(errno));
      return(1);
    }
    fread(query_string, RDQL_FILE_BUF_SIZE, 1, fh);
    fclose(fh);
  } else {
    /* FIXME */
    fprintf(stderr, "%s: Sorry, can only read queries from files now.\n",
            program);
    return(1);
  }


  if(!quiet) {
    if (filename) {
      if(base_uri_string)
        fprintf(stdout, "%s: Querying from file %s with base URI %s\n", program,
                filename, base_uri_string);
      else
        fprintf(stdout, "%s: Querying from file %s\n", program, filename);
    } else {
      if(base_uri_string)
        fprintf(stdout, "%s: Querying URI %s with base URI %s\n", program,
                uri_string, base_uri_string);
      else
        fprintf(stdout, "%s: Querying URI %s\n", program, uri_string);
    }
  }
  
  rq=rasqal_new_query((const char*)ql_name, (const unsigned char*)ql_uri);

  if(rasqal_query_prepare(rq, (const unsigned char*)query_string, base_uri)) {
    fprintf(stderr, "%s: Parsing query '%s' failed\n", program, query_string);
    rc=1;
  }

  if(!quiet) {
    fprintf(stdout, "Query:\n");
    rasqal_query_print(rq, stdout);
  }

  if(!(results=rasqal_query_execute(rq))) {
    fprintf(stderr, "%s: Query execution failed\n", program);
    rc=1;
  }

  while(!rasqal_query_results_finished(results)) {
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
    
    rasqal_query_results_next(results);
  }


  if(!quiet)
    fprintf(stdout, "%s: Query returned %d results\n", program, 
            rasqal_query_results_get_count(results));

  rasqal_free_query_results(results);
  
  rasqal_free_query(rq);

  free(query_string);

  if(base_uri)
    raptor_free_uri(base_uri);
  if(uri)
    raptor_free_uri(uri);
  if(free_uri_string)
    free(uri_string);

  rasqal_finish();
  
  return (rc);
}

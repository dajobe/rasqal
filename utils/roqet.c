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
  OUTPUT_FORMAT_SIMPLE,
  OUTPUT_FORMAT_XML
} output_format = OUTPUT_FORMAT_SIMPLE;


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


#define GETOPT_STRING "cho:i:qs:v"

#ifdef HAVE_GETOPT_LONG
static struct option long_options[] =
{
  /* name, has_arg, flag, val */
  {"count", 0, 0, 'c'},
  {"help", 0, 0, 'h'},
  {"output", 1, 0, 'o'},
  {"input", 1, 0, 'i'},
  {"quiet", 0, 0, 'q'},
  {"source", 1, 0, 's'},
  {"version", 0, 0, 'v'},
  {NULL, 0, 0, 0}
};
#endif


static const char *title_format_string="Rasqal RDF query utility %s\n";


#define RDQL_FILE_BUF_SIZE 2048


static void
roqet_print_xml_attribute(FILE *handle,
                          unsigned char *attr, unsigned char *value) 
{
  size_t attr_len;
  size_t len;
  size_t escaped_len;
  unsigned char *buffer;
  unsigned char *p;
  
  attr_len=strlen((const char*)attr);
  len=strlen((const char*)value);

  escaped_len=raptor_xml_escape_string(value, len,
                                       NULL, 0, '"',
                                       NULL, NULL);

  buffer=(unsigned char*)malloc(1 + attr_len + 2 + escaped_len + 1 +1);
  if(!buffer)
    return;
  p=buffer;
  *p++=' ';
  strncpy((char*)p, (const char*)attr, attr_len);
  p+= attr_len;
  *p++='=';
  *p++='"';
  raptor_xml_escape_string(value, len,
                           p, escaped_len, '"',
                           NULL, NULL);
  p+= escaped_len;
  *p++='"';
  *p++='\0';
  
  fputs((const char*)buffer, handle);
  free(buffer);
}



static void
roqet_print_results_as_xml(FILE *fh, rasqal_query_results *results) 
{
  fputs("<results xmlns=\"http://www.w3.org/sw/2001/DataAccess/result1#\">\n\n",
        fh);
  

  while(!rasqal_query_results_finished(results)) {
    int i;

    fputs("  <result>\n", fh);
    for(i=0; i<rasqal_query_results_get_bindings_count(results); i++) {
      const unsigned char *name=rasqal_query_results_get_binding_name(results, i);
      rasqal_literal *l=rasqal_query_results_get_binding_value(results, i);
      int print_end=1;
      size_t len;
      
      if(!l)
        continue;
      
      fputs("    <", fh);
      fputs(name, fh);

      switch(l->type) {
        case RASQAL_LITERAL_URI:
          roqet_print_xml_attribute(fh, "uri",
                                    raptor_uri_as_string(l->value.uri));
          fputs("/>\n", fh);
          print_end=0;
          break;
        case RASQAL_LITERAL_STRING:
          len=strlen(l->string);
          if(!len) {
            fputs("/>\n", fh);
            print_end=0;
            break;
          }
          
          if(l->language)
            roqet_print_xml_attribute(fh, "xml:lang",
                                      (unsigned char *)l->language);
          if(l->datatype)
            roqet_print_xml_attribute(fh, "datatype",
                                      raptor_uri_as_string(l->datatype));
          fputc('>', fh);

          if(l->datatype &&
             !strcmp(raptor_uri_as_string(l->datatype),
                     "http://www.w3.org/1999/02/22-rdf-syntax-ns#XMLLiteral")) {
            fputs(l->string, fh);
          } else {
            int xml_string_len=raptor_xml_escape_string(l->string, len,
                                                        NULL, 0, 0,
                                                        NULL, NULL);
            unsigned char *xml_string=(unsigned char*)malloc(xml_string_len+1);
            
            xml_string_len=raptor_xml_escape_string(l->string, len,
                                                    xml_string, xml_string_len, 0,
                                                    NULL, NULL);
            fputs(xml_string, fh);
            free(xml_string);
          }
          
          break;
        case RASQAL_LITERAL_BLANK:
        case RASQAL_LITERAL_PATTERN:
        case RASQAL_LITERAL_QNAME:
        case RASQAL_LITERAL_INTEGER:
        case RASQAL_LITERAL_BOOLEAN:
        case RASQAL_LITERAL_FLOATING:
        case RASQAL_LITERAL_VARIABLE:
        default:
          fprintf(stderr, "%s: Cannot turn literal type %d '", program, l->type);
          rasqal_literal_print_type(l, stderr);
          fputs("' into XML\n", stderr);
          abort();
      }

      if(print_end) {
        fputs("</", fh);
        fputs(name, fh);
        fputs(">\n", fh);
      }
    }
    fputs("  </result>\n\n", fh);
    
    rasqal_query_results_next(results);
  }

  fputs("</results>\n", fh);

}



int
main(int argc, char *argv[]) 
{ 
  char *query_string=NULL;
  unsigned char *uri_string=NULL;
  int free_uri_string=0;
  unsigned char *base_uri_string=NULL;
  unsigned char *source_uri_string=NULL;
  int free_source_uri_string=0;
  rasqal_query *rq;
  rasqal_query_results *results;
  char *ql_name="rdql";
  char *ql_uri=NULL;
  int rc=0;
  raptor_uri *uri=NULL;
  raptor_uri *base_uri=NULL;
  raptor_uri *source_uri=NULL;
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
          if(!strcmp(optarg, "xml"))
            output_format=OUTPUT_FORMAT_XML;
          else {
            fprintf(stderr, "%s: invalid argument `%s' for `" HELP_ARG(o, output) "'\n",
                    program, optarg);
            fprintf(stderr, "Valid arguments are:\n  `simple'   for a simple format (default)\n  `xml'      for an experimental XML format\n");
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

      case 's':
        source_uri_string=(unsigned char*)optarg;
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
    puts("    'xml'                   An experimental XML format");
    puts("\nAdditional options:");
    puts(HELP_TEXT("c", "count           ", "Count triples - no output"));
    puts(HELP_TEXT("q", "quiet           ", "No extra information messages"));
    puts(HELP_TEXT("s", "source URI      ", "Query against RDF data at source URI"));
    puts(HELP_TEXT("v", "version         ", "Print the Rasqal version"));
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


  fprintf(stderr, "Query URI '%s'\n", raptor_uri_as_string(uri));


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

  if(source_uri_string) {
    if(!access((const char*)source_uri_string, R_OK)) {
      source_uri_string=raptor_uri_filename_to_uri_string(source_uri_string);
      free_source_uri_string=1;
    }
    source_uri=raptor_new_uri(source_uri_string);
    if(!source_uri) {
      fprintf(stderr, "%s: Failed to create source URI for %s\n",
              program, source_uri_string);
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

  if(rasqal_query_prepare(rq, (const unsigned char*)query_string, base_uri)) {
    fprintf(stderr, "%s: Parsing query '%s' failed\n", program, query_string);
    rc=1;
  }

  if(source_uri)
    rasqal_query_add_source(rq, source_uri);

  if(!quiet) {
    fprintf(stderr, "Query:\n");
    rasqal_query_print(rq, stderr);
  }

  if(!(results=rasqal_query_execute(rq))) {
    fprintf(stderr, "%s: Query execution failed\n", program);
    rc=1;
  }

  if(output_format == OUTPUT_FORMAT_XML) 
    roqet_print_results_as_xml(stdout, results);
  else {
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
  }

  if(!quiet)
    fprintf(stderr, "%s: Query returned %d results\n", program, 
            rasqal_query_results_get_count(results));
  
  
  rasqal_free_query_results(results);
  
  rasqal_free_query(rq);

  free(query_string);

  if(source_uri)
    raptor_free_uri(source_uri);
  if(base_uri)
    raptor_free_uri(base_uri);
  if(uri)
    raptor_free_uri(uri);
  if(free_source_uri_string)
    raptor_free_memory(source_uri_string);
  if(free_uri_string)
    raptor_free_memory(uri_string);

  rasqal_finish();
  
  return (rc);
}

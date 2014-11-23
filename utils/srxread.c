/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * srxread.c - SPARQL Results Formats reading test program
 *
 * Copyright (C) 2007-2008, David Beckett http://www.dajobe.org/
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
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif


#include <raptor.h>

/* Rasqal includes */
#include <rasqal.h>

#ifdef RAPTOR_V2_AVAILABLE
#else
#define raptor_new_uri(world, string) raptor_new_uri(string)
#define raptor_new_iostream_from_filename(world, filename) raptor_new_iostream_from_filename(filename)
#define raptor_new_iostream_to_file_handle(world, fh) raptor_new_iostream_to_file_handle(fh)
#endif


static const char *title_string = "Rasqal RDF query results utility";

static char *program = NULL;

int main(int argc, char *argv[]);

int
main(int argc, char *argv[]) 
{ 
  int rc = 0;
  const char* filename = NULL;
  raptor_iostream* iostr = NULL;
  char* p;
  unsigned char* uri_string = NULL;
  raptor_uri* base_uri = NULL;
  rasqal_query_results* results = NULL;
  const char* read_formatter_name = NULL;
  const char* write_formatter_name = NULL;
  rasqal_query_results_formatter* read_formatter = NULL;
  rasqal_query_results_formatter* write_formatter = NULL;
  raptor_iostream *write_iostr = NULL;
  rasqal_world *world;
#ifdef RAPTOR_V2_AVAILABLE
  raptor_world *raptor_world_ptr;
#endif
  
  program = argv[0];
  if((p=strrchr(program, '/')))
    program = p+1;
  else if((p=strrchr(program, '\\')))
    program = p+1;
  argv[0] = program;
  
  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    return(1);
  }

  if(argc < 2 || argc > 4) {
    int i;
    
    puts(title_string); puts(rasqal_version_string); putchar('\n');
    puts("Read an RDF Query results file in one format and print in another\n");
    printf("Usage: %s <results filename> [read format [write format]]\n\n",
           program);
    fputs(rasqal_copyright_string, stdout);
    fputs("\nLicense: ", stdout);
    puts(rasqal_license_string);
    fputs("Rasqal home page: ", stdout);
    puts(rasqal_home_url_string);
    
    puts("\nFormats supported are:");
    for(i = 0; 1; i++) {
      const raptor_syntax_description* desc;
      int need_comma = 0;
      
      desc = rasqal_world_get_query_results_format_description(world, i);
      if(!desc)
        break;

      printf("  %-10s %s (", desc->names[0], desc->label);
      if(desc->flags & RASQAL_QUERY_RESULTS_FORMAT_FLAG_READER) {
        fputs("read", stdout);
        need_comma = 1;
      }

      if(desc->flags & RASQAL_QUERY_RESULTS_FORMAT_FLAG_WRITER) {
        if(need_comma)
          fputs(", ", stdout);
        fputs("write", stdout);
        need_comma = 1;
      }

      if(!i) {
        if(need_comma)
          fputs(", ", stdout);
        fputs("default", stdout);
      }
      fputs(")\n", stdout);
      if(desc->mime_types[0].mime_type) {
        int mimei;
        const char* mt;
        for(mimei = 0; (mt = desc->mime_types[mimei].mime_type); mimei++) {
          fprintf(stdout, "               %s\n", mt);
        }
      }
   }


    rc = 1;
    goto tidy;
  }

  filename = argv[1];
  if(argc > 2) {
    if(strcmp(argv[2], "-"))
      read_formatter_name = argv[2];
    if(argc > 3) {
      if(strcmp(argv[3], "-"))
        write_formatter_name = argv[3];
    }
  }

#ifdef RAPTOR_V2_AVAILABLE
  raptor_world_ptr = rasqal_world_get_raptor(world);
#endif
  
  uri_string = raptor_uri_filename_to_uri_string((const char*)filename);
  if(!uri_string)
    goto tidy;
  
  base_uri = raptor_new_uri(raptor_world_ptr, uri_string);
  raptor_free_memory(uri_string);

  results = rasqal_new_query_results2(world, NULL,
                                      RASQAL_QUERY_RESULTS_BINDINGS);
  if(!results) {
    fprintf(stderr, "%s: Failed to create query results\n", program);
    rc = 1;
    goto tidy;
  }
  
  iostr = raptor_new_iostream_from_filename(raptor_world_ptr, filename);
  if(!iostr) {
    fprintf(stderr, "%s: Failed to open iostream to file %s\n", program,
            filename);
    rc = 1;
    goto tidy;
  }

  read_formatter = rasqal_new_query_results_formatter(world,
                                                      read_formatter_name,
                                                      NULL, NULL);
  if(!read_formatter) {
    fprintf(stderr, "%s: Failed to create query results read formatter '%s'\n",
            program, read_formatter_name);
    rc = 1;
    goto tidy;
  }
  
  rc = rasqal_query_results_formatter_read(world, iostr, read_formatter,
                                           results, base_uri);
  if(rc) {
    fprintf(stderr,
            "%s: Failed to read query results with read formatter '%s'\n",
            program, read_formatter_name);
    goto tidy;
  }
  
  write_formatter = rasqal_new_query_results_formatter(world, 
                                                       write_formatter_name,
                                                       NULL, NULL);
  if(!write_formatter) {
    fprintf(stderr, "%s: Failed to create query results write formatter '%s'\n",
            program, write_formatter_name);
    rc = 1;
    goto tidy;
  }
  
  write_iostr = raptor_new_iostream_to_file_handle(raptor_world_ptr, stdout);
  if(!write_iostr) {
    fprintf(stderr, "%s: Creating output iostream failed\n", program);
  } else {
    rasqal_query_results_formatter_write(write_iostr, write_formatter,
                                         results, base_uri);
    raptor_free_iostream(write_iostr);
  }


  tidy:
  if(write_formatter)
    rasqal_free_query_results_formatter(write_formatter);

  if(read_formatter)
    rasqal_free_query_results_formatter(read_formatter);

  if(iostr)
    raptor_free_iostream(iostr);
  
  if(results)
    rasqal_free_query_results(results);

  if(base_uri)
    raptor_free_uri(base_uri);

  rasqal_free_world(world);

  return (rc);
}

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * convert_graph_pattern.c - Rasqal test program to turn a GP into algebra tree
 *
 * Copyright (C) 2008, David Beckett http://www.dajobe.org/
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
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "rasqal.h"
#include "rasqal_internal.h"


#define QUERY_LANGUAGE "sparql"

#ifdef BUFSIZ
#define FILE_READ_BUF_SIZE BUFSIZ
#else
#define FILE_READ_BUF_SIZE 1024
#endif




int main(int argc, char *argv[]);

static unsigned char buffer[FILE_READ_BUF_SIZE];

static unsigned char*
file_read_string(const char* program, const char* filename, const char* label) 
{
  raptor_stringbuffer *sb=raptor_new_stringbuffer();
  unsigned char* string=NULL;
  size_t len=0;
  FILE *fh;
  int rc=0;
  
  fh=fopen(filename, "r");
  if(!fh) {
    fprintf(stderr, "%s: Failed to read %s file '%s' open failed - %s", 
            program, label, filename, strerror(errno));
    rc=1;
    goto tidy;
  }
  
  while(!feof(fh)) {
    size_t read_len;
    read_len=fread((char*)buffer, 1, FILE_READ_BUF_SIZE, fh);
    if(read_len > 0)
      raptor_stringbuffer_append_counted_string(sb, buffer, read_len, 1);
    if(read_len < FILE_READ_BUF_SIZE) {
      if(ferror(fh)) {
        fprintf(stderr, "%s: file '%s' read failed - %s\n",
                program, filename, strerror(errno));
        rc=1;
        goto tidy;
      }
      break;
    }
  }
  fclose(fh); fh=NULL;
  
  len=raptor_stringbuffer_length(sb);
  string=(unsigned char*)malloc(len+1);
  raptor_stringbuffer_copy_to_string(sb, (unsigned char*)string, len);

  tidy:
  if(sb)
    raptor_free_stringbuffer(sb);
  if(fh)
    fclose(fh);

  return rc ? NULL : string;
}



int
main(int argc, char *argv[])
{
  char const *program=rasqal_basename(*argv);
  const char *query_language_name=QUERY_LANGUAGE;
  int failures = 0;
#define FAIL do { failures++; goto tidy; } while(0)
  rasqal_world *world;
  rasqal_query* query = NULL;
  raptor_uri *base_uri = NULL;
  char *query_file;
  unsigned char *query_string = NULL;
  raptor_iostream* iostr = NULL;
  rasqal_algebra_node* node = NULL;

  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    return(1);
  }
  
  if(argc != 3) {
    fprintf(stderr, "%s: USAGE SPARQL-FILE BASE-URI\n", program);
    return(1);
  }
  

  query_file = argv[1];
  base_uri = raptor_new_uri(world->raptor_world_ptr,
                            (const unsigned char*)argv[2]);
  query = rasqal_new_query(world, query_language_name, NULL);
  if(!query) {
    fprintf(stderr, "%s: creating query in language %s FAILED\n", program,
            query_language_name);
    FAIL;
  }

  query_string = file_read_string(program, query_file, "query");
  if(!query_string) {
    FAIL;
  }

  if(rasqal_query_prepare(query, query_string, base_uri)) {
    fprintf(stderr, "%s: %s query prepare FAILED\n", program, 
            query_language_name);
    FAIL;
  }

  node = rasqal_algebra_query_to_algebra(query);
  if(!node) {
    fprintf(stderr, "%s: Failed to make algebra node\n", program);
    FAIL;
  }

  node = rasqal_algebra_query_add_orderby(query, node, query->projection,
                                          query->modifier);
  if(!node) {
    fprintf(stderr, "%s: Failed to add algebra modifiers\n", program);
    FAIL;
  }

  if(query->verb == RASQAL_QUERY_VERB_SELECT) {
    node = rasqal_algebra_query_add_projection(query, node, query->projection);
    if(!node) {
      fprintf(stderr, "%s: Failed to add algebra projection\n", program);
    FAIL;
    }
  } else if (query->verb == RASQAL_QUERY_VERB_CONSTRUCT) {
    node = rasqal_algebra_query_add_construct_projection(query, node);
    if(!node) {
      fprintf(stderr, "%s: Failed to add algebra construct projection\n",
              program);
    FAIL;
    }
  }

  node = rasqal_algebra_query_add_distinct(query, node, query->projection);
  if(!node) {
    fprintf(stderr, "%s: Failed to add algebra distinct\n", program);
    FAIL;
  }

  
  iostr = raptor_new_iostream_to_file_handle(world->raptor_world_ptr, stdout);
  if(!iostr) {
    fprintf(stderr, "%s: Failed to make iostream\n", program);
    FAIL;
  }
  
  rasqal_algebra_algebra_node_write(node, iostr);
  raptor_iostream_write_byte('\n', iostr);
  raptor_free_iostream(iostr); iostr = NULL;

  rasqal_free_algebra_node(node); node = NULL;
  
  rasqal_free_memory(query_string); query_string = NULL;

  tidy:
  if(node)
    rasqal_free_algebra_node(node);
  if(iostr)
    raptor_free_iostream(iostr);
  if(query)
    rasqal_free_query(query);
  if(base_uri)
    raptor_free_uri(base_uri);
  if(world)
    rasqal_free_world(world);
  
  return failures;
}

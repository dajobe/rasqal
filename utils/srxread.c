/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * srxread.c - SRX Read test program
 *
 * Copyright (C) 2007, David Beckett http://purl.org/net/dajobe/
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


#define RAPTOR_INTERNAL
#include <raptor.h>
#include <raptor_internal.h>

/* Rasqal includes */
#include <rasqal.h>
#include <rasqal_internal.h>


int main(int argc, char *argv[]);


static char *program=NULL;


#ifdef BUFSIZ
#define FILE_READ_BUF_SIZE BUFSIZ
#else
#define FILE_READ_BUF_SIZE 1024
#endif


const char* const element_names[]=
{
  "!",
  /* In rough order they appear */
  "sparql",
  "head",
  "binding",
  "variable",
  "results",
  "result",
  "blank",
  "literal",
  "uri",
  NULL
};
  

typedef enum
{
  STATE_unknown,
  /* In same order as above */
  STATE_sparql,
  STATE_head,
  STATE_binding,
  STATE_variable,
  STATE_results,
  STATE_result,
  STATE_blank,
  STATE_literal,
  STATE_uri,
  STATE_first = STATE_sparql,
  STATE_last = STATE_uri
} srxread_state;
  

typedef struct 
{
  raptor_locator locator;
  srxread_state state;
  char* name;  /* variable name (from binding/@name) */
  char* value; /* URI string, literal string or blank node ID */
  char* datatype; /* literal datatype URI string from literal/@datatype */
  char* language; /* literal language from literal/@language */
  int failed;
  int depth;
  int trace;
  rasqal_query_results* results;
  int offset; /* current index into results */
  rasqal_query_result_row* row;
  int variables_count;
} srxread_userdata;
  

static void
pad(FILE* fh, int depth)
{
  int i;
  for(i=0; i< depth; i++)
    fputs("  ", fh);
}


static void
srxread_raptor_sax2_start_element_handler(void *user_data,
                                          raptor_xml_element *xml_element)
{
  srxread_userdata* ud=(srxread_userdata*)user_data;
  int i;
  raptor_qname* name;
  srxread_state state=STATE_unknown;
  int attr_count;

  name=raptor_xml_element_get_name(xml_element);

  for(i=STATE_first; i <= STATE_last; i++) {
    if(!strcmp((const char*)name->local_name, element_names[i])) {
      state=(srxread_state)i;
      ud->state=state;
    }
  }

  if(state == STATE_unknown) {
    fprintf(stderr, "UNKNOWN element %s\n", name->local_name);
    ud->failed++;
  }

  if(ud->trace) {
    pad(stderr, ud->depth);
    fprintf(stderr, "Element %s (%d)\n", name->local_name, state);
  }
  
  attr_count=raptor_xml_element_get_attributes_count(xml_element);
  if(attr_count > 0) {
    raptor_qname** attrs=raptor_xml_element_get_attributes(xml_element);
    for(i=0; i < attr_count; i++) {
      if(ud->trace) {
        pad(stderr, ud->depth+1);
        fprintf(stderr, "Attribute %s='%s'\n", attrs[i]->local_name,
                attrs[i]->value);
      }
    }
  }

  switch(state) {
    case STATE_sparql:
      break;
      
    case STATE_head:
      ud->variables_count=0;
      break;
      
    case STATE_variable:
      ud->variables_count++;
      break;
      
    case STATE_results:
      ud->results=rasqal_new_query_results(NULL);
      rasqal_query_results_set_variables(ud->results, NULL, 
                                         ud->variables_count);
      break;
      
    case STATE_result:
      ud->row=rasqal_new_query_result_row(ud->results);
      ud->offset++;
      break;
      
    case STATE_unknown:
    case STATE_binding:
    case STATE_blank:
    case STATE_literal:
    case STATE_uri:
    default:
      break;
  }
  
  ud->depth++;
}


static void
srxread_raptor_sax2_end_element_handler(void *user_data,
                                        raptor_xml_element* xml_element)
{
  srxread_userdata* ud=(srxread_userdata*)user_data;
  raptor_qname* name;

  name=raptor_xml_element_get_name(xml_element);

  ud->depth--;
  if(ud->trace) {
    pad(stderr, ud->depth);
    fprintf(stderr, "End Element %s (%d)\n", name->local_name, ud->state);
  }
}


static void
srxread_raptor_sax2_characters_handler(void *user_data,
                                       raptor_xml_element* xml_element,
                                       const unsigned char *s, int len)
{
  srxread_userdata* ud=(srxread_userdata*)user_data;

  if(ud->trace) {
    pad(stderr, ud->depth);
    fputs("Text '", stderr);
    fwrite(s, sizeof(char), len, stderr);
    fprintf(stderr, "' (%d bytes)\n", len);
  }
}


int
main(int argc, char *argv[]) 
{ 
  int rc=0;
  const char* srx_filename=NULL;
  FILE *fh;
  char* p;
  raptor_sax2* sax2=NULL;
  srxread_userdata ud; /* static */
  raptor_error_handlers error_handlers; /* static */
  unsigned char* uri_string=NULL;
  raptor_uri* base_uri=NULL;
  
  program=argv[0];
  if((p=strrchr(program, '/')))
    program=p+1;
  else if((p=strrchr(program, '\\')))
    program=p+1;
  argv[0]=program;
  

  rasqal_init();

  if(argc != 2) {
    fprintf(stderr, "USAGE: %s SRX file\n", program);

    rc=1;
    goto tidy;
  }

  memset(&ud, sizeof(srxread_userdata), '\0');
  ud.state=STATE_unknown;
  ud.results=rasqal_new_query_results(NULL);
  
  srx_filename=argv[1];

  uri_string=raptor_uri_filename_to_uri_string((const char*)srx_filename);
  if(!uri_string)
    goto tidy;
  
  base_uri=raptor_new_uri(uri_string);
  raptor_free_memory(uri_string);

  ud.locator.uri=base_uri;

  memset(&error_handlers, sizeof(raptor_error_handlers), '\0');
  raptor_error_handlers_init(&error_handlers, 
                             NULL, NULL, /* fatal error data/handler */
                             NULL, NULL, /* error data/handler */
                             NULL, NULL, /* warning data/handler */
                             &ud.locator);
  
  sax2=raptor_new_sax2(&ud, &error_handlers);

  raptor_sax2_set_start_element_handler(sax2,
                                        srxread_raptor_sax2_start_element_handler);
  raptor_sax2_set_characters_handler(sax2,
                                     srxread_raptor_sax2_characters_handler);
  raptor_sax2_set_characters_handler(sax2,
                                     (raptor_sax2_characters_handler)srxread_raptor_sax2_characters_handler);

  raptor_sax2_set_end_element_handler(sax2,
                                      srxread_raptor_sax2_end_element_handler);


  fh=fopen(srx_filename, "r");
  if(!fh) {
    fprintf(stderr, "%s: file '%s' open failed - %s", 
            program, srx_filename, strerror(errno));
    rc=1;
    goto tidy;
  }
  
  ud.trace=1;
  ud.depth=0;
  raptor_sax2_parse_start(sax2, base_uri);

  while(!feof(fh)) {
    unsigned char buffer[FILE_READ_BUF_SIZE];
    size_t read_len;
    read_len=fread((char*)buffer, 1, FILE_READ_BUF_SIZE, fh);
    if(read_len > 0) {
      fprintf(stderr, "%s: processing %d bytes\n", program, (int)read_len);
      raptor_sax2_parse_chunk(sax2, buffer, read_len, 0);
      ud.locator.byte += read_len;
    }
    
    if(read_len < FILE_READ_BUF_SIZE) {
      if(ferror(fh)) {
        fprintf(stderr, "%s: file '%s' read failed - %s\n",
                program, srx_filename, strerror(errno));
        fclose(fh);
        return(1);
      }
      break;
    }
  }
  fclose(fh); fh=NULL;

  raptor_sax2_parse_chunk(sax2, NULL, 0, 1);
  
  raptor_free_sax2(sax2); sax2=NULL;

  if(ud.failed)
    rc=1;

  
  tidy:
  if(ud.results)
    rasqal_free_query_results(ud.results);

  if(base_uri)
    raptor_free_uri(base_uri);

  if(sax2)
    raptor_free_sax2(sax2);

  if(fh)
    fclose(fh);
  
  rasqal_finish();

  return (rc);
}

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


#include <raptor.h>

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
  "bnode",
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
  STATE_bnode,
  STATE_literal,
  STATE_uri,
  STATE_first = STATE_sparql,
  STATE_last = STATE_uri
} srxread_state;
  

typedef struct 
{
  raptor_locator locator;
  srxread_state state;
  const char* name;  /* variable name (from binding/@name) */
  char* value; /* URI string, literal string or blank node ID */
  size_t value_len;
  const char* datatype; /* literal datatype URI string from literal/@datatype */
  const char* language; /* literal language from literal/@xml:lang */
  int failed;
  int depth;
  int trace;
  rasqal_query_results* results;
  int offset; /* current index into results */
  rasqal_query_result_row* row;
  int variables_count;
  int result_offset;
  raptor_sequence* variable_names;
  int free_variable_names;
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
    if(!strcmp((const char*)raptor_qname_get_local_name(name), element_names[i])) {
      state=(srxread_state)i;
      ud->state=state;
    }
  }

  if(state == STATE_unknown) {
    fprintf(stderr, "UNKNOWN element %s\n", raptor_qname_get_local_name(name));
    ud->failed++;
  }

  if(ud->trace) {
    pad(stderr, ud->depth);
    fprintf(stderr, "Element %s (%d)\n", raptor_qname_get_local_name(name), state);
  }
  
  attr_count=raptor_xml_element_get_attributes_count(xml_element);
  ud->name=NULL;
  ud->datatype=NULL;
  ud->language=NULL;
  
  if(attr_count > 0) {
    raptor_qname** attrs=raptor_xml_element_get_attributes(xml_element);
    for(i=0; i < attr_count; i++) {
      if(ud->trace) {
        pad(stderr, ud->depth+1);
        fprintf(stderr, "Attribute %s='%s'\n",
                raptor_qname_get_local_name(attrs[i]),
                raptor_qname_get_value(attrs[i]));
      }
      if(!strcmp((const char*)raptor_qname_get_local_name(attrs[i]),
                 "name"))
        ud->name=(const char*)raptor_qname_get_value(attrs[i]);
      else if(!strcmp((const char*)raptor_qname_get_local_name(attrs[i]),
                      "datatype"))
        ud->datatype=(const char*)raptor_qname_get_value(attrs[i]);
    }
  }
  if(raptor_xml_element_get_language(xml_element)) {
    ud->language=(const char*)raptor_xml_element_get_language(xml_element);
    if(ud->trace) {
      pad(stderr, ud->depth+1);
      fprintf(stderr, "xml:lang '%s'\n", ud->language);
    }
  }

  switch(state) {
    case STATE_sparql:
      break;
      
    case STATE_head:
      ud->variables_count=0;
      ud->variable_names=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_memory, NULL);
      break;
      
    case STATE_variable:
      if(1) {
        size_t var_name_len=strlen(ud->name);
        unsigned char* var_name=(unsigned char*)RASQAL_MALLOC(cstring, var_name_len+1);

        strncpy((char*)var_name, ud->name, var_name_len+1);
        raptor_sequence_set_at(ud->variable_names, ud->variables_count, var_name);

        ud->variables_count++;
      }
      break;
      
    case STATE_results:
      ud->results=rasqal_new_query_results(NULL);
      rasqal_query_results_set_variables(ud->results, ud->variable_names,
                                         ud->variables_count);
      /* variable_names is now owned by ud->results */
      ud->free_variable_names=0;

      ud->results->results_sequence=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_query_result_row, (raptor_sequence_print_handler*)rasqal_query_result_row_print);

      break;
      
    case STATE_result:
      ud->row=rasqal_new_query_result_row(ud->results);
      ud->row->offset=ud->offset;
      ud->offset++;
      break;
      
    case STATE_binding:
      ud->result_offset= -1;
      for(i=0; i < ud->variables_count; i++) {
        const char* var_name=(const char*)raptor_sequence_get_at(ud->variable_names, i);
        if(!strcmp(var_name, ud->name)) {
          ud->result_offset=i;
          break;
        }
      }
      break;
      
    case STATE_literal:

    case STATE_bnode:
    case STATE_uri:
    case STATE_unknown:
    default:
      break;
  }
  
  ud->depth++;
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

  if(ud->state == STATE_literal ||
     ud->state == STATE_uri ||
     ud->state == STATE_bnode) {
    ud->value_len=len;
    ud->value=(char*)RASQAL_MALLOC(cstring, len+1);
    memcpy(ud->value, s, len);
    ud->value[len]='\0';
  }
}


static void
srxread_raptor_sax2_end_element_handler(void *user_data,
                                        raptor_xml_element* xml_element)
{
  srxread_userdata* ud=(srxread_userdata*)user_data;
  raptor_qname* name;
  int i;
  srxread_state state=STATE_unknown;
  
  name=raptor_xml_element_get_name(xml_element);

  for(i=STATE_first; i <= STATE_last; i++) {
    if(!strcmp((const char*)raptor_qname_get_local_name(name), element_names[i])) {
      state=(srxread_state)i;
      ud->state=state;
    }
  }

  if(state == STATE_unknown) {
    fprintf(stderr, "UNKNOWN element %s\n", raptor_qname_get_local_name(name));
    ud->failed++;
  }

  ud->depth--;
  if(ud->trace) {
    pad(stderr, ud->depth);
    fprintf(stderr, "End Element %s (%d)\n", raptor_qname_get_local_name(name), ud->state);
  }

  switch(ud->state) {
    case STATE_literal:
      if(1) {
        rasqal_literal* l;
        unsigned char* lvalue;
        raptor_uri* datatype_uri=NULL;
        char* language_str=NULL;

        lvalue=(unsigned char*)RASQAL_MALLOC(cstring, ud->value_len+1);
        strncpy((char*)lvalue, ud->value, ud->value_len+1);
        if(ud->datatype)
          datatype_uri=raptor_new_uri((const unsigned char*)ud->datatype);
        if(ud->language) {
          language_str=(char*)RASQAL_MALLOC(cstring, strlen(ud->language)+1);
          strcpy(language_str, ud->language);
        }
        l=rasqal_new_string_literal(lvalue, language_str, datatype_uri, NULL);
        ud->row->values[ud->result_offset]=l;
      }
      break;
      
    case STATE_bnode:
      if(1) {
        rasqal_literal* l;
        unsigned char* lvalue;
        lvalue=(unsigned char*)RASQAL_MALLOC(cstring, ud->value_len+1);
        strncpy((char*)lvalue, ud->value, ud->value_len+1);
        l=rasqal_new_simple_literal(RASQAL_LITERAL_BLANK, lvalue);
        ud->row->values[ud->result_offset]=l;
      }
      break;
      
    case STATE_uri:
      if(1) {
        raptor_uri* uri=raptor_new_uri((const unsigned char*)ud->value);
        ud->row->values[ud->result_offset]=rasqal_new_uri_literal(uri);
      }
      break;
      
    case STATE_result:
      rasqal_query_result_row_print(ud->row, stdout);
      fputc('\n', stdout);
      raptor_sequence_push(ud->results->results_sequence, ud->row);
      ud->row=NULL;
      break;

    case STATE_unknown:
    case STATE_sparql:
    case STATE_head:
    case STATE_variable:
    case STATE_results:
    case STATE_binding:
    default:
      break;
  }

  if(ud->value) {
    RASQAL_FREE(cstring, ud->value);
    ud->value=NULL;
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

  memset(&ud, '\0', sizeof(srxread_userdata));
  ud.state=STATE_unknown;
  ud.results=rasqal_new_query_results(NULL);
  ud.free_variable_names=1;
  
  if(argc != 2) {
    fprintf(stderr, "USAGE: %s SRX file\n", program);

    rc=1;
    goto tidy;
  }

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

  if(ud.variable_names && ud.free_variable_names)
    raptor_free_sequence(ud.variable_names);
  
  if(base_uri)
    raptor_free_uri(base_uri);

  if(sax2)
    raptor_free_sax2(sax2);

  if(fh)
    fclose(fh);
  
  rasqal_finish();

  return (rc);
}

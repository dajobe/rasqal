/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * srxread.c - SPARQL Results XML Format reading test program
 *
 * Copyright (C) 2007-2008, David Beckett http://purl.org/net/dajobe/
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


#if RASQAL_DEBUG > 1
#define TRACE_XML 1
#else
#undef TRACE_XML
#endif


#ifndef FILE_READ_BUF_SIZE
#ifdef BUFSIZ
#define FILE_READ_BUF_SIZE BUFSIZ
#else
#define FILE_READ_BUF_SIZE 1024
#endif
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
  int failed;
#ifdef TRACE_XML
  int trace;
#endif

  /* Input fields */
  raptor_uri* base_uri;
  raptor_iostream* iostr;
  rasqal_query_results* results;

  /* SAX2 fields */
  raptor_sax2* sax2;
  raptor_locator locator;
  int depth; /* element depth */
  raptor_error_handlers error_handlers; /* SAX2 error handler */

  /* SPARQL XML Results parsing */
  srxread_state state; /* state */
  /* state-based fields for turning XML into rasqal literals, rows */
  const char* name;  /* variable name (from binding/@name) */
  size_t name_length;
  char* value; /* URI string, literal string or blank node ID */
  size_t value_len;
  const char* datatype; /* literal datatype URI string from literal/@datatype */
  const char* language; /* literal language from literal/@xml:lang */
  rasqal_query_result_row* row; /* current result row */
  int offset; /* current result row number */
  int result_offset; /* current <result> column number */
  unsigned char buffer[FILE_READ_BUF_SIZE]; /* iostream read buffer */

  /* Output fields */
  raptor_sequence* variables_sequence; /* sequence of variables */
  raptor_sequence* results_sequence; /* saved result rows */
} srxread_userdata;
  

#ifdef TRACE_XML
static void
pad(FILE* fh, int depth)
{
  int i;
  for(i=0; i< depth; i++)
    fputs("  ", fh);
}
#endif

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

#ifdef TRACE_XML
  if(ud->trace) {
    pad(stderr, ud->depth);
    fprintf(stderr, "Element %s (%d)\n", raptor_qname_get_local_name(name), state);
  }
#endif
  
  attr_count=raptor_xml_element_get_attributes_count(xml_element);
  ud->name=NULL;
  ud->datatype=NULL;
  ud->language=NULL;
  
  if(attr_count > 0) {
    raptor_qname** attrs=raptor_xml_element_get_attributes(xml_element);
    for(i=0; i < attr_count; i++) {
#ifdef TRACE_XML
      if(ud->trace) {
        pad(stderr, ud->depth+1);
        fprintf(stderr, "Attribute %s='%s'\n",
                raptor_qname_get_local_name(attrs[i]),
                raptor_qname_get_value(attrs[i]));
      }
#endif
      if(!strcmp((const char*)raptor_qname_get_local_name(attrs[i]),
                 "name"))
        ud->name=(const char*)raptor_qname_get_counted_value(attrs[i],
                                                             &ud->name_length);
      else if(!strcmp((const char*)raptor_qname_get_local_name(attrs[i]),
                      "datatype"))
        ud->datatype=(const char*)raptor_qname_get_value(attrs[i]);
    }
  }
  if(raptor_xml_element_get_language(xml_element)) {
    ud->language=(const char*)raptor_xml_element_get_language(xml_element);
#ifdef TRACE_XML
    if(ud->trace) {
      pad(stderr, ud->depth+1);
      fprintf(stderr, "xml:lang '%s'\n", ud->language);
    }
#endif
  }

  switch(state) {
    case STATE_sparql:
      break;
      
    case STATE_head:
      ud->variables_sequence=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_variable, (raptor_sequence_print_handler*)rasqal_variable_print);
      break;
      
    case STATE_variable:
      if(1) {
        rasqal_variable* v;
        unsigned char* var_name;

        var_name=(unsigned char*)RASQAL_MALLOC(cstring, ud->name_length+1);
        strncpy((char*)var_name, ud->name, ud->name_length+1);

        v=rasqal_new_variable_typed(NULL, RASQAL_VARIABLE_TYPE_NORMAL,
                                    var_name, NULL);

        raptor_sequence_push(ud->variables_sequence, v);
      }
      break;
      
    case STATE_result:
      ud->row=rasqal_new_query_result_row(ud->results);
      RASQAL_DEBUG2("Made new row %d\n", ud->offset);
      ud->offset++;
      break;
      
    case STATE_binding:
      ud->result_offset= -1;
      for(i=0; i < raptor_sequence_size(ud->variables_sequence); i++) {
        rasqal_variable* v=(rasqal_variable*)raptor_sequence_get_at(ud->variables_sequence, i);
        if(!strcmp((const char*)v->name, ud->name)) {
          ud->result_offset=i;
          break;
        }
      }
      break;
      
    case STATE_results:
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

#ifdef TRACE_XML
  if(ud->trace) {
    pad(stderr, ud->depth);
    fputs("Text '", stderr);
    fwrite(s, sizeof(char), len, stderr);
    fprintf(stderr, "' (%d bytes)\n", len);
  }
#endif

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
#ifdef TRACE_XML
  if(ud->trace) {
    pad(stderr, ud->depth);
    fprintf(stderr, "End Element %s (%d)\n", raptor_qname_get_local_name(name), ud->state);
  }
#endif

  switch(ud->state) {
    case STATE_head:
      if(1) {
        int size=raptor_sequence_size(ud->variables_sequence);
        RASQAL_DEBUG2("Setting results to hold %d variables\n", size);
        rasqal_query_results_set_variables(ud->results, ud->variables_sequence,
                                           size);
      }
      break;
      
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
        l=rasqal_new_string_literal_node(lvalue, language_str, datatype_uri);
        rasqal_query_result_row_set_value_at(ud->row, ud->result_offset, l);
        RASQAL_DEBUG3("Saving row result %d string value at offset %d\n",
                      ud->offset, ud->result_offset);
      }
      break;
      
    case STATE_bnode:
      if(1) {
        rasqal_literal* l;
        unsigned char* lvalue;
        lvalue=(unsigned char*)RASQAL_MALLOC(cstring, ud->value_len+1);
        strncpy((char*)lvalue, ud->value, ud->value_len+1);
        l=rasqal_new_simple_literal(RASQAL_LITERAL_BLANK, lvalue);
        rasqal_query_result_row_set_value_at(ud->row, ud->result_offset, l);
        RASQAL_DEBUG3("Saving row result %d bnode value at offset %d\n",
                      ud->offset, ud->result_offset);
      }
      break;
      
    case STATE_uri:
      if(1) {
        raptor_uri* uri=raptor_new_uri((const unsigned char*)ud->value);
        rasqal_literal* l=rasqal_new_uri_literal(uri);
        rasqal_query_result_row_set_value_at(ud->row, ud->result_offset, l);
        RASQAL_DEBUG3("Saving row result %d uri value at offset %d\n",
                      ud->offset, ud->result_offset);
      }
      break;
      
    case STATE_result:
      if(ud->row) {
        RASQAL_DEBUG2("Saving row result %d\n", ud->offset);
        raptor_sequence_push(ud->results_sequence, ud->row);
      }
      ud->row=NULL;
      break;

    case STATE_unknown:
    case STATE_sparql:
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


static srxread_userdata*
create_ud(raptor_uri* base_uri, raptor_iostream* iostr,
          rasqal_query_results* results)
{
  srxread_userdata* ud;

  ud=(srxread_userdata*)RASQAL_CALLOC(srxread_userdata, 1, sizeof(srxread_userdata));
  if(!ud)
    return NULL;
  

  ud->base_uri=base_uri;
  ud->iostr=iostr;
  ud->results=results;

  ud->state=STATE_unknown;

  ud->locator.uri=base_uri;

  memset(&ud->error_handlers, sizeof(raptor_error_handlers), '\0');
  raptor_error_handlers_init(&ud->error_handlers, 
                             NULL, NULL, /* fatal error data/handler */
                             NULL, NULL, /* error data/handler */
                             NULL, NULL, /* warning data/handler */
                             &ud->locator);
  
  ud->sax2=raptor_new_sax2(ud, &ud->error_handlers);
  if(!ud->sax2)
    return NULL;
  
  raptor_sax2_set_start_element_handler(ud->sax2,
                                        srxread_raptor_sax2_start_element_handler);
  raptor_sax2_set_characters_handler(ud->sax2,
                                     srxread_raptor_sax2_characters_handler);
  raptor_sax2_set_characters_handler(ud->sax2,
                                     (raptor_sax2_characters_handler)srxread_raptor_sax2_characters_handler);

  raptor_sax2_set_end_element_handler(ud->sax2,
                                      srxread_raptor_sax2_end_element_handler);

  ud->state=STATE_unknown;

#ifdef TRACE_XML
  ud->trace=1;
#endif
  ud->depth=0;

  ud->results_sequence=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_query_result_row, (raptor_sequence_print_handler*)rasqal_query_result_row_print);

  raptor_sax2_parse_start(ud->sax2, ud->base_uri);

  return ud;
}


static rasqal_query_result_row*
get_row_ud(srxread_userdata* ud) 
{
  rasqal_query_result_row* row=NULL;
  
  if(!raptor_sequence_size(ud->results_sequence)) {

    /* do some parsing - need some results */
    while(!raptor_iostream_read_eof(ud->iostr)) {
      size_t read_len;

      read_len=raptor_iostream_read_bytes(ud->iostr, (char*)ud->buffer,
                                          1, FILE_READ_BUF_SIZE);
      if(read_len > 0) {
        RASQAL_DEBUG2("processing %d bytes\n", (int)read_len);
        raptor_sax2_parse_chunk(ud->sax2, ud->buffer, read_len, 0);
        ud->locator.byte += read_len;
      }

      if(read_len < FILE_READ_BUF_SIZE) {
        /* finished */
        raptor_sax2_parse_chunk(ud->sax2, NULL, 0, 1);
        break;
      }

      /* got at least one row */
      if(raptor_sequence_size(ud->results_sequence) > 0)
        break;
    }
  }
  
  if(ud->failed)
    row=NULL;
  else if(raptor_sequence_size(ud->results_sequence) > 0) {
    RASQAL_DEBUG1("getting row from stored sequence\n");
    row=(rasqal_query_result_row*)raptor_sequence_unshift(ud->results_sequence);
  }

  return row;
}

  
static void
free_ud(srxread_userdata* ud) 
{
  if(ud->sax2)
    raptor_free_sax2(ud->sax2);

  if(ud->results_sequence)
    raptor_free_sequence(ud->results_sequence);

  if(ud->variables_sequence)
    raptor_free_sequence(ud->variables_sequence);

  RASQAL_FREE(srxread_userdata, ud);
}


static char *program=NULL;

int main(int argc, char *argv[]);


int
main(int argc, char *argv[]) 
{ 
  int rc=0;
  const char* srx_filename=NULL;
  raptor_iostream* iostr=NULL;
  char* p;
  srxread_userdata* ud=NULL;
  unsigned char* uri_string=NULL;
  raptor_uri* base_uri=NULL;
  rasqal_query_results* results=NULL;
  
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

  srx_filename=argv[1];

  uri_string=raptor_uri_filename_to_uri_string((const char*)srx_filename);
  if(!uri_string)
    goto tidy;
  
  base_uri=raptor_new_uri(uri_string);
  raptor_free_memory(uri_string);

  results=rasqal_new_query_results(NULL);
  if(!results) {
    fprintf(stderr, "%s: Failed to create query results", program);
    rc=1;
    goto tidy;
  }
  
  iostr=raptor_new_iostream_from_filename(srx_filename);
  if(!iostr) {
    fprintf(stderr, "%s: Failed to open iostream to file %s", program,
            srx_filename);
    rc=1;
    goto tidy;
  }
  
  ud=create_ud(base_uri, iostr, results);
  if(!ud) {
    rc=1;
    goto tidy;
  }

  while(1) {
    rasqal_query_result_row* row=NULL;
    row=get_row_ud(ud);
    if(!row)
      break;
    rasqal_query_results_add_row(results, row);
  }


  if(!rc) {
    const char* results_formatter_name=NULL;
    rasqal_query_results_formatter* write_formatter=NULL;
    raptor_iostream *write_iostr=NULL;

    RASQAL_DEBUG2("Made query results with %d results\n",
                  rasqal_query_results_get_bindings_count(results));

    write_formatter=rasqal_new_query_results_formatter(results_formatter_name,
                                                       NULL);
    if(write_formatter)
      write_iostr=raptor_new_iostream_to_file_handle(stdout);

    if(!write_formatter || !write_iostr) {
      fprintf(stderr, "%s: Creating output iostream failed\n", program);
    } else {
      rasqal_query_results_formatter_write(write_iostr, write_formatter,
                                           results, base_uri);
      raptor_free_iostream(write_iostr);
      rasqal_free_query_results_formatter(write_formatter);
    }
  }


  tidy:
  if(ud)
    free_ud(ud);
  
  if(iostr)
    raptor_free_iostream(iostr);
  
  if(results)
    rasqal_free_query_results(results);

  if(base_uri)
    raptor_free_uri(base_uri);

  rasqal_finish();

  return (rc);
}

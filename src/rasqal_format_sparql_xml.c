/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_format_sparql_xml.c - Read and write formats in SPARQL XML
 *
 * Copyright (C) 2007-2014, David Beckett http://www.dajobe.org/
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
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif


#include <raptor.h>

/* Rasqal includes */
#include <rasqal.h>
#include <rasqal_internal.h>


static int rasqal_query_results_write_sparql_xml(rasqal_query_results_formatter* formatter, raptor_iostream *iostr, rasqal_query_results* results, raptor_uri *base_uri);
static rasqal_rowsource* rasqal_query_results_get_rowsource_sparql_xml(rasqal_query_results_formatter* formatter, rasqal_world *world, rasqal_variables_table* vars_table, raptor_iostream *iostr, raptor_uri *base_uri, unsigned int flags);

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
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



/*
 * rasqal_query_results_write_sparql_xml:
 * @iostr: #raptor_iostream to write the query results to
 * @results: #rasqal_query_results query results input
 * @base_uri: #raptor_uri base URI of the output format
 *
 * Write the fourth version of the SPARQL XML query results format to an
 * iostream in a format - INTERNAL.
 * 
 * If the writing succeeds, the query results will be exhausted.
 * 
 * Return value: non-0 on failure
 **/
static int
rasqal_query_results_write_sparql_xml(rasqal_query_results_formatter* formatter,
                                      raptor_iostream *iostr,
                                      rasqal_query_results* results,
                                      raptor_uri *base_uri)
{
  int rc=1;
  rasqal_world* world = rasqal_query_results_get_world(results);
  raptor_xml_writer* xml_writer=NULL;
  raptor_namespace *res_ns=NULL;
  raptor_namespace_stack *nstack=NULL;
  raptor_xml_element *sparql_element=NULL;
  raptor_xml_element *results_element=NULL;
  raptor_xml_element *result_element=NULL;
  raptor_xml_element *element1=NULL;
  raptor_xml_element *binding_element=NULL;
  raptor_xml_element *variable_element=NULL;
  raptor_qname **attrs=NULL;
  int i;
  rasqal_query_results_type type;

  type = rasqal_query_results_get_type(results);

  if(type != RASQAL_QUERY_RESULTS_BINDINGS &&
     type != RASQAL_QUERY_RESULTS_BOOLEAN) {
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR,
                            NULL,
                            "Cannot write XML format v3 for %s query result format",
                            rasqal_query_results_type_label(type));
    return 1;
  }

  nstack = raptor_new_namespaces(world->raptor_world_ptr, 1);
  if(!nstack)
    return 1;

  xml_writer = raptor_new_xml_writer(world->raptor_world_ptr,
                                     nstack,
                                     iostr);
  if(!xml_writer)
    goto tidy;

  res_ns=raptor_new_namespace(nstack,
                              NULL,
                              RASQAL_GOOD_CAST(const unsigned char*, "http://www.w3.org/2005/sparql-results#"),
                              0);
  if(!res_ns)
    goto tidy;

  sparql_element=raptor_new_xml_element_from_namespace_local_name(res_ns,
                                                                  RASQAL_GOOD_CAST(const unsigned char*, "sparql"),
                                                                  NULL, base_uri);
  if(!sparql_element)
    goto tidy;

  raptor_xml_writer_start_element(xml_writer, sparql_element);
  raptor_xml_writer_raw_counted(xml_writer, RASQAL_GOOD_CAST(const unsigned char*, "\n"), 1);

  /*   <head> */
  element1=raptor_new_xml_element_from_namespace_local_name(res_ns,
                                                            RASQAL_GOOD_CAST(const unsigned char*, "head"),
                                                            NULL, base_uri);
  if(!element1)
    goto tidy;

  raptor_xml_writer_raw_counted(xml_writer, RASQAL_GOOD_CAST(const unsigned char*, "  "), 2);
  raptor_xml_writer_start_element(xml_writer, element1);
  raptor_xml_writer_raw_counted(xml_writer, RASQAL_GOOD_CAST(const unsigned char*, "\n"), 1);
  
  if(rasqal_query_results_is_bindings(results)) {
    for(i=0; 1; i++) {
      const unsigned char *name;
      name=rasqal_query_results_get_binding_name(results, i);
      if(!name)
        break;
      
      /*     <variable name="x"/> */
      variable_element=raptor_new_xml_element_from_namespace_local_name(res_ns,
                                                                        RASQAL_GOOD_CAST(const unsigned char*,"variable"),
                                                                        NULL, base_uri);
      if(!variable_element)
        goto tidy;
      
      attrs=(raptor_qname **)raptor_alloc_memory(sizeof(raptor_qname*));
      if(!attrs)
        goto tidy;
      attrs[0] = raptor_new_qname_from_namespace_local_name(world->raptor_world_ptr,
                                                            res_ns,
                                                            RASQAL_GOOD_CAST(const unsigned char*,"name"),
                                                            RASQAL_GOOD_CAST(const unsigned char*, name)); /* attribute value */
      if(!attrs[0]) {
        raptor_free_memory((void*)attrs);
        goto tidy;
      }

      raptor_xml_element_set_attributes(variable_element, attrs, 1);
      
      raptor_xml_writer_raw_counted(xml_writer, RASQAL_GOOD_CAST(const unsigned char*, "    "), 4);
      raptor_xml_writer_empty_element(xml_writer, variable_element);
      raptor_xml_writer_raw_counted(xml_writer, RASQAL_GOOD_CAST(const unsigned char*, "\n"), 1);
      
      raptor_free_xml_element(variable_element);
      variable_element=NULL;
    }
  }

  /* FIXME - could add <link> inside <head> */

    
  /*   </head> */
  raptor_xml_writer_raw_counted(xml_writer, RASQAL_GOOD_CAST(const unsigned char*, "  "), 2);
  raptor_xml_writer_end_element(xml_writer, element1);
  raptor_xml_writer_raw_counted(xml_writer, RASQAL_GOOD_CAST(const unsigned char*, "\n"), 1);
  
  raptor_free_xml_element(element1);
  element1=NULL;


  /* Boolean Results */
  if(rasqal_query_results_is_boolean(results)) {
    result_element=raptor_new_xml_element_from_namespace_local_name(res_ns,
                                                                    RASQAL_GOOD_CAST(const unsigned char*, "boolean"),
                                                                    NULL, base_uri);
    if(!result_element)
      goto tidy;

    raptor_xml_writer_raw_counted(xml_writer, RASQAL_GOOD_CAST(const unsigned char*, "  "), 2);
    raptor_xml_writer_start_element(xml_writer, result_element);
    if(rasqal_query_results_get_boolean(results))
      raptor_xml_writer_raw(xml_writer, rasqal_xsd_boolean_true);
    else
      raptor_xml_writer_raw(xml_writer, rasqal_xsd_boolean_false);
    raptor_xml_writer_end_element(xml_writer, result_element);
    raptor_xml_writer_raw_counted(xml_writer, RASQAL_GOOD_CAST(const unsigned char*, "\n"), 1);

    goto results3done;
  }


  /* Variable Binding Results */

  /*   <results> */
  results_element=raptor_new_xml_element_from_namespace_local_name(res_ns,
                                                                   RASQAL_GOOD_CAST(const unsigned char*, "results"),
                                                                   NULL, base_uri);
  if(!results_element)
    goto tidy;

  raptor_xml_writer_raw_counted(xml_writer, RASQAL_GOOD_CAST(const unsigned char*, "  "), 2);
  raptor_xml_writer_start_element(xml_writer, results_element);
  raptor_xml_writer_raw_counted(xml_writer, RASQAL_GOOD_CAST(const unsigned char*, "\n"), 1);


  /* declare result element for later multiple use */
  result_element=raptor_new_xml_element_from_namespace_local_name(res_ns,
                                                                  RASQAL_GOOD_CAST(const unsigned char*, "result"),
                                                                  NULL, base_uri);
  if(!result_element)
    goto tidy;

  while(!rasqal_query_results_finished(results)) {
    /*     <result> */
    raptor_xml_writer_raw_counted(xml_writer, RASQAL_GOOD_CAST(const unsigned char*, "    "), 4);
    raptor_xml_writer_start_element(xml_writer, result_element);
    raptor_xml_writer_raw_counted(xml_writer, RASQAL_GOOD_CAST(const unsigned char*, "\n"), 1);

    for(i=0; i<rasqal_query_results_get_bindings_count(results); i++) {
      const unsigned char *name=rasqal_query_results_get_binding_name(results, i);
      rasqal_literal *l=rasqal_query_results_get_binding_value(results, i);

      /*       <binding> */
      binding_element=raptor_new_xml_element_from_namespace_local_name(res_ns,
                                                                       RASQAL_GOOD_CAST(const unsigned char*, "binding"),
                                                                       NULL, base_uri);
      if(!binding_element)
        goto tidy;

      attrs=(raptor_qname **)raptor_alloc_memory(sizeof(raptor_qname*));
      if(!attrs)
        goto tidy;
      attrs[0] = raptor_new_qname_from_namespace_local_name(world->raptor_world_ptr,
                                                            res_ns,
                                                            RASQAL_GOOD_CAST(const unsigned char*, "name"),
                                                            name);

      if(!attrs[0]) {
        raptor_free_memory((void*)attrs);
        goto tidy;
      }

      raptor_xml_element_set_attributes(binding_element, attrs, 1);
      

      raptor_xml_writer_raw_counted(xml_writer, RASQAL_GOOD_CAST(const unsigned char*, "      "), 6);
      raptor_xml_writer_start_element(xml_writer, binding_element);

      if(!l) {
        element1=raptor_new_xml_element_from_namespace_local_name(res_ns,
                                                                  RASQAL_GOOD_CAST(const unsigned char*, "unbound"),
                                                                  NULL, base_uri);
        if(!element1)
          goto tidy;
        raptor_xml_writer_empty_element(xml_writer, element1);

      } else switch(l->type) {
        case RASQAL_LITERAL_URI:
          element1=raptor_new_xml_element_from_namespace_local_name(res_ns,
                                                                    RASQAL_GOOD_CAST(const unsigned char*, "uri"),
                                                                    NULL, base_uri);
          if(!element1)
            goto tidy;
          
          raptor_xml_writer_start_element(xml_writer, element1);
          raptor_xml_writer_cdata(xml_writer, RASQAL_GOOD_CAST(const unsigned char*, raptor_uri_as_string(l->value.uri)));
          raptor_xml_writer_end_element(xml_writer, element1);

          break;

        case RASQAL_LITERAL_BLANK:
          element1=raptor_new_xml_element_from_namespace_local_name(res_ns,
                                                                    RASQAL_GOOD_CAST(const unsigned char*, "bnode"),
                                                                    NULL, base_uri);
          if(!element1)
            goto tidy;
          
          raptor_xml_writer_start_element(xml_writer, element1);
          raptor_xml_writer_cdata(xml_writer, RASQAL_GOOD_CAST(const unsigned char*, l->string));
          raptor_xml_writer_end_element(xml_writer, element1);
          break;

        case RASQAL_LITERAL_STRING:
        case RASQAL_LITERAL_UDT:
          element1=raptor_new_xml_element_from_namespace_local_name(res_ns,
                                                                    RASQAL_GOOD_CAST(const unsigned char*, "literal"),
                                                                    NULL, base_uri);
          if(!element1)
            goto tidy;

          if(l->language || l->datatype) {
            attrs=(raptor_qname **)raptor_alloc_memory(sizeof(raptor_qname*));
            if(!attrs)
              goto tidy;

            if(l->language)
              attrs[0]=raptor_new_qname(nstack,
                                        RASQAL_GOOD_CAST(const unsigned char*, "xml:lang"),
                                        RASQAL_GOOD_CAST(const unsigned char*, l->language));
            else
              attrs[0] = raptor_new_qname_from_namespace_local_name(world->raptor_world_ptr,
                                                                    res_ns,
                                                                    RASQAL_GOOD_CAST(const unsigned char*, "datatype"),
                                                                    RASQAL_GOOD_CAST(const unsigned char*, raptor_uri_as_string(l->datatype)));
            if(!attrs[0]) {
              raptor_free_memory((void*)attrs);
              goto tidy;
            }

            raptor_xml_element_set_attributes(element1, attrs, 1);
          }


          raptor_xml_writer_start_element(xml_writer, element1);


          raptor_xml_writer_cdata_counted(xml_writer,
                                          RASQAL_GOOD_CAST(const unsigned char*, l->string),
                                          l->string_len);

          raptor_xml_writer_end_element(xml_writer, element1);
          
          break;
        case RASQAL_LITERAL_PATTERN:
        case RASQAL_LITERAL_QNAME:
        case RASQAL_LITERAL_INTEGER:
        case RASQAL_LITERAL_XSD_STRING:
        case RASQAL_LITERAL_BOOLEAN:
        case RASQAL_LITERAL_DOUBLE:
        case RASQAL_LITERAL_FLOAT:
        case RASQAL_LITERAL_VARIABLE:
        case RASQAL_LITERAL_DECIMAL:
        case RASQAL_LITERAL_DATE:
        case RASQAL_LITERAL_DATETIME:
        case RASQAL_LITERAL_INTEGER_SUBTYPE:

        case RASQAL_LITERAL_UNKNOWN:
        default:
          rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR,
                                  NULL,
                                  "Cannot turn literal type %u into XML",
                                  l->type);
          goto tidy;
        }

      if(element1) {
        raptor_free_xml_element(element1);
        element1=NULL;
      }

      /*       </binding> */
      raptor_xml_writer_end_element(xml_writer, binding_element);
      raptor_xml_writer_raw_counted(xml_writer, RASQAL_GOOD_CAST(const unsigned char*, "\n"), 1);
      
      raptor_free_xml_element(binding_element);
      binding_element=NULL;
    }

    raptor_xml_writer_raw_counted(xml_writer, RASQAL_GOOD_CAST(const unsigned char*, "    "), 4);
    raptor_xml_writer_end_element(xml_writer, result_element);
    raptor_xml_writer_raw_counted(xml_writer, RASQAL_GOOD_CAST(const unsigned char*, "\n"), 1);
    
    rasqal_query_results_next(results);
  }

  raptor_xml_writer_raw_counted(xml_writer, RASQAL_GOOD_CAST(const unsigned char*, "  "), 2);
  raptor_xml_writer_end_element(xml_writer, results_element);
  raptor_xml_writer_raw_counted(xml_writer, RASQAL_GOOD_CAST(const unsigned char*, "\n"), 1);

  results3done:

  rc=0;

  raptor_xml_writer_end_element(xml_writer, sparql_element);
  raptor_xml_writer_raw_counted(xml_writer, RASQAL_GOOD_CAST(const unsigned char*, "\n"), 1);

  tidy:
  if(element1)
    raptor_free_xml_element(element1);
  if(variable_element)
    raptor_free_xml_element(variable_element);
  if(binding_element)
    raptor_free_xml_element(binding_element);
  if(result_element)
    raptor_free_xml_element(result_element);
  if(results_element)
    raptor_free_xml_element(results_element);
  if(sparql_element)
    raptor_free_xml_element(sparql_element);
  if(res_ns)
    raptor_free_namespace(res_ns);
  if(xml_writer)
    raptor_free_xml_writer(xml_writer);
  if(nstack)
    raptor_free_namespaces(nstack);

  return rc;
}


const char* const sparql_xml_element_names[]=
{
  "!",
  /* In rough order they appear */
  "sparql",
  "head",
  "boolean",
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
  STATE_boolean,
  STATE_binding,
  STATE_variable,
  STATE_results,
  STATE_result,
  STATE_bnode,
  STATE_literal,
  STATE_uri,
  STATE_first = STATE_sparql,
  STATE_last = STATE_uri
} rasqal_sparql_xml_read_state;
  

typedef struct 
{
  rasqal_world* world;
  rasqal_rowsource* rowsource;
  
  int failed;
#ifdef TRACE_XML
  int trace;
#endif

  /* Input fields */
  raptor_uri* base_uri;
  raptor_iostream* iostr;

  /* SAX2 fields */
  raptor_sax2* sax2;
  raptor_locator locator;
  int depth; /* element depth */

  /* SPARQL XML Results parsing */
  rasqal_sparql_xml_read_state state; /* state */
  /* state-based fields for turning XML into rasqal literals, rows */
  const char* name;  /* variable name (from binding/@name) */
  size_t name_length;

  raptor_stringbuffer *sb; /* URI string, literal string or blank node ID */

  const char* datatype; /* literal datatype URI string from literal/@datatype */
  const char* language; /* literal language from literal/@xml:lang */
  rasqal_row* row; /* current result row */
  int offset; /* current result row number */
  int result_offset; /* current <result> column number */
  unsigned char buffer[FILE_READ_BUF_SIZE]; /* iostream read buffer */

  /* Output fields */
  raptor_sequence* results_sequence; /* saved result rows */

  /* Variables table allocated for variables in the result set */
  rasqal_variables_table* vars_table;
  int variables_count;

  unsigned int flags;

  int boolean_value;
} rasqal_rowsource_sparql_xml_context;
  

static void rasqal_sparql_xml_free_context(rasqal_rowsource_sparql_xml_context* con);


#ifdef TRACE_XML
static void
pad(FILE* fh, int depth)
{
  int i;
  for(i=0; i< depth; i++)
    fputs("  ", fh);
}
#endif

static int
rasqal_sparql_xml_start(rasqal_rowsource_sparql_xml_context* con)
{
  con->state = STATE_unknown;

#ifdef TRACE_XML
  con->trace = 1;
#endif
  con->depth = 0;

  raptor_sax2_parse_start(con->sax2, con->base_uri);

  return 0;
}


static void
rasqal_sparql_xml_sax2_start_element_handler(void *user_data,
                                             raptor_xml_element *xml_element)
{
  rasqal_rowsource_sparql_xml_context* con;
  int i;
  raptor_qname* name;
  rasqal_sparql_xml_read_state state=STATE_unknown;
  int attr_count;

  con=(rasqal_rowsource_sparql_xml_context*)user_data;

  name=raptor_xml_element_get_name(xml_element);

  for(i=STATE_first; i <= STATE_last; i++) {
    if(!strcmp(RASQAL_GOOD_CAST(const char*, raptor_qname_get_local_name(name)),
               sparql_xml_element_names[i])) {
      state=(rasqal_sparql_xml_read_state)i;
      con->state=state;
    }
  }

  if(state == STATE_unknown) {
    fprintf(stderr, "UNKNOWN element %s\n", raptor_qname_get_local_name(name));
    con->failed++;
  }

#ifdef TRACE_XML
  if(con->trace) {
    pad(stderr, con->depth);
    fprintf(stderr, "Element %s (%d)\n", raptor_qname_get_local_name(name),
            state);
  }
#endif
  
  attr_count=raptor_xml_element_get_attributes_count(xml_element);
  con->name=NULL;
  con->sb = raptor_new_stringbuffer();
  con->datatype=NULL;
  con->language=NULL;
  
  if(attr_count > 0) {
    raptor_qname** attrs=raptor_xml_element_get_attributes(xml_element);
    for(i = 0; i < attr_count; i++) {
      const char* local_name = RASQAL_GOOD_CAST(const char*, raptor_qname_get_local_name(attrs[i]));
#ifdef TRACE_XML
      if(con->trace) {
        pad(stderr, con->depth+1);
        fprintf(stderr, "Attribute %s='%s'\n",
                local_name, raptor_qname_get_value(attrs[i]));
      }
#endif

      if(!strcmp(local_name, "name"))
        con->name = RASQAL_GOOD_CAST(const char*, raptor_qname_get_counted_value(attrs[i], &con->name_length));
      else if(!strcmp(local_name, "datatype"))
        con->datatype = RASQAL_GOOD_CAST(const char*, raptor_qname_get_value(attrs[i]));
    }
  }
  if(raptor_xml_element_get_language(xml_element)) {
    con->language = RASQAL_GOOD_CAST(const char*, raptor_xml_element_get_language(xml_element));
#ifdef TRACE_XML
    if(con->trace) {
      pad(stderr, con->depth+1);
      fprintf(stderr, "xml:lang '%s'\n", con->language);
    }
#endif
  }

  switch(state) {
    case STATE_variable:
      if(con->name) {
        rasqal_variable *v;
        v = rasqal_variables_table_add2(con->vars_table,
                                        RASQAL_VARIABLE_TYPE_NORMAL,
                                        RASQAL_GOOD_CAST(const unsigned char*, con->name),
                                        con->name_length, NULL);
        if(v) {
          rasqal_rowsource_add_variable(con->rowsource, v);
          /* above function takes a reference to v */
          rasqal_free_variable(v);
        }
      }
      break;
      
    case STATE_result:
      if(1) {
        con->row = rasqal_new_row(con->rowsource);
        RASQAL_DEBUG2("Made new row %d\n", con->offset);
        con->offset++;
      }
      break;
      
    case STATE_binding:
      con->result_offset = rasqal_rowsource_get_variable_offset_by_name(con->rowsource, RASQAL_GOOD_CAST(const unsigned char*, con->name));
      break;
      
    case STATE_sparql:
    case STATE_head:
    case STATE_boolean:
    case STATE_results:
    case STATE_literal:
    case STATE_bnode:
    case STATE_uri:
    case STATE_unknown:
    default:
      break;
  }
  
  con->depth++;
}


static void
rasqal_sparql_xml_sax2_characters_handler(void *user_data,
                                          raptor_xml_element* xml_element,
                                          const unsigned char *s, int len)
{
  rasqal_rowsource_sparql_xml_context* con;
  con=(rasqal_rowsource_sparql_xml_context*)user_data;

#ifdef TRACE_XML
  if(con->trace) {
    pad(stderr, con->depth);
    fputs("Text '", stderr);
    fwrite(s, sizeof(char), len, stderr);
    fprintf(stderr, "' (%d bytes)\n", len);
  }
#endif

  if(con->state == STATE_literal ||
     con->state == STATE_uri ||
     con->state == STATE_bnode ||
     con->state == STATE_boolean ) {
    raptor_stringbuffer_append_counted_string(con->sb, s, RASQAL_BAD_CAST(size_t, len), 1);
  }
}


static void
rasqal_sparql_xml_sax2_end_element_handler(void *user_data,
                                           raptor_xml_element* xml_element)
{
  rasqal_rowsource_sparql_xml_context* con;
  raptor_qname* name;
  int i;
  rasqal_sparql_xml_read_state state=STATE_unknown;
  const char* value = NULL;
  size_t value_len = 0;

  con=(rasqal_rowsource_sparql_xml_context*)user_data;

  name=raptor_xml_element_get_name(xml_element);

  if(con->sb) {
    value_len = raptor_stringbuffer_length(con->sb);
    value = RASQAL_GOOD_CAST(const char*, raptor_stringbuffer_as_string(con->sb));
  }

  for(i=STATE_first; i <= STATE_last; i++) {
    if(!strcmp(RASQAL_GOOD_CAST(const char*, raptor_qname_get_local_name(name)),
               sparql_xml_element_names[i])) {
      state=(rasqal_sparql_xml_read_state)i;
      con->state=state;
    }
  }

  if(state == STATE_unknown) {
    fprintf(stderr, "UNKNOWN element %s\n", raptor_qname_get_local_name(name));
    con->failed++;
  }

  con->depth--;
#ifdef TRACE_XML
  if(con->trace) {
    pad(stderr, con->depth);
    fprintf(stderr, "End Element %s (%d)\n", raptor_qname_get_local_name(name),
            con->state);
  }
#endif

  switch(con->state) {
    case STATE_head:
      if(con->vars_table) {
        /* Only now is the full number of variables correct in
         * con->rowsource->size */
        con->variables_count = con->rowsource->size;
      }
      break;
      
    case STATE_boolean:
      if(1) {
        con->boolean_value = -1;
        if(value_len == 4 && !strncmp(value, "true", value_len))
          con->boolean_value = 1;
        else if(value_len == 5 && !strncmp(value, "false", value_len))
          con->boolean_value = 0;
        RASQAL_DEBUG3("boolean result string '%s' value %d\n", value, con->boolean_value);
      }
      break;

    case STATE_literal:
      if(1) {
        rasqal_literal* l;
        unsigned char* lvalue;
        raptor_uri* datatype_uri=NULL;
        char* language_str=NULL;

        lvalue = RASQAL_MALLOC(unsigned char*, value_len + 1);
        if(!value_len)
          *lvalue = '\0';
        else
          memcpy(lvalue, value, value_len + 1);
        if(con->datatype)
          datatype_uri = raptor_new_uri(con->world->raptor_world_ptr, RASQAL_GOOD_CAST(const unsigned char*, con->datatype));
        if(con->language) {
          size_t language_len = strlen(con->language);
          language_str = RASQAL_MALLOC(char*, language_len + 1);
          memcpy(language_str, con->language, language_len + 1);
        }
        l = rasqal_new_string_literal_node(con->world, lvalue, language_str,
                                           datatype_uri);
        rasqal_row_set_value_at(con->row, con->result_offset, l);
        rasqal_free_literal(l);
        RASQAL_DEBUG3("Saving row result %d string value at offset %d\n",
                      con->offset, con->result_offset);
      }
      break;
      
    case STATE_bnode:
      if(1) {
        rasqal_literal* l;
        unsigned char* lvalue;
        lvalue = RASQAL_MALLOC(unsigned char*, value_len + 1);
        memcpy(lvalue, value, value_len + 1);
        l = rasqal_new_simple_literal(con->world, RASQAL_LITERAL_BLANK, lvalue);
        rasqal_row_set_value_at(con->row, con->result_offset, l);
        rasqal_free_literal(l);
        RASQAL_DEBUG3("Saving row result %d bnode value at offset %d\n",
                      con->offset, con->result_offset);
      }
      break;
      
    case STATE_uri:
      if(1) {
        raptor_uri* uri;
        rasqal_literal* l;
        uri = raptor_new_uri(con->world->raptor_world_ptr, RASQAL_GOOD_CAST(const unsigned char*, value));
        l = rasqal_new_uri_literal(con->world, uri);
        rasqal_row_set_value_at(con->row, con->result_offset, l);
        rasqal_free_literal(l);
        RASQAL_DEBUG3("Saving row result %d uri value at offset %d\n",
                      con->offset, con->result_offset);
      }
      break;
      
    case STATE_result:
      if(con->row) {
        RASQAL_DEBUG2("Saving row result %d\n", con->offset);
        con->row->offset = con->offset - 1;
        raptor_sequence_push(con->results_sequence, con->row);
      }
      con->row = NULL;
      break;

    case STATE_unknown:
    case STATE_sparql:
    case STATE_variable:
    case STATE_results:
    case STATE_binding:
    default:
      break;
  }

  if(con->sb) {
    raptor_free_stringbuffer(con->sb);
    con->sb = raptor_new_stringbuffer();
  }
}


/* Local handlers for turning sparql XML read from an iostream into rows */

static int
rasqal_rowsource_sparql_xml_init(rasqal_rowsource* rowsource, void *user_data) 
{
  rasqal_rowsource_sparql_xml_context* con;

  con = (rasqal_rowsource_sparql_xml_context*)user_data;

  con->rowsource = rowsource;

  return rasqal_sparql_xml_start(con);
}


static int
rasqal_rowsource_sparql_xml_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_rowsource_sparql_xml_context* con;

  con = (rasqal_rowsource_sparql_xml_context*)user_data;

  rasqal_sparql_xml_free_context(con);

  return 0;
}


static void
rasqal_rowsource_sparql_xml_process(rasqal_rowsource_sparql_xml_context* con)
{
  if(raptor_sequence_size(con->results_sequence) && con->variables_count > 0)
    return;

  /* do some parsing - need some results */
  while(!raptor_iostream_read_eof(con->iostr)) {
    size_t read_len;
    
    read_len = RASQAL_BAD_CAST(size_t,
                               raptor_iostream_read_bytes(RASQAL_GOOD_CAST(char*, con->buffer), 1,
                                                          FILE_READ_BUF_SIZE,
                                                          con->iostr));
    if(read_len > 0) {
#ifdef TRACE_XML
      RASQAL_DEBUG2("processing %d bytes\n", RASQAL_GOOD_CAST(int, read_len));
#endif
      raptor_sax2_parse_chunk(con->sax2, con->buffer, read_len, 0);
    }
    
    if(read_len < FILE_READ_BUF_SIZE) {
      /* finished */
      raptor_sax2_parse_chunk(con->sax2, NULL, 0, 1);
      break;
    }
    
    /* end with variables sequence done AND at least one row */
    if(con->variables_count > 0 &&
       raptor_sequence_size(con->results_sequence) > 0)
      break;
  }
  
}


static int
rasqal_rowsource_sparql_xml_ensure_variables(rasqal_rowsource* rowsource,
                                             void *user_data)
{
  rasqal_rowsource_sparql_xml_context* con;

  con = (rasqal_rowsource_sparql_xml_context*)user_data;

  rasqal_rowsource_sparql_xml_process(con);

  return con->failed;
}


static rasqal_row*
rasqal_rowsource_sparql_xml_read_row(rasqal_rowsource* rowsource,
                                     void *user_data)
{
  rasqal_rowsource_sparql_xml_context* con;
  rasqal_row* row=NULL;

  con=(rasqal_rowsource_sparql_xml_context*)user_data;

  rasqal_rowsource_sparql_xml_process(con);
  
  if(!con->failed && raptor_sequence_size(con->results_sequence) > 0) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("getting row from stored sequence\n");
#endif
    row=(rasqal_row*)raptor_sequence_unshift(con->results_sequence);
  }

  return row;
}


/*
 * rasqal_sparql_xml_init_context:
 * @world: rasqal world object
 * @iostr: #raptor_iostream to read the query results from
 * @base_uri: #raptor_uri base URI of the input format
 * @flags: flags
 *
 * INTERNAL - Initialise the SPARQL XML context
 *
 * Return value: context or NULL on failure
 **/
static rasqal_rowsource_sparql_xml_context*
rasqal_sparql_xml_init_context(rasqal_world *world,
                               raptor_iostream *iostr,
                               raptor_uri *base_uri,
                               unsigned int flags)
{
  rasqal_rowsource_sparql_xml_context* con;

  con = RASQAL_CALLOC(rasqal_rowsource_sparql_xml_context*, 1, sizeof(*con));
  if(!con)
    return NULL;

  con->world = world;
  con->base_uri = base_uri ? raptor_uri_copy(base_uri) : NULL;
  con->iostr = iostr;

  con->locator.uri = base_uri;

  con->sax2 = raptor_new_sax2(world->raptor_world_ptr, &con->locator, con);
  if(!con->sax2)
    return NULL;

  con->flags = flags;

  raptor_sax2_set_start_element_handler(con->sax2,
                                        rasqal_sparql_xml_sax2_start_element_handler);
  raptor_sax2_set_characters_handler(con->sax2,
                                     rasqal_sparql_xml_sax2_characters_handler);

  raptor_sax2_set_end_element_handler(con->sax2,
                                      rasqal_sparql_xml_sax2_end_element_handler);

  return con;
}



/*
 * rasqal_sparql_xml_free_context:
 * @con: SPARQL xml context
 *
 * INTERNAL - Free the SPARQL XML context
 **/
static void
rasqal_sparql_xml_free_context(rasqal_rowsource_sparql_xml_context* con)
{

  if(con->base_uri)
    raptor_free_uri(con->base_uri);

  if(con->sax2)
    raptor_free_sax2(con->sax2);

  if(con->results_sequence)
    raptor_free_sequence(con->results_sequence);

  if(con->vars_table)
    rasqal_free_variables_table(con->vars_table);

  if(con->flags) {
    if(con->iostr)
      raptor_free_iostream(con->iostr);
  }

  if(con->sb)
    raptor_free_stringbuffer(con->sb);

  RASQAL_FREE(rasqal_rowsource_sparql_xml_context, con);
}


static int
rasqal_rowsource_sparql_xml_get_boolean(rasqal_query_results_formatter *formatter,
                                        rasqal_world* world, raptor_iostream *iostr,
                                        raptor_uri *base_uri, unsigned int flags)
{
  rasqal_rowsource_sparql_xml_context* con;
  int bv;

  con = rasqal_sparql_xml_init_context(world, iostr, base_uri, flags);
  if(!con)
    return -1;

  rasqal_sparql_xml_start(con);

  /* do some parsing - until we get the boolean value */
  while(!raptor_iostream_read_eof(con->iostr)) {
    size_t read_len;

    read_len = RASQAL_BAD_CAST(size_t,
                               raptor_iostream_read_bytes(RASQAL_GOOD_CAST(char*, con->buffer), 1,
                                                          FILE_READ_BUF_SIZE,
                                                          con->iostr));
    if(read_len > 0) {
#ifdef TRACE_XML
      RASQAL_DEBUG2("processing %d bytes\n", RASQAL_GOOD_CAST(int, read_len));
#endif
      raptor_sax2_parse_chunk(con->sax2, con->buffer, read_len, 0);
    }

    if(read_len < FILE_READ_BUF_SIZE) {
      /* finished */
      raptor_sax2_parse_chunk(con->sax2, NULL, 0, 1);
      break;
    }

    /* end with any boolean value */
    if(con->boolean_value >= 0)
      break;
  }

  bv = con->boolean_value;
  
  rasqal_sparql_xml_free_context(con);

  return bv;
}


static const rasqal_rowsource_handler rasqal_rowsource_sparql_xml_handler={
  /* .version = */ 1,
  "SPARQL XML",
  /* .init = */ rasqal_rowsource_sparql_xml_init,
  /* .finish = */ rasqal_rowsource_sparql_xml_finish,
  /* .ensure_variables = */ rasqal_rowsource_sparql_xml_ensure_variables,
  /* .read_row = */ rasqal_rowsource_sparql_xml_read_row,
  /* .read_all_rows = */ NULL,
  /* .reset = */ NULL,
  /* .set_requirements = */ NULL,
  /* .get_inner_rowsource = */ NULL,
  /* .set_origin = */ NULL,
};



/*
 * rasqal_query_results_getrowsource_sparql_xml:
 * @world: rasqal world object
 * @iostr: #raptor_iostream to read the query results from
 * @base_uri: #raptor_uri base URI of the input format
 *
 * INTERNAL - Read the fourth version of the SPARQL XML query results
 * format from an iostream in a format returning a rowsource.
 * 
 * Return value: a new rasqal_rowsource or NULL on failure
 **/
static rasqal_rowsource*
rasqal_query_results_get_rowsource_sparql_xml(rasqal_query_results_formatter* formatter,
                                              rasqal_world *world,
                                              rasqal_variables_table* vars_table,
                                              raptor_iostream *iostr,
                                              raptor_uri *base_uri,
                                              unsigned int flags)
{
  rasqal_rowsource_sparql_xml_context* con;
  
  con = rasqal_sparql_xml_init_context(world, iostr, base_uri, flags);
  if(!con)
    return NULL;

  con->results_sequence = raptor_new_sequence((raptor_data_free_handler)rasqal_free_row, (raptor_data_print_handler)rasqal_row_print);

  con->vars_table = rasqal_new_variables_table_from_variables_table(vars_table);
  
  return rasqal_new_rowsource_from_handler(world, NULL,
                                           con,
                                           &rasqal_rowsource_sparql_xml_handler,
                                           con->vars_table,
                                           0);
}



static int
rasqal_query_results_xml_recognise_syntax(rasqal_query_results_format_factory* factory, 
                                          const unsigned char *buffer, 
                                          size_t len,
                                          const unsigned char *identifier,
                                          const unsigned char *suffix,
const char *mime_type)
{

  if(suffix && !strcmp(RASQAL_GOOD_CAST(const char*, suffix), "srx"))
    return 8;
  
  return 0;
}


static const char* const sparql_xml_names[] = { "xml", NULL};

static const char* const sparql_xml_uri_strings[] = {
  "http://www.w3.org/ns/formats/SPARQL_Results_XML",
  "http://www.w3.org/TR/rdf-sparql-XMLres/",
  "http://www.w3.org/TR/2008/REC-rdf-sparql-XMLres-20080115/",
  "http://www.w3.org/2005/sparql-results#",
  NULL
};

static const raptor_type_q sparql_xml_types[] = {
  { "application/sparql-results+xml", 30, 10}, 
  { NULL, 0, 0}
};

static int
rasqal_query_results_sparql_xml_register_factory(rasqal_query_results_format_factory *factory) 
{
  int rc = 0;

  factory->desc.names = sparql_xml_names;
  factory->desc.mime_types = sparql_xml_types;

  factory->desc.label = "SPARQL XML Query Results";
  factory->desc.uri_strings = sparql_xml_uri_strings;

  factory->desc.flags = 0;
  
  factory->write         = rasqal_query_results_write_sparql_xml;
  factory->get_rowsource = rasqal_query_results_get_rowsource_sparql_xml;
  factory->recognise_syntax = rasqal_query_results_xml_recognise_syntax;
  factory->get_boolean      = rasqal_rowsource_sparql_xml_get_boolean;

  return rc;
}


int
rasqal_init_result_format_sparql_xml(rasqal_world* world)
{
  return !rasqal_world_register_query_results_format_factory(world,
                                                             &rasqal_query_results_sparql_xml_register_factory);
}

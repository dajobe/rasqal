/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_result_formats.c - Rasqal RDF Query Result Formats
 *
 * Copyright (C) 2003-2008, David Beckett http://purl.org/net/dajobe/
 * Copyright (C) 2003-2005, University of Bristol, UK http://www.bristol.ac.uk/
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
#include <stdarg.h>

#include "rasqal.h"
#include "rasqal_internal.h"


static int rasqal_query_results_write_xml_result4(raptor_iostream *iostr, rasqal_query_results* results, raptor_uri *base_uri);
static int rasqal_query_results_read_xml_result4(raptor_iostream *iostr, rasqal_query_results* results, raptor_uri *base_uri);
static int rasqal_query_results_write_json1(raptor_iostream *iostr, rasqal_query_results* results, raptor_uri *base_uri);


static raptor_sequence* query_results_formats;


static
void rasqal_query_results_format_register_factory(const char *name,
                                                  const char *label,
                                                  const unsigned char* uri_string,
                                                  rasqal_query_results_formatter_func writer,
                                                  rasqal_query_results_formatter_func reader,
                                                  const char *mime_type)
{
  rasqal_query_results_format_factory* factory;

  factory=(rasqal_query_results_format_factory*)RASQAL_MALLOC(query_results_format_factory, 
                                                              sizeof(rasqal_query_results_format_factory));

  if(!factory)
    RASQAL_FATAL1("Out of memory\n");
  factory->name=name;
  factory->label=label;
  factory->uri_string=uri_string;
  factory->writer=writer;
  factory->reader=reader;
  factory->mime_type=mime_type;

  raptor_sequence_push(query_results_formats, factory);
}



static
void rasqal_free_query_results_format_factory(rasqal_query_results_format_factory* factory) 
{
  RASQAL_FREE(query_results_format_factory, factory);
}


void
rasqal_init_result_formats(void)
{
  rasqal_query_results_formatter_func writer_fn=NULL;
  rasqal_query_results_formatter_func reader_fn=NULL;

  query_results_formats=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_query_results_format_factory, NULL);
  if(!query_results_formats)
    RASQAL_FATAL1("Out of memory\n");

  /*
   * SPARQL XML Results 2007-06-14
   * http://www.w3.org/TR/2006/WD-rdf-sparql-XMLres-20070614/
   */
  writer_fn=&rasqal_query_results_write_xml_result4;
  reader_fn=&rasqal_query_results_read_xml_result4;
  rasqal_query_results_format_register_factory("xml",
                                               "SPARQL Query Results Format 2007-06-14",
                                               (unsigned char*)"http://www.w3.org/2005/sparql-results#",
                                               writer_fn, reader_fn,
                                               "application/sparql-results+xml");
  rasqal_query_results_format_register_factory(NULL,
                                               NULL,
                                               (unsigned char*)"http://www.w3.org/TR/2006/WD-rdf-sparql-XMLres-20070614/",
                                               writer_fn, reader_fn,
                                               "application/sparql-results+xml");

  /*
   * SPARQL Query Results in JSON (http://json.org/) draft
   * Defined in http://www.w3.org/2001/sw/DataAccess/json-sparql/
   * Version: 1.6 $ of $Date: 2006/04/05 15:55:17
   */
  writer_fn=&rasqal_query_results_write_json1;
  reader_fn=NULL;
  rasqal_query_results_format_register_factory("json",
                                               "JSON",
                                               (unsigned char*)"http://www.w3.org/2001/sw/DataAccess/json-sparql/",
                                               writer_fn, reader_fn,
                                               "text/json");
  rasqal_query_results_format_register_factory(NULL,
                                               NULL,
                                               (unsigned char*)"http://www.mindswap.org/%7Ekendall/sparql-results-json/",
                                               writer_fn, reader_fn,
                                               "text/json");
}


void
rasqal_finish_result_formats(void)
{
  if(query_results_formats) {
    raptor_free_sequence(query_results_formats);
    query_results_formats = NULL;
  }
}


/**
 * rasqal_query_results_formats_enumerate:
 * @counter: index into the list of query result syntaxes
 * @name: pointer to store the name of the query result syntax (or NULL)
 * @label: pointer to store query result syntax readable label (or NULL)
 * @uri_string: pointer to store query result syntax URI string (or NULL)
 * @mime_type: pointer to store query result syntax mime type string (or NULL)
 * @flags: pointer to store query result syntax flags (or NULL)
 *
 * Get information on query result syntaxes.
 * 
 * The current list of format names/URI is given below however
 * the results of this function will always return the latest.
 *
 * SPARQL XML Results 2007-06-14 (default format when @counter is 0)
 * name '<literal>xml</literal>' with
 * URIs http://www.w3.org/TR/2006/WD-rdf-sparql-XMLres-20070614/ or
 * http://www.w3.org/2005/sparql-results#
 *
 * JSON name '<literal>json</literal>' and
 * URI http://www.w3.org/2001/sw/DataAccess/json-sparql/
 *
 * All returned strings are shared and must be copied if needed to be
 * used dynamically.
 * 
 * Return value: non 0 on failure of if counter is out of range
 **/
int
rasqal_query_results_formats_enumerate(const unsigned int counter,
                                       const char **name,
                                       const char **label,
                                       const unsigned char **uri_string,
                                       const char **mime_type,
                                       int *flags)
{
  rasqal_query_results_format_factory *factory;
  int i;
  unsigned int real_counter;
  
  real_counter=0;
  for(i=0; 1; i++) {
    factory=(rasqal_query_results_format_factory*)raptor_sequence_get_at(query_results_formats, i);
    if(!factory)
      break;

    if(factory->name) {
      if(real_counter == counter)
        break;
      real_counter++;
    }
  }

  if(!factory)
    return 1;

  if(name)
    *name=factory->name;
  if(label)
    *label=factory->label;
  if(uri_string)
    *uri_string=factory->uri_string;
  if(mime_type)
    *mime_type=factory->mime_type;
  if(flags) {
    *flags=0;
    if(factory->reader)
      *flags |= RASQAL_QUERY_RESULTS_FORMAT_FLAG_READER;
    if(factory->writer)
      *flags |= RASQAL_QUERY_RESULTS_FORMAT_FLAG_WRITER;
  }
  
  return 0;
}


static rasqal_query_results_format_factory*
rasqal_get_query_results_formatter_factory(const char *name, raptor_uri* uri,
                                           const char *mime_type)
{
  int i;
  rasqal_query_results_format_factory* factory=NULL;
  
  for(i=0; 1; i++) {
    factory=(rasqal_query_results_format_factory*)raptor_sequence_get_at(query_results_formats,
                                                                         i);
    if(!factory)
      break;

    if(!name && !uri)
      /* the default is the first registered format */
      break;
    
    if(name && factory->name &&
       !strcmp(factory->name, (const char*)name))
      return factory;


    if(uri && factory->uri_string &&
       !strcmp((const char*)raptor_uri_as_string(uri),
               (const char*)factory->uri_string))
      break;


    if(mime_type && factory->mime_type &&
       !strcmp(factory->mime_type, (const char*)mime_type))
      return factory;
  }
  
  return factory;
}


/**
 * rasqal_query_results_formats_check:
 * @name: the query results format name (or NULL)
 * @uri: #raptor_uri query results format uri (or NULL)
 * @mime_type: mime type name
 * 
 * Check if a query results formatter exists for the requested format.
 * 
 * Return value: non-0 if a formatter exists.
 **/
int
rasqal_query_results_formats_check(const char *name, raptor_uri* uri,
                                   const char *mime_type)
{
  return (rasqal_get_query_results_formatter_factory(name, uri, mime_type) 
          != NULL);
}


/**
 * rasqal_new_query_results_formatter:
 * @name: the query results format name (or NULL)
 * @format_uri: #raptor_uri query results format uri (or NULL)
 *
 * Constructor - create a new rasqal_query_results_formatter object by identified format.
 *
 * A query results format can be named or identified by a URI, both
 * of which are optional.  The default query results format will be used
 * if both are NULL.  rasqal_query_results_formats_enumerate() returns
 * information on the known query results names, labels and URIs.
 *
 * Return value: a new #rasqal_query_results_formatter object or NULL on failure
 */
rasqal_query_results_formatter*
rasqal_new_query_results_formatter(const char *name, raptor_uri* format_uri)
{
  rasqal_query_results_format_factory* factory;
  rasqal_query_results_formatter* formatter;

  factory=rasqal_get_query_results_formatter_factory(name, format_uri, NULL);
  if(!factory)
    return NULL;

  formatter=(rasqal_query_results_formatter*)RASQAL_CALLOC(rasqal_query_results_formatter, 1, sizeof(rasqal_query_results_formatter));
  if(!formatter)
    return NULL;

  formatter->factory=factory;

  formatter->mime_type=factory->mime_type;
  
  return formatter;
}


/**
 * rasqal_new_query_results_formatter_by_mime_type:
 * @mime_type: mime type name
 *
 * Constructor - create a new rasqal_query_results_formatter object by mime type.
 *
 * A query results format generates a syntax with a mime type which
 * may be requested with this constructor.

 * Note that there may be several formatters that generate the same
 * MIME Type (such as SPARQL XML results format drafts) and in thot
 * case the rasqal_new_query_results_formatter() constructor allows
 * selecting of a specific one by name or URI.
 *
 * Return value: a new #rasqal_query_results_formatter object or NULL on failure
 */
rasqal_query_results_formatter*
rasqal_new_query_results_formatter_by_mime_type(const char *mime_type)
{
  rasqal_query_results_format_factory* factory;
  rasqal_query_results_formatter* formatter;

  if(!mime_type)
    return NULL;

  factory=rasqal_get_query_results_formatter_factory(NULL, NULL, mime_type);
  if(!factory)
    return NULL;

  formatter=(rasqal_query_results_formatter*)RASQAL_CALLOC(rasqal_query_results_formatter, 1, sizeof(rasqal_query_results_formatter));
  if(!formatter)
    return NULL;

  formatter->factory=factory;

  formatter->mime_type=factory->mime_type;
  
  return formatter;
}


/**
 * rasqal_free_query_results_formatter:
 * @formatter: #rasqal_query_results_formatter object
 * 
 * Destructor - destroy a #rasqal_query_results_formatter object.
 **/
void
rasqal_free_query_results_formatter(rasqal_query_results_formatter* formatter) 
{
  RASQAL_FREE(rasqal_query_results_formatter, formatter);
}


/*
 * rasqal_query_results_write_xml_result4:
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
rasqal_query_results_write_xml_result4(raptor_iostream *iostr,
                                       rasqal_query_results* results,
                                       raptor_uri *base_uri)
{
  int rc=1;
  rasqal_query* query=results->query;
  const raptor_uri_handler *uri_handler;
  void *uri_context;
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

  if(!rasqal_query_results_is_bindings(results) &&
     !rasqal_query_results_is_boolean(results)) {
    rasqal_query_error(query, "Can only write XML format v3 for variable binding and boolean results");
    return 1;
  }
  
  
  raptor_uri_get_handler(&uri_handler, &uri_context);

  nstack=raptor_new_namespaces(uri_handler, uri_context,
                               (raptor_simple_message_handler)rasqal_query_simple_error, query,
                               1);
  if(!nstack)
    return 1;

  xml_writer=raptor_new_xml_writer(nstack,
                                   uri_handler, uri_context,
                                   iostr,
                                   (raptor_simple_message_handler)rasqal_query_simple_error, query,
                                   1);
  if(!xml_writer)
    goto tidy;

  res_ns=raptor_new_namespace(nstack,
                              NULL,
                              (const unsigned char*)"http://www.w3.org/2005/sparql-results#",
                              0);
  if(!res_ns)
    goto tidy;

  sparql_element=raptor_new_xml_element_from_namespace_local_name(res_ns,
                                                                  (const unsigned char*)"sparql",
                                                                  NULL, base_uri);
  if(!sparql_element)
    goto tidy;

  if(rasqal_query_results_is_bindings(results)) {
    /* FIXME - consider when to write the XSD.  Need the XSD URI too. */
#if 0
    raptor_namespace* xsi_ns;
    xsi_ns=raptor_new_namespace(nstack,
                                (const unsigned char*)"xsi",
                                (const unsigned char*)"http://www.w3.org/2001/XMLSchema-instance",
                                0);
    raptor_xml_element_declare_namespace(sparql_element, xsi_ns);
    
    attrs=(raptor_qname **)raptor_alloc_memory(sizeof(raptor_qname*));
    attrs[0]=raptor_new_qname_from_namespace_local_name(xsi_ns,
                                                        (const unsigned char*)"schemaLocation",  
                                                        (const unsigned char*)"http://www.w3.org/2001/sw/DataAccess/rf1/result2.xsd");
    raptor_xml_element_set_attributes(sparql_element, attrs, 1);
#endif
  }
  
  raptor_xml_writer_start_element(xml_writer, sparql_element);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);

  /*   <head> */
  element1=raptor_new_xml_element_from_namespace_local_name(res_ns,
                                                            (const unsigned char*)"head",
                                                            NULL, base_uri);
  if(!element1)
    goto tidy;

  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"  ", 2);
  raptor_xml_writer_start_element(xml_writer, element1);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);
  
  if(rasqal_query_results_is_bindings(results)) {
    for(i=0; 1; i++) {
      const unsigned char *name;
      name=rasqal_query_results_get_binding_name(results, i);
      if(!name)
        break;
      
      /*     <variable name="x"/> */
      variable_element=raptor_new_xml_element_from_namespace_local_name(res_ns,
                                                                        (const unsigned char*)"variable",
                                                                        NULL, base_uri);
      if(!variable_element)
        goto tidy;
      
      attrs=(raptor_qname **)raptor_alloc_memory(sizeof(raptor_qname*));
      if(!attrs)
        goto tidy;
      attrs[0]=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                          (const unsigned char*)"name",
                                                          (const unsigned char*)name); /* attribute value */
      if(!attrs[0]) {
        raptor_free_memory((void*)attrs);
        goto tidy;
      }

      raptor_xml_element_set_attributes(variable_element, attrs, 1);
      
      raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"    ", 4);
      raptor_xml_writer_empty_element(xml_writer, variable_element);
      raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);
      
      raptor_free_xml_element(variable_element);
      variable_element=NULL;
    }
  }

  /* FIXME - could add <link> inside <head> */

    
  /*   </head> */
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"  ", 2);
  raptor_xml_writer_end_element(xml_writer, element1);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);
  
  raptor_free_xml_element(element1);
  element1=NULL;


  /* Boolean Results */
  if(rasqal_query_results_is_boolean(results)) {
    result_element=raptor_new_xml_element_from_namespace_local_name(res_ns,
                                                                    (const unsigned char*)"boolean",
                                                                    NULL, base_uri);
    if(!result_element)
      goto tidy;

    raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"  ", 2);
    raptor_xml_writer_start_element(xml_writer, result_element);
    if(rasqal_query_results_get_boolean(results))
      raptor_xml_writer_raw(xml_writer, RASQAL_XSD_BOOLEAN_TRUE);
    else
      raptor_xml_writer_raw(xml_writer, RASQAL_XSD_BOOLEAN_FALSE);
    raptor_xml_writer_end_element(xml_writer, result_element);
    raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);

    goto results3done;
  }


  /* Variable Binding Results */

  /*   <results> */
  results_element=raptor_new_xml_element_from_namespace_local_name(res_ns,
                                                                   (const unsigned char*)"results",
                                                                   NULL, base_uri);
  if(!results_element)
    goto tidy;

  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"  ", 2);
  raptor_xml_writer_start_element(xml_writer, results_element);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);


  /* declare result element for later multiple use */
  result_element=raptor_new_xml_element_from_namespace_local_name(res_ns,
                                                                  (const unsigned char*)"result",
                                                                  NULL, base_uri);
  if(!result_element)
    goto tidy;

  while(!rasqal_query_results_finished(results)) {
    /*     <result> */
    raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"    ", 4);
    raptor_xml_writer_start_element(xml_writer, result_element);
    raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);

    for(i=0; i<rasqal_query_results_get_bindings_count(results); i++) {
      const unsigned char *name=rasqal_query_results_get_binding_name(results, i);
      rasqal_literal *l=rasqal_query_results_get_binding_value(results, i);

      /*       <binding> */
      binding_element=raptor_new_xml_element_from_namespace_local_name(res_ns,
                                                                       (const unsigned char*)"binding",
                                                                       NULL, base_uri);
      if(!binding_element)
        goto tidy;

      attrs=(raptor_qname **)raptor_alloc_memory(sizeof(raptor_qname*));
      if(!attrs)
        goto tidy;
      attrs[0]=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                          (const unsigned char*)"name",
                                                          name);
      if(!attrs[0]) {
        raptor_free_memory((void*)attrs);
        goto tidy;
      }

      raptor_xml_element_set_attributes(binding_element, attrs, 1);
      

      raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"      ", 6);
      raptor_xml_writer_start_element(xml_writer, binding_element);

      if(!l) {
        element1=raptor_new_xml_element_from_namespace_local_name(res_ns,
                                                                  (const unsigned char*)"unbound",
                                                                  NULL, base_uri);
        if(!element1)
          goto tidy;
        raptor_xml_writer_empty_element(xml_writer, element1);

      } else switch(l->type) {
        case RASQAL_LITERAL_URI:
          element1=raptor_new_xml_element_from_namespace_local_name(res_ns,
                                                                    (const unsigned char*)"uri",
                                                                    NULL, base_uri);
          if(!element1)
            goto tidy;
          
          raptor_xml_writer_start_element(xml_writer, element1);
          raptor_xml_writer_cdata(xml_writer, (const unsigned char*)raptor_uri_as_string(l->value.uri));
          raptor_xml_writer_end_element(xml_writer, element1);

          break;

        case RASQAL_LITERAL_BLANK:
          element1=raptor_new_xml_element_from_namespace_local_name(res_ns,
                                                                    (const unsigned char*)"bnode",
                                                                    NULL, base_uri);
          if(!element1)
            goto tidy;
          
          raptor_xml_writer_start_element(xml_writer, element1);
          raptor_xml_writer_cdata(xml_writer, (const unsigned char*)l->string);
          raptor_xml_writer_end_element(xml_writer, element1);
          break;

        case RASQAL_LITERAL_STRING:
          element1=raptor_new_xml_element_from_namespace_local_name(res_ns,
                                                                    (const unsigned char*)"literal",
                                                                    NULL, base_uri);
          if(!element1)
            goto tidy;

          if(l->language || l->datatype) {
            attrs=(raptor_qname **)raptor_alloc_memory(sizeof(raptor_qname*));
            if(!attrs)
              goto tidy;

            if(l->language)
              attrs[0]=raptor_new_qname(nstack,
                                        (const unsigned char*)"xml:lang",
                                        (const unsigned char*)l->language,
                                        (raptor_simple_message_handler)rasqal_query_simple_error, query);
            else
              attrs[0]=raptor_new_qname_from_namespace_local_name(res_ns, 
                                                                  (const unsigned char*)"datatype",
                                                                  (const unsigned char*)raptor_uri_as_string(l->datatype));
            if(!attrs[0]) {
              raptor_free_memory((void*)attrs);
              goto tidy;
            }

            raptor_xml_element_set_attributes(element1, attrs, 1);
          }


          raptor_xml_writer_start_element(xml_writer, element1);


          raptor_xml_writer_cdata_counted(xml_writer,
                                          (const unsigned char*)l->string, 
                                          l->string_len);

          raptor_xml_writer_end_element(xml_writer, element1);
          
          break;
        case RASQAL_LITERAL_PATTERN:
        case RASQAL_LITERAL_QNAME:
        case RASQAL_LITERAL_INTEGER:
        case RASQAL_LITERAL_BOOLEAN:
        case RASQAL_LITERAL_DOUBLE:
        case RASQAL_LITERAL_FLOAT:
        case RASQAL_LITERAL_VARIABLE:
        case RASQAL_LITERAL_DECIMAL:
        case RASQAL_LITERAL_DATETIME:

        case RASQAL_LITERAL_UNKNOWN:
        default:
          rasqal_query_error(query, "Cannot turn literal type %d into XML", 
                             l->type);
      }

      if(element1) {
        raptor_free_xml_element(element1);
        element1=NULL;
      }

      /*       </binding> */
      raptor_xml_writer_end_element(xml_writer, binding_element);
      raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);
      
      raptor_free_xml_element(binding_element);
      binding_element=NULL;
    }

    raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"    ", 4);
    raptor_xml_writer_end_element(xml_writer, result_element);
    raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);
    
    rasqal_query_results_next(results);
  }

  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"  ", 2);
  raptor_xml_writer_end_element(xml_writer, results_element);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);

  results3done:

  rc=0;

  raptor_xml_writer_end_element(xml_writer, sparql_element);
  raptor_xml_writer_raw_counted(xml_writer, (const unsigned char*)"\n", 1);

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


/*
 * rasqal_query_results_read_xml_result4:
 * @iostr: #raptor_iostream to read the query results from
 * @results: #rasqal_query_results query results output
 * @base_uri: #raptor_uri base URI of the input format
 *
 * Read the fourth version of the SPARQL XML query results format from an
 * iostream in a format - INTERNAL.
 * 
 * If the reading succeeds, the query results will contain the results.
 * 
 * Return value: non-0 on failure
 **/
static int
rasqal_query_results_read_xml_result4(raptor_iostream *iostr,
                                      rasqal_query_results* results,
                                      raptor_uri *base_uri)
{
  int rc=1;
  rasqal_query* query=results->query;
  const raptor_uri_handler *uri_handler;
  void *uri_context;

  if(!rasqal_query_results_is_bindings(results) &&
     !rasqal_query_results_is_boolean(results)) {
    rasqal_query_error(query, "Can only write XML format v3 for variable binding and boolean results");
    return 1;
  }
  
  
  raptor_uri_get_handler(&uri_handler, &uri_context);

  /* FIXME - read the format here */

  rc=0;

  tidy;
  return rc;
}



static
void raptor_iostream_write_json_boolean(raptor_iostream* iostr, 
                                        const char* name, int json_bool)
{
  raptor_iostream_write_string(iostr, name);
  raptor_iostream_write_counted_string(iostr, "\" : ",4);

  if(json_bool)
    raptor_iostream_write_counted_string(iostr, "true", 4);
  else
    raptor_iostream_write_counted_string(iostr, "false", 5);

}


/*
 * rasqal_query_results_write_json1:
 * @iostr: #raptor_iostream to write the query to
 * @results: #rasqal_query_results query results format
 * @base_uri: #raptor_uri base URI of the output format
 *
 * Write a JSON version of the query results format to an
 * iostream in a format - INTERNAL.
 * 
 * If the writing succeeds, the query results will be exhausted.
 * 
 * Return value: non-0 on failure
 **/
static int
rasqal_query_results_write_json1(raptor_iostream *iostr,
                                 rasqal_query_results* results,
                                 raptor_uri *base_uri)
{
  rasqal_query* query=results->query;
  int i;
  int row_comma;
  int column_comma=0;
  
  if(!rasqal_query_results_is_bindings(results) &&
     !rasqal_query_results_is_boolean(results)) {
    rasqal_query_error(query, "Can only write JSON format for variable binding and boolean results");
    return 1;
  }
  
  
  raptor_iostream_write_counted_string(iostr, "{\n", 2);
  
  /* Header */
  raptor_iostream_write_counted_string(iostr, "  \"head\": {\n", 12);
  
  if(rasqal_query_results_is_bindings(results)) {
    raptor_iostream_write_counted_string(iostr, "    \"vars\": [ ", 14);
    for(i=0; 1; i++) {
      const unsigned char *name;
      
      name=rasqal_query_results_get_binding_name(results, i);
      if(!name)
        break;
      
      /*     'x', */
      if(i > 0)
        raptor_iostream_write_counted_string(iostr, ", ", 2);
      raptor_iostream_write_byte(iostr, '\"');
      raptor_iostream_write_string(iostr, name);
      raptor_iostream_write_byte(iostr, '\"');
    }
    raptor_iostream_write_counted_string(iostr, " ]\n", 3);
  }

  /* FIXME - could add link inside 'head': */
    
  /*   End Header */
  raptor_iostream_write_counted_string(iostr, "  },\n", 5);


  /* Boolean Results */
  if(rasqal_query_results_is_boolean(results)) {
    raptor_iostream_write_counted_string(iostr, "  ", 2);
    raptor_iostream_write_json_boolean(iostr, "boolean", 
                                       rasqal_query_results_get_boolean(results));
    goto results3done;
  }

  /* Variable Binding Results */
  raptor_iostream_write_counted_string(iostr, "  \"results\": {\n", 15);

  raptor_iostream_write_counted_string(iostr, "    \"", 5);
  raptor_iostream_write_json_boolean(iostr, "ordered", 
                                     (rasqal_query_get_order_condition(query, 0) != NULL));
  raptor_iostream_write_counted_string(iostr, ",\n", 2);

  raptor_iostream_write_counted_string(iostr, "    \"", 5);
  raptor_iostream_write_json_boolean(iostr, "distinct", 
                                     rasqal_query_get_distinct(query));
  raptor_iostream_write_counted_string(iostr, ",\n", 2);

  raptor_iostream_write_counted_string(iostr, "    \"bindings\" : [\n", 19);

  row_comma=0;
  while(!rasqal_query_results_finished(results)) {
    if(row_comma)
      raptor_iostream_write_counted_string(iostr, ",\n", 2);

    /* Result row */
    raptor_iostream_write_counted_string(iostr, "      {\n", 8);

    column_comma=0;
    for(i=0; i<rasqal_query_results_get_bindings_count(results); i++) {
      const unsigned char *name=rasqal_query_results_get_binding_name(results, i);
      rasqal_literal *l=rasqal_query_results_get_binding_value(results, i);

      if(column_comma)
        raptor_iostream_write_counted_string(iostr, ",\n", 2);

      /*       <binding> */
      raptor_iostream_write_counted_string(iostr, "        \"", 9);
      raptor_iostream_write_string(iostr, name);
      raptor_iostream_write_counted_string(iostr, "\" : { ", 6);

      if(!l) {
        raptor_iostream_write_string(iostr, "\"type\": \"unbound\", \"value\": null");
      } else switch(l->type) {
        const unsigned char* str;
        size_t len;
        
        case RASQAL_LITERAL_URI:
          raptor_iostream_write_string(iostr, "\"type\": \"uri\", \"value\": \"");
          str=(const unsigned char*)raptor_uri_as_counted_string(l->value.uri, &len);
          raptor_iostream_write_string_ntriples(iostr, str, len, '"');
          raptor_iostream_write_byte(iostr, '"');
          break;

        case RASQAL_LITERAL_BLANK:
          raptor_iostream_write_string(iostr, "\"type\": \"bnode\", \"value\": \"");
          raptor_iostream_write_string_ntriples(iostr, (const unsigned char*)l->string, 
                                                l->string_len, '"');
          raptor_iostream_write_byte(iostr, '"');
          break;

        case RASQAL_LITERAL_STRING:
          raptor_iostream_write_string(iostr, "\"type\": \"literal\", \"value\": \"");
          raptor_iostream_write_string_ntriples(iostr, (const unsigned char*)l->string,
                                                l->string_len, '"');
          raptor_iostream_write_byte(iostr, '"');

          if(l->language) {
            raptor_iostream_write_string(iostr, ",\n      \"xml:lang\" : \"");
            raptor_iostream_write_string(iostr, (const unsigned char*)l->language);
            raptor_iostream_write_byte(iostr, '"');
          }
          
          if(l->datatype) {
            raptor_iostream_write_string(iostr, ",\n      \"datatype\" : \"");
            str=(const unsigned char*)raptor_uri_as_counted_string(l->datatype, &len);
            raptor_iostream_write_string_ntriples(iostr, str, len, '"');
            raptor_iostream_write_byte(iostr, '"');
          }
          
          break;

        case RASQAL_LITERAL_PATTERN:
        case RASQAL_LITERAL_QNAME:
        case RASQAL_LITERAL_INTEGER:
        case RASQAL_LITERAL_BOOLEAN:
        case RASQAL_LITERAL_DOUBLE:
        case RASQAL_LITERAL_FLOAT:
        case RASQAL_LITERAL_VARIABLE:
        case RASQAL_LITERAL_DECIMAL:
        case RASQAL_LITERAL_DATETIME:

        case RASQAL_LITERAL_UNKNOWN:
        default:
          rasqal_query_error(query, "Cannot turn literal type %d into XML", 
                             l->type);
      }

      /* End Binding */
      raptor_iostream_write_counted_string(iostr, " }", 2);
      column_comma=1;
    }

    /* End Result Row */
    raptor_iostream_write_counted_string(iostr, "\n      }", 8);
    row_comma=1;
    
    rasqal_query_results_next(results);
  }

  raptor_iostream_write_counted_string(iostr, "\n    ]\n  }", 10);

  results3done:
  
  /* end sparql */
  raptor_iostream_write_counted_string(iostr, "\n}\n", 3);

  return 0;
}


/**
 * rasqal_query_results_formatter_get_mime_type:
 * @formatter: #rasqal_query_results_formatter object
 * 
 * Get the mime type of the syntax being formatted.
 * 
 * Return value: a shared mime type string
 **/
const char*
rasqal_query_results_formatter_get_mime_type(rasqal_query_results_formatter *formatter)
{
  return formatter->mime_type;
}


/**
 * rasqal_query_results_formatter_write:
 * @iostr: #raptor_iostream to write the query to
 * @formatter: #rasqal_query_results_formatter object
 * @results: #rasqal_query_results query results format
 * @base_uri: #raptor_uri base URI of the output format
 *
 * Write the query results using the given formatter to an iostream
 * 
 * See rasqal_query_results_formats_enumerate() to get the
 * list of syntax URIs and their description. 
 *
 * Return value: non-0 on failure
 **/
int
rasqal_query_results_formatter_write(raptor_iostream *iostr,
                                     rasqal_query_results_formatter* formatter,
                                     rasqal_query_results* results,
                                     raptor_uri *base_uri)
{
  if(!formatter->factory->writer)
     return 1;
  return formatter->factory->writer(iostr, results, base_uri);
}


/**
 * rasqal_query_results_formatter_read:
 * @iostr: #raptor_iostream to read the query from
 * @formatter: #rasqal_query_results_formatter object
 * @results: #rasqal_query_results query results format
 * @base_uri: #raptor_uri base URI of the input format
 *
 * Read the query results using the given formatter tfrom an iostream
 * 
 * See rasqal_query_results_formats_enumerate() to get the
 * list of syntax URIs and their description. 
 *
 * Return value: non-0 on failure
 **/
int
rasqal_query_results_formatter_read(raptor_iostream *iostr,
                                    rasqal_query_results_formatter* formatter,
                                    rasqal_query_results* results,
                                    raptor_uri *base_uri)
{
  if(!formatter->factory->reader)
     return 1;
  return formatter->factory->reader(iostr, results, base_uri);
}

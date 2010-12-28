/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_result_formats.c - Rasqal Query Result Format Class
 *
 * Copyright (C) 2003-2010, David Beckett http://www.dajobe.org/
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



/*
 * rasqal_world_register_query_result_format_factory:
 * @world: rasqal world
 * @factory: pointer to function to call to register the factory
 * 
 * INTERNAL - Register a query result format via a factory.
 *
 * All strings set in the @factory method are shared with the
 * #rasqal_query_result_format_factory
 *
 * Return value: new factory object or NULL on failure
 **/
rasqal_query_results_format_factory*
rasqal_world_register_query_results_format_factory(rasqal_world* world,
                                                   int (*factory) (rasqal_query_results_format_factory*))
{
  rasqal_query_results_format_factory *result_format = NULL;
  
  result_format = (rasqal_query_results_format_factory*)RASQAL_CALLOC(rasqal_query_result_format_factory, 1,
                                                                      sizeof(*result_format));
  if(!result_format)
    return NULL;

  result_format->world = world;

  result_format->desc.mime_types = NULL;
  
  if(raptor_sequence_push(world->query_results_formats, result_format))
    return NULL; /* on error, result_format is already freed by the sequence */
  
  /* Call the result_format registration function on the new object */
  if(factory(result_format))
    return NULL; /* result_format is owned and freed by the result_formats sequence */
  
  if(!result_format->desc.names || !result_format->desc.names[0] ||
     !result_format->desc.label) {
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                            "Result query result format failed to register required names and label fields\n");
    goto tidy;
  }

#ifdef RASQAL_DEBUG
  /* Maintainer only check of static data */
  if(result_format->desc.mime_types) {
    unsigned int i;
    const raptor_type_q* type_q = NULL;

    for(i = 0; 
        (type_q = &result_format->desc.mime_types[i]) && type_q->mime_type;
        i++) {
      size_t len = strlen(type_q->mime_type);
      if(len != type_q->mime_type_len) {
        fprintf(stderr,
                "Query result format %s  mime type %s  actual len %d  static len %d\n",
                result_format->desc.names[0], type_q->mime_type,
                (int)len, (int)type_q->mime_type_len);
      }
    }

    if(i != result_format->desc.mime_types_count) {
        fprintf(stderr,
                "Query result format %s  saw %d mime types  static count %d\n",
                result_format->desc.names[0], i, result_format->desc.mime_types_count);
    }
  }
#endif

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG3("Registered query result format %s with context size %d\n",
                result_format->names[0], result_format->context_length);
#endif

  return result_format;

  /* Clean up on failure */
  tidy:
  rasqal_free_query_results_format_factory(result_format);
  return NULL;
}


void
rasqal_free_query_results_format_factory(rasqal_query_results_format_factory* factory)
{
  RASQAL_FREE(query_results_format_factory, factory);
}


int
rasqal_init_result_formats(rasqal_world* world)
{
  int rc = 0;
  
#ifdef HAVE_RAPTOR2_API
  world->query_results_formats = raptor_new_sequence((raptor_data_free_handler)rasqal_free_query_results_format_factory, NULL);
#else
  world->query_results_formats = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_query_results_format_factory, NULL);
#endif
  if(!world->query_results_formats)
    return 1;

  rc += rasqal_init_result_format_sparql_xml(world) != 0;

  rc += rasqal_init_result_format_json(world) != 0;

  rc += rasqal_init_result_format_table(world) != 0;

  rc += rasqal_init_result_format_sv(world) != 0;

  rc += rasqal_init_result_format_html(world) != 0;

  rc += rasqal_init_result_format_turtle(world) != 0;

  rc += rasqal_init_result_format_rdf(world) != 0;

  return rc;
}


void
rasqal_finish_result_formats(rasqal_world* world)
{
  if(world->query_results_formats) {
    raptor_free_sequence(world->query_results_formats);
    world->query_results_formats = NULL;
  }
}


/**
 * rasqal_query_results_formats_enumerate:
 * @world: rasqal_world object
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
 * Table name '<literal>table</literal>' with no URI.
 *
 * All returned strings are shared and must be copied if needed to be
 * used dynamically.
 * 
 * Return value: non 0 on failure of if counter is out of range
 **/
int
rasqal_query_results_formats_enumerate(rasqal_world* world,
                                       unsigned int counter,
                                       const char **name,
                                       const char **label,
                                       const unsigned char **uri_string,
                                       const char **mime_type,
                                       int *flags)



{
  rasqal_query_results_format_factory *factory;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, 1);

  factory = (rasqal_query_results_format_factory*)raptor_sequence_get_at(world->query_results_formats, counter);
  if(!factory)
    return 1;

  if(name)
    *name = factory->desc.names[0];
  if(label)
    *label = factory->desc.label;
  if(uri_string)
    *uri_string = (const unsigned char*)factory->desc.uri_string;
  if(mime_type)
    *mime_type = (const char*)factory->desc.mime_types[0].mime_type;
  if(flags) {
    *flags = 0;
    if(factory->get_rowsource)
      *flags |= RASQAL_QUERY_RESULTS_FORMAT_FLAG_READER;
    if(factory->write)
      *flags |= RASQAL_QUERY_RESULTS_FORMAT_FLAG_WRITER;
  }
  
  return 0;
}


static rasqal_query_results_format_factory*
rasqal_get_query_results_formatter_factory(rasqal_world* world,
                                           const char *name, raptor_uri* uri,
                                           const char *mime_type,
                                           int flags)
{
  int i;
  rasqal_query_results_format_factory* factory = NULL;
  
  for(i = 0; 1; i++) {
    int factory_flags = 0;
    
    factory = (rasqal_query_results_format_factory*)raptor_sequence_get_at(world->query_results_formats,
                                                                           i);
    if(!factory)
      break;

    if(factory->get_rowsource)
      factory_flags |= RASQAL_QUERY_RESULTS_FORMAT_FLAG_READER;
    if(factory->write)
      factory_flags |= RASQAL_QUERY_RESULTS_FORMAT_FLAG_WRITER;

    /* Flags must match */
    if(flags && factory_flags != flags)
      continue;

    if(!name && !uri)
      /* the default is the first registered format */
      break;

    if(name) {
      int namei;
      const char* fname;
      
      for(namei = 0; (fname = factory->desc.names[namei]); namei++) {
        if(!strcmp(fname, name))
          return factory;
      }
    }

    if(uri && factory->desc.uri_string &&
       !strcmp((const char*)raptor_uri_as_string(uri),
               (const char*)factory->desc.uri_string))
      break;

    if(mime_type) {
      int mti;
      const char* mname;
      
      for(mti = 0; (mname = factory->desc.mime_types[mti].mime_type); mti++) {
        if(!strcmp(mname, mime_type))
          return factory;
      }
    }
  }
  
  return factory;
}


/**
 * rasqal_query_results_formats_check2:
 * @world: rasqal_world object
 * @name: the query results format name (or NULL)
 * @uri: #raptor_uri query results format uri (or NULL)
 * @mime_type: mime type name
 * @flags: bitmask of flags to signify that format is needed for reading (#RASQAL_QUERY_RESULTS_FORMAT_FLAG_READER ) or writing ( #RASQAL_QUERY_RESULTS_FORMAT_FLAG_WRITER )
 * 
 * Check if a query results formatter exists for the requested format.
 * 
 * Return value: non-0 if a formatter exists.
 **/
int
rasqal_query_results_formats_check2(rasqal_world* world,
                                    const char *name, raptor_uri* uri,
                                    const char *mime_type,
                                    int flags)
{
  rasqal_query_results_format_factory* factory = NULL;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, 1);

  factory = rasqal_get_query_results_formatter_factory(world,
                                                       name, uri, mime_type,
                                                       flags);
  return (factory != NULL);
}


/**
 * rasqal_query_results_formats_check:
 * @world: rasqal_world object
 * @name: the query results format name (or NULL)
 * @uri: #raptor_uri query results format uri (or NULL)
 * @mime_type: mime type name
 * 
 * Check if a query results formatter exists for the requested format.
 * 
 * @Deprecated: Use rasqal_query_results_formats_check() with extra flags argument.
 *
 * Return value: non-0 if a formatter exists.
 **/
int
rasqal_query_results_formats_check(rasqal_world* world,
                                   const char *name, raptor_uri* uri,
                                   const char *mime_type)
{
  return rasqal_query_results_formats_check2(world, name, uri, mime_type, 0);
}


/**
 * rasqal_new_query_results_formatter2:
 * @world: rasqal_world object
 * @name: the query results format name (or NULL)
 * @mime_type: the query results format mime type (or NULL)
 * @format_uri: #raptor_uri query results format uri (or NULL)
 *
 * Constructor - create a new rasqal_query_results_formatter for an identified format.
 *
 * A query results format can be found by name, mime type or URI, all
 * of which are optional.  If multiple fields are given, the first
 * match is given that matches the name, URI, mime_type in that
 * order.  The default query results format will be used if all are
 * format identifying fields are NULL.
 *
 * rasqal_query_results_formats_enumerate() returns information on
 * the known query results names, labels, mime types and URIs.
 *
 * Return value: a new #rasqal_query_results_formatter object or NULL on failure
 */
rasqal_query_results_formatter*
rasqal_new_query_results_formatter2(rasqal_world* world,
                                    const char *name, 
                                    const char *mime_type,
                                    raptor_uri* format_uri)
{
  rasqal_query_results_format_factory* factory;
  rasqal_query_results_formatter* formatter;
  int flags = 0;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);

  factory = rasqal_get_query_results_formatter_factory(world, name, 
                                                       format_uri, mime_type,
                                                       flags);
  if(!factory)
    return NULL;

  formatter = (rasqal_query_results_formatter*)RASQAL_CALLOC(rasqal_query_results_formatter, 1, sizeof(*formatter));
  if(!formatter)
    return NULL;

  formatter->factory = factory;

  formatter->context = NULL;
  if(factory->context_length) {
    formatter->context = (char*)RASQAL_CALLOC(rasqal_query_results_formatter_context, 1,
                                              factory->context_length);
    if(!formatter->context) {
      rasqal_free_query_results_formatter(formatter);
    return NULL;
    }
  }
  
  if(formatter->factory->init) {
    if(formatter->factory->init(formatter, name)) {
      rasqal_free_query_results_formatter(formatter);
      return NULL;
    }
  }
  
  return formatter;
}


/**
 * rasqal_new_query_results_formatter:
 * @world: rasqal_world object
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
 * @Deprecated: Use rasqal_new_query_results_formatter2() with extra
 * mime_type arg.
 *
 * Return value: a new #rasqal_query_results_formatter object or NULL on failure
 */
rasqal_query_results_formatter*
rasqal_new_query_results_formatter(rasqal_world* world,
                                   const char *name, raptor_uri* format_uri)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);

  return rasqal_new_query_results_formatter2(world, 
                                             name, NULL, format_uri);
}


/**
 * rasqal_new_query_results_formatter_by_mime_type:
 * @world: rasqal_world object
 * @mime_type: mime type name
 *
 * Constructor - create a new rasqal_query_results_formatter object by mime type.
 *
 * A query results format generates a syntax with a mime type which
 * may be requested with this constructor.
 *
 * Note that there may be several formatters that generate the same
 * MIME Type (such as SPARQL XML results format drafts) and in thot
 * case the rasqal_new_query_results_formatter() constructor allows
 * selecting of a specific one by name or URI.
 *
 * @Deprecated: Use rasqal_new_query_results_formatter2() with extra
 * name and format_uri args.
 *
 * Return value: a new #rasqal_query_results_formatter object or NULL on failure
 */
rasqal_query_results_formatter*
rasqal_new_query_results_formatter_by_mime_type(rasqal_world* world,
                                                const char *mime_type)
{
  return rasqal_new_query_results_formatter2(world, 
                                             NULL, mime_type, NULL);
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
  if(!formatter)
    return;

  if(formatter->context)
    RASQAL_FREE(context, formatter->context);

  RASQAL_FREE(rasqal_query_results_formatter, formatter);
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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(formatter, rasqal_query_results_formatter, NULL);

  return formatter->factory->desc.mime_types[0].mime_type;
}


/**
 * rasqal_query_results_formatter_write:
 * @iostr: #raptor_iostream to write the query to
 * @formatter: #rasqal_query_results_formatter object
 * @results: #rasqal_query_results query results format
 * @base_uri: #raptor_uri base URI of the output format (or NULL)
 *
 * Write the query results using the given formatter to an iostream
 *
 * Note that after calling this method, the query results will be
 * empty and rasqal_query_results_finished() will return true (non-0)
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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(iostr, raptor_iostream, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(formatter, rasqal_query_results_formatter, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(results, rasqal_query_results, 1);

  if(!formatter->factory->write)
     return 1;
  return formatter->factory->write(formatter, iostr, results, base_uri);
}


/**
 * rasqal_query_results_formatter_read:
 * @world: rasqal world object
 * @iostr: #raptor_iostream to read the query from
 * @formatter: #rasqal_query_results_formatter object
 * @results: #rasqal_query_results query results format
 * @base_uri: #raptor_uri base URI of the input format
 *
 * Read the query results using the given formatter from an iostream
 * 
 * See rasqal_query_results_formats_enumerate() to get the
 * list of syntax URIs and their description. 
 *
 * Return value: non-0 on failure
 **/
int
rasqal_query_results_formatter_read(rasqal_world *world,
                                    raptor_iostream *iostr,
                                    rasqal_query_results_formatter* formatter,
                                    rasqal_query_results* results,
                                    raptor_uri *base_uri)
{
  rasqal_rowsource* rowsource = NULL;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(iostr, raptor_iostream, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(formatter, rasqal_query_results_formatter, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(results, rasqal_query_results, 1);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(base_uri, raptor_uri, 1);

  if(!formatter->factory->get_rowsource)
    return 1;
  
  rowsource = formatter->factory->get_rowsource(formatter, world, 
                                                rasqal_query_results_get_variables_table(results),
                                                iostr, base_uri);
  if(!rowsource)
    return 1;

  while(1) {
    rasqal_row* row = rasqal_rowsource_read_row(rowsource);
    if(!row)
      break;
    rasqal_query_results_add_row(results, row);
  }

  if(rowsource)
    rasqal_free_rowsource(rowsource);
  
  return 0;
}

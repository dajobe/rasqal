/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_general.c - Rasqal library startup, shutdown and factories
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
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdarg.h>

#include "rasqal.h"
#include "rasqal_internal.h"


/* prototypes for helper functions */
static void rasqal_delete_query_engine_factories(void);


/* statics */

/* list of query factories */
static rasqal_query_engine_factory* query_engines=NULL;

const char * const rasqal_short_copyright_string = "Copyright (C) 2004 David Beckett, ILRT, University of Bristol";

const char * const rasqal_copyright_string = "Copyright (C) 2004 David Beckett - http://purl.org/net/dajobe/\nInstitute for Learning and Research Technology - http://www.ilrt.bristol.ac.uk/,\nUniversity of Bristol - http://www.bristol.ac.uk/";

const char * const rasqal_version_string = VERSION;

const unsigned int rasqal_version_major = RASQAL_VERSION_MAJOR;
const unsigned int rasqal_version_minor = RASQAL_VERSION_MINOR;
const unsigned int rasqal_version_release = RASQAL_VERSION_RELEASE;

const unsigned int rasqal_version_decimal = RASQAL_VERSION_DECIMAL;



/*
 * rasqal_init - Initialise the rasqal library
 * 
 * Initialises the library.
 *
 * MUST be called before using any of the rasqal APIs.
 **/
void
rasqal_init(void) 
{
  if(query_engines)
    return;

  raptor_init();

  /* last one declared is the default - RDQL */

#ifdef RASQAL_QUERY_BRQL  
  rasqal_init_query_engine_brql();
#endif

#ifdef RASQAL_QUERY_RDQL
  rasqal_init_query_engine_rdql();
#endif

#ifdef RAPTOR_TRIPLES_SOURCE_RAPTOR
  rasqal_raptor_init();
#endif
#ifdef RAPTOR_TRIPLES_SOURCE_REDLAND
  rasqal_redland_init();
#endif
}


/*
 * rasqal_finish - Terminate the rasqal library
 *
 * Cleans up state of the library.
 **/
void
rasqal_finish(void) 
{
  rasqal_delete_query_engine_factories();
#ifdef RAPTOR_TRIPLES_SOURCE_RAPTOR
  raptor_finish();
#endif
#ifdef RAPTOR_TRIPLES_SOURCE_REDLAND
  rasqal_redland_finish();
#endif
}


/* helper functions */


/*
 * rasqal_delete_query_engine_factories - helper function to delete all the registered query engine factories
 */
static void
rasqal_delete_query_engine_factories(void)
{
  rasqal_query_engine_factory *factory, *next;
  
  for(factory=query_engines; factory; factory=next) {
    next=factory->next;

    if(factory->finish_factory)
      factory->finish_factory(factory);

    RASQAL_FREE(rasqal_query_engine_factory, factory->name);
    RASQAL_FREE(rasqal_query_engine_factory, factory->label);
    if(factory->alias)
      RASQAL_FREE(rasqal_query_engine_factory, factory->alias);
    if(factory->uri_string)
      RASQAL_FREE(rasqal_query_engine_factory, factory->uri_string);

    RASQAL_FREE(rasqal_query_engine_factory, factory);
  }
  query_engines=NULL;
}


/* class methods */

/*
 * rasqal_query_engine_register_factory - Register a syntax handled by a query factory
 * @name: the short syntax name
 * @label: readable label for syntax
 * @uri_string: URI string of the syntax (or NULL)
 * @factory: pointer to function to call to register the factory
 * 
 * INTERNAL
 *
 **/
void
rasqal_query_engine_register_factory(const char *name, const char *label,
                                     const char *alias,
                                     const unsigned char *uri_string,
                                     void (*factory) (rasqal_query_engine_factory*)) 
{
  rasqal_query_engine_factory *query, *h;
  char *name_copy, *label_copy, *alias_copy;
  unsigned char *uri_string_copy;
  
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG4("Received registration for syntax %s '%s' with alias '%s'\n", 
                name, label, (alias ? alias : "none"));
  RASQAL_DEBUG2("URI %s\n", (uri_string ? (const char*)uri_string : (const char*)"none"));
#endif
  
  query=(rasqal_query_engine_factory*)RASQAL_CALLOC(rasqal_query_engine_factory, 1,
                                                    sizeof(rasqal_query_engine_factory));
  if(!query)
    RASQAL_FATAL1("Out of memory\n");

  for(h = query_engines; h; h = h->next ) {
    if(!strcmp(h->name, name) ||
       (alias && !strcmp(h->name, alias))) {
      RASQAL_FATAL2("query %s already registered\n", h->name);
    }
  }
  
  name_copy=(char*)RASQAL_CALLOC(cstring, strlen(name)+1, 1);
  if(!name_copy) {
    RASQAL_FREE(rasqal_query, query);
    RASQAL_FATAL1("Out of memory\n");
  }
  strcpy(name_copy, name);
  query->name=name_copy;
        
  label_copy=(char*)RASQAL_CALLOC(cstring, strlen(label)+1, 1);
  if(!label_copy) {
    RASQAL_FREE(rasqal_query, query);
    RASQAL_FATAL1("Out of memory\n");
  }
  strcpy(label_copy, label);
  query->label=label_copy;

  if(uri_string) {
    uri_string_copy=(unsigned char*)RASQAL_CALLOC(cstring, strlen((const char*)uri_string)+1, 1);
    if(!uri_string_copy) {
    RASQAL_FREE(rasqal_query, query);
    RASQAL_FATAL1("Out of memory\n");
    }
    strcpy((char*)uri_string_copy, (const char*)uri_string);
    query->uri_string=uri_string_copy;
  }
        
  if(alias) {
    alias_copy=(char*)RASQAL_CALLOC(cstring, strlen(alias)+1, 1);
    if(!alias_copy) {
      RASQAL_FREE(rasqal_query, query);
      RASQAL_FATAL1("Out of memory\n");
    }
    strcpy(alias_copy, alias);
    query->alias=alias_copy;
  }

  /* Call the query registration function on the new object */
  (*factory)(query);
  
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG3("%s has context size %d\n", name, (int)query->context_length);
#endif
  
  query->next = query_engines;
  query_engines = query;
}


/**
 * rasqal_get_query_engine_factory - Get a query factory by name
 * @name: the factory name or NULL for the default factory
 * @uri: the query syntax URI or NULL
 * 
 * Return value: the factory object or NULL if there is no such factory
 **/
rasqal_query_engine_factory*
rasqal_get_query_engine_factory (const char *name, const unsigned char *uri)
{
  rasqal_query_engine_factory *factory;

  /* return 1st query if no particular one wanted - why? */
  if(!name && !uri) {
    factory=query_engines;
    if(!factory) {
      RASQAL_DEBUG1("No (default) query_engines registered\n");
      return NULL;
    }
  } else {
    for(factory=query_engines; factory; factory=factory->next) {
      if((name && !strcmp(factory->name, name)) ||
         (factory->alias && !strcmp(factory->alias, name)))
        break;
      if(uri && !strcmp((const char*)factory->uri_string, (const char*)uri))
        break;
    }
    /* else FACTORY name not found */
    if(!factory) {
      RASQAL_DEBUG2("No query language with name %s found\n", name);
      return NULL;
    }
  }
        
  return factory;
}


/**
 * rasqal_languages_enumerate - Get information on query languages
 * @counter: index into the list of syntaxes
 * @name: pointer to store the name of the syntax (or NULL)
 * @label: pointer to store syntax readable label (or NULL)
 * @uri_string: pointer to store syntax URI string (or NULL)
 * 
 * Return value: non 0 on failure of if counter is out of range
 **/
int
rasqal_languages_enumerate(const unsigned int counter,
                           const char **name, const char **label,
                           const unsigned char **uri_string)
{
  unsigned int i;
  rasqal_query_engine_factory *factory=query_engines;

  if(!factory || counter < 0)
    return 1;

  for(i=0; factory && i<=counter ; i++, factory=factory->next) {
    if(i == counter) {
      if(name)
        *name=factory->name;
      if(label)
        *label=factory->label;
      if(uri_string)
        *uri_string=factory->uri_string;
      return 0;
    }
  }
        
  return 1;
}


/*
 * rasqal_language_name_check -  Check name of a query language
 * @name: the query language name
 *
 * Return value: non 0 if name is a known query language
 */
int
rasqal_language_name_check(const char *name) {
  return (rasqal_get_query_engine_factory(name, NULL) != NULL);
}


/*
 * rasqal_query_fatal_error - Fatal Error from a query - Internal
 **/
void
rasqal_query_fatal_error(rasqal_query* query, const char *message, ...)
{
  va_list arguments;

  va_start(arguments, message);

  rasqal_query_fatal_error_varargs(query, message, arguments);
  
  va_end(arguments);
}


/*
 * rasqal_query_fatal_error_varargs - Fatal Error from a query - Internal
 **/
void
rasqal_query_fatal_error_varargs(rasqal_query* query, const char *message,
                                 va_list arguments)
{
  query->failed=1;

  if(query->fatal_error_handler) {
    char *buffer=raptor_vsnprintf(message, arguments);
    if(!buffer) {
      fprintf(stderr, "rasqal_query_fatal_error_varargs: Out of memory\n");
      return;
    }

    query->fatal_error_handler(query->fatal_error_user_data, 
                                &query->locator, buffer); 
    RASQAL_FREE(cstring, buffer);
    abort();
  }

  raptor_print_locator(stderr, &query->locator);
  fprintf(stderr, " rasqal fatal error - ");
  vfprintf(stderr, message, arguments);
  fputc('\n', stderr);

  abort();
}


/*
 * rasqal_query_error - Error from a query - Internal
 **/
void
rasqal_query_error(rasqal_query* query, const char *message, ...)
{
  va_list arguments;

  va_start(arguments, message);

  rasqal_query_error_varargs(query, message, arguments);
  
  va_end(arguments);
}


/*
 * rasqal_query_simple_error - Error from a query - Internal
 *
 * Matches the rasqal_simple_message_handler API but same as
 * rasqal_query_error 
 **/
void
rasqal_query_simple_error(void* query, const char *message, ...)
{
  va_list arguments;

  va_start(arguments, message);

  rasqal_query_error_varargs((rasqal_query*)query, message, arguments);
  
  va_end(arguments);
}


/*
 * rasqal_query_error_varargs - Error from a query - Internal
 **/
void
rasqal_query_error_varargs(rasqal_query* query, const char *message, 
                           va_list arguments)
{
  query->failed=1;

  if(query->error_handler) {
    char *buffer=raptor_vsnprintf(message, arguments);
    if(!buffer) {
      fprintf(stderr, "rasqal_query_error_varargs: Out of memory\n");
      return;
    }
    query->error_handler(query->error_user_data, 
                          &query->locator, buffer);
    RASQAL_FREE(cstring, buffer);
    return;
  }

  raptor_print_locator(stderr, &query->locator);
  fprintf(stderr, " rasqal error - ");
  vfprintf(stderr, message, arguments);
  fputc('\n', stderr);
}


/*
 * rasqal_query_warning - Warning from a query - Internal
 **/
void
rasqal_query_warning(rasqal_query* query, const char *message, ...)
{
  va_list arguments;

  va_start(arguments, message);

  rasqal_query_warning_varargs(query, message, arguments);

  va_end(arguments);
}


/*
 * rasqal_query_warning - Warning from a query - Internal
 **/
void
rasqal_query_warning_varargs(rasqal_query* query, const char *message, 
                             va_list arguments)
{

  if(query->warning_handler) {
    char *buffer=raptor_vsnprintf(message, arguments);
    if(!buffer) {
      fprintf(stderr, "rasqal_query_warning_varargs: Out of memory\n");
      return;
    }
    query->warning_handler(query->warning_user_data,
                            &query->locator, buffer);
    RASQAL_FREE(cstring, buffer);
    return;
  }

  raptor_print_locator(stderr, &query->locator);
  fprintf(stderr, " rasqal warning - ");
  vfprintf(stderr, message, arguments);
  fputc('\n', stderr);
}



#if defined (RASQAL_DEBUG) && defined(HAVE_DMALLOC_H) && defined(RASQAL_MEMORY_DEBUG_DMALLOC)

#undef malloc
void*
rasqal_system_malloc(size_t size)
{
  return malloc(size);
}

#undef free
void
rasqal_system_free(void *ptr)
{
  return free(ptr);
  
}

#endif


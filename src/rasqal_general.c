/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_general.c - Rasqal library startup, shutdown and factories
 *
 * $Id$
 *
 * Copyright (C) 2004-2005, David Beckett http://purl.org/net/dajobe/
 * Institute for Learning and Research Technology http://www.ilrt.bristol.ac.uk/
 * University of Bristol, UK http://www.bristol.ac.uk/
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


/* prototypes for helper functions */
static void rasqal_delete_query_engine_factories(void);


/* statics */
static int rasqal_initialised=0;

static int rasqal_initialising=0;
static int rasqal_finishing=0;

/* list of query factories */
static rasqal_query_engine_factory* query_engines=NULL;

const char * const rasqal_short_copyright_string = "Copyright (C) 2003-2005 David Beckett, ILRT, University of Bristol";

const char * const rasqal_copyright_string = "Copyright (C) 2003-2005 David Beckett - http://purl.org/net/dajobe/\nInstitute for Learning and Research Technology - http://www.ilrt.bristol.ac.uk/,\nUniversity of Bristol - http://www.bristol.ac.uk/";

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
  if(rasqal_initialised || rasqal_initialising)
    return;
  rasqal_initialising=1;

  raptor_init();

  rasqal_uri_init();

  /* last one declared is the default - RDQL */

#ifdef RASQAL_QUERY_RDQL
  rasqal_init_query_engine_rdql();
#endif

#ifdef RASQAL_QUERY_SPARQL  
  rasqal_init_query_engine_sparql();
#endif

#ifdef RAPTOR_TRIPLES_SOURCE_RAPTOR
  rasqal_raptor_init();
#endif
#ifdef RAPTOR_TRIPLES_SOURCE_REDLAND
  rasqal_redland_init();
#endif

  rasqal_initialising=0;
  rasqal_initialised=1;
  rasqal_finishing=0;
}


/*
 * rasqal_finish - Terminate the rasqal library
 *
 * Cleans up state of the library.
 **/
void
rasqal_finish(void) 
{
  if(!rasqal_initialised || rasqal_finishing)
    return;
  rasqal_finishing=1;

  rasqal_delete_query_engine_factories();

#ifdef RAPTOR_TRIPLES_SOURCE_REDLAND
  rasqal_redland_finish();
#endif

  rasqal_uri_finish();

  raptor_finish();

  rasqal_initialising=0;
  rasqal_initialised=0;
  rasqal_finishing=0;
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



/* wrapper */
const char*
rasqal_basename(const char *name)
{
  char *p;
  if((p=strrchr(name, '/')))
    name=p+1;
  else if((p=strrchr(name, '\\')))
    name=p+1;

  return name;
}


/**
 * rasqal_escaped_name_to_utf8_string - get a UTF-8 and/or \u-escaped name as UTF-8
 * @src: source name string
 * @len: length of source name string
 * @dest_lenp: pointer to store result string (or NULL)
 * @error_handler: error handling function
 * @error_data: data for error handle
 *
 * If dest_lenp is not NULL, the length of the resulting string is
 * stored at the pointed size_t.
 *
 * Return value: new UTF-8 string or NULL on failure.
 */
unsigned char*
rasqal_escaped_name_to_utf8_string(const unsigned char *src, size_t len,
                                   size_t *dest_lenp,
                                   raptor_simple_message_handler error_handler,
                                   void *error_data) {
  const unsigned char *p=src;
  size_t ulen=0;
  unsigned long unichar=0;
  unsigned char *result;
  unsigned char *dest;

  result=(unsigned char*)RASQAL_MALLOC(cstring, len+1);
  if(!result)
    return NULL;

  dest=result;

  /* find end of string, fixing backslashed characters on the way */
  while(len > 0) {
    unsigned char c=*p;

    if(c > 0x7f) {
      /* just copy the UTF-8 bytes through */
      size_t unichar_len=raptor_utf8_to_unicode_char(NULL, (const unsigned char*)p, len+1);
      if(unichar_len < 0 || unichar_len > len) {
        if(error_handler)
          error_handler(error_data, "UTF-8 encoding error at character %d (0x%02X) found.", c, c);
        /* UTF-8 encoding had an error or ended in the middle of a string */
        RASQAL_FREE(cstring, result);
        return NULL;
      }
      memcpy(dest, p, unichar_len);
      dest+= unichar_len;
      p += unichar_len;
      len -= unichar_len;
      continue;
    }

    p++; len--;
    
    if(c != '\\') {
      /* not an escape - store and move on */
      *dest++=c;
      continue;
    }

    if(!len) {
      RASQAL_FREE(cstring, result);
      return NULL;
    }

    c = *p++; len--;

    switch(c) {
      case '"':
      case '\\':
        *dest++=c;
        break;
      case 'u':
      case 'U':
        ulen=(c == 'u') ? 4 : 8;
        
        if(len < ulen) {
          if(error_handler)
            error_handler(error_data, "%c over end of line", c);
          RASQAL_FREE(cstring, result);
          return 0;
        }
        
        sscanf((const char*)p, ((ulen == 4) ? "%04lx" : "%08lx"), &unichar);

        p+=ulen;
        len-=ulen;
        
        if(unichar < 0 || unichar > 0x10ffff) {
          if(error_handler)
            error_handler(error_data, "Illegal Unicode character with code point #x%lX.", unichar);
          break;
        }
          
        dest+=raptor_unicode_char_to_utf8(unichar, dest);
        break;

      default:
        if(error_handler)
          error_handler(error_data, "Illegal string escape \\%c in \"%s\"", c, src);
        RASQAL_FREE(cstring, result);
        return 0;
    }

  } /* end while */

  
  /* terminate dest, can be shorter than source */
  *dest='\0';

  if(dest_lenp)
    *dest_lenp=p-src;

  return result;
}


raptor_uri* rasqal_xsd_namespace_uri=NULL;

raptor_uri* rasqal_xsd_integer_uri=NULL;
raptor_uri* rasqal_xsd_double_uri=NULL;
raptor_uri* rasqal_xsd_boolean_uri=NULL;

raptor_uri* rasqal_rdf_namespace_uri=NULL;

raptor_uri* rasqal_rdf_first_uri=NULL;
raptor_uri* rasqal_rdf_rest_uri=NULL;
raptor_uri* rasqal_rdf_nil_uri=NULL;


void
rasqal_uri_init() 
{
  rasqal_xsd_namespace_uri=raptor_new_uri(raptor_xmlschema_datatypes_namespace_uri);
  
  rasqal_xsd_integer_uri=raptor_new_uri_from_uri_local_name(rasqal_xsd_namespace_uri, (const unsigned char*)"integer");
  rasqal_xsd_double_uri=raptor_new_uri_from_uri_local_name(rasqal_xsd_namespace_uri, (const unsigned char*)"double");
  rasqal_xsd_boolean_uri=raptor_new_uri_from_uri_local_name(rasqal_xsd_namespace_uri, (const unsigned char*)"boolean");

  rasqal_rdf_namespace_uri=raptor_new_uri(raptor_rdf_namespace_uri);

  rasqal_rdf_first_uri=raptor_new_uri_from_uri_local_name(rasqal_rdf_namespace_uri, (const unsigned char*)"first");
  rasqal_rdf_rest_uri=raptor_new_uri_from_uri_local_name(rasqal_rdf_namespace_uri, (const unsigned char*)"rest");
  rasqal_rdf_nil_uri=raptor_new_uri_from_uri_local_name(rasqal_rdf_namespace_uri, (const unsigned char*)"nil");
  
}


void
rasqal_uri_finish() 
{
  if(rasqal_xsd_integer_uri)
    raptor_free_uri(rasqal_xsd_integer_uri);
  if(rasqal_xsd_double_uri)
    raptor_free_uri(rasqal_xsd_double_uri);
  if(rasqal_xsd_boolean_uri)
    raptor_free_uri(rasqal_xsd_boolean_uri);
  if(rasqal_xsd_namespace_uri)
    raptor_free_uri(rasqal_xsd_namespace_uri);

  if(rasqal_rdf_first_uri)
    raptor_free_uri(rasqal_rdf_first_uri);
  if(rasqal_rdf_rest_uri)
    raptor_free_uri(rasqal_rdf_rest_uri);
  if(rasqal_rdf_nil_uri)
    raptor_free_uri(rasqal_rdf_nil_uri);
  if(rasqal_rdf_namespace_uri)
    raptor_free_uri(rasqal_rdf_namespace_uri);

}



/**
 * rasqal_query_set_default_generate_bnodeid_parameters - Set default bnodeid generation parameters
 * @rdf_query: &rasqal_parse object
 * @prefix: prefix string
 * @base: integer base identifier
 *
 * Sets the parameters for the default algorithm used to generate
 * blank node IDs.  The default algorithm uses both @prefix and @base
 * to generate a new identifier.  The exact identifier generated is
 * not guaranteed to be a strict concatenation of @prefix and @base
 * but will use both parts.
 *
 * For finer control of the generated identifiers, use
 * &rasqal_set_default_generate_bnodeid_handler.
 *
 * If prefix is NULL, the default prefix is used (currently "bnodeid")
 * If base is less than 1, it is initialised to 1.
 * 
 **/
void
rasqal_query_set_default_generate_bnodeid_parameters(rasqal_query* rdf_query, 
                                                     char *prefix, int base)
{
  char *prefix_copy=NULL;
  size_t length=0;

  if(--base<0)
    base=0;

  if(prefix) {
    length=strlen(prefix);
    
    prefix_copy=(char*)RASQAL_MALLOC(cstring, length+1);
    if(!prefix_copy)
      return;
    strcpy(prefix_copy, prefix);
  }
  
  if(rdf_query->default_generate_bnodeid_handler_prefix)
    RASQAL_FREE(cstring, rdf_query->default_generate_bnodeid_handler_prefix);

  rdf_query->default_generate_bnodeid_handler_prefix=prefix_copy;
  rdf_query->default_generate_bnodeid_handler_prefix_length=length;
  rdf_query->default_generate_bnodeid_handler_base=base;
}


/**
 * rasqal_query_set_generate_bnodeid_handler - Set the generate blank node ID handler function for the query
 * @query: &rasqal_query query object
 * @user_data: user data pointer for callback
 * @handler: generate blank ID callback function
 *
 * Sets the function to generate blank node IDs for the query.
 * The handler is called with a pointer to the rasqal_query, the
 * &user_data pointer and a user_bnodeid which is the value of
 * a user-provided blank node identifier (may be NULL).
 * It can either be returned directly as the generated value when present or
 * modified.  The passed in value must be free()d if it is not used.
 *
 * If handler is NULL, the default method is used
 * 
 **/
void
rasqal_query_set_generate_bnodeid_handler(rasqal_query* query,
                                          void *user_data,
                                          rasqal_generate_bnodeid_handler handler)
{
  query->generate_bnodeid_handler_user_data=user_data;
  query->generate_bnodeid_handler=handler;
}


static unsigned char*
rasqal_default_generate_bnodeid_handler(void *user_data,
                                        unsigned char *user_bnodeid) 
{
  rasqal_query *rdf_query=(rasqal_query *)user_data;
  int id;
  unsigned char *buffer;
  int length;
  int tmpid;

  if(user_bnodeid)
    return user_bnodeid;

  id=++rdf_query->default_generate_bnodeid_handler_base;

  tmpid=id;
  length=2; /* min length 1 + \0 */
  while(tmpid/=10)
    length++;

  if(rdf_query->default_generate_bnodeid_handler_prefix)
    length += rdf_query->default_generate_bnodeid_handler_prefix_length;
  else
    length += 7; /* bnodeid */
  
  buffer=(unsigned char*)RASQAL_MALLOC(cstring, length);
  if(!buffer)
    return NULL;
  if(rdf_query->default_generate_bnodeid_handler_prefix) {
    strncpy((char*)buffer, rdf_query->default_generate_bnodeid_handler_prefix,
            rdf_query->default_generate_bnodeid_handler_prefix_length);
    sprintf((char*)buffer + rdf_query->default_generate_bnodeid_handler_prefix_length,
            "%d", id);
  } else 
    sprintf((char*)buffer, "bnodeid%d", id);

  return buffer;
}


/*
 * rasqal_query_generate_bnodeid - Default generate id - internal
 */
unsigned char*
rasqal_query_generate_bnodeid(rasqal_query* rdf_query,
                              unsigned char *user_bnodeid)
{
  if(rdf_query->generate_bnodeid_handler)
    return rdf_query->generate_bnodeid_handler(rdf_query, 
                                               rdf_query->generate_bnodeid_handler_user_data, user_bnodeid);
  else
    return rasqal_default_generate_bnodeid_handler(rdf_query, user_bnodeid);
}




#if defined (RASQAL_DEBUG) && defined(RASQAL_MEMORY_SIGN)
void*
rasqal_sign_malloc(size_t size)
{
  int *p;
  
  size += sizeof(int);
  
  p=(int*)malloc(size);
  *p++ = RASQAL_SIGN_KEY;
  return p;
}

void*
rasqal_sign_calloc(size_t nmemb, size_t size)
{
  int *p;
  
  /* turn into bytes */
  size = nmemb*size + sizeof(int);
  
  p=(int*)calloc(1, size);
  *p++ = RASQAL_SIGN_KEY;
  return p;
}

void*
rasqal_sign_realloc(void *ptr, size_t size)
{
  int *p;

  if(!ptr)
    return rasqal_sign_malloc(size);
  
  p=(int*)ptr;
  p--;

  if(*p != RASQAL_SIGN_KEY)
    RASQAL_FATAL3("memory signature %08X != %08X", *p, RASQAL_SIGN_KEY);

  size += sizeof(int);
  
  p=(int*)realloc(p, size);
  *p++= RASQAL_SIGN_KEY;
  return p;
}

void
rasqal_sign_free(void *ptr)
{
  int *p;

  if(!ptr)
    return;
  
  p=(int*)ptr;
  p--;

  if(*p != RASQAL_SIGN_KEY)
    RASQAL_FATAL3("memory signature %08X != %08X", *p, RASQAL_SIGN_KEY);

  free(p);
}
#endif


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


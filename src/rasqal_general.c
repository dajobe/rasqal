/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_general.c - Rasqal library startup, shutdown and factories
 *
 * Copyright (C) 2004-2014, David Beckett http://www.dajobe.org/
 * Copyright (C) 2004-2005, University of Bristol, UK http://www.bristol.ac.uk/
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

#ifdef MAINTAINER_MODE
#include <git-version.h>
#endif

/* prototypes for helper functions */
static void rasqal_delete_query_language_factories(rasqal_world*);
static void rasqal_free_query_language_factory(rasqal_query_language_factory *factory);


/* statics */

const char * const rasqal_short_copyright_string = "Copyright 2003-2014 David Beckett.  Copyright 2003-2005 University of Bristol";

const char * const rasqal_copyright_string = "Copyright (C) 2003-2014 David Beckett - http://www.dajobe.org/\nCopyright (C) 2003-2005 University of Bristol - http://www.bristol.ac.uk/";

const char * const rasqal_license_string = "LGPL 2.1 or newer, GPL 2 or newer, Apache 2.0 or newer.\nSee http://librdf.org/rasqal/LICENSE.html for full terms.";

const char * const rasqal_home_url_string = "http://librdf.org/rasqal/";

/**
 * rasqal_version_string:
 *
 * Library full version as a string.
 *
 * See also #rasqal_version_decimal.
 */
const char * const rasqal_version_string = RASQAL_VERSION_STRING
#ifdef GIT_VERSION
" GIT " GIT_VERSION
#endif
;

/**
 * rasqal_version_major:
 *
 * Library major version number as a decimal integer.
 */
const unsigned int rasqal_version_major = RASQAL_VERSION_MAJOR;

/**
 * rasqal_version_minor:
 *
 * Library minor version number as a decimal integer.
 */
const unsigned int rasqal_version_minor = RASQAL_VERSION_MINOR;

/**
 * rasqal_version_release:
 *
 * Library release version number as a decimal integer.
 */
const unsigned int rasqal_version_release = RASQAL_VERSION_RELEASE;

/**
 * rasqal_version_decimal:
 *
 * Library full version as a decimal integer.
 *
 * See also #rasqal_version_string.
 */
const unsigned int rasqal_version_decimal = RASQAL_VERSION;


/**
 * rasqal_new_world:
 * 
 * Allocate a new rasqal_world object.
 *
 * The rasqal_world is initialized with rasqal_world_open().
 * Allocation and initialization are decoupled to allow
 * changing settings on the world object before init.
 *
 * Return value: rasqal_world object or NULL on failure
 **/
rasqal_world*
rasqal_new_world(void)
{
  rasqal_world* world;

  world = RASQAL_CALLOC(rasqal_world*, 1, sizeof(*world));
  if(!world)
    return NULL;
  
  world->warning_level = RASQAL_WARNING_LEVEL_DEFAULT;

  world->genid_counter = 1;

  return world;
}


/**
 * rasqal_world_open:
 * @world: rasqal_world object
 * 
 * Initialise the rasqal library.
 *
 * Initializes a #rasqal_world object created by rasqal_new_world().
 * Allocation and initialization are decoupled to allow
 * changing settings on the world object before init.
 * These settings include e.g. the raptor library instance set with
 * rasqal_world_set_raptor().
 *
 * The initialized world object is used with subsequent rasqal API calls.
 *
 * Return value: non-0 on failure
 **/
int
rasqal_world_open(rasqal_world *world)
{
  int rc;

  if(!world)
    return -1;
    
  if(world->opened++)
    return 0; /* not an error */

  /* Create and init a raptor_world unless one is provided externally with rasqal_world_set_raptor() */
  if(!world->raptor_world_ptr) {
    world->raptor_world_ptr = raptor_new_world();
    if(!world->raptor_world_ptr)
      return -1;
    world->raptor_world_allocated_here = 1;
    rc = raptor_world_open(world->raptor_world_ptr);
    if(rc)
      return rc;
  }

  rc = rasqal_uri_init(world);
  if(rc)
    return rc;

  rc = rasqal_xsd_init(world);
  if(rc)
    return rc;

  world->query_languages = raptor_new_sequence((raptor_data_free_handler)rasqal_free_query_language_factory, NULL);
  if(!world->query_languages)
    return 1;
  
  /* first query language declared is the default */

#ifdef RASQAL_QUERY_SPARQL  
  rc = rasqal_init_query_language_sparql(world);
  if(rc)
    return rc;

  rc = rasqal_init_query_language_sparql11(world);
  if(rc)
    return rc;
#endif

#ifdef RASQAL_QUERY_LAQRS
  rc = rasqal_init_query_language_laqrs(world);
  if(rc)
    return rc;
#endif

  rc = rasqal_raptor_init(world);
  if(rc)
    return rc;

  rc = rasqal_init_query_results();
  if(rc)
    return rc;
  
  rc = rasqal_init_result_formats(world);
  if(rc)
    return rc;

  return 0;
}


/**
 * rasqal_free_world:
 * @world: rasqal_world object
 * 
 * Terminate the rasqal library.
 *
 * Destroys a rasqal_world object and all static information.
 *
 **/
void
rasqal_free_world(rasqal_world* world) 
{
  if(!world)
    return;
  
  rasqal_finish_result_formats(world);
  rasqal_finish_query_results();

  rasqal_delete_query_language_factories(world);

#ifdef RAPTOR_TRIPLES_SOURCE_REDLAND
  rasqal_redland_finish();
#endif

  rasqal_xsd_finish(world);

  rasqal_uri_finish(world);

  if(world->raptor_world_ptr && world->raptor_world_allocated_here)
    raptor_free_world(world->raptor_world_ptr);

  RASQAL_FREE(rasqal_world, world);
}


/**
 * rasqal_world_set_raptor:
 * @world: rasqal_world object
 * @raptor_world_ptr: raptor_world object
 * 
 * Set the #raptor_world instance to be used with this #rasqal_world.
 *
 * If no raptor_world instance is set with this function,
 * rasqal_world_open() creates a new instance.
 *
 * Ownership of the raptor_world is not taken. If the raptor library
 * instance is set with this function, rasqal_free_world() will not
 * free it.
 *
 **/
void
rasqal_world_set_raptor(rasqal_world* world, raptor_world* raptor_world_ptr)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(world, rasqal_world);
  world->raptor_world_ptr = raptor_world_ptr;
}


/**
 * rasqal_world_get_raptor:
 * @world: rasqal_world object
 * 
 * Get the #raptor_world instance used by this #rasqal_world.
 *
 * Return value: raptor_world object or NULL on failure (e.g. not initialized)
 **/
raptor_world*
rasqal_world_get_raptor(rasqal_world* world)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);
  return world->raptor_world_ptr;
}


/**
 * rasqal_world_set_log_handler:
 * @world: rasqal_world object
 * @user_data: user data for log handler function
 * @handler: log handler function
 *
 * Set the log handler for this rasqal_world.
 *
 * Also sets the raptor log handler to the same @user_data and
 * @handler via raptor_world_set_log_handler(). (Rasqal 0.9.26+)
 **/
void
rasqal_world_set_log_handler(rasqal_world* world, void *user_data,
                             raptor_log_handler handler)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(world, rasqal_world);

  world->log_handler = handler;
  world->log_handler_user_data = user_data;

  raptor_world_set_log_handler(world->raptor_world_ptr, user_data, handler);
}


/* helper functions */

/*
 * rasqal_free_query_language_factory - delete a query language factory
 */
static void
rasqal_free_query_language_factory(rasqal_query_language_factory *factory)
{
  if(!factory)
    return;
  
  if(factory->finish_factory)
    factory->finish_factory(factory);

  RASQAL_FREE(rasqal_query_language_factory, factory);
}


/*
 * rasqal_delete_query_language_factories - helper function to delete all the registered query language factories
 */
static void
rasqal_delete_query_language_factories(rasqal_world *world)
{
  if(world->query_languages) {
    raptor_free_sequence(world->query_languages);
    world->query_languages = NULL;
  }
}


/* class methods */

/*
 * rasqal_query_language_register_factory:
 * @factory: pointer to function to call to register the factory
 * 
 * INTERNAL - Register a query language syntax handled by a query factory
 *
 * Return value: new factory or NULL on failure
 **/
RASQAL_EXTERN_C
rasqal_query_language_factory*
rasqal_query_language_register_factory(rasqal_world *world,
                                       int (*factory) (rasqal_query_language_factory*))
{
  rasqal_query_language_factory *query = NULL;
  
  query = RASQAL_CALLOC(rasqal_query_language_factory*, 1, sizeof(*query));
  if(!query)
    goto tidy;

  query->world = world;
  
  if(raptor_sequence_push(world->query_languages, query))
    return NULL; /* on error, query is already freed by the sequence */
  
  /* Call the query registration function on the new object */
  if(factory(query))
    return NULL; /* query is owned and freed by the query_languages sequence */
  
  if(raptor_syntax_description_validate(&query->desc)) {
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                            "Query language format description failed to validate\n");
    goto tidy;
  }

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG3("Registered query language %s with context size %d\n",
                query->desc.names[0], query->context_length);
#endif

  return query;

  /* Clean up on failure */
  tidy:
  if(query)
    rasqal_free_query_language_factory(query);

  return NULL;
}


/**
 * rasqal_get_query_language_factory:
 * @name: the factory name or NULL for the default factory
 * @uri: the query syntax URI or NULL
 *
 * Get a query factory by name.
 * 
 * Return value: the factory object or NULL if there is no such factory
 **/
rasqal_query_language_factory*
rasqal_get_query_language_factory(rasqal_world *world, const char *name,
                                  const unsigned char *uri)
{
  rasqal_query_language_factory *factory = NULL;

  /* return 1st query language if no particular one wanted - why? */
  if(!name) {
    factory = (rasqal_query_language_factory*)raptor_sequence_get_at(world->query_languages, 0);
    if(!factory) {
      RASQAL_DEBUG1("No (default) query languages registered\n");
      return NULL;
    }
  } else {
    int i;
    
    for(i = 0;
        (factory = (rasqal_query_language_factory*)raptor_sequence_get_at(world->query_languages, i));
        i++) {
      int namei;
      const char* fname;
      
      for(namei = 0; (fname = factory->desc.names[namei]); namei++) {
        if(!strcmp(fname, name))
          break;
      }
      if(fname)
        break;
    }
  }
        
  return factory;
}


/**
 * rasqal_world_get_query_language_description:
 * @world: world object
 * @counter: index into the list of query languages
 *
 * Get query language descriptive information
 *
 * Return value: description or NULL if counter is out of range
 **/
const raptor_syntax_description*
rasqal_world_get_query_language_description(rasqal_world* world,
                                            unsigned int counter)
{
  rasqal_query_language_factory *factory;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);

  rasqal_world_open(world);

  factory = (rasqal_query_language_factory *)raptor_sequence_get_at(world->query_languages,
                                                                    RASQAL_GOOD_CAST(int, counter));
  if(!factory)
    return NULL;

  return &factory->desc;
}


#ifndef RASQAL_DISABLE_DEPRECATED
/**
 * rasqal_languages_enumerate:
 * @world: rasqal_world object
 * @counter: index into the list of syntaxes
 * @name: pointer to store the name of the syntax (or NULL)
 * @label: pointer to store syntax readable label (or NULL)
 * @uri_string: pointer to store syntax URI string (or NULL)
 *
 * @deprecated: Use rasqal_world_get_query_language_description() instead.
 *
 * Get information on query languages.
 * 
 * Return value: non 0 on failure of if counter is out of range
 **/
int
rasqal_languages_enumerate(rasqal_world *world,
                           unsigned int counter,
                           const char **name, const char **label,
                           const unsigned char **uri_string)
{
  rasqal_query_language_factory *factory;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, 1);
  if(!name && !label && !uri_string)
    return 1;
  
  /* for compatibility with old API that does not call this - FIXME Remove V2 */
  rasqal_world_open(world);
  
  factory = (rasqal_query_language_factory*)raptor_sequence_get_at(world->query_languages,
                                                                   RASQAL_GOOD_CAST(int, counter));
  if(!factory)
    return 1;

  if(name)
    *name = factory->desc.names[0];

  if(label)
    *label = factory->desc.label;

  if(uri_string && factory->desc.uri_strings)
    *uri_string = RASQAL_GOOD_CAST(const unsigned char*, factory->desc.uri_strings[0]);

  return 0;
}
#endif

/**
 * rasqal_language_name_check:
 * @world: rasqal_world object
 * @name: the query language name
 *
 * Check name of a query language.
 *
 * Return value: non 0 if name is a known query language
 */
int
rasqal_language_name_check(rasqal_world* world, const char *name)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, 0);

  return (rasqal_get_query_language_factory(world, name, NULL) != NULL);
}


static const char* const rasqal_log_level_labels[RAPTOR_LOG_LEVEL_LAST+1]={
 "none",
 "trace",
 "debug",
 "info",
 "warn",
 "error",
 "fatal"
};



/* internal */
void
rasqal_log_error_simple(rasqal_world* world, raptor_log_level level,
                        raptor_locator* locator, const char* message, ...)
{
  va_list arguments;

  if(level == RAPTOR_LOG_LEVEL_NONE)
    return;

  va_start(arguments, message);
  rasqal_log_error_varargs(world, level, locator, message, arguments);
  va_end(arguments);
}


/* internal */
void
rasqal_log_warning_simple(rasqal_world* world,
                          rasqal_warning_level warn_level,
                          raptor_locator* locator, const char* message, ...)
{
  va_list arguments;

  if(warn_level > world->warning_level)
    return;

  va_start(arguments, message);
  rasqal_log_error_varargs(world, RAPTOR_LOG_LEVEL_WARN,
                           locator, message, arguments);
  va_end(arguments);
}


void
rasqal_log_error_varargs(rasqal_world* world, raptor_log_level level,
                         raptor_locator* locator,
                         const char* message, va_list arguments)
{
  char *buffer;
  size_t length;
  raptor_log_message logmsg;
  raptor_log_handler handler = world->log_handler;
  void* handler_data = world->log_handler_user_data;
  
  if(level == RAPTOR_LOG_LEVEL_NONE)
    return;

  buffer = NULL;
  if(raptor_vasprintf(&buffer, message, arguments) < 0)
    buffer = NULL;

  if(!buffer) {
    if(locator) {
      raptor_locator_print(locator, stderr);
      fputc(' ', stderr);
    }
    fputs("rasqal ", stderr);
    fputs(rasqal_log_level_labels[level], stderr);
    fputs(" - ", stderr);
    vfprintf(stderr, message, arguments);
    fputc('\n', stderr);
    return;
  }

  length=strlen(buffer);
  if(buffer[length-1]=='\n')
    buffer[length-1]='\0';
  
  if(handler) {
    /* This is the single place in rasqal that the user error handler
     * functions are called.
     */
    /* raptor2 raptor_log_handler */
    logmsg.code = -1; /* no information to put as code */
    logmsg.level = level;
    logmsg.locator = locator;
    logmsg.text = buffer;
    handler(handler_data, &logmsg);
  } else {
    if(locator) {
      raptor_locator_print(locator, stderr);
      fputc(' ', stderr);
    }
    fputs("rasqal ", stderr);
    fputs(rasqal_log_level_labels[level], stderr);
    fputs(" - ", stderr);
    fputs(buffer, stderr);
    fputc('\n', stderr);
  }

  RASQAL_FREE(char*, buffer);
}


/*
 * rasqal_query_simple_error - Error from a query - Internal
 *
 * Matches the raptor_simple_message_handler API but same as
 * rasqal_query_error 
 */
void
rasqal_query_simple_error(void* user_data, const char *message, ...)
{
  rasqal_query* query=(rasqal_query*)user_data;

  va_list arguments;

  va_start(arguments, message);

  query->failed=1;
  rasqal_log_error_varargs(query->world,
                           RAPTOR_LOG_LEVEL_ERROR, NULL,
                           message, arguments);
  
  va_end(arguments);
}


/*
 * rasqal_world_simple_error:
 *
 * INTERNAL - Handle a simple error
 *
 * Matches the raptor_simple_message_handler API but with a world object
 */
void
rasqal_world_simple_error(void* user_data, const char *message, ...)
{
  rasqal_world* world = (rasqal_world*)user_data;

  va_list arguments;

  va_start(arguments, message);

  rasqal_log_error_varargs(world,
                           RAPTOR_LOG_LEVEL_ERROR, NULL,
                           message, arguments);
  
  va_end(arguments);
}


/* wrapper */
const char*
rasqal_basename(const char *name)
{
  const char *p;
  if((p = strrchr(name, '/')))
    name = p+1;
  else if((p = strrchr(name, '\\')))
    name = p+1;

  return name;
}


const raptor_unichar rasqal_unicode_max_codepoint = 0x10FFFF;

/**
 * rasqal_escaped_name_to_utf8_string:
 * @src: source name string
 * @len: length of source name string
 * @dest_lenp: pointer to store result string (or NULL)
 * @error_handler: error handling function
 * @error_data: data for error handle
 *
 * Get a UTF-8 and/or \u-escaped name as UTF-8.
 *
 * If dest_lenp is not NULL, the length of the resulting string is
 * stored at the pointed size_t.
 *
 * Return value: new UTF-8 string or NULL on failure.
 */
unsigned char*
rasqal_escaped_name_to_utf8_string(const unsigned char *src, size_t len,
                                   size_t *dest_lenp,
                                   int (*error_handler)(rasqal_query *error_data, const char *message, ...),
                                   rasqal_query* error_data)
{
  const unsigned char *p=src;
  size_t ulen=0;
  unsigned long unichar=0;
  unsigned char *result;
  unsigned char *dest;
  unsigned char *endp;
  int n;
  
  result = RASQAL_MALLOC(unsigned char*, len + 1);
  if(!result)
    return NULL;

  dest = result;
  endp = result + len;

  /* find end of string, fixing backslashed characters on the way */
  while(len > 0) {
    unsigned char c=*p;

    if(c > 0x7f) {
      /* just copy the UTF-8 bytes through */
      size_t unichar_len = RASQAL_GOOD_CAST(size_t, raptor_unicode_utf8_string_get_char(RASQAL_GOOD_CAST(const unsigned char*, p), len + 1, NULL));
      if(unichar_len > len) {
        if(error_handler)
          error_handler(error_data, "UTF-8 encoding error at character %d (0x%02X) found.", c, c);
        /* UTF-8 encoding had an error or ended in the middle of a string */
        RASQAL_FREE(char*, result);
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
      RASQAL_FREE(char*, result);
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
          RASQAL_FREE(char*, result);
          return 0;
        }
        
        n = sscanf(RASQAL_GOOD_CAST(const char*, p), ((ulen == 4) ? "%04lx" : "%08lx"), &unichar);
        if(n != 1) {
          if(error_handler)
            error_handler(error_data, "Bad %c escape", c);
          break;
        }
        
        p+=ulen;
        len-=ulen;
        
        if(unichar > 0x10ffff) {
          if(error_handler)
            error_handler(error_data, "Illegal Unicode character with code point #x%lX.", unichar);
          break;
        }
          
        dest += raptor_unicode_utf8_string_put_char(unichar, dest,
                                                    RASQAL_GOOD_CAST(size_t, endp - dest));
        break;

      default:
        if(error_handler)
          error_handler(error_data, "Illegal string escape \\%c in \"%s\"", c, src);
        RASQAL_FREE(char*, result);
        return 0;
    }

  } /* end while */

  
  /* terminate dest, can be shorter than source */
  *dest='\0';

  if(dest_lenp)
    *dest_lenp = RASQAL_GOOD_CAST(size_t, dest - result);

  return result;
}


int
rasqal_uri_init(rasqal_world* world) 
{
  world->rdf_namespace_uri = raptor_new_uri(world->raptor_world_ptr, raptor_rdf_namespace_uri);
  if(!world->rdf_namespace_uri)
    goto oom;

  world->rdf_first_uri = raptor_new_uri_from_uri_local_name(world->raptor_world_ptr, world->rdf_namespace_uri, RASQAL_GOOD_CAST(const unsigned char*, "first"));
  world->rdf_rest_uri = raptor_new_uri_from_uri_local_name(world->raptor_world_ptr, world->rdf_namespace_uri, RASQAL_GOOD_CAST(const unsigned char*, "rest"));
  world->rdf_nil_uri = raptor_new_uri_from_uri_local_name(world->raptor_world_ptr, world->rdf_namespace_uri, RASQAL_GOOD_CAST(const unsigned char*, "nil"));
  if(!world->rdf_first_uri || !world->rdf_rest_uri || !world->rdf_nil_uri)
    goto oom;

  return 0;

  oom:
  rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_FATAL,
                          NULL,
                          "Out of memory in rasqal_uri_init()");
  return 1;
}


void
rasqal_uri_finish(rasqal_world* world) 
{
  if(world->rdf_first_uri) {
    raptor_free_uri(world->rdf_first_uri);
    world->rdf_first_uri=NULL;
  }
  if(world->rdf_rest_uri) {
    raptor_free_uri(world->rdf_rest_uri);
    world->rdf_rest_uri=NULL;
  }
  if(world->rdf_nil_uri) {
    raptor_free_uri(world->rdf_nil_uri);
    world->rdf_nil_uri=NULL;
  }
  if(world->rdf_namespace_uri) {
    raptor_free_uri(world->rdf_namespace_uri);
    world->rdf_namespace_uri=NULL;
  }
}


/**
 * rasqal_world_set_default_generate_bnodeid_parameters:
 * @world: #rasqal_world object
 * @prefix: prefix string
 * @base: integer base identifier
 *
 * Set default bnodeid generation parameters
 *
 * Sets the parameters for the default algorithm used to generate
 * blank node IDs.  The default algorithm uses both @prefix and @base
 * to generate a new identifier.  The exact identifier generated is
 * not guaranteed to be a strict concatenation of @prefix and @base
 * but will use both parts.
 *
 * For finer control of the generated identifiers, use
 * rasqal_world_set_generate_bnodeid_handler()
 *
 * If prefix is NULL, the default prefix is used (currently "bnodeid")
 * If base is less than 1, it is initialised to 1.
 *
 * Return value: non-0 on failure
 **/
int
rasqal_world_set_default_generate_bnodeid_parameters(rasqal_world* world, 
                                                     char *prefix, int base)
{
  char *prefix_copy = NULL;
  size_t length = 0;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, 1);

  if(--base < 0)
    base = 0;

  if(prefix) {
    length = strlen(prefix);
    
    prefix_copy = RASQAL_MALLOC(char*, length + 1);
    if(!prefix_copy)
      return 1;
    memcpy(prefix_copy, prefix, length + 1);
  }
  
  if(world->default_generate_bnodeid_handler_prefix)
    RASQAL_FREE(char*, world->default_generate_bnodeid_handler_prefix);

  world->default_generate_bnodeid_handler_prefix = prefix_copy;
  world->default_generate_bnodeid_handler_prefix_length = length;
  world->default_generate_bnodeid_handler_base = base;

  return 0;
}


/**
 * rasqal_world_set_generate_bnodeid_handler:
 * @world: #rasqal_world object
 * @user_data: user data pointer for callback
 * @handler: generate blank ID callback function
 *
 * Set the generate blank node ID handler function
 *
 * Sets the function to generate blank node IDs.
 * The handler is called with a pointer to the rasqal_world, the
 * @user_data pointer and a user_bnodeid which is the value of
 * a user-provided blank node identifier (may be NULL).
 * It can either be returned directly as the generated value when present or
 * modified.  The passed in value must be free()d if it is not used.
 *
 * If handler is NULL, the default method is used
 *
 * Return value: non-0 on failure
 **/
int
rasqal_world_set_generate_bnodeid_handler(rasqal_world* world,
                                          void *user_data,
                                          rasqal_generate_bnodeid_handler handler)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, 1);

  world->generate_bnodeid_handler_user_data = user_data;
  world->generate_bnodeid_handler = handler;

  return 0;  
}


unsigned char*
rasqal_world_default_generate_bnodeid_handler(void *user_data,
                                              unsigned char *user_bnodeid) 
{
  rasqal_world *world = (rasqal_world*)user_data;
  int id;
  unsigned char *buffer;
  size_t length;
  int tmpid;

  if(user_bnodeid)
    return user_bnodeid;

  id = ++world->default_generate_bnodeid_handler_base;

  tmpid = id;
  length = 2; /* min length 1 + \0 */
  while(tmpid /= 10)
    length++;

  if(world->default_generate_bnodeid_handler_prefix)
    length += world->default_generate_bnodeid_handler_prefix_length;
  else
    length += 7; /* bnodeid */
  
  buffer = RASQAL_MALLOC(unsigned char*, length);
  if(!buffer)
    return NULL;
  if(world->default_generate_bnodeid_handler_prefix) {
    memcpy(buffer, world->default_generate_bnodeid_handler_prefix,
           world->default_generate_bnodeid_handler_prefix_length);
    sprintf(RASQAL_GOOD_CAST(char*, buffer) + world->default_generate_bnodeid_handler_prefix_length,
            "%d", id);
  } else 
    sprintf(RASQAL_GOOD_CAST(char*, buffer), "bnodeid%d", id);

  return buffer;
}


/*
 * rasqal_generate_bnodeid:
 *
 * Internal - Default generate ID
 */
unsigned char*
rasqal_world_generate_bnodeid(rasqal_world* world,
                              unsigned char *user_bnodeid)
{
  if(world->generate_bnodeid_handler)
    return world->generate_bnodeid_handler(world, 
                                           world->generate_bnodeid_handler_user_data, user_bnodeid);
  else
    return rasqal_world_default_generate_bnodeid_handler(world, user_bnodeid);
}


/**
 * rasqal_world_reset_now:
 * @world: world
 *
 * Internal - Mark current now value as invalid
 *
 * Intended to be run before starting a query so that the
 * value is recalculated.
 *
 * Return value: non-0 on failure
 */
int
rasqal_world_reset_now(rasqal_world* world)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, 1);

  world->now_set = 0;

  return 0;
}

#ifndef HAVE_GETTIMEOFDAY
#define gettimeofday(x,y) rasqal_gettimeofday(x,y)
#endif

/**
 * rasqal_world_get_now_timeval:
 * @world: world
 *
 * Internal - Get current now timeval
 *
 * Return value: pointer to timeval or NULL on failure
 */
struct timeval*
rasqal_world_get_now_timeval(rasqal_world* world)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);

  if(!world->now_set) {
    if(gettimeofday(&world->now, NULL))
      return NULL;
    
    world->now_set = 1;
  }

  return &world->now;
}


/**
 * rasqal_world_set_warning_level:
 * @world: world
 * @warning_level: warning level 0..100
 *
 * Set the rasqal warning reporting level
 *
 * The warning levels used are as follows:
 * <orderedlist>
 * <listitem><para>Level 10 is used for serious warnings that may be errors.
 * </para></listitem>
 * <listitem><para>Level 30 is used for moderate style warnings.
 * </para></listitem>
 * <listitem><para>Level 90 is used for strict conformance warnings.
 * </para></listitem>
 * </orderedlist>
 *
 * When this method is called to set a warning level, only warnings
 * of less than @warning_level are reported.  The default warning
 * level is 50.
 *
 * Return value: non-0 on failure
 */
int
rasqal_world_set_warning_level(rasqal_world* world,
                               unsigned int warning_level)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, 1);

  if(warning_level > RASQAL_WARNING_LEVEL_MAX)
    return 1;
  
  world->warning_level = (rasqal_warning_level)warning_level;
  
  return 0;
}


/**
 * rasqal_free_memory:
 * @ptr: memory pointer
 *
 * Free memory allocated inside rasqal.
 * 
 * Some systems require memory allocated in a library to
 * be deallocated in that library.  This function allows
 * memory allocated by rasqal to be freed.
 *
 **/
void
rasqal_free_memory(void *ptr)
{
  if(!ptr)
    return;
  
  RASQAL_FREE(void, ptr);
}


/**
 * rasqal_alloc_memory:
 * @size: size of memory to allocate
 *
 * Allocate memory inside rasqal.
 * 
 * Some systems require memory allocated in a library to
 * be deallocated in that library.  This function allows
 * memory to be allocated inside the rasqal shared library
 * that can be freed inside rasqal either internally or via
 * rasqal_free_memory().
 *
 * Return value: the address of the allocated memory or NULL on failure
 *
 **/
void*
rasqal_alloc_memory(size_t size)
{
  return RASQAL_MALLOC(void*, size);
}


/**
 * rasqal_calloc_memory:
 * @nmemb: number of members
 * @size: size of item
 *
 * Allocate zeroed array of items inside rasqal.
 * 
 * Some systems require memory allocated in a library to
 * be deallocated in that library.  This function allows
 * memory to be allocated inside the rasqal shared library
 * that can be freed inside rasqal either internally or via
 * rasqal_free_memory().
 *
 * Return value: the address of the allocated memory or NULL on failure
 *
 **/
void*
rasqal_calloc_memory(size_t nmemb, size_t size)
{
  return RASQAL_CALLOC(void*, nmemb, size);
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

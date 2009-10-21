/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_literal.c - Rasqal literals
 *
 * Copyright (C) 2003-2009, David Beckett http://www.dajobe.org/
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
/* for isnan() */
#include <math.h>

#ifdef RASQAL_REGEX_PCRE
#include <pcre.h>
#endif

#ifdef RASQAL_REGEX_POSIX
#include <sys/types.h>
#include <regex.h>
#endif

#include "rasqal.h"
#include "rasqal_internal.h"

/* prototypes */
static rasqal_literal_type rasqal_literal_promote_numerics(rasqal_literal* l1, rasqal_literal* l2, int flags);
static int rasqal_literal_set_typed_value(rasqal_literal* l, rasqal_literal_type type, const unsigned char* string);


/**
 * rasqal_new_integer_literal:
 * @world: rasqal world object
 * @type: Type of literal such as RASQAL_LITERAL_INTEGER or RASQAL_LITERAL_BOOLEAN
 * @integer: int value
 *
 * Constructor - Create a new Rasqal integer literal.
 * 
 * The integer decimal number is turned into a rasqal integer literal
 * and given a datatype of xsd:integer
 * 
 * Return value: New #rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_integer_literal(rasqal_world* world, rasqal_literal_type type, int integer)
{
  raptor_uri* dt_uri;
  rasqal_literal* l=(rasqal_literal*)RASQAL_CALLOC(rasqal_literal, 1, sizeof(rasqal_literal));
  if(l) {
    l->valid=1;
    l->usage=1;
    l->world=world;
    l->type=type;
    l->value.integer=integer;
    l->string = rasqal_xsd_format_integer(integer, (size_t*)&l->string_len);
    if(!l->string) {
      rasqal_free_literal(l);
      return NULL;
    }
    dt_uri=rasqal_xsd_datatype_type_to_uri(world, l->type);
    if(!dt_uri) {
      rasqal_free_literal(l);
      return NULL;
    }
#ifdef RAPTOR_V2_AVAILABLE
    l->datatype = raptor_uri_copy_v2(world->raptor_world_ptr, dt_uri);
#else
    l->datatype = raptor_uri_copy(dt_uri);
#endif
    l->parent_type = rasqal_xsd_datatype_parent_type(type);
  }
  return l;
}


/**
 * rasqal_new_typed_literal:
 * @world: rasqal world object
 * @type: Type of literal such as RASQAL_LITERAL_INTEGER or RASQAL_LITERAL_BOOLEAN
 * @string: lexical form - ownership not taken
 *
 * Constructor - Create a new Rasqal integer literal from a string
 * 
 * The integer decimal number is turned into a rasqal integer literal
 * and given a datatype of xsd:integer
 * 
 * Return value: New #rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_typed_literal(rasqal_world* world, rasqal_literal_type type, const unsigned char* string)
{
  rasqal_literal* l=(rasqal_literal*)RASQAL_CALLOC(rasqal_literal, 1,
                                                   sizeof(rasqal_literal));
  if(!l)
    return NULL;

  l->valid=1;
  l->usage=1;
  l->world=world;
  l->type=type;

  if(!rasqal_xsd_datatype_check(type, string, 0)) {
    rasqal_free_literal(l);
    return NULL;
  }

  if(rasqal_literal_set_typed_value(l, type, string)) {
    rasqal_free_literal(l);
    l=NULL;
  }

  return l;
}


/**
 * rasqal_new_double_literal:
 * @world: rasqal world object
 * @d: double literal
 *
 * Constructor - Create a new Rasqal double literal.
 * 
 * Return value: New #rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_double_literal(rasqal_world*world, double d)
{
  raptor_uri* dt_uri;
  rasqal_literal* l=(rasqal_literal*)RASQAL_CALLOC(rasqal_literal, 1, sizeof(rasqal_literal));
  if(l) {
    l->valid=1;
    l->usage=1;
    l->world=world;
    l->type=RASQAL_LITERAL_DOUBLE;
    l->value.floating=d;
    l->string=rasqal_xsd_format_double(d, (size_t*)&l->string_len);
    if(!l->string) {
      rasqal_free_literal(l);
      return NULL;
    }
    dt_uri=rasqal_xsd_datatype_type_to_uri(world, l->type);
    if(!dt_uri) {
      rasqal_free_literal(l);
      return NULL;
    }
#ifdef RAPTOR_V2_AVAILABLE
    l->datatype = raptor_uri_copy_v2(world->raptor_world_ptr, dt_uri);
#else
    l->datatype = raptor_uri_copy(dt_uri);
#endif
  }
  return l;
}


/**
 * rasqal_new_float_literal:
 * @world: rasqal world object
 * @f:  float literal
 * 
 * Constructor - Create a new Rasqal float literal.
 *
 * Return value: New #rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_float_literal(rasqal_world *world, float f)
{
  raptor_uri* dt_uri;
  rasqal_literal* l=(rasqal_literal*)RASQAL_CALLOC(rasqal_literal, 1, sizeof(rasqal_literal));
  if(l) {
    l->valid=1;
    l->usage=1;
    l->world=world;
    l->type=RASQAL_LITERAL_FLOAT;
    l->value.floating=(double)f;
    l->string = rasqal_xsd_format_double(f, (size_t*)&l->string_len);
    if(!l->string) {
      rasqal_free_literal(l);
      return NULL;
    }
    dt_uri=rasqal_xsd_datatype_type_to_uri(world, l->type);
    if(!dt_uri) {
      rasqal_free_literal(l);
      return NULL;
    }
#ifdef RAPTOR_V2_AVAILABLE
    l->datatype = raptor_uri_copy_v2(world->raptor_world_ptr, dt_uri);
#else
    l->datatype = raptor_uri_copy(dt_uri);
#endif
  }
  return l;
}


/**
 * rasqal_new_uri_literal:
 * @world: rasqal world object
 * @uri: #raptor_uri uri
 *
 * Constructor - Create a new Rasqal URI literal from a raptor URI.
 *
 * The uri is an input parameter and is stored in the literal, not copied.
 * The uri is freed also on failure.
 * 
 * Return value: New #rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_uri_literal(rasqal_world* world, raptor_uri *uri)
{
  rasqal_literal* l=(rasqal_literal*)RASQAL_CALLOC(rasqal_literal, 1, sizeof(rasqal_literal));
  if(l) {
    l->valid=1;
    l->usage=1;
    l->world=world;
    l->type=RASQAL_LITERAL_URI;
    l->value.uri=uri;
  } else {
#ifdef RAPTOR_V2_AVAILABLE
    raptor_free_uri_v2(world->raptor_world_ptr, uri);
#else
    raptor_free_uri(uri);
#endif
  }
  return l;
}


/**
 * rasqal_new_pattern_literal:
 * @world: rasqal world object
 * @pattern: regex pattern
 * @flags: regex flags
 *
 * Constructor - Create a new Rasqal pattern literal.
 *
 * The pattern and flags are input parameters and are stored in the
 * literal, not copied. They are freed also on failure.
 * The set of flags recognised depends on the regex library and the query
 * language.
 * 
 * Return value: New #rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_pattern_literal(rasqal_world* world,
                           const unsigned char *pattern, 
                           const char *flags)
{
  rasqal_literal* l=(rasqal_literal*)RASQAL_CALLOC(rasqal_literal, 1, sizeof(rasqal_literal));
  if(l) {
    l->valid=1;
    l->usage=1;
    l->world=world;
    l->type=RASQAL_LITERAL_PATTERN;
    l->string=pattern;
    l->string_len=strlen((const char*)pattern);
    l->flags=(const unsigned char*)flags;
  } else {
    if(flags)
      RASQAL_FREE(cstring, (void*)flags);
    RASQAL_FREE(cstring, (void*)pattern);
  }
  return l;
}


/**
 * rasqal_new_decimal_literal:
 * @world: rasqal world object
 * @string: decimal literal
 *
 * Constructor - Create a new Rasqal decimal literal.
 * 
 * Return value: New #rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_decimal_literal(rasqal_world* world, const unsigned char *string)
{
  return rasqal_new_decimal_literal_from_decimal(world, string, NULL);
}


/**
 * rasqal_new_decimal_literal_from_decimal:
 * @world: rasqal world object
 * @string: decimal literal string
 * @decimal: rasqal XSD Decimal
 *
 * Constructor - Create a new Rasqal decimal literal.
 * 
 * Return value: New #rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_decimal_literal_from_decimal(rasqal_world* world,
                                        const unsigned char *string,
                                        rasqal_xsd_decimal* decimal)
{
  rasqal_literal* l;
  raptor_uri *dt_uri;

  l=(rasqal_literal*)RASQAL_CALLOC(rasqal_literal, 1, sizeof(rasqal_literal));
  if(!l)
    return NULL;
  
  l->valid=1;
  l->usage=1;
  l->world=world;
  l->type=RASQAL_LITERAL_DECIMAL;
  if(string) {
    if(!rasqal_xsd_datatype_check(l->type, string, 0)) {
      rasqal_free_literal(l);
      return NULL;
    }

    if(rasqal_literal_set_typed_value(l, l->type, string)) {
      rasqal_free_literal(l);
      l=NULL;
    }
  } else if(decimal) {
    dt_uri=rasqal_xsd_datatype_type_to_uri(world, l->type);
    if(!dt_uri) {
      rasqal_free_literal(l);
      l=NULL;
    } else {
#ifdef RAPTOR_V2_AVAILABLE
      l->datatype = raptor_uri_copy_v2(world->raptor_world_ptr, dt_uri);
#else
      l->datatype = raptor_uri_copy(dt_uri);
#endif
      l->value.decimal=decimal;
      /* string is owned by l->value.decimal */
      l->string=(unsigned char*)rasqal_xsd_decimal_as_counted_string(l->value.decimal,
                                                                     (size_t*)&l->string_len);
      if(!l->string) {
        rasqal_free_literal(l);
        l=NULL;
      }
    }
  } else {
    /* no string or decimal was given */
    rasqal_free_literal(l);
    l=NULL;
  }
  
  return l;
}


/**
 * rasqal_new_numeric_literal:
 * @world: rasqal world object
 * @type: datatype
 * @double: double
 *
 * INTERNAL - Make a numeric datatype from a double  
 *
 * Return value: new literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_numeric_literal(rasqal_world* world, rasqal_literal_type type, double d)
{
  char buffer[30];
  
  switch(type) {
    case RASQAL_LITERAL_INTEGER:
      return rasqal_new_integer_literal(world, type, (int)d);
      break;

    case RASQAL_LITERAL_DOUBLE:
      return rasqal_new_double_literal(world, d);
      break;

    case RASQAL_LITERAL_FLOAT:
      return rasqal_new_float_literal(world, d);
      break;

    case RASQAL_LITERAL_DECIMAL:
      sprintf(buffer, "%g", d);
      return rasqal_new_decimal_literal(world, (unsigned char*)buffer);
      break;

    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_UNKNOWN:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_VARIABLE:
    case RASQAL_LITERAL_UDT:
      RASQAL_FATAL2("Unexpected numeric type %d\n", type);
  }

  return NULL;
}


static int
rasqal_literal_string_interpreted_as_boolean(const unsigned char* string) 
{
  int integer = 0;
  if(!strcmp((const char*)string, "true") || 
     !strcmp((const char*)string, "TRUE") ||
     !strcmp((const char*)string, "1"))
    integer = 1;

  return integer;
}


/*
 * rasqal_literal_set_typed_value:
 * @l: literal
 * @type: type
 * @string: string or NULL to use existing literal string
 *
 * INTERNAL - Set a literal typed value
 *
 * Return value: non-0 on failure
 **/
static int
rasqal_literal_set_typed_value(rasqal_literal* l, rasqal_literal_type type,
                               const unsigned char* string)
{  
  char *eptr;
  raptor_uri* dt_uri;
  int i;
  double d;
  const unsigned char *new_string;

  l->valid = rasqal_xsd_datatype_check(type, string ? string : l->string,
                                       0 /* no flags set */);
  if(!l->valid) {
    RASQAL_DEBUG3("Invalid type %s string '%s' - setting to type UDT\n",
                  rasqal_xsd_datatype_label(type), string ? string : l->string);
    type = RASQAL_LITERAL_UDT;
  }
            
  if(l->language) {
    RASQAL_FREE(cstring, (void*)l->language);
    l->language=NULL;
  }
  l->type=type;

  if(string) {
    if(l->string)
      RASQAL_FREE(cstring, (void*)l->string);
    l->string_len=strlen((const char*)string);
    l->string=(unsigned char*)RASQAL_MALLOC(cstring, l->string_len+1);
    if(!l->string)
      return 1;
    strncpy((char*)l->string, (const char*)string, l->string_len+1);
  }

  dt_uri=rasqal_xsd_datatype_type_to_uri(l->world, l->type);
  if(!dt_uri)
    return 1;

#ifdef RAPTOR_V2_AVAILABLE
  if(l->datatype)
    raptor_free_uri_v2(l->world->raptor_world_ptr, l->datatype);
  l->datatype = raptor_uri_copy_v2(l->world->raptor_world_ptr, dt_uri);
#else
  if(l->datatype)
    raptor_free_uri(l->datatype);
  l->datatype = raptor_uri_copy(dt_uri);
#endif

  l->parent_type = rasqal_xsd_datatype_parent_type(type);

  switch(type) {
    case RASQAL_LITERAL_INTEGER:
      eptr=NULL;
      i=(int)strtol((const char*)l->string, &eptr, 10);
      if(*eptr)
        return 1;

      l->value.integer=i;
      break;

    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_FLOAT:
      d=0.0;
      (void)sscanf((char*)l->string, "%lf", &d);
      l->value.floating=d;
      break;

    case RASQAL_LITERAL_DECIMAL:
      l->value.decimal=rasqal_new_xsd_decimal();
      if(!l->value.decimal) {
        RASQAL_FREE(cstring, (void*)l->string);
        return 1;
      }
      if(rasqal_xsd_decimal_set_string(l->value.decimal,
                                       (const char*)l->string)) {
        RASQAL_FREE(cstring, (void*)l->string);
        return 1;
      }
      RASQAL_FREE(cstring, (void*)l->string);
      /* string is owned by l->value.decimal */
      l->string=(unsigned char*)rasqal_xsd_decimal_as_counted_string(l->value.decimal,
                                                                     (size_t*)&l->string_len);
      if(!l->string)
        return 1;
      break;

    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_UDT:
      /* No change - kept as same type - xsd:string or user defined */
      break;

    case RASQAL_LITERAL_BOOLEAN:
      i = rasqal_literal_string_interpreted_as_boolean(l->string);
      /* Free passed in string */
      RASQAL_FREE(cstring, (void*)l->string);
      /* and replace with a static string */
      l->string=i ? RASQAL_XSD_BOOLEAN_TRUE : RASQAL_XSD_BOOLEAN_FALSE;
      l->string_len=(i ? 4 : 5);
      
      l->value.integer=i;
      break;

  case RASQAL_LITERAL_STRING:
    /* No change - kept as a string */
    break;

  case RASQAL_LITERAL_DATETIME:
    new_string=rasqal_xsd_datetime_string_to_canonical(l->string);
    if(new_string) {
      RASQAL_DEBUG3("converted xsd:dateTime \"%s\" to canonical form \"%s\"\n", l->string, new_string);
      RASQAL_FREE(cstring, l->string);
      l->string=new_string;
      l->string_len=strlen((const char*)l->string);
      break; /* success */
    }
    RASQAL_DEBUG2("rasqal_xsd_datetime_string_to_canonical(\"%s\") failed\n", l->string);
    return 1; /* error */

  case RASQAL_LITERAL_UNKNOWN:
  case RASQAL_LITERAL_BLANK:
  case RASQAL_LITERAL_URI:
  case RASQAL_LITERAL_PATTERN:
  case RASQAL_LITERAL_QNAME:
  case RASQAL_LITERAL_VARIABLE:
    RASQAL_FATAL2("Unexpected native type %d\n", type);
    break;
    
  default:
    RASQAL_FATAL2("Unknown native type %d\n", type);
  }

  return 0;
}



/*
 * rasqal_literal_string_to_native:
 * @l: #rasqal_literal to operate on inline
 * @error_handler: error handling function
 * @error_data: data for error handle
 * @flags: flags for literal checking.  non-0 to ignore type errors
 *
 * INTERNAL Upgrade a datatyped literal string to an internal typed literal
 *
 * At present this promotes datatyped literals
 * xsd:integer to RASQAL_LITERAL_INTEGER
 * xsd:double to RASQAL_LITERAL_DOUBLE
 * xsd:float to RASQAL_LITERAL_FLOAT
 * xsd:boolean to RASQAL_LITERAL_BOOLEAN
 * xsd:decimal to RASQAL_LITERAL_DECIMAL
 * xsd:dateTime to RASQAL_LITERAL_DATETIME
 *
 * Return value: non-0 on failure
 **/
int
rasqal_literal_string_to_native(rasqal_literal *l,
                                raptor_simple_message_handler error_handler,
                                void *error_data, int flags)
{
  rasqal_literal_type native_type=RASQAL_LITERAL_UNKNOWN;
  int rc=0;
  
  /* RDF literal with no datatype (plain literal) */
  if(!l->datatype)
    return 0;
  
  native_type = rasqal_xsd_datatype_uri_to_type(l->world, l->datatype);
  /* plain literal - nothing to do */
  if(native_type == RASQAL_LITERAL_STRING)
    return 0;

  /* xsd:string - mark and return */
  if(native_type == RASQAL_LITERAL_XSD_STRING) {
    l->type = native_type;
    return 0;
  }

  /* If a user defined type - update the literal */
  if(native_type == RASQAL_LITERAL_UNKNOWN) {
    l->type = RASQAL_LITERAL_UDT;
    return 0;
  }

  rc=rasqal_literal_set_typed_value(l, native_type, NULL /* existing string */);

  if(flags) {
    int valid = rasqal_xsd_datatype_check(native_type, l->string, flags);
    if(!valid)
      return 0;
  }
  
  return rc;
}


/*
 * rasqal_new_string_literal_common:
 * @world: rasqal world object
 * @string: UTF-8 string lexical form
 * @language: RDF language (xml:lang) (or NULL)
 * @datatype: datatype URI (or NULL for plain literal)
 * @datatype_qname: datatype qname string (or NULL for plain literal)
 * @flags: flags - 1 to do native type promotion
 *
 * INTERNAL Constructor - Create a new Rasqal string literal.
 * 
 * All parameters are input parameters and if present are stored in
 * the literal, not copied. They are freed also on failure.
 * 
 * The datatype and datatype_qname parameters are alternatives; the
 * qname is a datatype that cannot be resolved till later since the
 * prefixes have not yet been declared or checked.
 * 
 * If the string literal is datatyped and of certain types recognised
 * it may be converted to a different literal type by 
 * rasqal_literal_string_to_native() only if @flags is 1.
 *
 * Return value: New #rasqal_literal or NULL on failure
 **/
static rasqal_literal*
rasqal_new_string_literal_common(rasqal_world* world,
                                 const unsigned char *string,
                                 const char *language,
                                 raptor_uri *datatype, 
                                 const unsigned char *datatype_qname,
                                 int flags)
{
  rasqal_literal* l=(rasqal_literal*)RASQAL_CALLOC(rasqal_literal, 1, sizeof(rasqal_literal));
  if(l) {
    rasqal_literal_type datatype_type = RASQAL_LITERAL_STRING;

    l->valid=1;
    l->usage=1;
    l->world=world;

    if(datatype && language) {
      /* RDF typed literal but this is not allowed so delete language */
      RASQAL_FREE(cstring, (void*)language);
      language=NULL;
    }

    l->type=RASQAL_LITERAL_STRING;
    l->string=string;
    l->string_len=strlen((const char*)string);
    l->language=language;
    l->datatype=datatype;
    l->flags=datatype_qname;

    if(datatype)
      datatype_type = rasqal_xsd_datatype_uri_to_type(world, datatype);
    l->parent_type = rasqal_xsd_datatype_parent_type(datatype_type);
    
    if((flags == 1) && rasqal_literal_string_to_native(l, NULL, NULL, 1)) {
      rasqal_free_literal(l);
      l=NULL;
    }
  } else {
    if(language)
      RASQAL_FREE(cstring, (void*)language);
    if(datatype)
#ifdef RAPTOR_V2_AVAILABLE
      raptor_free_uri_v2(world->raptor_world_ptr, datatype);
#else
      raptor_free_uri(datatype);
#endif
    if(datatype_qname)
      RASQAL_FREE(cstring, (void*)datatype_qname);
    RASQAL_FREE(cstring, (void*)string);
  }
    
  return l;
}


/**
 * rasqal_new_string_literal:
 * @world: rasqal world object
 * @string: UTF-8 string lexical form
 * @language: RDF language (xml:lang) (or NULL)
 * @datatype: datatype URI (or NULL for plain literal)
 * @datatype_qname: datatype qname string (or NULL for plain literal)
 *
 * Constructor - Create a new Rasqal string literal.
 * 
 * All parameters are input parameters and if present are stored in
 * the literal, not copied. They are freed also on failure.
 * 
 * The datatype and datatype_qname parameters are alternatives; the
 * qname is a datatype that cannot be resolved till later since the
 * prefixes have not yet been declared or checked.
 * 
 * If the string literal is datatyped and of certain types recognised
 * it may be converted to a different literal type by
 * rasqal_literal_string_to_native()
 *
 * Return value: New #rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_string_literal(rasqal_world* world,
                          const unsigned char *string,
                          const char *language,
                          raptor_uri *datatype, 
                          const unsigned char *datatype_qname)
{
  return rasqal_new_string_literal_common(world, string, language, datatype, 
                                          datatype_qname, 1);
}

rasqal_literal*
rasqal_new_string_literal_node(rasqal_world* world, const unsigned char *string,
                               const char *language, raptor_uri *datatype)
{
  return rasqal_new_string_literal_common(world, string, language, datatype, NULL, 0);
}


/**
 * rasqal_new_simple_literal:
 * @world: rasqal world object
 * @type: RASQAL_LITERAL_BLANK or RASQAL_LITERAL_BLANK_QNAME
 * @string: the UTF-8 string value to store
 *
 * Constructor - Create a new Rasqal simple literal.
 * 
 * The string is an input parameter and is stored in the
 * literal, not copied. It is freed also on failure.
 * 
 * Return value: New #rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_simple_literal(rasqal_world* world,
                          rasqal_literal_type type, 
                          const unsigned char *string)
{
  rasqal_literal* l=(rasqal_literal*)RASQAL_CALLOC(rasqal_literal, 1, sizeof(rasqal_literal));
  if(l) {
    l->valid=1;
    l->usage=1;
    l->world=world;
    l->type=type;
    l->string=string;
    l->string_len=strlen((const char*)string);
  } else {
    RASQAL_FREE(cstring, (void*)string);
  }
  return l;
}


/**
 * rasqal_new_boolean_literal:
 * @world: rasqal world object
 * @value: non-0 for true, 0 for false
 *
 * Constructor - Create a new Rasqal boolean literal.
 *
 * Return value: New #rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_boolean_literal(rasqal_world* world, int value)
{
  raptor_uri* dt_uri;
  rasqal_literal* l=(rasqal_literal*)RASQAL_CALLOC(rasqal_literal, 1, sizeof(rasqal_literal));
  if(l) {
    l->valid=1;
    l->usage=1;
    l->world=world;
    l->type=RASQAL_LITERAL_BOOLEAN;
    l->value.integer=value;
    l->string=value ? RASQAL_XSD_BOOLEAN_TRUE : RASQAL_XSD_BOOLEAN_FALSE;
    l->string_len=(value ? 4 : 5);
    dt_uri=rasqal_xsd_datatype_type_to_uri(world, l->type);
    if(!dt_uri) {
      rasqal_free_literal(l);
      return NULL;
    }
#ifdef RAPTOR_V2_AVAILABLE
    l->datatype = raptor_uri_copy_v2(world->raptor_world_ptr, dt_uri);
#else
    l->datatype = raptor_uri_copy(dt_uri);
#endif
  }
  return l;
}


/**
 * rasqal_new_variable_literal:
 * @world: rasqal_world object
 * @variable: #rasqal_variable to use
 *
 * Constructor - Create a new Rasqal variable literal.
 * 
 * variable is an input parameter and stored in the literal, not copied.
 * 
 * Return value: New #rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_variable_literal(rasqal_world* world, rasqal_variable *variable)
{
  rasqal_literal* l=(rasqal_literal*)RASQAL_CALLOC(rasqal_literal, 1, sizeof(rasqal_literal));
  if(l) {
    l->valid=1;
    l->usage=1;
    l->world=world;
    l->type=RASQAL_LITERAL_VARIABLE;
    l->value.variable=variable;
  }

  /* Do not rasqal_free_variable(variable) on error since
   * all variables are shared and owned by rasqal_query
   * variables_sequence */

  return l;
}


/**
 * rasqal_new_literal_from_literal:
 * @l: #rasqal_literal object to copy
 *
 * Copy Constructor - create a new rasqal_literal object from an existing rasqal_literal object.
 * 
 * Return value: a new #rasqal_literal object or NULL
 **/
rasqal_literal*
rasqal_new_literal_from_literal(rasqal_literal* l)
{
  if(!l)
    return NULL;
  
  l->usage++;
  return l;
}


/**
 * rasqal_free_literal:
 * @l: #rasqal_literal object
 *
 * Destructor - destroy an rasqal_literal object.
 * 
 **/
void
rasqal_free_literal(rasqal_literal* l)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(l, rasqal_literal);
  
  if(--l->usage)
    return;
  
  switch(l->type) {
    case RASQAL_LITERAL_URI:
      if(l->value.uri)
#ifdef RAPTOR_V2_AVAILABLE
        raptor_free_uri_v2(l->world->raptor_world_ptr, l->value.uri);
#else
        raptor_free_uri(l->value.uri);
#endif
      break;
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_INTEGER: 
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_UDT:
      if(l->string)
        RASQAL_FREE(cstring, (void*)l->string);
      if(l->language)
        RASQAL_FREE(cstring, (void*)l->language);
      if(l->datatype)
#ifdef RAPTOR_V2_AVAILABLE
        raptor_free_uri_v2(l->world->raptor_world_ptr, l->datatype);
#else
        raptor_free_uri(l->datatype);
#endif
      if(l->type == RASQAL_LITERAL_STRING ||
         l->type == RASQAL_LITERAL_PATTERN) {
        if(l->flags)
          RASQAL_FREE(cstring, (void*)l->flags);
      }
      break;
    case RASQAL_LITERAL_DECIMAL:
      /* l->string is owned by l->value.decimal - do not free it */
      if(l->datatype)
#ifdef RAPTOR_V2_AVAILABLE
        raptor_free_uri_v2(l->world->raptor_world_ptr, l->datatype);
#else
        raptor_free_uri(l->datatype);
#endif
      if(l->value.decimal)
        rasqal_free_xsd_decimal(l->value.decimal);
      break;

    case RASQAL_LITERAL_BOOLEAN:
      /* static l->string for boolean, does not need freeing */
      if(l->datatype)
#ifdef RAPTOR_V2_AVAILABLE
        raptor_free_uri_v2(l->world->raptor_world_ptr, l->datatype);
#else
        raptor_free_uri(l->datatype);
#endif
      break;

    case RASQAL_LITERAL_VARIABLE:
      /* It is correct that this is not called here
       * since all variables are shared and owned by
       * the rasqal_query sequence variables_sequence */

      /* rasqal_free_variable(l->value.variable); */
      break;

    case RASQAL_LITERAL_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown literal type %d", l->type);
  }
  RASQAL_FREE(rasqal_literal, l);
}


/* 
 * The order here must match that of rasqal_literal_type
 * in rasqal.h and is significant as rasqal_literal_compare
 * uses it for type comparisons with the RASQAL_COMPARE_XQUERY
 * flag.
 */
static const char* const rasqal_literal_type_labels[RASQAL_LITERAL_LAST+1]={
  "UNKNOWN",
  "blank",
  "uri",
  "string",
  "xsdstring",
  "boolean",
  "integer",
  "float",
  "double",
  "decimal",
  "datetime",
  "udt",
  "pattern",
  "qname",
  "variable"
};


/**
 * rasqal_literal_write_type:
 * @l: the #rasqal_literal object
 * @iostr: the #raptor_iostream handle to print to
 * 
 * Write a string form for a rasqal literal type to an iostream
 *
 **/
void
rasqal_literal_write_type(rasqal_literal* l, raptor_iostream* iostr)
{
  rasqal_literal_type type;

  if(!l) {
    raptor_iostream_write_counted_string(iostr, "null", 4);
    return;
  }
  
  type=l->type;
  if(type > RASQAL_LITERAL_LAST)
    type=RASQAL_LITERAL_UNKNOWN;
  raptor_iostream_write_string(iostr, rasqal_literal_type_labels[(int)type]);
}


/**
 * rasqal_literal_print_type:
 * @l: the #rasqal_literal object
 * @fh: the #FILE* handle to print to
 * 
 * Print a string form for a rasqal literal type.
 *
 **/
void
rasqal_literal_print_type(rasqal_literal* l, FILE* fh)
{
  rasqal_literal_type type;

  if(!l) {
    fputs("null", fh);
    return;
  }
  
  type=l->type;
  if(type > RASQAL_LITERAL_LAST)
    type=RASQAL_LITERAL_UNKNOWN;
  fputs(rasqal_literal_type_labels[(int)type], fh);
}


/**
 * rasqal_literal_write:
 * @l: the #rasqal_literal object
 * @iostr: the #raptor_iostream handle to write to
 *
 * Write a Rasqal literal to an iostream in a debug format.
 * 
 * The print debug format may change in any release.
 **/
void
rasqal_literal_write(rasqal_literal* l, raptor_iostream* iostr)
{
  const unsigned char*str;
  size_t len;
  
  if(!l) {
    raptor_iostream_write_counted_string(iostr, "null", 4);
    return;
  }

  if(!l->valid)
    raptor_iostream_write_counted_string(iostr, "INV:", 4);

  if(l->type != RASQAL_LITERAL_VARIABLE)
    rasqal_literal_write_type(l, iostr);

  switch(l->type) {
    case RASQAL_LITERAL_URI:
      raptor_iostream_write_byte(iostr, '<');
#ifdef RAPTOR_V2_AVAILABLE
      str = raptor_uri_as_counted_string_v2(l->world->raptor_world_ptr, l->value.uri, &len);
#else
      str = raptor_uri_as_counted_string(l->value.uri, &len);
#endif
      raptor_iostream_write_string_ntriples(iostr, str, len, '>');
      raptor_iostream_write_byte(iostr, '>');
      break;
    case RASQAL_LITERAL_BLANK:
      raptor_iostream_write_byte(iostr, ' ');
      raptor_iostream_write_counted_string(iostr, l->string, l->string_len);
      break;
    case RASQAL_LITERAL_PATTERN:
      raptor_iostream_write_byte(iostr, '/');
      raptor_iostream_write_counted_string(iostr, l->string, l->string_len);
      raptor_iostream_write_byte(iostr, '/');
      if(l->flags)
        raptor_iostream_write_string(iostr, l->flags);
      break;
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_UDT:
      raptor_iostream_write_counted_string(iostr, "(\"", 2);
      raptor_iostream_write_string_ntriples(iostr, l->string, l->string_len, '"');
      raptor_iostream_write_byte(iostr, '"');
      if(l->language) {
        raptor_iostream_write_byte(iostr, '@');
        raptor_iostream_write_string(iostr, l->language);
      }
      if(l->datatype) {
        raptor_iostream_write_counted_string(iostr, "^^<", 3);
#ifdef RAPTOR_V2_AVAILABLE
        str = raptor_uri_as_counted_string_v2(l->world->raptor_world_ptr, l->datatype, &len);
#else
        str = raptor_uri_as_counted_string(l->datatype, &len);
#endif
        raptor_iostream_write_string_ntriples(iostr, str, len, '>');
        raptor_iostream_write_byte(iostr, '>');
      }
      raptor_iostream_write_byte(iostr, ')');
      break;
    case RASQAL_LITERAL_VARIABLE:
      rasqal_variable_write(l->value.variable, iostr);
      break;

    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_DECIMAL:
    case RASQAL_LITERAL_DATETIME:
      raptor_iostream_write_byte(iostr, '(');
      raptor_iostream_write_counted_string(iostr, l->string, l->string_len);
      raptor_iostream_write_byte(iostr, ')');
      break;

    case RASQAL_LITERAL_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown literal type %d", l->type);
  }
}



/**
 * rasqal_literal_print:
 * @l: the #rasqal_literal object
 * @fh: the #FILE* handle to print to
 *
 * Print a Rasqal literal in a debug format.
 * 
 * The print debug format may change in any release.
 **/
void
rasqal_literal_print(rasqal_literal* l, FILE* fh)
{
  if(!l) {
    fputs("null", fh);
    return;
  }

  if(!l->valid)
    fputs("INV:", fh);

  if(l->type != RASQAL_LITERAL_VARIABLE)
    rasqal_literal_print_type(l, fh);

  switch(l->type) {
    case RASQAL_LITERAL_URI:
      fputc('<', fh);
      raptor_print_ntriples_string(fh,
#ifdef RAPTOR_V2_AVAILABLE
                                   raptor_uri_as_string_v2(l->world->raptor_world_ptr, l->value.uri),
#else
                                   raptor_uri_as_string(l->value.uri),
#endif
                                   '>');
      fputc('>', fh);
      break;
    case RASQAL_LITERAL_BLANK:
      fprintf(fh, " %s", l->string);
      break;
    case RASQAL_LITERAL_PATTERN:
      fprintf(fh, "/%s/%s", l->string, l->flags ? (const char*)l->flags : "");
      break;
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_UDT:
      fputs("(\"", fh);
      raptor_print_ntriples_string(fh, l->string, '"');
      fputc('"', fh);
      if(l->language)
        fprintf(fh, "@%s", l->language);
      if(l->datatype) {
        fputs("^^<", fh);
        raptor_print_ntriples_string(fh,
#ifdef RAPTOR_V2_AVAILABLE
                                     raptor_uri_as_string_v2(l->world->raptor_world_ptr, l->datatype),
#else
                                     raptor_uri_as_string(l->datatype),
#endif
                                     '>');
        fputc('>', fh);
      }
      fputc(')', fh);
      break;
    case RASQAL_LITERAL_VARIABLE:
      rasqal_variable_print(l->value.variable, fh);
      break;

    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_DECIMAL:
    case RASQAL_LITERAL_DATETIME:
      fputc('(', fh);
      fwrite(l->string, 1, l->string_len, fh);
      fputc(')', fh);
      break;

    case RASQAL_LITERAL_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown literal type %d", l->type);
  }
}



/*
 * rasqal_literal_as_boolean:
 * @l: #rasqal_literal object
 * @error: pointer to error flag
 * 
 * INTERNAL: Return a literal as a boolean value
 *
 * SPARQL Effective Boolean Value (EBV) rules:
 *  - If the argument is a typed literal with a datatype of xsd:boolean, the
 *    EBV is the value of that argument.
 *  - If the argument is a plain literal or a typed literal with a datatype of
 *    xsd:string, the EBV is false if the operand value has zero length;
 *    otherwise the EBV is true.
 *  - If the argument is a numeric type or a typed literal with a datatype
 *    derived from a numeric type, the EBV is false if the operand value is NaN
 *    or is numerically equal to zero; otherwise the EBV is true.
 *  - All other arguments, including unbound arguments, produce a type error.
 *
 * Return value: non-0 if true
 **/
int
rasqal_literal_as_boolean(rasqal_literal* l, int *error)
{
  if(!l)
    return 0;
  
  switch(l->type) {
    case RASQAL_LITERAL_STRING:
      if(l->datatype) {
        if(
#ifdef RAPTOR_V2_AVAILABLE
           raptor_uri_equals_v2(l->world->raptor_world_ptr, l->datatype, 
                             rasqal_xsd_datatype_type_to_uri(l->world, RASQAL_LITERAL_STRING))
#else
           raptor_uri_equals(l->datatype, 
                             rasqal_xsd_datatype_type_to_uri(l->world, RASQAL_LITERAL_STRING))
#endif
           )
        {
          /* typed literal with xsd:string datatype -> true if non-empty */
          return l->string && *l->string;
        }
        /* typed literal with other datatype -> type error */
        *error = 1;
        return 0;
      }
      /* plain literal -> true if non-empty */
      return l->string && *l->string;

    case RASQAL_LITERAL_XSD_STRING:
      /* xsd:string -> true if non-empty */
      return l->string && *l->string;

    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_DECIMAL:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_UDT:
      *error = 1;
      return 0;
      break;

    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_BOOLEAN:
      return l->value.integer != 0;
      break;

    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_FLOAT:
      return l->value.floating != 0.0 && !isnan(l->value.floating);
      break;

    case RASQAL_LITERAL_VARIABLE:
      return rasqal_literal_as_boolean(l->value.variable->value, error);
      break;

    case RASQAL_LITERAL_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown literal type %d", l->type);
      return 0; /* keep some compilers happy */
  }
}


/*
 * rasqal_literal_as_integer - INTERNAL Return a literal as an integer value
 * @l: #rasqal_literal object
 * @error: pointer to error flag
 * 
 * Integers, booleans, double and float literals natural are turned into
 * integers. If string values are the lexical form of an integer, that is
 * returned.  Otherwise the error flag is set.
 * 
 * Return value: integer value
 **/
int
rasqal_literal_as_integer(rasqal_literal* l, int *error)
{
  if(!l)
    return 0;
  
  switch(l->type) {
    case RASQAL_LITERAL_INTEGER:
      return l->value.integer;
      break;

    case RASQAL_LITERAL_BOOLEAN:
      return l->value.integer != 0;
      break;

    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_FLOAT:
      return (int)l->value.floating;
      break;

    case RASQAL_LITERAL_DECIMAL:
      return (int)rasqal_xsd_decimal_get_double(l->value.decimal);
      break;

    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_XSD_STRING:
      {
        char *eptr;
        double  d;
        int v;

        eptr=NULL;
        v=(int)strtol((const char*)l->string, &eptr, 10);
        if((unsigned char*)eptr != l->string && *eptr=='\0')
          return v;

        eptr=NULL;
        d=strtod((const char*)l->string, &eptr);
        if((unsigned char*)eptr != l->string && *eptr=='\0')
          return (int)d;
      }
      if(error)
        *error=1;
      return 0;
      break;

    case RASQAL_LITERAL_VARIABLE:
      return rasqal_literal_as_integer(l->value.variable->value, error);
      break;

    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_UDT:
      if(error)
        *error=1;
      return 0;
      
    case RASQAL_LITERAL_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown literal type %d", l->type);
      return 0; /* keep some compilers happy */
  }
}


/*
 * rasqal_literal_as_floating - INTERNAL Return a literal as a floating value
 * @l: #rasqal_literal object
 * @error: pointer to error flag
 * 
 * Integers, booleans, double and float literals natural are turned into
 * integers. If string values are the lexical form of an floating, that is
 * returned.  Otherwise the error flag is set.
 * 
 * Return value: floating value
 **/
double
rasqal_literal_as_floating(rasqal_literal* l, int *error)
{
  if(!l)
    return 0;
  
  switch(l->type) {
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_BOOLEAN:
      return (double)l->value.integer;
      break;

    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_FLOAT:
      return l->value.floating;
      break;

    case RASQAL_LITERAL_DECIMAL:
      return rasqal_xsd_decimal_get_double(l->value.decimal);
      break;

    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_XSD_STRING:
      {
        char *eptr=NULL;
        double  d=strtod((const char*)l->string, &eptr);
        if((unsigned char*)eptr != l->string && *eptr=='\0')
          return d;
      }
      if(error)
        *error=1;
      return 0.0;
      break;

    case RASQAL_LITERAL_VARIABLE:
      return rasqal_literal_as_integer(l->value.variable->value, error);
      break;

    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_UDT:
      if(error)
        *error=1;
      return 0.0;
      
    case RASQAL_LITERAL_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown literal type %d", l->type);
      return 0.0; /* keep some compilers happy */
  }
}


/*
 * rasqal_literal_as_uri - INTERNAL Return a literal as a raptor_uri*
 * @l: #rasqal_literal object
 * 
 * Return value: raptor_uri* value or NULL on failure
 **/
raptor_uri*
rasqal_literal_as_uri(rasqal_literal* l)
{
  if(!l)
    return NULL;
  
  if(l->type==RASQAL_LITERAL_URI)
    return l->value.uri;

  if(l->type==RASQAL_LITERAL_VARIABLE)
    return rasqal_literal_as_uri(l->value.variable->value);

  RASQAL_FATAL2("Literal type %d has no URI value", l->type);

  return NULL;
}


/**
 * rasqal_literal_as_string_flags:
 * @l: #rasqal_literal object
 * @flags: comparison flags
 * @error: pointer to error
 *
 * Return the string format of a literal according to flags.
 * 
 * flag bits affects conversion:
 *   RASQAL_COMPARE_XQUERY: use XQuery conversion rules
 * 
 * If @error is not NULL, *error is set to non-0 on error
 *
 * Return value: pointer to a shared string format of the literal.
 **/
const unsigned char*
rasqal_literal_as_string_flags(rasqal_literal* l, int flags, int *error)
{
  if(!l)
    return NULL;
  
  switch(l->type) {
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_DECIMAL:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_UDT:
      return l->string;

    case RASQAL_LITERAL_URI:
      if(flags & RASQAL_COMPARE_XQUERY) {
        if(error)
          *error=1;
        return NULL;
      }
#ifdef RAPTOR_V2_AVAILABLE
      return raptor_uri_as_string_v2(l->world->raptor_world_ptr, l->value.uri);
#else
      return raptor_uri_as_string(l->value.uri);
#endif

    case RASQAL_LITERAL_VARIABLE:
      return rasqal_literal_as_string_flags(l->value.variable->value, flags,
                                            error);

    case RASQAL_LITERAL_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown literal type %d", l->type);
      return NULL; /* keep some compilers happy */
  }
}


/**
 * rasqal_literal_as_string:
 * @l: #rasqal_literal object
 *
 * Return the string format of a literal.
 * 
 * Return value: pointer to a shared string format of the literal.
 **/
const unsigned char*
rasqal_literal_as_string(rasqal_literal* l)
{
  return rasqal_literal_as_string_flags(l, 0, NULL);
}

/**
 * rasqal_literal_as_variable:
 * @l: #rasqal_literal object
 *
 * Get the variable inside a literal.
 * 
 * Return value: the #rasqal_variable or NULL if the literal is not a variable
 **/
rasqal_variable*
rasqal_literal_as_variable(rasqal_literal* l)
{
  return (l->type == RASQAL_LITERAL_VARIABLE) ? l->value.variable : NULL;
}


/**
 * rasqal_literal_promote_numerics:
 * @l1: first literal
 * @l2: second literal
 * @flags: promotion flags
 *
 * INTERNAL - Calculate the type to promote a pair of literals to
 *
 * Numeric type promotion
 * http://www.w3.org/TR/xpath20/#dt-type-promotion
 *
 * [[xs:decimal (or any type derived by restriction from xs:decimal,
 * including xs:integer) can be promoted to either of the types
 * xs:float or xs:double.]]
 *
 * For here that means xs:integer to xs:double and xs:decimal to xs:double
 *
 * Return value: promote type or RASQAL_LITERAL_UNKNOWN
 */
static rasqal_literal_type
rasqal_literal_promote_numerics(rasqal_literal* l1, rasqal_literal* l2,
                                 int flags)
{
  rasqal_literal_type type1=l1->type;
  rasqal_literal_type type2=l2->type;
  rasqal_literal_type promotion_type;

  for(promotion_type = RASQAL_LITERAL_FIRST_XSD;
      promotion_type <= RASQAL_LITERAL_LAST_XSD;
      promotion_type++) {
    rasqal_literal_type parent_type1 = rasqal_xsd_datatype_parent_type(type1);
    rasqal_literal_type parent_type2 = rasqal_xsd_datatype_parent_type(type2);
    
    RASQAL_DEBUG3("literal 1: type %s   parent type %s\n",
                  rasqal_literal_type_labels[type1],
                  rasqal_literal_type_labels[parent_type1]);
    RASQAL_DEBUG3("literal 2: type %s   parent type %s\n",
                  rasqal_literal_type_labels[type2],
                  rasqal_literal_type_labels[parent_type2]);
  
    /* Finished */
    if(type1 == type2)
      return type1;

    if(parent_type1 == type2)
      return type2;

    if(parent_type2 == type1)
      return type1;

    if(parent_type1 == promotion_type)
      type1 = promotion_type;
    if(parent_type2 == promotion_type)
      type2 = promotion_type;
  }

  /* failed! */
  return RASQAL_LITERAL_UNKNOWN;
}


/**
 * rasqal_literal_get_rdf_term_type:
 * @l: literal
 *
 * INTERNAL - Get the RDF term type of a literal
 *
 * Return value: type or RASQAL_LITERAL_UNKNOWN if cannot be an RDF term
 */
rasqal_literal_type
rasqal_literal_get_rdf_term_type(rasqal_literal* l)
{
  rasqal_literal_type type = l->type;
 
  /* squash literal datatypes into one type: RDF Literal */
  if(type >= RASQAL_LITERAL_FIRST_XSD &&
     type <= RASQAL_LITERAL_LAST_XSD)
    type = RASQAL_LITERAL_STRING;

  if(type == RASQAL_LITERAL_UDT)
    type = RASQAL_LITERAL_STRING;
  
  if(type != RASQAL_LITERAL_URI &&
     type != RASQAL_LITERAL_STRING &&
     type != RASQAL_LITERAL_BLANK)
    type = RASQAL_LITERAL_UNKNOWN;

  return type;
}




/*
 * rasqal_new_literal_from_promotion:
 * @lit: existing literal
 * @type: type to promote to
 * @flags; if RASQAL_COMPARE_URI is set, do sloppy promotion from string to bool (RDQL)
 *
 * Make a new literal from a type promotion
 *
 * New literal or NULL on failure
*/
static rasqal_literal*
rasqal_new_literal_from_promotion(rasqal_literal* lit,
                                  rasqal_literal_type type,
                                  int flags)
{
  rasqal_literal* new_lit=NULL;
  int errori=0;
  double d;
  int i;
  unsigned char *new_s=NULL;
  const unsigned char* s;
  size_t len;
  
  if(lit->type == type)
    return rasqal_new_literal_from_literal(lit);

  RASQAL_DEBUG3("promoting literal type %s to type %s\n", 
                rasqal_literal_type_labels[lit->type],
                rasqal_literal_type_labels[type]);

  /* May not promote to non-numerics */
  if(!rasqal_xsd_datatype_is_numeric(type)) {
    RASQAL_DEBUG2("NOT promoting to non-numeric type %s\n", 
                  rasqal_literal_type_labels[lit->type]);

    if(type == RASQAL_LITERAL_STRING || type ==  RASQAL_LITERAL_UDT) {
      s = rasqal_literal_as_string(lit);
      len = strlen((const char*)s);
      new_s = (unsigned char*)RASQAL_MALLOC(sstring, len+1);
      if(new_s) {
        raptor_uri* dt_uri = NULL;
        strncpy((char*)new_s, (const char*)s, len+1);
        if(lit->datatype) {
#ifdef RAPTOR_V2_AVAILABLE
          dt_uri = raptor_uri_copy_v2(lit->world->raptor_world_ptr, lit->datatype);
#else
          dt_uri = raptor_uri_copy(lit->datatype);
#endif
        }
        return rasqal_new_string_literal_node(lit->world, new_s, NULL, dt_uri);
      } else
        return NULL;
    }
    return NULL;
  }
    
  switch(type) {
    case RASQAL_LITERAL_DECIMAL:
      new_lit=rasqal_new_decimal_literal(lit->world, rasqal_literal_as_string(lit));
      break;
      
    case RASQAL_LITERAL_DOUBLE:
      d=rasqal_literal_as_floating(lit, &errori);
      /* failure always means no match */
      if(errori)
        new_lit=NULL;
      else
        new_lit=rasqal_new_double_literal(lit->world, d);
      break;
      
    case RASQAL_LITERAL_FLOAT:
      d=rasqal_literal_as_floating(lit, &errori);
      /* failure always means no match */
      if(errori)
        new_lit=NULL;
      else
        new_lit=rasqal_new_float_literal(lit->world, d);
      break;
      

    case RASQAL_LITERAL_INTEGER:
      i=rasqal_literal_as_integer(lit, &errori);
      /* failure always means no match */
      if(errori)
        new_lit=NULL;
      else
        new_lit=rasqal_new_integer_literal(lit->world, type, i);
      break;
    
    case RASQAL_LITERAL_BOOLEAN:
      if(flags & RASQAL_COMPARE_URI)
        i = rasqal_literal_string_interpreted_as_boolean(lit->string);
      else
        i = rasqal_literal_as_boolean(lit, &errori);
      /* failure always means no match */
      if(errori)
        new_lit=NULL;
      else
        new_lit=rasqal_new_integer_literal(lit->world, type, i);
      break;
    
    case RASQAL_LITERAL_STRING:
      s=rasqal_literal_as_string(lit);
      len=strlen((const char*)s);
      new_s=(unsigned char*)RASQAL_MALLOC(sstring, len+1);
      if(new_s) {
        strncpy((char*)new_s, (const char*)s, len+1);
        new_lit=rasqal_new_string_literal(lit->world, new_s, NULL, NULL, NULL);
      }
      break;

    case RASQAL_LITERAL_XSD_STRING:
      s = rasqal_literal_as_string(lit);
      len = strlen((const char*)s);
      new_s = (unsigned char*)RASQAL_MALLOC(sstring, len+1);
      if(new_s) {
        raptor_uri* dt_uri;
        strncpy((char*)new_s, (const char*)s, len+1);
        dt_uri = rasqal_xsd_datatype_type_to_uri(lit->world, lit->type);
#ifdef RAPTOR_V2_AVAILABLE
        dt_uri = raptor_uri_copy_v2(lit->world->raptor_world_ptr, dt_uri);
#else
        dt_uri = raptor_uri_copy(dt_uri);
#endif
        new_lit = rasqal_new_string_literal(lit->world, new_s, NULL, dt_uri,
                                            NULL);
      }
      break;

    case RASQAL_LITERAL_UNKNOWN:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_VARIABLE:
    case RASQAL_LITERAL_UDT:
    default:
      errori=1;
      new_lit=NULL;
  }

#ifdef RASQAL_DEBUG
  if(new_lit)
    RASQAL_DEBUG4("promoted literal type %s to type %s, with value '%s'\n", 
                  rasqal_literal_type_labels[lit->type],
                  rasqal_literal_type_labels[new_lit->type],
                  rasqal_literal_as_string(new_lit));
  else
    RASQAL_DEBUG3("failed to promote literal type %s to type %s\n", 
                  rasqal_literal_type_labels[lit->type],
                  rasqal_literal_type_labels[type]);
#endif

  return new_lit;
}  


static int
rasqal_literal_string_compare(rasqal_literal* l1, rasqal_literal* l2,
                              int flags, int* error)
{
  if((l1->type != RASQAL_LITERAL_STRING && l1->type != RASQAL_LITERAL_UDT) ||
      (l2->type != RASQAL_LITERAL_STRING && l2->type != RASQAL_LITERAL_UDT)) {
    if(error)
      *error=0;
    return 0;
  }
    
  if(l1->language || l2->language) {
    /* if either is null, the comparison fails */
    if(!l1->language || !l2->language)
      return 1;
    if(rasqal_strcasecmp(l1->language, l2->language))
      return 1;
  }
  
  if(l1->datatype || l2->datatype) {
    int result;
    
    /* there is no ordering between typed and plain literals:       
       if either is NULL, do not compare but return an error
       (also implies inequality) */
    if(!l1->datatype || !l2->datatype) {
      if(error)
        *error=1;
      return 0;
    }
#ifdef RAPTOR_V2_AVAILABLE
    result = raptor_uri_compare_v2(l1->world->raptor_world_ptr, l1->datatype, l2->datatype);
#else
    result = raptor_uri_compare(l1->datatype, l2->datatype);
#endif

    if(result)
      return result;
  }
  
  if(flags & RASQAL_COMPARE_NOCASE)
    return rasqal_strcasecmp((const char*)l1->string, (const char*)l2->string);
  else
    return strcmp((const char*)l1->string, (const char*)l2->string);
}


static rasqal_literal_type
rasqal_literal_rdql_promote_calculate(rasqal_literal* l1, rasqal_literal* l2)
{    
  int seen_string=0;
  int seen_int=0;
  int seen_double=0;
  int seen_boolean=0;
  int i;
  rasqal_literal *lits[2];
  rasqal_literal_type type=RASQAL_LITERAL_UNKNOWN;

  lits[0]=l1;
  lits[1]=l2;

  for(i=0; i<2; i++) {
    switch(lits[i]->type) {
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_DECIMAL:
      break;
      
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_UDT:
      seen_string++;
      break;
      
    case RASQAL_LITERAL_BOOLEAN:
      seen_boolean=1;
      break;
      
    case RASQAL_LITERAL_INTEGER:
      seen_int++;
      break;
      
    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_FLOAT:
      seen_double++;
      break;
      
    case RASQAL_LITERAL_VARIABLE:
      /* this case was dealt with elsewhere */
      
    case RASQAL_LITERAL_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown literal type %d", lits[i]->type);
    }
  }

  
  if(lits[0]->type != lits[1]->type) {
    type=seen_string ? RASQAL_LITERAL_STRING : RASQAL_LITERAL_INTEGER;
    if((seen_int & seen_double) || (seen_int & seen_string))
      type=RASQAL_LITERAL_DOUBLE;
    if(seen_boolean & seen_string)
      type=RASQAL_LITERAL_BOOLEAN;
  } else
    type=lits[0]->type;
  
  return type;
}



/**
 * rasqal_literal_compare:
 * @l1: #rasqal_literal first literal
 * @l2: #rasqal_literal second literal
 * @flags: comparison flags
 * @error: pointer to error
 *
 * Compare two literals with type promotion.
 * 
 * The two literals are compared across their range.  If the types
 * are not the same, they are promoted.  If one is a double or float, the
 * other is promoted to double, otherwise for integers, otherwise
 * to strings (all literals have a string value).
 *
 * The comparison returned is as for strcmp, first before second
 * returns <0.  equal returns 0, and first after second returns >0.
 * For URIs, the string value is used for the comparsion.
 *
 * flag bits affects comparisons:
 *   RASQAL_COMPARE_NOCASE: use case independent string comparisons
 *   RASQAL_COMPARE_XQUERY: use XQuery comparison and type promotion rules
 *   RASQAL_COMPARE_RDF: use RDF term comparison
 *   RASQAL_COMPARE_URI: allow comparison of URIs (typically for SPARQL ORDER)
 * 
 * If @error is not NULL, *error is set to non-0 on error
 *
 * Return value: <0, 0, or >0 as described above.
 **/
int
rasqal_literal_compare(rasqal_literal* l1, rasqal_literal* l2, int flags,
                       int *error)
{
  rasqal_literal *lits[2];
  rasqal_literal* new_lits[2]; /* after promotions */
  rasqal_literal_type type; /* target promotion type */
  int i;
  int result=0;
  double d=0;
  int promotion=0;
  
  if(error)
    *error=0;

  lits[0]=rasqal_literal_value(l1);
  lits[1]=rasqal_literal_value(l2);

  /* null literals */
  if(!lits[0] || !lits[1]) {
    /* if either is not NULL, the comparison fails */
    if(lits[0] || lits[1]) {
      if(error)
        *error=1;
    }
    return 0;
  }

  new_lits[0]=NULL;
  new_lits[1]=NULL;

  RASQAL_DEBUG3("literal 0 type %s.  literal 1 type %s\n", 
                rasqal_literal_type_labels[lits[0]->type],
                rasqal_literal_type_labels[lits[1]->type]);
  
  if(flags & RASQAL_COMPARE_RDF) {
    /* no promotion but compare as RDF terms; like rasqal_literal_as_node() */
    rasqal_literal_type type0=rasqal_literal_get_rdf_term_type(lits[0]);
    rasqal_literal_type type1=rasqal_literal_get_rdf_term_type(lits[1]);
    int type_diff;
    
    if(type0 == RASQAL_LITERAL_UNKNOWN || type1 == RASQAL_LITERAL_UNKNOWN)
      return 1;
    type_diff=type0 - type1;
    if(type_diff != 0) {
      RASQAL_DEBUG2("RDF term literal returning type difference %d\n",
                    type_diff);
      return type_diff;
    }
    type=type1;
  } else if(flags & RASQAL_COMPARE_XQUERY) { 
    /* SPARQL / XQuery promotion rules */
    rasqal_literal_type type0=lits[0]->type;
    rasqal_literal_type type1=lits[1]->type;

    RASQAL_DEBUG3("xquery literal compare types %s vs %s\n",
                rasqal_literal_type_labels[type0],
                rasqal_literal_type_labels[type1]);

    /* cannot compare UDTs */
    if(type0 == RASQAL_LITERAL_UDT || type1 == RASQAL_LITERAL_UDT) {
      if(error)
        *error = 1;
      return 0;
    }

    type=rasqal_literal_promote_numerics(lits[0], lits[1], flags);
    if(type == RASQAL_LITERAL_UNKNOWN) {
      int type_diff;

      /* no promotion but compare as RDF terms; like rasqal_literal_as_node() */
      type0=rasqal_literal_get_rdf_term_type(lits[0]);
      type1=rasqal_literal_get_rdf_term_type(lits[1]);
      
      if(type0 == RASQAL_LITERAL_UNKNOWN || type1 == RASQAL_LITERAL_UNKNOWN)
        return 1;
      type_diff=type0 - type1;
      if(type_diff != 0) {
        RASQAL_DEBUG2("RDF term literal returning type difference %d\n",
                      type_diff);
        return type_diff;
      }
      if(error)
        *error=1;
      return 0;
    }
    promotion=1;
  } else {
    /* RDQL promotion rules */
    type=rasqal_literal_rdql_promote_calculate(lits[0], lits[1]);
    promotion=1;
  }

#ifdef RASQAL_DEBUG
  if(promotion)
    RASQAL_DEBUG2("promoting to type %s\n", rasqal_literal_type_labels[type]);
#endif

  /* do promotions */
  for(i=0; i<2; i++) {
    if(promotion) {
      new_lits[i] = rasqal_new_literal_from_promotion(lits[i], type, flags);
      if(!new_lits[i]) {
        if(error)
          *error=1;
        goto done;
      }
    } else {
      new_lits[i]=lits[i];
    }
  }


  switch(type) {
    case RASQAL_LITERAL_URI:
      if(flags & RASQAL_COMPARE_URI)
#ifdef RAPTOR_V2_AVAILABLE
        result = raptor_uri_compare_v2(l1->world->raptor_world_ptr,
                                       new_lits[0]->value.uri,
                                       new_lits[1]->value.uri);
#else
        result = raptor_uri_compare(new_lits[0]->value.uri,
                                    new_lits[1]->value.uri);
#endif
      else {
        if(error)
          *error=1;
        return 0;
      }
      break;

    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_UDT:
      result=rasqal_literal_string_compare(new_lits[0], new_lits[1],
                                           flags, error);
      if(*error)
        result=1;
      break;
      
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_DATETIME:
      if(flags & RASQAL_COMPARE_NOCASE)
        result=rasqal_strcasecmp((const char*)new_lits[0]->string,
                                 (const char*)new_lits[1]->string);
      else
        result=strcmp((const char*)new_lits[0]->string,
                      (const char*)new_lits[1]->string);
      break;

    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_BOOLEAN:
      result=new_lits[0]->value.integer - new_lits[1]->value.integer;
      break;

    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_FLOAT:
      d=new_lits[0]->value.floating - new_lits[1]->value.floating;
      result=(d > 0.0) ? 1: (d < 0.0) ? -1 : 0;
      break;
      
    case RASQAL_LITERAL_DECIMAL:
      result=rasqal_xsd_decimal_compare(new_lits[0]->value.decimal,
                                        new_lits[1]->value.decimal);
      break;

    case RASQAL_LITERAL_UNKNOWN:
    case RASQAL_LITERAL_VARIABLE:
    default:
      RASQAL_FATAL2("Literal type %d cannot be compared", type);
      result=0; /* keep some compilers happy */
  }

  done:
  if(promotion) {
    for(i=0; i<2; i++) {
      if(new_lits[i])
        rasqal_free_literal(new_lits[i]);
    }
  }
  
  return result;
}


/**
 * rasqal_literal_string_equals:
 * @l1: #rasqal_literal first literal
 * @l2: #rasqal_literal second literal
 * @error: pointer to error
 *
 * INTERNAL - Compare two typed literals
 *
 * Return value: non-0 if equal
 */
static int
rasqal_literal_string_equals(rasqal_literal* l1, rasqal_literal* l2,
                             int* error_p)
{
  int result=1;
  raptor_uri* dt1=l1->datatype;
  raptor_uri* dt2=l2->datatype;
  raptor_uri* xsd_string_uri;

  xsd_string_uri = rasqal_xsd_datatype_type_to_uri(l1->world,
                                                   RASQAL_LITERAL_XSD_STRING);

  if(l1->language || l2->language) {
    /* if either is NULL, the comparison fails */
    if(!l1->language || !l2->language)
      return 0;
    if(rasqal_strcasecmp(l1->language,l2->language))
      return 0;
  }

  /* Promote plain literal to typed literal "xx"^^xsd:string 
   * if the other literal is typed
   */
  if(l1->type == RASQAL_LITERAL_STRING && 
     l2->type == RASQAL_LITERAL_XSD_STRING) {
#ifdef RAPTOR_V2_AVAILABLE
    dt1 = raptor_uri_copy_v2(l->world->raptor_world_ptr, xsd_string_uri);
#else
    dt1 = raptor_uri_copy(xsd_string_uri);
#endif
  } else if(l1->type == RASQAL_LITERAL_XSD_STRING && 
            l2->type == RASQAL_LITERAL_STRING) {
#ifdef RAPTOR_V2_AVAILABLE
    dt2 = raptor_uri_copy_v2(l->world->raptor_world_ptr, xsd_string_uri);
#else
    dt2 = raptor_uri_copy(xsd_string_uri);
#endif
  }

  if(dt1 || dt2) {
    /* if either is NULL - type error */
    if(!dt1 || !dt2) {
      if(error_p)
        *error_p = 1;
      return 0;
    }
    /* if different - type error */
    if(
#ifdef RAPTOR_V2_AVAILABLE
       !raptor_uri_equals_v2(l1->world->raptor_world_ptr, dt1, dt2)
#else
       !raptor_uri_equals(dt1, dt2)
#endif
       )
    {
      if(error_p)
        *error_p = 1;
      return 0;
    }
    /* at this point the datatypes (URIs) are the same */
  }

  /* Finally check the lexical forms */

  /* not-equal if lengths are different - cheaper to try this first */
  if(l1->string_len != l2->string_len)
    return 0;

  result = !strcmp((const char*)l1->string, (const char*)l2->string);

  /* If result is equality but literals were both typed literals with
   * user-defined types then cause a type error; equality is unknown.
   */
  if(!result &&
     (l1->type == RASQAL_LITERAL_UDT && l2->type == RASQAL_LITERAL_UDT)) {
    if(error_p)
      *error_p = 1;
  }
  
  return result;
}


static int
rasqal_literal_uri_equals(rasqal_literal* l1, rasqal_literal* l2)
{
#ifdef RAPTOR_V2_AVAILABLE
  return raptor_uri_equals_v2(l1->world->raptor_world_ptr, l1->value.uri, l2->value.uri);
#else
  return raptor_uri_equals(l1->value.uri, l2->value.uri);
#endif
}


static int
rasqal_literal_blank_equals(rasqal_literal* l1, rasqal_literal* l2)
{
  /* not-equal if lengths are different - cheap to compare this first */
  if(l1->string_len != l2->string_len)
    return 0;
      
  return !strcmp((const char*)l1->string, (const char*)l2->string);
}


/**
 * rasqal_literal_not_equals_flags:
 * @l1: #rasqal_literal literal
 * @l2: #rasqal_literal data literal
 *
 * Check if two literals are not equal with optional type promotion.
 * 
 * Return value: non-0 if not-equal
 **/
int
rasqal_literal_not_equals_flags(rasqal_literal* l1, rasqal_literal* l2,
                                int flags, int* error_p)
{
  return !rasqal_literal_equals_flags(l1, l2, flags, error_p);
}


/**
 * rasqal_literal_equals:
 * @l1: #rasqal_literal literal
 * @l2: #rasqal_literal data literal
 *
 * Compare two literals with no type promotion.
 * 
 * If the l2 data literal value is a boolean, it will match
 * the string "true" or "false" in the first literal l1.
 *
 * Return value: non-0 if equal
 **/
int
rasqal_literal_equals(rasqal_literal* l1, rasqal_literal* l2)
{
  return rasqal_literal_equals_flags(l1, l2, 0, NULL);
}


/**
 * rasqal_literal_equals_flags:
 * @l1: #rasqal_literal literal
 * @l2: #rasqal_literal data literal
 * @flags: comparison flags
 * @error_p: type error
 *
 * Compare two literals with optional type promotion.
 * 
 * flag bits affects equality:
 *   RASQAL_COMPARE_XQUERY: use XQuery comparison and type promotion rules
 *   RASQAL_COMPARE_RDF: use RDF term equality
 *
 * Return value: non-0 if equal
 **/
int
rasqal_literal_equals_flags(rasqal_literal* l1, rasqal_literal* l2,
                            int flags, int* error_p)
{
  rasqal_literal_type type;
  rasqal_literal* l1_p=NULL;
  rasqal_literal* l2_p=NULL;
  int result=0;
  int promotion=0;
  
  /* null literals */
  if(!l1 || !l2) {
    /* if either is not null, the comparison fails */
    return (l1 || l2);
  }

#if RASQAL_DEBUG > 1
  RASQAL_DEBUG1(" ");
  rasqal_literal_print(l1, stderr);
  fputs( " to ", stderr);
  rasqal_literal_print(l2, stderr);
  fprintf(stderr, " with flags %d\n", flags);
#endif

  if(flags & RASQAL_COMPARE_RDF) {
    /* no promotion but compare as RDF terms; like rasqal_literal_as_node() */
    rasqal_literal_type type1=rasqal_literal_get_rdf_term_type(l1);
    rasqal_literal_type type2=rasqal_literal_get_rdf_term_type(l2);

    if(type1 == RASQAL_LITERAL_UNKNOWN || type2 == RASQAL_LITERAL_UNKNOWN ||
       type1 != type2)
      goto tidy;

    type=type1;
  } else if(flags & RASQAL_COMPARE_XQUERY) { 
    /* SPARQL / XSD promotion rules */
    if(l1->type != l2->type) {
      type=rasqal_literal_promote_numerics(l1, l2, flags);
      if(type == RASQAL_LITERAL_UNKNOWN) {
        /* Cannot numeric promote - try RDF equality */
        rasqal_literal_type type1=rasqal_literal_get_rdf_term_type(l1);
        rasqal_literal_type type2=rasqal_literal_get_rdf_term_type(l2);
        
        if(type1 == RASQAL_LITERAL_UNKNOWN || type2 == RASQAL_LITERAL_UNKNOWN ||
           type1 != type2)
          goto tidy;

        type=type1;
      } else
        promotion=1;
#if RASQAL_DEBUG > 1
      RASQAL_DEBUG4("xquery promoted literals types (%s, %s) to type %s\n", 
                    rasqal_literal_type_labels[l1->type],
                    rasqal_literal_type_labels[l2->type],
                    rasqal_literal_type_labels[type]);
#endif
    } else
      type=l1->type;
  } else {
    /* RDQL rules: compare as values with no promotion */
    if(l1->type != l2->type) {
      /* booleans can be compared to strings */
      if(l2->type == RASQAL_LITERAL_BOOLEAN &&
         l1->type == RASQAL_LITERAL_STRING)
        result=!strcmp((const char*)l1->string, (const char*)l2->string);
      goto tidy;
    }
    type=l1->type;
  }

  if(promotion) {
    l1_p = rasqal_new_literal_from_promotion(l1, type, flags);
    if(l1_p)
      l2_p = rasqal_new_literal_from_promotion(l2, type, flags);
    if(!l1_p || !l2_p) {
      result=1;
      goto tidy;
    }
  } else {
    l1_p=l1;
    l2_p=l2;
  }

  switch(type) {
    case RASQAL_LITERAL_URI:
      result = rasqal_literal_uri_equals(l1_p, l2_p);
      break;

    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_UDT:
      result = rasqal_literal_string_equals(l1_p, l2_p, error_p);
      break;

    case RASQAL_LITERAL_BLANK:
      result = rasqal_literal_blank_equals(l1_p, l2_p);
      break;

    case RASQAL_LITERAL_DATETIME:
      /* FIXME this should be xsd:dateTime equality */
      if(l1_p->string_len != l2_p->string_len)
        /* not-equal if lengths are different - cheap to compare this first */
        result=0;
      else
        result=!strcmp((const char*)l1_p->string, (const char*)l2_p->string);
      break;
      
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_BOOLEAN:
      result=l1_p->value.integer == l2_p->value.integer;
      break;

    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_FLOAT:
      result=l1_p->value.floating == l2_p->value.floating;
      break;

    case RASQAL_LITERAL_DECIMAL:
      result=rasqal_xsd_decimal_equals(l1_p->value.decimal,
                                       l2_p->value.decimal);
      break;

    case RASQAL_LITERAL_VARIABLE:
      /* both are variables */
      result=rasqal_literal_equals(l1_p->value.variable->value,
                                   l2_p->value.variable->value);
      
    case RASQAL_LITERAL_UNKNOWN:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    default:
      RASQAL_FATAL2("Literal type %d cannot be equaled", type);
      result=0; /* keep some compilers happy */
  }

  tidy:
  if(promotion) {
    if(l1_p)
      rasqal_free_literal(l1_p);
    if(l2_p)
      rasqal_free_literal(l2_p);
  }

#if RASQAL_DEBUG > 1
  RASQAL_DEBUG2("equals result %d\n", result);
#endif

  return result;
}


/*
 * rasqal_literal_expand_qname - INTERNAL Expand any qname in a literal into a URI
 * @user_data: #rasqal_query cast as void for use with raptor_sequence_foreach
 * @l: #rasqal_literal literal
 * 
 * Expands any QName inside the literal using prefixes that are
 * declared in the query that may not have been present when the
 * literal was first declared.  Intended to be used standalone
 * as well as with raptor_sequence_foreach which takes a function
 * signature that this function matches.
 * 
 * Return value: non-0 on failure
 **/
int
rasqal_literal_expand_qname(void *user_data, rasqal_literal *l)
{
  rasqal_query *rq=(rasqal_query *)user_data;

  if(l->type == RASQAL_LITERAL_QNAME) {
    /* expand a literal qname */
    raptor_uri *uri=raptor_qname_string_to_uri(rq->namespaces,
                                               l->string, l->string_len,
                                               (raptor_simple_message_handler)rasqal_query_simple_error, rq);
    if(!uri)
      return 1;
    RASQAL_FREE(cstring, (void*)l->string);
    l->string=NULL;
    l->type=RASQAL_LITERAL_URI;
    l->value.uri=uri;
  } else if (l->type == RASQAL_LITERAL_STRING) {
    raptor_uri *uri;
    
    if(l->flags) {
      /* expand a literal string datatype qname */
      uri=raptor_qname_string_to_uri(rq->namespaces,
                                     l->flags, 
                                     strlen((const char*)l->flags),
                                     (raptor_simple_message_handler)rasqal_query_simple_error, rq);
      if(!uri)
        return 1;
      l->datatype=uri;
      RASQAL_FREE(cstring, (void*)l->flags);
      l->flags=NULL;

      if(l->language && uri) {
        RASQAL_FREE(cstring, (void*)l->language);
        l->language=NULL;
      }

      if(rasqal_literal_string_to_native(l, (raptor_simple_message_handler)rasqal_query_simple_error, rq, 0)) {
        rasqal_free_literal(l);
        return 1;
      }
    }
  }
  return 0;
}


/*
 * rasqal_literal_has_qname - INTERNAL Check if literal has a qname part
 * @l: #rasqal_literal literal
 * 
 * Checks if any part ofthe literal has an unexpanded QName.
 * 
 * Return value: non-0 if a QName is present
 **/
int
rasqal_literal_has_qname(rasqal_literal *l) {
  return (l->type == RASQAL_LITERAL_QNAME) ||
         (l->type == RASQAL_LITERAL_STRING && (l->flags));
}


/**
 * rasqal_literal_as_node:
 * @l: #rasqal_literal object
 *
 * Turn a literal into a new RDF string, URI or blank literal.
 * 
 * Return value: the new #rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_literal_as_node(rasqal_literal* l)
{
  raptor_uri* dt_uri;
  rasqal_literal* new_l=NULL;
  
  reswitch:
  if(!l)
    return NULL;
  switch(l->type) {
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_BLANK:
      new_l=rasqal_new_literal_from_literal(l);
      break;
      
    case RASQAL_LITERAL_VARIABLE:
      l=l->value.variable->value;
      goto reswitch;

    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_DECIMAL:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_UDT:
      new_l=(rasqal_literal*)RASQAL_CALLOC(rasqal_literal, 1, sizeof(rasqal_literal));
      if(new_l) {
        new_l->valid=1;
        new_l->usage=1;
        new_l->world=l->world;
        new_l->type=RASQAL_LITERAL_STRING;
        new_l->string_len=l->string_len;
        new_l->string=(unsigned char*)RASQAL_MALLOC(cstring, l->string_len+1);
        if(!new_l->string) {
          rasqal_free_literal(new_l);
          return NULL; 
        }
        strncpy((char*)new_l->string, (const char*)l->string, l->string_len+1);
        dt_uri=rasqal_xsd_datatype_type_to_uri(l->world, l->type);
        if(!dt_uri) {
          rasqal_free_literal(new_l);
          return NULL;
        }
#ifdef RAPTOR_V2_AVAILABLE
        new_l->datatype = raptor_uri_copy_v2(l->world->raptor_world_ptr, dt_uri);
#else
        new_l->datatype = raptor_uri_copy(dt_uri);
#endif
        new_l->flags=NULL;
      }
      break;
      
    case RASQAL_LITERAL_QNAME:
      /* QNames should be gone by the time expression eval happens */

    case RASQAL_LITERAL_PATTERN:
      /* FALLTHROUGH */

    case RASQAL_LITERAL_UNKNOWN:
    default:
      RASQAL_FATAL2("Literal type %d has no node value", l->type);
  }
  
  return new_l;
}


/*
 * rasqal_literal_ebv - INTERNAL Get the rasqal_literal effective boolean value
 * @l: #rasqal_literal literal
 * 
 * Return value: non-0 if EBV is true, else false
 **/
int
rasqal_literal_ebv(rasqal_literal* l) 
{
  rasqal_variable* v;
  /* Result is true unless... */
  int b=1;
  
  v=rasqal_literal_as_variable(l);
  if(v) {
    if(v->value == NULL) {
      /* ... The operand is unbound */
      b=0;
      goto done;
    }
    l=v->value;
  }
  
  if(l->type == RASQAL_LITERAL_BOOLEAN && !l->value.integer) {
    /* ... The operand is an xs:boolean with a FALSE value. */
    b=0;
  } else if(l->type == RASQAL_LITERAL_STRING && 
            !l->datatype && !l->string_len) {
    /* ... The operand is a 0-length untyped RDF literal or xs:string. */
    b=0;
  } else if((l->type == RASQAL_LITERAL_INTEGER && !l->value.integer) ||
            ((l->type == RASQAL_LITERAL_DOUBLE || 
              l->type == RASQAL_LITERAL_FLOAT) &&
             !l->value.floating)
            ) {
    /* ... The operand is any numeric type with a value of 0. */
    b=0;
  } else if(l->type == RASQAL_LITERAL_DECIMAL &&
            rasqal_xsd_decimal_is_zero(l->value.decimal)) {
    /* ... The operand is any numeric type with a value of 0 (decimal) */
    b=0;
  } else if((l->type == RASQAL_LITERAL_DOUBLE || 
             l->type == RASQAL_LITERAL_FLOAT) &&
            isnan(l->value.floating)
            ) {
    /* ... The operand is an xs:double or xs:float with a value of NaN */
    b=0;
  }
  
  done:
  return b;
}


/*
 * rasqal_literal_is_constant - INTERNAL Check if a literal is a constant
 * @l: #rasqal_literal literal
 * 
 * Return value: non-0 if literal is a constant
 **/
int
rasqal_literal_is_constant(rasqal_literal* l)
{
  switch(l->type) {
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_DECIMAL:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_UDT:
      return 1;

    case RASQAL_LITERAL_VARIABLE:
      return 0;

    case RASQAL_LITERAL_UNKNOWN:
    default:
      RASQAL_FATAL2("Literal type %d cannot be checked for constant", l->type);
      return 0; /* keep some compilers happy */
  }
}


rasqal_formula*
rasqal_new_formula(void) 
{
  return (rasqal_formula*)RASQAL_CALLOC(rasqal_formula, 1, sizeof(rasqal_formula));
}

void
rasqal_free_formula(rasqal_formula* formula)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(formula, rasqal_formula);
  
  if(formula->triples)
    raptor_free_sequence(formula->triples);
  if(formula->value)
    rasqal_free_literal(formula->value);
  RASQAL_FREE(rasqal_formula, formula);
}
  

void
rasqal_formula_print(rasqal_formula* formula, FILE *stream)
{
  fputs("formula(triples=", stream);
  if(formula->triples)
    raptor_sequence_print(formula->triples, stream);
  else
    fputs("[]", stream);
  fputs(", value=", stream);
  if(formula->value)
    rasqal_literal_print(formula->value, stream);
  else
    fputs("NULL", stream);
  fputc(')', stream);
}


rasqal_formula*
rasqal_formula_join(rasqal_formula* first_formula, 
                    rasqal_formula* second_formula)
{
  if(!first_formula && !second_formula)
    return NULL;

  if(!first_formula)
    return second_formula;
  
  if(!second_formula)
    return first_formula;
  
  if(first_formula->triples || second_formula->triples) {
    if(!first_formula->triples) {
      first_formula->triples=second_formula->triples;
      second_formula->triples=NULL;
    } else if(second_formula->triples)
      if(raptor_sequence_join(first_formula->triples, second_formula->triples)) {
        rasqal_free_formula(first_formula);
        first_formula=NULL;
      }
  }
  rasqal_free_formula(second_formula);

  return first_formula;
}


/**
 * rasqal_literal_datatype:
 * @l: #rasqal_literal object
 *
 * Get the datatype URI of a literal
 *
 * Return value: shared pointer to #raptor_uri of datatype or NULL on failure or no value
 */
raptor_uri*
rasqal_literal_datatype(rasqal_literal* l)
{
  if(!l)
    return NULL;
  
  if(l->type != RASQAL_LITERAL_VARIABLE)
    return l->datatype;
  return rasqal_literal_datatype(l->value.variable->value);
}


rasqal_literal*
rasqal_literal_cast(rasqal_literal* l, raptor_uri* to_datatype, int flags, 
                    int* error_p)
{
  raptor_uri* from_datatype=NULL;
  const unsigned char *string=NULL;
  unsigned char *new_string;
  rasqal_literal* result=NULL;
  rasqal_literal_type from_native_type;
  rasqal_literal_type to_native_type;

  l=rasqal_literal_value(l);
  if(!l)
    return NULL;

  from_datatype=l->datatype;
  from_native_type=l->type;

  to_native_type=rasqal_xsd_datatype_uri_to_type(l->world, to_datatype);

  if(from_native_type == to_native_type) {
    /* cast to same type is always allowed */
    return rasqal_new_literal_from_literal(l);

  } else {
    /* switch on FROM type to check YES/NO conversions and get the string */
    switch(from_native_type) {
      /* string */
      case RASQAL_LITERAL_STRING:
      case RASQAL_LITERAL_XSD_STRING:
      case RASQAL_LITERAL_UDT:
        string=l->string;
        break;

      /* XSD datatypes: RASQAL_LITERAL_FIRST_XSD to RASQAL_LITERAL_LAST_XSD */
      case RASQAL_LITERAL_BOOLEAN:
      case RASQAL_LITERAL_INTEGER:
      case RASQAL_LITERAL_DOUBLE:
      case RASQAL_LITERAL_FLOAT:
      case RASQAL_LITERAL_DECIMAL:
        /* XSD (boolean, integer, decimal, double, float) may NOT be
         * cast to dateTime */
        if(to_native_type == RASQAL_LITERAL_DATETIME) {
          *error_p=1;
          break;
        }
        string=l->string;
        break;

      case RASQAL_LITERAL_DATETIME:
        string=l->string;
        break;

      /* SPARQL casts - FIXME */
      case RASQAL_LITERAL_BLANK:
      case RASQAL_LITERAL_PATTERN:
      case RASQAL_LITERAL_QNAME:
        string=l->string;
        break;

      case RASQAL_LITERAL_URI:
        /* URI (IRI) May ONLY be cast to an xsd:string */
        if(to_native_type != RASQAL_LITERAL_XSD_STRING) {
          *error_p=1;
          break;
        }

#ifdef RAPTOR_V2_AVAILABLE
        string = raptor_uri_as_string_v2(l->world->raptor_world_ptr, l->value.uri);
#else
        string = raptor_uri_as_string(l->value.uri);
#endif
        break;

      case RASQAL_LITERAL_VARIABLE:
        /* fallthrough since rasqal_literal_value() handled this above */
      case RASQAL_LITERAL_UNKNOWN:
      default:
        RASQAL_FATAL2("Literal type %d cannot be cast", l->type);
        return NULL; /* keep some compilers happy */
    }

    if(to_native_type == RASQAL_LITERAL_DATETIME) {
      /* XSD dateTime may ONLY be cast from string (cast from dateTime
       * is checked above)
       */
      if(from_native_type != RASQAL_LITERAL_STRING) {
        *error_p=1;
      }
    }

    if(*error_p)
      return NULL;

  }
  

  /* switch on the TO type to check MAYBE conversions */

#ifdef RAPTOR_V2_AVAILABLE
  RASQAL_DEBUG4("CAST from \"%s\" type %s to type %s\n",
                string, 
                from_datatype ? (const char*)raptor_uri_as_string_v2(l->world->raptor_world_ptr, from_datatype) : "(NONE)",
                raptor_uri_as_string_v2(l->world->raptor_world_ptr, to_datatype));
#else
  RASQAL_DEBUG4("CAST from \"%s\" type %s to type %s\n",
                string, 
                from_datatype ? (const char*)raptor_uri_as_string(from_datatype) : "(NONE)",
                raptor_uri_as_string(to_datatype));
#endif
  
  if(!rasqal_xsd_datatype_check(to_native_type, string, flags)) {
    *error_p=1;
    RASQAL_DEBUG3("Illegal cast to type %s string '%s'",
                  rasqal_xsd_datatype_label(to_native_type), string);
    return NULL;
  }

  new_string=(unsigned char*)RASQAL_MALLOC(string, 
                                           strlen((const char*)string)+1);
  if(!new_string) {
    *error_p=1;
    return NULL;
  }
  strcpy((char*)new_string, (const char*)string);
#ifdef RAPTOR_V2_AVAILABLE
  to_datatype = raptor_uri_copy_v2(l->world->raptor_world_ptr, to_datatype);
#else
  to_datatype = raptor_uri_copy(to_datatype);  
#endif
  
  result=rasqal_new_string_literal(l->world, new_string, NULL, to_datatype, NULL);
  if(!result)
    *error_p=1;
  return result;
}


/**
 * rasqal_literal_value:
 * @l: #rasqal_literal object
 *
 * Get the literal value looking up any variables needed
 *
 * Return value: literal value or NULL if has no value
 */
rasqal_literal*
rasqal_literal_value(rasqal_literal* l)
{
  while(l) {
    if(l->type != RASQAL_LITERAL_VARIABLE)
      break;

    l=l->value.variable->value;
  }
  
  return l;
}


int
rasqal_literal_is_numeric(rasqal_literal* literal)
{
  rasqal_literal_type parent_type;
  parent_type = rasqal_xsd_datatype_parent_type(literal->type);

  return (rasqal_xsd_datatype_is_numeric(literal->type) ||
          rasqal_xsd_datatype_is_numeric(parent_type));
}


rasqal_literal*
rasqal_literal_add(rasqal_literal* l1, rasqal_literal* l2, int *error_p)
{
  int i;
  double d;
  rasqal_xsd_decimal* dec;
  int error=0;
  rasqal_literal_type type;
  rasqal_literal* l1_p=NULL;
  rasqal_literal* l2_p=NULL;
  int flags=0;
  rasqal_literal* result=NULL;
  
  type=rasqal_literal_promote_numerics(l1, l2, flags);
  switch(type) {
    case RASQAL_LITERAL_INTEGER:
      i=rasqal_literal_as_integer(l1, &error);
      if(error)
        break;
      i=i + rasqal_literal_as_integer(l2, &error);
      if(error)
        break;

      result=rasqal_new_integer_literal(l1->world, RASQAL_LITERAL_INTEGER, i);
      break;
      
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_DOUBLE:
      d=rasqal_literal_as_floating(l1, &error);
      if(error)
        break;
      d=d + rasqal_literal_as_floating(l2, &error);
      if(error)
        break;

      result=rasqal_new_numeric_literal(l1->world, type, d);
      break;
      
    case RASQAL_LITERAL_DECIMAL:
      l1_p = rasqal_new_literal_from_promotion(l1, type, flags);
      if(l1_p)
        l2_p = rasqal_new_literal_from_promotion(l2, type, flags);
      if(l1_p && l2_p) {
        dec=rasqal_new_xsd_decimal();
        if(rasqal_xsd_decimal_add(dec, l1_p->value.decimal,
                                  l2_p->value.decimal)) {
          error=1;
          rasqal_free_xsd_decimal(dec);
        } else
          result=rasqal_new_decimal_literal_from_decimal(l1->world, NULL, dec);
      }
      break;
      
    case RASQAL_LITERAL_UNKNOWN:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_VARIABLE:
    case RASQAL_LITERAL_UDT:
    default:
      error=1;
      break;
  }

  if(error) {
    if(error_p)
      *error_p=1;
  }
  
  if(l1_p)
    rasqal_free_literal(l1_p);
  if(l2_p)
    rasqal_free_literal(l2_p);

  return result;
}


rasqal_literal*
rasqal_literal_subtract(rasqal_literal* l1, rasqal_literal* l2, int *error_p)
{
  int i;
  double d;
  rasqal_xsd_decimal* dec;
  int error=0;
  rasqal_literal_type type;
  rasqal_literal* l1_p=NULL;
  rasqal_literal* l2_p=NULL;
  int flags=0;
  rasqal_literal* result=NULL;
  
  type=rasqal_literal_promote_numerics(l1, l2, flags);
  switch(type) {
    case RASQAL_LITERAL_INTEGER:
      i=rasqal_literal_as_integer(l1, &error);
      if(error)
        break;
      i=i - rasqal_literal_as_integer(l2, &error);
      if(error)
        break;

      result=rasqal_new_integer_literal(l1->world, RASQAL_LITERAL_INTEGER, i);
      break;
      
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_DOUBLE:
      d=rasqal_literal_as_floating(l1, &error);
      if(error)
        break;
      d=d - rasqal_literal_as_floating(l2, &error);
      if(error)
        break;

      result=rasqal_new_numeric_literal(l1->world, type, d);
      break;
      
    case RASQAL_LITERAL_DECIMAL:
      l1_p = rasqal_new_literal_from_promotion(l1, type, flags);
      if(l1_p)
        l2_p = rasqal_new_literal_from_promotion(l2, type, flags);
      if(l1_p && l2_p) {
        dec=rasqal_new_xsd_decimal();
        if(rasqal_xsd_decimal_subtract(dec, l1_p->value.decimal,
                                       l2_p->value.decimal)) {
          error=1;
          rasqal_free_xsd_decimal(dec);
        } else
          result=rasqal_new_decimal_literal_from_decimal(l1->world, NULL, dec);
      }
      break;
      
    case RASQAL_LITERAL_UNKNOWN:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_VARIABLE:
    case RASQAL_LITERAL_UDT:
    default:
      error=1;
      break;
  }

  if(error) {
    if(error_p)
      *error_p=1;
  }
  
  if(l1_p)
    rasqal_free_literal(l1_p);
  if(l2_p)
    rasqal_free_literal(l2_p);

  return result;
}


rasqal_literal*
rasqal_literal_multiply(rasqal_literal* l1, rasqal_literal* l2, int *error_p)
{
  int i;
  double d;
  rasqal_xsd_decimal* dec;
  int error=0;
  rasqal_literal_type type;
  rasqal_literal* l1_p=NULL;
  rasqal_literal* l2_p=NULL;
  int flags=0;
  rasqal_literal* result=NULL;
  
  type=rasqal_literal_promote_numerics(l1, l2, flags);
  switch(type) {
    case RASQAL_LITERAL_INTEGER:
      i=rasqal_literal_as_integer(l1, &error);
      if(error)
        break;
      i=i * rasqal_literal_as_integer(l2, &error);
      if(error)
        break;

      result=rasqal_new_integer_literal(l1->world, RASQAL_LITERAL_INTEGER, i);
      break;
      
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_DOUBLE:
      d=rasqal_literal_as_floating(l1, &error);
      if(error)
        break;
      d=d * rasqal_literal_as_floating(l2, &error);
      if(error)
        break;

      result=rasqal_new_numeric_literal(l1->world, type, d);
      break;
      
    case RASQAL_LITERAL_DECIMAL:
      l1_p = rasqal_new_literal_from_promotion(l1, type, flags);
      if(l1_p)
        l2_p = rasqal_new_literal_from_promotion(l2, type, flags);
      if(l1_p && l2_p) {
        dec=rasqal_new_xsd_decimal();
        if(rasqal_xsd_decimal_multiply(dec, l1_p->value.decimal,
                                       l2_p->value.decimal)) {
          error=1;
          rasqal_free_xsd_decimal(dec);
        } else
          result=rasqal_new_decimal_literal_from_decimal(l1->world, NULL, dec);
      }
      break;
      
    case RASQAL_LITERAL_UNKNOWN:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_VARIABLE:
    case RASQAL_LITERAL_UDT:
    default:
      error=1;
      break;
  }

  if(error) {
    if(error_p)
      *error_p=1;
  }
  
  if(l1_p)
    rasqal_free_literal(l1_p);
  if(l2_p)
    rasqal_free_literal(l2_p);

  return result;
}


rasqal_literal*
rasqal_literal_divide(rasqal_literal* l1, rasqal_literal* l2, int *error_p)
{
  int i1, i2;
  double d1, d2;
  rasqal_xsd_decimal* dec;
  int error=0;
  rasqal_literal_type type;
  rasqal_literal* l1_p=NULL;
  rasqal_literal* l2_p=NULL;
  int flags=0;
  rasqal_literal* result=NULL;
  
  type=rasqal_literal_promote_numerics(l1, l2, flags);
  switch(type) {
    case RASQAL_LITERAL_INTEGER:
      i2=rasqal_literal_as_integer(l2, &error);
      if(!i2)
        error=1;
      if(error)
        break;
      i1=rasqal_literal_as_integer(l1, &error);
      if(error)
        break;
      i1=i1 / i2;
      if(error)
        break;

      result=rasqal_new_integer_literal(l1->world, RASQAL_LITERAL_INTEGER, i1);
      break;
      
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_DOUBLE:
      d2=rasqal_literal_as_floating(l2, &error);
      if(!d2)
        error=1;
      if(error)
        break;
      d1=rasqal_literal_as_floating(l1, &error);
      if(error)
        break;
      d1=d1 / d2;
      if(error)
        break;

      result=rasqal_new_numeric_literal(l1->world, type, d1);
      break;
      
    case RASQAL_LITERAL_DECIMAL:
      l1_p = rasqal_new_literal_from_promotion(l1, type, flags);
      if(l1_p)
        l2_p = rasqal_new_literal_from_promotion(l2, type, flags);
      if(l1_p && l2_p) {
        dec=rasqal_new_xsd_decimal();
        if(rasqal_xsd_decimal_add(dec, l1_p->value.decimal,
                                  l2_p->value.decimal)) {
          error=1;
          rasqal_free_xsd_decimal(dec);
        } else
          result=rasqal_new_decimal_literal_from_decimal(l1->world, NULL, dec);
      }
      break;
      
    case RASQAL_LITERAL_UNKNOWN:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_VARIABLE:
    case RASQAL_LITERAL_UDT:
    default:
      error=1;
      break;
  }

  if(error) {
    if(error_p)
      *error_p=1;
  }
  
  if(l1_p)
    rasqal_free_literal(l1_p);
  if(l2_p)
    rasqal_free_literal(l2_p);

  return result;
}


rasqal_literal*
rasqal_literal_negate(rasqal_literal* l, int *error_p)
{
  int i;
  double d;
  rasqal_xsd_decimal* dec;
  int error=0;
  rasqal_literal* result=NULL;
  
  switch(l->type) {
    case RASQAL_LITERAL_INTEGER:
      i=rasqal_literal_as_integer(l, &error);
      if(error)
        break;
      i= -i;
      result=rasqal_new_integer_literal(l->world, RASQAL_LITERAL_INTEGER, i);
      break;
      
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_DOUBLE:
      d=rasqal_literal_as_floating(l, &error);
      if(!d)
        error=1;
      d= -d;
      result=rasqal_new_numeric_literal(l->world, l->type, d);
      break;
      
    case RASQAL_LITERAL_DECIMAL:
      dec=rasqal_new_xsd_decimal();
      if(rasqal_xsd_decimal_negate(dec, l->value.decimal)) {
        error=1;
        rasqal_free_xsd_decimal(dec);
      } else
        result=rasqal_new_decimal_literal_from_decimal(l->world, NULL, dec);
      break;
      
    case RASQAL_LITERAL_UNKNOWN:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_VARIABLE:
    case RASQAL_LITERAL_UDT:
    default:
      error=1;
      break;
  }

  if(error) {
    if(error_p)
      *error_p=1;
  }
  
  return result;
}


/**
 * rasqal_literal_same_term:
 * @l1: #rasqal_literal literal
 * @l2: #rasqal_literal data literal
 *
 * Check if literals are same term (URI, literal, blank)
 * 
 * Return value: non-0 if same
 **/
int
rasqal_literal_same_term(rasqal_literal* l1, rasqal_literal* l2)
{
  rasqal_literal_type type1 = rasqal_literal_get_rdf_term_type(l1);
  rasqal_literal_type type2 = rasqal_literal_get_rdf_term_type(l2);

  if(type1 != type2)
    return 0;
  
  if(type1 == RASQAL_LITERAL_UNKNOWN)
    return 0;
  
  if(type1 == RASQAL_LITERAL_URI)
    return rasqal_literal_uri_equals(l1, l2);

  if(type1 == RASQAL_LITERAL_STRING)
    return rasqal_literal_string_equals(l1, l2, NULL);

  if(type1 == RASQAL_LITERAL_BLANK)
    return rasqal_literal_blank_equals(l1, l2);

  return 0;
}

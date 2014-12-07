/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_literal.c - Rasqal literals
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

/* for strtof() and round() prototypes */
#define _ISOC99_SOURCE 1

#include <stdio.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <ctype.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
/* for ptrdiff_t */
#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif
#include <stdarg.h>
/* for isnan() */
#ifdef HAVE_MATH_H
#include <math.h>
#endif
/* for INT_MIN and INT_MAX */
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FLOAT_H
#include <float.h>
#endif

#include "rasqal.h"
#include "rasqal_internal.h"

#define DEBUG_FH stderr


#ifndef STANDALONE

/* prototypes */
static rasqal_literal_type rasqal_literal_promote_numerics(rasqal_literal* l1, rasqal_literal* l2, int flags);
static int rasqal_literal_set_typed_value(rasqal_literal* l, rasqal_literal_type type, const unsigned char* string, int canonicalize);


const unsigned char* rasqal_xsd_boolean_true = (const unsigned char*)"true";
const unsigned char* rasqal_xsd_boolean_false = (const unsigned char*)"false";


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
rasqal_new_integer_literal(rasqal_world* world, rasqal_literal_type type,
                           int integer)
{
  raptor_uri* dt_uri;
  rasqal_literal* l;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);

  l  = RASQAL_CALLOC(rasqal_literal*, 1, sizeof(*l));
  if(l) {
    l->valid = 1;
    l->usage = 1;
    l->world = world;
    l->type = type;
    l->value.integer = integer;
    if(type == RASQAL_LITERAL_BOOLEAN) {
       /* static l->string for boolean, does not need freeing */
       l->string = integer ? rasqal_xsd_boolean_true : rasqal_xsd_boolean_false;
       l->string_len = integer ? RASQAL_XSD_BOOLEAN_TRUE_LEN : RASQAL_XSD_BOOLEAN_FALSE_LEN;
    } else  {
      size_t slen = 0;      
      l->string = rasqal_xsd_format_integer(integer, &slen);
      l->string_len = RASQAL_BAD_CAST(unsigned int, slen);
      if(!l->string) {
        rasqal_free_literal(l);
        return NULL;
      }
    }
    dt_uri = rasqal_xsd_datatype_type_to_uri(world, l->type);
    if(!dt_uri) {
      rasqal_free_literal(l);
      return NULL;
    }
    l->datatype = raptor_uri_copy(dt_uri);
    l->parent_type = rasqal_xsd_datatype_parent_type(type);
  }
  return l;
}


/**
 * rasqal_new_numeric_literal_from_long:
 * @world: rasqal world object
 * @type: Type of literal such as RASQAL_LITERAL_INTEGER or RASQAL_LITERAL_BOOLEAN
 * @value: long value
 *
 * Constructor - Create a new Rasqal numeric literal from a long.
 * 
 * The value is turned into a rasqal integer or decimal literal and
 * given a datatype of xsd:integer
 * 
 * Return value: New #rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_numeric_literal_from_long(rasqal_world* world,
                                     rasqal_literal_type type,
                                     long value)
{
  rasqal_xsd_decimal* d;
  unsigned char *string;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);

  /* boolean values should always be in range */
  if(type == RASQAL_LITERAL_BOOLEAN) {
    int ivalue = value ? 1 : 0;
    return rasqal_new_integer_literal(world, type, ivalue);
  }
  
  /* For other types, if in int range, make an integer literal */
  if(value >= INT_MIN && value <= INT_MAX) {
    return rasqal_new_integer_literal(world, type, RASQAL_GOOD_CAST(int, value));
  }

  /* Otherwise turn it into a decimal */
  d = rasqal_new_xsd_decimal(world);
  rasqal_xsd_decimal_set_long(d, value);
  string = RASQAL_GOOD_CAST(unsigned char*, rasqal_xsd_decimal_as_counted_string(d, NULL));

  return rasqal_new_decimal_literal_from_decimal(world, string, d);
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
rasqal_new_typed_literal(rasqal_world* world, rasqal_literal_type type,
                         const unsigned char* string)
{
  rasqal_literal* l;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);

  l = RASQAL_CALLOC(rasqal_literal*, 1, sizeof(*l));
  if(!l)
    return NULL;

  l->valid = 1;
  l->usage = 1;
  l->world = world;
  l->type = type;

  if(!rasqal_xsd_datatype_check(type, string, 0)) {
    rasqal_free_literal(l);
    return NULL;
  }

  if(rasqal_literal_set_typed_value(l, type, string, 0)) {
    rasqal_free_literal(l);
    l = NULL;
  }

  return l;
}


/**
 * rasqal_new_floating_literal:
 * @world: rasqal world object
 * @type: type - #RASQAL_LITERAL_FLOAT or #RASQAL_LITERAL_DOUBLE
 * @d:  floating literal (double)
 * 
 * Constructor - Create a new Rasqal float literal from a double.
 *
 * Return value: New #rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_floating_literal(rasqal_world *world,
                            rasqal_literal_type type, double d)
{
  raptor_uri* dt_uri;
  rasqal_literal* l;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);

  if(type != RASQAL_LITERAL_FLOAT && type != RASQAL_LITERAL_DOUBLE)
    return NULL;

  l = RASQAL_CALLOC(rasqal_literal*, 1, sizeof(*l));
  if(l) {
    size_t slen = 0;
    l->valid = 1;
    l->usage = 1;
    l->world = world;
    l->type = type;
    l->value.floating = d;
    l->string = rasqal_xsd_format_double(d, &slen);
    l->string_len = RASQAL_BAD_CAST(unsigned int, slen);
    if(!l->string) {
      rasqal_free_literal(l);
      return NULL;
    }
    dt_uri = rasqal_xsd_datatype_type_to_uri(world, l->type);
    if(!dt_uri) {
      rasqal_free_literal(l);
      return NULL;
    }
    l->datatype = raptor_uri_copy(dt_uri);
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
rasqal_new_double_literal(rasqal_world* world, double d)
{
  return rasqal_new_floating_literal(world, RASQAL_LITERAL_DOUBLE, d);
}


#ifndef RASQAL_DISABLE_DEPRECATED
/**
 * rasqal_new_float_literal:
 * @world: rasqal world object
 * @f:  float literal
 *
 * Constructor - Create a new Rasqal float literal.
 *
 * @Deprecated: Use rasqal_new_floating_literal() with type
 * #RASQAL_LITERAL_FLOAT and double value.
 *
 * Return value: New #rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_float_literal(rasqal_world *world, float f)
{
  return rasqal_new_floating_literal(world, RASQAL_LITERAL_FLOAT, (double)f);
}
#endif

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
  rasqal_literal* l;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);

  l = RASQAL_CALLOC(rasqal_literal*, 1, sizeof(*l));
  if(l) {
    l->valid = 1;
    l->usage = 1;
    l->world = world;
    l->type = RASQAL_LITERAL_URI;
    l->value.uri = uri;
  } else {
    raptor_free_uri(uri);
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
  rasqal_literal* l;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(pattern, char*, NULL);

  l = RASQAL_CALLOC(rasqal_literal*, 1, sizeof(*l));
  if(l) {
    l->valid = 1;
    l->usage = 1;
    l->world = world;
    l->type = RASQAL_LITERAL_PATTERN;
    l->string = pattern;
    l->string_len = RASQAL_BAD_CAST(unsigned int, strlen(RASQAL_GOOD_CAST(const char*, pattern)));
    l->flags = RASQAL_GOOD_CAST(const unsigned char*, flags);
  } else {
    if(flags)
      RASQAL_FREE(char*, flags);
    RASQAL_FREE(char*, pattern);
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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(string, char*, NULL);

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

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);
  /* string and decimal NULLness are checked below */

  l = RASQAL_CALLOC(rasqal_literal*, 1, sizeof(*l));
  if(!l)
    return NULL;
  
  l->valid = 1;
  l->usage = 1;
  l->world = world;
  l->type = RASQAL_LITERAL_DECIMAL;
  if(string) {
    if(!rasqal_xsd_datatype_check(l->type, string, 0)) {
      rasqal_free_literal(l);
      return NULL;
    }

    if(rasqal_literal_set_typed_value(l, l->type, string, 0)) {
      rasqal_free_literal(l);
      l = NULL;
    }
  } else if(decimal) {
    dt_uri = rasqal_xsd_datatype_type_to_uri(world, l->type);
    if(!dt_uri) {
      rasqal_free_literal(l);
      l = NULL;
    } else {
      size_t slen = 0;      
      l->datatype = raptor_uri_copy(dt_uri);
      l->value.decimal = decimal;
      /* string is owned by l->value.decimal */
      l->string = RASQAL_GOOD_CAST(unsigned char*, rasqal_xsd_decimal_as_counted_string(l->value.decimal, &slen));
      l->string_len = RASQAL_BAD_CAST(unsigned int, slen);
      if(!l->string) {
        rasqal_free_literal(l);
        l = NULL;
      }
    }
  } else {
    /* no string or decimal was given */
    rasqal_free_literal(l);
    l = NULL;
  }
  
  return l;
}


/*
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
rasqal_new_numeric_literal(rasqal_world* world, rasqal_literal_type type,
                           double d)
{
  char buffer[30];
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);

  switch(type) {
    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_FLOAT:
      return rasqal_new_floating_literal(world, type, d);

    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_INTEGER_SUBTYPE: 
      if(d >= (double)INT_MIN && d <= (double)INT_MAX)
        return rasqal_new_integer_literal(world, type, RASQAL_GOOD_CAST(int, d));

      /* otherwise FALLTHROUGH and make it a decimal */

    case RASQAL_LITERAL_DECIMAL:
      sprintf(buffer, "%g", d);
      return rasqal_new_decimal_literal(world, RASQAL_GOOD_CAST(unsigned char*, buffer));

    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_DATE:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_UNKNOWN:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_VARIABLE:
    case RASQAL_LITERAL_UDT:
      RASQAL_FATAL2("Unexpected numeric type %u", type);
  }

  return NULL;
}


/**
 * rasqal_new_datetime_literal_from_datetime:
 * @world: rasqal world object
 * @dt: rasqal XSD Datetime
 *
 * Constructor - Create a new Rasqal datetime literal from an existing datetime.
 * 
 * Takes ownership of @dt
 *
 * Return value: New #rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_datetime_literal_from_datetime(rasqal_world* world,
                                          rasqal_xsd_datetime* dt)
{
  rasqal_literal* l;
  raptor_uri *dt_uri;
  size_t slen = 0;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(dt, rasqal_xsd_datetime, NULL);

  l = RASQAL_CALLOC(rasqal_literal*, 1, sizeof(*l));
  if(!l)
    goto failed;
  
  l->valid = 1;
  l->usage = 1;
  l->world = world;
  l->type = RASQAL_LITERAL_DATETIME;

  dt_uri = rasqal_xsd_datatype_type_to_uri(world, l->type);
  if(!dt_uri)
    goto failed;

  l->datatype = raptor_uri_copy(dt_uri);

  l->value.datetime = dt;
  
  l->string = RASQAL_GOOD_CAST(unsigned char*, rasqal_xsd_datetime_to_counted_string(l->value.datetime, &slen));
  l->string_len = RASQAL_BAD_CAST(unsigned int, slen);
  if(!l->string)
    goto failed;

  return l;

  failed:
  if(l)
    rasqal_free_literal(l);
  if(dt)
    rasqal_free_xsd_datetime(dt);

  return NULL;
}



/*
 * rasqal_literal_set_typed_value:
 * @l: literal
 * @type: type
 * @string: string or NULL to use existing literal string
 * @canonicalize: non-0 to canonicalize the existing string
 *
 * INTERNAL - Set a literal typed value
 *
 * Return value: non-0 on failure
 **/
static int
rasqal_literal_set_typed_value(rasqal_literal* l, rasqal_literal_type type,
                               const unsigned char* string,
                               int canonicalize)
{  
  char *eptr;
  raptor_uri* dt_uri;
  int i;
  double d;
  rasqal_literal_type original_type = l->type;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l, rasqal_literal, 1);

retype:
  l->valid = rasqal_xsd_datatype_check(type, string ? string : l->string,
                                       0 /* no flags set */);
  if(!l->valid) {
    RASQAL_DEBUG3("Invalid type %s string '%s' - setting to type UDT\n",
                  rasqal_xsd_datatype_label(type), string ? string : l->string);
    type = RASQAL_LITERAL_UDT;
  }
            
  if(l->language) {
    RASQAL_FREE(char*, l->language);
    l->language = NULL;
  }
  l->type = type;

  if(string && l->type != RASQAL_LITERAL_DECIMAL) {
    if(l->string)
      RASQAL_FREE(char*, l->string);

    l->string_len = RASQAL_BAD_CAST(unsigned int, strlen(RASQAL_GOOD_CAST(const char*, string)));
    l->string = RASQAL_MALLOC(unsigned char*, l->string_len + 1);
    if(!l->string)
      return 1;
    memcpy((void*)l->string, string, l->string_len + 1);
  }

  if(l->type <= RASQAL_LITERAL_LAST_XSD) {
    dt_uri = rasqal_xsd_datatype_type_to_uri(l->world, l->type);
    if(!dt_uri)
      return 1;

    if(l->datatype)
      raptor_free_uri(l->datatype);
    l->datatype = raptor_uri_copy(dt_uri);

    l->parent_type = rasqal_xsd_datatype_parent_type(type);
  }

  switch(type) {
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_INTEGER_SUBTYPE:
      if(1) {
        long long_i;

        eptr = NULL;
        errno = 0;
        long_i = strtol(RASQAL_GOOD_CAST(const char*, l->string), &eptr, 10);
        if(*eptr)
          return 1;

        if(errno == ERANGE) {
          /* under or overflow of strtol() so fallthrough to DECIMAL */

        } else if(long_i >= INT_MIN && long_i <= INT_MAX) {
          l->value.integer = RASQAL_GOOD_CAST(int, long_i);
          break;
        }
      }
      
      /* Will not fit in an int so turn it into a decimal */
      type = RASQAL_LITERAL_DECIMAL;
      goto retype;

    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_FLOAT:
      if(1) {
        size_t slen = 0;
        d = 0.0;
        (void)sscanf(RASQAL_GOOD_CAST(char*, l->string), "%lf", &d);
        l->value.floating = d;
        if(canonicalize) {
          RASQAL_FREE(char*, l->string);
          l->string = rasqal_xsd_format_double(d, &slen);
          l->string_len = RASQAL_BAD_CAST(unsigned int, slen);
        }
      }
      break;

    case RASQAL_LITERAL_DECIMAL:
      if(1) {
        size_t slen = 0;
        rasqal_xsd_decimal* new_d;

        new_d = rasqal_new_xsd_decimal(l->world);
        if(!new_d)
          return 1;

        if(!string)
          /* use existing literal decimal object (SHARED) string */
          string = l->string;

        if(rasqal_xsd_decimal_set_string(new_d,
                                         RASQAL_GOOD_CAST(const char*, string))) {
          rasqal_free_xsd_decimal(new_d);
          return 1;
        }
        if(l->value.decimal) {
          rasqal_free_xsd_decimal(l->value.decimal);
        }
        l->value.decimal = new_d;

        /* old l->string is now invalid and MAY need to be freed */
        if(l->string && original_type != RASQAL_LITERAL_DECIMAL)
          RASQAL_FREE(char*, l->string);

        /* new l->string is owned by l->value.decimal and will be
         * freed on literal destruction
         */
        l->string = RASQAL_GOOD_CAST(unsigned char*, rasqal_xsd_decimal_as_counted_string(l->value.decimal, &slen));
        l->string_len = RASQAL_BAD_CAST(unsigned int, slen);
        if(!l->string)
          return 1;
      }
      break;

    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_UDT:
      /* No change - kept as same type - xsd:string or user defined */
      break;

    case RASQAL_LITERAL_BOOLEAN:
      i = rasqal_xsd_boolean_value_from_string(l->string);
      /* Free passed in string if it is not our static objects */
      if(l->string != rasqal_xsd_boolean_true &&
         l->string != rasqal_xsd_boolean_false)
        RASQAL_FREE(char*, l->string);
      /* and replace with a static string */
      l->string = i ? rasqal_xsd_boolean_true : rasqal_xsd_boolean_false;
      l->string_len = i ? RASQAL_XSD_BOOLEAN_TRUE_LEN : RASQAL_XSD_BOOLEAN_FALSE_LEN;
      
      l->value.integer = i;
      break;

  case RASQAL_LITERAL_STRING:
    /* No change - kept as a string */
    break;

  case RASQAL_LITERAL_DATE:
    if(1) {
      size_t slen = 0;

      if(l->value.date)
        rasqal_free_xsd_date(l->value.date);

      l->value.date = rasqal_new_xsd_date(l->world, RASQAL_GOOD_CAST(const char*, l->string));
      if(!l->value.date) {
        RASQAL_FREE(char*, l->string);
        return 1;
      }
      RASQAL_FREE(char*, l->string);
      l->string = RASQAL_GOOD_CAST(unsigned char*, rasqal_xsd_date_to_counted_string(l->value.date, &slen));
      l->string_len = RASQAL_BAD_CAST(unsigned int, slen);
      if(!l->string)
        return 1;
    }
    break;

  case RASQAL_LITERAL_DATETIME:
    if(1) {
      size_t slen = 0;

      if(l->value.datetime)
        rasqal_free_xsd_datetime(l->value.datetime);

      l->value.datetime = rasqal_new_xsd_datetime(l->world,
                                                  RASQAL_GOOD_CAST(const char*, l->string));
      if(!l->value.datetime) {
        RASQAL_FREE(char*, l->string);
        return 1;
      }
      RASQAL_FREE(char*, l->string);
      l->string = RASQAL_GOOD_CAST(unsigned char*, rasqal_xsd_datetime_to_counted_string(l->value.datetime, &slen));
      l->string_len = RASQAL_BAD_CAST(unsigned int, slen);
      if(!l->string)
        return 1;
    }
    break;

  case RASQAL_LITERAL_UNKNOWN:
  case RASQAL_LITERAL_BLANK:
  case RASQAL_LITERAL_URI:
  case RASQAL_LITERAL_PATTERN:
  case RASQAL_LITERAL_QNAME:
  case RASQAL_LITERAL_VARIABLE:
    RASQAL_FATAL2("Unexpected native type %u", type);
    break;
    
  default:
    RASQAL_FATAL2("Unknown native type %u", type);
  }

  return 0;
}



/*
 * rasqal_literal_string_to_native:
 * @l: #rasqal_literal to operate on inline
 * @flags: flags for literal checking.  1 to canonicalize string
 *
 * INTERNAL - Upgrade a datatyped literal string to an internal typed literal
 *
 * At present this promotes datatyped literals
 * xsd:integer to RASQAL_LITERAL_INTEGER
 * xsd:double to RASQAL_LITERAL_DOUBLE
 * xsd:float to RASQAL_LITERAL_FLOAT
 * xsd:boolean to RASQAL_LITERAL_BOOLEAN
 * xsd:decimal to RASQAL_LITERAL_DECIMAL
 * xsd:dateTime to RASQAL_LITERAL_DATETIME
 * xsd:date to RASQAL_LITERAL_DATE
 *
 * Return value: non-0 on failure
 **/
int
rasqal_literal_string_to_native(rasqal_literal *l, int flags)
{
  rasqal_literal_type native_type = RASQAL_LITERAL_UNKNOWN;
  int rc = 0;
  int canonicalize = (flags & 1 );
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l, rasqal_literal, 1);

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

  rc = rasqal_literal_set_typed_value(l, native_type,
                                      NULL /* existing string */,
                                      canonicalize);

  if(!rasqal_xsd_datatype_check(native_type, l->string, 1))
    return 0;
  
  return rc;
}


/*
 * rasqal_new_string_literal_common:
 * @world: rasqal world object
 * @string: UTF-8 string lexical form
 * @language: RDF language (xml:lang) (or NULL)
 * @datatype: datatype URI (or NULL for plain literal)
 * @datatype_qname: datatype qname string (or NULL for plain literal)
 * @flags: bitflags - 1 to do native type promotion; 2 to canonicalize string
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
 * rasqal_literal_string_to_native() only if @flags has bit 1 set.
 * If bit 2 is set AS WELL, literals will have their string converted
 * to the canonical format.
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
  rasqal_literal* l;
  int native_type_promotion = (flags & 1);
  int canonicalize = (flags & 2) >> 1;

  l = RASQAL_CALLOC(rasqal_literal*, 1, sizeof(*l));
  if(l) {
    rasqal_literal_type datatype_type = RASQAL_LITERAL_STRING;

    l->valid = 1;
    l->usage = 1;
    l->world = world;

    if(datatype && language) {
      /* RDF typed literal but this is not allowed so delete language */
      RASQAL_FREE(char*, language);
      language = NULL;
    }

    l->type = RASQAL_LITERAL_STRING;
    l->string = string;
    l->string_len = RASQAL_BAD_CAST(unsigned int, strlen(RASQAL_GOOD_CAST(const char*, string)));
    if(language) {
      /* Normalize language to lowercase on construction */
      size_t lang_len = strlen(language);
      unsigned int i;

      l->language = RASQAL_MALLOC(char*, lang_len + 1);
      for(i = 0; i < lang_len; i++) {
        char c = language[i];
        if(isupper(RASQAL_GOOD_CAST(int, c)))
          c = RASQAL_GOOD_CAST(char, tolower(RASQAL_GOOD_CAST(int, c)));
        l->language[i] = c;
      }
      l->language[i] = '\0';
      RASQAL_FREE(char*, language);
    }
    l->datatype = datatype;
    l->flags = datatype_qname;

    if(datatype)
      datatype_type = rasqal_xsd_datatype_uri_to_type(world, datatype);
    l->parent_type = rasqal_xsd_datatype_parent_type(datatype_type);
    
    if(native_type_promotion &&
       rasqal_literal_string_to_native(l, canonicalize)) {
      rasqal_free_literal(l);
      l = NULL;
    }
  } else {
    if(language)
      RASQAL_FREE(char*, language);
    if(datatype)
      raptor_free_uri(datatype);
    if(datatype_qname)
      RASQAL_FREE(char*, datatype_qname);
    RASQAL_FREE(char*, string);
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
 * it may be converted to a different literal type.
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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(string, char*, NULL);

  return rasqal_new_string_literal_common(world, string, language, datatype, 
                                          datatype_qname, 1);
}


/*
 * rasqal_new_string_literal_node:
 * @world: rasqal world object
 * @string: UTF-8 string lexical form
 * @language: RDF language (xml:lang) (or NULL)
 * @datatype: datatype URI (or NULL for plain literal)
 *
 * INTERNAL Constructor - Create a new Rasqal literal with promotion and canonicalization
 * 
 * All parameters are input parameters and if present are stored in
 * the literal, not copied. They are freed also on failure.
 * 
 * Return value: New #rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_string_literal_node(rasqal_world* world, const unsigned char *string,
                               const char *language, raptor_uri *datatype)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(string, char*, NULL);

  return rasqal_new_string_literal_common(world, string, language, datatype, 
                                          NULL, 1 | 2);
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
  rasqal_literal* l;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(string, char*, NULL);

  l = RASQAL_CALLOC(rasqal_literal*, 1, sizeof(*l));
  if(l) {
    l->valid = 1;
    l->usage = 1;
    l->world = world;
    l->type = type;
    l->string = string;
    l->string_len = RASQAL_BAD_CAST(unsigned int, strlen(RASQAL_GOOD_CAST(const char*, string)));
  } else {
    RASQAL_FREE(char*, string);
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
  rasqal_literal* l;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);

  l = RASQAL_CALLOC(rasqal_literal*, 1, sizeof(*l));
  if(l) {
    l->valid = 1;
    l->usage = 1;
    l->world = world;
    l->type = RASQAL_LITERAL_BOOLEAN;
    l->value.integer = value;
    l->string = value ? rasqal_xsd_boolean_true : rasqal_xsd_boolean_false;
    l->string_len = value ? RASQAL_XSD_BOOLEAN_TRUE_LEN : RASQAL_XSD_BOOLEAN_FALSE_LEN;
    dt_uri = rasqal_xsd_datatype_type_to_uri(world, l->type);
    if(!dt_uri) {
      rasqal_free_literal(l);
      return NULL;
    }
    l->datatype = raptor_uri_copy(dt_uri);
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
  rasqal_literal* l;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(variable, rasqal_variable, NULL);

  l = RASQAL_CALLOC(rasqal_literal*, 1, sizeof(*l));
  if(l) {
    l->valid = 1;
    l->usage = 1;
    l->world = world;
    l->type = RASQAL_LITERAL_VARIABLE;
    l->value.variable = variable;
  } else
    rasqal_free_variable(variable);

  return l;
}


/**
 * rasqal_new_literal_from_literal:
 * @l: #rasqal_literal object to copy or NULL
 *
 * Copy Constructor - create a new rasqal_literal object from an existing rasqal_literal object.
 *
 * Return value: a new #rasqal_literal object or NULL if @l was NULL.
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
  if(!l)
    return;
  
  if(--l->usage)
    return;
  
  switch(l->type) {
    case RASQAL_LITERAL_URI:
      if(l->value.uri)
        raptor_free_uri(l->value.uri);
      break;
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_INTEGER: 
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_UDT:
    case RASQAL_LITERAL_INTEGER_SUBTYPE:
      if(l->string)
        RASQAL_FREE(char*, l->string);
      if(l->language)
        RASQAL_FREE(char*, l->language);
      if(l->datatype)
        raptor_free_uri(l->datatype);
      if(l->type == RASQAL_LITERAL_STRING ||
         l->type == RASQAL_LITERAL_PATTERN) {
        if(l->flags)
          RASQAL_FREE(char*, l->flags);
      }
      break;

    case RASQAL_LITERAL_DATE:
      if(l->string)
        RASQAL_FREE(char*, l->string);
      if(l->datatype)
        raptor_free_uri(l->datatype);
      if(l->value.date)
        rasqal_free_xsd_date(l->value.date);
      break;

    case RASQAL_LITERAL_DATETIME:
      if(l->string)
        RASQAL_FREE(char*, l->string);
      if(l->datatype)
        raptor_free_uri(l->datatype);
      if(l->value.datetime)
        rasqal_free_xsd_datetime(l->value.datetime);
      break;

    case RASQAL_LITERAL_DECIMAL:
      /* l->string is owned by l->value.decimal - do not free it */
      if(l->datatype)
        raptor_free_uri(l->datatype);
      if(l->value.decimal)
        rasqal_free_xsd_decimal(l->value.decimal);
      break;

    case RASQAL_LITERAL_BOOLEAN:
       /* static l->string for boolean, does not need freeing */
      if(l->datatype)
        raptor_free_uri(l->datatype);
      break;

    case RASQAL_LITERAL_VARIABLE:
      if(l->value.variable)
        rasqal_free_variable(l->value.variable);
      break;

    case RASQAL_LITERAL_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown literal type %u", l->type);
  }
  RASQAL_FREE(rasqal_literal, l);
}


/* 
 * The order here must match that of rasqal_literal_type
 * in rasqal.h and is significant as rasqal_literal_compare
 * uses it for type comparisons with the RASQAL_COMPARE_XQUERY
 * flag.
 */
static const char* const rasqal_literal_type_labels[RASQAL_LITERAL_LAST + 1]={
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
  "variable",
  "<integer subtype>",
  "date"
};


/**
 * rasqal_literal_type_label:
 * @type: the #rasqal_literal_type object
 * 
 * Get a label for the rasqal literal type
 *
 * Return value: the label (shared string) or NULL if type is out of
 * range or unknown
 **/
const char*
rasqal_literal_type_label(rasqal_literal_type type)
{
  if(type > RASQAL_LITERAL_LAST)
    type = RASQAL_LITERAL_UNKNOWN;

  return rasqal_literal_type_labels[RASQAL_GOOD_CAST(int, type)];
}


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
  if(!l) {
    raptor_iostream_counted_string_write("null", 4, iostr);
    return;
  }
  
  raptor_iostream_string_write(rasqal_literal_type_label(l->type), iostr);
}


/**
 * rasqal_literal_print_type:
 * @l: the #rasqal_literal object
 * @fh: the FILE* handle to print to
 * 
 * Print a string form for a rasqal literal type.
 *
 **/
void
rasqal_literal_print_type(rasqal_literal* l, FILE* fh)
{
  raptor_iostream *iostr;

  if(!l) {
    fputs("null", fh);
    return;
  }

  iostr = raptor_new_iostream_to_file_handle(l->world->raptor_world_ptr, fh);
  if(!iostr)
    return;

  rasqal_literal_write_type(l, iostr);

  raptor_free_iostream(iostr);
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
    raptor_iostream_counted_string_write("null", 4, iostr);
    return;
  }

  if(!l->valid)
    raptor_iostream_counted_string_write("INV:", 4, iostr);

  if(l->type != RASQAL_LITERAL_VARIABLE)
    rasqal_literal_write_type(l, iostr);

  switch(l->type) {
    case RASQAL_LITERAL_URI:
      raptor_iostream_write_byte('<', iostr);
      str = raptor_uri_as_counted_string(l->value.uri, &len);
      raptor_string_ntriples_write(str, len, '>', iostr);
      raptor_iostream_write_byte('>', iostr);
      break;
    case RASQAL_LITERAL_BLANK:
      raptor_iostream_write_byte(' ', iostr);
      raptor_iostream_counted_string_write(l->string, l->string_len, iostr);
      break;
    case RASQAL_LITERAL_PATTERN:
      raptor_iostream_write_byte('/', iostr);
      raptor_iostream_counted_string_write(l->string, l->string_len, iostr);
      raptor_iostream_write_byte('/', iostr);
      if(l->flags)
        raptor_iostream_string_write(l->flags, iostr);
      break;
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_UDT:
      raptor_iostream_counted_string_write("(\"", 2, iostr);
      raptor_string_ntriples_write(l->string, l->string_len, '"', iostr);
      raptor_iostream_write_byte('"', iostr);
      if(l->language) {
        raptor_iostream_write_byte('@', iostr);
        raptor_iostream_string_write(l->language, iostr);
      }
      if(l->datatype) {
        raptor_iostream_counted_string_write("^^<", 3, iostr);
        str = raptor_uri_as_counted_string(l->datatype, &len);
        raptor_string_ntriples_write(str, len, '>', iostr);
        raptor_iostream_write_byte('>', iostr);
      }
      raptor_iostream_write_byte(')', iostr);
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
    case RASQAL_LITERAL_DATE:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_INTEGER_SUBTYPE:
      raptor_iostream_write_byte('(', iostr);
      raptor_iostream_counted_string_write(l->string, l->string_len, iostr);
      raptor_iostream_write_byte(')', iostr);
      break;

    case RASQAL_LITERAL_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown literal type %u", l->type);
  }
}



/**
 * rasqal_literal_print:
 * @l: the #rasqal_literal object
 * @fh: the FILE handle to print to
 *
 * Print a Rasqal literal in a debug format.
 * 
 * The print debug format may change in any release.
 *
 * Return value: non-0 on failure
 **/
int
rasqal_literal_print(rasqal_literal* l, FILE* fh)
{
  raptor_iostream *iostr;

  if(!l) {
    fputs("NULL", fh);
    return 0;
  }

  iostr = raptor_new_iostream_to_file_handle(l->world->raptor_world_ptr, fh);
  rasqal_literal_write(l, iostr);
  raptor_free_iostream(iostr);

  return 0;
}



/*
 * rasqal_literal_as_boolean:
 * @l: #rasqal_literal object
 * @error_p: pointer to error flag
 * 
 * INTERNAL - Return a literal as a boolean value
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
rasqal_literal_as_boolean(rasqal_literal* l, int *error_p)
{
  if(!l) {
    /* type error */
    if(error_p)
      *error_p = 1;
    return 0;
  }
  
  switch(l->type) {
    case RASQAL_LITERAL_STRING:
      if(l->datatype) {
        if(raptor_uri_equals(l->datatype,
                             rasqal_xsd_datatype_type_to_uri(l->world, RASQAL_LITERAL_STRING))) {
          /* typed literal with xsd:string datatype -> true if non-empty */
          return l->string && *l->string;
        }
        /* typed literal with other datatype -> type error */
        if(error_p)
          *error_p = 1;
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
    case RASQAL_LITERAL_DATE:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_UDT:
      if(error_p)
        *error_p = 1;
      return 0;

    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_INTEGER_SUBTYPE:
      return l->value.integer != 0;

    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_FLOAT: 
      if(isnan(l->value.floating))
        return 0;
        
      return fabs(l->value.floating) > RASQAL_DOUBLE_EPSILON;

    case RASQAL_LITERAL_VARIABLE:
      return rasqal_literal_as_boolean(l->value.variable->value, error_p);

    case RASQAL_LITERAL_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown literal type %u", l->type);
      return 0; /* keep some compilers happy */
  }
}


/*
 * rasqal_literal_as_integer
 * @l: #rasqal_literal object
 * @error_p: pointer to error flag
 * 
 * INTERNAL - Return a literal as an integer value
 *
 * Integers, booleans, double and float literals natural are turned into
 * integers. If string values are the lexical form of an integer, that is
 * returned.  Otherwise the error flag is set.
 * 
 * Return value: integer value
 **/
int
rasqal_literal_as_integer(rasqal_literal* l, int *error_p)
{
  if(!l) {
    /* type error */
    if(error_p)
      *error_p = 1;
    return 0;
  }
  
  switch(l->type) {
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_INTEGER_SUBTYPE:
      return l->value.integer;

    case RASQAL_LITERAL_BOOLEAN:
      return l->value.integer != 0;

    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_FLOAT:
      return RASQAL_FLOATING_AS_INT(l->value.floating);

    case RASQAL_LITERAL_DECIMAL:
      {
        int error = 0;
        
        long lvalue = rasqal_xsd_decimal_get_long(l->value.decimal, &error);
        if(lvalue < INT_MIN || lvalue > INT_MAX)
          error = 1;

        if(error) {
          if(error_p)
            *error_p = 1;
          return 0;
        }
        
        return RASQAL_GOOD_CAST(int, lvalue);
      }

    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_XSD_STRING:
      {
        char *eptr;
        double  d;
        long long_i;

        eptr = NULL;
        errno = 0;
        long_i = strtol(RASQAL_GOOD_CAST(const char*, l->string), &eptr, 10);
        /* If formatted correctly and no under or overflow */
        if(RASQAL_GOOD_CAST(unsigned char*, eptr) != l->string && *eptr=='\0' &&
           errno != ERANGE)
          /* FIXME may lose precision or be out of range from long to int */
          return RASQAL_BAD_CAST(int, long_i);

        eptr = NULL;
        d = strtod(RASQAL_GOOD_CAST(const char*, l->string), &eptr);
        if(RASQAL_GOOD_CAST(unsigned char*, eptr) != l->string && *eptr=='\0')
          return RASQAL_FLOATING_AS_INT(d);
      }
      if(error_p)
        *error_p = 1;
      return 0;

    case RASQAL_LITERAL_VARIABLE:
      return rasqal_literal_as_integer(l->value.variable->value, error_p);

    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_DATE:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_UDT:
      if(error_p)
        *error_p = 1;
      return 0;
      
    case RASQAL_LITERAL_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown literal type %u", l->type);
      return 0; /* keep some compilers happy */
  }
}


/*
 * rasqal_literal_as_double:
 * @l: #rasqal_literal object
 * @error_p: pointer to error flag
 * 
 * INTERNAL - Return a literal as a double precision floating value
 *
 * Integers, booleans, double and float literals natural are turned into
 * integers. If string values are the lexical form of an floating, that is
 * returned.  Otherwise the error flag is set.
 * 
 * Return value: double precision floating value
 **/
double
rasqal_literal_as_double(rasqal_literal* l, int *error_p)
{
  if(!l) {
    /* type error */
    *error_p = 1;
    return 0.0;
  }
  
  switch(l->type) {
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_INTEGER_SUBTYPE:
      return (double)l->value.integer;

    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_FLOAT:
      return l->value.floating;

    case RASQAL_LITERAL_DECIMAL:
      return rasqal_xsd_decimal_get_double(l->value.decimal);

    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_XSD_STRING:
      {
        char *eptr = NULL;
        double  d = strtod(RASQAL_GOOD_CAST(const char*, l->string), &eptr);
        if(RASQAL_GOOD_CAST(unsigned char*, eptr) != l->string && *eptr == '\0')
          return d;
      }
      if(error_p)
        *error_p = 1;
      return 0.0;

    case RASQAL_LITERAL_VARIABLE:
      return rasqal_literal_as_double(l->value.variable->value, error_p);

    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_DATE:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_UDT:
      if(error_p)
        *error_p = 1;
      return 0.0;
      
    case RASQAL_LITERAL_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown literal type %u", l->type);
      return 0.0; /* keep some compilers happy */
  }
}


/*
 * rasqal_literal_as_uri:
 * @l: #rasqal_literal object
 *
 * INTERNAL - Return a literal as a raptor_uri*
 * 
 * Return value: shared raptor_uri* value or NULL on failure
 **/
raptor_uri*
rasqal_literal_as_uri(rasqal_literal* l)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l, rasqal_literal, NULL);
  
  if(l->type == RASQAL_LITERAL_URI)
    return l->value.uri;

  if(l->type == RASQAL_LITERAL_VARIABLE && l->value.variable->value)
    return rasqal_literal_as_uri(l->value.variable->value);

  return NULL;
}


/**
 * rasqal_literal_as_counted_string:
 * @l: #rasqal_literal object
 * @len_p: pointer to store length of string (or NULL)
 * @flags: comparison flags
 * @error_p: pointer to error
 *
 * Return a counted string format of a literal according to flags.
 * 
 * flag bits affects conversion:
 *   RASQAL_COMPARE_XQUERY: use XQuery conversion rules
 * 
 * If @error is not NULL, *error is set to non-0 on error
 *
 * Return value: pointer to a shared string format of the literal.
 **/
const unsigned char*
rasqal_literal_as_counted_string(rasqal_literal* l, size_t *len_p,
                                 int flags, int *error_p)
{
  if(!l) {
    /* type error */
    if(error_p)
      *error_p = 1;
    return NULL;
  }
  
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
    case RASQAL_LITERAL_DATE:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_UDT:
    case RASQAL_LITERAL_INTEGER_SUBTYPE:
      if(len_p)
        *len_p = l->string_len;

      return l->string;

    case RASQAL_LITERAL_URI:
      if(flags & RASQAL_COMPARE_XQUERY) {
        if(error_p)
          *error_p = 1;
        return NULL;
      }
      return raptor_uri_as_counted_string(l->value.uri, len_p);

    case RASQAL_LITERAL_VARIABLE:
      return rasqal_literal_as_counted_string(l->value.variable->value, len_p,
                                              flags, error_p);

    case RASQAL_LITERAL_UNKNOWN:
    default:
      RASQAL_FATAL2("Unknown literal type %u", l->type);
  }

  return NULL;
}


/**
 * rasqal_literal_as_string_flags:
 * @l: #rasqal_literal object
 * @flags: comparison flags
 * @error_p: pointer to error
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
rasqal_literal_as_string_flags(rasqal_literal* l, int flags, int *error_p)
{
  if(!l) {
    /* type error */
    *error_p = 1;
    return NULL;
  }
  
  return rasqal_literal_as_counted_string(l, NULL, flags, error_p);
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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l, rasqal_literal, NULL);

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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l, rasqal_literal, NULL);

  return (l->type == RASQAL_LITERAL_VARIABLE) ? l->value.variable : NULL;
}


/*
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
  rasqal_literal_type type1 = l1->type;
  rasqal_literal_type type2 = l2->type;
  rasqal_literal_type promotion_type;
  rasqal_literal_type result_type = RASQAL_LITERAL_UNKNOWN;

  /* B1 1.b http://www.w3.org/TR/xpath20/#dt-type-promotion */
  if(type1 == RASQAL_LITERAL_DECIMAL &&
     (type2 == RASQAL_LITERAL_FLOAT || type2 == RASQAL_LITERAL_DOUBLE)) {
    result_type = type2;
    goto done;
  } else if(type2 == RASQAL_LITERAL_DECIMAL &&
     (type1 == RASQAL_LITERAL_FLOAT || type1 == RASQAL_LITERAL_DOUBLE)) {
    result_type = type1;
    goto done;
  }

  for(promotion_type = RASQAL_LITERAL_FIRST_XSD;
      promotion_type <= RASQAL_LITERAL_LAST_XSD;
      promotion_type = (rasqal_literal_type)(RASQAL_GOOD_CAST(unsigned int, promotion_type) + 1)) {
    rasqal_literal_type parent_type1 = rasqal_xsd_datatype_parent_type(type1);
    rasqal_literal_type parent_type2 = rasqal_xsd_datatype_parent_type(type2);
    
    /* Finished */
    if(type1 == type2) {
      result_type = type1;
      break;
    }

    if(parent_type1 == type2) {
      result_type = type2;
      break;
    }

    if(parent_type2 == type1) {
      result_type = type1;
      break;
    }
  
    if(parent_type1 == promotion_type)
      type1 = promotion_type;
    if(parent_type2 == promotion_type)
      type2 = promotion_type;
  }

  done:
  RASQAL_DEBUG4("literal 1: type %s   literal 2: type %s  promoting to %s\n",
                rasqal_literal_type_label(type1),
                rasqal_literal_type_label(type2),
                rasqal_literal_type_label(result_type));

  return result_type;
}


/**
 * rasqal_literal_get_rdf_term_type:
 * @l: literal
 *
 * Get the RDF term type of a literal
 *
 * An RDF term can be one of three choices:
 *   1. URI:  RASQAL_LITERAL_URI
 *   2. literal: RASQAL_LITERAL_STRING
 *   3. blank node: RASQAL_LITERAL_BLANK
 *
 * Other non RDF-term cases include: NULL pointer, invalid literal,
 * unknown type, a variable or other special cases (such as XML QName
 * or Regex pattern) which all turn into RASQAL_LITERAL_UNKNOWN
 *
 * Return value: type or RASQAL_LITERAL_UNKNOWN if cannot be an RDF term
 */
rasqal_literal_type
rasqal_literal_get_rdf_term_type(rasqal_literal* l)
{
  rasqal_literal_type type;
  
  if(!l)
    return RASQAL_LITERAL_UNKNOWN;
  
  type = l->type;
  
  /* squash literal datatypes into one type: RDF Literal */
  if((type >= RASQAL_LITERAL_FIRST_XSD && type <= RASQAL_LITERAL_LAST_XSD) ||
     type == RASQAL_LITERAL_DATE || type == RASQAL_LITERAL_INTEGER_SUBTYPE)
    type = RASQAL_LITERAL_STRING;

  if(type == RASQAL_LITERAL_UDT)
    type = RASQAL_LITERAL_STRING;
  
  if(type != RASQAL_LITERAL_URI &&
     type != RASQAL_LITERAL_STRING &&
     type != RASQAL_LITERAL_BLANK)
    type = RASQAL_LITERAL_UNKNOWN;

  return type;
}

/**
 * rasqal_literal_get_type:
 * @l: literal
 *
 * Get the type of a literal
 *
 * Return value: the rasqal literal type or RASQAL_LITERAL_UNKNOWN if l is NULL
 */
rasqal_literal_type
rasqal_literal_get_type(rasqal_literal* l)
{
  rasqal_literal_type type;
  
  if(!l)
    return RASQAL_LITERAL_UNKNOWN;
  
  type = l->type;
  
  return type;
}

/**
 * rasqal_literal_get_language:
 * @l: literal
 *
 * Get the language of a literal (if set)
 *
 * Return value: the literal language or NULL
 */
char*
rasqal_literal_get_language(rasqal_literal* l)
{
  if(!l)
    return NULL;
  
  return l->language;
}




/*
 * rasqal_new_literal_from_promotion:
 * @lit: existing literal
 * @type: type to promote to
 * @flags: 0 (flag #RASQAL_COMPARE_URI is unused: was RDQL)
 *
 * INTERNAL - Make a new literal from a type promotion
 *
 * New literal or NULL on failure
*/
static rasqal_literal*
rasqal_new_literal_from_promotion(rasqal_literal* lit,
                                  rasqal_literal_type type,
                                  int flags)
{
  rasqal_literal* new_lit = NULL;
  int errori = 0;
  double d;
  int i;
  unsigned char *new_s = NULL;
  unsigned char* s;
  size_t len = 0;
  rasqal_xsd_decimal* dec = NULL;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(lit, rasqal_literal, NULL);

  if(lit->type == type)
    return rasqal_new_literal_from_literal(lit);

  RASQAL_DEBUG3("promoting literal type %s to type %s\n", 
                rasqal_literal_type_label(lit->type),
                rasqal_literal_type_label(type));

  if(lit->type == RASQAL_LITERAL_DATE && type == RASQAL_LITERAL_DATETIME) {
    rasqal_xsd_datetime* dt;

    dt = rasqal_new_xsd_datetime_from_xsd_date(lit->world, lit->value.date);

    /* Promotion for comparison ensures a timezone is present.
     *
     * " If either operand to a comparison function on date or time
     * values does not have an (explicit) timezone then, for the
     * purpose of the operation, an implicit timezone, provided by the
     * dynamic context Section C.2 Dynamic Context ComponentsXP, is
     * assumed to be present as part of the value."
     * -- XQuery & XPath F&O
     *    Section 10.4 Comparison Operators on Duration, Date and Time Values
     * http://www.w3.org/TR/xpath-functions/#comp.duration.datetime
     */
    if(dt->have_tz == 'N') {
      dt->have_tz = 'Z';
      dt->timezone_minutes = 0;
    }
    return rasqal_new_datetime_literal_from_datetime(lit->world, dt);
  }

  /* May not promote to non-numerics */
  if(!rasqal_xsd_datatype_is_numeric(type)) {
    RASQAL_DEBUG2("NOT promoting to non-numeric type %s\n", 
                  rasqal_literal_type_label(lit->type));

    if(type == RASQAL_LITERAL_STRING || type ==  RASQAL_LITERAL_UDT) {
      s = RASQAL_GOOD_CAST(unsigned char*, rasqal_literal_as_counted_string(lit, &len, 0, NULL));
      new_s = RASQAL_MALLOC(unsigned char*, len + 1);
      if(new_s) {
        raptor_uri* dt_uri = NULL;
        memcpy(new_s, s, len + 1);
        if(lit->datatype) {
          dt_uri = raptor_uri_copy(lit->datatype);
        }
        return rasqal_new_string_literal_node(lit->world, new_s, NULL, dt_uri);
      } else
        return NULL;
    }
    return NULL;
  }
    
  switch(type) {
    case RASQAL_LITERAL_DECIMAL:
      dec = rasqal_new_xsd_decimal(lit->world);
      if(dec) {
        d = rasqal_literal_as_double(lit, &errori);
        if(errori) {
          rasqal_free_xsd_decimal(dec);
          new_lit = NULL;
        } else {
          rasqal_xsd_decimal_set_double(dec, d);
          new_lit = rasqal_new_decimal_literal_from_decimal(lit->world,
                                                            NULL, dec);
        }
      }
      break;
      

    case RASQAL_LITERAL_DOUBLE:
      d = rasqal_literal_as_double(lit, &errori);
      if(errori)
        new_lit = NULL;
      else
        new_lit = rasqal_new_double_literal(lit->world, d);
      break;
      

    case RASQAL_LITERAL_FLOAT:
      d = rasqal_literal_as_double(lit, &errori);
      if(errori)
        new_lit = NULL;
      else if(d < FLT_MIN || d > FLT_MAX)
        /* Cannot be stored in a float - fail */
        new_lit = NULL;
      else
        new_lit = rasqal_new_floating_literal(lit->world, RASQAL_LITERAL_FLOAT,
                                              d);
      break;
      

    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_INTEGER_SUBTYPE:
      i = rasqal_literal_as_integer(lit, &errori);
      /* failure always means no match */
      if(errori)
        new_lit = NULL;
      else
        new_lit = rasqal_new_integer_literal(lit->world, type, i);
      break;
    
    case RASQAL_LITERAL_BOOLEAN:
      if(flags & RASQAL_COMPARE_URI)
        i = rasqal_xsd_boolean_value_from_string(lit->string);
      else
        i = rasqal_literal_as_boolean(lit, &errori);
      /* failure always means no match */
      if(errori)
        new_lit = NULL;
      else
        new_lit = rasqal_new_integer_literal(lit->world, type, i);
      break;
    
    case RASQAL_LITERAL_STRING:
      s = RASQAL_GOOD_CAST(unsigned char*, rasqal_literal_as_counted_string(lit, &len, 0, NULL));
      new_s = RASQAL_MALLOC(unsigned char*, len + 1);
      if(new_s) {
        memcpy(new_s, s, len + 1);
        new_lit = rasqal_new_string_literal(lit->world, new_s, NULL, NULL, NULL);
      }
      break;

    case RASQAL_LITERAL_XSD_STRING:
      s = RASQAL_GOOD_CAST(unsigned char*, rasqal_literal_as_counted_string(lit, &len, 0, NULL));
      new_s = RASQAL_MALLOC(unsigned char*, len + 1);
      if(new_s) {
        raptor_uri* dt_uri;
        memcpy(new_s, s, len + 1);
        dt_uri = rasqal_xsd_datatype_type_to_uri(lit->world, lit->type);
        dt_uri = raptor_uri_copy(dt_uri);
        new_lit = rasqal_new_string_literal(lit->world, new_s, NULL, dt_uri,
                                            NULL);
      }
      break;

    case RASQAL_LITERAL_UNKNOWN:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_DATE:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_VARIABLE:
    case RASQAL_LITERAL_UDT:
    default:
      errori = 1;
      new_lit = NULL;
  }

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  if(new_lit)
    RASQAL_DEBUG4("promoted literal type %s to type %s, with value '%s'\n", 
                  rasqal_literal_type_label(lit->type),
                  rasqal_literal_type_label(new_lit->type),
                  rasqal_literal_as_string(new_lit));
  else
    RASQAL_DEBUG3("failed to promote literal type %s to type %s\n", 
                  rasqal_literal_type_label(lit->type),
                  rasqal_literal_type_label(type));
#endif

  return new_lit;
}  


/*
 * rasqal_literal_string_languages_compare
 * @l1: #rasqal_literal first literal
 * @l2: #rasqal_literal second literal
 *
 * INTERNAL - Compare two string literals languages
 *
 * Return value: non-0 if equal
 */
int
rasqal_literal_string_languages_compare(rasqal_literal* l1, rasqal_literal* l2)
{
  int rc = 0;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l1, rasqal_literal, 0);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l2, rasqal_literal, 0);

  if(l1->language && l2->language)
    /* both have a language */
    rc = rasqal_strcasecmp(RASQAL_GOOD_CAST(const char*, l1->language),
                           RASQAL_GOOD_CAST(const char*, l2->language));
  else if(l1->language || l2->language)
    /* only one has a language; the language-less one is earlier */
    rc = (!l1->language ? -1 : 1);

  return rc;
}


/*
 * rasqal_literal_string_datatypes_compare:
 * @l1: first string literal
 * @l2: first string literal
 *
 * INTERNAL - Compare the datatypes of two string RDF literals
 *
 * Return value: <1, 0, >0
 */
int
rasqal_literal_string_datatypes_compare(rasqal_literal* l1, rasqal_literal* l2)
{
  int rc = 0;

  if(l1->datatype && l2->datatype) {
    /* both have a datatype */
    rc = raptor_uri_compare(l1->datatype, l2->datatype);
  } else if(l1->datatype || l2->datatype)
    /* only one has a datatype; the datatype-less one is earlier */
    rc = (!l1->datatype ? -1 : 1);

  return rc;
}


/*
 * rasqal_literal_string_compare:
 * @l1: first string literal
 * @l2: first string literal
 * @flags: string compare flags (flag #RASQAL_COMPARE_NOCASE is unused; was RDQL)
 *
 * INTERNAL - Compare two string RDF literals.  Bother are the same
 * type and either #RASQAL_LITERAL_STRING or #RASQAL_LITERAL_UDT
 * however their datatype URIs are the non-promoted types and may differ.
 *
 * Uses raptor_term_compare() logic for the #RAPTOR_TERM_TYPE_LITERAL
 * case, except adding case independent compare for RDQL.
 *
 * Return value: <1, 0, >0
 */
static int
rasqal_literal_string_compare(rasqal_literal* l1, rasqal_literal* l2,
                              int flags)
{
  int rc;
  
  if(flags & RASQAL_COMPARE_NOCASE)
    rc = rasqal_strcasecmp(RASQAL_GOOD_CAST(const char*, l1->string),
                           RASQAL_GOOD_CAST(const char*, l2->string));
  else
    rc = strcmp(RASQAL_GOOD_CAST(const char*, l1->string),
                RASQAL_GOOD_CAST(const char*, l2->string));
  if(rc)
    return rc;

  rc = rasqal_literal_string_languages_compare(l1, l2);
  if(rc)
    return rc;
      
  return rasqal_literal_string_datatypes_compare(l1, l2);
}


/*
 * rasqal_literal_rdql_promote_calculate:
 * @l1: first literal
 * @l2: second literal
 *
 * INTERNAL - Handle RDQL type promotion rules
 *
 * Return value: type to promote to or RASQAL_LITERAL_UNKNOWN if not possible.
 */
static rasqal_literal_type
rasqal_literal_rdql_promote_calculate(rasqal_literal* l1, rasqal_literal* l2)
{    
  int seen_string = 0;
  int seen_int = 0;
  int seen_double = 0;
  int seen_boolean = 0;
  int i;
  rasqal_literal *lits[2];
  rasqal_literal_type type = RASQAL_LITERAL_UNKNOWN;

  lits[0] = l1;
  lits[1] = l2;

  for(i = 0; i < 2; i++) {
    switch(lits[i]->type) {
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_DECIMAL:
      break;
      
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_DATE:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_UDT:
      seen_string++;
      break;
      
    case RASQAL_LITERAL_BOOLEAN:
      seen_boolean = 1;
      break;
      
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_INTEGER_SUBTYPE:
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
      RASQAL_FATAL2("Unknown literal type %u", lits[i]->type);
    }
  }

  
  if(lits[0]->type != lits[1]->type) {
    type = seen_string ? RASQAL_LITERAL_STRING : RASQAL_LITERAL_INTEGER;
    if((seen_int & seen_double) || (seen_int & seen_string))
      type = RASQAL_LITERAL_DOUBLE;
    if(seen_boolean & seen_string)
      type = RASQAL_LITERAL_BOOLEAN;
  } else
    type = lits[0]->type;
  
  return type;
}



/**
 * rasqal_literal_compare:
 * @l1: #rasqal_literal first literal
 * @l2: #rasqal_literal second literal
 * @flags: comparison flags
 * @error_p: pointer to error
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
                       int *error_p)
{
  rasqal_literal *lits[2];
  rasqal_literal* new_lits[2]; /* after promotions */
  rasqal_literal_type type; /* target promotion type */
  int i;
  int result = 0;
  double d = 0;
  int promotion = 0;
  
  if(error_p)
    *error_p = 0;

  if(!l1 || !l2) {
    if(error_p)
      *error_p = 1;
    return 0;
  }

  lits[0] = rasqal_literal_value(l1);
  lits[1] = rasqal_literal_value(l2);

  /* null literals */
  if(!lits[0] || !lits[1]) {
    /* if either is not NULL, the comparison fails */
    if(lits[0] || lits[1]) {
      if(error_p)
        *error_p = 1;
    }
    return 0;
  }

  new_lits[0] = NULL;
  new_lits[1] = NULL;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG3("literal 0 type %s.  literal 1 type %s\n", 
                rasqal_literal_type_label(lits[0]->type),
                rasqal_literal_type_label(lits[1]->type));
#endif

  if(flags & RASQAL_COMPARE_RDF) {
    /* no promotion but compare as RDF terms; like rasqal_literal_as_node() */
    rasqal_literal_type type0 = rasqal_literal_get_rdf_term_type(lits[0]);
    rasqal_literal_type type1 = rasqal_literal_get_rdf_term_type(lits[1]);
    int type_diff;
    
    if(type0 == RASQAL_LITERAL_UNKNOWN || type1 == RASQAL_LITERAL_UNKNOWN)
      return 1;

    type_diff = RASQAL_GOOD_CAST(int, type0) - RASQAL_GOOD_CAST(int, type1);
    if(type_diff != 0) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      RASQAL_DEBUG2("RDF term literal returning type difference %d\n",
                    type_diff);
#endif
      return type_diff;
    }
    type = type1;
  } else if(flags & RASQAL_COMPARE_XQUERY) { 
    /* SPARQL / XQuery promotion rules */
    rasqal_literal_type type0 = lits[0]->type;
    rasqal_literal_type type1 = lits[1]->type;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG3("xquery literal compare types %s vs %s\n",
                  rasqal_literal_type_label(type0),
                  rasqal_literal_type_label(type1));
#endif

    /* cannot compare UDTs */
    if(type0 == RASQAL_LITERAL_UDT || type1 == RASQAL_LITERAL_UDT) {
      if(error_p)
        *error_p = 1;
      return 0;
    }

    type = rasqal_literal_promote_numerics(lits[0], lits[1], flags);
    if(type == RASQAL_LITERAL_UNKNOWN) {
      int type_diff;

      /* no promotion but compare as RDF terms; like rasqal_literal_as_node() */
      type0 = rasqal_literal_get_rdf_term_type(lits[0]);
      type1 = rasqal_literal_get_rdf_term_type(lits[1]);
      
      if(type0 == RASQAL_LITERAL_UNKNOWN || type1 == RASQAL_LITERAL_UNKNOWN)
        return 1;

      type_diff = RASQAL_GOOD_CAST(int, type0) - RASQAL_GOOD_CAST(int, type1);
      if(type_diff != 0) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
        RASQAL_DEBUG2("RDF term literal returning type difference %d\n",
                      type_diff);
#endif
        return type_diff;
      }
      if(error_p)
        *error_p = 1;
      return 0;
    }
    promotion = 1;
  } else {
    /* RDQL promotion rules */
    type = rasqal_literal_rdql_promote_calculate(lits[0], lits[1]);
    promotion = 1;
  }

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  if(promotion)
    RASQAL_DEBUG2("promoting to type %s\n", rasqal_literal_type_label(type));
#endif

  /* do promotions */
  for(i = 0; i < 2; i++) {
    if(promotion) {
      new_lits[i] = rasqal_new_literal_from_promotion(lits[i], type, flags);
      if(!new_lits[i]) {
        if(error_p)
          *error_p = 1;
        goto done;
      }
    } else {
      new_lits[i] = lits[i];
    }
  }


  switch(type) {
    case RASQAL_LITERAL_URI:
      if(flags & RASQAL_COMPARE_URI)
        result = raptor_uri_compare(new_lits[0]->value.uri,
                                    new_lits[1]->value.uri);
      else {
        if(error_p)
          *error_p = 1;
        result = 0;
        goto done;
      }
      break;

    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_UDT:
      result = rasqal_literal_string_compare(new_lits[0], new_lits[1],
                                             flags);
      break;
      
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_XSD_STRING:
      if(flags & RASQAL_COMPARE_NOCASE)
        result = rasqal_strcasecmp(RASQAL_GOOD_CAST(const char*, new_lits[0]->string),
                                   RASQAL_GOOD_CAST(const char*, new_lits[1]->string));
      else
        result = strcmp(RASQAL_GOOD_CAST(const char*, new_lits[0]->string),
                        RASQAL_GOOD_CAST(const char*, new_lits[1]->string));
      break;

    case RASQAL_LITERAL_DATE:
      result = rasqal_xsd_date_compare(new_lits[0]->value.date,
                                       new_lits[1]->value.date,
                                       error_p);
      break;

    case RASQAL_LITERAL_DATETIME:
      result = rasqal_xsd_datetime_compare2(new_lits[0]->value.datetime,
                                            new_lits[1]->value.datetime,
                                            error_p);
      break;

    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_INTEGER_SUBTYPE:
      result = new_lits[0]->value.integer - new_lits[1]->value.integer;
      break;

    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_FLOAT:
      d = new_lits[0]->value.floating - new_lits[1]->value.floating;
      result = (d > 0.0) ? 1: (d < 0.0) ? -1 : 0;
      break;
      
    case RASQAL_LITERAL_DECIMAL:
      result = rasqal_xsd_decimal_compare(new_lits[0]->value.decimal,
                                        new_lits[1]->value.decimal);
      break;

    case RASQAL_LITERAL_UNKNOWN:
    case RASQAL_LITERAL_VARIABLE:
    default:
      RASQAL_FATAL2("Literal type %u cannot be compared", type);
      result = 0; /* keep some compilers happy */
  }

  done:
  if(promotion) {
    for(i = 0; i < 2; i++) {
      if(new_lits[i])
        rasqal_free_literal(new_lits[i]);
    }
  }
  
  return result;
}


/*
 * rasqal_literal_is_string:
 * @l1: #rasqal_literal first literal
 *
 * INTERNAL - check literal is a string literal
 *
 * Return value: non-0 if literal is a string
 */
int
rasqal_literal_is_string(rasqal_literal* l1)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l1, rasqal_literal, 1);

  return (l1->type == RASQAL_LITERAL_STRING || l1->type == RASQAL_LITERAL_XSD_STRING);
}


/*
 * rasqal_literal_string_equals_flags:
 * @l1: #rasqal_literal first literal
 * @l2: #rasqal_literal second literal
 * @flags: comparison flags
 * @error_p: pointer to error
 *
 * INTERNAL - Compare two typed literals
 *
 * flag bits affects equality:
 *   RASQAL_COMPARE_XQUERY: use value equality
 *   RASQAL_COMPARE_RDF: use RDF term equality
 *
 * Return value: non-0 if equal
 */
static int
rasqal_literal_string_equals_flags(rasqal_literal* l1, rasqal_literal* l2,
                                   int flags, int* error_p)
{
  int result = 1;
  raptor_uri* dt1;
  int free_dt1 = 0;
  raptor_uri* dt2;
  int free_dt2 = 0;
  raptor_uri* xsd_string_uri;

  if(error_p)
    *error_p = 0;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l1, rasqal_literal, 0);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l2, rasqal_literal, 0);

  dt1 = l1->datatype;
  dt2 = l2->datatype;

  xsd_string_uri = rasqal_xsd_datatype_type_to_uri(l1->world,
                                                   RASQAL_LITERAL_XSD_STRING);

  if(rasqal_literal_string_languages_compare(l1, l2))
    return 0;

  /* For a value comparison (or RDQL), promote plain literal to typed
   * literal "xx"^^xsd:string if the other literal is typed
   */
  if(flags & RASQAL_COMPARE_XQUERY || flags & RASQAL_COMPARE_URI) {
    if(l1->type == RASQAL_LITERAL_STRING && 
       l2->type == RASQAL_LITERAL_XSD_STRING) {
      dt1 = raptor_uri_copy(xsd_string_uri);
      free_dt1 = 1;
    } else if(l1->type == RASQAL_LITERAL_XSD_STRING && 
              l2->type == RASQAL_LITERAL_STRING) {
      dt2 = raptor_uri_copy(xsd_string_uri);
      free_dt2 = 1;
    }
  }

  if(dt1 || dt2) {
    /* if either is NULL - type error */
    if(!dt1 || !dt2) {
      if(error_p)
        *error_p = 1;
      result = 0;
      goto done;
    }
    /* if different - type error */
    if(!raptor_uri_equals(dt1, dt2)) {
      if(error_p)
        *error_p = 1;
      result = 0;
      goto done;
    }
    /* at this point the datatypes (URIs) are the same */
  }

  /* Finally check the lexical forms */

  /* not-equal if lengths are different - cheaper to try this first */
  if(l1->string_len != l2->string_len) {
    result = 0;
    goto done;
  }

  result = !strcmp(RASQAL_GOOD_CAST(const char*, l1->string),
                   RASQAL_GOOD_CAST(const char*, l2->string));

  /* If result is equality but literals were both typed literals with
   * user-defined types then cause a type error; equality is unknown.
   */
  if(!result &&
     (l1->type == RASQAL_LITERAL_UDT && l2->type == RASQAL_LITERAL_UDT)) {
    if(error_p)
      *error_p = 1;
  }

  done:
  if(dt1 && free_dt1)
    raptor_free_uri(dt1);
  if(dt2 && free_dt2)
    raptor_free_uri(dt2);

  return result;
}


static int
rasqal_literal_uri_equals(rasqal_literal* l1, rasqal_literal* l2)
{
  return raptor_uri_equals(l1->value.uri, l2->value.uri);
}


static int
rasqal_literal_blank_equals(rasqal_literal* l1, rasqal_literal* l2)
{
  /* not-equal if lengths are different - cheap to compare this first */
  if(l1->string_len != l2->string_len)
    return 0;
      
  return !strcmp(RASQAL_GOOD_CAST(const char*, l1->string),
                 RASQAL_GOOD_CAST(const char*, l2->string));
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
  /* rasqal_literal_equals_flags() checks for NULLs */

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
  /* rasqal_literal_equals_flags() checks for NULLs */

  return rasqal_literal_equals_flags(l1, l2, 0, NULL);
}


/*
 * rasqal_literal_equals_flags:
 * @l1: #rasqal_literal literal
 * @l2: #rasqal_literal data literal
 * @flags: comparison flags
 * @error_p: type error
 *
 * INTERNAL - Compare two literals with optional type promotion.
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
  rasqal_literal* l1_p = NULL;
  rasqal_literal* l2_p = NULL;
  int result = 0;
  int promotion = 0;
  
  /* NULL literals */
  if(!l1 || !l2) {
    /* if either is not null, the comparison fails */
    return !(l1 || l2);
  }

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG1(" ");
  rasqal_literal_print(l1, stderr);
  fputs( " to ", stderr);
  rasqal_literal_print(l2, stderr);
  fprintf(stderr, " with flags %d\n", flags);
#endif

  if(flags & RASQAL_COMPARE_RDF) {
    /* no promotion but compare as RDF terms; like rasqal_literal_as_node() */
    rasqal_literal_type type1 = rasqal_literal_get_rdf_term_type(l1);
    rasqal_literal_type type2 = rasqal_literal_get_rdf_term_type(l2);

    if(type1 == RASQAL_LITERAL_UNKNOWN || type2 == RASQAL_LITERAL_UNKNOWN ||
       type1 != type2)
      goto tidy;

    type = type1;
  } else if(flags & RASQAL_COMPARE_XQUERY) { 
    /* SPARQL / XSD promotion rules */

    /* Ensure the values are native */
    rasqal_literal_string_to_native(l1, 0);
    rasqal_literal_string_to_native(l2, 0);

    if((l1->type == RASQAL_LITERAL_DATE && 
        l2->type == RASQAL_LITERAL_DATETIME) ||
       (l1->type == RASQAL_LITERAL_DATETIME &&
        l2->type == RASQAL_LITERAL_DATE)) {
      type = RASQAL_LITERAL_DATETIME;
      promotion = 1;
    } else if(l1->type != l2->type) {
      type = rasqal_literal_promote_numerics(l1, l2, flags);
      if(type == RASQAL_LITERAL_UNKNOWN) {
        /* Cannot numeric promote - try RDF equality */
        rasqal_literal_type type1 = rasqal_literal_get_rdf_term_type(l1);
        rasqal_literal_type type2 = rasqal_literal_get_rdf_term_type(l2);
        
        if(type1 == RASQAL_LITERAL_UNKNOWN || type2 == RASQAL_LITERAL_UNKNOWN ||
           type1 != type2)
          goto tidy;

        type = type1;
      } else
        promotion = 1;
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      RASQAL_DEBUG4("xquery promoted literals types (%s, %s) to type %s\n", 
                    rasqal_literal_type_label(l1->type),
                    rasqal_literal_type_label(l2->type),
                    rasqal_literal_type_label(type));
#endif
    } else
      type = l1->type;
  } else {
    /* RDQL rules: compare as values with no promotion */
    if(l1->type != l2->type) {
      /* booleans can be compared to strings */
      if(l2->type == RASQAL_LITERAL_BOOLEAN &&
         l1->type == RASQAL_LITERAL_STRING)
        result = !strcmp(RASQAL_GOOD_CAST(const char*, l1->string),
                         RASQAL_GOOD_CAST(const char*, l2->string));
      goto tidy;
    }
    type = l1->type;
  }

  if(promotion) {
    l1_p = rasqal_new_literal_from_promotion(l1, type, flags);
    if(l1_p)
      l2_p = rasqal_new_literal_from_promotion(l2, type, flags);
    if(!l1_p || !l2_p) {
      result = 1;
      goto tidy;
    }
  } else {
    l1_p = l1;
    l2_p = l2;
  }

  switch(type) {
    case RASQAL_LITERAL_URI:
      result = rasqal_literal_uri_equals(l1_p, l2_p);
      break;

    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_UDT:
      result = rasqal_literal_string_equals_flags(l1_p, l2_p, flags, error_p);
      break;

    case RASQAL_LITERAL_BLANK:
      result = rasqal_literal_blank_equals(l1_p, l2_p);
      break;

    case RASQAL_LITERAL_DATE:
      result = rasqal_xsd_date_equals(l1_p->value.date, l2_p->value.date,
                                      error_p);
      break;
      
    case RASQAL_LITERAL_DATETIME:
      result = rasqal_xsd_datetime_equals2(l1_p->value.datetime,
                                           l2_p->value.datetime,
                                           error_p);
      break;
      
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_INTEGER_SUBTYPE:
      result = l1_p->value.integer == l2_p->value.integer;
      break;


    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_FLOAT:
      result = rasqal_double_approximately_equal(l1_p->value.floating,
                                                 l2_p->value.floating);
      break;


    case RASQAL_LITERAL_DECIMAL:
      result = rasqal_xsd_decimal_equals(l1_p->value.decimal,
                                       l2_p->value.decimal);
      break;

    case RASQAL_LITERAL_VARIABLE:
      /* both are variables */
      result = rasqal_literal_equals(l1_p->value.variable->value,
                                     l2_p->value.variable->value);
      break;

    case RASQAL_LITERAL_UNKNOWN:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    default:
      if(error_p)
        *error_p = 1;
      result = 0; /* keep some compilers happy */
  }

  tidy:
  if(promotion) {
    if(l1_p)
      rasqal_free_literal(l1_p);
    if(l2_p)
      rasqal_free_literal(l2_p);
  }

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG2("equals result %d\n", result);
#endif

  return result;
}


/*
 * rasqal_literal_expand_qname:
 * @user_data: #rasqal_query cast as void for use with raptor_sequence_foreach
 * @l: #rasqal_literal literal
 *
 * INTERNAL - Expand any qname in a literal into a URI
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
  rasqal_query *rq = (rasqal_query *)user_data;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l, rasqal_literal, 1);

  if(l->type == RASQAL_LITERAL_QNAME) {
    raptor_uri *uri;

    /* expand a literal qname */
    uri = raptor_qname_string_to_uri(rq->namespaces,
                                     l->string, l->string_len);
    if(!uri)
      return 1;
    RASQAL_FREE(char*, l->string);
    l->string = NULL;
    l->type = RASQAL_LITERAL_URI;
    l->value.uri = uri;
  } else if (l->type == RASQAL_LITERAL_STRING) {
    raptor_uri *uri;
    
    if(l->flags) {
      /* expand a literal string datatype qname */
      uri = raptor_qname_string_to_uri(rq->namespaces,
                                       l->flags,
                                       strlen(RASQAL_GOOD_CAST(const char*, l->flags)));
      if(!uri)
        return 1;
      l->datatype = uri;
      RASQAL_FREE(char*, l->flags);
      l->flags = NULL;

      if(l->language && uri) {
        RASQAL_FREE(char*, l->language);
        l->language = NULL;
      }

      if(rasqal_literal_string_to_native(l, 0)) {
        rasqal_free_literal(l);
        return 1;
      }
    }
  }
  return 0;
}


/*
 * rasqal_literal_has_qname
 * @l: #rasqal_literal literal
 *
 * INTERNAL - Check if literal has a qname part
 *
 * Checks if any part ofthe literal has an unexpanded QName.
 * 
 * Return value: non-0 if a QName is present
 **/
int
rasqal_literal_has_qname(rasqal_literal *l)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l, rasqal_literal, 0);

  return (l->type == RASQAL_LITERAL_QNAME) ||
         (l->type == RASQAL_LITERAL_STRING && (l->flags));
}


/**
 * rasqal_literal_as_node:
 * @l: #rasqal_literal object
 *
 * Turn a literal into a new RDF string, URI or blank literal.
 * 
 * Return value: the new #rasqal_literal or NULL on failure or if the literal was an unbound variable.
 **/
rasqal_literal*
rasqal_literal_as_node(rasqal_literal* l)
{
  raptor_uri* dt_uri;
  rasqal_literal* new_l = NULL;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l, rasqal_literal, NULL);

  reswitch:
  switch(l->type) {
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_BLANK:
      new_l = rasqal_new_literal_from_literal(l);
      break;
      
    case RASQAL_LITERAL_VARIABLE:
      l = l->value.variable->value;
      if(!l)
        return NULL;
      goto reswitch;

    case RASQAL_LITERAL_DOUBLE:
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_DECIMAL:
    case RASQAL_LITERAL_DATE:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_UDT:
    case RASQAL_LITERAL_INTEGER_SUBTYPE:
      new_l = RASQAL_CALLOC(rasqal_literal*, 1, sizeof(*new_l));
      if(new_l) {
        new_l->valid = 1;
        new_l->usage = 1;
        new_l->world = l->world;
        new_l->type = RASQAL_LITERAL_STRING;
        new_l->string_len = l->string_len;
        new_l->string = RASQAL_MALLOC(unsigned char*, l->string_len + 1);
        if(!new_l->string) {
          rasqal_free_literal(new_l);
          return NULL; 
        }
        memcpy((void*)new_l->string, l->string, l->string_len + 1);

        if(l->type <= RASQAL_LITERAL_LAST_XSD) {
          dt_uri = rasqal_xsd_datatype_type_to_uri(l->world, l->type);
          if(!dt_uri) {
            rasqal_free_literal(new_l);
            return NULL;
          }
        } else {
          /* from the case: above this is UDT and INTEGER_SUBTYPE */
          dt_uri = l->datatype;
        }
        new_l->datatype = raptor_uri_copy(dt_uri);
        new_l->flags = NULL;
      }
      break;
      
    case RASQAL_LITERAL_QNAME:
      /* QNames should be gone by the time expression eval happens */

    case RASQAL_LITERAL_PATTERN:
      /* FALLTHROUGH */

    case RASQAL_LITERAL_UNKNOWN:
    default:
      RASQAL_FATAL2("Literal type %u has no node value", l->type);
  }
  
  return new_l;
}


/*
 * rasqal_literal_ebv:
 * @l: #rasqal_literal literal
 * 
 * INTERNAL -  Get the rasqal_literal effective boolean value
 *
 * Return value: non-0 if EBV is true, else false
 **/
int
rasqal_literal_ebv(rasqal_literal* l) 
{
  rasqal_variable* v;
  /* Result is true unless... */
  int b = 1;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l, rasqal_literal, 0);

  v = rasqal_literal_as_variable(l);
  if(v) {
    if(v->value == NULL) {
      /* ... The operand is unbound */
      b = 0;
      goto done;
    }
    l = v->value;
  }
  
  if(l->type == RASQAL_LITERAL_BOOLEAN && !l->value.integer) {
    /* ... The operand is an xs:boolean with a FALSE value. */
    b = 0;
  } else if(l->type == RASQAL_LITERAL_STRING && 
            !l->datatype && !l->string_len) {
    /* ... The operand is a 0-length untyped RDF literal or xs:string. */
    b = 0;
  } else if(((l->type == RASQAL_LITERAL_INTEGER || l->type == RASQAL_LITERAL_INTEGER_SUBTYPE) && !l->value.integer) ||
            ((l->type == RASQAL_LITERAL_DOUBLE || 
              l->type == RASQAL_LITERAL_FLOAT) &&
             !RASQAL_FLOATING_AS_INT(l->value.floating))
            ) {
    /* ... The operand is any numeric type with a value of 0. */
    b = 0;
  } else if(l->type == RASQAL_LITERAL_DECIMAL &&
            rasqal_xsd_decimal_is_zero(l->value.decimal)) {
    /* ... The operand is any numeric type with a value of 0 (decimal) */
    b = 0;
  } else if((l->type == RASQAL_LITERAL_DOUBLE || 
             l->type == RASQAL_LITERAL_FLOAT) &&
            isnan(l->value.floating)
            ) {
    /* ... The operand is an xs:double or xs:float with a value of NaN */
    b = 0;
  }
  
  done:
  return b;
}


/*
 * rasqal_literal_is_constant:
 * @l: #rasqal_literal literal
 * 
 * INTERNAL - Check if a literal is a constant
 *
 * Return value: non-0 if literal is a constant
 **/
int
rasqal_literal_is_constant(rasqal_literal* l)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l, rasqal_literal, 0);

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
    case RASQAL_LITERAL_DATE:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_UDT:
    case RASQAL_LITERAL_INTEGER_SUBTYPE:
      return 1;

    case RASQAL_LITERAL_VARIABLE:
      return 0;

    case RASQAL_LITERAL_UNKNOWN:
    default:
      RASQAL_FATAL2("Literal type %u cannot be checked for constant", l->type);
      return 0; /* keep some compilers happy */
  }
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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l, rasqal_literal, NULL);
  
  if(l->type != RASQAL_LITERAL_VARIABLE)
    return l->datatype;
  return rasqal_literal_datatype(l->value.variable->value);
}


rasqal_literal*
rasqal_literal_cast(rasqal_literal* l, raptor_uri* to_datatype, int flags, 
                    int* error_p)
{
#ifdef RASQAL_DEBUG
  raptor_uri* from_datatype = NULL;
#endif
  const unsigned char *string = NULL;
  unsigned char *new_string;
  rasqal_literal* result = NULL;
  rasqal_literal_type from_native_type;
  rasqal_literal_type to_native_type;
  size_t len;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l, rasqal_literal, NULL);
  
  l = rasqal_literal_value(l);
  if(!l)
    return NULL;

#ifdef RASQAL_DEBUG
  from_datatype = l->datatype;
#endif
  from_native_type = l->type;

  to_native_type = rasqal_xsd_datatype_uri_to_type(l->world, to_datatype);

  if(from_native_type == to_native_type) {
    /* cast to same type is always allowed */
    return rasqal_new_literal_from_literal(l);

  } else {
    int failed = 0;

    /* switch on FROM type to check YES/NO conversions and get the string */
    switch(from_native_type) {
      /* string */
      case RASQAL_LITERAL_STRING:
      case RASQAL_LITERAL_XSD_STRING:
      case RASQAL_LITERAL_UDT:
        string = l->string;
        len =  l->string_len;
        break;

      /* XSD datatypes: RASQAL_LITERAL_FIRST_XSD to RASQAL_LITERAL_LAST_XSD */
      case RASQAL_LITERAL_BOOLEAN:
      case RASQAL_LITERAL_INTEGER:
      case RASQAL_LITERAL_DOUBLE:
      case RASQAL_LITERAL_FLOAT:
      case RASQAL_LITERAL_DECIMAL:
      case RASQAL_LITERAL_INTEGER_SUBTYPE:
        /* XSD (boolean, integer, decimal, double, float) may NOT be
         * cast to dateTime or date */
        if(to_native_type == RASQAL_LITERAL_DATE ||
           to_native_type == RASQAL_LITERAL_DATETIME) {
          failed = 1;
          if(error_p)
            *error_p = 1;
          break;
        }
        string = l->string;
        len =  l->string_len;
        break;

      case RASQAL_LITERAL_DATE:
      case RASQAL_LITERAL_DATETIME:
        string = l->string;
        len =  l->string_len;
        break;

      /* SPARQL casts - FIXME */
      case RASQAL_LITERAL_BLANK:
      case RASQAL_LITERAL_PATTERN:
      case RASQAL_LITERAL_QNAME:
        string = l->string;
        len =  l->string_len;
        break;

      case RASQAL_LITERAL_URI:
        /* URI (IRI) May ONLY be cast to an xsd:string */
        if(to_native_type != RASQAL_LITERAL_XSD_STRING) {
          failed = 1;
          if(error_p)
            *error_p = 1;
          break;
        }

        string = raptor_uri_as_counted_string(l->value.uri, &len);
        if(!string) {
          failed = 1;
          if(error_p)
            *error_p = 1;
        }
        break;

      case RASQAL_LITERAL_VARIABLE:
        /* fallthrough since rasqal_literal_value() handled this above */
      case RASQAL_LITERAL_UNKNOWN:
      default:
        RASQAL_FATAL2("Literal type %u cannot be cast", l->type);
        failed = 1;
        return NULL; /* keep some compilers happy */
    }

    if(to_native_type == RASQAL_LITERAL_DATE ||
       to_native_type == RASQAL_LITERAL_DATETIME) {
      /* XSD date and dateTime may ONLY be cast from string (cast
       * from dateTime is checked above)
       */
      if(from_native_type != RASQAL_LITERAL_STRING) {
        failed = 1;
        if(error_p)
          *error_p = 1;
      }
    }

    if(failed)
      return NULL;
  }
  

  /* switch on the TO type to check MAYBE conversions */

  RASQAL_DEBUG4("CAST from \"%s\" type %s to type %s\n",
                string, 
                from_datatype ? RASQAL_GOOD_CAST(const char*, raptor_uri_as_string(from_datatype)) : "(NONE)",
                raptor_uri_as_string(to_datatype));
  
  if(!rasqal_xsd_datatype_check(to_native_type, string, flags)) {
    if(error_p)
      *error_p = 1;
    RASQAL_DEBUG3("Illegal cast to type %s string '%s'",
                  rasqal_xsd_datatype_label(to_native_type), string);
    return NULL;
  }

  new_string = RASQAL_MALLOC(unsigned char*, len + 1);
  if(!new_string) {
    if(error_p)
      *error_p = 1;
    return NULL;
  }
  memcpy(new_string, string, len + 1);
  to_datatype = raptor_uri_copy(to_datatype);  
  
  result = rasqal_new_string_literal(l->world, new_string, NULL,
                                     to_datatype, NULL);
  if(!result) {
    if(error_p)
      *error_p = 1;
  }
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
  if(!l)
    return NULL;
  
  while(l && l->type == RASQAL_LITERAL_VARIABLE) {
    l = l->value.variable->value;
  }
  
  return l;
}


int
rasqal_literal_is_numeric(rasqal_literal* literal)
{
  rasqal_literal_type parent_type;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(literal, rasqal_literal, 0);
  
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
  int error = 0;
  rasqal_literal_type type;
  rasqal_literal* l1_p = NULL;
  rasqal_literal* l2_p = NULL;
  int flags = 0;
  rasqal_literal* result = NULL;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l1, rasqal_literal, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l2, rasqal_literal, NULL);

  type = rasqal_literal_promote_numerics(l1, l2, flags);
  switch(type) {
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_INTEGER_SUBTYPE:
      i = rasqal_literal_as_integer(l1, &error);
      if(error)
        break;
      i = i + rasqal_literal_as_integer(l2, &error);
      if(error)
        break;

      result = rasqal_new_integer_literal(l1->world, RASQAL_LITERAL_INTEGER, i);
      break;
      
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_DOUBLE:
      d = rasqal_literal_as_double(l1, &error);
      if(error)
        break;
      d = d + rasqal_literal_as_double(l2, &error);
      if(error)
        break;

      result = rasqal_new_numeric_literal(l1->world, type, d);
      break;
      
    case RASQAL_LITERAL_DECIMAL:
      l1_p = rasqal_new_literal_from_promotion(l1, type, flags);
      if(l1_p)
        l2_p = rasqal_new_literal_from_promotion(l2, type, flags);
      if(l1_p && l2_p) {
        dec = rasqal_new_xsd_decimal(l1->world);
        if(rasqal_xsd_decimal_add(dec, l1_p->value.decimal,
                                  l2_p->value.decimal)) {
          error = 1;
          rasqal_free_xsd_decimal(dec);
        } else
          result = rasqal_new_decimal_literal_from_decimal(l1->world, NULL, dec);
      }
      break;
      
    case RASQAL_LITERAL_UNKNOWN:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_DATE:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_VARIABLE:
    case RASQAL_LITERAL_UDT:
    default:
      error = 1;
      break;
  }

  if(error) {
    if(error_p)
      *error_p = 1;
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
  int error = 0;
  rasqal_literal_type type;
  rasqal_literal* l1_p = NULL;
  rasqal_literal* l2_p = NULL;
  int flags = 0;
  rasqal_literal* result = NULL;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l1, rasqal_literal, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l2, rasqal_literal, NULL);

  type = rasqal_literal_promote_numerics(l1, l2, flags);
  switch(type) {
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_INTEGER_SUBTYPE:
      i = rasqal_literal_as_integer(l1, &error);
      if(error)
        break;
      i = i - rasqal_literal_as_integer(l2, &error);
      if(error)
        break;

      result = rasqal_new_integer_literal(l1->world, RASQAL_LITERAL_INTEGER, i);
      break;
      
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_DOUBLE:
      d = rasqal_literal_as_double(l1, &error);
      if(error)
        break;
      d = d - rasqal_literal_as_double(l2, &error);
      if(error)
        break;

      result = rasqal_new_numeric_literal(l1->world, type, d);
      break;
      
    case RASQAL_LITERAL_DECIMAL:
      l1_p = rasqal_new_literal_from_promotion(l1, type, flags);
      if(l1_p)
        l2_p = rasqal_new_literal_from_promotion(l2, type, flags);
      if(l1_p && l2_p) {
        dec = rasqal_new_xsd_decimal(l1->world);
        if(rasqal_xsd_decimal_subtract(dec, l1_p->value.decimal,
                                       l2_p->value.decimal)) {
          error = 1;
          rasqal_free_xsd_decimal(dec);
        } else
          result = rasqal_new_decimal_literal_from_decimal(l1->world, NULL, dec);
      }
      break;
      
    case RASQAL_LITERAL_UNKNOWN:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_DATE:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_VARIABLE:
    case RASQAL_LITERAL_UDT:
    default:
      error = 1;
      break;
  }

  if(error) {
    if(error_p)
      *error_p = 1;
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
  int error = 0;
  rasqal_literal_type type;
  rasqal_literal* l1_p = NULL;
  rasqal_literal* l2_p = NULL;
  int flags = 0;
  rasqal_literal* result = NULL;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l1, rasqal_literal, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l2, rasqal_literal, NULL);

  type = rasqal_literal_promote_numerics(l1, l2, flags);
  switch(type) {
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_INTEGER_SUBTYPE:
      i = rasqal_literal_as_integer(l1, &error);
      if(error)
        break;
      i = i * rasqal_literal_as_integer(l2, &error);
      if(error)
        break;

      result = rasqal_new_integer_literal(l1->world, RASQAL_LITERAL_INTEGER, i);
      break;
      
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_DOUBLE:
      d = rasqal_literal_as_double(l1, &error);
      if(error)
        break;
      d = d * rasqal_literal_as_double(l2, &error);
      if(error)
        break;

      result = rasqal_new_numeric_literal(l1->world, type, d);
      break;
      
    case RASQAL_LITERAL_DECIMAL:
      l1_p = rasqal_new_literal_from_promotion(l1, type, flags);
      if(l1_p)
        l2_p = rasqal_new_literal_from_promotion(l2, type, flags);
      if(l1_p && l2_p) {
        dec = rasqal_new_xsd_decimal(l1->world);
        if(rasqal_xsd_decimal_multiply(dec, l1_p->value.decimal,
                                       l2_p->value.decimal)) {
          error = 1;
          rasqal_free_xsd_decimal(dec);
        } else
          result = rasqal_new_decimal_literal_from_decimal(l1->world, NULL, dec);
      }
      break;
      
    case RASQAL_LITERAL_UNKNOWN:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_DATE:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_VARIABLE:
    case RASQAL_LITERAL_UDT:
    default:
      error = 1;
      break;
  }

  if(error) {
    if(error_p)
      *error_p = 1;
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
  double d1, d2;
  rasqal_xsd_decimal* dec;
  int error = 0;
  rasqal_literal_type type;
  rasqal_literal* l1_p = NULL;
  rasqal_literal* l2_p = NULL;
  int flags = 0;
  rasqal_literal* result = NULL;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l1, rasqal_literal, NULL);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l2, rasqal_literal, NULL);

  type = rasqal_literal_promote_numerics(l1, l2, flags);
  switch(type) {
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_DOUBLE:
      d2 = rasqal_literal_as_double(l2, &error);
      if(!RASQAL_FLOATING_AS_INT(d2))
        /* division by zero error */
        error = 1;
      if(error)
        break;
      d1 = rasqal_literal_as_double(l1, &error);
      if(error)
        break;
      d1 = d1 / d2;

      result = rasqal_new_numeric_literal(l1->world, type, d1);
      break;
      
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_INTEGER_SUBTYPE:
      /* "As a special case, if the types of both $arg1 and $arg2 are
       * xs:integer, then the return type is xs:decimal." - F&O
       */
      type = RASQAL_LITERAL_DECIMAL;
      /* fallthrough */

    case RASQAL_LITERAL_DECIMAL:
      l1_p = rasqal_new_literal_from_promotion(l1, type, flags);
      if(l1_p)
        l2_p = rasqal_new_literal_from_promotion(l2, type, flags);
      if(l1_p && l2_p) {
        dec = rasqal_new_xsd_decimal(l1->world);
        if(rasqal_xsd_decimal_divide(dec, l1_p->value.decimal,
                                     l2_p->value.decimal)) {
          error = 1;
          rasqal_free_xsd_decimal(dec);
        } else
          result = rasqal_new_decimal_literal_from_decimal(l1->world, NULL, dec);
      }
      break;
      
    case RASQAL_LITERAL_UNKNOWN:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_DATE:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_VARIABLE:
    case RASQAL_LITERAL_UDT:
    default:
      error = 1;
      break;
  }

  if(error) {
    if(error_p)
      *error_p = 1;
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
  int error = 0;
  rasqal_literal* result = NULL;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l, rasqal_literal, NULL);

  switch(l->type) {
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_INTEGER_SUBTYPE:
      i = rasqal_literal_as_integer(l, &error);
      if(error)
        break;
      i = -i;
      result = rasqal_new_integer_literal(l->world, RASQAL_LITERAL_INTEGER, i);
      break;
      
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_DOUBLE:
      d = rasqal_literal_as_double(l, &error);
      if(!RASQAL_FLOATING_AS_INT(d))
        error = 1;
      d = -d;
      result = rasqal_new_numeric_literal(l->world, l->type, d);
      break;
      
    case RASQAL_LITERAL_DECIMAL:
      dec = rasqal_new_xsd_decimal(l->world);
      if(rasqal_xsd_decimal_negate(dec, l->value.decimal)) {
        error = 1;
        rasqal_free_xsd_decimal(dec);
      } else
        result = rasqal_new_decimal_literal_from_decimal(l->world, NULL, dec);
      break;
      
    case RASQAL_LITERAL_UNKNOWN:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_DATE:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_VARIABLE:
    case RASQAL_LITERAL_UDT:
    default:
      error = 1;
      break;
  }

  if(error) {
    if(error_p)
      *error_p = 1;
  }
  
  return result;
}


rasqal_literal*
rasqal_literal_abs(rasqal_literal* l, int *error_p)
{
  int i;
  double d;
  rasqal_xsd_decimal* dec;
  int error = 0;
  rasqal_literal* result = NULL;

  if(!rasqal_literal_is_numeric(l))
    return NULL;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l, rasqal_literal, NULL);

  switch(l->type) {
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_INTEGER_SUBTYPE:
      i = rasqal_literal_as_integer(l, &error);
      if(error)
        break;

      i = abs(i);
      result = rasqal_new_integer_literal(l->world, RASQAL_LITERAL_INTEGER, i);
      break;
      
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_DOUBLE:
      d = rasqal_literal_as_double(l, &error);
      if(!RASQAL_FLOATING_AS_INT(d))
        error = 1;

      d = fabs(d);
      result = rasqal_new_numeric_literal(l->world, l->type, d);
      break;
      
    case RASQAL_LITERAL_DECIMAL:
      dec = rasqal_new_xsd_decimal(l->world);
      if(rasqal_xsd_decimal_abs(dec, l->value.decimal)) {
        error = 1;
        rasqal_free_xsd_decimal(dec);
      } else
        result = rasqal_new_decimal_literal_from_decimal(l->world, NULL, dec);
      break;
      
    case RASQAL_LITERAL_UNKNOWN:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_DATE:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_VARIABLE:
    case RASQAL_LITERAL_UDT:
    default:
      error = 1;
      break;
  }

  if(error) {
    if(error_p)
      *error_p = 1;
  }

  return result;
}


rasqal_literal*
rasqal_literal_round(rasqal_literal* l, int *error_p)
{
  double d;
  rasqal_xsd_decimal* dec;
  int error = 0;
  rasqal_literal* result = NULL;

  if(!rasqal_literal_is_numeric(l))
    return NULL;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l, rasqal_literal, NULL);

  switch(l->type) {
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_INTEGER_SUBTYPE:
      /* Result is same as input for integral types */
      result = rasqal_new_literal_from_literal(l);
      break;
      
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_DOUBLE:
      d = rasqal_literal_as_double(l, &error);
      if(!RASQAL_FLOATING_AS_INT(d))
        error = 1;

      d = round(d);
      result = rasqal_new_numeric_literal(l->world, l->type, d);
      break;
      
    case RASQAL_LITERAL_DECIMAL:
      dec = rasqal_new_xsd_decimal(l->world);
      if(rasqal_xsd_decimal_round(dec, l->value.decimal)) {
        error = 1;
        rasqal_free_xsd_decimal(dec);
      } else
        result = rasqal_new_decimal_literal_from_decimal(l->world, NULL, dec);
      break;
      
    case RASQAL_LITERAL_UNKNOWN:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_DATE:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_VARIABLE:
    case RASQAL_LITERAL_UDT:
    default:
      error = 1;
      break;
  }

  if(error) {
    if(error_p)
      *error_p = 1;
  }
  
  return result;
}


rasqal_literal*
rasqal_literal_ceil(rasqal_literal* l, int *error_p)
{
  double d;
  rasqal_xsd_decimal* dec;
  int error = 0;
  rasqal_literal* result = NULL;

  if(!rasqal_literal_is_numeric(l))
    return NULL;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l, rasqal_literal, NULL);

  switch(l->type) {
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_INTEGER_SUBTYPE:
      /* Result is same as input for integral types */
      result = rasqal_new_literal_from_literal(l);
      break;
      
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_DOUBLE:
      d = rasqal_literal_as_double(l, &error);
      if(!RASQAL_FLOATING_AS_INT(d))
        error = 1;

      d = ceil(d);
      result = rasqal_new_numeric_literal(l->world, l->type, d);
      break;
      
    case RASQAL_LITERAL_DECIMAL:
      dec = rasqal_new_xsd_decimal(l->world);
      if(rasqal_xsd_decimal_ceil(dec, l->value.decimal)) {
        error = 1;
        rasqal_free_xsd_decimal(dec);
      } else
        result = rasqal_new_decimal_literal_from_decimal(l->world, NULL, dec);
      break;
      
    case RASQAL_LITERAL_UNKNOWN:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_DATE:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_VARIABLE:
    case RASQAL_LITERAL_UDT:
    default:
      error = 1;
      break;
  }

  if(error) {
    if(error_p)
      *error_p = 1;
  }

  return result;
}


rasqal_literal*
rasqal_literal_floor(rasqal_literal* l, int *error_p)
{
  double d;
  rasqal_xsd_decimal* dec;
  int error = 0;
  rasqal_literal* result = NULL;

  if(!rasqal_literal_is_numeric(l))
    return NULL;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l, rasqal_literal, NULL);

  switch(l->type) {
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_INTEGER_SUBTYPE:
      /* Result is same as input for integral types */
      result = rasqal_new_literal_from_literal(l);
      break;
      
    case RASQAL_LITERAL_FLOAT:
    case RASQAL_LITERAL_DOUBLE:
      d = rasqal_literal_as_double(l, &error);
      if(!RASQAL_FLOATING_AS_INT(d))
        error = 1;

      d = floor(d);
      result = rasqal_new_numeric_literal(l->world, l->type, d);
      break;
      
    case RASQAL_LITERAL_DECIMAL:
      dec = rasqal_new_xsd_decimal(l->world);
      if(rasqal_xsd_decimal_floor(dec, l->value.decimal)) {
        error = 1;
        rasqal_free_xsd_decimal(dec);
      } else
        result = rasqal_new_decimal_literal_from_decimal(l->world, NULL, dec);
      break;
      
    case RASQAL_LITERAL_UNKNOWN:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_XSD_STRING:
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_DATE:
    case RASQAL_LITERAL_DATETIME:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
    case RASQAL_LITERAL_VARIABLE:
    case RASQAL_LITERAL_UDT:
    default:
      error = 1;
      break;
  }

  if(error) {
    if(error_p)
      *error_p = 1;
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
  rasqal_literal_type type1;
  rasqal_literal_type type2;

  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l1, rasqal_literal, 0);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l2, rasqal_literal, 0);

  type1 = rasqal_literal_get_rdf_term_type(l1);
  type2 = rasqal_literal_get_rdf_term_type(l2);

  if(type1 != type2)
    return 0;
  
  if(type1 == RASQAL_LITERAL_UNKNOWN)
    return 0;
  
  if(type1 == RASQAL_LITERAL_URI)
    return rasqal_literal_uri_equals(l1, l2);

  if(type1 == RASQAL_LITERAL_STRING)
    /* value compare */
    return rasqal_literal_string_equals_flags(l1, l2, RASQAL_COMPARE_XQUERY,
                                              NULL);

  if(type1 == RASQAL_LITERAL_BLANK)
    return rasqal_literal_blank_equals(l1, l2);

  return 0;
}


/**
 * rasqal_literal_is_rdf_literal:
 * @l: #rasqal_literal literal
 *
 * Check if a literal is any RDF term literal - plain or typed literal
 *
 * Return value: non-0 if the value is an RDF term literal
 **/
int
rasqal_literal_is_rdf_literal(rasqal_literal* l)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(l, rasqal_literal, 0);

  return (rasqal_literal_get_rdf_term_type(l) == RASQAL_LITERAL_STRING);
}


/**
 * rasqal_literal_sequence_compare:
 * @compare_flags: comparison flags for rasqal_literal_compare()
 * @values_a: first sequence of literals
 * @values_b: second sequence of literals
 *
 * INTERNAL - compare two sequences of literals
 *
 * Return value: <0, 0 or >1 comparison
 */
int
rasqal_literal_sequence_compare(int compare_flags,
                                raptor_sequence* values_a,
                                raptor_sequence* values_b)
{
  int result = 0;
  int i;
  int size_a = 0;
  int size_b = 0;
  
  /* Turn 0-length sequences into NULL */
  if(values_a) {
    size_a = raptor_sequence_size(values_a);
    if(!size_a)
      values_a = NULL;
  }

  if(values_b) {
    size_b = raptor_sequence_size(values_b);
    if(!size_b)
      values_b = NULL;
  }

  /* Handle empty sequences: equal if both empty, otherwise empty is earlier */
  if(!size_a && !size_b)
    return 0;
  else if(!size_a)
    return -1;
  else if(!size_b)
    return 1;
  

  /* Now know they are not 0 length */

  /* Walk maximum length of the values */
  if(size_b > size_a)
    size_a = size_b;
  
  for(i = 0; i < size_a; i++) {
    rasqal_literal* literal_a = (rasqal_literal*)raptor_sequence_get_at(values_a, i);
    rasqal_literal* literal_b = (rasqal_literal*)raptor_sequence_get_at(values_b, i);
    int error = 0;
    
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("Comparing ");
    rasqal_literal_print(literal_a, DEBUG_FH);
    fputs(" to ", DEBUG_FH);
    rasqal_literal_print(literal_b, DEBUG_FH);
    fputs("\n", DEBUG_FH);
#endif

    if(!literal_a || !literal_b) {
      if(!literal_a && !literal_b) {
        result = 0;
      } else {
        result = literal_a ? 1 : -1;
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
        RASQAL_DEBUG2("Got one NULL literal comparison, returning %d\n",
                      result);
#endif
      }
      break;
    }
    
    result = rasqal_literal_compare(literal_a, literal_b,
                                    compare_flags, &error);

    if(error) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      RASQAL_DEBUG2("Got literal comparison error at literal %d, returning 0\n",
                    i);
#endif
      result = 0;
      break;
    }
        
    if(!result)
      continue;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG3("Returning comparison result %d at literal %d\n", result, i);
#endif
    break;
  }

  return result;
}


int
rasqal_literal_write_turtle(rasqal_literal* l, raptor_iostream* iostr)
{
  const unsigned char* str;
  size_t len;
  int rc = 0;
  
  if(!l)
    return rc;

  switch(l->type) {
    case RASQAL_LITERAL_URI:
      str = RASQAL_GOOD_CAST(const unsigned char*, raptor_uri_as_counted_string(l->value.uri, &len));
      
      raptor_iostream_write_byte('<', iostr);
      if(str)
        raptor_string_ntriples_write(str, len, '>', iostr);
      raptor_iostream_write_byte('>', iostr);
      break;
      
    case RASQAL_LITERAL_BLANK:
      raptor_iostream_counted_string_write("_:", 2, iostr);
      raptor_iostream_counted_string_write(l->string, l->string_len,
                                           iostr);
      break;
      
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_UDT:
      raptor_iostream_write_byte('"', iostr);
      raptor_string_ntriples_write(l->string, l->string_len, '"', iostr);
      raptor_iostream_write_byte('"', iostr);
      
      if(l->language) {
        raptor_iostream_write_byte('@', iostr);
        raptor_iostream_string_write(l->language, iostr);
      }
      
      if(l->datatype) {
        str = RASQAL_GOOD_CAST(const unsigned char*, raptor_uri_as_counted_string(l->datatype, &len));
        raptor_iostream_counted_string_write("^^<", 3, iostr);
        raptor_string_ntriples_write(str, len, '>', iostr);
        raptor_iostream_write_byte('>', iostr);
      }
      
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
      rasqal_log_error_simple(l->world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                              "Cannot turn literal type %u into Turtle",
                              l->type);
      rc = 1;
  }

  return rc;
}


/*
 * rasqal_literal_array_compare:
 * @values_a: first array of literals
 * @values_b: second array of literals
 * @exprs_seq: array of expressions (or NULL)
 * @size: size of arrays
 * @compare_flags: comparison flags for rasqal_literal_compare()
 *
 * INTERNAL - compare two arrays of literals evaluated in an optional array of expressions
 *
 * Return value: <0, 0 or >0 comparison
 */
int
rasqal_literal_array_compare(rasqal_literal** values_a,
                             rasqal_literal** values_b,
                             raptor_sequence* exprs_seq,
                             int size,
                             int compare_flags)
{
  int result = 0;
  int i;

  for(i = 0; i < size; i++) {
    rasqal_expression* e = NULL;
    int error = 0;
    rasqal_literal* literal_a = values_a[i];
    rasqal_literal* literal_b = values_b[i];
    
    if(exprs_seq)
      e = (rasqal_expression*)raptor_sequence_get_at(exprs_seq, i);

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("Comparing ");
    rasqal_literal_print(literal_a, DEBUG_FH);
    fputs(" to ", DEBUG_FH);
    rasqal_literal_print(literal_b, DEBUG_FH);
    fputs("\n", DEBUG_FH);
#endif

    /* NULLs order first */
    if(!literal_a || !literal_b) {
      if(!literal_a && !literal_b) {
        result = 0;
      } else {
        result = (!literal_a) ? -1 : 1;
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
        RASQAL_DEBUG2("Got one NULL literal comparison, returning %d\n", result);
#endif
      }
      break;
    }
    
    result = rasqal_literal_compare(literal_a, literal_b,
                                    compare_flags | RASQAL_COMPARE_URI,
                                    &error);
    if(error) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      RASQAL_DEBUG2("Got literal comparison error at expression %d, returning 0\n", i);
#endif
      result = 0;
      break;
    }
        
    if(!result)
      continue;

    if(e && e->op == RASQAL_EXPR_ORDER_COND_DESC)
      result = -result;
    /* else Order condition is RASQAL_EXPR_ORDER_COND_ASC so nothing to do */
    
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG3("Returning comparison result %d at expression %d\n", result, i);
#endif
    break;
  }

  return result;
}


/*
 * rasqal_literal_array_compare_by_order:
 * @values_a: first array of literals
 * @values_b: second array of literals
 * @order: array of order offsets
 * @size: size of arrays
 * @compare_flags: comparison flags for rasqal_literal_compare()
 *
 * INTERNAL - compare two arrays of literals evaluated in a given order
 *
 * Return value: <0, 0 or >0 comparison
 */
int
rasqal_literal_array_compare_by_order(rasqal_literal** values_a,
                                      rasqal_literal** values_b,
                                      int* order,
                                      int size,
                                      int compare_flags)
{
  int result = 0;
  int i;

  for(i = 0; i < size; i++) {
    int error = 0;
    int order_i = order[i];
    rasqal_literal* literal_a = values_a[order_i];
    rasqal_literal* literal_b = values_b[order_i];

    /* NULLs order first */
    if(!literal_a || !literal_b) {
      if(!literal_a && !literal_b) {
        result = 0;
      } else {
        result = (!literal_a) ? -1 : 1;
      }
      break;
    }

    result = rasqal_literal_compare(literal_a, literal_b,
                                    compare_flags | RASQAL_COMPARE_URI,
                                    &error);
    if(error) {
      result = 0;
      break;
    }

    if(!result)
      continue;
    break;
  }

  return result;
}


/**
 * rasqal_literal_array_equals:
 * @values_a: first array of literals
 * @values_b: second array of literals
 * @size: size of arrays
 *
 * INTERNAL - compare two arrays of literals for equality
 *
 * Return value: non-0 if equal
 */
int
rasqal_literal_array_equals(rasqal_literal** values_a,
                            rasqal_literal** values_b,
                            int size)
{
  int result = 1; /* equal */
  int i;
  int error = 0;

  for(i = 0; i < size; i++) {
    rasqal_literal* literal_a = values_a[i];
    rasqal_literal* literal_b = values_b[i];
    
    result = rasqal_literal_equals_flags(literal_a, literal_b,
                                         RASQAL_COMPARE_RDF, &error);
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("Comparing ");
    rasqal_literal_print(literal_a, DEBUG_FH);
    fputs(" to ", DEBUG_FH);
    rasqal_literal_print(literal_b, DEBUG_FH);
    fprintf(DEBUG_FH, " gave %s\n", (result ? "equality" : "not equal"));
#endif

    if(error)
      result = 0;
    
    /* if different, end */
    if(!result)
      break;
  }

  return result;
}


/**
 * rasqal_literal_sequence_equals:
 * @values_a: first sequence of literals
 * @values_b: second sequence of literals
 *
 * INTERNAL - compare two arrays of literals for equality
 *
 * Return value: non-0 if equal
 */
int
rasqal_literal_sequence_equals(raptor_sequence* values_a,
                               raptor_sequence* values_b)
{
  int result = 1; /* equal */
  int i;
  int error = 0;
  int size = raptor_sequence_size(values_a);

  for(i = 0; i < size; i++) {
    rasqal_literal* literal_a = (rasqal_literal*)raptor_sequence_get_at(values_a, i);
    rasqal_literal* literal_b = (rasqal_literal*)raptor_sequence_get_at(values_b, i);
    
    result = rasqal_literal_equals_flags(literal_a, literal_b,
                                         RASQAL_COMPARE_RDF, &error);
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("Comparing ");
    rasqal_literal_print(literal_a, DEBUG_FH);
    fputs(" to ", DEBUG_FH);
    rasqal_literal_print(literal_b, DEBUG_FH);
    fprintf(DEBUG_FH, " gave %s\n", (result ? "equality" : "not equal"));
#endif

    if(error)
      result = 0;
    
    /* if different, end */
    if(!result)
      break;
  }

  return result;
}


typedef struct 
{ 
  int is_distinct;
  int compare_flags;
} literal_sequence_sort_compare_data;


/**
 * rasqal_literal_sequence_sort_map_compare:
 * @user_data: comparison user data pointer
 * @a: pointer to address of first array
 * @b: pointer to address of second array
 *
 * INTERNAL - compare two pointers to array of iterals objects
 *
 * Suitable for use as a compare function in qsort_r() or similar.
 *
 * Return value: <0, 0 or >1 comparison
 */
static int
rasqal_literal_sequence_sort_map_compare(void* user_data,
                                         const void *a,
                                         const void *b)
{
  raptor_sequence* literal_seq_a;
  raptor_sequence* literal_seq_b;
  literal_sequence_sort_compare_data* lsscd;
  int result = 0;

  lsscd = (literal_sequence_sort_compare_data*)user_data;

  literal_seq_a = (raptor_sequence*)a;
  literal_seq_b = (raptor_sequence*)b;

  if(lsscd->is_distinct) {
    result = !rasqal_literal_sequence_equals(literal_seq_a, literal_seq_b);
    if(!result)
      /* duplicate, so return that */
      return 0;
  }
  
  /* now order it */
  result = rasqal_literal_sequence_compare(lsscd->compare_flags,
                                           literal_seq_a, literal_seq_b);

  /* still equal?  make sort stable by using the pointers */
  if(!result) {
    ptrdiff_t d;

    /* Have to cast raptor_sequence* to something with a known type
     * (not void*, not raptor_sequence* whose size is private to
     * raptor) so we can do pointer arithmetic.  We only care about
     * the relative pointer difference.
     */
    d = RASQAL_GOOD_CAST(char*, literal_seq_a) - RASQAL_GOOD_CAST(char*, literal_seq_b);

    /* copy the sign of the (unknown size) signed integer 'd' into an
     * int result
     */
    result = (d > 0) - (d < 0);
    RASQAL_DEBUG2("Got equality result so using pointers, returning %d\n",
                  result);
  }
  
  return result;
}


static int
rasqal_literal_sequence_sort_map_print_literal_sequence(void *object, FILE *fh)
{
  if(object)
    raptor_sequence_print((raptor_sequence*)object, fh);
  else
    fputs("NULL", fh);
  return 0;
}


/**
 * rasqal_new_literal_sequence_sort_map:
 * @compare_flags: flags for rasqal_literal_compare()
 *
 * INTERNAL - create a new map for sorting arrays of literals
 *
 */
rasqal_map*
rasqal_new_literal_sequence_sort_map(int is_distinct, int compare_flags)
{
  literal_sequence_sort_compare_data* lsscd;

  lsscd = RASQAL_MALLOC(literal_sequence_sort_compare_data*, sizeof(*lsscd));
  if(!lsscd)
    return NULL;
  
  lsscd->is_distinct = is_distinct;
  lsscd->compare_flags = compare_flags;
  
  return rasqal_new_map(rasqal_literal_sequence_sort_map_compare,
                        lsscd,
                        (raptor_data_free_handler)rasqal_free_memory,
                        (raptor_data_free_handler)raptor_free_sequence,
                        NULL, /* free_value_fn */
                        rasqal_literal_sequence_sort_map_print_literal_sequence,
                        NULL,
                        0 /* do not allow duplicates */);
}


/**
 * rasqal_literal_sequence_sort_map_add_literal_sequence:
 * @map: literal sort map
 * @literals_seq: Sequence of #rasqal_literal to add
 *
 * INTERNAL - Add a row to an literal sequence sort map for sorting
 *
 * The literals array @literals_sequence becomes owned by the map.
 *
 * return value: non-0 if the array of literals was a duplicate (and not added)
 */
int
rasqal_literal_sequence_sort_map_add_literal_sequence(rasqal_map* map,
                                                      raptor_sequence *literals_seq)
{
  if(!rasqal_map_add_kv(map, literals_seq, NULL))
    return 0;

  /* duplicate, and not added so delete it */
#ifdef RASQAL_DEBUG
  RASQAL_DEBUG1("Got duplicate array of literals ");
  raptor_sequence_print(literals_seq, DEBUG_FH);
  fputc('\n', DEBUG_FH);
#endif
  raptor_free_sequence(literals_seq);

  return 1;
}


/**
 * rasqal_new_literal_sequence_of_sequence_from_data:
 * @world: world object ot use
 * @row_data: row data
 *
 * INTERNAL - Make a sequence of sequence of #rasqal_literal* literals
 *
 * The @row_data parameter is an array of strings forming a table of
 * width (literals_count * 2).
 * The rows are values where offset 0 is a string literal and
 * offset 1 is a URI string literal.  If a string literal looks like
 * a number (strtol passes), an integer literal is formed.
 *
 * The end of data is indicated by an entire row of NULLs.
 *
 * Return value: sequence of rows or NULL on failure
 */
raptor_sequence*
rasqal_new_literal_sequence_of_sequence_from_data(rasqal_world* world,
                                                  const char* const row_data[],
                                                  int width)
{
  raptor_sequence *seq = NULL;
  int row_i;
  int column_i;
  int failed = 0;
  
#define GET_CELL(row, column, offset) \
  row_data[((((row) * width) + (column))<<1) + (offset)]

  seq = raptor_new_sequence((raptor_data_free_handler)raptor_free_sequence,
                            (raptor_data_print_handler)raptor_sequence_print);
  if(!seq)
    return NULL;

  for(row_i = 0; 1; row_i++) {
    raptor_sequence* row;
    int data_values_seen = 0;

    /* Terminate on an entire row of NULLs */
    for(column_i = 0; column_i < width; column_i++) {
      if(GET_CELL(row_i, column_i, 0) || GET_CELL(row_i, column_i, 1)) {
        data_values_seen++;
        break;
      }
    }
    if(!data_values_seen)
      break;
    
    row = raptor_new_sequence((raptor_data_free_handler)rasqal_free_literal,
                              (raptor_data_print_handler)rasqal_literal_print);
    if(!row) {
      raptor_free_sequence(seq); seq = NULL;
      goto tidy;
    }

    for(column_i = 0; column_i < width; column_i++) {
      rasqal_literal* l = NULL;

      if(GET_CELL(row_i, column_i, 0)) {
        /* string literal */
        const char* str = GET_CELL(row_i, column_i, 0);
        size_t str_len = strlen(str);
        char *eptr = NULL;
        long number;
        
        number = strtol(RASQAL_GOOD_CAST(const char*, str), &eptr, 10);
        if(!*eptr) {
          /* is numeric */
          l = rasqal_new_numeric_literal_from_long(world,
                                                   RASQAL_LITERAL_INTEGER, 
                                                   number);
        } else {
          unsigned char *val;
          val = RASQAL_MALLOC(unsigned char*, str_len + 1);
          if(val) {
            memcpy(val, str, str_len + 1);

            l = rasqal_new_string_literal_node(world, val, NULL, NULL);
          } else 
            failed = 1;
        }
      } else if(GET_CELL(row_i, column_i, 1)) {
        /* URI */
        const unsigned char* str;
        raptor_uri* u;

        str = RASQAL_GOOD_CAST(const unsigned char*, GET_CELL(row_i, column_i, 1));
        u = raptor_new_uri(world->raptor_world_ptr, str);

        if(u)
          l = rasqal_new_uri_literal(world, u);
        else
          failed = 1;
      } else {
        /* variable is not defined for this row */
        continue;
      }

      if(!l) {
        raptor_free_sequence(row);
        failed = 1;
        goto tidy;
      }
      raptor_sequence_set_at(row, column_i, l);
    }

    raptor_sequence_push(seq, row);
  }

  tidy:
  if(failed) {
    if(seq) {
      raptor_free_sequence(seq);
      seq = NULL;
    }
  }
  
  return seq;
}



/*
 * rasqal_new_literal_from_term:
 * @world: rasqal world
 * @term: term object
 *
 * INTERNAL - create a new literal from a #raptor_term
 *
 * Return value: new literal or NULL on failure
*/
rasqal_literal*
rasqal_new_literal_from_term(rasqal_world* world, raptor_term* term)
{
  rasqal_literal* l = NULL;
  size_t len;
  unsigned char* new_str = NULL;

  if(!term)
    return NULL;

  if(term->type == RAPTOR_TERM_TYPE_LITERAL) {
    char *language = NULL;
    raptor_uri* uri = NULL;

    len = term->value.literal.string_len;
    new_str = RASQAL_MALLOC(unsigned char*, len + 1);
    if(!new_str)
      goto fail;

    memcpy(new_str, term->value.literal.string, len + 1);

    if(term->value.literal.language) {
      len = term->value.literal.language_len;
      language = RASQAL_MALLOC(char*, len + 1);
      if(!language)
        goto fail;

      memcpy(language, term->value.literal.language, len + 1);
    }

    if(term->value.literal.datatype)
      uri = raptor_uri_copy(term->value.literal.datatype);

    l = rasqal_new_string_literal(world, new_str, language, uri, NULL);
  } else if(term->type == RAPTOR_TERM_TYPE_BLANK) {
    len = term->value.blank.string_len;
    new_str = RASQAL_MALLOC(unsigned char*, len + 1);
    if(!new_str)
      goto fail;

    memcpy(new_str, term->value.blank.string, len + 1);
    l = rasqal_new_simple_literal(world, RASQAL_LITERAL_BLANK, new_str);
  } else if(term->type == RAPTOR_TERM_TYPE_URI) {
    raptor_uri* uri;
    uri = raptor_uri_copy((raptor_uri*)term->value.uri);
    l = rasqal_new_uri_literal(world, uri);
  } else
    goto fail;

  return l;

  fail:
  if(new_str)
    RASQAL_FREE(unsigned char*, new_str);

  return NULL;
}


#endif /* not STANDALONE */




#ifdef STANDALONE
#include <stdio.h>

int main(int argc, char *argv[]);

/* Test 0 */
static const char* const data_3x3_unique_rows[] =
{
  /* 3 literals in 3 rows - all distinct */
  /* row 1 data */
  "0",  NULL, "1",  NULL, "2", NULL,
  /* row 2 data */
  "3",  NULL, "4",  NULL, "5", NULL,
  /* row 3 data */
  "6",  NULL, "7",  NULL, "8", NULL,
  /* end of data */
  NULL, NULL, NULL, NULL, NULL, NULL,
};

static const char* const data_3x4_1_duplicate_rows[] =
{
  /* 3 literals in 4 rows - with one duplicate */
  /* row 1 data */
  "0",  NULL, "1",  NULL, "2", NULL,
  /* row 2 data */
  "3",  NULL, "4",  NULL, "5", NULL,
  /* row 3 data */
  "0",  NULL, "1",  NULL, "2", NULL,
  /* row 4 data */
  "6",  NULL, "7",  NULL, "8", NULL,
  /* end of data */
  NULL, NULL, NULL, NULL, NULL, NULL,
};

static const char* const data_3x6_2_duplicate_rows[] =
{
  /* 3 literals in 6 rows - with one duplicate */
  /* row 1 data */
  "0",  NULL, "1",  NULL, "2", NULL,
  /* row 2 data */
  "3",  NULL, "4",  NULL, "5", NULL,
  /* row 3 data */
  "0",  NULL, "1",  NULL, "2", NULL,
  /* row 4 data */
  "3",  NULL, "4",  NULL, "5", NULL,
  /* row 5 data */
  "6",  NULL, "7",  NULL, "8", NULL,
  /* row 6 data */
  "8",  NULL, "9",  NULL, "0", NULL,
  /* end of data */
  NULL, NULL, NULL, NULL, NULL, NULL,
};



#define TESTS_COUNT 3

static const struct {
  int width;
  int expected_rows;
  const char* const *data;
} test_data[TESTS_COUNT] = {
  /* Test 0: 3 literals, 3 rows (no duplicates) */
  {3, 3, data_3x3_unique_rows },
  /* Test 1: 3 literals, 4 rows (1 duplicate) */
  {3, 3, data_3x4_1_duplicate_rows },
  /* Test 2: 3 literals, 6 rows (2 duplicate2) */
  {3, 4, data_3x6_2_duplicate_rows }
};


int
main(int argc, char *argv[]) 
{
  const char *program = rasqal_basename(argv[0]);
  int failures = 0;
  rasqal_world *world;
  int test_id;
  
  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    return(1);
  }
    
  /* test */
  fprintf(stderr, "%s: Testing literals\n", program);

  for(test_id = 0; test_id < TESTS_COUNT; test_id++) {
    int expected_rows = test_data[test_id].expected_rows;
    int width = test_data[test_id].width;
    raptor_sequence* seq;
    int duplicates;
    int count;
    int i;
    raptor_sequence* seq2;
    rasqal_map* map;
    
    seq = rasqal_new_literal_sequence_of_sequence_from_data(world,
                                                            test_data[test_id].data,
                                                            width);
    if(!seq) {
      fprintf(stderr, "%s: failed to create seq of literal seq\n", program);
      failures++;
      goto tidy;
    }

    fprintf(DEBUG_FH, "%s: Test %d data (seq of seq of literals) is: ", program,
            test_id);
    raptor_sequence_print(seq, DEBUG_FH);
    fputc('\n', DEBUG_FH);
    
    map = rasqal_new_literal_sequence_sort_map(1 /* is_distinct */,
                                               0 /* compare_flags */);
    if(!map) {
      fprintf(DEBUG_FH, "%s: Test %d failed to create map\n",
              program, test_id);
      failures++;
      raptor_free_sequence(seq);
      continue;
    }
    
    duplicates = 0;
    count = 0;
    for(i = 0;
        (seq2 = (raptor_sequence*)raptor_sequence_delete_at(seq, i)); 
        i++) {
      int rc;
      
      rc = rasqal_literal_sequence_sort_map_add_literal_sequence(map, seq2);
      if(rc) {
        fprintf(DEBUG_FH, "%s: Test %d literal seq %d is a duplicate\n", 
                program, test_id, i);
        duplicates++;
      } else
        count++;
    }
    rasqal_free_map(map);

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    fprintf(DEBUG_FH, "%s: Test %d had %d duplicates\n", program, test_id, 
            duplicates);
#endif

    raptor_free_sequence(seq);

    if(count != expected_rows) {
      fprintf(DEBUG_FH, "%s: Test %d returned %d rows expected %d\n", program,
              test_id, count, expected_rows);
      failures++;
    }
  }
  

  tidy:
  rasqal_free_world(world);

  return failures;
}
#endif /* STANDALONE */

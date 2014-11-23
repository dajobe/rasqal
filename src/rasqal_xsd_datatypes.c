/*
 * rasqal_xsd_datatypes.c - Rasqal XML Schema Datatypes support
 *
 * Copyright (C) 2005-2010, David Beckett http://www.dajobe.org/
 * Copyright (C) 2005-2005, University of Bristol, UK http://www.bristol.ac.uk/
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
#include <ctype.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdarg.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "rasqal.h"
#include "rasqal_internal.h"

/* local prototypes */
int rasqal_xsd_check_double_format(const unsigned char* string, int flags);


#ifndef STANDALONE

/*
 *
 * References
 *
 * XPath Functions and Operators
 * http://www.w3.org/TR/xpath-functions/
 *
 * Datatypes hierarchy
 * http://www.w3.org/TR/xpath-functions/#datatypes
 *
 * Casting
 * http://www.w3.org/TR/xpath-functions/#casting-from-primitive-to-primitive
 *
 */


int
rasqal_xsd_boolean_value_from_string(const unsigned char* string)
{
  int integer = 0;

  /* FIXME
   * Strictly only {true, false, 1, 0} are allowed according to
   * http://www.w3.org/TR/xmlschema-2/#boolean
   */
  if(!strcmp(RASQAL_GOOD_CAST(const char*, string), "true") ||
     !strcmp(RASQAL_GOOD_CAST(const char*, string), "TRUE") ||
     !strcmp(RASQAL_GOOD_CAST(const char*, string), "1"))
    integer = 1;

  return integer;
}


static int
rasqal_xsd_check_boolean_format(const unsigned char* string, int flags)
{
  /* FIXME
   * Strictly only {true, false, 1, 0} are allowed according to
   * http://www.w3.org/TR/xmlschema-2/#boolean
   */
  if(!strcmp(RASQAL_GOOD_CAST(const char*, string), "true") ||
     !strcmp(RASQAL_GOOD_CAST(const char*, string), "TRUE") ||
     !strcmp(RASQAL_GOOD_CAST(const char*, string), "1") ||
     !strcmp(RASQAL_GOOD_CAST(const char*, string), "false") ||
     !strcmp(RASQAL_GOOD_CAST(const char*, string), "FALSE") ||
     !strcmp(RASQAL_GOOD_CAST(const char*, string), "0"))
    return 1;

  return 0;
}


#define ADVANCE_OR_DIE(p) if(!*(++p)) return 0;


/**
 * rasqal_xsd_check_date_format:
 * @string: lexical form string
 * flags: flags
 *
 * INTERNAL - Check an XSD date lexical form
 *
 * Return value: non-0 if the string is valid
 */
static int
rasqal_xsd_check_date_format(const unsigned char* string, int flags) 
{
  /* This should be correct according to 
   * http://www.w3.org/TR/xmlschema-2/#date
   */
  return rasqal_xsd_date_check(RASQAL_GOOD_CAST(const char*, string));
}


/**
 * rasqal_xsd_check_dateTime_format:
 * @string: lexical form string
 * flags: flags
 *
 * INTERNAL - Check an XSD dateTime lexical form
 *
 * Return value: non-0 if the string is valid
 */
static int
rasqal_xsd_check_dateTime_format(const unsigned char* string, int flags) 
{
  /* This should be correct according to 
   * http://www.w3.org/TR/xmlschema-2/#dateTime
   */
  return rasqal_xsd_datetime_check(RASQAL_GOOD_CAST(const char*, string));
}


/**
 * rasqal_xsd_check_decimal_format:
 * @string: lexical form string
 * flags: flags
 *
 * INTERNAL - Check an XSD decimal lexical form
 *
 * Return value: non-0 if the string is valid
 */
static int
rasqal_xsd_check_decimal_format(const unsigned char* string, int flags) 
{
  const char* p;
  
  /* This should be correct according to 
   * http://www.w3.org/TR/xmlschema-2/#decimal
   */
  p = RASQAL_GOOD_CAST(const char*, string);
  if(*p == '+' || *p == '-') {
    ADVANCE_OR_DIE(p);
  }

  while(*p && isdigit(RASQAL_GOOD_CAST(int, *p)))
    p++;
  if(!*p)
    return 1;
  /* Fail if first non-digit is not '.' */
  if(*p != '.')
    return 0;
  p++;
  
  while(*p && isdigit(RASQAL_GOOD_CAST(int, *p)))
    p++;
  /* Fail if anything other than a digit seen before NUL */
  if(*p)
    return 0;

  return 1;
}


/* Legal special double values */
#define XSD_DOUBLE_SPECIALS_LEN 3
static const char * const xsd_double_specials[XSD_DOUBLE_SPECIALS_LEN] = {
  "-INF",
  "INF",
  "NaN"
};

/**
 * rasqal_xsd_check_double_format:
 * @string: lexical form string
 * flags: flags
 *
 * INTERNAL - Check an XSD double lexical form
 *
 * Return value: non-0 if the string is valid
 */
int
rasqal_xsd_check_double_format(const unsigned char* string, int flags) 
{
  const char *p = RASQAL_GOOD_CAST(const char*, string);
  const char *saved_p;
  int i;
  
  if(!*p)
    return 0;

  /* Validating http://www.w3.org/TR/xmlschema-2/#double */

  /* check for specials */
  for(i = 0; i < XSD_DOUBLE_SPECIALS_LEN; i++) {
    if(!strcmp(xsd_double_specials[i], p))
      return 1;
  }

  /* mantissa: follows http://www.w3.org/TR/xmlschema-2/#decimal */
  if(*p == '-' || *p == '+')
    p++;
  if(!*p)
    return 0;

  saved_p = p;
  while(isdigit(*p))
    p++;
  
  /* no digits is a failure */
  if(p == saved_p)
    return 0;

  /* ending now is ok - whole number (-1, +2, 3) */
  if(!*p)
    return 1;

  /* .DIGITS is optional */
  if(*p == '.') {
    p++;
    /* ending after . is not ok */
    if(!*p)
      return 0;

    while(isdigit(*p))
      p++;

    /* ending with digits now is ok (-1.2, +2.3, 2.3) */
    if(!*p)
      return 1;
  }

  /* must be an exponent letter here (-1E... +2e... , -1.3E...,
   * +2.4e..., 2E..., -4e...) */
  if(*p != 'e' && *p != 'E')
    return 0;
  p++;

  /* exponent: follows http://www.w3.org/TR/xmlschema-2/#integer */
  if(*p == '-' || *p == '+')
    p++;

  saved_p = p;
  while(isdigit(*p))
    p++;
  if(p == saved_p)
    return 0;

  if(*p)
    return 0;

  return 1;
}


/**
 * rasqal_xsd_check_float_format:
 * @string: lexical form string
 * flags: flags
 *
 * INTERNAL - Check an XSD float lexical form
 *
 * Return value: non-0 if the string is valid
 */
static int
rasqal_xsd_check_float_format(const unsigned char* string, int flags) 
{
  /* http://www.w3.org/TR/xmlschema-2/#float is the same as double */
  return rasqal_xsd_check_double_format(string, flags);
}


/**
 * rasqal_xsd_check_integer_format:
 * @string: lexical form string
 * flags: flags
 *
 * INTERNAL - Check an XSD integer lexical form
 *
 * Return value: non-0 if the string is valid
 */
static int
rasqal_xsd_check_integer_format(const unsigned char* string, int flags)
{
  /* This should be correct according to 
   * http://www.w3.org/TR/xmlschema-2/#integer
   */

  /* Forbid empty string */
  if(!*string)
    return 0;

  if(*string == '+' || *string == '-') {
    string++;
    /* Forbid "+" and "-" */
    if(!*string)
      return 0;
  }

  /* Digits */
  for(;*string; string++) {
    if(*string < '0' || *string > '9')
      return 0;
  }
  return 1;
}


/**
 * rasqal_xsd_format_integer:
 * @i: integer
 * @len_p: pointer to length of result or NULL
 *
 * INTERNAL - Format a C integer as a string in XSD decimal integer format.
 *
 * This is suitable for multiple XSD decimal integer types that are
 * xsd:integer or sub-types such as xsd:short, xsd:int, xsd:long,
 *
 * See http://www.w3.org/TR/xmlschema-2/#built-in-datatypes for the full list.
 *
 * Return value: new string or NULL on failure
 */
unsigned char*
rasqal_xsd_format_integer(int i, size_t *len_p)
{
  unsigned char* string;
  
  /* Buffer sizes need to format:
   *   4:  8 bit decimal integers (xsd:byte)  "-128" to "127"
   *   6: 16 bit decimal integers (xsd:short) "-32768" to "32767" 
   *  11: 32 bit decimal integers (xsd:int)   "-2147483648" to "2147483647"
   *  20: 64 bit decimal integers (xsd:long)  "-9223372036854775808" to "9223372036854775807"
   * (the lexical form may have leading 0s in non-canonical representations)
   */
#define INTEGER_BUFFER_SIZE 20
  string = RASQAL_MALLOC(unsigned char*, INTEGER_BUFFER_SIZE + 1);
  if(!string)
    return NULL;
  /* snprintf() takes as length the buffer size including NUL */
  snprintf(RASQAL_GOOD_CAST(char*, string), INTEGER_BUFFER_SIZE + 1, "%d", i);
  if(len_p)
    *len_p = strlen(RASQAL_GOOD_CAST(const char*, string));

  return string;
}


/**
 * rasqal_xsd_format_float:
 * @i: float
 * @len_p: pointer to length of result or NULL
 *
 * INTERNAL - Format a new an xsd:float correctly
 *
 * Return value: new string or NULL on failure
 */
unsigned char*
rasqal_xsd_format_float(float f, size_t *len_p)
{
  unsigned char* string;
  
  /* FIXME: This is big enough for C float formatted in decimal as %1g */
#define FLOAT_BUFFER_SIZE 30
  string = RASQAL_MALLOC(unsigned char*, FLOAT_BUFFER_SIZE + 1);
  if(!string)
    return NULL;
  /* snprintf() takes as length the buffer size including NUL */
  /* FIXME: %1g may not be the nearest to XSD xsd:float canonical format */
  snprintf(RASQAL_GOOD_CAST(char*, string), FLOAT_BUFFER_SIZE + 1, "%1g", (double)f);
  if(len_p)
    *len_p = strlen(RASQAL_GOOD_CAST(const char*, string));

  return string;
}


/**
 * rasqal_xsd_format_double:
 * @d: double
 * @len_p: pointer to length of result or NULL
 *
 * INTERNAL - Format a new an xsd:double correctly
 *
 * Return value: new string or NULL on failure
 */
unsigned char*
rasqal_xsd_format_double(double d, size_t *len_p)
{
  unsigned int e_index = 0;
  char have_trailing_zero = 0;
  size_t trailing_zero_start = 0;
  unsigned int exponent_start;
  size_t len = 0;
  unsigned char* buf = NULL;

  len = 20;
  buf = RASQAL_MALLOC(unsigned char*, len + 1);
  if(!buf)
    return NULL;
  
  /* snprintf needs the length + 1 because it writes a \0 too */
  snprintf(RASQAL_GOOD_CAST(char*, buf), len + 1, "%1.14E", d);

  /* find the 'e' and start of mantissa trailing zeros */

  for( ; buf[e_index]; ++e_index) {
    if(e_index > 0 && buf[e_index] == '0' && buf[e_index - 1] != '0') {
      trailing_zero_start = e_index;
      have_trailing_zero = 1;
    }
    
    else if(buf[e_index] == 'E')
      break;
  }

  if(have_trailing_zero) {
    if(buf[trailing_zero_start - 1] == '.')
      ++trailing_zero_start;

    /* write an 'E' where the trailing zeros started */
    buf[trailing_zero_start] = 'E';
    if(buf[e_index + 1] == '-') {
      buf[trailing_zero_start + 1] = '-';
      ++trailing_zero_start;
    }
  } else {
    buf[e_index] = 'E';
    trailing_zero_start = e_index + 1;
    have_trailing_zero = 1;
  }
  
  exponent_start = e_index + 2;
  while(buf[exponent_start] == '0')
    ++exponent_start;

  if(have_trailing_zero) {
    len = strlen(RASQAL_GOOD_CAST(const char*, buf));
    if(exponent_start == len) {
      len = trailing_zero_start + 2;
      buf[len - 1] = '0';
      buf[len] = '\0';
    } else {
      /* copy the exponent (minus leading zeros) after the new E */
      memmove(buf + trailing_zero_start + 1, buf + exponent_start,
              len - exponent_start + 1);
      len = strlen(RASQAL_GOOD_CAST(const char*, buf));
    }
  }
  
  if(len_p)
    *len_p = len;

  return buf;
}


typedef rasqal_literal* (*rasqal_extension_fn)(raptor_uri* name, raptor_sequence *args, char **error_p);


typedef struct {
  const unsigned char *name;
  int min_nargs;
  int max_nargs;
  rasqal_extension_fn fn;
  raptor_uri* uri;
} rasqal_xsd_datatype_fn_info;


#define XSD_INTEGER_DERIVED_COUNT 12
#define XSD_INTEGER_DERIVED_FIRST (RASQAL_LITERAL_LAST_XSD + 1)
#define XSD_INTEGER_DERIVED_LAST (RASQAL_LITERAL_LAST_XSD + XSD_INTEGER_DERIVED_COUNT - 1)

#define XSD_DATE_OFFSET (XSD_INTEGER_DERIVED_LAST + 2)

/* atomic XSD literals + 12 types derived from xsd:integer plus DATE plus a NULL */
#define SPARQL_XSD_NAMES_COUNT (RASQAL_LITERAL_LAST_XSD + 1 + XSD_INTEGER_DERIVED_COUNT + 1)


static const char* const sparql_xsd_names[SPARQL_XSD_NAMES_COUNT + 1] =
{
  NULL, /* RASQAL_LITERAL_UNKNOWN */
  NULL, /* ...BLANK */
  NULL, /* ...URI */ 
  NULL, /* ...LITERAL */
  "string",
  "boolean",
  "integer", /* may type-promote all the way to xsd:decimal */
  "float",
  "double",
  "decimal",
  "dateTime",
  /* all of the following always type-promote to xsd:integer */
  "nonPositiveInteger", "negativeInteger",
  "long", "int", "short", "byte",
  "nonNegativeInteger", "unsignedLong", "postiveInteger",
  "unsignedInt", "unsignedShort", "unsignedByte",
  /* RASQAL_LITERAL_DATE onwards (NOT next to dateTime) */
  "date",
  NULL
};

#define CHECKFNS_COUNT (RASQAL_LITERAL_LAST_XSD - RASQAL_LITERAL_FIRST_XSD + 2)
static int (*const sparql_xsd_checkfns[CHECKFNS_COUNT])(const unsigned char* string, int flags) =
{
  NULL, /* RASQAL_LITERAL_XSD_STRING */
  rasqal_xsd_check_boolean_format, /* RASQAL_LITERAL_BOOLEAN */
  rasqal_xsd_check_integer_format, /* RASQAL_LITERAL_INTEGER */
  rasqal_xsd_check_float_format, /* RASQAL_LITERAL_FLOAT */
  rasqal_xsd_check_double_format, /* RASQAL_LITERAL_DOUBLE */
  rasqal_xsd_check_decimal_format, /* RASQAL_LITERAL_DECIMAL */
  rasqal_xsd_check_dateTime_format, /* RASQAL_LITERAL_DATETIME */
  /* GAP */
  rasqal_xsd_check_date_format /* RASQAL_LITERAL_DATE */
};

#define CHECKFN_DATE_OFFSET (RASQAL_LITERAL_DATETIME - RASQAL_LITERAL_FIRST_XSD + 1)


int
rasqal_xsd_init(rasqal_world* world) 
{
  int i;

  world->xsd_namespace_uri = raptor_new_uri(world->raptor_world_ptr,
                                            raptor_xmlschema_datatypes_namespace_uri);
  if(!world->xsd_namespace_uri)
    return 1;

  world->xsd_datatype_uris = RASQAL_CALLOC(raptor_uri**, SPARQL_XSD_NAMES_COUNT + 1, sizeof(raptor_uri*));
  if(!world->xsd_datatype_uris)
    return 1;

  for(i = RASQAL_LITERAL_FIRST_XSD; i < SPARQL_XSD_NAMES_COUNT; i++) {
    const unsigned char* name = RASQAL_GOOD_CAST(const unsigned char*, sparql_xsd_names[i]);
    world->xsd_datatype_uris[i] =
      raptor_new_uri_from_uri_local_name(world->raptor_world_ptr,
                                         world->xsd_namespace_uri, name);
    if(!world->xsd_datatype_uris[i])
      return 1;
  }

  return 0;
}


void
rasqal_xsd_finish(rasqal_world* world) 
{
  if(world->xsd_datatype_uris) {
    int i;
    
    for(i = RASQAL_LITERAL_FIRST_XSD; i < SPARQL_XSD_NAMES_COUNT; i++) {
      if(world->xsd_datatype_uris[i])
        raptor_free_uri(world->xsd_datatype_uris[i]);
    }

    RASQAL_FREE(table, world->xsd_datatype_uris);
    world->xsd_datatype_uris = NULL;
  }

  if(world->xsd_namespace_uri) {
    raptor_free_uri(world->xsd_namespace_uri);
    world->xsd_namespace_uri = NULL;
  }
}
 

  
rasqal_literal_type
rasqal_xsd_datatype_uri_to_type(rasqal_world* world, raptor_uri* uri)
{
  int i;
  rasqal_literal_type native_type = RASQAL_LITERAL_UNKNOWN;
  
  if(!uri || !world->xsd_datatype_uris)
    return native_type;
  
  for(i = RASQAL_GOOD_CAST(int, RASQAL_LITERAL_FIRST_XSD); i <= RASQAL_GOOD_CAST(int, XSD_INTEGER_DERIVED_LAST); i++) {
    if(raptor_uri_equals(uri, world->xsd_datatype_uris[i])) {
      if(i >= XSD_INTEGER_DERIVED_FIRST)
        native_type = RASQAL_LITERAL_INTEGER_SUBTYPE;
      else
        native_type = (rasqal_literal_type)i;
      break;
    }
  }

  if(native_type == RASQAL_LITERAL_UNKNOWN) {
    /* DATE is not in the range FIRST_XSD .. INTEGER_DERIVED_LAST */
    i = RASQAL_GOOD_CAST(int, XSD_DATE_OFFSET);
    if(raptor_uri_equals(uri, world->xsd_datatype_uris[i]))
      native_type = RASQAL_LITERAL_DATE;
  }

  return native_type;
}


raptor_uri*
rasqal_xsd_datatype_type_to_uri(rasqal_world* world, rasqal_literal_type type)
{
  if(world->xsd_datatype_uris &&
     ((type >= RASQAL_LITERAL_FIRST_XSD &&
       type <= RASQAL_LITERAL_LAST_XSD) ||
      type == RASQAL_LITERAL_DATE))
    return world->xsd_datatype_uris[RASQAL_GOOD_CAST(int, type)];

  return NULL;
}


/**
 * rasqal_xsd_datatype_check:
 * @native_type: rasqal XSD type
 * @string: string
 * @flags: check flags
 *
 * INTERNAL - check a string as a valid lexical form of an XSD datatype
 *
 * Return value: non-0 if the string is valid
 */
int
rasqal_xsd_datatype_check(rasqal_literal_type native_type, 
                          const unsigned char* string, int flags)
{
  /* calculate check function index in sparql_xsd_checkfns table */
  int checkidx = -1;

  if(native_type >= RASQAL_GOOD_CAST(int, RASQAL_LITERAL_FIRST_XSD) &&
     native_type <= RASQAL_GOOD_CAST(int, RASQAL_LITERAL_LAST_XSD))
    checkidx = RASQAL_GOOD_CAST(int, native_type - RASQAL_LITERAL_FIRST_XSD);
  else if(native_type == RASQAL_LITERAL_DATE)
    checkidx = CHECKFN_DATE_OFFSET;

  /* test for index out of bounds and check function not defined */
  if(checkidx < 0 || !sparql_xsd_checkfns[checkidx])
    return 1;

  return sparql_xsd_checkfns[checkidx](string, flags);
}


const char*
rasqal_xsd_datatype_label(rasqal_literal_type native_type)
{
  return sparql_xsd_names[native_type];
}


int
rasqal_xsd_is_datatype_uri(rasqal_world* world, raptor_uri* uri)
{
  return (rasqal_xsd_datatype_uri_to_type(world, uri) != RASQAL_LITERAL_UNKNOWN);
}


int
rasqal_xsd_datatype_is_numeric(rasqal_literal_type type)
{
  return ((type >= RASQAL_LITERAL_BOOLEAN && type <= RASQAL_LITERAL_DECIMAL) ||
          (type == RASQAL_LITERAL_INTEGER_SUBTYPE));
}


static const rasqal_literal_type parent_xsd_type[RASQAL_LITERAL_LAST + 1] =
{
  /*   RASQAL_LITERAL_UNKNOWN  */  RASQAL_LITERAL_UNKNOWN,
  /* RDF Blank / RDF Term: Blank */
  /*   RASQAL_LITERAL_BLANK    */  RASQAL_LITERAL_UNKNOWN,
  /* RDF URI / RDF Term: URI */
  /*   RASQAL_LITERAL_URI      */  RASQAL_LITERAL_UNKNOWN,
  /* RDF Plain Literal / RDF Term: Literal */
  /*   RASQAL_LITERAL_STRING   */  RASQAL_LITERAL_UNKNOWN,
  /* XSD types / RDF Term Literal */
  /*   RASQAL_LITERAL_XSD_STRING */  RASQAL_LITERAL_UNKNOWN,
  /*   RASQAL_LITERAL_BOOLEAN  */  RASQAL_LITERAL_INTEGER,
  /*   RASQAL_LITERAL_INTEGER  */  RASQAL_LITERAL_FLOAT,
  /*   RASQAL_LITERAL_FLOAT    */  RASQAL_LITERAL_DOUBLE,
  /*   RASQAL_LITERAL_DOUBLE   */  RASQAL_LITERAL_DECIMAL,
  /*   RASQAL_LITERAL_DECIMAL  */  RASQAL_LITERAL_UNKNOWN,
  /*   RASQAL_LITERAL_DATETIME */  RASQAL_LITERAL_UNKNOWN,
  /* not datatypes */
  /*   RASQAL_LITERAL_UDT      */  RASQAL_LITERAL_UNKNOWN,
  /*   RASQAL_LITERAL_PATTERN  */  RASQAL_LITERAL_UNKNOWN,
  /*   RASQAL_LITERAL_QNAME    */  RASQAL_LITERAL_UNKNOWN,
  /*   RASQAL_LITERAL_VARIABLE */  RASQAL_LITERAL_UNKNOWN,
  /*   RASQAL_LITERAL_INTEGER_SUBTYPE */  RASQAL_LITERAL_UNKNOWN,
  /*   RASQAL_LITERAL_DATE     */  RASQAL_LITERAL_UNKNOWN
};

rasqal_literal_type
rasqal_xsd_datatype_parent_type(rasqal_literal_type type)
{
  if(type == RASQAL_LITERAL_INTEGER_SUBTYPE)
    return RASQAL_LITERAL_INTEGER;
  
  if((type >= RASQAL_LITERAL_FIRST_XSD && type <= RASQAL_LITERAL_LAST_XSD) ||
     type == RASQAL_LITERAL_DATE)
    return parent_xsd_type[type];

  return RASQAL_LITERAL_UNKNOWN;
}

#endif /* not STANDALONE */


#ifdef STANDALONE
#include <stdio.h>

int main(int argc, char *argv[]);

#define N_VALID_TESTS 27
const char *double_valid_tests[N_VALID_TESTS+1] = {
  "-INF", "INF", 
  "NaN",
  "-0", "+0", "0",
  "-12", "+12", "12",
  "-12.34", "+12.34", "12.34",
  "-1E4", "+1E4", "1267.43233E12", "-1267.43233E12", "12.78E-2",
  "-1e4", "+1e4", "1267.43233e12", "-1267.43233e12", "12.78e-2",
  "-1e0", "1e0",  "1267.43233e0",  "-1267.43233e0",  "12.78e-0",
  NULL
};

#define N_INVALID_TESTS 27
const char *double_invalid_tests[N_INVALID_TESTS+1] = {
  "-inf", "inf", "+inf", "+INF",
  "NAN", "nan",
  "-0.", "+0.", "0.",
  "-12.", "+12.", "12.",
  "-E4", "E4", "+E4",
  "-e4", "e4", "+e4",
  "-1E", "+1E", "1E",
  "-1e", "+1e", "1e",
  "-1.E", "+1.E", "1.E",
  NULL
};

int
main(int argc, char *argv[])
{
  rasqal_world* world;
  const char *program = rasqal_basename(argv[0]);
  int failures = 0;
  int test = 0;

  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    failures++;
    goto tidy;
  }

  for(test = 0; test < N_VALID_TESTS; test++) {
    const unsigned char *str;
    str = RASQAL_GOOD_CAST(const unsigned char*, double_valid_tests[test]);
    
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    fprintf(stderr, "%s: Valid Test %3d value: %s\n", program, test, str);
#endif
    if(!rasqal_xsd_check_double_format(str, 0 /* flags */)) {
      fprintf(stderr, "%s: Valid Test %3d value: %s FAILED\n", 
              program, test, str);
      failures++;
    }
  }

  for(test = 0; test < N_INVALID_TESTS; test++) {
    const unsigned char *str;
    str = RASQAL_GOOD_CAST(const unsigned char*, double_invalid_tests[test]);
    
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    fprintf(stderr, "%s: Invalid Test %3d value: %s\n", program, test, str);
#endif
    if(rasqal_xsd_check_double_format(str, 0 /* flags */)) {
      fprintf(stderr,
              "%s: Invalid Test %3d value: %s PASSED (expected failure)\n", 
              program, test, str);
      failures++;
    }
  }

  tidy:

  rasqal_free_world(world);

  return failures;
}
#endif /* STANDALONE */

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_literal.c - Rasqal literals
 *
 * $Id$
 *
 * Copyright (C) 2003-2005, David Beckett http://purl.org/net/dajobe/
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

#ifdef RASQAL_REGEX_PCRE
#include <pcre.h>
#endif

#ifdef RASQAL_REGEX_POSIX
#include <sys/types.h>
#include <regex.h>
#endif

#include "rasqal.h"
#include "rasqal_internal.h"



/**
 * rasqal_new_integer_literal - Constructor - Create a new Rasqal integer literal
 * @type: Type of literal such as RASQAL_LITERAL_INTEGER or RASQAL_LITERAL_BOOLEAN
 * @integer: int value
 * 
 * The integer decimal number is turned into a rasqal integer literal
 * and given a datatype of xsd:integer
 * 
 * Return value: New &rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_integer_literal(rasqal_literal_type type, int integer)
{
  rasqal_literal* l=(rasqal_literal*)RASQAL_CALLOC(rasqal_literal, sizeof(rasqal_literal), 1);

  l->type=type;
  l->value.integer=integer;
  l->string=(unsigned char*)RASQAL_MALLOC(cstring, 30); /* FIXME */
  sprintf((char*)l->string, "%d", integer);
  l->datatype=raptor_new_uri((const unsigned char*)"http://www.w3.org/2001/XMLSchema#integer");
  l->usage=1;
  return l;
}


/**
 * rasqal_new_floating_literal - Constructor - Create a new Rasqal floating literal
 * @f: floating literal double
 * 
 * Return value: New &rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_floating_literal(double f)
{
  rasqal_literal* l=(rasqal_literal*)RASQAL_CALLOC(rasqal_literal, sizeof(rasqal_literal), 1);

  l->type=RASQAL_LITERAL_FLOATING;
  l->value.floating=f;
  l->string=(unsigned char*)RASQAL_MALLOC(cstring, 30); /* FIXME */
  sprintf((char*)l->string, "%1g", f);
  l->datatype=raptor_new_uri((const unsigned char*)"http://www.w3.org/2001/XMLSchema#double");
  l->usage=1;
  return l;
}


/**
 * rasqal_new_uri_literal - Constructor - Create a new Rasqal URI literal from a raptor URI
 * @uri: &raptor_uri uri
 *
 * The uri is an input parameter and is stored in the literal, not copied.
 * 
 * Return value: New &rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_uri_literal(raptor_uri *uri)
{
  rasqal_literal* l=(rasqal_literal*)RASQAL_CALLOC(rasqal_literal, sizeof(rasqal_literal), 1);

  l->type=RASQAL_LITERAL_URI;
  l->value.uri=uri;
  l->usage=1;
  return l;
}


/**
 * rasqal_new_pattern_literal - Constructor - Create a new Rasqal pattern literal
 * @pattern: regex pattern
 * @flags: regex flags
 *
 * The pattern and flags are input parameters and are stored in the
 * literal, not copied.  The set of flags recognised depends
 * on the regex engine and the query language.
 * 
 * Return value: New &rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_pattern_literal(const unsigned char *pattern, 
                           const char *flags)
{
  rasqal_literal* l=(rasqal_literal*)RASQAL_CALLOC(rasqal_literal, sizeof(rasqal_literal), 1);

  l->type=RASQAL_LITERAL_PATTERN;
  l->string=pattern;
  l->flags=(const unsigned char*)flags;
  l->usage=1;
  return l;
}


/*
 * rasqal_literal_string_to_native - INTERNAL Upgrade a literal string to a datatyped literal
 * @l: &rasqal_literal to operate on inline
 *
 * At present this promotes xsd:decimal to RASQAL_LITERAL_INTEGER and
 * xsd:double to RASQAL_INTEGER_FLOATING.
 **/
void
rasqal_literal_string_to_native(rasqal_literal *l)
{
  if(!l->datatype)
    return;

  if(!strcmp((const char*)raptor_uri_as_string(l->datatype), "http://www.w3.org/2001/XMLSchema#integer")) {
    int i=atoi((const char*)l->string);

    if(l->language) {
      RASQAL_FREE(cstring, l->language);
      l->language=NULL;
    }

    l->type=RASQAL_LITERAL_INTEGER;
    l->value.integer=i;
    return;
  }
  
  if(!strcmp((const char*)raptor_uri_as_string(l->datatype), "http://www.w3.org/2001/XMLSchema#double")) {
    double d=0.0;
    sscanf((char*)l->string, "%lf", &d);

    if(l->language) {
      RASQAL_FREE(cstring, l->language);
      l->language=NULL;
    }

    l->type=RASQAL_LITERAL_FLOATING;
    l->value.floating=d;
    return;
  }
}


/**
 * rasqal_new_string_literal - Constructor - Create a new Rasqal string literal
 * @string: UTF-8 string lexical form
 * @language: RDF language (xml:lang) (or NULL)
 * @datatype: datatype URI (or NULL)
 * @datatype_qname: datatype qname string (or NULL)
 * 
 * All parameters are input parameters and if present are stored in
 * the literal, not copied.
 * 
 * The datatype and datatype_qname parameters are alternatives; the
 * qname is a datatype that cannot be resolved till later since the
 * prefixes have not yet been declared or checked.
 * 
 * If the string literal is datatyped and of certain types recognised
 * it may be converted to a different literal type by
 * rasqal_literal_string_to_native.
 *
 * Return value: New &rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_string_literal(const unsigned char *string,
                          const char *language,
                          raptor_uri *datatype, 
                          const unsigned char *datatype_qname)
{
  rasqal_literal* l=(rasqal_literal*)RASQAL_CALLOC(rasqal_literal, sizeof(rasqal_literal), 1);

  if(datatype && language) {
    RASQAL_FREE(cstring, language);
    language=NULL;
  }

  l->type=RASQAL_LITERAL_STRING;
  l->string=string;
  l->language=language;
  l->datatype=datatype;
  l->flags=datatype_qname;
  l->usage=1;

  rasqal_literal_string_to_native(l);
  return l;
}


/**
 * rasqal_new_simple_literal - Constructor - Create a new Rasqal simple literal
 * @type: RASQAL_LITERAL_BLANK or RASQAL_LITERAL_BLANK_QNAME
 * @string: the UTF-8 string value to store
 * 
 * The string is an input parameter and is stored in the
 * literal, not copied.
 * 
 * Return value: New &rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_simple_literal(rasqal_literal_type type, 
                          const unsigned char *string)
{
  rasqal_literal* l=(rasqal_literal*)RASQAL_CALLOC(rasqal_literal, sizeof(rasqal_literal), 1);

  l->type=type;
  l->string=string;
  l->usage=1;
  return l;
}


/**
 * rasqal_new_boolean_literal - Constructor - Create a new Rasqal boolean literal
 * @value: non-0 for true, 0 for false
 *
 * Return value: New &rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_boolean_literal(int value)
{
  rasqal_literal* l=(rasqal_literal*)RASQAL_CALLOC(rasqal_literal, sizeof(rasqal_literal), 1);

  l->type=RASQAL_LITERAL_BOOLEAN;
  l->value.integer=value;
  l->string=(const unsigned char*)(value ? "true":"false");
  l->usage=1;
  return l;
}


/**
 * rasqal_new_variable_literal - Constructor - Create a new Rasqal variable literal
 * @variable: &rasqal_variable to use
 * 
 * variable is an input parameter and stored in the literal, not copied.
 * 
 * Return value: New &rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_variable_literal(rasqal_variable *variable)
{
  rasqal_literal* l=(rasqal_literal*)RASQAL_CALLOC(rasqal_literal, sizeof(rasqal_literal), 1);
  l->type=RASQAL_LITERAL_VARIABLE;
  l->value.variable=variable;
  l->usage=1;
  return l;
}


/**
 * rasqal_new_literal_from_literal - Copy Constructor - create a new rasqal_literal object from an existing rasqal_literal object
 * @l: &rasqal_literal object to copy
 * 
 * Return value: a new &rasqal_literal object or NULL on failure
 **/
rasqal_literal*
rasqal_new_literal_from_literal(rasqal_literal* l)
{
  l->usage++;
  return l;
}


/**
 * rasqal_free_literal - Destructor - destroy an rasqal_literal object
 * @l: &rasqal_literal object
 * 
 **/
void
rasqal_free_literal(rasqal_literal* l)
{
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
    case RASQAL_LITERAL_FLOATING:
    case RASQAL_LITERAL_INTEGER: 
     if(l->string)
        RASQAL_FREE(cstring,l->string);
      if(l->language)
        RASQAL_FREE(cstring,l->language);
      if(l->datatype)
        raptor_free_uri(l->datatype);
      if(l->type == RASQAL_LITERAL_STRING ||
         l->type == RASQAL_LITERAL_PATTERN) {
        if(l->flags)
          RASQAL_FREE(cstring, l->flags);
      }
      break;
    case RASQAL_LITERAL_BOOLEAN:
      break;
    case RASQAL_LITERAL_VARIABLE:
      /* It is correct that this is not called here
       * since all variables are shared and owned by
       * the rasqal_query sequence variables_sequence */

      /* rasqal_free_variable(l->value.variable); */
      break;

    case RASQAL_LITERAL_UNKNOWN:
    default:
      abort();
  }
  RASQAL_FREE(rasqal_literal, l);
}


static const char* rasqal_literal_type_labels[]={
  "UNKNOWN",
  "uri",
  "qname",
  "string",
  "blank",
  "pattern",
  "boolean",
  "integer",
  "floating",
  "variable"
};


void
rasqal_literal_print_type(rasqal_literal* literal, FILE* fh)
{
  rasqal_literal_type type;

  if(!literal) {
    fputs("null", fh);
    return;
  }
  
  type=literal->type;
  if(type > RASQAL_LITERAL_LAST)
    type=RASQAL_LITERAL_UNKNOWN;
  fputs(rasqal_literal_type_labels[(int)type], fh);
}


/**
 * rasqal_literal_print - Print a Rasqal literal in a debug format
 * @l: the &rasqal_literal object
 * @fh: the &FILE* handle to print to
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

  if(l->type != RASQAL_LITERAL_VARIABLE)
    rasqal_literal_print_type(l, fh);

  switch(l->type) {
    case RASQAL_LITERAL_URI:
      fprintf(fh, "<%s>", raptor_uri_as_string(l->value.uri));
      break;
    case RASQAL_LITERAL_BLANK:
      fprintf(fh, " %s", l->string);
      break;
    case RASQAL_LITERAL_PATTERN:
      fprintf(fh, "/%s/%s", l->string, l->flags ? (const char*)l->flags : "");
      break;
    case RASQAL_LITERAL_STRING:
      fputs("(\"", fh);
      raptor_print_ntriples_string(fh, l->string, '"');
      fputc('"', fh);
      if(l->language)
        fprintf(fh, "@%s", l->language);
      if(l->datatype)
        fprintf(fh, "^^<%s>", raptor_uri_as_string(l->datatype));
      fputc(')', fh);
      break;
    case RASQAL_LITERAL_QNAME:
      fprintf(fh, "(%s)", l->string);
      break;
    case RASQAL_LITERAL_INTEGER:
      fprintf(fh, " %d", l->value.integer);
      break;
    case RASQAL_LITERAL_BOOLEAN:
      fprintf(fh, "(%s)", l->string);
      break;
    case RASQAL_LITERAL_FLOATING:
      fprintf(fh, " %g", l->value.floating);
      break;
    case RASQAL_LITERAL_VARIABLE:
      rasqal_variable_print(l->value.variable, fh);
      break;

    case RASQAL_LITERAL_UNKNOWN:
    default:
      abort();
  }
}



/*
 * rasqal_literal_as_boolean - INTERNAL Return a literal as a boolean value
 * @l: &rasqal_literal object
 * @error: pointer to error flag
 * 
 * Literals are true if not NULL (uris, strings) or zero (0, 0.0).
 * Otherwise the error flag is set.
 * 
 * Return value: non-0 if true
 **/
int
rasqal_literal_as_boolean(rasqal_literal* l, int *error)
{
  if(!l)
    return 0;
  
  switch(l->type) {
    case RASQAL_LITERAL_URI:
      return (l->value.uri) != NULL;
      break;
      
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
      return (l->string) != NULL;
      break;

    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_BOOLEAN:
      return l->value.integer != 0;
      break;

    case RASQAL_LITERAL_FLOATING:
      return l->value.floating != 0.0;
      break;

    case RASQAL_LITERAL_VARIABLE:
      return rasqal_literal_as_boolean(l->value.variable->value, error);
      break;

    case RASQAL_LITERAL_UNKNOWN:
    default:
      abort();
  }
}


/*
 * rasqal_literal_as_integer - INTERNAL Return a literal as an integer value
 * @l: &rasqal_literal object
 * @error: pointer to error flag
 * 
 * Integers, booleans and floating literals natural are turned into
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
    case RASQAL_LITERAL_BOOLEAN:
      return l->value.integer != 0;
      break;

    case RASQAL_LITERAL_FLOATING:
      return (int)l->value.floating;
      break;

    case RASQAL_LITERAL_STRING:
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
      *error=1;
      return 0;
      
    case RASQAL_LITERAL_UNKNOWN:
    default:
      abort();
  }
}


/*
 * rasqal_literal_as_floating - INTERNAL Return a literal as a floating value
 * @l: &rasqal_literal object
 * @error: pointer to error flag
 * 
 * Integers, booleans and floating literals natural are turned into
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

    case RASQAL_LITERAL_FLOATING:
      return l->value.floating;
      break;

    case RASQAL_LITERAL_STRING:
      {
        char *eptr=NULL;
        double  d=strtod((const char*)l->string, &eptr);
        if((unsigned char*)eptr != l->string && *eptr=='\0')
          return d;
      }
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
      *error=1;
      return 0.0;
      
    case RASQAL_LITERAL_UNKNOWN:
    default:
      abort();
  }
}


/*
 * rasqal_literal_as_uri - INTERNAL Return a literal as a raptor_uri*
 * @l: &rasqal_literal object
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

  abort();

  return NULL;
}


/**
 * rasqal_literal_as_string - Return the string format of a literal
 * @l: &rasqal_literal object
 * 
 * Return value: pointer to a shared string format of the literal.
 **/
const unsigned char*
rasqal_literal_as_string(rasqal_literal* l)
{
  if(!l)
    return NULL;
  
  switch(l->type) {
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_FLOATING:
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
      return l->string;

    case RASQAL_LITERAL_URI:
      return raptor_uri_as_string(l->value.uri);

    case RASQAL_LITERAL_VARIABLE:
      return rasqal_literal_as_string(l->value.variable->value);

    case RASQAL_LITERAL_UNKNOWN:
    default:
      abort();
  }
}


/**
 * rasqal_literal_as_variable - Get the variable inside a literal
 * @l: &rasqal_literal object
 * 
 * Return value: the &rasqal_variable or NULL if the literal is not a variable
 **/
rasqal_variable*
rasqal_literal_as_variable(rasqal_literal* l)
{
  return (l->type == RASQAL_LITERAL_VARIABLE) ? l->value.variable : NULL;
}


/* turn the sign of the double into an int, for comparison purposes */
static RASQAL_INLINE int
double_to_int(double d) 
{
  if(d == 0.0)
    return 0;
  return (d < 0.0) ? -1 : 1;
}


/**
 * rasqal_literal_compare - Compare two literals with type promotion
 * @l1: &rasqal_literal first literal
 * @l2: &rasqal_literal second literal
 * @flags: comparison flags
 * @error: pointer to error
 * 
 * The two literals are compared across their range.  If the types
 * are not the same, they are promoted.  If one is a floating, the
 * other is promoted to floating, otherwise for integers, otherwise
 * to strings (all literals have a string value).
 *
 * The comparison returned is as for strcmp, first before second
 * returns <0.  equal returns 0, and first after second returns >0.
 * If there is no ordering, such as for URIs, the return value
 * is 0 for equal, non-0 for different (using raptor_uri_equals).
 *
 * flags affects string comparisons and if the
 * RASQAL_COMPARE_NOCASE bit is set, a case independent
 * comparison is made.
 * 
 * Return value: <0, 0, or >0 as described above.
 **/
int
rasqal_literal_compare(rasqal_literal* l1, rasqal_literal* l2, int flags,
                       int *error)
{
  rasqal_literal *lits[2];
  int type;
  int i;
  int ints[2];
  double doubles[2];
  const unsigned char* strings[2];
  int errori=0;
  int seen_string=0;
  int seen_int=0;
  int seen_double=0;
  int seen_boolean=0;
  
  *error=0;

  /* null literals */
  if(!l1 || !l2) {
    /* if either is not null, the comparison fails */
    if(l1 || l2)
      *error=1;
    return 0;
  }

  lits[0]=l1;  lits[1]=l2;
  for(i=0; i<2; i++) {
    if(lits[i]->type == RASQAL_LITERAL_VARIABLE) {
      lits[i]=lits[i]->value.variable->value;
      RASQAL_DEBUG3("literal %d is a variable, value is a %s\n", i,
                    rasqal_literal_type_labels[lits[i]->type]);
    }
    

    switch(lits[i]->type) {
      case RASQAL_LITERAL_URI:
        break;

      case RASQAL_LITERAL_STRING:
      case RASQAL_LITERAL_BLANK:
      case RASQAL_LITERAL_PATTERN:
      case RASQAL_LITERAL_QNAME:
        strings[i]=lits[i]->string;
        seen_string=1;
      break;

      case RASQAL_LITERAL_BOOLEAN:
        seen_boolean=1;

      case RASQAL_LITERAL_INTEGER:
        ints[i]=lits[i]->value.integer;
        seen_int=1;
        break;
    
      case RASQAL_LITERAL_FLOATING:
        doubles[i]=lits[i]->value.floating;
        seen_double=1;
        break;

    case RASQAL_LITERAL_VARIABLE:
      /* this case was dealt with above, retrieving the value */

    case RASQAL_LITERAL_UNKNOWN:
      default:
        abort();
    }
  } /* end for i=0,1 */


  /* work out type to aim for */
  if(lits[0]->type != lits[1]->type) {
    RASQAL_DEBUG3("literal 0 type %s.  literal 1 type %s\n", 
                  rasqal_literal_type_labels[lits[0]->type],
                  rasqal_literal_type_labels[lits[1]->type]);

    type=seen_string ? RASQAL_LITERAL_STRING : RASQAL_LITERAL_INTEGER;
    if((seen_int & seen_double) || (seen_int & seen_string))
      type=RASQAL_LITERAL_FLOATING;
    if(seen_boolean & seen_string)
      type=RASQAL_LITERAL_STRING;
  } else
    type=lits[0]->type;
  

  /* do promotions */
  for(i=0; i<2; i++ ) {
    if(lits[i]->type == type)
      continue;
    
    switch(type) {
      case RASQAL_LITERAL_FLOATING:
        doubles[i]=rasqal_literal_as_floating(lits[i], &errori);
        /* failure always means no match */
        if(errori)
          return 1;
        RASQAL_DEBUG4("promoted literal %d (type %s) to a floating, with value %g\n", 
                      i, rasqal_literal_type_labels[lits[i]->type], doubles[i]);
        break;

      case RASQAL_LITERAL_INTEGER:
        ints[i]=rasqal_literal_as_integer(lits[i], &errori);
        /* failure always means no match */
        if(errori)
          return 1;
        RASQAL_DEBUG4("promoted literal %d (type %s) to an integer, with value %d\n", 
                      i, rasqal_literal_type_labels[lits[i]->type], ints[i]);
        break;
    
      case RASQAL_LITERAL_STRING:
       strings[i]=rasqal_literal_as_string(lits[i]);
       RASQAL_DEBUG4("promoted literal %d (type %s) to a string, with value '%s'\n", 
                     i, rasqal_literal_type_labels[lits[i]->type], strings[i]);
       break;

      case RASQAL_LITERAL_BOOLEAN:
        ints[i]=rasqal_literal_as_boolean(lits[i], &errori);
        /* failure always means no match */
        if(errori)
          return 1;
        RASQAL_DEBUG4("promoted literal %d (type %s) to a boolean, with value %d\n", 
                      i, rasqal_literal_type_labels[lits[i]->type], ints[i]);
        break;
    
      default:
        *error=1;
        return 0;
    }

  } /* check types are promoted */
  

  switch(type) {
    case RASQAL_LITERAL_URI:
      return !raptor_uri_equals(lits[0]->value.uri,lits[1]->value.uri);

    case RASQAL_LITERAL_STRING:
      if(lits[0]->language || lits[1]->language) {
        /* if either is null, the comparison fails */
        if(!lits[0]->language || !lits[1]->language)
          return 1;
        if(rasqal_strcasecmp(lits[0]->language,lits[1]->language))
          return 1;
      }

      if(lits[0]->datatype || lits[1]->datatype) {
        /* if either is null, the comparison fails */
        if(!lits[0]->datatype || !lits[1]->datatype)
          return 1;
        if(!raptor_uri_equals(lits[0]->datatype,lits[1]->datatype))
          return 1;
      }
      
      /* FALLTHROUGH */
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
      if(flags & RASQAL_COMPARE_NOCASE)
        return rasqal_strcasecmp((const char*)strings[0], (const char*)strings[1]);
      else
        return strcmp((const char*)strings[0], (const char*)strings[1]);

    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_BOOLEAN:
      return ints[1] - ints[0];
      break;

    case RASQAL_LITERAL_FLOATING:
      return double_to_int(doubles[1] - doubles[0]);
      break;

    default:
      abort();
  }
}


/**
 * rasqal_literal_equals - Compare two literals with no type promotion
 * @l1: &rasqal_literal literal
 * @l2: &rasqal_literal data literal
 * 
 * If the l2 data literal value is a boolean, it will match
 * the string "true" or "false" in the first literal l1.
 *
 * Return value: non-0 if equal
 **/
int
rasqal_literal_equals(rasqal_literal* l1, rasqal_literal* l2)
{
  /* null literals */
  if(!l1 || !l2) {
    /* if either is not null, the comparison fails */
    return (l1 || l2);
  }

  if(l1->type != l2->type) {
    if(l2->type == RASQAL_LITERAL_BOOLEAN &&
       l1->type == RASQAL_LITERAL_STRING)
      return !strcmp((const char*)l1->string, (const char*)l2->string);
    return 0;
  }
  
  switch(l1->type) {
    case RASQAL_LITERAL_URI:
      return raptor_uri_equals(l1->value.uri, l2->value.uri);

    case RASQAL_LITERAL_STRING:
      if(l1->language || l2->language) {
        /* if either is null, the comparison fails */
        if(!l1->language || !l2->language)
          return 0;
        if(rasqal_strcasecmp(l1->language,l2->language))
          return 0;
      }

      if(l1->datatype || l2->datatype) {
        /* if either is null, the comparison fails */
        if(!l1->datatype || !l2->datatype)
          return 0;
        if(!raptor_uri_equals(l1->datatype,l2->datatype))
          return 0;
      }
      
      /* FALLTHROUGH */
    case RASQAL_LITERAL_BLANK:
    case RASQAL_LITERAL_PATTERN:
    case RASQAL_LITERAL_QNAME:
      return !strcmp((const char*)l1->string, (const char*)l2->string);
      break;
      
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_BOOLEAN:
      return l1->value.integer == l2->value.integer;
      break;

    case RASQAL_LITERAL_FLOATING:
      return l1->value.floating == l2->value.floating;
      break;

    case RASQAL_LITERAL_VARIABLE:
      /* both are variables */
      return rasqal_literal_equals(l1->value.variable->value,
                                   l2->value.variable->value);
      
    case RASQAL_LITERAL_UNKNOWN:
    default:
      abort();
  }
}


/*
 * rasqal_literal_expand_qname - INTERNAL Expand any qname in a literal into a URI
 * @user_data: &rasqal_query cast as void for use with raptor_sequence_foreach
 * @l: &rasqal_literal literal
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
                                               l->string, 
                                               strlen((const char*)l->string),
                                               rasqal_query_simple_error, rq);
    if(!uri)
      return 1;
    RASQAL_FREE(cstring, l->string);
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
                                     rasqal_query_simple_error, rq);
      if(!uri)
        return 1;
      l->datatype=uri;
      RASQAL_FREE(cstring, l->flags);
      l->flags=NULL;

      if(l->language && uri) {
        RASQAL_FREE(cstring, l->language);
        l->language=NULL;
      }

      rasqal_literal_string_to_native(l);
    }
  }
  return 0;
}


/*
 * rasqal_literal_has_qname - INTERNAL Check if literal has a qname part
 * @l: &rasqal_literal literal
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
 * rasqal_literal_as_node - Turn a literal into a new RDF string, URI or blank literal
 * @l: &rasqal_literal object
 * 
 * Return value: the new &rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_literal_as_node(rasqal_literal* l)
{
  raptor_uri *dt_uri=NULL;
  rasqal_literal* new_l;
  
  switch(l->type) {
    case RASQAL_LITERAL_URI:
    case RASQAL_LITERAL_STRING:
    case RASQAL_LITERAL_BLANK:
      new_l=rasqal_new_literal_from_literal(l);
      break;
      
    case RASQAL_LITERAL_VARIABLE:
      return rasqal_new_literal_from_literal(l->value.variable->value);

    case RASQAL_LITERAL_FLOATING:
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_BOOLEAN:
      if(l->type == RASQAL_LITERAL_BOOLEAN)
        dt_uri=raptor_new_uri((const unsigned char*)"http://www.w3.org/2001/XMLSchema#boolean");
      else
        dt_uri=raptor_uri_copy(l->datatype);

      new_l=(rasqal_literal*)RASQAL_CALLOC(rasqal_literal, sizeof(rasqal_literal), 1);

      new_l->type=RASQAL_LITERAL_STRING;
      new_l->string=(unsigned char*)RASQAL_MALLOC(cstring, strlen((const char*)l->string)+1);
      strcpy((char*)new_l->string, (const char*)l->string);
      new_l->datatype=dt_uri;
      new_l->flags=NULL;
      new_l->usage=1;
      break;
      
    case RASQAL_LITERAL_QNAME:
      /* QNames should be gone by the time expression eval happens */

    case RASQAL_LITERAL_PATTERN:
      /* FALLTHROUGH */

    case RASQAL_LITERAL_UNKNOWN:
    default:
      RASQAL_FATAL2("Cannot turn literal type %d into a node", l->type);
      abort();
  }
  
  return new_l;
}

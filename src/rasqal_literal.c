/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_literal.c - Rasqal literals
 *
 * $Id$
 *
 * Copyright (C) 2003-2004 David Beckett - http://purl.org/net/dajobe/
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
#include <win32_config.h>
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
  l->string=RASQAL_MALLOC(cstring, 30); /* FIXME */
  sprintf(l->string, "%d", integer);
  l->datatype=raptor_new_uri("http://www.w3.org/2001/XMLSchema#integer");
  l->usage=1;
  return l;
}


/**
 * rasqal_new_floating_literal - Constructor - Create a new Rasqal floating literal from a string
 * @string: formatted version of floating literal
 *
 * The floating point decimal number encoded in the string is
 * turned into a rasqal floating literal (C double) and given
 * a datatype of xsd:double.
 * 
 * Return value: New &rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_floating_literal(const char *string)
{
  rasqal_literal* l=(rasqal_literal*)RASQAL_CALLOC(rasqal_literal, sizeof(rasqal_literal), 1);
  double f;

  sscanf(string, "%lf", &f);

  l->type=RASQAL_LITERAL_FLOATING;
  l->value.floating=f;
  l->string=RASQAL_MALLOC(cstring, 30); /* FIXME */
  sprintf(l->string, "%1g", f);
  l->datatype=raptor_new_uri("http://www.w3.org/2001/XMLSchema#double");
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
rasqal_new_pattern_literal(char *pattern, char *flags)
{
  rasqal_literal* l=(rasqal_literal*)RASQAL_CALLOC(rasqal_literal, sizeof(rasqal_literal), 1);

  l->type=RASQAL_LITERAL_PATTERN;
  l->string=pattern;
  l->flags=flags;
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

  if(!strcmp(raptor_uri_as_string(l->datatype), "http://www.w3.org/2001/XMLSchema#integer")) {
    int i=atoi(l->string);

    if(l->language) {
      RASQAL_FREE(cstring, l->language);
      l->language=NULL;
    }

    l->type=RASQAL_LITERAL_INTEGER;
    l->value.integer=i;
    return;
  }
  
  if(!strcmp(raptor_uri_as_string(l->datatype), "http://www.w3.org/2001/XMLSchema#double")) {
    double d=0.0;
    sscanf(l->string, "%lf", &d);

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
 * @string: string lexical form
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
rasqal_new_string_literal(char *string, char *language,
                          raptor_uri *datatype, char *datatype_qname)
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
 * @string: the string value to store
 * 
 * The string is an input parameter and is stored in the
 * literal, not copied.
 * 
 * Return value: New &rasqal_literal or NULL on failure
 **/
rasqal_literal*
rasqal_new_simple_literal(rasqal_literal_type type, char *string)
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
  l->string=value ? "true":"false";
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


static void
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
      fprintf(fh, "/%s/%s", l->string, l->flags ? l->flags : "");
      break;
    case RASQAL_LITERAL_STRING:
      fprintf(fh, "(\"%s\"", l->string);
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
        char *eptr=NULL;
        int v=(int)strtol(l->string, &eptr, 10);
        if(eptr != l->string && *eptr=='\0')
          return v;
      }
      *error=1;
      return 0;
      break;

    case RASQAL_LITERAL_VARIABLE:
      return rasqal_literal_as_integer(l->value.variable->value, error);
      break;

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
static inline double
rasqal_literal_as_floating(rasqal_literal* l, int *error)
{
  if(!l)
    return 0;
  
  switch(l->type) {
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_BOOLEAN:
      return (double)l->value.integer != 0;
      break;

    case RASQAL_LITERAL_FLOATING:
      return l->value.floating;
      break;

    case RASQAL_LITERAL_STRING:
      {
        char *eptr=NULL;
        double  d=strtod(l->string, &eptr);
        if(eptr != l->string && *eptr=='\0')
          return d;
      }
      *error=1;
      return 0;
      break;

    case RASQAL_LITERAL_VARIABLE:
      return rasqal_literal_as_integer(l->value.variable->value, error);
      break;

    default:
      abort();
  }
}


/**
 * rasqal_literal_as_string - Return the string format of a literal
 * @l: &rasqal_literal object
 * 
 * Return value: pointer to a shared string format of the literal.
 **/
char*
rasqal_literal_as_string(rasqal_literal* l)
{
  if(!l)
    return NULL;
  
  switch(l->type) {
    case RASQAL_LITERAL_BOOLEAN:
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_FLOATING:
    case RASQAL_LITERAL_STRING:
      return l->string;

    case RASQAL_LITERAL_URI:
      return raptor_uri_as_string(l->value.uri);

    case RASQAL_LITERAL_VARIABLE:
      return rasqal_literal_as_string(l->value.variable->value);

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
static inline int
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
  char* strings[2];
  int errori=0;
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
      break;

      case RASQAL_LITERAL_INTEGER:
      case RASQAL_LITERAL_BOOLEAN:
        ints[i]=lits[i]->value.integer;
        break;
    
      case RASQAL_LITERAL_FLOATING:
        doubles[i]=lits[i]->value.floating;
        break;

      default:
        abort();
    }
  } /* end for i=0,1 */

  type=lits[0]->type;
  if(type != lits[1]->type) {
    type=RASQAL_LITERAL_UNKNOWN;
    /* types differ so try to promote one term to match */

    /* if one is a floating point number, do a comparison as such */
    for(i=0; i<2; i++ )
      if(!lits[i]->type != RASQAL_LITERAL_FLOATING &&
         lits[1-i]->type == RASQAL_LITERAL_FLOATING) {
        doubles[i]=rasqal_literal_as_floating(lits[i], &errori);
        /* failure always means no match */
        if(errori)
          return 1;
        RASQAL_DEBUG4("promoted literal %d (type %s) to a floating, with value %g\n", 
                      i, rasqal_literal_type_labels[lits[i]->type], doubles[i]);
        type=lits[1-i]->type;
        break;
      }
    
    if(type == RASQAL_LITERAL_UNKNOWN)
      for(i=0; i<2; i++ )
        if(!lits[i]->type != RASQAL_LITERAL_INTEGER &&
           lits[1-i]->type == RASQAL_LITERAL_INTEGER) {
          ints[i]=rasqal_literal_as_integer(lits[i], &errori);
          /* failure always means no match */
          if(errori)
            return 1;
          RASQAL_DEBUG4("promoted literal %d (type %s) to an integer, with value %d\n", 
                        i, rasqal_literal_type_labels[lits[i]->type], ints[i]);
          type=lits[1-i]->type;
          break;
        }
    
    if(type == RASQAL_LITERAL_UNKNOWN)
      for(i=0; i<2; i++ )
        if(!lits[i]->type != RASQAL_LITERAL_STRING &&
           lits[1-i]->type == RASQAL_LITERAL_STRING) {
          strings[i]=rasqal_literal_as_string(lits[i]);
          RASQAL_DEBUG4("promoted literal %d (type %s) to a string, with value '%s'\n", 
                        i, rasqal_literal_type_labels[lits[i]->type], strings[i]);
          type=lits[1-i]->type;
          break;
        }
        
    /* otherwise cannot promote - FIXME?  or do as strings? */
    if(type==RASQAL_LITERAL_UNKNOWN) {
      *error=1;
      return 0;
    }

  } /* end if types differ */


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
        return rasqal_strcasecmp(strings[0],strings[1]);
      else
        return strcmp(strings[0],strings[1]);

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
 * If the data literal's value is a boolean, it will match
 * the string "true" or "false" in the first literal.
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
      return !strcmp(l1->string,l2->string);
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
      return !strcmp(l1->string,l2->string);
      break;
      
    case RASQAL_LITERAL_INTEGER:
    case RASQAL_LITERAL_BOOLEAN:
      return l1->value.integer == l2->value.integer;
      break;

    case RASQAL_LITERAL_FLOATING:
      return l1->value.floating == l2->value.floating;
      break;

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
                                               strlen(l->string),
                                               rasqal_query_simple_error, rq);
    if(!uri)
      return 1;
    RASQAL_FREE(cstring, l->string);
    l->type=RASQAL_LITERAL_URI;
    l->value.uri=uri; /* uri field is unioned with string field */
  } else if (l->type == RASQAL_LITERAL_STRING) {
    raptor_uri *uri;
    
    if(l->flags) {
      /* expand a literal string datatype qname */
      uri=raptor_qname_string_to_uri(rq->namespaces,
                                     l->flags, 
                                     strlen(l->flags),
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
      
    case RASQAL_LITERAL_FLOATING:
    case RASQAL_LITERAL_INTEGER:
      dt_uri=raptor_uri_copy(l->datatype);
    default:
      new_l=(rasqal_literal*)RASQAL_CALLOC(rasqal_literal, sizeof(rasqal_literal), 1);

      new_l->type=RASQAL_LITERAL_STRING;
      new_l->string=RASQAL_MALLOC(cstring, strlen(l->string)+1);
      strcpy(new_l->string, l->string);
      new_l->datatype=dt_uri;
      new_l->flags=NULL;
      new_l->usage=1;
  }
  
  return new_l;
}

/*
 * rasqal_xsd_datatypes.c - Rasqal XML Schema Datatypes support
 *
 * $Id$
 *
 * Copyright (C) 2005, David Beckett http://purl.org/net/dajobe/
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


typedef struct {
  const char *name;
  raptor_uri* uri;
} rasqal_xsd_datatype_info;


#define RASQAL_XPFO_BASE_URI "http://www.w3.org/2004/07/xpath-functions"

#define RASQAL_SPARQL_OP_NAMESPACE_URI "http://www.w3.org/2001/sw/DataAccess/operations"


#define RASQAL_XSD_DATATYPES_SIZE 7

typedef enum {
  DT_dateTime,
  DT_time,
  DT_date,
  DT_string,
  DT_numeric,
  DT_double,
  DT_integer,
} rasqal_xsd_datatype_id;


static rasqal_xsd_datatype_info rasqal_xsd_datatypes[RASQAL_XSD_DATATYPES_SIZE]={
  { "dateTime" },
  { "time" },
  { "date" },
  { "string" },
  { "numeric" },
  { "double" },
  { "integer" }
};


typedef rasqal_literal* (*rasqal_extension_fn)(raptor_uri* name, raptor_sequence *args, char **error_p);


typedef struct {
  const unsigned char *name;
  int min_nargs;
  int max_nargs;
  rasqal_extension_fn fn;
  raptor_uri* uri;
} rasqal_xsd_datatype_fn_info;



static rasqal_literal*
rasqal_xsd_datatypes_date_less_than(raptor_uri* name, raptor_sequence *args,
                                    char **error_p) {
  int error=0;
  int b;
  rasqal_literal* l1;
  rasqal_literal* l2;
  
  if(raptor_sequence_size(args) != 2)
    return NULL;
  
  l1=(rasqal_literal*)raptor_sequence_get_at(args, 0);
  l2=(rasqal_literal*)raptor_sequence_get_at(args, 1);
  
  b=(rasqal_literal_compare(l1, l2, 0, &error) < 0);
  if(error)
    return NULL;

  return rasqal_new_boolean_literal(b);
}


static rasqal_literal*
rasqal_xsd_datatypes_date_greater_than(raptor_uri* name, raptor_sequence *args,
                                       char **error_p) {
  int error=0;
  int b;
  rasqal_literal* l1;
  rasqal_literal* l2;
  
  if(raptor_sequence_size(args) != 2)
    return NULL;
  
  l1=(rasqal_literal*)raptor_sequence_get_at(args, 0);
  l2=(rasqal_literal*)raptor_sequence_get_at(args, 1);
  
  b=(rasqal_literal_compare(l1, l2, 0, &error) > 0);
  if(error)
    return NULL;

  return rasqal_new_boolean_literal(b);
}


static rasqal_literal*
rasqal_xsd_datatypes_date_equal(raptor_uri* name, raptor_sequence *args,
                                char **error_p) {
  int error=0;
  int b;
  rasqal_literal* l1;
  rasqal_literal* l2;
  
  if(raptor_sequence_size(args) != 2)
    return NULL;
  
  l1=(rasqal_literal*)raptor_sequence_get_at(args, 0);
  l2=(rasqal_literal*)raptor_sequence_get_at(args, 1);
  
  b=(rasqal_literal_compare(l1, l2, 0, &error) == 0);
  if(error)
    return NULL;

  return rasqal_new_boolean_literal(b);
}


#define RASQAL_XSD_DATATYPE_FNS_SIZE 9
static rasqal_xsd_datatype_fn_info rasqal_xsd_datatype_fns[RASQAL_XSD_DATATYPE_FNS_SIZE]={
  { "date-less-than",        1, 1, rasqal_xsd_datatypes_date_less_than },
  { "dateTime-less-than",    1, 1, rasqal_xsd_datatypes_date_less_than },
  { "time-less-than",        1, 1, rasqal_xsd_datatypes_date_less_than },
  { "date-greater-than",     1, 1, rasqal_xsd_datatypes_date_greater_than },
  { "dateTime-greater-than", 1, 1, rasqal_xsd_datatypes_date_greater_than },
  { "time-greater-than",     1, 1, rasqal_xsd_datatypes_date_greater_than },
  { "date-equal",            1, 1, rasqal_xsd_datatypes_date_equal },
  { "dateTime-equal",        1, 1, rasqal_xsd_datatypes_date_equal },
  { "time-equal",            1, 1, rasqal_xsd_datatypes_date_equal }
};



static raptor_uri* raptor_xpfo_base_uri=NULL;
static raptor_uri* rasqal_sparql_op_namespace_uri=NULL;


static void
rasqal_init_datatypes(void) {
  int i;
  
  raptor_xpfo_base_uri=raptor_new_uri(RASQAL_XPFO_BASE_URI);
  rasqal_sparql_op_namespace_uri=raptor_new_uri(RASQAL_SPARQL_OP_NAMESPACE_URI);

  for(i=0; i< RASQAL_XSD_DATATYPES_SIZE; i++) {
    rasqal_xsd_datatypes[i].uri=raptor_new_uri_from_uri_local_name(raptor_xpfo_base_uri,
                                                               rasqal_xsd_datatypes[i].name);
  }

  for(i=0; i< RASQAL_XSD_DATATYPE_FNS_SIZE; i++) {
    rasqal_xsd_datatype_fns[i].uri=raptor_new_uri_from_uri_local_name(rasqal_sparql_op_namespace_uri,
                                                                  rasqal_xsd_datatype_fns[i].name);
  }

}


static void
rasqal_finish_datatypes(void) {
  int i;
  
  for(i=0; i< RASQAL_XSD_DATATYPES_SIZE; i++)
    if(rasqal_xsd_datatypes[i].uri)
      raptor_free_uri(rasqal_xsd_datatypes[i].uri);

  for(i=0; i< RASQAL_XSD_DATATYPE_FNS_SIZE; i++)
    if(rasqal_xsd_datatype_fns[i].uri)
      raptor_free_uri(rasqal_xsd_datatype_fns[i].uri);

  if(raptor_xpfo_base_uri)
    raptor_free_uri(raptor_xpfo_base_uri);

  if(rasqal_sparql_op_namespace_uri)
    raptor_free_uri(rasqal_sparql_op_namespace_uri);
}


/*
 * 
 * Facets
 * 
 * Ordered
 * [Definition:] A value space, and hence a datatype, is said to be
 * ordered if there exists an order-relation defined for that
 * value space.
 * -- http://www.w3.org/TR/xmlschema-2/#dt-ordered
 * 
 * Bounded
 * [Definition:] A datatype is bounded if its value space has either
 * an inclusive upper bound or an exclusive upper bound and either
 * an inclusive lower bound or an exclusive lower bound.
 * -- http://www.w3.org/TR/xmlschema-2/#dt-bounded
 * 
 * Cardinality
 * [Definition:] Every value space has associated with it the concept
 * of cardinality. Some value spaces are finite, some are countably
 * infinite while still others could conceivably be uncountably infinite
 * (although no value space defined by this specification is
 * uncountable infinite). A datatype is said to have the cardinality of
 * its value space.
 * -- http://www.w3.org/TR/xmlschema-2/#dt-cardinality
 * 
 * Numeric
 * [Definition:] A datatype is said to be numeric if its values are
 * conceptually quantities (in some mathematical number system).
 * -- http://www.w3.org/TR/xmlschema-2/#dt-numeric
 */



/*
 * Types: dateTime, date, time
 *   http://www.w3.org/TR/xmlschema-2/#dateTime
 *   http://www.w3.org/TR/xmlschema-2/#date
 *   http://www.w3.org/TR/xmlschema-2/#time
 * all (partial ordered, bounded, countably infinite, not numeric)
 * 
 * Functions (all operators)
 * op:date-equal, op:date-less-than, op:date-greater-than
 *
 * ??? dateTime equiv???
 * op:dateTime-equal, op:dateTime-less-than, op:dateTime-greater-than
 *
 * ??? time equiv???
 * op:time-equal, op:time-less-than, op:time-greater-than
 */

typedef struct
{
  /* dateTime and date */
  int year;
  unsigned int month         :4;  /* 1..12 (4 bits)  */
  unsigned int day           :5;  /* 1..31 (5 bits)  */

  /* dateTime and time */
  unsigned int hour          :5;  /* 0..24 (5 bits) */
  unsigned int minute        :6;  /* 0..59 (6 bits) */
  double       second;

  /* optional (when have_timezone=1) dateTime, date, time */
  unsigned int have_timezone :1;  /* boolean (1 bit) */
  int          timezone      :11; /* +/-14 hours in minutes (-14*60..14*60) */
} rasqal_xsd_datetime;



/* 
 * Type: string
 * (not ordered, not bounded, countably infinite, not numeric)
 * 
 * fn:contains
 *   Indicates whether one xs:string contains another xs:string. A
 *   collation may be specified.
 *
 * fn:starts-with
 *   Indicates whether the value of one xs:string begins with the
 *   collation units of another xs:string. A collation may be
 *   specified.
 *
 * fn:ends-with
 *   Indicates whether the value of one xs:string ends with the
 *   collation units of another xs:string. A collation may be
 *   specified.
 *
 * fn:substring-before
 *   Returns the collation units of one xs:string that precede in
 *   that xs:string the collation units of another xs:string. A
 *   collation may be specified.
 *
 * fn:substring-after
 *   Returns the collation units of xs:string that follow in that
 *   xs:string the collation units of another xs:string. A collation
 *   may be specified.
 *
 * fn:string-length
 *   Returns the length of the argument.
 *
 * fn:upper-case
 *   Returns the upper-cased value of the argument.
 *
 * fn:lower-case
 *   Returns the lower-cased value of the argument.
 *
 * fn:matches (input, pattern)
 *   fn:matches (input, pattern, flags)
 *
 *   Returns an xs:boolean value that indicates whether the
 *   value of the first argument is matched by the regular expression that
 *   is the value of the second argument.
 *
 *   flags = string of s,m,i,x char combinations ("" when omitted)
 *
 *   Regular expressions: Perl5 syntax as defined in "Functions and
 *   Operators".
 *
 *  http://www.w3.org/TR/xpath-functions/#func-contains
 *  http://www.w3.org/TR/xpath-functions/#func-starts-with
 *  http://www.w3.org/TR/xpath-functions/#func-ends-with
 *  http://www.w3.org/TR/xpath-functions/#func-substring-before
 *  http://www.w3.org/TR/xpath-functions/#func-substring-after
 *  http://www.w3.org/TR/xpath-functions/#func-string-length
 *  http://www.w3.org/TR/xpath-functions/#func-upper-case
 *  http://www.w3.org/TR/xpath-functions/#func-lower-case
 *  http://www.w3.org/TR/xpath-functions/#func-matches
 *
 * ??? no equality comparison fn:compare???
 *  fn:compare($comparand1 as xs:string, $comparand2 as xs:string) as xs:integer
 *  fn:compare($comparand1 as xs:string, $comparand2 as xs:string,
 *             $collation as xs:string) as xs:integer
 * [[This function, invoked with the first signature, backs up the
 * "eq", "ne", "gt", "lt", "le" and "ge" operators on string
 * values.]]
 *
 */

typedef struct
{
  unsigned char *string;
  size_t length;
} rasqal_xsd_string;


/*
 * Type: double
 *   (partial ordered, bounded, countably infinite, numeric)
 * 
 * Type: decimal
 *   (total ordered, not bounded, countably infinite, numeric)
 *
 * Derived Type: integer (derived from decimal)
 *   (total ordered, not bounded, countably infinite, numeric)
 * 
 * Functions:
 * 1 arguments
 *   op:numeric-unary-plus
 *   op:numeric-unary-minus
 *   http://www.w3.org/TR/xpath-functions/#func-numeric-unary-plus
 *   http://www.w3.org/TR/xpath-functions/#func-numeric-unary-minus
 *
 * 2 arguments
 *   op:numeric-equal
 *   op:numeric-less-than
 *   op:numeric-greater-than
 *   op:numeric-add
 *   op:numeric-subtract
 *   op:numeric-multiply
 *   op:numeric-divide
 *   http://www.w3.org/TR/xpath-functions/#func-numeric-equal
 *   http://www.w3.org/TR/xpath-functions/#func-numeric-less-than
 *   http://www.w3.org/TR/xpath-functions/#func-numeric-greater-than
 *   http://www.w3.org/TR/xpath-functions/#func-numeric-add
 *   http://www.w3.org/TR/xpath-functions/#func-numeric-subtract
 *   http://www.w3.org/TR/xpath-functions/#func-numeric-multiply
 *   http://www.w3.org/TR/xpath-functions/#func-numeric-divide
 *
 * [[The parameters and return types for the above operators are the
 * basic numeric types: xs:integer, xs:decimal, xs:float and
 * xs:double, and types derived from them.  The word "numeric" in
 * function signatures signifies these four types. For simplicity,
 * each operator is defined to operate on operands of the same type
 * and to return the same type. The exceptions are op:numeric-divide,
 * which returns an xs:decimal if called with two xs:integer operands
 * and op:numeric-integer-divide which always returns an xs:integer.]]
 * -- http://www.w3.org/TR/xpath-functions/#op.numeric
 *
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
 * [[A function that expects a parameter $p of type xs:decimal can be
 * invoked with a value of type xs:integer. This is an example of
 * subtype substitution. The value retains its original type. Within
 * the body of the function, $p instance of xs:integer returns
 * true.]]
 *
 *
 * B.2 Operator Mapping
 * http://www.w3.org/TR/xpath20/#mapping
 *
 * [[When referring to a type, the term numeric denotes the types
 * xs:integer, xs:decimal, xs:float, and xs:double]]
 *
 * [[If the result type of an operator is listed as numeric, it means
 * "the first type in the ordered list (xs:integer, xs:decimal,
 * xs:float, xs:double) into which all operands can be converted by
 * subtype substitution and numeric type promotion."]]
 * 
 */



#ifdef STANDALONE
#include <stdio.h>

int main(int argc, char *argv[]);


int
main(int argc, char *argv[]) {
  raptor_uri *xsd_uri;
  raptor_uri *dateTime_uri;
  rasqal_literal *l1, *l2;
  int fn_i;
  raptor_uri* fn_uri;
  const unsigned char *fn_name;
  rasqal_extension_fn fn;
  raptor_sequence *fn_args;
  char *error;
  rasqal_literal *fn_result;


  raptor_init();

  xsd_uri=raptor_new_uri(raptor_xmlschema_datatypes_namespace_uri);
  dateTime_uri=raptor_new_uri_from_uri_local_name(xsd_uri, "dateTime");

  rasqal_init_datatypes();

  fn_args=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_literal, (raptor_sequence_print_handler*)rasqal_literal_print);
  l1=rasqal_new_string_literal(strdup("2004-05-04"), NULL, raptor_uri_copy(dateTime_uri), NULL);
  raptor_sequence_push(fn_args, l1);
  l2=rasqal_new_string_literal(strdup("2003-01-02"), NULL, raptor_uri_copy(dateTime_uri), NULL);
  raptor_sequence_push(fn_args, l2);
  
  fn_i=0;
  fn_name=rasqal_xsd_datatype_fns[fn_i].name;
  fn=rasqal_xsd_datatype_fns[fn_i].fn;
  fn_uri=rasqal_xsd_datatype_fns[fn_i].uri;

  error=NULL;
  fn_result=fn(fn_uri, fn_args, &error);
  raptor_free_sequence(fn_args);

  if(!fn_result) {
    if(error)
      fprintf(stderr, "function %s failed with error %s\n", fn_name, error);
    else
      fprintf(stderr, "function %s unknown error\n", fn_name);
  } else {
    fprintf(stderr, "function %s returned result: ", fn_name);
    rasqal_literal_print(fn_result, stderr);
    fputc('\n', stderr);
  }
  

  if(fn_result) 
    rasqal_free_literal(fn_result);

  rasqal_finish_datatypes();
  
  raptor_free_uri(xsd_uri);
  raptor_free_uri(dateTime_uri);

  raptor_finish();

  return 0;
}
#endif

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal.h - Rasqal RDF Query library interfaces and definition
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
 */



#ifndef RASQAL_H
#define RASQAL_H


#ifdef __cplusplus
extern "C" {
#endif


#ifdef WIN32
#  ifdef RASQAL_INTERNAL
#    define RASQAL_API _declspec(dllexport)
#  else
#    define RASQAL_API _declspec(dllimport)
#  endif
#else
#  define RASQAL_API
#endif

/* Use gcc 3.1+ feature to allow marking of deprecated API calls.
 * This gives a warning during compiling.
 */
#if ( __GNUC__ == 3 && __GNUC_MINOR__ > 0 ) || __GNUC__ > 3
#ifdef __APPLE_CC__
/* OSX gcc cpp-precomp is broken */
#define RASQAL_DEPRECATED
#else
#define RASQAL_DEPRECATED __attribute__((deprecated))
#endif
#else
#define RASQAL_DEPRECATED
#endif


#ifndef LIBRDF_OBJC_FRAMEWORK
#include <raptor.h>
#else
#include <Redland/raptor.h>
#endif

/* Public statics */
RASQAL_API
extern const char * const rasqal_short_copyright_string;
RASQAL_API
extern const char * const rasqal_copyright_string;
RASQAL_API
extern const char * const rasqal_version_string;
RASQAL_API
extern const unsigned int rasqal_version_major;
RASQAL_API
extern const unsigned int rasqal_version_minor;
RASQAL_API
extern const unsigned int rasqal_version_release;
RASQAL_API
extern const unsigned int rasqal_version_decimal;


/* Public structures */
typedef struct rasqal_query_s rasqal_query;
typedef struct rasqal_query_results_s rasqal_query_results;
typedef struct rasqal_literal_s rasqal_literal;
typedef struct rasqal_graph_pattern_s rasqal_graph_pattern;

/**
 * rasqal_feature:
 *
 * Query features.  None currently defined.
 */
typedef enum {
  RASQAL_FEATURE_LAST
} rasqal_feature;


/**
 * rasqal_prefix:
 *
 * Rasqal namespace (prefix, uri) pair.  Also includes flags
 * for making when they are declared and at what XML element depth
 * when used in XML formats.
 */
typedef struct {
  const unsigned char *prefix;
  raptor_uri* uri;
  int declared;
  int depth;
} rasqal_prefix;


/**
 * rasqal_variable_type:
 *
 * Rasqal variable types.  NORMAL is the regular variable,
 * ANONYMOUS can be used in queries but cannot be used in SELECT or
 * returned in a result.
 */
typedef enum {
  RASQAL_VARIABLE_TYPE_UNKNOWN   = 0,
  RASQAL_VARIABLE_TYPE_NORMAL    = 1,
  RASQAL_VARIABLE_TYPE_ANONYMOUS = 2
} rasqal_variable_type;


/**
 * rasqal_variable:
 *
 * Rasqal binding between a variable name and a #rasqal_literal value.
 * of a type #rasqal_variable_type.
 *
 * Also includes internal flags for recording the offset into the
 * (internal) variables array.
 */
typedef struct {
  const unsigned char *name;
  struct rasqal_literal_s *value;
  int offset;   /* offset in the rasqal_query variables array */
  rasqal_variable_type type;     /* variable type */
} rasqal_variable;


/**
 * rasqal_data_graph_flags:
 *
 * Flags for the type of #rasqal_data_graph as used by
 * rasqal_query_add_data_graph().  NAMED graphs make use of the graph
 * name URI, BACKGROUND graphs do not.  See #rasqal_data_graph.
 */
typedef enum {
  RASQAL_DATA_GRAPH_NONE  = 0,
  RASQAL_DATA_GRAPH_NAMED = 1,
  RASQAL_DATA_GRAPH_BACKGROUND = 2,
} rasqal_data_graph_flags;


/**
 * rasqal_data_graph:
 *
 * A source of RDF data for querying.  The URI is the original source
 * (base URI) of the content.  It may also have an additional name
 * @name_uri as long as the flags are RASQAL_DATA_NAMED.
 */
typedef struct {
  raptor_uri* uri;
  raptor_uri* name_uri;
  int flags;
} rasqal_data_graph;


/**
 * rasqal_literal_type:
 *
 * Rasqal literal types
 *
 * The order in the following enumeration is significant as it encodes
 * the SPARQL term ordering conditions:
 *   Blank Nodes << IRIS << RDF literals << typed literals
 * which coresponds to in enum values
 *   BLANK << URI << STRING << 
 *     (BOOLEAN | INTEGER | DOUBLE | FLOAT | DECIMAL | DATETIME)
 *     (RASQAL_LITERAL_FIRST_XSD ... RASQAL_LITERAL_LAST_XSD)
 * Not used (internal): PATTERN, QNAME, VARIABLE
 *
 * See rasqal_literal_compare() when used with flags
 * RASQAL_COMPARE_XQUERY.
 */
typedef enum {
  RASQAL_LITERAL_UNKNOWN,
  RASQAL_LITERAL_BLANK,    /* r:bNode RDF blank node */
  RASQAL_LITERAL_URI,      /* r:URI */
  RASQAL_LITERAL_STRING,   /* r:Literal RDF literal (includes xsd:string ) */
  RASQAL_LITERAL_BOOLEAN,  /* xsd:boolean */
  RASQAL_LITERAL_INTEGER,  /* xsd:integer */
  RASQAL_LITERAL_DOUBLE,   /* xsd:double  */
  RASQAL_LITERAL_FLOATING = RASQAL_LITERAL_DOUBLE,
  RASQAL_LITERAL_FLOAT,    /* xsd:float    */
  RASQAL_LITERAL_DECIMAL,  /* xsd:decimal  */
  RASQAL_LITERAL_DATETIME, /* xsd:dateTime */
  RASQAL_LITERAL_FIRST_XSD = RASQAL_LITERAL_BOOLEAN,
  RASQAL_LITERAL_LAST_XSD = RASQAL_LITERAL_DATETIME,
  RASQAL_LITERAL_PATTERN,
  RASQAL_LITERAL_QNAME,
  RASQAL_LITERAL_VARIABLE,
  RASQAL_LITERAL_LAST= RASQAL_LITERAL_VARIABLE
} rasqal_literal_type;

struct rasqal_literal_s {
  int usage;
  rasqal_literal_type type;
  /* UTF-8 string, pattern, qname, blank, double, float, decimal, datetime */
  const unsigned char *string;
  unsigned int string_len;
  
  union {
    /* integer and boolean types */
    int integer;
    /* double and float */
    double floating;
    /* uri (can be temporarily NULL if a qname, see flags below) */
    raptor_uri* uri;
    /* variable */
    rasqal_variable* variable;
  } value;

  /* for string */
  const char *language;
  raptor_uri *datatype;

  /* various flags for literal types:
   *  pattern  regex flags
   *  string   datatype of qname
   *  uri      qname of URI not yet expanded (temporary)
   */
  const unsigned char *flags;
};


/**
 * rasqal_op:
 *
 * Rasqal expression operators.  A mixture of unary, binary and
 * tertiary operators (string matches).  Also includes casting and
 * two ordering operators from ORDER BY in SPARQL.
 */
typedef enum {
  RASQAL_EXPR_UNKNOWN,
  RASQAL_EXPR_AND,
  RASQAL_EXPR_OR,
  RASQAL_EXPR_EQ,
  RASQAL_EXPR_NEQ,
  RASQAL_EXPR_LT,
  RASQAL_EXPR_GT,
  RASQAL_EXPR_LE,
  RASQAL_EXPR_GE,
  RASQAL_EXPR_UMINUS,
  RASQAL_EXPR_PLUS,
  RASQAL_EXPR_MINUS,
  RASQAL_EXPR_STAR,
  RASQAL_EXPR_SLASH,
  RASQAL_EXPR_REM,
  RASQAL_EXPR_STR_EQ,
  RASQAL_EXPR_STR_NEQ,
  RASQAL_EXPR_STR_MATCH,
  RASQAL_EXPR_STR_NMATCH,
  RASQAL_EXPR_TILDE,
  RASQAL_EXPR_BANG,
  RASQAL_EXPR_LITERAL,
  RASQAL_EXPR_FUNCTION,
  RASQAL_EXPR_BOUND,
  RASQAL_EXPR_STR,
  RASQAL_EXPR_LANG,
  RASQAL_EXPR_DATATYPE,
  RASQAL_EXPR_ISURI,
  RASQAL_EXPR_ISBLANK,
  RASQAL_EXPR_ISLITERAL,
  RASQAL_EXPR_CAST,
  RASQAL_EXPR_ORDER_COND_ASC,
  RASQAL_EXPR_ORDER_COND_DESC,
  RASQAL_EXPR_LAST= RASQAL_EXPR_ORDER_COND_DESC
} rasqal_op;


struct rasqal_variable_s;

/**
 * rasqal_expression:
 *
 * expression (arg1), unary op (arg1), binary op (arg1,arg2),
 * literal or variable 
 */
struct rasqal_expression_s {
  rasqal_op op;
  struct rasqal_expression_s* arg1;
  struct rasqal_expression_s* arg2;
  rasqal_literal* literal;
  rasqal_variable* variable;
  unsigned char *value; /* UTF-8 value */

  /* for extension function qname(args...) and cast-to-uri */
  raptor_uri* name;
  raptor_sequence* args;
};
typedef struct rasqal_expression_s rasqal_expression;


/**
 * rasqal_triple_flags:
 *
 * Flags for triple patterns.
 */
typedef enum {

  /* Not used - was only used internally in the execution engine */
  RASQAL_TRIPLE_FLAGS_EXACT=1,

  /* Not used - this is now a property of a graph pattern */
  RASQAL_TRIPLE_FLAGS_OPTIONAL=2,

  RASQAL_TRIPLE_FLAGS_LAST=RASQAL_TRIPLE_FLAGS_OPTIONAL
} rasqal_triple_flags;


/**
 * rasqal_triple:
 *
 * A triple pattern or RDF triple.
 *
 * This is used as a triple pattern in queries and
 * an RDF triple when generating RDF triples such as with SPARQL CONSTRUCT.
 */
typedef struct {
  rasqal_literal* subject;
  rasqal_literal* predicate;
  rasqal_literal* object;
  rasqal_literal* origin;
  unsigned int flags; /* | of enum rasqal_triple_flags bits */
} rasqal_triple;


/*
 * rasqal_pattern_flags:
 *
 * Flags for #rasqal_graph_pattern.
 */
typedef enum {
  /* true when the graph pattern is an optional match */
  RASQAL_PATTERN_FLAGS_OPTIONAL=1,

  RASQAL_PATTERN_FLAGS_LAST=RASQAL_PATTERN_FLAGS_OPTIONAL
} rasqal_pattern_flags;


typedef unsigned char* (*rasqal_generate_bnodeid_handler)(rasqal_query* query, void *user_data, unsigned char *user_bnodeid);


/*
 * rasqal_query_verb:
 *
 * Query verbs.
 */
typedef enum {
  RASQAL_QUERY_VERB_UNKNOWN   = 0,
  RASQAL_QUERY_VERB_SELECT    = 1,
  RASQAL_QUERY_VERB_CONSTRUCT = 2,
  RASQAL_QUERY_VERB_DESCRIBE  = 3,
  RASQAL_QUERY_VERB_ASK       = 4,

  RASQAL_QUERY_VERB_LAST=RASQAL_QUERY_VERB_ASK
} rasqal_query_verb;


/* Graph pattern operators  */
typedef enum {
  RASQAL_GRAPH_PATTERN_OPERATOR_UNKNOWN   = 0,
  /* Basic - just triple patterns and constraints */
  RASQAL_GRAPH_PATTERN_OPERATOR_BASIC     = 1,
  /* Optional - set of graph patterns (ANDed) and constraints */
  RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL  = 2,
  /* Union - set of graph patterns (UNIONed) and constraints */
  RASQAL_GRAPH_PATTERN_OPERATOR_UNION     = 3,
  /* Group - set of graph patterns (ANDed) and constraints */
  RASQAL_GRAPH_PATTERN_OPERATOR_GROUP     = 4,
  /* Graph - a graph term + a graph pattern and constraints */
  RASQAL_GRAPH_PATTERN_OPERATOR_GRAPH     = 5,

  RASQAL_GRAPH_PATTERN_OPERATOR_LAST=RASQAL_GRAPH_PATTERN_OPERATOR_GRAPH
} rasqal_graph_pattern_operator;


/* RASQAL API */

/* Public functions */

RASQAL_API
void rasqal_init(void);
RASQAL_API
void rasqal_finish(void);


RASQAL_API
int rasqal_languages_enumerate(const unsigned int counter, const char **name, const char **label, const unsigned char **uri_string);
RASQAL_API
int rasqal_language_name_check(const char *name);


/* Query class */

/* Create */
RASQAL_API
rasqal_query* rasqal_new_query(const char *name, const unsigned char *uri);
/* Destroy */
RASQAL_API
void rasqal_free_query(rasqal_query* query);

/* Methods */
RASQAL_API
const char* rasqal_query_get_name(rasqal_query* query);
RASQAL_API
const char* rasqal_query_get_label(rasqal_query* query);
RASQAL_API
void rasqal_query_set_fatal_error_handler(rasqal_query* query, void *user_data, raptor_message_handler handler);
RASQAL_API
void rasqal_query_set_error_handler(rasqal_query* query, void *user_data, raptor_message_handler handler);
RASQAL_API
void rasqal_query_set_warning_handler(rasqal_query* query, void *user_data, raptor_message_handler handler);
RASQAL_API
void rasqal_query_set_feature(rasqal_query *query, rasqal_feature feature, int value);
RASQAL_API
void rasqal_query_set_default_generate_bnodeid_parameters(rasqal_query* rdf_query, char *prefix, int base);
RASQAL_API
void rasqal_query_set_generate_bnodeid_handler(rasqal_query* query, void *user_data, rasqal_generate_bnodeid_handler handler);

RASQAL_API
rasqal_query_verb rasqal_query_get_verb(rasqal_query *query);
RASQAL_API
int rasqal_query_get_wildcard(rasqal_query *query);
RASQAL_API
int rasqal_query_get_distinct(rasqal_query *query);
RASQAL_API
void rasqal_query_set_distinct(rasqal_query *query, int is_distinct);
RASQAL_API
int rasqal_query_get_limit(rasqal_query *query);
RASQAL_API
void rasqal_query_set_limit(rasqal_query *query, int limit);
RASQAL_API
int rasqal_query_get_offset(rasqal_query *query);
RASQAL_API
void rasqal_query_set_offset(rasqal_query *query, int limit);

RASQAL_API
int rasqal_query_add_data_graph(rasqal_query* query, raptor_uri* uri, raptor_uri* name_uri, int flags);
RASQAL_API
raptor_sequence* rasqal_query_get_data_graph_sequence(rasqal_query* query);
RASQAL_API
rasqal_data_graph* rasqal_query_get_data_graph(rasqal_query* query, int idx);

RASQAL_API
void rasqal_query_add_variable(rasqal_query* query, rasqal_variable* var);
RASQAL_API
raptor_sequence* rasqal_query_get_bound_variable_sequence(rasqal_query* query);
RASQAL_API
raptor_sequence* rasqal_query_get_all_variable_sequence(rasqal_query* query);
RASQAL_API
rasqal_variable* rasqal_query_get_variable(rasqal_query* query, int idx);
RASQAL_API
int rasqal_query_has_variable(rasqal_query* query, const unsigned char *name);
RASQAL_API
int rasqal_query_set_variable(rasqal_query* query, const unsigned char *name, rasqal_literal* value);
RASQAL_API
raptor_sequence* rasqal_query_get_triple_sequence(rasqal_query* query);
RASQAL_API
rasqal_triple* rasqal_query_get_triple(rasqal_query* query, int idx);
RASQAL_API
void rasqal_query_add_prefix(rasqal_query* query, rasqal_prefix* prefix);
RASQAL_API
raptor_sequence* rasqal_query_get_prefix_sequence(rasqal_query* query);
RASQAL_API
rasqal_prefix* rasqal_query_get_prefix(rasqal_query* query, int idx);
RASQAL_API
raptor_sequence* rasqal_query_get_order_conditions_sequence(rasqal_query* query);
RASQAL_API
rasqal_expression* rasqal_query_get_order_condition(rasqal_query* query, int idx);

/* graph patterns */
RASQAL_API
rasqal_graph_pattern* rasqal_query_get_query_graph_pattern(rasqal_query* query);
RASQAL_API
raptor_sequence* rasqal_query_get_graph_pattern_sequence(rasqal_query* query);
RASQAL_API
rasqal_graph_pattern* rasqal_query_get_graph_pattern(rasqal_query* query, int idx);
RASQAL_API
void rasqal_graph_pattern_add_sub_graph_pattern(rasqal_graph_pattern* graph_pattern, rasqal_graph_pattern* sub_graph_pattern);
RASQAL_API
rasqal_triple* rasqal_graph_pattern_get_triple(rasqal_graph_pattern* graph_pattern, int idx);
RASQAL_API
raptor_sequence* rasqal_graph_pattern_get_sub_graph_pattern_sequence(rasqal_graph_pattern* graph_pattern);
RASQAL_API
rasqal_graph_pattern* rasqal_graph_pattern_get_sub_graph_pattern(rasqal_graph_pattern* graph_pattern, int idx);
RASQAL_API RASQAL_DEPRECATED
int rasqal_graph_pattern_get_flags(rasqal_graph_pattern* graph_pattern);
RASQAL_API
rasqal_graph_pattern_operator rasqal_graph_pattern_get_operator(rasqal_graph_pattern* graph_pattern);
RASQAL_API
const char* rasqal_graph_pattern_operator_as_string(rasqal_graph_pattern_operator verb);
RASQAL_API
void rasqal_graph_pattern_print(rasqal_graph_pattern* gp, FILE* fh);
RASQAL_API
int rasqal_graph_pattern_add_constraint(rasqal_graph_pattern* gp, rasqal_expression* expr);
RASQAL_API
raptor_sequence* rasqal_graph_pattern_get_constraint_sequence(rasqal_graph_pattern* gp);
RASQAL_API
rasqal_expression* rasqal_graph_pattern_get_constraint(rasqal_graph_pattern* gp, int idx);
RASQAL_API
raptor_sequence* rasqal_query_get_construct_triples_sequence(rasqal_query* query);
RASQAL_API
rasqal_triple* rasqal_query_get_construct_triple(rasqal_query* query, int idx);

/* Utility methods */
RASQAL_API
const char* rasqal_query_verb_as_string(rasqal_query_verb verb);
RASQAL_API
void rasqal_query_print(rasqal_query* query, FILE *stream);

/* Query */
RASQAL_API
int rasqal_query_prepare(rasqal_query* query, const unsigned char *query_string, raptor_uri *base_uri);
RASQAL_API
rasqal_query_results* rasqal_query_execute(rasqal_query* query);

RASQAL_API
void* rasqal_query_get_user_data(rasqal_query *query);
RASQAL_API
void rasqal_query_set_user_data(rasqal_query *query, void *user_data);

/* query results */
RASQAL_API
void rasqal_free_query_results(rasqal_query_results *query_results);

/* Bindings result format */
RASQAL_API
int rasqal_query_results_is_bindings(rasqal_query_results *query_results);
RASQAL_API
int rasqal_query_results_get_count(rasqal_query_results *query_results);
RASQAL_API
int rasqal_query_results_next(rasqal_query_results *query_results);
RASQAL_API
int rasqal_query_results_finished(rasqal_query_results *query_results);
RASQAL_API
int rasqal_query_results_get_bindings(rasqal_query_results *query_results, const unsigned char ***names, rasqal_literal ***values);
RASQAL_API
rasqal_literal* rasqal_query_results_get_binding_value(rasqal_query_results *query_results, int offset);
RASQAL_API
const unsigned char* rasqal_query_results_get_binding_name(rasqal_query_results *query_results, int offset);
RASQAL_API
rasqal_literal* rasqal_query_results_get_binding_value_by_name(rasqal_query_results *query_results, const unsigned char *name);
RASQAL_API
int rasqal_query_results_get_bindings_count(rasqal_query_results *query_results);

/* Boolean result format */
RASQAL_API
int rasqal_query_results_is_boolean(rasqal_query_results *query_results);
RASQAL_API
int rasqal_query_results_get_boolean(rasqal_query_results *query_results);

/* Graph result format */
RASQAL_API
int rasqal_query_results_is_graph(rasqal_query_results *query_results);
RASQAL_API
raptor_statement* rasqal_query_results_get_triple(rasqal_query_results *query_results);
RASQAL_API
int rasqal_query_results_next_triple(rasqal_query_results *query_results);

RASQAL_API
int rasqal_query_results_write(raptor_iostream *iostr, rasqal_query_results *results, raptor_uri *format_uri, raptor_uri *base_uri);

/* Data graph class */
RASQAL_API
rasqal_data_graph* rasqal_new_data_graph(raptor_uri* uri, raptor_uri* name_uri, int flags);
RASQAL_API
void rasqal_free_data_graph(rasqal_data_graph* dg);
RASQAL_API
void rasqal_data_graph_print(rasqal_data_graph* dg, FILE* fh);


/* Flags for rasqal_expression_evaluate or rasqal_literal_compare */
#define RASQAL_COMPARE_NOCASE 1
#define RASQAL_COMPARE_XQUERY 2

/* Expression class */
RASQAL_API
rasqal_expression* rasqal_new_1op_expression(rasqal_op op, rasqal_expression* arg);
RASQAL_API
rasqal_expression* rasqal_new_2op_expression(rasqal_op op, rasqal_expression* arg1, rasqal_expression* arg2);
RASQAL_API
rasqal_expression* rasqal_new_string_op_expression(rasqal_op op, rasqal_expression* arg1, rasqal_literal* literal);
RASQAL_API
rasqal_expression* rasqal_new_literal_expression(rasqal_literal* literal);
RASQAL_API
rasqal_expression* rasqal_new_variable_expression(rasqal_variable *variable);
RASQAL_API
rasqal_expression* rasqal_new_function_expression(raptor_uri* name, raptor_sequence* args);
RASQAL_API
rasqal_expression* rasqal_new_cast_expression(raptor_uri* name, rasqal_expression *value);

RASQAL_API
void rasqal_free_expression(rasqal_expression* expr);
RASQAL_API
void rasqal_expression_print_op(rasqal_expression* expr, FILE* fh);
RASQAL_API
void rasqal_expression_print(rasqal_expression* expr, FILE* fh);
RASQAL_API
rasqal_literal* rasqal_expression_evaluate(rasqal_query *query, rasqal_expression* expr, int flags);
typedef int (*rasqal_expression_foreach_fn)(void *user_data, rasqal_expression *e);
RASQAL_API
int rasqal_expression_foreach(rasqal_expression* expr, rasqal_expression_foreach_fn fn, void *user_data);

/* Literal class */
RASQAL_API
rasqal_literal* rasqal_new_integer_literal(rasqal_literal_type type, int integer);
RASQAL_API RASQAL_DEPRECATED
rasqal_literal* rasqal_new_floating_literal(double f);
RASQAL_API
rasqal_literal* rasqal_new_double_literal(double f);
RASQAL_API
rasqal_literal* rasqal_new_uri_literal(raptor_uri* uri);
RASQAL_API
rasqal_literal* rasqal_new_pattern_literal(const unsigned char *pattern, const char *flags);
RASQAL_API
rasqal_literal* rasqal_new_string_literal(const unsigned char *string, const char *language, raptor_uri *datatype, const unsigned char *datatype_qname);
RASQAL_API
rasqal_literal* rasqal_new_simple_literal(rasqal_literal_type type, const unsigned char *string);
RASQAL_API
rasqal_literal* rasqal_new_boolean_literal(int value);
RASQAL_API
rasqal_literal* rasqal_new_variable_literal(rasqal_variable *variable);

RASQAL_API
rasqal_literal* rasqal_new_literal_from_literal(rasqal_literal* literal);
RASQAL_API
void rasqal_free_literal(rasqal_literal* literal);
RASQAL_API
void rasqal_literal_print(rasqal_literal* literal, FILE* fh);
RASQAL_API
void rasqal_literal_print_type(rasqal_literal* literal, FILE* fh);
RASQAL_API
rasqal_variable* rasqal_literal_as_variable(rasqal_literal* literal);
RASQAL_API
const unsigned char* rasqal_literal_as_string(rasqal_literal* literal);
RASQAL_API
rasqal_literal* rasqal_literal_as_node(rasqal_literal* literal);

RASQAL_API
int rasqal_literal_compare(rasqal_literal* l1, rasqal_literal* l2, int flags, int *error);
RASQAL_API
int rasqal_literal_equals(rasqal_literal* l1, rasqal_literal* l2);

RASQAL_API
rasqal_prefix* rasqal_new_prefix(const unsigned char* prefix, raptor_uri* uri);
RASQAL_API
void rasqal_free_prefix(rasqal_prefix* prefix);
RASQAL_API
void rasqal_prefix_print(rasqal_prefix* p, FILE* fh);

/* Triple class */
RASQAL_API
rasqal_triple* rasqal_new_triple(rasqal_literal* subject, rasqal_literal* predicate, rasqal_literal* object);
RASQAL_API
rasqal_triple* rasqal_new_triple_from_triple(rasqal_triple* t);
RASQAL_API
void rasqal_free_triple(rasqal_triple* t);
RASQAL_API
void rasqal_triple_print(rasqal_triple* t, FILE* fh);
RASQAL_API
void rasqal_triple_set_origin(rasqal_triple* t, rasqal_literal *l);
RASQAL_API
rasqal_literal* rasqal_triple_get_origin(rasqal_triple* t);

/* Variable class */
RASQAL_API
rasqal_variable* rasqal_new_variable_typed(rasqal_query* rq, rasqal_variable_type type, unsigned char *name, rasqal_literal *value);
RASQAL_API
rasqal_variable* rasqal_new_variable(rasqal_query* query, unsigned char *name, rasqal_literal *value);
RASQAL_API
void rasqal_free_variable(rasqal_variable* variable);
RASQAL_API
void rasqal_variable_print(rasqal_variable* t, FILE* fh);
RASQAL_API
void rasqal_variable_set_value(rasqal_variable* v, rasqal_literal *l);

/* memory functions */
RASQAL_API
void rasqal_free_memory(void *ptr);
RASQAL_API
void* rasqal_alloc_memory(size_t size);
RASQAL_API
void* rasqal_calloc_memory(size_t nmemb, size_t size);

/* rasqal_engine.c */

typedef enum {
  RASQAL_TRIPLE_SUBJECT  = 1,
  RASQAL_TRIPLE_PREDICATE= 2,
  RASQAL_TRIPLE_OBJECT   = 4,
  RASQAL_TRIPLE_ORIGIN   = 8
} rasqal_triple_parts;

struct rasqal_triples_match_s {
  void *user_data;

  /* the [4]array (s,p,o,origin) bindings against the current triple match
   * only touching triple parts given.
   * returns parts that were bound or 0 on failure
   */
  rasqal_triple_parts (*bind_match)(struct rasqal_triples_match_s*, void *user_data, rasqal_variable *bindings[4], rasqal_triple_parts parts);

  /* move to next match */
  void (*next_match)(struct rasqal_triples_match_s*, void *user_data);

  /* check for end of triple match - return non-0 if is end */
  int (*is_end)(struct rasqal_triples_match_s*, void *user_data);

  /* finish triples match and destroy any allocated memory */
  void (*finish)(struct rasqal_triples_match_s*, void *user_data);
};
typedef struct rasqal_triples_match_s rasqal_triples_match;


typedef struct 
{
  /* triple (subject, predicate, object) and origin */
  rasqal_variable* bindings[4];

  rasqal_triples_match *triples_match;

  void *context;

  /* parts of the triple*/
  rasqal_triple_parts parts;

  /* non-0 if all parts of the triple are given */
  int is_exact;
} rasqal_triple_meta;


struct rasqal_triples_source_s {
  /* A source for this query */
  rasqal_query *query;

  void *user_data;

  /* the triples_source_factory initialises these method */
  int (*init_triples_match)(rasqal_triples_match* rtm, struct rasqal_triples_source_s* rts, void *user_data, rasqal_triple_meta *m, rasqal_triple *t);

  int (*triple_present)(struct rasqal_triples_source_s* rts, void *user_data, rasqal_triple *t);

  void (*free_triples_source)(void *user_data);
};
typedef struct rasqal_triples_source_s rasqal_triples_source;


/**
 * rasqal_triples_source_factory:
 *
 * A factory that initialises #rasqal_triples_source structures
 * to returning matches to a triple pattern.
 */
typedef struct {
  void *user_data; /* user data for triples_source_factory */
  size_t user_data_size; /* size of user data for new_triples_source */

  /*
   * create a new triples source - returns non-zero on failure
   * < 0 is a 'no rdf data error', > 0 is an unspecified error
   */
  int (*new_triples_source)(rasqal_query *query, void *factory_user_data, void *user_data, rasqal_triples_source* rts);
} rasqal_triples_source_factory;
  

/* set the triples_source_factory */
RASQAL_API
void rasqal_set_triples_source_factory(void (*register_fn)(rasqal_triples_source_factory *factory), void* user_data);

#ifdef __cplusplus
}
#endif

#endif

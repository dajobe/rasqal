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



#ifndef RDQL_H
#define RDQL_H


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

#ifndef LIBRDF_OBJC_FRAMEWORK
#include <raptor.h>
#else
#include <Redland/raptor.h>
#endif

/* Public statics */
extern const char * const rasqal_short_copyright_string;
extern const char * const rasqal_copyright_string;
extern const char * const rasqal_version_string;
extern const unsigned int rasqal_version_major;
extern const unsigned int rasqal_version_minor;
extern const unsigned int rasqal_version_release;
extern const unsigned int rasqal_version_decimal;


/* Public structure */
typedef struct rasqal_query_s rasqal_query;
typedef struct rasqal_query_results_s rasqal_query_results;
typedef struct rasqal_literal_s rasqal_literal;

typedef enum {
  RASQAL_FEATURE_LAST
} rasqal_feature;

typedef struct {
  const unsigned char *prefix;
  raptor_uri* uri;
  int declared;
  int depth;
} rasqal_prefix;


/* variable binding */
typedef struct {
  const unsigned char *name;
  struct rasqal_literal_s *value;
  int offset;   /* offset in the rasqal_query variables array */
} rasqal_variable;


typedef enum {
  RASQAL_LITERAL_UNKNOWN,
  RASQAL_LITERAL_URI,
  RASQAL_LITERAL_QNAME,
  RASQAL_LITERAL_STRING,
  RASQAL_LITERAL_BLANK,
  RASQAL_LITERAL_PATTERN,
  RASQAL_LITERAL_BOOLEAN,
  RASQAL_LITERAL_INTEGER,
  RASQAL_LITERAL_FLOATING,
  RASQAL_LITERAL_VARIABLE,
  RASQAL_LITERAL_LAST= RASQAL_LITERAL_VARIABLE
} rasqal_literal_type;

struct rasqal_literal_s {
  int usage;
  rasqal_literal_type type;
  /* UTF-8 string, pattern, qname, blank, float types */
  const unsigned char *string;
  union {
    /* integer and boolean types */
    int integer;
    /* floating */
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
  RASQAL_EXPR_PATTERN,
  RASQAL_EXPR_FUNCTION,
  RASQAL_EXPR_LAST= RASQAL_EXPR_FUNCTION
} rasqal_op;


struct rasqal_variable_s;

/* expression (arg1), unary op (arg1), binary op (arg1,arg2),
 * literal or variable 
*/
struct rasqal_expression_s {
  rasqal_op op;
  struct rasqal_expression_s* arg1;
  struct rasqal_expression_s* arg2;
  rasqal_literal* literal;
  rasqal_variable* variable;
  unsigned char *value; /* UTF-8 value */

  /* for extension function qname(args...) */
  raptor_uri* name;
  raptor_sequence* args;
};
typedef struct rasqal_expression_s rasqal_expression;


/* Extra flags for triples */
typedef enum {
  /* true when all of subject, predicate, object are given */
  RASQAL_TRIPLE_FLAGS_EXACT=1,
  /* true when the triple is an optional match */
  RASQAL_TRIPLE_FLAGS_OPTIONAL=2,
  RASQAL_TRIPLE_FLAGS_LAST=RASQAL_TRIPLE_FLAGS_OPTIONAL
} rasqal_triple_flags;


/* an RDF triple or a triple pattern */
typedef struct {
  rasqal_literal* subject;
  rasqal_literal* predicate;
  rasqal_literal* object;
  rasqal_literal* origin;
  unsigned int flags; /* | of enum rasqal_triple_flags bits */
} rasqal_triple ;



/* RASQAL API */

/* Public functions */

RASQAL_API void rasqal_init(void);
RASQAL_API void rasqal_finish(void);


RASQAL_API int rasqal_languages_enumerate(const unsigned int counter, const char **name, const char **label, const unsigned char **uri_string);
RASQAL_API int rasqal_language_name_check(const char *name);


/* Query class */

/* Create */
RASQAL_API rasqal_query* rasqal_new_query(const char *name, const unsigned char *uri);
/* Destroy */
RASQAL_API void rasqal_free_query(rasqal_query* query);

/* Methods */
RASQAL_API const char* rasqal_query_get_name(rasqal_query* query);
RASQAL_API const char* rasqal_query_get_label(rasqal_query* query);
RASQAL_API void rasqal_query_set_fatal_error_handler(rasqal_query* query, void *user_data, raptor_message_handler handler);
RASQAL_API void rasqal_query_set_error_handler(rasqal_query* query, void *user_data, raptor_message_handler handler);
RASQAL_API void rasqal_query_set_warning_handler(rasqal_query* query, void *user_data, raptor_message_handler handler);
RASQAL_API void rasqal_query_set_feature(rasqal_query *query, rasqal_feature feature, int value);

RASQAL_API void rasqal_query_add_source(rasqal_query* query, raptor_uri* uri);
RASQAL_API raptor_sequence* rasqal_query_get_source_sequence(rasqal_query* query);
RASQAL_API raptor_uri* rasqal_query_get_source(rasqal_query* query, int idx);
RASQAL_API void rasqal_query_add_variable(rasqal_query* query, rasqal_variable* var);
RASQAL_API raptor_sequence* rasqal_query_get_variable_sequence(rasqal_query* query);
RASQAL_API rasqal_variable* rasqal_query_get_variable(rasqal_query* query, int idx);
RASQAL_API int rasqal_query_has_variable(rasqal_query* query, const unsigned char *name);
RASQAL_API int rasqal_query_set_variable(rasqal_query* query, const unsigned char *name, rasqal_literal* value);
RASQAL_API void rasqal_query_add_triple(rasqal_query* query, rasqal_triple* triple);
RASQAL_API raptor_sequence* rasqal_query_get_triple_sequence(rasqal_query* query);
RASQAL_API rasqal_triple* rasqal_query_get_triple(rasqal_query* query, int idx);
RASQAL_API void rasqal_query_add_constraint(rasqal_query* query, rasqal_expression* expr);
RASQAL_API raptor_sequence* rasqal_query_get_constraint_sequence(rasqal_query* query);
RASQAL_API rasqal_expression* rasqal_query_get_constraint(rasqal_query* query, int idx);
RASQAL_API void rasqal_query_add_prefix(rasqal_query* query, rasqal_prefix* prefix);
RASQAL_API raptor_sequence* rasqal_query_get_prefix_sequence(rasqal_query* query);
RASQAL_API rasqal_prefix* rasqal_query_get_prefix(rasqal_query* query, int idx);

/* Utility methods */
RASQAL_API void rasqal_query_print(rasqal_query* query, FILE *stream);

/* Query */
RASQAL_API int rasqal_query_prepare(rasqal_query* query, const unsigned char *query_string, raptor_uri *base_uri);
RASQAL_API rasqal_query_results* rasqal_query_execute(rasqal_query* query);

RASQAL_API void* rasqal_query_get_user_data(rasqal_query *query);
RASQAL_API void rasqal_query_set_user_data(rasqal_query *query, void *user_data);

/* query results */
RASQAL_API void rasqal_free_query_results(rasqal_query_results *query_results);

/* Bindings result format */
RASQAL_API int rasqal_query_results_is_bindings(rasqal_query_results *query_results);
RASQAL_API int rasqal_query_results_get_count(rasqal_query_results *query_results);
RASQAL_API int rasqal_query_results_next(rasqal_query_results *query_results);
RASQAL_API int rasqal_query_results_finished(rasqal_query_results *query_results);
RASQAL_API int rasqal_query_results_get_bindings(rasqal_query_results *query_results, const unsigned char ***names, rasqal_literal ***values);
RASQAL_API rasqal_literal* rasqal_query_results_get_binding_value(rasqal_query_results *query_results, int offset);
RASQAL_API const unsigned char* rasqal_query_results_get_binding_name(rasqal_query_results *query_results, int offset);
RASQAL_API rasqal_literal* rasqal_query_results_get_binding_value_by_name(rasqal_query_results *query_results, const unsigned char *name);
RASQAL_API int rasqal_query_results_get_bindings_count(rasqal_query_results *query_results);

/* Boolean result format */
RASQAL_API int rasqal_query_results_is_boolean(rasqal_query_results *query_results);
RASQAL_API int rasqal_query_results_get_boolean(rasqal_query_results *query_results);

/* Graph result format */
RASQAL_API int rasqal_query_results_is_graph(rasqal_query_results *query_results);
RASQAL_API raptor_statement* rasqal_query_results_get_triple(rasqal_query_results *query_results);
RASQAL_API int rasqal_query_results_next_triple(rasqal_query_results *query_results);

RAPTOR_API int rasqal_query_results_write(raptor_iostream *iostr, rasqal_query_results *results, raptor_uri *format_uri, raptor_uri *base_uri);

/* Expression class */
RASQAL_API rasqal_expression* rasqal_new_1op_expression(rasqal_op op, rasqal_expression* arg);
RASQAL_API rasqal_expression* rasqal_new_2op_expression(rasqal_op op, rasqal_expression* arg1, rasqal_expression* arg2);
RASQAL_API rasqal_expression* rasqal_new_string_op_expression(rasqal_op op, rasqal_expression* arg1, rasqal_literal* literal);
RASQAL_API rasqal_expression* rasqal_new_literal_expression(rasqal_literal* literal);
RASQAL_API rasqal_expression* rasqal_new_variable_expression(rasqal_variable *variable);
RASQAL_API rasqal_expression* rasqal_new_function_expression(raptor_uri* name, raptor_sequence* args);

RASQAL_API void rasqal_free_expression(rasqal_expression* expr);
RASQAL_API void rasqal_expression_print_op(rasqal_expression* expr, FILE* fh);
RASQAL_API void rasqal_expression_print(rasqal_expression* expr, FILE* fh);
RASQAL_API rasqal_literal* rasqal_expression_evaluate(rasqal_query *query, rasqal_expression* expr);
typedef int (*rasqal_expression_foreach_fn)(void *user_data, rasqal_expression *e);
RASQAL_API int rasqal_expression_foreach(rasqal_expression* expr, rasqal_expression_foreach_fn fn, void *user_data);

/* Literal class */
RASQAL_API rasqal_literal* rasqal_new_integer_literal(rasqal_literal_type type, int integer);
RASQAL_API rasqal_literal* rasqal_new_floating_literal(double f);
RASQAL_API rasqal_literal* rasqal_new_uri_literal(raptor_uri* uri);
RASQAL_API rasqal_literal* rasqal_new_pattern_literal(const unsigned char *pattern, const char *flags);
RASQAL_API rasqal_literal* rasqal_new_string_literal(const unsigned char *string, const char *language, raptor_uri *datatype, const unsigned char *datatype_qname);
RASQAL_API rasqal_literal* rasqal_new_simple_literal(rasqal_literal_type type, const unsigned char *string);
RASQAL_API rasqal_literal* rasqal_new_boolean_literal(int value);
RASQAL_API rasqal_literal* rasqal_new_variable_literal(rasqal_variable *variable);

RASQAL_API rasqal_literal* rasqal_new_literal_from_literal(rasqal_literal* literal);
RASQAL_API void rasqal_free_literal(rasqal_literal* literal);
RASQAL_API void rasqal_literal_print(rasqal_literal* literal, FILE* fh);
RASQAL_API void rasqal_literal_print_type(rasqal_literal* literal, FILE* fh);
RASQAL_API rasqal_variable* rasqal_literal_as_variable(rasqal_literal* literal);
RASQAL_API const unsigned char* rasqal_literal_as_string(rasqal_literal* literal);
RASQAL_API rasqal_literal* rasqal_literal_as_node(rasqal_literal* literal);

#define RASQAL_COMPARE_NOCASE 1
RASQAL_API int rasqal_literal_compare(rasqal_literal* l1, rasqal_literal* l2, int flags, int *error);
RASQAL_API int rasqal_literal_equals(rasqal_literal* l1, rasqal_literal* l2);

RASQAL_API rasqal_prefix* rasqal_new_prefix(const unsigned char* prefix, raptor_uri* uri);
RASQAL_API void rasqal_free_prefix(rasqal_prefix* prefix);
RASQAL_API void rasqal_prefix_print(rasqal_prefix* p, FILE* fh);

/* Triple class */
RASQAL_API rasqal_triple* rasqal_new_triple(rasqal_literal* subject, rasqal_literal* predicate, rasqal_literal* object);
RASQAL_API rasqal_triple* rasqal_new_triple_from_triple(rasqal_triple* t);
RASQAL_API void rasqal_free_triple(rasqal_triple* t);
RASQAL_API void rasqal_triple_print(rasqal_triple* t, FILE* fh);
RASQAL_API void rasqal_triple_set_origin(rasqal_triple* t, rasqal_literal *l);
RASQAL_API rasqal_literal* rasqal_triple_get_origin(rasqal_triple* t);
RASQAL_API void rasqal_triple_set_flags(rasqal_triple* t, unsigned int flags);
RASQAL_API unsigned int rasqal_triple_get_flags(rasqal_triple* t);

/* Variable class */
RASQAL_API rasqal_variable* rasqal_new_variable(rasqal_query* query, const unsigned char *name, rasqal_literal *value);
RASQAL_API void rasqal_free_variable(rasqal_variable* variable);
RASQAL_API void rasqal_variable_print(rasqal_variable* t, FILE* fh);
RASQAL_API void rasqal_variable_set_value(rasqal_variable* v, rasqal_literal *l);

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
} rasqal_triple_meta;


struct rasqal_triples_source_s {
  /* A source for this query */
  rasqal_query *query;

  void *user_data;

  /* the triples_source_factory initialises these method */
  rasqal_triples_match* (*new_triples_match)(struct rasqal_triples_source_s* rts, void *user_data, rasqal_triple_meta *m, rasqal_triple *t);

  int (*triple_present)(struct rasqal_triples_source_s* rts, void *user_data, rasqal_triple *t);

  void (*free_triples_source)(void *user_data);
};
typedef struct rasqal_triples_source_s rasqal_triples_source;


typedef struct {
  void *user_data; /* user data for triples_source_factory */
  size_t user_data_size; /* size of user data for new_triples_source */
  int (*new_triples_source)(rasqal_query *query, void *factory_user_data, void *user_data, rasqal_triples_source* rts);
} rasqal_triples_source_factory;
  

/* set the triples_source_factory */
RASQAL_API void rasqal_set_triples_source_factory(void (*register_fn)(rasqal_triples_source_factory *factory), void* user_data);

#ifdef __cplusplus
}
#endif

#endif

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rdql.h - Rasqal RDF Query library interfaces and definition
 *
 * $Id$
 *
 * Copyright (C) 2003 David Beckett - http://purl.org/net/dajobe/
 * Institute for Learning and Research Technology - http://www.ilrt.org/
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

#include <raptor.h>

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

typedef struct rasqal_sequence_s rasqal_sequence;

typedef enum {
  RASQAL_FEATURE_LAST
} rasqal_feature;

typedef struct {
  const char *prefix;
  raptor_uri *uri;
} rasqal_prefix ;


/* variable binding */
typedef struct {
  const char *name;
  struct rasqal_expression_s *value;
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
  RASQAL_LITERAL_NULL,
  RASQAL_LITERAL_INTEGER,
  RASQAL_LITERAL_FLOATING,
  RASQAL_LITERAL_LAST= RASQAL_LITERAL_FLOATING
} rasqal_literal_type;

typedef struct {
  rasqal_literal_type type;
  union {
    char *string;
    int integer;
    float floating;
    raptor_uri *uri;
  } value;
} rasqal_literal ;


typedef enum {
  RASQAL_EXPR_UNKNOWN,
  RASQAL_EXPR_EXPR,
  RASQAL_EXPR_AND,
  RASQAL_EXPR_OR,
  RASQAL_EXPR_EQ,
  RASQAL_EXPR_NEQ,
  RASQAL_EXPR_LT,
  RASQAL_EXPR_GT,
  RASQAL_EXPR_LE,
  RASQAL_EXPR_GE,
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
  RASQAL_EXPR_VARIABLE,
  RASQAL_EXPR_PATTERN,
  RASQAL_EXPR_LAST= RASQAL_EXPR_PATTERN
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
  char *value;
};
typedef struct rasqal_expression_s rasqal_expression;




/* three expressions */
typedef struct {
  rasqal_expression* subject;
  rasqal_expression* predicate;
  rasqal_expression* object;
} rasqal_triple ;



/* RASQAL API */

/* Public functions */

RASQAL_API void rasqal_init(void);
RASQAL_API void rasqal_finish(void);


RASQAL_API int rasqal_languages_enumerate(const unsigned int counter, const char **name, const char **label, const unsigned char **uri_string);

RASQAL_API int rasqal_language_name_check(const char *name);
RASQAL_API const char* rasqal_get_name(rasqal_query *rdf_query);
RASQAL_API const char* rasqal_get_label(rasqal_query *rdf_query);

RASQAL_API void rasqal_set_fatal_error_handler(rasqal_query* query, void *user_data, raptor_message_handler handler);
RASQAL_API void rasqal_set_error_handler(rasqal_query* query, void *user_data, raptor_message_handler handler);
RASQAL_API void rasqal_set_warning_handler(rasqal_query* query, void *user_data, raptor_message_handler handler);

RASQAL_API void rasqal_set_feature(rasqal_query *query, rasqal_feature feature, int value);

/* Query class */

/* Create */
RASQAL_API rasqal_query* rasqal_new_query(const char *name, const unsigned char *uri);
/* Destroy */
RASQAL_API void rasqal_free_query(rasqal_query* query);

/* Methods */
RASQAL_API void rasqal_query_add_source(rasqal_query* query, const unsigned char* uri);
RASQAL_API rasqal_sequence* rasqal_query_get_source_sequence(rasqal_query* query);
RASQAL_API const unsigned char* rasqal_query_get_source(rasqal_query* query, int idx);
RASQAL_API int rasqal_query_has_variable(rasqal_query* query, const char *name);
RASQAL_API int rasqal_query_set_variable(rasqal_query* query, const char *name, rasqal_expression* value);

RASQAL_API int rasqal_parse_query(rasqal_query *query, const unsigned char *uri_string, const char *query_string, size_t len);

/* Utility methods */
RASQAL_API void rasqal_query_print(rasqal_query*, FILE *stream);

/* Query */
RASQAL_API int rasqal_query_prepare(rasqal_query *rdf_query, const unsigned char *query_string, raptor_uri *base_uri);
RASQAL_API int rasqal_query_execute(rasqal_query *rdf_query);




/* Sequence class */

typedef void* (rasqal_free_handler(void*));
typedef void (rasqal_print_handler(void *object, FILE *fh));

/* Create */
RASQAL_API rasqal_sequence* rasqal_new_sequence(rasqal_free_handler* free_handler, rasqal_print_handler* print_handler);
/* Destroy */
RASQAL_API void rasqal_free_sequence(rasqal_sequence* seq);
/* Methods */
RASQAL_API int rasqal_sequence_size(rasqal_sequence* seq);
RASQAL_API int rasqal_sequence_set_at(rasqal_sequence* seq, int idx, void *data);
RASQAL_API int rasqal_sequence_push(rasqal_sequence* seq, void *data);
RASQAL_API int rasqal_sequence_shift(rasqal_sequence* seq, void *data);
RASQAL_API void* rasqal_sequence_get_at(rasqal_sequence* seq, int idx);
RASQAL_API void* rasqal_sequence_pop(rasqal_sequence* seq);
RASQAL_API void* rasqal_sequence_unshift(rasqal_sequence* seq);

RASQAL_API int rasqal_compare_strings(const void *a, const void *b);

RASQAL_API void rasqal_sequence_sort(rasqal_sequence* seq, int(*compare)(const void *, const void *));

/* helper for printing sequences of strings */ 
RASQAL_API void rasqal_sequence_print_string(char *data, FILE *fh);
RASQAL_API void rasqal_sequence_print_uri(char *data, FILE *fh);
RASQAL_API void rasqal_sequence_print(rasqal_sequence* seq, FILE* fh);

/* Expression class */
RASQAL_API rasqal_expression* rasqal_new_1op_expression(rasqal_op op, rasqal_expression* arg);
RASQAL_API rasqal_expression* rasqal_new_2op_expression(rasqal_op op, rasqal_expression* arg1, rasqal_expression* arg2);
RASQAL_API rasqal_expression* rasqal_new_string_op_expression(rasqal_op op, rasqal_expression* arg1, rasqal_literal* literal);
RASQAL_API rasqal_expression* rasqal_new_literal_expression(rasqal_literal *literal);
RASQAL_API rasqal_expression* rasqal_new_variable_expression(rasqal_variable *variable);

RASQAL_API void rasqal_free_expression(rasqal_expression* e);
RASQAL_API void rasqal_expression_print_op(rasqal_expression* expression, FILE* fh);
RASQAL_API void rasqal_expression_print(rasqal_expression* e, FILE* fh);
RASQAL_API int rasqal_expression_is_variable(rasqal_expression* e);
RASQAL_API rasqal_variable* rasqal_expression_as_variable(rasqal_expression* e);
RASQAL_API rasqal_literal* rasqal_expression_evaluate(rasqal_expression* e);

/* Literal class */
RASQAL_API rasqal_literal* rasqal_new_literal(rasqal_literal_type type, int integer, float floating, char *string, raptor_uri *uri);
RASQAL_API void rasqal_free_literal(rasqal_literal* l);
RASQAL_API void rasqal_literal_print(rasqal_literal* literal, FILE* fh);
RASQAL_API int rasqal_literal_as_boolean(rasqal_literal* literal);

RASQAL_API rasqal_prefix* rasqal_new_prefix(const char *prefix, raptor_uri *uri);
RASQAL_API void rasqal_free_prefix(rasqal_prefix* prefix);
RASQAL_API void rasqal_prefix_print(rasqal_prefix* p, FILE* fh);

/* Triple class */
RASQAL_API rasqal_triple* rasqal_new_triple(rasqal_expression* subject, rasqal_expression* predicate, rasqal_expression* object);
RASQAL_API void rasqal_free_triple(rasqal_triple* t);
RASQAL_API void rasqal_triple_print(rasqal_triple* t, FILE* fh);

/* Variable class */
RASQAL_API rasqal_variable* rasqal_new_variable(rasqal_query* rdf_query, const char *name, rasqal_expression *value);
RASQAL_API void rasqal_free_variable(rasqal_variable* variable);
RASQAL_API void rasqal_variable_print(rasqal_variable* t, FILE* fh);
RASQAL_API void rasqal_variable_set_value(rasqal_variable* v, rasqal_expression *e);

/* rasqal_engine.c */

#ifdef __cplusplus
}
#endif

#endif

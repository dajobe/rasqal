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

/* Public structure */
typedef struct rasqal_query_s rasqal_query;

typedef struct rasqal_sequence_s rasqal_sequence;


typedef struct {
  const char *prefix;
  const char *uri;
} rasqal_prefix ;


typedef struct {
  const char *name;
  const char *value;
} rasqal_variable ;


typedef enum {
  RASQAL_LITERAL_UNKNOWN,
  RASQAL_LITERAL_URI,
  RASQAL_LITERAL_STRING,
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
  } value;
} rasqal_literal ;


typedef enum {
  RASQAL_EXPR_UNKNOWN,
  RASQAL_EXPR_EXPR,
  RASQAL_EXPR_AND,
  RASQAL_EXPR_OR,
  RASQAL_EXPR_BIT_AND,
  RASQAL_EXPR_BIT_OR,
  RASQAL_EXPR_BIT_XOR,
  RASQAL_EXPR_LSHIFT,
  RASQAL_EXPR_RSIGNEDSHIFT,
  RASQAL_EXPR_RUNSIGNEDSHIFT,
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
  RASQAL_EXPR_LAST= RASQAL_EXPR_VARIABLE
} rasqal_op;


/* expression (arg1), unary op (arg1), binary op (arg1,arg2),
 * literal or variable 
*/
struct rasqal_expression_s {
  rasqal_op op;
  struct rasqal_expression_s* arg1;
  struct rasqal_expression_s* arg2;
  rasqal_literal* literal;
  rasqal_variable* variable;
};
typedef struct rasqal_expression_s rasqal_expression;



typedef enum {
  RASQAL_TERM_UNKNOWN,    /* Unknown term type - illegal */
  RASQAL_TERM_VAR,
  RASQAL_TERM_URI,
  RASQAL_TERM_PATTERN,
  RASQAL_TERM_LITERAL,
} rasqal_term_type;

/* variable or URI (string) or pattern (string) or literal (string) */
typedef struct {
  rasqal_term_type type;
  void *value;
} rasqal_term ;


/* three terms */
typedef struct {
  rasqal_term* subject;
  rasqal_term* predicate;
  rasqal_term* object;
} rasqal_triple ;



/* RASQAL API */

/* Public functions */

RASQAL_API void rasqal_init(void);
RASQAL_API void rasqal_finish(void);


/* Query class */

/* Create */
RASQAL_API rasqal_query* rasqal_new_query(const char *name, const unsigned char *uri);
/* Destroy */
RASQAL_API void rasqal_free_query(rasqal_query* query);

/* Methods */
RASQAL_API void rasqal_query_add_source(rasqal_query* query, const unsigned char* uri);
RASQAL_API rasqal_sequence* rasqal_query_get_source_sequence(rasqal_query* query);
RASQAL_API const unsigned char* rasqal_query_get_source(rasqal_query* query, int idx);

RASQAL_API int rasqal_parse_query(rasqal_query *query, const char *query_string);

/* Utility methods */
RASQAL_API void rasqal_query_print(rasqal_query*, FILE *stream);


/* RDQL query parsing */
RASQAL_API int rdql_parse(rasqal_query* query, const char *query_string);



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

/* helper for printing sequences of strings */ 
void rasqal_sequence_print_string(char *data, FILE *fh);
void rasqal_sequence_print(rasqal_sequence* seq, FILE* fh);

/* Expression class */
rasqal_expression* rasqal_new_expression(rasqal_op op, rasqal_expression* arg1, rasqal_expression* arg2, rasqal_literal *literal, rasqal_variable *variable);
void rasqal_free_expression(rasqal_expression* e);
void rasqal_print_expression_op(rasqal_expression* expression, FILE* fh);
void rasqal_print_expression(rasqal_expression* e, FILE* fh);

/* Literal class */
rasqal_literal* rasqal_new_literal(rasqal_literal_type type, int integer, float floating, char *string);
void rasqal_free_literal(rasqal_literal* l);
void rasqal_print_literal_type(rasqal_literal* literal, FILE* fh);
void rasqal_print_literal(rasqal_literal* literal, FILE* fh);

rasqal_prefix* rasqal_new_prefix(const char *prefix, const char *uri);
void rasqal_free_prefix(rasqal_prefix* prefix);
void rasqal_print_prefix(rasqal_prefix* p, FILE* fh);

/* Term class */
rasqal_term* rasqal_new_term(rasqal_term_type type, void *value);
void rasqal_free_term(rasqal_term* term);
void rasqal_print_term(rasqal_term* t, FILE* fh);

/* Triple class */
rasqal_triple* rasqal_new_triple(rasqal_term* subject, rasqal_term* predicate, rasqal_term* object);
void rasqal_free_triple(rasqal_triple* t);
void rasqal_print_triple(rasqal_triple* t, FILE* fh);

/* Variable class */
rasqal_variable* rasqal_new_variable(const char *name, const char *value);
void rasqal_free_variable(rasqal_variable* variable);
void rasqal_print_variable(rasqal_variable* t, FILE* fh);


#ifdef __cplusplus
}
#endif

#endif

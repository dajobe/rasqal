/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_internal.h - Rasqal RDF Query library internals
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
 */



#ifndef RASQAL_INTERNAL_H
#define RASQAL_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef RASQAL_INTERNAL

/* for the memory allocation functions */
#if defined(HAVE_DMALLOC_H) && defined(RASQAL_MEMORY_DEBUG_DMALLOC)
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#undef HAVE_STDLIB_H
#endif
#include <dmalloc.h>
#endif

/* Can be over-ridden or undefined */
#define RASQAL_INLINE inline

#define RASQAL_MALLOC(type, size) malloc(size)
#define RASQAL_CALLOC(type, size, count) calloc(size, count)
#define RASQAL_FREE(type, ptr)   free((void*)ptr)

#ifdef RASQAL_DEBUG
/* Debugging messages */
#define RASQAL_DEBUG1(msg) do {fprintf(stderr, "%s:%d:%s: " msg, __FILE__, __LINE__, __func__); } while(0)
#define RASQAL_DEBUG2(msg, arg1) do {fprintf(stderr, "%s:%d:%s: " msg, __FILE__, __LINE__, __func__, arg1);} while(0)
#define RASQAL_DEBUG3(msg, arg1, arg2) do {fprintf(stderr, "%s:%d:%s: " msg, __FILE__, __LINE__, __func__, arg1, arg2);} while(0)
#define RASQAL_DEBUG4(msg, arg1, arg2, arg3) do {fprintf(stderr, "%s:%d:%s: " msg, __FILE__, __LINE__, __func__, arg1, arg2, arg3);} while(0)
#define RASQAL_DEBUG5(msg, arg1, arg2, arg3, arg4) do {fprintf(stderr, "%s:%d:%s: " msg, __FILE__, __LINE__, __func__, arg1, arg2, arg3, arg4);} while(0)
#define RASQAL_DEBUG6(msg, arg1, arg2, arg3, arg4, arg5) do {fprintf(stderr, "%s:%d:%s: " msg, __FILE__, __LINE__, __func__, arg1, arg2, arg3, arg4, arg5);} while(0)

#if defined(HAVE_DMALLOC_H) && defined(RASQAL_MEMORY_DEBUG_DMALLOC)
void* rasqal_system_malloc(size_t size);
void rasqal_system_free(void *ptr);
#define SYSTEM_MALLOC(size)   rasqal_system_malloc(size)
#define SYSTEM_FREE(ptr)   rasqal_system_free(ptr)
#else
#define SYSTEM_MALLOC(size)   malloc(size)
#define SYSTEM_FREE(ptr)   free(ptr)
#endif

#else
/* DEBUGGING TURNED OFF */

/* No debugging messages */
#define RASQAL_DEBUG1(msg)
#define RASQAL_DEBUG2(msg, arg1)
#define RASQAL_DEBUG3(msg, arg1, arg2)
#define RASQAL_DEBUG4(msg, arg1, arg2, arg3)
#define RASQAL_DEBUG5(msg, arg1, arg2, arg3, arg4)
#define RASQAL_DEBUG6(msg, arg1, arg2, arg3, arg4, arg5)

#define SYSTEM_MALLOC(size)   malloc(size)
#define SYSTEM_FREE(ptr)   free(ptr)

#endif


/* Fatal errors - always happen */
#define RASQAL_FATAL1(msg) do {fprintf(stderr, "%s:%d:%s: fatal error: " msg, __FILE__, __LINE__ , __func__); abort();} while(0)
#define RASQAL_FATAL2(msg,arg) do {fprintf(stderr, "%s:%d:%s: fatal error: " msg, __FILE__, __LINE__ , __func__, arg); abort();} while(0)


typedef struct rasqal_query_engine_factory_s rasqal_query_engine_factory;


/*
 * A query in some query language
 */
struct rasqal_query_s {
  int usage; /* reference count - 1 for itself, plus for query_results */
  
  unsigned char *query_string;

  raptor_namespace_stack *namespaces;

  /* sequences of ... */
  raptor_sequence *selects;     /* ... rasqal_variable* names only */
  raptor_sequence *sources;     /* ... raptor_uri*                 */
  raptor_sequence *triples;     /* ... rasqal_triple*              */
  raptor_sequence *constraints; /* ... rasqal_expression*          */
  raptor_sequence *prefixes;    /* ... rasqal_prefix*              */
  raptor_sequence *constructs;  /* ... rasqal_triple*        BRQL  */
  raptor_sequence *optional_triples; /* ... rasqal_triple*   BRQL  */
  raptor_sequence *describes;   /* ... rasqal_uri*           BRQL  */

  /* non-0 if '*' was seen in SELECT or DESCRIBE (selects will be NULL) */
  int select_all;

  /* DESCRIBE support */
  /* non-0 if selects array above was given by DESCRIBE */
  int select_is_describe;

  /* CONSTRUCT support */
  /* non-0 if 'CONSTRUCT *' was seen (constructs will be NULL) */
  int construct_all;

  int prepared;
  int executed;
  
  /* variable name/value table built from all distinct variables seen
   * in selects, triples, constraints.  An array of size variables_count
   *
   * The first select_variables_count of this array are from the selects
   * and are typically returned to the user.
   */
  rasqal_variable **variables;
  int variables_count;
  int select_variables_count;

  /* holds one copy of all the variables - this is where they are freed */
  raptor_sequence* variables_sequence;

  /* array of variable names to bind or NULL if no variables wanted
   * (size select_variables_count+1, last NULL)
   * indexes into the names in variables_sequence above.
   */
  const unsigned char** variable_names;

  /* array of result binding values, per result or NULL if no variables wanted
   * (size select_variables_count) 
   * indexes into the values in variables_sequence above, per-binding
   */
  rasqal_literal** binding_values;
  
  /* A reordered list of conjunctive triples from triples above
   * used as a better order to join in.
   *
   * NOTE: Shares the rasqal_triple* pointers with 'triples'.
   * The entries in this sequence are not freed.
   */
  raptor_sequence *ordered_triples;

  /* An array of items, one per triple in a query */
  rasqal_triple_meta *triple_meta;

  /* the expression version of the sequence of constraints above - this is
   * where the constraints are freed
   */
  rasqal_expression* constraints_expression;

  /* can be filled with error location information */
  raptor_locator locator;

  /* base URI of this query for resolving relative URIs in queries */
  raptor_uri* base_uri;

  /* non 0 if parser had fatal error and cannot continue */
  int failed;

  /* stuff for our user */
  void *user_data;

  void *fatal_error_user_data;
  void *error_user_data;
  void *warning_user_data;

  raptor_message_handler fatal_error_handler;
  raptor_message_handler error_handler;
  raptor_message_handler warning_handler;

  /* query engine specific stuff */
  void *context;

  /* stopping? */
  int abort;

  /* how many results already found */
  int result_count;

  /* non-0 if got all results */
  int finished;

  struct rasqal_query_engine_factory_s* factory;

  rasqal_triples_source* triples_source;

  rasqal_triples_source_factory* triples_source_factory;

  /* executing variables */
  int column;

  /* (linked list of) query results made from this query */
  rasqal_query_results *results;
};


/*
 * A query engine factory for a query language
 */
struct rasqal_query_engine_factory_s {
  struct rasqal_query_engine_factory_s* next;

  /* query language name */
  const char* name;

  /* query language readable label */
  const char* label;

  /* query language alternate name */
  const char* alias;

  /* query language MIME type (or NULL) */
  const char* mime_type;

  /* query language URI (or NULL) */
  const unsigned char* uri_string;
  
  /* the rest of this structure is populated by the
     query-engine-specific register function */
  size_t context_length;
  
  /* create a new query */
  int (*init)(rasqal_query* rq, const char *name);
  
  /* destroy a query */
  void (*terminate)(rasqal_query* rq);
  
  /* prepare a query */
  int (*prepare)(rasqal_query* rq);
  
  /* execute a query */
  int (*execute)(rasqal_query* rq);

  /* finish the query engine factory */
  void (*finish_factory)(rasqal_query_engine_factory* factory);
};


/*
 * A query result for some query
 */
struct rasqal_query_results_s {
  /* query that this was executed over */
  rasqal_query *query;

  /* next query result */
  rasqal_query_results *next;
};
    

/* rasqal_general.c */
char* rasqal_vsnprintf(const char *message, va_list arguments);

void rasqal_query_engine_register_factory(const char *name, const char *label, const char *alias, const unsigned char *uri_string, void (*factory) (rasqal_query_engine_factory*));
rasqal_query_engine_factory* rasqal_get_query_engine_factory (const char *name, const unsigned char *uri);

void rasqal_query_fatal_error(rasqal_query* query, const char *message, ...);
void rasqal_query_fatal_error_varargs(rasqal_query* query, const char *message, va_list arguments);
void rasqal_query_error(rasqal_query* query, const char *message, ...);
void rasqal_query_simple_error(void* query, const char *message, ...);
void rasqal_query_error_varargs(rasqal_query* query, const char *message, va_list arguments);
void rasqal_query_warning(rasqal_query* query, const char *message, ...);
void rasqal_query_warning_varargs(rasqal_query* query, const char *message, va_list arguments);

/* rdql_parser.y */
void rasqal_init_query_engine_rdql (void);

/* sparql_parser.y */
void rasqal_init_query_engine_sparql (void);

/* rasqal_engine.c */
int rasqal_query_order_triples(rasqal_query* query);
int rasqal_engine_declare_prefix(rasqal_query *rq, rasqal_prefix *prefix);
int rasqal_engine_declare_prefixes(rasqal_query *rq);
int rasqal_engine_expand_triple_qnames(rasqal_query* rq);
int rasqal_engine_expand_constraints_qnames(rasqal_query* rq);
int rasqal_engine_build_constraints_expression(rasqal_query* rq);
int rasqal_engine_assign_variables(rasqal_query* rq);

int rasqal_engine_execute_init(rasqal_query *query);
int rasqal_engine_execute_finish(rasqal_query *query);
int rasqal_engine_run(rasqal_query *q);

rasqal_triples_source* rasqal_new_triples_source(rasqal_query *query);
void rasqal_free_triples_source(rasqal_triples_source *rts);
int rasqal_triples_source_next_source(rasqal_triples_source *rts);

int rasqal_engine_get_next_result(rasqal_query *query);
void rasqal_engine_assign_binding_values(rasqal_query *query);

/* rasqal_expr.c */
int rasqal_literal_as_boolean(rasqal_literal* literal, int *error);
int rasqal_literal_as_integer(rasqal_literal* l, int *error);
void rasqal_literal_string_to_native(rasqal_literal *l);
int rasqal_literal_expand_qname(void *user_data, rasqal_literal *l);
int rasqal_expression_expand_qname(void *user_data, rasqal_expression *e);

/* strcasecmp.c */
#ifdef HAVE_STRCASECMP
#define rasqal_strcasecmp strcasecmp
#define rasqal_strncasecmp strncasecmp
#else
#ifdef HAVE_STRICMP
#define rasqal_strcasecmp stricmp
#define rasqal_strncasecmp strnicmp
#endif
#endif

/* rasqal_raptor.c */
void rasqal_raptor_init(void);

#ifdef RAPTOR_TRIPLES_SOURCE_REDLAND
/* rasqal_redland.c */
void rasqal_redland_init(void);
void rasqal_redland_finish(void);
#endif  

/* end of RASQAL_INTERNAL */
#endif


#ifdef __cplusplus
}
#endif

#endif

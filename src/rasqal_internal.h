/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_internal.h - Rasqal RDF Query library internals
 *
 * $Id$
 *
 * Copyright (C) 2003-2007, David Beckett http://purl.org/net/dajobe/
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

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
#define RASQAL_PRINTF_FORMAT(string_index, first_to_check_index) \
  __attribute__((__format__(__printf__, string_index, first_to_check_index)))
#else
#define RASQAL_PRINTF_FORMAT(string_index, first_to_check_index)
#endif

/* Can be over-ridden or undefined in a config.h file or -Ddefine */
#ifndef RASQAL_INLINE
#define RASQAL_INLINE inline
#endif

#ifdef LIBRDF_DEBUG
#define RASQAL_DEBUG 1
#endif

#if defined(RASQAL_MEMORY_SIGN)
#define RASQAL_SIGN_KEY 0x08A59A10
void* rasqal_sign_malloc(size_t size);
void* rasqal_sign_calloc(size_t nmemb, size_t size);
void* rasqal_sign_realloc(void *ptr, size_t size);
void rasqal_sign_free(void *ptr);
  
#define RASQAL_MALLOC(type, size)   rasqal_sign_malloc(size)
#define RASQAL_CALLOC(type, nmemb, size) rasqal_sign_calloc(nmemb, size)
#define RASQAL_REALLOC(type, ptr, size) rasqal_sign_realloc(ptr, size)
#define RASQAL_FREE(type, ptr)   rasqal_sign_free(ptr)

#else
#define RASQAL_MALLOC(type, size) malloc(size)
#define RASQAL_CALLOC(type, size, count) calloc(size, count)
#define RASQAL_FREE(type, ptr)   free((void*)ptr)

#endif

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
#define RASQAL_FATAL3(msg,arg1,arg2) do {fprintf(stderr, "%s:%d:%s: fatal error: " msg, __FILE__, __LINE__ , __func__, arg1, arg2); abort();} while(0)

#define RASQAL_DEPRECATED_MESSAGE(msg) do {static int warning_given=0; if(!warning_given++) fprintf(stderr, "Function %s is deprecated - " msg,  __func__); } while(0)
#define RASQAL_DEPRECATED_WARNING(rq, msg) do {static int warning_given=0; if(!warning_given++) rasqal_query_warning(rq, msg); } while(0)


typedef struct rasqal_query_engine_factory_s rasqal_query_engine_factory;


/*
 * Pattern graph for executing
 */
struct rasqal_graph_pattern_s {
  rasqal_query* query;

  /* operator for this graph pattern's contents */
  rasqal_graph_pattern_operator op;
  
  raptor_sequence* triples;          /* ... rasqal_triple*         */
  raptor_sequence* graph_patterns;   /* ... rasqal_graph_pattern*  */

  int start_column;
  int end_column;

  /* Max optional graph pattern allowed so far to stop backtracking
   * going over old graph patterns
   */
  int max_optional_graph_pattern;

  /* true when this graph pattern matched last time */
  int matched;

  /* true when an optional graph pattern finished last time round */
  int finished;

  raptor_sequence *constraints; /* ... rasqal_expression*          */
  /* the expression version of the sequence of constraints above - this is
   * where the constraints are freed
   */
  rasqal_expression* constraints_expression;

  /* index of the graph pattern in the query (0.. query->graph_pattern_count-1) */
  int gp_index;
};

rasqal_graph_pattern* rasqal_new_graph_pattern(rasqal_query* query);
rasqal_graph_pattern* rasqal_new_graph_pattern_from_triples(rasqal_query* query, raptor_sequence* triples, int start_column, int end_column, rasqal_graph_pattern_operator op);
rasqal_graph_pattern* rasqal_new_graph_pattern_from_sequence(rasqal_query* query, raptor_sequence* graph_patterns, rasqal_graph_pattern_operator op);
void rasqal_free_graph_pattern(rasqal_graph_pattern* gp);
void rasqal_graph_pattern_adjust(rasqal_graph_pattern* gp, int offset);
void rasqal_graph_pattern_set_origin(rasqal_graph_pattern* graph_pattern, rasqal_literal* origin);
void rasqal_graph_pattern_add_triples(rasqal_graph_pattern* gp, raptor_sequence* triples, int start_column, int end_column, rasqal_graph_pattern_operator op);
int rasqal_reset_triple_meta(rasqal_triple_meta* m);


/*
 * A query in some query language
 */
struct rasqal_query_s {
  int usage; /* reference count - 1 for itself, plus for query_results */
  
  unsigned char* query_string;

  raptor_namespace_stack* namespaces;

  /* query graph pattern, containing the sequence of graph_patterns below */
  rasqal_graph_pattern* query_graph_pattern;
  
  /* the query verb - in SPARQL terms: SELECT, CONSTRUCT, DESCRIBE or ASK */
  rasqal_query_verb verb;
  
  /* sequences of ... */
  raptor_sequence* selects;     /* ... rasqal_variable* names only */
  raptor_sequence* data_graphs; /* ... rasqal_data_graph*          */
  raptor_sequence* triples;     /* ... rasqal_triple*              */
  raptor_sequence* prefixes;    /* ... rasqal_prefix*              */
  raptor_sequence* constructs;  /* ... rasqal_triple*       SPARQL */
  raptor_sequence* optional_triples; /* ... rasqal_triple*  SPARQL */
  raptor_sequence* describes;   /* ... rasqal_literal* (var or URIs) SPARQL */

  /* non-0 if DISTINCT was seen in the query (SELECT or DESCRIBE) */
  int distinct;

  /* result limit LIMIT (>=0) or <0 if not given */
  int limit;

  /* result offset OFFSET (>=0) or <0 if not given */
  int offset;

  /* non-0 if '*' was seen after a verb (the appropriate list such as selects or constructs will be NULL) */
  int wildcard;

  int prepared;
  
  /* variable name/value table built from all distinct variables seen
   * in selects, triples, constraints and anonymous (no name, cannot
   * be selected or refered to).  An array of size variables_count
   *
   * The first select_variables_count of this array are from the
   * selects.  The selected variables only are typically returned to
   * the user.
   *
   * Anonymous variables appear at the end of the 'variables' array but
   * are taken from the anon_variables_sequence.
   */
  rasqal_variable** variables;
  int variables_count;
  int select_variables_count;

  /* array of size variables_count
   * pointing to triple column where variable[i] is declared
   */
  int* variables_declared_in;

  /* holds one copy of all the variables - this is where they are freed */
  raptor_sequence* variables_sequence;

  /* holds one copy of all anonymous variables - this is where they are freed */
  raptor_sequence* anon_variables_sequence;

  int anon_variables_count;

  /* array of variable names to bind or NULL if no variables wanted
   * (size (select_variables_count OR construct_variables_count)+1, last NULL)
   * indexes into the names in variables_sequence above.
   */
  const unsigned char** variable_names;

  /* array of result binding values, per result or NULL if no variables wanted
   * (size select_variables_count or construct_variables_count) 
   * indexes into the values in variables_sequence above, per-binding
   */
  rasqal_literal** binding_values;
  
  /* can be filled with error location information */
  raptor_locator locator;

  /* base URI of this query for resolving relative URIs in queries */
  raptor_uri* base_uri;

  /* non 0 if query had fatal error in parsing and cannot be executed */
  int failed;

  /* stuff for our user */
  void* user_data;

  void* fatal_error_user_data;
  void* error_user_data;
  void* warning_user_data;

  raptor_message_handler fatal_error_handler;
  raptor_message_handler error_handler;
  raptor_message_handler warning_handler;

  int default_generate_bnodeid_handler_base;
  char *default_generate_bnodeid_handler_prefix;
  size_t default_generate_bnodeid_handler_prefix_length;

  void *generate_bnodeid_handler_user_data;
  rasqal_generate_bnodeid_handler generate_bnodeid_handler;


  /* query engine specific stuff */
  void* context;

  struct rasqal_query_engine_factory_s* factory;

  rasqal_triples_source_factory* triples_source_factory;

  /* (linked list of) query results made from this query */
  rasqal_query_results* results;

  /* incrementing counter for declaring prefixes in order of appearance */
  int prefix_depth;

  /* sequence of constraints - internal for RDQL parsing, not returned */
  raptor_sequence* constraints_sequence;
  
  /* sequence of order condition expressions */
  raptor_sequence* order_conditions_sequence;

  /* sequence of group by condition expressions */
  raptor_sequence* group_conditions_sequence;

  /* INTERNAL rasqal_literal_compare / rasqal_expression_evaluate flags */
  int compare_flags;

  /* Number of graph patterns in this query */
  int graph_pattern_count;
  
  /* Graph pattern shared pointers by gp index (after prepare) */
  raptor_sequence* graph_patterns_sequence;

  /* Features */
  int features[RASQAL_FEATURE_LAST+1];

  /* Name of requested query results syntax.  If present, this
   * is the name used for constructing a rasqal_query_formatter
   * from the results.
   */
  const char* query_results_formatter_name;

  /* EXPLAIN was given */
  int explain;

  /* generated counter - increments at every generation */
  int genid_counter;
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
  int (*execute)(rasqal_query* rq, rasqal_query_results* results);

  /* finish the query engine factory */
  void (*finish_factory)(rasqal_query_engine_factory* factory);

  /* Write a string to an iostream in escaped form suitable for the query */
  int (*iostream_write_escaped_counted_string)(rasqal_query* rq, raptor_iostream* iostr, const unsigned char* string, size_t len);
};


/*
 * A row of query results 
 */
typedef struct {
  /* reference count */
  int usage;

  /* Query results this row is associated with */
  rasqal_query_results* results;

  /* current row number in the sequence of rows*/
  int offset;

  /* values for each variable in the query sequence of values */
  int size;
  rasqal_literal** values;

  /* literal values for ORDER BY expressions evaluated for this row */
  /* number of expressions (can be 0) */
  int order_size;
  rasqal_literal** order_values;
} rasqal_query_result_row;


typedef struct rasqal_map_s rasqal_map;

/* The execution data here is a sequence of
 * rasqal_graph_pattern_data execution data of size
 * query->graph_pattern_count with each rasqal_graph_pattern_data
 */
typedef struct {
  raptor_sequence* seq;

  /* offset into stored results sequence */
  int offset;

  /* for ordering results during execution */
  rasqal_map* map;
} rasqal_engine_execution_data;


/*
 * A query result for some query
 */
struct rasqal_query_results_s {
  /* stopping? */
  int abort;

  /* non-0 if got all results */
  int finished;

  /* non-0 if query has been executed */
  int executed;

  /* non 0 if query had fatal error and cannot be executed */
  int failed;

  /* query that this was executed over */
  rasqal_query* query;

  /* how many results already found */
  int result_count;

  /* execution data - depends on execution engine */
  void* execution_data;

  /* pointer to function that tidies the query_results execution data above */
  void (*free_execution_data)(rasqal_query* query, struct rasqal_query_results_s* query_results, void *execution_data);
  
  /* next query result */
  rasqal_query_results *next;

  /* current row of results */
  rasqal_query_result_row* row;

  /* boolean ASK result >0 true, 0 false or -1 uninitialised */
  int ask_result;

  /* New variables bound from during the current 'next result' run */
  int new_bindings_count;

  rasqal_triples_source* triples_source;

  /* current triple in the sequence of triples 'constructs' or -1 */
  int current_triple_result;

  /* constructed triple result (SHARED) */
  raptor_statement result_triple;

  /* INTERNAL triple used to store literals for subject, predicate, object
   * never returned
   */
  rasqal_triple* triple;
  
  /* INTERNAL sequence of results for ordering */
  raptor_sequence* results_sequence;
};
    

typedef int (*rasqal_query_results_writer)(raptor_iostream *iostr,
                                           rasqal_query_results* results,
                                           raptor_uri *base_uri);

typedef struct {
  /* query results format name */
  const char* name;

  /* query results format name */
  const char* label;

  /* query results format URI (or NULL) */
  const unsigned char* uri_string;
  
  rasqal_query_results_writer writer;

  /* MIME Type of the constructed syntax */
  const char* mime_type;
} rasqal_query_results_format_factory;


/*
 * A query results formatter for some query_results
 */
struct rasqal_query_results_formatter_s {
  rasqal_query_results_format_factory* factory;

  const char *mime_type;
};


/* rasqal_general.c */
char* rasqal_vsnprintf(const char* message, va_list arguments);

void rasqal_query_engine_register_factory(const char* name, const char* label, const char* alias, const unsigned char* uri_string, void (*factory) (rasqal_query_engine_factory*));
rasqal_query_engine_factory* rasqal_get_query_engine_factory (const char* name, const unsigned char* uri);

void rasqal_query_fatal_error(rasqal_query* query, const char* message, ...) RASQAL_PRINTF_FORMAT(2, 3);
void rasqal_query_fatal_error_varargs(rasqal_query* query, const char* message, va_list arguments) RASQAL_PRINTF_FORMAT(2, 0);
void rasqal_query_error(rasqal_query* query, const char* message, ...) RASQAL_PRINTF_FORMAT(2, 3);
void rasqal_query_simple_error(void* query, const char* message, ...) RASQAL_PRINTF_FORMAT(2, 3);
void rasqal_query_error_varargs(rasqal_query* query, const char* message, va_list arguments) RASQAL_PRINTF_FORMAT(2, 0);
void rasqal_query_warning(rasqal_query* query, const char* message, ...) RASQAL_PRINTF_FORMAT(2, 3);
void rasqal_query_warning_varargs(rasqal_query* query, const char* message, va_list arguments) RASQAL_PRINTF_FORMAT(2, 0);

const char* rasqal_basename(const char* name);


/* rasqal_graph_pattern.c */
unsigned char* rasqal_escaped_name_to_utf8_string(const unsigned char* src, size_t len, size_t* dest_lenp, raptor_simple_message_handler error_handler, void* error_data);

unsigned char* rasqal_query_generate_bnodeid(rasqal_query* rdf_query, unsigned char *user_bnodeid);

/* rdql_parser.y */
void rasqal_init_query_engine_rdql (void);

/* sparql_parser.y */
void rasqal_init_query_engine_sparql (void);
void rasqal_init_query_engine_laqrs (void);

/* rasqal_engine.c */
int rasqal_engine_sequence_has_qname(raptor_sequence* seq);
int rasqal_engine_expand_triple_qnames(rasqal_query* rq);
int rasqal_engine_query_constraints_has_qname(rasqal_query* gp);
int rasqal_engine_graph_pattern_constraints_has_qname(rasqal_graph_pattern* gp);
int rasqal_engine_expand_query_constraints_qnames(rasqal_query* rq);
int rasqal_engine_expand_graph_pattern_constraints_qnames(rasqal_query* rq, rasqal_graph_pattern* gp);
int rasqal_engine_build_constraints_expression(rasqal_graph_pattern* gp);
int rasqal_engine_assign_variables(rasqal_query* rq);

int rasqal_engine_prepare(rasqal_query* query);
int rasqal_engine_execute_init(rasqal_query_results* query_results);
int rasqal_engine_execute_finish(rasqal_query_results* query_results);
int rasqal_engine_run(rasqal_query_results* query_results);
void rasqal_engine_join_graph_patterns(rasqal_graph_pattern *dest_gp, rasqal_graph_pattern *src_gp);
int rasqal_engine_check_limit_offset(rasqal_query_results* query_results);
int rasqal_engine_merge_triples(rasqal_query* query, rasqal_graph_pattern* gp, void* data);
int rasqal_engine_merge_graph_patterns(rasqal_query* query, rasqal_graph_pattern* gp, void* data);
int rasqal_engine_expression_fold(rasqal_query* rq, rasqal_expression* e);
int rasqal_engine_graph_pattern_fold_expressions(rasqal_query* rq, rasqal_graph_pattern* gp);
int rasqal_engine_query_fold_expressions(rasqal_query* rq);
int rasqal_engine_remove_empty_group_graph_patterns(rasqal_query* query, rasqal_graph_pattern* gp, void* data);
  
rasqal_triples_source* rasqal_new_triples_source(rasqal_query_results* query_results);
void rasqal_free_triples_source(rasqal_triples_source* rts);
int rasqal_triples_source_next_source(rasqal_triples_source* rts);

int rasqal_engine_get_next_result(rasqal_query_results* query_results);
void rasqal_engine_assign_binding_values(rasqal_query* query);
void rasqal_engine_move_constraints(rasqal_graph_pattern* dest_gp, rasqal_graph_pattern* src_gp);
int rasqal_engine_execute_run(rasqal_query_results* query_results);
void rasqal_engine_free_query_result_row(rasqal_query_result_row* row);
rasqal_literal** rasqal_engine_get_results_values(rasqal_query_results* query_results);
rasqal_literal* rasqal_engine_get_result_value(rasqal_query_results* query_results, int offset);
int rasqal_engine_execute_next(rasqal_query_results* query_results);

/* rasqal_expr.c */
int rasqal_literal_as_boolean(rasqal_literal* literal, int* error);
int rasqal_literal_as_integer(rasqal_literal* l, int* error);
double rasqal_literal_as_floating(rasqal_literal* l, int* error);
raptor_uri* rasqal_literal_as_uri(rasqal_literal* l);
int rasqal_literal_string_to_native(rasqal_literal *l, raptor_simple_message_handler error_handler, void *error_data);
int rasqal_literal_has_qname(rasqal_literal* l);
int rasqal_literal_expand_qname(void* user_data, rasqal_literal* l);
int rasqal_literal_is_constant(rasqal_literal* l);
int rasqal_expression_has_qname(void* user_data, rasqal_expression* e);
int rasqal_expression_expand_qname(void* user_data, rasqal_expression* e);
int rasqal_literal_ebv(rasqal_literal* l);
int rasqal_expression_is_constant(rasqal_expression* e);
void rasqal_expression_clear(rasqal_expression* e);
void rasqal_expression_convert_to_literal(rasqal_expression* e, rasqal_literal* l);

/* strcasecmp.c */
#ifdef HAVE_STRCASECMP
#  define rasqal_strcasecmp strcasecmp
#  define rasqal_strncasecmp strncasecmp
#else
#  ifdef HAVE_STRICMP
#    define rasqal_strcasecmp stricmp
#    define rasqal_strncasecmp strnicmp
#   else
int rasqal_strcasecmp(const char* s1, const char* s2);
int rasqal_strncasecmp(const char* s1, const char* s2, size_t n);
#  endif
#endif

/* rasqal_raptor.c */
void rasqal_raptor_init(void);

#ifdef RAPTOR_TRIPLES_SOURCE_REDLAND
/* rasqal_redland.c */
void rasqal_redland_init(void);
void rasqal_redland_finish(void);
#endif  


/* rasqal_general.c */
extern raptor_uri* rasqal_xsd_namespace_uri;
extern raptor_uri* rasqal_xsd_integer_uri;
extern raptor_uri* rasqal_xsd_double_uri;
extern raptor_uri* rasqal_xsd_float_uri;
extern raptor_uri* rasqal_xsd_boolean_uri;
extern raptor_uri* rasqal_xsd_decimal_uri;
extern raptor_uri* rasqal_xsd_datetime_uri;
extern raptor_uri* rasqal_xsd_string_uri;
extern raptor_uri* rasqal_rdf_namespace_uri;
extern raptor_uri* rasqal_rdf_first_uri;
extern raptor_uri* rasqal_rdf_rest_uri;
extern raptor_uri* rasqal_rdf_nil_uri;
void rasqal_uri_init(void);
void rasqal_uri_finish(void);

/* rasqal_literal.c */
typedef struct {
  raptor_sequence *triples;
  rasqal_literal *value;
} rasqal_formula;

rasqal_formula* rasqal_new_formula(void);
void rasqal_free_formula(rasqal_formula* formula);
void rasqal_formula_print(rasqal_formula* formula, FILE *stream);
rasqal_formula* rasqal_formula_join(rasqal_formula* first_formula, rasqal_formula* second_formula);
rasqal_graph_pattern* rasqal_engine_new_graph_pattern_from_formula(rasqal_query* query, rasqal_formula* formula, rasqal_graph_pattern_operator op);
rasqal_graph_pattern* rasqal_engine_group_2_graph_patterns(rasqal_query* query, rasqal_graph_pattern* first_gp, rasqal_graph_pattern* second_gp);

/* The following should be public eventually in rasqal.h or raptor.h or ...? */

typedef int (rasqal_compare_fn)(const void *a, const void *b);
typedef void (rasqal_kv_free_fn)(const void *key, const void *value);


#define RASQAL_XSD_BOOLEAN_TRUE (const unsigned char*)"true"
#define RASQAL_XSD_BOOLEAN_FALSE (const unsigned char*)"false"


/* rasqal_map.c */
typedef void (*rasqal_map_visit_fn)(void *key, void *value, void *user_data);

rasqal_map* rasqal_new_map(rasqal_compare_fn* compare_fn, rasqal_kv_free_fn* free_fn, raptor_sequence_print_handler* print_key_fn, raptor_sequence_print_handler* print_value_fn, int flags);
void rasqal_free_map(rasqal_map *map);
int rasqal_map_add_kv(rasqal_map* map, void* key, void *value);
void rasqal_map_visit(rasqal_map* map, rasqal_map_visit_fn fn, void *user_data);
void rasqal_map_print(rasqal_map* map, FILE* fh);

/* rasqal_query.c */
void rasqal_query_remove_query_result(rasqal_query* query, rasqal_query_results* query_results);
unsigned char* rasqal_prefix_id(int prefix_id, unsigned char *string);
int rasqal_query_declare_prefix(rasqal_query* rq, rasqal_prefix* prefix);
int rasqal_query_declare_prefixes(rasqal_query* rq);
unsigned char* rasqal_query_get_genid(rasqal_query* query, const unsigned char* base, int counter);

/* rasqal_query_results.c */
void rasqal_init_query_results(void);
void rasqal_finish_query_results(void);

rasqal_query_results* rasqal_new_query_results(rasqal_query* query);
void rasqal_query_results_init(rasqal_query_results* query_results);

/* end of RASQAL_INTERNAL */
#endif


#ifdef __cplusplus
}
#endif

#endif

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_internal.h - Rasqal RDF Query library internals
 *
 * Copyright (C) 2003-2010, David Beckett http://www.dajobe.org/
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

#if defined(_MSC_VER) && _MSC_VER < 1600
typedef unsigned __int32 uint32_t;
typedef __int16 int16_t;
#else
#include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#define RASQAL_EXTERN_C extern "C"
#else
#define RASQAL_EXTERN_C
#endif

#ifdef RASQAL_INTERNAL

#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
#define RASQAL_PRINTF_FORMAT(string_index, first_to_check_index) \
  __attribute__((__format__(__printf__, string_index, first_to_check_index)))
#else
#define RASQAL_PRINTF_FORMAT(string_index, first_to_check_index)
#endif

#ifdef __GNUC__
#define RASQAL_NORETURN __attribute__((noreturn)) 
#else
#define RASQAL_NORETURN
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
  
#define RASQAL_MALLOC(type, size)   (type)rasqal_sign_malloc(size)
#define RASQAL_CALLOC(type, nmemb, size) (type)rasqal_sign_calloc(nmemb, size)
#define RASQAL_REALLOC(type, ptr, size) (type)rasqal_sign_realloc(ptr, size)
#define RASQAL_FREE(type, ptr)   rasqal_sign_free((void*)ptr)

#else
#define RASQAL_MALLOC(type, size) (type)malloc(size)
#define RASQAL_CALLOC(type, size, count) (type)calloc(size, count)
#define RASQAL_REALLOC(type, ptr, size) (type)realloc(ptr, size)
#define RASQAL_FREE(type, ptr)   free((void*)ptr)

#endif

#ifdef HAVE___FUNCTION__
#else
#define __FUNCTION__ "???"
#endif

#ifndef RASQAL_DEBUG_FH
#define RASQAL_DEBUG_FH stderr
#endif

#ifdef RASQAL_DEBUG
#include <stdlib.h>

/* Runtime debug level control via RASQAL_DEBUG_LEVEL environment variable.
 *
 * Debug levels:
 *   0 (Off): No debug output (for testing)
 *   1 (Normal): Standard RASQAL_DEBUG1-6 macro output (default)
 *   2 (Verbose): Level 1 + detailed algebra dumps, scope analysis, dataset operations
 *   3 (Very Verbose): Level 2 + expression evaluation traces, variable usage tracking
 *
 * Default level is 1 when RASQAL_DEBUG_LEVEL is not set or invalid.
 * Values outside 0-3 are clamped to valid range.
 */
static inline int
rasqal_get_debug_level(void)
{
  static int level = -1;  /* -1 = not yet initialized */
  if (level == -1) {
    const char* env = getenv("RASQAL_DEBUG_LEVEL");
    if (env && *env) {
      level = atoi(env);
      /* Clamp to valid range 0-3 */
      if (level < 0) level = 0;
      if (level > 3) level = 3;
    } else {
      level = 1;  /* Default: normal debug output */
    }
  }
  return level;
}

/* Debugging messages - only output if level >= 1 (normal debug) */
#define RASQAL_DEBUG1(msg) \
  do { if(rasqal_get_debug_level() >= 1) { \
    fprintf(RASQAL_DEBUG_FH, "%s:%d:%s: " msg, __FILE__, __LINE__, __FUNCTION__); \
  } } while(0)
#define RASQAL_DEBUG2(msg, arg1) \
  do { if(rasqal_get_debug_level() >= 1) { \
    fprintf(RASQAL_DEBUG_FH, "%s:%d:%s: " msg, __FILE__, __LINE__, __FUNCTION__, arg1); \
  } } while(0)
#define RASQAL_DEBUG3(msg, arg1, arg2) \
  do { if(rasqal_get_debug_level() >= 1) { \
    fprintf(RASQAL_DEBUG_FH, "%s:%d:%s: " msg, __FILE__, __LINE__, __FUNCTION__, arg1, arg2); \
  } } while(0)
#define RASQAL_DEBUG4(msg, arg1, arg2, arg3) \
  do { if(rasqal_get_debug_level() >= 1) { \
    fprintf(RASQAL_DEBUG_FH, "%s:%d:%s: " msg, __FILE__, __LINE__, __FUNCTION__, arg1, arg2, arg3); \
  } } while(0)
#define RASQAL_DEBUG5(msg, arg1, arg2, arg3, arg4) \
  do { if(rasqal_get_debug_level() >= 1) { \
    fprintf(RASQAL_DEBUG_FH, "%s:%d:%s: " msg, __FILE__, __LINE__, __FUNCTION__, arg1, arg2, arg3, arg4); \
  } } while(0)
#define RASQAL_DEBUG6(msg, arg1, arg2, arg3, arg4, arg5) \
  do { if(rasqal_get_debug_level() >= 1) { \
    fprintf(RASQAL_DEBUG_FH, "%s:%d:%s: " msg, __FILE__, __LINE__, __FUNCTION__, arg1, arg2, arg3, arg4, arg5); \
  } } while(0)

#if defined(HAVE_DMALLOC_H) && defined(RASQAL_MEMORY_DEBUG_DMALLOC)
void* rasqal_system_malloc(size_t size);
void rasqal_system_free(void *ptr);
#define SYSTEM_MALLOC(size)   rasqal_system_malloc(size)
#define SYSTEM_FREE(ptr)   rasqal_system_free(ptr)
#else
#define SYSTEM_MALLOC(size)   malloc(size)
#define SYSTEM_FREE(ptr)   free(ptr)
#endif

#ifndef RASQAL_ASSERT_DIE
#define RASQAL_ASSERT_DIE(x) abort();
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

#ifndef RASQAL_ASSERT_DIE
#define RASQAL_ASSERT_DIE(x) x;
#endif

#endif


#ifdef RASQAL_DISABLE_ASSERT_MESSAGES
#define RASQAL_ASSERT_REPORT(line)
#else
#define RASQAL_ASSERT_REPORT(msg) fprintf(RASQAL_DEBUG_FH, "%s:%d: (%s) assertion failed: " msg "\n", __FILE__, __LINE__, __FUNCTION__);
#endif


#ifdef RASQAL_DISABLE_ASSERT

#define RASQAL_ASSERT(condition, msg) 
#define RASQAL_ASSERT_RETURN(condition, msg, ret) 
#define RASQAL_ASSERT_OBJECT_POINTER_RETURN(pointer, type) do { \
  if(!pointer) \
    return; \
} while(0)
#define RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(pointer, type, ret)

#else

#define RASQAL_ASSERT(condition, msg) do { \
  if(condition) { \
    RASQAL_ASSERT_REPORT(msg) \
      RASQAL_ASSERT_DIE()     \
  } \
} while(0)

#define RASQAL_ASSERT_RETURN(condition, msg, ret) do { \
  if(condition) { \
    RASQAL_ASSERT_REPORT(msg) \
    RASQAL_ASSERT_DIE(return ret) \
  } \
} while(0)

#define RASQAL_ASSERT_OBJECT_POINTER_RETURN(pointer, type) do { \
  if(!pointer) { \
    RASQAL_ASSERT_REPORT("object pointer of type " #type " is NULL.") \
    RASQAL_ASSERT_DIE(return) \
  } \
} while(0)

#define RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(pointer, type, ret) do { \
  if(!pointer) { \
    RASQAL_ASSERT_REPORT("object pointer of type " #type " is NULL.") \
    RASQAL_ASSERT_DIE(return ret) \
  } \
} while(0)

#endif


/* _Pragma() is C99 and is the only way to include pragmas since you
 * cannot use #pragma in a macro
 *
 * #if defined __STDC_VERSION__ && (__STDC_VERSION__ >= 199901L)
 *
 * Valid for clang or GCC >= 4.9.0
 */
#if (defined(__GNUC__) && ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((4) << 16) + (9)))
#define PRAGMA_IGNORE_WARNING_FORMAT_NONLITERAL_START \
  _Pragma ("GCC diagnostic push") \
  _Pragma ("GCC diagnostic ignored \"-Wformat-nonliteral\"")
#define PRAGMA_IGNORE_WARNING_FORMAT_OVERFLOW_START     \
  _Pragma ("GCC diagnostic push") \
  _Pragma ("GCC diagnostic ignored \"-Wformat-overflow\"")
#define PRAGMA_IGNORE_WARNING_FORMAT_TRUNCATION_START \
  _Pragma ("GCC diagnostic push") \
  _Pragma ("GCC diagnostic ignored \"-Wformat-truncation\"")
#define PRAGMA_IGNORE_WARNING_REDUNDANT_DECLS_START \
  _Pragma ("GCC diagnostic push") \
  _Pragma ("GCC diagnostic ignored \"-Wredundant-decls\"")
#define PRAGMA_IGNORE_WARNING_END \
  _Pragma ("GCC diagnostic pop")
#define FALLTHROUGH_IS_OK \
   __attribute__((fallthrough));
/* C++17 should be: [[fallthrough]] */
#elif defined(__clang__)
#define PRAGMA_IGNORE_WARNING_FORMAT_NONLITERAL_START \
  _Pragma ("GCC diagnostic push") \
  _Pragma ("GCC diagnostic ignored \"-Wformat-nonliteral\"")
#define PRAGMA_IGNORE_WARNING_FORMAT_OVERFLOW_START     \
  _Pragma ("GCC diagnostic push")
#define PRAGMA_IGNORE_WARNING_FORMAT_TRUNCATION_START \
  _Pragma ("GCC diagnostic push")
#define PRAGMA_IGNORE_WARNING_REDUNDANT_DECLS_START \
  _Pragma ("GCC diagnostic push")
#define PRAGMA_IGNORE_WARNING_END \
  _Pragma ("GCC diagnostic pop")
#define FALLTHROUGH_IS_OK \
   __attribute__((fallthrough));
#else
#define PRAGMA_IGNORE_WARNING_FORMAT_NONLITERAL_START
#define PRAGMA_IGNORE_WARNING_FORMAT_OVERFLOW_START
#define PRAGMA_IGNORE_WARNING_FORMAT_TRUNCATION_START
#define PRAGMA_IGNORE_WARNING_REDUNDANT_DECLS_START
#define PRAGMA_IGNORE_WARNING_END
#define FALLTHROUGH_IS_OK
#endif

/* Fatal errors - always happen */
#define RASQAL_FATAL1(msg) do {fprintf(RASQAL_DEBUG_FH, "%s:%d:%s: fatal error: " msg, __FILE__, __LINE__ , __FUNCTION__); abort();} while(0)
#define RASQAL_FATAL2(msg,arg) do {fprintf(RASQAL_DEBUG_FH, "%s:%d:%s: fatal error: " msg, __FILE__, __LINE__ , __FUNCTION__, arg); abort();} while(0)
#define RASQAL_FATAL3(msg,arg1,arg2) do {fprintf(RASQAL_DEBUG_FH, "%s:%d:%s: fatal error: " msg, __FILE__, __LINE__ , __FUNCTION__, arg1, arg2); abort();} while(0)

#ifndef NO_STATIC_DATA
#define RASQAL_DEPRECATED_MESSAGE(msg) do {static int warning_given=0; if(!warning_given++) fprintf(RASQAL_DEBUG_FH, "Function %s is deprecated - " msg,  __FUNCTION__); } while(0)
#define RASQAL_DEPRECATED_WARNING(rq, msg) do {static int warning_given=0; if(!warning_given++) rasqal_query_warning(rq, msg); } while(0)
#else
#define RASQAL_DEPRECATED_MESSAGE(msg) do { fprintf(RASQAL_DEBUG_FH, "Function %s is deprecated - " msg,  __FUNCTION__); } while(0)
#define RASQAL_DEPRECATED_WARNING(rq, msg) do { rasqal_query_warning(rq, msg); } while(0)
#endif


typedef struct rasqal_query_execution_factory_s rasqal_query_execution_factory;
typedef struct rasqal_query_language_factory_s rasqal_query_language_factory;


/**
 * rasqal_projection:
 * @query: rasqal query
 * @wildcard: non-0 if @variables was '*'
 * @distinct: 1 if distinct, 2 if reduced, otherwise neither
 *
 * Query projection (SELECT vars, SELECT *, SELECT DISTINCT/REDUCED ...)
 *
 */
typedef struct {
  rasqal_query* query;
  
  raptor_sequence* variables;

  unsigned int wildcard:1;

  int distinct;
} rasqal_projection;  


/**
 * rasqal_solution_modifier:
 * @query: rasqal query
 * @order_conditions: sequence of order condition expressions (or NULL)
 * @group_conditions: sequence of group by condition expressions (or NULL)
 * @having_conditions: sequence of (group by ...) having condition expressions (or NULL)
 * @limit: result limit LIMIT (>=0) or <0 if not given
 * @offset: result offset OFFSET (>=0) or <0 if not given
 *
 * Query solution modifiers
 *
 */
typedef struct {
  rasqal_query* query;
  
  raptor_sequence* order_conditions;

  raptor_sequence* group_conditions;

  raptor_sequence* having_conditions;
  
  int limit;

  int offset;
} rasqal_solution_modifier;


/**
 * rasqal_bindings:
 * @query: rasqal query
 * @variables: sequence of variables
 * @rows: sequence of #rasqal_row (or NULL)
 *
 * Result bindings from SPARQL 1.1 BINDINGS block
 *
 */
typedef struct {
  /* usage/reference count */
  int usage;
  
  rasqal_query* query;
  
  raptor_sequence* variables;

  raptor_sequence* rows;
} rasqal_bindings;  


/* Forward declarations */
struct rasqal_query_scope_s;
typedef struct rasqal_query_scope_s rasqal_query_scope;
struct rasqal_rowsource_s;
typedef struct rasqal_rowsource_s rasqal_rowsource;

/*
 * Graph Pattern Scope for hierarchical triple and variable management
 * Implements the scope model from rasqal-graph-pattern-scopes.md
 */

/* Scope type constants for query scopes */
#define RASQAL_QUERY_SCOPE_TYPE_ROOT      0
#define RASQAL_QUERY_SCOPE_TYPE_EXISTS    1
#define RASQAL_QUERY_SCOPE_TYPE_NOT_EXISTS 2
#define RASQAL_QUERY_SCOPE_TYPE_MINUS     3
#define RASQAL_QUERY_SCOPE_TYPE_UNION     4
#define RASQAL_QUERY_SCOPE_TYPE_SUBQUERY  5
#define RASQAL_QUERY_SCOPE_TYPE_GROUP     6

/* Variable search flags for scope-aware lookup */
#define RASQAL_VAR_SEARCH_LOCAL_ONLY      0x01
#define RASQAL_VAR_SEARCH_INHERIT_PARENT  0x02
#define RASQAL_VAR_SEARCH_CROSS_SCOPE     0x04
#define RASQAL_VAR_SEARCH_LOCAL_FIRST     0x08
#define RASQAL_VAR_SEARCH_PARENT_FIRST    0x10

/* Variable precedence constants */
#define RASQAL_VAR_PRECEDENCE_LOCAL_FIRST    0  /* Local variables take precedence */
#define RASQAL_VAR_PRECEDENCE_NEWEST_FIRST   1  /* Most recent binding wins */
#define RASQAL_VAR_PRECEDENCE_OLDEST_FIRST   2  /* First binding wins (legacy) */

/**
 * rasqal_query_scope:
 * @usage: reference count
 * @scope_id: unique identifier for this scope
 * @scope_name: descriptive name (e.g., "ROOT", "EXISTS_1", "MINUS_2")
 * @scope_type: scope type (RASQAL_QUERY_SCOPE_TYPE_*)
 *
 * @owned_triples: triples belonging exclusively to this scope
 *
 * @local_vars: variables introduced in this scope
 * @visible_vars: computed: parent + local variables (cached)
 *
 * @parent_scope: parent scope for hierarchy (read-only access)
 * @child_scopes: nested scopes owned by this scope
 *
 * @triples_source: data source for this scope's evaluation
 *
 * Hierarchical scope for query execution that ensures proper triple ownership
 * and variable visibility throughout the query processing pipeline.
 */
struct rasqal_query_scope_s {
  /* usage/reference count */
  int usage;

  /* Identity */
  int scope_id;                                /* unique identifier */
  char* scope_name;                           /* "ROOT", "EXISTS_1", "MINUS_2" */
  int scope_type;                             /* RASQAL_QUERY_SCOPE_TYPE_* */

  /* Triple Ownership - CRITICAL */
  raptor_sequence* owned_triples;             /* triples belonging ONLY to this scope */

  /* Variable Management */
  rasqal_variables_table* local_vars;         /* variables introduced in this scope */
  rasqal_variables_table* visible_vars;       /* computed: parent + local (cached) */

  /* Scope Hierarchy */
  rasqal_query_scope* parent_scope;           /* parent context (read-only access) */
  raptor_sequence* child_scopes;              /* nested scopes (owned) */

  /* Evaluation Context */
  rasqal_triples_source* triples_source;      /* data source for this scope */
};

/**
 * rasqal_variable_lookup_context:
 * @current_scope: current execution scope
 * @search_scope: scope to search in
 * @rowsource: current rowsource context
 * @query: query context
 * @search_flags: search configuration flags
 * @binding_precedence: variable precedence rules
 * @resolved_variable: result of variable resolution
 * @defining_scope: scope where variable was defined
 * @resolution_path: scope traversal path for debugging
 *
 * Context structure for scope-aware variable resolution that respects
 * hierarchical scope boundaries and variable precedence rules.
 */
typedef struct rasqal_variable_lookup_context {
  /* Scope Context */
  rasqal_query_scope* current_scope;
  rasqal_query_scope* search_scope;

  /* Rowsource Context */
  rasqal_rowsource* rowsource;
  rasqal_query* query;

  /* Current Row Context */
  rasqal_row* current_row;  /* Current row for variable value lookup */

  /* Search Configuration */
  int search_flags;  /* RASQAL_VAR_SEARCH_* constants */
  int binding_precedence; /* RASQAL_VAR_PRECEDENCE_* constants */

  /* Result Context */
  rasqal_variable* resolved_variable;
  rasqal_query_scope* defining_scope;
  int resolution_path[16]; /* Scope traversal path for debugging */
} rasqal_variable_lookup_context;

/**
 * rasqal_variable_usage_info:
 * @variable: the variable instance
 * @name: variable name
 * @defining_scope: scope where variable was introduced
 * @visible_from_scope: scope where variable is visible
 * @usage_flags: usage tracking flags
 * @binding_order: order for precedence resolution
 * @scope_depth: depth in scope hierarchy
 * @last_resolution_context: last resolution context used
 *
 * Enhanced variable information that tracks scope relationships
 * and usage patterns for proper variable resolution.
 */
typedef struct rasqal_variable_usage_info {
  /* Variable Identity */
  rasqal_variable* variable;
  const char* name;
  
  /* Scope Information */
  rasqal_query_scope* defining_scope;
  rasqal_query_scope* visible_from_scope;
  
  /* Usage Tracking */
  int usage_flags;  /* BOUND, USED, PROJECTED, SHADOWED, etc. */
  int binding_order; /* For precedence resolution */
  int scope_depth;   /* Depth in scope hierarchy */
  
  /* Resolution Context */
  rasqal_variable_lookup_context* last_resolution_context;
} rasqal_variable_usage_info;


/*
 * Graph Pattern
 */
struct rasqal_graph_pattern_s {
  rasqal_query* query;

  /* operator for this graph pattern's contents */
  rasqal_graph_pattern_operator op;
  
  raptor_sequence* triples;          /* ... rasqal_triple*         */
  raptor_sequence* graph_patterns;   /* ... rasqal_graph_pattern*  */

  int start_column;
  int end_column;

  /* the FILTER / LET expression */
  rasqal_expression* filter_expression;

  /* index of the graph pattern in the query (0.. query->graph_pattern_count-1) */
  int gp_index;

  /* Graph literal / SERVICE literal */
  rasqal_literal *origin;

  /* Variable for LET graph pattern */
  rasqal_variable *var;

  /* SELECT projection */
  rasqal_projection* projection;

  /* SELECT modifiers */
  rasqal_solution_modifier* modifier;

  /* SILENT flag for SERVICE graph pattern */
  unsigned int silent : 1;

  /* Sub-pattern flag - marks patterns that are sub-patterns (EXISTS, GRAPH, etc.) */
  unsigned int is_subpattern : 1;

  /* Memory management flag - marks patterns that free their triples sequence */
  unsigned int frees_triples : 1;

  /* SELECT graph pattern: sequence of #rasqal_data_graph */
  raptor_sequence* data_graphs;

  /* VALUES bindings for VALUES and sub-SELECT graph patterns */
  rasqal_bindings* bindings;

  /* Variable scope for this graph pattern - REFERENCE to execution contextv*/
  rasqal_query_scope* execution_scope;
};

rasqal_graph_pattern* rasqal_new_basic_graph_pattern(rasqal_query* query, raptor_sequence* triples, int start_column, int end_column, int frees_triples);
rasqal_graph_pattern* rasqal_new_graph_pattern_from_sequence(rasqal_query* query, raptor_sequence* graph_patterns, rasqal_graph_pattern_operator op);
rasqal_graph_pattern* rasqal_new_filter_graph_pattern(rasqal_query* query, rasqal_expression* expr);
rasqal_graph_pattern* rasqal_new_bind_graph_pattern(rasqal_query *query, rasqal_variable *var, rasqal_expression *expr);  
rasqal_graph_pattern* rasqal_new_select_graph_pattern(rasqal_query *query, rasqal_projection* projection, raptor_sequence* data_graphs, rasqal_graph_pattern* where, rasqal_solution_modifier* modifier, rasqal_bindings* bindings);
rasqal_graph_pattern* rasqal_new_single_graph_pattern(rasqal_query* query, rasqal_graph_pattern_operator op, rasqal_graph_pattern* single);
rasqal_graph_pattern* rasqal_new_exists_graph_pattern(rasqal_query* query, rasqal_graph_pattern* pattern);
rasqal_graph_pattern* rasqal_new_not_exists_graph_pattern(rasqal_query* query, rasqal_graph_pattern* pattern);
rasqal_graph_pattern* rasqal_new_values_graph_pattern(rasqal_query* query, rasqal_bindings* bindings);
void rasqal_free_graph_pattern(rasqal_graph_pattern* gp);
void rasqal_graph_pattern_adjust(rasqal_graph_pattern* gp, int offset);
void rasqal_graph_pattern_set_origin(rasqal_graph_pattern* graph_pattern, rasqal_literal* origin);
int rasqal_graph_pattern_write(rasqal_graph_pattern* gp, raptor_iostream* iostr);



/* Query Scope Functions - Phase 1 Implementation */
rasqal_query_scope* rasqal_new_query_scope(rasqal_query* query, int scope_type, rasqal_query_scope* parent_scope);
void rasqal_free_query_scope(rasqal_query_scope* scope);
int rasqal_query_scope_compute_visible_variables(rasqal_query_scope* scope);
int rasqal_query_scope_add_child_scope(rasqal_query_scope* parent, rasqal_query_scope* child);
int rasqal_query_scope_add_triple(rasqal_query_scope* scope, rasqal_triple* triple);
int rasqal_query_scope_bind_row_variables(rasqal_query_scope* scope, rasqal_row* row, rasqal_rowsource* rowsource);
rasqal_query_scope* rasqal_query_scope_get_root(rasqal_query_scope* scope);

/* Scope helper functions for variable correlation analysis */
int rasqal_scope_provides_variable(rasqal_query_scope* scope, const char* var_name);
int rasqal_scope_defines_variable(rasqal_query_scope* scope, const char* var_name);



/* Scope-Aware Variable Resolution Functions - Phase 2 Implementation */
rasqal_variable* rasqal_resolve_variable_with_scope(const char* var_name, rasqal_variable_lookup_context* context);
rasqal_variable* rasqal_rowsource_get_variable_by_name_with_scope(rasqal_rowsource* rowsource, const char* name, rasqal_query_scope* scope);
int rasqal_rowsource_get_variable_offset_by_name_with_scope(rasqal_rowsource* rowsource, const char* name, rasqal_query_scope* scope);

/* Expression Variable Resolution with Scope */
int rasqal_expression_resolve_variables_with_scope(rasqal_expression* expr, rasqal_variable_lookup_context* context);
rasqal_literal* rasqal_expression_evaluate_with_scope(rasqal_expression* expr, rasqal_evaluation_context* eval_context, rasqal_variable_lookup_context* scope_context);

/* Scope Boundary and Access Control Functions */
int rasqal_validate_scope_boundaries(rasqal_query_scope* scope, rasqal_variable* variable);
int rasqal_check_cross_scope_access(rasqal_query_scope* from_scope, rasqal_query_scope* to_scope, rasqal_variable* variable);

/* Migration Interface Functions */
rasqal_variable* rasqal_get_variable_usage_static(const char* name, int scope_id);
rasqal_variable* rasqal_get_variable_usage_dynamic(const char* name, rasqal_variable_lookup_context* ctx);
rasqal_variable* rasqal_get_variable_usage_hybrid(const char* name, rasqal_variable_lookup_context* ctx, int fallback_to_static);

/**
 * rasqal_var_use_map_flags:
 * @RASQAL_VAR_USE_IN_SCOPE: variable is in-scope in this GP
 * @RASQAL_VAR_USE_MENTIONED_HERE: variable is mentioned/used in this GP
 * @RASQAL_VAR_USE_BOUND_HERE: variable is bound in this GP
 */
typedef enum {
  RASQAL_VAR_USE_IN_SCOPE       = 1 << 0,
  RASQAL_VAR_USE_MENTIONED_HERE = 1 << 1,
  RASQAL_VAR_USE_BOUND_HERE     = 1 << 2
} rasqal_var_use_map_flags;


/**
 * rasqal_var_use_map_offset:
 * @RASQAL_VAR_USE_MAP_OFFSET_VERBS: Variables in query verbs: ASK: never, SELECT: project-expressions (SPARQL 1.1), CONSTRUCT: in constructed triple patterns, DESCRIBE: in argument (SPARQL 1.0)
 * @RASQAL_VAR_USE_MAP_OFFSET_GROUP_BY: Variables in GROUP BY expr/var (SPARQL 1.1)
 * @RASQAL_VAR_USE_MAP_OFFSET_HAVING: Variables in HAVING expr (SPARQL 1.1)
 * @RASQAL_VAR_USE_MAP_OFFSET_ORDER_BY: Variables in ORDER BY list-of-expr (SPARQL 1.0)
 * @RASQAL_VAR_USE_MAP_OFFSET_VALUES: Variables bound in VALUES (SPARQL 1.1)
 * @RASQAL_VAR_USE_MAP_OFFSET_LAST: internal
 *
 * Offsets into variables use-map for non-graph pattern parts of #rasqal_query structure
 */
typedef enum {
  RASQAL_VAR_USE_MAP_OFFSET_VERBS    = 0,
  RASQAL_VAR_USE_MAP_OFFSET_GROUP_BY = 1,
  RASQAL_VAR_USE_MAP_OFFSET_HAVING   = 2,
  RASQAL_VAR_USE_MAP_OFFSET_ORDER_BY = 3,
  RASQAL_VAR_USE_MAP_OFFSET_VALUES   = 4,
  RASQAL_VAR_USE_MAP_OFFSET_LAST     = RASQAL_VAR_USE_MAP_OFFSET_VALUES
} rasqal_var_use_map_offset;


/**
 * rasqal_triples_use_map_flags:
 * @RASQAL_TRIPLES_USE_SUBJECT: variable used in subject of this triple
 * @RASQAL_TRIPLES_USE_PREDICATE: ditto predicate
 * @RASQAL_TRIPLES_USE_OBJECT: ditto object
 * @RASQAL_TRIPLES_USE_GRAPH: ditto graph
 * @RASQAL_TRIPLES_BOUND_SUBJECT: variable bound in subject of this triple
 * @RASQAL_TRIPLES_BOUND_PREDICATE: ditto predicate
 * @RASQAL_TRIPLES_BOUND_OBJECT: ditto object
 * @RASQAL_TRIPLES_BOUND_GRAPH: ditto graph
 * @RASQAL_TRIPLES_USE_MASK: mask to check if variable is used in any part of TP
 * @RASQAL_TRIPLES_BOUND_MASK: mask to check if variable is bound in any part of TP (a single variable can be bound at most once in a triple pattern)
 */
typedef enum {
  RASQAL_TRIPLES_USE_SUBJECT     = RASQAL_TRIPLE_SUBJECT,
  RASQAL_TRIPLES_USE_PREDICATE   = RASQAL_TRIPLE_PREDICATE,
  RASQAL_TRIPLES_USE_OBJECT      = RASQAL_TRIPLE_OBJECT,
  RASQAL_TRIPLES_USE_GRAPH       = RASQAL_TRIPLE_GRAPH,
  RASQAL_TRIPLES_BOUND_SUBJECT   = RASQAL_TRIPLE_SUBJECT   << 4,
  RASQAL_TRIPLES_BOUND_PREDICATE = RASQAL_TRIPLE_PREDICATE << 4,
  RASQAL_TRIPLES_BOUND_OBJECT    = RASQAL_TRIPLE_OBJECT    << 4,
  RASQAL_TRIPLES_BOUND_GRAPH     = RASQAL_TRIPLE_GRAPH     << 4,
  RASQAL_TRIPLES_USE_MASK        = (RASQAL_TRIPLES_USE_SUBJECT | RASQAL_TRIPLES_USE_PREDICATE | RASQAL_TRIPLES_USE_OBJECT | RASQAL_TRIPLES_USE_GRAPH),
  RASQAL_TRIPLES_BOUND_MASK      = (RASQAL_TRIPLES_BOUND_SUBJECT | RASQAL_TRIPLES_BOUND_PREDICATE | RASQAL_TRIPLES_BOUND_OBJECT | RASQAL_TRIPLES_BOUND_GRAPH)
} rasqal_triples_use_map_flags;


/*
 * A query in some query language
 */
struct rasqal_query_s {
  rasqal_world* world; /* world object */

  int usage; /* reference count - 1 for itself, plus for query_results */
  
  unsigned char* query_string;
  size_t query_string_length; /* length including NULs */

  raptor_namespace_stack* namespaces;

  /* query graph pattern, containing the sequence of graph_patterns below */
  rasqal_graph_pattern* query_graph_pattern;
  
  /* the query verb - in SPARQL terms: SELECT, CONSTRUCT, DESCRIBE or ASK */
  rasqal_query_verb verb;

  /* WAS: selects - sequence of rasqal_variable# */
  raptor_sequence* unused10;     /* ... rasqal_variable* names only */

  /* sequences of ... */
  raptor_sequence* data_graphs; /* ... rasqal_data_graph*          */
  /* NOTE: Cannot assume that triples are in any of 
   * graph pattern use / query execution / document order 
   */
  raptor_sequence* triples;     /* ... rasqal_triple*              */
  raptor_sequence* prefixes;    /* ... rasqal_prefix*              */
  raptor_sequence* constructs;  /* ... rasqal_triple*       SPARQL */
  raptor_sequence* optional_triples; /* ... rasqal_triple*  SPARQL */
  raptor_sequence* describes;   /* ... rasqal_literal* (var or URIs) SPARQL */

  /* WAS: distinct 0..2; now projection->distinct */
  unsigned int unused9;

  /* WAS: result limit LIMIT (>=0) or <0 if not given */
  int unused4;

  /* WAS: result offset OFFSET (>=0) or <0 if not given */
  int unused5;

  /* WAS: wildcard flag; now projection->wildcard  */
  int unused12;

  /* flag: non-0 if query has been prepared */
  int prepared;

  rasqal_variables_table* vars_table;

  /* WAS: The number of selected variables: these are always the first
   * in the variables table and are the ones returned to the user.
   */
  int unused11;

  /* can be filled with error location information */
  raptor_locator locator;

  /* base URI of this query for resolving relative URIs in queries */
  raptor_uri* base_uri;

  /* non 0 if query had fatal error in parsing and cannot be executed */
  int failed;

  /* stuff for our user */
  void* user_data;

  /* former state for generating blank node IDs; now in world object */
  int unused1;
  char *unused2;
  size_t unused3;

  /* query engine specific stuff */
  void* context;

  struct rasqal_query_language_factory_s* factory;

  rasqal_triples_source_factory* triples_source_factory;

  /* sequence of query results made from this query */
  raptor_sequence* results;

  /* incrementing counter for declaring prefixes in order of appearance */
  int prefix_depth;

  /* incrementing counter for generating unique scope IDs within this query */
  int scope_id_counter;

  /* WAS: sequence of order condition expressions */
  void* unused6;

  /* WAS: sequence of group by condition expressions */
  void* unused7;

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
  char* query_results_formatter_name;

  /* flag: non-0 if EXPLAIN was given */
  int explain;

  /* INTERNAL lexer internal data */
  void* lexer_user_data;

  /* INTERNAL flag for now: non-0 to store results otherwise lazy eval results */
  int store_results;

  /* sequence of #rasqal_update_operation when @verb is
   * INSERT (deprecated), DELETE (deprecated) or UPDATE
   */
  raptor_sequence* updates;

  /* WAS: sequence of (group by ...) having condition expressions */
  raptor_sequence* unused8;
  
  /* INTERNAL solution modifier */
  rasqal_solution_modifier* modifier;

  /* INTERNAL SELECT bindings */
  rasqal_bindings* bindings;

  /* INTERNAL static structure for expression evaluation*/
  rasqal_evaluation_context *eval_context;

  /* INTERNAL flag: non-0 if user set a random seed via RASQAL_FEATURE_RAND_SEED */
  unsigned int user_set_rand : 1;

  /* Variable projection (or NULL when invalid such as for ASK) */
  rasqal_projection* projection;
};


/*
 * A query language factory for a query language.
 *
 * This structure is about turning a query syntax string into a
 * #rasqal_query structure.  It does not deal with execution of the
 * query in any manner.
 */
struct rasqal_query_language_factory_s {
  rasqal_world* world;
  
  struct rasqal_query_language_factory_s* next;

  /* static description that the query language registration initialises */
  raptor_syntax_description desc;
  
  /* the rest of this structure is populated by the
     query-language-specific register function */
  size_t context_length;
  
  /* create a new query */
  int (*init)(rasqal_query* rq, const char *name);
  
  /* destroy a query */
  void (*terminate)(rasqal_query* rq);
  
  /* prepare a query */
  int (*prepare)(rasqal_query* rq);
  
  /* finish the query language factory */
  void (*finish_factory)(rasqal_query_language_factory* factory);

  /* Write a string to an iostream in escaped form suitable for the query */
  int (*iostream_write_escaped_counted_string)(rasqal_query* rq, raptor_iostream* iostr, const unsigned char* string, size_t len);
};


#define RASQAL_ROW_FLAG_WEAK_ROWSOURCE 0x01

/*
 * A row of values from a query result, usually generated by a rowsource
 */
struct rasqal_row_s {
  /* reference count */
  int usage;

  /* Rowsource this row is associated with (or NULL if none) */
  rasqal_rowsource* rowsource;

  /* current row number in the sequence of rows*/
  int offset;

  /* values for each variable in the query sequence of values */
  int size;
  rasqal_literal** values;

  /* literal values for ORDER BY expressions evaluated for this row */
  /* number of expressions (can be 0) */
  int order_size;
  rasqal_literal** order_values;

  /* Group ID */
  int group_id;

  /* Bit mask of flags: bit 0 = WEAK ROWSOURCE */
  unsigned int flags;
};


typedef struct rasqal_map_s rasqal_map;

/**
 * rasqal_join_type:
 * @RASQAL_JOIN_TYPE_UNKNOWN: unknown join type
 * @RASQAL_JOIN_TYPE_NATURAL: natural join.  returns compatible rows and no NULLs
 * @RASQAL_JOIN_TYPE_LEFT: left join.  returns compatible rows plus rows from left rowsource that are not compatible or fail filter condition
 *
 * Rowsource join type.
 */
typedef enum {
  RASQAL_JOIN_TYPE_UNKNOWN,
  RASQAL_JOIN_TYPE_NATURAL,
  RASQAL_JOIN_TYPE_LEFT
} rasqal_join_type;



/* rasqal_rowsource_aggregation.c */
rasqal_rowsource* rasqal_new_aggregation_rowsource(rasqal_world *world, rasqal_query* query, rasqal_rowsource* rowsource, raptor_sequence* exprs_seq, raptor_sequence* vars_seq);

/* rasqal_rowsource_empty.c */
rasqal_rowsource* rasqal_new_empty_rowsource(rasqal_world *world, rasqal_query* query);

/* rasqal_rowsource_exists.c */
rasqal_rowsource* rasqal_new_exists_rowsource(rasqal_world *world, rasqal_query* query, rasqal_triples_source* triples_source, rasqal_graph_pattern* exists_pattern, rasqal_row* outer_row, rasqal_literal* graph_origin, int is_negated);

/* rasqal_rowsource_extend.c */
rasqal_rowsource* rasqal_new_extend_rowsource(rasqal_world *world, rasqal_query* query, rasqal_rowsource* input_rs, rasqal_variable* var, rasqal_expression* expr, rasqal_query_scope* execution_scope);

/* rasqal_rowsource_engine.c */
rasqal_rowsource* rasqal_new_execution_rowsource(rasqal_query_results* query_results);

/* rasqal_rowsource_bindings.c */
rasqal_rowsource* rasqal_new_bindings_rowsource(rasqal_world *world, rasqal_query *query, rasqal_bindings* bindings);

/* rasqal_rowsource_values.c */
rasqal_rowsource* rasqal_new_values_rowsource(rasqal_world *world, rasqal_query *query, rasqal_bindings* bindings);

/* rasqal_rowsource_distinct.c */
rasqal_rowsource* rasqal_new_distinct_rowsource(rasqal_world *world, rasqal_query *query, rasqal_rowsource* rs);

/* rasqal_rowsource_filter.c */
rasqal_rowsource* rasqal_new_filter_rowsource(rasqal_world *world, rasqal_query *query, rasqal_rowsource* rs, rasqal_expression* expr, rasqal_query_scope* evaluation_scope);

/* rasqal_rowsource_graph.c */
rasqal_rowsource* rasqal_new_graph_rowsource(rasqal_world *world, rasqal_query *query, rasqal_rowsource* rowsource, rasqal_variable *var);

/* rasqal_rowsource_groupby.c */
rasqal_rowsource* rasqal_new_groupby_rowsource(rasqal_world *world, rasqal_query* query, rasqal_rowsource* rowsource, raptor_sequence* exprs_seq);

/* rasqal_rowsource_having.c */
rasqal_rowsource* rasqal_new_having_rowsource(rasqal_world *world, rasqal_query *query, rasqal_rowsource* rowsource, raptor_sequence* exprs_seq);

/* rasqal_rowsource_join.c */
rasqal_rowsource* rasqal_new_join_rowsource(rasqal_world *world, rasqal_query* query, rasqal_rowsource* left, rasqal_rowsource* right, rasqal_join_type join_type, rasqal_expression *expr, rasqal_query_scope *scope);

/* rasqal_rowsource_project.c */
rasqal_rowsource* rasqal_new_project_rowsource(rasqal_world *world, rasqal_query *query, rasqal_rowsource* rowsource, raptor_sequence* projection_variables, rasqal_query_scope* scope);

/* rasqal_rowsource_rowsequence.c */
rasqal_rowsource* rasqal_new_rowsequence_rowsource(rasqal_world *world, rasqal_query* query, rasqal_variables_table* vt, raptor_sequence* rows_seq, raptor_sequence* vars_seq);

/* rasqal_rowsource_slice.c */
rasqal_rowsource* rasqal_new_slice_rowsource(rasqal_world *world, rasqal_query *query, rasqal_rowsource* rowsource, int limit, int offset);

/* rasqal_rowsource_service.c */
rasqal_rowsource* rasqal_new_service_rowsource(rasqal_world *world, rasqal_query* query, raptor_uri* service_uri, const unsigned char* query_string, raptor_sequence* data_graphs, unsigned int rs_flags);
  
/* rasqal_rowsource_sort.c */
rasqal_rowsource* rasqal_new_sort_rowsource(rasqal_world *world, rasqal_query *query, rasqal_rowsource *rowsource, raptor_sequence* order_seq, int distinct);

/* rasqal_rowsource_triples.c */
rasqal_rowsource* rasqal_new_triples_rowsource(rasqal_world *world, rasqal_query* query, rasqal_triples_source* triples_source, raptor_sequence* triples, int start_column, int end_column);
rasqal_rowsource* rasqal_new_triples_rowsource_with_vars(rasqal_world *world, rasqal_variables_table* vars_table, rasqal_triples_source* triples_source, raptor_sequence* triples, int start_column, int end_column);

/* Enhanced BGP constructor with scope support - Phase 3 */
rasqal_rowsource* rasqal_new_triples_rowsource_with_scope(rasqal_world *world, rasqal_variables_table* vars_table, rasqal_triples_source* triples_source, rasqal_query_scope* scope, int start_column, int end_column);

/* rasqal_rowsource_union.c */
rasqal_rowsource* rasqal_new_union_rowsource(rasqal_world *world, rasqal_query* query, rasqal_rowsource* left, rasqal_rowsource* right, rasqal_query_scope *scope);

/* rasqal_rowsource_minus.c */
rasqal_rowsource* rasqal_new_minus_rowsource(rasqal_world *world,
                                             rasqal_query* query,
                                             rasqal_rowsource* lhs_rowsource,
                                             rasqal_rowsource* rhs_rowsource,
                                             int needs_correlation);


/**
 * rasqal_rowsource_init_func:
 * @context: stream context data
 *
 * Handler function for #rasqal_rowsource initialising.
 *
 * Return value: non-0 on failure.
 */
typedef int (*rasqal_rowsource_init_func) (rasqal_rowsource* rowsource, void *user_data);

/**
 * rasqal_rowsource_finish_func:
 * @user_data: user data
 *
 * Handler function for #rasqal_rowsource terminating.
 *
 * Return value: non-0 on failure
 */
typedef int (*rasqal_rowsource_finish_func) (rasqal_rowsource* rowsource, void *user_data);

/**
 * rasqal_rowsource_ensure_variables_func
 * @rowsource: #rasqal_rowsource
 * @user_data: user data
 *
 * Handler function for ensuring rowsource variables fields are initialised
 *
 * Return value: non-0 on failure
 */
typedef int (*rasqal_rowsource_ensure_variables_func) (rasqal_rowsource* rowsource, void *user_data);

/**
 * rasqal_rowsource_read_row_func
 * @user_data: user data
 *
 * Handler function for returning the next result row
 *
 * Return value: a query result row or NULL if exhausted
 */
typedef rasqal_row* (*rasqal_rowsource_read_row_func) (rasqal_rowsource* rowsource, void *user_data);


/**
 * rasqal_rowsource_read_all_rows_func
 * @user_data: user data
 *
 * Handler function for returning all rows as a sequence
 *
 * Return value: a sequence of result rows (which may be size 0) or NULL if exhausted
 */
typedef raptor_sequence* (*rasqal_rowsource_read_all_rows_func) (rasqal_rowsource* rowsource, void *user_data);


/**
 * rasqal_rowsource_reset_func
 * @user_data: user data
 *
 * Handler function for resetting a rowsource to generate the same set of rows
 *
 * Return value: non-0 on failure
 */
typedef int (*rasqal_rowsource_reset_func) (rasqal_rowsource* rowsource, void *user_data);


/* bit flags */
#define RASQAL_ROWSOURCE_REQUIRE_RESET (1 << 0)

/**
 * rasqal_rowsource_set_requirements_func
 * @user_data: user data
 * @flags: bit flags
 *
 * Handler function for one rowsource setting requirements on inner rowsources
 *
 * Return value: non-0 on failure
 */
typedef int (*rasqal_rowsource_set_requirements_func) (rasqal_rowsource* rowsource, void *user_data, unsigned int flags);


/**
 * rasqal_rowsource_get_inner_rowsource_func
 * @user_data: user data
 * @offset: offset
 *
 * Handler function for getting an inner rowsource at an offset
 *
 * Return value: rowsource object or NULL if offset out of range
 */
typedef rasqal_rowsource* (*rasqal_rowsource_get_inner_rowsource_func) (rasqal_rowsource* rowsource, void *user_data, int offset);


/**
 * rasqal_rowsource_set_origin_func
 * @user_data: user data
 * @origin: Graph URI literal
 *
 * Handler function for setting the rowsource origin (GRAPH)
 *
 * Return value: non-0 on failure
 */
typedef int (*rasqal_rowsource_set_origin_func) (rasqal_rowsource* rowsource, void *user_data, rasqal_literal *origin);


/**
 * rasqal_rowsource_handler:
 * @version: API version - 1
 * @name: rowsource name for debugging
 * @init:  initialisation handler - optional, called at most once (V1)
 * @finish: finishing handler - optional, called at most once (V1)
 * @ensure_variables: update variables handler- optional, called at most once (V1)
 * @read_row: read row handler - this or @read_all_rows required (V1)
 * @read_all_rows: read all rows handler - this or @read_row required (V1)
 * @reset: reset rowsource to starting state handler - optional (V1)
 * @set_requirements: set requirements flag handler - optional (V1)
 * @get_inner_rowsource: get inner rowsource handler - optional if has no inner rowsources (V1)
 * @set_origin: set origin (GRAPH) handler - optional (V1)
 *
 * Row Source implementation factory handler structure.
 * 
 */
typedef struct {
  int version;
  const char* name;
  /* API V1 methods */
  rasqal_rowsource_init_func                 init;
  rasqal_rowsource_finish_func               finish;
  rasqal_rowsource_ensure_variables_func     ensure_variables;
  rasqal_rowsource_read_row_func             read_row;
  rasqal_rowsource_read_all_rows_func        read_all_rows;
  rasqal_rowsource_reset_func                reset;
  rasqal_rowsource_set_requirements_func     set_requirements;
  rasqal_rowsource_get_inner_rowsource_func  get_inner_rowsource;
  rasqal_rowsource_set_origin_func           set_origin;
} rasqal_rowsource_handler;


/**
 * rasqal_row_compatible:
 * @variables_table: variables table
 * @rowsource1: first rowsource
 * @rowsource2: second rowsource
 * @variables_count: number of variables in the map
 * @variables_in_both_rows_count: number of shared variables
 * @defined_in_map: of size @variables_count
 *
 * Lookup data constructed for two rowsources to enable quick
 * checking if rows from the two rowsource are compatible with
 * rasqal_row_compatible_check()
 *
 */
typedef struct {
  rasqal_variables_table* variables_table;
  rasqal_rowsource *first_rowsource;
  rasqal_rowsource *second_rowsource;
  int variables_count;
  int variables_in_both_rows_count;
  int* defined_in_map;
} rasqal_row_compatible;


typedef struct rasqal_results_compare_s rasqal_results_compare;


/* 
 * Rowsource Internal flags
 *
 * RASQAL_ROWSOURCE_FLAGS_SAVE_ROWS: need to save all rows in
 * @rows_sequence for reset operation
 *
 * RASQAL_ROWSOURCE_FLAGS_SAVED_ROWS: have saved rows ready for reply
 */
#define RASQAL_ROWSOURCE_FLAGS_SAVE_ROWS  0x01
#define RASQAL_ROWSOURCE_FLAGS_SAVED_ROWS 0x02

/**
 * rasqal_rowsource:
 * @world: rasqal world
 * @query: query that this may be associated with (or NULL)
 * @flags: flags - none currently defined.
 * @user_data: rowsource handler data
 * @handler: rowsource handler pointer
 * @finished: non-0 if rowsource has been exhausted
 * @count:  number of rows returned
 * @updated_variables: non-0 if ensure_variables factory method has been called to get the variables_sequence updated
 * @vars_table: variables table where variables used in this row are declared/owned
 * @variables_sequence: variables declared in this row from @vars_table
 * @size: number of variables in @variables_sequence
 * @rows_sequence: stored sequence of rows for use by rasqal_rowsource_read_row() (or NULL)
 * @offset: size of @rows_sequence
 * @generate_group: non-0 to generate a group (ID 0) around all the returned rows, if there is no grouping returned.
 * @usage: reference count
 *
 * Rasqal Row Source class providing a sequence of rows of values similar to a SQL table.
 *
 * The table has @size columns which are #rasqal_variable names that
 * are declared in the associated variables table.
 * @variables_sequence contains the ordered projection of the
 * variables for the columns for this row sequence, from the full set
 * of variables in @vars_table.
 * 
 * Each row has @size #rasqal_literal values for the variables or
 * NULL if unset.
 *
 * Row sources are constructed indirectly via an @handler passed
 * to the rasqal_new_rowsource_from_handler() constructor.
 *
 * The main methods are rasqal_rowsource_read_row() to read one row
 * and rasqal_rowsource_read_all_rows() to get all rows as a
 * sequence, draining the row source.
 * rasqal_rowsource_get_rows_count() returns the current number of
 * rows that have been read which is only useful in the read one row
 * case.
 * 
 * The variables associated with a rowsource can be read by
 * rasqal_rowsource_get_variable_by_offset() and
 * rasqal_rowsource_get_variable_offset_by_name() which all are
 * offsets into @variables_sequence but refer to variables owned by
 * the full internal variables table @vars_table
 *
 * The @rows_sequence and @offset variables are used by the
 * rasqal_rowsource_read_row() function when operating over a handler
 * that will only return a full sequence: handler->read_all_rows is NULL.
 */
struct rasqal_rowsource_s
{
  rasqal_world* world;

  rasqal_query* query;
  
  int flags;
  
  void *user_data;

  const rasqal_rowsource_handler* handler;

  unsigned int finished : 1;

  int count;

  int updated_variables;

  rasqal_variables_table* vars_table;

  raptor_sequence* variables_sequence;
  
  int size;

  raptor_sequence* rows_sequence;

  int offset;

  unsigned int generate_group : 1;

  int usage;
};


/* rasqal_rowsource.c */
rasqal_rowsource* rasqal_new_rowsource_from_handler(rasqal_world *world, rasqal_query* query, void* user_data, const rasqal_rowsource_handler *handler, rasqal_variables_table* vars_table, int flags);
rasqal_rowsource* rasqal_new_rowsource_from_rowsource(rasqal_rowsource* rowsource);
void rasqal_free_rowsource(rasqal_rowsource *rowsource);

rasqal_row* rasqal_rowsource_read_row(rasqal_rowsource *rowsource);
int rasqal_rowsource_get_rows_count(rasqal_rowsource *rowsource);
raptor_sequence* rasqal_rowsource_read_all_rows(rasqal_rowsource *rowsource);
int rasqal_rowsource_get_size(rasqal_rowsource *rowsource);
int rasqal_rowsource_add_variable(rasqal_rowsource *rowsource, rasqal_variable* v);
rasqal_variable* rasqal_rowsource_get_variable_by_offset(rasqal_rowsource *rowsource, int offset);
int rasqal_rowsource_get_variable_offset_by_name(rasqal_rowsource *rowsource, const unsigned char* name);
int rasqal_rowsource_copy_variables(rasqal_rowsource *dest_rowsource, rasqal_rowsource *src_rowsource);
void rasqal_rowsource_print_row_sequence(rasqal_rowsource* rowsource,raptor_sequence* seq, FILE* fh);
int rasqal_rowsource_reset(rasqal_rowsource* rowsource);
int rasqal_rowsource_set_requirements(rasqal_rowsource* rowsource, unsigned int requirement);
rasqal_rowsource* rasqal_rowsource_get_inner_rowsource(rasqal_rowsource* rowsource, int offset);
int rasqal_rowsource_write(rasqal_rowsource *rowsource,  raptor_iostream *iostr);
void rasqal_rowsource_print(rasqal_rowsource* rs, FILE* fh);
int rasqal_rowsource_ensure_variables(rasqal_rowsource *rowsource);
int rasqal_rowsource_set_origin(rasqal_rowsource* rowsource, rasqal_literal *literal);
int rasqal_rowsource_request_grouping(rasqal_rowsource* rowsource);
void rasqal_rowsource_remove_all_variables(rasqal_rowsource *rowsource);

typedef struct rasqal_query_results_format_factory_s rasqal_query_results_format_factory;

typedef int (*rasqal_query_results_init_func)(rasqal_query_results_formatter* formatter, const char* name);

typedef void (*rasqal_query_results_finish_func)(rasqal_query_results_formatter* formatter);

typedef int (*rasqal_query_results_write_func)(rasqal_query_results_formatter* formatter, raptor_iostream *iostr, rasqal_query_results* results, raptor_uri *base_uri);

typedef rasqal_rowsource* (*rasqal_query_results_get_rowsource_func)(rasqal_query_results_formatter* formatter, rasqal_world* world, rasqal_variables_table* vars_table, raptor_iostream *iostr, raptor_uri *base_uri, unsigned int flags);

typedef int (*rasqal_query_results_recognise_syntax_func)(struct rasqal_query_results_format_factory_s* factory, const unsigned char *buffer, size_t len, const unsigned char *identifier, const unsigned char *suffix, const char *mime_type);

typedef int (*rasqal_query_results_get_boolean_func)(rasqal_query_results_formatter *formatter, rasqal_world* world, raptor_iostream *iostr, raptor_uri *base_uri, unsigned int flags);


typedef int (*rasqal_rowsource_visit_fn)(rasqal_rowsource* rowsource, void *user_data);

int rasqal_rowsource_visit(rasqal_rowsource* rowsource, rasqal_rowsource_visit_fn fn, void *user_data);


struct rasqal_query_results_format_factory_s {
  rasqal_world* world;

  struct rasqal_query_results_format_factory_s* next;

  /* static desc that the parser registration initialises */
  raptor_syntax_description desc;

  /* Memory to allocate for per-formatter data */
  int context_length;
  
  /* format initialisation (OPTIONAL) */
  rasqal_query_results_init_func init;

  /* format initialisation (OPTIONAL) */
  rasqal_query_results_finish_func finish;

  /* format writer: READ from results, WRITE syntax (using base URI) to iostr */
  rasqal_query_results_write_func write;

  /* format get rowsource: get a rowsource that will return a sequence of rows from an iostream */
  rasqal_query_results_get_rowsource_func get_rowsource;

  /* recognize a format (OPTIONAL) */
  rasqal_query_results_recognise_syntax_func recognise_syntax;

  /* get a boolean result (OPTIONAL) */
  rasqal_query_results_get_boolean_func get_boolean;
};


/*
 * A query results formatter for some query_results
 */
struct rasqal_query_results_formatter_s {
  rasqal_query_results_format_factory* factory;

  /* Per-formatter data */
  void* context;
};

/* rasqal_results_formats.c */
rasqal_rowsource* rasqal_query_results_formatter_get_read_rowsource(rasqal_world *world, raptor_iostream *iostr, rasqal_query_results_formatter* formatter, rasqal_variables_table* vars_table, raptor_uri *base_uri, unsigned int flags);


typedef struct {
  rasqal_world *world;
  raptor_sequence *triples;
  rasqal_literal *value;
} rasqal_formula;


/* rasqal_datetime.c */
int rasqal_xsd_datetime_check(const char* string);
int rasqal_xsd_date_check(const char* string);


/* rasqal_dataset.c */
typedef struct rasqal_dataset_s rasqal_dataset;
typedef struct rasqal_dataset_term_iterator_s rasqal_dataset_term_iterator;
typedef struct rasqal_dataset_triples_iterator_s rasqal_dataset_triples_iterator;

rasqal_dataset* rasqal_new_dataset(rasqal_world* world);
void rasqal_free_dataset(rasqal_dataset* ds);
int rasqal_dataset_load_graph_iostream(rasqal_dataset* ds, const char* name, raptor_iostream* iostr, raptor_uri* base_uri);
int rasqal_dataset_load_graph_uri(rasqal_dataset* ds, const char* name, raptor_uri* uri, raptor_uri* base_uri);
void rasqal_free_dataset_term_iterator(rasqal_dataset_term_iterator* iter);
rasqal_literal* rasqal_dataset_term_iterator_get(rasqal_dataset_term_iterator* iter);
int rasqal_dataset_term_iterator_next(rasqal_dataset_term_iterator* iter);
rasqal_dataset_term_iterator* rasqal_dataset_get_sources_iterator(rasqal_dataset* ds, rasqal_literal* predicate, rasqal_literal* object);
rasqal_dataset_term_iterator* rasqal_dataset_get_targets_iterator(rasqal_dataset* ds, rasqal_literal* subject, rasqal_literal* predicate);
rasqal_literal* rasqal_dataset_get_source(rasqal_dataset* ds, rasqal_literal* predicate, rasqal_literal* object);
rasqal_literal* rasqal_dataset_get_target(rasqal_dataset* ds, rasqal_literal* subject, rasqal_literal* predicate);
rasqal_dataset_triples_iterator* rasqal_dataset_get_triples_iterator(rasqal_dataset* ds);
void rasqal_free_dataset_triples_iterator(rasqal_dataset_triples_iterator* ti);
rasqal_triple* rasqal_dataset_triples_iterator_get(rasqal_dataset_triples_iterator* ti);
int rasqal_dataset_triples_iterator_next(rasqal_dataset_triples_iterator* ti);
int rasqal_dataset_print(rasqal_dataset* ds, FILE *fh);


/* rasqal_general.c */
char* rasqal_vsnprintf(const char* message, va_list arguments);
unsigned char* rasqal_world_generate_bnodeid(rasqal_world* world, unsigned char *user_bnodeid);
int rasqal_world_reset_now(rasqal_world* world);
struct timeval* rasqal_world_get_now_timeval(rasqal_world* world);


typedef enum {
  /* Warnings in 0..100 range.  Warn if LEVEL < world->warning_level */
  RASQAL_WARNING_LEVEL_MAYBE_ERROR  = 10,
  RASQAL_WARNING_LEVEL_STYLE        = 30,
  RASQAL_WARNING_LEVEL_STRICT_STYLE = 90,

  /* Default warning level */
  RASQAL_WARNING_LEVEL_DEFAULT      = 50,
  RASQAL_WARNING_LEVEL_MAX          = 100,

  /* Warnings level in code base */
  RASQAL_WARNING_LEVEL_DUPLICATE_VARIABLE = RASQAL_WARNING_LEVEL_STYLE,
  RASQAL_WARNING_LEVEL_VARIABLE_UNUSED    = RASQAL_WARNING_LEVEL_STYLE,
  RASQAL_WARNING_LEVEL_MULTIPLE_BG_GRAPHS = RASQAL_WARNING_LEVEL_STYLE,
  RASQAL_WARNING_LEVEL_QUERY_SYNTAX       = RASQAL_WARNING_LEVEL_STYLE,

  RASQAL_WARNING_LEVEL_NOT_IMPLEMENTED      = RASQAL_WARNING_LEVEL_MAYBE_ERROR,
  RASQAL_WARNING_LEVEL_MISSING_SUPPORT      = RASQAL_WARNING_LEVEL_MAYBE_ERROR,
  RASQAL_WARNING_LEVEL_BAD_TRIPLE           = RASQAL_WARNING_LEVEL_MAYBE_ERROR,
  RASQAL_WARNING_LEVEL_SELECTED_NEVER_BOUND = RASQAL_WARNING_LEVEL_MAYBE_ERROR,

  RASQAL_WARNING_LEVEL_UNUSED_SELECTED_VARIABLE = RASQAL_WARNING_LEVEL_STRICT_STYLE
} rasqal_warning_level;


rasqal_query_language_factory* rasqal_query_language_register_factory(rasqal_world *world, int (*factory) (rasqal_query_language_factory*));
rasqal_query_language_factory* rasqal_get_query_language_factory (rasqal_world*, const char* name, const unsigned char* uri);
void rasqal_log_error_simple(rasqal_world* world, raptor_log_level level, raptor_locator* locator, const char* message, ...) RASQAL_PRINTF_FORMAT(4, 5);
void rasqal_log_error_varargs(rasqal_world* world, raptor_log_level level, raptor_locator* locator, const char* message, va_list arguments) RASQAL_PRINTF_FORMAT(4, 0);
void rasqal_query_simple_error(void* user_data /* query */, const char *message, ...) RASQAL_PRINTF_FORMAT(2, 3);
void rasqal_world_simple_error(void* user_data /* world */, const char *message, ...) RASQAL_PRINTF_FORMAT(2, 3);
void rasqal_log_warning_simple(rasqal_world* world, rasqal_warning_level warn_level, raptor_locator* locator, const char* message, ...) RASQAL_PRINTF_FORMAT(4, 5);
void rasqal_log_trace_simple(rasqal_world* world, raptor_locator* locator, const char* message, ...) RASQAL_PRINTF_FORMAT(3, 4);

const char* rasqal_basename(const char* name);
unsigned char* rasqal_world_default_generate_bnodeid_handler(void *user_data, unsigned char *user_bnodeid);

extern const raptor_unichar rasqal_unicode_max_codepoint;

unsigned char* rasqal_escaped_name_to_utf8_string(const unsigned char* src, size_t len, size_t* dest_lenp, int (*error_handler)(rasqal_query *error_data, const char *message, ...) RASQAL_PRINTF_FORMAT(2, 3), rasqal_query* error_data);

/* rasqal_graph_pattern.c */
unsigned char* rasqal_query_generate_bnodeid(rasqal_query* rdf_query, unsigned char *user_bnodeid);

rasqal_graph_pattern* rasqal_new_basic_graph_pattern_from_formula(rasqal_query* query, rasqal_formula* formula);
rasqal_graph_pattern* rasqal_new_basic_graph_pattern_from_formula_exists(rasqal_query* query, rasqal_formula* formula);
rasqal_graph_pattern* rasqal_new_basic_graph_pattern_from_triples(rasqal_query* query, raptor_sequence* triples);

rasqal_graph_pattern* rasqal_new_2_group_graph_pattern(rasqal_query* query, rasqal_graph_pattern* first_gp, rasqal_graph_pattern* second_gp);

rasqal_graph_pattern* rasqal_graph_pattern_get_parent(rasqal_query *query, rasqal_graph_pattern* gp, rasqal_graph_pattern* tree_gp);

/* Sub-pattern support (EXISTS, NOT EXISTS, GRAPH, subgraphs) */
int rasqal_graph_pattern_needs_scope_isolation(rasqal_graph_pattern* gp);
void rasqal_graph_pattern_mark_as_subpattern(rasqal_graph_pattern* gp);
int rasqal_graph_pattern_copy_triples_to_scope(rasqal_graph_pattern* graph_pattern);
int rasqal_query_variable_only_in_exists(rasqal_query* query, rasqal_variable* var);


/* sparql_parser.y */
typedef struct 
{
  raptor_uri* uri;
  rasqal_update_graph_applies applies;
} sparql_uri_applies;

typedef struct 
{
  rasqal_op op;
  rasqal_expression *expr;
} sparql_op_expr;

int rasqal_init_query_language_sparql(rasqal_world*);
int rasqal_init_query_language_sparql11(rasqal_world*);
int rasqal_init_query_language_laqrs(rasqal_world*);



rasqal_graph_pattern* rasqal_graph_pattern_copy(rasqal_graph_pattern* gp);
rasqal_expression* rasqal_expression_copy(rasqal_expression* expr);
rasqal_projection* rasqal_projection_copy(rasqal_projection* p);
rasqal_solution_modifier* rasqal_solution_modifier_copy(rasqal_solution_modifier* sm);
rasqal_variable* rasqal_variable_copy(rasqal_variable* var);
rasqal_bindings* rasqal_bindings_copy(rasqal_bindings* b);
void rasqal_triple_substitute_variables(rasqal_triple* t, rasqal_variables_table* vars_table, rasqal_row* row);

/* rasqal_query_transform.c */

int rasqal_query_expand_triple_qnames(rasqal_query* rq);
int rasqal_sequence_has_qname(raptor_sequence* seq);
int rasqal_query_constraints_has_qname(rasqal_query* gp);
int rasqal_query_expand_graph_pattern_constraints_qnames(rasqal_query* rq, rasqal_graph_pattern* gp);
int rasqal_query_expand_query_constraints_qnames(rasqal_query* rq);
int rasqal_query_build_anonymous_variables(rasqal_query* rq);
int rasqal_query_expand_wildcards(rasqal_query* rq, rasqal_projection* projection);
int rasqal_query_remove_duplicate_select_vars(rasqal_query* rq, rasqal_projection* projection);
int rasqal_query_build_variables_use(rasqal_query* query, rasqal_projection* projection);
int rasqal_query_prepare_common(rasqal_query *query);
int rasqal_query_merge_graph_patterns(rasqal_query* query, rasqal_graph_pattern* gp, void* data);
int rasqal_graph_patterns_join(rasqal_graph_pattern *dest_gp, rasqal_graph_pattern *src_gp);
int rasqal_graph_pattern_move_constraints(rasqal_graph_pattern* dest_gp, rasqal_graph_pattern* src_gp);
int rasqal_graph_pattern_variable_bound_below(rasqal_graph_pattern *gp, rasqal_variable *v);

/* rasqal_double.c */
int rasqal_double_approximately_compare(double a, double b);
int rasqal_double_approximately_equal(double a, double b);

/* rasqal_expr.c */
rasqal_literal* rasqal_new_string_literal_node(rasqal_world*, const unsigned char *string, const char *language, raptor_uri *datatype);
int rasqal_literal_as_boolean(rasqal_literal* literal, int* error_p);
int rasqal_literal_as_integer(rasqal_literal* l, int* error_p);
double rasqal_literal_as_double(rasqal_literal* l, int* error_p);
raptor_uri* rasqal_literal_as_uri(rasqal_literal* l);
int rasqal_literal_string_to_native(rasqal_literal *l, int flags);
int rasqal_literal_has_qname(rasqal_literal* l);
int rasqal_literal_expand_qname(void* user_data, rasqal_literal* l);
int rasqal_literal_is_constant(rasqal_literal* l);
int rasqal_expression_has_qname(void* user_data, rasqal_expression* e);
int rasqal_expression_expand_qname(void* user_data, rasqal_expression* e);
int rasqal_literal_ebv(rasqal_literal* l);
int rasqal_expression_is_constant(rasqal_expression* e);
void rasqal_expression_clear(rasqal_expression* e);
void rasqal_expression_convert_to_literal(rasqal_expression* e, rasqal_literal* l);
int rasqal_expression_mentions_variable(rasqal_expression* e, rasqal_variable* v);
void rasqal_triple_write(rasqal_triple* t, raptor_iostream* iostr);
void rasqal_variable_write(rasqal_variable* v, raptor_iostream* iostr);
int rasqal_expression_is_aggregate(rasqal_expression* e);
int rasqal_expression_convert_aggregate_to_variable(rasqal_expression* e_in, rasqal_variable* v, rasqal_expression** e_out);
int rasqal_expression_mentions_aggregate(rasqal_expression* e);

raptor_sequence* rasqal_expression_copy_expression_sequence(raptor_sequence* exprs_seq);
int rasqal_literal_sequence_compare(int compare_flags, raptor_sequence* values_a, raptor_sequence* values_b);
raptor_sequence* rasqal_expression_sequence_evaluate(rasqal_query* query, raptor_sequence* exprs_seq, int ignore_errors, int* error_p);
int rasqal_literal_sequence_equals(raptor_sequence* values_a, raptor_sequence* values_b);


/* rasqal_expr_evaluate.c */
int rasqal_language_matches(const unsigned char* lang_tag, const unsigned char* lang_range);

/* rasqal_expr_datetimes.c */
rasqal_literal* rasqal_expression_evaluate_now(rasqal_expression *e, rasqal_evaluation_context *eval_context, int *error_p);
rasqal_literal* rasqal_expression_evaluate_to_unixtime(rasqal_expression *e, rasqal_evaluation_context *eval_context, int *error_p);
rasqal_literal* rasqal_expression_evaluate_from_unixtime(rasqal_expression *e, rasqal_evaluation_context *eval_context, int *error_p);
rasqal_literal* rasqal_expression_evaluate_datetime_part(rasqal_expression *e, rasqal_evaluation_context *eval_context, int *error_p);
rasqal_literal* rasqal_expression_evaluate_datetime_timezone(rasqal_expression *e, rasqal_evaluation_context *eval_context, int *error_p);
rasqal_literal* rasqal_expression_evaluate_datetime_tz(rasqal_expression *e, rasqal_evaluation_context *eval_context, int *error_p);

/* rasqal_expr_numerics.c */
rasqal_literal* rasqal_expression_evaluate_abs(rasqal_expression *e, rasqal_evaluation_context *eval_context, int *error_p);
rasqal_literal* rasqal_expression_evaluate_round(rasqal_expression *e, rasqal_evaluation_context *eval_context, int *error_p);
rasqal_literal* rasqal_expression_evaluate_ceil(rasqal_expression *e, rasqal_evaluation_context *eval_context, int *error_p);
rasqal_literal* rasqal_expression_evaluate_floor(rasqal_expression *e, rasqal_evaluation_context *eval_context, int *error_p);
rasqal_literal* rasqal_expression_evaluate_rand(rasqal_expression *e, rasqal_evaluation_context *eval_context, int *error_p);
rasqal_literal* rasqal_expression_evaluate_digest(rasqal_expression *e, rasqal_evaluation_context *eval_context, int *error_p);
rasqal_literal* rasqal_expression_evaluate_uriuuid(rasqal_expression *e, rasqal_evaluation_context *eval_context, int *error_p);
rasqal_literal* rasqal_expression_evaluate_struuid(rasqal_expression *e, rasqal_evaluation_context *eval_context, int *error_p);

/* rasqal_expr_strings.c */
rasqal_literal* rasqal_expression_evaluate_substr(rasqal_expression *e, rasqal_evaluation_context *eval_context, int *error_p);
rasqal_literal* rasqal_expression_evaluate_set_case(rasqal_expression *e, rasqal_evaluation_context *eval_context, int *error_p);
rasqal_literal* rasqal_expression_evaluate_str_prefix_suffix(rasqal_expression *e, rasqal_evaluation_context *eval_context, int *error_p);
rasqal_literal* rasqal_expression_evaluate_strlen(rasqal_expression *e, rasqal_evaluation_context *eval_context, int *error_p);
rasqal_literal* rasqal_expression_evaluate_encode_for_uri(rasqal_expression *e, rasqal_evaluation_context *eval_context, int *error_p);
rasqal_literal* rasqal_expression_evaluate_concat(rasqal_expression *e, rasqal_evaluation_context *eval_context, int *error_p);
rasqal_literal* rasqal_expression_evaluate_langmatches(rasqal_expression *e, rasqal_evaluation_context *eval_context, int *error_p);
rasqal_literal* rasqal_expression_evaluate_strmatch(rasqal_expression *e, rasqal_evaluation_context *eval_context, int *error_p);
rasqal_literal* rasqal_expression_evaluate_strbefore(rasqal_expression *e, rasqal_evaluation_context *eval_context, int *error_p);
rasqal_literal* rasqal_expression_evaluate_strafter(rasqal_expression *e, rasqal_evaluation_context *eval_context, int *error_p);
rasqal_literal* rasqal_expression_evaluate_replace(rasqal_expression *e, rasqal_evaluation_context *eval_context, int *error_p);

/* EXISTS/NOT EXISTS Expression Helper Function */
rasqal_expression* rasqal_new_exists_expression(rasqal_query* rq, rasqal_graph_pattern* pattern, int is_negated);


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

/* for time_t */
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

/* timegm.c */
#ifdef HAVE_TIMEGM
#define rasqal_timegm timegm
#else
time_t rasqal_timegm(struct tm *tm);
#endif

/* rasqal_raptor.c */
int rasqal_raptor_init(rasqal_world*);

#ifdef RAPTOR_TRIPLES_SOURCE_REDLAND
/* rasqal_redland.c */
int rasqal_redland_init(rasqal_world*);
void rasqal_redland_finish(void);
#endif

rasqal_triple* raptor_statement_as_rasqal_triple(rasqal_world* world, const raptor_statement *statement);
int rasqal_raptor_triple_match(rasqal_world* world, rasqal_triple *triple, rasqal_triple *match, unsigned int parts);


/* rasqal_general.c */
int rasqal_uri_init(rasqal_world*);
void rasqal_uri_finish(rasqal_world*);

/* rasqal_literal.c */
rasqal_formula* rasqal_new_formula(rasqal_world* world);
void rasqal_free_formula(rasqal_formula* formula);
int rasqal_formula_print(rasqal_formula* formula, FILE *stream);
rasqal_formula* rasqal_formula_join(rasqal_formula* first_formula, rasqal_formula* second_formula);

/* The following should be public eventually in rasqal.h or raptor.h or ...? */

typedef int (rasqal_compare_fn)(void* user_data, const void *a, const void *b);
typedef void (rasqal_kv_free_fn)(const void *key, const void *value);

#define RASQAL_XSD_BOOLEAN_TRUE_LEN 4
extern const unsigned char* rasqal_xsd_boolean_true;

#define RASQAL_XSD_BOOLEAN_FALSE_LEN 5
extern const unsigned char* rasqal_xsd_boolean_false;

rasqal_literal* rasqal_literal_cast(rasqal_literal* l, raptor_uri* datatype, int flags,  int* error_p);
rasqal_literal* rasqal_new_numeric_literal(rasqal_world*, rasqal_literal_type type, double d);
int rasqal_literal_is_numeric(rasqal_literal* literal);
rasqal_literal* rasqal_literal_bound_value(rasqal_literal* literal);
rasqal_literal* rasqal_literal_add(rasqal_literal* l1, rasqal_literal* l2, int *error);
rasqal_literal* rasqal_literal_subtract(rasqal_literal* l1, rasqal_literal* l2, int *error);
rasqal_literal* rasqal_literal_multiply(rasqal_literal* l1, rasqal_literal* l2, int *error);
rasqal_literal* rasqal_literal_divide(rasqal_literal* l1, rasqal_literal* l2, int *error);
rasqal_literal* rasqal_literal_negate(rasqal_literal* l, int *error_p);
rasqal_literal* rasqal_literal_abs(rasqal_literal* l1,  int *error_p);
rasqal_literal* rasqal_literal_round(rasqal_literal* l1, int *error_p);
rasqal_literal* rasqal_literal_ceil(rasqal_literal* l1,  int *error_p);
rasqal_literal* rasqal_literal_floor(rasqal_literal* l1, int *error_p);
int rasqal_literal_equals_flags(rasqal_literal* l1, rasqal_literal* l2, int flags, int* error);
int rasqal_literal_not_equals_flags(rasqal_literal* l1, rasqal_literal* l2, int flags, int* error);
void rasqal_literal_write_type(rasqal_literal* l, raptor_iostream* iostr);
void rasqal_literal_write(rasqal_literal* l, raptor_iostream* iostr);
void rasqal_expression_write_op(rasqal_expression* e, raptor_iostream* iostr);
void rasqal_expression_write(rasqal_expression* e, raptor_iostream* iostr);
int rasqal_literal_write_turtle(rasqal_literal* l, raptor_iostream* iostr);
int rasqal_literal_array_equals(rasqal_literal** values_a, rasqal_literal** values_b, int size);
int rasqal_literal_array_compare(rasqal_literal** values_a, rasqal_literal** values_b, raptor_sequence* exprs_seq, int size, int compare_flags);
int rasqal_literal_array_compare_by_order(rasqal_literal** values_a, rasqal_literal** values_b, int* order, int size, int compare_flags);
rasqal_map* rasqal_new_literal_sequence_sort_map(int is_distinct, int compare_flags);
int rasqal_literal_sequence_sort_map_add_literal_sequence(rasqal_map* map, raptor_sequence* literals_sequence);
raptor_sequence* rasqal_new_literal_sequence_of_sequence_from_data(rasqal_world* world, const char* const row_data[], int width);
rasqal_literal* rasqal_new_literal_from_term(rasqal_world* world, raptor_term* term);
int rasqal_literal_string_datatypes_compare(rasqal_literal* l1, rasqal_literal* l2);
int rasqal_literal_string_languages_compare(rasqal_literal* l1, rasqal_literal* l2);
int rasqal_literal_is_string(rasqal_literal* l1);

/* rasqal_map.c */
typedef void (*rasqal_map_visit_fn)(void *key, void *value, void *user_data);

rasqal_map* rasqal_new_map(rasqal_compare_fn* compare_fn, void* compare_user_data, raptor_data_free_handler free_compare_user_data, raptor_data_free_handler free_key_fn, raptor_data_free_handler free_value_fn, raptor_data_print_handler print_key_fn, raptor_data_print_handler print_value_fn, int flags);

void rasqal_free_map(rasqal_map *map);
int rasqal_map_add_kv(rasqal_map* map, void* key, void *value);
void rasqal_map_visit(rasqal_map* map, rasqal_map_visit_fn fn, void *user_data);
int rasqal_map_print(rasqal_map* map, FILE* fh);
void* rasqal_map_search(rasqal_map* map, const void* key);


/* rasqal_query.c */
rasqal_query_results* rasqal_query_execute_with_engine(rasqal_query* query, const rasqal_query_execution_factory* engine);
int rasqal_query_remove_query_result(rasqal_query* query, rasqal_query_results* query_results);
int rasqal_query_declare_prefix(rasqal_query* rq, rasqal_prefix* prefix);
int rasqal_query_declare_prefixes(rasqal_query* rq);
void rasqal_query_set_base_uri(rasqal_query* rq, raptor_uri* base_uri);
rasqal_variable* rasqal_query_get_variable_by_offset(rasqal_query* query, int idx);
const rasqal_query_execution_factory* rasqal_query_get_engine_by_name(const char* name);
int rasqal_query_variable_is_bound(rasqal_query* query, rasqal_variable* v);
int rasqal_query_variable_is_bound_in_scope(rasqal_query* query, rasqal_variable* v, rasqal_query_scope* scope);
int rasqal_query_variable_bound_at_root_level(rasqal_query* query, rasqal_variable* v);
rasqal_variable* rasqal_query_get_variable_in_graph_pattern(rasqal_query* query, const char* name, rasqal_graph_pattern* gp);
rasqal_triple_parts rasqal_query_variable_bound_in_triple(rasqal_query* query, rasqal_variable* v, int column);
int rasqal_query_store_select_query(rasqal_query* query, rasqal_projection* projection, raptor_sequence* data_graphs, rasqal_graph_pattern* where_gp, rasqal_solution_modifier* modifier);
int rasqal_query_reset_select_query(rasqal_query* query);
rasqal_projection* rasqal_query_get_projection(rasqal_query* query);
int rasqal_query_set_projection(rasqal_query* query, rasqal_projection* projection);
int rasqal_query_set_modifier(rasqal_query* query, rasqal_solution_modifier* modifier);

/* rasqal_query_results.c */
int rasqal_init_query_results(void);
void rasqal_finish_query_results(void);
int rasqal_query_results_execute_with_engine(rasqal_query_results* query_results, const rasqal_query_execution_factory* factory, int store_results);
int rasqal_query_check_limit_offset_core(int result_offset, int limit, int offset);
int rasqal_query_check_limit_offset(rasqal_query* query, int result_offset);
void rasqal_query_results_remove_query_reference(rasqal_query_results* query_results);
rasqal_variables_table* rasqal_query_results_get_variables_table(rasqal_query_results* query_results);
rasqal_row* rasqal_query_results_get_current_row(rasqal_query_results* query_results);
rasqal_world* rasqal_query_results_get_world(rasqal_query_results* query_results);
#if RAPTOR_VERSION < 20015
typedef int (*raptor_data_compare_arg_handler)(const void *data1, const void *data2, void *user_data);
#endif
int rasqal_query_results_sort(rasqal_query_results* query_result);
int rasqal_query_results_set_boolean(rasqal_query_results* query_results, int value);

/* Internal functions for adding data to query results (for testing and internal use) */
int rasqal_query_results_add_binding_variable(rasqal_query_results* query_results, const unsigned char* name);
int rasqal_query_results_add_binding(rasqal_query_results* query_results, int var_offset, raptor_term* term);
int rasqal_query_results_add_triple(rasqal_query_results* query_results, raptor_statement* triple);

/* rasqal_query_write.c */
int rasqal_query_write_sparql_20060406_graph_pattern(rasqal_graph_pattern* gp, raptor_iostream *iostr,raptor_uri* base_uri);
int rasqal_query_write_sparql_20060406(raptor_iostream *iostr, rasqal_query* query, raptor_uri *base_uri);

/* rasqal_result_formats.c */
rasqal_query_results_format_factory* rasqal_world_register_query_results_format_factory(rasqal_world* world, int (*register_factory) (rasqal_query_results_format_factory*));
void rasqal_free_query_results_format_factory(rasqal_query_results_format_factory* factory);
int rasqal_init_result_formats(rasqal_world*);
void rasqal_finish_result_formats(rasqal_world*);

/* rasqal_format_sv.c */
int rasqal_init_result_format_sv(rasqal_world* world);

/* rasqal_format_json.c */
int rasqal_init_result_format_json(rasqal_world*);

/* rasqal_format_sparql_xml.c */
int rasqal_init_result_format_sparql_xml(rasqal_world*);

/* rasqal_format_table.c */
int rasqal_init_result_format_table(rasqal_world*);

/* rasqal_format_html.c */
int rasqal_init_result_format_html(rasqal_world*);

/* rasqal_format_turtle.c */
int rasqal_init_result_format_turtle(rasqal_world*);

/* rasqal_format_rdf.c */
int rasqal_init_result_format_rdf(rasqal_world*);

#ifdef RASQAL_RESULT_FORMAT_SRJ
/* SRJ (SPARQL Results JSON) reader via YAJL */
int rasqal_init_result_format_srj(rasqal_world*);
#endif

/* rasqal_row.c */
rasqal_row* rasqal_new_row(rasqal_rowsource* rowsource);
rasqal_row* rasqal_new_row_from_row(rasqal_row* row);
rasqal_row* rasqal_new_row_from_row_deep(rasqal_row* row);
int rasqal_row_print(rasqal_row* row, FILE* fh);
int rasqal_row_write(rasqal_row* row, raptor_iostream* iostr);
raptor_sequence* rasqal_new_row_sequence(rasqal_world* world, rasqal_variables_table* vt, const char* const row_data[], int vars_count, raptor_sequence** vars_seq_p);
int rasqal_row_to_nodes(rasqal_row* row);
void rasqal_row_set_values_from_variables_table(rasqal_row* row, rasqal_variables_table* vars_table);
int rasqal_row_set_order_size(rasqal_row *row, int order_size);
int rasqal_row_expand_size(rasqal_row *row, int size);
int rasqal_row_bind_variables(rasqal_row* row, rasqal_variables_table* vars_table);
raptor_sequence* rasqal_row_sequence_copy(raptor_sequence *seq);
void rasqal_row_set_rowsource(rasqal_row* row, rasqal_rowsource* rowsource);
void rasqal_row_set_weak_rowsource(rasqal_row* row, rasqal_rowsource* rowsource);
rasqal_variable* rasqal_row_get_variable_by_offset(rasqal_row* row, int offset);

/* rasqal_row_compatible.c */
rasqal_row_compatible* rasqal_new_row_compatible(rasqal_variables_table* vt, rasqal_rowsource *first_rowsource, rasqal_rowsource *second_rowsource);
void rasqal_free_row_compatible(rasqal_row_compatible* map);
int rasqal_row_compatible_check(rasqal_row_compatible* map, rasqal_row *first_row, rasqal_row *second_row);
void rasqal_print_row_compatible(FILE *handle, rasqal_row_compatible* map);

/* rasqal_triples_source.c */
rasqal_triples_source* rasqal_new_triples_source(rasqal_query* query);
int rasqal_reset_triple_meta(rasqal_triple_meta* m);
void rasqal_free_triples_source(rasqal_triples_source *rts);
int rasqal_triples_source_triple_present(rasqal_triples_source *rts, rasqal_triple *t);
int rasqal_triples_source_support_feature(rasqal_triples_source *rts, rasqal_triples_source_feature feature);

rasqal_triples_match* rasqal_new_triples_match(rasqal_query* query, rasqal_triples_source* triples_source, rasqal_triple_meta *m, rasqal_triple *t);

void rasqal_free_triples_match(rasqal_triples_match* rtm);
rasqal_triple_parts rasqal_triples_match_bind_match(struct rasqal_triples_match_s* rtm, rasqal_variable *bindings[4],rasqal_triple_parts parts);
void rasqal_triples_match_next_match(struct rasqal_triples_match_s* rtm);
int rasqal_triples_match_is_end(struct rasqal_triples_match_s* rtm);


/* rasqal_xsd_datatypes.c */
int rasqal_xsd_init(rasqal_world*);
void rasqal_xsd_finish(rasqal_world*);
rasqal_literal_type rasqal_xsd_datatype_uri_to_type(rasqal_world*, raptor_uri* uri);
raptor_uri* rasqal_xsd_datatype_type_to_uri(rasqal_world*, rasqal_literal_type type);
int rasqal_xsd_datatype_check(rasqal_literal_type native_type, const unsigned char* string, int flags);
const char* rasqal_xsd_datatype_label(rasqal_literal_type native_type);
int rasqal_xsd_is_datatype_uri(rasqal_world*, raptor_uri* uri);

int rasqal_xsd_datatype_is_numeric(rasqal_literal_type type);
unsigned char* rasqal_xsd_format_integer(int i, size_t *len_p);
unsigned char* rasqal_xsd_format_float(float f, size_t *len_p);
unsigned char* rasqal_xsd_format_double(double d, size_t *len_p);
rasqal_literal_type rasqal_xsd_datatype_parent_type(rasqal_literal_type type);
raptor_uri* rasqal_xsd_decimal_subtype_promote_uri(rasqal_world* world,
                                                   rasqal_literal* l1,
                                                   rasqal_literal* l2);

int rasqal_xsd_boolean_value_from_string(const unsigned char* string);


typedef struct rasqal_graph_factory_s rasqal_graph_factory;

/* rasqal_world structure */
struct rasqal_world_s {
  /* opened flag */
  int opened;
  
  /* raptor_world object */
  raptor_world *raptor_world_ptr;

  /* should rasqal free the raptor_world */
  int raptor_world_allocated_here;

  /* log handler */
  raptor_log_handler log_handler;
  void *log_handler_user_data;

  /* sequence of query language factories */
  raptor_sequence *query_languages;

  /* registered query results formats */
  raptor_sequence *query_results_formats;

  /* rasqal_uri rdf uris */
  raptor_uri *rdf_namespace_uri;
  raptor_uri *rdf_first_uri;
  raptor_uri *rdf_rest_uri;
  raptor_uri *rdf_nil_uri;

  /* triples source factory */
  rasqal_triples_source_factory triples_source_factory;

  /* rasqal_xsd_datatypes */
  raptor_uri *xsd_namespace_uri;
  raptor_uri **xsd_datatype_uris;

  /* graph factory */
  rasqal_graph_factory *graph_factory;
  void *graph_factory_user_data;

  int default_generate_bnodeid_handler_base;
  char *default_generate_bnodeid_handler_prefix;
  size_t default_generate_bnodeid_handler_prefix_length;

  void *generate_bnodeid_handler_user_data;
  rasqal_generate_bnodeid_handler generate_bnodeid_handler;

  /* used for NOW() value */
  struct timeval now;
  /* set when now is a cached value */
  unsigned int now_set : 1;

  rasqal_warning_level warning_level;

  /* generated counter - increments at every generation */
  int genid_counter;
};


/*
 * Rasqal Algebra
 *
 * Based on http://www.w3.org/TR/rdf-sparql-query/#sparqlAlgebra
 */

typedef enum {
  RASQAL_ALGEBRA_OPERATOR_UNKNOWN  = 0,
  RASQAL_ALGEBRA_OPERATOR_BGP      = 1,
  RASQAL_ALGEBRA_OPERATOR_FILTER   = 2,
  RASQAL_ALGEBRA_OPERATOR_JOIN     = 3,
  RASQAL_ALGEBRA_OPERATOR_DIFF     = 4,
  RASQAL_ALGEBRA_OPERATOR_LEFTJOIN = 5,
  RASQAL_ALGEBRA_OPERATOR_UNION    = 6,
  RASQAL_ALGEBRA_OPERATOR_TOLIST   = 7,
  RASQAL_ALGEBRA_OPERATOR_ORDERBY  = 8,
  RASQAL_ALGEBRA_OPERATOR_PROJECT  = 9,
  RASQAL_ALGEBRA_OPERATOR_DISTINCT = 10,
  RASQAL_ALGEBRA_OPERATOR_REDUCED  = 11,
  RASQAL_ALGEBRA_OPERATOR_SLICE    = 12,
  RASQAL_ALGEBRA_OPERATOR_GRAPH    = 13,
  RASQAL_ALGEBRA_OPERATOR_EXTEND = 14,
  RASQAL_ALGEBRA_OPERATOR_GROUP    = 15,
  RASQAL_ALGEBRA_OPERATOR_AGGREGATION = 16,
  RASQAL_ALGEBRA_OPERATOR_HAVING   = 17,
  RASQAL_ALGEBRA_OPERATOR_VALUES   = 18,
  RASQAL_ALGEBRA_OPERATOR_SERVICE  = 19,

  RASQAL_ALGEBRA_OPERATOR_LAST = RASQAL_ALGEBRA_OPERATOR_SERVICE
} rasqal_algebra_node_operator;


/* bitflags used by rasqal_algebra_node and rasqal_rowsource */
typedef enum {
  /* used by */
  RASQAL_ENGINE_BITFLAG_SILENT = 1,
  /* DIFF algebra needs correlated evaluation (NOT EXISTS with LHS variable dependencies) */
  RASQAL_ALGEBRA_DIFF_NEEDS_CORRELATION = 2
} rasqal_engine_bitflags;


/*
 * SPARQL 1.2 Variable Correlation Map
 *
 * Pre-computed correlation analysis for MINUS operations to determine
 * which variables need LHS→RHS correlation per SPARQL 1.2 specification.
 */
typedef struct {
  int requires_lhs_context;              /* Does RHS need LHS variable values? */
  raptor_sequence* lhs_variables;        /* Variables available from LHS scope */
  raptor_sequence* rhs_not_exists_vars;  /* Variables used in RHS NOT EXISTS */
  raptor_sequence* correlation_pairs;    /* LHS→RHS variable mapping pairs */
  raptor_sequence* saved_bindings;       /* Backup for variable restoration */
} rasqal_variable_correlation_map;

/* Variable binding backup structure for correlation operations */
typedef struct {
  rasqal_variable* var;                  /* Variable being backed up */
  rasqal_literal* original_value;        /* Original value for restoration */
} rasqal_variable_binding_backup;

/*
 * Algebra Node
 *
 * Rasqal graph pattern class.
 */
struct rasqal_algebra_node_s {
  rasqal_query* query;

  /* operator for this algebra_node's contents */
  rasqal_algebra_node_operator op;

  /* type BGP (otherwise NULL and start_column and end_column are -1) */
  raptor_sequence* triples;
  int start_column;
  int end_column;
  
  /* types JOIN, DIFF, LEFTJOIN, UNION, ORDERBY: node1 and node2 ALWAYS present
   * types FILTER, TOLIST: node1 ALWAYS present, node2 ALWAYS NULL
   * type PROJECT, GRAPH, GROUPBY, AGGREGATION, HAVING: node1 always present
   * (otherwise NULL)
   */
  struct rasqal_algebra_node_s *node1;
  struct rasqal_algebra_node_s *node2;

  /* types FILTER, LEFTJOIN
   * (otherwise NULL) 
   */
  rasqal_expression* expr;

  /* types ORDERBY, GROUPBY, AGGREGATION, HAVING always present: sequence of
   * #rasqal_expression
   * (otherwise NULL)
   */
  raptor_sequence* seq;

  /* types PROJECT, DISTINCT, REDUCED
   * FIXME: sequence of solution mappings */

  /* types PROJECT, AGGREGATION: sequence of #rasqal_variable */
  raptor_sequence* vars_seq;

  /* type SLICE: limit and offset rows */
  int limit;
  int offset;

  /* type GRAPH */
  rasqal_literal *graph;

  /* type LET */
  rasqal_variable *var;

  /* type ORDERBY */
  int distinct;

  /* type VALUES */
  rasqal_bindings *bindings;

  /* type SERVICE */
  raptor_uri* service_uri;
  const unsigned char* query_string;
  raptor_sequence* data_graphs;

  /* flags */
  unsigned int flags;

  /* Scope context for variable resolution */
  rasqal_query_scope* execution_scope;

  /* SPARQL 1.2: Variable correlation metadata for MINUS operations */
  rasqal_variable_correlation_map* correlation_map;
};
typedef struct rasqal_algebra_node_s rasqal_algebra_node;


/**
 * rasqal_algebra_node_visit_fn:
 * @query: #rasqal_query containing the graph pattern
 * @gp: current algebra_node
 * @user_data: user data passed in
 *
 * User function to visit an algebra_node and operate on it with
 * rasqal_algebra_node_visit() or rasqal_query_algebra_node_visit()
 *
 * Return value: 0 to truncate the visit
 */
typedef int (*rasqal_algebra_node_visit_fn)(rasqal_query* query, rasqal_algebra_node* node, void *user_data);

typedef struct
{
  rasqal_query* query;
  
  /* aggregate expression variables map
   * key: rasqal_expression* tree with an aggregate function at top
   * value: rasqal_variable*
   */
  rasqal_map* agg_vars;

  /* sequence of aggregate #rasqal_expression in same order as @agg_vars_seq */
  raptor_sequence* agg_exprs;

  /* sequence of aggregate #rasqal_expression in same order as @agg_exprs */
  raptor_sequence* agg_vars_seq;

  /* number of aggregate expressions seen
   * ( = number of internal variables created )
   */
  int counter;

  /* compare flags */
  int flags;

  /* error indicator */
  int error;

  /* if set, finding a new expression is an error */
  unsigned int adding_new_vars_is_error : 1;

  /* query part for error messages */
  const char* error_part;
} rasqal_algebra_aggregate;


/* rasqal_algebra.c */

rasqal_algebra_node* rasqal_new_distinct_algebra_node(rasqal_query* query, rasqal_algebra_node* node1);
rasqal_algebra_node* rasqal_new_filter_algebra_node(rasqal_query* query, rasqal_expression* expr, rasqal_algebra_node* node, rasqal_query_scope* execution_scope);
rasqal_algebra_node* rasqal_new_empty_algebra_node(rasqal_query* query);
rasqal_algebra_node* rasqal_new_triples_algebra_node(rasqal_query* query, raptor_sequence* triples, int start_column, int end_column);
rasqal_algebra_node* rasqal_new_2op_algebra_node(rasqal_query* query, rasqal_algebra_node_operator op, rasqal_algebra_node* node1, rasqal_algebra_node* node2);
rasqal_algebra_node* rasqal_new_leftjoin_algebra_node(rasqal_query* query, rasqal_algebra_node* node1, rasqal_algebra_node* node2, rasqal_expression* expr);
rasqal_algebra_node* rasqal_new_join_algebra_node(rasqal_query* query, rasqal_algebra_node* node1, rasqal_algebra_node* node2, rasqal_expression* expr);
rasqal_algebra_node* rasqal_new_orderby_algebra_node(rasqal_query* query, rasqal_algebra_node* node, raptor_sequence* seq, int distinct);
rasqal_algebra_node* rasqal_new_slice_algebra_node(rasqal_query* query, rasqal_algebra_node* node1, int limit, int offset);
rasqal_algebra_node* rasqal_new_project_algebra_node(rasqal_query* query, rasqal_algebra_node* node1, raptor_sequence* vars_seq);
rasqal_algebra_node* rasqal_new_graph_algebra_node(rasqal_query* query, rasqal_algebra_node* node1, rasqal_literal *graph);
rasqal_algebra_node* rasqal_new_bind_algebra_node(rasqal_query* query, rasqal_variable *var, rasqal_expression *expr);
rasqal_algebra_node* rasqal_new_groupby_algebra_node(rasqal_query* query, rasqal_algebra_node* node1, raptor_sequence* seq);
rasqal_algebra_node* rasqal_new_aggregation_algebra_node(rasqal_query* query, rasqal_algebra_node* node1, raptor_sequence* exprs_seq, raptor_sequence* vars_seq);
rasqal_algebra_node* rasqal_new_having_algebra_node(rasqal_query* query,rasqal_algebra_node* node1, raptor_sequence* exprs_seq);
rasqal_algebra_node* rasqal_new_values_algebra_node(rasqal_query* query, rasqal_bindings* bindings);
rasqal_algebra_node* rasqal_new_service_algebra_node(rasqal_query* query, raptor_uri* service_uri, const unsigned char* query_string, raptor_sequence* data_graphs, int silent);
rasqal_algebra_node* rasqal_new_extend_algebra_node(rasqal_query* query, rasqal_algebra_node* input, rasqal_variable* var, rasqal_expression* expr);

void rasqal_free_algebra_node(rasqal_algebra_node* node);

/* SPARQL 1.2 Variable Correlation Map Functions */
rasqal_variable_correlation_map* rasqal_new_variable_correlation_map(void);
void rasqal_free_variable_correlation_map(rasqal_variable_correlation_map* map);
rasqal_variable_correlation_map* rasqal_analyze_scope_variable_correlation(rasqal_algebra_node* lhs_node, rasqal_algebra_node* rhs_node, raptor_sequence* rhs_not_exists_vars);
rasqal_variable_correlation_map* rasqal_algebra_analyze_direct_minus_correlation(rasqal_algebra_node* lhs_node, rasqal_algebra_node* rhs_node);
rasqal_algebra_node_operator rasqal_algebra_node_get_operator(rasqal_algebra_node* node);
const char* rasqal_algebra_node_operator_as_counted_string(rasqal_algebra_node_operator op, size_t* length_p);
int rasqal_algebra_algebra_node_write(rasqal_algebra_node *node, raptor_iostream* iostr);
int rasqal_algebra_node_print(rasqal_algebra_node* node, FILE* fh);
int rasqal_algebra_node_visit(rasqal_query *query, rasqal_algebra_node* node, rasqal_algebra_node_visit_fn fn, void *user_data);
rasqal_algebra_node* rasqal_algebra_query_to_algebra(rasqal_query* query);
rasqal_algebra_node* rasqal_algebra_query_add_group_by(rasqal_query* query, rasqal_algebra_node* node, rasqal_solution_modifier* modifier);
rasqal_algebra_node* rasqal_algebra_query_add_orderby(rasqal_query* query, rasqal_algebra_node* node, rasqal_projection* projection, rasqal_solution_modifier* modifier);
rasqal_algebra_node* rasqal_algebra_query_add_slice(rasqal_query* query, rasqal_algebra_node* node, rasqal_solution_modifier* modifier);
rasqal_algebra_node* rasqal_algebra_query_add_aggregation(rasqal_query* query, rasqal_algebra_aggregate* ae, rasqal_algebra_node* node);
rasqal_algebra_node* rasqal_algebra_query_add_projection(rasqal_query* query, rasqal_algebra_node* node, rasqal_projection* projection);
rasqal_algebra_node* rasqal_algebra_query_add_construct_projection(rasqal_query* query, rasqal_algebra_node* node);
rasqal_algebra_node* rasqal_algebra_query_add_distinct(rasqal_query* query, rasqal_algebra_node* node, rasqal_projection* projection);
rasqal_algebra_node* rasqal_algebra_query_add_having(rasqal_query* query, rasqal_algebra_node* node, rasqal_solution_modifier* modifier);
int rasqal_algebra_node_is_empty(rasqal_algebra_node* node);

rasqal_algebra_aggregate* rasqal_algebra_query_prepare_aggregates(rasqal_query* query, rasqal_algebra_node* node, rasqal_projection* projection, rasqal_solution_modifier* modifier);
void rasqal_free_algebra_aggregate(rasqal_algebra_aggregate* ae);

/* rasqal_variable.c */
rasqal_variables_table* rasqal_new_variables_table_from_variables_table(rasqal_variables_table* vt);
rasqal_variable* rasqal_variables_table_get(rasqal_variables_table* vt, int idx);
rasqal_literal* rasqal_variables_table_get_value(rasqal_variables_table* vt, int idx);
int rasqal_variables_table_set(rasqal_variables_table* vt, rasqal_variable_type type, const unsigned char *name, rasqal_literal* value);
int rasqal_variables_table_get_named_variables_count(rasqal_variables_table* vt);
int rasqal_variables_table_get_anonymous_variables_count(rasqal_variables_table* vt);
int rasqal_variables_table_get_total_variables_count(rasqal_variables_table* vt);
raptor_sequence* rasqal_variables_table_get_named_variables_sequence(rasqal_variables_table* vt);
raptor_sequence* rasqal_variables_table_get_anonymous_variables_sequence(rasqal_variables_table* vt);
const unsigned char** rasqal_variables_table_get_names(rasqal_variables_table* vt);
raptor_sequence* rasqal_variable_copy_variable_sequence(raptor_sequence* vars_seq);
int rasqal_variables_write(raptor_sequence* seq, raptor_iostream* iostr);

/**
 * rasqal_engine_error:
 * @RASQAL_ENGINE_OK:
 * @RASQAL_ENGINE_FAILED:
 * @RASQAL_ENGINE_FINISHED:
 *
 * Execution engine errors.
 *
 */
typedef enum {
  RASQAL_ENGINE_OK,
  RASQAL_ENGINE_FAILED,
  RASQAL_ENGINE_FINISHED,
  RASQAL_ENGINE_ERROR_LAST = RASQAL_ENGINE_FINISHED
} rasqal_engine_error;


/*
 * A query execution engine factory
 *
 * This structure is about executing the query recorded in
 * #rasqal_query structure into results accessed via #rasqal_query_results
 */
struct rasqal_query_execution_factory_s {
  /* execution engine name */
  const char* name;

  /* size of execution engine private data */
  size_t execution_data_size;
  
  /*
   * @ex_data: execution data
   * @query: query to execute
   * @query_results: query results
   * @flags: execution flags.  1: execute and store results
   * @error_p: execution error (OUT variable)
   *
   * Initialise a new execution
   *
   * Return value: non-0 on failure
   */
  int (*execute_init)(void* ex_data, rasqal_query* query, rasqal_query_results* query_results, int flags, rasqal_engine_error *error_p);

  /**
   * @ex_data: execution data
   * @error_p: execution error (OUT variable)
   *
   * Get all bindings result rows (returning a new raptor_sequence object holding new objects.
   *
   * Will not be called if query results is NULL, finished or failed.
   */
  raptor_sequence* (*get_all_rows)(void* ex_data, rasqal_engine_error *error_p);

  /*
   * @ex_data: execution object
   * @error_p: execution error (OUT variable)
   *
   * Get current bindings result row (returning a new object) 
   *
   * Will not be called if query results is NULL, finished or failed.
   */
  rasqal_row* (*get_row)(void* ex_data, rasqal_engine_error *error_p);

  /* finish (free) execution */
  int (*execute_finish)(void* ex_data, rasqal_engine_error *error_p);
  
  /* finish the query execution factory */
  void (*finish_factory)(rasqal_query_execution_factory* factory);

};


/* rasqal_engine.c */
#ifdef RASQAL_DEBUG
const char* rasqal_engine_get_parts_string(rasqal_triple_parts parts);
const char* rasqal_engine_error_as_string(rasqal_engine_error error);
#endif


/* rasqal_engine_sort.c */
rasqal_map* rasqal_engine_new_rowsort_map(int is_distinct, int compare_flags, raptor_sequence* order_conditions_sequence);
int rasqal_engine_rowsort_map_add_row(rasqal_map* map, rasqal_row* row);
raptor_sequence* rasqal_engine_rowsort_map_to_sequence(rasqal_map* map, raptor_sequence* seq);
int rasqal_engine_rowsort_calculate_order_values(rasqal_query* query, raptor_sequence* order_seq, rasqal_row* row);


typedef struct {
  rasqal_query* query;
  rasqal_query_results* query_results;

  /* query algebra representation of query */
  rasqal_algebra_node* algebra_node;

  /* number of nodes in #algebra_node tree */
  int nodes_count;

  /* rowsource that provides the result rows */
  rasqal_rowsource* rowsource;

  rasqal_triples_source* triples_source;
} rasqal_engine_algebra_data;


/* rasqal_engine_algebra.c */

/* New query engine based on executing over query algebra */
extern const rasqal_query_execution_factory rasqal_query_engine_algebra;


/* rasqal_iostream.c */
raptor_iostream* rasqal_new_iostream_from_stringbuffer(raptor_world *raptor_world_ptr, raptor_stringbuffer* sb);

/* rasqal_service.c */
rasqal_rowsource* rasqal_service_execute_as_rowsource(rasqal_service* svc, rasqal_variables_table* vars_table);

/* rasqal_triples_source.c */
void rasqal_triples_source_error_handler(rasqal_query* rdf_query, raptor_locator* locator, const char* message);
void rasqal_triples_source_error_handler2(rasqal_world* world, raptor_locator* locator,  const char* message);

/* rasqal_update.c */
const char* rasqal_update_type_label(rasqal_update_type type);
rasqal_update_operation* rasqal_new_update_operation(rasqal_update_type type, raptor_uri* graph_uri, raptor_uri* document_uri, raptor_sequence* insert_templates, raptor_sequence* delete_templates, rasqal_graph_pattern* graph_pattern, int flags, rasqal_update_graph_applies applies);
void rasqal_free_update_operation(rasqal_update_operation *update);
int rasqal_update_operation_print(rasqal_update_operation *update, FILE* stream);
int rasqal_query_add_update_operation(rasqal_query* query, rasqal_update_operation *update);


/* rasqal_bindings.c */
rasqal_bindings* rasqal_new_bindings(rasqal_query* query, raptor_sequence* variables, raptor_sequence* rows);
rasqal_bindings* rasqal_new_bindings_from_var_values(rasqal_query* query, rasqal_variable* var, raptor_sequence* values);
rasqal_bindings* rasqal_new_bindings_from_bindings(rasqal_bindings* bindings);
void rasqal_free_bindings(rasqal_bindings* bindings);
int rasqal_bindings_print(rasqal_bindings* bindings, FILE* fh);
rasqal_row* rasqal_bindings_get_row(rasqal_bindings* bindings, int offset);

/* rasqal_ntriples.c */
rasqal_literal* rasqal_new_literal_from_ntriples_counted_string(rasqal_world* world, unsigned char* string, size_t length);

/* rasqal_projection.c */
rasqal_projection* rasqal_new_projection(rasqal_query* query, raptor_sequence* variables, int wildcard, int distinct);
void rasqal_free_projection(rasqal_projection* projection);
raptor_sequence* rasqal_projection_get_variables_sequence(rasqal_projection* projection);
int rasqal_projection_add_variable(rasqal_projection* projection, rasqal_variable* var);

/* rasqal_regex.c */
int rasqal_regex_match(rasqal_world* world, raptor_locator* locator, const char* pattern, const char* regex_flags, const char* subject, size_t subject_len);

/* rasqal_results_compare.c */
rasqal_results_compare* rasqal_new_results_compare(rasqal_world* world, rasqal_query_results *first_qr, const char* first_qr_label, rasqal_query_results *second_qr, const char* second_qr_label);
void rasqal_free_results_compare(rasqal_results_compare* rrc);
void rasqal_results_compare_set_log_handler(rasqal_results_compare* rrc, void* log_user_data, raptor_log_handler log_handler);
int rasqal_results_compare_compare(rasqal_results_compare* rrc);
rasqal_variable* rasqal_results_compare_get_variable_by_offset(rasqal_results_compare* rrc, int idx);
int rasqal_results_compare_get_variable_offset_for_result(rasqal_results_compare* rrc, int var_idx, int qr_index);
int rasqal_results_compare_variables_equal(rasqal_results_compare* rrc);
void rasqal_print_results_compare(FILE *handle, rasqal_results_compare* rrc);

/* rasqal_service.c */
rasqal_service* rasqal_new_service_from_service(rasqal_service* svc);

/* rasqal_solution_modifier.c */
rasqal_solution_modifier* rasqal_new_solution_modifier(rasqal_query* query, raptor_sequence* order_conditions, raptor_sequence* group_conditions, raptor_sequence* having_conditions, int limit, int offset);
void rasqal_free_solution_modifier(rasqal_solution_modifier* sm);

/* rasqal_triples.c */
int rasqal_triples_sequence_set_origin(raptor_sequence* dest_seq, raptor_sequence* src_seq, rasqal_literal* origin);

/* rasqal_random.c */
/**
 * RASQAL_RANDOM_STATE_SIZE:
 *
 * Size of BSD random state
 * 
 * "With 256 bytes of state information, the period of the random
 * number generator is greater than 2**69 , which should be
 * sufficient for most purposes." - BSD random(3) man page
 */
#define RASQAL_RANDOM_STATE_SIZE 256


/**
 * rasqal_random:
 * @world: world object
 * @seed: used for rand_r() (if available) or srand()
 * @state: used for BSD initstate(), setstate() and random() (if available)
 * @data: internal to random algorithm
 *
 * A class providing a random number generator
 *
 */
struct rasqal_random_s {
  rasqal_world* world;
  unsigned int seed;
  char state[RASQAL_RANDOM_STATE_SIZE];
  void* data;
};

unsigned int rasqal_random_get_system_seed(rasqal_world *world);
rasqal_random* rasqal_new_random(rasqal_world *world);
void rasqal_free_random(rasqal_random *random_object);
int rasqal_random_seed(rasqal_random *random_object, unsigned int seed);
int rasqal_random_irand(rasqal_random *random_object);
double rasqal_random_drand(rasqal_random *random_object);

/* rasqal_sort.c */
#if RAPTOR_VERSION < 20015
void** rasqal_sequence_as_sorted(raptor_sequence* seq,  raptor_data_compare_arg_handler compare, void* user_data);
#endif
int* rasqal_variables_table_get_order(rasqal_variables_table* vt);

/*
 * rasqal_digest_type:
 * RASQAL_DIGEST_NONE: No digest
 * RASQAL_DIGEST_MD5: MD5
 * RASQAL_DIGEST_SHA1: SHA1
 * RASQAL_DIGEST_SHA224: SHA224
 * RASQAL_DIGEST_SHA256: SHA256
 * RASQAL_DIGEST_SHA384: SHA384
 * RASQAL_DIGEST_SHA512: SHA512
 * RASQAL_DIGEST_LAST: Internal
 *
 * INTERNAL - Message digest algorithm for rasqal_digest_buffer()
*/
typedef enum {
  RASQAL_DIGEST_NONE,
  RASQAL_DIGEST_MD5,
  RASQAL_DIGEST_SHA1,
  RASQAL_DIGEST_SHA224,
  RASQAL_DIGEST_SHA256,
  RASQAL_DIGEST_SHA384,
  RASQAL_DIGEST_SHA512,
  RASQAL_DIGEST_LAST = RASQAL_DIGEST_SHA512
} rasqal_digest_type;

int rasqal_digest_buffer(rasqal_digest_type type, unsigned char *output, const unsigned char * const input, size_t len);
#ifdef RASQAL_DIGEST_INTERNAL

int rasqal_digest_sha1_buffer(const unsigned char *output,
                              const unsigned char *input, size_t len);
int rasqal_digest_md5_buffer(const unsigned char *output,
                             const unsigned char *input, size_t len);
#endif

/* snprint.c */
size_t rasqal_format_integer(char* buffer, size_t bufsize, int integer, int width, char padding);

/* Safe casts: widening a value */
#define RASQAL_GOOD_CAST(t, v) (t)(v)

/* Unsafe casts: narrowing a value */
#define RASQAL_BAD_CAST(t, v) (t)(v)

/* Converting a double / float to int - OK but not great */
#define RASQAL_FLOATING_AS_INT(v) ((int)(v))

/* IEEE 32 bit double ~ 1E-07 and 64 bit double  ~ 2E-16 */
#define RASQAL_DOUBLE_EPSILON (DBL_EPSILON)

/* Query scope functions */
int rasqal_query_build_scope_hierarchy(rasqal_query* query);

/* end of RASQAL_INTERNAL */
#endif


#ifdef __cplusplus
}
#endif

#endif

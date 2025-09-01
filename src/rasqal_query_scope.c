/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_query_scope.c - Rasqal query scope support
 *
 * Copyright (C) 2025, David Beckett http://www.dajobe.org/
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

#ifdef HAVE_CONFIG_H
#include <rasqal_config.h>
#endif

#ifdef WIN32
#include <win32_rasqal_config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdarg.h>

#include "rasqal.h"
#include "rasqal_internal.h"


#define DEBUG_FH stderr


#ifndef STANDALONE

/**
 * rasqal_new_query_scope:
 * @query: rasqal query (borrowed, must not be NULL)
 * @scope_type: scope type (RASQAL_QUERY_SCOPE_TYPE_*)
 * @parent_scope: parent scope (borrowed, may be NULL for root scope)
 *
 * Create a new query scope with proper hierarchy.
 *
 * The scope manages triple ownership and variable visibility for
 * a specific query execution context (e.g., EXISTS, MINUS, UNION).
 *
 * Ownership:
 * - @query: borrowed reference, must remain valid for scope lifetime
 * - @parent_scope: borrowed reference, must remain valid for scope lifetime
 * - Return value: owned reference, caller must free with rasqal_free_query_scope()
 *
 * The created scope will:
 * - Own its local variables table (local_vars)
 * - Own its visible variables table (visible_vars)
 * - Own its owned triples sequence (owned_triples)
 * - Own its child scopes sequence (child_scopes)
 * - Own its scope name string (scope_name)
 *
 * Return value: new query scope or NULL on failure
 */
rasqal_query_scope*
rasqal_new_query_scope(rasqal_query* query, int scope_type, rasqal_query_scope* parent_scope)
{
  rasqal_query_scope* scope;
  static const char* scope_type_names[] = {
    "ROOT",
    "EXISTS",
    "NOT_EXISTS",
    "MINUS",
    "UNION",
    "SUBQUERY",
    "GROUP"
  };
  int scope_name_size;
  char* scope_name = NULL;

  if(!query)
    return NULL;

  scope = RASQAL_CALLOC(rasqal_query_scope*, 1, sizeof(*scope));
  if(!scope)
    return NULL;

  scope->usage = 1;
  scope->scope_id = query->scope_id_counter++;
  scope->scope_type = scope_type;

  /* Create scope name - always include ID for uniqueness */
  if(scope_type >= 0 && scope_type < (int)(sizeof(scope_type_names)/sizeof(scope_type_names[0]))) {
    scope_name_size = (int)strlen(scope_type_names[scope_type]) + 1 + 10 + 1; /* name + _ + id + \0 */
    scope_name = RASQAL_MALLOC(char*, scope_name_size);
    if(scope_name) {
      snprintf(scope_name, scope_name_size, "%s_%d",
               scope_type_names[scope_type], scope->scope_id);
    }
  }

  if(scope_name) {
    scope->scope_name = scope_name;
  } else {
    /* Fallback to numbered name */
    scope_name_size = 20; /* "SCOPE_" + digits + \0 */
    scope_name = RASQAL_MALLOC(char*, scope_name_size);
    if(scope_name) {
      snprintf(scope_name, scope_name_size, "SCOPE_%d", scope->scope_id);
      scope->scope_name = scope_name;
    }
  }

  scope->parent_scope = parent_scope;

  /* Create owned triples sequence */
  scope->owned_triples = raptor_new_sequence((raptor_data_free_handler)rasqal_free_triple,
                                           (raptor_data_print_handler)rasqal_triple_print);
  if(!scope->owned_triples)
    goto tidy;

  /* Create local variables table */
  scope->local_vars = rasqal_new_variables_table(query->world);
  if(!scope->local_vars)
    goto tidy;

  /* Create visible variables table (computed from local + parent) */
  scope->visible_vars = rasqal_new_variables_table(query->world);
  if(!scope->visible_vars)
    goto tidy;

  /* Create child scopes sequence */
  scope->child_scopes = raptor_new_sequence((raptor_data_free_handler)rasqal_free_query_scope, NULL);
  if(!scope->child_scopes)
    goto tidy;

  return scope;

  tidy:
  if(scope)
    rasqal_free_query_scope(scope);
  return NULL;
}


/**
 * rasqal_free_query_scope:
 * @scope: query scope (owned, may be NULL)
 *
 * Destroy a query scope and all its owned resources.
 *
 * Ownership:
 * - @scope: owned reference, will be freed and set to NULL
 * - All owned resources are freed:
 *   - local_vars (variables table)
 *   - visible_vars (variables table)
 *   - owned_triples (triple sequence)
 *   - child_scopes (scope sequence)
 *   - scope_name (string)
 * - parent_scope is NOT freed (borrowed reference)
 */
void
rasqal_free_query_scope(rasqal_query_scope* scope)
{
  if(!scope)
    return;

  if(--scope->usage)
    return;

  if(scope->scope_name)
    RASQAL_FREE(char*, scope->scope_name);

  if(scope->owned_triples)
    raptor_free_sequence(scope->owned_triples);

  if(scope->local_vars)
    rasqal_free_variables_table(scope->local_vars);

  if(scope->visible_vars)
    rasqal_free_variables_table(scope->visible_vars);

  if(scope->child_scopes)
    raptor_free_sequence(scope->child_scopes);

  /* Don't free parent_scope - it's managed by its own creator */

  RASQAL_FREE(rasqal_query_scope, scope);
}


/**
 * rasqal_query_scope_compute_visible_variables:
 * @scope: query scope
 *
 * Compute the visible variables for this scope by inheriting from parent
 * and including local variables. This implements the SPARQL 1.1 variable
 * visibility rules.
 *
 * Return value: non-zero on failure
 */
int
rasqal_query_scope_compute_visible_variables(rasqal_query_scope* scope)
{
  int i;
  int parent_visible_count = 0;

  if(!scope)
    return 1;

  /* Start with fresh visible variables table */
  rasqal_free_variables_table(scope->visible_vars);
  scope->visible_vars = rasqal_new_variables_table_from_variables_table(scope->local_vars);
  if(!scope->visible_vars)
    return 1;

  /* Inherit visible variables from parent scope */
  if(scope->parent_scope && scope->parent_scope->visible_vars) {
    parent_visible_count = rasqal_variables_table_get_total_variables_count(scope->parent_scope->visible_vars);

    for(i = 0; i < parent_visible_count; i++) {
      rasqal_variable* var = rasqal_variables_table_get(scope->parent_scope->visible_vars, i);
      if(var && !rasqal_variables_table_contains(scope->visible_vars, var->type, var->name)) {
        rasqal_variables_table_add_variable(scope->visible_vars, var);
      }
    }
  }

  return 0;
}


/**
 * rasqal_query_scope_add_child_scope:
 * @parent: parent scope
 * @child: child scope to add
 *
 * Add a child scope to a parent scope, establishing the hierarchy.
 * The parent takes ownership of the child.
 *
 * Return value: non-zero on failure
 */
int
rasqal_query_scope_add_child_scope(rasqal_query_scope* parent, rasqal_query_scope* child)
{
  if(!parent || !child)
    return 1;

  if(!parent->child_scopes)
    return 1;

  /* Set parent reference in child */
  child->parent_scope = parent;

  /* Add child to parent's children sequence */
  return raptor_sequence_push(parent->child_scopes, child);
}


/**
 * rasqal_query_scope_add_triple:
 * @scope: query scope
 * @triple: triple to add to scope (scope takes ownership)
 *
 * Add a triple to the scope's owned triples.
 *
 * Return value: non-zero on failure
 */
int
rasqal_query_scope_add_triple(rasqal_query_scope* scope, rasqal_triple* triple)
{
  if(!scope || !triple)
    return 1;

  if(!scope->owned_triples)
    return 1;

  return raptor_sequence_push(scope->owned_triples, triple);
}


/**
 * rasqal_query_scope_get_root:
 * @scope: query scope
 *
 * Get the root scope of the scope hierarchy.
 *
 * Return value: root scope or NULL if not found
 */
rasqal_query_scope*
rasqal_query_scope_get_root(rasqal_query_scope* scope)
{
  if(!scope)
    return NULL;

  while(scope->parent_scope) {
    scope = scope->parent_scope;
  }

  return scope;
}

#endif /* not STANDALONE */

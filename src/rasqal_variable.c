/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_variable.c - Rasqal variable support
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

#if 0
#define RASQAL_DEBUG_VARIABLE_USAGE
#endif

#ifndef STANDALONE


/**
 * rasqal_new_variable_from_variable:
 * @v: #rasqal_variable to copy
 *
 * Copy Constructor - Create a new Rasqal variable from an existing one
 *
 * This adds a new reference to the variable, it does not do a deep copy
 *
 * Return value: a new #rasqal_variable or NULL on failure.
 **/
rasqal_variable*
rasqal_new_variable_from_variable(rasqal_variable* v)
{
  if(!v)
    return NULL;
  
  v->usage++;
  
#ifdef RASQAL_DEBUG_VARIABLE_USAGE
  RASQAL_DEBUG3("Variable %s usage increased to %d\n", v->name, v->usage);
#endif

  return v;
}

/**
 * rasqal_free_variable:
 * @v: #rasqal_variable object
 *
 * Destructor - Destroy a Rasqal variable object.
 *
 **/
void
rasqal_free_variable(rasqal_variable* v)
{
  if(!v)
    return;

#ifdef RASQAL_DEBUG_VARIABLE_USAGE
  v->usage--;
  RASQAL_DEBUG3("Variable %s usage decreased to %d\n", v->name, v->usage);
  if(v->usage)
    return;
#else
  if(--v->usage)
    return;
#endif
  
  if(v->name)
    RASQAL_FREE(char*, v->name);

  if(v->value)
    rasqal_free_literal(v->value);

  if(v->expression)
    rasqal_free_expression(v->expression);

  RASQAL_FREE(rasqal_variable, v);
}


/**
 * rasqal_variable_write:
 * @v: the #rasqal_variable object
 * @iostr: the #raptor_iostream handle to write to
 *
 * Write a Rasqal variable to an iostream in a debug format.
 * 
 * The write debug format may change in any release.
 * 
 **/
void
rasqal_variable_write(rasqal_variable* v, raptor_iostream* iostr)
{
  if(!v || !iostr)
    return;
    
  if(v->type == RASQAL_VARIABLE_TYPE_ANONYMOUS)
    raptor_iostream_counted_string_write("anon-variable(", 14, iostr);
  else
    raptor_iostream_counted_string_write("variable(", 9, iostr);

  raptor_iostream_string_write(v->name, iostr);

  if(v->expression) {
    raptor_iostream_write_byte('=', iostr);
    rasqal_expression_write(v->expression, iostr);
  }

  if(v->value) {
    raptor_iostream_write_byte('=', iostr);
    rasqal_literal_write(v->value, iostr);
  }

#ifdef RASQAL_DEBUG_VARIABLE_USAGE
  raptor_iostream_write_byte('[', iostr);
  raptor_iostream_decimal_write(v->usage, iostr);
  raptor_iostream_write_byte(']', iostr);
#endif

  raptor_iostream_write_byte(')', iostr);
}


/*
 * rasqal_variables_write:
 * @seq: sequence of #rasqal_variable to write
 * @iostr: the #raptor_iostream handle to write to
 *
 * INTERNAL - Write a sequence of Rasqal variable to an iostream in a debug format.
 *
 * The write debug format may change in any release.
 *
 **/
int
rasqal_variables_write(raptor_sequence* seq, raptor_iostream* iostr)
{
  int vars_size;
  int i;

  if(!seq || !iostr)
    return 1;

  vars_size = raptor_sequence_size(seq);
  for(i = 0; i < vars_size; i++) {
    rasqal_variable* v;

    v = (rasqal_variable*)raptor_sequence_get_at(seq, i);
    if(i > 0)
      raptor_iostream_counted_string_write(", ", 2, iostr);

    rasqal_variable_write(v, iostr);
  }

  return 0;
}


/**
 * rasqal_variable_print:
 * @v: the #rasqal_variable object
 * @fh: the FILE* handle to print to
 *
 * Print a Rasqal variable in a debug format.
 * 
 * The print debug format may change in any release.
 * 
 * Return value: non-0 on failure
 **/
int
rasqal_variable_print(rasqal_variable* v, FILE* fh)
{
  if(v->type == RASQAL_VARIABLE_TYPE_ANONYMOUS)
    fprintf(fh, "anon-variable(%s", v->name);
  else
    fprintf(fh, "variable(%s", v->name);

  if(v->expression) {
    fputc('=', fh);
    rasqal_expression_print(v->expression, fh);
  }

  if(v->value) {
    fputc('=', fh);
    rasqal_literal_print(v->value, fh);
  }

#ifdef RASQAL_DEBUG_VARIABLE_USAGE
  fprintf(fh, "[%d]", v->usage);
#endif

  fputc(')', fh);

  return 0;
}


/**
 * rasqal_variable_set_value:
 * @v: the #rasqal_variable object
 * @l: the #rasqal_literal value to set (or NULL)
 *
 * Set the value of a Rasqal variable.
 * 
 * The variable value is an input parameter and is copied in, not shared.
 * If the variable value is NULL, any existing value is deleted.
 * 
 **/
void
rasqal_variable_set_value(rasqal_variable* v, rasqal_literal* l)
{
  if(v->value)
    rasqal_free_literal(v->value);

  v->value = l;

#ifdef RASQAL_DEBUG
  if(!v->name)
    RASQAL_FATAL1("variable has no name");

  RASQAL_DEBUG2("setting variable %s to value ", v->name);
  rasqal_literal_print(v->value, stderr);
  fputc('\n', stderr);
#endif
}



/*
 * A table of variables with optional binding values
 *
 * variables are named or anonymous (cannot be selected).
 */
struct rasqal_variables_table_s {
  rasqal_world* world;

  /* usage/reference count */
  int usage;
  
  /* The variables of size @variables_count + @anon_variables_count
   * shared pointers into @variables_sequence and @anon_variables_sequence
   */
  rasqal_variable** variables;

  /* Named variables (owner) */
  raptor_sequence* variables_sequence;
  int variables_count;

  /* Anonymous variables (owner) */
  raptor_sequence* anon_variables_sequence;
  int anon_variables_count;

  /* array of variable names.  The array is allocated here but the
   * pointers are into the #variables_sequence above.  It is only
   * allocated if rasqal_variables_table_get_names() is called
   * on demand, otherwise is NULL.
   */
  const unsigned char** variable_names;
};



/**
 * rasqal_new_variables_table:
 * @world: rasqal world
 *
 * Constructor - create a new variables table
 *
 * Return value: new variables table or NULL On failure
 */
rasqal_variables_table*
rasqal_new_variables_table(rasqal_world* world)
{
  rasqal_variables_table* vt;
  
  vt = RASQAL_CALLOC(rasqal_variables_table*, 1, sizeof(*vt));
  if(!vt)
    return NULL;

  vt->usage = 1;
  vt->world = world;

  vt->variables_sequence = raptor_new_sequence((raptor_data_free_handler)rasqal_free_variable,
                                               (raptor_data_print_handler)rasqal_variable_print);
  if(!vt->variables_sequence)
    goto tidy;

  vt->anon_variables_sequence = raptor_new_sequence((raptor_data_free_handler)rasqal_free_variable,
                                                    (raptor_data_print_handler)rasqal_variable_print);
  if(!vt->anon_variables_sequence)
    goto tidy;

  vt->variable_names = NULL;

  return vt;

  tidy:
  rasqal_free_variables_table(vt);
  vt = NULL;
  
  return vt;
}


rasqal_variables_table*
rasqal_new_variables_table_from_variables_table(rasqal_variables_table* vt)
{
  vt->usage++;
  return vt;
}


/**
 * rasqal_free_variables_table:
 * @vt: rasqal variables table
 *
 * Destructor - destroy a new variables table
 */
void
rasqal_free_variables_table(rasqal_variables_table* vt)
{
  if(!vt)
    return;

  if(--vt->usage)
    return;
  
  if(vt->variables)
    RASQAL_FREE(vararray, vt->variables);

  if(vt->anon_variables_sequence)
    raptor_free_sequence(vt->anon_variables_sequence);

  if(vt->variables_sequence)
    raptor_free_sequence(vt->variables_sequence);

  if(vt->variable_names)
    RASQAL_FREE(cstrings, vt->variable_names);

  RASQAL_FREE(rasqal_variables_table, vt);
}


/**
 * rasqal_variables_table_add_variable:
 * @vt: #rasqal_variables_table to associate the variable with
 * @variable: existing variable to add
 *
 * Constructor - Add an existing variable to the variables table
 *
 * The variables table @vt takes a reference to @variable.  This
 * function will fail if the variable is already in the table.  Use
 * rasqal_variables_table_contains() to check before calling.
 *
 * Return value: non-0 on failure (such as name already exists)
 **/
int
rasqal_variables_table_add_variable(rasqal_variables_table* vt,
                                    rasqal_variable* variable)
{
  raptor_sequence* seq = NULL;
  int* count_p = NULL;

  if(!vt)
    return 1;
  
  switch(variable->type) {
    case RASQAL_VARIABLE_TYPE_ANONYMOUS:
      seq = vt->anon_variables_sequence;
      count_p = &vt->anon_variables_count;
      break;
    case RASQAL_VARIABLE_TYPE_NORMAL:
      seq = vt->variables_sequence;
      count_p = &vt->variables_count;
      break;
      
    case RASQAL_VARIABLE_TYPE_UNKNOWN:
    default:
      RASQAL_DEBUG2("Unknown variable type %u", variable->type);
      return 1;
  }
  
  if(rasqal_variables_table_contains(vt, variable->type, variable->name))
    /* variable with this name already present - error */
    return 1;
  
  /* add a new v reference for the variables table's sequence */
  variable = rasqal_new_variable_from_variable(variable);
  if(raptor_sequence_push(seq, variable))
    return 1;

  variable->offset = (*count_p);

  (*count_p)++;

  if(variable->type == RASQAL_VARIABLE_TYPE_ANONYMOUS) {
    /* new anon variable: add base offset */
    variable->offset += vt->variables_count;
  } else {
    int i;

    /* new normal variable: move all anon variable offsets up 1 */
    for(i = 0; i < vt->anon_variables_count; i++) {
      rasqal_variable* anon_v;
      anon_v = (rasqal_variable*)raptor_sequence_get_at(vt->anon_variables_sequence, i);
      anon_v->offset++;
    }
  }

  if(vt->variable_names) {
    RASQAL_FREE(cstrings, vt->variable_names);
    vt->variable_names = NULL;
  }
    
  return 0;
}


/**
 * rasqal_variables_table_add2:
 * @vt: #rasqal_variables_table to associate the variable with
 * @type: variable type defined by enumeration rasqal_variable_type
 * @name: variable name
 * @name_len: length of @name (or 0)
 * @value: variable #rasqal_literal value (or NULL)
 *
 * Constructor - Create a new variable and add it to the variables table
 * 
 * The @name and @value fields are copied.  If a variable with the
 * name already exists, that is returned and the new @value is
 * ignored.
 *
 * Return value: a new #rasqal_variable or NULL on failure.
 **/
rasqal_variable*
rasqal_variables_table_add2(rasqal_variables_table* vt,
                            rasqal_variable_type type, 
                            const unsigned char *name, size_t name_len,
                            rasqal_literal *value)
{
  rasqal_variable* v = NULL;

  if(!vt || !name)
    goto failed;

  if(!name_len)
    name_len = strlen(RASQAL_GOOD_CAST(const char*, name));

  if(!name_len)
    goto failed;

  /* If already present, just return a new reference to it */
  v = rasqal_variables_table_get_by_name(vt, type, name);
  if(v)
    return rasqal_new_variable_from_variable(v);

  v = RASQAL_CALLOC(rasqal_variable*, 1, sizeof(*v));
  if(!v)
    goto failed;

  v->offset = -1;
  v->usage = 1;
  v->vars_table = vt;
  v->type = type;
  v->name = RASQAL_MALLOC(unsigned char*, name_len + 1);
  memcpy(RASQAL_GOOD_CAST(char*, v->name), name, name_len + 1);
  v->value = rasqal_new_literal_from_literal(value);
  
  if(rasqal_variables_table_add_variable(vt, v))
    goto failed;

  return v;


  failed:
  if(v)
    RASQAL_FREE(rasqal_variable*, v);
  
  return NULL;
}


/**
 * rasqal_variables_table_add:
 * @vt: #rasqal_variables_table to associate the variable with
 * @type: variable type defined by enumeration rasqal_variable_type
 * @name: variable name
 * @value: variable #rasqal_literal value (or NULL)
 *
 * Constructor - Create a new variable and add it to the variables table
 *
 * @Deprecated: for rasqal_variables_table_add2() which copies the @name
 * and @value
 *
 * The @name and @value become owned by the rasqal_variable
 * structure.  If a variable with the name already exists, that is
 * returned and the new @value is ignored.
 *
 * Return value: a new #rasqal_variable or NULL on failure.
 **/
rasqal_variable*
rasqal_variables_table_add(rasqal_variables_table* vt,
                           rasqal_variable_type type,
                           const unsigned char *name, rasqal_literal *value)
{
  rasqal_variable* v;

  if(!vt || !name)
    return NULL;

  v = rasqal_variables_table_add2(vt, type, name, 0, value);
  RASQAL_FREE(char*, name);
  if(value)
    rasqal_free_literal(value);
  return v;
}


rasqal_variable*
rasqal_variables_table_get(rasqal_variables_table* vt, int idx)
{
  raptor_sequence* seq = NULL;

  if(idx < 0)
    return NULL;
  
  if(idx < vt->variables_count)
    seq = vt->variables_sequence;
  else {
    idx -= vt->variables_count;
    seq = vt->anon_variables_sequence;
  }
  
  return (rasqal_variable*)raptor_sequence_get_at(seq, idx);
}


rasqal_literal*
rasqal_variables_table_get_value(rasqal_variables_table* vt, int idx)
{
  rasqal_variable* v;

  v = rasqal_variables_table_get(vt, idx);
  if(!v)
    return NULL;

  return v->value;
}


/**
 * rasqal_variables_table_get_by_name:
 * @vt: the variables table
 * @type: the variable type to match or #RASQAL_VARIABLE_TYPE_UNKNOWN for any.
 * @name: the variable type
 *
 * Lookup a variable by type and name in the variables table.
 *
 * Note that looking up for any type #RASQAL_VARIABLE_TYPE_UNKNOWN
 * may a name match but for any type so in cases where the query has
 * both a named and anonymous (extensional) variable, an arbitrary one
 * will be returned.
 *
 * Return value: a shared pointer to the #rasqal_variable or NULL if not found
 **/
rasqal_variable*
rasqal_variables_table_get_by_name(rasqal_variables_table* vt,
                                   rasqal_variable_type type,
                                   const unsigned char *name)
{
  int i;
  rasqal_variable* v;

  for(i = 0; (v = rasqal_variables_table_get(vt, i)); i++) {
    if(((type != RASQAL_VARIABLE_TYPE_UNKNOWN) && v->type == type) &&
       !strcmp(RASQAL_GOOD_CAST(const char*, v->name),
               RASQAL_GOOD_CAST(const char*, name)))
      return v;
  }
  return NULL;
}


/**
 * rasqal_variables_table_contains:
 * @vt: #rasqal_variables_table to lookup
 * @type: variable type
 * @name: variable name
 *
 * Check if there is a variable with the given type and name in the variables table
 *
 * Return value: non-0 if the variable is present
 */
int
rasqal_variables_table_contains(rasqal_variables_table* vt,
                                rasqal_variable_type type,
                                const unsigned char *name)
{
  return (rasqal_variables_table_get_by_name(vt, type, name) != NULL);
}


int
rasqal_variables_table_set(rasqal_variables_table* vt,
                           rasqal_variable_type type,
                           const unsigned char *name, rasqal_literal* value)
{
  rasqal_variable* v;
  
  v = rasqal_variables_table_get_by_name(vt, type, name);
  if(!v)
    return 1;

  rasqal_variable_set_value(v, value);
  return 0;
}


int
rasqal_variables_table_get_named_variables_count(rasqal_variables_table* vt)
{
  return vt->variables_count;
}


int
rasqal_variables_table_get_anonymous_variables_count(rasqal_variables_table* vt)
{
  return vt->anon_variables_count;
}


int
rasqal_variables_table_get_total_variables_count(rasqal_variables_table* vt)
{
  return vt->variables_count + vt->anon_variables_count;
}


raptor_sequence*
rasqal_variables_table_get_named_variables_sequence(rasqal_variables_table* vt)
{
  return vt->variables_sequence;
}


raptor_sequence*
rasqal_variables_table_get_anonymous_variables_sequence(rasqal_variables_table* vt)
{
  return vt->anon_variables_sequence;
}


const unsigned char**
rasqal_variables_table_get_names(rasqal_variables_table* vt)
{
  int size = vt->variables_count;
  
  if(!vt->variable_names && size) {
    int i;
    
    vt->variable_names = RASQAL_CALLOC(const unsigned char**, RASQAL_GOOD_CAST(size_t, (size + 1)),
                                       sizeof(unsigned char*));
    if(!vt->variable_names)
      return NULL;

    for(i = 0; i < size; i++) {
      rasqal_variable* v;

      v = (rasqal_variable*)raptor_sequence_get_at(vt->variables_sequence, i);
      vt->variable_names[i] = v->name;
    }
  }

  return vt->variable_names;
}


/*
 * Deep copy a sequence of rasqal_variable to a new one.
 */
raptor_sequence*
rasqal_variable_copy_variable_sequence(raptor_sequence* vars_seq)
{
  raptor_sequence* nvars_seq = NULL;
  int size;
  int i;
  
  if(!vars_seq)
    return NULL;
  
  nvars_seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_variable,
                                  (raptor_data_print_handler)rasqal_variable_print);
  if(!nvars_seq)
    return NULL;

  size = raptor_sequence_size(vars_seq);
  for(i = 0; i < size; i++) {
    rasqal_variable* v;

    v = (rasqal_variable*)raptor_sequence_get_at(vars_seq, i);
    v = rasqal_new_variable_from_variable(v);
    raptor_sequence_set_at(nvars_seq, i, v);
  }
  
  return nvars_seq;
}



#if RAPTOR_VERSION < 20015
/* pointers are rasqal_variable** */
static int
rasqal_variable_compare_by_name_arg(const void *a, const void *b, void *arg)
{
  rasqal_variable *var_a;
  rasqal_variable *var_b;

  var_a = *(rasqal_variable**)a;
  var_b = *(rasqal_variable**)b;

  return strcmp(RASQAL_GOOD_CAST(const char*, var_a->name),
                RASQAL_GOOD_CAST(const char*, var_b->name));
}

#else
/* pointers are int* */
static int
rasqal_order_compare_by_name_arg(const void *a, const void *b, void *arg)
{
  int offset_a;
  int offset_b;
  rasqal_variables_table* vt;
  const unsigned char* name_a;
  const unsigned char* name_b;

  offset_a = *(int*)a;
  offset_b = *(int*)b;
  vt = (rasqal_variables_table*)arg;

  name_a = rasqal_variables_table_get(vt, offset_a)->name;
  name_b = rasqal_variables_table_get(vt, offset_b)->name;

  return strcmp(RASQAL_GOOD_CAST(const char*, name_a),
                RASQAL_GOOD_CAST(const char*, name_b));
}
#endif


/*
 * rasqal_variables_table_get_order:
 * @vt: variables table
 *
 * INTERNAL - Get the order of the variables in sort order
 *
 * Return value: array of integers of order variables (terminated by integer < 0) or NULL on failure
*/
int*
rasqal_variables_table_get_order(rasqal_variables_table* vt)
{
  raptor_sequence* seq;
  int size;
  int* order;
  int i;
#if RAPTOR_VERSION < 20015
  void** array;
#endif

  seq = rasqal_variables_table_get_named_variables_sequence(vt);
  if(!seq)
    return NULL;

  size = raptor_sequence_size(seq);
  if(!size)
    return NULL;

  order = RASQAL_CALLOC(int*, RASQAL_GOOD_CAST(size_t, size + 1), sizeof(int));
  if(!order)
    return NULL;

#if RAPTOR_VERSION < 20015
  array = rasqal_sequence_as_sorted(seq, rasqal_variable_compare_by_name_arg,
                                    vt);
  if(!array)
    return NULL;

  for(i = 0; i < size; i++)
    order[i] = (RASQAL_GOOD_CAST(rasqal_variable*, array[i]))->offset;
  RASQAL_FREE(void*, array);
#else
  for(i = 0; i < size; i++)
    order[i] = i;

  raptor_sort_r(order, size, sizeof(int), rasqal_order_compare_by_name_arg, vt);
#endif
  order[size] = -1;

  return order;
}


#endif /* not STANDALONE */



#ifdef STANDALONE
#include <stdio.h>

int main(int argc, char *argv[]);


int
main(int argc, char *argv[]) 
{
  const char *program = rasqal_basename(argv[0]);
  rasqal_world* world = NULL;
  rasqal_variables_table* vt = NULL;
#define NUM_VARS 3
  const char* var_names[NUM_VARS] = {"normal-null", "normal-value", "anon"};
  rasqal_variable* vars[NUM_VARS];
  rasqal_literal *value = NULL;
  int i;
  int rc = 0;
  
  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    rc = 1;
    goto tidy;
  }
  
  vt = rasqal_new_variables_table(world);
  if(!vt) {
    fprintf(stderr, "%s: Failed to make variables table\n", program);
    rc = 1;
    goto tidy;
  }

  vars[0] = rasqal_variables_table_add2(vt, RASQAL_VARIABLE_TYPE_NORMAL,
                                        RASQAL_GOOD_CAST(const unsigned char*, var_names[0]),
                                        0, NULL);
  if(!vars[0]) {
    fprintf(stderr, "%s: Failed to make normal variable with NULL value\n",
            program);
    rc = 1;
    goto tidy;
  }
  /* vars[0] now owned by vt */

  value = rasqal_new_double_literal(world, 42.0);
  if(!value) {
    fprintf(stderr, "%s: Failed to make double literal\n", program);
    rc = 1;
    goto tidy;
  }
  vars[1] = rasqal_variables_table_add2(vt, RASQAL_VARIABLE_TYPE_NORMAL,
                                        RASQAL_GOOD_CAST(const unsigned char*, var_names[1]),
                                        0, value);
  if(!vars[1]) {
    fprintf(stderr, "%s: Failed to make normal variable with literal value\n",
            program);
    rc = 1;
    goto tidy;
  }
  /* vars[1] now owned by vt */
  
  vars[2] = rasqal_variables_table_add2(vt, RASQAL_VARIABLE_TYPE_ANONYMOUS,
                                        RASQAL_GOOD_CAST(const unsigned char*, var_names[2]),
                                        0, NULL);
  if(!vars[2]) {
    fprintf(stderr, "%s: Failed to make anonymous variable with NULL value\n",
            program);
    rc = 1;
    goto tidy;
  }
  /* vars[2] now owned by vt */
  
  tidy:
  for(i = 0; i < NUM_VARS; i++) {
    if(vars[i])
      rasqal_free_variable(vars[i]);
  }
  
  if(value)
    rasqal_free_literal(value);
  if(vt)
    rasqal_free_variables_table(vt);
  
  if(world)
    rasqal_free_world(world);

  return rc;
}
#endif /* STANDALONE */

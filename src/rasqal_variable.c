/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_variable.c - Rasqal variable support
 *
 * Copyright (C) 2003-2008, David Beckett http://www.dajobe.org/
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

#ifdef RASQAL_REGEX_PCRE
#include <pcre.h>
#endif

#ifdef RASQAL_REGEX_POSIX
#include <sys/types.h>
#include <regex.h>
#endif

#include "rasqal.h"
#include "rasqal_internal.h"


#ifndef STANDALONE


/**
 * rasqal_new_variable_typed:
 * @rq: #rasqal_query to associate the variable with
 * @type: variable type defined by enumeration rasqal_variable_type
 * @name: variable name
 * @value: variable #rasqal_literal value (or NULL)
 *
 * Constructor - Create a new typed Rasqal variable.
 * 
 * The variable must be associated with a query, since variable
 * names are only significant with a single query.
 * 
 * The @name and @value become owned by the rasqal_variable structure
 *
 * Return value: a new #rasqal_variable or NULL on failure.
 **/
rasqal_variable*
rasqal_new_variable_typed(rasqal_query* rq,
                          rasqal_variable_type type, 
                          unsigned char *name, rasqal_literal *value)
{
  return rasqal_variables_table_add(rq->vars_table, type, name, value);
}


/**
 * rasqal_new_variable:
 * @rq: #rasqal_query to associate the variable with
 * @name: variable name
 * @value: variable #rasqal_literal value (or NULL)
 *
 * Constructor - Create a new Rasqal normal variable.
 * 
 * The variable must be associated with a query, since variable
 * names are only significant with a single query.
 *
 * This creates a regular variable that can be returned of type
 * RASQAL_VARIABLE_TYPE_NORMAL.  Use rasqal_new_variable_typed
 * to create other variables.
 * 
 * The @name and @value become owned by the rasqal_variable structure
 *
 * Return value: a new #rasqal_variable or NULL on failure.
 **/
rasqal_variable*
rasqal_new_variable(rasqal_query* rq,
                    unsigned char *name, rasqal_literal *value) 
{
  return rasqal_variables_table_add(rq->vars_table, RASQAL_VARIABLE_TYPE_NORMAL,
                                    name, value);
}


/**
 * rasqal_new_variable_from_variable:
 * @v: #rasqal_variable to copy
 *
 * Copy Constructor - Create a new Rasqal variable from an existing one
 *
 * This does a deep copy of all variable fields
 *
 * Return value: a new #rasqal_variable or NULL on failure.
 **/
rasqal_variable*
rasqal_new_variable_from_variable(rasqal_variable* v)
{
  rasqal_variable* new_v;
  size_t name_len;
  unsigned char *new_name;

  new_v=(rasqal_variable*)RASQAL_CALLOC(rasqal_variable, 1, sizeof(rasqal_variable));
  if(!new_v)
    return NULL;
  
  name_len=strlen((const char*)v->name);
  new_name=(unsigned char*)RASQAL_MALLOC(cstring, name_len+1);
  if(!new_name) {
    RASQAL_FREE(rasqal_variable, new_v);
    return NULL;
  }
  memcpy(new_name, v->name, name_len+1);
  
  new_v->name= new_name;
  new_v->value= rasqal_new_literal_from_literal(v->value);
  new_v->offset= v->offset;
  new_v->type= v->type;
  new_v->expression= rasqal_new_expression_from_expression(v->expression);

  return new_v;
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
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(v, rasqal_variable);
  
  if(v->name)
    RASQAL_FREE(cstring, (void*)v->name);
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
  if(v->type == RASQAL_VARIABLE_TYPE_ANONYMOUS)
    raptor_iostream_write_counted_string(iostr, "anon-variable(", 14);
  else
    raptor_iostream_write_counted_string(iostr, "variable(", 9);
  raptor_iostream_write_string(iostr, v->name);
  if(v->expression) {
    raptor_iostream_write_byte(iostr, '=');
    rasqal_expression_write(v->expression, iostr);
  }
  if(v->value) {
    raptor_iostream_write_byte(iostr, '=');
    rasqal_literal_write(v->value, iostr);
  }
  raptor_iostream_write_byte(iostr, ')');
}


/**
 * rasqal_variable_print:
 * @v: the #rasqal_variable object
 * @fh: the #FILE* handle to print to
 *
 * Print a Rasqal variable in a debug format.
 * 
 * The print debug format may change in any release.
 * 
 **/
void
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
  fputc(')', fh);
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
  v->value=l;
#ifdef RASQAL_DEBUG
  if(!v->name)
    RASQAL_FATAL1("variable has no name");
  RASQAL_DEBUG2("setting variable %s to value ", v->name);
  if(v->value)
    rasqal_literal_print(v->value, stderr);
  else
    fputs("(NULL)", stderr);
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



rasqal_variables_table*
rasqal_new_variables_table(rasqal_world* world)
{
  rasqal_variables_table* vt;
  
  vt = (rasqal_variables_table*)RASQAL_CALLOC(rasqal_variables_table, 1,
                                              sizeof(rasqal_variables_table));
  if(!vt)
    return NULL;

  vt->world = world;
  
  vt->variables_sequence = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_variable, (raptor_sequence_print_handler*)rasqal_variable_print);
  if(!vt->variables_sequence)
    goto tidy;

  vt->anon_variables_sequence = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_variable, (raptor_sequence_print_handler*)rasqal_variable_print);
  if(!vt->anon_variables_sequence)
    goto tidy;

  vt->variable_names = NULL;

  vt->usage = 1;
  
  return vt;

  tidy:
  rasqal_free_variables_table(vt);
  vt=NULL;
  
  return vt;
}


rasqal_variables_table*
rasqal_new_variables_table_from_variables_table(rasqal_variables_table* vt)
{
  vt->usage++;
  return vt;
}


void
rasqal_free_variables_table(rasqal_variables_table* vt)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN(vt, rasqal_variables_table);

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
 * rasqal_variables_table_add:
 * @vt: #rasqal_variables_table to associate the variable with
 * @type: variable type defined by enumeration rasqal_variable_type
 * @name: variable name
 * @value: variable #rasqal_literal value (or NULL)
 *
 * Constructor - Add a variable to the variables table
 * 
 * The @name and @value become owned by the rasqal_variable structure
 *
 * Return value: a new #rasqal_variable or NULL on failure.
 **/
rasqal_variable*
rasqal_variables_table_add(rasqal_variables_table* vt,
                           rasqal_variable_type type, 
                           const unsigned char *name, rasqal_literal *value)
{
  int i;
  rasqal_variable* v;
  raptor_sequence* seq=NULL;
  int* count_p=NULL;

  if(!vt)
    return NULL;
  
  switch(type) {
    case RASQAL_VARIABLE_TYPE_ANONYMOUS:
      seq=vt->anon_variables_sequence;
      count_p=&vt->anon_variables_count;
      break;
    case RASQAL_VARIABLE_TYPE_NORMAL:
      seq=vt->variables_sequence;
      count_p=&vt->variables_count;
      break;
      
    case RASQAL_VARIABLE_TYPE_UNKNOWN:
    default:
      RASQAL_DEBUG2("Unknown variable type %d", type);
      return NULL;
  }
  
  for(i=0; i< raptor_sequence_size(seq); i++) {
    v=(rasqal_variable*)raptor_sequence_get_at(seq, i);
    if(!strcmp((const char*)v->name, (const char*)name)) {
      /* name already present, do not need a copy */
      RASQAL_FREE(cstring, name);
      return v;
    }
  }

  
  v=(rasqal_variable*)RASQAL_CALLOC(rasqal_variable, 1,
                                    sizeof(rasqal_variable));
  if(v) {
    v->type= type;
    v->name= name;
    v->value= value;
    if(count_p)
      v->offset= (*count_p);

    if(seq && raptor_sequence_push(seq, v))
      return NULL;

    if(type == RASQAL_VARIABLE_TYPE_ANONYMOUS) {
      /* new anon variable: add base offset */
      v->offset += vt->variables_count;
    } else {
      /* new normal variable: move all anon variable offsets up 1 */
      for(i=0; i < vt->anon_variables_count; i++) {
        rasqal_variable* anon_v;
        anon_v=(rasqal_variable*)raptor_sequence_get_at(vt->anon_variables_sequence, i);
        anon_v->offset++;
      }
    }
    

    /* Increment count and free var names only after sequence push succeeded */
    if(count_p)
      (*count_p)++;

    if(vt->variable_names) {
      RASQAL_FREE(cstrings, vt->variable_names);
      vt->variable_names = NULL;
    }
  } else {
    RASQAL_FREE(cstring, name);
    if(value)
      rasqal_free_literal(value);
  }
  
  return v;
}



rasqal_variable*
rasqal_variables_table_get(rasqal_variables_table* vt, int idx)
{
  raptor_sequence* seq=NULL;

  if(idx < 0)
    return NULL;
  
  if(idx < vt->variables_count)
    seq=vt->variables_sequence;
  else {
    idx -= vt->variables_count;
    seq=vt->anon_variables_sequence;
  }
  
  return (rasqal_variable*)raptor_sequence_get_at(seq, idx);
}


rasqal_literal*
rasqal_variables_table_get_value(rasqal_variables_table* vt, int idx)
{
  rasqal_variable* v;
  v=rasqal_variables_table_get(vt, idx);
  if(!v)
    return NULL;
  return v->value;
}


rasqal_variable*
rasqal_variables_table_get_by_name(rasqal_variables_table* vt,
                                   const unsigned char *name)
{
  int i;
  rasqal_variable* v;

  for(i=0; (v=rasqal_variables_table_get(vt, i)); i++) {
    if(!strcmp((const char*)v->name, (const char*)name))
      return v;
  }
  return NULL;
}


int
rasqal_variables_table_has(rasqal_variables_table* vt,
                           const unsigned char *name)
{
  return (rasqal_variables_table_get_by_name(vt, name) != NULL);
}


int
rasqal_variables_table_set(rasqal_variables_table* vt,
                           const unsigned char *name, rasqal_literal* value)
{
  rasqal_variable* v;
  
  v=rasqal_variables_table_get_by_name(vt, name);
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
    
    vt->variable_names = (const unsigned char**)RASQAL_CALLOC(cstrings, sizeof(unsigned char*), (size+1));
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

#endif /* not STANDALONE */



#ifdef STANDALONE
#include <stdio.h>

int main(int argc, char *argv[]);


int
main(int argc, char *argv[]) 
{
  const char *program=rasqal_basename(argv[0]);
  rasqal_world* world=NULL;
  rasqal_variables_table* vt=NULL;
#define NUM_VARS 3
  const char* var_names[NUM_VARS]={"normal-null", "normal-value", "anon"};
  unsigned char* names[NUM_VARS];
  rasqal_variable* vars[NUM_VARS];
  rasqal_literal *value=NULL;
  int i;
  int rc=0;
  
  world=rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    rc=1;
    goto tidy;
  }
  
  vt=rasqal_new_variables_table(world);
  if(!vt) {
    fprintf(stderr, "%s: Failed to make variables table\n", program);
    rc=1;
    goto tidy;
  }

  for(i=0; i < NUM_VARS; i++) {
    size_t len=strlen(var_names[i]);
    names[i]=(unsigned char*)malloc(len+1);
    strncpy((char*)names[i], var_names[i], len+1);
  }
  
  vars[0]=rasqal_variables_table_add(vt, RASQAL_VARIABLE_TYPE_NORMAL,
                                     names[0], NULL);
  if(!vars[0]) {
    fprintf(stderr, "%s: Failed to make normal variable with NULL value\n",
            program);
    rc=1;
    goto tidy;
  } else {
    /* now owned by vars[0] owned by vt */
    names[0]=NULL;
  }
  /* vars[0] now owned by vt */

  value=rasqal_new_double_literal(world, 42.0);
  if(!value) {
    fprintf(stderr, "%s: Failed to make double literal\n", program);
    rc=1;
    goto tidy;
  }
  vars[1]=rasqal_variables_table_add(vt, RASQAL_VARIABLE_TYPE_NORMAL,
                                     names[1], value);
  if(!vars[1]) {
    fprintf(stderr, "%s: Failed to make normal variable with literal value\n",
            program);
    rc=1;
    goto tidy;
  } else {
    /* now owned by vars[1] owned by vt */
    names[1]=NULL;
    value=NULL;
  }
  /* vars[1] now owned by vt */
  
  vars[2]=rasqal_variables_table_add(vt, RASQAL_VARIABLE_TYPE_ANONYMOUS,
                                     names[2], NULL);
  if(!vars[2]) {
    fprintf(stderr, "%s: Failed to make anonymous variable with NULL value\n",
            program);
    rc=1;
    goto tidy;
  } else {
    /* now owned by vars[2] owned by vt */
    names[2]=NULL;
  }
  /* vars[2] now owned by vt */
  
  tidy:
  for(i=0; i < NUM_VARS; i++) {
    if(names[i])
      free(names[i]);
  }
  
  if(value)
    rasqal_free_literal(value);
  if(vt)
    rasqal_free_variables_table(vt);
  
  if(world)
    rasqal_free_world(world);

  return 0;
}
#endif

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_format_srj.c - Read SPARQL Results JSON format
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
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <raptor.h>

/* Rasqal includes */
#include <rasqal.h>
#include <rasqal_internal.h>

#include <yajl/yajl_parse.h>

#ifdef HAVE_YAJL2
#define RASQAL_YAJL_LEN_TYPE size_t
#else
#define RASQAL_YAJL_LEN_TYPE unsigned int
#endif

/* Forward declarations */
static rasqal_rowsource* rasqal_query_results_get_rowsource_srj(rasqal_query_results_formatter* formatter, rasqal_world *world, rasqal_variables_table* vars_table, raptor_iostream *iostr, raptor_uri *base_uri, unsigned int flags);
static int rasqal_srj_recognise_syntax(rasqal_query_results_format_factory* factory, const unsigned char *buffer, size_t len, const unsigned char *identifier, const unsigned char *suffix, const char *mime_type);
static int rasqal_srj_rowsource_ensure_variables(rasqal_rowsource* rowsource, void* user_data);
static int rasqal_srj_get_boolean(rasqal_query_results_formatter *formatter, rasqal_world* world, raptor_iostream *iostr, raptor_uri *base_uri, unsigned int flags);
static rasqal_literal* rasqal_srj_create_literal(rasqal_world* world, const char* type, const char* value, const char* datatype, const char* lang);

/* Writer forward declarations */
static int rasqal_query_results_write_srj(rasqal_query_results_formatter* formatter, raptor_iostream *iostr, rasqal_query_results* results, raptor_uri *base_uri);
static void rasqal_srj_write_head(raptor_iostream* iostr, rasqal_query_results* results);
static void rasqal_srj_write_boolean(raptor_iostream* iostr, rasqal_query_results* results);
static void rasqal_srj_write_results(raptor_iostream* iostr, rasqal_query_results* results, rasqal_query* query);
static void rasqal_srj_write_uri(raptor_iostream* iostr, raptor_uri* uri);
static void rasqal_srj_write_literal(raptor_iostream* iostr, rasqal_literal* literal);
static void rasqal_srj_write_bnode(raptor_iostream* iostr, const unsigned char* bnode_id);

/* SRJ parsing context */
typedef struct {
  rasqal_rowsource* rowsource;
  rasqal_variables_table* vars_table;
  rasqal_world* world;
  raptor_iostream* iostr;
  yajl_handle handle;

  /* Parsing state machine */
  enum {
    STATE_BEFORE_ROOT,
    STATE_IN_HEAD,
    STATE_IN_VARS_ARRAY,
    STATE_IN_RESULTS,
    STATE_IN_BINDINGS_ARRAY,
    STATE_IN_BINDING_OBJECT,
    STATE_IN_VALUE_OBJECT,
    STATE_COMPLETE,
    STATE_ERROR
  } state;

  /* Current parsing context */
  char* current_key;
  rasqal_row* current_row;
  rasqal_variable* current_variable;

  /* Current binding value context */
  char* value_type;
  char* value_value;
  char* value_datatype;
  char* value_lang;

  /* Error handling */
  int error_count;
  char* error_message;

  /* Row queue for streaming */
  rasqal_row** rows;
  int rows_count;
  int rows_size;
  int current_row_index;

  /* Boolean result support */
  int is_boolean_result;
  int boolean_value;

  /* Finished flag */
  int finished;
} rasqal_srj_context;

/* YAJL memory allocators using Rasqal's memory management */
static void*
rasqal_srj_yajl_malloc(void* ctx, RASQAL_YAJL_LEN_TYPE size)
{
  return RASQAL_MALLOC(void*, size);
}

static void*
rasqal_srj_yajl_realloc(void* ctx, void* ptr, RASQAL_YAJL_LEN_TYPE size)
{
  return RASQAL_REALLOC(void*, ptr, size);
}

static void
rasqal_srj_yajl_free(void* ctx, void* ptr)
{
  RASQAL_FREE(void*, ptr);
}

static yajl_alloc_funcs rasqal_srj_yajl_alloc_funcs = {
  rasqal_srj_yajl_malloc,
  rasqal_srj_yajl_realloc,
  rasqal_srj_yajl_free,
  NULL
};

/* YAJL callback functions */
static int rasqal_srj_null_handler(void* ctx);
static int rasqal_srj_boolean_handler(void* ctx, int value);
static int rasqal_srj_string_handler(void* ctx, const unsigned char* str, RASQAL_YAJL_LEN_TYPE len);
static int rasqal_srj_start_map_handler(void* ctx);
static int rasqal_srj_map_key_handler(void* ctx, const unsigned char* key, RASQAL_YAJL_LEN_TYPE len);
static int rasqal_srj_end_map_handler(void* ctx);
static int rasqal_srj_start_array_handler(void* ctx);
static int rasqal_srj_end_array_handler(void* ctx);

static yajl_callbacks srj_callbacks = {
  rasqal_srj_null_handler,
  rasqal_srj_boolean_handler,
  NULL, /* integer */
  NULL, /* double */
  NULL, /* number */
  rasqal_srj_string_handler,
  rasqal_srj_start_map_handler,
  rasqal_srj_map_key_handler,
  rasqal_srj_end_map_handler,
  rasqal_srj_start_array_handler,
  rasqal_srj_end_array_handler
};

static const char* const srj_names[] = { "srj", NULL};

static const char* const srj_uri_strings[] = {
  "http://www.w3.org/ns/formats/SPARQL_Results_JSON",
  NULL
};

static const raptor_type_q srj_types[] = {
  { "application/sparql-results+json", 31, 10},
  { "application/json", 16, 5},
  { NULL, 0, 0}
};

static int
rasqal_query_results_srj_register_factory(rasqal_query_results_format_factory *factory)
{
  int rc = 0;

  factory->desc.names = srj_names;
  factory->desc.mime_types = srj_types;

  factory->desc.label = "SPARQL Results JSON";
  factory->desc.uri_strings = srj_uri_strings;

  factory->desc.flags = 0;

  factory->write         = rasqal_query_results_write_srj;
  factory->get_rowsource = rasqal_query_results_get_rowsource_srj;
  factory->recognise_syntax = rasqal_srj_recognise_syntax;
  factory->get_boolean      = rasqal_srj_get_boolean;

  return rc;
}

/* Initialize the SRJ reader subsystem */
int
rasqal_init_result_format_srj(rasqal_world* world)
{
  return !rasqal_world_register_query_results_format_factory(world,
                                                             &rasqal_query_results_srj_register_factory);
}

/* Format recognition function */
static int
rasqal_srj_recognise_syntax(rasqal_query_results_format_factory* factory,
                            const unsigned char *buffer, size_t len,
                            const unsigned char *identifier,
                            const unsigned char *suffix,
                            const char *mime_type)
{
  int score = 0;

  if(suffix) {
    if(!strcmp((const char*)suffix, "srj"))
      score = 9;
  }

  if(mime_type) {
    if(!strcmp(mime_type, "application/sparql-results+json") ||
       !strcmp(mime_type, "application/json"))
      score = 6;
  }

  if(buffer && len > 10) {
    /* Look for SRJ-specific JSON structure */
    if(strstr((const char*)buffer, "\"head\"") &&
       (strstr((const char*)buffer, "\"vars\"") ||
        strstr((const char*)buffer, "\"boolean\"")))
      score = 4;
  }

  return score;
}

/* Error handling */
static void
rasqal_srj_handle_parse_error(rasqal_srj_context* context,
                              const unsigned char* json_text,
                              size_t json_length)
{
  unsigned char* error_str;

#ifdef HAVE_YAJL2
  error_str = yajl_get_error(context->handle, 1, json_text, json_length);
#else
  error_str = yajl_get_error(context->handle, 1, json_text, json_length);
#endif

  rasqal_log_error_simple(context->world, RAPTOR_LOG_LEVEL_ERROR,
                          NULL, /* FIXME: add locator */
                          "SRJ parse error: %s", error_str);

  context->error_count++;
  context->state = STATE_ERROR;
  yajl_free_error(context->handle, error_str);
}

/* Clean up SRJ context */
static void
rasqal_srj_context_finish(rasqal_srj_context* context)
{
  int i;

  if(!context)
    return;

  if(context->handle) {
    yajl_free(context->handle);
    context->handle = NULL;
  }

  if(context->current_key) {
    RASQAL_FREE(char*, context->current_key);
    context->current_key = NULL;
  }

  if(context->value_type) {
    RASQAL_FREE(char*, context->value_type);
    context->value_type = NULL;
  }

  if(context->value_value) {
    RASQAL_FREE(char*, context->value_value);
    context->value_value = NULL;
  }

  if(context->value_datatype) {
    RASQAL_FREE(char*, context->value_datatype);
    context->value_datatype = NULL;
  }

  if(context->value_lang) {
    RASQAL_FREE(char*, context->value_lang);
    context->value_lang = NULL;
  }

  if(context->error_message) {
    RASQAL_FREE(char*, context->error_message);
    context->error_message = NULL;
  }

  if(context->current_row) {
    rasqal_free_row(context->current_row);
    context->current_row = NULL;
  }

  /* Free remaining rows in queue */
  if(context->rows) {
    for(i = context->current_row_index; i < context->rows_count; i++) {
      if(context->rows[i])
        rasqal_free_row(context->rows[i]);
    }
    RASQAL_FREE(rasqal_row**, context->rows);
    context->rows = NULL;
  }

  RASQAL_FREE(rasqal_srj_context*, context);
}

/* Helper function to duplicate a string with length */
static char*
rasqal_srj_strndup(const unsigned char* str, RASQAL_YAJL_LEN_TYPE len)
{
  char* result = RASQAL_MALLOC(char*, len + 1);
  if(result) {
    memcpy(result, str, len);
    result[len] = '\0';
  }
  return result;
}

/* Create literal from SRJ type/value/datatype/lang */
static rasqal_literal*
rasqal_srj_create_literal(rasqal_world* world,
                          const char* type,
                          const char* value,
                          const char* datatype,
                          const char* lang)
{
  if(!type || !value)
    return NULL;

  if(!strcmp(type, "uri")) {
    raptor_uri* uri = raptor_new_uri(world->raptor_world_ptr,
                                     (const unsigned char*)value);
    return rasqal_new_uri_literal(world, uri);
  } else if(!strcmp(type, "literal")) {
    raptor_uri* dt_uri = NULL;
    unsigned char* copied_value;
    char* copied_lang = NULL;

    /* Copy the value string to avoid memory corruption */
    size_t value_len = strlen(value);
    copied_value = RASQAL_MALLOC(unsigned char*, value_len + 1);
    if(!copied_value)
      return NULL;
    memcpy(copied_value, value, value_len + 1);

    /* Copy the language string if present */
    if(lang) {
      size_t lang_len = strlen(lang);
      copied_lang = RASQAL_MALLOC(char*, lang_len + 1);
      if(copied_lang) {
        memcpy(copied_lang, lang, lang_len + 1);
      }
    }

    if(datatype) {
      dt_uri = raptor_new_uri(world->raptor_world_ptr,
                              (const unsigned char*)datatype);
    }
    return rasqal_new_string_literal(world, copied_value,
                                     copied_lang, dt_uri, NULL);
  } else if(!strcmp(type, "bnode")) {
    /* Copy the value string for bnode as well */
    size_t value_len = strlen(value);
    unsigned char* copied_value = RASQAL_MALLOC(unsigned char*, value_len + 1);
    if(!copied_value)
      return NULL;
    memcpy(copied_value, value, value_len + 1);
    return rasqal_new_simple_literal(world, RASQAL_LITERAL_BLANK,
                                     copied_value);
  }
  return NULL;
}

/* YAJL callback implementations */
static int
rasqal_srj_null_handler(void* ctx)
{
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  rasqal_srj_context* context = (rasqal_srj_context*)ctx;
  RASQAL_DEBUG2("SRJ null in state %d", context->state);
#else
  (void)ctx; /* avoid unused parameter warning */
#endif

  /* NULL values are generally not meaningful in SRJ bindings */
  return 1;
}

static int
rasqal_srj_boolean_handler(void* ctx, int value)
{
  rasqal_srj_context* context = (rasqal_srj_context*)ctx;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG3("SRJ boolean %d in state %d", value, context->state);
#endif

  if(context->state == STATE_BEFORE_ROOT &&
     context->current_key && !strcmp(context->current_key, "boolean")) {
    context->is_boolean_result = 1;
    context->boolean_value = value;
    context->finished = 1;
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG2("SRJ: Set boolean result to %d\n", value);
#endif
  }

  return 1;
}

static int
rasqal_srj_string_handler(void* ctx, const unsigned char* str, RASQAL_YAJL_LEN_TYPE len)
{
  rasqal_srj_context* context = (rasqal_srj_context*)ctx;
  char* string_value;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG4("SRJ string '%.*s' in state %d", (int)len, str, context->state);
#endif

  string_value = rasqal_srj_strndup(str, len);
  if(!string_value)
    return 0;

  switch(context->state) {
    case STATE_IN_VARS_ARRAY:
      /* Variable name in vars array */
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      RASQAL_DEBUG2("SRJ: Adding variable '%s' to vars table\n", string_value);
#endif
      if(!rasqal_variables_table_add2(context->vars_table,
                                      RASQAL_VARIABLE_TYPE_NORMAL,
                                      (const unsigned char*)string_value,
                                      strlen(string_value), NULL)) {
        RASQAL_FREE(char*, string_value);
        return 0;
      }
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      RASQAL_DEBUG2("SRJ: Successfully added variable '%s' to vars table\n", string_value);
#endif
      break;

    case STATE_IN_VALUE_OBJECT:
      /* Value in binding object - store based on current key */
      if(context->current_key) {
        if(!strcmp(context->current_key, "type")) {
          if(context->value_type)
            RASQAL_FREE(char*, context->value_type);
          context->value_type = string_value;
          string_value = NULL; /* transferred ownership */
        } else if(!strcmp(context->current_key, "value")) {
          if(context->value_value)
            RASQAL_FREE(char*, context->value_value);
          context->value_value = string_value;
          string_value = NULL; /* transferred ownership */
        } else if(!strcmp(context->current_key, "datatype")) {
          if(context->value_datatype)
            RASQAL_FREE(char*, context->value_datatype);
          context->value_datatype = string_value;
          string_value = NULL; /* transferred ownership */
        } else if(!strcmp(context->current_key, "xml:lang")) {
          if(context->value_lang)
            RASQAL_FREE(char*, context->value_lang);
          context->value_lang = string_value;
          string_value = NULL; /* transferred ownership */
        }
      }
      break;

    case STATE_BEFORE_ROOT:
    case STATE_IN_HEAD:
    case STATE_IN_RESULTS:
    case STATE_IN_BINDINGS_ARRAY:
    case STATE_IN_BINDING_OBJECT:
    case STATE_COMPLETE:
    case STATE_ERROR:
    default:
      /* String in other contexts - ignore */
      break;
  }

  if(string_value)
    RASQAL_FREE(char*, string_value);

  return 1;
}

static int
rasqal_srj_start_map_handler(void* ctx)
{
  rasqal_srj_context* context = (rasqal_srj_context*)ctx;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG2("SRJ start map in state %d", context->state);
#endif

  switch(context->state) {
    case STATE_BEFORE_ROOT:
      /* Root object */
      context->state = STATE_BEFORE_ROOT; /* Stay here until we see a key */
      break;

    case STATE_IN_BINDINGS_ARRAY:
      /* Start of binding object */
      context->state = STATE_IN_BINDING_OBJECT;
      break;

    case STATE_IN_BINDING_OBJECT:
      /* Start of value object for a variable */
      context->state = STATE_IN_VALUE_OBJECT;
      /* Clear value context */
      if(context->value_type) {
        RASQAL_FREE(char*, context->value_type);
        context->value_type = NULL;
      }
      if(context->value_value) {
        RASQAL_FREE(char*, context->value_value);
        context->value_value = NULL;
      }
      if(context->value_datatype) {
        RASQAL_FREE(char*, context->value_datatype);
        context->value_datatype = NULL;
      }
      if(context->value_lang) {
        RASQAL_FREE(char*, context->value_lang);
        context->value_lang = NULL;
      }
      break;

    case STATE_IN_HEAD:
    case STATE_IN_VARS_ARRAY:
    case STATE_IN_RESULTS:
    case STATE_IN_VALUE_OBJECT:
    case STATE_COMPLETE:
    case STATE_ERROR:
    default:
      /* Nested objects in other contexts */
      break;
  }

  return 1;
}

static int
rasqal_srj_map_key_handler(void* ctx, const unsigned char* key, RASQAL_YAJL_LEN_TYPE len)
{
  rasqal_srj_context* context = (rasqal_srj_context*)ctx;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG4("SRJ map key '%.*s' in state %d", (int)len, key, context->state);
#endif

  /* Store current key */
  if(context->current_key) {
    RASQAL_FREE(char*, context->current_key);
  }
  context->current_key = rasqal_srj_strndup(key, len);
  if(!context->current_key)
    return 0;

  /* Update state based on key and current state */
  if(context->state == STATE_BEFORE_ROOT) {
    if(!strcmp(context->current_key, "head")) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      RASQAL_DEBUG1("SRJ: State transition BEFORE_ROOT -> IN_HEAD\n");
#endif
      context->state = STATE_IN_HEAD;
    } else if(!strcmp(context->current_key, "results")) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      RASQAL_DEBUG1("SRJ: State transition BEFORE_ROOT -> IN_RESULTS\n");
#endif
      context->state = STATE_IN_RESULTS;
    } else if(!strcmp(context->current_key, "boolean")) {
      /* Boolean result - stay in BEFORE_ROOT to handle value */
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      RASQAL_DEBUG1("SRJ: Found boolean key, staying in BEFORE_ROOT\n");
#endif
    }
  } else if(context->state == STATE_IN_BINDING_OBJECT) {
    /* Variable name in binding object */
    context->current_variable = rasqal_variables_table_get_by_name(context->vars_table,
                                                                   RASQAL_VARIABLE_TYPE_NORMAL,
                                                                   (const unsigned char*)context->current_key);
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    if(context->current_variable) {
      RASQAL_DEBUG3("SRJ: Found variable '%s' at offset %d", context->current_key, context->current_variable->offset);
    } else {
      RASQAL_DEBUG2("SRJ: Variable '%s' not found in vars table\n", context->current_key);
    }
#endif
  }

  return 1;
}

static int
rasqal_srj_end_map_handler(void* ctx)
{
  rasqal_srj_context* context = (rasqal_srj_context*)ctx;
  rasqal_literal* literal;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG2("SRJ end map in state %d", context->state);
#endif

  switch(context->state) {
    case STATE_IN_HEAD:
      /* End of head object */
      context->state = STATE_BEFORE_ROOT;
      break;

    case STATE_IN_RESULTS:
      /* End of results object */
      context->state = STATE_BEFORE_ROOT;
      context->finished = 1;
      break;

    case STATE_IN_BINDING_OBJECT:
      /* End of binding object - add row to queue */
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      RASQAL_DEBUG3("SRJ: End binding object, current_row=%p, rows_count=%d", 
                    context->current_row, context->rows_count);
#endif
      if(context->current_row) {
        /* Ensure row queue has space */
        if(context->rows_count >= context->rows_size) {
          rasqal_row** new_rows;
          context->rows_size *= 2;
          new_rows = RASQAL_REALLOC(rasqal_row**, context->rows,
                                    context->rows_size * sizeof(rasqal_row*));
          if(!new_rows)
            return 0;
          context->rows = new_rows;
        }

        context->rows[context->rows_count++] = context->current_row;
        context->current_row = NULL;
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
        RASQAL_DEBUG2("SRJ: Added row to queue, new rows_count=%d", context->rows_count);
#endif
      }
      context->state = STATE_IN_BINDINGS_ARRAY;
      break;

    case STATE_IN_VALUE_OBJECT:
      /* End of value object - create literal and set in row */
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      if(context->current_variable) {
        RASQAL_DEBUG2("SRJ: End value object - var=%s", (const char*)context->current_variable->name);
      } else {
        RASQAL_DEBUG1("SRJ: End value object - var=NULL");
      }
      if(context->value_type) {
        RASQAL_DEBUG2("SRJ: Value type: %s", context->value_type);
      }
      if(context->value_value) {
        RASQAL_DEBUG2("SRJ: Value value: %s", context->value_value);
      }
#endif
      if(context->current_variable && context->value_type && context->value_value) {
        /* Create row if we don't have one and rowsource size is correct */
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
        RASQAL_DEBUG4("SRJ: Creating row: current_row=%p, rowsource=%p, size=%d", 
                      context->current_row, context->rowsource, 
                      context->rowsource ? context->rowsource->size : -1);
#endif
        if(!context->current_row && context->rowsource && context->rowsource->size > 0) {
          context->current_row = rasqal_new_row(context->rowsource);
          if(!context->current_row)
            return 0;
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
          RASQAL_DEBUG2("SRJ: Created new row, size %d", context->current_row->size);
#endif
        }

        if(context->current_row) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
          RASQAL_DEBUG4("SRJ: Creating literal: type='%s', value='%s', datatype='%s'", 
                        context->value_type ? context->value_type : "NULL",
                        context->value_value ? context->value_value : "NULL",
                        context->value_datatype ? context->value_datatype : "NULL");
#endif
          literal = rasqal_srj_create_literal(context->world,
                                              context->value_type,
                                              context->value_value,
                                              context->value_datatype,
                                              context->value_lang);
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
          if(literal) {
            RASQAL_DEBUG2("SRJ: Created literal, setting at offset %d", context->current_variable->offset);
          } else {
            RASQAL_DEBUG1("SRJ: Failed to create literal");
          }
#endif
          if(literal) {
            rasqal_row_set_value_at(context->current_row,
                                    context->current_variable->offset,
                                    literal);
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
            RASQAL_DEBUG3("SRJ: Set literal at offset %d, row size %d", 
                          context->current_variable->offset, context->current_row->size);
#endif
          }
        }
      } else {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
        RASQAL_DEBUG1("SRJ: Missing required fields for literal creation");
#endif
      }
      context->state = STATE_IN_BINDING_OBJECT;
      context->current_variable = NULL;
      break;

    case STATE_BEFORE_ROOT:
    case STATE_IN_VARS_ARRAY:
    case STATE_IN_BINDINGS_ARRAY:
    case STATE_COMPLETE:
    case STATE_ERROR:
    default:
      /* End of root or other objects */
      if(context->state != STATE_COMPLETE)
        context->finished = 1;
      break;
  }

  return 1;
}

static int
rasqal_srj_start_array_handler(void* ctx)
{
  rasqal_srj_context* context = (rasqal_srj_context*)ctx;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG2("SRJ start array in state %d", context->state);
#endif

  if(context->state == STATE_IN_HEAD &&
     context->current_key && !strcmp(context->current_key, "vars")) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("SRJ: State transition IN_HEAD -> IN_VARS_ARRAY\n");
#endif
    context->state = STATE_IN_VARS_ARRAY;
  } else if(context->state == STATE_IN_RESULTS &&
            context->current_key && !strcmp(context->current_key, "bindings")) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("SRJ: State transition IN_RESULTS -> IN_BINDINGS_ARRAY\n");
#endif
    context->state = STATE_IN_BINDINGS_ARRAY;
  }

  return 1;
}

static int
rasqal_srj_end_array_handler(void* ctx)
{
  rasqal_srj_context* context = (rasqal_srj_context*)ctx;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG2("SRJ end array in state %d", context->state);
#endif

  switch(context->state) {
    case STATE_IN_VARS_ARRAY:
      context->state = STATE_IN_HEAD;
      /* Update rowsource size now that we know the variables */
      if(context->rowsource && context->vars_table) {
        int vars_count = rasqal_variables_table_get_total_variables_count(context->vars_table);
        if(vars_count > 0) {
          context->rowsource->size = vars_count;
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
          RASQAL_DEBUG2("SRJ: Updated rowsource size to %d (from vars array)", vars_count);
#endif
        }
      }
      break;

    case STATE_IN_BINDINGS_ARRAY:
      context->state = STATE_IN_RESULTS;
      break;

    case STATE_BEFORE_ROOT:
    case STATE_IN_HEAD:
    case STATE_IN_RESULTS:
    case STATE_IN_BINDING_OBJECT:
    case STATE_IN_VALUE_OBJECT:
    case STATE_COMPLETE:
    case STATE_ERROR:
    default:
      /* Other array ends */
      break;
  }

  return 1;
}

/* Parse JSON chunk and update context */
static int
rasqal_srj_parse_chunk(rasqal_srj_context* context, raptor_iostream* iostr)
{
  unsigned char buffer[1024]; /* Reduced buffer size to avoid stack warnings */
  size_t bytes_read;
  yajl_status status;

  if(context->finished || context->state == STATE_ERROR)
    return 1;

  bytes_read = raptor_iostream_read_bytes(buffer, 1, sizeof(buffer), iostr);

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG2("SRJ: read %zu bytes from stream\n", bytes_read);
  if(bytes_read > 0) {
    RASQAL_DEBUG3("SRJ: data: %.*s", (int)bytes_read, buffer);
  }
#endif

  if(bytes_read == 0) {
    /* EOF reached - finalize parsing */
#ifdef HAVE_YAJL2
    status = yajl_complete_parse(context->handle);
#else
    status = yajl_parse_complete(context->handle);
#endif
    if(status != yajl_status_ok) {
      rasqal_srj_handle_parse_error(context, buffer, bytes_read);
      return 0;
    }
    context->finished = 1;
    return 1;
  }

  /* Parse the chunk */
  status = yajl_parse(context->handle, buffer, bytes_read);
  if(status != yajl_status_ok
#ifndef HAVE_YAJL2
     && status != yajl_status_insufficient_data
#endif
  ) {
    rasqal_srj_handle_parse_error(context, buffer, bytes_read);
    return 0;
  }

  return 1;
}

/* Rowsource implementation */
static int
rasqal_srj_rowsource_init(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_srj_context* context = (rasqal_srj_context*)user_data;
  context->rowsource = rowsource;
  return 0;
}

static int
rasqal_srj_rowsource_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_srj_context* context = (rasqal_srj_context*)user_data;
  rasqal_srj_context_finish(context);
  return 0;
}

static rasqal_row*
rasqal_srj_rowsource_read_row(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_srj_context* context = (rasqal_srj_context*)user_data;

  if(!context)
    return NULL;

  /* Ensure variables are set up before doing anything */
  if(rasqal_srj_rowsource_ensure_variables(rowsource, context) != 0)
    return NULL;

  /* Handle boolean results */
  if(context->is_boolean_result) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("SRJ: Boolean result, returning no rows");
#endif
    return NULL; /* Boolean results have no rows */
  }

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG4("SRJ: read_row: current_row_index=%d, rows_count=%d, finished=%d", 
                context->current_row_index, context->rows_count, context->finished);
#endif

  /* Return next row if available */
  if(context->current_row_index < context->rows_count) {
    rasqal_row* row = context->rows[context->current_row_index];
    context->rows[context->current_row_index] = NULL; /* Transfer ownership */
    context->current_row_index++;
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG3("SRJ: Returning row %d of %d\n", context->current_row_index, context->rows_count);
#endif
    return row;
  }

  return NULL; /* EOF */
}

/* Ensure variables are extracted from SRJ header */
static int
rasqal_srj_rowsource_ensure_variables(rasqal_rowsource* rowsource, void* user_data)
{
  rasqal_srj_context* context = (rasqal_srj_context*)user_data;
  raptor_iostream* iostr;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG1("SRJ: ensure_variables called\n");
#endif

  if(!context)
    return 1;

  /* If we already finished or have an error, return */
  if(context->finished || context->state == STATE_ERROR)
    return (context->state == STATE_ERROR) ? 1 : 0;

  /* Parse JSON until we have processed the head section and have at least one row */
  iostr = context->iostr;
  if(iostr) {
    while(!context->finished && context->state != STATE_ERROR) {
      if(!rasqal_srj_parse_chunk(context, iostr))
        return 1; /* Parse error */

      /* If we've found a boolean result, we can stop */
      if(context->is_boolean_result) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
        RASQAL_DEBUG2("SRJ: Found boolean result %d\n", context->boolean_value);
#endif
        break;
      }

      /* If we've finished parsing completely, we can stop */
      if(context->finished) {
        break;
      }
    }
  }

  /* Update rowsource size based on variables table */
  if(rowsource && context->vars_table) {
    int vars_count = rasqal_variables_table_get_total_variables_count(context->vars_table);
    if(vars_count > 0) {
      rowsource->size = vars_count;
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
      RASQAL_DEBUG2("SRJ: Updated rowsource size to %d", rowsource->size);
#endif
    }
  }

  return (context->state == STATE_ERROR) ? 1 : 0;
}

static const rasqal_rowsource_handler rasqal_srj_rowsource_handler = {
  /* .version = */ 1,
  "srj",
  /* .init = */ rasqal_srj_rowsource_init,
  /* .finish = */ rasqal_srj_rowsource_finish,
  /* .ensure_variables = */ rasqal_srj_rowsource_ensure_variables,
  /* .read_row = */ rasqal_srj_rowsource_read_row,
  /* .read_all_rows = */ NULL,
  /* .reset = */ NULL,
  /* .set_requirements = */ NULL,
  /* .get_inner_rowsource = */ NULL,
  /* .set_origin = */ NULL
};

/* Formatter boolean result support */
static int
rasqal_srj_get_boolean(rasqal_query_results_formatter *formatter,
                       rasqal_world* world, raptor_iostream *iostr,
                       raptor_uri *base_uri, unsigned int flags)
{
  rasqal_srj_context* context;
  int bv;

  context = RASQAL_CALLOC(rasqal_srj_context*, sizeof(*context), 1);
  if(!context)
    return -1;

  context->world = world;
  context->state = STATE_BEFORE_ROOT;
  context->finished = 0;
  context->boolean_value = -1; /* unknown */

  /* Initialize YAJL parser */
#ifdef HAVE_YAJL2
  context->handle = yajl_alloc(&srj_callbacks, &rasqal_srj_yajl_alloc_funcs, context);
  if(context->handle) {
    yajl_config(context->handle, yajl_allow_comments, 0);
    yajl_config(context->handle, yajl_dont_validate_strings, 0);
  }
#else
  yajl_parser_config config = { 0, 0 }; /* no comments, validate strings */
  context->handle = yajl_alloc(&srj_callbacks, &config, &rasqal_srj_yajl_alloc_funcs, context);
#endif

  if(!context->handle) {
    rasqal_srj_context_finish(context);
    return -1;
  }

  context->iostr = iostr;

  /* Parse JSON until we get the boolean value or hit an error */
  while(!context->finished && context->state != STATE_ERROR) {
    if(!rasqal_srj_parse_chunk(context, iostr))
      break;

    /* If we've found a boolean result, we can stop */
    if(context->is_boolean_result)
      break;
  }

  bv = context->boolean_value;

  rasqal_srj_context_finish(context);

  return bv;
}

/* Main rowsource constructor */
static rasqal_rowsource*
rasqal_query_results_get_rowsource_srj(rasqal_query_results_formatter* formatter,
                                       rasqal_world* world,
                                       rasqal_variables_table* vars_table,
                                       raptor_iostream* iostr,
                                       raptor_uri* base_uri,
                                       unsigned int flags)
{
  rasqal_srj_context* context;
  rasqal_rowsource* rowsource = NULL;

  context = RASQAL_CALLOC(rasqal_srj_context*, sizeof(*context), 1);
  if(!context)
    return NULL;

  context->world = world;
  context->vars_table = vars_table;
  context->state = STATE_BEFORE_ROOT;
  context->finished = 0;
  context->rows_size = 16; /* Initial row queue size */

  context->rows = RASQAL_CALLOC(rasqal_row**, sizeof(rasqal_row*), context->rows_size);
  if(!context->rows) {
    rasqal_srj_context_finish(context);
    return NULL;
  }

  /* Initialize YAJL parser */
#ifdef HAVE_YAJL2
  context->handle = yajl_alloc(&srj_callbacks, &rasqal_srj_yajl_alloc_funcs, context);
  if(context->handle) {
    yajl_config(context->handle, yajl_allow_comments, 0);
    yajl_config(context->handle, yajl_dont_validate_strings, 0);
  }
#else
  yajl_parser_config config = { 0, 0 }; /* no comments, validate strings */
  context->handle = yajl_alloc(&srj_callbacks, &config, &rasqal_srj_yajl_alloc_funcs, context);
#endif

  if(!context->handle) {
    rasqal_srj_context_finish(context);
    return NULL;
  }

  rowsource = rasqal_new_rowsource_from_handler(world, NULL, context,
                                                &rasqal_srj_rowsource_handler,
                                                vars_table, 0);
  if(!rowsource) {
    rasqal_srj_context_finish(context);
    return NULL;
  }

  context->rowsource = rowsource;

  /* Store iostream reference in context for parsing */
  context->iostr = iostr;

  return rowsource;
}

/* SRJ Writing Implementation */

/* Helper function to write URI terms */
static void
rasqal_srj_write_uri(raptor_iostream* iostr, raptor_uri* uri)
{
  const unsigned char* str;
  size_t str_len;

  raptor_iostream_counted_string_write("\"type\": \"uri\", \"value\": \"", 25,
                                       iostr);
  str = raptor_uri_as_counted_string(uri, &str_len);
  str_len = strlen(RASQAL_GOOD_CAST(const char*, str));
  raptor_string_escaped_write(str, str_len, '\"',
                              RAPTOR_ESCAPED_WRITE_JSON_LITERAL, iostr);
  raptor_iostream_write_byte('\"', iostr);
}

/* Helper function to write literal terms */
static void
rasqal_srj_write_literal(raptor_iostream* iostr, rasqal_literal* literal)
{
  raptor_iostream_counted_string_write("\"type\": \"literal\", \"value\": \"",
                                       29, iostr);
  raptor_string_escaped_write(literal->string, literal->string_len, '\"',
                              RAPTOR_ESCAPED_WRITE_JSON_LITERAL, iostr);
  raptor_iostream_write_byte('\"', iostr);

  if(literal->language) {
    raptor_iostream_counted_string_write(", \"xml:lang\": \"", 15, iostr);
    raptor_iostream_string_write(RASQAL_GOOD_CAST(const unsigned char*, literal->language), iostr);
    raptor_iostream_write_byte('\"', iostr);
  }

  if(literal->datatype) {
    const unsigned char* dt_str;
    raptor_iostream_counted_string_write(", \"datatype\": \"", 15, iostr);
    dt_str = raptor_uri_as_string(literal->datatype);
    raptor_iostream_string_write(dt_str, iostr);
    raptor_iostream_write_byte('\"', iostr);
  }
}

/* Helper function to write blank node terms */
static void
rasqal_srj_write_bnode(raptor_iostream* iostr, const unsigned char* bnode_id)
{
  raptor_iostream_counted_string_write("\"type\": \"bnode\", \"value\": \"", 27, iostr);
  raptor_string_ntriples_write(bnode_id, strlen(RASQAL_GOOD_CAST(const char*, bnode_id)), '\"', iostr);
  raptor_iostream_write_byte('\"', iostr);
}

/* Write head section */
static void
rasqal_srj_write_head(raptor_iostream* iostr, rasqal_query_results* results)
{
  int i;

  raptor_iostream_counted_string_write("  \"head\": {\n", 12, iostr);

  if(rasqal_query_results_is_bindings(results)) {
    raptor_iostream_counted_string_write("    \"vars\": [", 13, iostr);

    for(i = 0; 1; i++) {
      const unsigned char *name = rasqal_query_results_get_binding_name(results, i);
      if(!name)
        break;

      if(i > 0)
        raptor_iostream_counted_string_write(", ", 2, iostr);

      raptor_iostream_write_byte('\"', iostr);
      raptor_iostream_string_write(name, iostr);
      raptor_iostream_write_byte('\"', iostr);
    }

    raptor_iostream_counted_string_write("]\n", 2, iostr);
  }

  raptor_iostream_counted_string_write("  },\n", 5, iostr);
}

/* Write boolean results */
static void
rasqal_srj_write_boolean(raptor_iostream* iostr, rasqal_query_results* results)
{
  raptor_iostream_counted_string_write("  \"boolean\": ", 12, iostr);

  if(rasqal_query_results_get_boolean(results))
    raptor_iostream_counted_string_write("true", 4, iostr);
  else
    raptor_iostream_counted_string_write("false", 5, iostr);
}

/* Write results section */
static void
rasqal_srj_write_results(raptor_iostream* iostr, rasqal_query_results* results, rasqal_query* query)
{
  int row_comma = 0;

  raptor_iostream_counted_string_write("  \"results\": {\n", 15, iostr);

  /* Write optional metadata */
  if(query) {
    raptor_iostream_counted_string_write("    \"ordered\": ", 15, iostr);
    if(rasqal_query_get_order_condition(query, 0) != NULL)
      raptor_iostream_counted_string_write("true", 4, iostr);
    else
      raptor_iostream_counted_string_write("false", 5, iostr);
    raptor_iostream_counted_string_write(",\n", 2, iostr);

    raptor_iostream_counted_string_write("    \"distinct\": ", 16, iostr);
    if(rasqal_query_get_distinct(query))
      raptor_iostream_counted_string_write("true", 4, iostr);
    else
      raptor_iostream_counted_string_write("false", 5, iostr);
    raptor_iostream_counted_string_write(",\n", 2, iostr);
  }

  /* Write bindings array */
  raptor_iostream_counted_string_write("    \"bindings\": [\n", 18, iostr);

  while(!rasqal_query_results_finished(results)) {
    int column_comma = 0;
    int i;

    if(row_comma)
      raptor_iostream_counted_string_write(",\n", 2, iostr);

    raptor_iostream_counted_string_write("      {\n", 8, iostr);

    for(i = 0; i < rasqal_query_results_get_bindings_count(results); i++) {
      const unsigned char *name = rasqal_query_results_get_binding_name(results, i);
      rasqal_literal *l = rasqal_query_results_get_binding_value(results, i);

      if(l) {
        if(column_comma)
          raptor_iostream_counted_string_write(",\n", 2, iostr);

        raptor_iostream_counted_string_write("        \"", 9, iostr);
        raptor_iostream_string_write(name, iostr);
        raptor_iostream_counted_string_write("\": { ", 5, iostr);

        switch(l->type) {
          case RASQAL_LITERAL_URI:
            rasqal_srj_write_uri(iostr, l->value.uri);
            break;
          case RASQAL_LITERAL_BLANK:
            rasqal_srj_write_bnode(iostr, l->string);
            break;
          case RASQAL_LITERAL_STRING:
          default:
            rasqal_srj_write_literal(iostr, l);
            break;
        }

        raptor_iostream_counted_string_write(" }", 2, iostr);
        column_comma = 1;
      }
    }

    raptor_iostream_counted_string_write("\n      }", 8, iostr);
    row_comma = 1;

    rasqal_query_results_next(results);
  }

  raptor_iostream_counted_string_write("\n    ]\n", 6, iostr);
  raptor_iostream_counted_string_write("  }", 3, iostr);
}

/* Main writer function */
static int
rasqal_query_results_write_srj(rasqal_query_results_formatter* formatter,
                               raptor_iostream *iostr,
                               rasqal_query_results* results,
                               raptor_uri *base_uri)
{
  rasqal_world* world = rasqal_query_results_get_world(results);
  rasqal_query* query = rasqal_query_results_get_query(results);
  rasqal_query_results_type type;

  type = rasqal_query_results_get_type(results);

  if(type != RASQAL_QUERY_RESULTS_BINDINGS &&
     type != RASQAL_QUERY_RESULTS_BOOLEAN) {
    rasqal_log_error_simple(world, RAPTOR_LOG_LEVEL_ERROR, NULL,
                            "Cannot write SRJ for %s query result format",
                            rasqal_query_results_type_label(type));
    return 1;
  }

  /* Write opening brace */
  raptor_iostream_counted_string_write("{\n", 2, iostr);

  /* Write head section */
  rasqal_srj_write_head(iostr, results);

  /* Handle boolean results */
  if(rasqal_query_results_is_boolean(results)) {
    rasqal_srj_write_boolean(iostr, results);
    goto done;
  }

  /* Write results section */
  rasqal_srj_write_results(iostr, results, query);

done:
  /* Write closing brace */
  raptor_iostream_counted_string_write("\n}\n", 3, iostr);

  return 0;
}

/* End of SRJ format implementation */

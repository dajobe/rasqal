/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal-mcp-server.c - Rasqal Model Context Protocol (MCP) Server
 *
 * Copyright (C) 2025, David Beckett https://www.dajobe.org/
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
 * This is a reference implementation of an MCP server that demonstrates
 * how to expose SPARQL query capabilities to AI agents.
 */

#ifdef HAVE_CONFIG_H
#include <rasqal_config.h>
#endif

#ifdef WIN32
#include <win32_rasqal_config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>

/* Rasqal includes */
#include <rasqal.h>
#include <raptor.h>
#include "rasqalcmdline.h"

/* YAJL includes for parsing only */
#include <yajl/yajl_parse.h>
#include <yajl/yajl_tree.h>

/* MCP Protocol constants */
#define MCP_VERSION "2024-11-05"
#define MCP_PROTOCOL_VERSION "1.0"

/* JSON-RPC constants */
#define JSONRPC_VERSION "2.0"

/* Buffer sizes */
#define JSON_BUFFER_SIZE 8192
#define MAX_LINE_SIZE 65536

/* MCP Server constants */
static const char* const MCP_PROTOCOL_VERSION_STRING = "2024-11-05";
static const char* const MCP_SERVER_NAME = "rasqal-mcp-server";
static const char* const MCP_SERVER_INSTRUCTIONS = "SPARQL query server for RDF data. Use execute_sparql_query to run queries, validate_sparql_query to check syntax, and list_formats to see supported formats.";


/* Global state */
static rasqal_world* world = NULL;
static raptor_world* raptor_world_ptr = NULL;

/* Command line options */
static int help = 0;
static int version = 0;
static int quiet = 0;
static int debug = 0;
static char* log_file = NULL;
static FILE* log_fp = NULL;

/* Program name for error messages */
static char* program = NULL;


/* Printf format attribute for log_message() */
#if __GNUC__ > 2 || (__GNUC__ == 2 && __GNUC_MINOR__ > 4)
#define PRINTF_FORMAT(string_index, first_to_check_index) \
  __attribute__((__format__(__printf__, string_index, first_to_check_index)))
#else
#define PRINTF_FORMAT(string_index, first_to_check_index)
#endif

static void
log_message(raptor_log_level level, const char* format, ...) PRINTF_FORMAT(2, 3);


static void
log_message(raptor_log_level level, const char* format, ...)
{
  va_list args;
  char* buffer = NULL;
  size_t length;
  const char* level_str;
  time_t now;
  struct tm* tm_info;
  /* 20 = strlen("YYYY-MM-DD HH:MM:SS\n") + 1 for null terminator */
  char time_str[20];

  va_start(args, format);

  /* Debug messages are only shown when debug is enabled */
  if(level == RAPTOR_LOG_LEVEL_DEBUG && !debug) {
    va_end(args);
    return;
  }

  /* Get level string directly from raptor */
  level_str = raptor_log_level_get_label(level);

  /* Format the timestamp for this log message in ISO 8601 format */
  now = time(NULL);
  tm_info = localtime(&now);
  strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);

  length = raptor_vasprintf(&buffer, format, args);
  if(!buffer) {
    /* Fallback to direct vfprintf if allocation fails */
    if(level == RAPTOR_LOG_LEVEL_ERROR || !quiet) {
      fprintf(stderr, "%s %s: %s - ", time_str, program, level_str);
      vfprintf(stderr, format, args);
      fprintf(stderr, "\n");
    }

    /* Write to log file if specified (always, regardless of quiet mode) */
    if(log_fp) {
      fprintf(log_fp, "%s %s: %s - ", time_str, program, level_str);
      vfprintf(log_fp, format, args);
      fprintf(log_fp, "\n");
      fflush(log_fp);
    }

    va_end(args);
    return;
  }

  /* Remove trailing newline if present */
  if(length >= 1 && buffer[length-1] == '\n')
    buffer[length-1] = '\0';

  /* Always write to stderr for errors, and to stderr for other levels unless quiet */
  if(level == RAPTOR_LOG_LEVEL_ERROR || !quiet)
    fprintf(stderr, "%s %s: %s - %s\n", time_str, program, level_str, buffer);

  /* Write to log file if specified (always, regardless of quiet mode) */
  if(log_fp) {
    fprintf(log_fp, "%s %s: %s - %s\n", time_str, program, level_str, buffer);
    fflush(log_fp);
  }

  raptor_free_memory(buffer);

  va_end(args);
}


/* Tool definitions */
typedef struct {
  const char* name;
  const char* description;
  const char* inputSchema;
} mcp_tool_t;

static const mcp_tool_t mcp_tools[] = {
  {
    "execute_sparql_query",
    "Execute a SPARQL query against loaded data graphs",
    "{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\"},\"data_graphs\":{\"type\":\"array\"},\"result_format\":{\"type\":\"string\"},\"query_language\":{\"type\":\"string\"}}}"
  },
  {
    "validate_sparql_query",
    "Parse and validate SPARQL query syntax without execution",
    "{\"type\":\"object\",\"properties\":{\"query\":{\"type\":\"string\"},\"query_language\":{\"type\":\"string\"}}}"
  },

  {
    "list_formats",
    "List supported RDF input formats and result output formats",
    "{\"type\":\"object\",\"properties\":{}}"
  }
};

/* Data graph structure */
typedef struct data_graph_t {
  char* uri;
  char* format;
  char* type;
  char* name;
} data_graph_t;

/* JSON parsing context for MCP requests */
typedef struct {
  char* method;
  char* id;
  char* current_key;
  char* query;
  char* result_format;
  char* query_language;
  data_graph_t** data_graphs;
  int data_graphs_count;
  int in_data_graphs;
  int in_arguments;
  int parse_error;
  char* error_message;
} mcp_json_context;


/* Create JSON-RPC error response */
static int
create_error_response(int code, const char* message, const char* data,
                      const char* id, char* buffer, size_t* len)
{
  unsigned char* output_string = NULL;
  raptor_iostream* iostr;

  iostr = raptor_new_iostream_to_string(raptor_world_ptr, (void**)&output_string, NULL, 0);
  if(!iostr)
    return 1;

  raptor_iostream_string_write((const unsigned char*)"{\"jsonrpc\":\"", iostr);
  raptor_iostream_string_write((const unsigned char*)JSONRPC_VERSION, iostr);
  raptor_iostream_string_write((const unsigned char*)"\",\"id\":", iostr);

  if(id) {
    raptor_iostream_write_byte('\"', iostr);
    raptor_iostream_string_write((const unsigned char*)id, iostr);
    raptor_iostream_write_byte('\"', iostr);
  } else {
    raptor_iostream_counted_string_write("null", 4, iostr);
  }

  raptor_iostream_string_write((const unsigned char*)"\",\"error\":{\"code\":", iostr);
  raptor_iostream_decimal_write(code, iostr);
  raptor_iostream_string_write((const unsigned char*)",\"message\":\"", iostr);
  raptor_string_escaped_write((const unsigned char*)message, strlen(message), '\"', RAPTOR_ESCAPED_WRITE_JSON_LITERAL, iostr);

  if(data) {
    raptor_iostream_string_write((const unsigned char*)"\",\"data\":\"", iostr);
    raptor_string_escaped_write((const unsigned char*)data, strlen(data), '\"', RAPTOR_ESCAPED_WRITE_JSON_LITERAL, iostr);
    raptor_iostream_write_byte('\"', iostr);
  } else {
    raptor_iostream_write_byte('\"', iostr);
  }

  raptor_iostream_string_write((const unsigned char*)"}}", iostr);
  raptor_iostream_write_byte('\n', iostr);

  raptor_free_iostream(iostr);

  if(output_string) {
    strncpy(buffer, (const char*)output_string, *len);
    *len = strlen((const char*)output_string);

    /* Debug log the error response being created */
    log_message(RAPTOR_LOG_LEVEL_DEBUG, "create_error_response - code: %d, message: '%s', data: '%s'",
                code, message, data ? data : "null");
    log_message(RAPTOR_LOG_LEVEL_DEBUG, "Error response buffer content: '%s'", (const char*)output_string);
    log_message(RAPTOR_LOG_LEVEL_DEBUG, "Error response buffer length: %zu", *len);

    raptor_free_memory(output_string);
  }

  return 0;
}


/* Create MCP tool response with proper content format */
static int
create_mcp_tool_response(const char* id, const char* tool_result, char* buffer, size_t* len)
{
  unsigned char* output_string = NULL;
  raptor_iostream* iostr;

  iostr = raptor_new_iostream_to_string(raptor_world_ptr, (void**)&output_string, NULL, 0);
  if(!iostr)
    return 1;

  raptor_iostream_string_write((const unsigned char*)"{\"jsonrpc\":\"", iostr);
  raptor_iostream_string_write((const unsigned char*)JSONRPC_VERSION, iostr);
  raptor_iostream_string_write((const unsigned char*)"\",\"id\":", iostr);

  if(id) {
    raptor_iostream_write_byte('\"', iostr);
    raptor_iostream_string_write((const unsigned char*)id, iostr);
    raptor_iostream_write_byte('\"', iostr);
  } else {
    raptor_iostream_counted_string_write("null", 4, iostr);
  }

  raptor_iostream_string_write((const unsigned char*)",\"result\":{\"content\":[{\"type\":\"text\",\"text\":\"", iostr);
  /* Escape the JSON string for the text field */
  raptor_string_escaped_write((const unsigned char*)tool_result, strlen(tool_result), '\"',
                              RAPTOR_ESCAPED_WRITE_JSON_LITERAL, iostr);
  raptor_iostream_string_write((const unsigned char*)"\"}],\"structuredContent\":", iostr);
  raptor_iostream_string_write((const unsigned char*)tool_result, iostr);
  raptor_iostream_string_write((const unsigned char*)",\"isError\":false}", iostr);

  raptor_iostream_write_byte('}', iostr);
  raptor_iostream_write_byte('\n', iostr);

  raptor_free_iostream(iostr);

  if(output_string) {
    size_t output_len = strlen((const char*)output_string);

    /* Copy the response to the buffer */
    if(output_len < *len) {
      strcpy(buffer, (const char*)output_string);
      *len = output_len;
    } else {
      /* Buffer too small, truncate */
      strncpy(buffer, (const char*)output_string, *len - 1);
      buffer[*len - 1] = '\0';
      *len = strlen(buffer);
    }

    /* Debug log the response being created */
    log_message(RAPTOR_LOG_LEVEL_DEBUG, "create_mcp_tool_response - id: '%s', tool_result: '%s'",
                id ? id : "null", tool_result ? tool_result : "null");
    log_message(RAPTOR_LOG_LEVEL_DEBUG, "MCP Response buffer content: '%s'", buffer);
    log_message(RAPTOR_LOG_LEVEL_DEBUG, "MCP Response buffer length: %zu", *len);

    raptor_free_memory(output_string);
  }

  return 0;
}


/* Initialize log file if specified */
static int
init_log_file(void)
{
  time_t now;
  struct tm* tm_info;
  char time_str[26];

  if(!log_file)
    return 0;

  log_fp = fopen(log_file, "w");
  if(!log_fp) {
    fprintf(stderr, "%s: Failed to open log file '%s': %s\n",
            program, log_file, strerror(errno));
    return 1;
  }

  /* Write log header */
  now = time(NULL);
  tm_info = localtime(&now);
  strftime(time_str, 26, "%Y-%m-%d %H:%M:%S", tm_info);
  fprintf(log_fp, "%s %s: Log file opened\n", time_str, program);
  fflush(log_fp);

  return 0;
}

/* MCP Server initialization */
static int
mcp_server_init(void)
{
  log_message(RAPTOR_LOG_LEVEL_INFO, "Initializing MCP server");

  world = rasqal_new_world();
  if(!world)
    return 1;

  if(rasqal_world_open(world))
    return 1;

  raptor_world_ptr = rasqal_world_get_raptor(world);
  if(!raptor_world_ptr)
    return 1;

  log_message(RAPTOR_LOG_LEVEL_DEBUG, "Rasqal world initialized successfully");

  return 0;
}


/* Handle initialize method */
static int
handle_initialize(const char* id, char* response_buffer, size_t* response_len)
{
  unsigned char* output_string = NULL;
  raptor_iostream* iostr;
  char response_str[1024];

  iostr = raptor_new_iostream_to_string(raptor_world_ptr,
                                        (void**)&output_string, NULL, 0);
  if(!iostr)
    return create_error_response(-32603,
                                 "Failed to create output stream", NULL, id,
                                 response_buffer, response_len);

  /* Build JSON-RPC response with proper MCP structure */
  log_message(RAPTOR_LOG_LEVEL_DEBUG, "Building initialize response with id: '%s'", id ? id : "null");

  /* Build the complete response - only the id field varies */
  raptor_snprintf(response_str, sizeof(response_str),
    "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":{\"protocolVersion\":\"%s\",\"capabilities\":{\"tools\":{\"listChanged\":true}},\"serverInfo\":{\"name\":\"%s\",\"version\":\"%s\"},\"instructions\":\"%s\"}}\n",
    id ? id : "null", MCP_PROTOCOL_VERSION_STRING, MCP_SERVER_NAME, rasqal_version_string, MCP_SERVER_INSTRUCTIONS);

  raptor_iostream_string_write((const unsigned char*)response_str, iostr);

  /* Free iostream to get the string */
  raptor_free_iostream(iostr);

  /* Copy to response buffer */
  if(output_string) {
    strncpy(response_buffer, (const char*)output_string, *response_len);
    *response_len = strlen((const char*)output_string);
    raptor_free_memory(output_string);
  }

  return 0;
}

/* Handle list_tools method */
static int
handle_list_tools(const char* id, char* response_buffer, size_t* response_len)
{
  unsigned char* output_string = NULL;
  raptor_iostream* iostr;

  iostr = raptor_new_iostream_to_string(raptor_world_ptr,
                                        (void**)&output_string, NULL, 0);
  if(!iostr)
    return create_error_response(-32603,
                                 "Failed to create output stream", NULL, id,
                                 response_buffer, response_len);

  /* Build JSON-RPC response */
  raptor_iostream_string_write((const unsigned char*)"{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{\"tools\":[", iostr);

  for(size_t i = 0; i < sizeof(mcp_tools) / sizeof(mcp_tools[0]); i++) {
    if(i > 0)
      raptor_iostream_string_write((const unsigned char*)",", iostr);

    raptor_iostream_string_write((const unsigned char*)"{\"name\":\"", iostr);
    raptor_iostream_string_write((const unsigned char*)mcp_tools[i].name, iostr);
    raptor_iostream_string_write((const unsigned char*)"\",\"description\":\"", iostr);
    raptor_iostream_string_write((const unsigned char*)mcp_tools[i].description, iostr);
    raptor_iostream_string_write((const unsigned char*)"\",\"inputSchema\":", iostr);
    raptor_iostream_string_write((const unsigned char*)mcp_tools[i].inputSchema, iostr);
    raptor_iostream_string_write((const unsigned char*)"}", iostr);
  }

  raptor_iostream_string_write((const unsigned char*)"],\"query_languages\":[\"sparql\"]}}\n", iostr);

  /* Free iostream to get the string */
  raptor_free_iostream(iostr);

  /* Copy to response buffer */
  if(output_string) {
    strncpy(response_buffer, (const char*)output_string, *response_len);
    *response_len = strlen((const char*)output_string);
    raptor_free_memory(output_string);
  }

  return 0;
}

/* Handle execute_sparql_query method */
static int
handle_execute_sparql_query(mcp_json_context* ctx, char* response_buffer, size_t* response_len)
{
  const char* result_format;
  const char* query_language;
  rasqal_query* query_obj;
  rasqal_query_results* results;
  rasqal_query_results_formatter* formatter;
  raptor_iostream* iostr;
  unsigned char* output_string;
  unsigned char* result_string = NULL;
  raptor_iostream* result_iostr;
  int i;
  rasqal_data_graph* data_graph;
  raptor_uri* source_uri;
  raptor_uri* name_uri;
  int result;

  if(!ctx->query)
    return create_error_response(-32602, "Missing or invalid query parameter",
                                 NULL, ctx->id, response_buffer, response_len);

  result_format = ctx->result_format ? ctx->result_format : "json";
  query_language = ctx->query_language ? ctx->query_language : "sparql";

  if(strcmp(query_language, "sparql"))
    return create_error_response(-32602, "Only SPARQL query language supported",
                                 NULL, ctx->id, response_buffer, response_len);

  /* Create query */
  query_obj = rasqal_new_query(world, query_language, NULL);
  if(!query_obj)
    return create_error_response(-32603, "Failed to create query",
                                 NULL, ctx->id, response_buffer, response_len);

  /* Parse query */
  if(rasqal_query_prepare(query_obj, (const unsigned char*)ctx->query, NULL)) {
    rasqal_free_query(query_obj);
    return create_error_response(-32603, "Query parse error",
                                 "Failed to parse SPARQL query", ctx->id,
                                 response_buffer, response_len);
  }

  /* Load data graphs */
  if(ctx->data_graphs && ctx->data_graphs_count > 0) {
    for(i = 0; i < ctx->data_graphs_count; i++) {
      const char* uri = ctx->data_graphs[i]->uri;
      const char* graph_format = ctx->data_graphs[i]->format;
      const char* type = ctx->data_graphs[i]->type;
      const char* name = ctx->data_graphs[i]->name;

      if(!strcmp(type, "background")) {
        /* Load as background graph */
        source_uri = raptor_new_uri_from_uri_or_file_string(raptor_world_ptr,
                         NULL, (const unsigned char*)uri);
        if(!source_uri) {
          rasqal_free_query(query_obj);
          return create_error_response(-32603,
                                       "Failed to create URI for background graph",
                                       uri, ctx->id,
                                       response_buffer, response_len);
        }

        /* Create data graph without name URI */
        data_graph = rasqal_new_data_graph_from_uri(world, source_uri, NULL,
                                                    RASQAL_DATA_GRAPH_BACKGROUND,
                                                    NULL, graph_format, NULL);
        if(!data_graph) {
          raptor_free_uri(source_uri);
          rasqal_free_query(query_obj);
          return create_error_response(-32603,
                                       "Failed to load background graph", uri,
                                       ctx->id, response_buffer, response_len);
        }

        /* Add the data graph to the query */
        rasqal_query_add_data_graph(query_obj, data_graph);

        /* Clean up URI */
        raptor_free_uri(source_uri);
      } else if(!strcmp(type, "named") && name) {
        /* Load as named graph */
        name_uri = raptor_new_uri(raptor_world_ptr, (const unsigned char*)name);
        source_uri = raptor_new_uri_from_uri_or_file_string(raptor_world_ptr,
                         NULL, (const unsigned char*)uri);
        if(!source_uri) {
          raptor_free_uri(name_uri);
          rasqal_free_query(query_obj);
          return create_error_response(-32603,
                                       "Failed to create URI for named graph",
                                       uri, ctx->id,
                                       response_buffer, response_len);
        }

        /* Create data graph with name URI */
        data_graph = rasqal_new_data_graph_from_uri(world, source_uri, name_uri, RASQAL_DATA_GRAPH_NAMED, NULL, graph_format, NULL);
        if(!data_graph) {
          raptor_free_uri(name_uri);
          raptor_free_uri(source_uri);
          rasqal_free_query(query_obj);
          return create_error_response(-32603,
                                       "Failed to load named graph", uri,
                                       ctx->id, response_buffer, response_len);
        }

        /* Add the data graph to the query */
        rasqal_query_add_data_graph(query_obj, data_graph);

        /* Clean up URIs */
        raptor_free_uri(name_uri);
        raptor_free_uri(source_uri);
      }
    }
  }

  /* Execute query */
  results = rasqal_query_execute(query_obj);
  if(!results) {
    rasqal_free_query(query_obj);
    return create_error_response(-32603, "Query execution failed",
                                 "Failed to execute SPARQL query",
                                 ctx->id, response_buffer, response_len);
  }

  /* Format results */
  formatter = rasqal_new_query_results_formatter(world, result_format,
                                                 NULL, NULL);
  if(!formatter) {
    rasqal_free_query_results(results);
    rasqal_free_query(query_obj);
    return create_error_response(-32603, "Failed to create formatter",
                                 NULL, ctx->id, response_buffer, response_len);
  }

  /* Capture formatted output */
  output_string = NULL;
  iostr = raptor_new_iostream_to_string(raptor_world_ptr,
                                        (void**)&output_string, NULL, 0);
  if(!iostr) {
    rasqal_free_query_results_formatter(formatter);
    rasqal_free_query_results(results);
    rasqal_free_query(query_obj);
    return create_error_response(-32603, "Failed to create output stream",
                                 NULL, ctx->id, response_buffer, response_len);
  }

  if(rasqal_query_results_formatter_write(iostr, formatter, results, NULL)) {
    raptor_free_iostream(iostr);
    rasqal_free_query_results_formatter(formatter);
    rasqal_free_query_results(results);
    rasqal_free_query(query_obj);
    return create_error_response(-32603, "Failed to format results",
                                 NULL, ctx->id, response_buffer, response_len);
  }

  /* Free iostream to get the formatted string */
  raptor_free_iostream(iostr);

  /* Create result using raptor_iostream */
  result_string = NULL;
  result_iostr = raptor_new_iostream_to_string(raptor_world_ptr,
                                               (void**)&result_string, NULL, 0);
  if(!result_iostr) {
    if(output_string) raptor_free_memory(output_string);
    rasqal_free_query_results_formatter(formatter);
    rasqal_free_query_results(results);
    rasqal_free_query(query_obj);
    return create_error_response(-32603, "Failed to create result stream",
                                 NULL, ctx->id, response_buffer, response_len);
  }

  raptor_iostream_counted_string_write("{\"output\":\"", 11, result_iostr);
  if(output_string) {
    size_t output_string_len = strlen((const char*)output_string);
    raptor_string_escaped_write(output_string, output_string_len , '\"',
                                RAPTOR_ESCAPED_WRITE_JSON_LITERAL, result_iostr);
  }
  raptor_iostream_counted_string_write("\",\"format\":\"", 12, result_iostr);
  raptor_iostream_string_write((const unsigned char*)result_format,
                               result_iostr);
  raptor_iostream_counted_string_write("\"}", 2, result_iostr);

  /* Free result iostream to get the string */
  raptor_free_iostream(result_iostr);

  result = create_mcp_tool_response(ctx->id,
                                   (const char*)result_string,
                                   response_buffer, response_len);

  /* Cleanup */
  if(output_string) raptor_free_memory(output_string);
  if(result_string) raptor_free_memory(result_string);
  rasqal_free_query_results_formatter(formatter);
  rasqal_free_query_results(results);
  rasqal_free_query(query_obj);

  return result;
}

/* Handle validate_sparql_query method */
static int
handle_validate_sparql_query(mcp_json_context* ctx, char* response_buffer,
                             size_t* response_len)
{
  const char* query_language;
  rasqal_query* query_obj;
  int parse_result;
  unsigned char* result_string = NULL;
  raptor_iostream* result_iostr;
  int result;

  log_message(RAPTOR_LOG_LEVEL_DEBUG, "handle_validate_sparql_query called");

  if(!ctx->query)
    return create_error_response(-32602, "Missing or invalid query parameter",
                                 NULL, ctx->id, response_buffer, response_len);

  query_language = ctx->query_language ? ctx->query_language : "sparql";

  if(strcmp(query_language, "sparql"))
    return create_error_response(-32602, "Only SPARQL query language supported",
                                 NULL, ctx->id, response_buffer, response_len);

  /* Create query */
  query_obj = rasqal_new_query(world, query_language, NULL);
  if(!query_obj)
    return create_error_response(-32603, "Failed to create query",
                                 NULL, ctx->id, response_buffer, response_len);

  /* Parse query */
  parse_result = rasqal_query_prepare(query_obj,
                                      (const unsigned char*)ctx->query, NULL);

  /* Create result using raptor_iostream */
  result_string = NULL;
  result_iostr = raptor_new_iostream_to_string(raptor_world_ptr,
                                               (void**)&result_string, NULL, 0);
  if(!result_iostr) {
    rasqal_free_query(query_obj);
    return create_error_response(-32603, "Failed to create result stream",
                                 NULL, ctx->id, response_buffer, response_len);
  }

  if(parse_result != 0) {
    raptor_iostream_string_write("{\"valid\":false,\"errors\":[{\"message\":\"Query parse error\"}]}", result_iostr);
  } else {
    /* Get query type */
    rasqal_query_results_type query_type = rasqal_query_get_result_type(query_obj);
    const char* type_str = "unknown";
    log_message(RAPTOR_LOG_LEVEL_DEBUG, "Query type enum value: %d", query_type);

    switch(query_type) {
      case RASQAL_QUERY_RESULTS_BINDINGS: type_str = "SELECT"; break;
      case RASQAL_QUERY_RESULTS_BOOLEAN: type_str = "ASK"; break;
      case RASQAL_QUERY_RESULTS_GRAPH: type_str = "CONSTRUCT"; break;
      case RASQAL_QUERY_RESULTS_SYNTAX: type_str = "DESCRIBE"; break;
      case RASQAL_QUERY_RESULTS_UNKNOWN: type_str = "UNKNOWN"; break;
      default: break;
    }

    log_message(RAPTOR_LOG_LEVEL_DEBUG, "Query type string: %s", type_str);

    if(query_type == RASQAL_QUERY_RESULTS_BINDINGS) {
      /* For SELECT queries, use bound variables which includes all variables used in the query */
      raptor_sequence* vars_seq = rasqal_query_get_bound_variable_sequence(query_obj);
      int num_vars = vars_seq ? raptor_sequence_size(vars_seq) : 0;

      log_message(RAPTOR_LOG_LEVEL_DEBUG, "SELECT query - bound variables: %d", num_vars);

      raptor_iostream_string_write((const unsigned char*)"{\"valid\":true,\"query_type\":\"", result_iostr);
      raptor_iostream_string_write((const unsigned char*)type_str, result_iostr);
      raptor_iostream_string_write((const unsigned char*)"\",\"variables\":[", result_iostr);

      for(int i = 0; i < num_vars; i++) {
        rasqal_variable* var = (rasqal_variable*)raptor_sequence_get_at(vars_seq, i);
        if(var) {
          if(i > 0)
            raptor_iostream_counted_string_write(",", 1, result_iostr);
          raptor_iostream_write_byte('\"', result_iostr);
          raptor_iostream_string_write(var->name, result_iostr);
          raptor_iostream_write_byte('\"', result_iostr);
          log_message(RAPTOR_LOG_LEVEL_DEBUG, "Added variable: %s", var->name);
        }
      }

      raptor_iostream_counted_string_write("]}", 2, result_iostr);
    } else {
      raptor_iostream_string_write((const unsigned char*)"{\"valid\":true,\"query_type\":\"", result_iostr);
      raptor_iostream_string_write((const unsigned char*)type_str, result_iostr);
      raptor_iostream_string_write((const unsigned char*)"\"}", result_iostr);
    }
  }

  /* Free result iostream to get the string */
  raptor_free_iostream(result_iostr);

  result = create_mcp_tool_response(ctx->id, (const char*)result_string,
                                   response_buffer, response_len);

  /* Cleanup */
  if(result_string) raptor_free_memory(result_string);
  rasqal_free_query(query_obj);

  return result;
}


/* Handle list_formats method */
static int
handle_list_formats(const char* id, char* response_buffer, size_t* response_len)
{
  unsigned char* output_string = NULL;
  raptor_iostream* iostr;
  raptor_world* raptor_world;
  int parser_count;
  int first;
  int i;
  int result;

  iostr = raptor_new_iostream_to_string(raptor_world_ptr,
                                        (void**)&output_string, NULL, 0);
  if(!iostr) {
    return create_error_response(-32603, "Failed to create output stream",
                                 NULL, id, response_buffer, response_len);
  }

  /* Get Raptor world from Rasqal world */
  raptor_world = rasqal_world_get_raptor(world);
  if(!raptor_world) {
    raptor_free_iostream(iostr);
    return create_error_response(-32603, "Failed to get Raptor world",
                                 NULL, id, response_buffer, response_len);
  }

  /* Build comprehensive JSON with descriptions */
  raptor_iostream_string_write((const unsigned char*)"{\"rdf_formats\":[", iostr);

  /* Get supported RDF input formats from Raptor with descriptions */
  parser_count = raptor_world_get_parsers_count(raptor_world);
  first = 1;
  for (i = 0; i < parser_count; i++) {
    const raptor_syntax_description* desc;
    desc = raptor_world_get_parser_description(raptor_world, i);
    if(desc && desc->names && desc->names[0]) {
      if(!first)
        raptor_iostream_string_write((const unsigned char*)",", iostr);

      raptor_iostream_string_write((const unsigned char*)"{\"name\":\"", iostr);
      raptor_iostream_string_write((const unsigned char*)desc->names[0], iostr);
      raptor_iostream_string_write((const unsigned char*)"\",\"description\":\"", iostr);

      /* Add descriptions for common formats */
      if(!strcmp(desc->names[0], "rdfxml")) {
        raptor_iostream_string_write((const unsigned char*)"RDF/XML format - W3C standard XML serialization of RDF", iostr);
      } else if(!strcmp(desc->names[0], "ntriples")) {
        raptor_iostream_string_write((const unsigned char*)"N-Triples format - Simple line-based RDF serialization", iostr);
      } else if(!strcmp(desc->names[0], "turtle")) {
        raptor_iostream_string_write((const unsigned char*)"Turtle format - Human-readable RDF serialization", iostr);
      } else if(!strcmp(desc->names[0], "trig")) {
        raptor_iostream_string_write((const unsigned char*)"TriG format - Turtle-based format for named graphs", iostr);
      } else if(!strcmp(desc->names[0], "rss-tag-soup")) {
        raptor_iostream_string_write((const unsigned char*)"RSS Tag Soup - RSS feed parsing with tag soup approach", iostr);
      } else if(!strcmp(desc->names[0], "grddl")) {
        raptor_iostream_string_write((const unsigned char*)"GRDDL format - Gleaning Resource Descriptions from Dialects of Languages", iostr);
      } else if(!strcmp(desc->names[0], "guess")) {
        raptor_iostream_string_write((const unsigned char*)"Auto-detect format - Automatically determine input format", iostr);
      } else if(!strcmp(desc->names[0], "rdfa")) {
        raptor_iostream_string_write((const unsigned char*)"RDFa format - RDF annotations embedded in HTML", iostr);
      } else if(!strcmp(desc->names[0], "nquads")) {
        raptor_iostream_string_write((const unsigned char*)"N-Quads format - N-Triples with graph context", iostr);
      } else {
        /* Generic description for other formats */
        raptor_iostream_string_write((const unsigned char*)"RDF format supported by Raptor parser", iostr);
      }

      raptor_iostream_string_write((const unsigned char*)"\"}", iostr);
      first = 0;
    }
  }

  raptor_iostream_string_write((const unsigned char*)"],\"result_formats\":[", iostr);

  /* Get supported result output formats from Rasqal with descriptions */
  first = 1;
  i = 0;
  while (1) {
    const raptor_syntax_description* desc;
    desc = rasqal_world_get_query_results_format_description(world, i);
    if (!desc) break;

    if(desc->names && desc->names[0]) {
      if(!first) {
        raptor_iostream_string_write((const unsigned char*)",", iostr);
      }

      raptor_iostream_string_write((const unsigned char*)"{\"name\":\"", iostr);
      raptor_iostream_string_write((const unsigned char*)desc->names[0], iostr);
      raptor_iostream_string_write((const unsigned char*)"\",\"description\":\"", iostr);

      /* Add descriptions for result formats */
      if(!strcmp(desc->names[0], "xml")) {
        raptor_iostream_string_write((const unsigned char*)"SPARQL Results XML - W3C standard XML format for query results", iostr);
      } else if(!strcmp(desc->names[0], "json")) {
        raptor_iostream_string_write((const unsigned char*)"SPARQL Results JSON - JSON format for query results", iostr);
      } else if(!strcmp(desc->names[0], "table")) {
        raptor_iostream_string_write((const unsigned char*)"Table format - Human-readable tabular output", iostr);
      } else if(!strcmp(desc->names[0], "csv")) {
        raptor_iostream_string_write((const unsigned char*)"CSV format - Comma-separated values for spreadsheet import", iostr);
      } else if(!strcmp(desc->names[0], "tsv")) {
        raptor_iostream_string_write((const unsigned char*)"TSV format - Tab-separated values for spreadsheet import", iostr);
      } else if(!strcmp(desc->names[0], "html")) {
        raptor_iostream_string_write((const unsigned char*)"HTML format - Web-ready HTML table output", iostr);
      } else if(!strcmp(desc->names[0], "turtle")) {
        raptor_iostream_string_write((const unsigned char*)"Turtle format - RDF serialization of query results", iostr);
      } else if(!strcmp(desc->names[0], "rdfxml")) {
        raptor_iostream_string_write((const unsigned char*)"RDF/XML format - XML serialization of query results", iostr);
      } else if(!strcmp(desc->names[0], "srj")) {
        raptor_iostream_string_write((const unsigned char*)"SPARQL Results JSON - Alternative JSON format", iostr);
      } else {
        /* Generic description for other formats */
        raptor_iostream_string_write((const unsigned char*)"Query result format supported by Rasqal", iostr);
      }

      raptor_iostream_string_write((const unsigned char*)"\"}", iostr);
      first = 0;
    }
    i++;
  }

  raptor_iostream_string_write((const unsigned char*)"],\"query_languages\":[{\"name\":\"sparql\",\"description\":\"SPARQL 1.1 Query Language - W3C standard for querying RDF data\"}]}", iostr);

  /* Free iostream to get the string */
  raptor_free_iostream(iostr);

  /* Create proper MCP tool response */
  result = create_mcp_tool_response(id,
                                   (const char*)output_string,
                                   response_buffer, response_len);

  /* Cleanup */
  if(output_string)
    raptor_free_memory(output_string);

  return result;
}




/* Cleanup resources */
static void
cleanup(void)
{
  if(log_fp) {
    time_t now;
    struct tm* tm_info;
    char time_str[26];

    now = time(NULL);
    tm_info = localtime(&now);
    strftime(time_str, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(log_fp, "%s %s: Log file closed\n", time_str, program);
    fclose(log_fp);
    log_fp = NULL;
  }

  if(world) {
    rasqal_free_world(world);
    world = NULL;
  }
}

/* Tool handler functions for tools/call */
static int
handle_execute_sparql_query_tool(const char* id, yajl_val arguments, char* response_buffer, size_t* response_len)
{
  yajl_val query_val;
  const char* query;
  yajl_val result_format_val;
  yajl_val query_language_val;
  yajl_val data_graphs_val;
  const char* result_format;
  const char* query_language;
  mcp_json_context ctx;
  size_t data_graphs_count;
  size_t i;
  yajl_val graph_val;
  data_graph_t* graph;
  yajl_val uri_val;
  yajl_val format_val;
  yajl_val type_val;
  yajl_val name_val;
  int result;
  int cleanup_i;

  /* Extract query from arguments */
  query_val = yajl_tree_get(arguments, (const char*[]){"query", 0}, yajl_t_any);
  if(!query_val || !YAJL_IS_STRING(query_val))
    return create_error_response(-32602, "Invalid params",
                                 "Missing query parameter", ctx.id,
                                 response_buffer, response_len);

  query = YAJL_GET_STRING(query_val);

  /* Extract optional parameters */
  result_format_val = yajl_tree_get(arguments, (const char*[]){"result_format", 0}, yajl_t_any);
  query_language_val = yajl_tree_get(arguments, (const char*[]){"query_language", 0}, yajl_t_any);
  data_graphs_val = yajl_tree_get(arguments, (const char*[]){"data_graphs", 0}, yajl_t_any);

  result_format = result_format_val && YAJL_IS_STRING(result_format_val) ? YAJL_GET_STRING(result_format_val) : "json";
  query_language = query_language_val && YAJL_IS_STRING(query_language_val) ? YAJL_GET_STRING(query_language_val) : "sparql";

  log_message(RAPTOR_LOG_LEVEL_DEBUG, "Executing SPARQL query: %s", query);

  /* Create a temporary context for the query */
  memset(&ctx, 0, sizeof(ctx));
  ctx.id = (char*)id;
  ctx.query = (char*)query;
  ctx.result_format = (char*)result_format;
  ctx.query_language = (char*)query_language;

  /* Parse data graphs if present */
  if(data_graphs_val && YAJL_IS_ARRAY(data_graphs_val)) {
    data_graphs_count = YAJL_GET_ARRAY(data_graphs_val)->len;
    if(data_graphs_count > 0) {
      ctx.data_graphs = malloc(data_graphs_count * sizeof(data_graph_t*));
      ctx.data_graphs_count = (int)data_graphs_count;

      for (i = 0; i < data_graphs_count; i++) {
        graph_val = YAJL_GET_ARRAY(data_graphs_val)->values[i];
        if(graph_val && YAJL_IS_OBJECT(graph_val)) {
          graph = malloc(sizeof(data_graph_t));
          memset(graph, 0, sizeof(data_graph_t));

          /* Extract graph properties */
          uri_val = yajl_tree_get(graph_val, (const char*[]){"uri", 0}, yajl_t_any);
          format_val = yajl_tree_get(graph_val, (const char*[]){"format", 0}, yajl_t_any);
          type_val = yajl_tree_get(graph_val, (const char*[]){"type", 0}, yajl_t_any);
          name_val = yajl_tree_get(graph_val, (const char*[]){"name", 0}, yajl_t_any);

          if(uri_val && YAJL_IS_STRING(uri_val))
            graph->uri = strdup(YAJL_GET_STRING(uri_val));

          if(format_val && YAJL_IS_STRING(format_val))
            graph->format = strdup(YAJL_GET_STRING(format_val));

          if(type_val && YAJL_IS_STRING(type_val))
            graph->type = strdup(YAJL_GET_STRING(type_val));

          if(name_val && YAJL_IS_STRING(name_val))
            graph->name = strdup(YAJL_GET_STRING(name_val));

          ctx.data_graphs[i] = graph;
        }
      }
    }
  }

  /* Execute the query */
  result = handle_execute_sparql_query(&ctx, response_buffer, response_len);

  /* Clean up data graphs */
  if(ctx.data_graphs) {
    for (cleanup_i = 0; cleanup_i < ctx.data_graphs_count; cleanup_i++) {
      if(ctx.data_graphs[cleanup_i]) {
        if(ctx.data_graphs[cleanup_i]->uri) free(ctx.data_graphs[cleanup_i]->uri);
        if(ctx.data_graphs[cleanup_i]->format) free(ctx.data_graphs[cleanup_i]->format);
        if(ctx.data_graphs[cleanup_i]->type) free(ctx.data_graphs[cleanup_i]->type);
        if(ctx.data_graphs[cleanup_i]->name) free(ctx.data_graphs[cleanup_i]->name);
        free(ctx.data_graphs[cleanup_i]);
      }
    }
    free(ctx.data_graphs);
  }

  return result;
}

static int
handle_validate_sparql_query_tool(const char* id, yajl_val arguments, char* response_buffer, size_t* response_len)
{
  yajl_val query_val;
  const char* query;
  mcp_json_context ctx;
  int result;

  /* Extract query from arguments */
  query_val = yajl_tree_get(arguments, (const char*[]){"query", 0}, yajl_t_any);
  if(!query_val || !YAJL_IS_STRING(query_val))
    return create_error_response(-32602, "Invalid params",
                                 "Missing query parameter",
                                 ctx.id, response_buffer, response_len);

  query = YAJL_GET_STRING(query_val);

  log_message(RAPTOR_LOG_LEVEL_DEBUG, "Validating SPARQL query: %s", query);

  /* Create a temporary context for the query */
  memset(&ctx, 0, sizeof(ctx));
  ctx.id = (char*)id;
  ctx.query = (char*)query;

  /* Validate the query */
  result = handle_validate_sparql_query(&ctx, response_buffer, response_len);

  return result;
}


/* Handle JSON-RPC request */
static int
handle_jsonrpc_request(yajl_val request, char* response_buffer, size_t* response_len)
{
  yajl_val method_val;
  const char* method;
  yajl_val id_val;
  const char* id;
  yajl_val params_val;
  yajl_val name_val;
  const char* tool_name;
  yajl_val arguments_val;

  /* Extract method */
  method_val = yajl_tree_get(request, (const char*[]){"method", 0}, yajl_t_any);
  if(!method_val || !YAJL_IS_STRING(method_val))
    return create_error_response(-32600, "Invalid Request", "Missing or invalid method", id, response_buffer, response_len);

  method = YAJL_GET_STRING(method_val);

  /* Extract ID */
  id_val = yajl_tree_get(request, (const char*[]){"id", 0}, yajl_t_any);
  id = NULL;
  if(id_val && YAJL_IS_STRING(id_val)) {
    id = YAJL_GET_STRING(id_val);
  } else if(id_val && YAJL_IS_NUMBER(id_val)) {
    /* Convert number to string for simplicity */
    static char id_str[32];
    raptor_snprintf(id_str, sizeof(id_str), "%lld", YAJL_GET_INTEGER(id_val));
    id = id_str;
  }

  log_message(RAPTOR_LOG_LEVEL_DEBUG, "Handling method: %s, id: %s", method, id ? id : "null");

  /* Route to appropriate handler */
  if(!strcmp(method, "tools/list")) {
    return handle_list_tools(id, response_buffer, response_len);
  } else if(!strcmp(method, "tools/call")) {
    /* Extract tool name and arguments */
    params_val = yajl_tree_get(request, (const char*[]){"params", 0}, yajl_t_any);
    if(!params_val || !YAJL_IS_OBJECT(params_val))
      return create_error_response(-32602, "Invalid params", "Missing or invalid params", id, response_buffer, response_len);

    name_val = yajl_tree_get(params_val, (const char*[]){"name", 0}, yajl_t_any);
    if(!name_val || !YAJL_IS_STRING(name_val))
      return create_error_response(-32602, "Invalid params", "Missing tool name", id, response_buffer, response_len);

    tool_name = YAJL_GET_STRING(name_val);
    arguments_val = yajl_tree_get(params_val, (const char*[]){"arguments", 0}, yajl_t_any);

    log_message(RAPTOR_LOG_LEVEL_DEBUG, "Calling tool: %s", tool_name);

    if(!strcmp(tool_name, "execute_sparql_query"))
      return handle_execute_sparql_query_tool(id, arguments_val, response_buffer, response_len);
    else if(!strcmp(tool_name, "validate_sparql_query"))
      return handle_validate_sparql_query_tool(id, arguments_val, response_buffer, response_len);

    else if(!strcmp(tool_name, "list_formats"))
      return handle_list_formats(id, response_buffer, response_len);
    else
      return create_error_response(-32601, "Method not found", "Unknown tool", id, response_buffer, response_len);

  } else if(!strcmp(method, "initialize")) {
    /* Send capabilities response with tool definitions */
    return handle_initialize(id, response_buffer, response_len);
  } else if(!strcmp(method, "notifications/initialized")) {
    /* This is a notification, no response needed */
    log_message(RAPTOR_LOG_LEVEL_DEBUG, "Received notifications/initialized notification");
    *response_len = 0;  /* No response for notifications */
    return 0;
  } else if(!strcmp(method, "notifications/cancelled")) {
    /* This is a notification, no response needed */
    log_message(RAPTOR_LOG_LEVEL_DEBUG, "Received notifications/cancelled notification");
    *response_len = 0;  /* No response for notifications */
    return 0;
  } else {
    return create_error_response(-32601, "Method not found", "Unknown method", id, response_buffer, response_len);
  }
}


/* Help text macros */
#ifdef HAVE_GETOPT_LONG
#define HELP_TEXT(short, long, description) "  -" short ", --" long "  " description
#define HELP_TEXT_LONG(long, description) "      --" long "  " description
#else
#define HELP_TEXT(short, long, description) "  -" short "  " description
#define HELP_TEXT_LONG(long, description)
#endif

/* Getopt string */
#define GETOPT_STRING "hvqdl:"

#ifdef HAVE_GETOPT_LONG
static struct option long_options[] = {
  {"help", 0, 0, 'h'},
  {"version", 0, 0, 'v'},
  {"quiet", 0, 0, 'q'},
  {"debug", 0, 0, 'd'},
  {"log-file", 1, 0, 'l'},
  {NULL, 0, 0, 0}
};
#endif

/* Title string */
static const char* title_string = "Rasqal MCP Server ";


/* Parse command line arguments */
static int parse_args(int argc, char* argv[])
{
  int c;

  program = argv[0];
  if(strrchr(program, '/'))
    program = strrchr(program, '/') + 1;

#ifdef HAVE_GETOPT_LONG
  while ((c = getopt_long(argc, argv, GETOPT_STRING, long_options, NULL)) != -1) {
#else
  while ((c = getopt(argc, argv, GETOPT_STRING)) != -1) {
#endif
    switch (c) {
      case 'h':
        help = 1;
        break;
      case 'v':
        version = 1;
        break;
      case 'q':
        quiet = 1;
        break;
      case 'd':
        debug = 1;
        break;
      case 'l':
        log_file = optarg;
        if(!log_file || !strlen(log_file)) {
          fprintf(stderr, "%s: Invalid log file path\n", program);
          return 1;
        }
        break;
      case '?':
        /* getopt already printed an error message */
        return 1;
      default:
        return 1;
    }
  }

  if(optind < argc) {
    fprintf(stderr, "%s: unexpected argument '%s'\n", program, argv[optind]);
    return 1;
  }

  return 0;
}


/* Print help information */
static void print_help(void)
{
  puts(title_string);
  puts("Model Context Protocol (MCP) server for Rasqal SPARQL queries.\n");
  printf("Usage: %s [OPTIONS]\n\n", program);

  puts("The MCP server runs as a JSON-RPC server over stdin/stdout, providing");
  puts("SPARQL query capabilities to AI agents and other MCP clients.\n");

  puts("Options:");
  puts(HELP_TEXT("h", "help", "Print this help, then exit"));
  puts(HELP_TEXT("v", "version", "Print version information, then exit"));
  puts(HELP_TEXT("q", "quiet", "Suppress non-error messages"));
  puts(HELP_TEXT("d", "debug", "Enable debug output"));
  puts(HELP_TEXT("l", "log-file FILE", "Write log output to FILE"));

  puts("\nThe server implements the following MCP tools:");
  puts("  execute_sparql_query - Execute a SPARQL query against RDF data");
  puts("  validate_sparql_query - Validate SPARQL query syntax");
  puts("  list_formats - List supported input/output formats");

  puts("\nExample usage:");
  puts("  echo '{\"jsonrpc\":\"2.0\",\"method\":\"tools/list\",\"id\":1}' | ./rasqal-mcp-server");
  puts("  echo '{\"jsonrpc\":\"2.0\",\"method\":\"tools/call\",\"params\":{\"name\":\"execute_sparql_query\",\"arguments\":{\"query\":\"SELECT * WHERE { ?s ?p ?o }\"}},\"id\":1}' | ./rasqal-mcp-server");
}

/* Print version information */
static void print_version(void)
{
  puts(title_string);
  puts(rasqal_version_string);
  printf("\nRasqal home page: %s\n", rasqal_home_url_string);
  puts("This package is Free Software and part of Redland http://librdf.org/");
  puts("\nIt is licensed under the following three licenses as alternatives:");
  puts("  1. GNU Lesser General Public License (LGPL) V2.1 or any newer version");
  puts("  2. GNU General Public License (GPL) V2 or any newer version");
  puts("  3. Apache License, V2.0 or any newer version");
}


/* Main function */
int main(int argc, char *argv[])
{
  /* Variable declarations */
  char* response_buffer;
  char* line_buffer;
  char* result;
  size_t len;
  yajl_val request;
  size_t response_len;

  /* Allocate buffers on heap */
  response_buffer = malloc(JSON_BUFFER_SIZE);
  if(!response_buffer) {
    fprintf(stderr, "Failed to allocate response buffer\n");
    return 1;
  }

  line_buffer = malloc(4096);
  if(!line_buffer) {
    fprintf(stderr, "Failed to allocate line buffer\n");
    goto cleanup;
  }

  /* Parse command line arguments first */
  if(parse_args(argc, argv)) {
    print_help();
    return 1;
  }

  if(help) {
    print_help();
    return 0;
  }

  if(version) {
    print_version();
    return 0;
  }

  /* Initialize log file if specified (after parsing args) */
  if(init_log_file()) {
    fprintf(stderr, "Failed to initialize log file\n");
    goto cleanup;
  }

  /* Initialize MCP server */
  if(mcp_server_init()) {
    fprintf(stderr, "Failed to initialize MCP server\n");
    goto cleanup;
  }

  /* Set up cleanup on exit */
  atexit(cleanup);

  if(help) {
    print_help();
    return 0;
  }

  if(version) {
    print_version();
    return 0;
  }

  /* Start MCP server */
  if(!quiet)
    log_message(RAPTOR_LOG_LEVEL_INFO, "Starting MCP server");

  log_message(RAPTOR_LOG_LEVEL_DEBUG, "Waiting for MCP requests");

  /* Main MCP server loop */
  while(1) {
    result = fgets(line_buffer, 4096, stdin);

    if(!result) {
      if(feof(stdin)) {
        log_message(RAPTOR_LOG_LEVEL_INFO, "End of input, shutting down");
        break;
      }
              log_message(RAPTOR_LOG_LEVEL_ERROR, "Error reading from stdin");
      break;
    }

    /* Remove newline */
    len = strlen(line_buffer);
    if(len > 0 && line_buffer[len-1] == '\n') {
      line_buffer[len-1] = '\0';
      len--;
    }

    if(!len)
      continue;

    log_message(RAPTOR_LOG_LEVEL_DEBUG, "Received request: %s", line_buffer);

    /* Parse JSON-RPC request */
    request = yajl_tree_parse(line_buffer, NULL, 0);
    if(!request) {
      log_message(RAPTOR_LOG_LEVEL_WARN, "Failed to parse JSON-RPC request");
      continue;
    }

    /* Handle the request */
    response_len = JSON_BUFFER_SIZE;

    if(handle_jsonrpc_request(request, response_buffer, &response_len)) {
      log_message(RAPTOR_LOG_LEVEL_ERROR, "Failed to handle JSON-RPC request");
      yajl_tree_free(request);
      continue;
    }

    /* Send response (only if response_len > 0) */
    if(response_len > 0) {
      size_t written;
      log_message(RAPTOR_LOG_LEVEL_DEBUG, "About to send response - length: %zu", response_len);
      log_message(RAPTOR_LOG_LEVEL_DEBUG, "Response content: '%s'", response_buffer);

      written = fwrite(response_buffer, 1, response_len, stdout);
      if(written != response_len) {
        log_message(RAPTOR_LOG_LEVEL_ERROR, "Failed to write response: wrote %zu of %zu bytes", written, response_len);
        yajl_tree_free(request);
        break;
      }

      log_message(RAPTOR_LOG_LEVEL_DEBUG, "Wrote %zu bytes to stdout", written);
      fflush(stdout);

      log_message(RAPTOR_LOG_LEVEL_DEBUG, "Response sent successfully to stdout");
    } else {
      log_message(RAPTOR_LOG_LEVEL_DEBUG, "No response needed (notification)");
    }

    yajl_tree_free(request);
  }

  log_message(RAPTOR_LOG_LEVEL_INFO, "MCP server shutdown complete");

  return 0;

cleanup:
  if(line_buffer)
    free(line_buffer);
  if(response_buffer)
    free(response_buffer);

  return 1;
}

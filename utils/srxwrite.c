/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * srxwrite.c - SPARQL Results Format writing test program
 *
 * Copyright (C) 2010, David Beckett http://www.dajobe.org/
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
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif


#include <raptor.h>

/* Rasqal includes */
#include <rasqal.h>

#ifdef RAPTOR_V2_AVAILABLE
#else
#define raptor_new_iostream_to_file_handle(world, fh) raptor_new_iostream_to_file_handle(fh)
#endif

static char *program = NULL;

int main(int argc, char *argv[]);

int
main(int argc, char *argv[]) 
{ 
  int rc = 0;
  raptor_iostream* iostr = NULL;
  char* p;
  raptor_uri* base_uri = NULL;
  rasqal_query_results* results = NULL;
  const char* write_formatter_name = NULL;
  rasqal_query_results_formatter* write_formatter = NULL;
  rasqal_world *world;
  rasqal_variables_table* vars_table = NULL;
  rasqal_row* row = NULL;
  rasqal_literal* l = NULL;
#ifdef RAPTOR_V2_AVAILABLE
  raptor_world *raptor_world_ptr;
#endif
  
  program = argv[0];
  if((p=strrchr(program, '/')))
    program = p+1;
  else if((p=strrchr(program, '\\')))
    program = p+1;
  argv[0] = program;
  
  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    return(1);
  }

  if(argc < 2 || argc > 2) {
    fprintf(stderr, "USAGE: %s [<write formatter>]\n",
            program);

    rc = 1;
    goto tidy;
  }

  if(argc > 1) {
    if(strcmp(argv[1], "-"))
      write_formatter_name = argv[1];
  }

#ifdef RAPTOR_V2_AVAILABLE
  raptor_world_ptr = rasqal_world_get_raptor(world);
#endif
  
  vars_table = rasqal_new_variables_table(world);
  if(!vars_table) {
    fprintf(stderr, "%s: Failed to create variables table\n", program);
    rc = 1;
    goto tidy;
  }

#define NUMBER_VARIABLES 2
  rasqal_variables_table_add2(vars_table, RASQAL_VARIABLE_TYPE_NORMAL,
                              (const unsigned char*)"a", 1, NULL);
  rasqal_variables_table_add2(vars_table, RASQAL_VARIABLE_TYPE_NORMAL,
                              (const unsigned char*)"b", 1, NULL);

  results = rasqal_new_query_results2(world, NULL,
                                      RASQAL_QUERY_RESULTS_BINDINGS);
  if(!results) {
    fprintf(stderr, "%s: Failed to create query results\n", program);
    rc = 1;
    goto tidy;
  }

  row = rasqal_new_row_for_size(world, NUMBER_VARIABLES);

  l = rasqal_new_boolean_literal(world, 1);
  if(!l) {
    fprintf(stderr, "%s: Failed to create boolean literal\n", program);
    rc = 1;
    goto tidy;
  }
  rasqal_row_set_value_at(row, 0, l);
  rasqal_free_literal(l); l = NULL;
  
  l = rasqal_new_integer_literal(world, RASQAL_LITERAL_INTEGER, 42);
  if(!l) {
    fprintf(stderr, "%s: Failed to integer boolean literal\n", program);
    rc = 1;
    goto tidy;
  }
  rasqal_row_set_value_at(row, 1, l);
  rasqal_free_literal(l); l = NULL;

  rasqal_query_results_add_row(results, row);
  row = NULL; /* now owned by results */


  write_formatter = rasqal_new_query_results_formatter(world, 
                                                       write_formatter_name,
                                                       NULL, NULL);
  if(!write_formatter) {
    fprintf(stderr, "%s: Failed to create query results write formatter '%s'\n",
            program, write_formatter_name);
    rc = 1;
    goto tidy;
  }
  
  iostr = raptor_new_iostream_to_file_handle(raptor_world_ptr, stdout);
  if(!iostr) {
    fprintf(stderr, "%s: Creating output iostream failed\n", program);
  } else {
    rasqal_query_results_formatter_write(iostr, write_formatter,
                                         results, base_uri);
    raptor_free_iostream(iostr);
  }

  iostr = NULL;
  

  tidy:
  if(l)
    rasqal_free_literal(l);

  if(row)
    rasqal_free_row(row);

  if(write_formatter)
    rasqal_free_query_results_formatter(write_formatter);

  if(iostr)
    raptor_free_iostream(iostr);
  
  if(results)
    rasqal_free_query_results(results);

  if(vars_table)
    rasqal_free_variables_table(vars_table);

  if(base_uri)
    raptor_free_uri(base_uri);

  rasqal_free_world(world);

  return (rc);
}

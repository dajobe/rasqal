/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_graph.c - Rasqal graph rowsource class
 *
 * Copyright (C) 2008-2009, David Beckett http://www.dajobe.org/
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
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <raptor.h>

#include "rasqal.h"
#include "rasqal_internal.h"


#define DEBUG_FH stderr


/* 

This rowsource evaluates part #3 where a variable is given.

rasqal_algebra_graph_algebra_node_to_rowsource() implements #1 and #2


http://www.w3.org/TR/2008/REC-rdf-sparql-query-20080115/#sparqlAlgebraEval

SPARQL Query Language for RDF - Evaluation of a Graph Pattern

#1 if IRI is a graph name in D
eval(D(G), Graph(IRI,P)) = eval(D(D[IRI]), P)

#2 if IRI is not a graph name in D
eval(D(G), Graph(IRI,P)) = the empty multiset

#3 eval(D(G), Graph(var,P)) =
     Let R be the empty multiset
     foreach IRI i in D
        R := Union(R, Join( eval(D(D[i]), P) , Ω(?var->i) )
     the result is R
*/



typedef struct 
{
  /* inner rowsource */
  rasqal_rowsource *rowsource;
  
  /* GRAPH literal constant URI or variable */
  rasqal_variable* var;

  /* dataset graph offset */
  int dg_offset;
  
  /* number of graphs total */
  int dg_size;
  
  /* row offset for read_row() */
  int offset;

  int finished;

} rasqal_graph_rowsource_context;


static int
rasqal_graph_next_dg(rasqal_graph_rowsource_context *con) 
{
  rasqal_query *query = con->rowsource->query;
  rasqal_data_graph *dg;

  con->finished = 0;

  while(1) {
    rasqal_literal *o;

    con->dg_offset++;
    dg = rasqal_query_get_data_graph(query, con->dg_offset);
    if(!dg) {
      con->finished = 1;
      break;
    }
    
    if(!dg->name_uri)
      continue;
    
    o = rasqal_new_uri_literal(query->world, raptor_uri_copy(dg->name_uri));
    if(!o) {
      RASQAL_DEBUG1("Failed to create new URI literal\n");
      con->finished = 1;
      break;
    }

    RASQAL_DEBUG2("Using data graph URI literal <%s>\n",
                  rasqal_literal_as_string(o));
    
    rasqal_rowsource_set_origin(con->rowsource, o);

    /* this passes ownership of o to con->var */
    rasqal_variable_set_value(con->var, o);
      
    break;
  }

  return con->finished;
}


static int
rasqal_graph_rowsource_init(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_graph_rowsource_context *con;
  raptor_sequence* seq;

  con = (rasqal_graph_rowsource_context*)user_data;
  
  seq = rasqal_query_get_data_graph_sequence(rowsource->query);
  if(!seq)
    return 1;

  con->dg_size = raptor_sequence_size(seq);
  
  con->finished = 0;
  con->dg_offset = -1;
  con->offset = 0;

  /* Do not care if finished at this stage (it is not an
   * error). rasqal_graph_rowsource_read_row() will deal with
   * returning NULL for an empty result.
   */
  rasqal_graph_next_dg(con);

  return 0;
}


static int
rasqal_graph_rowsource_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_graph_rowsource_context *con;
  con = (rasqal_graph_rowsource_context*)user_data;

  if(con->rowsource)
    rasqal_free_rowsource(con->rowsource);
  
  rasqal_variable_set_value(con->var, NULL);

  RASQAL_FREE(rasqal_graph_rowsource_context, con);

  return 0;
}


static int
rasqal_graph_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                         void *user_data)
{
  rasqal_graph_rowsource_context* con;
  
  con = (rasqal_graph_rowsource_context*)user_data; 

  rasqal_rowsource_ensure_variables(con->rowsource);

  rowsource->size = 0;
  /* Put GRAPH variable first in result row */
  rasqal_rowsource_add_variable(rowsource, con->var);
  rasqal_rowsource_copy_variables(rowsource, con->rowsource);

  return 0;
}


static rasqal_row*
rasqal_graph_rowsource_read_row(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_graph_rowsource_context *con;
  rasqal_row* row = NULL;

  con = (rasqal_graph_rowsource_context*)user_data;

  if(con->finished)
    return NULL;
  
  while(1) {
    row = rasqal_rowsource_read_row(con->rowsource);
    if(row)
      break;
    
    if(rasqal_graph_next_dg(con)) {
      con->finished = 1;
      break;
    }
    if(rasqal_rowsource_reset(con->rowsource)) {
      con->finished = 1;
      break;
    }
  }

  /* If a row is returned, put the GRAPH variable value as first literal */
  if(row) {
    rasqal_row* nrow;
    int i;
    
    nrow = rasqal_new_row_for_size(rowsource->world, 1 + row->size);
    if(!nrow) {
      rasqal_free_row(row);
      row = NULL;
    } else {
      rasqal_row_set_rowsource(nrow, rowsource);
      nrow->offset = row->offset;
      
      /* Put GRAPH variable value (or NULL) first in result row */
      nrow->values[0] = rasqal_new_literal_from_literal(con->var->value);

      /* Copy (size-1) remaining variables from input row */
      for(i = 0; i < row->size; i++)
        nrow->values[i + 1] = rasqal_new_literal_from_literal(row->values[i]);
      rasqal_free_row(row);
      row = nrow;
    }
  }
  
  return row;
}


static int
rasqal_graph_rowsource_reset(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_graph_rowsource_context *con;
  con = (rasqal_graph_rowsource_context*)user_data;

  con->finished = 0;
  con->dg_offset = -1;
  con->offset = 0;

  rasqal_graph_next_dg(con);
  
  return rasqal_rowsource_reset(con->rowsource);
}


static rasqal_rowsource*
rasqal_graph_rowsource_get_inner_rowsource(rasqal_rowsource* rowsource,
                                           void *user_data, int offset)
{
  rasqal_graph_rowsource_context *con;
  con = (rasqal_graph_rowsource_context*)user_data;

  if(offset == 0)
    return con->rowsource;
  return NULL;
}


static const rasqal_rowsource_handler rasqal_graph_rowsource_handler = {
  /* .version =          */ 1,
  "graph",
  /* .init =             */ rasqal_graph_rowsource_init,
  /* .finish =           */ rasqal_graph_rowsource_finish,
  /* .ensure_variables = */ rasqal_graph_rowsource_ensure_variables,
  /* .read_row =         */ rasqal_graph_rowsource_read_row,
  /* .read_all_rows =    */ NULL,
  /* .reset =            */ rasqal_graph_rowsource_reset,
  /* .set_requirements = */ NULL,
  /* .get_inner_rowsource = */ rasqal_graph_rowsource_get_inner_rowsource,
  /* .set_origin =       */ NULL,
};


/**
 * rasqal_new_graph_rowsource:
 * @world: world object
 * @query: query object
 * @rowsource: input rowsource
 * @var: graph variable
 *
 * INTERNAL - create a new GRAPH rowsource that binds a variable
 *
 * The @rowsource becomes owned by the new rowsource
 *
 * Return value: new rowsource or NULL on failure
 */
rasqal_rowsource*
rasqal_new_graph_rowsource(rasqal_world *world,
                           rasqal_query *query,
                           rasqal_rowsource* rowsource,
                           rasqal_variable *var)
{
  rasqal_graph_rowsource_context *con;
  int flags = 0;
  
  if(!world || !query || !rowsource || !var)
    return NULL;
  
  con = RASQAL_CALLOC(rasqal_graph_rowsource_context*, 1, sizeof(*con));
  if(!con)
    return NULL;

  con->rowsource = rowsource;
  con->var = var;

  return rasqal_new_rowsource_from_handler(world, query,
                                           con,
                                           &rasqal_graph_rowsource_handler,
                                           query->vars_table,
                                           flags);
}

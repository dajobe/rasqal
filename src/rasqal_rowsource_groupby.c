/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_rowsource_groupby.c - Rasqal GROUP BY and HAVING rowsource class
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


#ifndef STANDALONE

typedef struct 
{
  /* inner rowsource to filter */
  rasqal_rowsource *rowsource;

  /* group expression list */
  raptor_sequence* expr_seq;

  /* size of above list: can be 0 if @expr_seq is NULL too */
  int expr_seq_size;

  /* last group ID assigned */
  int group_id;

  /* avltree for grouping.  [lit, lit, ...] -> [ row, row, row, ... ]
   *  key: raptor_sequence* of rasqal_literal*
   *  value: raptor_sequence* of rasqal_row*
   */
  raptor_avltree* tree;

  /* rasqal_literal_compare() flags */
  int compare_flags;

  /* iterator into tree above */
  raptor_avltree_iterator* group_iterator;
  /* index into sequence of rows at current avltree node */
  int group_row_index;

  /* output row offset */
  int offset;
} rasqal_groupby_rowsource_context;


typedef struct {
  rasqal_groupby_rowsource_context* con;

  /* Integer ID of this group */
  int group_id;

  /* Key of this group (seq of literals) */
  raptor_sequence* literals;

  /* Value of this group (seq of rows) */
  raptor_sequence* rows;

} rasqal_groupby_tree_node;


static void
rasqal_free_groupby_tree_node(rasqal_groupby_tree_node* node)
{
  if(!node)
    return;
  
  if(node->literals)
    raptor_free_sequence(node->literals);
  
  if(node->rows)
    raptor_free_sequence(node->rows);
  
  RASQAL_FREE(rasqal_groupby_tree_node, node);
}


#ifdef HAVE_RAPTOR2_API
static int
#else
static void
#endif
rasqal_rowsource_groupby_tree_print_node(void *object, FILE *fh)
{
  rasqal_groupby_tree_node* node = (rasqal_groupby_tree_node*)object;
  
  fputs("Key Literals:\n", fh);
  if(node->literals)
    /* sequence of literals */
    raptor_sequence_print(node->literals, fh);
  else
    fputs("None", fh);

  fputs("\nValue Rows:\n", fh);
  if(node->rows) {
    int i;
    int size = raptor_sequence_size(node->rows);
    
    /* sequence of rows */
    for(i = 0; i < size; i++) {
      rasqal_row* row = (rasqal_row*)raptor_sequence_get_at(node->rows, i);
      fprintf(fh, "  Row %d: ", i);
      rasqal_row_print(row, fh);
    }
  } else
    fputs("None", fh);

#ifdef HAVE_RAPTOR2_API
  return 0;
#endif
}


static int
rasqal_rowsource_groupby_literal_sequence_compare(const void *a, const void *b)
{
  rasqal_groupby_rowsource_context* con;
  rasqal_groupby_tree_node* node_a = (rasqal_groupby_tree_node*)a;
  rasqal_groupby_tree_node* node_b = (rasqal_groupby_tree_node*)b;

  con = node_a->con;

  return rasqal_literal_sequence_compare(con->compare_flags,
                                         node_a->literals, node_b->literals);
}


static int
rasqal_groupby_rowsource_init(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_groupby_rowsource_context* con;
  con = (rasqal_groupby_rowsource_context*)user_data;

  con->group_id = -1;

  con->compare_flags = RASQAL_COMPARE_URI;

  con->offset = 0;
  return 0;
}


static int
rasqal_groupby_rowsource_finish(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_groupby_rowsource_context* con;
  con = (rasqal_groupby_rowsource_context*)user_data;

  if(con->rowsource)
    rasqal_free_rowsource(con->rowsource);
  
  if(con->expr_seq)
    raptor_free_sequence(con->expr_seq);
  
  if(con->tree)
    raptor_free_avltree(con->tree);
  
  RASQAL_FREE(rasqal_groupby_rowsource_context, con);

  return 0;
}


static int
rasqal_groupby_rowsource_ensure_variables(rasqal_rowsource* rowsource,
                                          void *user_data)
{
  rasqal_groupby_rowsource_context* con;
  
  con = (rasqal_groupby_rowsource_context*)user_data; 

  if(rasqal_rowsource_ensure_variables(con->rowsource))
    return 1;

  rowsource->size = 0;
  if(rasqal_rowsource_copy_variables(rowsource, con->rowsource))
    return 1;

  return 0;
}


static int
rasqal_groupby_rowsource_process(rasqal_rowsource* rowsource,
                                 rasqal_groupby_rowsource_context* con)
{
  /* already processed */
  if(con->tree)
    return 0;

  con->tree = raptor_new_avltree(rasqal_rowsource_groupby_literal_sequence_compare,
                                 (raptor_data_free_handler)rasqal_free_groupby_tree_node,
                                 /* flags */ 0);

  if(!con->tree)
    return 1;

  raptor_avltree_set_print_handler(con->tree,
                                   rasqal_rowsource_groupby_tree_print_node);
  

  while(1) {
    rasqal_row* row;
    
    row = rasqal_rowsource_read_row(con->rowsource);
    if(!row)
      break;

    if(con->expr_seq) {
      raptor_sequence* literal_seq;
      rasqal_groupby_tree_node key;
      rasqal_groupby_tree_node* node;
      
      literal_seq = rasqal_expression_sequence_evaluate(rowsource->query,
                                                        con->expr_seq,
                                                        /* ignore_errors */ 0,
                                                        /* literal_seq */ NULL,
                                                        /* error_p */ NULL);
      
      if(!literal_seq) {
        /* FIXME - what to do on errors? */
        continue;
      }
      
      memset(&key, '\0', sizeof(key));
      key.literals = literal_seq;
      
      node = (rasqal_groupby_tree_node*)raptor_avltree_search(con->tree, &key);
      if(!node) {
        /* New Group */
        node = (rasqal_groupby_tree_node*)RASQAL_CALLOC(rasqal_groupby_tree_node, sizeof(*node), 1);
        if(!node) {
          raptor_free_sequence(literal_seq);
          return 1;
        }
        
        node->group_id = ++con->group_id;

        /* node now owns literal_seq */
        node->literals = literal_seq;

#ifdef HAVE_RAPTOR2_API
        node->rows = raptor_new_sequence((raptor_data_free_handler)rasqal_free_row, (raptor_data_print_handler)rasqal_row_print);
#else
        node->rows = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_row, (raptor_sequence_print_handler*)rasqal_row_print);
#endif
        if(!node->rows) {
          rasqal_free_groupby_tree_node(node);
          return 1;
        }
      
        node->rows = NULL;
      } else
        raptor_free_sequence(literal_seq);
      
      row->group_id = node->group_id;

      /* after this, row is owned by the sequence owned by con->tree */
      raptor_sequence_push(node->rows, row);
    }
  }

  con->group_iterator = raptor_new_avltree_iterator(con->tree,
                                                    NULL, NULL,
                                                    1);
  con->group_row_index = 0;

  con->offset = 0;

  return 0;
}


static rasqal_row*
rasqal_groupby_rowsource_read_row(rasqal_rowsource* rowsource, void *user_data)
{
  rasqal_groupby_rowsource_context* con;
  rasqal_row *row = NULL;
  rasqal_groupby_tree_node* node = NULL;

  con = (rasqal_groupby_rowsource_context*)user_data;

  /* ensure we have stored grouped rows */
  if(rasqal_groupby_rowsource_process(rowsource, con))
    return NULL;

  /* Now iterate through grouped rows */
  while(1) {
    node = (rasqal_groupby_tree_node*)raptor_avltree_iterator_get(con->group_iterator);
    if(!node) {
      /* No more nodes. finished last group and last row */
      raptor_free_avltree_iterator(con->group_iterator);
      con->group_iterator = NULL;

      raptor_free_avltree(con->tree);
      con->tree = NULL;

      /* row = NULL is already set */
      break;
    }

    row = (rasqal_row*)raptor_sequence_get_at(node->rows, 
                                              con->group_row_index++);
    if(row)
      break;
    
    /* End of sequence so reset row sequence index and advance iterator */
    con->group_row_index = 0;

    if(raptor_avltree_iterator_next(con->group_iterator))
      break;
  }

  if(node && row) {
    row->group_id = node->group_id;
    row->offset = con->offset++;
  }
  
  return row;
}


static int
rasqal_groupby_rowsource_reset(rasqal_rowsource* rowsource, void *user_data)
{
  return 0;
}


static const rasqal_rowsource_handler rasqal_groupby_rowsource_handler = {
  /* .version = */ 1,
  "groupby",
  /* .init = */ rasqal_groupby_rowsource_init,
  /* .finish = */ rasqal_groupby_rowsource_finish,
  /* .ensure_variables = */ rasqal_groupby_rowsource_ensure_variables,
  /* .read_row = */ rasqal_groupby_rowsource_read_row,
  /* .read_all_rows = */ NULL,
  /* .reset = */ rasqal_groupby_rowsource_reset,
  /* .set_preserve = */ NULL,
  /* .get_inner_rowsource = */ NULL,
  /* .set_origin = */ NULL,
};


rasqal_rowsource*
rasqal_new_groupby_rowsource(rasqal_world *world, rasqal_query* query,
                             raptor_sequence* expr_seq)
{
  rasqal_groupby_rowsource_context* con;
  int flags = 0;

  if(!world || !query)
    return NULL;
  
  con = (rasqal_groupby_rowsource_context*)RASQAL_CALLOC(rasqal_groupby_rowsource_context, 1, sizeof(*con));
  if(!con)
    return NULL;

  if(expr_seq) {
    con->expr_seq = rasqal_expression_copy_expression_sequence(expr_seq);

    if(!con->expr_seq) {
      RASQAL_FREE(rasqal_groupby_rowsource_context, con);
      return NULL;
    }

    con->expr_seq_size = raptor_sequence_size(expr_seq);
  } else
    con->expr_seq_size = 0;
  
  return rasqal_new_rowsource_from_handler(world, query,
                                           con,
                                           &rasqal_groupby_rowsource_handler,
                                           query->vars_table,
                                           flags);
}


#endif



#ifdef STANDALONE

/* one more prototype */
int main(int argc, char *argv[]);

int
main(int argc, char *argv[]) 
{
  const char *program = rasqal_basename(argv[0]);
  rasqal_rowsource *rowsource = NULL;
  rasqal_world* world = NULL;
  rasqal_query* query = NULL;
  raptor_sequence* expr_seq = NULL;
  int failures = 0;

  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    return(1);
  }
  
  query = rasqal_new_query(world, "sparql", NULL);
  
#ifdef HAVE_RAPTOR2_API
  expr_seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_expression,
                                 (raptor_data_print_handler)rasqal_expression_print);
#else
  expr_seq = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_expression,
                                 (raptor_sequence_print_handler*)rasqal_expression_print);
#endif
  
  rowsource = rasqal_new_groupby_rowsource(world, query, expr_seq);
  if(!rowsource) {
    fprintf(stderr, "%s: failed to create groupby rowsource\n", program);
    failures++;
    goto tidy;
  }


  tidy:
  if(expr_seq)
    raptor_free_sequence(expr_seq);
  if(rowsource)
    rasqal_free_rowsource(rowsource);
  if(query)
    rasqal_free_query(query);
  if(world)
    rasqal_free_world(world);

  return failures;
}

#endif

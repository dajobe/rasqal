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


#define DEBUG_FH stderr


#ifndef STANDALONE

#ifdef HAVE_RAPTOR2_API
#else

/* Fake AVL Tree implementation for Raptor V1 - using a sequence to
 *  store content and implementing just enough of the API for the
 *  grouping to work
*/

typedef int (*raptor_data_compare_handler)(const void* data1, const void* data2);
typedef void (*raptor_data_print_handler)(void *object, FILE *fh);

typedef struct {
  raptor_sequence* seq;
  raptor_data_compare_handler compare_handler;
  raptor_data_free_handler free_handler;
} raptor_avltree;

typedef struct {
  raptor_avltree* tree;
  int index;
} raptor_avltree_iterator;

static raptor_avltree* raptor_new_avltree(raptor_data_compare_handler compare_handler, raptor_data_free_handler free_handler, unsigned int flags)
{
  raptor_avltree* tree;

  tree = (raptor_avltree*)RASQAL_CALLOC(raptor_avltree, sizeof(*tree), 1);
  if(!tree)
    return NULL;

  tree->compare_handler = compare_handler;
  tree->free_handler = free_handler;

  tree->seq = raptor_new_sequence(free_handler, NULL);
  
  return tree;
}

static void
raptor_free_avltree(raptor_avltree* tree)
{
  if(!tree)
    return;
  
  if(tree->seq)
    raptor_free_sequence(tree->seq);
  RASQAL_FREE(avltree, tree);
}

static int
raptor_avltree_add(raptor_avltree* tree, void* p_data)
{
  if(!tree)
    return 1;
  
  return raptor_sequence_push(tree->seq, p_data);
}

static void*
raptor_avltree_search(raptor_avltree* tree, const void* p_data)
{
  int size;
  int i;

  if(!tree)
    return NULL;
  
  size = raptor_sequence_size(tree->seq);
  for(i = 0; i < size; i++) {
    void* data = raptor_sequence_get_at(tree->seq, i);
    if(tree->compare_handler(p_data, data))
      return data;
  }

  return NULL;
}

static void
raptor_avltree_set_print_handler(raptor_avltree* tree,
                                 raptor_data_print_handler print_handler)
{
  if(!tree)
    return;

  raptor_sequence_set_print_handler(tree->seq, print_handler);
}



static raptor_avltree_iterator*
raptor_new_avltree_iterator(raptor_avltree* tree, void* range,
                            raptor_data_free_handler range_free_handler,
                            int direction)
{
  raptor_avltree_iterator* iterator;

  iterator = (raptor_avltree_iterator*)RASQAL_CALLOC(avltree_iterator, sizeof(*iterator), 1);
  
  iterator->tree = tree;
  iterator->index = 0;

  return iterator;
}

static int
raptor_avltree_iterator_next(raptor_avltree_iterator* iterator) 
{
  iterator->index++;

  return (iterator->index > raptor_sequence_size(iterator->tree->seq));
}

static void*
raptor_avltree_iterator_get(raptor_avltree_iterator* iterator)
{
  return raptor_sequence_get_at(iterator->tree->seq, iterator->index);
}

static void
raptor_free_avltree_iterator(raptor_avltree_iterator* iterator)
{
  if(!iterator)
    return;

  RASQAL_FREE(iterator, iterator);
}

static int
raptor_avltree_print(raptor_avltree* tree, FILE* stream)
{
  if(!tree)
    return 1;
  
  fprintf(stream, "Group sequence with %d groups\n",
          raptor_sequence_size(tree->seq));
  raptor_sequence_print(tree->seq, stream);

  return 0;
}

#endif


/**
 * rasqal_groupby_rowsource_context:
 *
 * INTERNAL - GROUP BY rowsource context
 *
 * Structure for handing grouping an input rowsource by a sequence of
 * #rasqal_expression - in SPARQL, the GROUP BY exprList.
 *
 */
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

  /* non-0 if input has been processed */
  int processed;
  
  /* avltree for grouping.
   * the tree nodes are #rasqal_groupby_tree_node objects
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


/**
 * rasqal_groupby_tree_node:
 *
 * INTERNAL - Node structure for grouping rows by a sequence of literals
 *
 * Each node contains the data for one group
 *    [lit, lit, ...] -> [ row, row, row, ... ]
 *
 *  key: raptor_sequence* of rasqal_literal*
 *  value: raptor_sequence* of rasqal_row*
 *
 * Plus an integer group ID identifier.
 *
 */
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
  
  fputs("Group\n  Key Sequence of literals: ", fh);
  if(node->literals)
    /* sequence of literals */
    raptor_sequence_print(node->literals, fh);
  else
    fputs("None", fh);

  fputs("\n  Value Sequence of rows:\n", fh);
  if(node->rows) {
    int i;
    int size = raptor_sequence_size(node->rows);
    
    /* sequence of rows */
    for(i = 0; i < size; i++) {
      rasqal_row* row = (rasqal_row*)raptor_sequence_get_at(node->rows, i);
      
      fprintf(fh, "    Row %d: ", i);
      rasqal_row_print(row, fh);
      fputc('\n', fh);
    }
  } else
    fputs("None\n", fh);

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
  
  if(con->group_iterator)
    raptor_free_avltree_iterator(con->group_iterator);

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
  if(con->processed)
    return 0;

  con->processed = 1;

  /* Empty expression list - no need to read rows */
  if(!con->expr_seq || !con->expr_seq_size) {
    con->group_id++;
    return 0;
  }


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

    rasqal_row_bind_variables(row, rowsource->query->vars_table);
    
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
      key.con = con;
      key.literals = literal_seq;
      
      node = (rasqal_groupby_tree_node*)raptor_avltree_search(con->tree, &key);
      if(!node) {
        /* New Group */
        node = (rasqal_groupby_tree_node*)RASQAL_CALLOC(rasqal_groupby_tree_node, sizeof(*node), 1);
        if(!node) {
          raptor_free_sequence(literal_seq);
          return 1;
        }

        node->con = con;
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

        /* after this, node is owned by con->tree */
        raptor_avltree_add(con->tree, node);
      } else
        raptor_free_sequence(literal_seq);
      
      row->group_id = node->group_id;

      /* after this, node owns the row */
      raptor_sequence_push(node->rows, row);

    }
  }

#ifdef RASQAL_DEBUG
  if(con->tree) {
    fputs("Grouping ", DEBUG_FH);
    raptor_avltree_print(con->tree, DEBUG_FH);
    fputs("\n", DEBUG_FH);
  }
#endif
  
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

  con = (rasqal_groupby_rowsource_context*)user_data;

  /* ensure we have stored grouped rows */
  if(rasqal_groupby_rowsource_process(rowsource, con))
    return NULL;

  if(con->tree) {
    rasqal_groupby_tree_node* node = NULL;

    /* Rows were grouped so iterate through grouped rows */
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

      /* removes row from sequence and this code now owns the reference */
      row = (rasqal_row*)raptor_sequence_delete_at(node->rows, 
                                                   con->group_row_index++);
      if(row)
        break;

      /* End of sequence so reset row sequence index and advance iterator */
      con->group_row_index = 0;

      if(raptor_avltree_iterator_next(con->group_iterator))
        break;
    }

    if(node && row)
      row->group_id = node->group_id;
  } else {
    /* just pass rows through all in one group */
    row = rasqal_rowsource_read_row(con->rowsource);

    if(row)
      row->group_id = con->group_id;
  }

  if(row)
    row->offset = con->offset++;

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
                             rasqal_rowsource* rowsource,
                             raptor_sequence* expr_seq)
{
  rasqal_groupby_rowsource_context* con;
  int flags = 0;

  if(!world || !query)
    return NULL;
  
  con = (rasqal_groupby_rowsource_context*)RASQAL_CALLOC(rasqal_groupby_rowsource_context, 1, sizeof(*con));
  if(!con)
    goto fail;

  con->rowsource = rowsource;
  con->expr_seq_size = 0;

  if(expr_seq) {
    con->expr_seq = rasqal_expression_copy_expression_sequence(expr_seq);

    if(!con->expr_seq)
      goto fail;

    con->expr_seq_size = raptor_sequence_size(expr_seq);
  }
  
  return rasqal_new_rowsource_from_handler(world, query,
                                           con,
                                           &rasqal_groupby_rowsource_handler,
                                           query->vars_table,
                                           flags);

  fail:

  if(rowsource)
    rasqal_free_rowsource(rowsource);
  if(expr_seq)
    raptor_free_sequence(expr_seq);

  return NULL;
}


#endif



#ifdef STANDALONE

/* one more prototype */
int main(int argc, char *argv[]);


/*
 * Test 0 and Test 1 test the following example from SPARQL 1.1 Query Draft

"
For example, given a
   solution sequence S, ( {?x→2, ?y→3}, {?x→2, ?y→5}, {?x→6, ?y→7} ),

Group((?x), S) = {
  (2) → ( {?x→2, ?y→3}, {?x→2, ?y→5} ),
  (6) → ( {?x→6, ?y→7} )
}
"
*/


#define GROUP_TESTS_COUNT 2

#define MAX_TEST_ROWS 4
#define MAX_TEST_VARS 3

/* Test 0 and Test 1 */
static const char* const data_xy_3_rows[] =
{
  /* 2 variable names and 3 rows */
  "x",  NULL, "y",  NULL,
  /* row 1 data */
  "2",  NULL, "3",  NULL,
  /* row 2 data */
  "2",  NULL, "5",  NULL,
  /* row 3 data */
  "6",  NULL, "7",  NULL,
  /* end of data */
  NULL, NULL, NULL, NULL,
};


static const struct {
  int vars;
  int rows;
  const char* const *data;
  const int const group_ids[MAX_TEST_ROWS];
  const char* const expr_vars[MAX_TEST_VARS];
} test_data[GROUP_TESTS_COUNT] = {
  /* Test 0: No GROUP BY : 1 group expected */
  {2, 3, data_xy_3_rows, {  0,  0,  0 }, { NULL } },

  /* Test 1: GROUP BY ?x : 2 groups expected */
  {2, 3, data_xy_3_rows, {  0,  0,  1 }, { "x", NULL } }
};



int
main(int argc, char *argv[]) 
{
  const char *program = rasqal_basename(argv[0]);
  rasqal_rowsource *rowsource = NULL;
  rasqal_world* world = NULL;
  rasqal_query* query = NULL;
  raptor_sequence* row_seq = NULL;
  raptor_sequence* expr_seq = NULL;
  int failures = 0;
  rasqal_variables_table* vt;
  rasqal_rowsource *input_rs = NULL;
  int vars_count;
  raptor_sequence* vars_seq = NULL;
  int test_id;

  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    return(1);
  }
  
  query = rasqal_new_query(world, "sparql", NULL);

  vt = query->vars_table;
  
  for(test_id = 0; test_id < GROUP_TESTS_COUNT; test_id++) {
    int expected_rows_count = test_data[test_id].rows;
    int expected_vars_count = test_data[test_id].vars;
    const int* expected_group_ids = test_data[test_id].group_ids;
    raptor_sequence* seq = NULL;
    int count;
    int size;
    int i;
    
    vars_count = expected_vars_count;
    row_seq = rasqal_new_row_sequence(world, vt, test_data[test_id].data,
                                      vars_count, &vars_seq);
    if(row_seq) {
      input_rs = rasqal_new_rowsequence_rowsource(world, query, vt, 
                                                  row_seq, vars_seq);
      /* vars_seq and row_seq are now owned by input_rs */
      vars_seq = row_seq = NULL;
    }
    if(!input_rs) {
      fprintf(stderr, "%s: failed to create rowsequence rowsource\n", program);
      failures++;
      goto tidy;
    }


#ifdef HAVE_RAPTOR2_API
    expr_seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_expression,
                                   (raptor_data_print_handler)rasqal_expression_print);
#else
    expr_seq = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_expression,
                                   (raptor_sequence_print_handler*)rasqal_expression_print);
#endif

    if(test_data[test_id].expr_vars[0] != NULL) {
      int vindex;
      const unsigned char* var_name;
      
      for(vindex = 0;
          (var_name = (const unsigned char*)test_data[test_id].expr_vars[vindex] );
          vindex++) {
        rasqal_variable* v;
        rasqal_literal *l = NULL;
        rasqal_expression* e = NULL;

        v = rasqal_variables_table_get_by_name(vt, var_name);
        if(v)
          l = rasqal_new_variable_literal(world, v);

        if(l)
          e = rasqal_new_literal_expression(world, l);

        if(e)
          raptor_sequence_push(expr_seq, e);
        else {
          fprintf(stderr, "%s: failed to create variable %s\n", program,
                  (const char*)var_name);
          failures++;
          goto tidy;
        }
        
      }
    }
    
    rowsource = rasqal_new_groupby_rowsource(world, query, input_rs, expr_seq);
    /* input_rs is now owned by rowsource */
    input_rs = NULL;
   
    if(!rowsource) {
      fprintf(stderr, "%s: failed to create groupby rowsource\n", program);
      failures++;
      goto tidy;
    }

    seq = rasqal_rowsource_read_all_rows(rowsource);
    if(!seq) {
      fprintf(stderr,
              "%s: test %d rasqal_rowsource_read_all_rows() returned a NULL seq for a groupby rowsource\n",
              program, test_id);
      failures++;
      goto tidy;
    }
    count = raptor_sequence_size(seq);
    if(count != expected_rows_count) {
      fprintf(stderr,
              "%s: test %d rasqal_rowsource_read_all_rows() returned %d rows for a groupby rowsource, expected %d\n",
              program, test_id, count, expected_rows_count);
      failures++;
      goto tidy;
    }

    size = rasqal_rowsource_get_size(rowsource);
    if(size != expected_vars_count) {
      fprintf(stderr,
              "%s: test %d rasqal_rowsource_get_size() returned %d columns (variables) for a groupby rowsource, expected %d\n",
              program, test_id, size, expected_vars_count);
      failures++;
      goto tidy;
    }

    for(i = 0; i < count; i++) {
      rasqal_row* row = raptor_sequence_get_at(seq, i);

      if(row->group_id != expected_group_ids[i]) {
        fprintf(stderr, "%s: test %d row #%d has group_id %d, expected %d\n",
                program, test_id, i, row->group_id, expected_group_ids[i]);
        failures++;
        goto tidy;
      }
      
    }
    

#ifdef RASQAL_DEBUG
    rasqal_rowsource_print_row_sequence(rowsource, seq, stderr);
#endif

    raptor_free_sequence(seq); seq = NULL;

    rasqal_free_rowsource(rowsource); rowsource = NULL;

    if(expr_seq)
      raptor_free_sequence(expr_seq);
    expr_seq = NULL;
  }
  
  tidy:
  if(expr_seq)
    raptor_free_sequence(expr_seq);
  if(rowsource)
    rasqal_free_rowsource(rowsource);
  if(input_rs)
    rasqal_free_rowsource(input_rs);
  if(query)
    rasqal_free_query(query);
  if(world)
    rasqal_free_world(world);

  return failures;
}

#endif

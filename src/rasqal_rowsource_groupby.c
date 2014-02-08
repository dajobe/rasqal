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
  raptor_sequence* exprs_seq;

  /* size of above list: can be 0 if @exprs_seq is NULL too */
  int exprs_seq_size;

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


static int
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

  return 0;
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
  
  if(con->exprs_seq)
    raptor_free_sequence(con->exprs_seq);
  
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
  if(!con->exprs_seq || !con->exprs_seq_size) {
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
    
    if(con->exprs_seq) {
      raptor_sequence* literal_seq;
      rasqal_groupby_tree_node key;
      rasqal_groupby_tree_node* node;
      
      literal_seq = rasqal_expression_sequence_evaluate(rowsource->query,
                                                        con->exprs_seq,
                                                        /* ignore_errors */ 0,
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
        node = RASQAL_CALLOC(rasqal_groupby_tree_node*, 1, sizeof(*node));
        if(!node) {
          raptor_free_sequence(literal_seq);
          return 1;
        }

        node->con = con;
        node->group_id = ++con->group_id;

        /* node now owns literal_seq */
        node->literals = literal_seq;

        node->rows = raptor_new_sequence((raptor_data_free_handler)rasqal_free_row, (raptor_data_print_handler)rasqal_row_print);
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
  fputs("Grouping ", DEBUG_FH);
  raptor_avltree_print(con->tree, DEBUG_FH);
  fputs("\n", DEBUG_FH);
#endif

  if(raptor_avltree_size(con->tree))
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

  if(con->tree && con->group_iterator) {
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
      if(row) {
        /* Bind the values in the input row to the variables in the table */
        rasqal_row_bind_variables(row, rowsource->query->vars_table);
        break;
      }

      /* End of sequence so reset row sequence index and advance iterator */
      con->group_row_index = 0;

      if(raptor_avltree_iterator_next(con->group_iterator))
        break;
    }

    if(node && row)
      row->group_id = node->group_id;

  } else if(con->tree && !con->group_iterator) {
    /* we found inner rowsource with no rows - generate 1 row */
    if(!con->offset) {
      row = rasqal_new_row(rowsource);

      if(row)
        row->group_id = 0;
    }
  } else {
    /* no grouping: just pass rows through all in one group */
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


static rasqal_rowsource*
rasqal_groupby_rowsource_get_inner_rowsource(rasqal_rowsource* rowsource,
                                             void *user_data, int offset)
{
  rasqal_groupby_rowsource_context *con;
  con = (rasqal_groupby_rowsource_context*)user_data;

  if(offset == 0)
    return con->rowsource;

  return NULL;
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
  /* .set_requirements = */ NULL,
  /* .get_inner_rowsource = */ rasqal_groupby_rowsource_get_inner_rowsource,
  /* .set_origin = */ NULL,
};


/**
 * rasqal_new_groupby_rowsource:
 * @world: world object
 * @query: query object
 * @rowsource: input rowsource
 * @exprs_seq: sequence of group by expressions
 *
 * INTERNAL - create a new group by rowsource
 *
 * the @rowsource becomes owned by the new rowsource
 *
 * Return value: new rowsource or NULL on failure
 */
rasqal_rowsource*
rasqal_new_groupby_rowsource(rasqal_world *world,
                             rasqal_query* query,
                             rasqal_rowsource* rowsource,
                             raptor_sequence* exprs_seq)
{
  rasqal_groupby_rowsource_context* con;
  int flags = 0;

  if(!world || !query)
    return NULL;
  
  con = RASQAL_CALLOC(rasqal_groupby_rowsource_context*, 1, sizeof(*con));
  if(!con)
    return NULL;

  con->rowsource = rowsource;
  con->exprs_seq_size = 0;

  if(exprs_seq) {
    con->exprs_seq = rasqal_expression_copy_expression_sequence(exprs_seq);

    if(!con->exprs_seq)
      goto fail;

    con->exprs_seq_size = raptor_sequence_size(exprs_seq);
  }
  
  return rasqal_new_rowsource_from_handler(world, query,
                                           con,
                                           &rasqal_groupby_rowsource_handler,
                                           query->vars_table,
                                           flags);

  fail:

  if(rowsource)
    rasqal_free_rowsource(rowsource);
  if(exprs_seq)
    raptor_free_sequence(exprs_seq);
  if(con)
    RASQAL_FREE(rasqal_groupby_rowsource_context*, con);

  return NULL;
}


#endif /* not STANDALONE */



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


#define GROUP_TESTS_COUNT 4

#define MAX_TEST_GROUPS 100
#define MAX_TEST_VARS 5

/* Test 0 */
static const char* const data_xy_no_rows[] =
{
  /* 2 variable names and 0 rows */
  "x",  NULL, "y",  NULL,
  NULL, NULL, NULL, NULL,
};

/* Test 1 and Test 2 */
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


/* Test 3 */
static const char* const data_us_senators_100_rows[] =
{
  /* 3 variable names and 50 rows */
  "name", NULL,  "state", NULL,  "year", NULL, 
  /* row 1 data */
  "Al", NULL,  "Minnesota", NULL,  "1951", NULL, 
  "Amy", NULL,  "Minnesota", NULL,  "1960", NULL, 
  "Arlen", NULL,  "Pennsylvania", NULL,  "1930", NULL, 
  "Barbara", NULL,  "California", NULL,  "1940", NULL, 
  "Barbara", NULL,  "Maryland", NULL,  "1936", NULL, 
  "Ben", NULL,  "Maryland", NULL,  "1943", NULL, 
  "Ben", NULL,  "Nebraska", NULL,  "1941", NULL, 
  "Bernie", NULL,  "Vermont", NULL,  "1941", NULL, 
  "Bill", NULL,  "Florida", NULL,  "1942", NULL, 
  "Blanche", NULL,  "Arkansas", NULL,  "1960", NULL, 
  "Bob", NULL,  "Utah", NULL,  "1933", NULL, 
  "Bob", NULL,  "Pennsylvania", NULL,  "1960", NULL, 
  "Bob", NULL,  "Tennessee", NULL,  "1952", NULL, 
  "Bob", NULL,  "New Jersey", NULL,  "1954", NULL, 
  "Byron", NULL,  "North Dakota", NULL,  "1942", NULL, 
  "Carl", NULL,  "Michigan", NULL,  "1934", NULL, 
  "Carte", NULL,  "West Virginia", NULL,  "1974", NULL, 
  "Christopher", NULL,  "Connecticut", NULL,  "1944", NULL, 
  "Chuck", NULL,  "Iowa", NULL,  "1933", NULL, 
  "Chuck", NULL,  "New York", NULL,  "1950", NULL, 
  "Claire", NULL,  "Missouri", NULL,  "1953", NULL, 
  "Daniel", NULL,  "Hawaii", NULL,  "1924", NULL, 
  "Daniel", NULL,  "Hawaii", NULL,  "1924", NULL, 
  "David", NULL,  "Louisiana", NULL,  "1961", NULL, 
  "Debbie", NULL,  "Michigan", NULL,  "1950", NULL, 
  "Dianne", NULL,  "California", NULL,  "1933", NULL, 
  "Dick", NULL,  "Illinois", NULL,  "1944", NULL, 
  "Evan", NULL,  "Indiana", NULL,  "1955", NULL, 
  "Frank", NULL,  "New Jersey", NULL,  "1924", NULL, 
  "George", NULL,  "Florida", NULL,  "1969", NULL, 
  "George", NULL,  "Ohio", NULL,  "1936", NULL, 
  "Harry", NULL,  "Nevada", NULL,  "1939", NULL, 
  "Herb", NULL,  "Wisconsin", NULL,  "1935", NULL, 
  "Jack", NULL,  "Rhode Island", NULL,  "1949", NULL, 
  "Jay", NULL,  "West Virginia", NULL,  "1937", NULL, 
  "Jeanne", NULL,  "New Hampshire", NULL,  "1947", NULL, 
  "Jeff", NULL,  "New Mexico", NULL,  "1943", NULL, 
  "Jeff", NULL,  "Oregon", NULL,  "1956", NULL, 
  "Jeff", NULL,  "Alabama", NULL,  "1946", NULL, 
  "Jim", NULL,  "Kentucky", NULL,  "1931", NULL, 
  "Jim", NULL,  "South Carolina", NULL,  "1951", NULL, 
  "Jim", NULL,  "Oklahoma", NULL,  "1934", NULL, 
  "Jim", NULL,  "Idaho", NULL,  "1943", NULL, 
  "Jim", NULL,  "Virginia", NULL,  "1946", NULL, 
  "Joe", NULL,  "Connecticut", NULL,  "1942", NULL, 
  "John", NULL,  "Wyoming", NULL,  "1952", NULL, 
  "John", NULL,  "Texas", NULL,  "1952", NULL, 
  "John", NULL,  "Nevada", NULL,  "1958", NULL, 
  "John", NULL,  "Massachusetts", NULL,  "1943", NULL, 
  "John", NULL,  "Arizona", NULL,  "1936", NULL, 
  "John", NULL,  "South Dakota", NULL,  "1961", NULL, 
  "Johnny", NULL,  "Georgia", NULL,  "1944", NULL, 
  "Jon", NULL,  "Arizona", NULL,  "1942", NULL, 
  "Jon", NULL,  "Montana", NULL,  "1956", NULL, 
  "Judd", NULL,  "New Hampshire", NULL,  "1947", NULL, 
  "Kay", NULL,  "Texas", NULL,  "1943", NULL, 
  "Kay", NULL,  "North Carolina", NULL,  "1953", NULL, 
  "Kent", NULL,  "North Dakota", NULL,  "1948", NULL, 
  "Kirsten", NULL,  "New York", NULL,  "1966", NULL, 
  "Kit", NULL,  "Missouri", NULL,  "1939", NULL, 
  "Lamar", NULL,  "Tennessee", NULL,  "1940", NULL, 
  "Lindsey", NULL,  "South Carolina", NULL,  "1955", NULL, 
  "Lisa", NULL,  "Alaska", NULL,  "1957", NULL, 
  "Maria", NULL,  "Washington", NULL,  "1958", NULL, 
  "Mark", NULL,  "Alaska", NULL,  "1962", NULL, 
  "Mark", NULL,  "Arkansas", NULL,  "1963", NULL, 
  "Mark", NULL,  "Colorado", NULL,  "1950", NULL, 
  "Mark", NULL,  "Virginia", NULL,  "1954", NULL, 
  "Mary", NULL,  "Louisiana", NULL,  "1955", NULL, 
  "Max", NULL,  "Montana", NULL,  "1941", NULL, 
  "Michael", NULL,  "Colorado", NULL,  "1964", NULL, 
  "Mike", NULL,  "Idaho", NULL,  "1951", NULL, 
  "Mike", NULL,  "Wyoming", NULL,  "1944", NULL, 
  "Mike", NULL,  "Nebraska", NULL,  "1950", NULL, 
  "Mitch", NULL,  "Kentucky", NULL,  "1942", NULL, 
  "Olympia", NULL,  "Maine", NULL,  "1947", NULL, 
  "Orrin", NULL,  "Utah", NULL,  "1934", NULL, 
  "Pat", NULL,  "Kansas", NULL,  "1936", NULL, 
  "Patrick", NULL,  "Vermont", NULL,  "1940", NULL, 
  "Patty", NULL,  "Washington", NULL,  "1950", NULL, 
  "Richard", NULL,  "North Carolina", NULL,  "1955", NULL, 
  "Richard", NULL,  "Indiana", NULL,  "1932", NULL, 
  "Richard", NULL,  "Alabama", NULL,  "1934", NULL, 
  "Roger", NULL,  "Mississippi", NULL,  "1951", NULL, 
  "Roland", NULL,  "Illinois", NULL,  "1937", NULL, 
  "Ron", NULL,  "Oregon", NULL,  "1949", NULL, 
  "Russ", NULL,  "Wisconsin", NULL,  "1953", NULL, 
  "Sam", NULL,  "Kansas", NULL,  "1956", NULL, 
  "Saxby", NULL,  "Georgia", NULL,  "1943", NULL, 
  "Scott", NULL,  "Massachusetts", NULL,  "1959", NULL, 
  "Sheldon", NULL,  "Rhode Island", NULL,  "1955", NULL, 
  "Sherrod", NULL,  "Ohio", NULL,  "1952", NULL, 
  "Susan", NULL,  "Maine", NULL,  "1952", NULL, 
  "Ted", NULL,  "Delaware", NULL,  "1939", NULL, 
  "Thad", NULL,  "Mississippi", NULL,  "1937", NULL, 
  "Tim", NULL,  "South Dakota", NULL,  "1946", NULL, 
  "Tom", NULL,  "Delaware", NULL,  "1947", NULL, 
  "Tom", NULL,  "Oklahoma", NULL,  "1948", NULL, 
  "Tom", NULL,  "Iowa", NULL,  "1939", NULL, 
  "Tom", NULL,  "New Mexico", NULL,  "1948", NULL, 
  /* end of data */
  NULL, NULL, NULL, NULL, NULL, NULL,
};


/* Group IDs expected */
/* Test 0 */
static const int test0_groupids[] = {
  0
};

/* Test 1 */
static const int test1_groupids[] = {
  0, 0, 0 
};

/* Test 2 */
static const int test2_groupids[] = {
  0, 0, 1 
};

  
/* Raptor AVL Tree - Enumerated by order in AVL Tree which is sorted by expression list */
static const int results_us_senators_97_groups[] =
  { 21, 21, 27, 2, 38, 79, 10, 18, 24, 15, 40, 74, 80, 31, 4, 29, 47, 75, 33, 82, 92, 30, 57, 91, 96, 3, 58, 76, 6, 7, 67, 8, 14, 43, 50, 72, 5, 35, 41, 46, 53, 86, 17, 25, 49, 70, 37, 42, 93, 34, 52, 73, 94, 55, 95, 95, 32, 83, 19, 23, 64, 71, 77, 0, 39, 69, 81, 12, 44, 44, 89, 90, 20, 54, 84, 13, 65, 26, 59, 66, 78, 88, 36, 51, 85, 60, 45, 61, 87, 1, 9, 11, 22, 48, 62, 63, 68, 56, 28, 16 };



static const struct {
  int vars;
  int rows;
  int ngroups;
  const char* const *data;
  const int *group_ids;
  const char* const expr_vars[MAX_TEST_VARS];
} test_data[GROUP_TESTS_COUNT] = {
  /* Test 0: No GROUP BY : 1 group expected with NULL values */
  {2, 1, 1, data_xy_no_rows, test0_groupids, { "x", NULL } },

  /* Test 1: No GROUP BY : 1 group expected */
  {2, 3, 1, data_xy_3_rows, test1_groupids, { NULL } },

  /* Test 2: GROUP BY ?x : 2 groups expected */
  {2, 3, 2, data_xy_3_rows, test2_groupids, { "x", NULL } },

  /* Test 3: GROUP BY ?year, ?name : 97 groups expected */
  {3, 100, 97, data_us_senators_100_rows, results_us_senators_97_groups, { "year", "name", NULL } },
   
};



int
main(int argc, char *argv[]) 
{
  const char *program = rasqal_basename(argv[0]);
  rasqal_rowsource *rowsource = NULL;
  rasqal_world* world = NULL;
  rasqal_query* query = NULL;
  raptor_sequence* row_seq = NULL;
  raptor_sequence* exprs_seq = NULL;
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
    int expected_ngroups = test_data[test_id].ngroups;
    raptor_sequence* seq = NULL;
    int count;
    int size;
    int i;
    int groups_counted;
    int last_group_id;
    
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


    exprs_seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_expression,
                                   (raptor_data_print_handler)rasqal_expression_print);

    if(test_data[test_id].expr_vars[0] != NULL) {
      int vindex;
      const unsigned char* var_name;
      
      for(vindex = 0;
          (var_name = RASQAL_GOOD_CAST(const unsigned char*, test_data[test_id].expr_vars[vindex] ));
          vindex++) {
        rasqal_variable* v;
        rasqal_literal *l = NULL;
        rasqal_expression* e = NULL;

        v = rasqal_variables_table_get_by_name(vt, RASQAL_VARIABLE_TYPE_NORMAL,
                                               var_name);
        /* returns SHARED pointer to variable */
        if(v) {
          v = rasqal_new_variable_from_variable(v);
          l = rasqal_new_variable_literal(world, v);
        }

        if(l)
          e = rasqal_new_literal_expression(world, l);

        if(e)
          raptor_sequence_push(exprs_seq, e);
        else {
          fprintf(stderr, "%s: failed to create variable %s\n", program,
                  RASQAL_GOOD_CAST(const char*, var_name));
          failures++;
          goto tidy;
        }
        
      }
    }
    
    rowsource = rasqal_new_groupby_rowsource(world, query, input_rs, exprs_seq);
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

    groups_counted = 0;
    last_group_id = -1;
    for(i = 0; i < count; i++) {
      rasqal_row* row = (rasqal_row*)raptor_sequence_get_at(seq, i);

      if(row->group_id != last_group_id) {
        groups_counted++;
        last_group_id = row->group_id;
      }

      if(row->group_id != expected_group_ids[i]) {
        fprintf(stderr, "%s: test %d row #%d has group_id %d, expected %d\n",
                program, test_id, i, row->group_id, expected_group_ids[i]);
        failures++;
        goto tidy;
      }
      
    }
    
    if(groups_counted != expected_ngroups) {
        fprintf(stderr, "%s: test %d returnd %d groups, expected %d\n",
                program, test_id, groups_counted, expected_ngroups);
        failures++;
        goto tidy;
      }

#ifdef RASQAL_DEBUG
    rasqal_rowsource_print_row_sequence(rowsource, seq, stderr);
#endif

    raptor_free_sequence(seq); seq = NULL;

    rasqal_free_rowsource(rowsource); rowsource = NULL;

    if(exprs_seq)
      raptor_free_sequence(exprs_seq);
    exprs_seq = NULL;
  }
  
  tidy:
  if(exprs_seq)
    raptor_free_sequence(exprs_seq);
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

#endif /* STANDALONE */

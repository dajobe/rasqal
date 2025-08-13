/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_expr_unbound_test.c - Tests for expression unbound handling
 *
 * This exercises behavior for unbound operands across operators.
 * It currently asserts behavior that is already implemented, and
 * marks not-yet-consistent cases as SKIP pending unification work.
 */

#ifdef HAVE_CONFIG_H
#include <rasqal_config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "rasqal.h"
#include "rasqal_internal.h"

static int verbose = 1;

static void
print_result_prefix(const char* name)
{
  if(verbose)
    printf("%s: ", name);
}

static int
expect_bool(rasqal_literal* l, int expected)
{
  if(!l)
    return 1; /* failure */
  /* boolean is an internal literal type; check directly */
  if(l->type != RASQAL_LITERAL_BOOLEAN)
    return 1;
  return ((l->value.integer != 0) != (expected != 0));
}

/* Helper to create a variable and its literal expression */
static rasqal_expression*
create_variable_expression(rasqal_world* world, rasqal_query* query, 
                          const char* var_name, size_t var_len)
{
  rasqal_variable* v;
  rasqal_literal* lit_var;
  rasqal_expression* ex_var;

  v = rasqal_variables_table_add2(query->vars_table, RASQAL_VARIABLE_TYPE_NORMAL,
                                  (const unsigned char*)var_name, var_len, NULL);
  lit_var = rasqal_new_variable_literal(world, v);
  ex_var = rasqal_new_literal_expression(world, lit_var);
  return ex_var;
}

/* Helper to create a constant integer literal expression */
static rasqal_expression*
create_integer_expression(rasqal_world* world, int value)
{
  rasqal_literal* lit;
  rasqal_expression* ex;

  lit = rasqal_new_integer_literal(world, RASQAL_LITERAL_INTEGER, value);
  ex = rasqal_new_literal_expression(world, lit);
  return ex;
}

/* Helper to create a constant string literal expression */
static rasqal_expression*
create_string_expression(rasqal_world* world, const char* str)
{
  rasqal_literal* lit;
  rasqal_expression* ex;
  unsigned char* s;

  size_t len = strlen(str);
  s = RASQAL_MALLOC(unsigned char*, len + 1);
  if(s) {
    memcpy(s, str, len + 1);
  }
  lit = rasqal_new_string_literal(world, s, NULL, NULL, NULL);
  ex = rasqal_new_literal_expression(world, lit);
  return ex;
}

/* Helper to initialize evaluation context */
static void
init_eval_context(rasqal_evaluation_context* ctx, rasqal_world* world, rasqal_query* query)
{
  memset(ctx, 0, sizeof(*ctx));
  ctx->world = world;
  ctx->query = query;
}

/* Helper to test binary operators with unbound variable */
static int
test_binary_ops_unbound(rasqal_world* world, rasqal_query* query,
                        const char* test_name, rasqal_op* ops, size_t num_ops,
                        rasqal_expression* var_expr, rasqal_expression* const_expr,
                        int expect_null, int expected_bool)
{
  size_t i;
  int failures = 0;
  rasqal_expression* left;
  rasqal_expression* right;
  rasqal_expression* e;
  rasqal_evaluation_context ctx;
  int err;
  rasqal_literal* r;

  print_result_prefix(test_name);

  for(i = 0; i < num_ops; i++) {
    left = rasqal_new_expression_from_expression(var_expr);
    right = rasqal_new_expression_from_expression(const_expr);
    e = rasqal_new_2op_expression(world, ops[i], left, right);

    err = 0;
    init_eval_context(&ctx, world, query);
    r = rasqal_expression_evaluate2(e, &ctx, &err);

    if(expect_null) {
      if(r) {
        failures++;
        rasqal_free_literal(r);
      }
    } else {
      if(expect_bool(r, expected_bool))
        failures++;
      if(r)
        rasqal_free_literal(r);
    }
    rasqal_free_expression(e);
  }

  if(failures == 0) {
    if(verbose)
      printf("PASS\n");
    return 0;
  }
  if(verbose)
    printf("FAIL (%d)\n", failures);
  return 1;
}

/* Helper to test unary operators with unbound variable */
static int
test_unary_ops_unbound(rasqal_world* world, rasqal_query* query,
                       const char* test_name, rasqal_op* ops, size_t num_ops,
                       rasqal_expression* var_expr, int expect_null)
{
  size_t i;
  int failures = 0;
  rasqal_expression* arg;
  rasqal_expression* e;
  rasqal_evaluation_context ctx;
  int err;
  rasqal_literal* r;

  print_result_prefix(test_name);

  for(i = 0; i < num_ops; i++) {
    arg = rasqal_new_expression_from_expression(var_expr);
    e = rasqal_new_1op_expression(world, ops[i], arg);

    err = 0;
    init_eval_context(&ctx, world, query);
    r = rasqal_expression_evaluate2(e, &ctx, &err);

    if(expect_null) {
      if(r) {
        failures++;
        rasqal_free_literal(r);
      }
    }
    rasqal_free_expression(e);
  }

  if(failures == 0) {
    if(verbose)
      printf("PASS\n");
    return 0;
  }
  if(verbose)
    printf("FAIL (%d)\n", failures);
  return 1;
}

/* Helper to test specific expressions with unbound variable */
static int
test_specific_expressions_unbound(rasqal_world* world, rasqal_query* query,
                                 const char* test_name, 
                                 rasqal_expression* var_expr, rasqal_expression* const_expr,
                                 int expected_bool)
{
  int failures = 0;
  rasqal_expression* e1;
  rasqal_expression* e2;
  rasqal_evaluation_context ctx;
  int err;
  rasqal_literal* r;

  print_result_prefix(test_name);

  e1 = rasqal_new_2op_expression(world, RASQAL_EXPR_STR_EQ,
                                  rasqal_new_expression_from_expression(var_expr),
                                  rasqal_new_expression_from_expression(const_expr));
  e2 = rasqal_new_2op_expression(world, RASQAL_EXPR_STR_NEQ,
                                  rasqal_new_expression_from_expression(var_expr),
                                  rasqal_new_expression_from_expression(const_expr));

  init_eval_context(&ctx, world, query);

  err = 0;
  r = rasqal_expression_evaluate2(e1, &ctx, &err);
  if(expect_bool(r, expected_bool))
    failures++;
  if(r)
    rasqal_free_literal(r);

  err = 0;
  r = rasqal_expression_evaluate2(e2, &ctx, &err);
  if(expect_bool(r, expected_bool))
    failures++;
  if(r)
    rasqal_free_literal(r);

  rasqal_free_expression(e1);
  rasqal_free_expression(e2);

  if(failures == 0) {
    if(verbose)
      printf("PASS\n");
    return 0;
  }
  if(verbose)
    printf("FAIL (%d)\n", failures);
  return 1;
}

/* Helper to test IN/NOT IN expressions with unbound variable */
static int
test_in_expressions_unbound(rasqal_world* world, rasqal_query* query,
                           const char* test_name, 
                           rasqal_expression* var_expr, rasqal_expression* const_expr,
                           int expected_bool)
{
  int failures = 0;
  raptor_sequence* args;
  rasqal_expression* in_e;
  rasqal_evaluation_context ctx;
  int err;
  rasqal_literal* r;
  raptor_sequence* args2;
  rasqal_expression* notin_e;

  print_result_prefix(test_name);

  args = raptor_new_sequence((raptor_data_free_handler)rasqal_free_expression, NULL);
  raptor_sequence_push(args, rasqal_new_expression_from_expression(const_expr));

  in_e = rasqal_new_set_expression(world, RASQAL_EXPR_IN,
                                   rasqal_new_expression_from_expression(var_expr), args);

  init_eval_context(&ctx, world, query);

  err = 0;
  r = rasqal_expression_evaluate2(in_e, &ctx, &err);
  if(expect_bool(r, expected_bool))
    failures++;
  if(r)
    rasqal_free_literal(r);
  rasqal_free_expression(in_e);

  /* fresh clones for NOT IN */
  args2 = raptor_new_sequence((raptor_data_free_handler)rasqal_free_expression, NULL);
  raptor_sequence_push(args2, rasqal_new_expression_from_expression(const_expr));
  notin_e = rasqal_new_set_expression(world, RASQAL_EXPR_NOT_IN,
                                      rasqal_new_expression_from_expression(var_expr), args2);

  r = rasqal_expression_evaluate2(notin_e, &ctx, &err);
  if(expect_bool(r, expected_bool))
    failures++;
  if(r)
    rasqal_free_literal(r);

  rasqal_free_expression(notin_e);

  if(failures == 0) {
    if(verbose)
      printf("PASS\n");
    return 0;
  }
  if(verbose)
    printf("FAIL (%d)\n", failures);
  return 1;
}

static int
test_numeric_comparisons_false_on_unbound(rasqal_world* world, rasqal_query* query)
{
  const char* name = "Numeric comparisons FALSE on unbound";
  rasqal_op ops[] = { RASQAL_EXPR_EQ, RASQAL_EXPR_NEQ, RASQAL_EXPR_LT, RASQAL_EXPR_LE, RASQAL_EXPR_GT, RASQAL_EXPR_GE };
  rasqal_expression* var_expr;
  rasqal_expression* const_expr;
  int result;

  var_expr = create_variable_expression(world, query, "x", 1);
  const_expr = create_integer_expression(world, 1);

  result = test_binary_ops_unbound(world, query, name, ops, 
                                   sizeof(ops)/sizeof(ops[0]), 
                                   var_expr, const_expr, 0, 0);

  rasqal_free_expression(var_expr);
  rasqal_free_expression(const_expr);

  return result;
}

static int
test_arithmetic_null_on_unbound(rasqal_world* world, rasqal_query* query)
{
  const char* name = "Arithmetic NULL on unbound";
  rasqal_op ops[] = { RASQAL_EXPR_PLUS, RASQAL_EXPR_MINUS, RASQAL_EXPR_STAR, RASQAL_EXPR_SLASH, RASQAL_EXPR_REM };
  rasqal_expression* var_expr;
  rasqal_expression* const_expr;
  int result;

  var_expr = create_variable_expression(world, query, "y", 1);
  const_expr = create_integer_expression(world, 2);

  /* Test binary arithmetic operators */
  result = test_binary_ops_unbound(world, query, name, ops, 
                                   sizeof(ops)/sizeof(ops[0]), 
                                   var_expr, const_expr, 1, 0);

  /* Test unary minus */
  if(result == 0) {
    rasqal_op unary_ops[] = { RASQAL_EXPR_UMINUS };
    result = test_unary_ops_unbound(world, query, name, unary_ops, 1, var_expr, 1);
  }

  rasqal_free_expression(var_expr);
  rasqal_free_expression(const_expr);

  return result;
}

static int
test_string_transforms_null_on_unbound(rasqal_world* world, rasqal_query* query)
{
  const char* name = "String transforms NULL on unbound";
  rasqal_op ops[] = { RASQAL_EXPR_STRLEN, RASQAL_EXPR_UCASE, RASQAL_EXPR_LCASE };
  rasqal_expression* var_expr;
  int result;

  var_expr = create_variable_expression(world, query, "z", 1);

  result = test_unary_ops_unbound(world, query, name, ops, 
                                  sizeof(ops)/sizeof(ops[0]), 
                                  var_expr, 1);

  rasqal_free_expression(var_expr);

  return result;
}

static int
test_string_comparisons_false_on_unbound(rasqal_world* world, rasqal_query* query)
{
  const char* name = "String comparisons FALSE on unbound";
  rasqal_expression* var_expr;
  rasqal_expression* const_expr;
  int result;

  var_expr = create_variable_expression(world, query, "sc", 2);
  const_expr = create_string_expression(world, "x");

  result = test_specific_expressions_unbound(world, query, name, var_expr, const_expr, 0);

  rasqal_free_expression(var_expr);
  rasqal_free_expression(const_expr);

  return result;
}

static int
test_string_predicates_false_on_unbound(rasqal_world* world, rasqal_query* query)
{
  const char* name = "String predicates FALSE on unbound";
  rasqal_op ops[] = { RASQAL_EXPR_STRSTARTS, RASQAL_EXPR_STRENDS, RASQAL_EXPR_CONTAINS };
  rasqal_expression* var_expr;
  rasqal_expression* const_expr;
  int result;

  var_expr = create_variable_expression(world, query, "sp", 2);
  const_expr = create_string_expression(world, "x");

  result = test_binary_ops_unbound(world, query, name, ops, 
                                   sizeof(ops)/sizeof(ops[0]), 
                                   var_expr, const_expr, 0, 0);

  rasqal_free_expression(var_expr);
  rasqal_free_expression(const_expr);

  return result;
}

static int
test_regex_false_on_unbound(rasqal_world* world, rasqal_query* query)
{
  const char* name = "Regex FALSE on unbound";
  rasqal_op ops[] = { RASQAL_EXPR_REGEX };
  rasqal_expression* var_expr;
  rasqal_expression* const_expr;
  int result;

  var_expr = create_variable_expression(world, query, "rg", 2);
  const_expr = create_string_expression(world, "x");

  result = test_binary_ops_unbound(world, query, name, ops, 1, 
                                   var_expr, const_expr, 0, 0);

  rasqal_free_expression(var_expr);
  rasqal_free_expression(const_expr);

  return result;
}

static int
test_in_notin_false_on_unbound(rasqal_world* world, rasqal_query* query)
{
  const char* name = "IN/NOT IN FALSE on unbound";
  rasqal_expression* var_expr;
  rasqal_expression* const_expr;
  int result;

  var_expr = create_variable_expression(world, query, "in", 2);
  const_expr = create_integer_expression(world, 1);

  result = test_in_expressions_unbound(world, query, name, var_expr, const_expr, 0);

  rasqal_free_expression(var_expr);
  rasqal_free_expression(const_expr);

  return result;
}

static int
test_sameterm_false_on_unbound(rasqal_world* world, rasqal_query* query)
{
  const char* name = "SAME TERM FALSE on unbound";
  rasqal_op ops[] = { RASQAL_EXPR_SAMETERM };
  rasqal_expression* var_expr;
  rasqal_expression* const_expr;
  int result;

  var_expr = create_variable_expression(world, query, "st", 2);
  const_expr = create_integer_expression(world, 1);

  result = test_binary_ops_unbound(world, query, name, ops, 1, 
                                   var_expr, const_expr, 0, 0);

  rasqal_free_expression(var_expr);
  rasqal_free_expression(const_expr);

  return result;
}

/* no skipped tests */

int
main(int argc, char *argv[])
{
  const char *program = rasqal_basename(argv[0]);
  int failures = 0;
  int i;
  rasqal_world* world;
  rasqal_query* query;

  for(i = 1; i < argc; i++) {
    if(!strcmp(argv[i], "-q") || !strcmp(argv[i], "--quiet"))
      verbose = 0;
  }

  world = rasqal_new_world();
  if(!world || rasqal_world_open(world)) {
    fprintf(stderr, "%s: rasqal_world init failed\n", program);
    return 1;
  }

  query = rasqal_new_query(world, "sparql", NULL);
  if(!query) {
    fprintf(stderr, "%s: query init failed\n", program);
    rasqal_free_world(world);
    return 1;
  }

  if(verbose)
    printf("%s: Testing expression unbound handling\n", program);

  failures += test_numeric_comparisons_false_on_unbound(world, query);
  failures += test_arithmetic_null_on_unbound(world, query);
  failures += test_string_transforms_null_on_unbound(world, query);
  failures += test_string_comparisons_false_on_unbound(world, query);
  failures += test_string_predicates_false_on_unbound(world, query);
  failures += test_regex_false_on_unbound(world, query);
  failures += test_in_notin_false_on_unbound(world, query);
  failures += test_sameterm_false_on_unbound(world, query);

  rasqal_free_query(query);
  rasqal_free_world(world);

  if(verbose) {
    if(failures)
      printf("%s: %d test%s FAILED\n", program, failures, failures == 1 ? "" : "s");
    else
      printf("%s: All tests PASSED\n", program);
  }

  return failures;
}



/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * sparql_parser.y - Rasqal SPARQL parser over tokens from sparql_lexer.l
 *
 * $Id$
 *
 * Copyright (C) 2004, David Beckett http://purl.org/net/dajobe/
 * Institute for Learning and Research Technology http://www.ilrt.bristol.ac.uk/
 * University of Bristol, UK http://www.bristol.ac.uk/
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
 * References:
 *   SPARQL Query Language for RDF
 *   http://www.w3.org/TR/rdf-sparql-query/
 *
 * Editor's draft of above http://www.w3.org/2001/sw/DataAccess/rq23/
 *
 */

%{
#ifdef HAVE_CONFIG_H
#include <rasqal_config.h>
#endif

#ifdef WIN32
#include <win32_rasqal_config.h>
#endif

#include <stdio.h>
#include <stdarg.h>

#include <rasqal.h>
#include <rasqal_internal.h>

#include <sparql_parser.h>

#define YY_DECL int sparql_lexer_lex (YYSTYPE *sparql_parser_lval, yyscan_t yyscanner)
#include <sparql_lexer.h>

#include <sparql_common.h>

#undef RASQAL_DEBUG
#define RASQAL_DEBUG 2

/* Make verbose error messages for syntax errors */
/*
#ifdef RASQAL_DEBUG
#define YYERROR_VERBOSE 1
#endif
*/
#define YYERROR_VERBOSE 1

/* Slow down the grammar operation and watch it work */
#if RASQAL_DEBUG > 2
#define YYDEBUG 1
#endif

/* the lexer does not seem to track this */
#undef RASQAL_SPARQL_USE_ERROR_COLUMNS

/* Missing sparql_lexer.c/h prototypes */
int sparql_lexer_get_column(yyscan_t yyscanner);
/* Not used here */
/* void sparql_lexer_set_column(int  column_no , yyscan_t yyscanner);*/


/* What the lexer wants */
extern int sparql_lexer_lex (YYSTYPE *sparql_parser_lval, yyscan_t scanner);
#define YYLEX_PARAM ((rasqal_sparql_query_engine*)(((rasqal_query*)rq)->context))->scanner

/* Pure parser argument (a void*) */
#define YYPARSE_PARAM rq

/* Make the yyerror below use the rdf_parser */
#undef yyerror
#define yyerror(message) sparql_query_error((rasqal_query*)rq, message)

/* Make lex/yacc interface as small as possible */
#undef yylex
#define yylex sparql_lexer_lex


static int sparql_parse(rasqal_query* rq, const unsigned char *string);
static void sparql_query_error(rasqal_query* rq, const char *message);
static void sparql_query_error_full(rasqal_query *rq, const char *message, ...);
%}


/* directives */


%pure-parser


/* Interface between lexer and parser */
%union {
  raptor_sequence *seq;
  rasqal_variable *variable;
  rasqal_literal *literal;
  rasqal_triple *triple;
  rasqal_expression *expr;
  rasqal_graph_pattern *graph_pattern;
  double floating;
  raptor_uri *uri;
  unsigned char *name;
}


/*
 * 1 shift/reduce conflicts
 */
%expect 1


/* word symbols */
%token SELECT SOURCE FROM WHERE AND
%token OPTIONAL PREFIX DESCRIBE CONSTRUCT ASK NOT DISTINCT LIMIT UNION

/* expression delimitors */

%token ',' '(' ')' '[' ']' '{' '}'
%token '?' '$'

/* function call indicator */
%token '&'

/* SC booleans */
%left SC_OR SC_AND

/* string operations */
%left STR_EQ STR_NE STR_MATCH STR_NMATCH

/* operations */
%left EQ NEQ LT GT LE GE

/* arithmetic operations */
%left '+' '-' '*' '/' '%'

/* unary operations */
%left '~' '!'

/* literals */
%token <literal> FLOATING_POINT_LITERAL
%token <literal> STRING_LITERAL PATTERN_LITERAL INTEGER_LITERAL
%token <literal> BOOLEAN_LITERAL
%token <literal> NULL_LITERAL
%token <uri> URI_LITERAL
%token <name> QNAME_LITERAL

%token <name> IDENTIFIER

/* syntax error */
%token ERROR_TOKEN


%type <seq> SelectClause ConstructClause DescribeClause
%type <seq> PrefixDeclOpt FromClauseOpt WhereClauseOpt
%type <seq> VarList VarOrURIList ArgList URIList
%type <seq> TriplePatternList

%type <seq> GraphPattern 

%type <graph_pattern> GraphPattern1 PatternElement PatternElementForms

%type <expr> Expression ConditionalAndExpression ValueLogical
%type <expr> EqualityExpression RelationalExpression AdditiveExpression
%type <expr> MultiplicativeExpression UnaryExpression
%type <expr> UnaryExpressionNotPlusMinus

%type <literal> Literal URI VarOrLiteral VarOrURI

%type <variable> Var

%type <triple> TriplePattern


%%


/* SPARQL Grammar: [1] Query*/
Query : PrefixDeclOpt ReportFormat PrefixDeclOpt FromClauseOpt WhereClauseOpt
{
}
;


/* SPARQL Grammar: [2] ReportFormat */
ReportFormat : SELECT SelectClause
{
  ((rasqal_query*)rq)->selects=$2;
}
|  CONSTRUCT ConstructClause
{
  ((rasqal_query*)rq)->constructs=$2;
}
|  DESCRIBE DescribeClause
{
  ((rasqal_query*)rq)->select_is_describe=1;
  ((rasqal_query*)rq)->describes=$2;
}
| ASK
{
  ((rasqal_query*)rq)->ask=1;
}
;


/* NEW Grammar Term */
SelectClause : DISTINCT VarList
{
  $$=$2;
  ((rasqal_query*)rq)->distinct=1;
}
| DISTINCT '*'
{
  $$=NULL;
  ((rasqal_query*)rq)->select_all=1;
  ((rasqal_query*)rq)->distinct=1;
}
| VarList
{
  $$=$1;
}
| '*'
{
  $$=NULL;
  ((rasqal_query*)rq)->select_all=1;
}
;


/* NEW Grammar Term */
ConstructClause : TriplePatternList
{
  $$=$1;
}
| '*'
{
  $$=NULL;
  ((rasqal_query*)rq)->construct_all=1;
}
;


/* NEW Grammar Term */
DescribeClause : VarOrURIList
{
  $$=$1;
}
| '*'
{
  $$=NULL;
}
;


/* SPARQL Grammar: [3] FromClause - renamed for clarity */
FromClauseOpt : FROM URIList
{
  ((rasqal_query*)rq)->sources=$2;
}
| /* empty */
{
}
;

/* SPARQL Grammar: [4] FromSelector - junk */

/* SPARQL Grammar: [5] WhereClause - remained for clarity*/
WhereClauseOpt :  WHERE GraphPattern
{
  ((rasqal_query*)rq)->graph_patterns=$2;
}
| /* empty */
{
}
;


/* SPARQL Grammar: [6] SourceGraphPattern - merged into PatternElementForms */

/* SPARQL Grammar: [7] OptionalGraphPattern - merged into PatternElementForms */

/* SPARQL Grammar: [8] GraphPattern */
GraphPattern : GraphPattern PatternElement
{
#if RASQAL_DEBUG > 1  
  printf("GraphPattern 1\n  graphpattern=");
  raptor_sequence_print($1, stdout);
  printf(", patternelement=");
  if($2)
    rasqal_graph_pattern_print($2, stdout);
  else
    fputs("NULL", stdout);
  fputs("\n\n", stdout);
#endif

  $$=$1;
  if($2)
    raptor_sequence_push($$, $2);
}
| PatternElement
{
#if RASQAL_DEBUG > 1  
  printf("GraphPattern 2\n  patternelement=");
  if($1)
    rasqal_graph_pattern_print($1, stdout);
  else
    fputs("NULL", stdout);
  fputs("\n\n", stdout);
#endif

  $$=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_graph_pattern, (raptor_sequence_print_handler*)rasqal_graph_pattern_print);
  if($1)
    raptor_sequence_push($$, $1);
}
;


/* SPARQL Grammar: [9] PatternElement */
PatternElement : TriplePatternList
{
#if RASQAL_DEBUG > 1  
  printf("PatternElement 1\n  triplepatternlist=");
  raptor_sequence_print($1, stdout);
  fputs("\n\n", stdout);
#endif
  
  if($1) {
    raptor_sequence *s=((rasqal_query*)rq)->triples;
    int offset=raptor_sequence_size(s);
    int triple_pattern_size=raptor_sequence_size($1);
    
    raptor_sequence_join(s, $1);
    raptor_free_sequence($1);

    $$=rasqal_new_graph_pattern_from_triples(s, offset, offset+triple_pattern_size-1, 0);
  } else
    $$=NULL;
}
| '{' GraphPattern '}' /*  ExplicitGroup inlined */
{
#if RASQAL_DEBUG > 1  
  printf("PatternElement 2\n  graphpattern=");
  raptor_sequence_print($2, stdout);
  fputs("\n\n", stdout);
#endif
  
  $$=rasqal_new_graph_pattern_from_sequence($2, 0);
}
| PatternElementForms
{
#if RASQAL_DEBUG > 1  
  printf("PatternElement 3\n  patternelementforms=");
  if($1)
    rasqal_graph_pattern_print($1, stdout);
  else
    fputs("NULL", stdout);
  fputs("\n\n", stdout);
#endif
  
  $$=$1;
}
;


/* SPARQL Grammar: [10] GraphPattern1 */
GraphPattern1 : TriplePattern
{
#if RASQAL_DEBUG > 1  
  printf("GraphPattern1 1\n  triplepattern=");
  if($1)
    rasqal_triple_print($1, stdout);
  else
    fputs("NULL", stdout);
  fputs("\n\n", stdout);
#endif

  if($1) {
    raptor_sequence *s=((rasqal_query*)rq)->triples;
    int offset=raptor_sequence_size(s);
    raptor_sequence_push(s, $1);

    $$=rasqal_new_graph_pattern_from_triples(s, offset, offset, 0);
  } else
    $$=NULL;
}
| '{' GraphPattern '}' /*  ExplicitGroup inlined */
{
#if RASQAL_DEBUG > 1  
  printf("GraphPattern1 2\n  triplepattern=");
  raptor_sequence_print($2, stdout);
  fputs("\n\n", stdout);
#endif

  $$=rasqal_new_graph_pattern_from_sequence($2, 0);
}
| PatternElementForms
{
#if RASQAL_DEBUG > 1  
  printf("PatternElement 3\n  patternelementforms=");
  rasqal_graph_pattern_print($1, stdout);
  fputs("\n\n", stdout);
#endif
  
  $$=$1;
}
;


/* SPARQL Grammar: [11] PatternElement1 - merged into GraphPattern1 */

/* SPARQL Grammar: [12] PatternElementForms */

/* This inlines use-once SourceGraphPattern and OptionalGraphPattern */
PatternElementForms: SOURCE '*' GraphPattern1  /* from SourceGraphPattern */
{
#if RASQAL_DEBUG > 1  
  printf("PatternElementForms 1\n  graphpattern=");
  rasqal_graph_pattern_print($3, stdout);
  fputs("\n\n", stdout);
#endif

  /* FIXME - SOURCE * has no defined meaning */
  sparql_syntax_warning(((rasqal_query*)rq), "SOURCE * ignored");
  $$=$3;
}
| SOURCE VarOrURI GraphPattern1 /* from SourceGraphPattern */
{
  int i;
  raptor_sequence *s=$3->graph_patterns;
  
#if RASQAL_DEBUG > 1  
  printf("PatternElementForms 2\n  varoruri=");
  rasqal_literal_print($2, stdout);
  printf(", graphpattern=");
  rasqal_graph_pattern_print($3, stdout);
  fputs("\n\n", stdout);
#endif

  if(s) {
    /* Flag all the triples in GraphPattern1 with origin $2 */
    for(i=0; i < raptor_sequence_size(s); i++) {
      rasqal_triple *t=(rasqal_triple*)raptor_sequence_get_at(s, i);
      rasqal_triple_set_origin(t, rasqal_new_literal_from_literal($2));
    }
  }
  rasqal_free_literal($2);
  $$=$3;
}
| OPTIONAL GraphPattern1 /* from OptionalGraphPattern */
{
  int i;
  raptor_sequence *s=$2->graph_patterns;

#if RASQAL_DEBUG > 1  
  printf("PatternElementForms 3\n  graphpattern=");
  rasqal_graph_pattern_print($2, stdout);
  fputs("\n\n", stdout);
#endif

  if(s) {
    /* Flag all the triples in GraphPattern1 as optional */
    for(i=0; i < raptor_sequence_size(s); i++) {
      rasqal_triple *t=(rasqal_triple*)raptor_sequence_get_at(s, i);
      t->flags |= RASQAL_TRIPLE_FLAGS_OPTIONAL;
    }
  }
  $$=$2;
}
| '[' GraphPattern ']' /* from OptionalGraphPattern */
{
  int i;

#if RASQAL_DEBUG > 1  
  printf("PatternElementForms 4\n  graphpattern=");
  if($2)
    raptor_sequence_print($2, stdout);
  else
    fputs("NULL", stdout);
  fputs("\n\n", stdout);
#endif

  if($2) {
    /* Make all graph patterns in GraphPattern, optional */
    for(i=0; i < raptor_sequence_size($2); i++) {
      rasqal_graph_pattern *gp=(rasqal_graph_pattern*)raptor_sequence_get_at($2, i);
      gp->flags |= RASQAL_PATTERN_FLAGS_OPTIONAL;
    }
    $$=rasqal_new_graph_pattern_from_sequence($2, RASQAL_PATTERN_FLAGS_OPTIONAL);
  } else
    $$=NULL;
}
| AND Expression
{
#if RASQAL_DEBUG > 1  
  printf("PatternElementForms 4\n expression=");
  rasqal_expression_print($2, stdout);
  fputs("\n\n", stdout);
#endif

  rasqal_query_add_constraint(((rasqal_query*)rq), $2);
  $$=NULL;
};


/* SPARQL Grammar: [13] SingleTriplePatternOrGroup - merged into PatternElement1, merged into GraphPattern1 */

/* SPARQL Grammar: [14] ExplicitGroup - merged into GraphPattern and GraphPattern1 for clarity */

/* SPARQL Grammar: [15] TriplePatternList */
TriplePatternList : TriplePatternList TriplePattern
{
  $$=$1;
  raptor_sequence_push($$, $2);
}
| TriplePattern
{
  $$=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_triple, (raptor_sequence_print_handler*)rasqal_triple_print);
  raptor_sequence_push($$, $1);
}
;


/* SPARQL Grammar: [16] TriplePattern */
TriplePattern : '(' VarOrURI VarOrURI VarOrLiteral ')'
{
  $$=rasqal_new_triple($2, $3, $4);
}
;


/* NEW Grammar Term */
VarOrURIList : VarOrURIList Var
{
  $$=$1;
  raptor_sequence_push($$, rasqal_new_variable_literal($2));
}
| VarOrURIList ',' Var
{
  $$=$1;
  raptor_sequence_push($$, rasqal_new_variable_literal($3));
}
| VarOrURIList URI
{
  $$=$1;
  raptor_sequence_push($$, $2);
}
| VarOrURIList ',' URI
{
  $$=$1;
  raptor_sequence_push($$, $3);
}
| Var 
{
  /* rasqal_variable* are freed from the raptor_query field variables */
  $$=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_literal, (raptor_sequence_print_handler*)rasqal_literal_print);
  raptor_sequence_push($$, rasqal_new_variable_literal($1));
}
| URI
{
  $$=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_literal, (raptor_sequence_print_handler*)rasqal_literal_print);
  raptor_sequence_push($$, $1);
}
;

/* NEW Grammar Term */
VarList : VarList Var
{
  $$=$1;
  raptor_sequence_push($$, $2);
}
| VarList ',' Var
{
  $$=$1;
  raptor_sequence_push($$, $3);
}
| Var 
{
  /* The variables are freed from the raptor_query field variables */
  $$=raptor_new_sequence(NULL, (raptor_sequence_print_handler*)rasqal_variable_print);
  raptor_sequence_push($$, $1);
}
;

/* NEW Grammar Term */
URIList : URIList URI
{
  raptor_uri* uri=rasqal_literal_as_uri($2);
  $$=$1;
  if(uri)
    raptor_sequence_push($$, uri);
}
| URIList ',' URI
{
  raptor_uri* uri=rasqal_literal_as_uri($3);
  $$=$1;
  if(uri)
    raptor_sequence_push($$, uri);
}
| /* empty */
{
  $$=raptor_new_sequence((raptor_sequence_free_handler*)raptor_free_uri, (raptor_sequence_print_handler*)raptor_sequence_print_uri);
}
;


/* SPARQL Grammar: [17] VarOrURI */
VarOrURI : Var
{
  $$=rasqal_new_variable_literal($1);
}
| URI
{
  $$=$1;
}
;

/* SPARQL Grammar: [18] VarOrLiteral */
VarOrLiteral : Var
{
  $$=rasqal_new_variable_literal($1);
}
| Literal
{
  $$=$1;
}
;


/* SPARQL Grammar: [19] PrefixDecl */
PrefixDeclOpt : PrefixDeclOpt PREFIX IDENTIFIER URI_LITERAL
{
  rasqal_prefix *p=rasqal_new_prefix($3, $4);
  $$=((rasqal_query*)rq)->prefixes;
  raptor_sequence_push($$, p);
  rasqal_engine_declare_prefix(((rasqal_query*)rq), p);
}
| /* empty */
{
  /* nothing to do, rq->prefixes already initialised */
}
;


/* SPARQL Grammar: [20] Expression */
Expression : ConditionalAndExpression SC_OR Expression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_OR, $1, $3);
}
| ConditionalAndExpression
{
  $$=$1;
}
;

/* SPARQL Grammar: [21] ConditionalOrExpression - merged into Expression */

/* SPARQL Grammar: [22] ConditionalAndExpression */
ConditionalAndExpression: ValueLogical SC_AND ConditionalAndExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_AND, $1, $3);
;
}
| ValueLogical
{
  $$=$1;
}
;

/* SPARQL Grammar: [23] ValueLogical */
ValueLogical : EqualityExpression STR_EQ EqualityExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_STR_EQ, $1, $3);
}
| EqualityExpression STR_NE EqualityExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_STR_NEQ, $1, $3);
}
| EqualityExpression STR_MATCH PATTERN_LITERAL
{
  $$=rasqal_new_string_op_expression(RASQAL_EXPR_STR_MATCH, $1, $3);
}
| EqualityExpression STR_NMATCH PATTERN_LITERAL
{
  $$=rasqal_new_string_op_expression(RASQAL_EXPR_STR_NMATCH, $1, $3);
}
| EqualityExpression
{
  $$=$1;
}
;

/* SPARQL Grammar: [24] StringEqualityExpression - merged into ValueLogical */

/* SPARQL Grammar: [25] StringComparitor - merged into StringEqualityExpression, merged into ValueLogical */

/* SPARQL Grammar: [26] EqualityExpression */
EqualityExpression : RelationalExpression EQ RelationalExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_EQ, $1, $3);
}
| RelationalExpression NEQ RelationalExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_NEQ, $1, $3);
}
| RelationalExpression
{
  $$=$1;
}
;

/* SPARQL Grammar: [27] RelationalComparitor - merged into EqualityExpression */

/* SPARQL Grammar: [28] RelationalExpression */
RelationalExpression : AdditiveExpression LT AdditiveExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_LT, $1, $3);
}
| AdditiveExpression GT AdditiveExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_GT, $1, $3);
}
| AdditiveExpression LE AdditiveExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_LE, $1, $3);
}
| AdditiveExpression GE AdditiveExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_GE, $1, $3);
}
| AdditiveExpression
{
  $$=$1;
}
;

/* SPARQL Grammar: [29] NumericComparitor - merged into RelationalExpression */

/* SPARQL Grammar: [30] AdditiveExpression */
AdditiveExpression : MultiplicativeExpression '+' AdditiveExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_PLUS, $1, $3);
}
| MultiplicativeExpression '-' AdditiveExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_MINUS, $1, $3);
}
| MultiplicativeExpression
{
  $$=$1;
}
;

/* SPARQL Grammar: [31] AdditiveOperation - merged into AdditiveExpression */


/* SPARQL Grammar: [32] MultiplicativeExpression */
MultiplicativeExpression : UnaryExpression '*' MultiplicativeExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_STAR, $1, $3);
}
| UnaryExpression '/' MultiplicativeExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_SLASH, $1, $3);
}
| UnaryExpression '%' MultiplicativeExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_REM, $1, $3);
}
| UnaryExpression
{
  $$=$1;
}
;

/* SPARQL Grammar: [33] MultiplicativeOperation - merged into MultiplicativeExpression */

/* SPARQL Grammar: [34] UnaryExpression */
UnaryExpression : '+' UnaryExpression
{
  $$=$2;
}
| '-' UnaryExpression
{
  $$=rasqal_new_1op_expression(RASQAL_EXPR_UMINUS, $2);
}
| UnaryExpressionNotPlusMinus
{
  $$=$1;
}
;

/* SPARQL Grammar: [35] UnaryExpressionNotPlusMinus */
UnaryExpressionNotPlusMinus : '~' UnaryExpression
{
  $$=rasqal_new_1op_expression(RASQAL_EXPR_TILDE, $2);
}
| '!' UnaryExpression
{
  $$=rasqal_new_1op_expression(RASQAL_EXPR_BANG, $2);
}
| Var
{
  rasqal_literal *l=rasqal_new_variable_literal($1);
  $$=rasqal_new_literal_expression(l);
}
| Literal
{
  $$=rasqal_new_literal_expression($1);
}
| '&' QNAME_LITERAL '(' ArgList ')'
{
  /* FIXME - do something with the function name, args */
  raptor_free_sequence($4);

  $$=NULL;
}
| '(' Expression ')'
{
  $$=$2;
}
;

/* SPARQL Grammar: [36] PrimaryExpression - merged into UnaryExpressionNotPlusMinus */

/* SPARQL Grammar: [37] FunctionCall - merged into UnaryExpressionNotPlusMinus */

/* SPARQL Grammar: [38] ArgList */
ArgList : ArgList VarOrLiteral
{
  $$=$1;
  raptor_sequence_push($$, $2);
}
| /* empty */
{
  $$=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_literal, (raptor_sequence_print_handler*)rasqal_literal_print);
}
;


/* SPARQL Grammar: [39] Literal */
Literal : URI_LITERAL
{
  $$=rasqal_new_uri_literal($1);
}
| INTEGER_LITERAL
{
  $$=$1;
}
| FLOATING_POINT_LITERAL
{
  $$=$1;
}
| STRING_LITERAL
{
  $$=$1;
}
| BOOLEAN_LITERAL
{
  $$=$1;
}
| NULL_LITERAL
{
  $$=$1;
} | QNAME_LITERAL
{
  $$=rasqal_new_simple_literal(RASQAL_LITERAL_QNAME, $1);
  if(rasqal_literal_expand_qname((rasqal_query*)rq, $$)) {
    sparql_query_error_full((rasqal_query*)rq,
                            "QName %s cannot be expanded", $1);
    rasqal_free_literal($$);
    $$=NULL;
  }
}
;

/* SPARQL Grammar: [40] NumericLiteral - merged into Literal */

/* SPARQL Grammar: [41] TextLiteral - merged into Literal */

/* SPARQL Grammar: [42] String - made into terminal STRING_LITERAL */

/* SPARQL Grammar: [43] URI */
URI : URI_LITERAL
{
  $$=rasqal_new_uri_literal($1);
}
| QNAME_LITERAL
{
  $$=rasqal_new_simple_literal(RASQAL_LITERAL_QNAME, $1);
  if(rasqal_literal_expand_qname((rasqal_query*)rq, $$)) {
    sparql_query_error_full((rasqal_query*)rq,
                            "QName %s cannot be expanded", $1);
    rasqal_free_literal($$);
    $$=NULL;
  }
}

/* SPARQL Grammar: [44] QName - made into terminal QNAME_LITERAL */

/* SPARQL Grammar: [45] QuotedURI - made into terminal URI_LITERAL */

/* SPARQL Grammar: [46] CommaOpt - merged inline */


/* NEW Grammar Term - terminal <VAR> in SPARQL */
Var : '?' IDENTIFIER
{
  $$=rasqal_new_variable((rasqal_query*)rq, $2, NULL);
}
| '$' IDENTIFIER
{
  $$=rasqal_new_variable((rasqal_query*)rq, $2, NULL);
}
;

%%


/* Support functions */


/* This is declared in sparql_lexer.h but never used, so we always get
 * a warning unless this dummy code is here.  Used once below in an error case.
 */
static int yy_init_globals (yyscan_t yyscanner ) { return 0; };



/**
 * rasqal_sparql_query_engine_init - Initialise the SPARQL query engine
 *
 * Return value: non 0 on failure
 **/
static int
rasqal_sparql_query_engine_init(rasqal_query* rdf_query, const char *name) {
  /* rasqal_sparql_query_engine* sparql=(rasqal_sparql_query_engine*)rdf_query->context; */

  /* Initialise rdf, rdfs, owl and xsd prefixes and namespaces */
  raptor_namespaces_start_namespace_full(rdf_query->namespaces, 
                                         (const unsigned char*)"rdf",
                                         (const unsigned char*)RAPTOR_RDF_MS_URI,0);
  raptor_namespaces_start_namespace_full(rdf_query->namespaces, 
                                         (const unsigned char*)"rdfs", 
                                         (const unsigned char*)RAPTOR_RDF_SCHEMA_URI,0);
  raptor_namespaces_start_namespace_full(rdf_query->namespaces,
                                         (const unsigned char*)"xsd",
                                         (const unsigned char*)RAPTOR_XMLSCHEMA_DATATYPES_URI, 0);
  raptor_namespaces_start_namespace_full(rdf_query->namespaces,
                                         (const unsigned char*)"owl",
                                         (const unsigned char*)RAPTOR_OWL_URI, 0);

  return 0;
}


/**
 * rasqal_sparql_query_engine_terminate - Free the SPARQL query engine
 *
 * Return value: non 0 on failure
 **/
static void
rasqal_sparql_query_engine_terminate(rasqal_query* rdf_query) {
  rasqal_sparql_query_engine* sparql=(rasqal_sparql_query_engine*)rdf_query->context;

  if(sparql->scanner_set) {
    sparql_lexer_lex_destroy(sparql->scanner);
    sparql->scanner_set=0;
  }

}


static int
rasqal_sparql_query_engine_prepare(rasqal_query* rdf_query) {
  /* rasqal_sparql_query_engine* sparql=(rasqal_sparql_query_engine*)rdf_query->context; */
  int rc;
  
  if(!rdf_query->query_string)
    return 1;
  
  rc=sparql_parse(rdf_query, rdf_query->query_string);
  if(rc)
    return rc;

  return rasqal_engine_prepare(rdf_query);
}


static int
rasqal_sparql_query_engine_execute(rasqal_query* rdf_query) 
{
  /* rasqal_sparql_query_engine* sparql=(rasqal_sparql_query_engine*)rdf_query->context; */
  
  /* nothing needed here */
  return 0;
}


static int
sparql_parse(rasqal_query* rq, const unsigned char *string) {
  rasqal_sparql_query_engine* rqe=(rasqal_sparql_query_engine*)rq->context;
  raptor_locator *locator=&rq->locator;
  char *buf=NULL;
  size_t len;
  void *buffer;

  if(!string || !*string)
    return yy_init_globals(NULL); /* 0 but a way to use yy_init_globals */

  locator->line=1;
  locator->column= -1; /* No column info */
  locator->byte= -1; /* No bytes info */

#if RASQAL_DEBUG > 2
  sparql_parser_debug=1;
#endif

  rqe->lineno=1;

  sparql_lexer_lex_init(&rqe->scanner);
  rqe->scanner_set=1;

  sparql_lexer_set_extra(((rasqal_query*)rq), rqe->scanner);

  /* This
   *   buffer= sparql_lexer__scan_string((const char*)string, rqe->scanner);
   * is replaced by the code below.  
   * 
   * The extra space appended to the buffer is the least-pain
   * workaround to the lexer crashing by reading EOF twice in
   * sparql_copy_regex_token; at least as far as I can diagnose.  The
   * fix here costs little to add as the above function does
   * something very similar to this anyway.
   */
  len= strlen((const char*)string);
  buf= (char *)RASQAL_MALLOC(cstring, len+3);
  strncpy(buf, (const char*)string, len);
  buf[len]= ' ';
  buf[len+1]= buf[len+2]='\0'; /* YY_END_OF_BUFFER_CHAR; */
  buffer= sparql_lexer__scan_buffer(buf, len+3, rqe->scanner);

  sparql_parser_parse(rq);

  if(buf)
    RASQAL_FREE(cstring, buf);

  sparql_lexer_lex_destroy(rqe->scanner);
  rqe->scanner_set=0;

  /* Parsing failed */
  if(rq->failed)
    return 1;
  
  /* FIXME - should check remaining query parts  */
  if(rasqal_engine_sequence_has_qname(rq->triples) ||
     rasqal_engine_sequence_has_qname(rq->constructs) ||
     rasqal_engine_constraints_has_qname(rq)) {
    sparql_query_error(rq, "Query has unexpanded QNames");
    return 1;
  }

  return 0;
}


static void
sparql_query_error(rasqal_query *rq, const char *msg) {
  rasqal_sparql_query_engine* rqe=(rasqal_sparql_query_engine*)rq->context;

  rq->locator.line=rqe->lineno;
#ifdef RASQAL_SPARQL_USE_ERROR_COLUMNS
  /*  rq->locator.column=sparql_lexer_get_column(yyscanner);*/
#endif

  rasqal_query_error(rq, "%s", msg);
}


static void
sparql_query_error_full(rasqal_query *rq, const char *message, ...) {
  va_list arguments;
  rasqal_sparql_query_engine* rqe=(rasqal_sparql_query_engine*)rq->context;

  rq->locator.line=rqe->lineno;
#ifdef RASQAL_SPARQL_USE_ERROR_COLUMNS
  /*  rq->locator.column=sparql_lexer_get_column(yyscanner);*/
#endif

  va_start(arguments, message);

  rasqal_query_error_varargs(rq, message, arguments);

  va_end(arguments);
}


int
sparql_syntax_error(rasqal_query *rq, const char *message, ...)
{
  rasqal_sparql_query_engine *rqe=(rasqal_sparql_query_engine*)rq->context;
  va_list arguments;

  rq->locator.line=rqe->lineno;
#ifdef RASQAL_SPARQL_USE_ERROR_COLUMNS
  /*  rp->locator.column=sparql_lexer_get_column(yyscanner);*/
#endif

  va_start(arguments, message);
  rasqal_query_error_varargs(rq, message, arguments);
  va_end(arguments);

   return (0);
}


int
sparql_syntax_warning(rasqal_query *rq, const char *message, ...)
{
  rasqal_sparql_query_engine *rqe=(rasqal_sparql_query_engine*)rq->context;
  va_list arguments;

  rq->locator.line=rqe->lineno;
#ifdef RASQAL_SPARQL_USE_ERROR_COLUMNS
  /*  rq->locator.column=sparql_lexer_get_column(yyscanner);*/
#endif

  va_start(arguments, message);
  rasqal_query_warning_varargs(rq, message, arguments);
  va_end(arguments);

   return (0);
}


static void
rasqal_sparql_query_engine_register_factory(rasqal_query_engine_factory *factory)
{
  factory->context_length = sizeof(rasqal_sparql_query_engine);

  factory->init      = rasqal_sparql_query_engine_init;
  factory->terminate = rasqal_sparql_query_engine_terminate;
  factory->prepare   = rasqal_sparql_query_engine_prepare;
  factory->execute   = rasqal_sparql_query_engine_execute;
}


void
rasqal_init_query_engine_sparql(void) {
  rasqal_query_engine_register_factory("sparql", 
                                       "SPARQL W3C DAWG RDF Query Language",
                                       NULL,
                                       (const unsigned char*)"http://www.w3.org/TR/rdf-sparql-query/",
                                       &rasqal_sparql_query_engine_register_factory);
}



#ifdef STANDALONE
#include <stdio.h>
#include <locale.h>

#define SPARQL_FILE_BUF_SIZE 2048

int
main(int argc, char *argv[]) 
{
  const char *program=rasqal_basename(argv[0]);
  char query_string[SPARQL_FILE_BUF_SIZE];
  rasqal_query *query;
  FILE *fh;
  int rc;
  char *filename=NULL;
  unsigned char *uri_string;

#if RASQAL_DEBUG > 2
  sparql_parser_debug=1;
#endif

  if(argc > 1) {
    filename=argv[1];
    fh = fopen(argv[1], "r");
    if(!fh) {
      fprintf(stderr, "%s: Cannot open file %s - %s\n", program, filename,
              strerror(errno));
      exit(1);
    }
  } else {
    filename="<stdin>";
    fh = stdin;
  }

  memset(query_string, 0, SPARQL_FILE_BUF_SIZE);
  fread(query_string, SPARQL_FILE_BUF_SIZE, 1, fh);
  
  if(argc>1)
    fclose(fh);

  rasqal_init();

  query=rasqal_new_query("sparql", NULL);

  uri_string=raptor_uri_filename_to_uri_string(filename);
  query->base_uri=raptor_new_uri(uri_string);

  rc=sparql_parse(query, (const unsigned char*)query_string);

  rasqal_query_print(query, stdout);

  rasqal_free_query(query);

  raptor_free_memory(uri_string);

  rasqal_finish();

  return rc;
}
#endif

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * sparql_parser.y - Rasqal SPARQL parser over tokens from sparql_lexer.l
 *
 * $Id$
 *
 * Copyright (C) 2004 David Beckett - http://purl.org/net/dajobe/
 * Institute for Learning and Research Technology - http://www.ilrt.bris.ac.uk/
 * University of Bristol - http://www.bristol.ac.uk/
 * 
 * This package is Free Software or Open Source available under the
 * following licenses (these are alternatives):
 *   1. GNU Lesser General Public License (LGPL)
 *   2. GNU General Public License (GPL)
 *   3. Mozilla Public License (MPL)
 * 
 * See LICENSE.html or LICENSE.txt at the top of this package for the
 * full license terms.
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


/* Make verbose error messages for syntax errors */
#ifdef RASQAL_DEBUG
#define YYERROR_VERBOSE 1
#endif

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
static int sparql_query_error(rasqal_query* rq, const char *message);

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
  double floating;
  raptor_uri *uri;
  unsigned char *name;
}


/*
 * shift/reduce conflicts
 */
%expect 3


/* word symbols */
%token SELECT SOURCE FROM WHERE AND
%token OPTIONAL PREFIX DESCRIBE CONSTRUCT ASK NOT DISTINCT LIMIT

/* expression delimitors */

%token COMMA LPAREN RPAREN LSQUARE RSQUARE
%token VARPREFIX

/* function call indicator */
%token AMP

/* SC booleans */
%left SC_OR SC_AND

/* string operations */
%left STR_EQ STR_NE STR_MATCH STR_NMATCH

/* operations */
%left EQ NEQ LT GT LE GE

/* arithmetic operations */
%left PLUS MINUS STAR SLASH REM

/* ? operations */
%left TILDE BANG

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
%type <seq> GraphPattern GraphPattern1
%type <seq> VarList VarOrURIList ArgList URIList
%type <seq> TriplePatternList
%type <seq> PatternElement PatternElementForms

%type <expr> Expression ConditionalAndExpression ValueLogical
%type <expr> EqualityExpression RelationalExpression AdditiveExpression
%type <expr> MultiplicativeExpression UnaryExpression
%type <expr> UnaryExpressionNotPlusMinus
%type <literal> VarOrLiteral VarOrURI

%type <variable> Var
%type <triple> TriplePattern
%type <literal> Literal URI

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
| DISTINCT STAR
{
  $$=NULL;
  ((rasqal_query*)rq)->select_all=1;
  ((rasqal_query*)rq)->distinct=1;
}
| VarList
{
  $$=$1;
}
| STAR
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
| STAR
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
| STAR
{
  $$=NULL;
}
;


/* SPARQL Grammar: [3] FromClause - renamed for clarity */
FromClauseOpt : FROM URIList
{
  /* FIXME - make a list of URI sources */
  $$=$2;
}
| /* empty */
{
}
;

/* SPARQL Grammar: [4] FromSelector - junk */

/* SPARQL Grammar: [5] WhereClause - remained for clarity*/
WhereClauseOpt :  WHERE GraphPattern
{
  ((rasqal_query*)rq)->triples=$2;
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
  /* FIXME - make graph pattern structure from element */
  $$=$1;
}
| PatternElement
{
  /* FIXME - make a graph pattern structure from element */
  $$=$1;
}
;


/* SPARQL Grammar: [9] PatternElement */
PatternElement : TriplePatternList
{
  /* FIXME - make a pattern element */
}
| LPAREN GraphPattern RPAREN /*  ExplicitGroup inlined */
{
  $$=$2;
}
| PatternElementForms
{
  /* FIXME - make a pattern element */
}
;


/* SPARQL Grammar: [10] GraphPattern1 */
GraphPattern1 : TriplePattern
{
  /* FIXME - make a graphpattern */
  $$=NULL;
}
| LPAREN GraphPattern RPAREN /*  ExplicitGroup inlined */
{
  /* FIXME - make a graphpattern */
  $$=$2;
}
| PatternElementForms
{
  /* FIXME - make a graphpattern */
  $$=NULL;
}
;


/* SPARQL Grammar: [11] PatternElement1 - merged into GraphPattern1 */

/* SPARQL Grammar: [12] PatternElementForms */

/* This inlines use-once SourceGraphPattern and OptionalGraphPattern */
PatternElementForms : SOURCE STAR GraphPattern1  /* from SourceGraphPattern */
{
  /* FIXME - SOURCE * has no defined meaning */
  $$=$3;
}
| SOURCE VarOrURI GraphPattern1 /* from SourceGraphPattern */
{
  /* FIXME flag all the triples in GraphPattern1 with source $2 optional */
  $$=$3;
}
| OPTIONAL GraphPattern1 /* from OptionalGraphPattern */
{
  /* FIXME flag all the triples in GraphPattern1 as optional */
  $$=$2;
}
| LSQUARE GraphPattern RSQUARE /* from OptionalGraphPattern */
{
  /* FIXME flag all the triples in GraphPattern1 as optional */
  $$=$2;
}
| AND Expression
{
  raptor_sequence* cons;
  
  /* FIXME - should append $2 to constraints, an already inited sequence */
  cons=raptor_new_sequence(NULL, (raptor_sequence_print_handler*)rasqal_expression_print);
  raptor_sequence_push(cons, $2);
  
  ((rasqal_query*)rq)->constraints=cons;
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
TriplePattern : LPAREN VarOrURI VarOrURI VarOrLiteral RPAREN
{
  $$=rasqal_new_triple($2, $3, $4);
}
;


/* NEW Grammar Term */
VarOrURIList : VarOrURIList Var
{
  $$=$1;
  raptor_sequence_push($$, $2);
}
| VarOrURIList COMMA Var
{
  $$=$1;
  raptor_sequence_push($$, $3);
}
| VarOrURIList URI
{
  $$=$1;
  raptor_sequence_push($$, $2);
}
| VarOrURIList COMMA URI
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
| URI
{
  /* The variables are freed from the raptor_query field variables */
  $$=raptor_new_sequence(NULL, (raptor_sequence_print_handler*)rasqal_variable_print);
  raptor_sequence_push($$, $1);
}
;

/* NEW Grammar Term */
VarList : VarList Var
{
  $$=$1;
  raptor_sequence_push($$, $2);
}
| VarList COMMA Var
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
  $$=$1;
  raptor_sequence_push($$, $2);
}
| URIList COMMA URI
{
  $$=$1;
  raptor_sequence_push($$, $3);
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
AdditiveExpression : MultiplicativeExpression PLUS AdditiveExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_PLUS, $1, $3);
}
| MultiplicativeExpression MINUS AdditiveExpression
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
MultiplicativeExpression : UnaryExpression STAR MultiplicativeExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_STAR, $1, $3);
}
| UnaryExpression SLASH MultiplicativeExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_SLASH, $1, $3);
}
| UnaryExpression REM MultiplicativeExpression
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
UnaryExpression : UnaryExpressionNotPlusMinus PLUS UnaryExpression 
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_PLUS, $1, $3);
}
| UnaryExpressionNotPlusMinus MINUS UnaryExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_MINUS, $1, $3);
}
| UnaryExpressionNotPlusMinus
{
  /* FIXME - 2 shift/reduce conflicts here
   *
   * The original grammar and this one is ambiguous in allowing
   * PLUS/MINUS in UnaryExpression as well as AdditiveExpression
   */
  $$=$1;
}
;

/* SPARQL Grammar: [35] UnaryExpressionNotPlusMinus */
UnaryExpressionNotPlusMinus : TILDE UnaryExpression
{
  $$=rasqal_new_1op_expression(RASQAL_EXPR_TILDE, $2);
}
| BANG UnaryExpression
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
| AMP QNAME_LITERAL LPAREN ArgList RPAREN
{
  /* FIXME - do something with the function name, args */
  $$=NULL;
}
| LPAREN Expression RPAREN
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
}
;

/* SPARQL Grammar: [40] NumericLiteral - merged into Literal */

/* SPARQL Grammar: [41] TextLiteral - merged into Literal */

/* SPARQL Grammar: [42] String - made into terminal STRING_LITERAL */

/* SPARQL Grammar: [43] URI */
URI: URI_LITERAL
{
  $$=rasqal_new_uri_literal($1);
}
| QNAME_LITERAL
{
  $$=rasqal_new_simple_literal(RASQAL_LITERAL_QNAME, $1);
}

/* SPARQL Grammar: [44] QName - made into terminal QNAME_LITERAL */

/* SPARQL Grammar: [45] QuotedURI - made into terminal URI_LITERAL */

/* SPARQL Grammar: [46] CommaOpt - merged inline */


/* NEW Grammar Term - terminal <VAR> in SPARQL */
Var : VARPREFIX IDENTIFIER
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

  if(rdf_query->query_string)
    return sparql_parse(rdf_query, rdf_query->query_string);
  else
    return 0;
}


static int
rasqal_sparql_query_engine_execute(rasqal_query* rdf_query) 
{
  /* rasqal_sparql_query_engine* sparql=(rasqal_sparql_query_engine*)rdf_query->context; */
  
  /* FIXME: not implemented */
  return 0;
}


static int
sparql_parse(rasqal_query* rq, const unsigned char *string) {
  rasqal_sparql_query_engine* rqe=(rasqal_sparql_query_engine*)rq->context;
  raptor_locator *locator=&rq->locator;
  void *buffer;

  if(!string || !*string)
    return yy_init_globals(NULL); /* 0 but a way to use yy_init_globals */

  locator->line=1;
  locator->column= -1; /* No column info */
  locator->byte= -1; /* No bytes info */

  rqe->lineno=1;

  sparql_lexer_lex_init(&rqe->scanner);
  rqe->scanner_set=1;

  sparql_lexer_set_extra(((rasqal_query*)rq), rqe->scanner);
  buffer= sparql_lexer__scan_string((const char*)string, rqe->scanner);

  sparql_parser_parse(rq);

  sparql_lexer_lex_destroy(rqe->scanner);
  rqe->scanner_set=0;

  /* Parsing failed */
  if(rq->failed)
    return 1;
  
  /* Only now can we handle the prefixes and qnames */
  if(rasqal_engine_declare_prefixes(rq) ||
     rasqal_engine_expand_triple_qnames(rq) ||
     rasqal_engine_expand_constraints_qnames(rq))
    return 1;

  return 0;
}


int
sparql_query_error(rasqal_query *rq, const char *msg) {
  rasqal_sparql_query_engine* rqe=(rasqal_sparql_query_engine*)rq->context;

  rq->locator.line=rqe->lineno;
#ifdef RASQAL_SPARQL_USE_ERROR_COLUMNS
  /*  rq->locator.column=sparql_lexer_get_column(yyscanner);*/
#endif

  rasqal_query_error(rq, "%s", msg);

  return 0;
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
rasqal_init_query_engine_sparql (void) {
  rasqal_query_engine_register_factory("sparql", 
                                       "SPARQL W3C DAWG RDF Query Language",
                                       "sparql",
                                       (const unsigned char*)"http://www.w3.org/2001/sw/DataAccess/rq23/",
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
  rasqal_query query; /* static */
  rasqal_sparql_query_engine sparql; /* static */
  raptor_locator *locator=&query.locator;
  FILE *fh;
  int rc;
  char *filename=NULL;
  raptor_uri_handler *uri_handler;
  void *uri_context;

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

  raptor_uri_init();

  memset(&query, 0, sizeof(rasqal_query));
  memset(&sparql, 0, sizeof(rasqal_sparql_query_engine));

  raptor_uri_get_handler(&uri_handler, &uri_context);
  query.namespaces=raptor_new_namespaces(uri_handler, uri_context,
                                         rasqal_query_simple_error,
                                         &query,
                                         0);
  query.variables_sequence=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_variable, (raptor_sequence_print_handler*)rasqal_variable_print);
  query.prefixes=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_prefix, (raptor_sequence_print_handler*)rasqal_prefix_print);
  
  locator->line= locator->column = -1;
  locator->file= filename;

  sparql.lineno= 1;

  query.context=&sparql;
  query.base_uri=raptor_new_uri(raptor_uri_filename_to_uri_string(filename));

  rasqal_sparql_query_engine_init(&query, "sparql");

  rc=sparql_parse(&query, (const unsigned char*)query_string);

  raptor_free_namespaces(query.namespaces);
  raptor_free_sequence(query.prefixes);

  raptor_free_uri(query.base_uri);

  return rc;
}
#endif

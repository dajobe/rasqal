/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rdql_parser.y - Rasqal RDQL parser - over tokens from rdql grammar lexer
 *
 * $Id$
 *
 * Copyright (C) 2003-2004 David Beckett - http://purl.org/net/dajobe/
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
 * 
 */

%{
#ifdef HAVE_CONFIG_H
#include <rasqal_config.h>
#endif

#ifdef WIN32
#include <win32_config.h>
#endif

#include <stdio.h>
#include <stdarg.h>

#include <rasqal.h>
#include <rasqal_internal.h>

#include <rdql_parser.tab.h>

#define YY_DECL int rdql_lexer_lex (YYSTYPE *rdql_parser_lval, yyscan_t yyscanner)
#include <rdql_lexer.h>

#include <rdql_common.h>


/* Make verbose error messages for syntax errors */
#ifdef RASQAL_DEBUG
#define YYERROR_VERBOSE 1
#endif

/* Slow down the grammar operation and watch it work */
#if RASQAL_DEBUG > 2
#define YYDEBUG 1
#endif

/* the lexer does not seem to track this */
#undef RASQAL_RDQL_USE_ERROR_COLUMNS

/* Missing rdql_lexer.c/h prototypes */
int rdql_lexer_get_column(yyscan_t yyscanner);
/* Not used here */
/* void rdql_lexer_set_column(int  column_no , yyscan_t yyscanner);*/


/* What the lexer wants */
extern int rdql_lexer_lex (YYSTYPE *rdql_parser_lval, yyscan_t scanner);
#define YYLEX_PARAM ((rasqal_rdql_query_engine*)(((rasqal_query*)rq)->context))->scanner

/* Pure parser argument (a void*) */
#define YYPARSE_PARAM rq

/* Make the yyerror below use the rdf_parser */
#undef yyerror
#define yyerror(message) rdql_query_error((rasqal_query*)rq, message)

/* Make lex/yacc interface as small as possible */
#undef yylex
#define yylex rdql_lexer_lex


static int rdql_parse(rasqal_query* rq, const char *string);
static int rdql_query_error(rasqal_query* rq, const char *message);

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
  unsigned char *string;
  unsigned char *flags;
  unsigned char *language;
  int integer;
  float floating;
  raptor_uri *uri;
}


/*
 * Two shift/reduce
 */
%expect 2


/* word symbols */
%token SELECT SOURCE FROM WHERE AND FOR

/* expression delimitors */

%token COMMA LPAREN RPAREN
%token VARPREFIX USING


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
%token <integer> INTEGER_LITERAL
%token <floating> FLOATING_POINT_LITERAL
%token <string> STRING_LITERAL PATTERN_LITERAL
%token <integer> BOOLEAN_LITERAL
%token <integer> NULL_LITERAL 
%token <uri> URI_LITERAL
%token <string> QNAME_LITERAL

%token <string> IDENTIFIER

/* syntax error */
%token ERROR


%type <seq> SelectClause SourceClause ConstraintClause UsingClause
%type <seq> CommaAndConstraintClause
%type <seq> VarList TriplePatternList PrefixDeclList ArgList URIList

%type <expr> Expression ConditionalAndExpression ValueLogical
%type <expr> EqualityExpression RelationalExpression NumericExpression
%type <expr> AdditiveExpression MultiplicativeExpression UnaryExpression
%type <expr> UnaryExpressionNotPlusMinus
%type <expr> VarOrLiteral VarOrURI

%type <variable> Var
%type <triple> TriplePattern
%type <literal> PatternLiteral Literal

%%


Document : Query
;


Query : SELECT SelectClause SourceClause WHERE TriplePatternList ConstraintClause UsingClause
{
  ((rasqal_query*)rq)->selects=$2;
  ((rasqal_query*)rq)->sources=$3;
  ((rasqal_query*)rq)->triples=$5;
  ((rasqal_query*)rq)->constraints=$6;
  ((rasqal_query*)rq)->prefixes=$7;
}
;

VarList : Var COMMA VarList 
{
  $$=$3;
  raptor_sequence_shift($$, $1);
}
| Var VarList 
{
  $$=$2;
  raptor_sequence_shift($$, $1);
}
| Var 
{
  /* The variables are freed from the raptor_query field variables */
  $$=raptor_new_sequence(NULL, (raptor_sequence_print_handler*)rasqal_variable_print);
  raptor_sequence_push($$, $1);
}
;


SelectClause : VarList
{
  $$=$1;
}
| STAR
{
  $$=NULL;
  ((rasqal_query*)rq)->select_all=1;
}
;

SourceClause : SOURCE URIList
{
  $$=$2;
}
| FROM URIList
{
  $$=$2;
}
| /* empty */
{
  $$=NULL;
}
;

/* Inlined into SourceClause: SourceSelector : URL */


/* Jena RDQL allows optional COMMA */
TriplePatternList : TriplePattern COMMA TriplePatternList
{
  $$=$3;
  raptor_sequence_shift($$, $1);
}
| TriplePattern TriplePatternList
{
  $$=$2;
  raptor_sequence_shift($$, $1);
}
| TriplePattern
{
  $$=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_triple, (raptor_sequence_print_handler*)rasqal_triple_print);
  raptor_sequence_push($$, $1);
}
;

/* Inlined:
 TriplePatternClause : WHERE TriplePatternList 
*/


/* FIXME - maybe a better way to do this optional COMMA? */
TriplePattern : LPAREN VarOrURI COMMA VarOrURI COMMA VarOrLiteral RPAREN
{
  $$=rasqal_new_triple($2, $4, $6);
}
| LPAREN VarOrURI VarOrURI COMMA VarOrLiteral RPAREN
{
  $$=rasqal_new_triple($2, $3, $5);
}
| LPAREN VarOrURI COMMA VarOrURI VarOrLiteral RPAREN
{
  $$=rasqal_new_triple($2, $4, $5);
}
| LPAREN VarOrURI VarOrURI VarOrLiteral RPAREN
{
  $$=rasqal_new_triple($2, $3, $4);
}
;


/* Was:
ConstraintClause : AND Expression ( ( COMMA | AND ) Expression )*
*/

ConstraintClause : AND CommaAndConstraintClause
{
  $$=$2;
}
| /* empty */
{
  $$=NULL;
}
;

CommaAndConstraintClause : Expression COMMA CommaAndConstraintClause
{
  $$=$3;
  raptor_sequence_shift($$, $1);
}
| Expression AND CommaAndConstraintClause
{
  $$=$3;
  raptor_sequence_shift($$, $1);
}
| Expression
{
  $$=raptor_new_sequence(NULL, (raptor_sequence_print_handler*)rasqal_expression_print);
  raptor_sequence_push($$, $1);
}
;



UsingClause : USING PrefixDeclList
{
  $$=$2;
}
| /* empty */
{
  $$=NULL;
}
;

PrefixDeclList : IDENTIFIER FOR URI_LITERAL COMMA PrefixDeclList 
{
  $$=$5;
  raptor_sequence_shift($$, rasqal_new_prefix($1, $3));
}
| IDENTIFIER FOR URI_LITERAL
{
  $$=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_prefix, (raptor_sequence_print_handler*)rasqal_prefix_print);
  raptor_sequence_push($$, rasqal_new_prefix($1, $3));
}
;


Expression : ConditionalAndExpression SC_OR Expression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_OR, $1, $3);
}
| ConditionalAndExpression
{
  $$=$1;
}
;

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

ValueLogical : EqualityExpression STR_EQ EqualityExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_STR_EQ, $1, $3);
}
| EqualityExpression STR_NE EqualityExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_STR_NEQ, $1, $3);
}
| EqualityExpression STR_MATCH PatternLiteral
{
  $$=rasqal_new_string_op_expression(RASQAL_EXPR_STR_MATCH, $1, $3);
}
| EqualityExpression STR_NMATCH PatternLiteral
{
  $$=rasqal_new_string_op_expression(RASQAL_EXPR_STR_NMATCH, $1, $3);
}
| EqualityExpression
{
  $$=$1;
}
;

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

RelationalExpression : NumericExpression LT NumericExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_LT, $1, $3);
}
| NumericExpression GT NumericExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_GT, $1, $3);
}
| NumericExpression LE NumericExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_LE, $1, $3);
}
| NumericExpression GE NumericExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_GE, $1, $3);
}
| NumericExpression
{
  $$=$1;
}
;

NumericExpression : AdditiveExpression
{
  $$=$1;
}
;


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
  $$=rasqal_new_variable_expression($1);
}
| Literal
{
  $$=rasqal_new_literal_expression($1);
}
| IDENTIFIER LPAREN ArgList RPAREN
{
  rasqal_literal *l=rasqal_new_literal(RASQAL_LITERAL_STRING, 0, 0.0, "functioncall", NULL);
  $$=rasqal_new_literal_expression(l);
}
| LPAREN Expression RPAREN
{
  $$=$2;
}
;

ArgList : VarOrLiteral COMMA ArgList
{
  $$=$3;
  raptor_sequence_shift($$, $1);
}
| VarOrLiteral
{
  $$=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_expression, (raptor_sequence_print_handler*)rasqal_expression_print);
  raptor_sequence_push($$, $1);
}
;

VarOrURI : Var
{
  $$=rasqal_new_variable_expression($1);
}
| URI_LITERAL
{
  rasqal_literal *l=rasqal_new_literal(RASQAL_LITERAL_URI, 0, 0.0, NULL, $1);
  $$=rasqal_new_literal_expression(l);
}
| QNAME_LITERAL
{
  rasqal_literal *l=rasqal_new_literal(RASQAL_LITERAL_QNAME, 0, 0.0, $1, NULL);
  $$=rasqal_new_literal_expression(l);
}
;

VarOrLiteral : Var
{
  $$=rasqal_new_variable_expression($1);
}
| Literal
{
  $$=rasqal_new_literal_expression($1);
}
;

Var : VARPREFIX IDENTIFIER
{
  $$=rasqal_new_variable(rq, $2, NULL);
}
;

PatternLiteral: PATTERN_LITERAL
{
  $$=rasqal_new_literal(RASQAL_LITERAL_PATTERN, 0, 0.0, $1, NULL);
}
;

Literal : URI_LITERAL
{
  $$=rasqal_new_literal(RASQAL_LITERAL_URI, 0, 0.0, NULL, $1);
}
| INTEGER_LITERAL
{
  $$=rasqal_new_literal(RASQAL_LITERAL_INTEGER, $1, 0.0, NULL, NULL);
}
| FLOATING_POINT_LITERAL
{
  $$=rasqal_new_literal(RASQAL_LITERAL_FLOATING, 0, $1, NULL, NULL);
}
| STRING_LITERAL
{
  $$=rasqal_new_literal(RASQAL_LITERAL_STRING, 0, 0.0, $1, NULL);
}
| BOOLEAN_LITERAL
{
  $$=rasqal_new_literal(RASQAL_LITERAL_BOOLEAN, $1, 0.0, NULL, NULL);
}
| NULL_LITERAL
{
  $$=rasqal_new_literal(RASQAL_LITERAL_NULL, $1, 0, NULL, NULL);
} | QNAME_LITERAL
{
  $$=rasqal_new_literal(RASQAL_LITERAL_QNAME, 0, 0.0, $1, NULL);
}

;

URIList : URI_LITERAL COMMA URIList
{
  $$=$3;
  raptor_sequence_shift($$, $1);
}
| URI_LITERAL
{
  $$=raptor_new_sequence((raptor_sequence_free_handler*)raptor_free_uri, (raptor_sequence_print_handler*)raptor_sequence_print_uri);
  raptor_sequence_push($$, $1);
}
;

%%


/* Support functions */


/* This is declared in rdql_lexer.h but never used, so we always get
 * a warning unless this dummy code is here.  Used once below in an error case.
 */
static int yy_init_globals (yyscan_t yyscanner ) { return 0; };



/**
 * rasqal_rdql_query_engine_init - Initialise the RDQL query engine
 *
 * Return value: non 0 on failure
 **/
static int
rasqal_rdql_query_engine_init(rasqal_query* rdf_query, const char *name) {
  /* rasqal_rdql_query_engine* rdql=(rasqal_rdql_query_engine*)rdf_query->context; */

  return 0;
}


/**
 * rasqal_rdql_query_engine_terminate - Free the RDQL query engine
 *
 * Return value: non 0 on failure
 **/
static void
rasqal_rdql_query_engine_terminate(rasqal_query* rdf_query) {
  rasqal_rdql_query_engine* rdql=(rasqal_rdql_query_engine*)rdf_query->context;

  if(rdql->scanner_set) {
    rdql_lexer_lex_destroy(&rdql->scanner);
    rdql->scanner_set=0;
  }

}


static int
rasqal_rdql_query_engine_prepare(rasqal_query* rdf_query) {
  /* rasqal_rdql_query_engine* rdql=(rasqal_rdql_query_engine*)rdf_query->context; */

  return rdql_parse(rdf_query, rdf_query->query_string);
}


static int
rasqal_rdql_query_engine_execute(rasqal_query* rdf_query) 
{
  /* rasqal_rdql_query_engine* rdql=(rasqal_rdql_query_engine*)rdf_query->context; */
  
  /* FIXME: not implemented */
  return 0;
}


static int
rdql_parse(rasqal_query* rq, const char *string) {
  rasqal_rdql_query_engine* rqe=(rasqal_rdql_query_engine*)rq->context;
  raptor_locator *locator=&rq->locator;
  void *buffer;

  if(!string || !*string)
    return yy_init_globals(NULL); /* 0 but a way to use yy_init_globals */

  locator->line=1;
  locator->column= -1; /* No column info */
  locator->byte= -1; /* No bytes info */

  rqe->lineno=1;

  rdql_lexer_lex_init(&rqe->scanner);
  rqe->scanner_set=1;

  rdql_lexer_set_extra(((rasqal_query*)rq), rqe->scanner);
  buffer= rdql_lexer__scan_string(string, rqe->scanner);

  rdql_parser_parse(rq);

  rdql_lexer_lex_destroy(rqe->scanner);
  rqe->scanner_set=0;

  /* Only now can we handle the prefixes and qnames */
  if(rasqal_engine_declare_prefixes(rq) ||
     rasqal_engine_expand_triple_qnames(rq) ||
     rasqal_engine_expand_constraints_qnames(rq))
    return 1;

  return 0;
}


int
rdql_query_error(rasqal_query *rq, const char *msg) {
  rasqal_rdql_query_engine* rqe=(rasqal_rdql_query_engine*)rq->context;

  rq->locator.line=rqe->lineno;
#ifdef RASQAL_RDQL_USE_ERROR_COLUMNS
  /*  rq->locator.column=rdql_lexer_get_column(yyscanner);*/
#endif

  rasqal_query_error(rq, msg);

  return 0;
}


int
rdql_syntax_error(rasqal_query *rq, const char *message, ...)
{
  rasqal_rdql_query_engine *rqe=(rasqal_rdql_query_engine*)rq->context;
  va_list arguments;

  rq->locator.line=rqe->lineno;
#ifdef RASQAL_RDQL_USE_ERROR_COLUMNS
  /*  rp->locator.column=rdql_lexer_get_column(yyscanner);*/
#endif

  va_start(arguments, message);
  rasqal_query_error_varargs(rq, message, arguments);
  va_end(arguments);

   return (0);
}


int
rdql_syntax_warning(rasqal_query *rq, const char *message, ...)
{
  rasqal_rdql_query_engine *rqe=(rasqal_rdql_query_engine*)rq->context;
  va_list arguments;

  rq->locator.line=rqe->lineno;
#ifdef RASQAL_RDQL_USE_ERROR_COLUMNS
  /*  rq->locator.column=rdql_lexer_get_column(yyscanner);*/
#endif

  va_start(arguments, message);
  rasqal_query_warning_varargs(rq, message, arguments);
  va_end(arguments);

   return (0);
}


static void
rasqal_rdql_query_engine_register_factory(rasqal_query_engine_factory *factory)
{
  factory->context_length = sizeof(rasqal_rdql_query_engine);

  factory->init      = rasqal_rdql_query_engine_init;
  factory->terminate = rasqal_rdql_query_engine_terminate;
  factory->prepare   = rasqal_rdql_query_engine_prepare;
  factory->execute   = rasqal_rdql_query_engine_execute;
}


void
rasqal_init_query_engine_rdql (void) {
  /* http://www.w3.org/Submission/2004/SUBM-RDQL-20040109/ */

  rasqal_query_engine_register_factory("rdql", 
                                       "RDF Data Query Language (RDQL)",
                                       NULL,
                                       (const unsigned char*)"http://jena.hpl.hp.com/2003/07/query/RDQL",
                                       &rasqal_rdql_query_engine_register_factory);
}



#ifdef STANDALONE
#include <stdio.h>
#include <locale.h>

#define RDQL_FILE_BUF_SIZE 2048

int
main(int argc, char *argv[]) 
{
  char query_string[RDQL_FILE_BUF_SIZE];
  rasqal_query query; /* static */
  rasqal_rdql_query_engine rdql; /* static */
  raptor_locator *locator=&query.locator;
  FILE *fh;
  int rc;
  unsigned char *filename=NULL;
  raptor_uri_handler *uri_handler;
  void *uri_context;

#if RASQAL_DEBUG > 2
  rdql_parser_debug=1;
#endif

  if(argc > 1) {
    filename=argv[1];
    fh = fopen(argv[1], "r");
    if(!fh) {
      fprintf(stderr, "%s: Cannot open file %s - %s\n", argv[0], filename,
              strerror(errno));
      exit(1);
    }
  } else {
    filename="<stdin>";
    fh = stdin;
  }

  memset(query_string, 0, RDQL_FILE_BUF_SIZE);
  fread(query_string, RDQL_FILE_BUF_SIZE, 1, fh);
  
  if(argc>1)
    fclose(fh);

  raptor_uri_init();

  memset(&query, 0, sizeof(rasqal_query));
  memset(&rdql, 0, sizeof(rasqal_rdql_query_engine));

  raptor_uri_get_handler(&uri_handler, &uri_context);
  query.namespaces=raptor_new_namespaces(uri_handler, uri_context,
                                         rasqal_query_simple_error,
                                         &query,
                                         0);
  query.variables_sequence=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_variable, (raptor_sequence_print_handler*)rasqal_variable_print);
  
  locator->line= locator->column = -1;
  locator->file= filename;

  rdql.lineno= 1;

  query.context=&rdql;
  query.base_uri=raptor_new_uri(raptor_uri_filename_to_uri_string(filename));

  rasqal_rdql_query_engine_init(&query, "rdql");

  rc=rdql_parse(&query, query_string);

  raptor_free_namespaces(query.namespaces);

  raptor_free_uri(query.base_uri);

  return rc;
}
#endif

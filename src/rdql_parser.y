/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rdql_parser.y - Rasqal RDQL parser - over tokens from rdql grammar lexer
 *
 * $Id$
 *
 * Copyright (C) 2003 David Beckett - http://purl.org/net/dajobe/
 * Institute for Learning and Research Technology - http://www.ilrt.org/
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

#include <raptor.h>

#ifdef RASQAL_IN_REDLAND
#include <librdf.h>
#endif


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

/* Prototypes */ 
int rdql_query_error(rdql_parser *rp, const char *msg);

/* Missing rdql_lexer.c/h prototypes */
int rdql_lexer_get_column(yyscan_t yyscanner);
/* Not used here */
/* void rdql_lexer_set_column(int  column_no , yyscan_t yyscanner);*/


/* What the lexer wants */
extern int rdql_lexer_lex (YYSTYPE *rdql_parser_lval, yyscan_t scanner);
#define YYLEX_PARAM ((rdql_parser*)rp)->scanner

/* Pure parser argument (a void*) */
#define YYPARSE_PARAM rp

/* Make the yyerror below use the rdf_parser */
#undef yyerror
#define yyerror(message) rdql_query_error(rp, message)

/* Make lex/yacc interface as small as possible */
#undef yylex
#define yylex rdql_lexer_lex

 
%}


/* directives */


%pure-parser


/* Interface between lexer and parser */
%union {
  rasqal_sequence *seq;
  rasqal_variable *variable;
  rasqal_literal *literal;
  rasqal_triple *triple;
  rasqal_expression *expr;
  unsigned char *string;
  unsigned char *flags;
  unsigned char *language;
  int integer;
  float floating;
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
%token <string> URI_LITERAL

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
  ((rdql_parser*)rp)->query->selects=$2;
  ((rdql_parser*)rp)->query->sources=$3;
  ((rdql_parser*)rp)->query->triples=$5;
  ((rdql_parser*)rp)->query->constraints=$6;
  ((rdql_parser*)rp)->query->prefixes=$7;
}
;

VarList : Var COMMA VarList 
{
  $$=$3;
  rasqal_sequence_shift($$, $1);
}
| Var VarList 
{
  $$=$2;
  rasqal_sequence_shift($$, $1);
}
| Var 
{
  $$=rasqal_new_sequence(NULL, (rasqal_print_handler*)rasqal_print_variable);
  rasqal_sequence_push($$, $1);
}
;


SelectClause : VarList
{
  $$=$1;
}
| STAR
{
  $$=NULL;
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
  rasqal_sequence_shift($$, $1);
}
| TriplePattern TriplePatternList
{
  $$=$2;
  rasqal_sequence_shift($$, $1);
}
| TriplePattern
{
  $$=rasqal_new_sequence(NULL, (rasqal_print_handler*)rasqal_print_triple);
  rasqal_sequence_push($$, $1);
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
  rasqal_sequence_shift($$, $1);
}
| Expression AND CommaAndConstraintClause
{
  $$=$3;
  rasqal_sequence_shift($$, $1);
}
| Expression
{
  $$=rasqal_new_sequence(NULL, (rasqal_print_handler*)rasqal_print_expression);
  rasqal_sequence_push($$, $1);
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
  rasqal_sequence_shift($$, rasqal_new_prefix($1, $3));
}
| IDENTIFIER FOR URI_LITERAL
{
  $$=rasqal_new_sequence(NULL, (rasqal_print_handler*)rasqal_print_prefix);
  rasqal_sequence_push($$, rasqal_new_prefix($1, $3));
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
  rasqal_literal *l=rasqal_new_literal(RASQAL_LITERAL_STRING, 0, 0.0, "functioncall");
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
  rasqal_sequence_shift($$, $1);
}
| VarOrLiteral
{
  $$=rasqal_new_sequence(NULL, (rasqal_print_handler*)rasqal_print_expression);
  rasqal_sequence_push($$, $1);
}
;

VarOrURI : Var
{
  $$=rasqal_new_variable_expression($1);
}
| URI_LITERAL
{
  rasqal_literal *l=rasqal_new_literal(RASQAL_LITERAL_URI, 0, 0.0, $1);
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
  $$=rasqal_new_variable($2, NULL);
}
;

PatternLiteral: PATTERN_LITERAL
{
  $$=rasqal_new_literal(RASQAL_LITERAL_PATTERN, 0, 0.0, $1);
}
;

Literal : URI_LITERAL
{
  $$=rasqal_new_literal(RASQAL_LITERAL_URI, 0, 0.0, $1);
}
| INTEGER_LITERAL
{
  $$=rasqal_new_literal(RASQAL_LITERAL_INTEGER, $1, 0.0, NULL);
}
| FLOATING_POINT_LITERAL
{
  $$=rasqal_new_literal(RASQAL_LITERAL_FLOATING, 0, $1, NULL);
}
| STRING_LITERAL
{
  $$=rasqal_new_literal(RASQAL_LITERAL_STRING, 0, 0.0, $1);
}
| BOOLEAN_LITERAL
{
  $$=rasqal_new_literal(RASQAL_LITERAL_BOOLEAN, $1, 0.0, NULL);
}
| NULL_LITERAL
{
  $$=rasqal_new_literal(RASQAL_LITERAL_NULL, $1, 0, NULL);
}
;

URIList : URI_LITERAL COMMA URIList
{
  $$=$3;
  rasqal_sequence_shift($$, $1);
}
| URI_LITERAL
{
  $$=rasqal_new_sequence(NULL, (rasqal_print_handler*)rasqal_sequence_print_string);
  rasqal_sequence_push($$, $1);
}
;

%%


/* Support functions */


/* This is declared in rdql_lexer.h but never used, so we always get
 * a warning unless this dummy code is here.  Used once below in an error case.
 */
static int yy_init_globals (yyscan_t yyscanner ) { return 0; };


int
rdql_parse(rasqal_query* rq,
           const unsigned char *uri_string, 
           const char *string) {
  rdql_parser rp;
  void *buffer;
  
  if(!string || !*string)
    return yy_init_globals(NULL); /* 0 but a way to use yy_init_globals */

  memset(&rp, 0, sizeof(rdql_parser));

  rp.query=rq;
  rp.uri_string=uri_string;
  
  rdql_lexer_lex_init(&rp.scanner);

  rdql_lexer_set_extra(&rp, rp.scanner);
  buffer= rdql_lexer__scan_string(string, rp.scanner);

  rdql_parser_parse(&rp);

  return rp.errors;
}


int
rdql_query_error(rdql_parser* rp, const char *msg) {
  yyscan_t yyscanner=rp->scanner;

  rp->line=rdql_lexer_get_lineno(yyscanner);
#ifdef RASQAL_RDQL_USE_ERROR_COLUMNS
  /*  rp->column=rdql_lexer_get_column(yyscanner);*/
#endif

  fprintf(stderr, "(rdql_query_error) %s:%d: %s\n", rp->uri_string, rp->line, msg);

  rp->errors++;
  return (0);
}


int
rdql_syntax_error(rdql_parser *rp, const char *message, ...)
{
  yyscan_t yyscanner=rp->scanner;
  va_list arguments;

  rp->line=rdql_lexer_get_lineno(yyscanner);
#ifdef RASQAL_RDQL_USE_ERROR_COLUMNS
  /*  rp->column=rdql_lexer_get_column(yyscanner);*/
#endif

  fprintf(stderr, "(rdql_syntax_error) %s:%d:", rp->uri_string, rp->line);
  va_start(arguments, message);
  vfprintf(stderr, message, arguments);
  va_end(arguments);
  fputc('\n', stderr);

  rp->errors++;
  return (0);
}


int
rdql_syntax_warning(rdql_parser *rp, const char *message, ...)
{
  yyscan_t yyscanner=rp->scanner;
  va_list arguments;

  rp->line=rdql_lexer_get_lineno(yyscanner);
#ifdef RASQAL_RDQL_USE_ERROR_COLUMNS
  /*  Rp->column=rdql_lexer_get_column(yyscanner);*/
#endif

  fprintf(stderr, "(rdql_syntax_warning) %s:%d:", rp->uri_string, rp->line);
  va_start(arguments, message);
  vfprintf(stderr, message, arguments);
  va_end(arguments);
  fputc('\n', stderr);

  rp->warnings++;
  return (0);
}


#ifdef STANDALONE
#include <stdio.h>
#include <locale.h>

#define RDQL_FILE_BUF_SIZE 2048

int
main(int argc, char *argv[]) 
{
  rasqal_query rq;
  unsigned char *filename=NULL;
  unsigned char *uri_string;
  char query_string[RDQL_FILE_BUF_SIZE];
  FILE *fh;
  int rc;
#ifdef RASQAL_IN_REDLAND
  librdf_world *world;
#endif

#ifdef RASQAL_IN_REDLAND
  world=librdf_new_world();
  librdf_world_open(world);
#else
  raptor_init();
#endif
  
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
  
  memset(&rq, 0, sizeof(rasqal_query));

  uri_string=raptor_uri_filename_to_uri_string(filename);

  rc=rdql_parse(&rq, uri_string, query_string);

  free(uri_string);

#ifdef RASQAL_IN_REDLAND
  librdf_free_world(world);
#else
  raptor_finish();
#endif

  return rc;
}
#endif

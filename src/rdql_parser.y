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

#include <rasqal.h>
#include <rasqal_internal.h>

#include <rdql_parser.tab.h>
#include <rdql_lexer.h>


/* Prototypes */ 
int rdql_parser_error(const char *msg);


/* Make lex/yacc interface as small as possible */
inline int
rdql_parser_lex(void) {
  return rdql_lexer_lex();
}

/* GLOBAL - FIXME */
static rasqal_query* Q;
 

%}


/* Interface between lexer and parser */
%union {
  rasqal_sequence *seq;
  rasqal_variable *variable;
  rasqal_literal *literal;
  rasqal_triple *triple;
  rasqal_expression *expr;
  unsigned char *string;
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

/* bit operations */
%left BIT_OR BIT_XOR BIT_AND

/* operations */
%left EQ NEQ LT GT LE GE

/* bitshift operations */
%left LSHIFT RSIGNEDSHIFT RUNSIGNEDSHIFT

/* arithmetic operations */
%left PLUS MINUS STAR SLASH REM

/* ? operations */
%left TILDE BANG

/* literals */
%token <integer> INTEGER_LITERAL
%token <floating> FLOATING_POINT_LITERAL
%token <string> STRING_LITERAL
%token <integer> BOOLEAN_LITERAL
%token <integer> NULL_LITERAL 
%token <string> URI_LITERAL

%token <string> IDENTIFIER

/* end of input */
%token END

/* syntax error */
%token ERROR


%type <seq> SelectClause SourceClause ConstraintClause UsingClause
%type <seq> CommaAndConstraintClause
%type <seq> VarList TriplePatternList PrefixDeclList ArgList URIList

%type <expr> Expression ConditionalAndExpression ValueLogical
%type <expr> InclusiveOrExpression ExclusiveOrExpression AndExpression
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
  Q->selects=$2;
  Q->sources=$3;
  Q->triples=$5;
  Q->constraints=$6;
  Q->prefixes=$7;
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

ValueLogical : InclusiveOrExpression STR_EQ InclusiveOrExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_STR_EQ, $1, $3);
}
| InclusiveOrExpression STR_NE InclusiveOrExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_STR_NEQ, $1, $3);
}
| InclusiveOrExpression STR_MATCH PatternLiteral
{
  $$=rasqal_new_string_op_expression(RASQAL_EXPR_STR_MATCH, $1, $3);
}
| InclusiveOrExpression STR_NMATCH PatternLiteral
{
  $$=rasqal_new_string_op_expression(RASQAL_EXPR_STR_NMATCH, $1, $3);
}
| InclusiveOrExpression
{
  $$=$1;
}
;


InclusiveOrExpression : ExclusiveOrExpression BIT_OR InclusiveOrExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_BIT_OR, $1, $3);
}
| ExclusiveOrExpression
{
  $$=$1;
}
;

ExclusiveOrExpression : AndExpression BIT_XOR ExclusiveOrExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_BIT_XOR, $1, $3);
}
| AndExpression
{
  $$=$1;
}
;

AndExpression : EqualityExpression BIT_AND AndExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_BIT_AND, $1, $3);
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

NumericExpression : AdditiveExpression LSHIFT NumericExpression
{
 $$=rasqal_new_2op_expression(RASQAL_EXPR_LSHIFT, $1, $3);
}
| AdditiveExpression RSIGNEDSHIFT NumericExpression
{
 $$=rasqal_new_2op_expression(RASQAL_EXPR_RSIGNEDSHIFT, $1, $3);
}
| AdditiveExpression RUNSIGNEDSHIFT NumericExpression
{
 $$=rasqal_new_2op_expression(RASQAL_EXPR_RUNSIGNEDSHIFT, $1, $3);
}
| AdditiveExpression
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

PatternLiteral: STRING_LITERAL
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

int
rdql_parse(rasqal_query* rq, const char *query_string) {
  void *buffer;

  /* FIXME LOCKING or re-entrant parser/lexer */

  Q=rq;

  buffer= rdql_lexer__scan_string(query_string);
  rdql_lexer__switch_to_buffer(buffer);
  rdql_parser_parse();

  rq=Q;
  
  Q=NULL;

  /* FIXME UNLOCKING or re-entrant parser/lexer */
  
  return 0;
}


#ifdef STANDALONE
#include <stdio.h>
#include <locale.h>

extern char *filename;
extern int lineno;
 
int
yyerror(const char *msg)
{
  fprintf(stderr, "%s:%d: %s\n", filename, lineno, msg);
  return (0);
}


#define RDQL_FILE_BUF_SIZE 2048

int
main(int argc, char *argv[]) 
{
  rasqal_query RQ;
  char query_string[RDQL_FILE_BUF_SIZE];
  FILE *fh;
  
  if(argc > 1) {
    filename=argv[1];
    fh = fopen(argv[1], "r");
  } else {
    filename="<stdin>";
    fh = stdin;
    puts("> ");
    fflush(stdout);
  }

  memset(query_string, 0, RDQL_FILE_BUF_SIZE);
  fread(query_string, RDQL_FILE_BUF_SIZE, 1, fh);
  
  if(argc>1)
    fclose(fh);
  
  rdql_parse(&RQ, query_string);

  return (0);
}
#endif

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

#include <rdql_parser.h>


int yyerror(const char *msg);

typedef struct
{
  int integer;
} query;

query Q;


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

extern FILE* yyin;

int
main(int argc, char *argv[]) 
{
  setlocale(LC_ALL, "");
  
  /* If the following parser is one created by lex, the
     application must be careful to ensure that LC_CTYPE
     and LC_COLLATE are set to the POSIX locale.  */

  if(argc > 1) {
    filename=argv[1];
    yyin = fopen(argv[1], "r");
  } else {
    filename="<stdin>";
    yyin = stdin;
    puts("> ");
    fflush(stdout);
  }


  (void) yyparse();
  return (0);
}
#endif


%}


/* Interface between lexer and parser */
%union {
  unsigned char *string;
  int integer;
  float floating;
}


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
%token INTEGER_LITERAL FLOATING_POINT_LITERAL
%token STRING_LITERAL
%token BOOLEAN_LITERAL NULL_LITERAL 
%token URI_LITERAL

%token IDENTIFIER

/* end of input */
%token END

/* syntax error */
%token ERROR

%%


Document : Query
;


Query : SELECT SelectClause SourceClause WHERE TriplePatternList ConstraintClause UsingClause
{
}
;

VarList : Var COMMA VarList 
{
}
| Var 
{
}
;


SelectClause : VarList | STAR
{
}
;

SourceClause : SOURCE URIList | FROM URIList
{
}
| /* empty */
{
}
;

/* Inlined into SourceClause: SourceSelector : URL */


TriplePatternList : TriplePattern COMMA TriplePatternList
{
}
| TriplePattern
{
}
;

/* Inlined:
 TriplePatternClause : WHERE TriplePatternList 
*/


/* FIXME was:
TriplePattern : LPAREN VarOrURI CommaOpt VarOrURI CommaOpt VarOrLiteral RPAREN
*/
TriplePattern : LPAREN VarOrURI COMMA VarOrURI COMMA VarOrLiteral RPAREN
{
}
;


/* Was:
ConstraintClause : AND Expression ( ( COMMA | AND ) Expression )*
*/

ConstraintClause : AND CommaAndConstraintClause
{
}
| /* empty */
{
}
;

CommaAndConstraintClause : Expression COMMA CommaAndConstraintClause
| Expression AND CommaAndConstraintClause
{
}
| Expression
{
}
;



UsingClause : USING PrefixDeclList
{
}
| /* empty */
{
}
;

PrefixDeclList : IDENTIFIER FOR URI_LITERAL COMMA PrefixDeclList 
{
}
| IDENTIFIER FOR URI_LITERAL
{
}
;


Expression : ConditionalAndExpression SC_OR Expression
{
}
| ConditionalAndExpression
{
}
;

ConditionalAndExpression: ValueLogical SC_AND ConditionalAndExpression
{
}
| ValueLogical
{
}
;

ValueLogical : InclusiveOrExpression STR_EQ InclusiveOrExpression
{
}
| InclusiveOrExpression STR_NE InclusiveOrExpression
{
}
| InclusiveOrExpression STR_MATCH PatternLiteral
{
}
| InclusiveOrExpression STR_NMATCH PatternLiteral
{
}
| InclusiveOrExpression
{
}
;


InclusiveOrExpression : ExclusiveOrExpression BIT_OR InclusiveOrExpression
{
}
| ExclusiveOrExpression
{
}
;

ExclusiveOrExpression : AndExpression BIT_XOR ExclusiveOrExpression
{
}
| AndExpression
{
}
;

AndExpression : EqualityExpression BIT_AND AndExpression
{
}
| EqualityExpression
{
}
;

EqualityExpression : RelationalExpression EQ RelationalExpression
| RelationalExpression NEQ RelationalExpression
{
}
| RelationalExpression
{
}
;

RelationalExpression : NumericExpression LT NumericExpression
| NumericExpression GT NumericExpression
| NumericExpression LE NumericExpression
| NumericExpression GE NumericExpression
{
}
| NumericExpression
{
}
;

NumericExpression : AdditiveExpression LSHIFT NumericExpression
| AdditiveExpression RSIGNEDSHIFT NumericExpression
| AdditiveExpression RUNSIGNEDSHIFT NumericExpression
{
}
| AdditiveExpression
{
}
;


AdditiveExpression : MultiplicativeExpression PLUS AdditiveExpression
| MultiplicativeExpression MINUS AdditiveExpression
{
}
| MultiplicativeExpression
{
}
;

MultiplicativeExpression : UnaryExpression STAR MultiplicativeExpression
| UnaryExpression SLASH MultiplicativeExpression
| UnaryExpression REM MultiplicativeExpression
{
}
| UnaryExpression
{
}
;

UnaryExpression : UnaryExpressionNotPlusMinus PLUS UnaryExpression 
| UnaryExpressionNotPlusMinus MINUS UnaryExpression
{
}
| UnaryExpressionNotPlusMinus
{
  /* FIXME - 2 shift/reduce conflicts here
   *
   * The original grammar and this one is ambiguous in allowing
   * PLUS/MINUS in UnaryExpression as well as AdditiveExpression
   */
}
;

UnaryExpressionNotPlusMinus : TILDE UnaryExpression
| BANG UnaryExpression
| Var
| Literal
| IDENTIFIER LPAREN ArgList RPAREN
| LPAREN Expression RPAREN
{
}
;

ArgList : VarOrLiteral COMMA ArgList
{
}
| VarOrLiteral
{
}
;

VarOrURI : Var | URI_LITERAL
{
}
;

VarOrLiteral : Var | Literal
{
}
;

Var : VARPREFIX IDENTIFIER
{
}
;

PatternLiteral: STRING_LITERAL ;


Literal : URI_LITERAL
| INTEGER_LITERAL
| FLOATING_POINT_LITERAL
| STRING_LITERAL
| BOOLEAN_LITERAL
| NULL_LITERAL
{
}
;

URIList : URI_LITERAL COMMA URIList
{
}
| URI_LITERAL
{
}
;

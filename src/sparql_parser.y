/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * brql_parser.y - Rasqal BRQL parser - over tokens from brql grammar lexer
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
 * BRQL defined in http://www.w3.org/2001/sw/DataAccess/rq23/ and
 * http://www.w3.org/2004/07/08-BRQL/
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

#include <brql_parser.h>

#define YY_DECL int brql_lexer_lex (YYSTYPE *brql_parser_lval, yyscan_t yyscanner)
#include <brql_lexer.h>

#include <brql_common.h>


/* Make verbose error messages for syntax errors */
#ifdef RASQAL_DEBUG
#define YYERROR_VERBOSE 1
#endif

/* Slow down the grammar operation and watch it work */
#if RASQAL_DEBUG > 2
#define YYDEBUG 1
#endif

/* the lexer does not seem to track this */
#undef RASQAL_BRQL_USE_ERROR_COLUMNS

/* Missing brql_lexer.c/h prototypes */
int brql_lexer_get_column(yyscan_t yyscanner);
/* Not used here */
/* void brql_lexer_set_column(int  column_no , yyscan_t yyscanner);*/


/* What the lexer wants */
extern int brql_lexer_lex (YYSTYPE *brql_parser_lval, yyscan_t scanner);
#define YYLEX_PARAM ((rasqal_brql_query_engine*)(((rasqal_query*)rq)->context))->scanner

/* Pure parser argument (a void*) */
#define YYPARSE_PARAM rq

/* Make the yyerror below use the rdf_parser */
#undef yyerror
#define yyerror(message) brql_query_error((rasqal_query*)rq, message)

/* Make lex/yacc interface as small as possible */
#undef yylex
#define yylex brql_lexer_lex


static int brql_parse(rasqal_query* rq, const unsigned char *string);
static int brql_query_error(rasqal_query* rq, const char *message);

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
%expect 2


/* word symbols */
%token SELECT SOURCE FROM WHERE AND FOR
%token OPTIONAL PREFIX DESCRIBE CONSTRUCT ASK NOT DISTINCT LIMIT

/* expression delimitors */

%token COMMA LPAREN RPAREN LSQUARE RSQUARE
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
%token <literal> FLOATING_POINT_LITERAL
%token <literal> STRING_LITERAL PATTERN_LITERAL INTEGER_LITERAL
%token <literal> BOOLEAN_LITERAL
%token <literal> NULL_LITERAL 
%token <uri> URI_LITERAL
%token <name> QNAME_LITERAL

%token <name> IDENTIFIER

/* syntax error */
%token ERROR


%type <seq> SelectClause SourceClause PrefixClause
%type <seq> VarList TriplePatternList PrefixDeclList URIList
%type <seq> ConstructClause

%type <expr> Expression ConditionalAndExpression ValueLogical
%type <expr> EqualityExpression RelationalExpression NumericExpression
%type <expr> AdditiveExpression MultiplicativeExpression UnaryExpression
%type <expr> UnaryExpressionNotPlusMinus
%type <literal> VarOrLiteral VarOrURI

%type <variable> Var
%type <triple> TriplePattern
%type <literal> PatternLiteral Literal

%%


Document : PrefixClause Query
{
  /* FIXME - should all be declared already */
  ((rasqal_query*)rq)->prefixes=$1;
}
;


Query : SELECT SelectClause SourceClause WHERE TriplePatternList
{
  ((rasqal_query*)rq)->selects=$2;
  ((rasqal_query*)rq)->sources=$3;
  ((rasqal_query*)rq)->triples=$5;
}
|  DESCRIBE VarList SourceClause WHERE TriplePatternList
{
  ((rasqal_query*)rq)->selects=$2;
  ((rasqal_query*)rq)->select_is_describe=1;
  ((rasqal_query*)rq)->sources=$3;
  ((rasqal_query*)rq)->triples=$5;
}
|  DESCRIBE URIList SourceClause
{
  ((rasqal_query*)rq)->describes=$2;
  ((rasqal_query*)rq)->select_is_describe=1;
}
|  CONSTRUCT ConstructClause SourceClause WHERE TriplePatternList
{
  ((rasqal_query*)rq)->constructs=$2;
  ((rasqal_query*)rq)->sources=$3;
  ((rasqal_query*)rq)->triples=$5;
}
;

VarList : Var VarList 
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

SourceClause : FROM URIList
{
  $$=$2;
}
| /* empty */
{
  $$=NULL;
}
;

/* Inlined into SourceClause: SourceSelector : URL */


TriplePatternList : TriplePatternList TriplePattern
{
  $$=$1;
  raptor_sequence_push($$, $2);
}
| TriplePatternList SOURCE VarOrURI TriplePattern
{
  $$=$1;
  rasqal_triple_set_origin($4, $3);
  raptor_sequence_push($$, $4);
}
| TriplePatternList LSQUARE TriplePatternList RSQUARE
{
  /* FIXME - should join sequence $3 to end of $$ */
  raptor_free_sequence($3);
  $$=$1;
}
| TriplePatternList OPTIONAL TriplePattern
{
  $$=$1;
  /* FIXME - should record optional triples */
  raptor_sequence_push($$, $3);
}
| TriplePatternList AND Expression
{
  raptor_sequence* cons;
  
  $$=$1;
  /* FIXME - should append $3 to constraints, an already inited sequence */
  cons=raptor_new_sequence(NULL, (raptor_sequence_print_handler*)rasqal_expression_print);
  raptor_sequence_push(cons, $3);
  
  ((rasqal_query*)rq)->constraints=cons;
}
| /* empty */
{
  $$=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_triple, (raptor_sequence_print_handler*)rasqal_triple_print);
}
;

/* Inlined:
 TriplePatternClause : WHERE TriplePatternList 
*/


TriplePattern : LPAREN VarOrURI VarOrURI VarOrLiteral RPAREN
{
  $$=rasqal_new_triple($2, $3, $4);
}
;

PrefixClause : PrefixDeclList
{
  $$=$1;
}
| /* empty */
{
  $$=NULL;
}
;

PrefixDeclList : PREFIX IDENTIFIER URI_LITERAL PrefixDeclList 
{
  rasqal_prefix *p=rasqal_new_prefix($2, $3);
  $$=$4;
  raptor_sequence_shift($$, p);
  rasqal_engine_declare_prefix(((rasqal_query*)rq), p);
}
| PREFIX IDENTIFIER URI_LITERAL
{
  rasqal_prefix *p=rasqal_new_prefix($2, $3);
  $$=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_prefix, (raptor_sequence_print_handler*)rasqal_prefix_print);
  raptor_sequence_push($$, p);
  rasqal_engine_declare_prefix(((rasqal_query*)rq), p);
}
;

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
  rasqal_literal *l=rasqal_new_variable_literal($1);
  $$=rasqal_new_literal_expression(l);
}
| Literal
{
  $$=rasqal_new_literal_expression($1);
}
| LPAREN Expression RPAREN
{
  $$=$2;
}
;

VarOrURI : Var
{
  $$=rasqal_new_variable_literal($1);
}
| URI_LITERAL
{
  $$=rasqal_new_uri_literal($1);
}
| QNAME_LITERAL
{
  $$=rasqal_new_simple_literal(RASQAL_LITERAL_QNAME, $1);
}
;

VarOrLiteral : Var
{
  $$=rasqal_new_variable_literal($1);
}
| Literal
{
  $$=$1;
}
;

Var : VARPREFIX IDENTIFIER
{
  $$=rasqal_new_variable((rasqal_query*)rq, $2, NULL);
}
;

PatternLiteral: PATTERN_LITERAL
{
  $$=$1;
}
;

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

URIList : URIList URI_LITERAL
{
  $$=$1;
  raptor_sequence_push($$, $2);
}
| /* empty */
{
  $$=raptor_new_sequence((raptor_sequence_free_handler*)raptor_free_uri, (raptor_sequence_print_handler*)raptor_sequence_print_uri);
}
;

%%


/* Support functions */


/* This is declared in brql_lexer.h but never used, so we always get
 * a warning unless this dummy code is here.  Used once below in an error case.
 */
static int yy_init_globals (yyscan_t yyscanner ) { return 0; };



/**
 * rasqal_brql_query_engine_init - Initialise the BRQL query engine
 *
 * Return value: non 0 on failure
 **/
static int
rasqal_brql_query_engine_init(rasqal_query* rdf_query, const char *name) {
  /* rasqal_brql_query_engine* brql=(rasqal_brql_query_engine*)rdf_query->context; */

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
 * rasqal_brql_query_engine_terminate - Free the BRQL query engine
 *
 * Return value: non 0 on failure
 **/
static void
rasqal_brql_query_engine_terminate(rasqal_query* rdf_query) {
  rasqal_brql_query_engine* brql=(rasqal_brql_query_engine*)rdf_query->context;

  if(brql->scanner_set) {
    brql_lexer_lex_destroy(brql->scanner);
    brql->scanner_set=0;
  }

}


static int
rasqal_brql_query_engine_prepare(rasqal_query* rdf_query) {
  /* rasqal_brql_query_engine* brql=(rasqal_brql_query_engine*)rdf_query->context; */

  if(rdf_query->query_string)
    return brql_parse(rdf_query, rdf_query->query_string);
  else
    return 0;
}


static int
rasqal_brql_query_engine_execute(rasqal_query* rdf_query) 
{
  /* rasqal_brql_query_engine* brql=(rasqal_brql_query_engine*)rdf_query->context; */
  
  /* FIXME: not implemented */
  return 0;
}


static int
brql_parse(rasqal_query* rq, const unsigned char *string) {
  rasqal_brql_query_engine* rqe=(rasqal_brql_query_engine*)rq->context;
  raptor_locator *locator=&rq->locator;
  void *buffer;

  if(!string || !*string)
    return yy_init_globals(NULL); /* 0 but a way to use yy_init_globals */

  locator->line=1;
  locator->column= -1; /* No column info */
  locator->byte= -1; /* No bytes info */

  rqe->lineno=1;

  brql_lexer_lex_init(&rqe->scanner);
  rqe->scanner_set=1;

  brql_lexer_set_extra(((rasqal_query*)rq), rqe->scanner);
  buffer= brql_lexer__scan_string((const char*)string, rqe->scanner);

  brql_parser_parse(rq);

  brql_lexer_lex_destroy(rqe->scanner);
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
brql_query_error(rasqal_query *rq, const char *msg) {
  rasqal_brql_query_engine* rqe=(rasqal_brql_query_engine*)rq->context;

  rq->locator.line=rqe->lineno;
#ifdef RASQAL_BRQL_USE_ERROR_COLUMNS
  /*  rq->locator.column=brql_lexer_get_column(yyscanner);*/
#endif

  rasqal_query_error(rq, "%s", msg);

  return 0;
}


int
brql_syntax_error(rasqal_query *rq, const char *message, ...)
{
  rasqal_brql_query_engine *rqe=(rasqal_brql_query_engine*)rq->context;
  va_list arguments;

  rq->locator.line=rqe->lineno;
#ifdef RASQAL_BRQL_USE_ERROR_COLUMNS
  /*  rp->locator.column=brql_lexer_get_column(yyscanner);*/
#endif

  va_start(arguments, message);
  rasqal_query_error_varargs(rq, message, arguments);
  va_end(arguments);

   return (0);
}


int
brql_syntax_warning(rasqal_query *rq, const char *message, ...)
{
  rasqal_brql_query_engine *rqe=(rasqal_brql_query_engine*)rq->context;
  va_list arguments;

  rq->locator.line=rqe->lineno;
#ifdef RASQAL_BRQL_USE_ERROR_COLUMNS
  /*  rq->locator.column=brql_lexer_get_column(yyscanner);*/
#endif

  va_start(arguments, message);
  rasqal_query_warning_varargs(rq, message, arguments);
  va_end(arguments);

   return (0);
}


static void
rasqal_brql_query_engine_register_factory(rasqal_query_engine_factory *factory)
{
  factory->context_length = sizeof(rasqal_brql_query_engine);

  factory->init      = rasqal_brql_query_engine_init;
  factory->terminate = rasqal_brql_query_engine_terminate;
  factory->prepare   = rasqal_brql_query_engine_prepare;
  factory->execute   = rasqal_brql_query_engine_execute;
}


void
rasqal_init_query_engine_brql (void) {
  rasqal_query_engine_register_factory("sparql", 
                                       "SPARQL W3C DAWG RDF Query Language",
                                       "brql",
                                       (const unsigned char*)"http://www.w3.org/2001/sw/DataAccess/rq23/",
                                       &rasqal_brql_query_engine_register_factory);
}



#ifdef STANDALONE
#include <stdio.h>
#include <locale.h>

#define BRQL_FILE_BUF_SIZE 2048

int
main(int argc, char *argv[]) 
{
  char query_string[BRQL_FILE_BUF_SIZE];
  rasqal_query query; /* static */
  rasqal_brql_query_engine brql; /* static */
  raptor_locator *locator=&query.locator;
  FILE *fh;
  int rc;
  char *filename=NULL;
  raptor_uri_handler *uri_handler;
  void *uri_context;

#if RASQAL_DEBUG > 2
  brql_parser_debug=1;
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

  memset(query_string, 0, BRQL_FILE_BUF_SIZE);
  fread(query_string, BRQL_FILE_BUF_SIZE, 1, fh);
  
  if(argc>1)
    fclose(fh);

  raptor_uri_init();

  memset(&query, 0, sizeof(rasqal_query));
  memset(&brql, 0, sizeof(rasqal_brql_query_engine));

  raptor_uri_get_handler(&uri_handler, &uri_context);
  query.namespaces=raptor_new_namespaces(uri_handler, uri_context,
                                         rasqal_query_simple_error,
                                         &query,
                                         0);
  query.variables_sequence=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_variable, (raptor_sequence_print_handler*)rasqal_variable_print);
  
  locator->line= locator->column = -1;
  locator->file= filename;

  brql.lineno= 1;

  query.context=&brql;
  query.base_uri=raptor_new_uri(raptor_uri_filename_to_uri_string(filename));

  rasqal_brql_query_engine_init(&query, "brql");

  rc=brql_parse(&query, (const unsigned char*)query_string);

  raptor_free_namespaces(query.namespaces);

  raptor_free_uri(query.base_uri);

  return rc;
}
#endif

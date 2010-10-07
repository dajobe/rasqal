/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rdql_parser.y - Rasqal RDQL parser - over tokens from rdql grammar lexer
 *
 * Copyright (C) 2003-2009, David Beckett http://www.dajobe.org/
 * Copyright (C) 2003-2005, University of Bristol, UK http://www.bristol.ac.uk/
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

#include <rdql_parser.h>

#define YY_DECL int rdql_lexer_lex (YYSTYPE *rdql_parser_lval, yyscan_t yyscanner)
#define YY_NO_UNISTD_H 1
#include <rdql_lexer.h>

#include <rdql_common.h>


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
#undef RASQAL_RDQL_USE_ERROR_COLUMNS

/* Missing rdql_lexer.c/h prototypes */
int rdql_lexer_get_column(yyscan_t yyscanner);
/* Not used here */
/* void rdql_lexer_set_column(int  column_no , yyscan_t yyscanner);*/


/* What the lexer wants */
extern int rdql_lexer_lex (YYSTYPE *rdql_parser_lval, yyscan_t scanner);
#define YYLEX_PARAM ((rasqal_rdql_query_language*)(((rasqal_query*)rq)->context))->scanner

/* Pure parser argument (a void*) */
#define YYPARSE_PARAM rq

/* Make the yyerror below use the rdf_parser */
#undef yyerror
#define yyerror(message) rdql_query_error((rasqal_query*)rq, message)

/* Make lex/yacc interface as small as possible */
#undef yylex
#define yylex rdql_lexer_lex


static int rdql_parse(rasqal_query* rq);
static void rdql_query_error(rasqal_query* rq, const char *message);

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
 * No conflicts
 */
%expect 0


/* word symbols */
%token SELECT SOURCE FROM WHERE AND FOR

/* expression delimitors */

%token ',' '(' ')'
%token '?'
%token USING


/* SC booleans */
%left SC_OR "||"
%left SC_AND "&&"

/* string operations */
%left STR_EQ "eq"
%left STR_NE "ne"
%left STR_MATCH "=~"
%left STR_NMATCH "!~"

/* operations */
%left EQ "="
%left NEQ "!="
%left LT "<"
%left GT ">"
%left LE "<="
%left GE ">="

/* arithmetic operations */
%left '+' '-' '*' '/' '%'

/* unary operations */
%left '~' '!'

/* literals */
%token <literal> FLOATING_POINT_LITERAL "floating point literal"
%token <literal> STRING_LITERAL "string literal"
%token <literal> INTEGER_LITERAL "integer literal"
%token <literal> PATTERN_LITERAL "pattern literal"
%token <literal> BOOLEAN_LITERAL "boolean literal"
%token <literal> NULL_LITERAL "null"
%token <uri> URI_LITERAL "URI literal"
%token <name> QNAME_LITERAL "QName literal"

%token <name> IDENTIFIER "identifier"


%type <seq> SelectClause SourceClause UsingClause
%type <seq> VarList TriplePatternList PrefixDeclList URIList

%type <expr> Expression ConditionalAndExpression ValueLogical
%type <expr> EqualityExpression RelationalExpression NumericExpression
%type <expr> AdditiveExpression MultiplicativeExpression UnaryExpression
%type <expr> UnaryExpressionNotPlusMinus
%type <expr> ConstraintClause CommaAndConstraintClause

%type <literal> VarOrLiteral VarOrURI

%type <variable> Var
%type <triple> TriplePattern
%type <literal> PatternLiteral Literal

%destructor {
  if($$)
    rasqal_free_literal($$);
} FLOATING_POINT_LITERAL STRING_LITERAL INTEGER_LITERAL PATTERN_LITERAL BOOLEAN_LITERAL NULL_LITERAL

%destructor {
  if($$)
    raptor_free_uri($$);
} URI_LITERAL

%destructor {
  if($$)
    RASQAL_FREE(cstring, $$);
} QNAME_LITERAL IDENTIFIER

%%


Document : Query
;


Query : SELECT SelectClause SourceClause WHERE TriplePatternList ConstraintClause UsingClause
{
  ((rasqal_query*)rq)->selects=$2;
  ((rasqal_query*)rq)->verb=RASQAL_QUERY_VERB_SELECT;

  if($3) {
    int i;
    
    for(i=0; i < raptor_sequence_size($3); i++) {
      raptor_uri* uri = (raptor_uri*)raptor_sequence_get_at($3, i);
      rasqal_query_add_data_graph_from_uri((rasqal_query*)rq, uri, NULL,
                                              RASQAL_DATA_GRAPH_BACKGROUND,
                                              NULL, NULL, NULL);
    }
    raptor_free_sequence($3);
  }

  /* ignoring $5 (sequence of triples): set in TriplePatternList to
   * ((rasqal_query*)rq)->triples=$5; 
   */

  /* $6 (expression): ConstraintClause */
  if($6) {
    rasqal_rdql_query_language* rdql=(rasqal_rdql_query_language*)((rasqal_query*)rq)->context;
    rdql->constraint_expression=$6;
  }

  /* ignoring $7 set in UsingClause ? */
}
;

VarList : VarList ',' Var
{
  $$=$1;
  raptor_sequence_push($$, $3);
}
| VarList Var
{
  $$=$1;
  raptor_sequence_push($$, $2);
}
| Var
{
  /* The variables are freed from the rasqal_query field variables */
#ifdef HAVE_RAPTOR2_API
  $$ = raptor_new_sequence(NULL, (raptor_data_print_handler)rasqal_variable_print);
#else
  $$ = raptor_new_sequence(NULL, (raptor_sequence_print_handler*)rasqal_variable_print);
#endif
  raptor_sequence_push($$, $1);
}
;


SelectClause : VarList
{
  $$=$1;
}
| '*'
{
  $$=NULL;
  ((rasqal_query*)rq)->wildcard=1;
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


/* Jena RDQL allows optional ',' */
TriplePatternList : TriplePatternList ',' TriplePattern
{
  $$=$1;
  raptor_sequence_push($$, $3);
}
| TriplePatternList TriplePattern
{
  $$=$1;
  raptor_sequence_push($$, $2);
}
| TriplePattern
{
  $$=((rasqal_query*)rq)->triples;
  raptor_sequence_push($$, $1);
}
;

/* Inlined:
 TriplePatternClause : WHERE TriplePatternList 
*/


/* FIXME - maybe a better way to do this optional COMMA? */
TriplePattern : '(' VarOrURI ',' VarOrURI ',' VarOrLiteral ')'
{
  $$=rasqal_new_triple($2, $4, $6);
}
| '(' VarOrURI VarOrURI ',' VarOrLiteral ')'
{
  $$=rasqal_new_triple($2, $3, $5);
}
| '(' VarOrURI ',' VarOrURI VarOrLiteral ')'
{
  $$=rasqal_new_triple($2, $4, $5);
}
| '(' VarOrURI VarOrURI VarOrLiteral ')'
{
  $$=rasqal_new_triple($2, $3, $4);
}
;


/* Was:
ConstraintClause : AND Expression ( ( ',' | AND ) Expression )*
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

CommaAndConstraintClause : CommaAndConstraintClause ',' Expression
{
  $$=rasqal_new_2op_expression(((rasqal_query*)rq)->world, RASQAL_EXPR_AND, $1, $3);
}
| CommaAndConstraintClause AND Expression
{
  $$=rasqal_new_2op_expression(((rasqal_query*)rq)->world, RASQAL_EXPR_AND, $1, $3);
}
| Expression
{
  $$=$1;
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

PrefixDeclList : IDENTIFIER FOR URI_LITERAL ',' PrefixDeclList 
{
  $$=((rasqal_query*)rq)->prefixes;
  raptor_sequence_shift($$, rasqal_new_prefix(((rasqal_query*)rq)->world, $1, $3));
}
| IDENTIFIER FOR URI_LITERAL PrefixDeclList 
{
  $$=((rasqal_query*)rq)->prefixes;
  raptor_sequence_shift($$, rasqal_new_prefix(((rasqal_query*)rq)->world, $1, $3));
}
| IDENTIFIER FOR URI_LITERAL
{
  $$=((rasqal_query*)rq)->prefixes;
  raptor_sequence_push($$, rasqal_new_prefix(((rasqal_query*)rq)->world, $1, $3));
}
;


Expression : ConditionalAndExpression SC_OR Expression
{
  $$=rasqal_new_2op_expression(((rasqal_query*)rq)->world, RASQAL_EXPR_OR, $1, $3);
}
| ConditionalAndExpression
{
  $$=$1;
}
;

ConditionalAndExpression: ValueLogical SC_AND ConditionalAndExpression
{
  $$=rasqal_new_2op_expression(((rasqal_query*)rq)->world, RASQAL_EXPR_AND, $1, $3);
;
}
| ValueLogical
{
  $$=$1;
}
;

ValueLogical : EqualityExpression STR_EQ EqualityExpression
{
  $$=rasqal_new_2op_expression(((rasqal_query*)rq)->world, RASQAL_EXPR_STR_EQ, $1, $3);
}
| EqualityExpression STR_NE EqualityExpression
{
  $$=rasqal_new_2op_expression(((rasqal_query*)rq)->world, RASQAL_EXPR_STR_NEQ, $1, $3);
}
| EqualityExpression STR_MATCH PatternLiteral
{
  $$=rasqal_new_string_op_expression(((rasqal_query*)rq)->world, RASQAL_EXPR_STR_MATCH, $1, $3);
}
| EqualityExpression STR_NMATCH PatternLiteral
{
  $$=rasqal_new_string_op_expression(((rasqal_query*)rq)->world, RASQAL_EXPR_STR_NMATCH, $1, $3);
}
| EqualityExpression
{
  $$=$1;
}
;

EqualityExpression : RelationalExpression EQ RelationalExpression
{
  $$=rasqal_new_2op_expression(((rasqal_query*)rq)->world, RASQAL_EXPR_EQ, $1, $3);
}
| RelationalExpression NEQ RelationalExpression
{
  $$=rasqal_new_2op_expression(((rasqal_query*)rq)->world, RASQAL_EXPR_NEQ, $1, $3);
}
| RelationalExpression
{
  $$=$1;
}
;

RelationalExpression : NumericExpression LT NumericExpression
{
  $$=rasqal_new_2op_expression(((rasqal_query*)rq)->world, RASQAL_EXPR_LT, $1, $3);
}
| NumericExpression GT NumericExpression
{
  $$=rasqal_new_2op_expression(((rasqal_query*)rq)->world, RASQAL_EXPR_GT, $1, $3);
}
| NumericExpression LE NumericExpression
{
  $$=rasqal_new_2op_expression(((rasqal_query*)rq)->world, RASQAL_EXPR_LE, $1, $3);
}
| NumericExpression GE NumericExpression
{
  $$=rasqal_new_2op_expression(((rasqal_query*)rq)->world, RASQAL_EXPR_GE, $1, $3);
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


AdditiveExpression : MultiplicativeExpression '+' AdditiveExpression
{
  $$=rasqal_new_2op_expression(((rasqal_query*)rq)->world, RASQAL_EXPR_PLUS, $1, $3);
}
| MultiplicativeExpression '-' AdditiveExpression
{
  $$=rasqal_new_2op_expression(((rasqal_query*)rq)->world, RASQAL_EXPR_MINUS, $1, $3);
}
| MultiplicativeExpression
{
  $$=$1;
}
;

MultiplicativeExpression : UnaryExpression '*' MultiplicativeExpression
{
  $$=rasqal_new_2op_expression(((rasqal_query*)rq)->world, RASQAL_EXPR_STAR, $1, $3);
}
| UnaryExpression '/' MultiplicativeExpression
{
  $$=rasqal_new_2op_expression(((rasqal_query*)rq)->world, RASQAL_EXPR_SLASH, $1, $3);
}
| UnaryExpression '%' MultiplicativeExpression
{
  $$=rasqal_new_2op_expression(((rasqal_query*)rq)->world, RASQAL_EXPR_REM, $1, $3);
}
| UnaryExpression
{
  $$=$1;
}
;

UnaryExpression : '+' UnaryExpression 
{
  $$=$2;
}
| '-' UnaryExpression
{
  $$=rasqal_new_1op_expression(((rasqal_query*)rq)->world, RASQAL_EXPR_UMINUS, $2);
}
| UnaryExpressionNotPlusMinus
{
  $$=$1;
}
;

UnaryExpressionNotPlusMinus : '~' UnaryExpression
{
  $$=rasqal_new_1op_expression(((rasqal_query*)rq)->world, RASQAL_EXPR_TILDE, $2);
}
| '!' UnaryExpression
{
  $$=rasqal_new_1op_expression(((rasqal_query*)rq)->world, RASQAL_EXPR_BANG, $2);
}
| Var
{
  rasqal_literal *l=rasqal_new_variable_literal(((rasqal_query*)rq)->world, $1);
  $$=rasqal_new_literal_expression(((rasqal_query*)rq)->world, l);
}
| Literal
{
  $$=rasqal_new_literal_expression(((rasqal_query*)rq)->world, $1);
}
| '(' Expression ')'
{
  $$=$2;
}
;

VarOrURI : Var
{
  $$=rasqal_new_variable_literal(((rasqal_query*)rq)->world, $1);
}
| URI_LITERAL
{
  $$=rasqal_new_uri_literal(((rasqal_query*)rq)->world, $1);
}
| QNAME_LITERAL
{
  $$=rasqal_new_simple_literal(((rasqal_query*)rq)->world, RASQAL_LITERAL_QNAME, $1);
}
;

VarOrLiteral : Var
{
  $$=rasqal_new_variable_literal(((rasqal_query*)rq)->world, $1);
}
| Literal
{
  $$=$1;
}
;

Var : '?' IDENTIFIER
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
  $$=rasqal_new_uri_literal(((rasqal_query*)rq)->world, $1);
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
  $$=rasqal_new_simple_literal(((rasqal_query*)rq)->world, RASQAL_LITERAL_QNAME, $1);
}

;

URIList : URI_LITERAL ',' URIList
{
  $$=$3;
  raptor_sequence_shift($$, $1);
}
| URI_LITERAL
{
#ifdef HAVE_RAPTOR2_API
  $$ = raptor_new_sequence((raptor_data_free_handler)raptor_free_uri,
                           (raptor_data_print_handler)raptor_uri_print);
#else
  $$ = raptor_new_sequence((raptor_sequence_free_handler*)raptor_free_uri,
                           (raptor_sequence_print_handler*)raptor_uri_print);
#endif
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
 * rasqal_rdql_query_language_init - Initialise the RDQL query language
 *
 * Return value: non 0 on failure
 **/
static int
rasqal_rdql_query_language_init(rasqal_query* rdf_query, const char *name) {
  /* rasqal_rdql_query_language* rdql=(rasqal_rdql_query_language*)rdf_query->context; */

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

  rdf_query->compare_flags = RASQAL_COMPARE_URI;

  return 0;
}


/**
 * rasqal_rdql_query_language_terminate - Free the RDQL query language
 *
 * Return value: non 0 on failure
 **/
static void
rasqal_rdql_query_language_terminate(rasqal_query* rdf_query) {
  rasqal_rdql_query_language* rdql=(rasqal_rdql_query_language*)rdf_query->context;

  if(rdql->scanner_set) {
    rdql_lexer_lex_destroy(rdql->scanner);
    rdql->scanner_set=0;
  }

}


static int
rasqal_rdql_query_language_prepare(rasqal_query* rdf_query) {
  rasqal_rdql_query_language* rdql=(rasqal_rdql_query_language*)rdf_query->context;
  int rc;
  rasqal_graph_pattern *gp;
  
  if(!rdf_query->query_string)
    return 1;

  rdql->constraint_expression=NULL;
  
  rc=rdql_parse(rdf_query);
  if(rc)
    return rc;

  rdf_query->query_graph_pattern=rasqal_new_graph_pattern_from_sequence(rdf_query, NULL, RASQAL_GRAPH_PATTERN_OPERATOR_GROUP);

  gp=rasqal_new_basic_graph_pattern(rdf_query, rdf_query->triples,
                                    0, raptor_sequence_size(rdf_query->triples)-1);

  rasqal_graph_pattern_add_sub_graph_pattern(rdf_query->query_graph_pattern,
                                             gp);

  /* Add a FILTER graph pattern if there is a constraint expression */
  if(rdql->constraint_expression) {
    rasqal_graph_pattern* cgp;
    cgp=rasqal_new_filter_graph_pattern(rdf_query, rdql->constraint_expression);
    if(cgp)
      rasqal_graph_pattern_add_sub_graph_pattern(rdf_query->query_graph_pattern,
                                                 cgp);
    rdql->constraint_expression=NULL;
  }
  
  /* Only now can we handle the prefixes and qnames */
  if(rasqal_query_declare_prefixes(rdf_query) ||
     rasqal_query_expand_triple_qnames(rdf_query) ||
     rasqal_query_expand_query_constraints_qnames(rdf_query))
    return 1;

  /* RDQL: Expand 'SELECT *' */
  if(rasqal_query_expand_wildcards(rdf_query))
    return 1;
  
  return 0;
}


static int
rdql_parse(rasqal_query* rq) {
  rasqal_rdql_query_language* rqe=(rasqal_rdql_query_language*)rq->context;
  raptor_locator *locator=&rq->locator;
  void *buffer;
  
  if(!rq->query_string)
    return yy_init_globals(NULL); /* 0 but a way to use yy_init_globals */

  locator->line=1;
  locator->column= -1; /* No column info */
  locator->byte= -1; /* No bytes info */

#if RASQAL_DEBUG > 2
  rdql_parser_debug=1;
#endif

  rqe->lineno=1;

  rdql_lexer_lex_init(&rqe->scanner);
  rqe->scanner_set=1;

  rdql_lexer_set_extra(((rasqal_query*)rq), rqe->scanner);

  buffer= rdql_lexer__scan_buffer((char*)rq->query_string, rq->query_string_length, rqe->scanner);

  rqe->error_count=0;

  rdql_parser_parse(rq);

  rdql_lexer_lex_destroy(rqe->scanner);
  rqe->scanner_set=0;

  /* Parsing failed */
  if(rq->failed)
    return 1;
  
  return 0;
}


void
rdql_query_error(rasqal_query *rq, const char *msg) {
  rasqal_rdql_query_language* rqe=(rasqal_rdql_query_language*)rq->context;

  if(rqe->error_count++)
    return;

  rq->locator.line=rqe->lineno;
#ifdef RASQAL_RDQL_USE_ERROR_COLUMNS
  /*  rq->locator.column=rdql_lexer_get_column(yyscanner);*/
#endif

  rq->failed=1;
  rasqal_log_error_simple(rq->world, RAPTOR_LOG_LEVEL_FATAL,
                          &rq->locator, "%s", msg);

  return;
}


int
rdql_syntax_error(rasqal_query *rq, const char *message, ...)
{
  rasqal_rdql_query_language *rqe=(rasqal_rdql_query_language*)rq->context;
  va_list arguments;

  if(rqe->error_count++)
    return 0;

  rq->locator.line=rqe->lineno;
#ifdef RASQAL_RDQL_USE_ERROR_COLUMNS
  /*  rp->locator.column=rdql_lexer_get_column(yyscanner);*/
#endif

  va_start(arguments, message);
  rq->failed=1;
  rasqal_log_error_varargs(rq->world, RAPTOR_LOG_LEVEL_FATAL, &rq->locator,
                           message, arguments);
  va_end(arguments);

  return 0;
}


int
rdql_syntax_warning(rasqal_query *rq, const char *message, ...)
{
  rasqal_rdql_query_language *rqe=(rasqal_rdql_query_language*)rq->context;
  va_list arguments;

  rq->locator.line=rqe->lineno;
#ifdef RASQAL_RDQL_USE_ERROR_COLUMNS
  /*  rq->locator.column=rdql_lexer_get_column(yyscanner);*/
#endif

  va_start(arguments, message);
  rasqal_log_error_varargs(rq->world,
                           RAPTOR_LOG_LEVEL_WARN,
                           &rq->locator,
                           message, arguments);
  va_end(arguments);

   return (0);
}


static const char* const rdql_names[2] = { "rdql", NULL};

#define RDQL_TYPES_COUNT 1
static const raptor_type_q rdql_types[RDQL_TYPES_COUNT + 1] = {
  { "application/x-rdql", 18, 10}, 
  { NULL, 0, 0}
};

static int
rasqal_rdql_query_language_register_factory(rasqal_query_language_factory *factory)
{
  int rc = 0;

  factory->desc.names = rdql_names;

  factory->desc.mime_types = rdql_types;
  factory->desc.mime_types_count = RDQL_TYPES_COUNT;

  factory->desc.label = "RDF Data Query Language (RDQL)";

  /* http://www.w3.org/Submission/2004/SUBM-RDQL-20040109/ */
  factory->desc.uri_string = "http://jena.hpl.hp.com/2003/07/query/RDQL";

  factory->context_length = sizeof(rasqal_rdql_query_language);

  factory->init      = rasqal_rdql_query_language_init;
  factory->terminate = rasqal_rdql_query_language_terminate;
  factory->prepare   = rasqal_rdql_query_language_prepare;

  return rc;
}


int
rasqal_init_query_language_rdql(rasqal_world* world)
{
  return !rasqal_query_language_register_factory(world,
                                                 &rasqal_rdql_query_language_register_factory);
}



#ifdef STANDALONE
#include <stdio.h>
#include <locale.h>

#define RDQL_FILE_BUF_SIZE 2048

int
main(int argc, char *argv[]) 
{
  const char *program=rasqal_basename(argv[0]);
  char query_string[RDQL_FILE_BUF_SIZE];
  rasqal_query *query;
  FILE *fh;
  int rc;
  const char *filename=NULL;
  raptor_uri* base_uri=NULL;
  unsigned char *uri_string;
  rasqal_world *world;
  
#if RASQAL_DEBUG > 2
  rdql_parser_debug=1;
#endif

  filename = getenv("RDQL_QUERY_FILE");
  if(!filename) {
    if(argc != 2) {
      fprintf(stderr, "%s: Too many arguments.\n", program);
      fprintf(stderr, "RDQL parser test for Rasqal %s\n", 
              rasqal_version_string);
      fprintf(stderr, "USAGE: %s RDQL-QUERY-FILE\n", program);
      exit(1);
    }

   filename=argv[argc-1];
  }
  
  fh = fopen(filename, "r");
  if(!fh) {
    fprintf(stderr, "%s: Cannot open file %s - %s\n", program, filename,
            strerror(errno));
    exit(1);
  }
  
  memset(query_string, 0, RDQL_FILE_BUF_SIZE);
  rc=fread(query_string, RDQL_FILE_BUF_SIZE, 1, fh);
  if(rc < RDQL_FILE_BUF_SIZE) {
    if(ferror(fh)) {
      fprintf(stderr, "%s: file '%s' read failed - %s\n",
              program, filename, strerror(errno));
      fclose(fh);
      return(1);
    }
  }
  
  fclose(fh);

  world=rasqal_new_world();
  if(!world || rasqal_world_open(world))
    exit(1);

  query=rasqal_new_query(world, "rdql", NULL);

  uri_string=raptor_uri_filename_to_uri_string(filename);
  base_uri = raptor_new_uri(world->raptor_world_ptr, uri_string);
  
  rc=rasqal_query_prepare(query, (const unsigned char*)query_string, base_uri);

  rasqal_query_print(query, stdout);

  rasqal_free_query(query);

  raptor_free_uri(base_uri);

  raptor_free_memory(uri_string);

  rasqal_free_world(world);

  return rc;
}
#endif

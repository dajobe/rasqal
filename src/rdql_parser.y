/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rdql_parser.y - Rasqal RDQL parser - over tokens from rdql grammar lexer
 *
 * $Id$
 *
 * Copyright (C) 2003-2005, David Beckett http://purl.org/net/dajobe/
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
#define YYLEX_PARAM ((rasqal_rdql_query_engine*)(((rasqal_query*)rq)->context))->scanner

/* Pure parser argument (a void*) */
#define YYPARSE_PARAM rq

/* Make the yyerror below use the rdf_parser */
#undef yyerror
#define yyerror(message) rdql_query_error((rasqal_query*)rq, message)

/* Make lex/yacc interface as small as possible */
#undef yylex
#define yylex rdql_lexer_lex


static int rdql_parse(rasqal_query* rq, const unsigned char *string);
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


%type <seq> SelectClause SourceClause ConstraintClause UsingClause
%type <seq> CommaAndConstraintClause
%type <seq> VarList TriplePatternList PrefixDeclList URIList

%type <expr> Expression ConditionalAndExpression ValueLogical
%type <expr> EqualityExpression RelationalExpression NumericExpression
%type <expr> AdditiveExpression MultiplicativeExpression UnaryExpression
%type <expr> UnaryExpressionNotPlusMinus
%type <literal> VarOrLiteral VarOrURI

%type <variable> Var
%type <triple> TriplePattern
%type <literal> PatternLiteral Literal

%%


Document : Query
;


Query : SELECT SelectClause SourceClause WHERE TriplePatternList ConstraintClause UsingClause
{
  ((rasqal_query*)rq)->selects=$2;

  if($3) {
    int i;
    
    for(i=0; i < raptor_sequence_size($3); i++) {
      raptor_uri* uri=(raptor_uri*)raptor_sequence_get_at($3, i);
      rasqal_query_add_data_graph((rasqal_query*)rq, uri, NULL, RASQAL_DATA_GRAPH_BACKGROUND);
    }
    raptor_free_sequence($3);
  }

  /* ignoring $5 sequence, set in TriplePatternList to
   * ((rasqal_query*)rq)->triples=$5; 
   */

  /* ignoring $6 sequence, set in ConstraintClause */
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
  $$=raptor_new_sequence(NULL, (raptor_sequence_print_handler*)rasqal_variable_print);
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
  $$=NULL;
}
| /* empty */
{
  $$=NULL;
}
;

CommaAndConstraintClause : CommaAndConstraintClause ',' Expression
{
  raptor_sequence_push(((rasqal_query*)rq)->constraints_sequence, $3);
  $$=NULL;
}
| CommaAndConstraintClause AND Expression
{
  raptor_sequence_push(((rasqal_query*)rq)->constraints_sequence, $3);
  $$=NULL;
}
| Expression
{
  raptor_sequence_push(((rasqal_query*)rq)->constraints_sequence, $1);
  $$=NULL;
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
  raptor_sequence_shift($$, rasqal_new_prefix($1, $3));
}
| IDENTIFIER FOR URI_LITERAL PrefixDeclList 
{
  $$=((rasqal_query*)rq)->prefixes;
  raptor_sequence_shift($$, rasqal_new_prefix($1, $3));
}
| IDENTIFIER FOR URI_LITERAL
{
  $$=((rasqal_query*)rq)->prefixes;
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
| '(' Expression ')'
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

URIList : URI_LITERAL ',' URIList
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
 * rasqal_rdql_query_engine_terminate - Free the RDQL query engine
 *
 * Return value: non 0 on failure
 **/
static void
rasqal_rdql_query_engine_terminate(rasqal_query* rdf_query) {
  rasqal_rdql_query_engine* rdql=(rasqal_rdql_query_engine*)rdf_query->context;

  if(rdql->scanner_set) {
    rdql_lexer_lex_destroy(rdql->scanner);
    rdql->scanner_set=0;
  }

}


static int
rasqal_rdql_query_engine_prepare(rasqal_query* rdf_query) {
  /* rasqal_rdql_query_engine* rdql=(rasqal_rdql_query_engine*)rdf_query->context; */
  int rc;
  rasqal_graph_pattern *gp;
  
  if(!rdf_query->query_string)
    return 1;

  /* for RDQL only, before the graph pattern is made */
  rdf_query->constraints_sequence=raptor_new_sequence(NULL, (raptor_sequence_print_handler*)rasqal_expression_print);
  
  rc=rdql_parse(rdf_query, rdf_query->query_string);
  if(rc)
    return rc;

  gp=rasqal_new_graph_pattern_from_triples(rdf_query,
                                           rdf_query->triples,
                                           0, raptor_sequence_size(rdf_query->triples)-1,
                                           0);
  rasqal_graph_pattern_add_sub_graph_pattern(rdf_query->query_graph_pattern,
                                             gp);

  /* Now assign the constraints to the graph pattern */
  while(raptor_sequence_size(rdf_query->constraints_sequence)) {
    rasqal_expression* e=(rasqal_expression*)raptor_sequence_pop(rdf_query->constraints_sequence);
    rasqal_graph_pattern_add_constraint(gp, e);
  }
  raptor_free_sequence(rdf_query->constraints_sequence);

  /* Only now can we handle the prefixes and qnames */
  if(rasqal_engine_declare_prefixes(rdf_query) ||
     rasqal_engine_expand_triple_qnames(rdf_query) ||
     rasqal_engine_expand_query_constraints_qnames(rdf_query))
    return 1;

  return rasqal_engine_prepare(rdf_query);
}


static int
rasqal_rdql_query_engine_execute(rasqal_query* rdf_query, 
                                 rasqal_query_results* results) 
{
  /* rasqal_rdql_query_engine* rdql=(rasqal_rdql_query_engine*)rdf_query->context; */
  
  /* FIXME: not implemented */
  return 0;
}


static int
rdql_parse(rasqal_query* rq, const unsigned char *string) {
  rasqal_rdql_query_engine* rqe=(rasqal_rdql_query_engine*)rq->context;
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
  rdql_parser_debug=1;
#endif

  rqe->lineno=1;

  rdql_lexer_lex_init(&rqe->scanner);
  rqe->scanner_set=1;

  rdql_lexer_set_extra(((rasqal_query*)rq), rqe->scanner);

  /* This
   *   buffer= rdql_lexer__scan_string((const char*)string, rqe->scanner);
   * is replaced by the code below.  
   * 
   * The extra space appended to the buffer is the least-pain
   * workaround to the lexer crashing by reading EOF twice in
   * rdql_copy_regex_token; at least as far as I can diagnose.  The
   * fix here costs little to add as the above function does
   * something very similar to this anyway.
   */
  len= strlen((const char*)string);
  buf= (char *)RASQAL_MALLOC(cstring, len+3);
  strncpy(buf, (const char*)string, len);
  buf[len]= ' ';
  buf[len+1]= buf[len+2]='\0'; /* YY_END_OF_BUFFER_CHAR; */
  buffer= rdql_lexer__scan_buffer(buf, len+3, rqe->scanner);

  rdql_parser_parse(rq);

  if(buf)
    RASQAL_FREE(cstring, buf);

  rdql_lexer_lex_destroy(rqe->scanner);
  rqe->scanner_set=0;

  /* Parsing failed */
  if(rq->failed)
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

  rasqal_query_error(rq, "%s", msg);

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
  const char *program=rasqal_basename(argv[0]);
  char query_string[RDQL_FILE_BUF_SIZE];
  rasqal_query *query;
  FILE *fh;
  int rc;
  char *filename=NULL;
  raptor_uri* base_uri=NULL;
  unsigned char *uri_string;

#if RASQAL_DEBUG > 2
  rdql_parser_debug=1;
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

  memset(query_string, 0, RDQL_FILE_BUF_SIZE);
  fread(query_string, RDQL_FILE_BUF_SIZE, 1, fh);
  
  if(argc>1)
    fclose(fh);

  rasqal_init();

  query=rasqal_new_query("rdql", NULL);

  uri_string=raptor_uri_filename_to_uri_string(filename);
  base_uri=raptor_new_uri(uri_string);
  
  rc=rasqal_query_prepare(query, (const unsigned char*)query_string, base_uri);

  rasqal_query_print(query, stdout);

  rasqal_free_query(query);

  raptor_free_uri(base_uri);

  raptor_free_memory(uri_string);

  rasqal_finish();

  return rc;
}
#endif

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * sparql_parser.y - Rasqal SPARQL parser over tokens from sparql_lexer.l
 *
 * Copyright (C) 2004-2010, David Beckett http://www.dajobe.org/
 * Copyright (C) 2004-2005, University of Bristol, UK http://www.bristol.ac.uk/
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
 *   SPARQL Query Language for RDF, W3C Recommendation 15 January 2008
 *   http://www.w3.org/TR/2008/REC-rdf-sparql-query-20080115/
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
#define YY_NO_UNISTD_H 1
#include <sparql_lexer.h>

#include <sparql_common.h>


#if 0
#undef RASQAL_DEBUG
#define RASQAL_DEBUG 3
#endif


#define DEBUG_FH stderr

/* Make verbose error messages for syntax errors */
#define YYERROR_VERBOSE 1

/* Fail with an debug error message if RASQAL_DEBUG > 1 */
#if RASQAL_DEBUG > 1
#define YYERROR_MSG(msg) do { fputs("** YYERROR ", DEBUG_FH); fputs(msg, DEBUG_FH); fputc('\n', DEBUG_FH); YYERROR; } while(0)
#else
#define YYERROR_MSG(ignore) YYERROR
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
#define YYLEX_PARAM ((rasqal_sparql_query_language*)(((rasqal_query*)rq)->context))->scanner

/* Pure parser argument (a void*) */
#define YYPARSE_PARAM rq

/* Make the yyerror below use the rdf_parser */
#undef yyerror
#define yyerror(message) sparql_query_error((rasqal_query*)rq, message)

/* Make lex/yacc interface as small as possible */
#undef yylex
#define yylex sparql_lexer_lex


static int sparql_parse(rasqal_query* rq);
static void sparql_query_error(rasqal_query* rq, const char *message);
static void sparql_query_error_full(rasqal_query *rq, const char *message, ...) RASQAL_PRINTF_FORMAT(2, 3);

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
  rasqal_formula *formula;
}


/*
 * shift/reduce conflicts
 * FIXME: document this
 */
%expect 5

/* word symbols */
%token SELECT FROM WHERE
%token OPTIONAL DESCRIBE CONSTRUCT ASK DISTINCT REDUCED LIMIT UNION
%token PREFIX BASE BOUND
%token GRAPH NAMED FILTER OFFSET ORDER BY REGEX ASC DESC LANGMATCHES
%token A "a"
%token STR "str"
%token LANG "lang"
%token DATATYPE "datatype"
%token ISURI "isUri"
%token ISBLANK "isBlank"
%token ISLITERAL "isLiteral"
%token SAMETERM "sameTerm"
/* SPARQL 1.1 (draft) / LAQRS */
%token EXPLAIN GROUP COUNT SUM AVG MIN MAX
%token DELETE INSERT WITH CLEAR CREATE SILENT DATA DROP LOAD INTO
/* LAQRS */
%token LET AS
%token COALESCE

/* expression delimiters */

%token ',' '(' ')' '[' ']' '{' '}'
%token '?' '$'

/* SC booleans */
%left SC_OR
%left SC_AND

/* operations */
%left EQ
%left NEQ
%left LT
%left GT
%left LE
%left GE

/* arithmetic operations */
%left '+' '-' '*' '/'

/* unary operations */

/* laqrs operations */
%token ASSIGN ":="

/* literals */
%token <literal> STRING_LITERAL "string literal"
%token <literal> DOUBLE_LITERAL "double literal"
%token <literal> DOUBLE_POSITIVE_LITERAL "double positive literal"
%token <literal> DOUBLE_NEGATIVE_LITERAL "double negative literal"
%token <literal> INTEGER_LITERAL "integer literal"
%token <literal> INTEGER_POSITIVE_LITERAL "integer positive literal"
%token <literal> INTEGER_NEGATIVE_LITERAL "integer negative literal"
%token <literal> DECIMAL_LITERAL "decimal literal"
%token <literal> DECIMAL_POSITIVE_LITERAL "decimal positive literal"
%token <literal> DECIMAL_NEGATIVE_LITERAL "decimal negative literal"
%token <literal> BOOLEAN_LITERAL "boolean literal"

%token <uri> URI_LITERAL "URI literal"
%token <uri> URI_LITERAL_BRACE "URI literal ("

%token <name> QNAME_LITERAL "QName literal"
%token <name> QNAME_LITERAL_BRACE "QName literal ("
%token <name> BLANK_LITERAL "blank node literal"
%token <name> IDENTIFIER "identifier"


%type <seq> SelectQuery ConstructQuery DescribeQuery
%type <seq> SelectExpressionList VarOrIRIrefList ArgList
%type <seq> ConstructTriples ConstructTriplesOpt
%type <seq> ConstructTemplate OrderConditionList
%type <seq> GraphNodeListNotEmpty SelectExpressionListTail
%type <seq> GraphTriples

%type <formula> TriplesSameSubject
%type <formula> PropertyList PropertyListTailOpt PropertyListNotEmpty
%type <formula> ObjectList ObjectTail Collection
%type <formula> VarOrTerm Verb Object GraphNode TriplesNode
%type <formula> BlankNodePropertyList
%type <formula> TriplesBlock TriplesBlockOpt

%type <graph_pattern> GroupGraphPattern
%type <graph_pattern> GraphGraphPattern OptionalGraphPattern
%type <graph_pattern> GroupOrUnionGraphPattern GroupOrUnionGraphPatternList
%type <graph_pattern> GraphPatternNotTriples
%type <graph_pattern> GraphPatternListOpt GraphPatternList GraphPatternListFilter
%type <graph_pattern> LetGraphPattern

%type <expr> Expression ConditionalOrExpression ConditionalAndExpression
%type <expr> RelationalExpression AdditiveExpression
%type <expr> MultiplicativeExpression UnaryExpression
%type <expr> BuiltInCall RegexExpression FunctionCall
%type <expr> BrackettedExpression PrimaryExpression
%type <expr> OrderCondition Filter Constraint SelectExpression
%type <expr> AggregateExpression CountAggregateExpression SumAggregateExpression
%type <expr> AvgAggregateExpression MinAggregateExpression MaxAggregateExpression
%type <expr> CoalesceExpression

%type <literal> GraphTerm IRIref BlankNode
%type <literal> VarOrIRIref
%type <literal> IRIrefBrace SourceSelector
%type <literal> NumericLiteral NumericLiteralUnsigned
%type <literal> NumericLiteralPositive NumericLiteralNegative

%type <variable> Var VarName SelectTerm


%destructor {
  if($$)
    rasqal_free_literal($$);
}
STRING_LITERAL 
DOUBLE_LITERAL INTEGER_LITERAL DECIMAL_LITERAL
DOUBLE_POSITIVE_LITERAL DOUBLE_NEGATIVE_LITERAL
INTEGER_POSITIVE_LITERAL INTEGER_NEGATIVE_LITERAL
DECIMAL_POSITIVE_LITERAL DECIMAL_NEGATIVE_LITERAL
BOOLEAN_LITERAL

%destructor {
  if($$)
#ifdef RAPTOR_V2_AVAILABLE
    raptor_free_uri_v2(((rasqal_query*)rq)->world->raptor_world_ptr, $$);
#else
    raptor_free_uri($$);
#endif
}
URI_LITERAL URI_LITERAL_BRACE

%destructor {
  if($$)
    RASQAL_FREE(cstring, $$);
}
QNAME_LITERAL QNAME_LITERAL_BRACE BLANK_LITERAL IDENTIFIER

%destructor {
  if($$)
    raptor_free_sequence($$);
}
SelectQuery ConstructQuery DescribeQuery
SelectExpressionList VarOrIRIrefList ArgList
ConstructTriples ConstructTriplesOpt
ConstructTemplate OrderConditionList
GraphNodeListNotEmpty SelectExpressionListTail

%destructor {
  if($$)
    rasqal_free_formula($$);
}
TriplesSameSubject
PropertyList PropertyListTailOpt PropertyListNotEmpty
ObjectList ObjectTail Collection
VarOrTerm Verb Object GraphNode TriplesNode
BlankNodePropertyList
TriplesBlock TriplesBlockOpt

%destructor {
  if($$)
    rasqal_free_graph_pattern($$);
}
GroupGraphPattern
GraphGraphPattern OptionalGraphPattern
GroupOrUnionGraphPattern GroupOrUnionGraphPatternList
GraphPatternNotTriples
GraphPatternListOpt GraphPatternList GraphPatternListFilter
LetGraphPattern

%destructor {
  if($$)
    rasqal_free_expression($$);
}
Expression ConditionalOrExpression ConditionalAndExpression
RelationalExpression AdditiveExpression
MultiplicativeExpression UnaryExpression
BuiltInCall RegexExpression FunctionCall
BrackettedExpression PrimaryExpression
OrderCondition Filter Constraint SelectExpression
AggregateExpression CountAggregateExpression SumAggregateExpression
AvgAggregateExpression MinAggregateExpression MaxAggregateExpression

%destructor {
  if($$)
    rasqal_free_literal($$);
}
GraphTerm IRIref BlankNode
VarOrIRIref
IRIrefBrace SourceSelector
NumericLiteral NumericLiteralUnsigned
NumericLiteralPositive NumericLiteralNegative

%destructor {
  if($$)
    rasqal_free_variable($$);
}
Var VarName SelectTerm


%%

/* Below here, grammar terms are numbered from
 * http://www.w3.org/TR/2005/WD-rdf-sparql-query-20051123/
 * except where noted
 */

/* SPARQL Grammar: [1] Query */
Query: Prologue ExplainOpt ReportFormat
{
}
;

/* LAQRS */
ExplainOpt: EXPLAIN
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(((rasqal_query*)rq)->context);

  if(sparql->extended)
    ((rasqal_query*)rq)->explain=1;
  else
    sparql_syntax_error((rasqal_query*)rq, 
                        "EXPLAIN cannot be used with SPARQL");
}
|
{
  /* nothing to do */
}
;


/* NEW Grammar Term pulled out of [1] Query */
ReportFormat: SelectQuery
{
  ((rasqal_query*)rq)->selects = $1;
  ((rasqal_query*)rq)->verb = RASQAL_QUERY_VERB_SELECT;
}
|  ConstructQuery
{
  ((rasqal_query*)rq)->constructs = $1;
  ((rasqal_query*)rq)->verb = RASQAL_QUERY_VERB_CONSTRUCT;
}
|  DescribeQuery
{
  ((rasqal_query*)rq)->describes = $1;
  ((rasqal_query*)rq)->verb = RASQAL_QUERY_VERB_DESCRIBE;
}
| AskQuery
{
  ((rasqal_query*)rq)->verb = RASQAL_QUERY_VERB_ASK;
}
| DeleteQuery
{
  ((rasqal_query*)rq)->verb = RASQAL_QUERY_VERB_DELETE;
}
| InsertQuery
{
  ((rasqal_query*)rq)->verb = RASQAL_QUERY_VERB_INSERT;
}
| ClearQuery
{
  ((rasqal_query*)rq)->verb = RASQAL_QUERY_VERB_CLEAR;
}
| CreateQuery
{
  ((rasqal_query*)rq)->verb = RASQAL_QUERY_VERB_CREATE;
}
| DropQuery
{
  ((rasqal_query*)rq)->verb = RASQAL_QUERY_VERB_DROP;
}
| LoadQuery
{
  ((rasqal_query*)rq)->verb = RASQAL_QUERY_VERB_LOAD;
}
;


/* SPARQL Grammar: [2] Prologue */
Prologue: BaseDeclOpt PrefixDeclListOpt
{
  /* nothing to do */
}
;


/* SPARQL Grammar: [3] BaseDecl */
BaseDeclOpt: BASE URI_LITERAL
{
  rasqal_query_set_base_uri((rasqal_query*)rq, $2);
}
| /* empty */
{
  /* nothing to do */
}
;


/* SPARQL Grammar: [4] PrefixDecl renamed to include optional list */
PrefixDeclListOpt: PrefixDeclListOpt PREFIX IDENTIFIER URI_LITERAL
{
  raptor_sequence *seq = ((rasqal_query*)rq)->prefixes;
  unsigned const char* prefix_string = $3;
  size_t l = 0;

  if(prefix_string)
    l=strlen((const char*)prefix_string);
  
  if(raptor_namespaces_find_namespace(((rasqal_query*)rq)->namespaces,
                                      prefix_string, l)) {
    /* A prefix may be defined only once */
    sparql_syntax_warning(((rasqal_query*)rq), 
                          "PREFIX %s can be defined only once.",
                          prefix_string ? (const char*)prefix_string : ":");
    RASQAL_FREE(cstring, prefix_string);
#ifdef RAPTOR_V2_AVAILABLE
    raptor_free_uri_v2(((rasqal_query*)rq)->world->raptor_world_ptr, $4);
#else
    raptor_free_uri($4);
#endif
  } else {
    rasqal_prefix *p;
    p = rasqal_new_prefix(((rasqal_query*)rq)->world, prefix_string, $4);
    if(!p)
      YYERROR_MSG("PrefixDeclOpt: failed to create new prefix");
    if(raptor_sequence_push(seq, p))
      YYERROR_MSG("PrefixDeclOpt: cannot push prefix to seq");
    if(rasqal_query_declare_prefix(((rasqal_query*)rq), p)) {
      YYERROR_MSG("PrefixDeclOpt: cannot declare prefix");
    }
  }
}
| /* empty */
{
  /* nothing to do, rq->prefixes already initialised */
}
;


/* SPARQL Grammar: [5] SelectQuery */
SelectQuery: SELECT DISTINCT SelectExpressionList
        DatasetClauseListOpt WhereClause SolutionModifier
{
  $$ = $3;
  ((rasqal_query*)rq)->distinct = 1;
}
| SELECT REDUCED SelectExpressionList
        DatasetClauseListOpt WhereClause SolutionModifier
{
  $$ = $3;
  ((rasqal_query*)rq)->distinct = 2;
}
| SELECT SelectExpressionList
        DatasetClauseListOpt WhereClause SolutionModifier
{
  $$ = $2;
}
;


/* NEW Grammar Term pulled out of [5] SelectQuery
 * A list of SelectTerm OR a NULL list and a wildcard
 */
SelectExpressionList: SelectExpressionListTail
{
  $$ = $1;
}
| '*'
{
  $$ = NULL;
  ((rasqal_query*)rq)->wildcard = 1;
}
;


/* NEW Grammar Term pulled out of [5] SelectQuery 
 * Non-empty list of SelectTerm with optional commas
 */
SelectExpressionListTail: SelectExpressionListTail SelectTerm
{
  $$ = $1;
  if(raptor_sequence_push($$, $2)) {
    raptor_free_sequence($$);
    $$ = NULL;
    YYERROR_MSG("SelectExpressionListTail 1: sequence push failed");
  }
}
| SelectExpressionListTail ',' SelectTerm
{
  $$ = $1;
  if(raptor_sequence_push($$, $3)) {
    raptor_free_sequence($$);
    $$ = NULL;
    YYERROR_MSG("SelectExpressionListTail 2: sequence push failed");
  }
}
| SelectTerm
{
  /* The variables are freed from the raptor_query field variables */
  $$ = raptor_new_sequence(NULL,
                           (raptor_sequence_print_handler*)rasqal_variable_print);
  if(!$$)
    YYERROR_MSG("SelectExpressionListTail 3: failed to create sequence");
  if(raptor_sequence_push($$, $1)) {
    raptor_free_sequence($$);
    $$ = NULL;
    YYERROR_MSG("SelectExpressionListTail 3: sequence push failed");
  }
}
;


/* NEW Grammar Term pulled out of [5] SelectQuery 
 * A variable (?x) or a select expression assigned to a name (x) with AS
 */
SelectTerm: Var
{
  $$ = $1;
}
| SelectExpression AS VarName
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(((rasqal_query*)rq)->context);

  $$ = NULL;
  if(!sparql->extended)
    sparql_syntax_error((rasqal_query*)rq,
                        "SELECT expression AS Variable cannot be used with SPARQL");
  else {
    if(rasqal_expression_mentions_variable($1, $3)) {
      sparql_query_error_full((rasqal_query*)rq, 
                              "SELECT expression contains the AS variable name '%s'",
                              $3->name);
    } else {
      $$ = $3;
      $3->expression = $1;
    }

  }
}
;


/* NEW Grammar Term pulled out of [5] SelectQuery */
SelectExpression: AggregateExpression
{
  $$ = $1;
}
| '(' AggregateExpression ')'
{
  $$ = $2;
}
| '(' Expression ')'
{
  $$ = $2;
}
;


AggregateExpression: CountAggregateExpression
{
  $$ = $1;
}
| SumAggregateExpression
{
  $$ = $1;
}
| AvgAggregateExpression
{
  $$ = $1;
}
| MinAggregateExpression
{
  $$ = $1;
}
| MaxAggregateExpression
{
  $$ = $1;
}
;


CountAggregateExpression: COUNT '(' Expression ')'
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(((rasqal_query*)rq)->context);

  if(!sparql->extended) {
    sparql_syntax_error((rasqal_query*)rq, "COUNT cannot be used with SPARQL");
    $$ = NULL;
  } else {
    $$ = rasqal_new_1op_expression(((rasqal_query*)rq)->world,
                                   RASQAL_EXPR_COUNT, $3);
    if(!$$)
      YYERROR_MSG("CountAggregateExpression 1: cannot create expr");
  }
}
| COUNT '(' '*' ')'
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(((rasqal_query*)rq)->context);

  if(!sparql->extended) {
    sparql_syntax_error((rasqal_query*)rq, "COUNT cannot be used with SPARQL");
    $$ = NULL;
  } else {
    rasqal_expression* vs;
    vs = rasqal_new_0op_expression(((rasqal_query*)rq)->world,
                                   RASQAL_EXPR_VARSTAR);
    if(!vs)
      YYERROR_MSG("CountAggregateExpression 2: cannot create varstar expr");
    $$ = rasqal_new_1op_expression(((rasqal_query*)rq)->world, 
                                   RASQAL_EXPR_COUNT, vs);
    if(!$$)
      YYERROR_MSG("CountAggregateExpression 2: cannot create expr");
  }
}
;


SumAggregateExpression: SUM '(' Expression ')'
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(((rasqal_query*)rq)->context);

  if(!sparql->extended) {
    sparql_syntax_error((rasqal_query*)rq, "SUM cannot be used with SPARQL");
    $$ = NULL;
  } else {
    $$ = rasqal_new_1op_expression(((rasqal_query*)rq)->world,
                                   RASQAL_EXPR_SUM, $3);
    if(!$$)
      YYERROR_MSG("SumAggregateExpression 1: cannot create expr");
  }
}
;


AvgAggregateExpression: AVG '(' Expression ')'
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(((rasqal_query*)rq)->context);

  if(!sparql->extended) {
    sparql_syntax_error((rasqal_query*)rq, "AVG cannot be used with SPARQL");
    $$ = NULL;
  } else {
    $$ = rasqal_new_1op_expression(((rasqal_query*)rq)->world,
                                   RASQAL_EXPR_AVG, $3);
    if(!$$)
      YYERROR_MSG("AvgAggregateExpression 1: cannot create expr");
  }
}
;


MinAggregateExpression: MIN '(' Expression ')'
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(((rasqal_query*)rq)->context);

  if(!sparql->extended) {
    sparql_syntax_error((rasqal_query*)rq, "MIN cannot be used with SPARQL");
    $$ = NULL;
  } else {
    $$ = rasqal_new_1op_expression(((rasqal_query*)rq)->world,
                                   RASQAL_EXPR_MIN, $3);
    if(!$$)
      YYERROR_MSG("MinAggregateExpression 1: cannot create expr");
  }
}
;


MaxAggregateExpression: MAX '(' Expression ')'
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(((rasqal_query*)rq)->context);

  if(!sparql->extended) {
    sparql_syntax_error((rasqal_query*)rq, "MAX cannot be used with SPARQL");
    $$ = NULL;
  } else {
    $$ = rasqal_new_1op_expression(((rasqal_query*)rq)->world,
                                   RASQAL_EXPR_MAX, $3);
    if(!$$)
      YYERROR_MSG("MaxAggregateExpression 1: cannot create expr");
  }
}
;


/* SPARQL Grammar: [6] ConstructQuery */
ConstructQuery: CONSTRUCT ConstructTemplate
        DatasetClauseListOpt WhereClause SolutionModifier
{
  $$ = $2;
}
;


/* SPARQL Grammar: [7] DescribeQuery */
DescribeQuery: DESCRIBE VarOrIRIrefList
        DatasetClauseListOpt WhereClauseOpt SolutionModifier
{
  $$ = $2;
}
| DESCRIBE '*'
        DatasetClauseListOpt WhereClauseOpt SolutionModifier
{
  $$ = NULL;
}
;


/* NEW Grammar Term pulled out of [7] DescribeQuery */
VarOrIRIrefList: VarOrIRIrefList VarOrIRIref
{
  $$ = $1;
  if(raptor_sequence_push($$, $2)) {
    raptor_free_sequence($$);
    $$ = NULL;
    YYERROR_MSG("VarOrIRIrefList 1: sequence push failed");
  }
}
| VarOrIRIrefList ',' VarOrIRIref
{
  $$ = $1;
  if(raptor_sequence_push($$, $3)) {
    raptor_free_sequence($$);
    $$ = NULL;
    YYERROR_MSG("VarOrIRIrefList 2: sequence push failed");
  }
}
| VarOrIRIref
{
  $$ = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_literal,
                           (raptor_sequence_print_handler*)rasqal_literal_print);
  if(!$$)
    YYERROR_MSG("VarOrIRIrefList 3: cannot create seq");
  if(raptor_sequence_push($$, $1)) {
    raptor_free_sequence($$);
    $$ = NULL;
    YYERROR_MSG("VarOrIRIrefList 3: sequence push failed");
  }
}
;


/* SPARQL Grammar: [8] AskQuery */
AskQuery: ASK 
        DatasetClauseListOpt WhereClause
{
  /* nothing to do */
}
;


/* LAQRS */
DeleteQuery: DELETE
        DatasetClauseListOpt WhereClauseOpt
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(((rasqal_query*)rq)->context);

  if(!sparql->extended)
    sparql_syntax_error((rasqal_query*)rq, "DELETE cannot be used with SPARQL");
  /* deleting from graph URIs - not deleting inline triples */
}
| DELETE DATA '{' GraphTriples '}'
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(((rasqal_query*)rq)->context);

  if(!sparql->extended)
    sparql_syntax_error((rasqal_query*)rq,
                        "DELETE DATA cannot be used with SPARQL");
  /* deleting inline triples - not inserting from graph URIs */
  ((rasqal_query*)rq)->constructs = $4;
}
;


/* SPARQL 1.1 Update (draft) */
GraphTriples: TriplesBlock
{
  $$ = NULL;
 
  if($1) {
    $$ = $1->triples;
    $1->triples = NULL;
    rasqal_free_formula($1);
  }
}
| GRAPH URI_LITERAL '{' TriplesBlock '}'
{
  $$ = NULL;

  ((rasqal_query*)rq)->graph_uri = $2;
  
  if($4) {
    $$ = $4->triples;
    $4->triples = NULL;
    rasqal_free_formula($4);
  }
}
;


/* SPARQL 1.1 Update (draft) / LAQRS */
InsertQuery: INSERT DatasetClauseListOpt WhereClauseOpt
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(((rasqal_query*)rq)->context);

  if(!sparql->extended)
    sparql_syntax_error((rasqal_query*)rq, "INSERT cannot be used with SPARQL");

  /* inserting from graph URIs - not inline triples */
}
| INSERT DATA '{' GraphTriples '}'
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(((rasqal_query*)rq)->context);

  if(!sparql->extended)
    sparql_syntax_error((rasqal_query*)rq,
                        "INSERT DATA cannot be used with SPARQL");
  /* inserting inline triples - not inserting from graph URIs */
  ((rasqal_query*)rq)->constructs = $4;
}
;


/* SPARQL 1.1 Update (draft) / LAQRS */
ClearQuery: CLEAR
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(((rasqal_query*)rq)->context);

  if(!sparql->extended)
    sparql_syntax_error((rasqal_query*)rq,
                        "CLEAR cannot be used with SPARQL");
}
| CLEAR GRAPH URI_LITERAL
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(((rasqal_query*)rq)->context);

  if(!sparql->extended)
    sparql_syntax_error((rasqal_query*)rq,
                        "CLEAR GRAPH <uri> cannot be used with SPARQL");

  ((rasqal_query*)rq)->graph_uri = $3;
}
;


/* SPARQL 1.1 Update (draft) / LAQRS */
CreateQuery: CREATE GRAPH URI_LITERAL
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(((rasqal_query*)rq)->context);

  if(!sparql->extended)
    sparql_syntax_error((rasqal_query*)rq, 
                        "CREATE GRAPH <uri> cannot be used with SPARQL");
  ((rasqal_query*)rq)->graph_uri = $3;
}
| CREATE SILENT GRAPH URI_LITERAL
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(((rasqal_query*)rq)->context);

  if(!sparql->extended)
    sparql_syntax_error((rasqal_query*)rq, 
                        "CREATE SILENT GRAPH <uri> cannot be used with SPARQL");

  ((rasqal_query*)rq)->graph_uri = $4;
}
;


/* SPARQL 1.1 Update (draft) / LAQRS */
DropQuery: DROP GRAPH URI_LITERAL
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(((rasqal_query*)rq)->context);

  if(!sparql->extended)
    sparql_syntax_error((rasqal_query*)rq, 
                        "DROP GRAPH <uri> cannot be used with SPARQL");
  ((rasqal_query*)rq)->graph_uri = $3;
}
| DROP SILENT GRAPH URI_LITERAL
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(((rasqal_query*)rq)->context);

  if(!sparql->extended)
    sparql_syntax_error((rasqal_query*)rq, 
                        "DROP SILENT GRAPH <uri> cannot be used with SPARQL");

  ((rasqal_query*)rq)->graph_uri = $4;
}
;


/* SPARQL 1.1 Update (draft) / LAQRS */
LoadQuery: LOAD URI_LITERAL
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(((rasqal_query*)rq)->context);

  if(!sparql->extended)
    sparql_syntax_error((rasqal_query*)rq, 
                        "LOAD <document uri> cannot be used with SPARQL");
  ((rasqal_query*)rq)->document_uri = $2;
  ((rasqal_query*)rq)->graph_uri = NULL;
}
| LOAD URI_LITERAL INTO URI_LITERAL
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(((rasqal_query*)rq)->context);

  if(!sparql->extended)
    sparql_syntax_error((rasqal_query*)rq, 
                        "LOAD <document uri> INTO <graph URI> cannot be used with SPARQL");
  ((rasqal_query*)rq)->document_uri = $2;
  ((rasqal_query*)rq)->graph_uri = $4;
}
;


/* SPARQL Grammar: [9] DatasetClause */
DatasetClauseListOpt: DatasetClauseListOpt FROM DefaultGraphClause
| DatasetClauseListOpt FROM NamedGraphClause
| /* empty */
;


/* SPARQL Grammar: [10] DefaultGraphClause */
DefaultGraphClause: SourceSelector
{
  if($1) {
    raptor_uri* uri = rasqal_literal_as_uri($1);
    if(rasqal_query_add_data_graph((rasqal_query*)rq, uri, NULL,
                                   RASQAL_DATA_GRAPH_BACKGROUND)) {
      rasqal_free_literal($1);
      YYERROR_MSG("DefaultGraphClause: rasqal_query_add_data_graph failed");
    }
    rasqal_free_literal($1);
  }
}
;  


/* SPARQL Grammar: [11] NamedGraphClause */
NamedGraphClause: NAMED SourceSelector
{
  if($2) {
    raptor_uri* uri = rasqal_literal_as_uri($2);
    if(rasqal_query_add_data_graph((rasqal_query*)rq, uri, uri,
                                   RASQAL_DATA_GRAPH_NAMED)) {
      rasqal_free_literal($2);
      YYERROR_MSG("NamedGraphClause: rasqal_query_add_data_graph failed");
    }
    rasqal_free_literal($2);
  }
}
;


/* SPARQL Grammar: [12] SourceSelector */
SourceSelector: IRIref
{
  $$ = $1;
}
;


/* SPARQL Grammar: [13] WhereClause - remained for clarity */
WhereClause:  WHERE GroupGraphPattern
{
  ((rasqal_query*)rq)->query_graph_pattern = $2;
}
| GroupGraphPattern
{
  ((rasqal_query*)rq)->query_graph_pattern = $1;
}
;


/* NEW Grammar Term pulled out of [13] WhereClause */
WhereClauseOpt:  WhereClause
| /* empty */
;


/* SPARQL Grammar: [14] SolutionModifier */
SolutionModifier: GroupClauseOpt OrderClauseOpt LimitOffsetClausesOpt
;


/* LAQRS */
GroupClauseOpt: GROUP BY OrderConditionList
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(((rasqal_query*)rq)->context);

  if(!sparql->extended)
    sparql_syntax_error((rasqal_query*)rq,
                        "GROUP BY cannot be used with SPARQL");
  else if(((rasqal_query*)rq)->verb == RASQAL_QUERY_VERB_ASK) {
    sparql_query_error((rasqal_query*)rq, 
                       "GROUP BY cannot be used with ASK");
  } else {
    raptor_sequence *seq = $3;
    ((rasqal_query*)rq)->group_conditions_sequence = seq;
    if(seq) {
      int i;
      int size = raptor_sequence_size(seq);
      
      for(i = 0; i < size; i++) {
        rasqal_expression* e;
        e = (rasqal_expression*)raptor_sequence_get_at(seq, i);
        if(e->op == RASQAL_EXPR_ORDER_COND_ASC)
          e->op = RASQAL_EXPR_GROUP_COND_ASC;
        else
          e->op = RASQAL_EXPR_GROUP_COND_DESC;
      }
    }
  }
}
| /* empty */
;


/* SPARQL Grammar: [15] LimitOffsetClauses */
LimitOffsetClausesOpt: LimitClause OffsetClause
| OffsetClause LimitClause
| LimitClause
| OffsetClause
| /* empty */
{ 
}
;


/* SPARQL Grammar: [16] OrderClause - remained for clarity */
OrderClauseOpt: ORDER BY OrderConditionList
{
  if(((rasqal_query*)rq)->verb == RASQAL_QUERY_VERB_ASK) {
    sparql_query_error((rasqal_query*)rq, "ORDER BY cannot be used with ASK");
  } else {
    ((rasqal_query*)rq)->order_conditions_sequence = $3;
  }
}
| /* empty */
;


/* NEW Grammar Term pulled out of [16] OrderClauseOpt */
OrderConditionList: OrderConditionList OrderCondition
{
  $$ = $1;
  if($2)
    if(raptor_sequence_push($$, $2)) {
      raptor_free_sequence($$);
      $$ = NULL;
      YYERROR_MSG("OrderConditionList 1: sequence push failed");
    }
}
| OrderCondition
{
  $$ = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_expression, 
                           (raptor_sequence_print_handler*)rasqal_expression_print);
  if(!$$) {
    if($1)
      rasqal_free_expression($1);
    YYERROR_MSG("OrderConditionList 2: cannot create sequence");
  }
  if($1)
    if(raptor_sequence_push($$, $1)) {
      raptor_free_sequence($$);
      $$ = NULL;
      YYERROR_MSG("OrderConditionList 2: sequence push failed");
    }
}
;


/* SPARQL Grammar: [17] OrderCondition */
OrderCondition: ASC BrackettedExpression
{
  $$ = rasqal_new_1op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_ORDER_COND_ASC, $2);
  if(!$$)
    YYERROR_MSG("OrderCondition 1: cannot create expr");
}
| DESC BrackettedExpression
{
  $$ = rasqal_new_1op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_ORDER_COND_DESC, $2);
  if(!$$)
    YYERROR_MSG("OrderCondition 2: cannot create expr");
}
| FunctionCall 
{
  /* The direction of ordering is ascending by default */
  $$ = rasqal_new_1op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_ORDER_COND_ASC, $1);
  if(!$$)
    YYERROR_MSG("OrderCondition 3: cannot create expr");
}
| Var
{
  rasqal_literal* l;
  rasqal_expression *e;
  l = rasqal_new_variable_literal(((rasqal_query*)rq)->world, $1);
  if(!l)
    YYERROR_MSG("OrderCondition 4: cannot create lit");
  e = rasqal_new_literal_expression(((rasqal_query*)rq)->world, l);
  if(!e)
    YYERROR_MSG("OrderCondition 4: cannot create lit expr");

  /* The direction of ordering is ascending by default */
  $$ = rasqal_new_1op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_ORDER_COND_ASC, e);
  if(!$$)
    YYERROR_MSG("OrderCondition 1: cannot create expr");
}
| BrackettedExpression
{
  /* The direction of ordering is ascending by default */
  $$ = rasqal_new_1op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_ORDER_COND_ASC, $1);
  if(!$$)
    YYERROR_MSG("OrderCondition 5: cannot create expr");
}
| BuiltInCall
{
  /* The direction of ordering is ascending by default */
  $$ = rasqal_new_1op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_ORDER_COND_ASC, $1);
  if(!$$)
    YYERROR_MSG("OrderCondition 6: cannot create expr");
}
;


/* SPARQL Grammar: [18] LimitClause - remained for clarity */
LimitClause:  LIMIT INTEGER_LITERAL
{
  if(((rasqal_query*)rq)->verb == RASQAL_QUERY_VERB_ASK) {
    sparql_query_error((rasqal_query*)rq, "LIMIT cannot be used with ASK");
  } else {
    if($2 != NULL) {
      ((rasqal_query*)rq)->limit = $2->value.integer;
      rasqal_free_literal($2);
    }
  }
  
}
;


/* SPARQL Grammar: [19] OffsetClause - remained for clarity */
OffsetClause:  OFFSET INTEGER_LITERAL
{
  if(((rasqal_query*)rq)->verb == RASQAL_QUERY_VERB_ASK) {
    sparql_query_error((rasqal_query*)rq, "LIMIT cannot be used with ASK");
  } else {
    if($2 != NULL) {
      ((rasqal_query*)rq)->offset = $2->value.integer;
      rasqal_free_literal($2);
    }
  }
}
;


/* SPARQL Grammar: [20] GroupGraphPattern 
 * TriplesBlockOpt: formula or NULL (on success or error)
 * GraphPatternListOpt: always 1 Group GP or NULL (on error)
 */
GroupGraphPattern: '{' TriplesBlockOpt GraphPatternListOpt '}'
{
  rasqal_graph_pattern *formula_gp = NULL;

#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "GroupGraphPattern\n  TriplesBlockOpt=");
  if($2)
    rasqal_formula_print($2, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fprintf(DEBUG_FH, ", GraphpatternListOpt=");
  if($3)
    rasqal_graph_pattern_print($3, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fputs("\n", DEBUG_FH);
#endif


  if(!$2 && !$3) {
    $$ = rasqal_new_2_group_graph_pattern((rasqal_query*)rq, NULL, NULL);
    if(!$$)
      YYERROR_MSG("GroupGraphPattern: cannot create group gp");
  } else {
    if($2) {
      formula_gp = rasqal_new_basic_graph_pattern_from_formula((rasqal_query*)rq,
                                                               $2);
      if(!formula_gp) {
        if($3)
          rasqal_free_graph_pattern($3);
        YYERROR_MSG("GroupGraphPattern: cannot create formula_gp");
      }
    }

    if($3) {
      $$ = $3;
      if(formula_gp && raptor_sequence_shift($$->graph_patterns, formula_gp)) {
        rasqal_free_graph_pattern($$);
        $$ = NULL;
        YYERROR_MSG("GroupGraphPattern: sequence push failed");
      }
    } else
      $$ = formula_gp;
  }
  
#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "  after graph pattern=");
  if($$)
    rasqal_graph_pattern_print($$, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif
}
;


/* Pulled out of SPARQL Grammar: [20] GroupGraphPattern */
TriplesBlockOpt: TriplesBlock
{
#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "TriplesBlockOpt 1\n  TriplesBlock=");
  if($1)
    rasqal_formula_print($1, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fputs("\n\n", DEBUG_FH);
#endif

  $$ = $1;
}
| /* empty */
{
  $$ = NULL;
}
;


/* Pulled out of SPARQL Grammar: [20] GroupGraphPattern 
 * GraphPatternListOpt: always 1 Group GP or NULL
 * GraphPatternList: always 1 Group GP or NULL (on error)
 *
 * Result: always 1 Group GP or NULL (on error)
 */
GraphPatternListOpt: GraphPatternListOpt GraphPatternList
{
#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "GraphPatternListOpt\n  GraphPatternListOpt=");
  if($1)
    rasqal_graph_pattern_print($1, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fprintf(DEBUG_FH, ", GraphPatternList=");
  if($2)
    rasqal_graph_pattern_print($2, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fputs("\n", DEBUG_FH);
#endif

  $$ =  ($1 ? $1 : $2);
  if($1 && $2) {
    $$ = $1;
    if(rasqal_graph_patterns_join($$, $2)) {
      rasqal_free_graph_pattern($$);
      rasqal_free_graph_pattern($2);
      $$ = NULL;
      YYERROR_MSG("GraphPatternListOpt: sequence join failed");
    }
    rasqal_free_graph_pattern($2);
  }
  
#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "  after grouping graph pattern=");
  if($$)
    rasqal_graph_pattern_print($$, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif
}
| GraphPatternList
{
  $$ = $1;
}
| /* empty */
{
  $$ = NULL;
}
;


/* Pulled out of SPARQL Grammar: [20] GroupGraphPattern 
 * GraphPatternListFilter: always 1 GP or NULL (on error)
 * TriplesBlockOpt: formula or NULL (on success or error)
 *
 * Result: always 1 Group GP or NULL (on success or error)
 */
GraphPatternList: GraphPatternListFilter DotOptional TriplesBlockOpt
{
  rasqal_graph_pattern *formula_gp = NULL;

#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "GraphPatternList\n  GraphPatternListFilter=");
  if($1)
    rasqal_graph_pattern_print($1, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fprintf(DEBUG_FH, ", TriplesBlockOpt=");
  if($3)
    rasqal_formula_print($3, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fputs("\n", DEBUG_FH);
#endif

  if($3) {
    formula_gp = rasqal_new_basic_graph_pattern_from_formula((rasqal_query*)rq, 
                                                             $3);
    if(!formula_gp) {
      if($1)
        rasqal_free_graph_pattern($1);
      YYERROR_MSG("GraphPatternList: cannot create formula_gp");
    }
  }
  $$ = rasqal_new_2_group_graph_pattern((rasqal_query*)rq, $1, formula_gp);
  if(!$$)
    YYERROR_MSG("GraphPatternList: cannot create sequence");

#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "  after graph pattern=");
  if($$)
    rasqal_graph_pattern_print($$, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif
}
;


/* Pulled out of SPARQL Grammar: [20] GroupGraphPattern 
 * GraphPatternNotTriples: always 1 GP or NULL (on error)
 *
 * Result: always 1 GP or NULL (on error)
 */
GraphPatternListFilter: GraphPatternNotTriples
{
#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "GraphPatternListFilter 1\n  GraphPatternNotTriples=");
  if($1)
    rasqal_graph_pattern_print($1, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fputs("\n\n", DEBUG_FH);
#endif

  $$ = $1;
}
| Filter
{
#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "GraphPatternListFilter 2\n  Filter=");
  if($1)
    rasqal_expression_print($1, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fputs("\n", DEBUG_FH);
#endif

  $$ = rasqal_new_filter_graph_pattern((rasqal_query*)rq, $1);
  if(!$$)
    YYERROR_MSG("GraphPatternListFilter 2: cannot create graph pattern");

#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "  after graph pattern=");
  if($$)
    rasqal_graph_pattern_print($$, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif
}
;


/* NEW Grammar Term */
DotOptional: '.'
| /* empty */
;


/* SPARQL Grammar: [21] TriplesBlock */
TriplesBlock: TriplesSameSubject '.' TriplesBlockOpt
{
#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "TriplesBlock\n  TriplesSameSubject=");
  if($1)
    rasqal_formula_print($1, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fprintf(DEBUG_FH, ", TriplesBlockOpt=");
  if($3)
    rasqal_formula_print($3, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fputs("\n", DEBUG_FH);
#endif


  $$ =  ($1 ? $1 : $3);
  if($1 && $3) {
    /* $1 and $3 are freed as necessary */
    $$ = rasqal_formula_join($1, $3);
    if(!$1)
      YYERROR_MSG("TriplesBlock: formula join failed");
  }

#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "  after joining formula=");
  rasqal_formula_print($$, DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif
}
| TriplesSameSubject
{
  $$ = $1;
}
;


/* SPARQL Grammar: [22] GraphPatternNotTriples */
GraphPatternNotTriples: OptionalGraphPattern
{
  $$ = $1;
}
| GroupOrUnionGraphPattern
{
  $$ = $1;
}
| GraphGraphPattern
{
  $$ = $1;
}
| LetGraphPattern
{
  $$ = $1;
}
;


/* SPARQL Grammar: [23] OptionalGraphPattern */
OptionalGraphPattern: OPTIONAL GroupGraphPattern
{
#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "PatternElementForms 4\n  graphpattern=");
  if($2)
    rasqal_graph_pattern_print($2, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fputs("\n\n", DEBUG_FH);
#endif

  $$ = NULL;

  if($2) {
    raptor_sequence *seq;

    seq = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_graph_pattern,
                              (raptor_sequence_print_handler*)rasqal_graph_pattern_print);
    if(!seq) {
      rasqal_free_graph_pattern($2);
      YYERROR_MSG("OptionalGraphPattern 1: cannot create sequence");
    } else {
      if(raptor_sequence_push(seq, $2)) {
        raptor_free_sequence(seq);
        YYERROR_MSG("OptionalGraphPattern 2: sequence push failed");
      } else {
        $$ = rasqal_new_graph_pattern_from_sequence((rasqal_query*)rq,
                                                    seq,
                                                    RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL);
        if(!$$)
          YYERROR_MSG("OptionalGraphPattern: cannot create graph pattern");
      }
    }
  }
}
;


/* SPARQL Grammar: [24] GraphGraphPattern */
GraphGraphPattern: GRAPH VarOrIRIref GroupGraphPattern
{
#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "GraphGraphPattern 2\n  varoruri=");
  rasqal_literal_print($2, DEBUG_FH);
  fprintf(DEBUG_FH, ", graphpattern=");
  if($3)
    rasqal_graph_pattern_print($3, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fputs("\n\n", DEBUG_FH);
#endif

  if($3) {
    raptor_sequence *seq;

    seq = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_graph_pattern,
                              (raptor_sequence_print_handler*)rasqal_graph_pattern_print);
    if(!seq) {
      rasqal_free_graph_pattern($3);
      YYERROR_MSG("GraphGraphPattern 1: cannot create sequence");
    } else {
      if(raptor_sequence_push(seq, $3)) {
        raptor_free_sequence(seq);
        YYERROR_MSG("GraphGraphPattern 2: sequence push failed");
      } else {
        $$ = rasqal_new_graph_pattern_from_sequence((rasqal_query*)rq,
                                                    seq,
                                                    RASQAL_GRAPH_PATTERN_OPERATOR_GRAPH);
        if(!$$)
          YYERROR_MSG("GraphGraphPattern: cannot create graph pattern");
        else
          rasqal_graph_pattern_set_origin($$, $2);
      }
    }
  }


#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "GraphGraphPattern\n  graphpattern=");
  rasqal_graph_pattern_print($$, DEBUG_FH);
  fputs("\n\n", DEBUG_FH);
#endif

  rasqal_free_literal($2);
}
;


/* SPARQL Grammar: [25] GroupOrUnionGraphPattern */
GroupOrUnionGraphPattern: GroupGraphPattern UNION GroupOrUnionGraphPatternList
{
  $$ = $3;
  if(raptor_sequence_shift($$->graph_patterns, $1)) {
    rasqal_free_graph_pattern($$);
    $$ = NULL;
    YYERROR_MSG("GroupOrUnionGraphPattern: sequence push failed");
  }

#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "UnionGraphPattern\n  graphpattern=");
  rasqal_graph_pattern_print($$, DEBUG_FH);
  fputs("\n\n", DEBUG_FH);
#endif
}
| GroupGraphPattern
{
  $$ = $1;
}
;

/* NEW Grammar Term pulled out of [25] GroupOrUnionGraphPattern */
GroupOrUnionGraphPatternList: GroupOrUnionGraphPatternList UNION GroupGraphPattern
{
  $$ = $1;
  if($3)
    if(raptor_sequence_push($$->graph_patterns, $3)) {
      rasqal_free_graph_pattern($$);
      $$ = NULL;
      YYERROR_MSG("GroupOrUnionGraphPatternList 1: sequence push failed");
    }
}
| GroupGraphPattern
{
  raptor_sequence *seq;
  seq = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_graph_pattern,
                            (raptor_sequence_print_handler*)rasqal_graph_pattern_print);
  if(!seq) {
    if($1)
      rasqal_free_graph_pattern($1);
    YYERROR_MSG("GroupOrUnionGraphPatternList 2: cannot create sequence");
  }
  if($1)
    if(raptor_sequence_push(seq, $1)) {
      raptor_free_sequence(seq);
      YYERROR_MSG("GroupOrUnionGraphPatternList 2: sequence push failed");
    }
  $$ = rasqal_new_graph_pattern_from_sequence((rasqal_query*)rq,
                                              seq,
                                              RASQAL_GRAPH_PATTERN_OPERATOR_UNION);
  if(!$$)
    YYERROR_MSG("GroupOrUnionGraphPatternList 1: cannot create gp");
}
;


/* LAQRS: LET (?var := expression) . */
LetGraphPattern: LET '(' Var ASSIGN Expression ')'
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(((rasqal_query*)rq)->context);

  if($3 && $5) {
    if(sparql->extended)
      $$ = rasqal_new_let_graph_pattern((rasqal_query*)rq, $3, $5);
    else {
      sparql_syntax_error((rasqal_query*)rq, "LET cannot be used with SPARQL");
      rasqal_free_expression($5);
      $$ = NULL;
    }
  } else
    $$ = NULL;
}
;


/* SPARQL Grammar: [26] Filter */
Filter: FILTER Constraint
{
  $$ = $2;
}
;


/* SPARQL Grammar: [27] Constraint */
Constraint: BrackettedExpression
{
  $$ = $1;
}
| BuiltInCall
{
  $$ = $1;
}
| FunctionCall
{
  $$ = $1;
}
;


/* SPARQL Grammar: [28] FunctionCall */
FunctionCall: IRIrefBrace ArgList ')'
{
  raptor_uri* uri = rasqal_literal_as_uri($1);
  
  if(!$2) {
    $2 = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_expression,
                             (raptor_sequence_print_handler*)rasqal_expression_print);
    if(!$2) {
      rasqal_free_literal($1);
      YYERROR_MSG("FunctionCall: cannot create sequence");
    }
  }

#ifdef RAPTOR_V2_AVAILABLE
  uri = raptor_uri_copy_v2(((rasqal_query*)rq)->world->raptor_world_ptr, uri);
#else
  uri = raptor_uri_copy(uri);
#endif

  if(raptor_sequence_size($2) == 1 &&
     rasqal_xsd_is_datatype_uri(((rasqal_query*)rq)->world, uri)) {
    rasqal_expression* e = (rasqal_expression*)raptor_sequence_pop($2);
    $$ = rasqal_new_cast_expression(((rasqal_query*)rq)->world, uri, e);
    raptor_free_sequence($2);
  } else {
    $$ = rasqal_new_function_expression(((rasqal_query*)rq)->world, uri, $2);
  }
  rasqal_free_literal($1);

  if(!$$)
    YYERROR_MSG("FunctionCall: cannot create expr");
}
;


/* LAQRS */
CoalesceExpression: COALESCE '(' ArgList ')'
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(((rasqal_query*)rq)->context);

  if(!sparql->extended)
    sparql_syntax_error((rasqal_query*)rq,
                        "COALESCE cannot be used with SPARQL");

  if(!$3) {
    $3 = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_expression, (raptor_sequence_print_handler*)rasqal_expression_print);
    if(!$3)
      YYERROR_MSG("FunctionCall: cannot create sequence");
  }

  $$ = rasqal_new_coalesce_expression(((rasqal_query*)rq)->world, $3);
  if(!$$)
    YYERROR_MSG("Coalesce: cannot create expr");
}
;


/* SPARQL Grammar: [29] ArgList */
ArgList: ArgList ',' Expression
{
  $$ = $1;
  if($3)
    if(raptor_sequence_push($$, $3)) {
      raptor_free_sequence($$);
      $$ = NULL;
      YYERROR_MSG("ArgList 1: sequence push failed");
    }
}
| Expression
{
  $$ = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_expression,
                           (raptor_sequence_print_handler*)rasqal_expression_print);
  if(!$$) {
    if($1)
      rasqal_free_expression($1);
    YYERROR_MSG("ArgList 2: cannot create sequence");
  }
  if($1)
    if(raptor_sequence_push($$, $1)) {
      raptor_free_sequence($$);
      $$ = NULL;
      YYERROR_MSG("ArgList 2: sequence push failed");
    }
}
| /* empty */
{
  $$ = NULL;
}
;


/* SPARQL Grammar: [30] ConstructTemplate */
ConstructTemplate:  '{' ConstructTriplesOpt '}'
{
  $$ = $2;
}
;


/* Pulled out of SPARQL Grammar: [31] ConstructTriples */
ConstructTriplesOpt: ConstructTriples
{
  $$ = $1;
}
| /* empty */
{
  $$ = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_triple,
                           (raptor_sequence_print_handler*)rasqal_triple_print);
  if(!$$) {
    YYERROR_MSG("ConstructTriplesOpt: cannot create sequence");
  }
}
;


/* SPARQL Grammar: [31] ConstructTriples */
ConstructTriples: TriplesSameSubject '.' ConstructTriplesOpt
{
  $$ = NULL;
 
  if($1) {
    $$ = $1->triples;
    $1->triples = NULL;
    rasqal_free_formula($1);
  }
  
  if($3) {
    if(!$$) {
      $$ = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_triple,
                               (raptor_sequence_print_handler*)rasqal_triple_print);
      if(!$$) {
        raptor_free_sequence($3);
        YYERROR_MSG("ConstructTriples: cannot create sequence");
      }
    }

    if(raptor_sequence_join($$, $3)) {
      raptor_free_sequence($3);
      raptor_free_sequence($$);
      $$ = NULL;
      YYERROR_MSG("ConstructTriples: sequence join failed");
    }
    raptor_free_sequence($3);
  }

 }
| TriplesSameSubject
{
  $$ = NULL;
  
  if($1) {
    $$ = $1->triples;
    $1->triples = NULL;
    rasqal_free_formula($1);
  }
  
}
;


/* SPARQL Grammar: [32] TriplesSameSubject */
TriplesSameSubject: VarOrTerm PropertyListNotEmpty
{
  int i;

#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "TriplesSameSubject 1\n  subject=");
  rasqal_formula_print($1, DEBUG_FH);
  if($2) {
    fprintf(DEBUG_FH, "\n  propertyList=");
    rasqal_formula_print($2, DEBUG_FH);
    fprintf(DEBUG_FH, "\n");
  } else     
    fprintf(DEBUG_FH, "\n  and empty propertyList\n");
#endif

  if($2) {
    raptor_sequence *seq = $2->triples;
    rasqal_literal *subject = $1->value;
    int size = raptor_sequence_size(seq);
    
    /* non-empty property list, handle it  */
    for(i = 0; i < size; i++) {
      rasqal_triple* t2 = (rasqal_triple*)raptor_sequence_get_at(seq, i);
      if(t2->subject)
        continue;
      t2->subject = rasqal_new_literal_from_literal(subject);
    }
#if RASQAL_DEBUG > 1  
    fprintf(DEBUG_FH, "  after substitution propertyList=");
    rasqal_formula_print($2, DEBUG_FH);
    fprintf(DEBUG_FH, "\n");
#endif
  }

  $$ = rasqal_formula_join($1, $2);
  if(!$$)
    YYERROR_MSG("TriplesSameSubject 1: formula join failed");

#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "  after joining formula=");
  rasqal_formula_print($$, DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif
}
| TriplesNode PropertyList
{
  int i;

#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "TriplesSameSubject 2\n  subject=");
  rasqal_formula_print($1, DEBUG_FH);
  if($2) {
    fprintf(DEBUG_FH, "\n  propertyList=");
    rasqal_formula_print($2, DEBUG_FH);
    fprintf(DEBUG_FH, "\n");
  } else     
    fprintf(DEBUG_FH, "\n  and empty propertyList\n");
#endif

  if($2) {
    raptor_sequence *seq = $2->triples;
    rasqal_literal *subject = $1->value;
    int size = raptor_sequence_size(seq);
    
    /* non-empty property list, handle it  */
    for(i = 0; i < size; i++) {
      rasqal_triple* t2 = (rasqal_triple*)raptor_sequence_get_at(seq, i);
      if(t2->subject)
        continue;
      t2->subject = rasqal_new_literal_from_literal(subject);
    }
#if RASQAL_DEBUG > 1  
    fprintf(DEBUG_FH, "  after substitution propertyList=");
    rasqal_formula_print($2, DEBUG_FH);
    fprintf(DEBUG_FH, "\n");
#endif
  }

  $$ = rasqal_formula_join($1, $2);
  if(!$$)
    YYERROR_MSG("TriplesSameSubject 2: formula join failed");

#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "  after joining formula=");
  rasqal_formula_print($$, DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif
}
;


/* SPARQL Grammar: [33] PropertyListNotEmpty */
PropertyListNotEmpty: Verb ObjectList PropertyListTailOpt
{
  int i;
  
#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "PropertyList 1\n  Verb=");
  rasqal_formula_print($1, DEBUG_FH);
  fprintf(DEBUG_FH, "\n  ObjectList=");
  rasqal_formula_print($2, DEBUG_FH);
  fprintf(DEBUG_FH, "\n  PropertyListTail=");
  if($3 != NULL)
    rasqal_formula_print($3, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fprintf(DEBUG_FH, "\n");
#endif
  
  if($2 == NULL) {
#if RASQAL_DEBUG > 1  
    fprintf(DEBUG_FH, " empty ObjectList not processed\n");
#endif
  } else if($1 && $2) {
    raptor_sequence *seq = $2->triples;
    rasqal_literal *predicate = $1->value;
    rasqal_formula *formula;
    rasqal_triple *t2;
    int size;
    
    formula = rasqal_new_formula();
    if(!formula) {
      rasqal_free_formula($1);
      rasqal_free_formula($2);
      if($3)
        rasqal_free_formula($3);
      YYERROR_MSG("PropertyList 1: cannot create formula");
    }
    formula->triples = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_triple,
                                           (raptor_sequence_print_handler*)rasqal_triple_print);
    if(!formula->triples) {
      rasqal_free_formula(formula);
      rasqal_free_formula($1);
      rasqal_free_formula($2);
      if($3)
        rasqal_free_formula($3);
      YYERROR_MSG("PropertyList 1: cannot create sequence");
    }

    /* non-empty property list, handle it  */
    size = raptor_sequence_size(seq);
    for(i = 0; i < size; i++) {
      t2 = (rasqal_triple*)raptor_sequence_get_at(seq, i);
      if(!t2->predicate)
        t2->predicate = (rasqal_literal*)rasqal_new_literal_from_literal(predicate);
    }
  
#if RASQAL_DEBUG > 1  
    fprintf(DEBUG_FH, "  after substitution ObjectList=");
    raptor_sequence_print(seq, DEBUG_FH);
    fprintf(DEBUG_FH, "\n");
#endif

    while(raptor_sequence_size(seq)) {
      t2 = (rasqal_triple*)raptor_sequence_unshift(seq);
      if(raptor_sequence_push(formula->triples, t2)) {
        rasqal_free_formula(formula);
        rasqal_free_formula($1);
        rasqal_free_formula($2);
        if($3)
          rasqal_free_formula($3);
        YYERROR_MSG("PropertyList 1: sequence push failed");
      }
    }

    $3 = rasqal_formula_join(formula, $3);
    if(!$3) {
      rasqal_free_formula($1);
      rasqal_free_formula($2);
      YYERROR_MSG("PropertyList 1: formula join failed");
    }

#if RASQAL_DEBUG > 1  
    fprintf(DEBUG_FH, "  after appending ObjectList=");
    rasqal_formula_print($3, DEBUG_FH);
    fprintf(DEBUG_FH, "\n\n");
#endif

    rasqal_free_formula($2);
  }

  if($1)
    rasqal_free_formula($1);

  $$ = $3;
}
;


/* NEW Grammar Term pulled out of [33] PropertyListNotEmpty */
PropertyListTailOpt: ';' PropertyList
{
  $$ = $2;
}
| /* empty */
{
  $$ = NULL;
}
;


/* SPARQL Grammar: [34] PropertyList */
PropertyList: PropertyListNotEmpty
{
  $$ = $1;
}
| /* empty */
{
  $$ = NULL;
}
;


/* SPARQL Grammar: [35] ObjectList */
ObjectList: Object ObjectTail
{
  rasqal_formula *formula;
  rasqal_triple *triple;

#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "ObjectList 1\n");
  fprintf(DEBUG_FH, "  Object=\n");
  rasqal_formula_print($1, DEBUG_FH);
  fprintf(DEBUG_FH, "\n");
  if($2) {
    fprintf(DEBUG_FH, "  ObjectTail=");
    rasqal_formula_print($2, DEBUG_FH);
    fprintf(DEBUG_FH, "\n");
  } else
    fprintf(DEBUG_FH, "  and empty ObjectTail\n");
#endif

  formula = rasqal_new_formula();
  if(!formula) {
    rasqal_free_formula($1);
    if($2)
      rasqal_free_formula($2);
    YYERROR_MSG("ObjectList: cannot create formula");
  }
  
  formula->triples = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_triple,
                                         (raptor_sequence_print_handler*)rasqal_triple_print);
  if(!formula->triples) {
    rasqal_free_formula(formula);
    rasqal_free_formula($1);
    if($2)
      rasqal_free_formula($2);
    YYERROR_MSG("ObjectList: cannot create sequence");
  }

  triple = rasqal_new_triple(NULL, NULL, $1->value);
  $1->value = NULL; /* value now owned by triple */
  if(!triple) {
    rasqal_free_formula(formula);
    rasqal_free_formula($1);
    if($2)
      rasqal_free_formula($2);
    YYERROR_MSG("ObjectList: cannot create triple");
  }

  if(raptor_sequence_push(formula->triples, triple)) {
    rasqal_free_formula(formula);
    rasqal_free_formula($1);
    if($2)
      rasqal_free_formula($2);
    YYERROR_MSG("ObjectList: sequence push failed");
  }

  $$ = rasqal_formula_join(formula, $1);
  if(!$$) {
    if($2)
      rasqal_free_formula($2);
    YYERROR_MSG("ObjectList: formula join $1 failed");
  }

  $$ = rasqal_formula_join($$, $2);
  if(!$$)
    YYERROR_MSG("ObjectList: formula join $2 failed");

#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "  objectList is now ");
  if($$)
    raptor_sequence_print($$->triples, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif
}
;


/* NEW Grammar Term pulled out of [35] ObjectList */
ObjectTail: ',' ObjectList
{
  $$ = $2;
}
| /* empty */
{
  $$ = NULL;
}
;


/* SPARQL Grammar: [36] Object */
Object: GraphNode
{
  $$ = $1;
}
;


/* SPARQL Grammar: [37] Verb */
Verb: VarOrIRIref
{
  $$ = rasqal_new_formula();
  if(!$$) {
    if($1)
      rasqal_free_literal($1);
    YYERROR_MSG("Verb 1: cannot create formula");
  }
  $$->value = $1;
}
| A
{
  raptor_uri *uri;

#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "verb Verb=rdf:type (a)\n");
#endif

#ifdef RAPTOR_V2_AVAILABLE
  uri = raptor_new_uri_for_rdf_concept_v2(((rasqal_query*)rq)->world->raptor_world_ptr, "type");
#else
  uri = raptor_new_uri_for_rdf_concept("type");
#endif
  if(!uri)
    YYERROR_MSG("Verb 2: uri for rdf concept type failed");
  $$ = rasqal_new_formula();
  if(!$$) {
#ifdef RAPTOR_V2_AVAILABLE
    raptor_free_uri_v2(((rasqal_query*)rq)->world->raptor_world_ptr, uri);
#else
    raptor_free_uri(uri);
#endif
    YYERROR_MSG("Verb 2: cannot create formula");
  }
  $$->value = rasqal_new_uri_literal(((rasqal_query*)rq)->world, uri);
  if(!$$->value) {
    rasqal_free_formula($$);
    $$ = NULL;
    YYERROR_MSG("Verb 2: cannot create uri literal");
  }
}
;


/* SPARQL Grammar: [38] TriplesNode */
TriplesNode: Collection
{
  $$ = $1;
}
| BlankNodePropertyList
{
  $$ = $1;
}
;


/* SPARQL Grammar: [39] BlankNodePropertyList */
BlankNodePropertyList: '[' PropertyListNotEmpty ']'
{
  int i;
  const unsigned char *id;

  if($2 == NULL) {
    $$ = rasqal_new_formula();
    if(!$$)
      YYERROR_MSG("BlankNodePropertyList: cannot create formula");
  } else {
    $$ = $2;
    if($$->value) {
      rasqal_free_literal($$->value);
      $$->value = NULL;
    }
  }
  
  id = rasqal_query_generate_bnodeid((rasqal_query*)rq, NULL);
  if(!id) {
    rasqal_free_formula($$);
    $$ = NULL;
    YYERROR_MSG("BlankNodeProperyList: cannot create bnodeid");
  }

  $$->value = rasqal_new_simple_literal(((rasqal_query*)rq)->world,
                                        RASQAL_LITERAL_BLANK, id);
  if(!$$->value) {
    rasqal_free_formula($$);
    $$ = NULL;
    YYERROR_MSG("BlankNodePropertyList: cannot create literal");
  }

  if($2 == NULL) {
#if RASQAL_DEBUG > 1  
    fprintf(DEBUG_FH, "TriplesNode\n  PropertyList=");
    rasqal_formula_print($$, DEBUG_FH);
    fprintf(DEBUG_FH, "\n");
#endif
  } else {
    raptor_sequence *seq = $2->triples;

    /* non-empty property list, handle it  */
#if RASQAL_DEBUG > 1  
    fprintf(DEBUG_FH, "TriplesNode\n  PropertyList=");
    raptor_sequence_print(seq, DEBUG_FH);
    fprintf(DEBUG_FH, "\n");
#endif

    for(i = 0; i<raptor_sequence_size(seq); i++) {
      rasqal_triple* t2 = (rasqal_triple*)raptor_sequence_get_at(seq, i);
      if(t2->subject)
        continue;
      
      t2->subject = (rasqal_literal*)rasqal_new_literal_from_literal($$->value);
    }

#if RASQAL_DEBUG > 1
    fprintf(DEBUG_FH, "  after substitution formula=");
    rasqal_formula_print($$, DEBUG_FH);
    fprintf(DEBUG_FH, "\n\n");
#endif
  }
}
;


/* SPARQL Grammar: [40] Collection (allowing empty case) */
Collection: '(' GraphNodeListNotEmpty ')'
{
  int i;
  rasqal_query* rdf_query = (rasqal_query*)rq;
  rasqal_literal* first_identifier = NULL;
  rasqal_literal* rest_identifier = NULL;
  rasqal_literal* object = NULL;
  rasqal_literal* blank = NULL;

#if RASQAL_DEBUG > 1
  char const *errmsg;
  #define YYERR_MSG_GOTO(label,msg) do { errmsg = msg; goto label; } while(0)
#else
  #define YYERR_MSG_GOTO(label,ignore) goto label
#endif

#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "Collection\n  GraphNodeListNotEmpty=");
  raptor_sequence_print($2, DEBUG_FH);
  fprintf(DEBUG_FH, "\n");
#endif

  $$ = rasqal_new_formula();
  if(!$$)
    YYERR_MSG_GOTO(err_Collection, "Collection: cannot create formula");

  $$->triples = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_triple, (raptor_sequence_print_handler*)rasqal_triple_print);
  if(!$$->triples)
    YYERR_MSG_GOTO(err_Collection, "Collection: cannot create sequence");

  first_identifier = rasqal_new_uri_literal(rdf_query->world,
#ifdef RAPTOR_V2_AVAILABLE
                                            raptor_uri_copy_v2(rdf_query->world->raptor_world_ptr, rdf_query->world->rdf_first_uri)
#else
                                            raptor_uri_copy(rdf_query->world->rdf_first_uri)
#endif
                                            );
  if(!first_identifier)
    YYERR_MSG_GOTO(err_Collection, "Collection: cannot first_identifier");
  
  rest_identifier = rasqal_new_uri_literal(rdf_query->world,
#ifdef RAPTOR_V2_AVAILABLE
                                           raptor_uri_copy_v2(rdf_query->world->raptor_world_ptr, rdf_query->world->rdf_rest_uri)
#else
                                           raptor_uri_copy(rdf_query->world->rdf_rest_uri)
#endif
                                           );
  if(!rest_identifier)
    YYERR_MSG_GOTO(err_Collection, "Collection: cannot create rest_identifier");
  
  object = rasqal_new_uri_literal(rdf_query->world,
#ifdef RAPTOR_V2_AVAILABLE
                                  raptor_uri_copy_v2(rdf_query->world->raptor_world_ptr, rdf_query->world->rdf_nil_uri)
#else
                                  raptor_uri_copy(rdf_query->world->rdf_nil_uri)
#endif
                                  );
  if(!object)
    YYERR_MSG_GOTO(err_Collection, "Collection: cannot create nil object");

  for(i = raptor_sequence_size($2)-1; i >= 0; i--) {
    rasqal_formula* f = (rasqal_formula*)raptor_sequence_get_at($2, i);
    rasqal_triple *t2;
    const unsigned char *blank_id = NULL;

    blank_id = rasqal_query_generate_bnodeid(rdf_query, NULL);
    if(!blank_id)
      YYERR_MSG_GOTO(err_Collection, "Collection: cannot create bnodeid");

    blank = rasqal_new_simple_literal(((rasqal_query*)rq)->world, RASQAL_LITERAL_BLANK, blank_id);
    if(!blank)
      YYERR_MSG_GOTO(err_Collection, "Collection: cannot create bnode");

    /* Move existing formula triples */
    if(f->triples)
      if(raptor_sequence_join($$->triples, f->triples))
        YYERR_MSG_GOTO(err_Collection, "Collection: sequence join failed");

    /* add new triples we needed */
    t2 = rasqal_new_triple(rasqal_new_literal_from_literal(blank),
                           rasqal_new_literal_from_literal(first_identifier),
                           rasqal_new_literal_from_literal(f->value));
    if(!t2)
      YYERR_MSG_GOTO(err_Collection, "Collection: cannot create triple");

    if(raptor_sequence_push($$->triples, t2))
      YYERR_MSG_GOTO(err_Collection, "Collection: cannot create triple");

    t2 = rasqal_new_triple(rasqal_new_literal_from_literal(blank),
                           rasqal_new_literal_from_literal(rest_identifier),
                           rasqal_new_literal_from_literal(object));
    if(!t2)
      YYERR_MSG_GOTO(err_Collection, "Collection: cannot create triple 2");

    if(raptor_sequence_push($$->triples, t2))
      YYERR_MSG_GOTO(err_Collection, "Collection: sequence push 2 failed");

    rasqal_free_literal(object);
    object=blank;
    blank = NULL;
  }

  /* free sequence of formulas just processed */
  raptor_free_sequence($2);
  
  $$->value=object;
  
#if RASQAL_DEBUG > 1
  fprintf(DEBUG_FH, "  after substitution collection=");
  rasqal_formula_print($$, DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif

  rasqal_free_literal(first_identifier);
  rasqal_free_literal(rest_identifier);

  break; /* success */

  err_Collection:
  
  if(blank)
    rasqal_free_literal(blank);
  if(object)
    rasqal_free_literal(object);
  if(rest_identifier)
    rasqal_free_literal(rest_identifier);
  if(first_identifier)
    rasqal_free_literal(first_identifier);
  if($2)
    raptor_free_sequence($2);
  if($$) {
    rasqal_free_formula($$);
    $$ = NULL;
  }
  YYERROR_MSG(errmsg);
}
;


/* NEW Grammar Term pulled out of [40] Collection */
/* Sequence of formula */
GraphNodeListNotEmpty: GraphNodeListNotEmpty GraphNode
{
#if RASQAL_DEBUG > 1  
  char const *errmsg;

  fprintf(DEBUG_FH, "GraphNodeListNotEmpty 1\n");
  if($2) {
    fprintf(DEBUG_FH, "  GraphNode=");
    rasqal_formula_print($2, DEBUG_FH);
    fprintf(DEBUG_FH, "\n");
  } else  
    fprintf(DEBUG_FH, "  and empty GraphNode\n");
  if($1) {
    fprintf(DEBUG_FH, "  GraphNodeListNotEmpty=");
    raptor_sequence_print($1, DEBUG_FH);
    fprintf(DEBUG_FH, "\n");
  } else
    fprintf(DEBUG_FH, "  and empty GraphNodeListNotEmpty\n");
#endif

  $$ = $1;
  if(!$$) {
    $$ = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_formula,
                             (raptor_sequence_print_handler*)rasqal_formula_print);
    if(!$$)
      YYERR_MSG_GOTO(err_GraphNodeListNotEmpty,
                     "GraphNodeListNotEmpty: cannot create formula");
  }
  
  if($2) {
    if(raptor_sequence_push($$, $2)) {
      YYERR_MSG_GOTO(err_GraphNodeListNotEmpty,
                     "GraphNodeListNotEmpty 1: sequence push failed");
    }
    $2 = NULL;
#if RASQAL_DEBUG > 1  
    fprintf(DEBUG_FH, "  itemList is now ");
    raptor_sequence_print($$, DEBUG_FH);
    fprintf(DEBUG_FH, "\n\n");
#endif
  }

  break; /* success */

  err_GraphNodeListNotEmpty:
  if($2)
    rasqal_free_formula($2);
  if($$) {
    raptor_free_sequence($$);
    $$ = NULL;
  }
  YYERROR_MSG(errmsg);
}
| GraphNode
{
#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "GraphNodeListNotEmpty 2\n");
  if($1) {
    fprintf(DEBUG_FH, "  GraphNode=");
    rasqal_formula_print($1, DEBUG_FH);
    fprintf(DEBUG_FH, "\n");
  } else  
    fprintf(DEBUG_FH, "  and empty GraphNode\n");
#endif

  if(!$1)
    $$ = NULL;
  else {
    $$ = raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_formula,
                             (raptor_sequence_print_handler*)rasqal_formula_print);
    if(!$$) {
      rasqal_free_formula($1);
      YYERROR_MSG("GraphNodeListNotEmpty 2: cannot create sequence");
    }
    if(raptor_sequence_push($$, $1)) {
      raptor_free_sequence($$);
      $$ = NULL;
      YYERROR_MSG("GraphNodeListNotEmpty 2: sequence push failed");
    }
  }
#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "  GraphNodeListNotEmpty is now ");
  raptor_sequence_print($$, DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif
}
;


/* SPARQL Grammar: [41] GraphNode */
GraphNode: VarOrTerm
{
  $$ = $1;
}
| TriplesNode
{
  $$ = $1;
}
;


/* SPARQL Grammar Term: [42] VarOrTerm */
VarOrTerm: Var
{
  $$ = rasqal_new_formula();
  if(!$$)
    YYERROR_MSG("VarOrTerm 1: cannot create formula");
  $$->value = rasqal_new_variable_literal(((rasqal_query*)rq)->world, $1);
  if(!$$->value) {
    rasqal_free_formula($$);
    $$ = NULL;
    YYERROR_MSG("VarOrTerm 1: cannot create literal");
  }
}
| GraphTerm
{
  $$ = rasqal_new_formula();
  if(!$$) {
    if($1)
      rasqal_free_literal($1);
    YYERROR_MSG("VarOrTerm 2: cannot create formula");
  }
  $$->value = $1;
}
;

/* SPARQL Grammar: [43] VarOrIRIref */
VarOrIRIref: Var
{
  $$ = rasqal_new_variable_literal(((rasqal_query*)rq)->world, $1);
  if(!$$)
    YYERROR_MSG("VarOrIRIref: cannot create literal");
}
| IRIref
{
  $$ = $1;
}
;


/* SPARQL Grammar: [44] Var */
Var: '?' VarName
{
  $$ = $2;
}
| '$' VarName
{
  $$ = $2;
}
;

/* NEW Grammar Term made from SPARQL Grammar: [44] Var */
VarName: IDENTIFIER
{
  $$ = rasqal_new_variable((rasqal_query*)rq, $1, NULL);
  if(!$$)
    YYERROR_MSG("VarName: cannot create var");
}
;



/* SPARQL Grammar: [45] GraphTerm */
GraphTerm: IRIref
{
  $$ = $1;
}
| STRING_LITERAL
{
  $$ = $1;
}
| NumericLiteral
{
  $$ = $1;
}
| BOOLEAN_LITERAL
{
  $$ = $1;
}
| BlankNode
{
  $$ = $1;
}
|  '(' ')'
{
#ifdef RAPTOR_V2_AVAILABLE
  $$ = rasqal_new_uri_literal(((rasqal_query*)rq)->world,
                              raptor_uri_copy_v2(((rasqal_query*)rq)->world->raptor_world_ptr,
                                                 ((rasqal_query*)rq)->world->rdf_nil_uri));
#else
  $$ = rasqal_new_uri_literal(((rasqal_query*)rq)->world, 
                              raptor_uri_copy(((rasqal_query*)rq)->world->rdf_nil_uri));
#endif
  if(!$$)
    YYERROR_MSG("GraphTerm: cannot create literal");
}
;

/* SPARQL Grammar: [46] Expression */
Expression: ConditionalOrExpression
{
  $$ = $1;
}
;


/* SPARQL Grammar: [47] ConditionalOrExpression */
ConditionalOrExpression: ConditionalOrExpression SC_OR ConditionalAndExpression
{
  $$ = rasqal_new_2op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_OR, $1, $3);
  if(!$$)
    YYERROR_MSG("ConditionalOrExpression: cannot create expr");
}
| ConditionalAndExpression
{
  $$ = $1;
}
;


/* SPARQL Grammar: [48] ConditionalAndExpression */
ConditionalAndExpression: ConditionalAndExpression SC_AND RelationalExpression
{
  $$ = rasqal_new_2op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_AND, $1, $3);
  if(!$$)
    YYERROR_MSG("ConditionalAndExpression: cannot create expr");
;
}
| RelationalExpression
{
  $$ = $1;
}
;

/* SPARQL Grammar: [49] ValueLogical - merged into RelationalExpression */

/* SPARQL Grammar: [50] RelationalExpression */
RelationalExpression: AdditiveExpression EQ AdditiveExpression
{
  $$ = rasqal_new_2op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_EQ, $1, $3);
  if(!$$)
    YYERROR_MSG("RelationalExpression 1: cannot create expr");
}
| AdditiveExpression NEQ AdditiveExpression
{
  $$ = rasqal_new_2op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_NEQ, $1, $3);
  if(!$$)
    YYERROR_MSG("RelationalExpression 2: cannot create expr");
}
| AdditiveExpression LT AdditiveExpression
{
  $$ = rasqal_new_2op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_LT, $1, $3);
  if(!$$)
    YYERROR_MSG("RelationalExpression 3: cannot create expr");
}
| AdditiveExpression GT AdditiveExpression
{
  $$ = rasqal_new_2op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_GT, $1, $3);
  if(!$$)
    YYERROR_MSG("RelationalExpression 4: cannot create expr");
}
| AdditiveExpression LE AdditiveExpression
{
  $$ = rasqal_new_2op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_LE, $1, $3);
  if(!$$)
    YYERROR_MSG("RelationalExpression 5: cannot create expr");
}
| AdditiveExpression GE AdditiveExpression
{
  $$ = rasqal_new_2op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_GE, $1, $3);
  if(!$$)
    YYERROR_MSG("RelationalExpression 6: cannot create expr");
}
| AdditiveExpression
{
  $$ = $1;
}
;

/* SPARQL Grammar: [51] NumericExpression - merged into AdditiveExpression */

/* SPARQL Grammar: [52] AdditiveExpression */
AdditiveExpression: MultiplicativeExpression '+' AdditiveExpression
{
  $$ = rasqal_new_2op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_PLUS, $1, $3);
  if(!$$)
    YYERROR_MSG("AdditiveExpression 1: cannot create expr");
}
| MultiplicativeExpression '-' AdditiveExpression
{
  $$ = rasqal_new_2op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_MINUS, $1, $3);
  if(!$$)
    YYERROR_MSG("AdditiveExpression 2: cannot create expr");
}
| MultiplicativeExpression NumericLiteralPositive
{
  rasqal_expression *e;
  e = rasqal_new_literal_expression(((rasqal_query*)rq)->world, $2);
  if(!e) {
    rasqal_free_expression($1);
    YYERROR_MSG("AdditiveExpression 3: cannot create expr");
  }
  $$ = rasqal_new_2op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_PLUS, $1, e);
  if(!$$)
    YYERROR_MSG("AdditiveExpression 4: cannot create expr");
}
| MultiplicativeExpression NumericLiteralNegative
{
  rasqal_expression *e;
  e = rasqal_new_literal_expression(((rasqal_query*)rq)->world, $2);
  if(!e) {
    rasqal_free_expression($1);
    YYERROR_MSG("AdditiveExpression 5: cannot create expr");
  }
  $$ = rasqal_new_2op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_PLUS, $1, e);
  if(!$$)
    YYERROR_MSG("AdditiveExpression 6: cannot create expr");
}
| MultiplicativeExpression
{
  $$ = $1;
}
;

/* SPARQL Grammar: [53] MultiplicativeExpression */
MultiplicativeExpression: UnaryExpression '*' MultiplicativeExpression
{
  $$ = rasqal_new_2op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_STAR, $1, $3);
  if(!$$)
    YYERROR_MSG("MultiplicativeExpression 1: cannot create expr");
}
| UnaryExpression '/' MultiplicativeExpression
{
  $$ = rasqal_new_2op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_SLASH, $1, $3);
  if(!$$)
    YYERROR_MSG("MultiplicativeExpression 2: cannot create expr");
}
| UnaryExpression
{
  $$ = $1;
}
;


/* SPARQL Grammar: [54] UnaryExpression */
UnaryExpression: '!' PrimaryExpression
{
  $$ = rasqal_new_1op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_BANG, $2);
  if(!$$)
    YYERROR_MSG("UnaryExpression 1: cannot create expr");
}
| '+' PrimaryExpression
{
  $$ = $2;
}
| '-' PrimaryExpression
{
  $$ = rasqal_new_1op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_UMINUS, $2);
  if(!$$)
    YYERROR_MSG("UnaryExpression 3: cannot create expr");
}
| PrimaryExpression
{
  $$ = $1;
}
;


/* SPARQL Grammar: [55] PrimaryExpression
 * == BrackettedExpression | BuiltInCall | IRIrefOrFunction | RDFLiteral | NumericLiteral | BooleanLiteral | BlankNode | Var
 * == BrackettedExpression | BuiltInCall | IRIref ArgList? | RDFLiteral | NumericLiteral | BooleanLiteral | BlankNode | Var
 * == BrackettedExpression | BuiltInCall | FunctionCall |
 *    approximately GraphTerm | Var
 * 
*/
PrimaryExpression: BrackettedExpression 
{
  $$ = $1;
}
| BuiltInCall
{
  $$ = $1;
}
| FunctionCall
{
  /* Grammar has IRIrefOrFunction here which is "IRIref ArgList?"
   * and essentially shorthand for FunctionCall | IRIref.  The Rasqal
   * SPARQL lexer distinguishes these for us with IRIrefBrace.
   * IRIref is covered below by GraphTerm.
   */
  $$ = $1;
}
| GraphTerm
{
  $$ = rasqal_new_literal_expression(((rasqal_query*)rq)->world, $1);
  if(!$$)
    YYERROR_MSG("PrimaryExpression 4: cannot create expr");
}
| Var
{
  rasqal_literal *l;
  l = rasqal_new_variable_literal(((rasqal_query*)rq)->world, $1);
  if(!l)
    YYERROR_MSG("PrimaryExpression 5: cannot create literal");
  $$ = rasqal_new_literal_expression(((rasqal_query*)rq)->world, l);
  if(!$$)
    YYERROR_MSG("PrimaryExpression 5: cannot create expr");
}
;


/* SPARQL Grammar: [56] BrackettedExpression */
BrackettedExpression: '(' Expression ')'
{
  $$ = $2;
}
;


/* SPARQL Grammar: [57] BuiltInCall */
BuiltInCall: STR '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_STR, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 1: cannot create expr");
}
| LANG '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_LANG, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 2: cannot create expr");
}
| LANGMATCHES '(' Expression ',' Expression ')'
{
  $$ = rasqal_new_2op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_LANGMATCHES, $3, $5);
  if(!$$)
    YYERROR_MSG("BuiltInCall 3: cannot create expr");
}
| DATATYPE '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_DATATYPE, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 4: cannot create expr");
}
| BOUND '(' Var ')'
{
  rasqal_literal *l;
  rasqal_expression *e;
  l = rasqal_new_variable_literal(((rasqal_query*)rq)->world, $3);
  if(!l)
    YYERROR_MSG("BuiltInCall 5: cannot create literal");
  e = rasqal_new_literal_expression(((rasqal_query*)rq)->world, l);
  if(!e)
    YYERROR_MSG("BuiltInCall 6: cannot create literal expr");
  $$ = rasqal_new_1op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_BOUND, e);
  if(!$$)
    YYERROR_MSG("BuiltInCall 7: cannot create expr");
}
| SAMETERM '(' Expression ',' Expression ')'
{
  $$ = rasqal_new_2op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_SAMETERM, $3, $5);
  if(!$$)
    YYERROR_MSG("BuiltInCall 8: cannot create expr");
}
| ISURI '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_ISURI, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 9: cannot create expr");
}
| ISBLANK '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_ISBLANK, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 10: cannot create expr");
}
| ISLITERAL '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_ISLITERAL, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 11: cannot create expr");
}
| RegexExpression
{
  $$ = $1;
}
| CoalesceExpression
{
  $$ = $1;
}
;


/* SPARQL Grammar: [58] RegexExpression */
RegexExpression: REGEX '(' Expression ',' Expression ')'
{
  $$ = rasqal_new_3op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_REGEX, $3, $5, NULL);
  if(!$$)
    YYERROR_MSG("RegexExpression 1: cannot create expr");
}
| REGEX '(' Expression ',' Expression ',' Expression ')'
{
  $$ = rasqal_new_3op_expression(((rasqal_query*)rq)->world,
                                 RASQAL_EXPR_REGEX, $3, $5, $7);
  if(!$$)
    YYERROR_MSG("RegexExpression 2: cannot create expr");
}
;

/* SPARQL Grammar: [59] IRIrefOrFunction - not necessary in this
   grammar as the IRIref ambiguity is determined in lexer with the
   help of the IRIrefBrace token below */

/* NEW Grammar Term made from SPARQL Grammar: [64] IRIref + '(' expanded */
IRIrefBrace: URI_LITERAL_BRACE
{
  $$ = rasqal_new_uri_literal(((rasqal_query*)rq)->world, $1);
  if(!$$)
    YYERROR_MSG("IRIrefBrace 1: cannot create literal");
}
| QNAME_LITERAL_BRACE
{
  $$ = rasqal_new_simple_literal(((rasqal_query*)rq)->world,
                                 RASQAL_LITERAL_QNAME, $1);
  if(!$$)
    YYERROR_MSG("IRIrefBrace 2: cannot create literal");
  if(rasqal_literal_expand_qname((rasqal_query*)rq, $$)) {
    sparql_query_error_full((rasqal_query*)rq,
                            "QName %s cannot be expanded", $1);
    rasqal_free_literal($$);
    $$ = NULL;
    YYERROR_MSG("IRIrefBrace 2: cannot expand qname");
  }
}
;


/* SPARQL Grammar: [60] RDFLiteral - merged into GraphTerm */

/* SPARQL Grammar: [61] NumericLiteral */
NumericLiteral: NumericLiteralUnsigned
{
  $$ = $1;
}
| NumericLiteralPositive
{
  $$ = $1;
}
| NumericLiteralNegative
{
  $$ = $1;
}
;

/* SPARQL Grammer: [62] NumericLiteralUnsigned */
NumericLiteralUnsigned: INTEGER_LITERAL
{
  $$ = $1;
}
| DECIMAL_LITERAL
{
  $$ = $1;
}
| DOUBLE_LITERAL
{
  $$ = $1;
}
;


 /* SPARQL Grammer: [63] NumericLiteralPositive */
NumericLiteralPositive: INTEGER_POSITIVE_LITERAL
{
  $$ = $1;
}
| DECIMAL_POSITIVE_LITERAL
{
  $$ = $1;
}
| DOUBLE_POSITIVE_LITERAL
{
  $$ = $1;
}
;


/* SPARQL Grammar: [64] NumericLiteralNegative */
NumericLiteralNegative: INTEGER_NEGATIVE_LITERAL
{
  $$ = $1;
}
| DECIMAL_NEGATIVE_LITERAL
{
  $$ = $1;
}
| DOUBLE_NEGATIVE_LITERAL
{
  $$ = $1;
}
;


/* SPARQL Grammar: [62] BooleanLiteral - merged into GraphTerm */

/* SPARQL Grammar: [63] String - merged into GraphTerm */

/* SPARQL Grammar: [64] IRIref */
IRIref: URI_LITERAL
{
  $$ = rasqal_new_uri_literal(((rasqal_query*)rq)->world, $1);
  if(!$$)
    YYERROR_MSG("IRIref 1: cannot create literal");
}
| QNAME_LITERAL
{
  $$ = rasqal_new_simple_literal(((rasqal_query*)rq)->world,
                                 RASQAL_LITERAL_QNAME, $1);
  if(!$$)
    YYERROR_MSG("IRIref 2: cannot create literal");
  if(rasqal_literal_expand_qname((rasqal_query*)rq, $$)) {
    sparql_query_error_full((rasqal_query*)rq,
                            "QName %s cannot be expanded", $1);
    rasqal_free_literal($$);
    $$ = NULL;
    YYERROR_MSG("IRIrefBrace 2: cannot expand qname");
  }
}
;


/* SPARQL Grammar: [65] QName - made into terminal QNAME_LITERAL */

/* SPARQL Grammar: [66] BlankNode */
BlankNode: BLANK_LITERAL
{
  $$ = rasqal_new_simple_literal(((rasqal_query*)rq)->world,
                                 RASQAL_LITERAL_BLANK, $1);
  if(!$$)
    YYERROR_MSG("BlankNode 1: cannot create literal");
} | '[' ']'
{
  const unsigned char *id;
  id = rasqal_query_generate_bnodeid((rasqal_query*)rq, NULL);
  if(!id)
    YYERROR_MSG("BlankNode 2: cannot create bnodeid");
  $$ = rasqal_new_simple_literal(((rasqal_query*)rq)->world,
                                 RASQAL_LITERAL_BLANK, id);
  if(!$$)
    YYERROR_MSG("BlankNode 2: cannot create literal");
}
;

/* SPARQL Grammar: [67] Q_IRI_REF onwards are all lexer items
 * with similar names or are inlined.
 */




%%


/* Support functions */


/* This is declared in sparql_lexer.h but never used, so we always get
 * a warning unless this dummy code is here.  Used once below in an error case.
 */
static int yy_init_globals (yyscan_t yyscanner ) { return 0; };


/**
 * rasqal_sparql_query_language_init - Initialise the SPARQL query language parser
 *
 * Return value: non 0 on failure
 **/
static int
rasqal_sparql_query_language_init(rasqal_query* rdf_query, const char *name)
{
  rasqal_sparql_query_language* rqe;

  rqe = (rasqal_sparql_query_language*)rdf_query->context;

  rdf_query->compare_flags = RASQAL_COMPARE_XQUERY;

  rqe->extended = (strcmp(name, "laqrs") == 0);
  return 0;
}


/**
 * rasqal_sparql_query_language_terminate - Free the SPARQL query language parser
 *
 * Return value: non 0 on failure
 **/
static void
rasqal_sparql_query_language_terminate(rasqal_query* rdf_query)
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)rdf_query->context;

  if(sparql && sparql->scanner_set) {
    sparql_lexer_lex_destroy(sparql->scanner);
    sparql->scanner_set = 0;
  }

}


static int
rasqal_sparql_query_language_prepare(rasqal_query* rdf_query)
{
  /* rasqal_sparql_query_language* sparql = (rasqal_sparql_query_language*)rdf_query->context; */
  int rc;
  
  if(!rdf_query->query_string)
    return 1;
  
  rc=sparql_parse(rdf_query);
  if(rc)
    return rc;

  /* FIXME - should check remaining query parts  */
  if(rasqal_sequence_has_qname(rdf_query->triples) ||
     rasqal_sequence_has_qname(rdf_query->constructs) ||
     rasqal_query_constraints_has_qname(rdf_query)) {
    sparql_query_error(rdf_query, "SPARQL query has unexpanded QNames");
    return 1;
  }

  /* SPARQL: Turn [] into anonymous variables */
  if(rasqal_query_build_anonymous_variables(rdf_query))
    return 1;
  
  /* SPARQL: Expand 'SELECT *' */
  if(rasqal_query_expand_wildcards(rdf_query))
    return 1;
  
  return 0;
}


static int
sparql_parse(rasqal_query* rq)
{
  rasqal_sparql_query_language* rqe;
  raptor_locator *locator=&rq->locator;
  void *buffer;

  rqe = (rasqal_sparql_query_language*)rq->context;

  if(!rq->query_string)
    return yy_init_globals(NULL); /* 0 but a way to use yy_init_globals */

  locator->line = 1;
  locator->column = -1; /* No column info */
  locator->byte = -1; /* No bytes info */

#if RASQAL_DEBUG > 2
  sparql_parser_debug = 1;
#endif

  rqe->lineno = 1;

  if(sparql_lexer_lex_init(&rqe->scanner))
    return 1;
  rqe->scanner_set = 1;

  sparql_lexer_set_extra(((rasqal_query*)rq), rqe->scanner);

  buffer = sparql_lexer__scan_buffer((char*)rq->query_string,
                                     rq->query_string_length, rqe->scanner);

  rqe->error_count = 0;

  sparql_parser_parse(rq);

  sparql_lexer_lex_destroy(rqe->scanner);
  rqe->scanner_set = 0;

  /* Parsing failed */
  if(rq->failed)
    return 1;
  
  return 0;
}


static void
sparql_query_error(rasqal_query *rq, const char *msg)
{
  rasqal_sparql_query_language* rqe;

  rqe = (rasqal_sparql_query_language*)rq->context;

  if(rqe->error_count++)
    return;

  rq->locator.line = rqe->lineno;
#ifdef RASQAL_SPARQL_USE_ERROR_COLUMNS
  /*  rq->locator.column = sparql_lexer_get_column(yyscanner);*/
#endif

  rq->failed = 1;
  rasqal_log_error_simple(((rasqal_query*)rq)->world, RAPTOR_LOG_LEVEL_ERROR,
                          &rq->locator, "%s", msg);
}


static void
sparql_query_error_full(rasqal_query *rq, const char *message, ...)
{
  va_list arguments;
  rasqal_sparql_query_language* rqe;

  rqe = (rasqal_sparql_query_language*)rq->context;

  if(rqe->error_count++)
    return;

  rq->locator.line = rqe->lineno;
#ifdef RASQAL_SPARQL_USE_ERROR_COLUMNS
  /*  rq->locator.column = sparql_lexer_get_column(yyscanner);*/
#endif

  va_start(arguments, message);

  rq->failed = 1;
  rasqal_log_error_varargs(((rasqal_query*)rq)->world, RAPTOR_LOG_LEVEL_ERROR,
                           &rq->locator, message, arguments);

  va_end(arguments);
}


int
sparql_syntax_error(rasqal_query *rq, const char *message, ...)
{
  rasqal_sparql_query_language *rqe;
  va_list arguments;

  rqe = (rasqal_sparql_query_language*)rq->context;

  if(rqe->error_count++)
    return 0;

  rq->locator.line=rqe->lineno;
#ifdef RASQAL_SPARQL_USE_ERROR_COLUMNS
  /*  rp->locator.column=sparql_lexer_get_column(yyscanner);*/
#endif

  va_start(arguments, message);
  rq->failed = 1;
  rasqal_log_error_varargs(((rasqal_query*)rq)->world, RAPTOR_LOG_LEVEL_ERROR,
                           &rq->locator, message, arguments);
  va_end(arguments);

  return 0;
}


int
sparql_syntax_warning(rasqal_query *rq, const char *message, ...)
{
  rasqal_sparql_query_language *rqe;
  va_list arguments;

  rqe = (rasqal_sparql_query_language*)rq->context;

  rq->locator.line = rqe->lineno;
#ifdef RASQAL_SPARQL_USE_ERROR_COLUMNS
  /*  rq->locator.column=sparql_lexer_get_column(yyscanner);*/
#endif

  va_start(arguments, message);
  rasqal_log_error_varargs(((rasqal_query*)rq)->world,
                           RAPTOR_LOG_LEVEL_WARNING, &rq->locator, message,
                           arguments);
  va_end(arguments);

  return (0);
}


static int
rasqal_sparql_query_language_iostream_write_escaped_counted_string(rasqal_query* query,
                                                                 raptor_iostream* iostr,
                                                                 const unsigned char* string,
                                                                 size_t len)
{
  const char delim = '"';
  
  raptor_iostream_write_byte(iostr, delim);
  if(raptor_iostream_write_string_ntriples(iostr, string, len, delim))
    return 1;
  
  raptor_iostream_write_byte(iostr, delim);

  return 0;
}


static void
rasqal_sparql_query_language_register_factory(rasqal_query_language_factory *factory)
{
  factory->context_length = sizeof(rasqal_sparql_query_language);

  factory->init      = rasqal_sparql_query_language_init;
  factory->terminate = rasqal_sparql_query_language_terminate;
  factory->prepare   = rasqal_sparql_query_language_prepare;
  factory->iostream_write_escaped_counted_string = rasqal_sparql_query_language_iostream_write_escaped_counted_string;
}


int
rasqal_init_query_language_sparql(rasqal_world* world) {
  return rasqal_query_language_register_factory(world,
                                                "sparql", 
                                                "SPARQL W3C DAWG RDF Query Language",
                                                NULL,
                                                (const unsigned char*)"http://www.w3.org/TR/rdf-sparql-query/",
                                                &rasqal_sparql_query_language_register_factory);
}

int
rasqal_init_query_language_laqrs(rasqal_world* world) {
  return rasqal_query_language_register_factory(world,
                                                "laqrs", 
                                                "LAQRS adds to Querying RDF in SPARQL",
                                                NULL,
                                                NULL,
                                                &rasqal_sparql_query_language_register_factory);
}


#ifdef STANDALONE
#include <stdio.h>
#include <locale.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifndef HAVE_GETOPT
#include <rasqal_getopt.h>
#endif

#ifdef NEED_OPTIND_DECLARATION
extern int optind;
extern char *optarg;
#endif

#define GETOPT_STRING "di:"

#define SPARQL_FILE_BUF_SIZE 2048

int
main(int argc, char *argv[]) 
{
  const char *program = rasqal_basename(argv[0]);
  char query_string[SPARQL_FILE_BUF_SIZE];
  rasqal_query *query;
  FILE *fh;
  int rc;
  const char *filename = NULL;
  raptor_uri* base_uri = NULL;
  unsigned char *uri_string;
  const char* query_languages[2] = {"sparql", "laqrs"};
  const char* query_language;
  int usage = 0;
  rasqal_world *world;
  
  query_language=query_languages[0];
  
  filename = getenv("SPARQL_QUERY_FILE");
    
  while(!usage) {
    int c = getopt (argc, argv, GETOPT_STRING);

    if (c == -1)
      break;

    switch (c) {
      case 0:
      case '?': /* getopt() - unknown option */
        usage = 1;
        break;
        
      case 'd':
#if RASQAL_DEBUG > 2
        sparql_parser_debug = 1;
#endif
        break;
  
      case 'i':
        if(optarg) {
          if(!strcmp(optarg, "laqrs")) {
            query_language = query_languages[1];
          } else if(!strcmp(optarg, "sparql")) {
            query_language = query_languages[0];
          } else {
            fprintf(stderr, "-i laqrs or -i sparql only\n");
            usage = 1;
          }
        }
        break;
    }
  }

  if(!filename) {
    if((argc-optind) != 1) {
      fprintf(stderr, "%s: Too many arguments.\n", program);
      usage = 1;
    } else
      filename = argv[optind];
  }
  
  if(usage) {
    fprintf(stderr, "SPARQL/LAQRS parser test for Rasqal %s\n", 
            rasqal_version_string);
    fprintf(stderr, "USAGE: %s [OPTIONS] SPARQL-QUERY-FILE\n", program);
    fprintf(stderr, "OPTIONS:\n");
#if RASQAL_DEBUG > 2
    fprintf(stderr, " -d           Bison parser debugging\n");
#endif
    fprintf(stderr, " -i LANGUAGE  Set query language\n");
    exit(1);
  }


 fh = fopen(filename, "r");
 if(!fh) {
   fprintf(stderr, "%s: Cannot open file %s - %s\n", program, filename,
           strerror(errno));
   exit(1);
 }
 
  memset(query_string, 0, SPARQL_FILE_BUF_SIZE);
  rc=fread(query_string, SPARQL_FILE_BUF_SIZE, 1, fh);
  if(rc < SPARQL_FILE_BUF_SIZE) {
    if(ferror(fh)) {
      fprintf(stderr, "%s: file '%s' read failed - %s\n",
              program, filename, strerror(errno));
      fclose(fh);
      return(1);
    }
  }
  
  fclose(fh);

  world = rasqal_new_world();
  if(!world || rasqal_world_open(world))
    exit(1);

  query = rasqal_new_query(world, query_language, NULL);

  uri_string = raptor_uri_filename_to_uri_string(filename);
#ifdef RAPTOR_V2_AVAILABLE
  base_uri = raptor_new_uri_v2(world->raptor_world_ptr, uri_string);
#else
  base_uri = raptor_new_uri(uri_string);
#endif

  rc = rasqal_query_prepare(query, 
                            (const unsigned char*)query_string, base_uri);

  rasqal_query_print(query, stdout);

  rasqal_free_query(query);

#ifdef RAPTOR_V2_AVAILABLE
  raptor_free_uri_v2(world->raptor_world_ptr, base_uri);
#else
  raptor_free_uri(base_uri);
#endif

  raptor_free_memory(uri_string);

  rasqal_free_world(world);

  return rc;
}
#endif

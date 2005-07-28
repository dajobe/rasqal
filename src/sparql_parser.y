/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * sparql_parser.y - Rasqal SPARQL parser over tokens from sparql_lexer.l
 *
 * $Id$
 *
 * Copyright (C) 2004-2005, David Beckett http://purl.org/net/dajobe/
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
 *   SPARQL Query Language for RDF, 19 April 2005
 *   http://www.w3.org/TR/2005/WD-rdf-sparql-query-20050419/
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
#define YY_NO_UNISTD_H 1
#include <sparql_lexer.h>

#include <sparql_common.h>

/*
#undef RASQAL_DEBUG
#define RASQAL_DEBUG 2
*/

/* Make verbose error messages for syntax errors */
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
static int sparql_is_builtin_xsd_datatype(raptor_uri* uri);

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
 * 6 in region of
 *   Triples: Subject *HERE* PropertyListOpt
 * for all the tokens: A, [, ?, $, URI/QNAME/BLANK LITERALs
 * either accepting PropertyListOpt (reduce, the right choice, $default)
 * or ending the triple at the subject (shift, the wrong choice)
 *
 * 6 in region of
 *   PropertyListTail: ';' *HERE* PropertyListOpt
 * for all the same tokens as above
 * accepting PropertyListOpt (reduce, $default)
 * or ending the list (shift, the wrong choice)
 */
%expect 12

/* word symbols */
%token SELECT FROM WHERE
%token OPTIONAL PREFIX DESCRIBE CONSTRUCT ASK DISTINCT LIMIT UNION
%token BASE BOUND STR LANG DATATYPE ISURI ISBLANK ISLITERAL
%token GRAPH NAMED FILTER OFFSET A ORDER BY REGEX ASC DESC

/* expression delimitors */

%token ',' '(' ')' '[' ']' '{' '}'
%token '?' '$'

/* SC booleans */
%left SC_OR SC_AND

/* operations */
%left EQ NEQ LT GT LE GE

/* arithmetic operations */
%left '+' '-' '*' '/' '%'

/* unary operations */
%left '~' '!'

/* literals */
%token <literal> FLOATING_POINT_LITERAL
%token <literal> STRING_LITERAL INTEGER_LITERAL
%token <literal> BOOLEAN_LITERAL
%token <literal> NULL_LITERAL
%token <uri> URI_LITERAL URI_LITERAL_BRACE
%token <name> QNAME_LITERAL BLANK_LITERAL QNAME_LITERAL_BRACE

%token <name> IDENTIFIER


%type <seq> SelectClause ConstructClause DescribeClause
%type <seq> VarList VarOrIRIrefList ArgList TriplesList
%type <seq> ConstructTemplate OrderConditionList
%type <seq> ItemList

%type <formula> Triples
%type <formula> PropertyListOpt PropertyList PropertyListTail 
%type <formula> ObjectList ObjectTail Collection
%type <formula> Subject Predicate Object TriplesNode

%type <graph_pattern> GroupGraphPattern GraphPattern PatternElement
%type <graph_pattern> GraphGraphPattern OptionalGraphPattern
%type <graph_pattern> UnionGraphPattern UnionGraphPatternList
%type <graph_pattern> PatternElementsList

%type <expr> Expression ConditionalAndExpression
%type <expr> RelationalExpression AdditiveExpression
%type <expr> MultiplicativeExpression UnaryExpression
%type <expr> CallExpression RegexExpression FunctionCall
%type <expr> BrackettedExpression PrimaryExpression
%type <expr> OrderCondition

%type <literal> RDFTerm IRIref BlankNode
%type <literal> VarOrLiteral VarOrIRIref VarOrBnodeOrURI
%type <literal> VarOrBlankNodeOrIRIref
%type <literal> IRIrefBrace

%type <variable> Var


%%

/* Below here, grammar terms are numbered from
 * http://www.w3.org/TR/2005/WD-rdf-sparql-query-20050217/
 * except where noted
 */

/* SPARQL Grammar: [1] Query */
Query : BaseDeclOpt PrefixDeclOpt ReportFormat
        DatasetClauseOpt WhereClauseOpt 
        OrderClauseOpt LimitClauseOpt OffsetClauseOpt
{
}
;


/* NEW Grammar Term pulled out of [1] Query */
ReportFormat : SelectClause
{
  ((rasqal_query*)rq)->selects=$1;
  ((rasqal_query*)rq)->verb=RASQAL_QUERY_VERB_SELECT;
}
|  ConstructClause
{
  ((rasqal_query*)rq)->constructs=$1;
  ((rasqal_query*)rq)->verb=RASQAL_QUERY_VERB_CONSTRUCT;
}
|  DescribeClause
{
  ((rasqal_query*)rq)->describes=$1;
  ((rasqal_query*)rq)->verb=RASQAL_QUERY_VERB_DESCRIBE;
}
| AskClause
{
  ((rasqal_query*)rq)->verb=RASQAL_QUERY_VERB_ASK;
}
;

/* SPARQL Grammar: [2] Prolog - merged into Query */


/* SPARQL Grammar: [3] BaseDecl */
BaseDeclOpt : BASE URI_LITERAL
{
  if(((rasqal_query*)rq)->base_uri)
    raptor_free_uri(((rasqal_query*)rq)->base_uri);
  ((rasqal_query*)rq)->base_uri=$2;
}
| /* empty */
{
  /* nothing to do */
}
;


/* SPARQL Grammar: [4] PrefixDecl */
PrefixDeclOpt : PrefixDeclOpt PREFIX IDENTIFIER URI_LITERAL
{
  rasqal_prefix *p=rasqal_new_prefix($3, $4);
  raptor_sequence *seq=((rasqal_query*)rq)->prefixes;
  raptor_sequence_push(seq, p);
  rasqal_engine_declare_prefix(((rasqal_query*)rq), p);
}
| /* empty */
{
  /* nothing to do, rq->prefixes already initialised */
}
;


/* SPARQL Grammar: [5] SelectClause */
SelectClause : SELECT DISTINCT VarList
{
  $$=$3;
  ((rasqal_query*)rq)->distinct=1;
}
| SELECT DISTINCT '*'
{
  $$=NULL;
  ((rasqal_query*)rq)->wildcard=1;
  ((rasqal_query*)rq)->distinct=1;
}
| SELECT VarList
{
  $$=$2;
}
| SELECT '*'
{
  $$=NULL;
  ((rasqal_query*)rq)->wildcard=1;
}
;


/* SPARQL Grammar: [6] DescribeClause */
DescribeClause : DESCRIBE VarOrIRIrefList
{
  $$=$2;
}
| DESCRIBE '*'
{
  $$=NULL;
}
;


/* SPARQL Grammar: [7] ConstructClause */
ConstructClause: CONSTRUCT ConstructTemplate
{
  $$=$2;
}
| CONSTRUCT '*'
{
  $$=NULL;
  ((rasqal_query*)rq)->wildcard=1;
}
;


/* SPARQL Grammar: [8] AskClause */
AskClause : ASK 
{
  /* nothing to do */
}


/* SPARQL Grammar: rq23 [9] DatasetClause */
DatasetClauseOpt : DatasetClauseOpt FROM DefaultGraphClause
| DatasetClauseOpt FROM NamedGraphClause
| /* empty */
;


/* SPARQL Grammar: rq23 [10] DefaultGraphClause */
DefaultGraphClause : IRIref
{
  if($1) {
    raptor_uri* uri=rasqal_literal_as_uri($1);
    rasqal_query_add_data_graph((rasqal_query*)rq, uri, uri, RASQAL_DATA_GRAPH_BACKGROUND);
    rasqal_free_literal($1);
  }
}
;  


/* SPARQL Grammar: rq23 [11] NamedGraphClause */
NamedGraphClause: NAMED IRIref
{
  if($2) {
    raptor_uri* uri=rasqal_literal_as_uri($2);
    rasqal_query_add_data_graph((rasqal_query*)rq, uri, uri, RASQAL_DATA_GRAPH_NAMED);
    rasqal_free_literal($2);
  }
}
;


/* SPARQL Grammar: [12] SourceSelector - junk */


/* SPARQL Grammar: [13] WhereClause - remained for clarity */
WhereClauseOpt:  WHERE GroupGraphPattern
{
  ((rasqal_query*)rq)->query_graph_pattern=$2;
}
| GroupGraphPattern
{
  sparql_syntax_warning(((rasqal_query*)rq), "WHERE omitted");
  ((rasqal_query*)rq)->query_graph_pattern=$1;
}
| /* empty */
;


/* SPARQL Grammar: rq23 [14] SolutionModifier - merged into SelectClause etc. */

/* SPARQL Grammar: rq23 [15] OrderClause - remained for clarity */
OrderClauseOpt:  ORDER BY OrderConditionList
{
  if(((rasqal_query*)rq)->verb == RASQAL_QUERY_VERB_ASK) {
    sparql_query_error((rasqal_query*)rq, "ORDER BY cannot be used with ASK");
  } else {
    ((rasqal_query*)rq)->order_conditions_sequence=$3;
  }
}
| /* empty */
;


/* NEW Grammar Term pulled out of rq23 [16] OrderClauseOpt */
OrderConditionList: OrderConditionList OrderCondition
{
  $$=$1;
  if($2)
    raptor_sequence_push($$, $2);
}
| OrderCondition
{
  $$=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_expression, (raptor_sequence_print_handler*)rasqal_expression_print);
  if($1)
    raptor_sequence_push($$, $1);
}
;


/* SPARQL Grammar: rq23 [16] OrderCondition */
OrderCondition: ASC BrackettedExpression
{
  $$=rasqal_new_1op_expression(RASQAL_EXPR_ORDER_COND_ASC, $2);
}
| DESC BrackettedExpression
{
  $$=rasqal_new_1op_expression(RASQAL_EXPR_ORDER_COND_DESC, $2);
}
| ASC '[' Expression ']'
{
  RASQAL_DEPRECATED_WARNING((rasqal_query*)rq, "ORDER BY ASC[] is deprecated and replaced by ORDER BY ASC()");
  $$=rasqal_new_1op_expression(RASQAL_EXPR_ORDER_COND_ASC, $3);
}
| DESC '[' Expression ']'
{
  RASQAL_DEPRECATED_WARNING((rasqal_query*)rq, "ORDER BY DESC[] is deprecated and replaced by ORDER BY DESC()");
  $$=rasqal_new_1op_expression(RASQAL_EXPR_ORDER_COND_DESC, $3);
}
| FunctionCall 
{
  /* The direction of ordering is ascending by default */
  $$=rasqal_new_1op_expression(RASQAL_EXPR_ORDER_COND_ASC, $1);
}
| Var
{
  rasqal_literal* l=rasqal_new_variable_literal($1);
  rasqal_expression *e=rasqal_new_literal_expression(l);
  /* The direction of ordering is ascending by default */
  $$=rasqal_new_1op_expression(RASQAL_EXPR_ORDER_COND_ASC, e);
}
| BrackettedExpression
{
  /* The direction of ordering is ascending by default */
  $$=rasqal_new_1op_expression(RASQAL_EXPR_ORDER_COND_ASC, $1);
}
;


/* SPARQL Grammar: rq23 [17] LimitClause - remained for clarity */
LimitClauseOpt :  LIMIT INTEGER_LITERAL
{
  if(((rasqal_query*)rq)->verb == RASQAL_QUERY_VERB_ASK) {
    sparql_query_error((rasqal_query*)rq, "LIMIT cannot be used with ASK");
  } else {
    if($2 != NULL)
      ((rasqal_query*)rq)->limit=$2->value.integer;
  }
  
}
| /* empty */
;

/* SPARQL Grammar: rq23 [18] OffsetClause - remained for clarity */
OffsetClauseOpt :  OFFSET INTEGER_LITERAL
{
  if(((rasqal_query*)rq)->verb == RASQAL_QUERY_VERB_ASK) {
    sparql_query_error((rasqal_query*)rq, "LIMIT cannot be used with ASK");
  } else {
    if($2 != NULL)
      ((rasqal_query*)rq)->offset=$2->value.integer;
  }
}
| /* empty */
;

/* SPARQL Grammar: rq23 [19] GroupGraphPattern */
GroupGraphPattern: '{' GraphPattern '}'
{
  $$=$2;
}

/* NEW Grammar Term */
DotOptional: '.'
| /* empty */
;

/* SPARQL Grammar: rq23 [20] GraphPattern */
GraphPattern: PatternElementsList DotOptional
{
#if RASQAL_DEBUG > 1  
  printf("GraphPattern 1\n  GraphPattern=");
  rasqal_graph_pattern_print($1, stdout);
  fputs("\n\n", stdout);
#endif

  $$=$1;
}
| DotOptional /* empty list */
{
  $$=NULL;
}
;

/* NEW Grammar Term */
PatternElementsList: PatternElementsList DotOptional PatternElement
{
#if RASQAL_DEBUG > 1  
  printf("PatternElementList 1\n  PatternElementList=");
  rasqal_graph_pattern_print($1, stdout);
  printf(", patternelement=");
  if($3)
    rasqal_graph_pattern_print($3, stdout);
  else
    fputs("NULL", stdout);
  fputs("\n\n", stdout);
#endif

  $$=$1;
  if($3)
    raptor_sequence_push($$->graph_patterns, $3);
}
| PatternElementsList '.' FILTER Expression
{
#if RASQAL_DEBUG > 1  
  printf("PatternElementsList 2\n  PatternElementsList=");
  rasqal_graph_pattern_print($1, stdout);
  printf(", expression=");
  rasqal_expression_print($4, stdout);
  fputs("\n\n", stdout);
#endif


  $$=$1;
  rasqal_graph_pattern_add_constraint($$, $4);
}
| PatternElement
{
  raptor_sequence *seq;

#if RASQAL_DEBUG > 1  
  printf("PatternElementsList 3\n  PatternElement=");
  if($1)
    rasqal_graph_pattern_print($1, stdout);
  else
    fputs("NULL", stdout);
  fputs("\n\n", stdout);
#endif

  seq=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_graph_pattern, (raptor_sequence_print_handler*)rasqal_graph_pattern_print);
  if($1)
    raptor_sequence_push(seq, $1);

  $$=rasqal_new_graph_pattern_from_sequence((rasqal_query*)rq, seq, 
                                            RASQAL_GRAPH_PATTERN_OPERATOR_GROUP);
}
;

/* SPARQL Grammar: rq23 [20] GraphPattern + [21] GraphPatternNotTriples */
PatternElement: Triples
{
#if RASQAL_DEBUG > 1  
  printf("PatternElement 1\n  Triples=");
  if($1)
    rasqal_formula_print($1, stdout);
  else
    fputs("NULL", stdout);
  fputs("\n\n", stdout);
#endif

  if($1) {
    raptor_sequence *s=((rasqal_query*)rq)->triples;
    raptor_sequence *t=$1->triples;
    int offset=raptor_sequence_size(s);
    int triple_pattern_size=0;

    if(t) {
      triple_pattern_size=raptor_sequence_size(t);
      raptor_sequence_join(s, t);
    }
    rasqal_free_formula($1);

    $$=rasqal_new_graph_pattern_from_triples((rasqal_query*)rq, s, offset, offset+triple_pattern_size-1, RASQAL_GRAPH_PATTERN_OPERATOR_BASIC);
  } else
    $$=NULL;
}
| OptionalGraphPattern
{
  $$=$1;
}
| GroupGraphPattern
{
  $$=$1;
}
| UnionGraphPattern
{
  $$=$1;
}
| GraphGraphPattern
{
  $$=$1;
}
;


/* SPARQL Grammar: rq23 [22] OptionalGraphPattern */
OptionalGraphPattern: OPTIONAL GroupGraphPattern
{
#if RASQAL_DEBUG > 1  
  printf("PatternElementForms 4\n  graphpattern=");
  if($2)
    rasqal_graph_pattern_print($2, stdout);
  else
    fputs("NULL", stdout);
  fputs("\n\n", stdout);
#endif

  if($2)
    $2->op = RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL;

  $$=$2;
}
;


/* SPARQL Grammar: rq23 [23] GraphGraphPattern */
GraphGraphPattern: GRAPH VarOrIRIref GroupGraphPattern
{
#if RASQAL_DEBUG > 1  
  printf("GraphGraphPattern 2\n  varoruri=");
  rasqal_literal_print($2, stdout);
  printf(", graphpattern=");
  rasqal_graph_pattern_print($3, stdout);
  fputs("\n\n", stdout);
#endif

  rasqal_graph_pattern_set_origin($3, $2);
  $3->op = RASQAL_GRAPH_PATTERN_OPERATOR_GRAPH;

  rasqal_free_literal($2);
  $$=$3;
}
;


/* SPARQL Grammar: rq23 [24] GroupOrUnionGraphPattern - just the UNION bit */
UnionGraphPattern: GroupGraphPattern UNION UnionGraphPatternList
{
  $$=$3;
  raptor_sequence_push($$->graph_patterns, $1);

#if RASQAL_DEBUG > 1  
  printf("UnionGraphPattern\n  graphpattern=");
  rasqal_graph_pattern_print($$, stdout);
  fputs("\n\n", stdout);
#endif

}
;

/* NEW Grammar Term pulled out of rq23 [24] UnionGraphPattern */
UnionGraphPatternList: UnionGraphPatternList UNION GroupGraphPattern
{
  $$=$1;
  if($3)
    raptor_sequence_push($$->graph_patterns, $3);
}
| GroupGraphPattern
{
  raptor_sequence *seq;
  seq=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_graph_pattern, (raptor_sequence_print_handler*)rasqal_graph_pattern_print);
  if($1)
    raptor_sequence_push(seq, $1);
  $$=rasqal_new_graph_pattern_from_sequence((rasqal_query*)rq,
                                            seq,
                                            RASQAL_GRAPH_PATTERN_OPERATOR_UNION);
}
;


/* SPARQL Grammar: rq23 [25] Constraint - inlined into PatternElement */


/* SPARQL Grammar: rq23 [26] ConstructTemplate */
ConstructTemplate:  '{' TriplesList DotOptional '}'
{
  $$=$2;
}
;


/* NEW Grammar Term pulled out of rq23 [27] ConstructTemplate */
TriplesList: TriplesList '.' Triples
{
  if($3) {
    if($3->triples)
      raptor_sequence_join($1, $3->triples);

    rasqal_free_formula($3);
  }
  
}
| Triples
{
#if RASQAL_DEBUG > 1  
  printf("TriplesList 2\n  Triples=");
  if($1)
    rasqal_formula_print($1, stdout);
  else
    fputs("NULL", stdout);
  fputs("\n\n", stdout);
#endif

  if($1) {
    $$=$1->triples;
    $1->triples=NULL;
    rasqal_free_formula($1);
  } else
    $$=NULL;
}
;


/* SPARQL Grammar: rq23 [28] Triples1 */
Triples: Subject PropertyListOpt
{
  int i;

#if RASQAL_DEBUG > 1  
  printf("Triples 1\n subject=");
  rasqal_formula_print($1, stdout);
  if($2) {
    printf("\n propertyList (reverse order to syntax)=");
    rasqal_formula_print($2, stdout);
    printf("\n");
  } else     
    printf("\n and empty propertyList\n");
#endif

  if($2) {
    raptor_sequence *seq=$2->triples;
    rasqal_literal *subject=$1->value;
    
    /* non-empty property list, handle it  */
    for(i=0; i < raptor_sequence_size(seq); i++) {
      rasqal_triple* t2=(rasqal_triple*)raptor_sequence_get_at(seq, i);
      if(t2->subject)
        continue;
      t2->subject=rasqal_new_literal_from_literal(subject);
    }
#if RASQAL_DEBUG > 1  
    printf(" after substitution propertyList=");
    rasqal_formula_print($2, stdout);
    printf("\n\n");
#endif
  }

  if($1) {
    if($2 && $1->triples) {
      raptor_sequence *seq=$2->triples;
      
      raptor_sequence_join($1->triples, seq);
      $2->triples=$1->triples;
      $1->triples=seq;

#if RASQAL_DEBUG > 1  
    printf(" after joining formula=");
    rasqal_formula_print($2, stdout);
    printf("\n\n");
#endif
    }
  }

  if($2) {
    if($1)
      rasqal_free_formula($1);
    $$=$2;
  } else
    $$=$1;
}
;


/* NEW Grammar Term pulled out of rq23 [29] PropertyList */
PropertyListOpt: PropertyList
{
  $$=$1;
}
| /* empty */
{
  $$=NULL;
}
;


/* SPARQL Grammar: rq23 [29] PropertyList
 * SPARQL Grammar: rq23 [30] PropertyListNotEmpty
 */
PropertyList: Predicate ObjectList PropertyListTail
{
  int i;
  
#if RASQAL_DEBUG > 1  
  printf("PropertyList 1\n Predicate=");
  rasqal_formula_print($1, stdout);
  printf("\n ObjectList=");
  rasqal_formula_print($2, stdout);
  printf("\n PropertyListTail=");
  if($3 != NULL)
    rasqal_formula_print($3, stdout);
  else
    fputs("NULL", stdout);
  printf("\n");
#endif
  
  if($2 == NULL) {
#if RASQAL_DEBUG > 1  
    printf(" empty ObjectList not processed\n");
#endif
  } else if($1 && $2) {
    raptor_sequence *seq=$2->triples;
    rasqal_literal *predicate=$1->value;

    /* non-empty property list, handle it  */
    for(i=0; i<raptor_sequence_size(seq); i++) {
      rasqal_triple* t2=(rasqal_triple*)raptor_sequence_get_at(seq, i);
      if(!t2->predicate)
        t2->predicate=(rasqal_literal*)rasqal_new_literal_from_literal(predicate);
    }
  
#if RASQAL_DEBUG > 1  
    printf(" after substitution ObjectList=");
    raptor_sequence_print(seq, stdout);
    printf("\n");
#endif
  }

  if ($1 && $2) {
    raptor_sequence *seq=$2->triples;

    if(!$3) {
      $3=rasqal_new_formula();
      $3->triples=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_triple, (raptor_sequence_print_handler*)rasqal_triple_print);
    }

    for(i=0; i < raptor_sequence_size(seq); i++) {
      rasqal_triple* t2=(rasqal_triple*)raptor_sequence_get_at(seq, i);
      raptor_sequence_push($3->triples, t2);
    }
    while(raptor_sequence_size(seq))
      raptor_sequence_pop(seq);

#if RASQAL_DEBUG > 1  
    printf(" after appending ObjectList (reverse order)=");
    rasqal_formula_print($3, stdout);
    printf("\n\n");
#endif

    rasqal_free_formula($2);
  }

  if($1)
    rasqal_free_formula($1);

  $$=$3;
}
;


/* NEW Grammar Term pulled out of rq23 [30] PropertyListNotEmpty */
PropertyListTail: ';' PropertyListOpt
{
  $$=$2;
}
| /* empty */
{
  $$=NULL;
}
;


/* SPARQL Grammar: rq23 [31] ObjectList */
ObjectList: Object ObjectTail
{
  rasqal_triple *triple;

#if RASQAL_DEBUG > 1  
  printf("ObjectList 1\n");
  printf(" Object=\n");
  rasqal_formula_print($1, stdout);
  printf("\n");
  if($2) {
    printf(" ObjectTail=");
    rasqal_formula_print($2, stdout);
    printf("\n");
  } else
    printf(" and empty ObjectTail\n");
#endif

  if($2)
    $$=$2;
  else
    $$=rasqal_new_formula();
  
  triple=rasqal_new_triple(NULL, NULL, $1->value);
  $1->value=NULL;

  if(!$$->triples)
    $$->triples=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_triple, (raptor_sequence_print_handler*)rasqal_triple_print);

  raptor_sequence_push($$->triples, triple);

  if($1->triples) {
    raptor_sequence *seq=$$->triples;
      
    raptor_sequence_join($1->triples, seq);
    $$->triples=$1->triples;
    $1->triples=seq;
  }

  rasqal_free_formula($1);

#if RASQAL_DEBUG > 1  
  printf(" objectList is now ");
  raptor_sequence_print($$->triples, stdout);
  printf("\n\n");
#endif
}
;


/* NEW Grammar Term pulled out of rq23 [31] ObjectList */
ObjectTail: ',' ObjectList
{
  $$=$2;
}
| /* empty */
{
  $$=NULL;
}
;


/* NEW Grammar Term from Turtle */
Subject: VarOrBlankNodeOrIRIref
{
  $$=rasqal_new_formula();
  $$->value=$1;
}
| TriplesNode
{
  $$=$1;
}
;


/* SPARQL Grammar: rq23 [33] Predicate renamed from Verb to match Turtle */
Predicate: VarOrBnodeOrURI
{
  $$=rasqal_new_formula();
  $$->value=$1;
}
| A
{
  raptor_uri *uri;

#if RASQAL_DEBUG > 1  
  printf("verb Predicate=rdf:type (a)\n");
#endif

  uri=raptor_new_uri_for_rdf_concept("type");
  $$=rasqal_new_formula();
  $$->value=rasqal_new_uri_literal(uri);
}
;


/* SPARQL Grammar: rq23 [33] Object */
Object: VarOrBlankNodeOrIRIref
{
  $$=rasqal_new_formula();
  $$->value=$1;
}
| TriplesNode
{
  $$=$1;
}
;


/* SPARQL Grammar: rq23 [34] TriplesNode 
 * SPARQL Grammar: rq23 [35] BlankNodePropertyList (allowing empty case)
 */
TriplesNode: '[' PropertyList ']'
{
  int i;
  const unsigned char *id=rasqal_query_generate_bnodeid((rasqal_query*)rq, NULL);
  
  if($2 == NULL)
    $$=rasqal_new_formula();
  else {
    $$=$2;
    if($$->value)
      rasqal_free_literal($$->value);
  }
  
  $$->value=rasqal_new_simple_literal(RASQAL_LITERAL_BLANK, id);

  if($2 == NULL) {
#if RASQAL_DEBUG > 1  
    printf("TriplesNode\n PropertyList=");
    rasqal_formula_print($$, stdout);
    printf("\n");
#endif
  } else {
    raptor_sequence *seq=$2->triples;

    /* non-empty property list, handle it  */
#if RASQAL_DEBUG > 1  
    printf("TriplesNode\n PropertyList=");
    raptor_sequence_print(seq, stdout);
    printf("\n");
#endif

    for(i=0; i<raptor_sequence_size(seq); i++) {
      rasqal_triple* t2=(rasqal_triple*)raptor_sequence_get_at(seq, i);
      if(t2->subject)
        continue;
      
      t2->subject=(rasqal_literal*)rasqal_new_literal_from_literal($$->value);
    }

#if RASQAL_DEBUG > 1
    printf(" after substitution formula=");
    rasqal_formula_print($$, stdout);
    printf("\n\n");
#endif
  }
  
}
| Collection
{
  $$=$1;
}
;


/* NEW Grammar Term pulled out of rq23 [36] Collection, based on Turtle */
/* Sequence of formula */
ItemList: ItemList Object
{
#if RASQAL_DEBUG > 1  
  printf("ItemList 1\n");
  if($2) {
    printf(" Object=");
    rasqal_formula_print($2, stdout);
    printf("\n");
  } else  
    printf(" and empty Object\n");
  if($1) {
    printf(" ItemList=");
    raptor_sequence_print($1, stdout);
    printf("\n");
  } else
    printf(" and empty ItemList\n");
#endif

  if(!$2)
    $$=NULL;
  else {
    raptor_sequence_push($$, $2);
#if RASQAL_DEBUG > 1  
    printf(" itemList is now ");
    raptor_sequence_print($$, stdout);
    printf("\n\n");
#endif
  }

}
| Object
{
#if RASQAL_DEBUG > 1  
  printf("ItemList 2\n");
  if($1) {
    printf(" Object=");
    rasqal_formula_print($1, stdout);
    printf("\n");
  } else  
    printf(" and empty Object\n");
#endif

  $$=NULL;
  if($1)
    $$=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_formula, (raptor_sequence_print_handler*)rasqal_formula_print);

  raptor_sequence_push($$, $1);
#if RASQAL_DEBUG > 1  
  printf(" ItemList is now ");
  raptor_sequence_print($$, stdout);
  printf("\n\n");
#endif
}
;


/* SPARQL Grammar: rq23 [36] Collection (allowing empty case) */
Collection: '(' ItemList ')'
{
  int i;
  rasqal_query* rdf_query=(rasqal_query*)rq;
  rasqal_literal* first_identifier;
  rasqal_literal* rest_identifier;
  rasqal_literal* object;

#if RASQAL_DEBUG > 1  
  printf("Collection\n ItemList=");
  raptor_sequence_print($2, stdout);
  printf("\n");
#endif

  first_identifier=rasqal_new_uri_literal(raptor_uri_copy(rasqal_rdf_first_uri));
  rest_identifier=rasqal_new_uri_literal(raptor_uri_copy(rasqal_rdf_rest_uri));
  
  object=rasqal_new_uri_literal(raptor_uri_copy(rasqal_rdf_nil_uri));

  $$=rasqal_new_formula();
  $$->triples=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_triple, (raptor_sequence_print_handler*)rasqal_triple_print);

  for(i=raptor_sequence_size($2)-1; i>=0; i--) {
    rasqal_formula* f=(rasqal_formula*)raptor_sequence_get_at($2, i);
    const unsigned char *blank_id=rasqal_query_generate_bnodeid(rdf_query, NULL);
    rasqal_literal* blank=rasqal_new_simple_literal(RASQAL_LITERAL_BLANK, blank_id);
    rasqal_triple *t2;

    /* Move existing formula triples */
    if(f->triples)
      raptor_sequence_join($$->triples, f->triples);

    /* add new triples we needed */
    t2=rasqal_new_triple(rasqal_new_literal_from_literal(blank),
                         rasqal_new_literal_from_literal(first_identifier),
                         rasqal_new_literal_from_literal(f->value));
    raptor_sequence_push($$->triples, t2);

    t2=rasqal_new_triple(rasqal_new_literal_from_literal(blank),
                         rasqal_new_literal_from_literal(rest_identifier),
                         rasqal_new_literal_from_literal(object));
    raptor_sequence_push($$->triples, t2);

    rasqal_free_literal(object);

    object=blank;
  }

  $$->value=object;
  
#if RASQAL_DEBUG > 1
  printf(" after substitution collection=");
  rasqal_formula_print($$, stdout);
  printf("\n\n");
#endif

  rasqal_free_literal(first_identifier);
  rasqal_free_literal(rest_identifier);
}
|  '(' ')'
{
#if RASQAL_DEBUG > 1  
  printf("Collection\n empty\n");
#endif

  $$=rasqal_new_formula();
  $$->value=rasqal_new_uri_literal(raptor_uri_copy(rasqal_rdf_nil_uri));
}



/* NEW Grammar Term */
VarOrIRIrefList : VarOrIRIrefList Var
{
  $$=$1;
  raptor_sequence_push($$, rasqal_new_variable_literal($2));
}
| VarOrIRIrefList ',' Var
{
  $$=$1;
  raptor_sequence_push($$, rasqal_new_variable_literal($3));
}
| VarOrIRIrefList IRIref
{
  $$=$1;
  raptor_sequence_push($$, $2);
}
| VarOrIRIrefList ',' IRIref
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
| IRIref
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


/* SPARQL Grammar: rq23 [37] GraphNode - junk, mostly replaced by Object */

/* SPARQL Grammar: rq23 [38] VarOrTerm - junk, mostly replaced by Object */

/* SPARQL Grammar: rq23 [41] VarOrIRIref */
VarOrIRIref : Var
{
  $$=rasqal_new_variable_literal($1);
}
| IRIref
{
  $$=$1;
}
;


/* NEW Grammar Term: VarOrLiteral */
VarOrLiteral : Var
{
  $$=rasqal_new_variable_literal($1);
}
| RDFTerm
{
  $$=$1;
}
;

/* SPARQL Grammar Term: rq23 [40] VarOrBlankNodeOrIRIref */
VarOrBlankNodeOrIRIref : VarOrLiteral
{
  $$=$1;
}
| BlankNode
{
  $$=$1;
}
;


/* NEW Grammar Term VarOrBNodeOrURI */
VarOrBnodeOrURI: VarOrIRIref
{
  $$=$1;
}
| BlankNode
{
  $$=$1;
}
;

/* SPARQL Grammar: rq23 [41] Var */
Var : '?' IDENTIFIER
{
  $$=rasqal_new_variable((rasqal_query*)rq, $2, NULL);
}
| '$' IDENTIFIER
{
  $$=rasqal_new_variable((rasqal_query*)rq, $2, NULL);
}
;


/* SPARQL Grammar: rq23 [42] GraphTerm - junk (empty collection is in Collection) */

/* SPARQL Grammar: rq23 [43] Expression */
Expression : ConditionalAndExpression SC_OR Expression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_OR, $1, $3);
}
| ConditionalAndExpression
{
  $$=$1;
}
;

/* SPARQL Grammar: rq23 [44] ConditionalOrExpression - merged into Expression */

/* SPARQL Grammar: rq23 [45] ConditionalAndExpression */
ConditionalAndExpression: RelationalExpression SC_AND RelationalExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_AND, $1, $3);
;
}
| RelationalExpression
{
  $$=$1;
}
;

/* SPARQL Grammar: rq23 [46] ValueLogical - merged into ConditionalAndExpression */

/* SPARQL Grammar: rq23 [47] RelationalExpression */
RelationalExpression : AdditiveExpression EQ AdditiveExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_EQ, $1, $3);
}
| AdditiveExpression NEQ AdditiveExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_NEQ, $1, $3);
}
| AdditiveExpression LT AdditiveExpression
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

/* SPARQL Grammar: rq23 [48] NumericExpression - merged into AdditiveExpression */

/* SPARQL Grammar: rq23 [49] AdditiveExpression */
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

/* SPARQL Grammar: rq23 [50] MultiplicativeExpression */
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

/* SPARQL Grammar: rq23 [51] UnaryExpression */
UnaryExpression : '!' PrimaryExpression
{
  $$=rasqal_new_1op_expression(RASQAL_EXPR_BANG, $2);
}
| '+' PrimaryExpression
{
  $$=$2;
}
| '-' PrimaryExpression
{
  $$=rasqal_new_1op_expression(RASQAL_EXPR_UMINUS, $2);
}
| PrimaryExpression
{
  $$=$1;
}
;

/* SPARQL Grammar: rq23 [52] CallExpression */
CallExpression: STR '(' VarOrLiteral ')'
{
  rasqal_expression *e=rasqal_new_literal_expression($3);
  $$=rasqal_new_1op_expression(RASQAL_EXPR_STR, e);
}
| LANG '(' VarOrLiteral ')'
{
  rasqal_expression *e=rasqal_new_literal_expression($3);
  $$=rasqal_new_1op_expression(RASQAL_EXPR_LANG, e);
}
| DATATYPE '(' VarOrLiteral ')'
{
  rasqal_expression *e=rasqal_new_literal_expression($3);
  $$=rasqal_new_1op_expression(RASQAL_EXPR_DATATYPE, e);
}
| BOUND '(' Var ')'
{
  rasqal_literal *l=rasqal_new_variable_literal($3);
  rasqal_expression *e=rasqal_new_literal_expression(l);
  $$=rasqal_new_1op_expression(RASQAL_EXPR_BOUND, e);
}
| ISURI '(' VarOrLiteral ')'
{
  rasqal_expression *e=rasqal_new_literal_expression($3);
  $$=rasqal_new_1op_expression(RASQAL_EXPR_ISURI, e);
}
| ISBLANK '(' VarOrLiteral ')'
{
  rasqal_expression *e=rasqal_new_literal_expression($3);
  $$=rasqal_new_1op_expression(RASQAL_EXPR_ISBLANK, e);
}
| ISLITERAL '(' VarOrLiteral ')'
{
  rasqal_expression *e=rasqal_new_literal_expression($3);
  $$=rasqal_new_1op_expression(RASQAL_EXPR_ISLITERAL, e);
}
| RegexExpression
{
  $$=$1;
}
| FunctionCall
{
  $$=$1;
}
;


/* SPARQL Grammar: rq23 [53] RegexExpression */
RegexExpression : REGEX '(' Expression ',' RDFTerm ')'
{
  /* FIXME - RDFTerm should be Expression */
  /* FIXME - regex needs more thought */
  const unsigned char* pattern=rasqal_literal_as_string($5);
  size_t len=strlen((const char*)pattern);
  unsigned char* npattern;
  rasqal_literal* l;

  npattern=(unsigned char *)RASQAL_MALLOC(cstring, len+1);
  strncpy((char*)npattern, (const char*)pattern, len+1);
  l=rasqal_new_pattern_literal(npattern, NULL);
  $$=rasqal_new_string_op_expression(RASQAL_EXPR_STR_MATCH, $3, l);
  rasqal_free_literal($5);
}
| REGEX '(' Expression ',' RDFTerm ',' RDFTerm ')'
{
  /* FIXME - RDFTerm-s should be Expression-s */
  /* FIXME - regex needs more thought */
  const unsigned char* pattern=rasqal_literal_as_string($5);
  size_t p_len=strlen((const char*)pattern);
  unsigned char* npattern;
  const char* flags=(const char*)rasqal_literal_as_string($7);
  size_t f_len=strlen(flags);
  char* nflags;
  rasqal_literal* l;

  npattern=(unsigned char *)RASQAL_MALLOC(cstring, p_len+1);
  strncpy((char*)npattern, (const char*)pattern, p_len+1);
  nflags=(char *)RASQAL_MALLOC(cstring, f_len+1);
  strncpy(nflags, flags, f_len+1);
  l=rasqal_new_pattern_literal(npattern, nflags);

  $$=rasqal_new_string_op_expression(RASQAL_EXPR_STR_MATCH, $3, l);
  rasqal_free_literal($5);
  rasqal_free_literal($7);
}
;


/* SPARQL Grammar: rq23 [54] FunctionCall */
FunctionCall: IRIrefBrace ArgList ')'
{
  raptor_uri* uri=rasqal_literal_as_uri($1);
  
  uri=raptor_uri_copy(uri);
  if(raptor_sequence_size($2) == 1 &&
     sparql_is_builtin_xsd_datatype(uri)) {
    rasqal_expression* e=(rasqal_expression*)raptor_sequence_pop($2);
    $$=rasqal_new_cast_expression(uri, e);
    raptor_free_sequence($2);
  } else
    $$=rasqal_new_function_expression(uri, $2);
  rasqal_free_literal($1);
}


/* SPARQL Grammar: rq23 [55] ArgList */
ArgList : ArgList ',' Expression
{
  $$=$1;
  if($3)
    raptor_sequence_push($$, $3);
}
| Expression
{
  $$=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_expression, (raptor_sequence_print_handler*)rasqal_expression_print);
  if($1)
    raptor_sequence_push($$, $1);
}
| /* empty */
{
  $$=NULL;
}
;

/* SPARQL Grammar: rq23 [56] BrackettedExpression */
BrackettedExpression: '(' Expression ')'
{
  $$=$2;
}
;

/* SPARQL Grammar: rq23 [57] PrimaryExpression */
PrimaryExpression: BrackettedExpression 
{
  $$=$1;
}
| CallExpression
{
  $$=$1;
}
| Var
{
  rasqal_literal *l=rasqal_new_variable_literal($1);
  $$=rasqal_new_literal_expression(l);
}
| RDFTerm
{
  $$=rasqal_new_literal_expression($1);
}
;

/* SPARQL Grammar: rq23 [58] RDFTerm */
RDFTerm : IRIref
{
  $$=$1;
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
}
;

/* SPARQL Grammar: rq23 [59] NumericLiteral - merged into RDFTerm */

/* SPARQL Grammar: rq23 [60] RDFLiteral - merged into RDFTerm */

/* SPARQL Grammar: rq23 [61] BooleanLiteral - merged into RDFTerm */

/* SPARQL Grammar: rq23 [62] String - merged into RDFTerm */

/* SPARQL Grammar: rq23 [63] IRIref */
IRIref : URI_LITERAL
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

/* NEW Grammar Term made from SPARQL Grammer: rq23 [63] IRIref + '(' expanded */
IRIrefBrace : URI_LITERAL_BRACE
{
  $$=rasqal_new_uri_literal($1);
}
| QNAME_LITERAL_BRACE
{
  $$=rasqal_new_simple_literal(RASQAL_LITERAL_QNAME, $1);
  if(rasqal_literal_expand_qname((rasqal_query*)rq, $$)) {
    sparql_query_error_full((rasqal_query*)rq,
                            "QName %s cannot be expanded", $1);
    rasqal_free_literal($$);
    $$=NULL;
  }
}

/* SPARQL Grammar: rq23 [64] QName - made into terminal QNAME_LITERAL */

/* SPARQL Grammar: rq23 [65] BlankNode */
BlankNode : BLANK_LITERAL
{
  $$=rasqal_new_simple_literal(RASQAL_LITERAL_BLANK, $1);
} | '[' ']'
{
  const unsigned char *id=rasqal_query_generate_bnodeid((rasqal_query*)rq, NULL);
  $$=rasqal_new_simple_literal(RASQAL_LITERAL_BLANK, id);
}
;

/* SPARQL Grammar: rq23 [66] QuotedIRIref onwards are all lexer items
 * with similar names or are inlined.
 */




%%


/* Support functions */


/* This is declared in sparql_lexer.h but never used, so we always get
 * a warning unless this dummy code is here.  Used once below in an error case.
 */
static int yy_init_globals (yyscan_t yyscanner ) { return 0; };


static int
sparql_is_builtin_xsd_datatype(raptor_uri* uri) 
{
  return (raptor_uri_equals(uri, rasqal_xsd_boolean_uri) ||
          raptor_uri_equals(uri, rasqal_xsd_string_uri) ||
          raptor_uri_equals(uri, rasqal_xsd_double_uri) ||
          raptor_uri_equals(uri, rasqal_xsd_float_uri) ||
          raptor_uri_equals(uri, rasqal_xsd_decimal_uri) ||
          raptor_uri_equals(uri, rasqal_xsd_integer_uri) ||
          raptor_uri_equals(uri, rasqal_xsd_datetime_uri)
          );
}


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

  rdf_query->compare_flags = RASQAL_COMPARE_XQUERY;
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

  /* FIXME - should check remaining query parts  */
  if(rasqal_engine_sequence_has_qname(rdf_query->triples) ||
     rasqal_engine_sequence_has_qname(rdf_query->constructs) ||
     rasqal_engine_query_constraints_has_qname(rdf_query)) {
    sparql_query_error(rdf_query, "SPARQL query has unexpanded QNames");
    return 1;
  }

  return rasqal_engine_prepare(rdf_query);
}


static int
rasqal_sparql_query_engine_execute(rasqal_query* rdf_query,
                                   rasqal_query_results *results) 
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
  raptor_uri* base_uri=NULL;
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
  rc=fread(query_string, SPARQL_FILE_BUF_SIZE, 1, fh);
  if(rc < SPARQL_FILE_BUF_SIZE) {
    if(ferror(fh)) {
      fprintf(stderr, "%s: file '%s' read failed - %s\n",
              program, filename, strerror(errno));
      fclose(fh);
      return(1);
    }
  }
  
  if(argc>1)
    fclose(fh);

  rasqal_init();

  query=rasqal_new_query("sparql", NULL);

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

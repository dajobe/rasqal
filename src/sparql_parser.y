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
#define YY_NO_UNISTD_H 1
#include <sparql_lexer.h>

#include <sparql_common.h>

#undef RASQAL_DEBUG
#define RASQAL_DEBUG 2

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
 */
%expect 0

/* word symbols */
%token SELECT FROM WHERE
%token OPTIONAL PREFIX DESCRIBE CONSTRUCT ASK DISTINCT LIMIT UNION
%token BASE BOUND STR LANG DATATYPE ISURI ISBLANK ISLITERAL
%token GRAPH NAMED FILTER OFFSET A ORDER BY REGEX ASC DESC

/* expression delimitors */

%token ',' '(' ')' '[' ']' '{' '}'
%token '?' '$'

/* function call indicator */
%token '&'

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
%token <uri> URI_LITERAL URI_LITERAL_BRACE
%token <name> QNAME_LITERAL BLANK_LITERAL QNAME_LITERAL_BRACE

%token <name> IDENTIFIER


%type <seq> SelectClause ConstructClause DescribeClause
%type <seq> VarList VarOrURIList ArgList TriplesList
%type <seq> ConstructTemplate OrderConditionList

%type <formula> Triples
%type <formula> PropertyList ObjectList ItemList Collection
%type <formula> Subject Predicate Object TriplesNode

%type <graph_pattern> GraphPattern PatternElement
%type <graph_pattern> GraphGraphPattern OptionalGraphPattern
%type <graph_pattern> UnionGraphPattern
%type <graph_pattern> PatternElementsList

%type <expr> Expression ConditionalAndExpression ValueLogical
%type <expr> EqualityExpression RelationalExpression AdditiveExpression
%type <expr> MultiplicativeExpression UnaryExpression
%type <expr> PrimaryExpression BuiltinExpression FunctionCall
%type <expr> OrderCondition OrderExpression

%type <literal> Literal URI BNode
%type <literal> VarOrLiteral VarOrURI
%type <literal> VarOrLiteralOrBNode
%type <literal> URIBrace

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
}
|  ConstructClause
{
  ((rasqal_query*)rq)->constructs=$1;
}
|  DescribeClause
{
  ((rasqal_query*)rq)->select_is_describe=1;
  ((rasqal_query*)rq)->describes=$1;
}
| AskClause
{
  ((rasqal_query*)rq)->ask=1;
}
;

/* SPARQL Grammar: rq23 [2] Prolog - merged into Query */


/* SPARQL Grammar: rq23 [3] BaseDecl */
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


/* SPARQL Grammar: rq23 [4] PrefixDecl */
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


/* SPARQL Grammar: rq23 [5] SelectClause */
SelectClause : SELECT DISTINCT VarList
{
  $$=$3;
  ((rasqal_query*)rq)->distinct=1;
}
| SELECT DISTINCT '*'
{
  $$=NULL;
  ((rasqal_query*)rq)->select_all=1;
  ((rasqal_query*)rq)->distinct=1;
}
| SELECT VarList
{
  $$=$2;
}
| SELECT '*'
{
  $$=NULL;
  ((rasqal_query*)rq)->select_all=1;
}
;


/* SPARQL Grammar: rq23 [6] DescribeClause */
DescribeClause : DESCRIBE VarOrURIList
{
  $$=$2;
}
| DESCRIBE '*'
{
  $$=NULL;
}
;


/* SPARQL Grammar: rq23 [7] ConstructClause */
ConstructClause : CONSTRUCT ConstructTemplate
{
  $$=$2;
}
| CONSTRUCT '*'
{
  $$=NULL;
  ((rasqal_query*)rq)->construct_all=1;
}
;


/* SPARQL Grammar: rq23 [8] AskClause */
AskClause : ASK 
{
  /* FIXME - do ask */
}


/* SPARQL Grammar: rq23 [9] DatasetClause */
DatasetClauseOpt : DefaultGraphClause
| DefaultGraphClause NamedGraphClauseList
| /* empty */
;


/* SPARQL Grammar: rq23 [10] DefaultGraphClause */
DefaultGraphClause : FROM URI
{
  if($2) {
    raptor_uri* uri=rasqal_literal_as_uri($2);
    rasqal_query_add_data_graph((rasqal_query*)rq, uri, uri, RASQAL_DATA_GRAPH_BACKGROUND);
  }
}
;  


/* NEW Grammar Term pulled out of [9] DatasetClause */
NamedGraphClauseList: NamedGraphClauseList NamedGraphClause
| NamedGraphClause
;

/* SPARQL Grammar: rq23 [11] NamedGraphClause */
NamedGraphClause: FROM NAMED URI
{
  if($3) {
    raptor_uri* uri=rasqal_literal_as_uri($3);
    rasqal_query_add_data_graph((rasqal_query*)rq, uri, uri, RASQAL_DATA_GRAPH_NAMED);
  }
}
;


/* SPARQL Grammar: rq23 [12] SourceSelector - junk */


/* SPARQL Grammar: rq23 [13] WhereClause - remained for clarity */
WhereClauseOpt:  WHERE GraphPattern
{
  ((rasqal_query*)rq)->query_graph_pattern=$2;
}
| GraphPattern
{
  sparql_syntax_warning(((rasqal_query*)rq), "WHERE omitted");
  ((rasqal_query*)rq)->query_graph_pattern=$1;
}
| /* empty */
;


/* SPARQL Grammar: rq23 [14] OrderClause - remained for clarity */
OrderClauseOpt:  ORDER BY OrderConditionList
{
  ((rasqal_query*)rq)->order_conditions_sequence=$3;
}
| /* empty */
;


/* NEW Grammar Term pulled out of [14] OrderClauseOpt */
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


/* SPARQL Grammar: rq23 [15] OrderCondition */
OrderCondition: ASC OrderExpression ']'
{
  $$=rasqal_new_1op_expression(RASQAL_EXPR_ORDER_COND_ASC, $2);
}
| DESC OrderExpression ']'
{
  $$=rasqal_new_1op_expression(RASQAL_EXPR_ORDER_COND_DESC, $2);
}
| OrderExpression
{
  $$=rasqal_new_1op_expression(RASQAL_EXPR_ORDER_COND_NONE, $1);
}
;

/* SPARQL Grammar: rq23 [16] OrderExpression - ignored */
OrderExpression: FunctionCall 
{
  $$=$1;
}
| Var
{
  rasqal_literal* l=rasqal_new_variable_literal($1);
  $$=rasqal_new_literal_expression(l);
}
;

/* SPARQL Grammar: rq23 [17] LimitClause - remained for clarity */
LimitClauseOpt :  LIMIT INTEGER_LITERAL
{
  if($2 != NULL)
    ((rasqal_query*)rq)->limit=$2->value.integer;
}
| /* empty */
;

/* SPARQL Grammar: rq23 [18] OffsetClause - remained for clarity */
OffsetClauseOpt :  OFFSET INTEGER_LITERAL
{
  if($2 != NULL)
    ((rasqal_query*)rq)->offset=$2->value.integer;
}
| /* empty */
;

/* SPARQL Grammar: rq23 [19] GraphPattern */
GraphPattern : '{' PatternElementsList DotOptional '}'
{
#if RASQAL_DEBUG > 1  
  printf("GraphPattern 1\n  GraphPattern=");
  rasqal_graph_pattern_print($2, stdout);
  fputs("\n\n", stdout);
#endif

  $$=$2;
}
| '{' DotOptional '}' /* empty list */
{
  $$=NULL;
}
;

/* NEW Grammar Term pulled out of rq23 [21] PatternElementsListTail */
DotOptional: '.'
| /* empty */
;


/* SPARQL Grammar Term: rq23 [20] PatternElementsList and rq23[21] PatternElementsListTail */
PatternElementsList: PatternElementsList '.' PatternElement
{
  rasqal_graph_pattern *mygp=$1;
  
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

  $$=mygp;
  if($3)
    raptor_sequence_push(mygp->graph_patterns, $3);
}
| PatternElementsList '.' FILTER Expression
{
  $$=$1;
  rasqal_graph_pattern_add_constraint($$, $4);
}
| PatternElement
{
  raptor_sequence *seq;

#if RASQAL_DEBUG > 1  
  printf("PatternElementsList 2\n  PatternElement=");
  if($1)
    rasqal_graph_pattern_print($1, stdout);
  else
    fputs("NULL", stdout);
  fputs("\n\n", stdout);
#endif

  seq=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_graph_pattern, (raptor_sequence_print_handler*)rasqal_graph_pattern_print);
  if($1)
    raptor_sequence_push(seq, $1);

  $$=rasqal_new_graph_pattern_from_sequence((rasqal_query*)rq, seq, 0);
}
;


/* SPARQL Grammar: rq23 [22] PatternElement */
PatternElement : Triples
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
    int triple_pattern_size=raptor_sequence_size(t);
    
    raptor_sequence_join(s, t);

    rasqal_free_formula($1);

    $$=rasqal_new_graph_pattern_from_triples((rasqal_query*)rq, s, offset, offset+triple_pattern_size-1, 0);
  } else
    $$=NULL;
}
| OptionalGraphPattern
{
  $$=$1;
}
| UnionGraphPattern
{
  $$=$1;
}
| GraphPattern
{
  $$=$1;
}
| GraphGraphPattern
{
  $$=$1;
}
;


/* SPARQL Grammar: rq23 [23] OptionalGraphPattern */
OptionalGraphPattern: OPTIONAL GraphPattern
{
  int i;
  raptor_sequence *s=$2->graph_patterns;

#if RASQAL_DEBUG > 1  
  printf("PatternElementForms 4\n  graphpattern=");
  rasqal_graph_pattern_print($2, stdout);
  fputs("\n\n", stdout);
#endif

  if(s) {
    /* Flag all the triples in GraphPattern1 as optional */
    for(i=0; i < raptor_sequence_size(s); i++) {
      rasqal_triple *t=(rasqal_triple*)raptor_sequence_get_at(s, i);
      t->flags |= RASQAL_TRIPLE_FLAGS_OPTIONAL;
    }
  }
  $$=$2;
  $$->flags |= RASQAL_PATTERN_FLAGS_OPTIONAL;
}
;


/* SPARQL Grammar: rq23 [24] GraphGraphPattern */
GraphGraphPattern: GRAPH VarOrURI GraphPattern
{
#if RASQAL_DEBUG > 1  
  printf("GraphGraphPattern 2\n  varoruri=");
  rasqal_literal_print($2, stdout);
  printf(", graphpattern=");
  rasqal_graph_pattern_print($3, stdout);
  fputs("\n\n", stdout);
#endif

  rasqal_graph_pattern_set_origin($3, $2);

  rasqal_free_literal($2);
  $$=$3;
}
;


/* SPARQL Grammar: rq23 [25] UnionGraphPattern */
UnionGraphPattern : GraphPattern UNION GraphPattern 
{
  /* FIXME - union graph pattern type */
  sparql_syntax_warning(((rasqal_query*)rq), "SPARQL UNION ignored");
  $$=$1;
}
;


/* SPARQL Grammar: rq23 [26] Constraint - inlined into PatternElement */


/* SPARQL Grammar: rq23 [27] ConstructTemplate */
ConstructTemplate:  '{' TriplesList DotOptional '}'
{
  $$=$2;
}
;


TriplesList: TriplesList '.' Triples
{
  if($3) {
    raptor_sequence *t=$3->triples;

    raptor_sequence_join($1, t);

    $3->triples=NULL;
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



/* SPARQL Grammar: rq23 [28] Triples */
Triples: Subject PropertyList
{
  int i;

#if RASQAL_DEBUG > 1  
  printf("triples 1\n subject=");
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

  if($1)
    rasqal_free_formula($1);
  $$=$2;
}
;


/* SPARQL Grammar: rq23 [29] PropertyList, rq23 [30] ProeprtyListNotEmpty, rq23 [31] PropertyListTail */
PropertyList: PropertyList ';' Predicate ObjectList
{
  int i;
  
#if RASQAL_DEBUG > 1  
  printf("PropertyList 1\n Predicate=");
  rasqal_formula_print($3, stdout);
  printf("\n ObjectList=");
  rasqal_formula_print($4, stdout);
  printf("\n PropertyList=");
  rasqal_formula_print($1, stdout);
  printf("\n\n");
#endif
  
  if($4 == NULL) {
#if RASQAL_DEBUG > 1  
    printf(" empty ObjectList not processed\n");
#endif
  } else if($3 && $4) {
    raptor_sequence *seq=$4->triples;
    rasqal_literal *predicate=$3->value;

    /* non-empty property list, handle it  */
    for(i=0; i<raptor_sequence_size(seq); i++) {
      rasqal_triple* t2=(rasqal_triple*)raptor_sequence_get_at(seq, i);
      t2->predicate=(rasqal_literal*)rasqal_new_literal_from_literal(predicate);
    }
  
#if RASQAL_DEBUG > 1  
    printf(" after substitution ObjectList=");
    raptor_sequence_print(seq, stdout);
    printf("\n");
#endif
  }

  if($1 == NULL) {
#if RASQAL_DEBUG > 1  
    printf(" empty PropertyList not copied\n\n");
#endif
  } else if ($3 && $4 && $1) {
    raptor_sequence *seq=$4->triples;

    for(i=0; i < raptor_sequence_size(seq); i++) {
      rasqal_triple* t2=(rasqal_triple*)raptor_sequence_get_at(seq, i);
      raptor_sequence_push($1->triples, t2);
    }
    while(raptor_sequence_size(seq))
      raptor_sequence_pop(seq);

#if RASQAL_DEBUG > 1  
    printf(" after appending ObjectList (reverse order)=");
    rasqal_formula_print($1, stdout);
    printf("\n\n");
#endif

    rasqal_free_formula($4);
  }

  if($3)
    rasqal_free_formula($3);

  $$=$1;
}
| Predicate ObjectList
{
  int i;
#if RASQAL_DEBUG > 1  
  printf("PropertyList 2\n Predicate=");
  rasqal_formula_print($1, stdout);
  if($2) {
    printf("\n ObjectList=");
    rasqal_formula_print($2, stdout);
    printf("\n");
  } else
    printf("\n and empty ObjectList\n");
#endif

  if($1 && $2) {
    raptor_sequence *seq=$2->triples;
    rasqal_literal *predicate=$1->value;
    
    for(i=0; i<raptor_sequence_size(seq); i++) {
      rasqal_triple* t2=(rasqal_triple*)raptor_sequence_get_at(seq, i);
      if(t2->predicate)
        continue;
      t2->predicate=(rasqal_literal*)rasqal_new_literal_from_literal(predicate);
    }

#if RASQAL_DEBUG > 1  
    printf(" after substitution ObjectList=");
    raptor_sequence_print(seq, stdout);
    printf("\n\n");
#endif
  }

  if($1)
    rasqal_free_formula($1);

  $$=$2;
}
|
{
#if RASQAL_DEBUG > 1  
  printf("Propertylist 4\n empty returning NULL\n\n");
#endif
  $$=NULL;
}
| PropertyList ';'
{
  $$=$1;
#if RASQAL_DEBUG > 1  
  printf("PropertyList 5\n trailing semicolon returning existing list ");
  rasqal_formula_print($$, stdout);
  printf("\n\n");
#endif
}
;


/* SPARQL Grammar: rq23 [32] ObjectList and rq23 [33] ObjectTail */
ObjectList: ObjectList ',' Object
{
  rasqal_triple *triple;

#if RASQAL_DEBUG > 1  
  printf("ObjectList 1\n");
  if($3) {
    printf(" object=\n");
    rasqal_formula_print($3, stdout);
    printf("\n");
  } else  
    printf(" and empty object\n");
  if($1) {
    printf(" ObjectList=");
    rasqal_formula_print($1, stdout);
    printf("\n");
  } else
    printf(" and empty ObjectList\n");
#endif

  if(!$3)
    $$=NULL;
  else {
    triple=rasqal_new_triple(NULL, NULL, $3->value);
    $3->value=NULL;
    
    $$=$1;
    raptor_sequence_push($$->triples, triple);
#if RASQAL_DEBUG > 1  
    printf(" objectList is now ");
    raptor_sequence_print($$->triples, stdout);
    printf("\n\n");
#endif
  }
}
| Object
{
  rasqal_triple *triple;
  
#if RASQAL_DEBUG > 1  
  printf("ObjectList 2\n");
  if($1) {
    printf(" Object=");
    rasqal_formula_print($1, stdout);
    printf("\n");
  } else  
    printf(" and empty Object\n");
#endif

  $$=$1;

  if($$) {
    rasqal_literal* object=rasqal_new_literal_from_literal($1->value);
    
    triple=rasqal_new_triple(NULL, NULL, object);

    if(!$$->triples) {
#ifdef RASQAL_DEBUG
      $$->triples=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_triple,
                                      (raptor_sequence_print_handler*)rasqal_triple_print);
#else
      $$->triples=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_triple, NULL);
#endif
    }
    raptor_sequence_push($$->triples, triple);

#if RASQAL_DEBUG > 1  
    printf(" objectList is now ");
    rasqal_formula_print($$, stdout);
    printf("\n\n");
#endif
  }
}
;


/* NEW Grammar Term from Turtle */
Subject: VarOrLiteralOrBNode
{
  $$=rasqal_new_formula();
  $$->value=$1;
}
| TriplesNode
{
  $$=$1;
}
;


/* NEW Grammar Term from Turtle */
Predicate: VarOrURI
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


/* NEW Grammar Term from Turtle */
Object: VarOrLiteralOrBNode
{
  $$=rasqal_new_formula();
  $$->value=$1;
}
| TriplesNode
{
  $$=$1;
}
;


/* SPARQL Grammar: rq23 [36] TriplesNode */
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
    printf(" after substitution ObjectList=");
    raptor_sequence_print(seq, stdout);
    printf("\n\n");
#endif
  }
  
}
| Collection
{
  $$=$1;
}
;


/* NEW Grammar Term pulled out of rq23 [38] Collection, based on Turtle */
ItemList: ItemList Object
{
  rasqal_triple *triple;

#if RASQAL_DEBUG > 1  
  printf("Objectlist 1\n");
  if($2) {
    printf(" Object=");
    rasqal_formula_print($2, stdout);
    printf("\n");
  } else  
    printf(" and empty Object\n");
  if($1) {
    printf(" ItemList=");
    rasqal_formula_print($1, stdout);
    printf("\n");
  } else
    printf(" and empty ObjectList\n");
#endif

  if(!$2)
    $$=NULL;
  else {
    triple=rasqal_new_triple(NULL, NULL, $2->value);
    $2->value=NULL;
    
    $$=$2;

    if(!$$->triples) {
#ifdef RASQAL_DEBUG
      $$->triples=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_triple,
                                      (raptor_sequence_print_handler*)rasqal_triple_print);
#else
      $$->triples=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_triple, NULL);
#endif
    }
    raptor_sequence_push($$->triples, triple);
#if RASQAL_DEBUG > 1  
    printf(" objectList is now ");
    raptor_sequence_print($$->triples, stdout);
    printf("\n\n");
#endif
  }
}
| Object
{
  rasqal_triple *triple;
  rasqal_formula *formula;

#if RASQAL_DEBUG > 1  
  printf("ObjectList 2\n");
  if($1) {
    printf(" Object=");
    rasqal_formula_print($1, stdout);
    printf("\n");
  } else  
    printf(" and empty Object\n");
#endif

  if(!$1)
    $$=NULL;
  else {
    formula=rasqal_new_formula();
    triple=rasqal_new_triple(NULL, NULL, $1->value);
    $1->value=NULL;
    
#ifdef RASQAL_DEBUG

    formula->triples=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_triple,
                           (raptor_sequence_print_handler*)rasqal_triple_print);
#else
    formula->triples=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_triple, NULL);
#endif
    raptor_sequence_push(formula->triples, triple);
#if RASQAL_DEBUG > 1  
    printf(" ObjectList is now ");
    raptor_sequence_print(formula->triples, stdout);
    printf("\n\n");
#endif
    formula->value=NULL;

    $$=formula;
  }
}
;


/* SPARQL Grammar: rq23 [38] Collection */
Collection: '(' ItemList ')'
{
  int i;
  rasqal_query* rdf_query=(rasqal_query*)rq;
  rasqal_literal* first_identifier;
  rasqal_literal* rest_identifier;
  rasqal_literal* object;
  raptor_sequence *seq=$2->triples;

  $$=$2;
  
#if RASQAL_DEBUG > 1  
  printf("Collection\n ItemList=");
  raptor_sequence_print(seq, stdout);
  printf("\n");
#endif

  first_identifier=rasqal_new_uri_literal(raptor_uri_copy(rasqal_rdf_first_uri));
  rest_identifier=rasqal_new_uri_literal(raptor_uri_copy(rasqal_rdf_rest_uri));
  
  /* non-empty property list, handle it  */
#if RASQAL_DEBUG > 1  
  printf("resource\n propertyList=");
  raptor_sequence_print(seq, stdout);
  printf("\n");
#endif

  object=rasqal_new_uri_literal(raptor_uri_copy(rasqal_rdf_nil_uri));

  for(i=raptor_sequence_size(seq)-1; i>=0; i--) {
    rasqal_triple* t2=(rasqal_triple*)raptor_sequence_get_at(seq, i);
    const unsigned char *blank_id=rasqal_query_generate_bnodeid(rdf_query, NULL);
    rasqal_literal* blank=rasqal_new_simple_literal(RASQAL_LITERAL_BLANK, blank_id);
    t2->subject=rasqal_new_literal_from_literal(blank);
    t2->predicate=rasqal_new_literal_from_literal(first_identifier);
    /* t2->object already set to the value we want */

    /* add new triple we needed */
    t2=rasqal_new_triple(rasqal_new_literal_from_literal(blank),
                         rasqal_new_literal_from_literal(rest_identifier),
                         rasqal_new_literal_from_literal(object));
    raptor_sequence_push(seq, t2);

    rasqal_free_literal(object);

    object=blank;
  }

  if($$->value)
    rasqal_free_literal($$->value);
  $$->value=object;
  
#if RASQAL_DEBUG > 1
  printf(" after substitution objectList=");
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
VarOrURIList : VarOrURIList Var
{
  $$=$1;
  raptor_sequence_push($$, rasqal_new_variable_literal($2));
}
| VarOrURIList ',' Var
{
  $$=$1;
  raptor_sequence_push($$, rasqal_new_variable_literal($3));
}
| VarOrURIList URI
{
  $$=$1;
  raptor_sequence_push($$, $2);
}
| VarOrURIList ',' URI
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
| URI
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


/* SPARQL Grammar: [26] VarOrURI */
VarOrURI : Var
{
  $$=rasqal_new_variable_literal($1);
}
| URI
{
  $$=$1;
}
;


/* SPARQL Grammar: [27] VarOrLiteral */
VarOrLiteral : Var
{
  $$=rasqal_new_variable_literal($1);
}
| Literal
{
  $$=$1;
}
;

/* SPARQL Grammar: [28] VarAsNnode - junk */

/* SPARQL Grammar: [29] VarAsExpr - junk */


/* NEW Grammar Term */
VarOrLiteralOrBNode : VarOrLiteral
{
  $$=$1;
}
| BNode
{
  $$=$1;
}
;


/* SPARQL Grammar: [32] Expression */
Expression : ConditionalAndExpression SC_OR Expression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_OR, $1, $3);
}
| ConditionalAndExpression
{
  $$=$1;
}
;

/* SPARQL Grammar: [33] ConditionalOrExpression - merged into Expression */

/* SPARQL Grammar: [34] ConditionalXorExpression - merged into ConditionalOrExpression */


/* SPARQL Grammar: [35] ConditionalAndExpression */
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

/* SPARQL Grammar: [36] ValueLogical */
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

/* SPARQL Grammar: [37] StringEqualityExpression - merged into ValueLogical */

/* SPARQL Grammar: [38] NumericalLogical - merged into ValueLogical */

/* SPARQL Grammar: [39] EqualityExpression */
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

/* SPARQL Grammar: [40] RelationalExpression */
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

/* SPARQL Grammar: [41] NumericExpression - merged into RelationalExpression */

/* SPARQL Grammar: [42] AdditiveExpression */
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

/* SPARQL Grammar: [43] MultiplicativeExpression */
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

/* SPARQL Grammar: [44] UnaryExpression */
UnaryExpression :  '-' BuiltinExpression
{
  $$=rasqal_new_1op_expression(RASQAL_EXPR_UMINUS, $2);
}
|'~' BuiltinExpression
{
  $$=rasqal_new_1op_expression(RASQAL_EXPR_BANG, $2);
}
|'!' BuiltinExpression
{
  $$=rasqal_new_1op_expression(RASQAL_EXPR_BANG, $2);
}
| '+' BuiltinExpression
{
  $$=$2;
}
| BuiltinExpression
{
  $$=$1;
}
;

/* SPARQL Grammar: [45] BuiltInExpression */
BuiltinExpression : BOUND '(' Var ')'
{
  rasqal_literal *l=rasqal_new_variable_literal($3);
  rasqal_expression *e=rasqal_new_literal_expression(l);
  $$=rasqal_new_1op_expression(RASQAL_EXPR_BOUND, e);
}
| STR '(' VarOrLiteral ')'
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
| REGEX '(' Expression ',' Literal ')'
{
  /* FIXME - regex needs more thought */
  const unsigned char* pattern=rasqal_literal_as_string($5);
  rasqal_literal* l=rasqal_new_pattern_literal(pattern, NULL);
  $$=rasqal_new_string_op_expression(RASQAL_EXPR_STR_MATCH, $3, l);
}
| REGEX '(' Expression ',' Literal ',' Literal ')'
{
  /* FIXME - regex needs more thought */
  const unsigned char* pattern=rasqal_literal_as_string($5);
  const char* flags=(const char*)rasqal_literal_as_string($7);
  rasqal_literal* l=rasqal_new_pattern_literal(pattern, flags);
  $$=rasqal_new_string_op_expression(RASQAL_EXPR_STR_MATCH, $3, l);
}
| URIBrace Expression ')' 
{
  raptor_uri* uri=rasqal_literal_as_uri($1);
  $$=rasqal_new_cast_expression(raptor_uri_copy(uri), $2);
  rasqal_free_literal($1);
}
| FunctionCall
{
  $$=$1;
}
|
PrimaryExpression
{
  $$=$1;
}
;


/* SPARQL Grammar: [46] PrimaryExpression */
PrimaryExpression: Var
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

/* SPARQL Grammar: [47] FunctionCall */
FunctionCall: URI '(' ArgList ')'
{
  raptor_uri* uri=rasqal_literal_as_uri($1);
  $$=rasqal_new_function_expression(raptor_uri_copy(uri), $3);
  rasqal_free_literal($1);
}


/* SPARQL Grammar: [48] ArgList */
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


/* SPARQL Grammar: [49] VarOrLiteralAsExpr - junk */

/* SPARQL Grammar: [50] Literal */
Literal : URI
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

/* SPARQL Grammar: [51] NumericLiteral - merged into Literal */

/* SPARQL Grammar: [52] TextLiteral - merged into Literal */

/* SPARQL Grammar: [53] PatternLiteral - made into terminal PATTERN_LITERAL */

/* SPARQL Grammar: [54] URI */
URI : URI_LITERAL
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

/* NEW Grammar Term made from SPARQL Grammer: [54] URI + '(' expanded */
URIBrace : URI_LITERAL_BRACE
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

/* SPARQL Grammar: [55] QName - made into terminal QNAME_LITERAL */

/* SPARQL Grammar: [56] BNode */
BNode : BLANK_LITERAL
{
  $$=rasqal_new_simple_literal(RASQAL_LITERAL_BLANK, $1);
}
;

/* SPARQL Grammar: [57] QuotedURI - made into terminal URI_LITERAL */

/* SPARQL Grammar: [58] Integer - made into terminal INTEGER_LITERAL */

/* SPARQL Grammar: [59] FloatingPoint - made into terminal FLOATING_POINT_LITERAL */


/* SPARQL Grammar: [60] onwards are all lexer items with similar
 * names or are inlined except for:
 */

/* SPARQL Grammer: [65] <VAR> */
Var : '?' IDENTIFIER
{
  $$=rasqal_new_variable((rasqal_query*)rq, $2, NULL);
}
| '$' IDENTIFIER
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

  /* FIXME.  This is a bit of a hack.  Fold the basic patterns and
   * then make a new top graph pattern so the query engine always
   * sees a sequence of graph patterns at the top.  It should
   * operate fine on a graph pattern with just triples but it
   * doesn't seem to.
   */
  if(rdf_query->query_graph_pattern) {
    raptor_sequence *seq;

    rasqal_engine_make_basic_graph_pattern(rdf_query->query_graph_pattern);
    
    seq=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_graph_pattern, (raptor_sequence_print_handler*)rasqal_graph_pattern_print);
    raptor_sequence_push(seq, rdf_query->query_graph_pattern);
    
    rdf_query->query_graph_pattern=rasqal_new_graph_pattern_from_sequence(rdf_query, seq, 0);
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
  fread(query_string, SPARQL_FILE_BUF_SIZE, 1, fh);
  
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

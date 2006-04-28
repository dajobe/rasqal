/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * sparql_parser.y - Rasqal SPARQL parser over tokens from sparql_lexer.l
 *
 * $Id$
 *
 * Copyright (C) 2004-2006, David Beckett http://purl.org/net/dajobe/
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
 *   SPARQL Query Language for RDF, 23 November 2005
 *   http://www.w3.org/TR/2005/WD-rdf-sparql-query-20051123/
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

#define DEBUG_FH stdout

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
 * FIXME: document this
 */
%expect 12

/* word symbols */
%token SELECT FROM WHERE
%token OPTIONAL PREFIX DESCRIBE CONSTRUCT ASK DISTINCT LIMIT UNION
%token BASE BOUND STR LANG DATATYPE ISURI ISBLANK ISLITERAL
%token GRAPH NAMED FILTER OFFSET A ORDER BY REGEX ASC DESC LANGMATCHES

/* expression delimitors */

%token ',' '(' ')' '[' ']' '{' '}'
%token '?' '$'

/* SC booleans */
%left SC_OR SC_AND

/* operations */
%left EQ NEQ LT GT LE GE

/* arithmetic operations */
%left '+' '-' '*' '/'

/* unary operations */

/* literals */
%token <literal> FLOATING_POINT_LITERAL
%token <literal> STRING_LITERAL INTEGER_LITERAL
%token <literal> BOOLEAN_LITERAL DECIMAL_LITERAL
%token <uri> URI_LITERAL URI_LITERAL_BRACE
%token <name> QNAME_LITERAL BLANK_LITERAL QNAME_LITERAL_BRACE

%token <name> IDENTIFIER


%type <seq> SelectQuery ConstructQuery DescribeQuery
%type <seq> VarList VarOrIRIrefList ArgList ConstructTriplesOpt
%type <seq> ConstructTemplate OrderConditionList
%type <seq> GraphNodeListNotEmpty

%type <formula> TriplesSameSubject TriplesSameSubjectDotListOpt
%type <formula> PropertyList PropertyListTailOpt PropertyListNotEmpty
%type <formula> ObjectList ObjectTail Collection
%type <formula> VarOrTerm Verb GraphNode TriplesNode
%type <formula> BlockOfTriplesOpt

%type <graph_pattern> GroupGraphPattern GraphPattern
%type <graph_pattern> GraphGraphPattern OptionalGraphPattern
%type <graph_pattern> GroupOrUnionGraphPattern GroupOrUnionGraphPatternList
%type <graph_pattern> GraphPatternNotTriples
%type <graph_pattern> BasicGraphPattern

%type <expr> Expression ConditionalOrExpression ConditionalAndExpression
%type <expr> RelationalExpression AdditiveExpression
%type <expr> MultiplicativeExpression UnaryExpression
%type <expr> BuiltInCall RegexExpression FunctionCall
%type <expr> BrackettedExpression PrimaryExpression
%type <expr> OrderCondition Constraint

%type <literal> GraphTerm IRIref BlankNode
%type <literal> VarOrIRIref VarOrBlankNodeOrIRIref
%type <literal> IRIrefBrace

%type <variable> Var


%%

/* Below here, grammar terms are numbered from
 * http://www.w3.org/TR/2005/WD-rdf-sparql-query-20051123/
 * except where noted
 */

/* SPARQL Grammar: [1] Query */
Query: Prolog ReportFormat
        DatasetClauseOpt WhereClauseOpt 
        OrderClauseOpt LimitClauseOpt OffsetClauseOpt
{
}
;


/* NEW Grammar Term pulled out of [1] Query */
ReportFormat: SelectQuery
{
  ((rasqal_query*)rq)->selects=$1;
  ((rasqal_query*)rq)->verb=RASQAL_QUERY_VERB_SELECT;
}
|  ConstructQuery
{
  ((rasqal_query*)rq)->constructs=$1;
  ((rasqal_query*)rq)->verb=RASQAL_QUERY_VERB_CONSTRUCT;
}
|  DescribeQuery
{
  ((rasqal_query*)rq)->describes=$1;
  ((rasqal_query*)rq)->verb=RASQAL_QUERY_VERB_DESCRIBE;
}
| AskQuery
{
  ((rasqal_query*)rq)->verb=RASQAL_QUERY_VERB_ASK;
}
;


/* SPARQL Grammar: [2] Prolog */
Prolog: BaseDeclOpt PrefixDeclOpt
{
  /* nothing to do */
}
;


/* SPARQL Grammar: [3] BaseDecl */
BaseDeclOpt: BASE URI_LITERAL
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
PrefixDeclOpt: PrefixDeclOpt PREFIX IDENTIFIER URI_LITERAL
{
  raptor_sequence *seq=((rasqal_query*)rq)->prefixes;
  unsigned const char* prefix_string=$3;
  size_t l=0;

  if(prefix_string)
    l=strlen((const char*)prefix_string);
  
  if(raptor_namespaces_find_namespace(((rasqal_query*)rq)->namespaces, prefix_string, l)) {
    /* A prefix may be defined only once */
    sparql_syntax_warning(((rasqal_query*)rq), 
                          "PREFIX %s can be defined only once.",
                          prefix_string ? (const char*)prefix_string : ":");
  } else {
    rasqal_prefix *p=rasqal_new_prefix(prefix_string, $4);
    raptor_sequence_push(seq, p);
    rasqal_query_declare_prefix(((rasqal_query*)rq), p);
  }
}
| /* empty */
{
  /* nothing to do, rq->prefixes already initialised */
}
;


/* SPARQL Grammar: [5] SelectQuery */
SelectQuery: SELECT DISTINCT VarList
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


/* NEW Grammar Term pulled out of [5] SelectQuery */
VarList: VarList Var
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


/* SPARQL Grammar: [6] DescribeQuery */
DescribeQuery: DESCRIBE VarOrIRIrefList
{
  $$=$2;
}
| DESCRIBE '*'
{
  $$=NULL;
}
;


/* NEW Grammar Term pulled out of [6] DescribeQuery */
VarOrIRIrefList: VarOrIRIrefList VarOrIRIref
{
  $$=$1;
  raptor_sequence_push($$, $2);
}
| VarOrIRIrefList ',' VarOrIRIref
{
  $$=$1;
  raptor_sequence_push($$, $3);
}
| VarOrIRIref
{
  $$=raptor_new_sequence(NULL, (raptor_sequence_print_handler*)rasqal_literal_print);
  raptor_sequence_push($$, $1);
}
;


/* SPARQL Grammar: [7] ConstructQuery */
ConstructQuery: CONSTRUCT ConstructTemplate
{
  $$=$2;
}
| CONSTRUCT '*'
{
  $$=NULL;
  ((rasqal_query*)rq)->wildcard=1;
}
;


/* SPARQL Grammar: [8] AskQuery */
AskQuery: ASK 
{
  /* nothing to do */
}


/* SPARQL Grammar: [9] DatasetClause */
DatasetClauseOpt: DatasetClauseOpt FROM DefaultGraphClause
| DatasetClauseOpt FROM NamedGraphClause
| /* empty */
;


/* SPARQL Grammar: [10] DefaultGraphClause */
DefaultGraphClause: IRIref
{
  if($1) {
    raptor_uri* uri=rasqal_literal_as_uri($1);
    rasqal_query_add_data_graph((rasqal_query*)rq, uri, uri, RASQAL_DATA_GRAPH_BACKGROUND);
    rasqal_free_literal($1);
  }
}
;  


/* SPARQL Grammar: [11] NamedGraphClause */
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


/* SPARQL Grammar: [14] SolutionModifier - merged into SelectQuery etc. */

/* SPARQL Grammar: [15] OrderClause - remained for clarity */
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


/* NEW Grammar Term pulled out of [16] OrderClauseOpt */
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


/* SPARQL Grammar: [16] OrderCondition */
OrderCondition: ASC BrackettedExpression
{
  $$=rasqal_new_1op_expression(RASQAL_EXPR_ORDER_COND_ASC, $2);
}
| DESC BrackettedExpression
{
  $$=rasqal_new_1op_expression(RASQAL_EXPR_ORDER_COND_DESC, $2);
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


/* SPARQL Grammar: [17] LimitClause - remained for clarity */
LimitClauseOpt:  LIMIT INTEGER_LITERAL
{
  if(((rasqal_query*)rq)->verb == RASQAL_QUERY_VERB_ASK) {
    sparql_query_error((rasqal_query*)rq, "LIMIT cannot be used with ASK");
  } else {
    if($2 != NULL) {
      ((rasqal_query*)rq)->limit=$2->value.integer;
      rasqal_free_literal($2);
    }
  }
  
}
| /* empty */
;


/* SPARQL Grammar: [18] OffsetClause - remained for clarity */
OffsetClauseOpt:  OFFSET INTEGER_LITERAL
{
  if(((rasqal_query*)rq)->verb == RASQAL_QUERY_VERB_ASK) {
    sparql_query_error((rasqal_query*)rq, "LIMIT cannot be used with ASK");
  } else {
    if($2 != NULL) {
      ((rasqal_query*)rq)->offset=$2->value.integer;
      rasqal_free_literal($2);
    }
  }
}
| /* empty */
;


/* SPARQL Grammar: [19] GroupGraphPattern */
GroupGraphPattern: '{' GraphPattern '}'
{
  $$=$2;
}


/* NEW Grammar Term */
DotOptional: '.'
| /* empty */
;


/* SPARQL Grammar: [20] GraphPattern */
GraphPattern: BasicGraphPattern GraphPatternNotTriples DotOptional GraphPattern
{
#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "GraphPattern 1\n  BasicGraphPattern=");
  if($1)
    rasqal_graph_pattern_print($1, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fprintf(DEBUG_FH, ", GraphPatternNotTriples=");
  if($2)
    rasqal_graph_pattern_print($2, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fprintf(DEBUG_FH, ", GraphPattern=");
  if($4)
    rasqal_graph_pattern_print($4, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fputs("\n", DEBUG_FH);
#endif

  $$=$4;
  /* push ($1,$2) to start of $4 graph sequence */
  if($2)
    raptor_sequence_shift($$->graph_patterns, $2);
  if($1)
    raptor_sequence_shift($$->graph_patterns, $1);

#if RASQAL_DEBUG > 1
  fprintf(DEBUG_FH, "  after grouping graph pattern=");
  if($$)
    rasqal_graph_pattern_print($$, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif
}
| BasicGraphPattern 
{
  raptor_sequence *seq;

#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "GraphPattern 2\n  BasicGraphPattern=");
  if($1)
    rasqal_graph_pattern_print($1, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fputs("\n", DEBUG_FH);
#endif

  seq=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_graph_pattern, (raptor_sequence_print_handler*)rasqal_graph_pattern_print);
  if($1)
    raptor_sequence_push(seq, $1);

  $$=rasqal_new_graph_pattern_from_sequence((rasqal_query*)rq, seq,
                                            RASQAL_GRAPH_PATTERN_OPERATOR_GROUP);
#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "  after grouping graph pattern=");
  if($$)
    rasqal_graph_pattern_print($$, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif
}
;


/* SPARQL Grammar Term rq23 [21]  BasicGraphPattern */
BasicGraphPattern: BlockOfTriplesOpt Constraint DotOptional BasicGraphPattern
{
#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "BasicGraphPattern 1\n  BlockOfTriplesOpt=");
  if($1)
    rasqal_formula_print($1, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fprintf(DEBUG_FH, ", Constraint=");
  if($2)
    rasqal_expression_print($2, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fprintf(DEBUG_FH, ", BasicGraphPattern=");
  if($4)
    rasqal_graph_pattern_print($4, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fputs("\n", DEBUG_FH);
#endif

  $$=$4;
  /* push $1 to start of $4 graph sequence */
  if($1) {
    rasqal_graph_pattern *gp;
    
    gp=rasqal_engine_new_graph_pattern_from_formula((rasqal_query*)rq, $1,
                                                    RASQAL_GRAPH_PATTERN_OPERATOR_BASIC);

    raptor_sequence_shift($$->graph_patterns, gp);
  }
  
  if($2)
    rasqal_graph_pattern_add_constraint($$, $2);
  
#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "  after grouping graph pattern=");
  if($$)
    rasqal_graph_pattern_print($$, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif
}
| BlockOfTriplesOpt
{
  rasqal_graph_pattern *formula_gp=NULL;
  raptor_sequence *seq;

#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "BasicGraphPattern 2\n  BlockOfTriplesOpt=");
  if($1)
    rasqal_formula_print($1, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fputs("\n", DEBUG_FH);
#endif

  if($1)
    formula_gp=rasqal_engine_new_graph_pattern_from_formula((rasqal_query*)rq, $1,
                                                            RASQAL_GRAPH_PATTERN_OPERATOR_BASIC);
  
  seq=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_graph_pattern, (raptor_sequence_print_handler*)rasqal_graph_pattern_print);
  if(formula_gp)
    raptor_sequence_push(seq, formula_gp);

  $$=rasqal_new_graph_pattern_from_sequence((rasqal_query*)rq,
                                            seq,
                                            RASQAL_GRAPH_PATTERN_OPERATOR_GROUP);

#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "  after, group graph pattern=");
  if($$)
    rasqal_graph_pattern_print($$, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif
}
;


/* SPARQL Grammar: rq23 [22] BlockOfTriples */
BlockOfTriplesOpt: TriplesSameSubject TriplesSameSubjectDotListOpt
{
#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "BlockOfTriplesOpt\n  TriplesSameSubject=");
  if($1)
    rasqal_formula_print($1, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fputs("  TriplesSameSubjectDotListOpt=", DEBUG_FH);
  if($2)
    rasqal_formula_print($2, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fputs("\n", DEBUG_FH);
#endif


  /* $1 and $2 are freed as necessary */
  $$=rasqal_formula_join($1, $2);
#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "  after joining formula=");
  rasqal_formula_print($$, DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif
}
| /* empty */
{
  $$=NULL;
}
;



/* New Grammar Term pulled out of rq23 [22] BlockOfTriples 
 * ( '.'  TriplesSameSubject? )*
 */
TriplesSameSubjectDotListOpt: TriplesSameSubjectDotListOpt TriplesSameSubject
{
#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "TriplesSameSubjectDotTriplesOpt\n  TriplesSameSubjectDotListOpt=");
  if($1)
    rasqal_formula_print($1, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fputs("  TriplesSameSubject=", DEBUG_FH);
  if($2)
    rasqal_formula_print($2, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fputs("\n", DEBUG_FH);
#endif

  /* $1 and $2 are freed as necessary */
  $$=rasqal_formula_join($1, $2);

#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "  after joining formula=");
  rasqal_formula_print($$, DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif
}
| TriplesSameSubjectDotListOpt '.'
{
  $$=$1;
}
| /* empty */
{
  $$=NULL;
}
;


/* SPARQL Grammar: rq23 [23] GraphPatternNotTriples */
GraphPatternNotTriples: OptionalGraphPattern
{
  $$=$1;
}
| GroupOrUnionGraphPattern
{
  $$=$1;
}
| GraphGraphPattern
{
  $$=$1;
}
;


/* SPARQL Grammar: rq23 [24] OptionalGraphPattern */
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

  if($2)
    $2->op = RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL;

  $$=$2;
}
;


/* SPARQL Grammar: rq23 [25] GraphGraphPattern */
GraphGraphPattern: GRAPH VarOrBlankNodeOrIRIref GroupGraphPattern
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
    rasqal_graph_pattern_set_origin($3, $2);
    $3->op = RASQAL_GRAPH_PATTERN_OPERATOR_GRAPH;
  }

  rasqal_free_literal($2);
  $$=$3;
}
;


/* SPARQL Grammar: rq23 [26] GroupOrUnionGraphPattern */
GroupOrUnionGraphPattern: GroupGraphPattern UNION GroupOrUnionGraphPatternList
{
  $$=$3;
  raptor_sequence_push($$->graph_patterns, $1);

#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "UnionGraphPattern\n  graphpattern=");
  rasqal_graph_pattern_print($$, DEBUG_FH);
  fputs("\n\n", DEBUG_FH);
#endif
}
| GroupGraphPattern
{
  $$=$1;
}
;

/* NEW Grammar Term pulled out of rq23 [26] GroupOrUnionGraphPattern */
GroupOrUnionGraphPatternList: GroupOrUnionGraphPatternList UNION GroupGraphPattern
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


/* SPARQL Grammar: rq23 [27] Constraint */
Constraint: FILTER BrackettedExpression
{
  $$=$2;
}
| FILTER BuiltInCall
{
  $$=$2;
}
| FILTER FunctionCall
{
  $$=$2;
}
;


/* SPARQL Grammar: rq23 [28] ConstructTemplate */
ConstructTemplate:  '{' ConstructTriplesOpt '}'
{
  $$=$2;
}
;


/* SPARQL Grammar: rq23 [29] ConstructTriples renamed for clarity */
ConstructTriplesOpt: TriplesSameSubject '.' ConstructTriplesOpt
{
  $$=NULL;
 
  if($1) {
    $$=$1->triples;
    $1->triples=NULL;
    rasqal_free_formula($1);
  }
  
  if($3) {
    if(!$$)
      $$=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_triple, (raptor_sequence_print_handler*)rasqal_triple_print);

    raptor_sequence_join($$, $3);
    raptor_free_sequence($3);
  }

 }
| TriplesSameSubject
{
  $$=NULL;
  
  if($1) {
    $$=$1->triples;
    $1->triples=NULL;
    rasqal_free_formula($1);
  }
  
}
| /* empty */
{
  $$=NULL;
}
;


/* SPARQL Grammar: rq23 [30] TriplesSameSubject */
TriplesSameSubject: VarOrTerm PropertyListNotEmpty
{
  int i;

#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "TriplesSameSubject 1\n subject=");
  rasqal_formula_print($1, DEBUG_FH);
  if($2) {
    fprintf(DEBUG_FH, "\n propertyList (reverse order to syntax)=");
    rasqal_formula_print($2, DEBUG_FH);
    fprintf(DEBUG_FH, "\n");
  } else     
    fprintf(DEBUG_FH, "\n and empty propertyList\n");
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
    fprintf(DEBUG_FH, "  after substitution propertyList=");
    rasqal_formula_print($2, DEBUG_FH);
    fprintf(DEBUG_FH, "\n");
#endif
  }

  $$=rasqal_formula_join($1, $2);
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
  fprintf(DEBUG_FH, "TriplesSameSubject 2\n subject=");
  rasqal_formula_print($1, DEBUG_FH);
  if($2) {
    fprintf(DEBUG_FH, "\n propertyList (reverse order to syntax)=");
    rasqal_formula_print($2, DEBUG_FH);
    fprintf(DEBUG_FH, "\n");
  } else     
    fprintf(DEBUG_FH, "\n and empty propertyList\n");
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
    fprintf(DEBUG_FH, "  after substitution propertyList=");
    rasqal_formula_print($2, DEBUG_FH);
    fprintf(DEBUG_FH, "\n");
#endif
  }

  $$=rasqal_formula_join($1, $2);
#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "  after joining formula=");
  rasqal_formula_print($$, DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif
}
;


/* SPARQL Grammar: rq23 [31] PropertyList */
PropertyList: PropertyListNotEmpty
{
  $$=$1;
}
| /* empty */
{
  $$=NULL;
}
;


/* SPARQL Grammar: rq23 [32] PropertyListNotEmpty */
PropertyListNotEmpty: Verb ObjectList PropertyListTailOpt
{
  int i;
  
#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "PropertyList 1\n Verb=");
  rasqal_formula_print($1, DEBUG_FH);
  fprintf(DEBUG_FH, "\n ObjectList=");
  rasqal_formula_print($2, DEBUG_FH);
  fprintf(DEBUG_FH, "\n PropertyListTail=");
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
    raptor_sequence *seq=$2->triples;
    rasqal_literal *predicate=$1->value;

    /* non-empty property list, handle it  */
    for(i=0; i<raptor_sequence_size(seq); i++) {
      rasqal_triple* t2=(rasqal_triple*)raptor_sequence_get_at(seq, i);
      if(!t2->predicate)
        t2->predicate=(rasqal_literal*)rasqal_new_literal_from_literal(predicate);
    }
  
#if RASQAL_DEBUG > 1  
    fprintf(DEBUG_FH, "  after substitution ObjectList=");
    raptor_sequence_print(seq, DEBUG_FH);
    fprintf(DEBUG_FH, "\n");
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
    fprintf(DEBUG_FH, "  after appending ObjectList (reverse order)=");
    rasqal_formula_print($3, DEBUG_FH);
    fprintf(DEBUG_FH, "\n\n");
#endif

    rasqal_free_formula($2);
  }

  if($1)
    rasqal_free_formula($1);

  $$=$3;
}
;


/* NEW Grammar Term pulled out of rq23 [32] PropertyListNotEmpty */
PropertyListTailOpt: ';' PropertyList
{
  $$=$2;
}
| /* empty */
{
  $$=NULL;
}
;


/* SPARQL Grammar: rq23 [33] ObjectList */
ObjectList: GraphNode ObjectTail
{
  rasqal_formula *formula=NULL;
  rasqal_triple *triple;

#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "ObjectList 1\n");
  fprintf(DEBUG_FH, " GraphNode=\n");
  rasqal_formula_print($1, DEBUG_FH);
  fprintf(DEBUG_FH, "\n");
  if($2) {
    fprintf(DEBUG_FH, " ObjectTail=");
    rasqal_formula_print($2, DEBUG_FH);
    fprintf(DEBUG_FH, "\n");
  } else
    fprintf(DEBUG_FH, " and empty ObjectTail\n");
#endif

  if($2)
    formula=$2;
  else
    formula=rasqal_new_formula();
  
  triple=rasqal_new_triple(NULL, NULL, $1->value);
  $1->value=NULL;

  if(!formula->triples)
    formula->triples=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_triple, (raptor_sequence_print_handler*)rasqal_triple_print);

  raptor_sequence_push(formula->triples, triple);

  $$=rasqal_formula_join($1, formula);

#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, " objectList is now ");
  if($$)
    raptor_sequence_print($$->triples, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif
}
;


/* NEW Grammar Term pulled out of rq23 [33] ObjectList */
ObjectTail: ',' ObjectList
{
  $$=$2;
}
| /* empty */
{
  $$=NULL;
}
;


/* SPARQL Grammar: rq23 [34] Verb */
Verb: VarOrBlankNodeOrIRIref
{
  $$=rasqal_new_formula();
  $$->value=$1;
}
| A
{
  raptor_uri *uri;

#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "verb Verb=rdf:type (a)\n");
#endif

  uri=raptor_new_uri_for_rdf_concept("type");
  $$=rasqal_new_formula();
  $$->value=rasqal_new_uri_literal(uri);
}
;


/* SPARQL Grammar: rq23 [35] TriplesNode */
TriplesNode: Collection
{
  $$=$1;
}
|
'[' PropertyListNotEmpty ']'
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
    fprintf(DEBUG_FH, "TriplesNode\n PropertyList=");
    rasqal_formula_print($$, DEBUG_FH);
    fprintf(DEBUG_FH, "\n");
#endif
  } else {
    raptor_sequence *seq=$2->triples;

    /* non-empty property list, handle it  */
#if RASQAL_DEBUG > 1  
    fprintf(DEBUG_FH, "TriplesNode\n PropertyList=");
    raptor_sequence_print(seq, DEBUG_FH);
    fprintf(DEBUG_FH, "\n");
#endif

    for(i=0; i<raptor_sequence_size(seq); i++) {
      rasqal_triple* t2=(rasqal_triple*)raptor_sequence_get_at(seq, i);
      if(t2->subject)
        continue;
      
      t2->subject=(rasqal_literal*)rasqal_new_literal_from_literal($$->value);
    }

#if RASQAL_DEBUG > 1
    fprintf(DEBUG_FH, "  after substitution formula=");
    rasqal_formula_print($$, DEBUG_FH);
    fprintf(DEBUG_FH, "\n\n");
#endif
  }
  
}
;


/* SPARQL Grammar: rq23 [37] Collection (allowing empty case) */
Collection: '(' GraphNodeListNotEmpty ')'
{
  int i;
  rasqal_query* rdf_query=(rasqal_query*)rq;
  rasqal_literal* first_identifier;
  rasqal_literal* rest_identifier;
  rasqal_literal* object;

#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "Collection\n GraphNodeListNotEmpty=");
  raptor_sequence_print($2, DEBUG_FH);
  fprintf(DEBUG_FH, "\n");
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
  fprintf(DEBUG_FH, "  after substitution collection=");
  rasqal_formula_print($$, DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif

  rasqal_free_literal(first_identifier);
  rasqal_free_literal(rest_identifier);
}
;


/* NEW Grammar Term pulled out of rq23 [37] Collection */
/* Sequence of formula */
GraphNodeListNotEmpty: GraphNodeListNotEmpty GraphNode
{
#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "GraphNodeListNotEmpty 1\n");
  if($2) {
    fprintf(DEBUG_FH, " GraphNode=");
    rasqal_formula_print($2, DEBUG_FH);
    fprintf(DEBUG_FH, "\n");
  } else  
    fprintf(DEBUG_FH, " and empty GraphNode\n");
  if($1) {
    fprintf(DEBUG_FH, " GraphNodeListNotEmpty=");
    raptor_sequence_print($1, DEBUG_FH);
    fprintf(DEBUG_FH, "\n");
  } else
    fprintf(DEBUG_FH, " and empty GraphNodeListNotEmpty\n");
#endif

  if(!$2)
    $$=NULL;
  else {
    raptor_sequence_push($$, $2);
#if RASQAL_DEBUG > 1  
    fprintf(DEBUG_FH, " itemList is now ");
    raptor_sequence_print($$, DEBUG_FH);
    fprintf(DEBUG_FH, "\n\n");
#endif
  }

}
| GraphNode
{
#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "GraphNodeListNotEmpty 2\n");
  if($1) {
    fprintf(DEBUG_FH, " GraphNode=");
    rasqal_formula_print($1, DEBUG_FH);
    fprintf(DEBUG_FH, "\n");
  } else  
    fprintf(DEBUG_FH, " and empty GraphNode\n");
#endif

  $$=NULL;
  if($1)
    $$=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_formula, (raptor_sequence_print_handler*)rasqal_formula_print);

  raptor_sequence_push($$, $1);
#if RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, " GraphNodeListNotEmpty is now ");
  raptor_sequence_print($$, DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif
}
;


/* SPARQL Grammar: rq23 [38] GraphNode */
GraphNode: VarOrTerm
{
  $$=$1;
}
| TriplesNode
{
  $$=$1;
}
;


/* SPARQL Grammar Term: rq23 [39] VarOrTerm */
VarOrTerm: Var
{
  $$=rasqal_new_formula();
  $$->value=rasqal_new_variable_literal($1);
}
| GraphTerm
{
  $$=rasqal_new_formula();
  $$->value=$1;
}
;

/* SPARQL Grammar: rq23 [40] VarOrIRIref */
VarOrIRIref: Var
{
  $$=rasqal_new_variable_literal($1);
}
| IRIref
{
  $$=$1;
}
;


/* SPARQL Grammar Term: rq23 [41] VarOrBlankNodeOrIRIref */
VarOrBlankNodeOrIRIref: Var
{
  $$=rasqal_new_variable_literal($1);
}
| BlankNode
{
  $$=$1;
}
| IRIref
{
  $$=$1;
}
;

/* SPARQL Grammar: rq23 [42] Var */
Var: '?' IDENTIFIER
{
  $$=rasqal_new_variable((rasqal_query*)rq, $2, NULL);
}
| '$' IDENTIFIER
{
  $$=rasqal_new_variable((rasqal_query*)rq, $2, NULL);
}
;


/* SPARQL Grammar: rq23 [43] GraphTerm */
/* + and - for numerics is in the lexer */
GraphTerm: IRIref
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
| DECIMAL_LITERAL
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
| BlankNode
{
  $$=$1;
}
|  '(' ')'
{
  $$=rasqal_new_uri_literal(raptor_uri_copy(rasqal_rdf_nil_uri));
}
;

/* SPARQL Grammar: rq23 [44] Expression */
Expression: ConditionalOrExpression
{
  $$=$1;
}
;


/* SPARQL Grammar: rq23 [45] ConditionalOrExpression */
ConditionalOrExpression: ConditionalOrExpression SC_OR ConditionalAndExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_OR, $1, $3);
}
| ConditionalAndExpression
{
  $$=$1;
}
;


/* SPARQL Grammar: [45] ConditionalAndExpression */
ConditionalAndExpression: ConditionalAndExpression SC_AND RelationalExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_AND, $1, $3);
;
}
| RelationalExpression
{
  $$=$1;
}
;

/* SPARQL Grammar: [46] ValueLogical - merged into RelationalExpression */

/* SPARQL Grammar: [47] RelationalExpression */
RelationalExpression: AdditiveExpression EQ AdditiveExpression
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

/* SPARQL Grammar: [48] NumericExpression - merged into AdditiveExpression */

/* SPARQL Grammar: [49] AdditiveExpression */
AdditiveExpression: MultiplicativeExpression '+' AdditiveExpression
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

/* SPARQL Grammar: [50] MultiplicativeExpression */
MultiplicativeExpression: UnaryExpression '*' MultiplicativeExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_STAR, $1, $3);
}
| UnaryExpression '/' MultiplicativeExpression
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_SLASH, $1, $3);
}
| UnaryExpression
{
  $$=$1;
}
;


/* SPARQL Grammar: [51] UnaryExpression */
UnaryExpression: '!' PrimaryExpression
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


/* SPARQL Grammar: [52] BuiltInCall */
BuiltInCall: STR '(' Expression ')'
{
  $$=rasqal_new_1op_expression(RASQAL_EXPR_STR, $3);
}
| LANG '(' Expression ')'
{
  $$=rasqal_new_1op_expression(RASQAL_EXPR_LANG, $3);
}
| LANGMATCHES '(' Expression ',' Expression ')'
{
  $$=rasqal_new_2op_expression(RASQAL_EXPR_LANGMATCHES, $3, $5);
}
| DATATYPE '(' Expression ')'
{
  $$=rasqal_new_1op_expression(RASQAL_EXPR_DATATYPE, $3);
}
| BOUND '(' Var ')'
{
  rasqal_literal *l=rasqal_new_variable_literal($3);
  rasqal_expression *e=rasqal_new_literal_expression(l);
  $$=rasqal_new_1op_expression(RASQAL_EXPR_BOUND, e);
}
| ISURI '(' Expression ')'
{
  $$=rasqal_new_1op_expression(RASQAL_EXPR_ISURI, $3);
}
| ISBLANK '(' Expression ')'
{
  $$=rasqal_new_1op_expression(RASQAL_EXPR_ISBLANK, $3);
}
| ISLITERAL '(' Expression ')'
{
  $$=rasqal_new_1op_expression(RASQAL_EXPR_ISLITERAL, $3);
}
| RegexExpression
{
  $$=$1;
}
;


/* SPARQL Grammar: [54] RegexExpression */
RegexExpression: REGEX '(' Expression ',' Expression ')'
{
  $$=rasqal_new_3op_expression(RASQAL_EXPR_REGEX, $3, $5, NULL);
}
| REGEX '(' Expression ',' Expression ',' Expression ')'
{
  $$=rasqal_new_3op_expression(RASQAL_EXPR_REGEX, $3, $5, $7);
}
;


/* SPARQL Grammar: [54] FunctionCall */
FunctionCall: IRIrefBrace ArgList ')'
{
  raptor_uri* uri=rasqal_literal_as_uri($1);
  
  uri=raptor_uri_copy(uri);

  if(!$2)
    $2=raptor_new_sequence((raptor_sequence_free_handler*)rasqal_free_expression, (raptor_sequence_print_handler*)rasqal_expression_print);

  if(raptor_sequence_size($2) == 1 &&
     sparql_is_builtin_xsd_datatype(uri)) {
    rasqal_expression* e=(rasqal_expression*)raptor_sequence_pop($2);
    $$=rasqal_new_cast_expression(uri, e);
    raptor_free_sequence($2);
  } else
    $$=rasqal_new_function_expression(uri, $2);
  rasqal_free_literal($1);
}
;


/* SPARQL Grammar: [55] ArgList */
ArgList: ArgList ',' Expression
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


/* SPARQL Grammar: [57] BrackettedExpression */
BrackettedExpression: '(' Expression ')'
{
  $$=$2;
}
;


/* SPARQL Grammar: [58] PrimaryExpression
 * == BrackettedExpression | BuiltInCall | IRIrefOrFunction | RDFLiteral | NumericLiteral | BooleanLiteral | BlankNode | Var
 * == BrackettedExpression | BuiltInCall | IRIref ArgList? | RDFLiteral | NumericLiteral | BooleanLiteral | BlankNode | Var
 * == BrackettedExpression | BuiltInCall | FunctionCall |
 *    approximately GraphTerm | Var
 * 
*/
PrimaryExpression: BrackettedExpression 
{
  $$=$1;
}
| BuiltInCall
{
  $$=$1;
}
| FunctionCall
{
  /* Grammar has "IRIref ArgList?" here which is confusing
   * shorthand for  FunctionCall | IRIref.  The SPARQL lexer distinguishes
   * these for us with IRIrefBrace.  IRIref is covered below
   * by GraphTerm.
   */
  $$=$1;
}
| GraphTerm
{
  $$=rasqal_new_literal_expression($1);
}
| Var
{
  rasqal_literal *l=rasqal_new_variable_literal($1);
  $$=rasqal_new_literal_expression(l);
}
;


/* SPARQL Grammar: [59] NumericLiteral - merged into GraphTerm */

/* SPARQL Grammar: [60] RDFLiteral - merged into GraphTerm */

/* SPARQL Grammar: [61] BooleanLiteral - merged into GraphTerm */

/* SPARQL Grammar: [62] String - merged into GraphTerm */

/* SPARQL Grammar: [63] IRIref */
IRIref: URI_LITERAL
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
;


/* NEW Grammar Term made from SPARQL Grammar: [63] IRIref + '(' expanded */
IRIrefBrace: URI_LITERAL_BRACE
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
;


/* SPARQL Grammar: [64] QName - made into terminal QNAME_LITERAL */

/* SPARQL Grammar: [65] BlankNode */
BlankNode: BLANK_LITERAL
{
  $$=rasqal_new_simple_literal(RASQAL_LITERAL_BLANK, $1);
} | '[' ']'
{
  const unsigned char *id=rasqal_query_generate_bnodeid((rasqal_query*)rq, NULL);
  $$=rasqal_new_simple_literal(RASQAL_LITERAL_BLANK, id);
}
;

/* SPARQL Grammar: [66] Q_IRI_REF onwards are all lexer items
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


static int
rasqal_sparql_query_engine_iostream_write_escaped_counted_string(rasqal_query* query,
                                                                 raptor_iostream* iostr,
                                                                 const unsigned char* string,
                                                                 size_t len)
{
  const char delim='"';
  
  raptor_iostream_write_byte(iostr, delim);
  if(raptor_iostream_write_string_ntriples(iostr, string, len, delim))
    return 1;
  
  raptor_iostream_write_byte(iostr, delim);

  return 0;
}


static void
rasqal_sparql_query_engine_register_factory(rasqal_query_engine_factory *factory)
{
  factory->context_length = sizeof(rasqal_sparql_query_engine);

  factory->init      = rasqal_sparql_query_engine_init;
  factory->terminate = rasqal_sparql_query_engine_terminate;
  factory->prepare   = rasqal_sparql_query_engine_prepare;
  factory->execute   = rasqal_sparql_query_engine_execute;
  factory->iostream_write_escaped_counted_string = rasqal_sparql_query_engine_iostream_write_escaped_counted_string;
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

  rasqal_query_print(query, DEBUG_FH);

  rasqal_free_query(query);

  raptor_free_uri(base_uri);

  raptor_free_memory(uri_string);

  rasqal_finish();

  return rc;
}
#endif

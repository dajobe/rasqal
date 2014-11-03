/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * sparql_parser.y - Rasqal SPARQL parser over tokens from sparql_lexer.l
 *
 * Copyright (C) 2004-2014, David Beckett http://www.dajobe.org/
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

#define YY_NO_UNISTD_H 1
#include <sparql_lexer.h>

#include <sparql_common.h>


/* Set RASQAL_DEBUG to 3 for super verbose parsing - watching the shift/reduces */
#if 0
#undef RASQAL_DEBUG
#define RASQAL_DEBUG 3
#endif


#define DEBUG_FH stderr

/* Make verbose error messages for syntax errors */
#define YYERROR_VERBOSE 1

/* Fail with an debug error message if RASQAL_DEBUG > 1 */
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
#define YYERROR_MSG(msg) do { fputs("** YYERROR ", DEBUG_FH); fputs(msg, DEBUG_FH); fputc('\n', DEBUG_FH); YYERROR; } while(0)
#else
#define YYERROR_MSG(ignore) YYERROR
#endif
#define YYERR_MSG_GOTO(label,msg) do { errmsg = msg; goto label; } while(0)

/* Slow down the grammar operation and watch it work */
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 2
#undef YYDEBUG
#define YYDEBUG 1
#endif

/* the lexer does not seem to track this */
#undef RASQAL_SPARQL_USE_ERROR_COLUMNS

/* Prototypes */ 
int sparql_parser_error(rasqal_query* rq, void* scanner, const char *msg);

/* Make lex/yacc interface as small as possible */
#undef yylex
#define yylex sparql_lexer_lex

/* Make the yyerror below use the rdf_parser */
#undef yyerror
#define yyerror(rq, scanner, message) sparql_query_error(rq, message)

/* Prototypes for local functions */
static int sparql_parse(rasqal_query* rq);
static void sparql_query_error(rasqal_query* rq, const char *message);
static void sparql_query_error_full(rasqal_query *rq, const char *message, ...) RASQAL_PRINTF_FORMAT(2, 3);


static sparql_uri_applies*
new_uri_applies(raptor_uri* uri, rasqal_update_graph_applies applies) 
{
  sparql_uri_applies* ua;

  ua = RASQAL_MALLOC(sparql_uri_applies*, sizeof(*ua));
  if(!ua)
    return NULL;
  
  ua->uri = uri;
  ua->applies = applies;

  return ua;
}


static void
free_uri_applies(sparql_uri_applies* ua)
{
  if(ua->uri)
    raptor_free_uri(ua->uri);
  RASQAL_FREE(sparql_uri_applies*, ua);
}



static sparql_op_expr*
new_op_expr(rasqal_op op, rasqal_expression *expr)
{
  sparql_op_expr* oe;

  oe = RASQAL_MALLOC(sparql_op_expr*, sizeof(*oe));
  if(!oe)
    return NULL;
  
  oe->op = op;
  oe->expr = expr;

  return oe;
}


static void
free_op_expr(sparql_op_expr* oe)
{
  if(oe->expr)
    rasqal_free_expression(oe->expr);
  RASQAL_FREE(sparql_op_expr*, oe);
}

static void
print_op_expr(sparql_op_expr* oe, FILE* fh)
{
  fputs("<op ", fh);
  fputs(rasqal_expression_op_label(oe->op), fh);
  fputs(", ", fh);
  if(oe->expr)
    rasqal_expression_print(oe->expr, fh);
  else
    fputs("NULL", fh);
  fputs(">", fh);
}



%}


/* directives */

%require "3.0.0"

/* File prefix (bison -b) */
%file-prefix "sparql_parser"

/* Symbol prefix (bison -d : deprecated) */
%name-prefix "sparql_parser_"

/* Write parser header file with macros (bison -d) */
%defines

/* Write output file with verbose descriptions of parser states */
%verbose

/* Generate code processing locations */
 /* %locations */

/* Pure parser - want a reentrant parser  */
%define api.pure full

/* Push or pull parser? */
%define api.push-pull pull

/* Pure parser argument: lexer - yylex() and parser - yyparse() */
%lex-param { yyscan_t yyscanner }
%parse-param { rasqal_query* rq } { void* yyscanner }


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
  rasqal_update_operation *update;
  unsigned int uinteger;
  rasqal_data_graph* data_graph;
  rasqal_row* row;
  rasqal_solution_modifier* modifier;
  int limit_offset[2];
  int integer;
  rasqal_projection* projection;
  rasqal_bindings* bindings;
  sparql_uri_applies* uri_applies;
  sparql_op_expr* op_expr;
}


/*
 * shift/reduce conflicts
 * FIXME: document this
 *  35 total
 *
 *   7 shift/reduce are OPTIONAL/GRAPH/FILTER/SERVICE/MINUS/LET/{
 *      after a TriplesBlockOpt has been accepted but before a
 *      GraphPatternListOpt.  Choice is made to reduce with GraphPatternListOpt.
 * 
 */
%expect 35

/* word symbols */
%token SELECT FROM WHERE
%token OPTIONAL DESCRIBE CONSTRUCT ASK DISTINCT REDUCED LIMIT UNION
%token PREFIX BASE BOUND
%token GRAPH NAMED FILTER OFFSET ORDER BY REGEX ASC DESC LANGMATCHES
%token A "a"
%token STRLANG "strlang"
%token STRDT "strdt"
%token STR "str"
%token IRI "iri"
%token URI "uri"
%token BNODE "bnode"
%token LANG "lang"
%token DATATYPE "datatype"
%token ISURI "isUri"
%token ISBLANK "isBlank"
%token ISLITERAL "isLiteral"
%token ISNUMERIC "isNumeric"
%token SAMETERM "sameTerm"
/* SPARQL 1.1 (draft) / LAQRS */
%token GROUP HAVING
%token COUNT SUM AVG MIN MAX GROUP_CONCAT SAMPLE SEPARATOR
%token DELETE INSERT WITH CLEAR CREATE SILENT DATA DROP LOAD INTO DEFAULT
%token TO ADD MOVE COPY ALL
%token COALESCE
%token AS IF
%token NOT IN
%token BINDINGS UNDEF SERVICE MINUS
%token YEAR MONTH DAY HOURS MINUTES SECONDS TIMEZONE TZ
%token STRLEN SUBSTR UCASE LCASE STRSTARTS STRENDS CONTAINS ENCODE_FOR_URI CONCAT
%token STRBEFORE STRAFTER REPLACE
%token BIND
%token ABS ROUND CEIL FLOOR RAND
%token MD5 SHA1 SHA224 SHA256 SHA384 SHA512
%token UUID STRUUID
%token VALUES
/* LAQRS */
%token EXPLAIN LET
%token CURRENT_DATETIME NOW FROM_UNIXTIME TO_UNIXTIME

/* expression delimiters */

%token ',' '(' ')' '[' ']' '{' '}'
%token '?' '$'

%token HATHAT "^^"

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

/* string */
%token <name> STRING "string"
%token <name> LANGTAG "langtag"

/* literals */
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


%type <seq> ConstructQuery DescribeQuery
%type <seq> VarOrIRIrefList ArgListNoBraces ArgList
%type <seq> ConstructTriples ConstructTriplesOpt
%type <seq> ConstructTemplate OrderConditionList GroupConditionList
%type <seq> HavingConditionList
%type <seq> GraphNodeListNotEmpty SelectExpressionListTail
%type <seq> ModifyTemplateList
%type <seq> IriRefList
%type <seq> ParamsOpt
%type <seq> ExpressionList
%type <seq> DataBlockValueList DataBlockValueListOpt DataBlockRowList DataBlockRowListOpt
%type <seq> DatasetClauseList DatasetClauseListOpt
%type <seq> VarList VarListOpt
%type <seq> GroupClauseOpt HavingClauseOpt OrderClauseOpt
%type <seq> GraphTemplate ModifyTemplate
%type <seq> AdExOpExpressionListInner AdExOpExpressionListOuter AdExOpUnaryExpressionList AdExOpUnaryExpressionListOpt MuExOpUnaryExpressionList

%type <data_graph> DatasetClause DefaultGraphClause NamedGraphClause

%type <update> GraphTriples

%type <formula> TriplesSameSubject
%type <formula> PropertyList PropertyListTailOpt PropertyListNotEmpty
%type <formula> ObjectList ObjectTail Collection
%type <formula> VarOrTerm Verb Object GraphNode TriplesNode
%type <formula> BlankNodePropertyList
%type <formula> TriplesBlock TriplesBlockOpt

%type <graph_pattern> SelectQuery
%type <graph_pattern> GroupGraphPattern SubSelect GroupGraphPatternSub
%type <graph_pattern> GraphGraphPattern OptionalGraphPattern MinusGraphPattern
%type <graph_pattern> GroupOrUnionGraphPattern GroupOrUnionGraphPatternList
%type <graph_pattern> GraphPatternNotTriples
%type <graph_pattern> GraphPatternListOpt GraphPatternList GraphPatternListFilter
%type <graph_pattern> LetGraphPattern Bind ServiceGraphPattern
%type <graph_pattern> WhereClause WhereClauseOpt
%type <graph_pattern> InlineDataGraphPattern

%type <expr> Expression ConditionalOrExpression ConditionalAndExpression
%type <expr> RelationalExpression AdditiveExpression
%type <expr> MultiplicativeExpression UnaryExpression
%type <expr> BuiltInCall RegexExpression FunctionCall StringExpression
%type <expr> DatetimeBuiltinAccessors DatetimeExtensions
%type <expr> BrackettedExpression PrimaryExpression
%type <expr> OrderCondition GroupCondition
%type <expr> Filter Constraint HavingCondition
%type <expr> AggregateExpression CountAggregateExpression SumAggregateExpression
%type <expr> AvgAggregateExpression MinAggregateExpression MaxAggregateExpression
%type <expr> CoalesceExpression GroupConcatAggregateExpression
%type <expr> SampleAggregateExpression ExpressionOrStar

%type <literal> GraphTerm IRIref RDFLiteral BlankNode
%type <literal> VarOrIRIref
%type <literal> IRIrefBrace SourceSelector
%type <literal> NumericLiteral NumericLiteralUnsigned
%type <literal> NumericLiteralPositive NumericLiteralNegative
%type <literal> SeparatorOpt
%type <literal> DataBlockValue

%type <variable> Var VarName VarOrBadVarName SelectTerm AsVarOpt

%type <uri> GraphRef GraphOrDefault OldGraphRef

%type <uinteger> DistinctOpt

%type <row> DataBlockRow

%type <modifier> SolutionModifier

%type <limit_offset> LimitOffsetClausesOpt

%type <integer> LimitClause OffsetClause
%type <integer> SilentOpt 

%type <uri_applies> GraphRefAll

%type <projection> SelectClause SelectExpressionList

%type <bindings> InlineData InlineDataOneVar InlineDataFull DataBlock ValuesClauseOpt

%type <op_expr> AdExOpUnaryExpression MuExOpUnaryExpression


%destructor {
  if($$)
    rasqal_free_literal($$);
}
DOUBLE_LITERAL INTEGER_LITERAL DECIMAL_LITERAL
DOUBLE_POSITIVE_LITERAL DOUBLE_NEGATIVE_LITERAL
INTEGER_POSITIVE_LITERAL INTEGER_NEGATIVE_LITERAL
DECIMAL_POSITIVE_LITERAL DECIMAL_NEGATIVE_LITERAL
BOOLEAN_LITERAL

%destructor {
  if($$)
    raptor_free_uri($$);
}
URI_LITERAL URI_LITERAL_BRACE

%destructor {
  if($$)
    free_uri_applies($$);
} GraphRefAll

%destructor {
  if($$)
    RASQAL_FREE(char*, $$);
}
STRING LANGTAG QNAME_LITERAL QNAME_LITERAL_BRACE BLANK_LITERAL IDENTIFIER

%destructor {
  if($$)
    raptor_free_sequence($$);
}
ConstructQuery DescribeQuery
VarOrIRIrefList ArgListNoBraces ArgList
ConstructTriples ConstructTriplesOpt
ConstructTemplate OrderConditionList GroupConditionList
HavingConditionList
GraphNodeListNotEmpty SelectExpressionListTail
ModifyTemplateList IriRefList ParamsOpt
DataBlockValueList DataBlockValueListOpt DataBlockRowList DataBlockRowListOpt
DatasetClauseList DatasetClauseListOpt
VarList VarListOpt
GroupClauseOpt HavingClauseOpt OrderClauseOpt
GraphTemplate ModifyTemplate
AdExOpExpressionListInner AdExOpExpressionListOuter AdExOpUnaryExpressionList AdExOpUnaryExpressionListOpt MuExOpUnaryExpressionList

%destructor {
  if($$)
    rasqal_free_update_operation($$);
}
GraphTriples 

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
SelectQuery
GroupGraphPattern SubSelect GroupGraphPatternSub
GraphGraphPattern OptionalGraphPattern MinusGraphPattern
GroupOrUnionGraphPattern GroupOrUnionGraphPatternList
GraphPatternNotTriples
GraphPatternListOpt GraphPatternList GraphPatternListFilter
LetGraphPattern Bind ServiceGraphPattern
InlineDataGraphPattern

%destructor {
  if($$)
    rasqal_free_expression($$);
}
Expression ConditionalOrExpression ConditionalAndExpression
RelationalExpression AdditiveExpression
MultiplicativeExpression UnaryExpression
BuiltInCall RegexExpression FunctionCall
DatetimeBuiltinAccessors DatetimeExtensions
BrackettedExpression PrimaryExpression
OrderCondition GroupCondition
Filter Constraint HavingCondition
AggregateExpression CountAggregateExpression SumAggregateExpression
AvgAggregateExpression MinAggregateExpression MaxAggregateExpression
CoalesceExpression GroupConcatAggregateExpression
SampleAggregateExpression ExpressionOrStar

%destructor {
  if($$)
    rasqal_free_literal($$);
}
GraphTerm IRIref RDFLiteral BlankNode DataBlockValue
VarOrIRIref
IRIrefBrace SourceSelector
NumericLiteral NumericLiteralUnsigned
NumericLiteralPositive NumericLiteralNegative
SeparatorOpt

%destructor {
  if($$)
    rasqal_free_variable($$);
}
Var VarName SelectTerm AsVarOpt

%destructor {
  if($$)
    rasqal_free_data_graph($$);
}
DatasetClause DefaultGraphClause NamedGraphClause

%destructor {
  if($$)
    rasqal_free_row($$);
}
DataBlockRow

%destructor {
  if($$)
    rasqal_free_solution_modifier($$);
}
SolutionModifier

%destructor {
  if($$)
    rasqal_free_projection($$);
}
SelectClause SelectExpressionList


%destructor {
  if($$)
    rasqal_free_bindings($$);
}
InlineData InlineDataOneVar InlineDataFull DataBlock ValuesClauseOpt

%destructor {
  if($$)
    free_op_expr($$);
}
AdExOpUnaryExpression MuExOpUnaryExpression



%%

/* Below here, grammar terms are numbered from
 * http://www.w3.org/TR/2010/WD-sparql11-query-20100601/
 * except where noted
 */

/* NEW Grammar Token: parse both Query and Update in one */
Sparql: Query
| Update
;


/* SPARQL Query 1.1 Grammar: Query */
Query: Prologue ExplainOpt ReportFormat ValuesClauseOpt
{
  if($4)
    rq->bindings = $4;
}
;


/* LAQRS */
ExplainOpt: EXPLAIN
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(rq->context);

  if(sparql->experimental)
    rq->explain = 1;
  else {
    sparql_syntax_error(rq,
                        "EXPLAIN can only used with LAQRS");
    YYERROR;
  }
}
|
{
  /* nothing to do */
}
;


/* NEW Grammar Term pulled out of Query */
ReportFormat: SelectQuery
{
  raptor_sequence* seq;
  rasqal_graph_pattern* where_gp;

  /* Query graph pattern is first GP inside sequence of sub-GPs */
  seq = rasqal_graph_pattern_get_sub_graph_pattern_sequence($1);
  where_gp = (rasqal_graph_pattern*)raptor_sequence_delete_at(seq, 0);

  rasqal_query_store_select_query(rq,
                                  $1->projection,
                                  $1->data_graphs,
                                  where_gp,
                                  $1->modifier);
  $1->projection = NULL;
  $1->data_graphs = NULL;
  $1->modifier = NULL;

  rasqal_free_graph_pattern($1);
}
|  ConstructQuery
{
  rq->constructs = $1;
  rq->verb = RASQAL_QUERY_VERB_CONSTRUCT;
}
|  DescribeQuery
{
  rq->describes = $1;
  rq->verb = RASQAL_QUERY_VERB_DESCRIBE;
}
| AskQuery
{
  rq->verb = RASQAL_QUERY_VERB_ASK;
}
;


/* SPARQL Update 1.1 Grammar: Update */
Update: Prologue UpdateOperation UpdateTailOpt
;


UpdateTailOpt: ';' Update
| ';'
| /* empty */
;


UpdateOperation: DeleteQuery
{
  rq->verb = RASQAL_QUERY_VERB_DELETE;
}
| InsertQuery
{
  rq->verb = RASQAL_QUERY_VERB_INSERT;
}
| UpdateQuery
{
  rq->verb = RASQAL_QUERY_VERB_UPDATE;
}
| ClearQuery
{
  rq->verb = RASQAL_QUERY_VERB_UPDATE;
}
| CreateQuery
{
  rq->verb = RASQAL_QUERY_VERB_UPDATE;
}
| DropQuery
{
  rq->verb = RASQAL_QUERY_VERB_UPDATE;
}
| LoadQuery
{
  rq->verb = RASQAL_QUERY_VERB_UPDATE;
}
| AddQuery
{
  rq->verb = RASQAL_QUERY_VERB_UPDATE;
}
| MoveQuery
{
  rq->verb = RASQAL_QUERY_VERB_UPDATE;
}
| CopyQuery
{
  rq->verb = RASQAL_QUERY_VERB_UPDATE;
}
;


/* SPARQL Grammar: Prologue */
Prologue: BaseDeclOpt PrefixDeclListOpt
{
  /* nothing to do */
}
;


/* SPARQL Grammar: BaseDecl */
BaseDeclOpt: BASE URI_LITERAL
{
  rasqal_query_set_base_uri(rq, $2);
  rasqal_evaluation_context_set_base_uri(rq->eval_context, $2);
}
| /* empty */
{
  /* nothing to do */
}
;


/* SPARQL Grammar: PrefixDecl renamed to include optional list */
PrefixDeclListOpt: PrefixDeclListOpt PREFIX IDENTIFIER URI_LITERAL
{
  raptor_sequence *seq = rq->prefixes;
  unsigned const char* prefix_string = $3;
  size_t prefix_length = 0;

  if(prefix_string)
    prefix_length = strlen(RASQAL_GOOD_CAST(const char*, prefix_string));
  
  if(raptor_namespaces_find_namespace(rq->namespaces,
                                      prefix_string, RASQAL_BAD_CAST(int, prefix_length))) {
    /* A prefix may be defined only once */
    sparql_syntax_warning(rq,
                          "PREFIX %s can be defined only once.",
                          prefix_string ? RASQAL_GOOD_CAST(const char*, prefix_string) : ":");
    RASQAL_FREE(char*, prefix_string);
    raptor_free_uri($4);
  } else {
    rasqal_prefix *p;
    p = rasqal_new_prefix(rq->world, prefix_string, $4);
    if(!p)
      YYERROR_MSG("PrefixDeclOpt: failed to create new prefix");
    if(raptor_sequence_push(seq, p))
      YYERROR_MSG("PrefixDeclOpt: cannot push prefix to seq");
    if(rasqal_query_declare_prefix(rq, p)) {
      YYERROR_MSG("PrefixDeclOpt: cannot declare prefix");
    }
  }
}
| /* empty */
{
  /* nothing to do, rq->prefixes already initialised */
}
;


/* SPARQL Grammar: SelectQuery */
SelectQuery: SelectClause DatasetClauseListOpt WhereClause SolutionModifier
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(rq->context);

  $$ = NULL;
  if(!sparql->sparql_scda) {
    sparql_syntax_error(rq,
                        "SELECT can only be used with a SPARQL query");
    YYERROR;
  } else {
    $$ = rasqal_new_select_graph_pattern(rq,
                                         $1, $2, $3, $4, NULL);
  }
}
;

/* SPARQL Grammar: SubSelect */
SubSelect: SelectClause WhereClause SolutionModifier ValuesClauseOpt
{
  if($1 && $2 && $3) {
    $$ = rasqal_new_select_graph_pattern(rq,
                                         $1,
                                         /* data graphs */ NULL,
                                         $2,
                                         $3,
                                         $4);
  } else
    $$ = NULL;
}


/* SPARQL Grammar: SelectClause */
SelectClause: SELECT DISTINCT SelectExpressionList
{
  $$ = $3;
  $$->distinct = 1;
}
| SELECT REDUCED SelectExpressionList
{
  $$ = $3;
  $$->distinct = 2;
}
| SELECT SelectExpressionList
{
  $$ = $2;
}
;


/* NEW Grammar Term pulled out of SelectClause
 * A list of SelectTerm OR a NULL list and a wildcard
 */
SelectExpressionList: SelectExpressionListTail
{
  $$ = rasqal_new_projection(rq, $1, 0, 0);
}
| '*'
{
  $$ = rasqal_new_projection(rq, NULL, /* wildcard */ 1, 0);
}
;


/* NEW Grammar Term pulled out of SelectClause
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
  $$ = raptor_new_sequence((raptor_data_free_handler)rasqal_free_variable,
                           (raptor_data_print_handler)rasqal_variable_print);
  if(!$$)
    YYERROR_MSG("SelectExpressionListTail 3: failed to create sequence");
  if(raptor_sequence_push($$, $1)) {
    raptor_free_sequence($$);
    $$ = NULL;
    YYERROR_MSG("SelectExpressionListTail 3: sequence push failed");
  }
}
;


/* NEW Grammar Term pulled out of SelectClause
 * A variable (?x) or a select expression assigned to a name (x) with AS
 */
SelectTerm: Var
{
  $$ = $1;
}
| '(' Expression AS VarOrBadVarName ')'
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(rq->context);

  $$ = NULL;
  if(!sparql->sparql11_query) {
    sparql_syntax_error(rq,
                        "SELECT ( expression ) AS Variable can only be used with SPARQL 1.1");
    YYERROR;
  } else if($2 && $4) {
    if(rasqal_expression_mentions_variable($2, $4)) {
      sparql_query_error_full(rq,
                              "Expression in SELECT ( expression ) AS %s contains the variable name '%s'",
                              $4->name, $4->name);
      YYERROR;
    } else {
      $$ = $4;
      $$->expression = $2;
    }

  }
}
;


/* SPARQL 1.1 Grammar: [107] Aggregate - renamed for clarity */
/*

Original definition:
  ( 'COUNT' '(' 'DISTINCT'? ( '*' | Expression ) ')' |
    'SUM' ExprAggArg |
    'MIN' ExprAggArg |
    'MAX' ExprAggArg |
    'AVG' ExprAggArg |
    'SAMPLE' ExprAggArg | 
    'GROUP_CONCAT' '(' 'DISTINCT'? Expression ( ',' Expression )* ( ';' 'SEPARATOR' '=' String )? ')'
   )

  where ExprAggArg is '(' 'DISTINCT'? Expression ')'

 */
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
| GroupConcatAggregateExpression
{
  $$ = $1;
}
| SampleAggregateExpression
{
  $$ = $1;
}
;


DistinctOpt: DISTINCT
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(rq->context);

  if(!sparql->sparql11_query) {
    sparql_syntax_error(rq,
                        "functions with DISTINCT can only be used with SPARQL 1.1");
    YYERROR;
  }
  
  $$ = RASQAL_EXPR_FLAG_DISTINCT;
}
| /* empty */
{
  $$ = 0;
}
;


ExpressionOrStar: Expression
{
  $$ = $1;
}
| '*'
{
  $$ = rasqal_new_0op_expression(rq->world,
                                 RASQAL_EXPR_VARSTAR);
}
;


CountAggregateExpression: COUNT '(' DistinctOpt ExpressionOrStar ')'
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(rq->context);

  $$ = NULL;
  if(!sparql->sparql11_query) {
    sparql_syntax_error(rq,
                        "COUNT() can only be used with SPARQL 1.1");
    YYERROR;
  } else {
    $$ = rasqal_new_aggregate_function_expression(rq->world,
                                                  RASQAL_EXPR_COUNT, $4,
                                                  NULL /* params */, $3);
    if(!$$)
      YYERROR_MSG("CountAggregateExpression: cannot create expr");
  }
}
;


SumAggregateExpression: SUM '(' DistinctOpt Expression ')'
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(rq->context);

  $$ = NULL;
  if(!sparql->sparql11_query) {
    sparql_syntax_error(rq,
                        "SUM() can only be used with SPARQL 1.1");
    YYERROR;
  } else {
    $$ = rasqal_new_aggregate_function_expression(rq->world,
                                                  RASQAL_EXPR_SUM, $4,
                                                  NULL /* params */, $3);
    if(!$$)
      YYERROR_MSG("SumAggregateExpression: cannot create expr");
  }
}
;


AvgAggregateExpression: AVG '(' DistinctOpt Expression ')'
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(rq->context);

  $$ = NULL;
  if(!sparql->sparql11_query) {
    sparql_syntax_error(rq,
                        "AVG() can only be used with SPARQL 1.1");
    YYERROR;
  } else {
    $$ = rasqal_new_aggregate_function_expression(rq->world,
                                                  RASQAL_EXPR_AVG, $4,
                                                  NULL /* params */, $3);
    if(!$$)
      YYERROR_MSG("AvgAggregateExpression: cannot create expr");
  }
}
;


MinAggregateExpression: MIN '(' DistinctOpt Expression ')'
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(rq->context);

  $$ = NULL;
  if(!sparql->sparql11_query) {
    sparql_syntax_error(rq,
                        "MIN() can only be used with SPARQL 1.1");
    YYERROR;
  } else {
    $$ = rasqal_new_aggregate_function_expression(rq->world,
                                                  RASQAL_EXPR_MIN, $4,
                                                  NULL /* params */, $3);
    if(!$$)
      YYERROR_MSG("MinAggregateExpression: cannot create expr");
  }
}
;


MaxAggregateExpression: MAX '(' DistinctOpt Expression ')'
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(rq->context);

  $$ = NULL;
  if(!sparql->sparql11_query) {
    sparql_syntax_error(rq,
                        "MAX() can only be used with SPARQL 1.1");
    YYERROR;
  } else {
    $$ = rasqal_new_aggregate_function_expression(rq->world,
                                                  RASQAL_EXPR_MAX, $4,
                                                  NULL /* params */, $3);
    if(!$$)
      YYERROR_MSG("MaxAggregateExpression: cannot create expr");
  }
}
;


SeparatorOpt: ';' SEPARATOR EQ STRING
{
  $$ = rasqal_new_string_literal(rq->world, $4, 
	                         NULL /* language */,
                                 NULL /* dt uri */, NULL /* dt_qname */);
}
| /* empty */
{
  $$ = NULL;
}
;


/* NEW Grammar Term pulled out of [107] Aggregate
 * A comma-separated non-empty list of Expression
 */
ExpressionList: ExpressionList ',' Expression
{
  $$ = $1;
  if(raptor_sequence_push($$, $3)) {
    raptor_free_sequence($$);
    $$ = NULL;
    YYERROR_MSG("ExpressionList 1: sequence push failed");
  }
}
| Expression
{
  $$ = raptor_new_sequence((raptor_data_free_handler)rasqal_free_expression,
                           (raptor_data_print_handler)rasqal_expression_print);
  if(!$$)
    YYERROR_MSG("ExpressionList 2: failed to create sequence");

  if(raptor_sequence_push($$, $1)) {
    raptor_free_sequence($$);
    $$ = NULL;
    YYERROR_MSG("ExpressionList 2: sequence push failed");
  }
}
;


GroupConcatAggregateExpression: GROUP_CONCAT '(' DistinctOpt ExpressionList SeparatorOpt ')'
{
  rasqal_sparql_query_language* sparql;
  
  sparql = (rasqal_sparql_query_language*)(rq->context);

  $$ = NULL;
  if(!sparql->sparql11_query) {
    sparql_syntax_error(rq,
                        "GROUP_CONCAT() can only be used with SPARQL 1.1");
    YYERROR;
  } else {
    int flags = 0;
    
    if($3)
      flags |= RASQAL_EXPR_FLAG_DISTINCT;

    $$ = rasqal_new_group_concat_expression(rq->world,
                                            flags /* flags */,
                                            $4 /* args */,
                                            $5 /* separator */);
    if(!$$)
      YYERROR_MSG("GroupConcatAggregateExpression: cannot create expr");
  }
}
;


SampleAggregateExpression: SAMPLE '(' DistinctOpt Expression ')'
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(rq->context);

  $$ = NULL;
  if(!sparql->sparql11_query) {
    sparql_syntax_error(rq,
                        "SAMPLE() can only be used with SPARQL 1.1");
    YYERROR;
  } else {
    $$ = rasqal_new_aggregate_function_expression(rq->world,
                                                  RASQAL_EXPR_SAMPLE, $4,
                                                  NULL /* params */, $3);
    if(!$$)
      YYERROR_MSG("SampleAggregateExpression: cannot create expr");
  }
}
;


/* SPARQL Grammar: ConstructQuery */
ConstructQuery: CONSTRUCT ConstructTemplate
        DatasetClauseListOpt WhereClause SolutionModifier
{
  rasqal_sparql_query_language* sparql;
  
  sparql = (rasqal_sparql_query_language*)(rq->context);

  $$ = NULL;
  if(!sparql->sparql_scda) {
    sparql_syntax_error(rq,
                        "CONSTRUCT can only be used with a SPARQL query");
    YYERROR;
  }
  
  $$ = $2;

  if($3)
    rasqal_query_add_data_graphs(rq, $3);
  rq->query_graph_pattern = $4;

  if($5)
    rq->modifier = $5;
}
| CONSTRUCT DatasetClauseListOpt WHERE '{' ConstructTriples '}' SolutionModifier
{
  rasqal_sparql_query_language* sparql;
  rasqal_graph_pattern* where_gp;
  raptor_sequence* seq = NULL;

  sparql = (rasqal_sparql_query_language*)(rq->context);

  if(!sparql->sparql_scda) {
    sparql_syntax_error(rq,
                        "CONSTRUCT can only be used with a SPARQL query");
    YYERROR;
  }

  if($5) {
    int i;
    int size = raptor_sequence_size($5);
    
    seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_triple,
                              (raptor_data_print_handler)rasqal_triple_print);
    for(i = 0; i < size; i++) {
      rasqal_triple* t = (rasqal_triple*)raptor_sequence_get_at($5, i);
      t = rasqal_new_triple_from_triple(t);
      raptor_sequence_push(seq, t);
    }
  }
  
  where_gp = rasqal_new_basic_graph_pattern_from_triples(rq, seq);
  seq = NULL;
  if(!where_gp)
    YYERROR_MSG("ConstructQuery: cannot create graph pattern");

  $$ = $5;

  if($2)
    rasqal_query_add_data_graphs(rq, $2);
  rq->query_graph_pattern = where_gp;

  if($7)
    rq->modifier = $7;
}
;


/* SPARQL Grammar: DescribeQuery */
DescribeQuery: DESCRIBE VarOrIRIrefList
        DatasetClauseListOpt WhereClauseOpt SolutionModifier
{
  rasqal_sparql_query_language* sparql;
  
  sparql = (rasqal_sparql_query_language*)(rq->context);

  $$ = NULL;
  if(!sparql->sparql_scda) {
    sparql_syntax_error(rq,
                        "DESCRIBE can only be used with a SPARQL query");
    YYERROR;
  }
  
  $$ = $2;

  if($3)
    rasqal_query_add_data_graphs(rq, $3);

  rq->query_graph_pattern = $4;

  if($5)
    rq->modifier = $5;
}
| DESCRIBE '*'
        DatasetClauseListOpt WhereClauseOpt SolutionModifier
{
  $$ = NULL;

  if($3)
    rasqal_query_add_data_graphs(rq, $3);

  rq->query_graph_pattern = $4;

  if($5)
    rq->modifier = $5;
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
  $$ = raptor_new_sequence((raptor_data_free_handler)rasqal_free_literal,
                           (raptor_data_print_handler)rasqal_literal_print);
  if(!$$)
    YYERROR_MSG("VarOrIRIrefList 3: cannot create seq");
  if(raptor_sequence_push($$, $1)) {
    raptor_free_sequence($$);
    $$ = NULL;
    YYERROR_MSG("VarOrIRIrefList 3: sequence push failed");
  }
}
;


/* SPARQL Grammar: AskQuery */
AskQuery: ASK 
        DatasetClauseListOpt WhereClause
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(rq->context);

  if(!sparql->sparql_scda) {
    sparql_syntax_error(rq,
                        "ASK can only be used with a SPARQL query");
    YYERROR;
  }
  
  if($2)
    rasqal_query_add_data_graphs(rq, $2);

  rq->query_graph_pattern = $3;
}
;


/* SPARQL Grammar: DatasetClause */
DatasetClause: FROM DefaultGraphClause
{
  $$ = $2;
}
| FROM NamedGraphClause
{
  $$ = $2;
}
;


/* SPARQL 1.1 Update (draft) */
GraphRef: GRAPH URI_LITERAL
{
  $$ = $2;
}
;


/* LAQRS */
DeleteQuery: DELETE DatasetClauseList WhereClauseOpt
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(rq->context);

  if(!sparql->sparql11_update) {
    sparql_syntax_error(rq,
                        "DELETE can only be used with a SPARQL 1.1 Update");
    YYERROR;
  }
  
  /* LAQRS: experimental syntax */
  sparql_syntax_warning(rq,
                        "DELETE FROM <uri> ... WHERE ... is deprecated LAQRS syntax.");

  if($2)
    rasqal_query_add_data_graphs(rq, $2);

  rq->query_graph_pattern = $3;
}
| DELETE '{' ModifyTemplateList '}' WhereClause
{
  rasqal_sparql_query_language* sparql;
  rasqal_update_operation* update;

  sparql = (rasqal_sparql_query_language*)(rq->context);

  if(!sparql->sparql11_update) {
    sparql_syntax_error(rq,
                        "DELETE can only be used with a SPARQL 1.1 Update");
    YYERROR;
  }

  /* SPARQL 1.1 (Draft) update:
   * deleting via template + query - not inline atomic triples 
   */

  update = rasqal_new_update_operation(RASQAL_UPDATE_TYPE_UPDATE,
                                       NULL /* graph_uri */,
                                       NULL /* document_uri */,
                                       NULL /* insert templates */,
                                       $3 /* delete templates */,
                                       $5 /* where */,
                                       0 /* flags */,
                                       RASQAL_UPDATE_GRAPH_ONE /* applies */);
  if(!update) {
    YYERROR_MSG("DeleteQuery: rasqal_new_update_operation failed");
  } else {
    if(rasqal_query_add_update_operation(rq, update))
      YYERROR_MSG("DeleteQuery: rasqal_query_add_update_operation failed");
  }
}
| DELETE DATA '{' GraphTriples '}'
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(rq->context);

  if(!sparql->sparql11_update) {
    sparql_syntax_error(rq,
                        "DELETE can only be used with a SPARQL 1.1 Update");
    YYERROR;
  }
  
  /* SPARQL 1.1 (Draft) update:
   * deleting inline triples - not inserting from graph URIs 
   */
  $4->type = RASQAL_UPDATE_TYPE_UPDATE;
  $4->delete_templates = $4->insert_templates; $4->insert_templates = NULL;
  $4->flags |= RASQAL_UPDATE_FLAGS_DATA;
  
  rasqal_query_add_update_operation(rq, $4);
}
| DELETE WHERE GroupGraphPattern
{
  rasqal_sparql_query_language* sparql;
  rasqal_update_operation* update;
  raptor_sequence* delete_templates = NULL;
  
  sparql = (rasqal_sparql_query_language*)(rq->context);

  if(!sparql->sparql11_update) {
    sparql_syntax_error(rq,
                        "DELETE WHERE { } can only be used with a SPARQL 1.1 Update");
    YYERROR;
  }

  /* SPARQL 1.1 (Draft) update:
   * deleting via template - not inline atomic triples 
   */

  /* Turn GP into flattened triples */
  if($3) {
    delete_templates = rasqal_graph_pattern_get_flattened_triples(rq, $3);
    rasqal_free_graph_pattern($3);
    $3 = NULL;
  }

  update = rasqal_new_update_operation(RASQAL_UPDATE_TYPE_UPDATE,
                                       NULL /* graph_uri */,
                                       NULL /* document_uri */,
                                       NULL /* insert templates */,
                                       delete_templates /* delete templates */,
                                       NULL /* where */,
                                       0 /* flags */,
                                       RASQAL_UPDATE_GRAPH_ONE /* applies */);
  if(!update) {
    YYERROR_MSG("DeleteQuery: rasqal_new_update_operation failed");
  } else {
    if(rasqal_query_add_update_operation(rq, update))
      YYERROR_MSG("DeleteQuery: rasqal_query_add_update_operation failed");
  }
}
;


/* SPARQL 1.1 Update (draft) */
GraphTriples: TriplesBlock
{
  $$ = NULL;
 
  if($1) {
    $$ = rasqal_new_update_operation(RASQAL_UPDATE_TYPE_UNKNOWN,
                                     NULL /* graph_uri */,
                                     NULL /* document_uri */,
                                     $1->triples /* insert templates */, 
                                     NULL /* delete templates */,
                                     NULL /* where */,
                                     0 /* flags */,
                                     RASQAL_UPDATE_GRAPH_ONE /* applies */);
    $1->triples = NULL;
    rasqal_free_formula($1);
  }
}
| GRAPH URI_LITERAL '{' TriplesBlock '}'
{
  $$ = NULL;

  if($4) {
    raptor_sequence* seq;
    seq = $4->triples;

    if($2) {
      rasqal_literal* origin_literal;
      
      origin_literal = rasqal_new_uri_literal(rq->world, $2);
      $2 = NULL;

      rasqal_triples_sequence_set_origin(/* dest */ NULL, seq, origin_literal);
      rasqal_free_literal(origin_literal);
    }
    $$ = rasqal_new_update_operation(RASQAL_UPDATE_TYPE_UNKNOWN,
                                     NULL /* graph uri */,
                                     NULL /* document uri */,
                                     seq /* insert templates */,
                                     NULL /* delete templates */,
                                     NULL /* where */,
                                     0 /* flags */,
                                     RASQAL_UPDATE_GRAPH_ONE /* applies */);
    $4->triples = NULL;
    rasqal_free_formula($4);
  }
}
;


/* SPARQL 1.1 Update (draft) SS 4.1.3 */
GraphTemplate: GRAPH VarOrIRIref '{' ConstructTriples '}'
{
  $$ = $4;

  if($2) {
    rasqal_triples_sequence_set_origin(NULL, $$, $2);
    rasqal_free_literal($2);
    $2 = NULL;
  }
}
;



/* SPARQL 1.1 Update (draft) SS 4.1.3 */
ModifyTemplate: ConstructTriples
{
  $$ = $1;
}
| GraphTemplate
{
  $$ = $1;
}
;


/* SPARQL 1.1 Update (draft) */
ModifyTemplateList: ModifyTemplateList ModifyTemplate
{
  $$ = $1;

  if($2) {
    if(raptor_sequence_join($$, $2)) {
      raptor_free_sequence($2);
      raptor_free_sequence($$);
      $$ = NULL;
      YYERROR_MSG("ModifyTemplateList: sequence join failed");
    }
    raptor_free_sequence($2);
  }

}
| ModifyTemplate
{
  $$ = $1;
}
;



/* SPARQL 1.1 Update (draft) / LAQRS */
InsertQuery: INSERT DatasetClauseList WhereClauseOpt
{
  rasqal_sparql_query_language* sparql;
  sparql  = (rasqal_sparql_query_language*)(rq->context);

  if(!sparql->sparql11_update) {
    sparql_syntax_error(rq,
                        "INSERT can only be used with a SPARQL 1.1 Update");
    YYERROR;
  }

  /* LAQRS: experimental syntax */
  sparql_syntax_warning(rq,
                        "INSERT FROM <uri> ... WHERE ... is deprecated LAQRS syntax.");

  if($2)
    rasqal_query_add_data_graphs(rq, $2);

  rq->query_graph_pattern = $3;
}
| INSERT '{' ModifyTemplateList '}' WhereClauseOpt
{
  rasqal_sparql_query_language* sparql;
  rasqal_update_operation* update;

  sparql = (rasqal_sparql_query_language*)(rq->context);

  if(!sparql->sparql11_update) {
    sparql_syntax_error(rq,
                        "INSERT can only be used with a SPARQL 1.1 Update");
    YYERROR;
  }
  
  /* inserting via template + query - not inline atomic triples */

  update = rasqal_new_update_operation(RASQAL_UPDATE_TYPE_UPDATE,
                                       NULL /* graph_uri */,
                                       NULL /* document_uri */,
                                       $3 /* insert templates */,
                                       NULL /* delete templates */,
                                       $5 /* where */,
                                       0 /* flags */,
                                       RASQAL_UPDATE_GRAPH_ONE /* applies */);
  if(!update) {
    YYERROR_MSG("InsertQuery: rasqal_new_update_operation failed");
  } else {
    if(rasqal_query_add_update_operation(rq, update))
      YYERROR_MSG("InsertQuery: rasqal_query_add_update_operation failed");
  }
}
| INSERT DATA '{' GraphTriples '}'
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(rq->context);

  if(!sparql->sparql11_update) {
    sparql_syntax_error(rq,
                        "INSERT DATA can only be used with a SPARQL 1.1 Update");
    YYERROR;
  }
  
  /* inserting inline atomic triples (no variables) - not via template */
  $4->type = RASQAL_UPDATE_TYPE_UPDATE;
  $4->flags |= RASQAL_UPDATE_FLAGS_DATA;

  rasqal_query_add_update_operation(rq, $4);
}
;


UpdateQuery: WITH URI_LITERAL 
  DELETE '{' ModifyTemplateList '}' 
  INSERT '{' ModifyTemplateList '}'
  WhereClauseOpt
{
  rasqal_sparql_query_language* sparql;
  rasqal_update_operation* update;

  sparql = (rasqal_sparql_query_language*)(rq->context);

  if(!sparql->sparql11_update) {
    sparql_syntax_error(rq,
                        "WITH can only be used with a SPARQL 1.1 Update");
    YYERROR;
  }
  
  if($2) {
    rasqal_literal* origin_literal;

    origin_literal = rasqal_new_uri_literal(rq->world, $2);
    $2 = NULL;

    rasqal_triples_sequence_set_origin(/* dest */ NULL, $9, origin_literal);
    rasqal_triples_sequence_set_origin(/* dest */ NULL, $5, origin_literal);

    rasqal_free_literal(origin_literal);
  }

  /* after this $5, $9 and $12 are owned by update */
  update = rasqal_new_update_operation(RASQAL_UPDATE_TYPE_UPDATE,
                                       NULL /* graph uri */, 
                                       NULL /* document uri */,
                                       $9 /* insert templates */,
                                       $5 /* delete templates */,
                                       $11 /* where */,
                                       0 /* flags */,
                                       RASQAL_UPDATE_GRAPH_ONE /* applies */);
  if(!update) {
    YYERROR_MSG("UpdateQuery 1: rasqal_new_update_operation failed");
  } else {
    if(rasqal_query_add_update_operation(rq, update))
      YYERROR_MSG("UpdateQuery 1: rasqal_query_add_update_operation failed");
  }
}
| WITH URI_LITERAL 
  DELETE '{' ModifyTemplateList '}' 
  WhereClauseOpt
{
  rasqal_sparql_query_language* sparql;
  rasqal_update_operation* update;

  sparql = (rasqal_sparql_query_language*)(rq->context);

  if(!sparql->sparql11_update) {
    sparql_syntax_error(rq,
                        "WITH can only be used with a SPARQL 1.1 Update");
    YYERROR;
  }
  
  if($2) {
    rasqal_literal* origin_literal;
    
    origin_literal = rasqal_new_uri_literal(rq->world, $2);
    $2 = NULL;

    rasqal_triples_sequence_set_origin(/* dest */ NULL, $5, origin_literal);

    rasqal_free_literal(origin_literal);
  }
  
  /* after this $5 and $7 are owned by update */
  update = rasqal_new_update_operation(RASQAL_UPDATE_TYPE_UPDATE,
                                       NULL /* graph uri */, 
                                       NULL /* document uri */,
                                       NULL /* insert templates */,
                                       $5 /* delete templates */,
                                       $7 /* where */,
                                       0 /* flags */,
                                       RASQAL_UPDATE_GRAPH_ONE /* applies */);
  if(!update) {
    YYERROR_MSG("UpdateQuery 2: rasqal_new_update_operation failed");
  } else {
    if(rasqal_query_add_update_operation(rq, update))
      YYERROR_MSG("UpdateQuery 2: rasqal_query_add_update_operation failed");
  }
}
| WITH URI_LITERAL 
  INSERT '{' ModifyTemplateList '}' 
  WhereClauseOpt
{
  rasqal_sparql_query_language* sparql;
  rasqal_update_operation* update;

  sparql = (rasqal_sparql_query_language*)(rq->context);

  if(!sparql->sparql11_update) {
    sparql_syntax_error(rq,
                        "WITH can only be used with a SPARQL 1.1 Update");
    YYERROR;
  }

  if($2) {
    rasqal_literal* origin_literal;
    
    origin_literal = rasqal_new_uri_literal(rq->world, $2);
    $2 = NULL;

    rasqal_triples_sequence_set_origin(/* dest */ NULL, $5, origin_literal);

    rasqal_free_literal(origin_literal);
  }

  /* after this $5 and $7 are owned by update */
  update = rasqal_new_update_operation(RASQAL_UPDATE_TYPE_UPDATE,
                                       NULL /* graph uri */, 
                                       NULL /* document uri */,
                                       $5 /* insert templates */,
                                       NULL /* delete templates */,
                                       $7 /* where */,
                                       0 /* flags */,
                                       RASQAL_UPDATE_GRAPH_ONE /* applies */);
  if(!update) {
    YYERROR_MSG("UpdateQuery 3: rasqal_new_update_operation failed");
  } else {
    if(rasqal_query_add_update_operation(rq, update))
      YYERROR_MSG("UpdateQuery 3: rasqal_query_add_update_operation failed");
  }
}
| WITH URI_LITERAL 
  INSERT DATA '{' GraphTriples '}'
{
  rasqal_sparql_query_language* sparql;

  sparql = (rasqal_sparql_query_language*)(rq->context);

  if(!sparql->sparql11_update) {
    sparql_syntax_error(rq,
                        "WITH can only be used with a SPARQL 1.1 Update");
    YYERROR;
  }

  /* inserting inline atomic triples (no variables) - not via template */
  $6->graph_uri = $2; /* graph uri */
  $6->type = RASQAL_UPDATE_TYPE_UPDATE;
  $6->flags |= RASQAL_UPDATE_FLAGS_DATA;

  rasqal_query_add_update_operation(rq, $6);
}
;


/* SPARQL 1.1 Update token */
GraphRefAll: GraphRef
{
  $$ = new_uri_applies($1, RASQAL_UPDATE_GRAPH_ONE);
}
| DEFAULT
{
  $$ = new_uri_applies(NULL, RASQAL_UPDATE_GRAPH_DEFAULT);
}
| NAMED
{
  $$ = new_uri_applies(NULL, RASQAL_UPDATE_GRAPH_NAMED);
}
| ALL
{
  $$ = new_uri_applies(NULL, RASQAL_UPDATE_GRAPH_ALL);
}
| GRAPH DEFAULT
{
  /* Early draft syntax - deprecated */
  sparql_syntax_warning(rq,
                        "CLEAR GRAPH DEFAULT is replaced by CLEAR DEFAULT in later SPARQL 1.1 drafts");


  $$ = new_uri_applies(NULL, RASQAL_UPDATE_GRAPH_DEFAULT);
}
;


/* SPARQL 1.1 Update (draft) / LAQRS */
ClearQuery: CLEAR SilentOpt GraphRefAll
{
  rasqal_sparql_query_language* sparql;
  rasqal_update_operation* update;

  sparql = (rasqal_sparql_query_language*)(rq->context);

  if(!sparql->sparql11_update) {
    sparql_syntax_error(rq,
                        "CLEAR (SILENT) DEFAULT | NAMED | ALL can only be used with a SPARQL 1.1 Update");
    YYERROR;
  }

  if($3) {
    update = rasqal_new_update_operation(RASQAL_UPDATE_TYPE_CLEAR,
                                         $3->uri ? raptor_uri_copy($3->uri) : NULL /* graph uri or NULL */,
                                         NULL /* document uri */,
                                         NULL, NULL,
                                         NULL /*where */,
                                         $2 /* flags */,
                                         $3->applies /* applies */);
    free_uri_applies($3);
    $3 = NULL;

    if(!update) {
      YYERROR_MSG("ClearQuery: rasqal_new_update_operation failed");
    } else {
      if(rasqal_query_add_update_operation(rq, update))
        YYERROR_MSG("ClearQuery: rasqal_query_add_update_operation failed");
    }
  }
}
| CLEAR
{
  rasqal_sparql_query_language* sparql;
  rasqal_update_operation* update;

  sparql = (rasqal_sparql_query_language*)(rq->context);

  if(!sparql->sparql11_update) {
    sparql_syntax_error(rq,
                        "CLEAR can only be used with a SPARQL 1.1 Update");
    YYERROR;
  }

  /* Early draft syntax - deprecated */
  sparql_syntax_warning(rq,
                        "CLEAR is replaced by CLEAR DEFAULT in later SPARQL 1.1 drafts");

  update = rasqal_new_update_operation(RASQAL_UPDATE_TYPE_CLEAR,
                                       NULL /* graph uri */, 
                                       NULL /* document uri */,
                                       NULL, NULL,
                                       NULL /* where */,
                                       0 /* flags */,
                                       RASQAL_UPDATE_GRAPH_ONE /* applies */);
  if(!update) {
    YYERROR_MSG("ClearQuery: rasqal_new_update_operation failed");
  } else {
    if(rasqal_query_add_update_operation(rq, update))
      YYERROR_MSG("ClearQuery: rasqal_query_add_update_operation failed");
  }
}
;


/* Optional SILENT flag pulled out of SPARQL 1.1 Load ... Copy */
SilentOpt: SILENT
{
  $$ = RASQAL_UPDATE_FLAGS_SILENT;
}
| /* empty */
{
  $$ = 0;
}
;


/* SPARQL 1.1 Update (draft) / LAQRS */
CreateQuery: CREATE SilentOpt URI_LITERAL
{
  rasqal_sparql_query_language* sparql;
  rasqal_update_operation* update;

  sparql = (rasqal_sparql_query_language*)(rq->context);

  if(!sparql->sparql11_update) {
    sparql_syntax_error(rq,
                        "CREATE (SILENT) <uri> can only be used with a SPARQL 1.1 Update");
    YYERROR;
  }

  update = rasqal_new_update_operation(RASQAL_UPDATE_TYPE_CREATE,
                                       $3 /* graph uri */, 
                                       NULL /* document uri */,
                                       NULL, NULL,
                                       NULL /*where */,
                                       $2 /* flags */,
                                       RASQAL_UPDATE_GRAPH_ONE /* applies */);
  if(!update) {
    YYERROR_MSG("CreateQuery: rasqal_new_update_operation failed");
  } else {
    if(rasqal_query_add_update_operation(rq, update))
      YYERROR_MSG("CreateQuery: rasqal_query_add_update_operation failed");
  }
}
| CREATE SilentOpt GraphRef
{
  rasqal_sparql_query_language* sparql;
  rasqal_update_operation* update;

  sparql = (rasqal_sparql_query_language*)(rq->context);

  if(!sparql->sparql11_update) {
    sparql_syntax_error(rq,
                        "CREATE (SILENT) GRAPH <uri> can only be used with a SPARQL 1.1 Update");
    YYERROR;
  }

  /* Early draft syntax - deprecated */
  sparql_syntax_warning(rq,
                        "CREATE (SILENT) GRAPH <uri> is replaced by CREATE (SILENT) <uri> in later SPARQL 1.1 drafts");

  update = rasqal_new_update_operation(RASQAL_UPDATE_TYPE_CREATE,
                                       $3 /* graph uri */, 
                                       NULL /* document uri */,
                                       NULL, NULL,
                                       NULL /*where */,
                                       RASQAL_UPDATE_FLAGS_SILENT /* flags */,
                                       RASQAL_UPDATE_GRAPH_ONE /* applies */);
  if(!update) {
    YYERROR_MSG("CreateQuery: rasqal_new_update_operation failed");
  } else {
    if(rasqal_query_add_update_operation(rq, update))
      YYERROR_MSG("CreateQuery: rasqal_query_add_update_operation failed");
  }
}
;


/* SPARQL 1.1 Update (draft) / LAQRS */
DropQuery: DROP SilentOpt GraphRefAll
{
  rasqal_sparql_query_language* sparql;
  rasqal_update_operation* update;
  
  sparql = (rasqal_sparql_query_language*)(rq->context);

  if(!sparql->sparql11_update) {
    sparql_syntax_error(rq,
                        "DROP (SILENT) DEFAULT | NAMED | ALL can only be used with a SPARQL 1.1 Update");
    YYERROR;
  }

  if($3) {
    update = rasqal_new_update_operation(RASQAL_UPDATE_TYPE_DROP,
                                         $3->uri ? raptor_uri_copy($3->uri) : NULL /* graph uri or NULL */,
                                         NULL /* document uri */,
                                         NULL, NULL,
                                         NULL /*where */,
                                         $2 /* flags */,
                                         $3->applies /* applies */);
    free_uri_applies($3);
    $3 = NULL;

    if(!update) {
      YYERROR_MSG("DropQuery: rasqal_new_update_operation failed");
    } else {
      if(rasqal_query_add_update_operation(rq, update))
        YYERROR_MSG("DropQuery: rasqal_query_add_update_operation failed");
    }
  }
}
;


/* NEW Grammar Term pulled out of SPARQL 1.1 draft [x] Load */
IriRefList: IriRefList URI_LITERAL
{
  $$ = $1;
  if(raptor_sequence_push($$, $2)) {
    raptor_free_sequence($$);
    $$ = NULL;
    YYERROR_MSG("IriRefList 1: sequence push failed");
  }
}
| URI_LITERAL
{
  $$ = raptor_new_sequence((raptor_data_free_handler)raptor_free_uri,
                           (raptor_data_print_handler)raptor_uri_print);
  if(!$$) {
    if($1)
      raptor_free_uri($1);
    YYERROR_MSG("IriRefList 2: cannot create sequence");
  }
  if(raptor_sequence_push($$, $1)) {
    raptor_free_sequence($$);
    $$ = NULL;
    YYERROR_MSG("IriRefList 2: sequence push failed");
  }
}
;

 
/* SPARQL 1.1 token GraphOrDefault */
GraphOrDefault: DEFAULT
{
  $$ = NULL;
}
| URI_LITERAL
{
  $$ = $1;
}
;


OldGraphRef: GraphRef 
{
  $$ = $1;
}
| URI_LITERAL
{
  /* Early draft syntax allowed a list of URIs - deprecated */
  sparql_syntax_warning(rq,
                        "LOAD <document uri list> INTO <graph uri> is replaced by LOAD <document uri> INTO GRAPH <graph uri> in later SPARQL 1.1 drafts");

  $$ = $1;
}
| DEFAULT
{
  /* Early draft syntax allowed a list of URIs - deprecated */
  sparql_syntax_warning(rq,
                        "LOAD <document uri list> INTO DEFAULT is replaced by LOAD <document uri> in later SPARQL 1.1 drafts");

  $$ = NULL;
}
;


/* SPARQL 1.1 Update (draft) */
LoadQuery: LOAD SilentOpt URI_LITERAL
{
  rasqal_sparql_query_language* sparql;
  rasqal_update_operation* update;
  
  sparql = (rasqal_sparql_query_language*)(rq->context);

  if(!sparql->sparql11_update) {
    sparql_syntax_error(rq,
                        "LOAD <uri> can only be used with a SPARQL 1.1 Update");
    YYERROR;
  }

  update = rasqal_new_update_operation(RASQAL_UPDATE_TYPE_LOAD,
                                       NULL /* graph uri */, 
                                       $3 /* document uri */,
                                       NULL, NULL,
                                       NULL /* where */,
                                       $2 /* flags */,
                                       RASQAL_UPDATE_GRAPH_ONE /* applies */);
  if(!update) {
    YYERROR_MSG("LoadQuery: rasqal_new_update_operation failed");
  } else {
    if(rasqal_query_add_update_operation(rq, update))
      YYERROR_MSG("LoadQuery: rasqal_query_add_update_operation failed");
  }
}
| LOAD SilentOpt IriRefList INTO OldGraphRef
{
  rasqal_sparql_query_language* sparql;
  int i;
  raptor_uri* doc_uri;

  sparql = (rasqal_sparql_query_language*)(rq->context);

  if(!sparql->sparql11_update) {
    sparql_syntax_error(rq,
                        "LOAD <document uri> INTO GRAPH <graph URI> / DEFAULT can only be used with a SPARQL 1.1 Update");
    YYERROR;
  }

  for(i = 0; (doc_uri = (raptor_uri*)raptor_sequence_get_at($3, i)); i++) {
    rasqal_update_operation* update;
    update = rasqal_new_update_operation(RASQAL_UPDATE_TYPE_LOAD,
                                         $5 ? raptor_uri_copy($5) : NULL /* graph uri */,
                                         raptor_uri_copy(doc_uri) /* document uri */,
                                         NULL, NULL,
                                         NULL /*where */,
                                         $2 /* flags */,
                                         RASQAL_UPDATE_GRAPH_ONE /* applies */);
    if(!update) {
      YYERROR_MSG("LoadQuery: rasqal_new_update_operation failed");
    } else {
      if(rasqal_query_add_update_operation(rq, update))
        YYERROR_MSG("LoadQuery: rasqal_query_add_update_operation failed");
    }

    if(i == 1)
      /* Early draft syntax allowed a list of URIs - deprecated */
      sparql_syntax_warning(rq,
                            "LOAD <document uri list> INTO <graph uri> / DEFAULT is replaced by LOAD <document uri> INTO GRAPH <graph uri> or LOAD <document uri> in later SPARQL 1.1 drafts");
    

  }

  raptor_free_sequence($3);
  if($5)
    raptor_free_uri($5);
}
;


/* SPARQL 1.1 Update (draft) */
AddQuery: ADD SilentOpt GraphOrDefault TO GraphOrDefault
{
  rasqal_sparql_query_language* sparql;
  rasqal_update_operation* update;

  sparql = (rasqal_sparql_query_language*)(rq->context);

  if(!sparql->sparql11_update) {
    sparql_syntax_error(rq,
                        "ADD (SILENT) <uri> TO <uri> can only be used with a SPARQL 1.1 Update");
    YYERROR;
  }

  update = rasqal_new_update_operation(RASQAL_UPDATE_TYPE_ADD,
                                       $3 /* graph uri or NULL */, 
                                       $5 /* document uri */,
                                       NULL, NULL,
                                       NULL /*where */,
                                       $2 /* flags */,
                                       RASQAL_UPDATE_GRAPH_ONE /* applies */);
  if(!update) {
    YYERROR_MSG("AddQuery: rasqal_new_update_operation failed");
  } else {
    if(rasqal_query_add_update_operation(rq, update))
      YYERROR_MSG("AddQuery: rasqal_query_add_update_operation failed");
  }
}
;


/* SPARQL 1.1 Update (draft) */
MoveQuery: MOVE SilentOpt GraphOrDefault TO GraphOrDefault
{
  rasqal_sparql_query_language* sparql;
  rasqal_update_operation* update;

  sparql = (rasqal_sparql_query_language*)(rq->context);

  if(!sparql->sparql11_update) {
    sparql_syntax_error(rq,
                        "MOVE (SILENT) <uri> TO <uri> can only be used with a SPARQL 1.1 Update");
    YYERROR;
  }

  update = rasqal_new_update_operation(RASQAL_UPDATE_TYPE_MOVE,
                                       $3 /* graph uri or NULL */, 
                                       $5 /* document uri */,
                                       NULL, NULL,
                                       NULL /*where */,
                                       $2 /* flags */,
                                       RASQAL_UPDATE_GRAPH_ONE /* applies */);
  if(!update) {
    YYERROR_MSG("MoveQuery: rasqal_new_update_operation failed");
  } else {
    if(rasqal_query_add_update_operation(rq, update))
      YYERROR_MSG("MoveQuery: rasqal_query_add_update_operation failed");
  }
}
;


/* SPARQL 1.1 Update (draft) */
CopyQuery: COPY SilentOpt GraphOrDefault TO GraphOrDefault
{
  rasqal_sparql_query_language* sparql;
  rasqal_update_operation* update;

  sparql = (rasqal_sparql_query_language*)(rq->context);

  if(!sparql->sparql11_update) {
    sparql_syntax_error(rq,
                        "COPY (SILENT) <uri> TO <uri> can only be used with a SPARQL 1.1 Update");
    YYERROR;
  }

  update = rasqal_new_update_operation(RASQAL_UPDATE_TYPE_COPY,
                                       $3 /* graph uri or NULL */, 
                                       $5 /* document uri */,
                                       NULL, NULL,
                                       NULL /*where */,
                                       $2 /* flags */,
                                       RASQAL_UPDATE_GRAPH_ONE /* applies */);
  if(!update) {
    YYERROR_MSG("CopyQuery: rasqal_new_update_operation failed");
  } else {
    if(rasqal_query_add_update_operation(rq, update))
      YYERROR_MSG("CopyQuery: rasqal_query_add_update_operation failed");
  }
}
;


/* SPARQL Grammar: DatasetClause */
DatasetClauseList: DatasetClauseList DatasetClause
{
  $$ = $1;
  if($1 && $2)
    raptor_sequence_push($1, $2);
}
| DatasetClause
{
  $$ = raptor_new_sequence((raptor_data_free_handler)rasqal_free_data_graph, (raptor_data_print_handler)rasqal_data_graph_print);
  if($$ && $1)
    raptor_sequence_push($$, $1);
}
;


DatasetClauseListOpt: DatasetClauseList
{
  $$ = $1;
}
| /* empty */
{
  $$ = NULL;
}
;


/* SPARQL Grammar: DefaultGraphClause */
DefaultGraphClause: SourceSelector
{
  if($1) {
    raptor_uri* uri = rasqal_literal_as_uri($1);
    rasqal_data_graph* dg;

    dg = rasqal_new_data_graph_from_uri(rq->world, uri,
                                        NULL, RASQAL_DATA_GRAPH_BACKGROUND,
                                        NULL, NULL, NULL);

    if(!dg) {
      rasqal_free_literal($1);
      YYERROR_MSG("DefaultGraphClause: rasqal_query_new_data_graph_from_uri() failed");
    }
    rasqal_free_literal($1);

    $$ = dg;
  } else
    $$ = NULL;
}
;  


/* SPARQL Grammar: NamedGraphClause */
NamedGraphClause: NAMED SourceSelector
{
  if($2) {
    raptor_uri* uri = rasqal_literal_as_uri($2);
    rasqal_data_graph* dg;

    dg = rasqal_new_data_graph_from_uri(rq->world, uri,
                                        uri, RASQAL_DATA_GRAPH_NAMED,
                                        NULL, NULL, NULL);
    
    if(!dg) {
      rasqal_free_literal($2);
      YYERROR_MSG("NamedGraphClause: rasqal_query_new_data_graph_from_uri() failed");
    }
    rasqal_free_literal($2);
    $$ = dg;
  } else
    $$ = NULL;
}
;


/* SPARQL Grammar: SourceSelector */
SourceSelector: IRIref
{
  $$ = $1;
}
;


/* SPARQL 1.1 Grammar: WhereClause */
WhereClause:  WHERE GroupGraphPattern
{
  $$ = $2;
}
| GroupGraphPattern
{
  $$ = $1;
}
;


/* NEW Grammar Term pulled out of DescribeQuery */
WhereClauseOpt:  WhereClause
{
  $$ = $1;
}
| /* empty */
{
  $$ = NULL;
}
;


/* SPARQL 1.1 Grammar: [18] SolutionModifier */
SolutionModifier: GroupClauseOpt HavingClauseOpt OrderClauseOpt LimitOffsetClausesOpt
{
  $$ = rasqal_new_solution_modifier(rq,
                                    /* order_conditions */ $3,
                                    /* group_conditions */ $1,
                                    /* having_conditions */ $2,
                                    /* limit */ $4[0],
                                    /* offset */ $4[1]);
  
}
;


/* NEW Grammar Term pulled out of SPARQL 1.1 [19] GroupClauseOpt */
GroupConditionList: GroupConditionList GroupCondition
{
  $$ = $1;
  if($2)
    if(raptor_sequence_push($$, $2)) {
      raptor_free_sequence($$);
      $$ = NULL;
      YYERROR_MSG("GroupConditionList 1: sequence push failed");
    }
}
| GroupCondition
{
  $$ = raptor_new_sequence((raptor_data_free_handler)rasqal_free_expression,
                           (raptor_data_print_handler)rasqal_expression_print);
  if(!$$) {
    if($1)
      rasqal_free_expression($1);
    YYERROR_MSG("GroupConditionList 2: cannot create sequence");
  }
  if($1)
    if(raptor_sequence_push($$, $1)) {
      raptor_free_sequence($$);
      $$ = NULL;
      YYERROR_MSG("GroupConditionList 2: sequence push failed");
    }
}
;


/* NEW Grammar Term pulled out of SPARQL 1.1 Grammar: [] GroupCondition */
AsVarOpt: AS Var
{
  $$ = $2;
}
| /* empty */
{
  $$ = NULL;
}
;
 

/* SPARQL 1.1 Grammar: [] GroupCondition */
GroupCondition: BuiltInCall
{
  $$ = $1;
}
| FunctionCall 
{
  $$ = $1;
}
| '(' Expression AsVarOpt ')'
{
  rasqal_literal* l;

  $$ = $2;
  if($3) {
    if(rasqal_expression_mentions_variable($$, $3)) {
      sparql_query_error_full(rq,
                              "Expression in GROUP BY ( expression ) AS %s contains the variable name '%s'",
                              $3->name, $3->name);
    } else {
      /* Expression AS Variable */
      $3->expression = $$;
      $$ = NULL;
      
      l = rasqal_new_variable_literal(rq->world, $3);
      if(!l)
        YYERROR_MSG("GroupCondition 4: cannot create variable literal");
      $3 = NULL;

      $$ = rasqal_new_literal_expression(rq->world, l);
      if(!$$)
        YYERROR_MSG("GroupCondition 4: cannot create variable literal expression");
    }
  }
  
}
| Var
{
  rasqal_literal* l;
  l = rasqal_new_variable_literal(rq->world, $1);
  if(!l)
    YYERROR_MSG("GroupCondition 5: cannot create lit");
  $$ = rasqal_new_literal_expression(rq->world, l);
  if(!$$)
    YYERROR_MSG("GroupCondition 5: cannot create lit expr");
}
;


/* SPARQL 1.1 Grammar: [19] GroupClause renamed for clarity */
GroupClauseOpt: GROUP BY GroupConditionList
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(rq->context);

  $$ = NULL;
  if(!sparql->sparql11_query) {
    sparql_syntax_error(rq,
                        "GROUP BY can only be used with SPARQL 1.1");
    YYERROR;
  } else
    $$ = $3;
}
| /* empty */
{
  $$ = NULL;
}
;


/* SPARQL 1.1 [22] HavingCondition */
HavingCondition: Constraint
{
  $$ = $1;
}
;

/* NEW Grammar Term pulled out of SPARQL 1.1 [19] HavingClauseOpt */
HavingConditionList: HavingConditionList HavingCondition
{
  $$ = $1;
  if($2)
    if(raptor_sequence_push($$, $2)) {
      raptor_free_sequence($$);
      $$ = NULL;
      YYERROR_MSG("HavingConditionList 1: sequence push failed");
    }
}
| HavingCondition
{
  $$ = raptor_new_sequence((raptor_data_free_handler)rasqal_free_expression,
                           (raptor_data_print_handler)rasqal_expression_print);
  if(!$$) {
    if($1)
      rasqal_free_expression($1);
    YYERROR_MSG("HavingConditionList 2: cannot create sequence");
  }
  if($1)
    if(raptor_sequence_push($$, $1)) {
      raptor_free_sequence($$);
      $$ = NULL;
      YYERROR_MSG("HavingConditionList 2: sequence push failed");
    }
}
;


/* SPARQL 1.1 Grammar: [19] HavingClause renamed for clarity */
HavingClauseOpt: HAVING HavingConditionList
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(rq->context);

  $$ = NULL;
  if(!sparql->sparql11_query) {
    sparql_syntax_error(rq,
                        "HAVING can only be used with SPARQL 1.1");
    YYERROR;
  } else 
    $$ = $2;
}
| /* empty */
{
  $$ = NULL;
}
;


/* SPARQL Grammar: LimitOffsetClauses */
LimitOffsetClausesOpt: LimitClause OffsetClause
{
  $$[0] = $1;
  $$[1] = $2;
}
| OffsetClause LimitClause
{
  $$[0] = $2;
  $$[1] = $1;
}
| LimitClause
{
  $$[0] = $1;
  $$[1] = -1;
}
| OffsetClause
{
  $$[0] = -1;
  $$[1] = $1;
}
| /* empty */
{
  $$[0] = -1;
  $$[1] = -1;
}
;


/* SPARQL Grammar: OrderClause - remained for clarity */
OrderClauseOpt: ORDER BY OrderConditionList
{
  $$ = $3;
}
| /* empty */
{
  $$ = NULL;
}
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
  $$ = raptor_new_sequence((raptor_data_free_handler)rasqal_free_expression,
                           (raptor_data_print_handler)rasqal_expression_print);
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


/* SPARQL Grammar: OrderCondition */
OrderCondition: ASC BrackettedExpression
{
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_ORDER_COND_ASC, $2);
  if(!$$)
    YYERROR_MSG("OrderCondition 1: cannot create expr");
}
| DESC BrackettedExpression
{
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_ORDER_COND_DESC, $2);
  if(!$$)
    YYERROR_MSG("OrderCondition 2: cannot create expr");
}
| FunctionCall 
{
  /* The direction of ordering is ascending by default */
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_ORDER_COND_ASC, $1);
  if(!$$)
    YYERROR_MSG("OrderCondition 3: cannot create expr");
}
| Var
{
  rasqal_literal* l;
  rasqal_expression *e;
  l = rasqal_new_variable_literal(rq->world, $1);
  if(!l)
    YYERROR_MSG("OrderCondition 4: cannot create lit");
  e = rasqal_new_literal_expression(rq->world, l);
  if(!e)
    YYERROR_MSG("OrderCondition 4: cannot create lit expr");

  /* The direction of ordering is ascending by default */
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_ORDER_COND_ASC, e);
  if(!$$)
    YYERROR_MSG("OrderCondition 1: cannot create expr");
}
| BrackettedExpression
{
  /* The direction of ordering is ascending by default */
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_ORDER_COND_ASC, $1);
  if(!$$)
    YYERROR_MSG("OrderCondition 5: cannot create expr");
}
| BuiltInCall
{
  /* The direction of ordering is ascending by default */
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_ORDER_COND_ASC, $1);
  if(!$$)
    YYERROR_MSG("OrderCondition 6: cannot create expr");
}
;


/* SPARQL Grammar: LimitClause - remained for clarity */
LimitClause: LIMIT INTEGER_LITERAL
{
  $$ = -1;

  if($2 != NULL) {
    $$ = $2->value.integer;
    rasqal_free_literal($2);
  }
  
}
;


/* SPARQL Grammar: OffsetClause - remained for clarity */
OffsetClause: OFFSET INTEGER_LITERAL
{
  $$ = -1;

  if($2 != NULL) {
    $$ = $2->value.integer;
    rasqal_free_literal($2);
  }
}
;


/* SPARQL Grammar: ValuesClause renamed for clarity */
ValuesClauseOpt: VALUES DataBlock
{
  $$ = $2;
}
| /* empty */
{
  $$ = NULL;
}
;


/* NEW Grammar Term pulled out of InlineDataFull */
VarListOpt: VarList
{
  $$ = $1;
}
| /* empty */
{
  $$ = NULL;
}
;

/* NEW Grammar Term pulled out of InlineDataFull */
VarList: VarList Var
{
  $$ = $1;
  if(raptor_sequence_push($$, $2)) {
    raptor_free_sequence($$);
    $$ = NULL;
    YYERROR_MSG("VarList 1: sequence push failed");
  }
}
| Var
{
  $$ = raptor_new_sequence((raptor_data_free_handler)rasqal_free_variable,
                           (raptor_data_print_handler)rasqal_variable_print);
  if(!$$)
    YYERROR_MSG("VarList 2: cannot create seq");

  if(raptor_sequence_push($$, $1)) {
    raptor_free_sequence($$);
    $$ = NULL;
    YYERROR_MSG("VarList 3: sequence push failed");
  }
}
;


/* NEW Grammar Term pulled out of InlineDataFull
 * Maybe empty list of rows of '(' DataBlockValue* ')'
 */
DataBlockRowListOpt: DataBlockRowList
{
  $$ = $1;
}
| /* empty */
{
  $$ = NULL;
}
;


/* NEW Grammar Term pulled out of InlineDataFull
 * Non-empty list of rows of '(' DataBlockValue* ')'
 */
DataBlockRowList: DataBlockRowList DataBlockRow
{
  $$ = $1;
  if(raptor_sequence_push($$, $2)) {
    raptor_free_sequence($$);
    $$ = NULL;
    YYERROR_MSG("DataBlockRowList 1: sequence push failed");
  } else {
    int size = raptor_sequence_size($$);
    $2->offset = size-1;
  }
}
| DataBlockRow
{
  $$ = raptor_new_sequence((raptor_data_free_handler)rasqal_free_row,
                           (raptor_data_print_handler)rasqal_row_print);
  if(!$$) {
    if($1)
      rasqal_free_row($1);

    YYERROR_MSG("DataBlockRowList 2: cannot create sequence");
  }
  if(raptor_sequence_push($$, $1)) {
    raptor_free_sequence($$);
    $$ = NULL;
    YYERROR_MSG("DataBlockRowList 2: sequence push failed");
  }
}
;


/* NEW Grammar Term pulled out of BindingsClause
 * Row of '(' DataBlockValue* ')'
 */
DataBlockRow: '(' DataBlockValueList ')'
{
  $$ = NULL;
  if($2) {
    int size;
    rasqal_row* row;
    int i;
    
    size = raptor_sequence_size($2);

    row = rasqal_new_row_for_size(rq->world, size);
    if(!row) {
      YYERROR_MSG("DataBlockRow: cannot create row");
    } else {
      for(i = 0; i < size; i++) {
        rasqal_literal* value = (rasqal_literal*)raptor_sequence_get_at($2, i);
        rasqal_row_set_value_at(row, i, value);
      }
    }
    raptor_free_sequence($2);
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("DataBlockRow returned: ");
    rasqal_row_print(row, stderr);
    fputc('\n', stderr);
#endif
    $$ = row;
  }
}
| '(' ')'
{
  $$ = NULL;
}
;

/* NEW Grammar Term pulled out of InlineDataFull */
DataBlockValueList: DataBlockValueList DataBlockValue
{
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
  RASQAL_DEBUG1("DataBlockValue 1 value: ");
  rasqal_literal_print($2, stderr);
  fputc('\n', stderr);
#endif
  $$ = $1;
  if(raptor_sequence_push($$, $2)) {
    raptor_free_sequence($$);
    $$ = NULL;
    YYERROR_MSG("IriRefList 1: sequence push failed");
  }
}
| DataBlockValue
{
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
  RASQAL_DEBUG1("DataBlockValue 2 value: ");
  rasqal_literal_print($1, stderr);
  fputc('\n', stderr);
#endif
  $$ = raptor_new_sequence((raptor_data_free_handler)rasqal_free_literal,
                           (raptor_data_print_handler)rasqal_literal_print);
  if(!$$) {
    if($1)
      rasqal_free_literal($1);
    YYERROR_MSG("IriRefList 2: cannot create sequence");
  }
  if(raptor_sequence_push($$, $1)) {
    raptor_free_sequence($$);
    $$ = NULL;
    YYERROR_MSG("IriRefList 2: sequence push failed");
  }
}
;


RDFLiteral: STRING
{
  $$ = rasqal_new_string_literal(rq->world, $1, 
	                         NULL /* language */,
                                 NULL /* dt uri */, NULL /* dt_qname */);
}
| STRING LANGTAG
{
  $$ = rasqal_new_string_literal(rq->world, $1, 
	                         RASQAL_GOOD_CAST(const char*, $2),
                                 NULL /* dt uri */, NULL /* dt_qname */);
}
| STRING HATHAT IRIref
{
  raptor_uri* dt_uri = raptor_uri_copy(rasqal_literal_as_uri($3));
  $$ = rasqal_new_string_literal(rq->world, $1, 
	                         NULL /* language */,
                                 dt_uri, NULL /* dt_qname */);
  rasqal_free_literal($3);
}
| NumericLiteral HATHAT IRIref
{
  if($1) {
    raptor_uri* dt_uri = raptor_uri_copy(rasqal_literal_as_uri($3));
    const unsigned char *str = $1->string;
    $1->string = NULL;

    $$ = rasqal_new_string_literal(rq->world, str,
                                   NULL /* language */,
                                   dt_uri, NULL /* dt_qname */);
  }
  rasqal_free_literal($3);
  rasqal_free_literal($1);
}
;


/* SPARQL Grammar: DataBlockValue */
DataBlockValue: IRIref
{
  $$ = $1;
}
| RDFLiteral
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
| UNDEF
{
  $$ = NULL;
}
;



/* SPARQL Grammar: GroupGraphPattern 
 * TriplesBlockOpt: formula or NULL (on success or error)
 * GraphPatternListOpt: always 1 Group GP or NULL (on error)
 */
GroupGraphPattern: '{' SubSelect '}'
{
  $$ = $2;
}
| '{' GroupGraphPatternSub '}'
{
  $$ = $2;
}
;


/* SPARQL Grammar: GroupGraphPatternSub
 * TriplesBlockOpt: formula or NULL (on success or error)
 * GraphPatternListOpt: always 1 Group GP or NULL (on error)
 */
GroupGraphPatternSub: TriplesBlockOpt GraphPatternListOpt
{
  rasqal_graph_pattern *formula_gp = NULL;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "GroupGraphPattern\n  TriplesBlockOpt=");
  if($2)
    rasqal_formula_print($1, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fprintf(DEBUG_FH, ", GraphpatternListOpt=");
  if($2)
    rasqal_graph_pattern_print($2, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fputs("\n", DEBUG_FH);
#endif


  if(!$1 && !$2) {
    $$ = rasqal_new_2_group_graph_pattern(rq, NULL, NULL);
    if(!$$)
      YYERROR_MSG("GroupGraphPattern: cannot create group gp");
  } else {
    if($1) {
      formula_gp = rasqal_new_basic_graph_pattern_from_formula(rq,
                                                               $1);
      if(!formula_gp) {
        if($2)
          rasqal_free_graph_pattern($2);
        YYERROR_MSG("GroupGraphPattern: cannot create formula_gp");
      }
    }

    if($2) {
      $$ = $2;
      if(formula_gp && raptor_sequence_shift($$->graph_patterns, formula_gp)) {
        rasqal_free_graph_pattern($$);
        $$ = NULL;
        YYERROR_MSG("GroupGraphPattern: sequence push failed");
      }
    } else
      $$ = formula_gp;
  }
  
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "  after graph pattern=");
  if($$)
    rasqal_graph_pattern_print($$, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif
}
;


/* Pulled out of SPARQL Grammar: GroupGraphPattern */
TriplesBlockOpt: TriplesBlock
{
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
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


/* Pulled out of SPARQL Grammar: GroupGraphPattern 
 * GraphPatternListOpt: always 1 Group GP or NULL
 * GraphPatternList: always 1 Group GP or NULL (on error)
 *
 * Result: always 1 Group GP or NULL (on error)
 */
GraphPatternListOpt: GraphPatternListOpt GraphPatternList
{
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
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
  
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
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


/* Pulled out of SPARQL Grammar: GroupGraphPattern 
 * GraphPatternListFilter: always 1 GP or NULL (on error)
 * TriplesBlockOpt: formula or NULL (on success or error)
 *
 * Result: always 1 Group GP or NULL (on success or error)
 */
GraphPatternList: GraphPatternListFilter DotOptional TriplesBlockOpt
{
  rasqal_graph_pattern *formula_gp = NULL;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
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
    formula_gp = rasqal_new_basic_graph_pattern_from_formula(rq,
                                                             $3);
    if(!formula_gp) {
      if($1)
        rasqal_free_graph_pattern($1);
      YYERROR_MSG("GraphPatternList: cannot create formula_gp");
    }
  }
  $$ = rasqal_new_2_group_graph_pattern(rq, $1, formula_gp);
  if(!$$)
    YYERROR_MSG("GraphPatternList: cannot create sequence");

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "  after graph pattern=");
  if($$)
    rasqal_graph_pattern_print($$, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif
}
;


/* Pulled out of SPARQL Grammar: GroupGraphPattern 
 * GraphPatternNotTriples: always 1 GP or NULL (on error)
 *
 * Result: always 1 GP or NULL (on error)
 */
GraphPatternListFilter: GraphPatternNotTriples
{
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
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
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "GraphPatternListFilter 2\n  Filter=");
  if($1)
    rasqal_expression_print($1, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fputs("\n", DEBUG_FH);
#endif

  $$ = rasqal_new_filter_graph_pattern(rq, $1);
  if(!$$)
    YYERROR_MSG("GraphPatternListFilter 2: cannot create graph pattern");

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
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


/* SPARQL Grammar: TriplesBlock */
TriplesBlock: TriplesSameSubject '.' TriplesBlockOpt
{
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
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

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
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


/* SPARQL Grammar: GraphPatternNotTriples */
GraphPatternNotTriples: GroupOrUnionGraphPattern
{
  $$ = $1;
}
| OptionalGraphPattern
{
  $$ = $1;
}
| MinusGraphPattern
{
  $$ = $1;
}
| GraphGraphPattern
{
  $$ = $1;
}
| ServiceGraphPattern
{
  $$ = $1;
}
| LetGraphPattern
{
  $$ = $1;
}
| Bind
{
  $$ = $1;
}
| InlineDataGraphPattern
{
  $$ = $1;
}
;


/* SPARQL Grammar: OptionalGraphPattern */
OptionalGraphPattern: OPTIONAL GroupGraphPattern
{
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
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

    seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_graph_pattern,
                              (raptor_data_print_handler)rasqal_graph_pattern_print);
    if(!seq) {
      rasqal_free_graph_pattern($2);
      YYERROR_MSG("OptionalGraphPattern 1: cannot create sequence");
    } else {
      if(raptor_sequence_push(seq, $2)) {
        raptor_free_sequence(seq);
        YYERROR_MSG("OptionalGraphPattern 2: sequence push failed");
      } else {
        $$ = rasqal_new_graph_pattern_from_sequence(rq,
                                                    seq,
                                                    RASQAL_GRAPH_PATTERN_OPERATOR_OPTIONAL);
        if(!$$)
          YYERROR_MSG("OptionalGraphPattern: cannot create graph pattern");
      }
    }
  }
}
;


/* SPARQL Grammar: GraphGraphPattern */
GraphGraphPattern: GRAPH VarOrIRIref GroupGraphPattern
{
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
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

    seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_graph_pattern,
                              (raptor_data_print_handler)rasqal_graph_pattern_print);
    if(!seq) {
      rasqal_free_graph_pattern($3);
      YYERROR_MSG("GraphGraphPattern 1: cannot create sequence");
    } else {
      if(raptor_sequence_push(seq, $3)) {
        raptor_free_sequence(seq);
        YYERROR_MSG("GraphGraphPattern 2: sequence push failed");
      } else {
        $$ = rasqal_new_graph_pattern_from_sequence(rq,
                                                    seq,
                                                    RASQAL_GRAPH_PATTERN_OPERATOR_GRAPH);
        if(!$$)
          YYERROR_MSG("GraphGraphPattern: cannot create graph pattern");
        else
          rasqal_graph_pattern_set_origin($$, $2);
      }
    }
  }


#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "GraphGraphPattern\n  graphpattern=");
  rasqal_graph_pattern_print($$, DEBUG_FH);
  fputs("\n\n", DEBUG_FH);
#endif

  rasqal_free_literal($2);
}
;


/* SPARQL Grammar: ServiceGraphPattern */
ServiceGraphPattern: SERVICE SilentOpt VarOrIRIref GroupGraphPattern
{
  $$ = rasqal_new_single_graph_pattern(rq,
                                       RASQAL_GRAPH_PATTERN_OPERATOR_SERVICE,
                                       $4);
  if($$) {
    $$->silent = ($2 & RASQAL_UPDATE_FLAGS_SILENT) ? 1 : 0;

    $$->origin = $3;
    $3 = NULL;
  } else if($3)
    rasqal_free_literal($3);
}
;


/* SPARQL 1.1: BIND (expression AS ?Var ) . */
Bind: BIND '(' Expression AS Var ')'
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(rq->context);

  $$ = NULL;
  if($3 && $5) {
    if(!sparql->sparql11_query) {
      sparql_syntax_error(rq,
                          "BIND can only be used with SPARQL 1.1");
      YYERROR;
    } else {
      $$ = rasqal_new_let_graph_pattern(rq, $5, $3);
    }
  } else
    $$ = NULL;
}
;


/* SPARQL 1.1: InlineData */
InlineData: VALUES DataBlock
{
  $$ = $2;
}
;

/* SPARQL 1.1: InlineData merged with InlineDataOneVar and InlineDataFull */
DataBlock: InlineDataOneVar 
{
  $$ = $1;
}
| InlineDataFull
{
  $$ = $1;
}
;


/* SPARQL 1.1: InlineDataOneVar */
InlineDataOneVar: Var '{' DataBlockValueListOpt '}'
{
  $$ = rasqal_new_bindings_from_var_values(rq, $1, $3);
}
;


/* Pulled out of InlineDataOneVar
*/
DataBlockValueListOpt: DataBlockValueList
{
  $$ = $1;
}
| /* empty */
{
  $$ = NULL;
}
;


/* SPARQL 1.1: InlineDataFull 
 * ( NIL | '(' Var* ')' ) '{' ( '(' DataBlockValue* ')' | NIL )* '}'
 * and since NIL = '(' ')' with whitespace and VarList handles Vars* etc.
 * = '(' Var* ')' '{' ( '(' DataBlockValue* ')')* '}'
 * = '(' VarListOpt ')' '{' DataBlockRowListOpt '}'
 *
 * DataBlockRowListOpt: ( '(' DataBlockValue* ')')*
 * 
 */
InlineDataFull: '(' VarListOpt ')' '{' DataBlockRowListOpt '}' 
{
  if($2) {
    $$ = rasqal_new_bindings(rq, $2, $5);
    if(!$$)
      YYERROR_MSG("InlineDataFull: cannot create bindings");
  } else {
    if($5)
      raptor_free_sequence($5);

    $$ = NULL;
  }
}
;


InlineDataGraphPattern: InlineData
{
  $$ = rasqal_new_values_graph_pattern(rq, $1);
  if(!$$)
    YYERROR_MSG("InlineDataGraphPattern: cannot create gp");
}
;


/* SPARQL Grammar: MinusGraphPattern */
MinusGraphPattern: MINUS GroupGraphPattern
{
  $$ = rasqal_new_single_graph_pattern(rq,
                                       RASQAL_GRAPH_PATTERN_OPERATOR_MINUS,
                                       $2);
}
;


/* SPARQL Grammar: GroupOrUnionGraphPattern */
GroupOrUnionGraphPattern: GroupGraphPattern UNION GroupOrUnionGraphPatternList
{
  $$ = $3;
  if(raptor_sequence_shift($$->graph_patterns, $1)) {
    rasqal_free_graph_pattern($$);
    $$ = NULL;
    YYERROR_MSG("GroupOrUnionGraphPattern: sequence push failed");
  }

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
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
  seq = raptor_new_sequence((raptor_data_free_handler)rasqal_free_graph_pattern,
                            (raptor_data_print_handler)rasqal_graph_pattern_print);
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
  $$ = rasqal_new_graph_pattern_from_sequence(rq,
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
  sparql = (rasqal_sparql_query_language*)(rq->context);

  $$ = NULL;
  if($3 && $5) {
    if(sparql->experimental)
      $$ = rasqal_new_let_graph_pattern(rq, $3, $5);
    else {
      sparql_syntax_error(rq,
                          "LET can only be used with LAQRS");
      YYERROR;
    }
  } else
    $$ = NULL;
}
;


/* SPARQL Grammar: Filter */
Filter: FILTER Constraint
{
  $$ = $2;
}
;


/* SPARQL Grammar: Constraint */
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


ParamsOpt: ';'
{
  $$ = NULL;
}
| /* empty */
{
  $$ = NULL;
}
;


/* SPARQL Grammar: FunctionCall */
FunctionCall: IRIref '(' DistinctOpt ArgListNoBraces ParamsOpt ')'
{
  raptor_uri* uri = rasqal_literal_as_uri($1);
  
  if(!$4) {
    $4 = raptor_new_sequence((raptor_data_free_handler)rasqal_free_expression,
                             (raptor_data_print_handler)rasqal_expression_print);
    if(!$4) {
      rasqal_free_literal($1);
      YYERROR_MSG("FunctionCall: cannot create sequence");
    }
  }

  uri = raptor_uri_copy(uri);

  if(raptor_sequence_size($4) == 1 &&
     rasqal_xsd_is_datatype_uri(rq->world, uri)) {
    rasqal_expression* e = (rasqal_expression*)raptor_sequence_pop($4);
    $$ = rasqal_new_cast_expression(rq->world, uri, e);
    if($$)
      $$->flags |= $3;
    raptor_free_sequence($4);
  } else {
    unsigned int flags = 0;
    if($3)
      flags |= 1;
    
    $$ = rasqal_new_function_expression(rq->world, 
                                        uri, $4, $5 /* params */,
                                        flags);
    if($$)
      $$->flags |= $3;
  }
  rasqal_free_literal($1);

  if(!$$)
    YYERROR_MSG("FunctionCall: cannot create expr");
}
|
IRIrefBrace ArgListNoBraces ')'
{
  raptor_uri* uri = rasqal_literal_as_uri($1);
  
  if(!$2) {
    $2 = raptor_new_sequence((raptor_data_free_handler)rasqal_free_expression,
                             (raptor_data_print_handler)rasqal_expression_print);
    if(!$2) {
      rasqal_free_literal($1);
      YYERROR_MSG("FunctionCall: cannot create sequence");
    }
  }

  uri = raptor_uri_copy(uri);

  if(raptor_sequence_size($2) == 1 &&
     rasqal_xsd_is_datatype_uri(rq->world, uri)) {
    rasqal_expression* e = (rasqal_expression*)raptor_sequence_pop($2);
    $$ = rasqal_new_cast_expression(rq->world, uri, e);
    raptor_free_sequence($2);
  } else {
    $$ = rasqal_new_function_expression(rq->world,
                                        uri, $2, NULL /* params */,
                                        0 /* flags */);
  }
  rasqal_free_literal($1);

  if(!$$)
    YYERROR_MSG("FunctionCall: cannot create expr");
}
;


/* SPARQL 1.1 */
CoalesceExpression: COALESCE ArgList
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(rq->context);

  $$ = NULL;
  if(!sparql->sparql11_query) {
    sparql_syntax_error(rq,
                        "COALESCE can only be used with SPARQL 1.1");
    YYERROR;
  }
  
  if(!$2) {
    $2 = raptor_new_sequence((raptor_data_free_handler)rasqal_free_expression,
                             (raptor_data_print_handler)rasqal_expression_print);
    if(!$2)
      YYERROR_MSG("FunctionCall: cannot create sequence");
  }

  $$ = rasqal_new_expr_seq_expression(rq->world, 
                                      RASQAL_EXPR_COALESCE, $2);
  if(!$$)
    YYERROR_MSG("Coalesce: cannot create expr");
}
;


/* SPARQL Grammar: ArgList - FIXME: add optional DISTINCT */
ArgList: '(' ArgListNoBraces ')'
{
  $$ = $2;
}


/* SPARQL Grammar: ArgList modified to not have '(' and ')' */
ArgListNoBraces: ArgListNoBraces ',' Expression
{
  $$ = $1;
  if($3)
    if(raptor_sequence_push($$, $3)) {
      raptor_free_sequence($$);
      $$ = NULL;
      YYERROR_MSG("ArgListNoBraces 1: sequence push failed");
    }
}
| Expression
{
  $$ = raptor_new_sequence((raptor_data_free_handler)rasqal_free_expression,
                           (raptor_data_print_handler)rasqal_expression_print);
  if(!$$) {
    if($1)
      rasqal_free_expression($1);
    YYERROR_MSG("ArgListNoBraces 2: cannot create sequence");
  }
  if($1)
    if(raptor_sequence_push($$, $1)) {
      raptor_free_sequence($$);
      $$ = NULL;
      YYERROR_MSG("ArgListNoBraces 2: sequence push failed");
    }
}
| /* empty */
{
  $$ = raptor_new_sequence((raptor_data_free_handler)rasqal_free_expression,
                           (raptor_data_print_handler)rasqal_expression_print);
}
;


/* SPARQL Grammar: ConstructTemplate */
ConstructTemplate:  '{' ConstructTriplesOpt '}'
{
  $$ = $2;
}
;


/* Pulled out of SPARQL Grammar: ConstructTriples */
ConstructTriplesOpt: ConstructTriples
{
  $$ = $1;
}
| /* empty */
{
  $$ = raptor_new_sequence((raptor_data_free_handler)rasqal_free_triple,
                           (raptor_data_print_handler)rasqal_triple_print);
  if(!$$) {
    YYERROR_MSG("ConstructTriplesOpt: cannot create sequence");
  }
}
;


/* SPARQL Grammar: ConstructTriples */
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
      $$ = raptor_new_sequence((raptor_data_free_handler)rasqal_free_triple,
                               (raptor_data_print_handler)rasqal_triple_print);
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


/* SPARQL Grammar: TriplesSameSubject */
TriplesSameSubject: VarOrTerm PropertyListNotEmpty
{
  int i;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
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
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
    fprintf(DEBUG_FH, "  after substitution propertyList=");
    rasqal_formula_print($2, DEBUG_FH);
    fprintf(DEBUG_FH, "\n");
#endif
  }

  $$ = rasqal_formula_join($1, $2);
  if(!$$)
    YYERROR_MSG("TriplesSameSubject 1: formula join failed");

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "  after joining formula=");
  rasqal_formula_print($$, DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif
}
| TriplesNode PropertyList
{
  int i;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "TriplesSameSubject 2\n  TriplesNode=");
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
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
    fprintf(DEBUG_FH, "  after substitution propertyList=");
    rasqal_formula_print($2, DEBUG_FH);
    fprintf(DEBUG_FH, "\n");
#endif
  }

  $$ = rasqal_formula_join($1, $2);
  if(!$$)
    YYERROR_MSG("TriplesSameSubject 2: formula join failed");

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "  after joining formula=");
  rasqal_formula_print($$, DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif
}
;


/* SPARQL Grammar: PropertyListNotEmpty */
PropertyListNotEmpty: Verb ObjectList PropertyListTailOpt
{
  int i;
  
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
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
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
    fprintf(DEBUG_FH, " empty ObjectList not processed\n");
#endif
  } else if($1 && $2) {
    raptor_sequence *seq = $2->triples;
    rasqal_literal *predicate = $1->value;
    rasqal_formula *formula;
    rasqal_triple *t2;
    int size;
    
    formula = rasqal_new_formula(rq->world);
    if(!formula) {
      rasqal_free_formula($1);
      rasqal_free_formula($2);
      if($3)
        rasqal_free_formula($3);
      YYERROR_MSG("PropertyList 1: cannot create formula");
    }
    formula->triples = raptor_new_sequence((raptor_data_free_handler)rasqal_free_triple,
                                           (raptor_data_print_handler)rasqal_triple_print);
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
  
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
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

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
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


/* SPARQL Grammar: PropertyList */
PropertyList: PropertyListNotEmpty
{
  $$ = $1;
}
| /* empty */
{
  $$ = NULL;
}
;


/* SPARQL Grammar: ObjectList */
ObjectList: Object ObjectTail
{
  rasqal_formula *formula;
  rasqal_triple *triple;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
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

  formula = rasqal_new_formula(rq->world);
  if(!formula) {
    rasqal_free_formula($1);
    if($2)
      rasqal_free_formula($2);
    YYERROR_MSG("ObjectList: cannot create formula");
  }
  
  formula->triples = raptor_new_sequence((raptor_data_free_handler)rasqal_free_triple,
                                         (raptor_data_print_handler)rasqal_triple_print);
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

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
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


/* SPARQL Grammar: Object */
Object: GraphNode
{
  $$ = $1;
}
;


/* SPARQL Grammar: Verb */
Verb: VarOrIRIref
{
  $$ = rasqal_new_formula(rq->world);
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

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "verb Verb=rdf:type (a)\n");
#endif

  uri = raptor_new_uri_for_rdf_concept(rq->world->raptor_world_ptr,
                                       RASQAL_GOOD_CAST(const unsigned char*, "type"));
  if(!uri)
    YYERROR_MSG("Verb 2: uri for rdf concept type failed");
  $$ = rasqal_new_formula(rq->world);
  if(!$$) {
    raptor_free_uri(uri);
    YYERROR_MSG("Verb 2: cannot create formula");
  }
  $$->value = rasqal_new_uri_literal(rq->world, uri);
  if(!$$->value) {
    rasqal_free_formula($$);
    $$ = NULL;
    YYERROR_MSG("Verb 2: cannot create uri literal");
  }
}
;


/* SPARQL Grammar: TriplesNode */
TriplesNode: Collection
{
  $$ = $1;
}
| BlankNodePropertyList
{
  $$ = $1;
}
;


/* SPARQL Grammar: BlankNodePropertyList */
BlankNodePropertyList: '[' PropertyListNotEmpty ']'
{
  int i;
  const unsigned char *id;

  if($2 == NULL) {
    $$ = rasqal_new_formula(rq->world);
    if(!$$)
      YYERROR_MSG("BlankNodePropertyList: cannot create formula");
  } else {
    $$ = $2;
    if($$->value) {
      rasqal_free_literal($$->value);
      $$->value = NULL;
    }
  }
  
  id = rasqal_query_generate_bnodeid(rq, NULL);
  if(!id) {
    rasqal_free_formula($$);
    $$ = NULL;
    YYERROR_MSG("BlankNodeProperyList: cannot create bnodeid");
  }

  $$->value = rasqal_new_simple_literal(rq->world,
                                        RASQAL_LITERAL_BLANK, id);
  if(!$$->value) {
    rasqal_free_formula($$);
    $$ = NULL;
    YYERROR_MSG("BlankNodePropertyList: cannot create literal");
  }

  if($2 == NULL) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
    fprintf(DEBUG_FH, "TriplesNode\n  PropertyList=");
    rasqal_formula_print($$, DEBUG_FH);
    fprintf(DEBUG_FH, "\n");
#endif
  } else {
    raptor_sequence *seq = $2->triples;

    /* non-empty property list, handle it  */
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
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

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    fprintf(DEBUG_FH, "  after substitution formula=");
    rasqal_formula_print($$, DEBUG_FH);
    fprintf(DEBUG_FH, "\n\n");
#endif
  }
}
;


/* SPARQL Grammar: Collection (allowing empty case) */
Collection: '(' GraphNodeListNotEmpty ')'
{
  int i;
  rasqal_literal* first_identifier = NULL;
  rasqal_literal* rest_identifier = NULL;
  rasqal_literal* object = NULL;
  rasqal_literal* blank = NULL;
  char const *errmsg = NULL;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "Collection\n  GraphNodeListNotEmpty=");
  raptor_sequence_print($2, DEBUG_FH);
  fprintf(DEBUG_FH, "\n");
#endif

  $$ = rasqal_new_formula(rq->world);
  if(!$$)
    YYERR_MSG_GOTO(err_Collection, "Collection: cannot create formula");

  $$->triples = raptor_new_sequence((raptor_data_free_handler)rasqal_free_triple,
                                    (raptor_data_print_handler)rasqal_triple_print);
  if(!$$->triples)
    YYERR_MSG_GOTO(err_Collection, "Collection: cannot create sequence");

  first_identifier = rasqal_new_uri_literal(rq->world,
                                            raptor_uri_copy(rq->world->rdf_first_uri));
  if(!first_identifier)
    YYERR_MSG_GOTO(err_Collection, "Collection: cannot first_identifier");
  
  rest_identifier = rasqal_new_uri_literal(rq->world,
                                           raptor_uri_copy(rq->world->rdf_rest_uri));
  if(!rest_identifier)
    YYERR_MSG_GOTO(err_Collection, "Collection: cannot create rest_identifier");
  
  object = rasqal_new_uri_literal(rq->world,
                                  raptor_uri_copy(rq->world->rdf_nil_uri));
  if(!object)
    YYERR_MSG_GOTO(err_Collection, "Collection: cannot create nil object");

  for(i = raptor_sequence_size($2)-1; i >= 0; i--) {
    rasqal_formula* f = (rasqal_formula*)raptor_sequence_get_at($2, i);
    rasqal_triple *t2;
    const unsigned char *blank_id = NULL;

    blank_id = rasqal_query_generate_bnodeid(rq, NULL);
    if(!blank_id)
      YYERR_MSG_GOTO(err_Collection, "Collection: cannot create bnodeid");

    blank = rasqal_new_simple_literal(rq->world, RASQAL_LITERAL_BLANK, blank_id);
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
  
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  fprintf(DEBUG_FH, "  after substitution collection=");
  rasqal_formula_print($$, DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif

  rasqal_free_literal(first_identifier);
  rasqal_free_literal(rest_identifier);

  err_Collection:
  if(errmsg) {
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
}
;


/* NEW Grammar Term pulled out of [40] Collection */
/* Sequence of formula */
GraphNodeListNotEmpty: GraphNodeListNotEmpty GraphNode
{
  char const *errmsg = NULL;
  
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
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
    $$ = raptor_new_sequence((raptor_data_free_handler)rasqal_free_formula,
                             (raptor_data_print_handler)rasqal_formula_print);
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
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
    fprintf(DEBUG_FH, "  itemList is now ");
    raptor_sequence_print($$, DEBUG_FH);
    fprintf(DEBUG_FH, "\n\n");
#endif
  }

  err_GraphNodeListNotEmpty:
  if(errmsg) {
    if($2)
      rasqal_free_formula($2);
    if($$) {
      raptor_free_sequence($$);
      $$ = NULL;
    }
    YYERROR_MSG(errmsg);
  }
}
| GraphNode
{
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
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
    $$ = raptor_new_sequence((raptor_data_free_handler)rasqal_free_formula,
                             (raptor_data_print_handler)rasqal_formula_print);
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
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1  
  fprintf(DEBUG_FH, "  GraphNodeListNotEmpty is now ");
  raptor_sequence_print($$, DEBUG_FH);
  fprintf(DEBUG_FH, "\n\n");
#endif
}
;


/* SPARQL Grammar: GraphNode */
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
  $$ = rasqal_new_formula(rq->world);
  if(!$$)
    YYERROR_MSG("VarOrTerm 1: cannot create formula");
  $$->value = rasqal_new_variable_literal(rq->world, $1);
  if(!$$->value) {
    rasqal_free_formula($$);
    $$ = NULL;
    YYERROR_MSG("VarOrTerm 1: cannot create literal");
  }
}
| GraphTerm
{
  $$ = rasqal_new_formula(rq->world);
  if(!$$) {
    if($1)
      rasqal_free_literal($1);
    YYERROR_MSG("VarOrTerm 2: cannot create formula");
  }
  $$->value = $1;
}
;

/* SPARQL Grammar: VarOrIRIref */
VarOrIRIref: Var
{
  $$ = rasqal_new_variable_literal(rq->world, $1);
  if(!$$)
    YYERROR_MSG("VarOrIRIref: cannot create literal");
}
| IRIref
{
  $$ = $1;
}
;


/* SPARQL Grammar: Var */
Var: '?' VarName
{
  $$ = $2;
}
| '$' VarName
{
  $$ = $2;
}
;

/* NEW Grammar Term made from SPARQL Grammar: Var */
VarName: IDENTIFIER
{
  $$ = rasqal_variables_table_add2(rq->vars_table,
                                   RASQAL_VARIABLE_TYPE_NORMAL, $1, 0, NULL);
  if(!$$)
    YYERROR_MSG("VarName: cannot create var");
  RASQAL_FREE(char*, $1);
}
;


/* LAQRS legacy  */
VarOrBadVarName: '?' VarName
{
  $$ = $2;
}
| '$' VarName
{
  $$ = $2;
}
| VarName
{
  $$ = $1;
  sparql_syntax_warning(rq,
                        "... AS varname is deprecated LAQRS syntax, use ... AS ?varname");
}
;


/* SPARQL Grammar: GraphTerm */
GraphTerm: IRIref
{
  $$ = $1;
}
| RDFLiteral
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
  $$ = rasqal_new_uri_literal(rq->world, 
                              raptor_uri_copy(rq->world->rdf_nil_uri));
  if(!$$)
    YYERROR_MSG("GraphTerm: cannot create literal");
}
;

/* SPARQL Grammar: Expression */
Expression: ConditionalOrExpression
{
  $$ = $1;
}
;


/* SPARQL Grammar: ConditionalOrExpression */
ConditionalOrExpression: ConditionalOrExpression SC_OR ConditionalAndExpression
{
  $$ = rasqal_new_2op_expression(rq->world,
                                 RASQAL_EXPR_OR, $1, $3);
  if(!$$)
    YYERROR_MSG("ConditionalOrExpression: cannot create expr");
}
| ConditionalAndExpression
{
  $$ = $1;
}
;


/* SPARQL Grammar: ConditionalAndExpression */
ConditionalAndExpression: ConditionalAndExpression SC_AND RelationalExpression
{
  $$ = rasqal_new_2op_expression(rq->world,
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

/* SPARQL Grammar: ValueLogical - merged into RelationalExpression */

/* SPARQL Grammar: RelationalExpression */
RelationalExpression: AdditiveExpression EQ AdditiveExpression
{
  $$ = rasqal_new_2op_expression(rq->world,
                                 RASQAL_EXPR_EQ, $1, $3);
  if(!$$)
    YYERROR_MSG("RelationalExpression 1: cannot create expr");
}
| AdditiveExpression NEQ AdditiveExpression
{
  $$ = rasqal_new_2op_expression(rq->world,
                                 RASQAL_EXPR_NEQ, $1, $3);
  if(!$$)
    YYERROR_MSG("RelationalExpression 2: cannot create expr");
}
| AdditiveExpression LT AdditiveExpression
{
  $$ = rasqal_new_2op_expression(rq->world,
                                 RASQAL_EXPR_LT, $1, $3);
  if(!$$)
    YYERROR_MSG("RelationalExpression 3: cannot create expr");
}
| AdditiveExpression GT AdditiveExpression
{
  $$ = rasqal_new_2op_expression(rq->world,
                                 RASQAL_EXPR_GT, $1, $3);
  if(!$$)
    YYERROR_MSG("RelationalExpression 4: cannot create expr");
}
| AdditiveExpression LE AdditiveExpression
{
  $$ = rasqal_new_2op_expression(rq->world,
                                 RASQAL_EXPR_LE, $1, $3);
  if(!$$)
    YYERROR_MSG("RelationalExpression 5: cannot create expr");
}
| AdditiveExpression GE AdditiveExpression
{
  $$ = rasqal_new_2op_expression(rq->world,
                                 RASQAL_EXPR_GE, $1, $3);
  if(!$$)
    YYERROR_MSG("RelationalExpression 6: cannot create expr");
}
| AdditiveExpression IN ArgList
{
  $$ = rasqal_new_set_expression(rq->world,
                                 RASQAL_EXPR_IN, $1, $3);
}
| AdditiveExpression NOT IN ArgList
{
  $$ = rasqal_new_set_expression(rq->world,
                                 RASQAL_EXPR_NOT_IN, $1, $4);
}
| AdditiveExpression
{
  $$ = $1;
}
;

/* SPARQL Grammar: NumericExpression - merged into AdditiveExpression */

/* SPARQL Grammar: AdditiveExpression

AdditiveExpression := MultiplicativeExpression ( '+' MultiplicativeExpression | '-' MultiplicativeExpression | ( NumericLiteralPositive | NumericLiteralNegative ) ( ( '*' UnaryExpression ) | ( '/' UnaryExpression ) )* )*

Expanded:

AdditiveExpression:
  MultiplicativeExpression AdExOpExpressionListOuter
| MultiplicativeExpression

AdExOpExpressionListOuter:
  AdExOpExpressionListOuter AdExOpExpressionListInner
| AdExOpExpressionListInner

AdExOpExpressionListInner:
  '+' MultiplicativeExpression
| '-' MultiplicativeExpression
| NumericLiteralPositive AdExOpUnaryExpressionListOpt
| NumericLiteralNegative AdExOpUnaryExpressionListOpt

AdExOpUnaryExpressionListOpt:
  AdExOpUnaryExpressionList
| empty

AdExOpUnaryExpressionList:
  AdExOpUnaryExpressionList AdExOpUnaryExpression
| AdExOpUnaryExpression

AdExOpUnaryExpression:
  '*' UnaryExpression
| '/' UnaryExpression

*/

AdditiveExpression: MultiplicativeExpression AdExOpExpressionListOuter
{
  $$ = $1;

  if($2) {
    int i;
    int size = raptor_sequence_size($2);

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("AdExOpExpressionListOuter sequence: ");
    if($2)
      raptor_sequence_print($2, DEBUG_FH);
    else
      fputs("NULL", DEBUG_FH);
    fputc('\n', DEBUG_FH);
#endif

    /* Walk sequence forming tree of exprs in $$ */
    for(i = 0; i < size; i++) {
      sparql_op_expr* op_expr = (sparql_op_expr*)raptor_sequence_get_at($2, i);
      $$ = rasqal_new_2op_expression(rq->world, op_expr->op, $$, op_expr->expr);
      op_expr->expr = NULL;
    }
    raptor_free_sequence($2);
  }
}
| MultiplicativeExpression
{
  $$ = $1;
}
;


AdExOpExpressionListOuter: AdExOpExpressionListOuter AdExOpExpressionListInner
{
  $$ = $1;

  if($2) {
    if(raptor_sequence_join($$, $2)) {
      raptor_free_sequence($2);
      raptor_free_sequence($$);
      $$ = NULL;
      YYERROR_MSG("AdExOpExpressionListOuter: sequence join failed");
    }
    raptor_free_sequence($2);
  }
}
| AdExOpExpressionListInner
{
  $$ = $1;
}
;

AdExOpExpressionListInner: '+' MultiplicativeExpression
{
  sparql_op_expr* oe;

  $$ = raptor_new_sequence((raptor_data_free_handler)free_op_expr,
                           (raptor_data_print_handler)print_op_expr);
  if(!$$)
    YYERROR_MSG("AdExOpExpressionListInner 1: failed to create sequence");

  oe = new_op_expr(RASQAL_EXPR_PLUS, $2);
  if(!oe)
    YYERROR_MSG("AdExOpExpressionListInner 1: cannot create plus expr");

  if(raptor_sequence_push($$, oe)) {
    raptor_free_sequence($$);
    $$ = NULL;
    YYERROR_MSG("AdExOpExpressionListInner 1: sequence push failed");
  }
}
| '-' MultiplicativeExpression
{
  sparql_op_expr* oe;

  $$ = raptor_new_sequence((raptor_data_free_handler)free_op_expr,
                           (raptor_data_print_handler)print_op_expr);
  if(!$$)
    YYERROR_MSG("AdExOpExpressionListInner 2: failed to create sequence");

  oe = new_op_expr(RASQAL_EXPR_MINUS, $2);
  if(!oe)
    YYERROR_MSG("AdExOpExpressionListInner 2: cannot create minus expr");

  if(raptor_sequence_push($$, oe)) {
    raptor_free_sequence($$);
    $$ = NULL;
    YYERROR_MSG("AdExOpExpressionListInner 2: sequence push failed");
  }
}
| NumericLiteralPositive AdExOpUnaryExpressionListOpt
{
  rasqal_expression *e;
  sparql_op_expr* oe;

  $$ = $2;
  if(!$$) {
    $$ = raptor_new_sequence((raptor_data_free_handler)free_op_expr,
                             (raptor_data_print_handler)print_op_expr);
    if(!$$)
      YYERROR_MSG("AdExOpExpressionListInner 2: failed to create sequence");
  }

  e = rasqal_new_literal_expression(rq->world, $1);
  if(!e)
    YYERROR_MSG("AdExOpExpressionListInner 2: cannot create NumericLiteralPositive literal expression");
  oe = new_op_expr(RASQAL_EXPR_PLUS, e);
  if(!oe)
    YYERROR_MSG("AdExOpExpressionListInner 2: cannot create plus expr");
  raptor_sequence_shift($$, oe);
}
| NumericLiteralNegative AdExOpUnaryExpressionListOpt
{
  rasqal_expression *e;
  sparql_op_expr* oe;

  $$ = $2;
  if(!$$) {
    $$ = raptor_new_sequence((raptor_data_free_handler)free_op_expr,
                             (raptor_data_print_handler)print_op_expr);
    if(!$$)
      YYERROR_MSG("AdExOpExpressionListInner 3: failed to create sequence");
  }

  e = rasqal_new_literal_expression(rq->world, $1);
  if(!e)
    YYERROR_MSG("AdExOpExpressionListInner 3: cannot create NumericLiteralNegative literal expression");
  oe = new_op_expr(RASQAL_EXPR_MINUS, e);
  if(!oe)
    YYERROR_MSG("AdExOpExpressionListInner 3: cannot create minus expr");
  raptor_sequence_shift($$, oe);
}
;

AdExOpUnaryExpressionListOpt: AdExOpUnaryExpressionList
{
  $$ = $1;

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
  RASQAL_DEBUG1("AEListOpt sequence: ");
  if($$)
    raptor_sequence_print($$, DEBUG_FH);
  else
    fputs("NULL", DEBUG_FH);
  fputc('\n', DEBUG_FH);
#endif
}
| /* empty */
{
  $$ = NULL;
}
;

AdExOpUnaryExpressionList: AdExOpUnaryExpressionList AdExOpUnaryExpression
{
  $$ = $1;

  if($$ && $2) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("AdExOpUnaryExpressionList adding AdExOpUnaryExpression: ");
    print_op_expr($2, DEBUG_FH);
    fputc('\n', DEBUG_FH);
#endif

    if(raptor_sequence_push($$, $2)) {
      raptor_free_sequence($$);
      $$ = NULL;
      YYERROR_MSG("AdExOpUnaryExpressionListOpt 1: sequence push failed");
    }
  }
}
| AdExOpUnaryExpression
{
  $$ = raptor_new_sequence((raptor_data_free_handler)free_op_expr,
                           (raptor_data_print_handler)print_op_expr);
  if(!$$)
    YYERROR_MSG("AdExOpUnaryExpressionListOpt 2: failed to create sequence");

  if($1) {
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("AdExOpUnaryExpressionList adding AdExOpUnaryExpression: ");
    print_op_expr($1, DEBUG_FH);
    fputc('\n', DEBUG_FH);
#endif

    if(raptor_sequence_push($$, $1)) {
      raptor_free_sequence($$);
      $$ = NULL;
      YYERROR_MSG("AdExOpUnaryExpressionListOpt 2: sequence push failed");
    }
  }
}
;

AdExOpUnaryExpression: '*' UnaryExpression
{
  $$ = new_op_expr(RASQAL_EXPR_STAR, $2);
  if(!$$)
    YYERROR_MSG("AdExOpUnaryExpression 1: cannot create star expr");
} 
| '/' UnaryExpression
{
  $$ = new_op_expr(RASQAL_EXPR_SLASH, $2);
  if(!$$)
    YYERROR_MSG("AdExOpUnaryExpression 2: cannot create slash expr");
}
;


/* SPARQL Grammar: MultiplicativeExpression

MultiplicativeExpression ::= UnaryExpression ( '*' UnaryExpression | '/' UnaryExpression )*

Expanded:

MultiplicativeExpression:
  UnaryExpression MuExOpUnaryExpressionList
| UnaryExpression

MuExOpUnaryExpressionList: 
  MuExOpUnaryExpressionList MuExOpUnaryExpression
| MuExOpUnaryExpression

MuExOpUnaryExpression:
  '*' UnaryExpression
| '/' UnaryExpression

*/
MultiplicativeExpression: UnaryExpression MuExOpUnaryExpressionList
{
  $$ = $1;

  if($2) {
    int i;
    int size = raptor_sequence_size($2);

#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 1
    RASQAL_DEBUG1("MuExOpUnaryExpressionList sequence: ");
    raptor_sequence_print($2, DEBUG_FH);
    fputc('\n', DEBUG_FH);
#endif

    /* Walk sequence forming tree of exprs in $$ */
    for(i = 0; i < size; i++) {
      sparql_op_expr* op_expr = (sparql_op_expr*)raptor_sequence_get_at($2, i);
      $$ = rasqal_new_2op_expression(rq->world, op_expr->op, $$, op_expr->expr);
      op_expr->expr = NULL;
    }
    raptor_free_sequence($2);
  }
}
| UnaryExpression
{
  $$ = $1;
}
;

MuExOpUnaryExpressionList: MuExOpUnaryExpressionList MuExOpUnaryExpression
{
  $$ = $1;
  if($$ && $2) {
    if(raptor_sequence_push($$, $2)) {
      raptor_free_sequence($$);
      $$ = NULL;
      YYERROR_MSG("MuExOpUnaryExpressionListOpt 1: sequence push failed");
    }
  }
}
| MuExOpUnaryExpression
{
  $$ = raptor_new_sequence((raptor_data_free_handler)free_op_expr,
                           (raptor_data_print_handler)print_op_expr);
  if(!$$)
    YYERROR_MSG("MuExOpUnaryExpressionListOpt 2: failed to create sequence");

  if(raptor_sequence_push($$, $1)) {
    raptor_free_sequence($$);
    $$ = NULL;
    YYERROR_MSG("MuExOpUnaryExpressionListOpt 2: sequence push failed");
  }
}
;


MuExOpUnaryExpression: '*' UnaryExpression
{
  $$ = new_op_expr(RASQAL_EXPR_STAR, $2);
  if(!$$)
    YYERROR_MSG("MuExOpUnaryExpression 1: cannot create star expr");
}
| '/' UnaryExpression
{
  $$ = new_op_expr(RASQAL_EXPR_SLASH, $2);
  if(!$$)
    YYERROR_MSG("MuExOpUnaryExpression 2: cannot create slash expr");
}
;
 


/* SPARQL Grammar: UnaryExpression */
UnaryExpression: '!' PrimaryExpression
{
  $$ = rasqal_new_1op_expression(rq->world,
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
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_UMINUS, $2);
  if(!$$)
    YYERROR_MSG("UnaryExpression 3: cannot create expr");
}
| PrimaryExpression
{
  $$ = $1;
}
;


/* SPARQL Grammar: PrimaryExpression
 * == BrackettedExpression | BuiltInCall | IRIrefOrFunction | RDFLiteral | NumericLiteral | BooleanLiteral | Var | Aggregate
 * == BrackettedExpression | BuiltInCall | IRIref ArgList? | RDFLiteral | NumericLiteral | BooleanLiteral | Var | Aggregate
 * == BrackettedExpression | BuiltInCall | FunctionCall |
 *    approximately GraphTerm | Var | Aggregate
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
  $$ = rasqal_new_literal_expression(rq->world, $1);
  if(!$$)
    YYERROR_MSG("PrimaryExpression 4: cannot create expr");
}
| Var
{
  rasqal_literal *l;
  l = rasqal_new_variable_literal(rq->world, $1);
  if(!l)
    YYERROR_MSG("PrimaryExpression 5: cannot create literal");
  $$ = rasqal_new_literal_expression(rq->world, l);
  if(!$$)
    YYERROR_MSG("PrimaryExpression 5: cannot create expr");
}
| AggregateExpression
{
  $$ = $1;
}
;


/* SPARQL Grammar: BrackettedExpression */
BrackettedExpression: '(' Expression ')'
{
  $$ = $2;
}
;


/* SPARQL Grammar: BuiltInCall */
BuiltInCall: STR '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_STR, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 1: cannot create expr");
}
| LANG '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_LANG, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 2: cannot create expr");
}
| LANGMATCHES '(' Expression ',' Expression ')'
{
  $$ = rasqal_new_2op_expression(rq->world,
                                 RASQAL_EXPR_LANGMATCHES, $3, $5);
  if(!$$)
    YYERROR_MSG("BuiltInCall 3: cannot create expr");
}
| DATATYPE '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_DATATYPE, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 4: cannot create expr");
}
| BOUND '(' Var ')'
{
  rasqal_literal *l;
  rasqal_expression *e;
  l = rasqal_new_variable_literal(rq->world, $3);
  if(!l)
    YYERROR_MSG("BuiltInCall 5: cannot create literal");
  e = rasqal_new_literal_expression(rq->world, l);
  if(!e)
    YYERROR_MSG("BuiltInCall 6: cannot create literal expr");
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_BOUND, e);
  if(!$$)
    YYERROR_MSG("BuiltInCall 7: cannot create expr");
}
| IRI '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world, 
                                 RASQAL_EXPR_IRI, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 7a: cannot create expr");
}
| URI '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world, 
                                 RASQAL_EXPR_IRI, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 7b: cannot create expr");
}
| BNODE '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world, 
                                 RASQAL_EXPR_BNODE, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 7c: cannot create expr");
}
| BNODE '(' ')'
{
  $$ = rasqal_new_1op_expression(rq->world, 
                                 RASQAL_EXPR_BNODE, NULL);
  if(!$$)
    YYERROR_MSG("BuiltInCall 7d: cannot create expr");
}
| RAND '(' ')'
{
  $$ = rasqal_new_0op_expression(rq->world, 
                                 RASQAL_EXPR_RAND);
  if(!$$)
    YYERROR_MSG("BuiltInCall 7e: cannot create expr");
}
| ABS '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world, 
                                 RASQAL_EXPR_ABS, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 7f: cannot create expr");
}
| CEIL '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world, 
                                 RASQAL_EXPR_CEIL, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 7g: cannot create expr");
}
| FLOOR '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world, 
                                 RASQAL_EXPR_FLOOR, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 7h: cannot create expr");
}
| ROUND '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world, 
                                 RASQAL_EXPR_ROUND, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 7i: cannot create expr");
}
| MD5 '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world, 
                                 RASQAL_EXPR_MD5, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 7j: cannot create expr");
}
| SHA1 '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world, 
                                 RASQAL_EXPR_SHA1, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 7k: cannot create expr");
}
| SHA224 '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world, 
                                 RASQAL_EXPR_SHA224, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 7l: cannot create expr");
}
| SHA256 '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world, 
                                 RASQAL_EXPR_SHA256, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 7m: cannot create expr");
}
| SHA384 '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world, 
                                 RASQAL_EXPR_SHA384, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 7n: cannot create expr");
}
| SHA512 '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world, 
                                 RASQAL_EXPR_SHA512, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 7o: cannot create expr");
}
| UUID '(' ')'
{
  $$ = rasqal_new_0op_expression(rq->world, 
                                 RASQAL_EXPR_UUID);
  if(!$$)
    YYERROR_MSG("BuiltInCall 7p: cannot create expr");
}
| STRUUID '(' ')'
{
  $$ = rasqal_new_0op_expression(rq->world, 
                                 RASQAL_EXPR_STRUUID);
  if(!$$)
    YYERROR_MSG("BuiltInCall 7q: cannot create expr");
}
| StringExpression
{
  $$ = $1;
}
| CoalesceExpression
{
  $$ = $1;
}
| IF '(' Expression ',' Expression ',' Expression ')'
{
  $$ = rasqal_new_3op_expression(rq->world,
                                 RASQAL_EXPR_IF, $3, $5, $7);
  if(!$$)
    YYERROR_MSG("BuiltInCall 7e: cannot create expr");
}
| STRLANG '(' Expression ',' Expression ')'
{
  $$ = rasqal_new_2op_expression(rq->world,
                                 RASQAL_EXPR_STRLANG, $3, $5);
  if(!$$)
    YYERROR_MSG("BuiltInCall 7f: cannot create expr");
}
| STRDT '(' Expression ',' Expression ')'
{
  $$ = rasqal_new_2op_expression(rq->world,
                                 RASQAL_EXPR_STRDT, $3, $5);
  if(!$$)
    YYERROR_MSG("BuiltInCall 7g: cannot create expr");
}
| SAMETERM '(' Expression ',' Expression ')'
{
  $$ = rasqal_new_2op_expression(rq->world,
                                 RASQAL_EXPR_SAMETERM, $3, $5);
  if(!$$)
    YYERROR_MSG("BuiltInCall 8: cannot create expr");
}
| ISURI '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_ISURI, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 9: cannot create expr");
}
| ISBLANK '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_ISBLANK, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 10: cannot create expr");
}
| ISLITERAL '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_ISLITERAL, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 11: cannot create expr");
}
| ISNUMERIC '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_ISNUMERIC, $3);
  if(!$$)
    YYERROR_MSG("BuiltInCall 12: cannot create expr");
}
| RegexExpression
{
  $$ = $1;
}
| DatetimeBuiltinAccessors
{
  $$ = $1;
}
| DatetimeExtensions
{
  $$ = $1;
}
;


StringExpression: STRLEN '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_STRLEN, $3);
  if(!$$)
    YYERROR_MSG("StringExpression: cannot create STRLEN() expr");
}
| SUBSTR '(' Expression ',' Expression ')'
{
  $$ = rasqal_new_3op_expression(rq->world,
                                 RASQAL_EXPR_SUBSTR, $3, $5, NULL);
  if(!$$)
    YYERROR_MSG("StringExpression: cannot create SUBSTR() expr");
}
| SUBSTR '(' Expression ',' Expression ',' Expression ')'
{
  $$ = rasqal_new_3op_expression(rq->world,
                                 RASQAL_EXPR_SUBSTR, $3, $5, $7);
  if(!$$)
    YYERROR_MSG("StringExpression: cannot create SUBSTR() expr");
}
| UCASE  '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_UCASE, $3);
  if(!$$)
    YYERROR_MSG("StringExpression: cannot create UCASE() expr");
}
| LCASE  '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_LCASE, $3);
  if(!$$)
    YYERROR_MSG("StringExpression: cannot create LCASE() expr");
}
| STRSTARTS  '(' Expression ',' Expression ')'
{
  $$ = rasqal_new_2op_expression(rq->world,
                                 RASQAL_EXPR_STRSTARTS, $3, $5);
  if(!$$)
    YYERROR_MSG("StringExpression: cannot create STRSTARTS() expr");
}
| STRENDS  '(' Expression ',' Expression ')'
{
  $$ = rasqal_new_2op_expression(rq->world,
                                 RASQAL_EXPR_STRENDS, $3, $5);
  if(!$$)
    YYERROR_MSG("StringExpression: cannot create STRENDS() expr");
}
| CONTAINS  '(' Expression ',' Expression  ')'
{
  $$ = rasqal_new_2op_expression(rq->world,
                                 RASQAL_EXPR_CONTAINS, $3, $5);
  if(!$$)
    YYERROR_MSG("StringExpression: cannot create YEAR expr");
}
| ENCODE_FOR_URI  '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_ENCODE_FOR_URI, $3);
  if(!$$)
    YYERROR_MSG("StringExpression: cannot create ENCODE_FOR_URI() expr");
}
| CONCAT '(' ExpressionList ')'
{
  $$ = rasqal_new_expr_seq_expression(rq->world, 
                                      RASQAL_EXPR_CONCAT, $3);
  if(!$$)
    YYERROR_MSG("StringExpression: cannot create CONCAT() expr");
}
| STRBEFORE  '(' Expression ',' Expression ')'
{
  $$ = rasqal_new_2op_expression(rq->world,
                                 RASQAL_EXPR_STRBEFORE, $3, $5);
  if(!$$)
    YYERROR_MSG("StringExpression: cannot create STRBEFORE() expr");
}
| STRAFTER  '(' Expression ',' Expression ')'
{
  $$ = rasqal_new_2op_expression(rq->world,
                                 RASQAL_EXPR_STRAFTER, $3, $5);
  if(!$$)
    YYERROR_MSG("StringExpression: cannot create STRAFTER() expr");
}
| REPLACE '(' Expression ',' Expression ',' Expression ')'
{
  $$ = rasqal_new_3op_expression(rq->world,
                                 RASQAL_EXPR_REPLACE, $3, $5, $7);
  if(!$$)
    YYERROR_MSG("StringExpression: cannot create REPLACE() expr");
}
| REPLACE '(' Expression ',' Expression ',' Expression ',' Expression ')'
{
  $$ = rasqal_new_4op_expression(rq->world,
                                 RASQAL_EXPR_REPLACE, $3, $5, $7, $9);
  if(!$$)
    YYERROR_MSG("StringExpression: cannot create REPLACE() expr");
}
;


/* SPARQL Grammar: RegexExpression */
RegexExpression: REGEX '(' Expression ',' Expression ')'
{
  $$ = rasqal_new_3op_expression(rq->world,
                                 RASQAL_EXPR_REGEX, $3, $5, NULL);
  if(!$$)
    YYERROR_MSG("RegexExpression 1: cannot create expr");
}
| REGEX '(' Expression ',' Expression ',' Expression ')'
{
  $$ = rasqal_new_3op_expression(rq->world,
                                 RASQAL_EXPR_REGEX, $3, $5, $7);
  if(!$$)
    YYERROR_MSG("RegexExpression 2: cannot create expr");
}
;


/* SPARQL 1.1 pre-draft */
DatetimeBuiltinAccessors: YEAR '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_YEAR, $3);
  if(!$$)
    YYERROR_MSG("DatetimeBuiltinAccessors: cannot create YEAR expr");
}
| MONTH '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_MONTH, $3);
  if(!$$)
    YYERROR_MSG("DatetimeBuiltinAccessors: cannot create MONTH expr");
}
| DAY '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_DAY, $3);
  if(!$$)
    YYERROR_MSG("DatetimeBuiltinAccessors: cannot create DAY expr");
}
| HOURS '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_HOURS, $3);
  if(!$$)
    YYERROR_MSG("DatetimeBuiltinAccessors: cannot create HOURS expr");
}
| MINUTES '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_MINUTES, $3);
  if(!$$)
    YYERROR_MSG("DatetimeBuiltinAccessors: cannot create MINUTES expr");
}
| SECONDS '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_SECONDS, $3);
  if(!$$)
    YYERROR_MSG("DatetimeBuiltinAccessors: cannot create SECONDS expr");
}
| TIMEZONE '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_TIMEZONE, $3);
  if(!$$)
    YYERROR_MSG("DatetimeBuiltinAccessors: cannot create TIMEZONE expr");
}
| TZ '(' Expression ')'
{
  $$ = rasqal_new_1op_expression(rq->world,
                                 RASQAL_EXPR_TZ, $3);
  if(!$$)
    YYERROR_MSG("DatetimeBuiltinAccessors: cannot create TZ expr");
}
;


/* LAQRS */
DatetimeExtensions: CURRENT_DATETIME '(' ')'
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(rq->context);

  $$ = NULL;
  if(sparql->experimental) {
    $$ = rasqal_new_0op_expression(rq->world,
                                   RASQAL_EXPR_CURRENT_DATETIME);
    if(!$$)
      YYERROR_MSG("DatetimeExtensions: cannot create CURRENT_DATETIME() expr");
  } else {
    sparql_syntax_error(rq,
                        "CURRENT_DATETIME() can only used with LAQRS");
    YYERROR;
  }
}
| NOW '(' ')'
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(rq->context);
  
  $$ = NULL;
  if(!sparql->sparql11_query) {
    sparql_syntax_error(rq,
                        "NOW() can only be used with SPARQL 1.1");
    YYERROR;
  }
  
  $$ = rasqal_new_0op_expression(rq->world,
                                   RASQAL_EXPR_NOW);
  if(!$$)
    YYERROR_MSG("DatetimeExtensions: cannot create NOW()");

}
| FROM_UNIXTIME '(' Expression ')'
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(rq->context);

  $$ = NULL;
  if(sparql->experimental) {
    $$ = rasqal_new_1op_expression(rq->world,
                                   RASQAL_EXPR_FROM_UNIXTIME, $3);
    if(!$$)
      YYERROR_MSG("DatetimeExtensions: cannot create FROM_UNIXTIME() expr");
  } else {
    sparql_syntax_error(rq,
                        "FROM_UNIXTIME() can only used with LAQRS");
    YYERROR;
  }
  
}
| TO_UNIXTIME '(' Expression ')'
{
  rasqal_sparql_query_language* sparql;
  sparql = (rasqal_sparql_query_language*)(rq->context);

  $$ = NULL;
  if(sparql->experimental) {
    $$ = rasqal_new_1op_expression(rq->world,
                                   RASQAL_EXPR_TO_UNIXTIME, $3);
    if(!$$)
      YYERROR_MSG("DatetimeExtensions: cannot create TO_UNIXTIME() expr");
  } else {
    sparql_syntax_error(rq,
                        "TO_UNIXTIME() can only used with LAQRS");
    YYERROR;
  }
  
}
;

/* SPARQL Grammar: IRIrefOrFunction - not necessary in this
   grammar as the IRIref ambiguity is determined in lexer with the
   help of the IRIrefBrace token below */

/* NEW Grammar Term made from SPARQL Grammar: IRIref + '(' expanded */
IRIrefBrace: URI_LITERAL_BRACE
{
  $$ = rasqal_new_uri_literal(rq->world, $1);
  if(!$$)
    YYERROR_MSG("IRIrefBrace 1: cannot create literal");
}
| QNAME_LITERAL_BRACE
{
  $$ = rasqal_new_simple_literal(rq->world,
                                 RASQAL_LITERAL_QNAME, $1);
  if(!$$)
    YYERROR_MSG("IRIrefBrace 2: cannot create literal");
  if(rasqal_literal_expand_qname(rq, $$)) {
    sparql_query_error_full(rq,
                            "QName %s cannot be expanded", $1);
    rasqal_free_literal($$);
    $$ = NULL;
    YYERROR_MSG("IRIrefBrace 2: cannot expand qname");
  }
}
;


/* SPARQL Grammar: RDFLiteral - merged into GraphTerm */

/* SPARQL Grammar: NumericLiteral */
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


/* SPARQL Grammar: NumericLiteralNegative */
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


/* SPARQL Grammar: BooleanLiteral - merged into GraphTerm */

/* SPARQL Grammar: String - merged into GraphTerm */

/* SPARQL Grammar: IRIref */
IRIref: URI_LITERAL
{
  $$ = rasqal_new_uri_literal(rq->world, $1);
  if(!$$)
    YYERROR_MSG("IRIref 1: cannot create literal");
}
| QNAME_LITERAL
{
  $$ = rasqal_new_simple_literal(rq->world,
                                 RASQAL_LITERAL_QNAME, $1);
  if(!$$)
    YYERROR_MSG("IRIref 2: cannot create literal");
  if(rasqal_literal_expand_qname(rq, $$)) {
    sparql_query_error_full(rq,
                            "QName %s cannot be expanded", $1);
    rasqal_free_literal($$);
    $$ = NULL;
    YYERROR_MSG("IRIrefBrace 2: cannot expand qname");
  }
}
;


/* SPARQL Grammar: QName - made into terminal QNAME_LITERAL */

/* SPARQL Grammar: BlankNode */
BlankNode: BLANK_LITERAL
{
  $$ = rasqal_new_simple_literal(rq->world,
                                 RASQAL_LITERAL_BLANK, $1);
  if(!$$)
    YYERROR_MSG("BlankNode 1: cannot create literal");
} | '[' ']'
{
  const unsigned char *id;
  id = rasqal_query_generate_bnodeid(rq, NULL);
  if(!id)
    YYERROR_MSG("BlankNode 2: cannot create bnodeid");
  $$ = rasqal_new_simple_literal(rq->world,
                                 RASQAL_LITERAL_BLANK, id);
  if(!$$)
    YYERROR_MSG("BlankNode 2: cannot create literal");
}
;

/* SPARQL Grammar: Q_IRI_REF onwards are all lexer items
 * with similar names or are inlined.
 */




%%


/* Support functions */


/* This is declared in sparql_lexer.h but never used, so we always get
 * a warning unless this dummy code is here.  Used once below in an error case.
 */
static int yy_init_globals (yyscan_t yyscanner ) { return 0; };


/*
 * rasqal_sparql_query_language_init:
 * @rdf_query: query
 * @name: language name (or NULL)
 *
 * Internal: Initialise the SPARQL query language parser
 *
 * Return value: non 0 on failure
 **/
static int
rasqal_sparql_query_language_init(rasqal_query* rdf_query, const char *name)
{
  rasqal_sparql_query_language* rqe;

  rqe = (rasqal_sparql_query_language*)rdf_query->context;

  rdf_query->compare_flags = RASQAL_COMPARE_XQUERY;

  /* All the sparql query families support this */
  rqe->sparql_scda = 1; /* SELECT CONSTRUCT DESCRIBE ASK */

  /* SPARQL 1.1 Query + Update is the default */
  rqe->sparql_scda = 1; /* SELECT CONSTRUCT DESCRIBE ASK */
  rqe->sparql11_query = 1;
  rqe->sparql11_property_paths = 1;
  rqe->sparql11_update = 1;

  if(name) {
    /* SPARQL 1.0 disables SPARQL 1.1 features */
    if(!strncmp(name, "sparql10", 8)) {
      rqe->sparql11_query = 0;
      rqe->sparql11_property_paths = 0;
      rqe->sparql11_update = 0;
    }

    if(!strcmp(name, "sparql11-query")) {
      /* No update if SPARQL 1.1 query */
      rqe->sparql11_update = 0;
    }

    if(!strcmp(name, "sparql11-update")) {
      /* No query (SELECT, CONSTRUCT, DESCRIBE, ASK) if SPARQL 1.1 update */
      rqe->sparql_scda = 0;
    }

    /* LAQRS for experiments */
    if(!strcmp(name, "laqrs"))
      rqe->experimental = 1;
  }

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

  rc = rasqal_query_reset_select_query(rdf_query);
  if(rc)
    return 1;

  rc = sparql_parse(rdf_query);
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
  if(rasqal_query_expand_wildcards(rdf_query,
                                   rasqal_query_get_projection(rdf_query)))
    return 1;

  return 0;
}


static int
sparql_parse(rasqal_query* rq)
{
  rasqal_sparql_query_language* rqe;
  raptor_locator *locator=&rq->locator;

  rqe = (rasqal_sparql_query_language*)rq->context;

  if(!rq->query_string)
    return yy_init_globals(NULL); /* 0 but a way to use yy_init_globals */

  locator->line = 1;
  locator->column = -1; /* No column info */
  locator->byte = -1; /* No bytes info */

  rqe->lineno = 1;

  if(sparql_lexer_lex_init(&rqe->scanner))
    return 1;
  rqe->scanner_set = 1;

 #if defined(YYDEBUG) && YYDEBUG > 0
   sparql_lexer_set_debug(1 ,&rqe->scanner);
   sparql_parser_debug = 1;
 #endif

  sparql_lexer_set_extra(rq, rqe->scanner);

  (void)sparql_lexer__scan_buffer(RASQAL_GOOD_CAST(char*, rq->query_string),
                                  rq->query_string_length, rqe->scanner);

  rqe->error_count = 0;

  sparql_parser_parse(rq, rqe->scanner);

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

  if(RASQAL_WARNING_LEVEL_QUERY_SYNTAX < rq->world->warning_level)
    return 0;
  
  rqe = (rasqal_sparql_query_language*)rq->context;

  rq->locator.line = rqe->lineno;
#ifdef RASQAL_SPARQL_USE_ERROR_COLUMNS
  /*  rq->locator.column=sparql_lexer_get_column(yyscanner);*/
#endif

  va_start(arguments, message);
  rasqal_log_error_varargs(((rasqal_query*)rq)->world, RAPTOR_LOG_LEVEL_WARN,
                           &rq->locator, message, arguments);
  va_end(arguments);

  return 0;
}


static int
rasqal_sparql_query_language_iostream_write_escaped_counted_string(rasqal_query* query,
                                                                   raptor_iostream* iostr,
                                                                   const unsigned char* string,
                                                                   size_t len)
{
  const char delim = '"';
  
  raptor_iostream_write_byte(delim, iostr);
  if(raptor_string_ntriples_write(string, len, delim, iostr))
    return 1;
  
  raptor_iostream_write_byte(delim, iostr);

  return 0;
}


static const char* const sparql_names[] = { "sparql10", NULL};

static const raptor_type_q sparql_types[] = {
  { NULL, 0, 0}
};


static int
rasqal_sparql_query_language_register_factory(rasqal_query_language_factory *factory)
{
  int rc = 0;

  factory->desc.names = sparql_names;

  factory->desc.mime_types = sparql_types;

  factory->desc.label = "SPARQL 1.0 W3C RDF Query Language";

  factory->desc.uri_strings = NULL;

  factory->context_length = sizeof(rasqal_sparql_query_language);

  factory->init      = rasqal_sparql_query_language_init;
  factory->terminate = rasqal_sparql_query_language_terminate;
  factory->prepare   = rasqal_sparql_query_language_prepare;
  factory->iostream_write_escaped_counted_string = rasqal_sparql_query_language_iostream_write_escaped_counted_string;

  return rc;
}


int
rasqal_init_query_language_sparql(rasqal_world* world)
{
  return !rasqal_query_language_register_factory(world,
                                                 &rasqal_sparql_query_language_register_factory);
}


static const char* const sparql11_names[] = { "sparql", "sparql11", NULL };


static const char* const sparql11_uri_strings[] = {
  "http://www.w3.org/TR/rdf-sparql-query/",
  NULL
};

static const raptor_type_q sparql11_types[] = {
  { "application/sparql", 18, 10}, 
  { NULL, 0, 0}
};


static int
rasqal_sparql11_language_register_factory(rasqal_query_language_factory *factory)
{
  int rc = 0;

  factory->desc.names = sparql11_names;

  factory->desc.mime_types = sparql11_types;

  factory->desc.label = "SPARQL 1.1 (DRAFT) Query and Update Languages";

  /* What URI describes Query and Update languages? */
  factory->desc.uri_strings = sparql11_uri_strings;

  factory->context_length = sizeof(rasqal_sparql_query_language);

  factory->init      = rasqal_sparql_query_language_init;
  factory->terminate = rasqal_sparql_query_language_terminate;
  factory->prepare   = rasqal_sparql_query_language_prepare;
  factory->iostream_write_escaped_counted_string = rasqal_sparql_query_language_iostream_write_escaped_counted_string;

  return rc;
}


static const char* const sparql11_query_names[] = { "sparql11-query", NULL };

static const char* const sparql11_query_uri_strings[] = {
  "http://www.w3.org/TR/2010/WD-sparql11-query-20101014/",
  NULL
};

static const raptor_type_q sparql11_query_types[] = {
  { NULL, 0, 0}
};


static int
rasqal_sparql11_query_language_register_factory(rasqal_query_language_factory *factory)
{
  int rc = 0;

  factory->desc.names = sparql11_query_names;

  factory->desc.mime_types = sparql11_query_types;

  factory->desc.label = "SPARQL 1.1 (DRAFT) Query Language";

  factory->desc.uri_strings = sparql11_query_uri_strings;

  factory->context_length = sizeof(rasqal_sparql_query_language);

  factory->init      = rasqal_sparql_query_language_init;
  factory->terminate = rasqal_sparql_query_language_terminate;
  factory->prepare   = rasqal_sparql_query_language_prepare;
  factory->iostream_write_escaped_counted_string = rasqal_sparql_query_language_iostream_write_escaped_counted_string;

  return rc;
}


static const char* const sparql11_update_names[] = { "sparql11-update", NULL };

static const char* const sparql11_update_uri_strings[] = {
  "http://www.w3.org/TR/2010/WD-sparql11-update-20101014/",
  NULL
};

static const raptor_type_q sparql11_update_types[] = {
  { NULL, 0, 0}
};


static int
rasqal_sparql11_update_language_register_factory(rasqal_query_language_factory *factory)
{
  int rc = 0;

  factory->desc.names = sparql11_update_names;

  factory->desc.mime_types = sparql11_update_types;

  factory->desc.label = "SPARQL 1.1 (DRAFT) Update Language";

  factory->desc.uri_strings = sparql11_update_uri_strings;

  factory->context_length = sizeof(rasqal_sparql_query_language);

  factory->init      = rasqal_sparql_query_language_init;
  factory->terminate = rasqal_sparql_query_language_terminate;
  factory->prepare   = rasqal_sparql_query_language_prepare;
  factory->iostream_write_escaped_counted_string = rasqal_sparql_query_language_iostream_write_escaped_counted_string;

  return rc;
}


int
rasqal_init_query_language_sparql11(rasqal_world* world)
{
  if(!rasqal_query_language_register_factory(world,
                                             &rasqal_sparql11_language_register_factory))
    return 1;
  
  if(!rasqal_query_language_register_factory(world,
                                             &rasqal_sparql11_query_language_register_factory))
    return 1;
  
  if(!rasqal_query_language_register_factory(world,
                                             &rasqal_sparql11_update_language_register_factory))
    return 1;
  
  return 0;
}


static const char* const laqrs_names[] = { "laqrs", NULL};

static const raptor_type_q laqrs_types[] = {
  { NULL, 0, 0}
};


static int
rasqal_laqrs_query_language_register_factory(rasqal_query_language_factory *factory)
{
  int rc = 0;

  factory->desc.names = laqrs_names;

  factory->desc.mime_types = laqrs_types;

  factory->desc.label = "LAQRS adds to Querying RDF in SPARQL";

  factory->desc.uri_strings = NULL;

  factory->context_length = sizeof(rasqal_sparql_query_language);

  factory->init      = rasqal_sparql_query_language_init;
  factory->terminate = rasqal_sparql_query_language_terminate;
  factory->prepare   = rasqal_sparql_query_language_prepare;
  factory->iostream_write_escaped_counted_string = rasqal_sparql_query_language_iostream_write_escaped_counted_string;

  return rc;
}


int
rasqal_init_query_language_laqrs(rasqal_world* world)
{
  return !rasqal_query_language_register_factory(world,
                                                 &rasqal_laqrs_query_language_register_factory);
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

#define SPARQL_FILE_BUF_SIZE 4096

static char query_string[SPARQL_FILE_BUF_SIZE];

int
main(int argc, char *argv[]) 
{
  const char *program = rasqal_basename(argv[0]);
  rasqal_query *query = NULL;
  FILE *fh;
  int rc;
  const char *filename = NULL;
  raptor_uri* base_uri = NULL;
  unsigned char *uri_string = NULL;
  const char* query_language = "sparql";
  int usage = 0;
  rasqal_world *world;
  size_t read_len;

  world = rasqal_new_world();
  if(!world || rasqal_world_open(world))
    exit(1);

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
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 2
        sparql_parser_debug = 1;
#endif
        break;
  
      case 'i':
        if(optarg) {
          if(rasqal_language_name_check(world, optarg)) {
            query_language = optarg;
          } else {
            fprintf(stderr, "%s: Unknown query language '%s'\n",
                    program, optarg);
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
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 2
    fprintf(stderr, " -d           Bison parser debugging\n");
#endif
    fprintf(stderr, " -i LANGUAGE  Set query language\n");
    rc = 1;
    goto tidy;
  }


 fh = fopen(filename, "r");
 if(!fh) {
   fprintf(stderr, "%s: Cannot open file %s - %s\n", program, filename,
           strerror(errno));
   rc = 1;
   goto tidy;
 }
 
  memset(query_string, 0, SPARQL_FILE_BUF_SIZE);
  read_len = fread(query_string, SPARQL_FILE_BUF_SIZE, 1, fh);
  if(read_len < SPARQL_FILE_BUF_SIZE) {
    if(ferror(fh)) {
      fprintf(stderr, "%s: file '%s' read failed - %s\n",
              program, filename, strerror(errno));
      fclose(fh);
      rc = 1;
      goto tidy;
    }
  }
  
  fclose(fh);

  query = rasqal_new_query(world, query_language, NULL);
  rc = 1;
  if(query) {
    uri_string = raptor_uri_filename_to_uri_string(filename);

    if(uri_string) {
      base_uri = raptor_new_uri(world->raptor_world_ptr, uri_string);

      if(base_uri) {
        rc = rasqal_query_prepare(query,
                                  RASQAL_GOOD_CAST(const unsigned char*, query_string),
                                  base_uri);

        if(!rc)
          rasqal_query_print(query, stdout);
      }
    }
  }

  tidy:
  if(query)
    rasqal_free_query(query);

  if(base_uri)
    raptor_free_uri(base_uri);

  if(uri_string)
    raptor_free_memory(uri_string);

  if(world)
    rasqal_free_world(world);

  return rc;
}

#endif /* STANDALONE */

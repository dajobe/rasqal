/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * sparql_lexer.l - Rasqal SPARQL lexer - making tokens for sparql grammar generator
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
 * To generate the C files from this source, rather than use the
 * shipped sparql_lexer.c/.h needs a patched version of flex 2.5.31 such
 * as the one available in Debian GNU/Linux.   Details below
 * near the %option descriptions.
 *
 * SPARQL defined in http://www.w3.org/TR/rdf-sparql-query/
 *   http://www.w3.org/TR/2005/WD-rdf-sparql-query-20050419/
 *
 * Editor's draft of above http://www.w3.org/2001/sw/DataAccess/rq23/
 */


/* recognise 8-bits */
%option 8bit
%option warn nodefault

/* all symbols prefixed by this */
%option prefix="sparql_lexer_"

/* This is not needed, flex is invoked -osparql_lexer.c */
%option outfile="sparql_lexer.c"

/* Emit a C header file for prototypes
 * Only available in flex 2.5.13 or newer.
 * It was renamed to header-file in flex 2.5.19
 */
%option header-file="sparql_lexer.h"

/* Do not emit #include <unistd.h>
 * Only available in flex 2.5.7 or newer.
 * Broken in flex 2.5.31 without patches.
 */
%option nounistd

/* Never interactive */
/*  No isatty() check */
%option never-interactive

/* Batch scanner */
%option batch

/* Never use yyunput */
%option nounput

/* Supply our own alloc/realloc/free functions */
%option noyyalloc noyyrealloc noyyfree

/* Re-entrant scanner */
%option reentrant

%option extra-type="rasqal_query*"

/* Makes sparql_lexer_get_lval() sparql_lexer_set_lval() functions
 * and yylval macro appear 
 */
%option bison-bridge
/* Makes yyget_lloc() yyset_lloc() and yylloc appear */
/* %option bison-locations */


%x ID SPID PREF LITERAL LITERAL2

  /* definitions */

%{

/* NOTE: These headers are NOT included here. They are inserted by fix-flex
 * since otherwise it appears far too late in the generated C
 */

/*
#ifdef HAVE_CONFIG_H
#include <rasqal_config.h>
#endif

#ifdef WIN32
#include <win32_rasqal_config.h>
#endif
*/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#ifdef HAVE_SETJMP_H
#include <setjmp.h>
#endif

#include <rasqal.h>
#include <rasqal_internal.h>

#include <sparql_parser.h>
#include <sparql_common.h>


/* Set RASQAL_DEBUG to 2 for verbose lexing (when building this as
 * sparql_lexer_test) and 3 for super verbose
 */
#if 0
#undef RASQAL_DEBUG
#define RASQAL_DEBUG 3
#endif

static int sparql_skip_c_comment(rasqal_query *rq);

/*
 * Extra checks beyond valid Namespaces in XML 1.1 name
 *
 * SPARQL_NAME_CHECK_VARNAME (token VARNAME)
 *   No '.' allowed.
 *   No '-' allowed.
 *
 * SPARQL_NAME_CHECK_PREFIX (token NCNAME_PREFIX)
 *   No '_' allowed as the first character.
 *   No '.' allowed as the last character.
 *
 * SPARQL_NAME_CHECK_NCNAME (token NCNAME)
 *  [0-9] allowed as the first character
 *   No '.' allowed as the first character.
 *   No '.' allowed as the last character.
 *
*/
typedef enum {
  SPARQL_NAME_CHECK_NO_UL_FIRST  = 1,
  SPARQL_NAME_CHECK_NO_DOT_LAST  = 2,
  SPARQL_NAME_CHECK_NO_DOT_MINUS = 4,
  SPARQL_NAME_CHECK_ALLOW_09_FIRST = 8,

  SPARQL_NAME_CHECK_VARNAME = SPARQL_NAME_CHECK_NO_DOT_MINUS,
  SPARQL_NAME_CHECK_PREFIX  = SPARQL_NAME_CHECK_NO_UL_FIRST | SPARQL_NAME_CHECK_NO_DOT_LAST,
  SPARQL_NAME_CHECK_NCNAME  = SPARQL_NAME_CHECK_NO_DOT_LAST | SPARQL_NAME_CHECK_ALLOW_09_FIRST
} sparql_name_check_flags;


static int rasqal_sparql_name_check(unsigned char *string, size_t length, sparql_name_check_flags check_flags);
static unsigned char *sparql_copy_name(rasqal_query *rq, const unsigned char *text, size_t len, sparql_name_check_flags check_flags);
static raptor_uri* sparql_copy_qname(rasqal_query *rq, const unsigned char *text, size_t len);
static unsigned char *sparql_copy_string_token(rasqal_query* rq, unsigned char *text, size_t len, int delim);
static int sparql_stringbuffer_append_sparql_string(rasqal_query* rq, raptor_stringbuffer* stringbuffer, const unsigned char *text, size_t len, int delim);

#ifdef RASQAL_DEBUG
const char * sparql_token_print(rasqal_world* world, int token, YYSTYPE *lval);
#endif

#ifdef __cplusplus
#define INPUT_FN yyinput
#else
#define INPUT_FN input
#endif

#if FLEX_VERSION_DECIMAL < 20536
/* debian flex 2.5.35-10.1 added these column header prototypes in
 * re-entrant mode.  standard flex omits them
 */
void sparql_lexer_set_column(int column_no, yyscan_t yyscanner);
int sparql_lexer_get_column(yyscan_t yyscanner);
#endif

static void sparql_lexer_cleanup(yyscan_t yyscanner);

/* Remove the re-fill function since it should never be called */
#define YY_INPUT(buf,result,max_size) { return YY_NULL; }

/* Log error handler wrapper */
static void sparql_lexer_log_error(yyconst char *msg, raptor_log_level level, yyscan_t yyscanner);


/* fatal error handler declaration 
 * If we have a setjmp / longjmp then report an ERROR and do not abort.
 *
 * Flex's use of this macro makes doing a return impossible since some
 * uses require a return NULL, some an int and some no value :(
 */
#ifdef HAVE_SETJMP
static jmp_buf sparql_lexer_log_error_longjmp_env;
#define YY_FATAL_ERROR(msg) do {		\
    sparql_lexer_log_error(msg, RAPTOR_LOG_LEVEL_ERROR, yyscanner);      \
    longjmp(sparql_lexer_log_error_longjmp_env, 1);        \
} while(0)
#else
#define YY_FATAL_ERROR(msg) do {		\
    sparql_lexer_log_error(msg, RAPTOR_LOG_LEVEL_FATAL, yyscanner);   \
    abort();                                    \
} while(0)
#endif
 
/* Error handler that returns EOF instead of abort() / longjmp()
 * so that parser can clean up properly */
#define YY_REPORT_ERROR_EOF(msg) do { \
    sparql_lexer_log_error(msg, RAPTOR_LOG_LEVEL_ERROR, yyscanner); \
    yyterminate(); \
} while(0)

%}

LANGTAG "@"[A-Za-z][-A-Z_a-z0-9]*


/*
 * rq23 is http://www.w3.org/2001/sw/DataAccess/rq23/
 * CVS ID 1.420 2005/07/12 15:38:40
 */


/* [85] NCCHAR1p ::= [A-Z] | [a-z] | [#x00C0-#x00D6] | [#x00D8-#x00F6] |
 *   [#x00F8-#x02FF] | [#x0370-#x037D] | [#x037F-#x1FFF] |
 *   [#x200C-#x200D] | [#x2070-#x218F] | [#x2C00-#x2FEF] |
 *   [#x3001-#xD7FF] | [#xF900-#xFDCF] | [#xFDF0-#xFFFD] |
 *   [#x10000-#xEFFFF]
 *
 * This is an XML 1.1 NameStartChar
 *   http://www.w3.org/TR/2004/REC-xml11-20040204/#NT-NameStartChar
 * except
 *   No '_' allowed.
 */
NCCHAR1p [A-Za-z\\\x80-\xff]

/* [86] NCCHAR1 ::= NCCHAR1p | '_'
 * This is an XML 1.1 NameStartChar
 *   http://www.w3.org/TR/2004/REC-xml11-20040204/#NT-NameStartChar
 */
NCCHAR1 [A-Za-z\\\x80-\xff_]

NCCHAR2 [A-Za-z\\\x80-\xff_0-9]

/* [87]  VARNAME ::= ( NCCHAR1 | _ ) 
 *   ( NCCHAR1 | [0-9] | #x00B7 | [#x0300-#x036F] | [#x203F-#x2040] )*
 *
 * This is an Namespaces in XML 1.1 Name except:
 *   No '.' allowed.
 *   No '-' allowed.
 */
VARNAME ({NCCHAR1}|[0-9])({NCCHAR1}|[0-9])*

/* [88]  NCCHAR ::= 
 *   NCCHAR1 | '-' | [0-9] | #x00B7 | [#x0300-#x036F] | [#x203F-#x2040]
 *
 * This is XML 1.1 NameChar
 *   http://www.w3.org/TR/2004/REC-xml11-20040204/#NT-NameChar
 * except:
 *   No '.' allowed.
 */
NCCHAR {NCCHAR1}|"-"|[0-9]

/* [89]  NCNAME_PREFIX ::= NCCHAR1p ((NCCHAR|".")* NCCHAR)?
 *
 * This is an Namespaces in XML 1.1 Name except:
 *   No '_' allowed as the first character.
 *   No '.' allowed as the last character.
 */
NCNAME_PREFIX {NCCHAR1p}(({NCCHAR}|".")*{NCCHAR})?

/* [90]  NCNAME ::= NCCHAR1 ((NCCHAR|".")* NCCHAR)?
 *
 * This is an Namespaces in XML 1.1 Name except:
 *   No '.' allowed as the last character.
 */
NCNAME {NCCHAR1}(({NCCHAR}|".")*{NCCHAR})?

/* SPARQL allows [0-9] after a : in the 
 * W3C Candidate Recommendation 14 June 2007 
 * as an at risk feature that may be removed.
 */
NCNAME2 {NCCHAR2}(({NCCHAR}|".")*{NCCHAR})?

/* rq23 [67] QNAME_NS ::= NCNAME_PREFIX? ':'
 * Inlined into in rule <PREF>{NCNAME_PREFIX}":" below 
 */

/* rq23 [68] QNAME ::= NCNAME_PREFIX? ':' NCNAME? */
QNAME {NCNAME_PREFIX}?":"{NCNAME2}?

/* rq23 [69] BNODE_LABEL (renamed to BNAME) ::= '_:' NCNAME */
BNAME "_:"{NCNAME2}

/* a blank node name written inside a URI - this is an illegal
 * URI form and for convienence 
 */
BNAME2 "<_:"{NCNAME2}">"

/* The initial char conditions are to ensure this doesn't grab < or <= 
 * as operators.  starting with <_: is caught by BNAME2 above.
 **/
QUOTEDURI \<[^><= ][^>]*\>|"<>"

INTEGER [0-9]+
DECIMAL [0-9]+"."[0-9]*|"."[0-9]+
DOUBLE [0-9]+"."[0-9]*{EXPONENT}|"."([0-9])+{EXPONENT}|([0-9])+{EXPONENT}
EXPONENT [eE][+-]?[0-9]+


%%
  /* rules */

%{

  int c;
  rasqal_query *rq;
  rasqal_sparql_query_language *rqe;

  rq = (rasqal_query*)yyextra;
  rqe = (rasqal_sparql_query_language*)rq->context;

#ifdef HAVE_SETJMP
  if(setjmp(sparql_lexer_log_error_longjmp_env))
    return 1;
#endif
  
%}

"//"[^\r\n]*(\r\n|\r|\n)	{ /* C++ comment */
        rqe->lineno++;
}

"/*"	{ int lines = sparql_skip_c_comment(rq);
          if(lines < 0)
            yyterminate();
          rqe->lineno += lines;
        }

\r\n|\r|\n     { rqe->lineno++; }

[\ \t\v]+   { /* eat up other whitespace */
	;
}

[Ss][Ee][Ll][Ee][Cc][Tt] { return SELECT; }
[Ff][Rr][Oo][Mm] { return FROM; }
[Ww][Hh][Ee][Rr][Ee] { return WHERE; }
[Pp][Rr][Ee][Ff][Ii][Xx] { BEGIN(PREF);
		return PREFIX; }
[Dd][Ee][Ss][Cc][Rr][Ii][Bb][Ee] { return DESCRIBE; }
[Cc][Oo][Nn][Ss][Tt][Rr][Uu][Cc][Tt] { return CONSTRUCT; }
[Aa][Ss][Kk] { return ASK; }
[Dd][Ii][Ss][Tt][Ii][Nn][Cc][Tt] { return DISTINCT; }
[Rr][Ee][Dd][Uu][Cc][Ee][Dd] { return REDUCED; }
[Ll][Ii][Mm][Ii][Tt] { return LIMIT; }
[Uu][Nn][Ii][Oo][Nn] { return UNION; }
[Oo][Pp][Tt][Ii][Oo][Nn][Aa][Ll] { return OPTIONAL; }
[Bb][Aa][Ss][Ee] { return BASE; }
[Bb][Oo][Uu][Nn][Dd] { return BOUND; }
[Ss][Tt][Rr][Ll][Aa][Nn][Gg] { return STRLANG; }
[Ss][Tt][Rr][Dd][Tt] { return STRDT; }
[Ss][Tt][Rr] { return STR; }
[Ll][Aa][Nn][Gg] { return LANG; }
[Dd][Aa][Tt][Aa][Tt][Yy][Pp][Ee] { return DATATYPE; }
[Ii][Ss][UuIi][Rr][Ii] { return ISURI; } /* isURI and isIRI */
[Ii][Ss][Bb][Ll][Aa][Nn][Kk] { return ISBLANK; }
[Ii][Ss][Ll][Ii][Tt][Ee][Rr][Aa][Ll] { return ISLITERAL; }
[Gg][Rr][Aa][Pp][Hh] { return GRAPH; }
[Nn][Aa][Mm][Ee][Dd] { return NAMED; }
[Ff][Ii][Ll][Tt][Ee][Rr] { return FILTER; }
[Oo][Ff][Ff][Ss][Ee][Tt] { return OFFSET; }
[Oo][Rr][Dd][Ee][Rr] { return ORDER; }
[Bb][Yy] { return BY; }
[Rr][Ee][Gg][Ee][Xx] { return REGEX; }
[Aa][Ss][Cc] { return ASC; }
[Dd][Ee][Ss][Cc] { return DESC; } 
[Ll][Aa][Nn][Gg][Mm][Aa][Tt][Cc][Hh][Ee][Ss] { return LANGMATCHES; } 
[Ee][Xx][Pp][Ll][Aa][Ii][Nn] { return EXPLAIN; }
[Gg][Rr][Oo][Uu][Pp] { return GROUP; }
[Hh][Aa][Vv][Ii][Nn][Gg] { return HAVING; }
[Cc][Oo][Uu][Nn][Tt] { return COUNT; }
[Ss][Uu][Mm] { return SUM; }
[Aa][Vv][Gg] { return AVG; }
[Mm][Ii][Nn] { return MIN; }
[Mm][Aa][Xx] { return MAX; }
[Aa][Ss] { BEGIN(SPID); return AS; }
[Dd][Ee][Ll][Ee][Tt][Ee]  { return DELETE; }
[Ii][Nn][Ss][Ee][Rr][Tt]  { return INSERT; }
[Ss][Aa][Mm][Ee][Tt][Ee][Rr][Mm] { return SAMETERM; }
[Ll][Ee][Tt] { return LET; }
[Cc][Oo][Aa][Ll][Ee][Ss][Cc][Ee] { return COALESCE; }
[Ww][Ii][Tt][Hh] { return WITH; }
[Cc][Ll][Ee][Aa][Rr] { return CLEAR; }
[Cc][Rr][Ee][Aa][Tt][Ee] { return CREATE; }
[Ss][Ii][Ll][Ee][Nn][Tt] { return SILENT; }
[Dd][Aa][Tt][Aa] { return DATA; }
[Dd][Rr][Oo][Pp] { return DROP; }
[Ll][Oo][Aa][Dd] { return LOAD; }
[Ii][Nn][Tt][Oo] { return INTO; }
[Ii][Ff] { return IF; }
[Ii][Rr][Ii] { return IRI; }
[Ur][Rr][Ii] { return URI; }
[Bb][Nn][Oo][Dd][Ee] { return BNODE; }
[Dd][Ee][Ff][Aa][Uu][Ll][Tt] { return DEFAULT; }
[Nn][Oo][Tt] { return NOT; }
[Ii][Nn] { return IN; }
[Gg][Rr][Oo][Uu][Pp]_[Cc][Oo][Nn][Cc][Aa][Tt] { return GROUP_CONCAT; }
[Ss][Ee][Pp][Aa][Rr][Aa][Tt][Oo][Rr] { return SEPARATOR; }
[Bb][Ii][Nn][Dd] { return BIND; }
[Bb][Ii][Nn][Dd][Ii][Nn][Gg][Ss] { return BINDINGS; }
[Uu][Nn][Dd][Ee][Ff] { return UNDEF; }
[Ss][Ee][Rr][Vv][Ii][Cc][Ee] { return SERVICE; }
[Mm][Ii][Nn][Uu][Ss] { return MINUS; }
[Ii][Ss][Nn][Uu][Mm][Ee][Rr][Ii][Cc] { return ISNUMERIC; }
[Ss][Aa][Mm][Pp][Ll][Ee] { return SAMPLE; }
[Yy][Ee][Aa][Rr] { return YEAR; }
[Mm][Oo][Nn][Tt][Hh] { return MONTH; }
[Dd][Aa][Yy] { return DAY; }
[Hh][Oo][Uu][Rr][Ss] { return HOURS; }
[Mm][Ii][Nn][Uu][Tt][Ee][Ss] { return MINUTES; }
[Ss][Ee][Cc][Oo][Nn][Dd][Ss] { return SECONDS; }
[Tt][Ii][Mm][Ee][Zz][Oo][Nn][Ee] { return TIMEZONE; }
[Cc][Uu][Rr][Rr][Ee][Nn][Tt][__][Dd][Aa][Tt][Ee][Tt][Ii][Mm][Ee] { return CURRENT_DATETIME; }
[Nn][Oo][Ww] { return NOW; }
[Ff][Rr][Oo][Mm][__][Uu][Nn][Ii][Xx][Tt][Ii][Mm][Ee] { return FROM_UNIXTIME; }
[Tt][Oo][__][Uu][Nn][Ii][Xx][Tt][Ii][Mm][Ee] { return TO_UNIXTIME; }
[Cc][Oo][Nn][Cc][Aa][Tt] { return CONCAT; }
[Ss][Tt][Rr][Ll][Ee][Nn] { return STRLEN; }
[Ss][Uu][Bb][Ss][Tt][Rr] { return SUBSTR; }
[Uu][Cc][Aa][Ss][Ee] { return UCASE; }
[Ll][Cc][Aa][Ss][Ee] { return LCASE; }
[Ss][Tt][Rr][Ss][Tt][Aa][Rr][Tt][Ss] { return STRSTARTS; }
[Ss][Tt][Rr][Ee][Nn][Dd][Ss] { return STRENDS; }
[Cc][Oo][Nn][Tt][Aa][Ii][Nn][Ss] { return CONTAINS; }
[Ee][Nn][Cc][Oo][Dd][Ee][__][Ff][Oo][Rr][__][Uu][Rr][Ii] { return ENCODE_FOR_URI; }
[Tt][Oo] { return TO; }
[Aa][Dd][Dd] { return ADD; }
[Mm][Oo][Vv][Ee] { return MOVE; }
[Cc][Oo][Pp][Yy] { return COPY; }
[Aa][Ll][Ll] { return ALL; }
[Tt][Zz] { return TZ; }
[Rr][Aa][Nn][Dd] { return RAND; }
[Aa][Bb][Ss] { return ABS; }
[Rr][Oo][Uu][Nn][Dd] { return ROUND; }
[Cc][Ee][Ii][Ll] { return CEIL; }
[Ff][Ll][Oo][Oo][Rr] { return FLOOR; }
[Mm][Dd][55] { return MD5; }
[Ss][Hh][Aa][11] { return SHA1; }
[Ss][Hh][Aa][22][22][44] { return SHA224; }
[Ss][Hh][Aa][22][55][66] { return SHA256; }
[Ss][Hh][Aa][33][88][44] { return SHA384; }
[Ss][Hh][Aa][55][11][22] { return SHA512; }
[Ss][Tt][Rr][Bb][Ee][Ff][Oo][Rr][Ee] { return STRBEFORE; }
[Ss][Tt][Rr][Aa][Ff][Tt][Ee][Rr] { return STRAFTER; }
[Rr][Ee][Pp][Ll][Aa][Cc][Ee] { return REPLACE; }
[Uu][Uu][Ii][Dd] { return UUID; }
[Ss][Tt][Rr][Uu][Uu][Ii][Dd] { return STRUUID; }
[Vv][Aa][Ll][Uu][Ee][Ss] { return VALUES; }

"a" { return A; }

","      { return ','; } 
"("      { return '('; } 
")"      { return ')'; } 
"["       { return '['; }
"]"       { return ']'; }
"?"  { BEGIN(ID); return '?'; }
"$"  { BEGIN(ID); return '$'; }
"{"      { return '{'; } 
"}"      { return '}'; } 
"."      { return '.'; } 
";"      { return ';'; } 

"||"         { return SC_OR; }
"&&"         { return SC_AND; }

"="            { return EQ; }
"!="            { return NEQ; }
"<"/[^A-Za-z=>]  { return LT; }
">"             { return GT; }
"<="         { return LE; }
">="         { return GE; }

"*"         { return '*'; }
"/"         { return '/'; }
"!"         { return '!'; }

":="         { return ASSIGN; }

{LANGTAG} {
			yylval->name = RASQAL_MALLOC(unsigned char*, yyleng);
                        if(!yylval->name)
                          YY_REPORT_ERROR_EOF("Out of memory");

                        memcpy(yylval->name, yytext + 1, yyleng - 1);
                        yylval->name[yyleng - 1] = '\0';
                  return LANGTAG;
		}

"^^"         { return HATHAT; }


[-+]?{INTEGER}   { c = *yytext;
                  yylval->literal = rasqal_new_typed_literal(rq->world, RASQAL_LITERAL_INTEGER, RASQAL_GOOD_CAST(const unsigned char*, yytext));
                  if(!yylval->literal)
                    YY_REPORT_ERROR_EOF("rasqal_new_typed_literal failed");
 		  return (c=='+' ? INTEGER_POSITIVE_LITERAL : (c == '-' ? INTEGER_NEGATIVE_LITERAL : INTEGER_LITERAL));
}

[-+]?{DECIMAL}  { 
                        c = *yytext;
                        
                        if(!rasqal_xsd_datatype_check(RASQAL_LITERAL_DECIMAL, RASQAL_GOOD_CAST(const unsigned char*, yytext), 0)) {
                          sparql_syntax_error(rq, "SPARQL syntax error - Illegal decimal constant %s", yytext);
                          yyterminate();
                        }
                        yylval->literal = rasqal_new_decimal_literal(rq->world, RASQAL_GOOD_CAST(const unsigned char*, yytext));
                        if(!yylval->literal)
                          YY_REPORT_ERROR_EOF("rasqal_new_decimal_literal failed");
                        return (c=='+' ? DECIMAL_POSITIVE_LITERAL : (c == '-' ? DECIMAL_NEGATIVE_LITERAL : DECIMAL_LITERAL));
}

[-+]?{DOUBLE} {
                        c = *yytext;
                        if(!rasqal_xsd_datatype_check(RASQAL_LITERAL_DOUBLE, RASQAL_GOOD_CAST(const unsigned char*, yytext), 0)) {
                          sparql_syntax_error(rq, "SPARQL syntax error - Illegal double constant %s", yytext);
                          yyterminate();
                        }
                        yylval->literal = rasqal_new_typed_literal(rq->world, RASQAL_LITERAL_DOUBLE, RASQAL_GOOD_CAST(const unsigned char*, yytext));
                        if(!yylval->literal)
                          YY_REPORT_ERROR_EOF("rasqal_new_typed_literal failed");
                        return (c=='+' ? DOUBLE_POSITIVE_LITERAL : (c == '-' ? DOUBLE_NEGATIVE_LITERAL : DOUBLE_LITERAL));
}

"+"         { return '+'; }
"-"         { return '-'; }

'([^'\\\n\r]|\\[^\n\r])*'    { /*' */
                        yylval->name = sparql_copy_string_token(rq, RASQAL_GOOD_CAST(unsigned char*, yytext) + 1, yyleng - 2, '\'');
                        if(!yylval->name)
                          YY_REPORT_ERROR_EOF("sparql_copy_string_token failed");
                        return STRING; }

\"([^"\\\n\r]|\\[^\n\r])*\"   { /* " */
                        yylval->name = sparql_copy_string_token(rq, RASQAL_GOOD_CAST(unsigned char*, yytext) + 1, yyleng - 2, '"');
                        if(!yylval->name)
                          YY_REPORT_ERROR_EOF("sparql_copy_string_token failed");
                        return STRING; }

\"\"\"				{ BEGIN(LITERAL); 
                                  rqe->sb = raptor_new_stringbuffer();
                                  if(!rqe->sb)
                                    YY_REPORT_ERROR_EOF("raptor_new_stringbuffer failed");
                          }

<LITERAL>\"\"\"			{
		  size_t len;
     
		  BEGIN(INITIAL);
                  len = raptor_stringbuffer_length(rqe->sb);
                  yylval->name = RASQAL_MALLOC(unsigned char*, len + 1);
                  if(!yylval->name)
                    YY_REPORT_ERROR_EOF("Out of memory");

                  raptor_stringbuffer_copy_to_string(rqe->sb,
                                                     RASQAL_GOOD_CAST(unsigned char*, yylval->name), len);
                  yylval->name[len] = '\0';

                  raptor_free_stringbuffer(rqe->sb);
                  rqe->sb = NULL;
                  return STRING; }

<LITERAL>\"|(\\.|[^\"\\]|\n)*	{
		  char *p;

		  if(*yytext == EOF) {
                    BEGIN(INITIAL);
                    sparql_syntax_error(rq, "End of file in middle of literal");
                    raptor_free_stringbuffer(rqe->sb);
                    rqe->sb = NULL;
                    return EOF;
                  }

		  for(p = yytext; *p; p++) {
                    if(*p == '\n')
                      rqe->lineno++;
                  }

                  if(sparql_stringbuffer_append_sparql_string(rq, rqe->sb, RASQAL_GOOD_CAST(unsigned char*, yytext), yyleng, '"')) { /* " */
                    BEGIN(INITIAL);
                    raptor_free_stringbuffer(rqe->sb);
                    rqe->sb = NULL;
                    YY_REPORT_ERROR_EOF("sparql_stringbuffer_append_sparql_string failed");
                  }
                  
   }

<LITERAL>\\	{
     		/* this should only happen if \ is at the end of the file so the Turtle doc is illegal anyway */
                sparql_syntax_error(rq, "End of file in middle of literal");
}

\'\'\'				{ BEGIN(LITERAL2); 
                                  rqe->sb = raptor_new_stringbuffer();
                                  if(!rqe->sb) {
                                    BEGIN(INITIAL);
                                    YY_REPORT_ERROR_EOF("raptor_new_stringbuffer failed");
                                  }
                          }

<LITERAL2>\'\'\'			{
		  size_t len;
     
		  BEGIN(INITIAL);
                  len = raptor_stringbuffer_length(rqe->sb);
                  yylval->name = RASQAL_MALLOC(unsigned char*, len + 1);
                  if(!yylval->name)
                    YY_REPORT_ERROR_EOF("Out of memory");

                  raptor_stringbuffer_copy_to_string(rqe->sb, RASQAL_GOOD_CAST(unsigned char*, yylval->name), len);
                  yylval->name[len] = '\0';

                  raptor_free_stringbuffer(rqe->sb);
                  rqe->sb = NULL;
                  return STRING; }

<LITERAL2>\'|(\\.|[^\'\\]|\n)*	{
		  char *p;

		  if(*yytext == EOF) {
                    BEGIN(INITIAL);
                    sparql_syntax_error(rq, "End of file in middle of \"\"\" literal");
                    raptor_free_stringbuffer(rqe->sb);
                    rqe->sb = NULL;
                    return EOF;
                  }

		  for(p = yytext; *p; p++) {
                    if(*p == '\n')
                      rqe->lineno++;
                  }

                  if(sparql_stringbuffer_append_sparql_string(rq, rqe->sb, RASQAL_GOOD_CAST(unsigned char*, yytext), yyleng, '\'')) {
                    BEGIN(INITIAL);
                    raptor_free_stringbuffer(rqe->sb);
                    rqe->sb = NULL;
                    YY_REPORT_ERROR_EOF("sparql_stringbuffer_append_sparql_string failed");
                  }
                  
   }

<LITERAL2>\\	{
     		/* this should only happen if \ is at the end of the file so the Turtle doc is illegal anyway */
                sparql_syntax_error(rq, "End of file in middle of ''' literal");
}



[Tt][Rr][Uu][Ee]	{ yylval->literal = rasqal_new_boolean_literal(rq->world, 1);
                    if(!yylval->literal)
                      YY_REPORT_ERROR_EOF("rasqal_new_boolean_literal failed");
                  return BOOLEAN_LITERAL; }

[Ff][Aa][Ll][Ss][Ee]	{ yylval->literal = rasqal_new_boolean_literal(rq->world, 0);
                        if(!yylval->literal)
                          YY_REPORT_ERROR_EOF("rasqal_new_boolean_literal failed");
                  return BOOLEAN_LITERAL; }

<ID>{VARNAME}	{ yylval->name = sparql_copy_name(rq, RASQAL_GOOD_CAST(const unsigned char*, yytext), yyleng, SPARQL_NAME_CHECK_VARNAME);
		  if(!yylval->name)
                    yyterminate();

                          BEGIN(INITIAL);
                          return IDENTIFIER; }
<ID>(.|\n)	{	BEGIN(INITIAL);
		sparql_syntax_error(rq, "SPARQL syntax error - missing variable name after ?");
                yyterminate();
}

<SPID>[\ \t\v]+	{ /* eat up leading whitespace */
	;
}

<SPID>\? {
	return '?';
}

<SPID>\$ {
	return '$';
}

<SPID>{VARNAME}	{ yylval->name = sparql_copy_name(rq, RASQAL_GOOD_CAST(const unsigned char*, yytext), yyleng, SPARQL_NAME_CHECK_VARNAME);
		  if(!yylval->name)
                    yyterminate();

                          BEGIN(INITIAL);
                          return IDENTIFIER; }

<SPID>(.|\n)	{	BEGIN(INITIAL);
		sparql_syntax_error(rq, "SPARQL syntax error - missing variable name after ?");
                yyterminate();
}

<PREF>[\ \t\v]+ { /* eat up leading whitespace */ }
<PREF>{NCNAME_PREFIX}":"	{ BEGIN(INITIAL);
		  	  yylval->name = sparql_copy_name(rq, RASQAL_GOOD_CAST(const unsigned char*, yytext), yyleng - 1, SPARQL_NAME_CHECK_PREFIX);
			  if(!yylval->name)
			    yyterminate();

                          return IDENTIFIER; }
<PREF>":"	{ BEGIN(INITIAL);
		  yylval->name = NULL;
                  return IDENTIFIER; }

<PREF>(.|\n)	{ BEGIN(INITIAL);
		  if(!*yytext)
                    return EOF;

                  sparql_syntax_error(rq, "SPARQL syntax error at '%c'", *yytext);
                  yyterminate();
}

{QNAME}\(?	{
		int have_brace = (yytext[yyleng-1]=='(');
		if(have_brace)
			yyleng--;
		yylval->uri = sparql_copy_qname(rq, RASQAL_GOOD_CAST(const unsigned char*, yytext), yyleng);
		if(!yylval->uri)
		  yyterminate();

 		return have_brace ? URI_LITERAL_BRACE : URI_LITERAL;
}

{BNAME}	{	yylval->name = sparql_copy_name(rq, RASQAL_GOOD_CAST(unsigned char*, yytext) + 2, yyleng - 2, SPARQL_NAME_CHECK_NCNAME);
		if(!yylval->name)
		  yyterminate();

		return BLANK_LITERAL;
}

{BNAME2}	{	yylval->name = sparql_copy_name(rq, RASQAL_GOOD_CAST(unsigned char*, yytext) + 3, yyleng - 4, SPARQL_NAME_CHECK_NCNAME);
		if(!yylval->name)
		  yyterminate();

		return BLANK_LITERAL;
}

{QUOTEDURI}\(?   { 
		int have_brace = (yytext[yyleng-1]=='(');
		if(have_brace)
			yyleng--;
		if(yyleng == 2) 
                  yylval->uri = raptor_uri_copy(rq->base_uri);
                else {
                  unsigned char* uri_string;

                  yytext[yyleng-1] = '\0';
                  uri_string = rasqal_escaped_name_to_utf8_string(RASQAL_GOOD_CAST(unsigned char*, yytext) + 1,
                                                                  yyleng-1,
                                                                  NULL,
                                                                  sparql_syntax_error, rq);
                  if(!uri_string)
                    YY_REPORT_ERROR_EOF("rasqal_escaped_name_to_utf8_string failed");

                  yylval->uri = raptor_new_uri_relative_to_base(rq->world->raptor_world_ptr, rq->base_uri, uri_string);
                  RASQAL_FREE(char*, uri_string);
                  if(!yylval->uri)
                    YY_REPORT_ERROR_EOF("raptor_new_uri_relative_to_base failed");
                 }
                 return have_brace ? URI_LITERAL_BRACE : URI_LITERAL; }

\#[^\r\n]*(\r\n|\r|\n)	{ /* # comment */
        	rqe->lineno++;
                }

.         	{ if(!*yytext)
                    return EOF;

                  sparql_syntax_error(rq, "SPARQL syntax error at '%c'", *yytext);
                  yyterminate();
		}

%%
  /* user code */

int
yywrap (yyscan_t yyscanner)
{
  return 1;
}


static int
rasqal_sparql_name_check(unsigned char *string, size_t length,
                         sparql_name_check_flags check_flags)
{
  int rc = 0;
  int c = -1;
#if defined(RASQAL_DEBUG) && RASQAL_DEBUG > 2
  RASQAL_DEBUG1("Checking name '");
  if(length)
     fwrite(string, length, sizeof(unsigned char), stderr);
  fprintf(stderr, "' (length %d), flags %d\n", RASQAL_BAD_CAST(int, length), RASQAL_GOOD_CAST(int, check_flags));
#endif

  if(!length)
    return 1;

  if((check_flags & SPARQL_NAME_CHECK_ALLOW_09_FIRST) &&
     (*string >= '0' && *string <= '9')) {
    c = *string;
    *string = 'X';
  }

  if(!raptor_xml_name_check(string, length, 11)) /* 11 = XML 1.1 */
    goto done;
  
  if((check_flags & SPARQL_NAME_CHECK_NO_UL_FIRST) && *string == '_')
    goto done;

  if((check_flags & SPARQL_NAME_CHECK_NO_DOT_LAST) && string[length-1] == '.')
    goto done;

  if(check_flags & SPARQL_NAME_CHECK_NO_DOT_MINUS) {
    size_t i;
    for(i = 0; i < length; i++)
      if(string[i] == '.' || string[i] == '-')
        goto done;
  }
  rc = 1;

  done:
  if(c >= 0)
    *string = c;
  return rc;
}


static unsigned char *
sparql_copy_name(rasqal_query *rq, const unsigned char *text, size_t len,
                 sparql_name_check_flags check_flags)
{
  size_t dest_len = 0;
  unsigned char *s;

  s = rasqal_escaped_name_to_utf8_string(RASQAL_GOOD_CAST(unsigned char*, text),
                                         len, &dest_len,
                                         sparql_syntax_error, rq);
  if(!s) {
    sparql_syntax_error(rq, "Failed to decode SPARQL string \"%s\"", text);
    return s;
  }

  if(!rasqal_sparql_name_check(s, dest_len, check_flags))
    sparql_syntax_error(rq, "Invalid SPARQL name \"%s\"", s);

  return s;
}


static raptor_uri*
sparql_copy_qname(rasqal_query *rq, const unsigned char *text, size_t len)
{
  unsigned char *p;
  size_t dest_len = 0;
  unsigned char *s;
  raptor_uri* uri = NULL;
  
  s = rasqal_escaped_name_to_utf8_string(RASQAL_GOOD_CAST(unsigned char*, text),
                                         len, &dest_len,
                                         sparql_syntax_error, rq);
  if(!s) {
    sparql_syntax_error(rq, "Failed to decode SPARQL ID \"%s\"", text);
    return NULL;
  }

  p = RASQAL_GOOD_CAST(unsigned char*, strchr(RASQAL_GOOD_CAST(const char*, s), ':'));
  if(!rasqal_sparql_name_check(s, p-s, SPARQL_NAME_CHECK_PREFIX))
    sparql_syntax_error(rq, "Invalid SPARQL prefix name \"%s\"", s);
  if(!rasqal_sparql_name_check(p+1, dest_len-((p+1)-s), SPARQL_NAME_CHECK_NCNAME))
    sparql_syntax_error(rq, "Invalid SPARQL local name \"%s\"", p+1);

#ifdef STANDALONE
  /* lexer test cannot declare namespaces - so just ignore expansion */
  uri = raptor_new_uri_relative_to_base(rq->world->raptor_world_ptr, rq->base_uri, s);
#else /* not */
  if(!rq->namespaces) {
    sparql_syntax_error(rq, "SPARQL syntax error - no namespaces declared");
    return NULL;
  }
  
  uri = raptor_qname_string_to_uri(rq->namespaces,
                                   s, dest_len);
#endif /* STANDALONE */
  RASQAL_FREE(char*, s);
  
  return uri;
}


static unsigned char*
sparql_copy_string_token(rasqal_query* rq,
                         unsigned char *string, size_t len, int delim)
{
  raptor_stringbuffer* sb = NULL;
  int rc;
  
  if(len) {
    sb = raptor_new_stringbuffer();
    if(!sb)
      return NULL;
    
    rc = sparql_stringbuffer_append_sparql_string(rq, sb, string, len, delim);
    if(rc) {
      raptor_free_stringbuffer(sb);
      return NULL;
    }

    len = raptor_stringbuffer_length(sb);
  }
  
  string = RASQAL_MALLOC(unsigned char*, len + 1);
  if(string) {
    if(sb) 
      raptor_stringbuffer_copy_to_string(sb, string, len + 1);
    string[len]='\0';
  }

  if(sb)
    raptor_free_stringbuffer(sb);
  
  return string;
}


static int
sparql_skip_c_comment(rasqal_query *rq)
{
  rasqal_sparql_query_language *rqe;
  yyscan_t yyscanner;
  int lines = 0;
  int lastc = -1;
  
  rqe = (rasqal_sparql_query_language*)rq->context;
  yyscanner = rqe->scanner;

  while(1) {
    int c = INPUT_FN(yyscanner);
    
    if(c == EOF) {
      /* EOF */
      sparql_syntax_error(rq, "SPARQL syntax error - EOF in comment");
      lines = -1;
      break;
    }

    if(c == '\r' || (c == '\n' && lastc != '\r'))
      lines++;

    if(lastc == '*' && c == '/')
      break;

    lastc =  c;
  }
  return lines;
}


/**
 * sparql_stringbuffer_append_sparql_string:
 * @rq: rasqal query
 * @stringbuffer: String buffer to add to
 * @text: turtle string to decode
 * @len: length of string
 * @delim: terminating delimiter for string - only ', " or &gt; are allowed
 *
 * INTERNAL - Append to a stringbuffer a SPARQL-escaped string.
 *
 * The passed in string is handled according to the SPARQL string
 * escape rules giving a UTF-8 encoded output of the Unicode codepoints.
 *
 * The SPARQL escapes are \n \r \f \b \t \\
 * \uXXXX \UXXXXXXXX where X is [A-F0-9]
 * and \ followed by the @delim char
 * 
 * Return value: non-0 on failure
 **/
static int
sparql_stringbuffer_append_sparql_string(rasqal_query* rq,
                                         raptor_stringbuffer* stringbuffer,
                                         const unsigned char *text,
                                         size_t len, int delim)
{
  size_t i;
  const unsigned char *s;
  unsigned char *d;
  unsigned char *string;

  string = RASQAL_MALLOC(unsigned char*, len + 1);
  
  if(!string)
    return -1;

  for(s = text, d = string, i = 0; i < len; s++, i++) {
    unsigned char c = *s;

    if(c == '\\' ) {
      s++; i++;
      c = *s;
      if(c == 'n')
        *d++ = '\n';
      else if(c == 'r')
        *d++ = '\r';
      else if(c == 'f')
        *d++ = '\f';
      else if(c == 'b')
        *d++ = '\b';
      else if(c == 't')
        *d++ = '\t';
      else if(c == '\\' || c == delim)
        *d++ = c;
      else if(c == 'u' || c == 'U') {
        size_t ulen = (c == 'u') ? 4 : 8;
        unsigned long unichar = 0;
        int n;
        
        s++; i++;
        if(i + ulen > len) {
          sparql_syntax_error(rq, "SPARQL string \\%c over end of line", c);
          RASQAL_FREE(char*, string);
          return 1;
        }
        
        n = sscanf(RASQAL_GOOD_CAST(const char*, s), ((ulen == 4) ? "%04lx" : "%08lx"), &unichar);
        if(n != 1) {
          sparql_syntax_error(rq, "Bad SPARQL string Uncode escape '%c%s...'",
                              c, s);
          RASQAL_FREE(char*, string);
          return 1;
        }

        s += ulen - 1;
        i += ulen - 1;
        
        if(unichar > rasqal_unicode_max_codepoint) {
          sparql_syntax_error(rq,
                              "Bad SPARQL string Unicode character with code point #x%lX (max #x%lX).", 
                              unichar, rasqal_unicode_max_codepoint);
          RASQAL_FREE(char*, string);
          return 1;
        }
          
        d += raptor_unicode_utf8_string_put_char(unichar, d, len - (d - string));

      } else {
        /* Ignore \x where x isn't the one of: \n \r \t \\ (delim) \u \U */
        sparql_syntax_warning(rq, "Unknown SPARQL string escape \\%c in \"%s\"", c, text);
        *d++ = c;
      }
    } else
      *d++ = c;
  }
  *d='\0';

  /* calculate output string size */
  len = d-string;
  
  /* string gets owned by the stringbuffer after this */
  return raptor_stringbuffer_append_counted_string(stringbuffer, 
                                                   string, len, 0);

}


/*
 * sparql_lexer_log_error:
 * @msg: message string
 * @level: log level
 * @yyscanner: scanner context
 *
 * INTERNAL - replacement for the generated error handler.
 * Uses rasqal_log_error_simple() when possible.
 */
static void sparql_lexer_log_error(yyconst char *msg,
                                   raptor_log_level level,
                                   yyscan_t yyscanner)
{
  rasqal_query *rq = NULL;

  if(yyscanner)
    rq = (rasqal_query *)sparql_lexer_get_extra(yyscanner);

  if(rq) {
    /* avoid "format not a string literal and no format arguments" warning with %s */
    rq->failed = 1;
    rasqal_log_error_simple(rq->world, level, &rq->locator, "%s", msg); 
  } else {
    fputs(msg, stderr);
    fputc('\n', stderr);
  }
}


/* Define LEXER_ALLOC_TRACKING to enable allocated memory tracking
 * - fixes lexer memory leak when ensure_buffer_stack fails
 */

#ifdef LEXER_ALLOC_TRACKING
typedef struct {
  /* Number of void* slots allocated */
  int lexer_allocs_size;
  /* Allocted void* slots follow in memory after this header */
} lexer_alloc_tracker_header;

/* Initial alloc tracker slot array size - 2 seems to be enough for almost all cases */
static const int initial_lexer_allocs_size = 2;
#endif

/*
 * sparql_lexer_cleanup:
 * @yyscanner:
 *
 * INTERNAL - Clean up unfreed lexer allocs if LEXER_ALLOC_TRACKING is enabled.
 */
static void sparql_lexer_cleanup(yyscan_t yyscanner)
{
#ifdef LEXER_ALLOC_TRACKING
  rasqal_query *rq;
  lexer_alloc_tracker_header *tracker;
  void **lexer_allocs;
  int i;

  if(!yyscanner)
    return;

  rq = (rasqal_query *)sparql_lexer_get_extra(yyscanner);
  if(!rq)
    return;

  tracker = (lexer_alloc_tracker_header *)rq->lexer_user_data;
  if(!tracker)
    return;
  lexer_allocs = (void**)&tracker[1];

  for(i = 0; i<tracker->lexer_allocs_size; ++i) {
    if(lexer_allocs[i])
      free(lexer_allocs[i]);
    lexer_allocs[i] = NULL;
  }
  free(rq->lexer_user_data);
  rq->lexer_user_data = NULL;
#endif
}


/*
 * sparql_lexer_alloc:
 * @size
 * @yyscanner
 *
 * INTERNAL - alloc replacement.
 * Tracks allocated cells if LEXER_ALLOC_TRACKING is enabled.
 */
void *sparql_lexer_alloc(yy_size_t size, yyscan_t yyscanner)
{
#ifdef LEXER_ALLOC_TRACKING
  rasqal_query *rq;
  lexer_alloc_tracker_header *tracker;
  void **lexer_allocs;
  int i;
  void *ptr;

  /* yyscanner not initialized -> probably initializing yyscanner itself
   * -> just malloc without tracking
   */
  if(!yyscanner)
    return malloc(size);

  rq = (rasqal_query *)sparql_lexer_get_extra(yyscanner);
  if(!rq)
    YY_FATAL_ERROR("lexer_alloc: yyscanner extra not initialized");

  /* try to allocate tracker if it does not exist */
  tracker = (lexer_alloc_tracker_header *)rq->lexer_user_data;
  if(!tracker) {
    /* allocate tracker header + array of void* slots */
    tracker = (lexer_alloc_tracker_header*)calloc(1, sizeof(lexer_alloc_tracker_header) + initial_lexer_allocs_size * sizeof(void*));
    if(!tracker)
      YY_FATAL_ERROR("lexer_alloc: cannot allocate tracker");
    tracker->lexer_allocs_size = initial_lexer_allocs_size;
    rq->lexer_user_data = (void *)tracker;
  }
  lexer_allocs = (void**)&tracker[1];

  /* allocate memory */
  ptr = malloc(size);
  
  /* find a free slot for ptr */
  for(i = 0; i<tracker->lexer_allocs_size; ++i) {
    if(!lexer_allocs[i]) {
      lexer_allocs[i] = ptr;
      break;
    }
  }

  /* no free slots -> grow tracker slot array */
  if(i >= tracker->lexer_allocs_size) {
    int j;
    void **dest;
    tracker = (lexer_alloc_tracker_header*)calloc(1, sizeof(lexer_alloc_tracker_header) + i * 2 * sizeof(void*));
    if(!tracker) {
      if(ptr)
        free(ptr);
      YY_FATAL_ERROR("lexer_alloc: cannot grow tracker");
    }
    tracker->lexer_allocs_size = i*2;
    
    /* copy data from old tracker */
    dest = (void**)&tracker[1];
    for(j = 0; j<i; ++j) {
      dest[j] = lexer_allocs[j];
    }
    
    /* set new item to first free slot */
    dest[j] = ptr;

    /* free old tracker and replace with new one */
    free(rq->lexer_user_data);
    rq->lexer_user_data = tracker;
  }

  return ptr;
#else
  return malloc(size);
#endif
}


/*
 * sparql_lexer_realloc:
 *
 * INTERNAL - realloc replacement
 * Tracks allocated cells if LEXER_ALLOC_TRACKING is enabled.
 */
void *sparql_lexer_realloc(void *ptr, yy_size_t size, yyscan_t yyscanner)
{
#ifdef LEXER_ALLOC_TRACKING
  rasqal_query *rq;
  lexer_alloc_tracker_header *tracker;
  void **lexer_allocs;
  int i;
  void *newptr;

  if(!yyscanner)
    YY_FATAL_ERROR("lexer_realloc: yyscanner not initialized");

  rq = (rasqal_query *)sparql_lexer_get_extra(yyscanner);
  if(!rq)
    YY_FATAL_ERROR("lexer_realloc: yyscanner extra not initialized");

  tracker = (lexer_alloc_tracker_header *)rq->lexer_user_data;
  if(!tracker)
    YY_FATAL_ERROR("lexer_realloc: no alloc tracker");
  lexer_allocs = (void**)&tracker[1];

  /* find the old slot for ptr */
  for(i = 0; i < tracker->lexer_allocs_size; ++i) {
    if(lexer_allocs[i] == ptr)
      break;
  }

  /* no old slot -> error */  
  if(i >= tracker->lexer_allocs_size)
    YY_FATAL_ERROR("lexer_realloc: cell not in tracker");

  /* realloc */
  newptr = realloc(RASQAL_GOOD_CAST(char*, ptr), size);

  /* replace entry in tracker */
  lexer_allocs[i] = newptr;

  return newptr;
#else
  return realloc(RASQAL_GOOD_CAST(char*, ptr), size);
#endif
}


/*
 * sparql_lexer_free:
 *
 * INTERNAL - free replacement.
 * Checks for NULL pointer to be freed unlike the default lexer free function.
 * Tracks allocated cells if LEXER_ALLOC_TRACKING is enabled.
 */
void sparql_lexer_free(void *ptr, yyscan_t yyscanner)
{
#ifdef LEXER_ALLOC_TRACKING
  rasqal_query *rq;
  lexer_alloc_tracker_header *tracker;
  void **lexer_allocs;
  int i;

  /* do not free NULL */
  if(!ptr)
    return;

  /* free ptr even if we would encounter an error */
  free(ptr);

  /* yyscanner is allocated with sparql_lexer_alloc() but it's never stored in the tracker
   * - we need yyscanner to access the tracker */
  if(!yyscanner || ptr==yyscanner)
    return;

  rq = (rasqal_query *)sparql_lexer_get_extra(yyscanner);
  if(!rq)
    return;

  tracker = (lexer_alloc_tracker_header *)rq->lexer_user_data;
  if(!tracker)
    return;
  lexer_allocs = (void**)&tracker[1];

  /* find the slot for ptr */
  for(i = 0; i<tracker->lexer_allocs_size; ++i) {
    if(lexer_allocs[i]==ptr)
      break;
  }

  /* no slot -> error */  
  if(i >= tracker->lexer_allocs_size)
    YY_FATAL_ERROR("lexer_free: cell not in tracker");

  /* remove entry from tracker */
  lexer_allocs[i] = NULL;
#else
  if(ptr)
    free(ptr);
#endif
}


#ifdef RASQAL_DEBUG

const char *
sparql_token_print(rasqal_world* world, int token, YYSTYPE *lval)
{
  static char buffer[2048];

  if(!token)
    return "<<EOF>>";
  
  switch(token) {
    case SELECT:
      return "SELECT";

    case FROM:
      return "FROM";

    case WHERE:
      return "WHERE";

    case PREFIX:
      return "PREFIX";

    case DESCRIBE:
      return "DESCRIBE";

    case CONSTRUCT:
      return "CONSTRUCT";

    case ASK:
      return "ASK";

    case DISTINCT:
      return "DISTINCT";

    case LIMIT:
      return "LIMIT";

    case UNION:
      return "UNION";

    case OPTIONAL:
      return "OPTIONAL";

    case BASE:
      return "BASE";

    case BOUND:
      return "BOUND";

    case STRLANG:
      return "STRLANG";
      
    case STRDT:
      return "STRDT";
      
    case STR:
      return "STR";
      
    case LANG:
      return "LANG";
      
    case DATATYPE:
      return "DATATYPE";
      
    case ISURI:
      return "ISURI";
      
    case ISBLANK:
      return "ISBLANK";
      
    case ISLITERAL:
      return "ISLITERAL";
      
    case GRAPH:
      return "GRAPH";
      
    case NAMED:
      return "NAMED";
      
    case FILTER:
      return "FILTER";

    case OFFSET:
      return "OFFSET";
      
    case A:
      return "a";
      
    case ORDER:
      return "ORDER";
      
    case BY:
      return "BY";
      
    case REGEX:
      return "REGEX";
      
    case ASC:
      return "ASC[";
      
    case DESC:
      return "DESC[";
      
    case LANGMATCHES:
      return "LANGMATCHES";
      
    case ',':
      return ",";

    case '(':
      return "(";

    case ')':
      return ")";

    case '[':
      return "[";

    case ']':
      return "]";

    case '{':
      return "{";

    case '}':
      return "}";

    case '.':
      return ".";

    case ';':
      return ";";

    case '?':
      return "?";

    case '$':
      return "$";

    case SC_AND:
      return "SC_AND";

    case SC_OR:
      return "SC_OR";

    case GE:
      return "GE";

    case LE:
      return "LE";

    case GT:
      return "GT";

    case LT:
      return "LT";

    case NEQ:
      return "NEQ";

    case EQ:
      return "EQ";

    case '/':
      return "/";

    case '*':
      return "*";

    case '-':
      return "-";

    case '+':
      return "+";

    case '!':
      return "!";

    case EXPLAIN:
      return "EXPLAIN";

    case GROUP:
      return "GROUP";

    case HAVING:
      return "HAVING";

    case COUNT:
      return "COUNT";

    case SUM:
      return "SUM";

    case AVG:
      return "AVG";

    case MIN:
      return "MIN";

    case MAX:
      return "MAX";

    case DELETE:
      return "DELETE";

    case INSERT:
      return "INSERT";

    case WITH:
      return "WITH";

    case CLEAR:
      return "CLEAR";

    case CREATE:
      return "CREATE";

    case SILENT:
      return "SILENT";

    case DATA:
      return "DATA";
      
    case DROP:
      return "DROP";
      
    case LOAD:
      return "LOAD";
      
    case INTO:
      return "INTO";
      
    case IF:
      return "IF";

    case LET:
      return "LET";

    case AS:
      return "AS";

    case ASSIGN:
      return ":=";

    case COALESCE:
      return "COALESCE";

    case IRI:
      return "IRI";

    case URI:
      return "URI";

    case BNODE:
      return "BNODE";

    case SAMPLE:
      return "SAMPLE";

    case DEFAULT:
      return "DEFAULT";

    case NOT:
      return "NOT";

    case IN:
      return "IN";

    case GROUP_CONCAT:
      return "GROUP_CONCAT";

    case SEPARATOR:
      return "SEPARATOR";

    case BINDINGS:
      return "BINDINGS";

    case UNDEF:
      return "UNDEF";

    case SERVICE:
      return "SERVICE";

    case MINUS:
      return "MINUS";

    case ISNUMERIC:
      return "isNUMERIC";

    case YEAR:
      return "YEAR";

    case MONTH:
      return "MONTH";

    case DAY:
      return "DAY";

    case HOURS:
      return "HOURS";

    case MINUTES:
      return "MINUTES";

    case SECONDS:
      return "SECONDS";

    case TIMEZONE:
      return "TIMEZONE";

    case CURRENT_DATETIME:
      return "CURRENT_DATETIME";

    case NOW:
      return "NOW";

    case FROM_UNIXTIME:
      return "FROM_UNIXTIME";

    case TO_UNIXTIME:
      return "TO_UNIXTIME";

    case CONCAT:
      return "CONCAT";

    case STRLEN:
      return "STRLEN";

    case SUBSTR:
      return "SUBSTR";

    case UCASE:
      return "UCASE";

    case LCASE:
      return "LCASE";

    case STRSTARTS:
      return "STRSTARTS";

    case STRENDS:
      return "STRENDS";

    case CONTAINS:
      return "CONTAINS";

    case ENCODE_FOR_URI:
      return "ENCODE_FOR_URI";

    case TO:
      return "TO";

    case ADD:
      return "ADD";

    case MOVE:
      return "MOVE";

    case COPY:
      return "COPY";

    case ALL:
      return "ALL";

    case TZ:
      return "TZ";

    case RAND:
      return "RAND";

    case BIND:
      return "BIND";

    case ABS:
      return "ABS";

    case ROUND:
      return "ROUND";

    case CEIL:
      return "CEIL";

    case FLOOR:
      return "FLOOR";

    case MD5:
      return "MD5";

    case SHA1:
      return "SHA1";

    case SHA224:
      return "SHA224";

    case SHA256:
      return "SHA256";

    case SHA384:
      return "SHA384";

    case SHA512:
      return "SHA512";

    case STRBEFORE:
      return "STRBEFORE";

    case STRAFTER:
      return "STRAFTER";

    case REPLACE:
      return "REPLACE";

    case UUID:
      return "UUID";

    case STRUUID:
      return "STRUUID";

    case VALUES:
      return "VALUES";

    case INTEGER_LITERAL:
    case INTEGER_POSITIVE_LITERAL:
    case INTEGER_NEGATIVE_LITERAL:
      sprintf(buffer, "INTEGER_LITERAL(%d)", lval->literal->value.integer);
      return buffer;

    case DOUBLE_LITERAL:
    case DOUBLE_POSITIVE_LITERAL:
    case DOUBLE_NEGATIVE_LITERAL:
      sprintf(buffer, "DOUBLE_LITERAL(%g)", lval->floating);
      return buffer;

    case STRING:
      sprintf(buffer, "STRING(\"%s\")", lval->name);
      return buffer;

    case BOOLEAN_LITERAL:
      return (lval->literal->value.integer ? "BOOLEAN_LITERAL(true)" : "BOOLEAN_LITERAL(false)");

    case URI_LITERAL:
      sprintf(buffer, "URI_LITERAL(%s)",
              raptor_uri_as_string(lval->uri)
              );
      return buffer;

    case QNAME_LITERAL:
      sprintf(buffer, "QNAME_LITERAL(%s)", lval->name);
      return buffer;

    case URI_LITERAL_BRACE:
      sprintf(buffer, "URI_LITERAL_BRACE(%s)",
              raptor_uri_as_string(lval->uri)
              );
      return buffer;

    case QNAME_LITERAL_BRACE:
      sprintf(buffer, "QNAME_LITERAL_BRACE(%s)", lval->name);
      return buffer;

    case IDENTIFIER:
      sprintf(buffer, "IDENTIFIER(%s)", lval->name);
      return buffer;

    case BLANK_LITERAL:
      sprintf(buffer, "BLANK_LITERAL(%s)", lval->name);
      return buffer;

    case DECIMAL_LITERAL:
    case DECIMAL_POSITIVE_LITERAL:
    case DECIMAL_NEGATIVE_LITERAL:
      sprintf(buffer, "DECIMAL_LITERAL(%s)", lval->literal->string);
      return buffer;

    case LANGTAG:
      sprintf(buffer, "LANGTAG(%s)", lval->name);
      return buffer;

    case HATHAT:
      return "HATHAT";

   default:
     RASQAL_DEBUG2("UNKNOWN token %d - add a new case\n", token);
     abort();
  }
}
#endif



#ifdef STANDALONE
static void
sparql_token_free(rasqal_world* world, int token, YYSTYPE *lval)
{
  if(!token)
    return;
  
  switch(token) {
    case INTEGER_LITERAL:
    case INTEGER_POSITIVE_LITERAL:
    case INTEGER_NEGATIVE_LITERAL:
    case DOUBLE_LITERAL:
    case DOUBLE_POSITIVE_LITERAL:
    case DOUBLE_NEGATIVE_LITERAL:
    case DECIMAL_POSITIVE_LITERAL:
    case DECIMAL_NEGATIVE_LITERAL:
    case DECIMAL_LITERAL:
    case BOOLEAN_LITERAL:
      rasqal_free_literal(lval->literal);
      break;
    case URI_LITERAL:
    case URI_LITERAL_BRACE:
      raptor_free_uri(lval->uri);
      break;
    case STRING:
    case IDENTIFIER:
    case BLANK_LITERAL:
    case PREFIX:
      RASQAL_FREE(char*, lval->name);
      break;
    case QNAME_LITERAL:
      if(lval->name)
        RASQAL_FREE(char*, lval->name);
      break;
    default:
      break;
  }
}


#define FILE_READ_BUF_SIZE 4096

int
main(int argc, char *argv[]) 
{
  const char *program = rasqal_basename(argv[0]);
  char *query_string = NULL;
  rasqal_query rq;
  rasqal_sparql_query_language sparql;
  yyscan_t scanner;
  int token = EOF;
  YYSTYPE lval;
  const unsigned char *uri_string;
  const char *filename = NULL;
  char *buf = NULL;
  size_t len;
  rasqal_world *world;
  FILE *fh;
  
  world = rasqal_new_world();
  if(!world || rasqal_world_open(world))
    exit(1);
  
  filename = getenv("SPARQL_QUERY_FILE");
  if(!filename) {
    if(argc != 2) {
      fprintf(stderr, "%s: Too many arguments.\n", program);
      fprintf(stderr, "SPARQL lexer test for Rasqal %s\n", 
              rasqal_version_string);
      fprintf(stderr, "USAGE: %s SPARQL-QUERY-FILE\n", program);
      exit(1);
    }
    
    filename = argv[1];
  }
  
  query_string = RASQAL_CALLOC(char*, FILE_READ_BUF_SIZE, 1);
  fh = fopen(filename, "r");
  if(fh) {
    fread(query_string, FILE_READ_BUF_SIZE, 1, fh);
    fclose(fh);
  } else {
    fprintf(stderr, "%s: Cannot open file %s - %s\n", program, filename,
            strerror(errno));
    exit(1);
  }

  memset(&rq, 0, sizeof(rq));
  rq.world = world;
  memset(&sparql, 0, sizeof(sparql));

  yylex_init(&sparql.scanner);
  scanner = sparql.scanner;

  len =  strlen(RASQAL_GOOD_CAST(const char*, query_string));
  buf =  RASQAL_MALLOC(char*, len + 3);
  memcpy(buf, query_string, len);
  buf[len] =  ' ';
  buf[len+1] =  buf[len+2] = '\0'; /* YY_END_OF_BUFFER_CHAR; */
  (void)sparql_lexer__scan_buffer(buf, len+3, scanner);

  sparql_lexer_set_extra(&rq, scanner);

  /* Initialise enough of the rasqal_query and locator to get error messages */
  rq.context = &sparql;
  sparql.lineno = 1;
  rq.locator.file = filename;
  rq.locator.column = -1;

  uri_string = raptor_uri_filename_to_uri_string(filename);
  rq.base_uri = raptor_new_uri(world->raptor_world_ptr, uri_string);
  raptor_free_memory((void*)uri_string);

  while(1) {
    memset(&lval, 0, sizeof(lval));
    if(sparql_lexer_get_text(scanner) != NULL)
      printf("yyinput '%s'\n", sparql_lexer_get_text(scanner));
    token = yylex(&lval, scanner);
#ifdef RASQAL_DEBUG
    printf("token %s\n", sparql_token_print(world, token, &lval));
#else
    printf("token %d\n", token);
#endif
    sparql_token_free(world, token, &lval);
    if(!token || token == EOF)
      break;
  }

  if(buf)
    RASQAL_FREE(char*, buf);
  
  yylex_destroy(scanner);

  raptor_free_uri(rq.base_uri);

  RASQAL_FREE(char*, query_string);

  rasqal_free_world(world);

  if(rq.failed)
    return 1;
 
  return 0;
}
#endif /* STANDALONE */

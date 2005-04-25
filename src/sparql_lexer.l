/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * sparql_lexer.l - Rasqal SPARQL lexer - making tokens for sparql grammar generator
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
 * To generate the C files from this source, rather than use the
 * shipped sparql_lexer.c/.h needs a patched version of flex 2.5.31 such
 * as the one available in Debian GNU/Linux.   Details below
 * near the %option descriptions.
 *
 * SPARQL defined in http://www.w3.org/TR/rdf-sparql-query/
 *
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

%option reentrant


%x PATTERN ID PREF LITERAL

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

#include <rasqal.h>
#include <rasqal_internal.h>

#include <sparql_parser.h>

#include <sparql_common.h>



static void sparql_skip_cpp_comment(rasqal_sparql_query_engine *rqe);
static int sparql_skip_c_comment(rasqal_query *rq);

static unsigned char *sparql_copy_name(rasqal_query *rq, const unsigned char *text, size_t len);
static raptor_uri* sparql_copy_qname(rasqal_query *rq, const unsigned char *text, size_t len);
static int sparql_copy_regex_token(rasqal_query *rq, YYSTYPE* lval, unsigned char delim);
static int sparql_copy_string_token(rasqal_query *rq, YYSTYPE* lval, const unsigned char *text, size_t len, int delim);

#ifdef RASQAL_DEBUG
const char * sparql_token_print(int token, YYSTYPE *lval);
#endif

int sparql_lexer_lex (YYSTYPE *sparql_parser_lval, yyscan_t yyscanner);
#define YY_DECL int sparql_lexer_lex (YYSTYPE *sparql_parser_lval, yyscan_t yyscanner)

#ifdef __cplusplus
#define INPUT_FN yyinput
#else
#define INPUT_FN input
#endif

/* Remove the re-fill function since it should never be called */
#define YY_INPUT(buf,result,max_size) { return YY_NULL; }


/* Missing sparql_lexer.c/h prototypes */
int sparql_lexer_get_column(yyscan_t yyscanner);
void sparql_lexer_set_column(int  column_no , yyscan_t yyscanner);

%}

LANGUAGETOKEN [A-Za-z][-A-Z_a-z0-9]*
/* See
 * http://www.w3.org/TR/xml11/#NT-NameStartChar and 
 * http://www.w3.org/TR/xml11/#NT-NameChar 
 */
PREFIX [A-Za-z\\\x80-\xff][-A-Z_a-z\\\x80-\xff0-9]*
NAME [A-Za-z_\\\x80-\xff][-A-Z_a-z\\\x80-\xff0-9]*
QNAME ":"|{PREFIX}":"|":"{NAME}|{PREFIX}":"{NAME}
BNAME "_:"{NAME}

/* The initial char conditions are to ensure this doesn't grab < or <= */
QUOTEDURI \<[A-Za-z][^>]+\>

%%
  /* rules */

  int c;
  rasqal_query *rq=(rasqal_query*)yyextra;
  rasqal_sparql_query_engine *rqe=(rasqal_sparql_query_engine*)rq->context;


"//"	{ /* C++ comment */
    	sparql_skip_cpp_comment(rqe);
}

"/*"	{ if(sparql_skip_c_comment(rq) < 0)
            yyterminate();
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
[Ll][Ii][Mm][Ii][Tt] { return LIMIT; }
[Uu][Nn][Ii][Oo][Nn] { return UNION; }
[Oo][Pp][Tt][Ii][Oo][Nn][Aa][Ll] { return OPTIONAL; }
[Bb][Aa][Ss][Ee] { return BASE; }
[Bb][Oo][Uu][Nn][Dd] { return BOUND; }
[Ss][Tt][Rr] { return STR; }
[Ll][Aa][Nn][Gg] { return LANG; }
[Dd][Aa][Tt][Aa][Tt][Yy][Pp][Ee] { return DATATYPE; }
[Ii][Ss][Uu][Rr][Ii] { return ISURI; }
[Ii][Ss][Bb][Ll][Aa][Nn][Kk] { return ISBLANK; }
[Ii][Ss][Ll][Ii][Tt][Ee][Rr][Aa][Ll] { return ISLITERAL; }
[Gg][Rr][Aa][Pp][Hh] { return GRAPH; }
[Nn][Aa][Mm][Ee][Dd] { return NAMED; }
[Ff][Ii][Ll][Tt][Ee][Rr] { return FILTER; }
[Oo][Ff][Ff][Ss][Ee][Tt] { return OFFSET; }
[Oo][Rr][Dd][Ee][Rr] { return ORDER; }
[Bb][Yy] { return BY; }
[Rr][Ee][Gg][Ee][Xx] { return REGEX; }

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

"EQ"|"eq"  { /* FIXME remove old token */ return STR_EQ; }
"NE"|"ne"  { /* FIXME remove old token */ return STR_NE; }

"=~"|"~~"  { BEGIN(PATTERN); return STR_MATCH; }
"!~"       { BEGIN(PATTERN); return STR_NMATCH; }

<PATTERN>[ \t\v]+   {
	;
}

<PATTERN>\r\n|\r|\n     { rqe->lineno++; }


<PATTERN>. { /* first non whitespace */
             if(!sparql_copy_regex_token(rq, sparql_parser_lval, *yytext)) {
               BEGIN(INITIAL);
               return PATTERN_LITERAL;
             }
             BEGIN(INITIAL);
             yyterminate();
         };


"="            { return EQ; }
"=="            { /* FIXME remove old token */ return EQ; }
"!="            { return NEQ; }
"<"/[^A-Za-z=]  { return LT; }
">"             { return GT; }
"<="         { return LE; }
">="         { return GE; }

"+"         { return '+'; }
"-"         { return '-'; }
"*"         { return '*'; }
"/"         { return '/'; }
"%"         { return '%'; }
"~"         { return '~'; }
"!"         { return '!'; }
"&"         { return '&'; }

[0-9]+["lL"]?   { c=yytext[yyleng-1];
                  if (c== 'l' || c == 'L')
                    yytext[yyleng-1]='\0';
                  sparql_parser_lval->literal=rasqal_new_integer_literal(RASQAL_LITERAL_INTEGER, atoi(yytext));
 		  return INTEGER_LITERAL; 
}

0[xX][0-9a-fA-F]+   { int i;
                      if(yytext[yyleng+1] == 'x') 
                        sscanf(yytext+2, "%x", &i);
                      else
                        sscanf(yytext+2, "%X", &i);
                      sparql_parser_lval->literal=rasqal_new_integer_literal(RASQAL_LITERAL_INTEGER, i);
                      return INTEGER_LITERAL; 
}

[0-9]+"."[0-9]*[eE][+-]?[0-9]+[fFdD]?|"."[0-9]+[eE][+-]?[0-9]+[fFdD]?|[0-9]+[eE][+-]?[0-9]+[fFdD]?|[0-9]+[eE][+-]?[0-9]+[fFdD]?|[0-9]+"."[0-9]*  { 
                        double d;
                        sscanf((const char*)yytext, "%lf", &d);
                        sparql_parser_lval->literal=rasqal_new_floating_literal(d);
                        return FLOATING_POINT_LITERAL;
}

'([^'\\\n\r]|\\[^\n\r])*'(@{LANGUAGETOKEN})?(^^({QUOTEDURI}|{QNAME}))?    { /*' */
                               sparql_copy_string_token(rq, sparql_parser_lval,
                                 (const unsigned char*)yytext+1, yyleng-1, '\'');
                               return STRING_LITERAL; }

\"([^"\\\n\r]|\\[^\n\r])*\"(@{LANGUAGETOKEN})?(^^({QUOTEDURI}|{QNAME}))?   { /* " */
                                sparql_copy_string_token(rq, sparql_parser_lval,
                                  (const unsigned char*)yytext+1, yyleng-1, '"');
                                return STRING_LITERAL; }

\"\"\"				{ BEGIN(LITERAL); }

<LITERAL>(.|\n)*\"\"\"	{ sparql_copy_string_token(rq, sparql_parser_lval, 
                            (unsigned char*)yytext, yyleng-3, '"'); /* ' */
                          BEGIN(INITIAL);
                          return STRING_LITERAL; }

<LITERAL>(.|\n)	{ BEGIN(INITIAL);
		  if (*yytext == EOF)
                    return EOF;

                  sparql_syntax_error(rq, "syntax error at %c - \"\"\"string was not terminated", *yytext);
                  yyterminate();  }


"true"|"false"	{ sparql_parser_lval->literal=rasqal_new_boolean_literal(*yytext== 't');
                  return BOOLEAN_LITERAL; }

"null"	{ sparql_parser_lval->literal=NULL;
          return NULL_LITERAL; }

<ID>{NAME}	{ sparql_parser_lval->name=sparql_copy_name(rq, (const unsigned char*)yytext, yyleng);
		  if(!sparql_parser_lval->name)
                    yyterminate();
                          BEGIN(INITIAL);
                          return IDENTIFIER; }
<ID>(.|\n)	{	BEGIN(INITIAL);
		sparql_syntax_error(rq, "SPARQL syntax error - missing variable name after ?");
                yyterminate();
}


<PREF>[\ \t\v]+ { /* eat up leading whitespace */ }
<PREF>{PREFIX}":"	{ BEGIN(INITIAL);
		  	  sparql_parser_lval->name=sparql_copy_name(rq, (const unsigned char*)yytext, yyleng-1);
			  if(!sparql_parser_lval->name)
			    yyterminate();
                          return IDENTIFIER; }
<PREF>":"	{ BEGIN(INITIAL);
		  sparql_parser_lval->name=NULL;
                  return IDENTIFIER; }

<PREF>(.|\n)	{ BEGIN(INITIAL);
		  if (*yytext == EOF)
                    return EOF;

                  sparql_syntax_error(rq, "SPARQL syntax error at '%c'", *yytext);
                  yyterminate();
}

{QNAME}\(?	{
		int have_brace=(yytext[yyleng-1]=='(');
		if(have_brace)
			yyleng--;
		sparql_parser_lval->uri=sparql_copy_qname(rq, (const unsigned char*)yytext, yyleng);
		if(!sparql_parser_lval->uri)
		  yyterminate();
 		return have_brace ? URI_LITERAL_BRACE : URI_LITERAL;
}

{BNAME}	{	sparql_parser_lval->name=sparql_copy_name(rq, (unsigned char*)yytext+2, yyleng-2);
		return BLANK_LITERAL;
}

{QUOTEDURI}\(?   { 
		int have_brace=(yytext[yyleng-1]=='(');
		if(have_brace)
			yyleng--;
		if(yyleng == 2) 
                  sparql_parser_lval->uri=raptor_uri_copy(rq->base_uri);
                else {
                  yytext[yyleng-1]='\0';
                  sparql_parser_lval->uri=raptor_new_uri_relative_to_base(rq->base_uri, (const unsigned char*)yytext+1);
                 }
                 return have_brace ? URI_LITERAL_BRACE : URI_LITERAL; }

\#              { while((c=INPUT_FN(yyscanner)) != '\n' && c)
                    ;
                }

.         	{ if (*yytext == EOF)
                    return EOF;

                  sparql_syntax_error(rq, "SPARQL syntax error at '%c'", *yytext);
                  yyterminate();
		}

%%
  /* user code */

int
yywrap (yyscan_t yyscanner) {
  return 1;
}


static unsigned char *
sparql_copy_name(rasqal_query *rq, const unsigned char *text, size_t len) {
  size_t dest_len=0;
  unsigned char *s;

  s=rasqal_escaped_name_to_utf8_string((unsigned char*)text, len,
                                       &dest_len,
                                       (raptor_simple_message_handler)sparql_syntax_error, rq);
  if(!s)
    return s;

  if(!raptor_xml_name_check(s, dest_len, 11))
    sparql_syntax_warning(rq, "Invalid SPARQL name \"%s\"", s);

  return s;
}


static raptor_uri*
sparql_copy_qname(rasqal_query *rq, const unsigned char *text, size_t len) {
  unsigned char *p;
  size_t dest_len=0;
  unsigned char *s;
  raptor_uri* uri=NULL;
  
  s=rasqal_escaped_name_to_utf8_string((unsigned char*)text, len,
                                       &dest_len,
                                       (raptor_simple_message_handler)sparql_syntax_error, rq);
  if(!s)
    return NULL;

  p=(unsigned char*)strchr((const char*)s, ':');
  if(!raptor_xml_name_check(s, p-s, 11))
    sparql_syntax_warning(rq, "Invalid SPARQL name \"%s\"", s);
  if(!raptor_xml_name_check(p+1, dest_len-((p+1)-s), 11))
    sparql_syntax_warning(rq, "Invalid SPARQL name \"%s\"", p+1);

#ifdef STANDALONE
  /* lexer test cannot declare namespaces - so just ignore expansion */
  uri=raptor_new_uri_relative_to_base(rq->base_uri, s);
#else
  if(!rq->namespaces) {
    sparql_syntax_error(rq, "SPARQL syntax error - no namespaces declared");
    return NULL;
  }
  
  uri=raptor_qname_string_to_uri(rq->namespaces,
                                 s, dest_len,
                                 rasqal_query_simple_error, rq);
#endif
  RASQAL_FREE(cstring, s);
  
  return uri;
}


static int
sparql_copy_regex_token(rasqal_query* rq, YYSTYPE* lval, unsigned char delim) {
  rasqal_sparql_query_engine *rqe=(rasqal_sparql_query_engine*)rq->context;
  yyscan_t yyscanner=rqe->scanner;
  unsigned int ind=0;
  size_t buffer_len=0;
  unsigned char *buffer=NULL;
  size_t flags_len=0;
  unsigned char *flags=NULL;
  char c;

  if(delim == 'm') {
    /* Handle pattern literal m/foo/ */
    c=INPUT_FN(yyscanner);
    if(c == EOF) {
      sparql_syntax_error(rq, "SPARQL syntax error - EOF in regex");
      return 1;
    }
    delim=(unsigned char)c;
  }

  while((c=INPUT_FN(yyscanner)) && c != EOF && c != delim) {
    /* May add 2 chars - \' */
    if(ind+2 > buffer_len) {
      unsigned char *new_buffer;
      size_t new_buffer_len=buffer_len <<1;

      if(new_buffer_len<10)
        new_buffer_len=10;
      new_buffer=(unsigned char *)calloc(1, new_buffer_len+1);
      if(buffer_len) {
        strncpy((char*)new_buffer, (const char*)buffer, buffer_len);
        free(buffer);
      }
      buffer=new_buffer;
      buffer_len=new_buffer_len;
    }
    buffer[ind++]=c;
    if(c == '\\') {
      c=INPUT_FN(yyscanner);
      buffer[ind++]=c;
    }
  }
  buffer[ind]='\0';

  if(c == EOF) {
    sparql_syntax_error(rq, "SPARQL syntax error - EOF in regex");
    return 1;
  }

  /* flags */
  ind=0;
  while((c=INPUT_FN(yyscanner)) && c != EOF && isalpha(c)) {
    if(ind+1 > flags_len) {
      unsigned char *new_flags;
      size_t new_flags_len=flags_len + 5;

      if(new_flags_len<5)
        new_flags_len=5;
      new_flags=(unsigned char *)calloc(1, new_flags_len+1);
      if(flags_len) {
        strncpy((char*)new_flags, (const char*)flags, flags_len);
        free(flags);
      }
      flags=new_flags;
      flags_len=new_flags_len;
    }
    flags[ind++]=c;
  }
  if(flags)
    flags[ind]='\0';

  lval->literal=rasqal_new_pattern_literal(buffer, (const char*)flags);
  return 0;
}


static int
sparql_copy_string_token(rasqal_query* rq, YYSTYPE* lval,
                         const unsigned char *text, size_t len, int delim) {
  unsigned int i;
  const unsigned char *s;
  unsigned char *d;
  unsigned char *string=(unsigned char *)RASQAL_MALLOC(cstring, len+1);
  char *language=NULL;
  unsigned char *dt=NULL;
  raptor_uri *dt_uri=NULL;
  unsigned char *dt_qname=NULL;

  for(s=text, d=string, i=0; i<len; s++, i++) {
    unsigned char c=*s;

    if(c == '\\' ) {
      s++; i++;
      c=*s;
      if(c == 'n')
        *d++= '\n';
      else if(c == 'r')
        *d++= '\r';
      else if(c == 't')
        *d++= '\t';
      else if(c == '\\' || c == delim)
        *d++=c;
      else if (c == 'u' || c == 'U') {
        int ulen=(c == 'u') ? 4 : 8;
        unsigned long unichar=0;
        
        s++; i++;
        if(i+ulen > len) {
          printf("\\%c over end of line", c);
          free(string);
          return 1;
        }
        
        sscanf((const char*)s, ((ulen == 4) ? "%04lx" : "%08lx"), &unichar);
        s+= ulen-1;
        i+= ulen-1;
        
        if(unichar < 0 || unichar > 0x10ffff) {
          sparql_syntax_error(rq, "SPARQL syntax error - illegal Unicode character with code point #x%lX.", unichar);
          free(string);
          return 1;
        }
          
        d+=raptor_unicode_char_to_utf8(unichar, d);
      } else {
        /* Ignore \x where x isn't the one of: \n \r \t \\ (delim) \u \U */
        sparql_syntax_warning(rq, "Unknown SPARQL string escape \\%c in \"%s\"", c, text);
        *d++=c;
      }
    } else if(c== delim) {
      *d++='\0';

      /* skip delim */
      s++; i++;

      c=*s++; i++;
      if(c=='@') {
        language=(char*)d;
        while(i<=len) {
          c=*s++; i++;
          if(!isalpha(c) && !isdigit(c))
            break;
          *d++=c;
        }
        *d++='\0';
      }
      if(c=='^') {
        /* skip second char of ^^ */
        s++; i++;

        dt=d;
        while(i++<=len)
          *d++=*s++;
        /* *d='\0' below */
      } else if (language)
        *d='\0';
      
      break;
   } else
    *d++=c;
  } /* end of for */

  *d='\0';

  if(language) {
    char *new_language=(char *)RASQAL_MALLOC(cstring, strlen((const char*)language)+1);
    strcpy(new_language, language);
    language=new_language;
  }
  
  if(dt) {
    /* dt can be a URI or qname */
    if(*dt == '<') {
      dt[strlen((const char*)dt)-1]='\0';
      dt_uri=raptor_new_uri(dt+1);
    } else {
      unsigned char *dt_p;
      size_t dest_len=0;
      unsigned char *dt_s;
      
      dt_s=rasqal_escaped_name_to_utf8_string(dt,
                                              strlen((const char*)dt),
                                              &dest_len,
                                              (raptor_simple_message_handler)sparql_syntax_error, rq);
      if(!dt_s)
        return 1;

      dt_p=(unsigned char*)strchr((const char*)dt_s, ':');
      if(!raptor_xml_name_check(dt_s, dt_p-dt_s, 11))
        sparql_syntax_warning(rq, "Invalid SPARQL name \"%s\"", dt_s);
      if(!raptor_xml_name_check(dt_p+1, dest_len-((dt_p+1)-dt_s), 11))
        sparql_syntax_warning(rq, "Invalid SPARQL name \"%s\"", dt_p+1);

#ifdef STANDALONE
      /* lexer test cannot declare namespaces - so just ignore expansion */
      dt_qname=dt_s;
#else
      if(!rq->namespaces) {
        sparql_syntax_error(rq, "SPARQL syntax error - no namespaces declared");
        RASQAL_FREE(cstring, dt_s);
        if(language)
          RASQAL_FREE(cstring, language);
        free(string);
        return 1;
      }
  
      dt_uri=raptor_qname_string_to_uri(rq->namespaces,
                                        dt_s, dest_len,
                                        rasqal_query_simple_error, rq);
      RASQAL_FREE(cstring, dt_s);

      if(!dt_uri) {
        if(language)
          RASQAL_FREE(cstring, language);
        free(string);
        return 1;
      }
#endif
    }
  }

#if RASQAL_DEBUG >3
  fprintf(stderr, "string='%s', language='%s'\n", 
          string, (language ? language : ""));
  fprintf(stderr, "datatype uri='%s'\n",
          (dt_uri ? (const char*)raptor_uri_as_string(dt_uri) : ""));
#endif

  lval->literal=rasqal_new_string_literal(string, language, dt_uri, dt_qname);

  return 0;
}


static int
sparql_skip_c_comment(rasqal_query *rq) {
  rasqal_sparql_query_engine *rqe=(rasqal_sparql_query_engine*)rq->context;
  yyscan_t yyscanner=rqe->scanner;
  int lines=0;
  int c;
  
  while(1) {
    while ((c=INPUT_FN(yyscanner)) != '*' && c!= EOF) {
      if(c == '\r' || c == '\n')
        lines++;
    }
    if( c == '*') {
      while ((c=INPUT_FN(yyscanner)) == '*') {
        if(c == '\r' || c == '\n')
          lines++;
      }

      if(c == '/')
        break;
    }
    if (c == EOF) {
      sparql_syntax_error(rq, "SPARQL syntax error - EOF in comment");
      lines= -1;
      break;
    }
  }
  return lines;
}


static void
sparql_skip_cpp_comment(rasqal_sparql_query_engine *rqe) {
  yyscan_t yyscanner=rqe->scanner;
  int c;
  
  while ((c=INPUT_FN(yyscanner)) != EOF)
    if(c == '\r' || c == '\n')
      break;
  return;
}


#ifdef RASQAL_DEBUG

const char *
sparql_token_print(int token, YYSTYPE *lval)
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

    case STR_NMATCH:
      return "STR_NMATCH";

    case STR_MATCH:
      return "STR_MATCH";

    case STR_NE:
      return "STR_NE";

    case STR_EQ:
      return "STR_EQ";

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

    case '%':
      return "%";

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

    case '~':
      return "~";

    case INTEGER_LITERAL:
      sprintf(buffer, "INTEGER_LITERAL(%d)", lval->literal->value.integer);
      return buffer;

    case FLOATING_POINT_LITERAL:
      sprintf(buffer, "FLOATING_POINT_LITERAL(%g)", lval->floating);
      return buffer;

    case STRING_LITERAL:
      if(lval->literal->language) {
        if(lval->literal->datatype)
          sprintf(buffer, "STRING_LITERAL(\"%s\"@%s^^%s)",
                  lval->literal->string, lval->literal->language,
                  raptor_uri_as_string(lval->literal->datatype));
        else
          sprintf(buffer, "STRING_LITERAL(\"%s\"@%s)",
                  lval->literal->string, lval->literal->language);
      } else {
        if(lval->literal->datatype)
          sprintf(buffer, "STRING_LITERAL(\"%s\"^^%s)", 
                  lval->literal->string,
                  raptor_uri_as_string(lval->literal->datatype));
        else
          sprintf(buffer, "STRING_LITERAL(\"%s\")", lval->literal->string);
      }
      return buffer;

    case PATTERN_LITERAL:
      sprintf(buffer, "PATTERN_LITERAL(%s,%s)", lval->literal->string,
              (lval->literal->flags ? (char*)lval->literal->flags : "-"));
      return buffer;

    case BOOLEAN_LITERAL:
      return (lval->literal->value.integer ? "BOOLEAN_LITERAL(true)" : "BOOLEAN_LITERAL(false)");

    case NULL_LITERAL:
      return "NULL_LITERAL";

    case URI_LITERAL:
      sprintf(buffer, "URI_LITERAL(%s)", raptor_uri_as_string(lval->uri));
      return buffer;

    case QNAME_LITERAL:
      sprintf(buffer, "QNAME_LITERAL(%s)", lval->name);
      return buffer;

    case URI_LITERAL_BRACE:
      sprintf(buffer, "URI_LITERAL_BRACE(%s)", raptor_uri_as_string(lval->uri));
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

   default:
     RASQAL_DEBUG2("UNKNOWN token %d - add a new case\n", token);
     abort();
  }
}
#endif



#ifdef STANDALONE
static void
sparql_token_free(int token, YYSTYPE *lval)
{
  if(!token)
    return;
  
  switch(token) {
    case STRING_LITERAL:
    case PATTERN_LITERAL:
      rasqal_free_literal(lval->literal);
      break;
    case URI_LITERAL:
      raptor_free_uri(lval->uri);
      break;
    case IDENTIFIER:
    case BLANK_LITERAL:
      free(lval->name);
      break;
    case QNAME_LITERAL:
      if(lval->name)
        free(lval->name);
      break;
    default:
      break;
  }
}


#define FILE_READ_BUF_SIZE 2048

int
main(int argc, char *argv[]) 
{
  const char *program=rasqal_basename(argv[0]);
  char *query_string=NULL;
  rasqal_query rq;
  rasqal_sparql_query_engine sparql;
  yyscan_t scanner;
  int token=EOF;
  YYSTYPE lval;
  const unsigned char *uri_string;
  char *filename=NULL;
  char *buf=NULL;
  size_t len;
  void *buffer;

  rasqal_init();
  
  if(argc > 1) {
    FILE *fh;
    query_string=(char*)calloc(FILE_READ_BUF_SIZE, 1);
    filename=argv[1];
    fh=fopen(filename, "r");
    if(fh) {
      fread(query_string, FILE_READ_BUF_SIZE, 1, fh);
      fclose(fh);
    } else {
      fprintf(stderr, "%s: Cannot open file %s - %s\n", program, filename,
              strerror(errno));
      exit(1);
    }
  } else {
    filename="<stdin>";
    query_string=(char*)calloc(FILE_READ_BUF_SIZE, 1);
    fread(query_string, FILE_READ_BUF_SIZE, 1, stdin);
  }

  memset(&rq, 0, sizeof(rasqal_query));
  memset(&sparql, 0, sizeof(rasqal_sparql_query_engine));

  yylex_init(&sparql.scanner);
  scanner=sparql.scanner;

  len= strlen((const char*)query_string);
  buf= (char *)RASQAL_MALLOC(cstring, len+3);
  strncpy(buf, query_string, len);
  buf[len]= ' ';
  buf[len+1]= buf[len+2]='\0'; /* YY_END_OF_BUFFER_CHAR; */
  buffer= sparql_lexer__scan_buffer(buf, len+3, scanner);

  sparql_lexer_set_extra(&rq, scanner);

  /* Initialise enough of the rasqal_query and locator to get error messages */
  rq.context=&sparql;
  sparql.lineno=1;
  rq.locator.file=filename;
  rq.locator.column= -1;

  uri_string=raptor_uri_filename_to_uri_string(filename);
  rq.base_uri=raptor_new_uri(uri_string);
  raptor_free_memory((void*)uri_string);

  while(1) {
    memset(&lval, 0, sizeof(YYSTYPE));
    if(sparql_lexer_get_text(scanner) != NULL)
      printf("yyinput '%s'\n", sparql_lexer_get_text(scanner));
    token=yylex(&lval, scanner);
#ifdef RASQAL_DEBUG
    printf("token %s\n", sparql_token_print(token, &lval));
#else
    printf("token %d\n", token);
#endif
    sparql_token_free(token, &lval);
    if(!token || token == EOF)
      break;
  }

  if(buf)
    RASQAL_FREE(cstring, buf);
  
  yylex_destroy(scanner);

  raptor_free_uri(rq.base_uri);

  free(query_string);

  rasqal_finish();

  if(rq.failed)
    return 1;
 
  return 0;
}
#endif

/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rdql_lexer.l - Rasqal RDQL lexer - making tokens for rdql grammar generator
 *
 * $Id$
 *
 * Copyright (C) 2003 David Beckett - http://purl.org/net/dajobe/
 * Institute for Learning and Research Technology - http://www.ilrt.org/
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
 * To generate the C files from this source, rather than use the
 * shipped n3_lexer.c/.h needs a patched version of flex 2.5.31 such
 * as the one available in Debian GNU/Linux.   Details below
 * near the %option descriptions.
 *
 */


/* recognise 8-bits */
%option 8bit
%option warn nodefault

/* all symbols prefixed by this */
%option prefix="rdql_lexer_"

/* This is not needed, flex is invoked -ordql_lexer.c */
%option outfile="rdql_lexer.c"

/* Emit a C header file for prototypes
 * Only available in flex 2.5.13 or newer.
 * It was renamed to header-file in flex 2.5.19
 */
%option header-file="rdql_lexer.h"

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

/* make a yylineno please */
%option yylineno

%option reentrant


%x PATTERN ID

  /* definitions */

%{
#ifdef HAVE_CONFIG_H
#include <rasqal_config.h>
#endif

#ifdef WIN32
#include <win32_config.h>
#endif


#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <rasqal.h>
#include <rasqal_internal.h>

#include <rdql_parser.tab.h>
#include <rdql_common.h>


static void skip_cpp_comment(rdql_parser *rp);
static int skip_c_comment(rdql_parser *rp);

static unsigned char *copy_token(rdql_parser *rp, unsigned char *text, size_t len);
static int copy_regex_token(rdql_parser *rp, unsigned char delim);
static int copy_string_token(rdql_parser *rp, unsigned char *text, size_t len, int delim);

char *filename=NULL;

#ifdef RASQAL_DEBUG
const char * rdql_token_print(int token, YYSTYPE *lval);
#endif

int rdql_lexer_lex (YYSTYPE *rdql_parser_lval, yyscan_t yyscanner);
#define YY_DECL int rdql_lexer_lex (YYSTYPE *rdql_parser_lval, yyscan_t yyscanner)


/* Missing rdql_lexer.c/h prototypes */
int rdql_lexer_get_column(yyscan_t yyscanner);
void rdql_lexer_set_column(int  column_no , yyscan_t yyscanner);

%}

LANGUAGETOKEN [[:alpha:]][[:alnum:]]*
PREFIX [[:alpha:]][[:alnum:]]*
QNAME [[:alpha:]][[:alnum:]]*:[[:alpha:]][[:alnum:]]*
/* The initial char conditions are to ensure this doesn't grab < or <= */
QUOTEDURI \<[A-Za-z][^>]+\>

%%
  /* rules */

  int c;
  rdql_parser *rp=(rdql_parser*)yyextra;


"//"	{ /* C++ comment */
    skip_cpp_comment(rp);
}

"/*"	{ if(skip_c_comment(rp)<0)
            return ERROR; 
        }

[\r\n]+     { /* empty - lineno++ */ }

[\ \t\v]+   { /* eat up other whitespace */
	;
}

"SELECT"|"select" { return SELECT; }
"SOURCE"|"source" { return SOURCE; }
"FROM"|"from"     { return FROM; } 
"WHERE"|"where"   { return WHERE; } 
"AND"|"and"       { return AND; } 
"USING"|"using"   { return USING; }
"FOR"|"for"       { return FOR; }

","      { return COMMA; } 
"("      { return LPAREN; } 
")"      { return RPAREN; } 
"?"      { BEGIN(ID); return VARPREFIX; }

"||"         { return SC_OR; }
"&&"         { return SC_AND; }

"EQ"|"eq"  { return STR_EQ; }
"NE"|"NE"  { return STR_NE; }

"=~"|"~~"  { BEGIN(PATTERN); return STR_MATCH; }
"!~"       { BEGIN(PATTERN); return STR_NMATCH; }

<PATTERN>[ \t\v\r\n]+   {
	;
}

<PATTERN>. { /* first non whitespace */
             copy_regex_token(rp, *yytext);
             BEGIN(INITIAL);
             return PATTERN_LITERAL;
         };


"=="            { return EQ; }
"!="            { return NEQ; }
"<"/[^A-Za-z=]  { return LT; }
">"             { return GT; }
"<="         { return LE; }
">="         { return GE; }

"+"         { return PLUS; }
"-"         { return MINUS; }
"*"         { return STAR; }
"/"         { return SLASH; }
"%"         { return REM; }
"~"         { return TILDE; }
"!"         { return BANG; }

[0-9]+["lL"]?   { c=yytext[yyleng-1];
                  if (c== 'l' || c == 'L')
                    yytext[yyleng-1]='\0';
                  rdql_parser_lval->integer=atoi(yytext);
 		  return INTEGER_LITERAL; 
}

0[xX][0-9a-fA-F]+   { if(yytext[yyleng+1] == 'x') 
                        sscanf(yytext+2, "%x", &rdql_parser_lval->integer);
                      else
                        sscanf(yytext+2, "%X", &rdql_parser_lval->integer);
                      return INTEGER_LITERAL; 
}

[0-9]+"."[0-9]*[eE][+-]?[0-9]+[fFdD]?|"."[0-9]+[eE][+-]?[0-9]+[fFdD]?|[0-9]+[eE][+-]?[0-9]+[fFdD]?|[0-9]+[eE][+-]?[0-9]+[fFdD]?  { 
                        sscanf(yytext, "%f", &rdql_parser_lval->floating);
                        return FLOATING_POINT_LITERAL;
}

'([^'\\\n\r]|\\[^\n\r])*'(@{LANGUAGETOKEN})?(^^({QUOTEDURI}|{QNAME}))?    { /*' */
                               copy_string_token(rp, yytext+1, yyleng-2, '\"');
                               return STRING_LITERAL; }

\"([^"\\\n\r]|\\[^\n\r])*\"(@{LANGUAGETOKEN})?(^^({QUOTEDURI}|{QNAME}))?   { /* " */
                                copy_string_token(rp, yytext+1, yyleng-2, '"');
                                return STRING_LITERAL; }

"true"|"false"	{ rdql_parser_lval->integer=(*yytext == 't');
                  return BOOLEAN_LITERAL; }

"null"	{ rdql_parser_lval->integer=0;
          return NULL_LITERAL; }

<*>[[:alpha:]][[:alnum:]_$\.]*	{ rdql_parser_lval->string=copy_token(rp, yytext, yyleng);
                          BEGIN(INITIAL);
                          return IDENTIFIER; }

{QUOTEDURI}   { rdql_parser_lval->string=copy_token(rp, yytext+1, yyleng-2);
                return URI_LITERAL; }

\#              { while((c=input(yyscanner)) != '\n' && c)
                    ;
                }

.         	{ if (*yytext == EOF)
                    return EOF;

                  rdql_syntax_error(rp, "syntax error at '%c'", *yytext);
                  return ERROR;
		}

%%
  /* user code */

int
yywrap (yyscan_t yyscanner) {
  return 1;
}


static unsigned char *
copy_token(rdql_parser *rp, unsigned char *text, size_t len) {
  unsigned char *s;
  if(!len)
    len=strlen(text);
  s=(unsigned char *)malloc(len+1);

  strncpy(s, text, len);
  s[len] = '\0';
  return s;
}


static int
copy_regex_token(rdql_parser* rp, unsigned char delim) {
  YYSTYPE* lval=&rp->lval;
  yyscan_t yyscanner=rp->scanner;
  int ind=0;
  size_t buffer_len=0;
  unsigned char *buffer=NULL;
  size_t flags_len=0;
  unsigned char *flags=NULL;
  char c;

  if(delim == 'm') {
    /* Handle pattern literal m/foo/ */
    delim=input(yyscanner);
  }

  while((c=input(yyscanner)) != delim && c) {
    /* May add 2 chars - \' */
    if(ind+2 > buffer_len) {
      unsigned char *new_buffer;
      size_t new_buffer_len=buffer_len <<1;

      if(new_buffer_len<10)
        new_buffer_len=10;
      new_buffer=(unsigned char *)malloc(new_buffer_len+1);
      if(buffer_len) {
        strncpy(new_buffer, buffer, buffer_len);
        free(buffer);
      }
      buffer=new_buffer;
      buffer_len=new_buffer_len;
    }
    buffer[ind++]=c;
    if(c == '\\') {
      c=input(yyscanner);
      buffer[ind++]=c;
    }
  }
  buffer[ind]='\0';
  lval->string=buffer;

  /* flags */
  ind=0;
  while((c=input(yyscanner)) && isalpha(c)) {
    if(ind+1 > flags_len) {
      unsigned char *new_flags;
      size_t new_flags_len=flags_len + 5;

      if(new_flags_len<5)
        new_flags_len=5;
      new_flags=(unsigned char *)malloc(new_flags_len+1);
      if(flags_len) {
        strncpy(new_flags, flags, flags_len);
        free(flags);
      }
      flags=new_flags;
      flags_len=new_flags_len;
    }
    flags[ind++]=c;
  }
  if(flags) {
    flags[ind]='\0';
    lval->flags=flags;
  }

  return 0;
}


static int
copy_string_token(rdql_parser* rp, unsigned char *text, size_t len, int delim) {
  YYSTYPE* lval=&rp->lval;
  int i;
  unsigned char *s, *d;
  unsigned char *string=(unsigned char *)malloc(len+1);
  unsigned char *language=NULL;
  unsigned char *dt=NULL;

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
        
        sscanf(s, ((ulen == 4) ? "%04lx" : "%08lx"), &unichar);
        s+= ulen-1;
        i+= ulen-1;
        
        if(unichar < 0 || unichar > 0x10ffff) {
          rdql_syntax_error(rp, "Illegal Unicode character with code point #x%lX.", unichar);
          free(string);
          return 1;
        }
          
/* FIXME - need to link with raptor */
#if 0
        d+=raptor_unicode_char_to_utf8(unichar, d);
#endif
	d+=1;

      } else {
        /* don't handle \x where x isn't one of: \n \r \\ (delim) */
        rdql_syntax_warning(rp, "Illegal string escape \\%c in \"%s\"", c, text);
        *d++=c;
      }
    } else if(c== delim) {
        s++; i++;
        c=*s;
        if(c=='@') {
          *d++='\0';
          s++; i++;
          language=d;
          while(i<len) {
            *d++=*s++; i++;
            if(!isalpha(*s) || !isdigit(*s))
              break;
          }
        }
        if(c=='^') {
          *d++='\0';
          s++; i++;
          /* FIXME assuming ^^ */
          s++; i++;
          dt=d;
          while(i<len) {
            *d++=*s++; i++;
            if(!isalpha(*s) || !isdigit(*s))
              break;
          }
          /* *d='\0' below */
        }
        break;
   } else
    *d++=c;
  } /* end of for */

  *d='\0';

  lval->string=string;
  return 0;
}


static int
skip_c_comment(rdql_parser *rp) {
  yyscan_t yyscanner=rp->scanner;
  int lines=0;
  int c;
  
  while(1) {
    while ((c=input(yyscanner)) != '*' && c!= EOF) {
      if(c == '\r' || c == '\n')
        lines++;
    }
    if( c == '*') {
      while ((c=input(yyscanner)) == '*') {
        if(c == '\r' || c == '\n')
          lines++;
      }

      if(c == '/')
        break;
    }
    if (c == EOF) {
      rdql_syntax_error(rp, "EOF in comment");
      lines= -1;
      break;
    }
  }
  return lines;
}


static void
skip_cpp_comment(rdql_parser *rp) {
  yyscan_t yyscanner=rp->scanner;
  int c;
  
  while ((c=input(yyscanner)) != EOF)
    if(c == '\r' || c == '\n')
      break;
  return;
}


#ifdef RASQAL_DEBUG

const char *
rdql_token_print(int token, YYSTYPE *lval)
{
  static char buffer[2048];

  if(!token)
    return "<<EOF>>";
  
  switch(token) {
    case SELECT:
      return "SELECT";

    case SOURCE:
      return "SOURCE";

    case FROM:
      return "FROM";

    case WHERE:
      return "WHERE";

    case AND:
      return "AND";

    case FOR:
      return "FOR";

    case COMMA:
      return "COMMA";

    case LPAREN:
      return "LPAREN";

    case RPAREN:
      return "RPAREN";

    case VARPREFIX:
      return "VARPREFIX";

    case USING:
      return "USING";

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

    case REM:
      return "REM";

    case SLASH:
      return "SLASH";

    case STAR:
      return "STAR";

    case MINUS:
      return "MINUS";

    case PLUS:
      return "PLUS";

    case BANG:
      return "BANG";

    case TILDE:
      return "TILDE";

    case INTEGER_LITERAL:
      sprintf(buffer, "INTEGER_LITERAL(%d)", lval->integer);
      return buffer;

    case FLOATING_POINT_LITERAL:
      sprintf(buffer, "FLOATING_POINT_LITERAL(%f)", lval->floating);
      return buffer;

    case STRING_LITERAL:
      sprintf(buffer, "STRING_LITERAL(%s)", lval->string);
      return buffer;

    case PATTERN_LITERAL:
      sprintf(buffer, "PATTERN_LITERAL(%s,%s)", lval->string,
              (lval->flags ? (char*)lval->flags : "-"));
      return buffer;

    case BOOLEAN_LITERAL:
      return (lval->integer ? "BOOLEAN_LITERAL(true)" : "BOOLEAN_LITERAL(false)");

    case NULL_LITERAL:
      return "NULL_LITERAL";

    case URI_LITERAL:
      sprintf(buffer, "URI_LITERAL(%s)", lval->string);
      return buffer;

    case IDENTIFIER:
      sprintf(buffer, "IDENTIFIER(%s)", lval->string);
      return buffer;

    case ERROR:
      return "ERROR";

   default:
     fprintf(stderr, "rdql_token_print: UNKNOWN token %d - add a new case\n", token);
     abort();
  }
}
#endif



#ifdef STANDALONE

int
rdql_syntax_error(rdql_parser *rp, const char *message, ...)
{
  yyscan_t yyscanner=(yyscan_t)rp;
  va_list arguments;

  fprintf(stderr, "%s:%d: ", filename, rdql_lexer_get_lineno(yyscanner));

  va_start(arguments, message);
  vfprintf(stderr, message, arguments);
  va_end(arguments);

  fputc('\n', stderr);

  return 0;
}

static void
rdql_token_free(int token, YYSTYPE *lval)
{
  if(!token)
    return;
  
  if(lval->string) {
    free(lval->string);
    lval->string=NULL;
  }
}


int
main(int argc, char *argv[]) 
{
  rdql_parser rp;
  int token=EOF;
  FILE *fh;
  YYSTYPE *lval;
  yyscan_t scanner;

  if(argc > 1) {
    filename=argv[1];
    fh=fopen(filename, "r");
    if(!fh) {
      fprintf(stderr, "%s: Cannot open file %s - %s\n", argv[0], filename,
              strerror(errno));
      exit(1);
    }
  } else {
    filename="<stdin>";
    fh=stdin;
  }

  memset(&rp, 0, sizeof(rdql_parser));

  lval=&rp.lval;
  
  yylex_init(&rp.scanner);
  scanner=rp.scanner;
  rdql_lexer_set_in(fh, scanner);
  rdql_lexer_set_extra(&rp, scanner);

  while(1) {
    if(rdql_lexer_get_text(scanner) != NULL)
      printf("yyinput '%s'\n", rdql_lexer_get_text(scanner));
    token=yylex(lval, scanner);
#ifdef RASQAL_DEBUG
    printf("token %s\n", rdql_token_print(token, lval));
#else
    printf("token %d\n", token);
#endif
    rdql_token_free(token, lval);
    if(!token || token == EOF || token == ERROR)
      break;
  }

  yylex_destroy(scanner);

  if(token==ERROR)
    return 1;
 
  return 0;
}
#endif

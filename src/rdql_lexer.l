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
 * 
 */


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

#include <rasqal.h>
#include <rasqal_internal.h>

#include <rdql_parser.h>

static int skip_c_comment(void);

static unsigned char *copy_token(unsigned char *text, size_t len);
unsigned char *copy_string_token(unsigned char *text, size_t len, int delim);

char *filename=NULL;
int lineno=1;

#ifdef STANDALONE
YYSTYPE yylval;
#endif


%}


%%
  /* rules */

  int c;


"//"[^\r\n]*	{ /* C++ comment */
    lineno++;
}

"/*"	{ int l=skip_c_comment();
          lineno+=l;
          if(!l)
            return ERROR; 
        }

[\r\n]+     { lineno++; }

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
"?"      { return VARPREFIX; }

"||"         { return SC_OR; }
"&&"         { return SC_AND; }

"EQ"|"eq"  { return STR_EQ; }
"NE"|"NE"  { return STR_NE; }
"=~"|"~~"|"LIKE"|"like"  { return STR_MATCH; }
"!~"       { return STR_NMATCH; }

"|"      { return BIT_OR; }
"^"     { return BIT_XOR; }
"&"     { return BIT_AND; }

"=="            { return EQ; }
"!="            { return NEQ; }
"<"/[^A-Za-z<=] { return LT; }
">"             { return GT; }
"<="         { return LE; }
">="         { return GE; }

"<<"         { return LSHIFT; }
">>"         { return RSIGNEDSHIFT; }
">>>"        { return RUNSIGNEDSHIFT; }

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
                  yylval.integer=atoi(yytext);
 		  return INTEGER_LITERAL; 
}

0[xX][0-9a-fA-F]+   { c=yytext[yyleng+1];
                      if(c == 'x') 
                        sscanf(yytext+2, "%x", &yylval.integer);
                      else
                        sscanf(yytext+2, "%X", &yylval.integer);
                      return INTEGER_LITERAL; 
}

[0-9]+"."[0-9]*[eE][+-]?[0-9]+[fFdD]?|"."[0-9]+[eE][+-]?[0-9]+[fFdD]?|[0-9]+[eE][+-]?[0-9]+[fFdD]?|[0-9]+[eE][+-]?[0-9]+[fFdD]?  { 
                        sscanf(yytext, "%f", &yylval.floating);
                        return FLOATING_POINT_LITERAL;
}

'([^'\\\n\r]|\\[^\n\r])*'    { yylval.string=copy_string_token(yytext+1, yyleng-2, '\"');
                               return STRING_LITERAL; }

\"([^"\\\n\r]|\\[^\n\r])*\"   { yylval.string=copy_string_token(yytext+1, yyleng-2, '"');
                                return STRING_LITERAL; }

"true"|"false"	{ yylval.integer=(*yytext = 't');
                  return BOOLEAN_LITERAL; }

"null"	{ return NULL_LITERAL; }

[A-Za-z0-9_$\.]+	{ yylval.string=copy_token(yytext, yyleng);
                          return IDENTIFIER; }

\<[A-Za-z][^>]+\>   { yylval.string=copy_token(yytext+1, yyleng-2);
                      return URI_LITERAL; }

\#              { while((c=input()) != '\n' && c)
                    ;
                }

.         	{ if (*yytext == EOF)
                    return END;
                  printf("%s:%d: Syntax error at '%s'\n", filename,
                         lineno, yytext);
                  return ERROR;
		}

%%
  /* user code */

int
yywrap (void) {
  return 1;
}


unsigned char *copy_token(unsigned char *text, size_t len) {
  unsigned char *s;
  if(!len)
    len=strlen(text);
  s=(unsigned char *)malloc(len+1);

  strncpy(s, text, len);
  s[len] = '\0';
  return s;
}


unsigned char *copy_string_token(unsigned char *text, size_t len, int delim) {
  int i;
  unsigned char *s, *d;
  unsigned char *string;
  string=(unsigned char *)malloc(len+1);
  for(s=text, d=string, i=0; i<len; s++, i++) {
    if(*s == '\\' ) {
      s++; i++;
      if(*s == '\n' || *s == '\r' || *s == '\\' || *s == delim)
        *d++=*s;
      else {
        /* don't handle \x where x isn't one of: \n \r \\ (delim) */
      }
    } else
      *d++=*s;
  }
  *d='\0';
  return string;
}


static int
skip_c_comment(void) {
  int lines=0;
  int c;
  
  while(1) {
    while ((c=input()) != '*' && c!= EOF) {
      if(c == '\r' || c == '\n')
        lines++;
    }
    if( c == '*') {
      while ((c=input()) == '*') {
        if(c == '\r' || c == '\n')
          lines++;
      }

      if(c == '/')
        break;
    }
    if (c == EOF) {
      fprintf(stderr, "%s:%d: EOF in comment\n", filename, lineno);
      lines=0;
      break;
    }
  }
  return lines;
}


#ifdef RASQAL_DEBUG

const char *
rdql_token_print(int token)
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

    case BIT_AND:
      return "BIT_AND";

    case BIT_XOR:
      return "BIT_XOR";

    case BIT_OR:
      return "BIT_OR";

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

    case RUNSIGNEDSHIFT:
      return "RUNSIGNEDSHIFT";

    case RSIGNEDSHIFT:
      return "RSIGNEDSHIFT";

    case LSHIFT:
      return "LSHIFT";

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
      sprintf(buffer, "INTEGER_LITERAL(%d)", yylval.integer);
      return buffer;

    case FLOATING_POINT_LITERAL:
      sprintf(buffer, "FLOATING_POINT_LITERAL(%f)", yylval.floating);
      return buffer;

    case STRING_LITERAL:
      sprintf(buffer, "STRING_LITERAL(%s)", yylval.string);
      return buffer;

    case BOOLEAN_LITERAL:
      return (yylval.integer ? "BOOLEAN_LITERAL(true)" : "BOOLEAN_LITERAL(false)");

    case NULL_LITERAL:
      return "NULL_LITERAL";

    case URI_LITERAL:
      sprintf(buffer, "URI_LITERAL(%s)", yylval.string);
      return buffer;

    case IDENTIFIER:
      sprintf(buffer, "IDENTIFIER(%s)", yylval.string);
      return buffer;

    case END:
      return "END";

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
main(int argc, char *argv[]) 
{
  int token=END;

  if(argc > 1) {
    filename=argv[1];
    yyin = fopen(argv[1], "r");
  } else {
    filename="<stdin>";
    yyin = stdin;
    fputs("> ", stdout);
    fflush(stdout);
  }

  while(1) {
    if(yytext != NULL)
      printf("yyinput '%s'\n", yytext);
    token=yylex();
#ifdef RASQAL_DEBUG
    printf("token %s\n", rdql_token_print(token));
#else
    printf("token %d\n", token);
#endif
    if(!token || token == END || token == ERROR)
      break;
  }

  if(token==ERROR)
    return 1;
 
  return 0;
}
#endif

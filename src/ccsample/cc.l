%{
#include <stdio.h>

#include "cc_par.h"

#define YY_USER_ACTION {yylloc->first_line = yylineno; } 

%}

%option 8bit reentrant bison-bridge
%option extra-type="YYNODESTATE *"
%option warn noyywrap nodefault
%option header-file="cc_lex.h"
%option bison-locations
%option yylineno

%%

"//".*                  { /* comment XXX should also suport multi-line comments */ }

[ \t\n\r]               { /* whitespace XXX does this not mean whitespace is not
required anymore between e.g. a keyword and a symbol? */ }

"CompUnit"              { return CompUnit; }
"Exported"              { return Exported; }
"Object"                { return Object; }
"Global"                { return Global; }
"Slot"                  { return Slot; }
"This"                  { return This; }
"Not"                   { return Not; }
"Clone"                 { return Clone; }
"And"                   { return And; }
"Or"                    { return Or; }
"=="                    { return EQ; }
"!="                    { return NE; }
"<="                    { return LE; }
"<"                     { return LT; }
">="                    { return GE; }
">"                     { return GT; }
"True"                  { yylval->bval = true; return BOOL; }
"False"                 { yylval->bval = false; return BOOL; }
"Nil"                   { return Nil; }
"Return"                { return Return; }
"While"                 { return While; }
"Do"                    { return Do; }
"Var"                   { return Var; }
"If"                    { return If; }
"For"                   { return For; }
"In"                    { return In; }
"Switch"                { return Switch; }
"Case"                  { return Case; }
"Default"               { return Default; }

_[A-Za-z][A-Za-z0-9_]*  { yylval->sval = malloc(strlen(yytext)+1);
                          strcpy(yylval->sval, yytext+1);
                          return SYSTEM_SYMBOL; }
\.[A-Za-z][A-Za-z0-9_]* { yylval->sval = malloc(strlen(yytext)+1);
                          strcpy(yylval->sval, yytext+1);
                          return LOCAL_SYMBOL; }
@[A-Za-z][A-Za-z0-9_]*  { yylval->sval = malloc(strlen(yytext)+1);
                          strcpy(yylval->sval, yytext+1);
                          return OBJECT_SYMBOL; }
[A-Za-z][A-Za-z0-9_]*   { yylval->sval = malloc(strlen(yytext)+1); 
						  strcpy(yylval->sval, yytext);
						  return SYMBOL; }

[0-9]+                  { char *yytext_end = yytext + strlen(yytext);
						  yylval->ival = strtol(yytext, &yytext_end, 10);
					      return INTEGER; }
[0-9]*\.[0-9]+          { char *yytext_end = yytext + strlen(yytext); 
						  yylval->fval = strtod(yytext, &yytext_end);
					      return FLOAT; }

\"(\\.|[^\\"])*\"       { yylval->sval = malloc(strlen(yytext));
						  strcpy(yylval->sval, yytext+1);
						  yylval->sval[strlen(yytext)-2] = '\0';
						  return STRING; }

.                       { return yytext[0]; }

%%

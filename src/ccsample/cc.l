%{
#include <stdio.h>

#include "cc_par.h"

#define YY_USER_ACTION {yylloc->first_line = yylineno; } 

%}

%option 8bit reentrant bison-bridge
%option warn noyywrap nodefault
%option header-file="cc_lex.h"
%option bison-locations
%option yylineno

%%

[ \t\n\r]               ;

"CompUnit"              { return CompUnit; }
"Exported"              { return Exported; }
"Object"                { return Object; }
"Global"                { return Global; }
"Slot"                  { return Slot; }
"This"                  { return This; }
"Not"                   { return Not; }
"New"                   { return New; }
"And"                   { return And; }
"Or"                    { return Or; }
"=="                    { return EQ; }
"!="                    { return NE; }
"<="                    { return LE; }
"<"                     { return LT; }
">="                    { return GE; }
">"                     { return GT; }
"True"                  { return True; }
"False"                 { return False; }
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

_[A-Za-z][A-Za-z0-9_]*  { return SYSTEM_SYMBOL; }
\.[A-Za-z][A-Za-z0-9_]* { return LOCAL_SYMBOL; }
@[A-Za-z][A-Za-z0-9_]*  { return OBJECT_SYMBOL; }
[A-Za-z][A-Za-z0-9_]*   { return SYMBOL; }

[0-9]+                  { /* XXX value */ return INTEGER; }
[0-9]*\.[0-9]+          { /* XXX value */ return FLOAT; }
\"[^\"]\"               { /* XXX value */ return STRING; }

.                       { return yytext[0]; }

%%

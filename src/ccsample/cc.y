%code top {

#include <stdio.h>
#include <stdlib.h>
#include "cc_par.h"
#include "cc_lex.h"

void yyerror(YYLTYPE *yyloc, yyscan_t scanner, char const *msg);

}

%code requires {
#include "cc_ast.h"
}

%define api.pure full
%define parse.error verbose
%locations
%parse-param {void *scanner}
%lex-param {void *scanner}

%union {
	int ival;
	float fval;
    char *sval;
    ast_node *node;
}

%token CompUnit
%token Exported
%token Object
%token Global
%token Slot
%token This
%token Not
%token New
%token And
%token Or
%token True
%token False
%token Nil
%token Do
%token While
%token Var
%token Return
%token If
%token Else
%token For
%token In
%token Switch
%token Case
%token Default

%token EQ
%token NE
%token GE
%token GT
%token LE
%token LT

%token LOCAL_SYMBOL
%token OBJECT_SYMBOL
%token SYSTEM_SYMBOL
%token SYMBOL

%token INTEGER
%token FLOAT
%token STRING

%start File

/* XXX Order in here? */
%left And Or
%left EQ NE
%left LT LE GT GE
%left '~'
%left '+' '-' 
%left '*' '/' '%'
%right Not
%right New
%left '.'

%type <node> ObjectDefList
%type <node> GlobalDefList
%type <node> SlotDefList
%type <node> ObjectDef
%type <node> GlobalDef
%type <node> SlotDef

%%

File: CompUnitLine ObjectDefList

CompUnitLine: CompUnit SYMBOL ';'

ObjectDefList: %empty {
        $$ = NULL;
    }
    | ObjectDefList ObjectDef {
        $$ = list_entry_create(yyget_extra(scanner), $1, $2);
    }

ObjectDef: ExportStatement Object SYMBOL '{' GlobalDefList SlotDefList '}' {
        // XXX create object
        $$ = NULL;
    }

ExportStatement: %empty 
    | Exported

GlobalDefList: %empty {
        $$ = NULL;
    }
    | GlobalDefList GlobalDef {
        $$ = list_entry_create(yyget_extra(scanner), $1, $2);
    }

GlobalDef: Global SYMBOL '=' Expression ';'

SlotDefList: %empty {
        $$ = NULL;
    }
    | SlotDefList SlotDef {
        $$ = list_entry_create(yyget_extra(scanner), $1, $2);
    }

SlotDef: Slot SYMBOL '(' OptArgList ')' '{' OptStatementList '}'

OptArgList: %empty
    | ArgList

ArgList: SYMBOL
    | ArgList ',' SYMBOL

Expression: Literal
    | This
    | '(' Expression ')'
    | BinaryMathExpression
    | ComparisonExpression
    | Not Expression
    | New Expression
    | LogicalExpression 
    | SystemCall 
    | ObjectCall
    | LocalCall 
    | SYMBOL
    | LOCAL_SYMBOL
    | OBJECT_SYMBOL

OptExpressionList: %empty
    | ExpressionList

ExpressionList: Expression
    | ExpressionList ',' Expression

SystemCall: SYSTEM_SYMBOL '(' OptExpressionList ')'

ObjectCall: Expression '.' SYMBOL '(' OptExpressionList ')'

LocalCall: LOCAL_SYMBOL '(' OptExpressionList ')'

BinaryMathExpression: Expression '+' Expression
    | Expression '-' Expression
    | Expression '*' Expression
    | Expression '/' Expression
    | Expression '%' Expression
    | Expression '~' Expression 
   
ComparisonExpression: Expression EQ Expression
    | Expression NE Expression
    | Expression LT Expression
    | Expression LE Expression
    | Expression GE Expression
    | Expression GT Expression

LogicalExpression: Expression And Expression
    | Expression Or Expression

Literal: INTEGER
    | FLOAT
    | STRING
    | True 
    | False
    | Nil

OptStatementList: %empty
    | StatementList

StatementList: Statement
    | StatementList Statement


Statement: VarDeclaration
    | Assignment
    | Block
    | ReturnStmt
    | IfStmt
    | WhileStmt
    | DoStmt
    | SwitchStmt
    | ForListStmt
    | ForLoopStmt

VarDeclaration: Var SYMBOL ';'
    | Var SYMBOL '=' Expression ';'

Assignment: SYMBOL '=' Expression ';'
    | LOCAL_SYMBOL '=' Expression ';'

Block: '{' OptStatementList '}'

ReturnStmt: Return Expression ';'

WhileStmt: While Expression Block

DoStmt: Do Block While Expression ';'

IfStmt: If Expression Block ElseIfList OptElse

ElseIfList: %empty 
    | ElseIfList Else If Expression Block

OptElse: %empty
    | Else Block

ForListStmt: For SYMBOL In Expression Block

ForLoopStmt: For '(' SYMBOL '=' Expression ';' Expression ';' Expression ')' Block

SwitchStmt: Switch Expression '{' SwitchCaseList SwitchDefault '}'

SwitchCaseList: %empty
    | SwitchCaseList SwitchCase

SwitchCase: Case Expression Block

SwitchDefault: %empty
    | Default Block

%%

void yyerror(YYLTYPE *yyloc, yyscan_t scanner, char const *msg) {
	fprintf(stderr, "Parse error in line %i: %s\n", 
        yyloc->first_line, msg);
	exit(1);
}

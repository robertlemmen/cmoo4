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
    bool bval;
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
%token <sval> SYMBOL

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

%type <sval> CompUnitLine
%type <node> ObjectDefList
%type <node> GlobalDefList
%type <node> SlotDefList
%type <node> ObjectDef
%type <node> GlobalDef
%type <node> SlotDef
%type <node> Block
%type <node> OptStatementList
%type <node> StatementList
%type <node> Statement
%type <node> VarDeclaration
%type <node> Assignment
%type <node> ReturnStmt
%type <node> IfStmt
%type <node> WhileStmt
%type <node> Expression
%type <node> DoStmt
%type <node> SwitchStmt
%type <node> ForListStmt
%type <node> ForLoopStmt
%type <bval> ExportStatement
%type <node> OptArgList
%type <node> ArgList

%%

/* XXX should assert all nodes are actually the expected type */

File: CompUnitLine ObjectDefList {
    printf("parsed file ok, compunit is '%s'\n", $1);
    printf("contained objects:\n");
    list_entry *cle = (list_entry*)$2;
    dump((ast_node*)cle, 0);
}

CompUnitLine: CompUnit SYMBOL ';' {
    $$ = $2;
}

ObjectDefList: %empty {
        $$ = NULL;
    }
    | ObjectDefList ObjectDef {
        $$ = list_entry_create(yyget_extra(scanner), $2, (list_entry*)$1);
    }

ObjectDef: ExportStatement Object SYMBOL '{' GlobalDefList SlotDefList '}' {
        $$ = object_def_create(yyget_extra(scanner), $3, $1, (list_entry*)$5,
                                            (list_entry*)$6);
    }

ExportStatement: %empty {
        $$ = false;
    }
    | Exported {
        $$ = true;
    }

GlobalDefList: %empty {
        $$ = NULL;
    }
    | GlobalDefList GlobalDef {
        $$ = list_entry_create(yyget_extra(scanner), $2, (list_entry*)$1);
    }

/* XXX perhaps this should only take literal expressions */
GlobalDef: Global SYMBOL '=' Expression ';' {
        $$ = global_create(yyget_extra(scanner), $2, NULL);
    }

SlotDefList: %empty {
        $$ = NULL;
    }
    | SlotDefList SlotDef {
        $$ = list_entry_create(yyget_extra(scanner), $2, (list_entry*)$1);
    }

SlotDef: Slot SYMBOL '(' OptArgList ')' Block {
        $$ = slot_create(yyget_extra(scanner), $2, (list_entry*)$4, (block*)$6);
    }

// XXX why do have optarglist and not just make the arglist start empty like the
// lists above?
OptArgList: %empty {
        $$ = NULL;
    }
    | ArgList {
        $$ = $1;
    }

ArgList: SYMBOL {
        $$ = list_entry_create(yyget_extra(scanner),
                argument_create(yyget_extra(scanner), $1), NULL);
    }
    | ArgList ',' SYMBOL {
        $$ = list_entry_create(yyget_extra(scanner), 
                argument_create(yyget_extra(scanner), $3), (list_entry*)$1);
    }

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

OptStatementList: %empty {
        $$ = NULL;
    }
    | StatementList {
        $$ = $1;
    }

StatementList: Statement {
        $$ = list_entry_create(yyget_extra(scanner),
                $1, NULL);
    }
    | StatementList Statement {
        $$ = list_entry_create(yyget_extra(scanner),
                $2, (list_entry*)$1);
    }


Statement: VarDeclaration {
        $$ = $1;
    }
    | Assignment {
        $$ = $1;
    }
    | Block {
        $$ = $1;
    }
    | ReturnStmt {
        $$ = $1;
    }
    | IfStmt {
        $$ = $1;
    }
    | WhileStmt {
        $$ = $1;
    }
    | DoStmt {
        $$ = $1;
    }
    | SwitchStmt {
        $$ = $1;
    }
    | ForListStmt {
        $$ = $1;
    }
    | ForLoopStmt {
        $$ = $1;
    }


VarDeclaration: Var SYMBOL ';' {
        $$ = (ast_node*)var_decl_create(yyget_extra(scanner),
            $2, NULL);
    }
    | Var SYMBOL '=' Expression ';' {
        $$ = (ast_node*)var_decl_create(yyget_extra(scanner),
            $2, (expression*)$4);
    }

Assignment: SYMBOL '=' Expression ';'
    | LOCAL_SYMBOL '=' Expression ';'

Block: '{' OptStatementList '}' {
        $$ = (ast_node*)block_create(yyget_extra(scanner), (list_entry*)$2);
    }

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

%code top {

#include <stdio.h>
#include <stdlib.h>
#include "cc_par.h"
#include "cc_lex.h"

void yyerror(YYLTYPE *yyloc, yyscan_t scanner, char const *msg);

}

%code requires {
#include "cc_ast.h"
#include "cmc.h"
}

%define api.pure full
%define parse.error verbose
%locations
%param {void *scanner}

%union {
	int ival;
	float fval;
    char *sval;
    bool bval;
    ast_node *node;
}

%start StartSelector

/* these two are used to select whether we compile a whole compilation
unit, or just a slot */
%token START_COMPUNIT 
%token START_SLOT 

%token CompUnit
%token Exported
%token Object
%token Global
%token Slot
%token This
%token Not
%token Clone
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

%token <sval> LOCAL_SYMBOL
%token <sval> OBJECT_SYMBOL
%token <sval> SYSTEM_SYMBOL
%token <sval> SYMBOL

%token <ival> INTEGER
%token <fval> FLOAT
%token <sval> STRING

<<<<<<< HEAD
=======
/* XXX Order in here? */
>>>>>>> 9066fa0f857cb7b17cb32e84208f8da27844d899
%left And Or
%left EQ NE
%left LT LE GT GE
%left '~'
%left '+' '-' 
%left '*' '/' '%'
%left Not
%left Clone
%left '.'

%token ';'
%token '{'
%token '}'
%token '='
%token '('
%token ')'
%token ','
%token '['
%token ']'

%type <node> File
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
%type <node> ExpressionList
%type <node> OptExpressionList
%type <node> BinaryMathExpression
%type <node> ComparisonExpression
%type <node> LogicalExpression
%type <node> SystemCall
/*%type <node> ObjectCall*/
%type <node> LocalCall
%type <node> Literal
%type <node> DoStmt
%type <node> SwitchStmt
%type <node> ForListStmt
%type <node> ForLoopStmt
%type <bval> ExportedTag
%type <node> OptArgList
%type <node> ArgList

%%

StartSelector:    START_COMPUNIT File {
                    yyget_extra(scanner)->resp = (ast_node*)$2;
                }
                | START_SLOT SlotDef {
                    yyget_extra(scanner)->resp = (ast_node*)$2;
                }

File: CompUnitLine ObjectDefList {
    $$ = comp_unit_create(&yyget_extra(scanner)->ast_state, $1, (list_entry*)$2);
}

CompUnitLine: CompUnit SYMBOL ';' {
    $$ = $2;
}

ObjectDefList: %empty {
        $$ = NULL;
    }
    | ObjectDefList ObjectDef {
        $$ = list_entry_create(&yyget_extra(scanner)->ast_state, $2, (list_entry*)$1);
    }

ObjectDef: ExportedTag Object SYMBOL '{' GlobalDefList SlotDefList '}' {
        $$ = object_def_create(&yyget_extra(scanner)->ast_state, $3, $1, (list_entry*)$5,
                                            (list_entry*)$6);
    }

ExportedTag: %empty {
        $$ = false;
    }
    | Exported {
        $$ = true;
    }

GlobalDefList: %empty {
        $$ = NULL;
    }
    | GlobalDefList GlobalDef {
        $$ = list_entry_create(&yyget_extra(scanner)->ast_state, $2, (list_entry*)$1);
    }

GlobalDef: Global SYMBOL ';' {
        $$ = global_create(&yyget_extra(scanner)->ast_state, $2, 
			(ast_node*)literal_create(&yyget_extra(scanner)->ast_state, val_make_nil()));
    }
	| Global SYMBOL '=' Literal ';' {
        $$ = global_create(&yyget_extra(scanner)->ast_state, $2, $4);
    }

SlotDefList: %empty {
        $$ = NULL;
    }
    | SlotDefList SlotDef {
        $$ = list_entry_create(&yyget_extra(scanner)->ast_state, $2, (list_entry*)$1);
    }

SlotDef: Slot SYMBOL '(' OptArgList ')' Block {
        $$ = slot_create(&yyget_extra(scanner)->ast_state, $2, (list_entry*)$4, (block*)$6);
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
        $$ = list_entry_create(&yyget_extra(scanner)->ast_state,
                argument_create(&yyget_extra(scanner)->ast_state, $1), NULL);
    }
    | ArgList ',' SYMBOL {
        $$ = list_entry_create(&yyget_extra(scanner)->ast_state, 
                argument_create(&yyget_extra(scanner)->ast_state, $3), (list_entry*)$1);
    }

Expression: Literal {
		$$ = $1;
	}
    | This {
		$$ = (ast_node*)this_expr_create(&yyget_extra(scanner)->ast_state);
	}
    | '(' Expression ')' {
		$$ = $2;
	}
    | BinaryMathExpression {
        $$ = $1;
    }
    | ComparisonExpression {
        $$ = $1;
    }
    | Not Expression {
		$$ = (ast_node*)not_expr_create(&yyget_extra(scanner)->ast_state, (expression*)$2);
    }
    | Clone Expression {
		$$ = (ast_node*)clone_expr_create(&yyget_extra(scanner)->ast_state, (expression*)$2);
    }
    | LogicalExpression  {
        $$ = $1;
    }
    | SystemCall  {
        $$ = $1;
    }
/*    | ObjectCall {
        $$ = $1;
    }*/
    | LocalCall  {
        $$ = $1;
    }
    | SYMBOL {
        $$ = (ast_node*)symbol_lookup_create(&yyget_extra(scanner)->ast_state, $1, SYMBOL_VAR);
    }
    | LOCAL_SYMBOL {
        $$ = (ast_node*)symbol_lookup_create(&yyget_extra(scanner)->ast_state, $1, SYMBOL_LOCAL);
    }

OptExpressionList: %empty {
        $$ = NULL;
    }
    | ExpressionList {
        $$ = $1;
    }

ExpressionList: Expression {
        $$ = list_entry_create(&yyget_extra(scanner)->ast_state, $1, NULL);
    }
    | ExpressionList ',' Expression {
        $$ = list_entry_create(&yyget_extra(scanner)->ast_state, $3, (list_entry*)$1);
    }

SystemCall: SYSTEM_SYMBOL '(' OptExpressionList ')' {
        $$ = (ast_node*)call_create(&yyget_extra(scanner)->ast_state, NULL, $1, (list_entry*)$3, SYMBOL_SYSTEM);
    }

/*
XXX this causes major shift/reduce conflicts, what do  I want with this anyway?
ObjectCall: Expression LOCAL_SYMBOL '(' OptExpressionList ')' {
        $$ = (ast_node*)call_create(&yyget_extra(scanner)->ast_state, (expression*)$1, $2, (list_entry*)$4, SYMBOL_OBJECT);
    }*/

LocalCall: LOCAL_SYMBOL '(' OptExpressionList ')' {
        $$ = (ast_node*)call_create(&yyget_extra(scanner)->ast_state, NULL, $1, (list_entry*)$3, SYMBOL_LOCAL);
    }

BinaryMathExpression: Expression '+' Expression {
        $$ = (ast_node*)binary_expr_create(&yyget_extra(scanner)->ast_state, (expression*)$1, (expression*)$3, OP_PLUS);
    }
    | Expression '-' Expression {
        $$ = (ast_node*)binary_expr_create(&yyget_extra(scanner)->ast_state, (expression*)$1, (expression*)$3, OP_MINUS);
    }
    | Expression '*' Expression {
        $$ = (ast_node*)binary_expr_create(&yyget_extra(scanner)->ast_state, (expression*)$1, (expression*)$3, OP_MUL);
    }
    | Expression '/' Expression {
        $$ = (ast_node*)binary_expr_create(&yyget_extra(scanner)->ast_state, (expression*)$1, (expression*)$3, OP_DIV);
    }
    | Expression '%' Expression {
        $$ = (ast_node*)binary_expr_create(&yyget_extra(scanner)->ast_state, (expression*)$1, (expression*)$3, OP_MOD);
    }
    | Expression '~' Expression {
        $$ = (ast_node*)binary_expr_create(&yyget_extra(scanner)->ast_state, (expression*)$1, (expression*)$3, OP_CONCAT);
    }
   
ComparisonExpression: Expression EQ Expression {
        $$ = (ast_node*)binary_expr_create(&yyget_extra(scanner)->ast_state, (expression*)$1, (expression*)$3, OP_EQ);
    }
    | Expression NE Expression {
        $$ = (ast_node*)binary_expr_create(&yyget_extra(scanner)->ast_state, (expression*)$1, (expression*)$3, OP_NE);
    }
    | Expression LT Expression {
        $$ = (ast_node*)binary_expr_create(&yyget_extra(scanner)->ast_state, (expression*)$1, (expression*)$3, OP_LT);
    }
    | Expression LE Expression {
        $$ = (ast_node*)binary_expr_create(&yyget_extra(scanner)->ast_state, (expression*)$1, (expression*)$3, OP_LE);
    }
    | Expression GE Expression {
        $$ = (ast_node*)binary_expr_create(&yyget_extra(scanner)->ast_state, (expression*)$1, (expression*)$3, OP_GE);
    }
    | Expression GT Expression {
        $$ = (ast_node*)binary_expr_create(&yyget_extra(scanner)->ast_state, (expression*)$1, (expression*)$3, OP_GT);
    }

LogicalExpression: Expression And Expression {
        $$ = (ast_node*)binary_expr_create(&yyget_extra(scanner)->ast_state, (expression*)$1, (expression*)$3, OP_AND);
    }
    | Expression Or Expression {
        $$ = (ast_node*)binary_expr_create(&yyget_extra(scanner)->ast_state, (expression*)$1, (expression*)$3, OP_OR);
    }

Literal: INTEGER {
		$$ = (ast_node*)literal_create(&yyget_extra(scanner)->ast_state, val_make_int($1));
	}
    | FLOAT {
		$$ = (ast_node*)literal_create(&yyget_extra(scanner)->ast_state, val_make_float($1));
	}
    | STRING {
		$$ = (ast_node*)literal_create(&yyget_extra(scanner)->ast_state, val_make_string(strlen($1), $1));
	}
    | True {
		$$ = (ast_node*)literal_create(&yyget_extra(scanner)->ast_state, val_make_bool(true));
	}
    | False {
		$$ = (ast_node*)literal_create(&yyget_extra(scanner)->ast_state, val_make_bool(false));
	}
    | Nil {
		$$ = (ast_node*)literal_create(&yyget_extra(scanner)->ast_state, val_make_nil());
	}
    | OBJECT_SYMBOL {
        $$ = (ast_node*)symbol_lookup_create(&yyget_extra(scanner)->ast_state, $1, SYMBOL_OBJECT);
    }

OptStatementList: %empty {
        $$ = NULL;
    }
    | StatementList {
        $$ = $1;
    }

StatementList: Statement {
        $$ = list_entry_create(&yyget_extra(scanner)->ast_state,
                $1, NULL);
    }
    | StatementList Statement {
        $$ = list_entry_create(&yyget_extra(scanner)->ast_state,
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
    | Expression ';' {
        $$ = (ast_node*)expr_exec_create(&yyget_extra(scanner)->ast_state, (expression*)$1);
    }

VarDeclaration: Var SYMBOL ';' {
        $$ = (ast_node*)var_decl_create(&yyget_extra(scanner)->ast_state, $2,
			(expression*)literal_create(&yyget_extra(scanner)->ast_state, val_make_nil()));
    }
    | Var SYMBOL '=' Expression ';' {
        $$ = (ast_node*)var_decl_create(&yyget_extra(scanner)->ast_state,
            $2, (expression*)$4);
    }

Assignment: SYMBOL '=' Expression ';' {
        $$ = (ast_node*)assignment_create(&yyget_extra(scanner)->ast_state, $1, (expression*)$3, SYMBOL_VAR);
    }
    | LOCAL_SYMBOL '=' Expression ';' {
        $$ = (ast_node*)assignment_create(&yyget_extra(scanner)->ast_state, $1, (expression*)$3, SYMBOL_LOCAL);
    }

Block: '{' OptStatementList '}' {
        $$ = (ast_node*)block_create(&yyget_extra(scanner)->ast_state, (list_entry*)$2);
    }

ReturnStmt: Return Expression ';' {
        $$ = (ast_node*)return_stmt_create(&yyget_extra(scanner)->ast_state, (expression*)$2);
    }

// XXX should also allow a local declaration ??
WhileStmt: While Expression Block {
        $$ = (ast_node*)while_stmt_create(&yyget_extra(scanner)->ast_state, 
                                          (expression*)$2, (block*)$3);
}

DoStmt: Do Block While Expression ';' {
        $$ = (ast_node*)do_stmt_create(&yyget_extra(scanner)->ast_state, 
                                          (block*)$2, (expression*)$4);
}

IfStmt: If Expression Block ElseIfList OptElse

ElseIfList: %empty 
    | ElseIfList Else If Expression Block

OptElse: %empty
    | Else Block

// XXX should also allow a local declaration!
ForListStmt: For SYMBOL In Expression Block

// xxx should also allow a local declaration!
ForLoopStmt: For '(' SYMBOL '=' Expression ';' Expression ';' Expression ')' Block

SwitchStmt: Switch Expression '{' SwitchCaseList SwitchDefault '}'

SwitchCaseList: %empty
    | SwitchCaseList SwitchCase

SwitchCase: Case Expression Block

SwitchDefault: %empty
    | Default Block

%%

// XXX this should just forward to the outer wih an extra parameter
void yyerror(YYLTYPE *yyloc, yyscan_t scanner, char const *msg) {
    struct cmc_ctx *ctx = yyget_extra(scanner);
    if (ctx->error_msg) {
        free(ctx->error_msg);
    }
    char *fmt = "Error in line %i: %s\n";
    int ret = snprintf(ctx->error_msg, 0, fmt, yyloc->first_line, msg);
    ctx->error_msg = malloc(ret);
    snprintf(ctx->error_msg, ret, fmt, yyloc->first_line, msg);
}

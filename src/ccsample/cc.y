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

%token EQ
%token NE
%token GE
%token GT
%token LE
%token LT

%token <sval> LOCAL_SYMBOL
%token <sval> OBJECT_SYMBOL
%token <sval> SYSTEM_SYMBOL
%token <sval> SYMBOL

%token <ival> INTEGER
%token <fval> FLOAT
%token <sval> STRING
%token <bval> BOOL

%start File

/* XXX Order in here? */
%left And Or
%left EQ NE
%left LT LE GT GE
%left '~'
%left '+' '-' 
%left '*' '/' '%'
%left Not
%left Clone
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
%type <node> ExpressionList
%type <node> OptExpressionList
%type <node> BinaryMathExpression
%type <node> ComparisonExpression
%type <node> LogicalExpression
%type <node> SystemCall
%type <node> ObjectCall
%type <node> LocalCall
%type <node> Literal
%type <node> DoStmt
%type <node> SwitchStmt
%type <node> ForListStmt
%type <node> ForLoopStmt
%type <bval> ExportStatement
%type <node> OptArgList
%type <node> ArgList

%%

File: CompUnitLine ObjectDefList {
    printf("parsed file ok, compunit is '%s'\n", $1);
    printf("contained objects:\n");
    list_entry *cle = (list_entry*)$2;
    dump((ast_node*)cle, 0, false, false);
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

GlobalDef: Global SYMBOL ';' {
        $$ = global_create(yyget_extra(scanner), $2, 
			(ast_node*)literal_create(yyget_extra(scanner), val_make_nil()));
    }
	| Global SYMBOL '=' Literal ';' {
        $$ = global_create(yyget_extra(scanner), $2, $4);
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

Expression: Literal {
		$$ = $1;
	}
    | This {
		$$ = (ast_node*)this_expr_create(yyget_extra(scanner));
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
		$$ = (ast_node*)not_expr_create(yyget_extra(scanner), (expression*)$2);
    }
    | Clone Expression {
		$$ = (ast_node*)clone_expr_create(yyget_extra(scanner), (expression*)$2);
    }
    | LogicalExpression  {
        $$ = $1;
    }
    | SystemCall  {
        $$ = $1;
    }
    | ObjectCall {
        $$ = $1;
    }
    | LocalCall  {
        $$ = $1;
    }
    | SYMBOL {
        $$ = (ast_node*)symbol_lookup_create(yyget_extra(scanner), $1, SYMBOL_VAR);
    }
    | LOCAL_SYMBOL {
        $$ = (ast_node*)symbol_lookup_create(yyget_extra(scanner), $1, SYMBOL_LOCAL);
    }

OptExpressionList: %empty {
        $$ = NULL;
    }
    | ExpressionList {
        $$ = $1;
    }

ExpressionList: Expression {
        $$ = list_entry_create(yyget_extra(scanner), $1, NULL);
    }
    | ExpressionList ',' Expression {
        $$ = list_entry_create(yyget_extra(scanner), $3, (list_entry*)$1);
    }

SystemCall: SYSTEM_SYMBOL '(' OptExpressionList ')' {
        $$ = (ast_node*)call_create(yyget_extra(scanner), NULL, $1, (list_entry*)$3, SYMBOL_SYSTEM);
    }

ObjectCall: Expression LOCAL_SYMBOL '(' OptExpressionList ')' {
        $$ = (ast_node*)call_create(yyget_extra(scanner), (expression*)$1, $2, (list_entry*)$4, SYMBOL_OBJECT);
    }

LocalCall: LOCAL_SYMBOL '(' OptExpressionList ')' {
        $$ = (ast_node*)call_create(yyget_extra(scanner), NULL, $1, (list_entry*)$3, SYMBOL_LOCAL);
    }

BinaryMathExpression: Expression '+' Expression {
        $$ = (ast_node*)binary_expr_create(yyget_extra(scanner), (expression*)$1, (expression*)$3, OP_PLUS);
    }
    | Expression '-' Expression {
        $$ = (ast_node*)binary_expr_create(yyget_extra(scanner), (expression*)$1, (expression*)$3, OP_MINUS);
    }
    | Expression '*' Expression {
        $$ = (ast_node*)binary_expr_create(yyget_extra(scanner), (expression*)$1, (expression*)$3, OP_MUL);
    }
    | Expression '/' Expression {
        $$ = (ast_node*)binary_expr_create(yyget_extra(scanner), (expression*)$1, (expression*)$3, OP_DIV);
    }
    | Expression '%' Expression {
        $$ = (ast_node*)binary_expr_create(yyget_extra(scanner), (expression*)$1, (expression*)$3, OP_MOD);
    }
    | Expression '~' Expression {
        $$ = (ast_node*)binary_expr_create(yyget_extra(scanner), (expression*)$1, (expression*)$3, OP_CONCAT);
    }
   
ComparisonExpression: Expression EQ Expression {
        $$ = (ast_node*)binary_expr_create(yyget_extra(scanner), (expression*)$1, (expression*)$3, OP_EQ);
    }
    | Expression NE Expression {
        $$ = (ast_node*)binary_expr_create(yyget_extra(scanner), (expression*)$1, (expression*)$3, OP_NE);
    }
    | Expression LT Expression {
        $$ = (ast_node*)binary_expr_create(yyget_extra(scanner), (expression*)$1, (expression*)$3, OP_LT);
    }
    | Expression LE Expression {
        $$ = (ast_node*)binary_expr_create(yyget_extra(scanner), (expression*)$1, (expression*)$3, OP_LE);
    }
    | Expression GE Expression {
        $$ = (ast_node*)binary_expr_create(yyget_extra(scanner), (expression*)$1, (expression*)$3, OP_GE);
    }
    | Expression GT Expression {
        $$ = (ast_node*)binary_expr_create(yyget_extra(scanner), (expression*)$1, (expression*)$3, OP_GT);
    }

LogicalExpression: Expression And Expression {
        $$ = (ast_node*)binary_expr_create(yyget_extra(scanner), (expression*)$1, (expression*)$3, OP_AND);
    }
    | Expression Or Expression {
        $$ = (ast_node*)binary_expr_create(yyget_extra(scanner), (expression*)$1, (expression*)$3, OP_OR);
    }

Literal: INTEGER {
		$$ = (ast_node*)literal_create(yyget_extra(scanner), val_make_int($1));
	}
    | FLOAT {
		$$ = (ast_node*)literal_create(yyget_extra(scanner), val_make_float($1));
	}
    | STRING {
		$$ = (ast_node*)literal_create(yyget_extra(scanner), val_make_string(strlen($1), $1));
	}
    | BOOL {
		$$ = (ast_node*)literal_create(yyget_extra(scanner), val_make_bool($1));
	}
    | Nil {
		$$ = (ast_node*)literal_create(yyget_extra(scanner), val_make_nil());
	}
    | OBJECT_SYMBOL {
        $$ = (ast_node*)symbol_lookup_create(yyget_extra(scanner), $1, SYMBOL_OBJECT);
    }

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
    | Expression ';' {
        $$ = (ast_node*)expr_exec_create(yyget_extra(scanner), (expression*)$1);
    }

VarDeclaration: Var SYMBOL ';' {
        $$ = (ast_node*)var_decl_create(yyget_extra(scanner), $2,
			(expression*)literal_create(yyget_extra(scanner), val_make_nil()));
    }
    | Var SYMBOL '=' Expression ';' {
        $$ = (ast_node*)var_decl_create(yyget_extra(scanner),
            $2, (expression*)$4);
    }

Assignment: SYMBOL '=' Expression ';' {
        $$ = (ast_node*)assignment_create(yyget_extra(scanner), $1, (expression*)$3, SYMBOL_VAR);
    }
    | LOCAL_SYMBOL '=' Expression ';' {
        $$ = (ast_node*)assignment_create(yyget_extra(scanner), $1, (expression*)$3, SYMBOL_LOCAL);
    }

Block: '{' OptStatementList '}' {
        $$ = (ast_node*)block_create(yyget_extra(scanner), (list_entry*)$2);
    }

ReturnStmt: Return Expression ';' {
        $$ = (ast_node*)return_stmt_create(yyget_extra(scanner), (expression*)$2);
    }

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

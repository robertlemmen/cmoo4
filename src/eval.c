#include "eval.h"

#include <stdlib.h>
// XXX
#include <stdio.h>

#include "types.h"

#define INITIAL_STACK_SIZE  1024

union stack_element {
    val val;
    union stack_element *se;
    opcode *code;
};

struct eval_ctx {
    // our base registers
    union stack_element *fp;
    union stack_element *sp;
    // the actual stack
    union stack_element *stack;
    union stack_element *stack_top;
    // debug handler
    void (*callback)(val v, void *a);
    void *cb_arg;
};

struct eval_ctx* eval_new_ctx(void) {
    struct eval_ctx *ret = malloc(sizeof(struct eval_ctx));
    ret->stack = malloc(sizeof(union stack_element) * INITIAL_STACK_SIZE);
    ret->stack_top = ret->stack 
        + sizeof(union stack_element) * INITIAL_STACK_SIZE;
    ret->fp = ret->stack;
    ret->sp = ret->stack;
    ret->callback = NULL;
    return ret;
}

void eval_set_dbg_handler(struct eval_ctx *ctx, 
        void (*callback)(val v, void *a), 
        void *a) {
    ctx->callback = callback;
    ctx->cb_arg = a;
}

void eval_free_ctx(struct eval_ctx *ctx) {
    free(ctx->stack);
    free(ctx);
}

void eval_exec(struct eval_ctx *ctx, opcode *code) {
    void* dispatch_table[] = {
        &&do_noop,
        &&do_abort,
        &&do_debugi,
        &&do_debugr,
        &&do_mov,
        &&do_push,
        &&do_pop,
        &&do_call,
        &&do_return,
        &&do_args_locals,
        &&do_clear,
        &&do_true,
        &&do_load_int,
        &&do_load_float,
        &&do_type,
        &&do_logical_and,
        &&do_logical_or,
        &&do_eq,
        &&do_lq,
        &&do_lt,
        &&do_add,
        &&do_sub,
        &&do_mul,
        &&do_div,
        &&do_mod,
        &&do_jump,
        &&do_jump_if,
        &&do_jump_eq,
        &&do_jump_ne,
        &&do_jump_le,
        &&do_jump_lt
    };
    #define DISPATCH() goto *dispatch_table[*ip++]

    opcode *ip = code;
    printf(",---------------------------------,\n");
    DISPATCH();
    while(1) {
        do_noop:
            printf("| NOOP                            |\n");
            DISPATCH();
        do_abort:
            printf("| ABORT                           |\n");
            return;
            DISPATCH();
        do_debugi:
            // XXX decode argument and pass in
            printf("| DEBUGI                          |\n");
            if (ctx->callback) {
                ctx->callback(val_make_int(42), ctx->cb_arg);
            }
            DISPATCH();
        do_debugr:
            DISPATCH();
        do_mov:
            DISPATCH();
        do_push:
            DISPATCH();
        do_pop:
            DISPATCH();
        do_call:
            DISPATCH();
        do_return:
            DISPATCH();
        do_args_locals:
            DISPATCH();
        do_clear:
            DISPATCH();
        do_true:
            DISPATCH();
        do_load_int:
            DISPATCH();
        do_load_float:
            DISPATCH();
        do_type:
            DISPATCH();
        do_logical_and:
            DISPATCH();
        do_logical_or:
            DISPATCH();
        do_eq:
            DISPATCH();
        do_lq:
            DISPATCH();
        do_lt:
            DISPATCH();
        do_add:
            DISPATCH();
        do_sub:
            DISPATCH();
        do_mul:
            DISPATCH();
        do_div:
            DISPATCH();
        do_mod:
            DISPATCH();
        do_jump:
            DISPATCH();
        do_jump_if:
            DISPATCH();
        do_jump_eq:
            DISPATCH();
        do_jump_ne:
            DISPATCH();
        do_jump_le:
            DISPATCH();
        do_jump_lt:
            DISPATCH();
    }
}

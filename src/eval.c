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
    // XXX temporary clutch for testing, need proper initialization of 
    // stack instead
    ret->fp++;
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
    // XXX we probably want to cache sp/fp/ip in register variables
    void* dispatch_table[] = {
        &&do_noop,
        &&do_halt,
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
    // some vars we need below, can't be declared after label...
    while(1) {
        do_noop: {
            printf("| NOOP                            |\n");
            DISPATCH();
        }
        do_halt: {
            printf("| HALT                            |\n");
            printf("'---------------------------------'\n");
            return;
            DISPATCH();
        }
        do_debugi: {
            int32_t msg = *((int32_t*)ip);
            ip += 4;
            printf("| DEBUGI 0x%08X               |\n", msg);
            if (ctx->callback) {
                ctx->callback(val_make_int(msg), ctx->cb_arg);
            }
            DISPATCH();
        }
        do_debugr: {
            uint8_t msg_r = *((uint8_t*)ip);
            ip += 1;
            printf("| DEBUGR r0x%02X                    |\n", msg_r);
            if (&ctx->fp[msg_r] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (ctx->callback) {
                ctx->callback(ctx->fp[msg_r].val, ctx->cb_arg);
            }
            DISPATCH();
        }
        do_mov: {
            uint8_t dst = *((uint8_t*)ip);
            ip += 1;
            uint8_t src = *((uint8_t*)ip);
            ip += 1;
            printf("| MOV r0x%02X <- r0x%02X              |\n", dst, src);
            if (&ctx->fp[dst] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (&ctx->fp[src] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            val_clear(&ctx->fp[dst].val);
            ctx->fp[dst].val = ctx->fp[src].val;
            // XXX need to increment refcount for non-immediates
            DISPATCH();
        }
        do_push: {
            DISPATCH();
        }
        do_pop: {
            DISPATCH();
        }
        do_call: {
            DISPATCH();
        }
        do_return: {
            DISPATCH();
        }
        do_args_locals: {
            uint8_t nargs = *((uint8_t*)ip);
            ip += 1;
            uint8_t nlocals = *((uint8_t*)ip);
            ip += 1;
            printf("| ARGS_LOCALS 0x%02X 0x%02X           |\n", nargs, nlocals);
            if (ctx->sp != &ctx->fp[nargs - 1]) {
                // XXX raise
                printf("!! invalid number of arguments\n");
            }
            // XXX needs to check and grow stack
            for (int i = 0; i < nlocals; i++) {
                val_init(&ctx->sp[i + 1].val);
            }
            ctx->sp += nlocals;
            DISPATCH();
        }
        do_clear: {
            uint8_t reg = *((uint8_t*)ip);
            ip += 1;
            printf("| CLEAR r0x%02X                     |\n", reg);
            if (&ctx->fp[reg] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            val_clear(&ctx->fp[reg].val);
            DISPATCH();
        }
        do_true: {
            uint8_t reg = *((uint8_t*)ip);
            ip += 1;
            printf("| TRUE r0x%02X <- TRUE              |\n", reg);
            if (&ctx->fp[reg] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            val_clear(&ctx->fp[reg].val);
            ctx->fp[reg].val = val_make_bool(true);
            DISPATCH();
        }
        do_load_int: {
            uint8_t reg = *((uint8_t*)ip);
            ip += 1;
            int32_t nval = *((int32_t*)ip);
            ip += 4;
            printf("| LOAD_INT r0x%02X <- 0x%08X    |\n", reg, nval);
            if (&ctx->fp[reg] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            val_clear(&ctx->fp[reg].val);
            ctx->fp[reg].val = val_make_int(nval);
            DISPATCH();
        }
        do_load_float: {
            DISPATCH();
        }
        do_type: {
            uint8_t dst = *((uint8_t*)ip);
            ip += 1;
            uint8_t src = *((uint8_t*)ip);
            ip += 1;
            printf("| TYPE r0x%02X <- r0x%02X             |\n", dst, src);
            if (&ctx->fp[dst] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (&ctx->fp[src] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            val_clear(&ctx->fp[dst].val);
            ctx->fp[dst].val = val_make_int(val_type(ctx->fp[src].val));
            DISPATCH();
        }
        do_logical_and: {
            DISPATCH();
        }
        do_logical_or: {
            DISPATCH();
        }
        do_eq: {
            DISPATCH();
        }
        do_lq: {
            DISPATCH();
        }
        do_lt: {
            DISPATCH();
        }
        do_add: {
            DISPATCH();
        }
        do_sub: {
            DISPATCH();
        }
        do_mul: {
            DISPATCH();
        }
        do_div: {
            DISPATCH();
        }
        do_mod: {
            DISPATCH();
        }
        do_jump: {
            DISPATCH();
        }
        do_jump_if: {
            DISPATCH();
        }
        do_jump_eq: {
            DISPATCH();
        }
        do_jump_ne: {
            DISPATCH();
        }
        do_jump_le: {
            DISPATCH();
        }
        do_jump_lt: {
            DISPATCH();
        }
    }
}

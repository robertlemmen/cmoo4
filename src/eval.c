#include "eval.h"

#include <stdlib.h>
#include <string.h>
// XXX
#include <stdio.h>

#include "types.h"

#define INITIAL_STACK_SIZE  1024

union stack_element {
    val val;
    union stack_element *se;
    opcode *code;
};

struct syscall_entry {
    uint8_t arity;
    char *name;
    union syscall_arity {
        val (*a0)(void);
        val (*a1)(val v1);
        val (*a2)(val v1, val v2);
    } funcptr;
    struct syscall_entry *next;
};

struct syscall_table {
    struct syscall_entry *syscalls;
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
    // syscall table
    struct syscall_table *syscall_table;
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
        &&do_logical_not,
        &&do_eq,
        &&do_le,
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
        &&do_jump_lt,
        &&do_syscall,
    };
    #define DISPATCH() goto *dispatch_table[*ip++]

    opcode *ip = code;
    printf(",----------------------------------,\n");
    DISPATCH();
    // some vars we need below, can't be declared after label...
    while(1) {
        do_noop: {
            printf("| NOOP                             |\n");
            DISPATCH();
        }
        do_halt: {
            printf("| HALT                             |\n");
            printf("'----------------------------------'\n");
            return;
            DISPATCH();
        }
        do_debugi: {
            int32_t msg = *((int32_t*)ip);
            ip += 4;
            printf("| DEBUGI 0x%08X                |\n", msg);
            if (ctx->callback) {
                ctx->callback(val_make_int(msg), ctx->cb_arg);
            }
            DISPATCH();
        }
        do_debugr: {
            uint8_t msg_r = *((uint8_t*)ip);
            ip += 1;
            printf("| DEBUGR r0x%02X                     |\n", msg_r);
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
            printf("| MOV r0x%02X <- r0x%02X               |\n", dst, src);
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
            val_inc_ref(ctx->fp[src].val);
            // XXX need to increment refcount for non-immediates
            DISPATCH();
        }
        do_push: {
            uint8_t src = *((uint8_t*)ip);
            ip += 1;
            printf("| PUSH r0x%02X                       |\n", src);
            if (&ctx->fp[src] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            // XXX make sure there is space on stack
            ctx->sp++;
            // no cleanup of target needed as we are growing the stack,
            // therefore the target is already clean
            ctx->sp->val = ctx->fp[src].val;
            val_inc_ref(ctx->fp[src].val);
            DISPATCH();
        }
        do_pop: {
            uint8_t dst = *((uint8_t*)ip);
            ip += 1;
            if (&ctx->fp[dst] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            val_clear(&ctx->fp[dst].val);
            ctx->fp[dst].val = ctx->sp->val;
            ctx->sp--;
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
            printf("| ARGS_LOCALS 0x%02X 0x%02X            |\n", nargs, nlocals);
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
            printf("| CLEAR r0x%02X                      |\n", reg);
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
            printf("| TRUE r0x%02X <- TRUE               |\n", reg);
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
            printf("| LOAD_INT r0x%02X <- 0x%08X     |\n", reg, nval);
            if (&ctx->fp[reg] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            val_clear(&ctx->fp[reg].val);
            ctx->fp[reg].val = val_make_int(nval);
            DISPATCH();
        }
        do_load_float: {
            uint8_t reg = *((uint8_t*)ip);
            ip += 1;
            float nval = *((float*)ip);
            ip += 4;
            printf("| LOAD_FLOAT r0x%02X <- %8.3f     |\n", reg, nval);
            if (&ctx->fp[reg] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            val_clear(&ctx->fp[reg].val);
            ctx->fp[reg].val = val_make_float(nval);
            DISPATCH();
        }
        do_type: {
            uint8_t dst = *((uint8_t*)ip);
            ip += 1;
            uint8_t src = *((uint8_t*)ip);
            ip += 1;
            printf("| TYPE r0x%02X <- r0x%02X              |\n", dst, src);
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
            uint8_t dst = *((uint8_t*)ip);
            ip += 1;
            uint8_t src_a = *((uint8_t*)ip);
            ip += 1;
            uint8_t src_b = *((uint8_t*)ip);
            ip += 1;
            printf("| LOGICAL_AND r0x%02X <- r0x%02X r0x%02X |\n", dst, src_a, src_b);
            if (&ctx->fp[dst] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (&ctx->fp[src_a] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (&ctx->fp[src_b] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (val_type(ctx->fp[src_a].val) != TYPE_BOOL) {
                // XXX raise
                printf("!! parameter type mismatch\n");
            }
            if (val_type(ctx->fp[src_b].val) != TYPE_BOOL) {
                // XXX raise
                printf("!! parameter type mismatch\n");
            }
            val_clear(&ctx->fp[dst].val);
            ctx->fp[dst].val = val_make_bool(
                   val_get_bool(ctx->fp[src_a].val) 
                && val_get_bool(ctx->fp[src_b].val) 
            );
            DISPATCH();
        }
        do_logical_or: {
            uint8_t dst = *((uint8_t*)ip);
            ip += 1;
            uint8_t src_a = *((uint8_t*)ip);
            ip += 1;
            uint8_t src_b = *((uint8_t*)ip);
            ip += 1;
            printf("| LOGICAL_OR r0x%02X <- r0x%02X r0x%02X  |\n", dst, src_a, src_b);
            if (&ctx->fp[dst] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (&ctx->fp[src_a] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (&ctx->fp[src_b] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (val_type(ctx->fp[src_a].val) != TYPE_BOOL) {
                // XXX raise
                printf("!! parameter type mismatch\n");
            }
            if (val_type(ctx->fp[src_b].val) != TYPE_BOOL) {
                // XXX raise
                printf("!! parameter type mismatch\n");
            }
            val_clear(&ctx->fp[dst].val);
            ctx->fp[dst].val = val_make_bool(
                   val_get_bool(ctx->fp[src_a].val) 
                || val_get_bool(ctx->fp[src_b].val) 
            );
            DISPATCH();
        }
        do_logical_not: {
            uint8_t dst = *((uint8_t*)ip);
            ip += 1;
            uint8_t src = *((uint8_t*)ip);
            ip += 1;
            printf("| LOGICAL_NOT r0x%02X <- r0x%02X       |\n", dst, src);
            if (&ctx->fp[dst] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (&ctx->fp[src] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (val_type(ctx->fp[src].val) != TYPE_BOOL) {
                // XXX raise
                printf("!! parameter type mismatch\n");
            }
            val_clear(&ctx->fp[dst].val);
            ctx->fp[dst].val = val_make_bool(
                ! val_get_bool(ctx->fp[src].val) 
            );
            DISPATCH();
        }
        do_eq: {
            uint8_t dst = *((uint8_t*)ip);
            ip += 1;
            uint8_t src_a = *((uint8_t*)ip);
            ip += 1;
            uint8_t src_b = *((uint8_t*)ip);
            ip += 1;
            printf("| EQ r0x%02X <- r0x%02X r0x%02X          |\n", dst, src_a, src_b);
            if (&ctx->fp[dst] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (&ctx->fp[src_a] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (&ctx->fp[src_b] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            val_clear(&ctx->fp[dst].val);
            bool result = false;
            if (       val_type(ctx->fp[src_a].val) 
                    != val_type(ctx->fp[src_b].val) ) {
                // types differ
                result = false;
            }
            else {
                if (val_type(ctx->fp[src_a].val) == TYPE_NIL) {
                    // nil always compares false, even to itself
                    result = false;
                }
                else if (val_type(ctx->fp[src_a].val) == TYPE_INT) {
                    result = val_get_int(ctx->fp[src_a].val) 
                                == val_get_int(ctx->fp[src_b].val);
                }
                else if (val_type(ctx->fp[src_a].val) == TYPE_BOOL) {
                    result = val_get_bool(ctx->fp[src_a].val) 
                                == val_get_bool(ctx->fp[src_b].val);
                }
                else if (val_type(ctx->fp[src_a].val) == TYPE_STRING) {
                    result = strcmp(val_get_string(ctx->fp[src_a].val), 
                                    val_get_string(ctx->fp[src_b].val)) 
                               == 0;
                }
                // XXX float
            }
            ctx->fp[dst].val = val_make_bool(result);
            DISPATCH();
        }
        do_le: {
            uint8_t dst = *((uint8_t*)ip);
            ip += 1;
            uint8_t src_a = *((uint8_t*)ip);
            ip += 1;
            uint8_t src_b = *((uint8_t*)ip);
            ip += 1;
            printf("| LE r0x%02X <- r0x%02X r0x%02X          |\n", dst, src_a, src_b);
            if (&ctx->fp[dst] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (&ctx->fp[src_a] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (&ctx->fp[src_b] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (       val_type(ctx->fp[src_a].val) 
                    != val_type(ctx->fp[src_b].val) ) {
                printf("!! argument type mismatch\n");
            }
            val_clear(&ctx->fp[dst].val);
            bool result = false;
            if (val_type(ctx->fp[src_a].val) == TYPE_INT) {
                result = val_get_int(ctx->fp[src_a].val) 
                            <= val_get_int(ctx->fp[src_b].val);
            }
            else if (val_type(ctx->fp[src_a].val) == TYPE_STRING) {
                result = strcmp(val_get_string(ctx->fp[src_a].val), 
                                val_get_string(ctx->fp[src_b].val)) 
                           <= 0;
            }
            // XXX float
            else {
                printf("!! argument type mismatch\n");
            }
            ctx->fp[dst].val = val_make_bool(result);
            DISPATCH();
        }
        do_lt: {
            uint8_t dst = *((uint8_t*)ip);
            ip += 1;
            uint8_t src_a = *((uint8_t*)ip);
            ip += 1;
            uint8_t src_b = *((uint8_t*)ip);
            ip += 1;
            printf("| LT r0x%02X <- r0x%02X r0x%02X          |\n", dst, src_a, src_b);
            if (&ctx->fp[dst] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (&ctx->fp[src_a] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (&ctx->fp[src_b] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (       val_type(ctx->fp[src_a].val) 
                    != val_type(ctx->fp[src_b].val) ) {
                printf("!! argument type mismatch\n");
            }
            val_clear(&ctx->fp[dst].val);
            bool result = false;
            if (val_type(ctx->fp[src_a].val) == TYPE_INT) {
                result = val_get_int(ctx->fp[src_a].val) 
                            < val_get_int(ctx->fp[src_b].val);
            }
            else if (val_type(ctx->fp[src_a].val) == TYPE_STRING) {
                result = strcmp(val_get_string(ctx->fp[src_a].val), 
                                val_get_string(ctx->fp[src_b].val)) 
                           < 0;
            }
            // XXX float
            else {
                printf("!! argument type mismatch\n");
            }
            ctx->fp[dst].val = val_make_bool(result);
            DISPATCH();
        }
        do_add: {
            uint8_t dst = *((uint8_t*)ip);
            ip += 1;
            uint8_t src_a = *((uint8_t*)ip);
            ip += 1;
            uint8_t src_b = *((uint8_t*)ip);
            ip += 1;
            printf("| ADD r0x%02X <- r0x%02X r0x%02X         |\n", dst, src_a, src_b);
            if (&ctx->fp[dst] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (&ctx->fp[src_a] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (&ctx->fp[src_b] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (       val_type(ctx->fp[src_a].val) 
                    != val_type(ctx->fp[src_b].val) ) {
                printf("!! parameter type mismatch\n");
            }
            if (val_type(ctx->fp[src_a].val) == TYPE_INT) {
                int result = val_get_int(ctx->fp[src_a].val)
                    + val_get_int(ctx->fp[src_b].val);
                val_clear(&ctx->fp[dst].val);
                ctx->fp[dst].val = val_make_int(result);
            }
            else if (val_type(ctx->fp[src_a].val) == TYPE_FLOAT) {
                float result = val_get_float(ctx->fp[src_a].val)
                    + val_get_float(ctx->fp[src_b].val);
                val_clear(&ctx->fp[dst].val);
                ctx->fp[dst].val = val_make_float(result);
            }
            else {
                printf("!! parameter type mismatch\n");
            }
            DISPATCH();
        }
        do_sub: {
            uint8_t dst = *((uint8_t*)ip);
            ip += 1;
            uint8_t src_a = *((uint8_t*)ip);
            ip += 1;
            uint8_t src_b = *((uint8_t*)ip);
            ip += 1;
            printf("| SUB r0x%02X <- r0x%02X r0x%02X         |\n", dst, src_a, src_b);
            if (&ctx->fp[dst] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (&ctx->fp[src_a] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (&ctx->fp[src_b] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (       val_type(ctx->fp[src_a].val) 
                    != val_type(ctx->fp[src_b].val) ) {
                printf("!! parameter type mismatch\n");
            }
            if (val_type(ctx->fp[src_a].val) == TYPE_INT) {
                int result = val_get_int(ctx->fp[src_a].val)
                    - val_get_int(ctx->fp[src_b].val);
                val_clear(&ctx->fp[dst].val);
                ctx->fp[dst].val = val_make_int(result);
            }
            else if (val_type(ctx->fp[src_a].val) == TYPE_FLOAT) {
                float result = val_get_float(ctx->fp[src_a].val)
                    - val_get_float(ctx->fp[src_b].val);
                val_clear(&ctx->fp[dst].val);
                ctx->fp[dst].val = val_make_float(result);
            }
            else {
                printf("!! parameter type mismatch\n");
            }
            DISPATCH();
        }
        do_mul: {
            uint8_t dst = *((uint8_t*)ip);
            ip += 1;
            uint8_t src_a = *((uint8_t*)ip);
            ip += 1;
            uint8_t src_b = *((uint8_t*)ip);
            ip += 1;
            printf("| MUL r0x%02X <- r0x%02X r0x%02X         |\n", dst, src_a, src_b);
            if (&ctx->fp[dst] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (&ctx->fp[src_a] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (&ctx->fp[src_b] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (       val_type(ctx->fp[src_a].val) 
                    != val_type(ctx->fp[src_b].val) ) {
                printf("!! parameter type mismatch\n");
            }
            if (val_type(ctx->fp[src_a].val) == TYPE_INT) {
                int result = val_get_int(ctx->fp[src_a].val)
                    * val_get_int(ctx->fp[src_b].val);
                val_clear(&ctx->fp[dst].val);
                ctx->fp[dst].val = val_make_int(result);
            }
            if (val_type(ctx->fp[src_a].val) == TYPE_FLOAT) {
                float result = val_get_float(ctx->fp[src_a].val)
                    * val_get_float(ctx->fp[src_b].val);
                val_clear(&ctx->fp[dst].val);
                ctx->fp[dst].val = val_make_float(result);
            }
            else {
                printf("!! parameter type mismatch\n");
            }
            DISPATCH();
        }
        do_div: {
            DISPATCH();
        }
        do_mod: {
            DISPATCH();
        }
        do_jump: {
            int32_t rel_addr = *((int32_t*)ip);
            ip += 4;
            printf("| JUMP %08i                    |\n", rel_addr);
            ip += rel_addr;
            DISPATCH();
        }
        do_jump_if: {
            uint8_t cond = *((uint8_t*)ip);
            ip += 1;
            int32_t rel_addr = *((int32_t*)ip);
            ip += 4;
            printf("| JUMP_IF r0x%02X %08i           |\n", cond, rel_addr);
            if (&ctx->fp[cond] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (       (val_type(ctx->fp[cond].val) == TYPE_BOOL) 
                    && (val_get_bool(ctx->fp[cond].val)) ) {
                ip += rel_addr;
            }
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
        do_syscall: {
            uint8_t nargs = *((uint8_t*)ip);
            ip += 1;
            printf("| SYSCALL %-4i                     |\n", nargs);
            // XXX determine args and system call name
            // XXX call it
            DISPATCH();
        }
    }
}

struct syscall_table* syscall_table_new(void) {
    struct syscall_table *ret = malloc(sizeof(struct syscall_table));
    ret->syscalls = NULL;
    return ret;
}

void syscall_table_free(struct syscall_table *st) {
    // XXX free the entries
    free(st);
}

void syscall_table_add_a0(struct syscall_table *st, char *name, val (*syscall)(void)) {
}

void syscall_table_add_a1(struct syscall_table *st, char *name, val (*syscall)(val v1)) {
}

void syscall_table_add_a2(struct syscall_table *st, char *name, val (*syscall)(val v1, val v2)) {
}

void eval_set_syscall_table(struct eval_ctx *ctx, struct syscall_table *st) {
    ctx->syscall_table = st;
}

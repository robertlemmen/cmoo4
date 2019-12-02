#include "eval.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <assert.h>
// XXX
#include <stdio.h>

#include "types.h"
#include "object.h"
#include "store.h"

#define INITIAL_STACK_SIZE  1024

// XXX this file needs reodering and sections

union stack_element {
    val val;
    union stack_element *se;
    opcode *code;
    struct lobject *obj;
};

struct syscall_entry {
    uint8_t arity;
    char *name;
    union syscall_arity {
        val (*a0)(void *ctx);
        val (*a1)(void *ctx, val v1);
        val (*a2)(void *ctx, val v1, val v2);
        val (*a3)(void *ctx, val v1, val v2, val v3);
    } funcptr;
    struct syscall_entry *next;
};

struct syscall_table {
    struct syscall_entry *syscalls;
    void *ctx;
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
    uint64_t task_id;
    struct store_tx *stx;
    // XXX bit of a kludge, need to find a better way to recurse
    struct lobject *obj;
};

struct eval_ctx* eval_new_ctx(uint64_t task_id, struct store_tx *stx) {
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
    ret->syscall_table = NULL;
    ret->task_id = task_id;
    ret->stx = stx;
    ret->obj = NULL;
    return ret;
}

void eval_set_dbg_handler(struct eval_ctx *ctx, 
        void (*callback)(val v, void *a), 
        void *a) {
    ctx->callback = callback;
    ctx->cb_arg = a;
}

void eval_free_ctx(struct eval_ctx *ctx) {
    // XXX hmm, do we need to clear the active parts of the stack first?
    free(ctx->stack);
    free(ctx);
}

int eval_get_code_recursive(struct lobject *lo, char *name, opcode **code_buf, struct store_tx *stx) {
    // XXX this should really be BFS rather than DFS
    int ret = obj_get_code(lobject_get_object(lo), name, code_buf);
    int idx = 0;
    int pc = obj_get_parent_count(lobject_get_object(lo));
    while ((ret == 0) && (idx < pc)) {
        object_id parent_id = obj_get_parent(lobject_get_object(lo), idx);
        struct lobject *parent = store_get_object(stx, parent_id);
        assert(parent);
        // XXX assert it is non-null, should be
        ret = eval_get_code_recursive(parent, name, code_buf, stx);
        idx++;
    }
    return ret;
}

int eval_exec(struct eval_ctx *ctx, opcode *code) {
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
        &&do_load_string,
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
        &&do_length,
        &&do_concat,
        &&do_getglobal,
        &&do_setglobal,
        &&do_make_obj,
        &&do_self,
        &&do_parent,
        &&do_usleep,
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
            while (ctx->sp != ctx->stack) {
                val_clear(&ctx->sp->val);
                ctx->sp--;
            }
            printf("'----------------------------------'\n");
            return EVAL_OK;
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
            char *val_text = val_print(ctx->fp[msg_r].val);
            printf("| DEBUGR r0x%02X 0x%016lX %s\n", msg_r, ctx->fp[msg_r].val, val_text);
            free(val_text);
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
            char *val_text = val_print(ctx->fp[src].val);
            printf("| PUSH r0x%02X %s\n", src, val_text);
            free(val_text);
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
            printf("| POP r0x%02X                        |\n", dst);
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
            uint8_t nargs = *((uint8_t*)ip);
            ip += 1;
            printf("| CALL %-4i                        |\n", nargs);
            val method_name = ctx->sp[nargs * -1 - 1].val;
            val obj_ref = ctx->sp[nargs * -1 - 2].val;
            // XXX assertions
            // XXX we need to push obj on the stack as well!!
            struct lobject *obj = store_get_object(ctx->stx, val_get_objref(obj_ref));
            assert(obj);
            opcode *ccode;
            int ret = eval_get_code_recursive(obj, val_get_string_data(method_name), &ccode, ctx->stx);
            if (!ret) {
                // XXX raise
                printf("!! method not found\n");
            }
            val_dec_ref(ctx->sp[nargs * -1 - 2].val);
            ctx->sp[nargs * -1 - 2].se = ctx->fp;
            val_dec_ref(ctx->sp[nargs * -1 - 1].val);
            ctx->sp[nargs * -1 - 1].code = ip;
            val_dec_ref(ctx->sp[nargs * -1].val);
            ctx->sp[nargs * -1].obj = ctx->obj;
            ctx->obj = obj;
            ctx->fp = &ctx->sp[nargs * -1 + 1];
            ip = ccode;
            DISPATCH();
        }
        do_return: {
            uint8_t reg = *((uint8_t*)ip);
            ip += 1;
            printf("| RETURN r0x%02X                     |\n", reg);
            val ret = ctx->fp[reg].val; 
            union stack_element *old_fp = ctx->fp;
            ctx->obj = old_fp[-1].obj;
            ip = old_fp[-2].code;
            ctx->fp = old_fp[-3].se;
            old_fp[-3].val = ret;
            val_inc_ref(ret);
            while (ctx->sp > &old_fp[-2]) {
                val_clear(&ctx->sp->val);
                ctx->sp--;
            }
            // this one is outside the loop because it's not actually a val,
            // it's the old frame pointer! and val_clear()ing that is quite
            // unsafe...
            old_fp[-2].val = TYPE_NIL;
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
        do_load_string: {
            uint8_t reg = *((uint8_t*)ip);
            ip += 1;
            uint16_t len = *((uint16_t*)ip);
            ip += 2;
            val s = val_make_string(len, (char*)ip);
            char *val_text = val_print(s);
            printf("| LOAD_STRING r0x%02X <- %2i %s\n", reg, len, val_text);
            free(val_text);
            val_clear(&ctx->fp[reg].val);
            ctx->fp[reg].val = s;
            ip += len;
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
                    int len_a = val_get_string_len(ctx->fp[src_a].val);
                    int len_b = val_get_string_len(ctx->fp[src_b].val);
                    if (len_a == len_b) {
                        result = strncmp(val_get_string_data(ctx->fp[src_a].val), 
                                        val_get_string_data(ctx->fp[src_b].val),
                                        len_a) 
                                   == 0;
                    }
                    else {
                        result = false;
                    }
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
                int len_a = val_get_string_len(ctx->fp[src_a].val);
                int len_b = val_get_string_len(ctx->fp[src_b].val);
                int min_len = (len_a < len_b) ? len_a : len_b;
                int cmp = strncmp(val_get_string_data(ctx->fp[src_a].val), 
                                  val_get_string_data(ctx->fp[src_b].val),
                                  min_len);
                if (cmp == 0) {
                    result = len_a <= len_b;
                }
                result = cmp <= 0;
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
            val_clear(&ctx->fp[dst].val); // XXX rubbish! dst can be a source as well, so we cannot clear until we have a result!
            bool result = false;
            if (val_type(ctx->fp[src_a].val) == TYPE_INT) {
                result = val_get_int(ctx->fp[src_a].val) 
                            < val_get_int(ctx->fp[src_b].val);
            }
            else if (val_type(ctx->fp[src_a].val) == TYPE_STRING) {
                int len_a = val_get_string_len(ctx->fp[src_a].val);
                int len_b = val_get_string_len(ctx->fp[src_b].val);
                int min_len = (len_a < len_b) ? len_a : len_b;
                int cmp = strncmp(val_get_string_data(ctx->fp[src_a].val), 
                                  val_get_string_data(ctx->fp[src_b].val),
                                  min_len); 
                if (cmp == 0) {
                    result = len_a < len_b;
                }
                result = cmp < 0;
            }
            // XXX float
            else {
                printf("!! argument type mismatch\n");
            }
            printf("=> %s\n", result ? "true" : "false");
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
            val syscall_name = ctx->sp[nargs * -1].val;
            if (val_type(syscall_name) == TYPE_STRING) {
                char *buf = malloc(val_get_string_len(syscall_name) + 1);
                memcpy(buf, val_get_string_data(syscall_name), val_get_string_len(syscall_name));
                buf[val_get_string_len(syscall_name)] = '\0';
                struct syscall_entry *se = NULL;
                struct syscall_entry *cse = ctx->syscall_table->syscalls;
                while (cse) {
                    if ( (cse->arity == nargs) && (!strcmp(buf, cse->name)) ) {
                        se = cse;
                        break;
                    }
                    cse = cse->next;
                }
                if (se) {
                    // XXX syscalls should leave their result on the stack
                    if (nargs == 0) {
                        se->funcptr.a0(ctx->syscall_table->ctx);
                    }
                    else if (nargs == 1) {
                        se->funcptr.a1(ctx->syscall_table->ctx, ctx->sp[0].val);
                    }
                    else if (nargs == 2) {
                        se->funcptr.a2(ctx->syscall_table->ctx, ctx->sp[-1].val, ctx->sp[-0].val);
                    }
                    else if (nargs == 3) {
                        se->funcptr.a3(ctx->syscall_table->ctx, ctx->sp[-2].val, ctx->sp[-1].val, ctx->sp[-0].val);
                    }
                    else {
                        // XXX raise
                        printf("!! unsupported arity in syscall\n");
                    }
                }
                else {
                    // XXX raise
                    printf("!! syscall '%s' not found\n", buf);
                }
                free(buf);
                for (int i = 0; i <= nargs; i++) {
                    val_clear(&ctx->sp->val);
                    ctx->sp--;
                }
            }
            else {
                // XXX raise
                printf("!! parameter type mismatch %i\n", val_type(syscall_name));
            }
            // XXX determine system call name and args from stack
            // XXX call it
            DISPATCH();
        }
        do_length: {
            uint8_t dst = *((uint8_t*)ip);
            ip += 1;
            uint8_t src = *((uint8_t*)ip);
            ip += 1;
            printf("| LENGTH r0x%02X <- r0x%02X            |\n", dst, src);
            if (&ctx->fp[dst] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (&ctx->fp[src] > ctx->sp) {
                // XXX raise
                printf("!! access to reg outside stack\n");
            }
            if (val_type(ctx->fp[src].val) == TYPE_STRING) {
                val_clear(&ctx->fp[dst].val);
                int result = val_get_string_len(ctx->fp[src].val);
                printf("=> %i\n", result);
                ctx->fp[dst].val = val_make_int(result);
            }
            else {
                // XXX raise
                printf("!! parameter type mismatch\n");
            }
            DISPATCH();
        }
        do_concat: {
            uint8_t dst = *((uint8_t*)ip);
            ip += 1;
            uint8_t src_a = *((uint8_t*)ip);
            ip += 1;
            uint8_t src_b = *((uint8_t*)ip);
            ip += 1;
            printf("| CONCAT r0x%02X <- r0x%02X r0x%02X      |\n", dst, src_a, src_b);
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
            if (   (val_type(ctx->fp[src_a].val) == TYPE_STRING) 
                && (val_type(ctx->fp[src_b].val) == TYPE_STRING) ) {
                val_clear(&ctx->fp[dst].val);
                int len_a = val_get_string_len(ctx->fp[src_a].val);
                int len_b = val_get_string_len(ctx->fp[src_b].val);
                char *buf = malloc(len_a + len_b);
                memcpy(&buf[0], val_get_string_data(ctx->fp[src_a].val), len_a);
                memcpy(&buf[len_a], val_get_string_data(ctx->fp[src_b].val), len_b);
                ctx->fp[dst].val = val_make_string(len_a + len_b, buf);
                free(buf);
            }
            else {
                // XXX raise
                printf("!! parameter type mismatch\n");
            }
            DISPATCH();
        }
        do_getglobal: {
            uint8_t dst = *((uint8_t*)ip);
            ip += 1;
            uint8_t name = *((uint8_t*)ip);
            ip += 1;
            printf("| GETGLOBAL r0x%02X r0x%02X            |\n", dst, name);
            val tval = obj_get_global(lobject_get_object(ctx->obj), val_get_string_data(ctx->fp[name].val));
            val_clear(&ctx->fp[dst].val);
            ctx->fp[dst].val = tval;
            DISPATCH();
        }
        do_setglobal: {
            uint8_t name = *((uint8_t*)ip);
            ip += 1;
            uint8_t rval = *((uint8_t*)ip);
            ip += 1;
            printf("| SETGLOBAL r0x%02X r0x%02X            |\n", name, rval);
            printf("### locking %li EXCLUSIVE\n", obj_get_id(lobject_get_object(ctx->obj)));
            if (lock_lock(lobject_get_lock(ctx->obj), LOCK_EXCLUSIVE, ctx->stx)) {
                printf("!!!! lock failed\n");
                return EVAL_RETRY_TX;
            }
            obj_set_global(lobject_get_object(ctx->obj), val_get_string_data(ctx->fp[name].val), ctx->fp[rval].val);
            DISPATCH();
        }
        do_make_obj: {
            uint8_t new_ref = *((uint8_t*)ip);
            ip += 1;
            uint8_t parent_ref = *((uint8_t*)ip);
            ip += 1;
            printf("| MAKE_OBJ r0x%02X r0x%02X             |\n", new_ref, parent_ref);
            // XXX assert parent_ref is an objref and that both are in range
            struct lobject *obj = store_make_object(ctx->stx, val_get_objref(ctx->fp[parent_ref].val));
            val_clear(&ctx->fp[new_ref].val);
            ctx->fp[new_ref].val = val_make_objref(obj_get_id(lobject_get_object(obj)));
            DISPATCH();
        }
        do_self: {
            uint8_t dst = *((uint8_t*)ip);
            ip += 1;
            printf("| SELF r0x%02X                       |\n", dst);
            // XXX check in range
            val_clear(&ctx->fp[dst].val);
            ctx->fp[dst].val = val_make_objref(obj_get_id(lobject_get_object(ctx->obj)));
            // XXX
            DISPATCH();
        }
        do_parent: {
            uint8_t dst = *((uint8_t*)ip);
            ip += 1;
            printf("| PARENT r0x%02X                     |\n", dst);
            // XXX check in range
            val_clear(&ctx->fp[dst].val);
            if (obj_get_parent_count(lobject_get_object(ctx->obj)) > 0) {
                ctx->fp[dst].val = val_make_objref(obj_get_parent(lobject_get_object(ctx->obj), 0));
            }
            DISPATCH();
        }
        do_usleep: {
            int32_t interval_us = *((int32_t*)ip);
            ip += 4;
            printf("| USLEEP %010i                |\n", interval_us);
            usleep(interval_us);
            DISPATCH();
        }
    }
    // XXX we can never get here...
    assert(false);
    return EVAL_OK;
}

int eval_exec_method(struct eval_ctx *ctx, struct lobject *obj, val method, int num_args, ...) {
    // XXX find method on object, load code, push args and exec
    // XXX ...for now, this isn't quite right around globals and an object stack

    va_list argp;
    va_start(argp, num_args);
    while (num_args--) {
        eval_push_arg(ctx, va_arg(argp, val));
    }

    opcode *code;
    int ret = eval_get_code_recursive(obj, val_get_string_data(method), &code, ctx->stx);
    if (ret) {
        ctx->obj = obj;
        return eval_exec(ctx, code);
    }
    else {
        printf("!! method '%s' not found on object %li\n", val_get_string_data(method),
            obj_get_id(lobject_get_object(obj)));
        return 2; // XXX actually the unrecoverable error
    }
}

void eval_push_arg(struct eval_ctx *ctx, val v) {
    val_inc_ref(v);
    ctx->sp++;
    ctx->sp->val = v;
}

struct syscall_table* syscall_table_new(void) {
    struct syscall_table *ret = malloc(sizeof(struct syscall_table));
    ret->syscalls = NULL;
    ret->ctx = NULL;
    return ret;
}

struct syscall_table* eval_get_syscall_table(struct eval_ctx *ctx) {
    return ctx->syscall_table;
}

void syscall_table_set_ctx(struct syscall_table *st, void *ctx) {
    st->ctx = ctx;
}

void syscall_table_free(struct syscall_table *st) {
    while (st->syscalls) {
        struct syscall_entry *se = st->syscalls;
        st->syscalls = se->next;
        free(se->name);
        free(se);
    }
    free(st);
}

void syscall_table_add_a0(struct syscall_table *st, char *name, val (*syscall)(void*)) {
    struct syscall_entry *se = malloc(sizeof(struct syscall_entry));
    se->arity = 0;
    se->name = malloc(strlen(name) + 1);
    strcpy(se->name, name);
    se->funcptr.a0 = syscall;
    se->next = st->syscalls;
    st->syscalls = se;
}

void syscall_table_add_a1(struct syscall_table *st, char *name, val (*syscall)(void*, val v1)) {
    struct syscall_entry *se = malloc(sizeof(struct syscall_entry));
    se->arity = 1;
    se->name = malloc(strlen(name) + 1);
    strcpy(se->name, name);
    se->funcptr.a1 = syscall;
    se->next = st->syscalls;
    st->syscalls = se;
}

void syscall_table_add_a2(struct syscall_table *st, char *name, val (*syscall)(void*, val v1, val v2)) {
    struct syscall_entry *se = malloc(sizeof(struct syscall_entry));
    se->arity = 2;
    se->name = malloc(strlen(name) + 1);
    strcpy(se->name, name);
    se->funcptr.a2 = syscall;
    se->next = st->syscalls;
    st->syscalls = se;
}

void syscall_table_add_a3(struct syscall_table *st, char *name, val (*syscall)(void*, val v1, val v2, val v3)) {
    struct syscall_entry *se = malloc(sizeof(struct syscall_entry));
    se->arity = 3;
    se->name = malloc(strlen(name) + 1);
    strcpy(se->name, name);
    se->funcptr.a3 = syscall;
    se->next = st->syscalls;
    st->syscalls = se;
}

void eval_set_syscall_table(struct eval_ctx *ctx, struct syscall_table *st) {
    ctx->syscall_table = st;
}

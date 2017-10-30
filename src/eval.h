#ifndef EVAL_H
#define EVAL_H

#include "defs.h"
#include "types.h"
#include "object.h"

/* the virtual machine in CMOO uses a stack where elements can be accessed 
 * indexed as well. to do this we have a stack pointer SP, a frame pointer FP
 * and an instruction pointer IP. The instructions are 1-octet in length but 
 * may be followed by variable length arguments. The VM executes the current 
 * opcode at IP and then increases IP accordingly. The instructions can 
 * modify SP (e.g. with PUSH, POP) or access cells relative to FP. a sample 
 * layout within a method call:
 *
 * | intermediate0  | <- SP
 * | local0         |
 * | argument1      |
 * | argument0      | <- FP
 * +----------------+
 * | return address 
 * | previous FP    |
 * | ...            |
 
 * In this case the current method has been called with two arguments, and has
 * reserved one local variable (at FP[2]). the intermediate value has been 
 * created with e.g. PUSH. Note that the arguments, locals and intermediate 
 * cells hold *values*, the return address and previous FP hold VM-internal 
 * addresses.
 *
 * When calling a method, the steck needs to have the arguments at the top, 
 * followed by a string naming the method and an objref naming the object to 
 * call on:
 *
 * | argument0      | <- SP
 * | method name    |
 * | callee objref  |
 * | intermediate0  | 
 * | local0         |
 * | argument1      |
 * | argument0      | <- FP
 * +----------------+
 * | return address | 
 * | previous FP    |
 * | ...            |
 * 
 * the code then executes a "CALL nargs" where "nargs" is the number of 
 * arguments on the stack. The CALL opcode will overwrite the method name and 
 * objref before executing the callee, so the Stack, SP and FP in the called 
 * method look like this:
 *
 * | argument0      | <- SP, FP
 * +----------------+
 * | return address |
 * | previous FP    | --,
 * | intermediate0  |   |
 * | local0         |   |
 * | argument1      |   |
 * | argument0      | <-'
 * | return address |
 * | previous FP    |
 * | ...            |
 *
 * At some point the callee will call a "RETURN" instruction (of which there 
 * are several variants), which will restore the previous FP, put the 
 * returned value in the slot that contained it, and set SP to that as well:
 *
 * | returned value | <- SP
 * | intermediate0  | 
 * | local0         |
 * | argument1      |
 * | argument0      | <- FP
 * +----------------+
 * | return address | 
 * | previous FP    |
 * | ...            |
 * 
 * Note that you can use intermediate (PUSHed) values for the callee 
 * declaration and the arguments, or local variables. But if you do use local
 * variables then of course their contents will be undefined after the return.
 *
 * Also note that if you store your callee objref, method name and arguments
 * in locals, then after the callee returned these are the last members on the
 * stack, and you may have shrunk your args_locals guarantees, it is therefore
 * recommended to use the last locals on your stack for this.
 *
 * Also note that in a method with not arguments and locals, FP is initially 
 * beyond SP!
 *
 * A full description of the instruction set is in the documentation, the 
 * below is only a brief reference. things like reg8:src mean type:name, reg8 
 * is an 8-bit value denoting a cell relative to FP, src is what we read in 
 * the instruction.
 * */

#define OP_NOOP              0 // no-op
#define OP_HALT              1 // halt code execution
#define OP_DEBUGI            2 // int32:msg      
#define OP_DEBUGR            3 // reg8:msg
#define OP_MOV               4 // reg8:dst <= reg8:src
#define OP_PUSH              5 // SP <= reg8:src; SP++
#define OP_POP               6 // reg8:dst <= SP; SP--
#define OP_CALL              7 // int8:nargs
#define OP_RETURN            8 // reg8:value
#define OP_ARGS_LOCALS       9 // assert we have int8:nargs and 
                               // reserve int8:nlocals on the stack
#define OP_CLEAR            10 // reg8:dst <= NIL
#define OP_TRUE             11 // reg8:dst <= BOOL(TRUE)
#define OP_LOAD_INT         12 // reg8:dst <= int32:value
#define OP_LOAD_FLOAT       13 // reg8:dst <= float32:value
#define OP_LOAD_STRING      14 // reg8:dst <= int16 length, string:value
#define OP_TYPE             15 // reg8:dst <= type(reg8:src)
#define OP_LOGICAL_AND      16 // reg8:dst <= logical_and(reg8:src1, reg8:src2)
#define OP_LOGICAL_OR       17 // reg8:dst <= logical_or(reg8:src1, reg8:src2)
#define OP_LOGICAL_NOT      18 // reg8:dst <= local_not(reg8:src)
#define OP_EQ               19 // reg8:dst <= eq(reg8:src1, reg8:src2)
#define OP_LE               20 // reg8:dst <= le(reg8:src1, reg8:src2)
#define OP_LT               21 // reg8:dst <= lt(reg8:src1, reg8:src2)
#define OP_ADD              22 // reg8:dst <= reg8:src1 + reg8:src2
#define OP_SUB              23 // reg8:dst <= reg8:src1 - reg8:src2
#define OP_MUL              24 // reg8:dst <= reg8:src1 * reg8:src2
#define OP_DIV              25 // reg8:dst <= reg8:src1 / reg8:src2
#define OP_MOD              26 // reg8:dst <= reg8:src1 % reg8:src2
#define OP_JUMP             27 // IP += int32:offset
#define OP_JUMP_IF          28 // if reg8:src is TRUE then IP += int32:offset
#define OP_JUMP_EQ          29 // if eq(reg8:src1, reg8:src2) then IP += int32:offset
#define OP_JUMP_NE          30 // if ne(reg8:src1, reg8:src2) then IP += int32:offset
#define OP_JUMP_LE          31 // if le(reg8:src1, reg8:src2 then IP += int32:offset
#define OP_JUMP_LT          32 // if lt(reg8:src1, reg8:src2) then IP += int32:offset

// XXX need to be reordered
#define OP_SYSCALL          33 // int8:nargs
#define OP_LENGTH           34 // reg8:dst <= strlen(reg8:src)
#define OP_CONCAT           35 // reg8:dst <= concat(reg8:src1, reg8:src2)

// XXX more ops

struct eval_ctx;
struct exec_ctx;

struct syscall_table;

struct eval_ctx* eval_new_ctx(void);
void eval_free_ctx(struct eval_ctx *ctx);

// XXX two functions to actually run code in the vm: the first one is quick and creates 
// an execution context from an eval_ctx and a objref. it "just" gets a locked version 
// of the obj in question to make sure calls to evaluate are serialized properly. The 
// second takes the exec_ctx and some arguments, and executes the code in question. this 
// may take some time, cause I/O, lock, deadlock and what not...
struct exec_ctx* eval_make_exec_ctx(struct eval_ctx *ctx, object_id oid);
// the variadic part is num_args items of type "val", which get passed to the function
// denoted by "method" on the object retrieved by "objref" on eval_make_exec_ctx.
// This consumes the exec_ctx
// XXX return value to signal success/error
void exec_run(struct exec_ctx *ectx, val method, int num_args, ...);

// set a callback that gets executed whenever OP_DEBUGI or OP_DEBUGR gets
// executed, this mainly for testing. whenever the callback gets executed, the
// argument a gets passed to it as well
void eval_set_dbg_handler(struct eval_ctx *ctx, 
    void (*callback)(val v, void *a), 
    void *a);

// debug clutch to allow direct execution of code without objects and the like.
// This is only used by test drivers and debug code, not by an actual CMOO server
void eval_exec(struct eval_ctx *ctx, opcode *code);

// XXX we will need some context object to pass around, perhaps store in the sycall table and
// pass a syscall table ptr to each callback?
struct syscall_table* syscall_table_new(void);
void syscall_table_free(struct syscall_table *st);
// set a context object that is being passed into each sycall invocation
void syscall_table_set_ctx(struct syscall_table *st, void *ctx);

void syscall_table_add_a0(struct syscall_table *st, char *name, val (*syscall)(void*));
void syscall_table_add_a1(struct syscall_table *st, char *name, val (*syscall)(void*, val v1));
void syscall_table_add_a2(struct syscall_table *st, char *name, val (*syscall)(void*, val v1, val v2));

void eval_set_syscall_table(struct eval_ctx *ctx, struct syscall_table *st);

#endif /* EVAL_H */

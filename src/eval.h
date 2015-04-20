#ifndef EVAL_H
#define EVAL_H

#include <stdint.h>

/* the virtual machine in CMOO uses a stack where elements can be accessed 
 * indexed as well. to do this we have a stack pointer SP, a frame pointer FB 
 * and an instruction pointer IP. The instructions are 1-octet in length but 
 * may be followed by variable length arguments. The VM executes the current 
 * opcode at IP and then increases IP accordingly. The instructions can 
 * modify SP (e.g. with PUSH, POP) or access cells relative to FP. a sample 
 * layout within a method call:
 *
 * | intermediate0  | <- SP
 * | local0         |
 * | argument1      |
 * | argument0      | 
 * +----------------+
 * | return address | <- FP
 * | previous FP    |
 * | ...            |
 *
 * In this case the current method has been called with two arguments, and has
 * reserved one local variable (at FP[2]). the intermediate value has been 
 * created with e.g. PUSH. Note that the arguments, locals and intermediate cells
 * hold *values*, the return address and previous FP hold VM-internal addresses.
 *
 * When calling a method, the steck needs to have the arguments at the top, 
 * followed by a string naming the method and an objref naming the object to call 
 * on:
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
 * Also note that in a method with not arguments and locals, FP is initially 
 * beyond SP!
 *
 * A full description of the instruction set is in the documentation, the 
 * below is only a brief reference. things like reg8:src mean type:name, reg8 
 * is an 8-bit value denoting a cell relative to FP, src is what we read in 
 * the instruction.
 * */

#define OP_NOOP              0  // no-op
#define OP_ABORT             1  // abort
#define OP_DEBUGI            2  // int32:msg      
#define OP_DEBUGR            3  // reg8:msg
#define OP_MOV               4  // reg8:dst <- reg8:src
#define OP_PUSH              5  // SP++ <- reg8:src
#define OP_POP               6  // reg8:dst <- SP--
#define OP_CALL              7  // int8:nargs
#define OP_RETURN            8  // reg8:value
#define OP_ARGS_LOCALS       9  // assert we have int8:nargs and 
                                // reserve int8:nlocals on the stack
#define OP_CLEAR            10  // reg8:dst <= NIL
#define OP_TRUE             11  // reg8:dst <= BOOL(TRUE)
#define OP_LOAD_INT         12  // reg8:dst <- int32:value
#define OP_LOAD_FLOAT       13  // reg8:dst <- float32:value
#define OP_TYPE             14  // reg8:dst <- type(reg8:src)
#define OP_LOGICAL_AND      15  // reg8:dst <- logical_and(reg8:src1, reg8:src2)
#define OP_LOGICAL_OR       16  // reg8:dst <- logical_or(reg8:src1, reg8:src2)
#define OP_EQ               17  // reg8:dst <- eq(reg8:src1, reg8:src2)
#define OP_LQ               18  // reg8:dst <- lq(reg8:src1, reg8:src2)
#define OP_LT               19  // reg8:dst <- lt(reg8:src1, reg8:src2)
#define OP_ADD              20  // reg8:dst <- reg8:src1 + reg8:src2
#define OP_SUB              21  // reg8:dst <- reg8:src1 - reg8:src2
#define OP_MUL              22  // reg8:dst <- reg8:src1 * reg8:src2
#define OP_DIV              23  // reg8:dst <- reg8:src1 / reg8:src2
#define OP_MOD              24  // reg8:dst <- reg8:src1 % reg8:src2
#define OP_JUMP             25  // IP += int32:offset
#define OP_JUMP_IF          26  // IP += int32:offset if reg8:src is TRUE
#define OP_JUMP_EQ          27  // IP += int32:offset if eq(reg8:src1, reg8:src2)
#define OP_JUMP_NE          28  // IP += int32:offset if ne(reg8:src1, reg8:src2)
#define OP_JUMP_LE          29  // IP += int32:offset if le(reg8:src1, reg8:src2)
#define OP_JUMP_LT          30  // IP += int32:offset if lt(reg8:src1, reg8:src2)
// XXX more ops

typedef uint8_t opcode;

struct eval_ctx;

struct eval_ctx* eval_new_ctx(void);
void eval_free_ctx(struct eval_ctx *ctx);

// XXX temporary clutch to allow direct execution of code without objects and the like
// it may be a good idea to allow direct calls anyway! then each slot could have
// sub-methods...
void eval(struct eval_ctx *ctx, opcode *code);

#endif /* EVAL_H */

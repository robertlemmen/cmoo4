#ifndef VM_H
#define VM_H

#include "defs.h"

/* the VM interface has two steps: you first get an evaluation context by providing
 * a object_id of an object that you want to call. This call returns without blocking. you 
 * then use that vm_eval_ctx object to make your actual call, which may well block if the object 
 * (or any object in the resulting call chain) is locked. The reason for this setup is that this
 * allows tha network/task subsystems to sequence calls and therefore guarantee the order of 
 * execution of calls against the same object (socket). */

struct vm;
struct vm_eval_ctx;
struct store;
struct tasks_ctx;

struct vm* vm_new(struct store *s);
void vm_free(struct vm *v);

void vm_init(struct vm *v, struct tasks_ctx *tc);

struct vm_eval_ctx* vm_get_eval_ctx(struct vm *v, object_id id, uint64_t task_id);
// returns the underlying error from eval.h
int vm_eval_ctx_exec(struct vm_eval_ctx *ex, val method, int num_args, ...);
void vm_free_eval_ctx(struct vm_eval_ctx *ex);

#endif /* VM_H */

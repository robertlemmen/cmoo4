#ifndef VM_H
#define VM_H

#include "defs.h"

/* the VM interface has two steps: you first get an evaluation context by providing
 * a object_id of an object that you want to call. This call returns without blocking. you 
 * then use that eval_ctx object to make your actual call, which may well block if the object 
 * (or any object in the resulting call chain) is locked. The reason for this setup is that this
 * allows tha network/task subsystems to sequence calls and therefore guarantee the order of 
 * execution of calls against the same object (socket). */

struct vm;
struct eval_ctx;

struct vm* vm_new(void);
void vm_free(struct vm *v);

// XXX needs task id or so to make sure older younger threads get faulted 
// out of deadlocks
// XXX vm also needs link to net/ntx
struct eval_ctx* vm_get_eval_ctx(struct vm *v, object_id id);
// XXX missing arguments for slot to call and arguments
void vm_eval_ctx_exec(struct eval_ctx *ex);

#endif /* VM_H */

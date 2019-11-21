#include "vm.h"

#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>

// XXX
#include <stdio.h>
#include <string.h>

#include "tasks.h"
#include "types.h"
#include "store.h"
#include "lobject.h"
#include "eval.h"

// -------- implementation of declared public structures --------

struct vm {
    struct store *store;
    struct syscall_table *syscalls;
};

struct vm_eval_ctx {
    struct vm *v;
    struct store_tx *stx;
    struct eval_ctx *eval_ctx;
    // XXX for now, we might want to store this on the stack instead
    struct lobject *start_obj;
    uint64_t task_id;
};

// -------- internal functions ------------

val syscall_net_make_listener(void *ctx, val port, val oid) {
    struct tasks_ctx *tasks_ctx = (struct tasks_ctx*)ctx;
    tasks_net_make_listener(tasks_ctx, val_get_int(port), val_get_objref(oid));
    return val_make_nil();
}

val syscall_net_shutdown_listener(void *ctx, val port) {
    struct tasks_ctx *tasks_ctx = (struct tasks_ctx*)ctx;
    tasks_net_shutdown_listener(tasks_ctx, val_get_int(port));
    return val_make_nil();
}

val syscall_net_accept_socket(void *ctx, val socket, val oid) {
    struct tasks_ctx *tasks_ctx = (struct tasks_ctx*)ctx;
    tasks_net_accept_socket(tasks_ctx, val_get_special(socket), val_get_objref(oid));
    return val_make_nil();
}

val syscall_net_socket_free(void *ctx, val socket) {
    struct tasks_ctx *tasks_ctx = (struct tasks_ctx*)ctx;
    tasks_net_socket_free(tasks_ctx, val_get_special(socket));
    return val_make_nil();
}

val syscall_net_socket_write(void *ctx, val socket, val tx, val data) {
    struct tasks_ctx *tasks_ctx = (struct tasks_ctx*)ctx;
    tasks_net_socket_write(tasks_ctx, val_get_special(socket), val_get_special(tx), val_get_string_data(data), val_get_string_len(data));
    return val_make_nil();
}

// -------- implementation of public functions --------

struct vm* vm_new(struct store *s) {
    struct vm *ret = malloc(sizeof(struct vm));
    ret->store = s;
    ret->syscalls = syscall_table_new();
    syscall_table_add_a2(ret->syscalls, "net_make_listener", &syscall_net_make_listener);
    syscall_table_add_a1(ret->syscalls, "net_shutdown_listener", &syscall_net_shutdown_listener);
    syscall_table_add_a2(ret->syscalls, "net_accept_socket", &syscall_net_accept_socket);
    syscall_table_add_a1(ret->syscalls, "net_socket_free", &syscall_net_socket_free);
    syscall_table_add_a3(ret->syscalls, "net_socket_write", &syscall_net_socket_write);
    return ret;
}

void vm_free(struct vm *v) {
    syscall_table_free(v->syscalls);
    free(v);
}

void vm_init(struct vm *v, struct tasks_ctx *tc) {
    printf("# vm_init\n");
    syscall_table_set_ctx(v->syscalls, tc);
    struct vm_eval_ctx *ec = vm_get_eval_ctx(v, 0, 0);
    // XXX the args are stubby, just needed until we have a more complete core with globals and
    // object creation
    val method_name = val_make_string(4, "init");
    vm_eval_ctx_exec(ec, method_name, 1, val_make_objref(11));
    vm_free_eval_ctx(ec);
    val_dec_ref(method_name);
}

struct vm_eval_ctx* vm_get_eval_ctx(struct vm *v, object_id id, uint64_t task_id) {
    struct vm_eval_ctx *ret = malloc(sizeof(struct vm_eval_ctx));
    ret->v = v;
    ret->task_id = task_id;
    ret->stx = store_start_tx(v->store);
    ret->start_obj = store_get_object(ret->stx, id);
    assert(ret->start_obj);
    printf("# vm_get_eval_ctx %li -> %p\n", id, ret);

    ret->eval_ctx = eval_new_ctx(task_id, ret->stx);
    eval_set_syscall_table(ret->eval_ctx, v->syscalls);

    // XXX more, need lock implementation

    return ret;
}

void vm_free_eval_ctx(struct vm_eval_ctx *ex) {
    eval_free_ctx(ex->eval_ctx);
    free(ex);
}

void vm_eval_ctx_exec(struct vm_eval_ctx *ex, val method, int num_args, ...) {
    va_list argp;
    va_start(argp, num_args);

    object_id oid = obj_get_id(lobject_get_object(ex->start_obj));

    printf("# vm_eval_ctx_exec %p oid=%li slot=%s num_args=%i\n", ex,
        oid, val_get_string_data(method), num_args);

    // argh... http://c-faq.com/varargs/handoff.html
    if (num_args == 0) {
        eval_exec_method(ex->eval_ctx, ex->start_obj, method, 0);
    }
    else if (num_args == 1) {
        val arg0 = va_arg(argp, val);
        char *arg_text = val_print(arg0);
        printf("    %s\n", arg_text);
        free(arg_text);
        eval_exec_method(ex->eval_ctx, ex->start_obj, method, 1, arg0);
    }
    else if (num_args == 2) {
        val arg0 = va_arg(argp, val);
        val arg1 = va_arg(argp, val);
        char *arg_text = val_print(arg0);
        printf("    %s\n", arg_text);
        free(arg_text);
        arg_text = val_print(arg1);
        printf("    %s\n", arg_text);
        free(arg_text);
        eval_exec_method(ex->eval_ctx, ex->start_obj, method, 2, arg0, arg1);
    }
    else {
        printf("#  unsupported number of args in call to VM!\n");
    }

    va_end(argp);
    store_finish_tx(ex->stx);
}

#include "vm.h"

#include <stdlib.h>
#include <stdarg.h>

// XXX
#include <stdio.h>
#include <string.h>

#include "tasks.h"
#include "types.h"
#include "store.h"
#include "object.h"
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
    // XXX for now, needs actual object reference instead
    struct object *start_obj;
    uint64_t task_id;
};

// -------- internal functions ------------

void syscall_net_make_listener(void *ctx, val port, val oid) {
    struct tasks_ctx *tasks_ctx = (struct tasks_ctx*)ctx;
    tasks_net_make_listener(tasks_ctx, val_get_int(port), val_get_objref(oid));
}

void syscall_net_shutdown_listener(void *ctx, val port) {
    struct tasks_ctx *tasks_ctx = (struct tasks_ctx*)ctx;
    tasks_net_shutdown_listener(tasks_ctx, val_get_int(port));
}

void syscall_net_accept_socket(void *ctx, val socket, val oid) {
    struct tasks_ctx *tasks_ctx = (struct tasks_ctx*)ctx;
    tasks_net_accept_socket(tasks_ctx, val_get_special(socket), val_get_objref(oid));
}

void syscall_net_socket_free(void *ctx, val socket) {
    struct tasks_ctx *tasks_ctx = (struct tasks_ctx*)ctx;
    tasks_net_socket_free(tasks_ctx, val_get_special(socket));
}

void syscall_net_socket_write(void *ctx, val socket, val tx, val data) {
    struct tasks_ctx *tasks_ctx = (struct tasks_ctx*)ctx;
    tasks_net_socket_write(tasks_ctx, val_get_special(socket), val_get_special(tx), val_get_string_data(data), val_get_string_len(data));
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

struct vm_eval_ctx* vm_get_eval_ctx(struct vm *v, object_id id, uint64_t task_id) {
    struct vm_eval_ctx *ret = malloc(sizeof(struct vm_eval_ctx));
    ret->v = v;
    ret->task_id = task_id;
    ret->stx = store_start_tx(v->store);
    ret->start_obj = store_get_object(ret->stx, id);
    printf("# vm_get_eval_ctx %li -> %p\n", id, ret);

    ret->eval_ctx = eval_new_ctx();
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

    static struct tasks_ctx *tasks_ctx;  // perhaps not needed anymore?
    static long int socket_oid_seq = 111;

    static val sockets[100];

    object_id oid = obj_get_id(ex->start_obj);

    printf("# vm_eval_ctx_exec %p oid=%li slot=%s num_args=%i\n", ex,
        oid, val_get_string_data(method), num_args);

    val arg0;
    if (num_args >= 1) {
        arg0 = va_arg(argp, val);
    }
    val arg1;
    if (num_args >= 2) {
        arg1 = va_arg(argp, val);
    }

    // XXX pseudo-core
    if ((oid == 0) && (strncmp(val_get_string_data(method), "init", 4) == 0)
            && (num_args == 1) && (val_type(arg0) == TYPE_SPECIAL)) {
        printf("#   0::init\n");
        tasks_ctx = val_get_special(arg0);
        syscall_table_set_ctx(ex->v->syscalls, tasks_ctx);

        opcode code[] = {
                            OP_ARGS_LOCALS, 0x01, 0x02,
                            OP_LOAD_STRING, 0x01, 0x11, 0x00, 'n', 'e', 't', '_', 'm', 'a', 'k', 'e', '_', 'l', 'i', 's', 't', 'e', 'n', 'e', 'r',
                            OP_LOAD_INT, 0x02, 0x39, 0x30, 0x00, 0x00,
                            OP_PUSH, 0x01,
                            OP_PUSH, 0x02,
                            OP_PUSH, 0x00,
                            OP_SYSCALL, 0x02,
                            OP_HALT};

        eval_push_arg(ex->eval_ctx, val_make_objref(11));
        eval_exec(ex->eval_ctx, code);

    }
    else if ((oid == 0) && (strncmp(val_get_string_data(method), "shutdown", 8) == 0)
            && (num_args == 0)) {
        printf("#   0::shutdown\n");

        opcode code[] = {
                            OP_ARGS_LOCALS, 0x00, 0x03,
                            OP_LOAD_STRING, 0x00, 0x15, 0x00, 'n', 'e', 't', '_', 's', 'h', 'u', 't', 'd', 'o', 'w', 'n', '_', 'l', 'i', 's', 't', 'e', 'n', 'e', 'r',
                            OP_LOAD_INT, 0x01, 0x39, 0x30, 0x00, 0x00,
                            OP_PUSH, 0x00,
                            OP_PUSH, 0x01,
                            OP_SYSCALL, 0x01,
                            OP_HALT};

        eval_exec(ex->eval_ctx, code);
    }
    else if ((oid == 11) && (strncmp(val_get_string_data(method), "accept", 6) == 0)
            && (num_args == 1) && (val_type(arg0) == TYPE_SPECIAL)) {
        printf("#   11::accept\n");
        sockets[socket_oid_seq] = arg0;

        opcode code[] = {
                            OP_ARGS_LOCALS, 0x02, 0x01,
                            OP_LOAD_STRING, 0x02, 0x11, 0x00, 'n', 'e', 't', '_', 'a', 'c', 'c', 'e', 'p', 't', '_', 's', 'o', 'c', 'k', 'e', 't',
                            OP_PUSH, 0x02,
                            OP_PUSH, 0x00,
                            OP_PUSH, 0x01,
                            OP_SYSCALL, 0x02,
                            OP_HALT};

        eval_push_arg(ex->eval_ctx, arg0);
        eval_push_arg(ex->eval_ctx, val_make_objref(socket_oid_seq));
        eval_exec(ex->eval_ctx, code);

        socket_oid_seq++;
    }
    else if ((oid == 11) && (strncmp(val_get_string_data(method), "error", 5) == 0)
            && (num_args == 1) && (val_type(arg0) == TYPE_INT)) {
        printf("#   11::error %s\n", strerror(val_get_int(arg0)));
    }
    else if ((oid >= 111) && (strncmp(val_get_string_data(method), "closed", 6) == 0)
            && (num_args == 1) && (val_type(arg0) == TYPE_SPECIAL)) {
        printf("#   %li::closed\n", oid);

        opcode code[] = {
                            OP_ARGS_LOCALS, 0x01, 0x01,
                            OP_LOAD_STRING, 0x01, 0x0F, 0x00, 'n', 'e', 't', '_', 's', 'o', 'c', 'k', 'e', 't', '_', 'f', 'r', 'e', 'e', 
                            OP_PUSH, 0x01,
                            OP_PUSH, 0x00,
                            OP_SYSCALL, 0x01,
                            OP_HALT};

        eval_push_arg(ex->eval_ctx, arg0);
        eval_exec(ex->eval_ctx, code);
    }
    else if ((oid >= 111) && (strncmp(val_get_string_data(method), "read", 4) == 0)
            && (num_args == 2) && (val_type(arg0) == TYPE_SPECIAL)
            && (val_type(arg1) == TYPE_STRING)) {
        printf("#   %li::read %s\n", oid, val_get_string_data(arg1));

        opcode code[] = {
                            OP_ARGS_LOCALS, 0x03, 0x03,
                            OP_LOAD_STRING, 0x03, 0x10, 0x00, 'n', 'e', 't', '_', 's', 'o', 'c', 'k', 'e', 't', '_', 'w', 'r', 'i', 't', 'e',
                            OP_LOAD_STRING, 0x04, 0x02, 0x00, '>', ' ',
                            OP_PUSH, 0x03,
                            OP_PUSH, 0x00,
                            OP_PUSH, 0x01,
                            OP_CONCAT, 0x05, 0x04, 0x02,
                            OP_PUSH, 0x05,
                            OP_SYSCALL, 0x03,
                            OP_HALT};

        eval_push_arg(ex->eval_ctx, sockets[oid]);
        eval_push_arg(ex->eval_ctx, arg0);
        eval_push_arg(ex->eval_ctx, arg1);
        eval_exec(ex->eval_ctx, code);
    }
    else if ((oid >= 111) ) {
        printf("#   %li::something\n", oid);
    }
    else {
        printf("#  unhandled call to core!\n");
    }

    store_finish_tx(ex->stx);
}

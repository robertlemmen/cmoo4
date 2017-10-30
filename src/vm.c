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

// -------- implementation of declared public structures --------

struct vm {
    struct store *store;
};

struct eval_ctx {
    struct vm *v;
    struct store_tx *stx;
    // XXX for now, needs actual object reference instead
    struct object *start_obj;
    uint64_t task_id;
};

// -------- implementation of public functions --------

struct vm* vm_new(struct store *s) {
    struct vm *ret = malloc(sizeof(struct vm));
    ret->store = s;
    return ret;
}

void vm_free(struct vm *v) {
    free(v);
}

struct eval_ctx* vm_get_eval_ctx(struct vm *v, object_id id, uint64_t task_id) {
    struct eval_ctx *ret = malloc(sizeof(struct eval_ctx));
    ret->v = v;
    ret->task_id = task_id;
    ret->stx = store_start_tx(v->store);
    ret->start_obj = store_get_object(ret->stx, id);
    printf("# vm_get_eval_ctx %li -> %p\n", id, ret);

    // XXX more, need lock implementation

    return ret;
}

void vm_eval_ctx_exec(struct eval_ctx *ex, val method, int num_args, ...) {
    va_list argp;
    va_start(argp, num_args);

    static struct tasks_ctx *tasks_ctx;
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
    if ((oid == 0) && (strcmp(val_get_string_data(method), "init") == 0)
            && (num_args == 1) && (val_type(arg0) == TYPE_SPECIAL)) {
        printf("#   0::init\n");
        tasks_ctx = val_get_special(arg0);
        // XXX also set up syscalls

        // XXX use syscall here
        tasks_net_make_listener(tasks_ctx, 12345, 11);
    }
    else if ((oid == 0) && (strcmp(val_get_string_data(method), "shutdown") == 0)
            && (num_args == 0)) {
        printf("#   0::shutdown\n");
        // XXX use syscall here
        tasks_net_shutdown_listener(tasks_ctx, 12345);
    }
    else if ((oid == 11) && (strcmp(val_get_string_data(method), "accept") == 0)
            && (num_args == 1) && (val_type(arg0) == TYPE_SPECIAL)) {
        printf("#   11::accept\n");
        sockets[socket_oid_seq] = arg0;
        // XXX use syscall here
        tasks_net_accept_socket(tasks_ctx, val_get_special(arg0), socket_oid_seq);
        socket_oid_seq++;
    }
    else if ((oid == 11) && (strcmp(val_get_string_data(method), "error") == 0)
            && (num_args == 1) && (val_type(arg0) == TYPE_INT)) {
        printf("#   11::error %s\n", strerror(val_get_int(arg0)));
    }
    else if ((oid >= 111) && (strcmp(val_get_string_data(method), "closed") == 0)
            && (num_args == 1) && (val_type(arg0) == TYPE_SPECIAL)) {
        printf("#   %li::closed\n", oid);
        // XXX use syscall here
        tasks_net_socket_free(tasks_ctx, val_get_special(arg0));
    }
    else if ((oid >= 111) && (strcmp(val_get_string_data(method), "read") == 0)
            && (num_args == 2) && (val_type(arg0) == TYPE_SPECIAL)
            && (val_type(arg1) == TYPE_STRING)) {
        printf("#   %li::read %s\n", oid, val_get_string_data(arg1));
        // XXX use syscall here
        tasks_net_socket_write(tasks_ctx,
            val_get_special(sockets[oid]),
            val_get_special(arg0),
            val_get_string_data(arg1), strlen(val_get_string_data(arg1)));
    }
    else if ((oid >= 111) ) {
        printf("#   %li::something\n", oid);
    }
    else {
        printf("#  unhandled call to core!\n");
    }

    store_finish_tx(ex->stx);
    free(ex);
}

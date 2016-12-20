#include "vm.h"

#include <stdlib.h>
#include <stdarg.h>

// XXX
#include <stdio.h>
#include <string.h>

#include "cache.h"
#include "tasks.h"

// -------- implementation of declared public structures --------

struct vm {
    struct cache *cache;
};

struct eval_ctx {
    struct vm *v;
    // XXX for now, needs actual object reference instead
    object_id oid;
    uint64_t task_id;
};

// -------- implementation of public functions --------

struct vm* vm_new(void) {
    struct vm *ret = malloc(sizeof(struct vm));
    ret->cache = cache_new(0);
    return ret;
}

void vm_free(struct vm *v) {
    cache_free(v->cache);
    free(v);
}

struct eval_ctx* vm_get_eval_ctx(struct vm *v, object_id id, uint64_t task_id) {
    struct eval_ctx *ret = malloc(sizeof(struct eval_ctx));
    ret->v = v;
    ret->oid = id;
    ret->task_id = task_id;

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

    printf("# vm_eval_ctx_exec %p oid=%li slot=%s num_args=%i\n", ex,
        ex->oid, val_get_string(method), num_args);

    val arg0;
    if (num_args >= 1) {
        arg0 = va_arg(argp, val);
    }
    val arg1;
    if (num_args >= 2) {
        arg1 = va_arg(argp, val);
    }

    // XXX pseudo-core
    if ((ex->oid == 0) && (strcmp(val_get_string(method), "init") == 0)
            && (num_args == 1) && (val_type(arg0) == TYPE_SPECIAL)) {
        printf("#   0::init\n");
        tasks_ctx = val_get_special(arg0);
        tasks_net_make_listener(tasks_ctx, 12345, 11);
    }
    else if ((ex->oid == 0) && (strcmp(val_get_string(method), "shutdown") == 0)
            && (num_args == 0)) {
        printf("#   0::shutdown\n");
        tasks_net_shutdown_listener(tasks_ctx, 12345);
    }
    else if ((ex->oid == 11) && (strcmp(val_get_string(method), "accept") == 0)
            && (num_args == 1) && (val_type(arg0) == TYPE_SPECIAL)) {
        printf("#   11::accept\n");
        sockets[socket_oid_seq] = arg0;
        tasks_net_accept_socket(tasks_ctx, val_get_special(arg0), socket_oid_seq);
        socket_oid_seq++;
    }
    else if ((ex->oid == 11) && (strcmp(val_get_string(method), "error") == 0)
            && (num_args == 1) && (val_type(arg0) == TYPE_INT)) {
        printf("#   11::error %s\n", strerror(val_get_int(arg0)));
    }
    else if ((ex->oid == 111) && (strcmp(val_get_string(method), "closed") == 0)
            && (num_args == 1) && (val_type(arg0) == TYPE_SPECIAL)) {
        printf("#   %li::closed\n", ex->oid);
        tasks_net_socket_free(tasks_ctx, val_get_special(arg0));
    }
    else if ((ex->oid == 111) && (strcmp(val_get_string(method), "read") == 0)
            && (num_args == 2) && (val_type(arg0) == TYPE_SPECIAL)
            && (val_type(arg1) == TYPE_STRING)) {
        printf("#   %li::read %s\n", ex->oid, val_get_string(arg1));
        tasks_net_socket_write(tasks_ctx,
            val_get_special(sockets[ex->oid]),
            val_get_special(arg0),
            val_get_string(arg1), strlen(val_get_string(arg1)));
    }
    else if ((ex->oid >= 111) ) {
        printf("#   %li::something\n", ex->oid);
    }
    else {
        printf("#  unhandled call to core!\n");
    }

    free(ex);
}

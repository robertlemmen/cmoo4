#include "tasks.h"

#include <stdlib.h>
#include <pthread.h>
// XXX
#include <stdio.h>
#include <string.h>

#include "types.h"

// -------- internal structures --------

// XXX set to 1 for now, needs to be configurable later
#define THREAD_COUNT            1

#define QUEUE_TYPE_STOP         0
#define QUEUE_TYPE_INIT         1
#define QUEUE_TYPE_ACCEPT       2
#define QUEUE_TYPE_LISTEN_ERROR 3
#define QUEUE_TYPE_CLOSED       4
#define QUEUE_TYPE_READ         5

struct accept_data_info {
    struct net_socket *socket;
    object_id oid;
};

struct listen_error_data_info {
    int errnum;
    object_id oid;
};

struct closed_data_info {
    struct net_socket *socket;
    object_id oid;
};

struct read_data_info {
    struct net_socket *socket;
    void *buf;
    size_t size;
    object_id oid;
};

struct queue_item {
    int type;
    union {
        struct accept_data_info accept_data;
        struct listen_error_data_info listen_error_data;
        struct closed_data_info closed_data;
        struct read_data_info read_data;
    };
    struct queue_item *next;
};

// -------- implementation of declared public structures --------

struct tasks_ctx {
    struct net_ctx *net;
    struct ntx_ctx *ntx;
    struct vm *vm;

    int stop_flag;

    // thread pool
    int num_threads;
    pthread_t *thread_ids;

    // queue of work items
    struct queue_item *queue_front;
    struct queue_item *queue_back;
    pthread_mutex_t queue_latch;
    pthread_cond_t queue_cond;

    uint64_t task_id_seq;
};

// -------- internal utilities --------

void tasks_enqueue_item(struct tasks_ctx *ctx, struct queue_item *item) {
    pthread_mutex_lock(&ctx->queue_latch);
    item->next = NULL;
    if (ctx->queue_back) {
        ctx->queue_back->next = item;
    }
    else {
        ctx->queue_front = item;
    }
    ctx->queue_back = item;
    pthread_cond_signal(&ctx->queue_cond);
    pthread_mutex_unlock(&ctx->queue_latch);
}

// -------- implementation of worker threads --------

// these four are called by networking and push work items on to our stack to  be handled
// in the worker threads
void tasks_read_cb(struct net_socket *socket, void *buf, size_t size, void *cb_data1, void *cb_data2) {
    printf("# tasks_read_cb\n");
    struct tasks_ctx *ctx = (struct tasks_ctx*)cb_data1;
    object_id oid = (object_id)cb_data2;
    struct queue_item *item = malloc(sizeof(struct queue_item));
    item->type = QUEUE_TYPE_READ;
    item->read_data.socket = socket;
    item->read_data.oid = oid;
    item->read_data.buf = buf;
    item->read_data.size = size;
    tasks_enqueue_item(ctx, item);
}

void tasks_closed_cb(struct net_socket *socket, void *cb_data1, void *cb_data2) {
    printf("# tasks_closed_cb\n");
    struct tasks_ctx *ctx = (struct tasks_ctx*)cb_data1;
    object_id oid = (object_id)cb_data2;
    struct queue_item *item = malloc(sizeof(struct queue_item));
    item->type = QUEUE_TYPE_CLOSED;
    item->closed_data.socket = socket;
    item->closed_data.oid = oid;
    tasks_enqueue_item(ctx, item);
}

void tasks_accept_cb(struct net_ctx *net, struct net_socket *socket, void *cb_data1, void *cb_data2) {
    printf("# tasks_accept_cb\n");
    struct tasks_ctx *ctx = (struct tasks_ctx*)cb_data1;
    object_id oid = (object_id)cb_data2;
    struct queue_item *item = malloc(sizeof(struct queue_item));
    item->type = QUEUE_TYPE_ACCEPT;
    item->accept_data.socket = socket;
    item->accept_data.oid = oid;
    tasks_enqueue_item(ctx, item);
}

void tasks_listen_error_cb(int errnum, void *cb_data1, void *cb_data2) {
    printf("# tasks_listen_error_cb\n");
    struct tasks_ctx *ctx = (struct tasks_ctx*)cb_data1;
    object_id oid = (object_id)cb_data2;
    struct queue_item *item = malloc(sizeof(struct queue_item));
    item->type = QUEUE_TYPE_LISTEN_ERROR;
    item->listen_error_data.errnum = errnum;
    item->listen_error_data.oid = oid;
    tasks_enqueue_item(ctx, item);
}

void* tasks_thread_func(void *arg) {
    struct tasks_ctx *ctx = (struct tasks_ctx*)arg;

    printf("tasks worker thread running...\n");

    while (!ctx->stop_flag) {
        pthread_mutex_lock(&ctx->queue_latch);
        // need to check stop_flag again
        if (ctx->stop_flag) {
            pthread_mutex_unlock(&ctx->queue_latch);
            break;
        }

        // wait for items to appear
        while (!ctx->queue_front) {
            pthread_cond_wait(&ctx->queue_cond, &ctx->queue_latch);
        }
        // pick off queue
        struct queue_item *current_item = NULL;
        current_item = ctx->queue_front;
        ctx->queue_front = current_item->next;
        current_item->next = NULL;
        if (!ctx->queue_front) {
            ctx->queue_back = NULL;
        }

        // determine a task_id for transaction priorities
        uint64_t task_id = ctx->task_id_seq++;

        struct vm_eval_ctx *vm_eval_ctx;

        // XXX all this processing needs to be aware of TX rollbacks and
        // retry if necessary

        // do first step of processing that requires lock to be held
        switch (current_item->type) {
            case QUEUE_TYPE_INIT:
                vm_eval_ctx = vm_get_eval_ctx(ctx->vm, 0, task_id);
                break;
            case QUEUE_TYPE_LISTEN_ERROR:
                vm_eval_ctx = vm_get_eval_ctx(ctx->vm, current_item->listen_error_data.oid, task_id);
                break;
            case QUEUE_TYPE_STOP:
                vm_eval_ctx = vm_get_eval_ctx(ctx->vm, 0, task_id);
                break;
            case QUEUE_TYPE_ACCEPT:
                vm_eval_ctx = vm_get_eval_ctx(ctx->vm, current_item->accept_data.oid, task_id);
                break;
            case QUEUE_TYPE_READ:;
                vm_eval_ctx = vm_get_eval_ctx(ctx->vm, current_item->read_data.oid, task_id);
                break;
            case QUEUE_TYPE_CLOSED:
                vm_eval_ctx = vm_get_eval_ctx(ctx->vm, current_item->closed_data.oid, task_id);
                break;
            default:
                printf("sdfggd\n");
        }

        pthread_mutex_unlock(&ctx->queue_latch);

        // create network transaction
        struct ntx_tx *net_tx = ntx_new_tx(ctx->ntx);

        // second step of processing the item, without global lock
        val slot;
        switch (current_item->type) {
            case QUEUE_TYPE_INIT:
                slot = val_make_string(4, "init");
                vm_eval_ctx_exec(vm_eval_ctx, slot, 1,
                    val_make_special(ctx));
                val_dec_ref(slot);
                break;
            case QUEUE_TYPE_LISTEN_ERROR:
                slot = val_make_string(5, "error");
                vm_eval_ctx_exec(vm_eval_ctx, slot, 1,
                    val_make_int(current_item->listen_error_data.errnum));
                val_dec_ref(slot);
                break;
            case QUEUE_TYPE_STOP:
                slot = val_make_string(8, "shutdown");
                vm_eval_ctx_exec(vm_eval_ctx, slot, 0);
                val_dec_ref(slot);
                ctx->stop_flag = 1;
                break;
            case QUEUE_TYPE_ACCEPT:
                slot = val_make_string(6, "accept");
                vm_eval_ctx_exec(vm_eval_ctx, slot, 1,
                    val_make_special(current_item->accept_data.socket));
                val_dec_ref(slot);
                break;
            case QUEUE_TYPE_READ:
                slot = val_make_string(4, "read");
                // XXX we really need a separate buffer type that takes pointer
                // and size, and that can be converted to a string using a
                // charset.
                // XXX strings require to be null-terminated, not 100%
                // sure that is really guaranteed at the moment...
                vm_eval_ctx_exec(vm_eval_ctx, slot, 2,
                    val_make_special(net_tx),
                    val_make_string(strlen(current_item->read_data.buf), current_item->read_data.buf));
                val_dec_ref(slot);
                // XXX for now
                free(current_item->read_data.buf);
                break;
            case QUEUE_TYPE_CLOSED:
                slot = val_make_string(6, "closed");
                vm_eval_ctx_exec(vm_eval_ctx, slot, 1,
                    val_make_special(current_item->closed_data.socket));
                val_dec_ref(slot);
                break;
            default:
                printf("sdfdsffsd\n");
        }

        // commit network transaction
        ntx_commit_tx(net_tx);
        vm_free_eval_ctx(vm_eval_ctx);
        free(current_item);
    }

    return NULL;
}

// -------- implementation of public functions --------

struct tasks_ctx* tasks_new_ctx(struct net_ctx *net, struct ntx_ctx *ntx, struct vm *vm) {
    struct tasks_ctx *ret = malloc(sizeof(struct tasks_ctx));
    ret->net = net;
    ret->ntx = ntx;
    ret->vm = vm;
    ret->stop_flag = 0;
    ret->task_id_seq = 1;

    ret->queue_front = NULL;
    ret->queue_back = NULL;
    if (pthread_mutex_init(&ret->queue_latch, NULL) != 0) {
        fprintf(stderr, "pthread_mutex_init failed\n");
        exit(1);
    }
    if (pthread_cond_init(&ret->queue_cond, NULL) != 0) {
        fprintf(stderr, "pthread_cond_init failed\n");
        exit(1);
    }

    ret->num_threads = THREAD_COUNT;
    ret->thread_ids = malloc(sizeof(pthread_t) * ret->num_threads);
    for (int i = 0; i < ret->num_threads; i++) {
        if (pthread_create(&ret->thread_ids[i], NULL, tasks_thread_func, ret) != 0) {
            fprintf(stderr, "pthread_create failed\n");
            exit(1);
        }
    }

    return ret;
}

void tasks_free_ctx(struct tasks_ctx *ctx) {
    pthread_mutex_destroy(&ctx->queue_latch);
    pthread_cond_destroy(&ctx->queue_cond);
    free(ctx);
}

void tasks_start(struct tasks_ctx *ctx) {
    struct queue_item *item = malloc(sizeof(struct queue_item));
    item->type = QUEUE_TYPE_INIT;
    tasks_enqueue_item(ctx, item);
}

void tasks_stop(struct tasks_ctx *ctx) {
    struct queue_item *item = malloc(sizeof(struct queue_item));
    item->type = QUEUE_TYPE_STOP;
    tasks_enqueue_item(ctx, item);

    for (int i = 0; i < ctx->num_threads; i++) {
        pthread_join(ctx->thread_ids[i], NULL);
    }
}

// -------- public functions called back by VM --------

void tasks_net_make_listener(struct tasks_ctx *ctx, unsigned int port, object_id oid) {
    printf("# tasks_net_make_listener\n");
    net_make_listener(ctx->net, port, tasks_listen_error_cb, tasks_accept_cb, ctx, (void*)oid);
}

void tasks_net_shutdown_listener(struct tasks_ctx *ctx, unsigned int port) {
    printf("# tasks_net_shutdown_listener\n");
    net_shutdown_listener(ctx->net, port);
}

void tasks_net_accept_socket(struct tasks_ctx *ctx, struct net_socket *socket, object_id oid) {
    printf("# tasks_net_accept_socket\n");
    net_socket_init(socket, tasks_read_cb, tasks_closed_cb, ctx, (void*)oid);
}

void tasks_net_socket_free(struct tasks_ctx *ctx, struct net_socket *socket) {
    printf("# tasks_net_socket_free\n");
    net_socket_free(socket);
}

void tasks_net_socket_write(struct tasks_ctx *ctx, struct net_socket *socket, struct ntx_tx *net_tx, void *buf, size_t size) {
    printf("# tasks_net_socket_write\n");
    // copy buffer for ownership
    void *cbuf = malloc(size);
    memcpy(cbuf, buf, size);
    ntx_socket_write(net_tx, socket, cbuf, size);
}

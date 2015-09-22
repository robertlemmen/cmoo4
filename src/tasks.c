#include "tasks.h"

#include <stdlib.h>
#include <pthread.h>
// XXX
#include <stdio.h>
#include <string.h>

// -------- internal structures --------

// XXX set to 1 for now, needs to be configurable later
#define THREAD_COUNT            1

#define QUEUE_TYPE_STOP         0
#define QUEUE_TYPE_INIT         1
#define QUEUE_TYPE_ACCEPT       2
#define QUEUE_TYPE_ACCEPT_ERROR 3
#define QUEUE_TYPE_CLOSED       4
#define QUEUE_TYPE_READ         5

struct accept_data_info {
    struct net_socket *socket;
};

struct accept_error_data_info {
    int errnum;
};

struct closed_data_info {
    struct net_socket *socket;
};

struct read_data_info {
    struct net_socket *socket;
    void *buf;
    size_t size;
};

struct queue_item {
    int type;
    union {
        struct accept_data_info accept_data;
        struct accept_error_data_info accept_error_data;
        struct closed_data_info closed_data;
        struct read_data_info read_data;
    };
    struct queue_item *next;
};

// -------- implementation of declared public structures --------

struct tasks_ctx {
    struct net_ctx *net;
    struct ntx_ctx *ntx;

    int stop_flag;

    // thread pool
    int num_threads;
    pthread_t *thread_ids;

    // queue of work items
    struct queue_item *queue_front;
    struct queue_item *queue_back;
    pthread_mutex_t queue_latch;
    pthread_cond_t queue_cond;
};

// -------- internal utilities --------

void tasks_enqueue_item(struct tasks_ctx *ctx, struct queue_item *item) {
    pthread_mutex_lock(&ctx->queue_latch);
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

// XXX these four need to push work items on stack, and be handled in worker thread
void tasks_read_cb(struct net_socket *socket, void *buf, size_t size, void *cb_data) {
    struct tasks_ctx *ctx = (struct tasks_ctx*)cb_data;
    struct queue_item *item = malloc(sizeof(struct queue_item));
    item->type = QUEUE_TYPE_READ;
    item->read_data.socket = socket;
    item->read_data.buf = buf;
    item->read_data.size = size;
    tasks_enqueue_item(ctx, item);
}

void tasks_closed_cb(struct net_socket *socket, void *cb_data) {
    struct tasks_ctx *ctx = (struct tasks_ctx*)cb_data;
    struct queue_item *item = malloc(sizeof(struct queue_item));
    item->type = QUEUE_TYPE_CLOSED;
    item->closed_data.socket = socket;
    tasks_enqueue_item(ctx, item);
}

void tasks_accept_cb(struct net_ctx *net, struct net_socket *socket, void *cb_data) {
    struct tasks_ctx *ctx = (struct tasks_ctx*)cb_data;
    struct queue_item *item = malloc(sizeof(struct queue_item));
    item->type = QUEUE_TYPE_ACCEPT;
    item->accept_data.socket = socket;
    tasks_enqueue_item(ctx, item);
}

void tasks_accept_error_cb(int errnum, void *cb_data) {
    struct tasks_ctx *ctx = (struct tasks_ctx*)cb_data;
    struct queue_item *item = malloc(sizeof(struct queue_item));
    item->type = QUEUE_TYPE_ACCEPT_ERROR;
    item->accept_error_data.errnum = errnum;
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
        while (ctx->queue_front == 0) {
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

        // do first step of processing that requires lock to be held
        switch (current_item->type) {
            case QUEUE_TYPE_INIT:
            case QUEUE_TYPE_CLOSED:
            case QUEUE_TYPE_ACCEPT:
            case QUEUE_TYPE_ACCEPT_ERROR:
                // nothing to do
                break;
            case QUEUE_TYPE_STOP:
                ctx->stop_flag = 1;
                break;
            case QUEUE_TYPE_READ:;
                // XXX read needs to be done in as well as outside the lock:
                // we need to call into the vm to make sure the socket structure is
                // locked before exiting this global lock, otehrwise
                // two messages coming in on the same socket could switch order
                struct ntx_tx *tx = ntx_new_tx(ctx->ntx);
                ntx_socket_write(tx, current_item->read_data.socket, 
                    current_item->read_data.buf, current_item->read_data.size);
                ntx_commit_tx(tx);
                break;
            default:
                printf("sdfggd\n");
        }

        pthread_mutex_unlock(&ctx->queue_latch);

        // second step of processing the item, without global lock
        switch (current_item->type) {
            case QUEUE_TYPE_READ:
            case QUEUE_TYPE_STOP:
                // nothing to do
                break;
            case QUEUE_TYPE_INIT:
                net_make_listener(ctx->net, 12345, tasks_accept_error_cb, tasks_accept_cb, ctx);
                break;
            case QUEUE_TYPE_CLOSED:
                net_socket_free(current_item->closed_data.socket);
                break;
            case QUEUE_TYPE_ACCEPT:
                net_socket_init(current_item->accept_data.socket, tasks_read_cb, tasks_closed_cb, ctx);
                break;
            case QUEUE_TYPE_ACCEPT_ERROR:
                printf("Could not accept: %i\n", current_item->accept_error_data.errnum);
                break;
            default:
                printf("sdfdsffsd\n");
        }

        free(current_item);
    }

    return NULL;
}

// -------- implementation of public functions --------

struct tasks_ctx* tasks_new_ctx(struct net_ctx *net, struct ntx_ctx *ntx) {
    struct tasks_ctx *ret = malloc(sizeof(struct tasks_ctx));
    ret->net = net;
    ret->ntx = ntx;
    ret->stop_flag = 0;

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

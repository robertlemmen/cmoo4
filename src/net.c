#include "net.h"

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <pthread.h>
#include <ev.h>

// XXX
#include <stdio.h>

#define QUEUE_TYPE_STOP         0
#define QUEUE_TYPE_NEW_LISTENER 1

// -------- internal structures --------

struct new_listener_info {
    int port;
    void (*error_callback)(int errnum);
    void (*accept_callback)(struct net_ctx *ctx,
        struct net_socket *socket, void *cb_data);    
    void *cb_data;
};

struct queue_item {
    int type;
    union {
        struct new_listener_info new_listener;
    };
    struct queue_item *next;
};

// -------- implementation of declared public structures --------

struct listener_ctx {
    int port;
    struct ev_io accept_event;
    struct net_ctx *net_ctx;
    void (*accept_callback)(struct net_ctx *ctx,
        struct net_socket *socket, void *cb_data);
    void *cb_data;
    struct listener_ctx *next;
};

struct net_ctx {
    pthread_t thread_id;
    struct ev_loop *loop;
    int stop_flag;

    struct queue_item *queue_front;
    struct queue_item *queue_back;
    pthread_mutex_t queue_mutex;
    struct ev_async queue_event;

    struct listener_ctx *listeners;

    void (*init_callback)(struct net_ctx *ctx);
};

// -------- worker thread implementation  --------

void accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int csock = accept(watcher->fd, (struct sockaddr *)&client_addr, &addr_len);

    struct listener_ctx *lctx = (struct listener_ctx*)watcher->data;

    // XXX create socket structure, register and pass back in callback
    lctx->accept_callback(lctx->net_ctx, NULL, lctx->cb_data);
}

void make_listener(struct net_ctx *ctx, struct new_listener_info *new_listener_request) {
    // create socket, bind, listen...
    int lsock = socket(PF_INET, SOCK_STREAM, 0);
    if (lsock == -1) {
        if (new_listener_request->error_callback) {
            new_listener_request->error_callback(errno);
        }
        return;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(new_listener_request->port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(lsock, (struct sockaddr*) &addr, sizeof(addr)) == -1) {
        if (new_listener_request->error_callback) {
            new_listener_request->error_callback(errno);
        }
        // XXX clean up socket
        return;
    }
    if (listen(lsock, SOMAXCONN) == -1) {
        if (new_listener_request->error_callback) {
            new_listener_request->error_callback(errno);
        }
        // XXX clean up bind, socket
        return;
    }
    
    // create structure for this listener and store in context
    struct listener_ctx *lctx = malloc(sizeof(struct listener_ctx));
    memset(lctx, 0, sizeof(struct listener_ctx));
    lctx->port = new_listener_request->port;
    lctx->net_ctx = ctx;
    lctx->accept_callback = new_listener_request->accept_callback;
    lctx->cb_data = new_listener_request->cb_data;
    // put into ctx
    lctx->next = ctx->listeners;
    ctx->listeners = lctx;

    // libev work to activate it
    ev_io_init(&lctx->accept_event, accept_cb, lsock, EV_READ);
    lctx->accept_event.data = lctx;
    ev_io_start(ctx->loop, &lctx->accept_event);
}

void queue_event_callback(struct ev_loop *loop, struct ev_async *w, int revents) {
    struct net_ctx *ctx = (struct net_ctx*)w->data;

    // get the first item of the queue
    struct queue_item *current_item = NULL;
    // XXX check for return value
    pthread_mutex_lock(&ctx->queue_mutex);
    if (ctx->queue_front) {
        current_item = ctx->queue_front;
        ctx->queue_front = current_item->next;
        current_item->next = NULL;
        if (!ctx->queue_front) {
            ctx->queue_back = NULL;
        }
    }
    pthread_mutex_unlock(&ctx->queue_mutex);

    if (current_item) {
        switch (current_item->type) {
            case QUEUE_TYPE_STOP:
                ctx->stop_flag = 1;
                ev_unloop(ctx->loop, EVUNLOOP_ONE);
                break;
            case QUEUE_TYPE_NEW_LISTENER:
                make_listener(ctx, &current_item->new_listener);
                break;
            default:
                printf("jhkkjh\n");
                // XXX complain
        }
        free(current_item);
    }
}

void* net_thread_func(void *arg) {
    struct net_ctx *ctx = (struct net_ctx*)arg;

    ctx->init_callback(ctx);

    while (!ctx->stop_flag) {
        ev_loop(ctx->loop, 0);
    }

    return NULL;
}

// -------- internal utilities  --------

void net_enqueue_item(struct net_ctx *ctx, struct queue_item *item) {
    pthread_mutex_lock(&ctx->queue_mutex);
    if (ctx->queue_back) {
        ctx->queue_back->next = item;
    }
    else {
        ctx->queue_front = item;
    }
    ctx->queue_back = item;
    pthread_mutex_unlock(&ctx->queue_mutex);

    ev_async_send(ctx->loop, &ctx->queue_event);
}

// -------- implementation of public functions  --------

struct net_ctx* net_new_ctx(void (*init_callback)(struct net_ctx *ctx)) {
    struct net_ctx *ret = malloc(sizeof(struct net_ctx));
    ret->init_callback = init_callback;

    ret->loop = ev_loop_new(EVFLAG_AUTO);

    ret->queue_front = NULL;
    ret->queue_back = NULL;
    if (pthread_mutex_init(&ret->queue_mutex, NULL) != 0) {
        fprintf(stderr, "pthread_mutex_init failed\n");
        // XXX we should die in other places as well!
        exit(1);
    }
    ev_async_init(&ret->queue_event, queue_event_callback);
    ret->queue_event.data = ret;
    ev_async_start(ret->loop, &ret->queue_event);

    return ret;
}

void net_free_ctx(struct net_ctx *ctx) {
    ev_loop_destroy(ctx->loop);
    pthread_mutex_destroy(&ctx->queue_mutex);
    // XXX more cleanups
    free(ctx);
}

void net_start(struct net_ctx *ctx) {
    ctx->stop_flag = 0;
    if (pthread_create(&ctx->thread_id, NULL, net_thread_func, ctx) != 0) {
        fprintf(stderr, "pthread_create failed\n");
        // XXX we should die in other places as well!
        exit(1);
    }
}

void net_stop(struct net_ctx *ctx) {
    // create work item and enqueue
    struct queue_item *stop_item = malloc(sizeof(struct queue_item));   
    stop_item->type = QUEUE_TYPE_STOP;
    stop_item->next = NULL;

    net_enqueue_item(ctx, stop_item);

    // wait for worker thread to finish
    pthread_join(ctx->thread_id, NULL);
}

// XXX may need an error callback as well
void net_make_listener(struct net_ctx *ctx, unsigned int port, 
        void (*error_callback)(int errnum),
	    void (*accept_callback)(struct net_ctx *ctx, struct net_socket *socket, 
            void *cb_data), 
        void *cb_data) {

    printf("net_make_listener()\n");

    struct queue_item *listener_item = malloc(sizeof(struct queue_item));   
    listener_item->type = QUEUE_TYPE_NEW_LISTENER;
    listener_item->next = NULL;
    listener_item->new_listener.port = port;
    listener_item->new_listener.error_callback = error_callback;
    listener_item->new_listener.accept_callback = accept_callback;
    listener_item->new_listener.cb_data = cb_data;

    net_enqueue_item(ctx, listener_item);
}

void net_shutdown_listener(struct net_ctx *ctx, unsigned int port) {
    // XXX
}

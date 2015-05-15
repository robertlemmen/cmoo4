#include "net.h"

#include <stdlib.h>
#include <pthread.h>
#include <ev.h>

// XXX
#include <stdio.h>

struct net_ctx {
    pthread_t thread_id;
    struct ev_loop *loop;
    int stop_flag; // XXX perhaps bitmask to accomodate all sorts of flags
    // XXX or just queue with work to do...

    struct ev_async notif_event;

    void (*init_callback)(struct net_ctx *ctx);
};

void notif_callback(struct ev_loop *loop, struct ev_async *w, int revents) {
    struct net_ctx *ctx = (struct net_ctx*)w->data;
    if (ctx->stop_flag) {
        ev_unloop(ctx->loop, EVUNLOOP_ONE);
        return;
    }
    else {
        // XXX other work
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

struct net_ctx* net_new_ctx(void (*init_callback)(struct net_ctx *ctx)) {
    struct net_ctx *ret = malloc(sizeof(struct net_ctx));
    ret->init_callback = init_callback;

    ret->loop = ev_loop_new(EVFLAG_AUTO);

    ev_async_init(&ret->notif_event, notif_callback);
    ret->notif_event.data = ret;
    ev_async_start(ret->loop, &ret->notif_event);

    return ret;
}

void net_free_ctx(struct net_ctx *ctx) {
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
    ctx->stop_flag = 1;
    ev_async_send(ctx->loop, &ctx->notif_event);
    pthread_join(ctx->thread_id, NULL);
}

// XXX this may need an error callback as well...
void net_make_listener(struct net_ctx *ctx, unsigned int port, 
    void (accept_callback)(struct net_socket *socket)) {
    // XXX
}

void net_shutdown_listener(struct net_ctx *ctx, unsigned int port) {
    // XXX
}

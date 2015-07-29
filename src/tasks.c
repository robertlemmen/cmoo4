#include "tasks.h"

#include <stdlib.h>
// XXX
#include <stdio.h>
#include <string.h>

// -------- internal structures --------

// -------- implementation of declared public structures --------

struct tasks_ctx {
    struct net_ctx *net;
    struct ntx_ctx *ntx;
};

// -------- internal utilities --------

// -------- implementation of worker threads --------

// XXX move all of this into worker thread

void read_cb(struct net_socket *socket, void *buf, size_t size, void *cb_data) {
    struct tasks_ctx *ctx = (struct tasks_ctx*)cb_data;
    printf("# socket read callback %li\n", size);
    // send it back...
    struct ntx_tx *tx = ntx_new_tx(ctx->ntx);
    ntx_socket_write(tx, socket, buf, size);
    ntx_commit_tx(tx);
}

void closed_cb(struct net_socket *socket, void *cb_data) {
//    struct tasks_ctx *ctx = (struct tasks_ctx*)cb_data;
    printf("# socket closed callback\n");
    net_socket_free(socket);
}

void accept_cb(struct net_ctx *net, struct net_socket *socket, void *data) {
    printf("# net accept callback\n");

    net_socket_init(socket, read_cb, closed_cb, data);
}

void accept_error_cb(int errnum) {
    printf("# net accept error: %s\n", strerror(errnum));
}

// -------- implementation of public functions --------

struct tasks_ctx* tasks_new_ctx(struct net_ctx *net, struct ntx_ctx *ntx) {
    struct tasks_ctx *ret = malloc(sizeof(struct tasks_ctx));
    ret->net = net;
    ret->ntx = ntx;
    return ret;
}

void tasks_free_ctx(struct tasks_ctx *ctx) {
    free(ctx);
}

void tasks_start(struct tasks_ctx *ctx) {
    // XXX move into worker thread, into some "init" procedure
    net_make_listener(ctx->net, 12345, accept_error_cb, accept_cb, ctx);
}

void tasks_stop(struct tasks_ctx *ctx) {
}

#include "ntx.h"

#include <stdlib.h>

// -------- internal structures --------

// -------- implementation of declared public structures --------

struct ntx_ctx {
    struct net_ctx *net_ctx;
};

struct ntx_tx {
    struct ntx_ctx *ctx;
};

// -------- internal utilities  --------

// -------- implementation of public functions  --------

struct ntx_ctx* ntx_new_ctx(struct net_ctx *net) {
    struct ntx_ctx *ret = malloc(sizeof(struct ntx_ctx));
    ret->net_ctx = net;
    return ret;
}

void ntx_free_ctx(struct ntx_ctx *ctx) {
    free(ctx);
}

struct ntx_tx* ntx_new_tx(struct ntx_ctx *ctx) {
    struct ntx_tx *ret = malloc(sizeof(struct ntx_ctx));
    ret->ctx = ctx;
    return ret;
}

// XXX this is just a stub implementation that passes everything through, we need to flesh this out once
// we have multiple threads
void ntx_commit_tx(struct ntx_tx *tx) {
    free(tx);
}

void ntx_rollback_tx(struct ntx_tx *tx) {
    free(tx);
}

void ntx_socket_close(struct ntx_tx *tx, struct net_socket *s) {
    net_socket_close(s);
}

void ntx_socket_write(struct ntx_tx *tx, struct net_socket *s, void *buf, size_t size) {
    net_socket_write(s, buf, size);
}


#include "ntx.h"

#include <stdlib.h>
#include <stdint.h>

#define SOCKET_CLOSE    0
#define SOCKET_WRITE    1

struct net_tx_op {
    struct net_socket *s;
    void *buf;
    size_t size;
    uint8_t op_type;
    struct net_tx_op *next;
};

// -------- implementation of declared public structures --------

struct ntx_ctx {
    struct net_ctx *net_ctx;
};

struct ntx_tx {
    struct ntx_ctx *ctx;
    struct net_tx_op *first;
    struct net_tx_op *last;
};

// -------- implementation of public functions --------

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
    ret->first = NULL;
    ret->last = NULL;
    return ret;
}

void ntx_commit_tx(struct ntx_tx *tx) {
    struct net_tx_op *cop = tx->first;
    while (cop) {
        switch (cop->op_type) {
            case SOCKET_WRITE:
                net_socket_write(cop->s, cop->buf, cop->size);
                break;
            case SOCKET_CLOSE:
                net_socket_close(cop->s);
                break;
        }

        struct net_tx_op *temp = cop;
        cop = cop->next;
        free(temp);
    }
    tx->first = NULL;
    tx->last = NULL;
}

void ntx_rollback_tx(struct ntx_tx *tx) {
    struct net_tx_op *cop = tx->first;
    while (cop) {
        switch (cop->op_type) {
            case SOCKET_WRITE:
                // free the owned data
                free(cop->buf);
                break;
            case SOCKET_CLOSE:
                // just discard
                break;
        }

        struct net_tx_op *temp = cop;
        cop = cop->next;
        free(temp);
    }
    tx->first = NULL;
    tx->last = NULL;
}

void ntx_free_tx(struct ntx_tx *tx) {
    free(tx);
}

void ntx_socket_close(struct ntx_tx *tx, struct net_socket *s) {
    struct net_tx_op *new_op = malloc(sizeof(struct net_tx_op));
    new_op->s = s;
    new_op->buf = NULL;
    new_op->size = 0;
    new_op->op_type = SOCKET_CLOSE;
    new_op->next = NULL;

    if (tx->last) {
        tx->last->next = new_op;
    }
    else {
        tx->first = new_op;
        tx->last = new_op;
    }
}

void ntx_socket_write(struct ntx_tx *tx, struct net_socket *s, void *buf, size_t size) {
    struct net_tx_op *new_op = malloc(sizeof(struct net_tx_op));
    new_op->s = s;
    new_op->buf = buf;
    new_op->size = size;
    new_op->op_type = SOCKET_WRITE;
    new_op->next = NULL;

    if (tx->last) {
        tx->last->next = new_op;
    }
    else {
        tx->first = new_op;
        tx->last = new_op;
    }
}


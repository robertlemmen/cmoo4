#ifndef NTX_H
#define NTX_H

#include "net.h"

/* This module implements a proxy layer over the network module that allows
 * "batching" of outgoing I/O operations so that they can be rolled back or 
 * comitted by the vm/task logic.
 *
 * Note that many operations are not managed by this, e.g. listener 
 * setup/teardown. 
 * */

struct ntx_ctx;
struct ntx_tx;

struct ntx_ctx* ntx_new_ctx(struct net_ctx *net); 
void ntx_free_ctx(struct ntx_ctx *ctx);

struct ntx_tx* ntx_new_tx(struct ntx_ctx *ctx);
void ntx_commit_tx(struct ntx_tx *tx);
void ntx_rollback_tx(struct ntx_tx *tx);

void ntx_socket_close(struct ntx_tx *tx, struct net_socket *s);
void ntx_socket_write(struct ntx_tx *tx, struct net_socket *s, void *buf, size_t size);

#endif /* NTX_H */

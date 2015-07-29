#ifndef NET_H
#define NET_H

#include <stddef.h>

/* this module implements a networking subsystem that uses a thread
 * to handle accepting connections, reading and writing.
 *
 * When creating this, you pass a init callback. When the subsystem is 
 * started, this callback is executed, which can then e.g. set up 
 * listener sockets etc. 
 *
 * All work is done in the main thread, calls into this module can be 
 * done by other threads safely
 * */

struct net_ctx;
struct net_socket;

// XXX if we would guarantee that all pending queue items are handled on start(), we
// would not need an init callback, just create, start and get going!
struct net_ctx* net_new_ctx(void (*init_callback)(struct net_ctx *ctx)); 
void net_free_ctx(struct net_ctx *ctx);

void net_start(struct net_ctx *ctx);
void net_stop(struct net_ctx *ctx);

void net_make_listener(struct net_ctx *ctx, unsigned int port, 
    void (*error_callback)(int errnum),
    void (*accept_callback)(struct net_ctx *ctx, struct net_socket *socket, 
        void *cb_data), void *cb_data);
void net_shutdown_listener(struct net_ctx *ctx, unsigned int port);

void net_socket_init(struct net_socket *s, 
    void (*read_callback)(struct net_socket *s, void *buf, size_t size, void *cb_data),
    void (*closed_callback)(struct net_socket *s, void *cb_data),
    void *cb_data);
void net_socket_close(struct net_socket *s);
void net_socket_free(struct net_socket *s);
void net_socket_write(struct net_socket *s, void *buf, size_t size);

#endif /* NET_H */

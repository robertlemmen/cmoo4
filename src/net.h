#ifndef NET_H
#define NET_H

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

struct net_ctx* net_new_ctx(void (*init_callback)(struct net_ctx *ctx)); 
void net_free_ctx(struct net_ctx *ctx);

void net_start(struct net_ctx *ctx);
void net_stop(struct net_ctx *ctx);

void net_make_listener(struct net_ctx *ctx, unsigned int port, 
    void (accept_callback)(struct net_socket *socket));
void net_shutdown_listener(struct net_ctx *ctx, unsigned int port);

#endif /* NET_H */

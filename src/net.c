#include "net.h"

#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <pthread.h>
#include <ev.h>
#include <unistd.h>

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
    pthread_mutex_t queue_latch;
    struct ev_async queue_event;

    struct listener_ctx *listeners;

    void (*init_callback)(struct net_ctx *ctx);
};

struct socket_buffer {
    void *data;
    size_t size;
    struct socket_buffer *next;
};

struct net_socket {
    int socket; // XXX "fd"?
    int refcount;
    struct net_ctx *net_ctx;
    
    void (*read_callback)(void *buf, size_t size, void *cb_data);
    void (*closed_callback)(void *cb_data);
    void *cb_data;

    ev_io read_event;
    ev_io write_event;

    struct socket_buffer *first_buffer;
    struct socket_buffer *last_buffer;

    pthread_mutex_t socket_latch; // XXX just "latch"?
};

// -------- worker thread implementation  --------

void net_read_cb(struct ev_loop *loop, ev_io *watcher, int revents) {
    struct net_socket *socket = (struct net_socket*)watcher->data;
    // XXX bufsize should really be configurable...
    char buf[256];
    ssize_t count = read(watcher->fd, buf, 256);
    if (count == 0) {
        printf("eof on socket\n");
        close(socket->socket);
        ev_io_stop(socket->net_ctx->loop, &socket->read_event);
        if (socket->first_buffer) {
            ev_io_stop(socket->net_ctx->loop, &socket->write_event);
        }
        socket->socket = 0;
        // XXX EOF, call closed cb 
    }
    else if (count < 0) {
        // XXX error, close as above
        printf("error on socket read: %s\n", strerror(errno));
    }
    else {
        printf("net_read_cb %li\n", count);
        if (socket->read_callback) {
            char *nbuf = malloc(count);
            memcpy(nbuf, buf, count);
            socket->read_callback(nbuf, count, socket->cb_data);
        }
    }
}

void net_write_cb(struct ev_loop *loop, ev_io *watcher, int revents) {
    struct net_socket *socket = (struct net_socket*)watcher->data;
    pthread_mutex_lock(&socket->socket_latch);
    ssize_t count = write(watcher->fd, socket->first_buffer->data, socket->first_buffer->size);
    printf("net_write_cb %li\n", count);
    ev_io_stop(socket->net_ctx->loop, &socket->write_event);
    pthread_mutex_unlock(&socket->socket_latch);
}

void net_accept_cb(struct ev_loop *loop, struct ev_io *watcher, int revents) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    struct listener_ctx *lctx = (struct listener_ctx*)watcher->data;

    struct net_socket *nsock = malloc(sizeof(struct net_socket));
    nsock->socket = accept(watcher->fd, (struct sockaddr *)&client_addr, &addr_len);
    nsock->refcount = 2;
    nsock->net_ctx = lctx->net_ctx;
/*    pthread_mutexattr_t mutex_attr;
    pthread_mutexattr_init(&mutex_attr);
    pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_RECURSIVE_NP);
*/
    if (pthread_mutex_init(&nsock->socket_latch, NULL) != 0) {
        fprintf(stderr, "pthread_mutex_init failed\n");
        exit(1);
    }
    nsock->read_callback = NULL;
    nsock->closed_callback = NULL;
    nsock->cb_data = NULL;
    ev_io_init(&nsock->read_event, net_read_cb, nsock->socket, EV_READ);
    nsock->read_event.data = nsock;
    ev_io_init(&nsock->write_event, net_write_cb, nsock->socket, EV_WRITE);
    nsock->write_event.data = nsock;
    nsock->first_buffer = NULL;
    nsock->last_buffer = NULL;

    // XXX also make non-blocking?

    lctx->accept_callback(lctx->net_ctx, nsock, lctx->cb_data);
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
    ev_io_init(&lctx->accept_event, net_accept_cb, lsock, EV_READ);
    lctx->accept_event.data = lctx;
    ev_io_start(ctx->loop, &lctx->accept_event);
}

void queue_event_callback(struct ev_loop *loop, struct ev_async *w, int revents) {
    struct net_ctx *ctx = (struct net_ctx*)w->data;

    // get the first item of the queue
    struct queue_item *current_item = NULL;
    // XXX check for return value
    pthread_mutex_lock(&ctx->queue_latch);
    if (ctx->queue_front) {
        current_item = ctx->queue_front;
        ctx->queue_front = current_item->next;
        current_item->next = NULL;
        if (!ctx->queue_front) {
            ctx->queue_back = NULL;
        }
    }
    pthread_mutex_unlock(&ctx->queue_latch);

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
    pthread_mutex_lock(&ctx->queue_latch);
    if (ctx->queue_back) {
        ctx->queue_back->next = item;
    }
    else {
        ctx->queue_front = item;
    }
    ctx->queue_back = item;
    pthread_mutex_unlock(&ctx->queue_latch);

    ev_async_send(ctx->loop, &ctx->queue_event);
}

void net_socket_dec_refcount(struct net_socket *s) {
    s->refcount--;
    if (s->refcount == 0) {
        // XXX cleanup, this requires telling the net_ctx event queue in order
        // to allow cleanup of all sockets when system shuts down, so net_ctx needs
        // to keep list of sockets...
    }
}


// -------- implementation of public functions  --------

struct net_ctx* net_new_ctx(void (*init_callback)(struct net_ctx *ctx)) {
    struct net_ctx *ret = malloc(sizeof(struct net_ctx));
    ret->init_callback = init_callback;

    ret->loop = ev_loop_new(EVFLAG_AUTO);

    ret->queue_front = NULL;
    ret->queue_back = NULL;
    if (pthread_mutex_init(&ret->queue_latch, NULL) != 0) {
        fprintf(stderr, "pthread_mutex_init failed\n");
        exit(1);
    }
    ev_async_init(&ret->queue_event, queue_event_callback);
    ret->queue_event.data = ret;
    ev_async_start(ret->loop, &ret->queue_event);

    return ret;
}

void net_free_ctx(struct net_ctx *ctx) {
    ev_loop_destroy(ctx->loop);
    pthread_mutex_destroy(&ctx->queue_latch);
    // XXX more cleanups
    free(ctx);
}

void net_start(struct net_ctx *ctx) {
    ctx->stop_flag = 0;
    if (pthread_create(&ctx->thread_id, NULL, net_thread_func, ctx) != 0) {
        fprintf(stderr, "pthread_create failed\n");
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

void net_socket_init(struct net_socket *s, 
        void (*read_callback)(void *buf, size_t size, void *cb_data),
        void (*closed_callback)(void *cb_data),
        void *cb_data) {
    s->read_callback = read_callback;
    s->closed_callback = closed_callback;
    s->cb_data = cb_data;

    ev_io_start(s->net_ctx->loop, &s->read_event);
}

void net_socket_close(struct net_socket *s) {
    pthread_mutex_lock(&s->socket_latch);
    shutdown(s->socket, SHUT_RDWR);
    close(s->socket);
    ev_io_stop(s->net_ctx->loop, &s->read_event);
    if (s->first_buffer) {
        ev_io_stop(s->net_ctx->loop, &s->write_event);
    }
    s->socket = 0;
    pthread_mutex_unlock(&s->socket_latch);
}

void net_socket_free(struct net_socket *s) {
    pthread_mutex_lock(&s->socket_latch);
    net_socket_dec_refcount(s);
    pthread_mutex_unlock(&s->socket_latch);
}

void net_socket_write(struct net_socket *s, void *buf, size_t size) {
    printf("net_socket_write\n");
    pthread_mutex_lock(&s->socket_latch);
    struct socket_buffer *sbuf = malloc(sizeof(struct socket_buffer));
    sbuf->data = buf;
    sbuf->size = size;
    sbuf->next = NULL;
    if (s->last_buffer) {
        s->last_buffer->next = sbuf;
    }
    s->last_buffer = sbuf;
    if (!s->first_buffer) {
        s->first_buffer = sbuf;
        ev_io_start(s->net_ctx->loop, &s->write_event);
    }
    pthread_mutex_unlock(&s->socket_latch);
}

#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "net.h"
#include "ntx.h"

struct ntx_ctx *ntx = NULL;

void read_cb(struct net_socket *socket, void *buf, size_t size, void *cb_data) {
    printf("# socket read callback %li\n", size);
    // send it back...
    struct ntx_tx *tx = ntx_new_tx(ntx);
    ntx_socket_write(tx, socket, buf, size);
    ntx_commit_tx(tx);
}

void closed_cb(struct net_socket *socket, void *cb_data) {
    printf("# socket closed callback\n");
    net_socket_free(socket);
}

void accept_cb(struct net_ctx *net, struct net_socket *socket, void *data) {
    printf("# net accept callback\n");

    net_socket_init(socket, read_cb, closed_cb, socket);
}

void accept_error_cb(int errnum) {
    printf("# net accept error: %s\n", strerror(errnum));
}

void net_init_cb(struct net_ctx *net) {
    printf("# net init callback\n");

    net_make_listener(net, 12345, accept_error_cb, accept_cb, NULL);
    ntx = ntx_new_ctx(net);
}

int main(int argc, char **argv) {
    printf("-=[ CMOO ]=-\n");

    struct net_ctx *net = net_new_ctx(net_init_cb);
    net_start(net);

    sleep(5);

    net_shutdown_listener(net, 12345);
    net_stop(net);
    ntx_free_ctx(ntx);
    net_free_ctx(net);

    return 0;
}

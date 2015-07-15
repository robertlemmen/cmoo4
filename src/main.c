#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "net.h"

void read_cb(void *buf, size_t size, void *cb_data) {
    struct net_socket *socket = (struct net_socket*)cb_data;
    printf("# socket read callback %li\n", size);
    // send it back...
    net_socket_write(socket, buf, size);
}

void closed_cb(void *cb_data) {
    struct net_socket *socket = (struct net_socket*)cb_data;
    printf("# socket closed callback\n");
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
}

int main(int argc, char **argv) {
    printf("-=[ CMOO ]=-\n");

    struct net_ctx *net = net_new_ctx(net_init_cb);
    net_start(net);

    sleep(10);

    net_stop(net);;
    net_free_ctx(net);

    return 0;
}

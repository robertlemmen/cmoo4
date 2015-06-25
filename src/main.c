#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "net.h"

void net_accept_cb(struct net_ctx *net, struct net_socket *socket, void *data) {
    printf("# net accept callback\n");
}

void net_accept_error_cb(int errnum) {
    printf("# net accept error: %s\n", strerror(errnum));
}

void net_init_cb(struct net_ctx *net) {
    printf("# net init callback\n");

    net_make_listener(net, 12345, net_accept_error_cb, net_accept_cb, NULL);
}

int main(int argc, char **argv) {
    printf("-=[ CMOO ]=-\n");

    struct net_ctx *net = net_new_ctx(net_init_cb);
    net_start(net);

    sleep(5);

    net_stop(net);;
    net_free_ctx(net);

    return 0;
}

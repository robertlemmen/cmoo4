#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "net.h"
#include "ntx.h"
#include "tasks.h"

struct ntx_ctx *ntx = NULL;
struct tasks_ctx *tasks = NULL;

void net_init_cb(struct net_ctx *net) {
    printf("# net init callback\n");

    ntx = ntx_new_ctx(net);
    tasks = tasks_new_ctx(net, ntx);
    tasks_start(tasks);
}

int main(int argc, char **argv) {
    printf("-=[ CMOO ]=-\n");

    struct net_ctx *net = net_new_ctx(net_init_cb);
    net_start(net);

    sleep(10);

    net_shutdown_listener(net, 12345);
    net_stop(net);
    tasks_stop(tasks);
    tasks_free_ctx(tasks);
    ntx_free_ctx(ntx);
    net_free_ctx(net);

    return 0;
}

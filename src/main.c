#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "net.h"
#include "ntx.h"
#include "tasks.h"
#include "vm.h"
#include "store.h"

struct ntx_ctx *ntx = NULL;
struct tasks_ctx *tasks = NULL;
struct vm *vm = NULL;

void net_init_cb(struct net_ctx *net) {
    printf("# net init callback\n");

    ntx = ntx_new_ctx(net);
    tasks = tasks_new_ctx(net, ntx, vm);
    tasks_start(tasks);
}

int main(int argc, char **argv) {
    printf("-=[ CMOO ]=-\n");

    struct persist *persist = persist_new();
    struct store *store = store_new(persist);
    vm = vm_new(store);
    struct net_ctx *net = net_new_ctx(net_init_cb);
    net_start(net);

    sleep(10);

    net_stop(net);
    tasks_stop(tasks);
    tasks_free_ctx(tasks);
    ntx_free_ctx(ntx);
    net_free_ctx(net);
    vm_free(vm);
    store_free(store);
    persist_free(persist);

    return 0;
}

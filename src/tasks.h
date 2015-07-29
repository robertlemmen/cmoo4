#ifndef TASKS_H
#define TASKS_H

#include "ntx.h"
#include "net.h"

struct tasks_ctx;

struct tasks_ctx* tasks_new_ctx(struct net_ctx *net, struct ntx_ctx *ntx); 
void tasks_free_ctx(struct tasks_ctx *ctx);

void tasks_start(struct tasks_ctx *ctx);
void tasks_stop(struct tasks_ctx *ctx);

#endif /* TASKS_H */

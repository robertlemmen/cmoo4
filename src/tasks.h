#ifndef TASKS_H
#define TASKS_H

#include "ntx.h"
#include "net.h"

/* there is a nasty complication in all this once we have more than one 
 * thread: two inputs could come from the net subsystem in rapid 
 * succession, so we need some sort of locking to make sure that the 
 * second one does not jump the queue. since all proper locking is done 
 * on the other side of the VM, this means that the VM needs a way to 
 * create an evaluation context which locks the first object and 
 * immediately returns. the tasks can then do:
 *
 * - lock
 * - take item of queue
 * - create vm eval context
 * - unlock
 * - continue work on eval context
 *
 * also note that this initial lock on the base object needs to be an 
 * exclusive lock! the assumption is that the object in question is a
 * socket representation, so it does not cause great contention to 
 * have this exclusive lock
 * */

struct tasks_ctx;

struct tasks_ctx* tasks_new_ctx(struct net_ctx *net, struct ntx_ctx *ntx); 
void tasks_free_ctx(struct tasks_ctx *ctx);

void tasks_start(struct tasks_ctx *ctx);
void tasks_stop(struct tasks_ctx *ctx);

#endif /* TASKS_H */

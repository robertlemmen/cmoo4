#include "vm.h"

#include <stdlib.h>

#include "cache.h"

// -------- implementation of declared public structures --------

struct vm {
    struct cache *cache;
};

struct eval_ctx {
    struct vm *v;
};

// -------- implementation of public functions --------

struct vm* vm_new(void) {
    struct vm *ret = malloc(sizeof(struct vm));
    ret->cache = cache_new(0);
    return ret;
}

void vm_free(struct vm *v) {
    cache_free(v->cache);
    free(v);
}

struct eval_ctx* vm_get_eval_ctx(struct vm *v, object_id id) {
    struct eval_ctx *ret = malloc(sizeof(struct eval_ctx));
    ret->v = v;

    // XXX more, need lock implementation

    return ret;
}

void vm_eval_ctx_exec(struct eval_ctx *ex) {
    // XXX implement

    free(ex);
}

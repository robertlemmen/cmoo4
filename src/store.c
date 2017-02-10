#include "store.h"

#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <assert.h>

#include "cache.h"

// XXX
#include "object.h"

// -------- implementation of declared public structures --------

struct store {
    struct cache *cache;
    pthread_mutex_t cache_latch;
};

struct store_tx {
    struct store *store;
};

// -------- utility functions internal to this module --------

void mk_duff_object(struct cache *c, object_id oid) {
    struct object *o = obj_new();
    obj_set_id(o, oid);
    struct lobject *lo = lobject_new();
    lobject_set_object(lo, o);
    cache_put_object(c, lo);
}

// -------- implementation of public functions --------

struct store* store_new(void) {
    struct store *ret = malloc(sizeof(struct store));
    ret->cache = cache_new(0);
    if (pthread_mutex_init(&ret->cache_latch, NULL) != 0) {
        fprintf(stderr, "pthread_mutex_init failed\n");
        exit(1);
    }

    // XXX put a few intiial objects into cache for test purposes
    mk_duff_object(ret->cache, 0);
    mk_duff_object(ret->cache, 11);
    mk_duff_object(ret->cache, 111);
    mk_duff_object(ret->cache, 112);
    mk_duff_object(ret->cache, 113);

    return ret;
}

void store_free(struct store *s) {
    cache_free(s->cache);
    pthread_mutex_destroy(&s->cache_latch);
    free(s);
}

struct store_tx* store_start_tx(struct store *s) {
    struct store_tx *ret = malloc(sizeof(struct store_tx));
    ret->store = s;
    return ret;
}

void store_finish_tx(struct store_tx *tx) {
    // XXX
    free(tx);
}

struct object* store_get_object(struct store_tx *tx, object_id oid) {
    struct store *s = tx->store;
    struct lobject *lo;
    pthread_mutex_lock(&s->cache_latch);
    lo = cache_get_object(s->cache, oid);
    assert(lo != NULL);
    // XXX if not found, get from persistence and put in cache
    // XXX lock object
    pthread_mutex_unlock(&s->cache_latch);
    // XXX store object in tx for later release

    return lobject_get_object(lo);
}


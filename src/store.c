#include "store.h"

#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <assert.h>

#include "cache.h"

#define CACHE_SIZE      1024

// -------- implementation of declared public structures --------

struct store {
    struct cache *cache;
    struct persist *persist;
    pthread_mutex_t cache_latch;
};

struct lobject_list_node {
    struct lobject *lo;
    struct lobject_list_node *next;
};

struct store_tx {
    struct store *store;
    struct lobject_list_node *locked;
};

// -------- implementation of public functions --------

struct store* store_new(struct persist *p) {
    struct store *ret = malloc(sizeof(struct store));
    ret->persist = p;
    // XXX should we get cache from args like persist?
    ret->cache = cache_new(CACHE_SIZE);
    if (pthread_mutex_init(&ret->cache_latch, NULL) != 0) {
        fprintf(stderr, "pthread_mutex_init failed\n");
        exit(1);
    }

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
    ret->locked = NULL;
    printf("## store_start_tx -> %p\n", ret);
    return ret;
}

void store_finish_tx(struct store_tx *tx) {
    printf("## store_finish_tx %p\n", tx);
    struct store *s = tx->store;
    pthread_mutex_lock(&s->cache_latch);
    while (tx->locked) {
        struct lobject_list_node *temp = tx->locked;
        tx->locked = temp->next;
        lock_unlock(lobject_get_lock(temp->lo));
        cache_release_object(s->cache, temp->lo);
        printf("##   %li\n", obj_get_id(lobject_get_object(temp->lo)));
    }
    pthread_mutex_unlock(&s->cache_latch);
    free(tx);
}

struct object* store_get_object(struct store_tx *tx, object_id oid) {
    printf("## store_get_object %li\n", oid);
    struct store *s = tx->store;
    struct lobject *lo;
    pthread_mutex_lock(&s->cache_latch);
    lo = cache_get_object(s->cache, oid);

    if (lo == NULL) {
        printf("##   getting from persistence\n");
        struct object *po = persist_get(s->persist, oid);
        // XXX not sure what to do in this case...
        assert(po != NULL);
        struct lock *l = lock_new();
        lo = lobject_new();
        lobject_set_object(lo, po);
        lobject_set_lock(lo, l);
        cache_put_object(s->cache, lo);
    }

    lock_lock(lobject_get_lock(lo));

    pthread_mutex_unlock(&s->cache_latch);

    // put in tx to release later
    struct lobject_list_node *list_node = malloc(sizeof(struct lobject_list_node));
    list_node->lo = lo;
    list_node->next = tx->locked;
    tx->locked = list_node;

    return lobject_get_object(lo);
}


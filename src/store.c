#include "store.h"

#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "cache.h"

#define CACHE_SIZE      1024

// -------- implementation of declared public structures --------

struct store {
    struct cache *cache;
    struct persist *persist;
    // the cache latch is outside the cache so that we can do overhand locking
    // with the object itself
    pthread_mutex_t cache_latch;
    // XXX kludge, need better allocator with persistence integration
    struct locks_ctx *locks_ctx;
    int alloc_id;
    // the sid is sequential per store_tx and wraps around, used to determine
    // the younger transaction in a deadlock
    uint64_t sid_seq;
    int max_tasks;
    bool *cid_used;
    pthread_mutex_t ids_latch;
};

struct lobject_list_node {
    struct lobject *lo;
    struct lobject_list_node *next;
};

struct store_tx {
    struct store *store;
    struct lobject_list_node *locked;
    uint64_t sid;
    int cid;
};

// -------- implementation of public functions --------

struct store* store_new(struct persist *p, int max_tasks) {
    struct store *ret = malloc(sizeof(struct store));
    ret->persist = p;
    // XXX should we get cache from args like persist?
    ret->cache = cache_new(CACHE_SIZE);
    if (pthread_mutex_init(&ret->cache_latch, NULL) != 0) {
        fprintf(stderr, "pthread_mutex_init failed\n");
        exit(1);
    }
    if (pthread_mutex_init(&ret->ids_latch, NULL) != 0) {
        fprintf(stderr, "pthread_mutex_init failed\n");
        exit(1);
    }
    ret->locks_ctx = locks_new_ctx(max_tasks);
    ret->alloc_id = 1000;
    ret->sid_seq = 0;
    ret->max_tasks = max_tasks;
    ret->cid_used = malloc(sizeof(bool) * max_tasks);
    memset(ret->cid_used, 0, sizeof(bool) * max_tasks); // memset for stdbool feels dirty...
    return ret;
}

void store_free(struct store *s) {
    cache_free(s->cache);
    locks_free_ctx(s->locks_ctx);
    pthread_mutex_destroy(&s->cache_latch);
    pthread_mutex_destroy(&s->ids_latch);
    free(s->cid_used);
    free(s);
}

struct store_tx* store_start_tx(struct store *s) {
    struct store_tx *ret = malloc(sizeof(struct store_tx));
    ret->store = s;
    ret->locked = NULL;
    pthread_mutex_lock(&s->ids_latch);
    ret->sid = s->sid_seq++;
    // allocate a cid, which is reused, and can never exceed max_tasks
    for (int i = 0; i < s->max_tasks; i++) {
        if (!s->cid_used[i]) {
            s->cid_used[i] = true;
            ret->cid = i;
            break;
        }
    }
    pthread_mutex_unlock(&s->ids_latch);
    assert(ret->cid < s->max_tasks);
    printf("## store_start_tx -> %p sid:%i cid:%i\n", ret, ret->sid, ret->cid);
    return ret;
}

void store_finish_tx(struct store_tx *tx) {
    printf("## store_finish_tx %p\n", tx);
    struct store *s = tx->store;
    pthread_mutex_lock(&s->cache_latch);
    while (tx->locked) {
        struct lobject_list_node *temp = tx->locked;
        tx->locked = temp->next;
        printf("### tx %lX unlocking obj %li\n", tx, obj_get_id(lobject_get_object(temp->lo)));
        lock_unlock(lobject_get_lock(temp->lo), tx);
        cache_release_object(s->cache, temp->lo);
        free(temp);
    }
    pthread_mutex_unlock(&s->cache_latch);

    pthread_mutex_lock(&s->ids_latch);
    assert(s->cid_used[tx->cid]);
    pthread_mutex_unlock(&s->ids_latch);
    s->cid_used[tx->cid] = false;
    free(tx);
}

uint64_t store_tx_get_sid(struct store_tx *tx) {
    return tx->sid;
}

int store_tx_get_cid(struct store_tx *tx) {
    return tx->cid;
}

struct store_tx *store_new_mock_tx(uint64_t sid, int cid) {
    struct store_tx *ret = malloc(sizeof(struct store_tx));
    memset(ret, 0, sizeof(struct store_tx));
    ret->sid = sid;
    ret->cid = cid;
}

void store_free_mock_tx(struct store_tx *tx) {
    free(tx);
}

struct lobject* store_get_object(struct store_tx *tx, object_id oid) {
    printf("## store_get_object %li\n", oid);
    struct store *s = tx->store;
    struct lobject *lo;
    pthread_mutex_lock(&s->cache_latch);
    lo = cache_get_object(s->cache, oid);

    if (lo == NULL) {
        struct object *po = persist_get(s->persist, oid);
        // XXX not sure what to do in this case...
        assert(po != NULL);
        struct lock *l = lock_new(s->locks_ctx);
        lo = lobject_new();
        lobject_set_object(lo, po);
        lobject_set_lock(lo, l);
        cache_put_object(s->cache, lo);
    }

    pthread_mutex_unlock(&s->cache_latch);

    printf("### tx %lX locking obj %li SHARED\n", tx, obj_get_id(lobject_get_object(lo)));
    if (lock_lock(lobject_get_lock(lo), LOCK_SHARED, tx)) {
        return NULL;
    }

    // put in tx to release later
    struct lobject_list_node *list_node = malloc(sizeof(struct lobject_list_node));
    list_node->lo = lo;
    list_node->next = tx->locked;
    tx->locked = list_node;

    return lo;
}

struct lobject* store_make_object(struct store_tx *tx, object_id parent_id) {
    printf("## store_make_object %li\n", parent_id);
    struct store *s = tx->store;
    pthread_mutex_lock(&s->cache_latch);
    struct object *obj = obj_new();
    obj_set_id(obj, s->alloc_id++);
    obj_add_parent(obj, parent_id);
    struct lock *l = lock_new(s->locks_ctx);
    struct lobject *lo = lobject_new();
    lobject_set_object(lo, obj);
    lobject_set_lock(lo, l);
    cache_put_object(s->cache, lo);
    printf("### tx %lX locking %li EXCLUSIVE\n", tx, obj_get_id(lobject_get_object(lo)));
    lock_lock(lobject_get_lock(lo), LOCK_EXCLUSIVE, tx);
    pthread_mutex_unlock(&s->cache_latch);
    printf("##   -> %li\n", obj_get_id(obj));

    // put in tx to release later
    // XXX refactor into own method
    struct lobject_list_node *list_node = malloc(sizeof(struct lobject_list_node));
    list_node->lo = lo;
    list_node->next = tx->locked;
    tx->locked = list_node;

    return lo;
}


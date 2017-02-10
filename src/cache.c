#include "cache.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

// XXX this is a stub that does not lru-purge and that is based
// on a small static vector instead of a search tree. only to get simple
// cases going. no hash collision resistance either

#define CACHE_SIZE      1024

// -------- implementation of declared public structures --------

struct cache {
    struct lobject* objects[CACHE_SIZE];
};

// -------- implementation of public functions --------

struct cache* cache_new(int size) {
    struct cache *ret = malloc(sizeof(struct cache));
    memset(ret->objects, 0, sizeof(struct lobject*) * CACHE_SIZE);
    return ret;
}

void cache_free(struct cache *c) {
    free(c);
}

struct lobject* cache_get_object(struct cache *c, object_id id) {
    struct lobject *ret = c->objects[id];
    assert(ret != NULL);
    assert(obj_get_id(lobject_get_object(ret)) == id);
    return ret;
}

void cache_put_object(struct cache *c, struct lobject *o) {
    // XXX check for collision
    object_id id = obj_get_id(lobject_get_object(o));
    assert(c->objects[id] == NULL);
    c->objects[id] = o;
}

void cache_release_object(struct cache *c, struct lobject *o) {
    // XXX
}

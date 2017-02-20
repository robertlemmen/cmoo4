#include "cache.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

// XXX this is a stub that does not do deal with any of the hard cases. it will
// break once you have more data

#define CACHE_SIZE      1024

// -------- implementation of declared public structures --------

struct cache {
    struct lobject** objects;
};

// -------- implementation of public functions --------

struct cache* cache_new(int initial_size) {
    struct cache *ret = malloc(sizeof(struct cache));
    ret->objects = malloc(sizeof(struct lobject*) * initial_size);
    memset(ret->objects, 0, sizeof(struct lobject*) * initial_size);
    return ret;
}

void cache_free(struct cache *c) {
    free(c->objects);
    free(c);
}

struct lobject* cache_get_object(struct cache *c, object_id id) {
    struct lobject *ret = c->objects[id];
    if (ret == NULL) {
        return ret;
    }
    assert(obj_get_id(lobject_get_object(ret)) == id);
    lobject_pin(ret);
    return ret;
}

void cache_put_object(struct cache *c, struct lobject *o) {
    object_id id = obj_get_id(lobject_get_object(o));
    assert(c->objects[id] == NULL);
    lobject_pin(o);
    c->objects[id] = o;
}

void cache_release_object(struct cache *c, struct lobject *o) {
    lobject_unpin(o);
}

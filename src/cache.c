#include "cache.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* when this proportion of slots are used, we resize the hashtable */
#define LOAD_FACTOR 0.7
/* if we grow the table, this is how many times larger the new one is 
 * going to be */
#define GROW_FACTOR 2
/* if we have more than this many items (times hash table size), we start 
 * removing old and unpinned ones */
#define CHAIN_FACTOR 2

// -------- implementation of declared public structures --------

struct cache_entry {
    struct lobject *object;
    struct cache_entry *list_next;
    struct cache_entry *newer;
    struct cache_entry *older;
};

struct cache {
    struct cache_entry **table;
    struct cache_entry *newest;
    struct cache_entry *oldest;
    int size;
    int slots_used;
    int entries;
};

// -------- internal functions --------

void cache_resize(struct cache *c) {
    int new_size = c->size * GROW_FACTOR;
    struct cache_entry **new_table = malloc(sizeof(struct cache_entry*) * new_size);
    memset(new_table, 0, sizeof(struct cache_entry*) * new_size);

    // XXX implement movement of old items across
    assert(false);

    c->table = new_table;
    c->size = new_size;    
}

// -------- implementation of public functions --------

struct cache* cache_new(int initial_size) {
    struct cache *ret = malloc(sizeof(struct cache));
    ret->table = malloc(sizeof(struct cache_entry*) * initial_size);
    ret->size = initial_size;
    ret->slots_used = 0;
    ret->entries = 0;
    ret->newest = NULL;
    ret->oldest = NULL;
    memset(ret->table, 0, sizeof(struct cache_entry*) * initial_size);
    return ret;
}

void cache_free(struct cache *c) {
    free(c->table);
    while (c->oldest) {
        struct cache_entry *ce = c->oldest;
        c->oldest = ce->newer;
        // XXX free object?
        free(ce);
    }
    free(c);
}

struct lobject* cache_get_object(struct cache *c, object_id id) {
    struct cache_entry *ce = c->table[id % c->size];
    while (ce != NULL) {
        if (obj_get_id(lobject_get_object(ce->object)) == id) {
            lobject_pin(ce->object);

            // remove from recently-used linkedlist
            if (c->newest == ce) {
                c->newest = ce->older;
            }
            if (c->oldest == ce) {
                c->oldest = ce->newer;
            }
            if (ce->older != NULL) {
                ce->older->newer = ce->newer;
            }
            if (ce->newer != NULL) {
                ce->newer->older = ce->older;
            }
            ce->older = NULL;
            ce->newer = NULL;

            return ce->object;
        }
        ce = ce->list_next;
    }
    return NULL;
}

void cache_put_object(struct cache *c, struct lobject *o) {
    object_id id = obj_get_id(lobject_get_object(o));
    struct cache_entry *ce = c->table[id % c->size];
    struct cache_entry *ne = malloc(sizeof(struct cache_entry));
    c->entries++;
    ne->object = o;
    ne->list_next = ce;
    ne->newer = NULL;
    ne->older = NULL;
    c->table[id % c->size] = ne;
    lobject_pin(o);
    if (ce == NULL) {
        c->slots_used++;
    }
    if (c->slots_used > c->size * LOAD_FACTOR) {
        cache_resize(c);
    }
    // XXX newer and older are never set...
}

void cache_release_object(struct cache *c, struct lobject *o) {
    lobject_unpin(o);
    object_id id = obj_get_id(lobject_get_object(o));
    struct cache_entry *ce = c->table[id % c->size];
    while (ce != NULL) {
        if (obj_get_id(lobject_get_object(ce->object)) == id) {
            // this is the object we are looking for, splice it into
            // recently-used linkedlist
            ce->newer = NULL;
            ce->older = c->newest;
            c->newest = ce;
            if (c->oldest == NULL) {
                c->oldest = ce;
            }
            break;
        }
        ce = ce->list_next;
    }
    assert(ce != NULL);

    while ((c->entries > c->size * CHAIN_FACTOR) && (c->oldest)) {
        ce = c->oldest;
        c->oldest = ce->newer;
        if (c->oldest == NULL) {
            c->newest = NULL;
        }
        else {
            c->oldest->older = NULL;
        }
        free(ce);
        c->entries--;
    }
}

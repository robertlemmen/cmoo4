#ifndef CACHE_H
#define CACHE_H

#include "defs.h"
#include "lobject.h"

/* this implements a LRU-cache of objects. It does reference counting so that
 * it does not throw away objects that are being used at the moment. it does 
 * not do any locking itself, and needs to be wrapped if concurrent access is 
 * required */

struct cache;

struct cache* cache_new(int initial_size);
void cache_free(struct cache *c);

// get object from cache, increase refcount by one. you need to release() the object
// so that it can be purged from cache
struct lobject* cache_get_object(struct cache *c, object_id id);
// put object in cache, it must not be there already. set refcount to 1
void cache_put_object(struct cache *c, struct lobject *o);
// decrease refcount, objects with refcount == 0 are eligible for garbage collection
// from cache
void cache_release_object(struct cache *c, struct lobject *o);

#endif /* CACHE_H */

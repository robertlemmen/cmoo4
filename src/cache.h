#ifndef CACHE_H
#define CACHE_H

#include "defs.h"
#include "lobject.h"

/* this implements a LRU-cache of objects. It does reference counting so that
 * it does not throw away objects that are being used at the moment. it does 
 * not do any locking itself, and needs to be wrapped if concurrent access is 
 * required */

// XXX how do we link objects and locks? have an object wrapper? or put the 
// locks inside objects? We could make this more generic by providing a 
// item_free() callback that is used when an item is purged from the cache, and
// just using void* or create a locked_object which wraps the object, optional source
// and the locks into one thing, which is then used in here

struct cache;

struct cache* cache_new(int size);
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

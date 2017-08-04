#ifndef CACHE_H
#define CACHE_H

#include "defs.h"
#include "lobject.h"

/* this implements a simple cache of objects. objects are pinned in the cache
 * until released, which means they will not be removed from the cache until
 * released. */

struct cache;

struct cache* cache_new(int initial_size);
void cache_free(struct cache *c);

// get object from cache, increase refcount by one. you need to release() the object
// so that it can be purged from cache. return NULL if not found.
struct lobject* cache_get_object(struct cache *c, object_id id);
// put object in cache, it must not be there already. sets to pinned as well
void cache_put_object(struct cache *c, struct lobject *o);
// unpin item so that it can be replaced in the cache
void cache_release_object(struct cache *c, struct lobject *o);

#endif /* CACHE_H */

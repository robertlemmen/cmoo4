#ifndef STORE_H
#define STORE_H

#include "defs.h"
#include "persist.h"

/* this store sits on top of a persistent backend and allows concurrent access
 * to object. The objects are cached, and the store manages locks against the
 * objects. So only one calling transaction can have access to an object at
 * a point in time. When an object is curretnly locked, access to it is 
 * blocked until that lock is released. The underlying locking system ensures
 * that deadlocks are detected though and reports these 
 */

struct store;
struct store_tx;

struct store* store_new(struct persist *p);
void store_free(struct store *s);

/* start/finish a transaction, you need to explicitely finish a transaction even 
 * if it failed */
struct store_tx* store_start_tx(struct store *s);
void store_finish_tx(struct store_tx *tx);

/* get an object from the store, returns NULL if the object is not found or if
 * a deadlock occured */
// XXX do we need to be able to distinguish between the two? surprisingly we might 
// not: no-such-object should never happen and would be fatal as well..
struct lobject* store_get_object(struct store_tx *tx, object_id oid);

/* create a new, empty object with an initial parent link. the id is allocated */
struct lobject* store_make_object(struct store_tx *tx, object_id parent_id);

// XXX we need a way to write to an object, but only have the writable copy in the TX
// until comitted

#endif /* STORE_H */

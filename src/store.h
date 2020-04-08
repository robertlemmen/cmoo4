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

struct store* store_new(struct persist *p, int max_tasks);
void store_free(struct store *s);

/* start/finish a transaction, you need to explicitely finish a transaction even 
 * if it failed */
struct store_tx* store_start_tx(struct store *s);
void store_finish_tx(struct store_tx *tx);
// the sid of a tx is a sequential id that can be used to determine the younger
// transaction for e.g. deadlocks. it might eventually wrap around but that's
// going to be a while and is ok even then 
uint64_t store_tx_get_sid(struct store_tx *tx);
// the cid is a small id bound by max_tasks and will be reused eagerly, it can
// be used to e.g. represent the transaction through an index into an array-based 
// wait-for-graph
int store_tx_get_cid(struct store_tx *tx);

#ifdef TESTABILITY_FEATURES
// these are for locking unit tests only, so you can create store_tx with sid/cid 
// but without connection to an actual store.
struct store_tx *store_new_mock_tx(uint64_t sid, int cid);
void store_free_mock_tx(struct store_tx *tx);
#endif

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

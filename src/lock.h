#ifndef LOCK_H
#define LOCK_H

/* this is recursive (same thread can take the same lock multiple times), fair,
 * R/W, upgrading (R->W) and deadlock-detecting lock implementation */

/* return values from lock_lock */
#define LOCK_TAKEN      0
#define LOCK_DEADLOCK   1

/* locking modes */
// XXX perhaps shared/exclusive?
#define LOCK_READ       0
#define LOCK_WRITE      1

struct lock;
struct store_tx;

struct lock* lock_new(void);
void lock_free(struct lock *l);

/* these two take a opaque tx argument that is used to identify the transaction
 * that wants the lock, this module never looks into it and treats it as a
 * void pointer. */
// XXX a small and reused tx integer would make things so much easier...
int lock_lock(struct lock *l, int lock_mode, struct store_tx *tx);
void lock_unlock(struct lock *l, struct store_tx *tx);

#endif /* LOCK_H */

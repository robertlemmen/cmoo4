#ifndef LOCK_H
#define LOCK_H

/* this is recursive (same thread can take the same lock multiple times), fair,
 * R/W, upgrading (R->W) and deadlock-detecting lock implementation */

/* return values from lock_lock, LOCK_TAKEN is success, everything else a
 * failure */
#define LOCK_TAKEN      0
#define LOCK_DEADLOCK   1
#define LOCK_STALE      2

/* locking modes */
#define LOCK_SHARED       0
#define LOCK_EXCLUSIVE    1

/* XXX we will need some sort of locking_ctx for the deadlock detector */

struct lock;
struct store_tx;

struct lock* lock_new(void);
void lock_free(struct lock *l);

/* these two take a opaque tx argument that is used to identify the transaction
 * that wants the lock, this module never looks into it and treats it as a
 * void pointer. */
// XXX a small and reused tx integer would make things so much easier, would
// also mean we need an upper bound on the number of concurrent TXes, which we
// kinda have anyway, just needs setting up...
int lock_lock(struct lock *l, int lock_mode, struct store_tx *tx);
void lock_unlock(struct lock *l, struct store_tx *tx);

#endif /* LOCK_H */

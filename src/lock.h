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

/* locks are not independent from each other due to the deadlock detector, so
 * they need to be constructed over a central locking support structure */
struct locks_ctx;

struct lock;
struct store_tx;

struct locks_ctx* locks_new_ctx(int max_tasks);
void locks_free_ctx(struct locks_ctx *ctx);

struct lock* lock_new(struct locks_ctx *ctx);
void lock_free(struct lock *l);

// XXX this comment wont be true much longer, we will need  to look into the TX
// for deadlock detection...
/* these two take a opaque tx argument that is used to identify the transaction
 * that wants the lock, this module never looks into it and treats it as a
 * void pointer. */
int lock_lock(struct lock *l, int lock_mode, struct store_tx *tx);
void lock_unlock(struct lock *l, struct store_tx *tx);

#endif /* LOCK_H */

#ifndef LOCK_H
#define LOCK_H

/* this subsystem implements fair and deadlock-detecting locks that can be used
 * in blocking and non-blocking mode. in blocking mode the attempt to lock it
 * may block just like a mutex, in non-blocking mode it just returns an object 
 * that can be used to query the current status of the requested reservation. this
 * still means this thread will get served before a later reservation or locking 
 * attempt */

// XXX upgrade to excl/shared lock
// XXX non-blocking access should allow async callback, which is tricky around race
// conditions

#define LOCK_FREE       0
#define LOCK_RESERVED   1
#define LOCK_TAKEN      2
#define LOCK_DEADLOCK   3

struct lock;
struct lock_reservation;

struct lock* lock_new(void);
void lock_free(struct lock *l);

// blocking access
int lock_lock(struct lock *l);
void lock_unlock(struct lock *l);

// non-blocking access
struct lock_reservation* lock_reserve(struct lock *l);
int lock_reservation_status(struct lock_reservation *lr);
int lock_reservation_wait(struct lock_reservation *lr);
void lock_reservation_unlock(struct lock_reservation *lr);

#endif /* LOCK_H */

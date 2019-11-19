#include "lock.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include <string.h>

// -------- internal structures ------------

struct lock_waitgroup {
    int mode;
    pthread_cond_t sema;
    struct store_tx **entries;
    int entry_count;
    struct lock_waitgroup *next;
};

// -------- implementation of declared public structures --------

struct lock {
    pthread_mutex_t latch;
    // the first wait group is who is currently holding the lock, the chain
    // from there are the ones waiting
    struct lock_waitgroup *first_wait_group;
    struct lock_waitgroup *last_wait_group;
};

// -------- internal functions ---------

struct lock_waitgroup* lock_waitgroup_new(int lock_mode, struct store_tx *tx) {
    struct lock_waitgroup *nwg = malloc(sizeof(struct lock_waitgroup));
    nwg->mode = lock_mode;
    if (pthread_cond_init(&nwg->sema, NULL) != 0) {
        fprintf(stderr, "pthread_cond_init failed\n");
        exit(1);
    }
    nwg->entry_count = 1;
    nwg->entries = malloc(sizeof(struct store_tx*) * nwg->entry_count);
    nwg->entries[0] = tx;
    nwg->next = NULL;
    return nwg;
}

// -------- implementation of public functions --------

struct lock* lock_new(void) {
    struct lock *ret = malloc(sizeof(struct lock));
    if (pthread_mutex_init(&ret->latch, NULL) != 0) {
        fprintf(stderr, "pthread_mutex_init failed\n");
        exit(1);
    }
    ret->first_wait_group = NULL;
    ret->last_wait_group = NULL;
    return ret;
}

void lock_free(struct lock *l) {
    if (l->first_wait_group) {
        fprintf(stderr, "fatal: lock_free with waiting transactions\n");
        exit(1);
    }
    pthread_mutex_destroy(&l->latch);
    free(l);
}

int lock_lock(struct lock *l, int lock_mode, struct store_tx *tx) {
    // XXX for now we only do exclusive locks, this is easier and functionally
    // ok, just more contention. once that works, future changes can improve on
    // this
    lock_mode = LOCK_EXCLUSIVE;
    pthread_mutex_lock(&l->latch);

    // case A: if the lock has no wait groups, just create one and we have the lock
    if (l->first_wait_group == NULL) {
        struct lock_waitgroup *nwg = lock_waitgroup_new(lock_mode, tx);
        l->first_wait_group = nwg;
        l->last_wait_group = nwg;
        pthread_mutex_unlock(&l->latch);
        return LOCK_TAKEN;
    }

    // case B: if we already hold the lock in the same or higher mode, just return
    // we actually want the same mode or a "higher" one, exclusive > shared,
    // but don't want to rely on the enum ordering
    if ((l->first_wait_group->mode == lock_mode) || (l->first_wait_group->mode == LOCK_EXCLUSIVE)) {
        for (int i = 0; i < l->first_wait_group->entry_count; i++) {
            if (l->first_wait_group->entries[i] == tx) {
                pthread_mutex_unlock(&l->latch);
                return LOCK_TAKEN;
            }
        }
    }

    // case C: if there is a waitgroup, and the last one is read-only, and this tx wants read-only,
    // just join and wait
    // XXX not at the moment as all our locks are exclusive
    assert(lock_mode == LOCK_EXCLUSIVE);

    // case D / otherwise: add a new waitgroup to the end, wait
    // XXX creation is the same as above, refactor
    struct lock_waitgroup *nwg = lock_waitgroup_new(lock_mode, tx);
    l->last_wait_group->next = nwg;
    l->last_wait_group = nwg;
    // now wait for the lock to be available
    do {
        pthread_cond_wait(&nwg->sema, &l->latch);
    } while (nwg != l->first_wait_group);
    pthread_mutex_unlock(&l->latch);
    return LOCK_TAKEN;
}

void lock_unlock(struct lock *l, struct store_tx *tx) {
    pthread_mutex_lock(&l->latch);

    // because of the recursive nature of the lock, it is possible that there
    // isn't a waitgroup at all...
    if (!l->first_wait_group) {
        pthread_mutex_unlock(&l->latch);
        return;
    }

    // find our position in the first wait group
    int found_idx = -1;
    for (int i = 0; i < l->first_wait_group->entry_count; i++) {
        if (l->first_wait_group->entries[i] == tx) {
            found_idx = i;
            break;
        }
    }
    // due to the recursive nature, it is possible that we do not actually hold
    // the lock anymore
    if (found_idx == -1) {
        pthread_mutex_unlock(&l->latch);
        return;
    }

    // remove ourselves from the waitgroup;
    memmove(&l->first_wait_group->entries[found_idx], &l->first_wait_group->entries[found_idx+1],
        (l->first_wait_group->entry_count - found_idx - 1) * sizeof(struct store_tx*));
    l->first_wait_group->entry_count--;

    // check if the waitgroup is now empty
    if (!l->first_wait_group->entry_count) {
        struct lock_waitgroup *old = l->first_wait_group;
        l->first_wait_group = old->next;
        if (!l->first_wait_group) {
            l->last_wait_group = NULL;
        }
        pthread_cond_destroy(&old->sema);
        free(old->entries);
        free(old);
        // now we can wake the threads in the next wait group
        if (l->first_wait_group) {
            pthread_cond_broadcast(&l->first_wait_group->sema);
        }
    }

    pthread_mutex_unlock(&l->latch);
}


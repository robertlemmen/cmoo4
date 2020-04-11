#include "lock.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include "store.h"

// -------- internal structures ------------

struct lock_waitgroup {
    int mode;
    pthread_cond_t sema;
    struct store_tx **entries;
    int entry_count;
    struct lock_waitgroup *next;
    struct store_tx *deadlocked;
};

// -------- implementation of declared public structures --------

struct locks_ctx {
    int max_tasks;
    pthread_mutex_t deadlock_latch;
    bool *wfg_matrix;   // we simply represent the wait-for-graph as a matrix
                        // where a TRUE in [x,y] means X is waiting for Y. not
                        // very compact or clever but there you go. this is a
                        // flat representation, but max_tasks sized
    struct store_tx **tx_by_cid;
    struct lock_waitgroup **blocked_waitgroup_by_cid;
    struct lock **blocked_lock_by_cid; // the lock that the waitgroups above are in
};

struct lock {
    struct locks_ctx *ctx;
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
    nwg->deadlocked = NULL;
    return nwg;
}

void lock_waitgroup_free(struct lock_waitgroup *wg) {
    pthread_cond_destroy(&wg->sema);
    free(wg->entries);
    free(wg);
}

// we return this from a dfs recursion. if tx set NULL, then there is no cycle.
// otherwise the fields are all filled in with the candidate we want to fault
// out of the cycle, i.e. the youngest by sid
struct wfg_result {
    struct store_tx *tx;
    int cid;
    uint64_t sid;
};

// this is the core deadlock detector, we DFS the wait-for-graph and return
// whether there is a cycle, and if so which transaction should be faulted out
// of the deadlock, the yougest by sid
struct wfg_result wfg_dfs(struct locks_ctx *ctx, int cid, int root_cid) {
    struct wfg_result res;
    for (int y = 0; y < ctx->max_tasks; y++) {
        if (ctx->wfg_matrix[cid + y * ctx->max_tasks]) {
            if (y == root_cid) {
                res.cid = y;
                res.tx = ctx->tx_by_cid[y];
                res.sid = store_tx_get_sid(res.tx);
                return res;
            }
            else {
                struct wfg_result rec_res = wfg_dfs(ctx, y, root_cid);
                if (rec_res.tx) {
                    res.cid = y;
                    res.tx = ctx->tx_by_cid[y];
                    res.sid = store_tx_get_sid(res.tx);
                    if (rec_res.sid < res.sid) {
                        return res;
                    }
                    else {
                        return rec_res;
                    }
                }
            }
        }
    }
    res.tx = NULL;
    return res;
}

// -------- implementation of public functions --------

struct locks_ctx* locks_new_ctx(int max_tasks) {
    struct locks_ctx *ret = malloc(sizeof(struct locks_ctx));
    ret->max_tasks = max_tasks;
    if (pthread_mutex_init(&ret->deadlock_latch, NULL) != 0) {
        fprintf(stderr, "pthread_mutex_init failed\n");
        exit(1);
    }
    ret->wfg_matrix = malloc(sizeof(bool) * max_tasks * max_tasks);
    memset(ret->wfg_matrix, 0, sizeof(bool) * max_tasks * max_tasks);
    ret->tx_by_cid = malloc(sizeof(struct store_tx*) * max_tasks);
    ret->blocked_waitgroup_by_cid = malloc(sizeof(struct lock_waitgroup*) * max_tasks);
    ret->blocked_lock_by_cid = malloc(sizeof(struct lock*) * max_tasks);
    return ret;
}

void locks_free_ctx(struct locks_ctx *ctx) {
    pthread_mutex_destroy(&ctx->deadlock_latch);
    for (int x = 0; x < ctx->max_tasks; x++) {
        for (int y = 0; y < ctx->max_tasks; y++) {
            assert(!ctx->wfg_matrix[x + y * ctx->max_tasks]);
        }
    }
    free(ctx->wfg_matrix);
    free(ctx->tx_by_cid);
    free(ctx->blocked_waitgroup_by_cid);
    free(ctx->blocked_lock_by_cid);
    free(ctx);
}

struct lock* lock_new(struct locks_ctx *ctx) {
    struct lock *ret = malloc(sizeof(struct lock));
    if (pthread_mutex_init(&ret->latch, NULL) != 0) {
        fprintf(stderr, "pthread_mutex_init failed\n");
        exit(1);
    }
    ret->ctx = ctx;
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
    // just join the group and return
    if ((lock_mode == LOCK_SHARED) && (l->last_wait_group->mode == LOCK_SHARED)) {
        struct lock_waitgroup *lwg = l->last_wait_group;
        lwg->entry_count++;
        lwg->entries = realloc(lwg->entries, sizeof(struct store_tx*) * lwg->entry_count);
        lwg->entries[lwg->entry_count-1] = tx;
        if (lwg != l->first_wait_group) {
            // XXX ah darn, this needs to deal with possible deadlock faults
            // as well, perhaps refactor the wait out of lock_lock()
            // we need to wait for that waitgroup to become active
            do {
                pthread_cond_wait(&lwg->sema, &l->latch);
            } while (lwg != l->first_wait_group);
        }
        // otherwise we joined an already active waitgroup, hoorah
        pthread_mutex_unlock(&l->latch);
        return LOCK_TAKEN;
    }

    // case D: if we already hold a shared lock and now want to have an
    // exclusive lock, we can upgrade under certain circumstances
    if ((lock_mode == LOCK_EXCLUSIVE) && (l->first_wait_group->mode == LOCK_SHARED)) {
        for (int i = 0; i < l->first_wait_group->entry_count; i++) {
            if (l->first_wait_group->entries[i] == tx) {
                // great, this is a candidate for upgrading the lock!
                if (l->first_wait_group != l->last_wait_group) {
                    // ah darn, someone else has come between the read lock that
                    // this tx already has, and the attempt to upgrade it. this
                    // lock request inbetween is a exclusive one because
                    // otherwise it would have been added to the current first
                    // wait group. therefore...
                    pthread_mutex_unlock(&l->latch);
                    return LOCK_STALE;
                }
                else {
                    // great, we can upgrade the lock!
                    if (l->first_wait_group->entry_count == 1) {
                        // even better, we can upgrade right away
                        l->first_wait_group->mode = LOCK_EXCLUSIVE;
                        pthread_mutex_unlock(&l->latch);
                        return LOCK_TAKEN;
                    }
                    else {
                        // we need to enqueue and wait for that waitgroup to
                        // only have the tx in question in it. lock_unlock() below
                        // will take care of that.
                        // Interestingly this does not require implementation,
                        // the implicit fall-through to case E below is
                        // exactly the right thing
                    }
                }
            }
        }
    }

    // case E / otherwise: add a new waitgroup to the end, wait
    struct lock_waitgroup *nwg = lock_waitgroup_new(lock_mode, tx);
    l->last_wait_group->next = nwg;
    l->last_wait_group = nwg;

    // we will be blocked, so we now inform the deadlock detector that the
    // wait-for-graph has changed
    pthread_mutex_lock(&l->ctx->deadlock_latch);
    int tx_cid = store_tx_get_cid(tx);
    for (int i = 0; i < l->first_wait_group->entry_count; i++) {
        // it is possible that the same tx shows up here, because there could
        // be another waitgroup inbetween
        if (tx != l->first_wait_group->entries[i]) {
            l->ctx->wfg_matrix[tx_cid + store_tx_get_cid(l->first_wait_group->entries[i])
                                        * l->ctx->max_tasks] = true;
        }
    }
    l->ctx->tx_by_cid[tx_cid] = tx;
    l->ctx->blocked_waitgroup_by_cid[tx_cid] = nwg;
    l->ctx->blocked_lock_by_cid[tx_cid] = l;

    // and of course we need to consult the updated wait-for-graph via a DFS to
    // figure out whether there is a deadlock
    struct wfg_result wfg_res = wfg_dfs(l->ctx, tx_cid, tx_cid);

    if (wfg_res.tx) {
        // we have indeed found a deadlock, so let's mark it and wake up the
        // corresponding waitgroup
        struct lock_waitgroup *fwg = l->ctx->blocked_waitgroup_by_cid[wfg_res.cid];
        struct lock *fl = l->ctx->blocked_lock_by_cid[wfg_res.cid];
        // it is safe to lock another's lock latch while holding this one only
        // because we are inside the deadlock latch, which guarantees that this
        // does not happen concurrently in the opposite direction
        if (fl != l) {
            pthread_mutex_lock(&fl->latch);
        }
        // XXX for now we only support a single deadlocked transaction per waitgroup,
        // which is kinda broken
        fwg->deadlocked = wfg_res.tx;
        pthread_cond_broadcast(&fwg->sema);
        if (fl != l) {
            pthread_mutex_unlock(&fl->latch);
        }
    }
    pthread_mutex_unlock(&l->ctx->deadlock_latch);

    // now wait for the lock to be available or for this tx to be marked as
    // deadlocked
    while ((nwg != l->first_wait_group) && (nwg->deadlocked != tx)) {
        pthread_cond_wait(&nwg->sema, &l->latch);
    }

    if (nwg->deadlocked == tx) {
        // remove ourselves from the waitgroup
        int found_idx = -1;
        for (int i = 0; i < nwg->entry_count; i++) {
            if (nwg->entries[i] == tx) {
                found_idx = i;
                break;
            }
        }
        memmove(&nwg[found_idx], &nwg->entries[found_idx+1],
            (nwg->entry_count - found_idx - 1) * sizeof(struct store_tx*));
        nwg->entry_count--;
        // now we only need to clean up the waitgroup if it is empty
        if (nwg->entry_count == 0) {
            // find the previous waitgroup
            struct lock_waitgroup *prev = l->first_wait_group;
            while (prev && (prev->next != nwg)) {
                prev = prev->next;
            }
            assert(prev->next == nwg);
            prev->next = nwg->next;
            lock_waitgroup_free(nwg);
        }
        nwg->deadlocked = NULL;

        pthread_mutex_unlock(&l->latch);
        return LOCK_DEADLOCK;
    }
    else {
        // regular case: we finally have the lock!
        pthread_mutex_unlock(&l->latch);
        return LOCK_TAKEN;
    }
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

    // we will need this for the deadlock detector later, but it's easier to get
    // it now because we have not yet and conditionally removed the first
    // waitgroup
    struct lock_waitgroup *cwg = l->first_wait_group->next;

    // check if the waitgroup is now empty
    if (l->first_wait_group->entry_count == 0) {
        struct lock_waitgroup *old = l->first_wait_group;
        l->first_wait_group = old->next;
        if (!l->first_wait_group) {
            l->last_wait_group = NULL;
        }
        lock_waitgroup_free(old);
        // now we can wake the threads in the next wait group
        if (l->first_wait_group) {
            pthread_cond_broadcast(&l->first_wait_group->sema);
        }
    }
    else if (l->first_wait_group->entry_count == 1) {
        // upgrade case: if there is only one entry left, and this is shared,
        // and the next is exclusive and that has the same tx in it, just upgrade to
        // the next waitgroup
        if (       (l->first_wait_group->mode == LOCK_SHARED)
                && (l->first_wait_group->next)
                && (l->first_wait_group->next->mode == LOCK_EXCLUSIVE)
                && (l->first_wait_group->entries[0] == l->first_wait_group->next->entries[0]) ) {
            struct lock_waitgroup *old = l->first_wait_group;
            l->first_wait_group = old->next;
            if (!l->first_wait_group) {
                l->last_wait_group = NULL;
            }
            lock_waitgroup_free(old);
            pthread_cond_broadcast(&l->first_wait_group->sema);
        }
    }

    // we now need to tell the deadlock detector that the wait-for-graph has
    // changed
    pthread_mutex_lock(&l->ctx->deadlock_latch);
    while (cwg) {
        for (int i = 0; i < cwg->entry_count; i++) {
            l->ctx->wfg_matrix[store_tx_get_cid(cwg->entries[i])
                               + store_tx_get_cid(tx)
                                 * l->ctx->max_tasks] = true;
        }
        cwg = cwg->next;
    }
    pthread_mutex_unlock(&l->ctx->deadlock_latch);

    pthread_mutex_unlock(&l->latch);
}


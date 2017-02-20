#include "lock.h"

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

// XXX stub, we want to be fair and detect deadlocks!

// -------- implementation of declared public structures --------

struct lock {
    pthread_mutex_t latch;
};

// -------- implementation of public functions --------

struct lock* lock_new(void) {
    struct lock *ret = malloc(sizeof(struct lock));
    if (pthread_mutex_init(&ret->latch, NULL) != 0) {
        fprintf(stderr, "pthread_mutex_init failed\n");
        exit(1);
    }
    return ret;
}

void lock_free(struct lock *l) {
    // XXX make sure it's not locked, and that there is noone waiting
    pthread_mutex_destroy(&l->latch);
    free(l);
}

int lock_lock(struct lock *l) {
    pthread_mutex_lock(&l->latch);
    return LOCK_TAKEN;
}

void lock_unlock(struct lock *l) {
    pthread_mutex_unlock(&l->latch);
}

/* old implementation, remove once happy with new one 

// XXX this is a stub, it does not detect deadlocks at this point, and does
// not allow shared/exclusive access

// -------- internal structures --------

struct lock_waiting {
    pthread_cond_t sema;
    struct lock_waiting *next;
};

// -------- implementation of declared public structures --------

struct lock {
    struct lock_waiting *wait_first;
    struct lock_waiting *wait_last;
    pthread_mutex_t latch;
};

struct lock_reservation {
    struct lock *l;
    struct lock_waiting *w;
};

// -------- implementation of public functions --------

struct lock* lock_new(void) {
    struct lock *ret = malloc(sizeof(struct lock));
    if (pthread_mutex_init(&ret->latch, NULL) != 0) {
        fprintf(stderr, "pthread_mutex_init failed\n");
        exit(1);
    }
    ret->wait_first = NULL;
    ret->wait_last = NULL;
    return ret;
}

void lock_free(struct lock *l) {
    // XXX make sure it's not locked, and that there is noone waiting
    pthread_mutex_destroy(&l->latch);
    free(l);
}

int lock_lock(struct lock *l) {
    pthread_mutex_lock(&l->latch);
    // create a new wait-struct and enqueue
    struct lock_waiting *w = malloc(sizeof(struct lock_waiting));
    w->next = NULL;
    if (pthread_cond_init(&w->sema, NULL) != 0) {
        fprintf(stderr, "pthread_cond_init failed\n");
        exit(1);
    }
    if (l->wait_last) {
        l->wait_last->next = w;
    }
    else {
        l->wait_last = w;
        l->wait_first = w;
    }
    // we need to wait until we get to the top of the queue
    while (w != l->wait_first) {
        pthread_cond_wait(&w->sema, &l->latch);
    }
    pthread_mutex_unlock(&l->latch);
    return LOCK_TAKEN;
}

void lock_unlock(struct lock *l) {
    pthread_mutex_lock(&l->latch);
    // we are at the head of the queue, otherwise this would not be reached
    // XXX (which should be asserted!
    struct lock_waiting *w = l->wait_first;
    l->wait_first = l->wait_first->next;
    if (!l->wait_first) {
        // noone waiting left
        l->wait_last = l->wait_first;
    }
    else {
        // wake up next one
        pthread_cond_signal(&l->wait_first->sema);
    }
    // clean up waiting struct
    pthread_cond_destroy(&w->sema);
    free(w);
    pthread_mutex_unlock(&l->latch);
}

struct lock_reservation* lock_reserve(struct lock *l) {
    struct lock_reservation *ret = malloc(sizeof(struct lock_reservation));
    ret->l = l;
    pthread_mutex_lock(&l->latch);
    // create a new wait-struct and enqueue
    struct lock_waiting *w = malloc(sizeof(struct lock_waiting));
    ret->w = w;
    w->next = NULL;
    if (pthread_cond_init(&w->sema, NULL) != 0) {
        fprintf(stderr, "pthread_cond_init failed\n");
        exit(1);
    }
    if (l->wait_last) {
        l->wait_last->next = w;
    }
    else {
        l->wait_last = w;
        l->wait_first = w;
    }
    // enqueued, but we do not have to wait at this point
    pthread_mutex_unlock(&l->latch);
    return ret;
}

int lock_reservation_status(struct lock_reservation *lr) {
    int ret;
    pthread_mutex_lock(&lr->l->latch);
    if (lr->w == lr->l->wait_first) {
        ret = LOCK_TAKEN;
    }
    else {
        ret = LOCK_RESERVED;
    }
    pthread_mutex_unlock(&lr->l->latch);
    return ret;
}

int lock_reservation_wait(struct lock_reservation *lr) {
    pthread_mutex_lock(&lr->l->latch);
    while (lr->w != lr->l->wait_first) {
        pthread_cond_wait(&lr->w->sema, &lr->l->latch);
    }
    pthread_mutex_unlock(&lr->l->latch);
    return LOCK_TAKEN;
}

void lock_reservation_unlock(struct lock_reservation *lr) {
    lock_unlock(lr->l);
    free(lr);
}
*/

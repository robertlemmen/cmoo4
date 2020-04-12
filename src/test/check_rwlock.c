#include "check_rwlock.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

#define TESTABILITY_FEATURES
#include "store.h"
#include "lock.h"

#define PHASE_SLEEP_NS  10000000

/* this whole scaff thing is a test fixture/framework to make it easier running
 * multiple threads and exercising the rwlock implementation. the basic idea is
 * that you configure it for a number of threads and a number of phases, and
 * then provide a single callback. the threads will step through the phases ib
 * kinda-lockstep (unless they are blocked are take too long). no hard
 * guarantees, but they will at least adhere to a minimum, syncronised delay
 * between phase steps. the callback can then do whatever the test requires, and
 * emits a single char as the phase x thread "result". these arer tallied up,
 * and can then be matched against an expectation */

struct scaff_ctx {
    int n_threads;
    int n_phases;
    char *results;  // threads x phases
    char (*test_func)(int t, int p, void *cb_arg);
    void *cb_arg;

    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int current_phase;
};

struct scaff_ctx* scaff_new_ctx(int n_threads, int n_phases,
        char (*test_func)(int t, int p, void *cb_arg), void *cb_arg) {
    struct scaff_ctx *ret = malloc(sizeof(struct scaff_ctx));
    ret->n_threads = n_threads;
    ret->n_phases = n_phases;
    ret->results = malloc(n_threads * n_phases);
    ret->test_func = test_func;
    ret->cb_arg = cb_arg;
    pthread_mutex_init(&ret->mutex, NULL);
    pthread_cond_init(&ret->cond, NULL);
    ret->current_phase = -1; // we have an extra phase to hold the threads in 
                             // initialisation
    memset(ret->results, '-', n_threads * n_phases);
    return ret;
}

void scaff_free_ctx(struct scaff_ctx *scaff) {
    pthread_cond_destroy(&scaff->cond);
    pthread_mutex_destroy(&scaff->mutex);
    free(scaff->results);
    free(scaff);
}

struct thread_arg {
    int thread_id;
    struct scaff_ctx *scaff;
};

void* scaff_run_thread_func(void *va) {
    struct thread_arg *arg = va;

    for (int want_phase = 0; want_phase < arg->scaff->n_phases; want_phase++) {
        pthread_mutex_lock(&arg->scaff->mutex);
        int loc_current_phase; 
        while (arg->scaff->current_phase < want_phase) {
            pthread_cond_wait(&arg->scaff->cond, &arg->scaff->mutex);
        }
        loc_current_phase = arg->scaff->current_phase;
        pthread_mutex_unlock(&arg->scaff->mutex);

        char result = arg->scaff->test_func(arg->thread_id, loc_current_phase, arg->scaff->cb_arg);

        pthread_mutex_lock(&arg->scaff->mutex);
        // test_func could have blocked longer than one phase, so need to get
        // current one again
        loc_current_phase = arg->scaff->current_phase;
        if (loc_current_phase > want_phase) {
            want_phase = loc_current_phase;
        }
        arg->scaff->results[arg->thread_id * arg->scaff->n_phases + loc_current_phase] = result; 

        pthread_mutex_unlock(&arg->scaff->mutex);
    }
    return NULL;
}

void scaff_run(struct scaff_ctx *scaff) {
    struct thread_arg *args = malloc(sizeof(struct thread_arg) * scaff->n_threads);
    pthread_t *threads = malloc(sizeof(pthread_t) * scaff->n_threads);
    for (int t = 0; t < scaff->n_threads; t++) {
        args[t].thread_id = t;
        args[t].scaff = scaff;
        if (pthread_create(&threads[t], NULL, &scaff_run_thread_func, &args[t]) != 0) {
            ck_abort_msg("Could not create thread");
        }
    }

    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = PHASE_SLEEP_NS;
    // step through the phases and wake up the other threads
    while (scaff->current_phase < scaff->n_phases) {
        if (nanosleep(&ts, NULL) == -1) {
            ck_abort_msg("interrupted in sleep");
        }
        pthread_mutex_lock(&scaff->mutex);
        scaff->current_phase++;
        pthread_cond_broadcast(&scaff->cond);
        pthread_mutex_unlock(&scaff->mutex);
    }

    void *retval;
    for (int t = 0; t < scaff->n_threads; t++) {
        if (pthread_timedjoin_np(threads[t], &retval, &ts) != 0) {
            // XXX mark as failure but go on executing? perhaps have the join as
            // a separate function and call after print_resutls?
            //ck_abort_msg("Could not join on thread");
        }
    }
}

void scaff_print_results(struct scaff_ctx *scaff) {
    printf("+-----");
    for (int p = 0; p < scaff->n_phases; p++) {
        printf("----");
    }
    printf("-+\n");
    printf("|     ");
    for (int p = 0; p < scaff->n_phases; p++) {
        printf("P%02i ", p);
    }
    printf(" |\n");
    for (int t = 0; t < scaff->n_threads; t++) {
        printf("| T%02i ", t);
        for (int p = 0; p < scaff->n_phases; p++) {
            printf(" %c  ", scaff->results[t * scaff->n_phases + p]);
        }
        printf(" |\n");
    }
    printf("+-");
    for (int p = 0; p < scaff->n_phases; p++) {
        printf("----");
    }
    printf("-----+\n");
}

char map_lock_ret(int code) {
    switch (code) {
        case LOCK_TAKEN:
            return 'T';
        case LOCK_DEADLOCK:
            return 'D';
        case LOCK_STALE:
            return 'S';
        default:
            return '?';
    }
}

void ck_scaff_assert(struct scaff_ctx *scaff, char *expected) {
    bool matches  =true;
    for (int t = 0; t < scaff->n_threads; t++) {
        for (int p = 0; p < scaff->n_phases; p++) {
            matches &= scaff->results[t * scaff->n_phases + p] == expected[t * scaff->n_phases + p];
        }
    }
    if (!matches) {
        for (int t = 0; t < scaff->n_threads; t++) {
            printf("  XXX ");
            for (int p = 0; p < scaff->n_phases; p++) {
                printf(" %c  ", expected[t * scaff->n_phases + p]);
            }
            printf(" |\n");
        }
        printf("+-");
        for (int p = 0; p < scaff->n_phases; p++) {
            printf("----");
        }
        printf("-----+\n");
    }
    ck_assert_msg(matches, "reported pattern does not match expectation");
}

// this is used to pass locks and transactions into the test functions as
// arrays, no real checking/safety on the number and contents!
struct tfunc_args {
    struct lock *locks[3];      // mind the limits!
    struct store_tx *txes[3];
};

/* does the rwlock work as a regular mutex? */
char tfunc01(int t, int p, void *arg) {
    struct tfunc_args *tfa = arg;
    if (t == 0) {
        if (p == 0) {
            return map_lock_ret(lock_lock(tfa->locks[0], LOCK_EXCLUSIVE, tfa->txes[t]));
        }
        else if (p == 2) {
            lock_unlock(tfa->locks[0], tfa->txes[t]);
            return 'U';
        }
    }
    else if (t == 1) {
        if (p == 1) {
            return map_lock_ret(lock_lock(tfa->locks[0], LOCK_EXCLUSIVE, tfa->txes[t]));
        }
        else if (p == 3) {
            lock_unlock(tfa->locks[0], tfa->txes[t]);
            return 'U';
        }
    }
    return '.';
}

START_TEST(test_rwlock_01) {
    printf("  test_rwlock_01...\n");

    struct tfunc_args tfa;
    struct locks_ctx *locks = locks_new_ctx(2);
    tfa.locks[0] = lock_new(locks);
    tfa.txes[0] = store_new_mock_tx(0, 0);
    tfa.txes[1] = store_new_mock_tx(1, 1);
    struct scaff_ctx *scaff = scaff_new_ctx(2, 4, &tfunc01, &tfa);

    scaff_run(scaff);
    scaff_print_results(scaff);

    ck_scaff_assert(scaff,
        "T.U."
        ".-TU");

    lock_free(tfa.locks[0]);
    store_free_mock_tx(tfa.txes[0]);
    store_free_mock_tx(tfa.txes[1]);
    scaff_free_ctx(scaff);
}
END_TEST

/* is the rwlock reentrant? */
char tfunc02(int t, int p, void *arg) {
    struct tfunc_args *tfa = arg;
    if (t == 0) {
        if ((p == 0) || (p == 1) || (p == 3)) {
            return map_lock_ret(lock_lock(tfa->locks[0], LOCK_SHARED, tfa->txes[t]));
        }
        else if (p == 2) {
            return map_lock_ret(lock_lock(tfa->locks[0], LOCK_EXCLUSIVE, tfa->txes[t]));
        }
        else if (p == 4) {
            lock_unlock(tfa->locks[0], tfa->txes[t]);
            return 'U';
        }
    }
    return '.';
}

START_TEST(test_rwlock_02) {
    printf("  test_rwlock_02...\n");

    struct tfunc_args tfa;
    struct locks_ctx *locks = locks_new_ctx(1);
    tfa.locks[0] = lock_new(locks);
    tfa.txes[0] = store_new_mock_tx(0, 0);
    struct scaff_ctx *scaff = scaff_new_ctx(1, 5, &tfunc02, &tfa);

    scaff_run(scaff);
    scaff_print_results(scaff);

    ck_scaff_assert(scaff,
        "TTTTU");

    lock_free(tfa.locks[0]);
    store_free_mock_tx(tfa.txes[0]);
    scaff_free_ctx(scaff);
}
END_TEST

/* can the lock be shared? */
char tfunc03(int t, int p, void *arg) {
    struct tfunc_args *tfa = arg;
    if (t == 0) {
        if (p == 0) {
            return map_lock_ret(lock_lock(tfa->locks[0], LOCK_SHARED, tfa->txes[t]));
        }
        else if (p == 1) {
            return map_lock_ret(lock_lock(tfa->locks[0], LOCK_EXCLUSIVE, tfa->txes[t]));
        }
        else if (p == 3) {
            lock_unlock(tfa->locks[0], tfa->txes[t]);
            return 'U';
        }
    }
    else if (t == 1) {
        if (p == 0) {
            return map_lock_ret(lock_lock(tfa->locks[0], LOCK_SHARED, tfa->txes[t]));
        }
        else if (p == 2) {
            lock_unlock(tfa->locks[0], tfa->txes[t]);
            return 'U';
        }
    }
    return '.';
}

START_TEST(test_rwlock_03) {
    printf("  test_rwlock_03...\n");

    struct tfunc_args tfa;
    struct locks_ctx *locks = locks_new_ctx(2);
    tfa.locks[0] = lock_new(locks);
    tfa.txes[0] = store_new_mock_tx(0, 0);
    tfa.txes[1] = store_new_mock_tx(1, 1);

    struct scaff_ctx *scaff = scaff_new_ctx(2, 4, &tfunc03, &tfa);

    scaff_run(scaff);
    scaff_print_results(scaff);

    ck_scaff_assert(scaff,
        "T-TU"
        "T.U.");

    lock_free(tfa.locks[0]);
    store_free_mock_tx(tfa.txes[0]);
    store_free_mock_tx(tfa.txes[1]);
    scaff_free_ctx(scaff);
}
END_TEST

/* two independent lock sequences after another */
char tfunc04(int t, int p, void *arg) {
    struct tfunc_args *tfa = arg;
    if (t == 0) {
        if (p == 0) {
            return map_lock_ret(lock_lock(tfa->locks[0], LOCK_SHARED, tfa->txes[t]));
        }
        else if (p == 1) {
            return map_lock_ret(lock_lock(tfa->locks[0], LOCK_EXCLUSIVE, tfa->txes[t]));
        }
        else if (p == 2) {
            lock_unlock(tfa->locks[0], tfa->txes[t]);
            return 'U';
        }
    }
    else if (t == 1) {
        if (p == 3) {
            return map_lock_ret(lock_lock(tfa->locks[0], LOCK_SHARED, tfa->txes[t]));
        }
        else if (p == 4) {
            return map_lock_ret(lock_lock(tfa->locks[0], LOCK_EXCLUSIVE, tfa->txes[t]));
        }
        else if (p == 5) {
            lock_unlock(tfa->locks[0], tfa->txes[t]);
            return 'U';
        }
    }
    return '.';
}

START_TEST(test_rwlock_04) {
    printf("  test_rwlock_04...\n");

    struct tfunc_args tfa;
    struct locks_ctx *locks = locks_new_ctx(2);
    tfa.locks[0] = lock_new(locks);
    tfa.txes[0] = store_new_mock_tx(0, 0);
    tfa.txes[1] = store_new_mock_tx(1, 1);
    struct scaff_ctx *scaff = scaff_new_ctx(2, 6, &tfunc04, &tfa);

    scaff_run(scaff);
    scaff_print_results(scaff);

    ck_scaff_assert(scaff,
        "TTU..."
        "...TTU");

    lock_free(tfa.locks[0]);
    store_free_mock_tx(tfa.txes[0]);
    store_free_mock_tx(tfa.txes[1]);
    scaff_free_ctx(scaff);
}
END_TEST

/* three threads that initially share the lock */
char tfunc05(int t, int p, void *arg) {
    struct tfunc_args *tfa = arg;
    if (t == 0) {
        if (p == 0) {
            return map_lock_ret(lock_lock(tfa->locks[0], LOCK_SHARED, tfa->txes[t]));
        }
        else if (p == 3) {
            lock_unlock(tfa->locks[0], tfa->txes[t]);
            return 'U';
        }
    }
    else if (t == 1) {
        if (p == 0) {
            return map_lock_ret(lock_lock(tfa->locks[0], LOCK_SHARED, tfa->txes[t]));
        }
        else if (p == 1) {
            return map_lock_ret(lock_lock(tfa->locks[0], LOCK_EXCLUSIVE, tfa->txes[t]));
        }
        else if (p == 4) {
            lock_unlock(tfa->locks[0], tfa->txes[t]);
            return 'U';
        }
    }
    else if (t == 2) {
        if (p == 0) {
            return map_lock_ret(lock_lock(tfa->locks[0], LOCK_SHARED, tfa->txes[t]));
        }
        else if (p == 2) {
            lock_unlock(tfa->locks[0], tfa->txes[t]);
            return 'U';
        }
    }
    return '.';
}

START_TEST(test_rwlock_05) {
    printf("  test_rwlock_05...\n");

    struct tfunc_args tfa;
    struct locks_ctx *locks = locks_new_ctx(3);
    tfa.locks[0] = lock_new(locks);
    tfa.txes[0] = store_new_mock_tx(0, 0);
    tfa.txes[1] = store_new_mock_tx(1, 1);
    tfa.txes[2] = store_new_mock_tx(2, 2);
    struct scaff_ctx *scaff = scaff_new_ctx(3, 5, &tfunc05, &tfa);

    scaff_run(scaff);
    scaff_print_results(scaff);

    ck_scaff_assert(scaff,
        "T..U."
        "T--TU"
        "T.U..");

    lock_free(tfa.locks[0]);
    store_free_mock_tx(tfa.txes[0]);
    store_free_mock_tx(tfa.txes[1]);
    store_free_mock_tx(tfa.txes[2]);
    scaff_free_ctx(scaff);
}
END_TEST

/* reentrant means we need to unlock a matching number of times */
/* XXX see TODO
char tfunc06(int t, int p, void *arg) {
    struct tfunc_args *tfa = arg;
    if (t == 0) {
        if (p == 0) {
            return map_lock_ret(lock_lock(tfa->locks[0], LOCK_SHARED, tfa->txes[t]));
        }
        else if (p == 1) {
            return map_lock_ret(lock_lock(tfa->locks[0], LOCK_SHARED, tfa->txes[t]));
        }
        else if (p == 2) {
            return map_lock_ret(lock_lock(tfa->locks[0], LOCK_EXCLUSIVE, tfa->txes[t]));
        }
        else if (p == 3) {
            lock_unlock(tfa->locks[0], tfa->txes[t]);
            return 'U';
        }
        else if (p == 4) {
            lock_unlock(tfa->locks[0], tfa->txes[t]);
            return 'U';
        }
        else if (p == 5) {
            lock_unlock(tfa->locks[0], tfa->txes[t]);
            return 'U';
        }
    }
    else if (t == 1) {
        if (p == 3) {
            return map_lock_ret(lock_lock(tfa->locks[0], LOCK_EXCLUSIVE, tfa->txes[t]));
        }
        else if (p == 6) {
            lock_unlock(tfa->locks[0], tfa->txes[t]);
            return 'U';
        }
    }
    return '.';
}

START_TEST(test_rwlock_06) {
    printf("  test_rwlock_06...\n");

    struct tfunc_args tfa;
    struct locks_ctx *locks = locks_new_ctx(2);
    tfa.locks[0] = lock_new(locks);
    tfa.txes[0] = store_new_mock_tx(0, 0);
    tfa.txes[1] = store_new_mock_tx(1, 1);
    struct scaff_ctx *scaff = scaff_new_ctx(2, 7, &tfunc06, &tfa);

    scaff_run(scaff);
    scaff_print_results(scaff);

    ck_scaff_assert(scaff,
        "TTTUUU."
        "...--TU");

    lock_free(tfa.locks[0]);
    store_free_mock_tx(tfa.txes[0]);
    store_free_mock_tx(tfa.txes[1]);
    scaff_free_ctx(scaff);
}
END_TEST*/

/* two threads that try to cross-lock two locks, this is a simple multi-lock
 * deadlock */
char tfdead01(int t, int p, void *arg) {
    struct tfunc_args *tfa = arg;
    if (t == 0) {
        if (p == 0) {
            return map_lock_ret(lock_lock(tfa->locks[0], LOCK_EXCLUSIVE, tfa->txes[t]));
        }
        if (p == 1) {
            return map_lock_ret(lock_lock(tfa->locks[1], LOCK_EXCLUSIVE, tfa->txes[t]));
        }
        else if (p == 5) {
            lock_unlock(tfa->locks[1], tfa->txes[t]);
            return 'U';
        }
        else if (p == 6) {
            lock_unlock(tfa->locks[0], tfa->txes[t]);
            return 'U';
        }
    }
    else if (t == 1) {
        if (p == 0) {
            return map_lock_ret(lock_lock(tfa->locks[1], LOCK_EXCLUSIVE, tfa->txes[t]));
        }
        if (p == 2) {
            return map_lock_ret(lock_lock(tfa->locks[0], LOCK_EXCLUSIVE, tfa->txes[t]));
        }
        else if (p == 4) {
            lock_unlock(tfa->locks[1], tfa->txes[t]);
            return 'U';
        }
    }
    return '.';
}

START_TEST(test_deadlock_01) {
    printf("  test_deadlock_01...\n");

    struct tfunc_args tfa;
    struct locks_ctx *locks = locks_new_ctx(2);
    tfa.locks[0] = lock_new(locks);
    tfa.locks[1] = lock_new(locks);
    tfa.txes[0] = store_new_mock_tx(0, 0);
    tfa.txes[1] = store_new_mock_tx(1, 1);
    struct scaff_ctx *scaff = scaff_new_ctx(2, 7, &tfdead01, &tfa);

    scaff_run(scaff);
    scaff_print_results(scaff);

    ck_scaff_assert(scaff,
        "T---TUU"
        "T.D.U..");

    lock_free(tfa.locks[0]);
    lock_free(tfa.locks[1]);
    store_free_mock_tx(tfa.txes[0]);
    store_free_mock_tx(tfa.txes[1]);
    scaff_free_ctx(scaff);
}
END_TEST

/* this time the other one gets faulted because of the sid. also we successfully
 * unlock a lock that we got a DEADLOCK on (idempotent unlock, kinda required by
 * the reentrant lock) */
char tfdead02(int t, int p, void *arg) {
    struct tfunc_args *tfa = arg;
    if (t == 0) {
        if (p == 0) {
            return map_lock_ret(lock_lock(tfa->locks[0], LOCK_EXCLUSIVE, tfa->txes[t]));
        }
        if (p == 1) {
            return map_lock_ret(lock_lock(tfa->locks[1], LOCK_EXCLUSIVE, tfa->txes[t]));
        }
        else if (p == 3) {
            lock_unlock(tfa->locks[1], tfa->txes[t]);
            return 'U';
        }
        else if (p == 4) {
            lock_unlock(tfa->locks[0], tfa->txes[t]);
            return 'U';
        }
    }
    else if (t == 1) {
        if (p == 0) {
            return map_lock_ret(lock_lock(tfa->locks[1], LOCK_EXCLUSIVE, tfa->txes[t]));
        }
        if (p == 2) {
            return map_lock_ret(lock_lock(tfa->locks[0], LOCK_EXCLUSIVE, tfa->txes[t]));
        }
        else if (p == 5) {
            lock_unlock(tfa->locks[0], tfa->txes[t]);
            return 'U';
        }
        else if (p == 6) {
            lock_unlock(tfa->locks[1], tfa->txes[t]);
            return 'U';
        }
    }
    return '.';
}

START_TEST(test_deadlock_02) {
    printf("  test_deadlock_02...\n");

    struct tfunc_args tfa;
    struct locks_ctx *locks = locks_new_ctx(2);
    tfa.locks[0] = lock_new(locks);
    tfa.locks[1] = lock_new(locks);
    tfa.txes[0] = store_new_mock_tx(1, 0);
    tfa.txes[1] = store_new_mock_tx(0, 1);
    struct scaff_ctx *scaff = scaff_new_ctx(2, 7, &tfdead02, &tfa);

    scaff_run(scaff);
    scaff_print_results(scaff);

    ck_scaff_assert(scaff,
        "T-DUU.."
        "T.--TUU");

    lock_free(tfa.locks[0]);
    lock_free(tfa.locks[1]);
    store_free_mock_tx(tfa.txes[0]);
    store_free_mock_tx(tfa.txes[1]);
    scaff_free_ctx(scaff);
}
END_TEST

TCase* make_rwlock_checks(void) {
    TCase *tc_rwlock;

    tc_rwlock = tcase_create("RWLock");
    tcase_add_test(tc_rwlock, test_rwlock_01);
    tcase_add_test(tc_rwlock, test_rwlock_02);
    tcase_add_test(tc_rwlock, test_rwlock_03);
    tcase_add_test(tc_rwlock, test_rwlock_04);
    tcase_add_test(tc_rwlock, test_rwlock_05);
    //tcase_add_test(tc_rwlock, test_rwlock_06);

    tcase_add_test(tc_rwlock, test_deadlock_01);
    tcase_add_test(tc_rwlock, test_deadlock_02);

    return tc_rwlock;
}

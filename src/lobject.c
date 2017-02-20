#include "lobject.h"

#include <stdlib.h>

// -------- implementation of declared public structures --------

struct lobject {
    struct object *obj;
    struct lock *lock;
    int pin;
};

// -------- implementation of public functions --------

struct lobject* lobject_new(void) {
    struct lobject *ret = malloc(sizeof(struct lobject));
    ret->obj = NULL;
    ret->lock = NULL;
    ret->pin = 0;
    return ret;
}

void lobject_free(struct lobject *lo) {
    free(lo);
}

void lobject_set_object(struct lobject *lo, struct object *obj) {
    lo->obj = obj;
}

struct object* lobject_get_object(struct lobject *lo) {
    return lo->obj;
}

void lobject_set_lock(struct lobject *lo, struct lock *l) {
    lo->lock = l;
}

struct lock* lobject_get_lock(struct lobject *lo) {
    return lo->lock;
}

void lobject_pin(struct lobject *lo) {
    lo->pin = 1;
}

void lobject_unpin(struct lobject *lo) {
    lo->pin = 0;
}

int lobject_is_pinned(struct lobject *lo) {
    return lo->pin;
}

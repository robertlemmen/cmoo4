#include "lobject.h"

#include <stdlib.h>

// -------- implementation of declared public structures --------

struct lobject {
    struct object *obj;
};

// -------- implementation of public functions --------

struct lobject* lobject_new(void) {
    struct lobject *ret = malloc(sizeof(struct lobject));
    ret->obj = NULL;
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

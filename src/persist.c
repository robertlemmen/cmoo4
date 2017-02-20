#include "persist.h"

#include <stdlib.h>

// XXX this is a stub, not actually going to disk. single-linbked-list
// is also about the worst possible data structure...

struct object_list_node {
    struct object *object;
    struct object_list_node *next;
};

// -------- implementation of declared public structures --------

struct persist {
    struct object_list_node *objects;
};

// -------- utility functions internal to this module --------

void mk_duff_object(struct persist *p, object_id oid) {
    struct object *o = obj_new();
    obj_set_id(o, oid);
    struct object_list_node *ln = malloc(sizeof(struct object_list_node));
    ln->object = o;
    ln->next = p->objects;
    p->objects = ln;
}

// -------- implementation of public functions --------

struct persist* persist_new(void) {
    struct persist *ret = malloc(sizeof(struct persist));
    ret->objects = NULL;

    // XXX stubby shit
    mk_duff_object(ret, 0);
    mk_duff_object(ret, 11);
    mk_duff_object(ret, 111);
    mk_duff_object(ret, 112);
    mk_duff_object(ret, 113);

    return ret;
}

void persist_free(struct persist *p) {
    free(p);
}

struct object* persist_get(struct persist *p, object_id oid) {
    struct object_list_node *ln = p->objects;
    while (ln) {
        if (obj_get_id(ln->object) == oid) {
            return ln->object;
        }
        ln = ln->next;
    }
    return NULL;
}

void persist_put(struct persist *p, struct object *o) {
    object_id oid = obj_get_id(o);
    struct object_list_node *ln = p->objects;
    while (ln) {
        if (obj_get_id(ln->object) == oid) {
            obj_free(ln->object);
            ln->object = o;
            return;
        }
        ln = ln->next;
    }
    // not already present, append
    ln = malloc(sizeof(struct object_list_node));
    ln->object = o;
    ln->next = p->objects;
    p->objects = ln;
}

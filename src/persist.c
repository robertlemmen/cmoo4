#include "persist.h"

#include <stdlib.h>

// XXX for stubby objects
#include "eval.h"

// XXX this is a stub, not actually going to disk. single-linked-list
// is also about the worst possible data structure for this

struct object_list_node {
    struct object *object;
    struct object_list_node *next;
};

// -------- implementation of declared public structures --------

struct persist {
    struct object_list_node *objects;
};

// -------- utility functions internal to this module --------

struct object* mk_duff_object(struct persist *p, object_id oid) {
    struct object *o = obj_new();
    obj_set_id(o, oid);
    struct object_list_node *ln = malloc(sizeof(struct object_list_node));
    ln->object = o;
    ln->next = p->objects;
    p->objects = ln;
    return o;
}

// -------- implementation of public functions --------

struct persist* persist_new(void) {
    struct persist *ret = malloc(sizeof(struct persist));
    ret->objects = NULL;

    struct object *o;
    // XXX stubby shit
    // XXX this could be split into the root object, a base listener, and a base
    // client handler!
    o = mk_duff_object(ret, 0);
    opcode code[] = {
                        OP_ARGS_LOCALS, 0x01, 0x02,
                        OP_LOAD_STRING, 0x01, 0x11, 0x00, 'n', 'e', 't', '_', 'm', 'a', 'k', 'e', '_', 'l', 'i', 's', 't', 'e', 'n', 'e', 'r',
                        OP_LOAD_INT, 0x02, 0x39, 0x30, 0x00, 0x00,
                        OP_PUSH, 0x01,
                        OP_PUSH, 0x02,
                        OP_SELF, 0x00,
                        OP_MAKE_OBJ, 0x00, 0x00,
                        OP_PUSH, 0x00,
                        OP_SYSCALL, 0x02,
                        OP_HALT};
    obj_set_code(o, "init", code, sizeof(code));

    opcode code2[] = {
                        OP_ARGS_LOCALS, 0x00, 0x03,
                        OP_LOAD_STRING, 0x00, 0x15, 0x00, 'n', 'e', 't', '_', 's', 'h', 'u', 't', 'd', 'o', 'w', 'n', '_', 'l', 'i', 's', 't', 'e', 'n', 'e', 'r',
                        OP_LOAD_INT, 0x01, 0x39, 0x30, 0x00, 0x00,
                        OP_PUSH, 0x00,
                        OP_PUSH, 0x01,
                        OP_SYSCALL, 0x01,
                        OP_HALT};
    obj_set_code(o, "shutdown", code2, sizeof(code2));

    opcode code3[] = {
                        OP_ARGS_LOCALS, 0x01, 0x03,
                        OP_SELF, 0x02,
                        OP_MAKE_OBJ, 0x02, 0x02,

                        OP_PUSH, 0x02,
                        OP_LOAD_STRING, 0x03, 0x0A, 0x00, 's', 'e', 't', '_', 's', 'o', 'c', 'k', 'e', 't',
                        OP_PUSH, 0x03,
                        OP_CLEAR, 0x03,
                        OP_PUSH, 0x03,
                        OP_PUSH, 0x00,
                        OP_CALL, 0x01,
                        OP_POP, 0x03,
                        OP_NOOP,

                        OP_LOAD_STRING, 0x01, 0x11, 0x00, 'n', 'e', 't', '_', 'a', 'c', 'c', 'e', 'p', 't', '_', 's', 'o', 'c', 'k', 'e', 't',
                        OP_PUSH, 0x01,
                        OP_PUSH, 0x00,
                        OP_PUSH, 0x02,
                        OP_DEBUGR, 0x02,
                        OP_SYSCALL, 0x02,
                        OP_HALT};
    obj_set_code(o, "accept", code3, sizeof(code3));

    opcode code4[] = {
                        OP_ARGS_LOCALS, 0x01, 0x01,
                        OP_LOAD_STRING, 0x01, 0x06, 0x00, 's', 'o', 'c', 'k', 'e', 't',
                        OP_SETGLOBAL, 0x01, 0x00,
                        OP_CLEAR, 0x01,
                        OP_RETURN, 0x01};
    obj_set_code(o, "set_socket", code4, sizeof(code4));

    opcode code5[] = {
                        OP_ARGS_LOCALS, 0x01, 0x01,
                        OP_LOAD_STRING, 0x01, 0x0F, 0x00, 'n', 'e', 't', '_', 's', 'o', 'c', 'k', 'e', 't', '_', 'f', 'r', 'e', 'e', 
                        OP_PUSH, 0x01,
                        OP_PUSH, 0x00,
                        OP_SYSCALL, 0x01,
                        OP_HALT};
    obj_set_code(o, "closed", code5, sizeof(code5));

    opcode code6[] = {
                        OP_ARGS_LOCALS, 0x02, 0x04,
                        OP_DEBUGR, 0x00,
                        OP_DEBUGR, 0x01,
                        OP_LOAD_STRING, 0x02, 0x10, 0x00, 'n', 'e', 't', '_', 's', 'o', 'c', 'k', 'e', 't', '_', 'w', 'r', 'i', 't', 'e',
                        OP_LOAD_STRING, 0x03, 0x02, 0x00, '>', ' ',
                        OP_LOAD_STRING, 0x04, 0x06, 0x00, 's', 'o', 'c', 'k', 'e', 't',
                        OP_GETGLOBAL, 0x04, 0x04,
                        OP_DEBUGR, 0x04,
                        OP_PUSH, 0x02,
                        OP_PUSH, 0x04,
                        OP_PUSH, 0x00,
                        OP_CONCAT, 0x05, 0x03, 0x01,
                        OP_PUSH, 0x05,
                        OP_SYSCALL, 0x03,
                        OP_HALT};
    obj_set_code(o, "read", code6, sizeof(code6));

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

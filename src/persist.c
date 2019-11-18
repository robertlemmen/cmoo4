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

    // XXX stubby shit
    // XXX this could be split into the root object, a base listener, and a base
    // client handler, driving some structural changes throughout the code. in
    // the first instance the IDs of the other objects could be hard-coded as
    // numbers in the object code, in the next step they should be injected as
    // object slots, 
    

    // object 0 is the root object, the driver calls "init" and "shutdown" on
    // it. on init, the object will set up a listening socket and bind it to
    // an newly created child object based on the base-listener (see below).
    struct object *o0 = mk_duff_object(ret, 0);
    opcode code[] = {
                        OP_ARGS_LOCALS, 0x01, 0x02,
                        OP_LOAD_STRING, 0x01, 0x11, 0x00, 'n', 'e', 't', '_', 'm', 'a', 'k', 'e', '_', 'l', 'i', 's', 't', 'e', 'n', 'e', 'r',
                        OP_LOAD_INT, 0x02, 0x39, 0x30, 0x00, 0x00, // 0x3039 is port 12345
                        OP_PUSH, 0x01,
                        OP_PUSH, 0x02,
                        OP_LOAD_STRING, 0x00, 0x0D, 0x00, 'b', 'a', 's', 'e', '-', 'l', 'i', 's', 't', 'e', 'n', 'e', 'r',
                        OP_GETGLOBAL, 0x00, 0x00,
                        OP_MAKE_OBJ, 0x00, 0x00,
                        OP_PUSH, 0x00,
                        OP_SYSCALL, 0x02,
                        // call the new object and pass the
                        // base-socket-handler reference
                        OP_LOAD_STRING, 0x01, 0x13, 0x00, 'b', 'a', 's', 'e', '-', 's', 'o', 'c', 'k', 'e', 't', '-', 'h', 'a', 'n', 'd', 'l', 'e', 'r', 
                        OP_GETGLOBAL, 0x01, 0x01,
                        OP_LOAD_STRING, 0x02, 0x17, 0x00, 's', 'e', 't', '-', 'b', 'a', 's', 'e', '-', 's', 'o', 'c', 'k', 'e', 't', '-', 'h', 'a', 'n', 'd', 'l', 'e', 'r',
                        OP_DEBUGR, 0x02,
                        OP_DEBUGR, 0x01,
                        OP_DEBUGR, 0x00,
                        OP_PUSH, 0x00,
                        OP_PUSH, 0x02,
                        OP_PUSH, 0x01, // ignored slot
                        OP_PUSH, 0x01,
                        OP_CALL, 0x01,
                        OP_POP, 0x00,
                        OP_DEBUGR, 0x00,
                        OP_HALT};
    obj_set_code(o0, "init", code, sizeof(code));

    opcode code2[] = {
                        // XXX this should really delegate the shutdown code to
                        // the listener object created in "init"
                        OP_ARGS_LOCALS, 0x00, 0x03,
                        OP_LOAD_STRING, 0x00, 0x15, 0x00, 'n', 'e', 't', '_', 's', 'h', 'u', 't', 'd', 'o', 'w', 'n', '_', 'l', 'i', 's', 't', 'e', 'n', 'e', 'r',
                        OP_LOAD_INT, 0x01, 0x39, 0x30, 0x00, 0x00, // XXX why do we have this number here and in init?
                        OP_PUSH, 0x00,
                        OP_PUSH, 0x01,
                        OP_SYSCALL, 0x01,
                        OP_HALT};
    obj_set_code(o0, "shutdown", code2, sizeof(code2));
    obj_set_global(o0, "base-listener", val_make_objref(1));
    obj_set_global(o0, "base-socket-handler", val_make_objref(2));

    // object 1 is the base-listener, it reacts to "accept" calls by setting up
    // an object that handles the socket in question 
    struct object *o1 = mk_duff_object(ret, 1);
    opcode code3[] = {
                        OP_ARGS_LOCALS, 0x01, 0x03,

                        OP_LOAD_STRING, 0x02, 0x13, 0x00, 'b', 'a', 's', 'e', '-', 's', 'o', 'c', 'k', 'e', 't', '-', 'h', 'a', 'n', 'd', 'l', 'e', 'r',
                        OP_GETGLOBAL, 0x02, 0x02,
                        OP_DEBUGR, 0x02,
                        OP_MAKE_OBJ, 0x02, 0x02,

                        // call the newly created object and pass the socket
                        // special
                        OP_PUSH, 0x02,
                        OP_LOAD_STRING, 0x03, 0x0A, 0x00, 's', 'e', 't', '_', 's', 'o', 'c', 'k', 'e', 't',
                        OP_PUSH, 0x03,
                        OP_CLEAR, 0x03,
                        OP_PUSH, 0x03,
                        OP_PUSH, 0x00,
                        OP_CALL, 0x01,
                        OP_POP, 0x03,


                        OP_LOAD_STRING, 0x01, 0x11, 0x00, 'n', 'e', 't', '_', 'a', 'c', 'c', 'e', 'p', 't', '_', 's', 'o', 'c', 'k', 'e', 't',
                        OP_PUSH, 0x01,
                        OP_PUSH, 0x00,
                        OP_PUSH, 0x02,
                        OP_SYSCALL, 0x02,

                        OP_HALT};
    obj_set_code(o1, "accept", code3, sizeof(code3));

    opcode code4[] = {
                        OP_ARGS_LOCALS, 0x01, 0x01,
                        OP_LOAD_STRING, 0x01, 0x13, 0x00, 'b', 'a', 's', 'e', '-', 's', 'o', 'c', 'k', 'e', 't', '-', 'h', 'a', 'n', 'd', 'l', 'e', 'r',
                        OP_SETGLOBAL, 0x01, 0x00,
                        OP_CLEAR, 0x01,
                        OP_RETURN, 0x01};

    obj_set_code(o1, "set-base-socket-handler", code4, sizeof(code4));

    // object 2 is a parent to all socket handlers, reacts to 'read' and 'closed'
    // calls
    struct object *o2 = mk_duff_object(ret, 2);
    opcode code5[] = {
                        OP_ARGS_LOCALS, 0x01, 0x01,
                        OP_LOAD_STRING, 0x01, 0x06, 0x00, 's', 'o', 'c', 'k', 'e', 't',
                        OP_SETGLOBAL, 0x01, 0x00,
                        OP_CLEAR, 0x01,
                        OP_RETURN, 0x01};
    obj_set_code(o2, "set_socket", code5, sizeof(code5));

    opcode code6[] = {
                        OP_ARGS_LOCALS, 0x01, 0x01,
                        OP_LOAD_STRING, 0x01, 0x0F, 0x00, 'n', 'e', 't', '_', 's', 'o', 'c', 'k', 'e', 't', '_', 'f', 'r', 'e', 'e', 
                        OP_PUSH, 0x01,
                        OP_PUSH, 0x00,
                        OP_SYSCALL, 0x01,
                        OP_HALT};
    obj_set_code(o2, "closed", code6, sizeof(code6));

    opcode code7[] = {
                        OP_ARGS_LOCALS, 0x02, 0x04,
                        OP_DEBUGR, 0x00,
                        OP_DEBUGR, 0x01,

                        // in order to see contention better, this calls into a
                        // object that does a write, and then sleeps a short
                        // while with that lock held
                        OP_PARENT, 0x02,
                        OP_DEBUGR, 0x02,
                        OP_PUSH, 0x02,
                        OP_LOAD_STRING, 0x02, 0x09, 0x00, 'i', 'n', 'c', '_', 'c', 'o', 'u', 'n', 't',
                        OP_PUSH, 0x02,
                        OP_CLEAR, 0x02,
                        OP_PUSH, 0x02,
                        OP_CALL, 0x00,
                        OP_USLEEP, 0x40, 0x42, 0x1F, 0x00,

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
    obj_set_code(o2, "read", code7, sizeof(code7));

    opcode code8[] = {
                        OP_ARGS_LOCALS, 0x00, 0x03,
                        OP_LOAD_STRING, 0x00, 0x05, 0x00, 'c', 'o', 'u', 'n', 't',
                        OP_LOAD_INT, 0x01, 0x01, 0x00, 0x00, 0x00,
                        OP_GETGLOBAL, 0x02, 0x00,
                        OP_ADD, 0x02, 0x02, 0x01,
                        OP_SETGLOBAL, 0x00, 0x02,
                        OP_DEBUGR, 0x02,
                        OP_RETURN, 0x02};
    obj_set_code(o2, "inc_count", code8, sizeof(code8));
    obj_set_global(o2, "count", val_make_int(0));

    return ret;
}

void persist_free(struct persist *p) {
    while (p->objects) {
        struct object_list_node *t = p->objects;
        p->objects = t->next;
        obj_free(t->object);
        free(t);
    }
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

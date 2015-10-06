#include "object.h"

#include <stdlib.h>
#include <string.h>

// -------- internal structures --------

struct method_slot {
    char *name;
    opcode *code_buf;
    int buf_len;
    struct method_slot *next;
};

struct global_slot {
    char *name;
    val global;
    struct global_slot *next;
};

// -------- implementation of declared public structures --------

struct object {
    object_id id;
    int parent_count;
    object_id *parents;
    struct method_slot *methods;
    struct global_slot *globals;
};

// -------- implementation of public functions --------

struct object* obj_new(void) {
    struct object *ret = malloc(sizeof(struct object));
    memset(ret, 0, sizeof(struct object));
    return ret;
}

void obj_free(struct object *o) {
    while (o->methods) {
        struct method_slot *tmp = o->methods;
        o->methods = o->methods->next;
        free(tmp->name);
        free(tmp->code_buf);
        free(tmp);
    }
    while (o->globals) {
        struct global_slot *tmp = o->globals;
        o->globals = o->globals->next;
        free(tmp->name);
        val_dec_ref(tmp->global);
        free(tmp);
    }
    free(o->parents);
    free(o);
}

object_id obj_get_id(struct object *o) {
    return o->id;
}

void obj_set_id( struct object *o, object_id id) {
    o->id = id;
}

int obj_get_parent_count(struct object *o) {
    return o->parent_count;
}

object_id obj_get_parent(struct object *o, int idx) {
    return o->parents[idx];
}

void obj_add_parent(struct object *o, object_id parent_id) {
    o->parents = realloc(o->parents, (o->parent_count + 1) * sizeof(object_id));
    o->parents[o->parent_count] = parent_id;
    o->parent_count++;
}

void obj_remove_parent(struct object *o, object_id parent_id) {
    for (int i = 0; i < o->parent_count; i++) {
        if (o->parents[i] == parent_id) {
            // found the one we are interested in, remove it
            memmove(&o->parents[i], &o->parents[i+1], o->parent_count - i - 1);
            o->parent_count--;
            // we don't actually shrink the memory region
            return;
        }
    }
}

int obj_get_code(struct object *o, char *name, opcode **code_buf) {
    struct method_slot *cms = o->methods;
    while (cms) {
        if (strcmp(cms->name, name) == 0) {
            // found!
            *code_buf = cms->code_buf;
            return cms->buf_len;
        }
        cms = cms->next;
    }
    // not found
    *code_buf = NULL;
    return 0;
}

void obj_set_code(struct object *o, char *name, opcode *code_buf, int buf_len) {
    struct method_slot *cms = o->methods;
    while (cms) {
        if (strcmp(cms->name, name) == 0) {
            // found!
            // XXX removal case
            cms->code_buf = realloc(cms->code_buf, buf_len);
            memcpy(cms->code_buf, code_buf, buf_len);
            cms->buf_len = buf_len;
            return;
        }
        cms = cms->next;
    }
    // not found
    cms = malloc(sizeof(struct method_slot));
    cms->name = malloc(strlen(name)+1);
    strcpy(cms->name, name);
    cms->code_buf = malloc(buf_len);
    memcpy(cms->code_buf, code_buf, buf_len);
    cms->buf_len = buf_len;
    cms->next = o->methods;
    o->methods = cms;
}

val obj_get_global(struct object *o, char *name) {
    struct global_slot *cgs = o->globals;
    while (cgs) {
        if (strcmp(cgs->name, name) == 0) {
            // found!
            val_inc_ref(cgs->global);
            return cgs->global;
        }
        cgs = cgs->next;
    }
    // not found!
    return val_make_nil();
}

void obj_set_global(struct object *o, char *name, val v) {
    struct global_slot *cgs = o->globals;
    while (cgs) {
        if (strcmp(cgs->name, name) == 0) {
            // found!
            // XXX removal case
            val_dec_ref(cgs->global);
            cgs->global = v;
            val_inc_ref(cgs->global);
            return;
        }
        cgs = cgs->next;
    }
    // not found
    cgs = malloc(sizeof(struct global_slot));
    cgs->name = malloc(strlen(name)+1);
    strcpy(cgs->name, name);
    cgs->global = v;
    val_inc_ref(cgs->global);
    cgs->next = o->globals;
    o->globals = cgs;
}

void obj_state_from_buffer(struct object *o, char *buf, int buf_len) {
    // XXX
}

void obj_code_from_buffer(struct object *o, char *buf, int buf_len) {
    // XXX
}

void obj_state_to_buffer(struct object *o, char **buffer, int *buf_len) {
    // XXX
}

void obj_code_to_buffer(struct object *o, char **buffer, int *buf_len) {
    // determine buffer size required, alloc and update buf_len
    int size_required =   sizeof(object_id) // id
                        + sizeof(int) // parent_count
                        + o->parent_count * sizeof(object_id) // parents
                        + sizeof(int); // method count
    struct method_slot *cms = o->methods;
    int method_count = 0;
    while (cms) {
        size_required +=  sizeof(int) // strlen(name)
                        + strlen(cms->name) // name
                        + sizeof(int) // buf_len
                        + cms->buf_len * sizeof(opcode); // code_buf
        method_count++;
        cms = cms->next;
    }
    if (*buf_len < size_required) {
        *buffer = realloc(*buffer, size_required);
    }
    *buf_len = size_required;
    // copy the data
    char *dst = *buffer;
    memcpy(dst, &o->id, sizeof(object_id)); dst += sizeof(object_id);
    memcpy(dst, &o->parent_count, sizeof(int)); dst += sizeof(int);
    for (int i = 0; i < o->parent_count; i++) {
        memcpy(dst, &o->parents[i], sizeof(object_id)); dst += sizeof(object_id);
    }
    memcpy(dst, &method_count, sizeof(int)); dst += sizeof(int);
    cms = o->methods;
    while (cms) {
        int nlen = strlen(cms->name);
        memcpy(dst, &nlen, sizeof(int)); dst += sizeof(int);
        memcpy(dst, cms->name, nlen), dst += nlen;
        memcpy(dst, &cms->buf_len, sizeof(int)); dst += sizeof(int);
        memcpy(dst, cms->code_buf, cms->buf_len * sizeof(opcode)); dst += cms->buf_len * sizeof(opcode);

        cms = cms->next;
    }
}

struct object* obj_copy(struct object *o) {
    // XXX this could be some sort of shallow copy that only does 
    // actually copying when written to...
    struct object *ret = malloc(sizeof(struct object));
    memset(ret, 0, sizeof(struct object));
    ret->id = o->id;
    ret->parent_count = o->parent_count;
    ret->parents = malloc(ret->parent_count * sizeof(object_id));
    memcpy(ret->parents, o->parents, ret->parent_count * sizeof(object_id));
    struct method_slot *oms = o->methods;
    struct method_slot *nms;
    struct method_slot *pms = NULL;
    while (oms) {
        nms = malloc(sizeof(struct method_slot));
        nms->name = malloc(strlen(oms->name)+1);
        strcpy(nms->name, oms->name);
        nms->buf_len = oms->buf_len;
        nms->code_buf = malloc(nms->buf_len);
        memcpy(nms->code_buf, oms->code_buf, nms->buf_len);
        nms->next = NULL;
        if (pms) {
            pms->next = nms;
        }
        else {
            ret->methods = pms;
        }
        pms = nms;
        oms = oms->next;
    }
    struct global_slot *ogs = o->globals;
    struct global_slot *ngs;
    struct global_slot *pgs = NULL;
    while (ogs) {
        ngs = malloc(sizeof(struct global_slot));
        ngs->name = malloc(strlen(ogs->name)+1);
        strcpy(ngs->name, ogs->name);
        ngs->global = ogs->global;
        val_inc_ref(ngs->global);
        ngs->next = NULL;
        if (pgs) {
            pgs->next = ngs;
        }
        else {
            ret->globals = pgs;
        }
        pgs = ngs;
        ogs = ogs->next;
    }
 
    return ret;
}


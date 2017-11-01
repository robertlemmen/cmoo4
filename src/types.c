#include "types.h"

#include <string.h>
#include <stdlib.h>
#include <assert.h>

/* XXX in the longer run we should change the type tagging scheme so that
 * the lowest bits only indicate whether this is a non-immediate, and if 
 * so some indication of type, or whether it is an immediate, in which case  
 * * the other 32bits can contain the payload and we have extra low bits to
 * further differentiate. this would allow us to have far more types, which
 * would be good given that with hashes and arrays we will already run out
 * of type bits */

// XXX all the shifts, shouldn't they be by 3???

struct heap_string {
    uint16_t ref_count;
    uint16_t length;
    char data[];
};

int val_type(val v) {
    return v & 0x7;
}

val val_make_nil(void) {
    return 0;
}

val val_make_bool(bool i) {
    return (i << 4) | TYPE_BOOL;
}

val val_make_int(int i) {
    // XXX wouldn't it be quicker to just use the top 4 bytes? same for float and bool
    return ((int64_t)i << 4) | TYPE_INT;
}

val val_make_float(float i) {
    union {
        uint32_t i;
        float f;
    } fv;
    fv.f = i;
    return ((val)fv.i << 4) | TYPE_FLOAT;
}

val val_make_string(uint16_t len, char *s) {
    struct heap_string *hs = malloc(len + sizeof(uint16_t) * 2 + 1);
    memcpy(hs->data, s, len);
    hs->data[len] = '\0';
    hs->ref_count = 1;
    hs->length = len;
    uint64_t ret = (uint64_t)hs;
    ret |= TYPE_STRING;
    return ret;
}

val val_make_objref(object_id ref) {
    assert(ref < 0x010000000000);
    return (ref << 4) | TYPE_OBJREF;
}

val val_make_special(void *special) {
    // XXX this needs some protection against sizeof(void*) != sizeof(uint64_t)
    assert(((uint64_t)special & 0x7) == 0);
    return (uint64_t)special | TYPE_SPECIAL;
}

void val_inc_ref(val v) {
    if (((uint64_t)v & 0x7) == TYPE_STRING) {
        struct heap_string *hs = (struct heap_string*)((uint64_t)v & (~0x7));
        hs->ref_count++;
    }
}

void val_dec_ref(val v) {
    // cleanup if non-immediate type
    if (((uint64_t)v & 0x7) == TYPE_STRING) {
        struct heap_string *hs = (struct heap_string*)((uint64_t)v & (~0x7));
        hs->ref_count--;
        if (hs->ref_count == 0) {
            free(hs);
        }
    }
}

void val_clear(val *v) {
    val_dec_ref(*v);
    *v = 0;
}

void val_init(val *v) {
    *v = 0;
}

bool val_get_bool(val v) {
    assert((v & 0x7) == TYPE_BOOL);
    return v >> 4;
}

int val_get_int(val v) {
    assert((v & 0x7) == TYPE_INT);
    return v >> 4;
}

float val_get_float(val v) {
    assert((v & 0x7) == TYPE_FLOAT);
    union {
        uint32_t i;
        float f;
    } fv;
    fv.i = (v >> 4);
    return fv.f;
}

char* val_get_string_data(val v) {
    assert((v & 0x7) == TYPE_STRING);
    struct heap_string *hs = (struct heap_string*)((uint64_t)v & (~0x7));
    return hs->data;
}

uint16_t val_get_string_len(val v) {
    assert((v & 0x7) == TYPE_STRING);
    struct heap_string *hs = (struct heap_string*)((uint64_t)v & (~0x7));
    return hs->length;
}

object_id val_get_objref(val v) {
    assert((v & 0x7) == TYPE_OBJREF);
    return v >> 4;
}

void* val_get_special(val v) {
    return (void*)((uint64_t)v & (~0x07));
}

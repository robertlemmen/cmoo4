#include "types.h"

#include <assert.h>

/* XXX in the longer run we should change the type tagging scheme so that
 * the lowest bits only indicate whether this is a non-immediate, and if 
 * so some indication of type, or whether it is an immediate, in which case  * the other 32bits can contain the payload and we have extra low bits to
 * further differentiate. this would allow us to have far more types, which
 * would be good given that with hashes and arrays we will already run out
 * of type bits */

int val_type(val v) {
    return v & 0x7;;
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

void val_clear(val *v) {
    // XXX cleanup if non-immediate type
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

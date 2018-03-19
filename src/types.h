#ifndef TYPES_H
#define TYPES_H

#include <stdbool.h>

#include "defs.h"

/* cmoo is a dynamic language, so types are associated with values rather 
 * than with variables. we differ between immediate values where the whole
 * state can be represented by the value itself, and non-immediates where
 * there is external information that the value only refers to. examples
 * of an immediate are integers and booleans, an example for a non-immediate
 * is a string, because the actual character buffer is kept somewhere on the 
 * heap. there is a type for "special" items like file handles and a type
 * for object references. these two are somewhere inbetween or are not very
 * well classified by the system above. supported types: 
 * */

#define TYPE_NIL        0
#define TYPE_BOOL       1
#define TYPE_INT        2
#define TYPE_FLOAT      3
#define TYPE_STRING     4
#define TYPE_OBJREF     5
#define TYPE_SPECIAL    6

/* this returns the type of a value */
int val_type(val v);

/* create values of a given type and initial value */
val val_make_nil(void);
val val_make_bool(bool i);
val val_make_int(int i);
val val_make_float(float i);
val val_make_string(uint16_t len, char *s); // copies, does not consume argument
val val_make_objref(object_id ref);
// XXX we need a way to tell the different specials apart
val val_make_special(void *special);
// XXX more creators

/* sets the value pointed to to NIL and runs cleanups for non-immediates
 * as required */
void val_clear(val *v);
/* also set to NIL, but performs no cleanups */
void val_init(val *v);

/* non-intermediates do reference counting to check when the heap needs
 * to be cleared up. with the functions below you can affect the counter
 * if you copy "val" items around */
void val_inc_ref(val v);
void val_dec_ref(val v);

/* get the value assuming that the type is correct */
bool val_get_bool(val v);
int val_get_int(val v);
float val_get_float(val v);
char* val_get_string_data(val v);
uint16_t val_get_string_len(val v);
object_id val_get_objref(val v);
void* val_get_special(val v);
// XXX more getters

/* return a textual representation, caller needs to free memory */
char* val_print(val v);

// serialize/deserialize type

#endif /* TYPES_H */

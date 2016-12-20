#ifndef DEFS_H
#define DEFS_H

/* this file contains definitions used all over the place. These would 
 * otherwise cause high coupling between comilation units, and they do not 
 * really belong in a single place anyway...
 * */

#include <stdint.h>

/* this is how opcodes are represented, see list of values in eval.h */
typedef uint8_t opcode;

/* this is used to identify/reference an object. the topmost 8 bits are not 
 * used, which allows shifting in a "val", and allows shifting 
 * within persistence to keep source, bytecode and objects separately
 * identifiable
 * */
typedef uint64_t object_id;

/* representation of the non-heap part of a value, see types.h for accessors
 * and details 
 * */
typedef uint64_t val;

#endif /* DEFS_H */

#ifndef OBJECT_H
#define OBJECT_H

#include "defs.h"
#include "types.h"

/* this module implements the in-core objects that make up the core logic. 
 * Each object has a set of object variables ("globals") and member 
 * functions, both identified within the object by their name. A global can 
 * have the same name as a member function, but that is of course not very 
 * good naming. Each object has a unique object_id that can be used to refer 
 * to the object and retrieve it from persistence. 
 *
 * The object system is prototype based, so there are no classes. Objects 
 * have one or more parent objects, except for the special object with 
 * object_id 0, the root of all objects. When a method is called on an 
 * object it is looked up by name (only, no multiple dispatch). If the 
 * method exists, it is invoked. If not, the parents are searched 
 * recursively for a matching function. If the method exist in multiple
 * parents, the behavior in undefined in so far that there is no guarantee
 * which of them will be invoked. 
 *
 * Object variables ("globals") are only accessible by methods on the object 
 * itself or parents of it, not by other objects. The prototype nature of 
 * the type system also means that if a method on a parent object is invoked 
 * implicitely through inheritance, and that method accesses a global, then 
 * the global on the child object is affected. This is a bit confusing 
 * initially, so an example might come in handy:
 *
 * +---------+      +---------+
 * | Parent  |      |  Child  |
 * +---------+      +---------+
 * | v_A     |      | v_A     |
 * | v_B     | <--- | v_B     |
 * +---------+      +---------+
 * | setA()  |      | setA()  |
 * | setB()  |      +---------+
 * +---------+
 *
 * with the functions looking like this (pseudocode):
 * Parent::setA(var a) {
 *   setGlobal("v_A", a); 
 * }
 * Parent::setB(var b) {
 *   setGlobal("v_B", b); 
 * }
 * Child::setA(var a) {
 *   parent.setA(a);
 * }
 *
 * Let's look at a call to Child::setB() first: the method is not found on 
 * the child object, so the parent is searched for one. That suceeds, so the 
 * method is invoked. The context for that invokation however is still the 
 * child object, so setGlobal("v_B", ...) affects the child object, not the 
 * parent. 
 *
 * In contrast, Child::setA() does execute the method on the child first, 
 * which calls setA() on the parent object explicitely through an object 
 * reference. This is not inheritance, and would be pretty much the same 
 * in structure if the methods on the two objects had different names, or if
 * the child would delegate to a object that is not it's parent at all. 
 * Consequently, the global Parent::v_A is affected, not any state of the
 * child object.
 *
 * Objects can be copied while retaining their ID, however this functionality 
 * ois reserved for the transactionality layer and does therefore not violate 
 * the conceptual uniqueness of the object id: each task only sees one 
 * consistent object per ID, the copying is purely to facilitate rollback 
 * capabilities.
 *
 * An object can be serialized/deserialized from/to a binary buffer to allow
 * persitence of the objects. This is done separately for code and state, as 
 * typically the state changes far more often than the code.
 * */

struct object;

/* create/destroy an object */
struct object* obj_new(void);
void obj_free(struct object *o);

/* each object in our world has a unique id. The id is of course not 
 * changeable on an existing object, the setter method here is purely 
 * for the purpose of the persistence layer or testing stubs, not 
 * exposed to in-core logic. */
object_id obj_get_id(struct object *o);
void obj_set_id( struct object *o, object_id id);

/* each object has a set of parent object from which it inherits behavior, 
 * these methods allow retrieval and manipulation of this set. Validation, 
 * like e.g. the fact that each object except from some special root object 
 * needs to have a parent, and that the child-parent graph is acyclic is left 
 * to the caller */
int obj_get_parent_count(struct object *o);
object_id obj_get_parent(struct object *o, int idx);
void obj_add_parent(struct object *o, object_id parent_id);
void obj_remove_parent(struct object *o, object_id parent_id);

/* updates code_buf and buf_len to refer to the code for the named
 * method. code_buf is set to NULL if the method does not exist, 
 * resolving to parent object is left to the caller. The code buffer 
 * is owned by the object and must not be modified or freed by the caller.
 * returns the size of the buffer, 0 if method not found */
int obj_get_code(struct object *o, char *name, opcode **code_buf);
/* sets the method from the provided buffer, copying the contents rather than 
 * consuming them. use NULL for code_buf to remove a method */
void obj_set_code(struct object *o, char *name, opcode *code_buf, int buf_len);

/* get set "global" member variable on object. getter returns nil if 
 * member does not exist, setter overwrites existing data. The setter 
 * copies val and does not consume, the getter creates a val that the
 * recipient has to clean up. */
val obj_get_global(struct object *o, char *name);
/* the the global, set to NIL to remove global */
void obj_set_global(struct object *o, char *name, val v);

/* reads the object state (globals) from the provided buffer, not consuming
 * it */
void obj_state_from_buffer(struct object *o, char *buf, int buf_len);
/* reads the object methods and properties (id, parents...) from the
 * supplied buffer, not consuming it. Typically persistence would call 
 * obj_new() to create an empty object, and then obj_code_from_buffer() and
 * obj_state_from_buffer() to initialize it */
void obj_code_from_buffer(struct object *o, char *buf, int buf_len);
/* serializes the object state/globals. This reallocates buffer to the required 
 * length if necessary (e.g. if buf_len is less than required or buffer is 
 * NULL), and sets *buf_len to the size taken up, deallocation of the buffer
 * is left to the caller */
void obj_state_to_buffer(struct object *o, char **buffer, int *buf_len);
/* serializes methods, id and parents to a buffer. same semantics as
 * obj_state_to_buffer() around the buffer handling. */
void obj_code_to_buffer(struct object *o, char **buffer, int *buf_len);

/* create a copy of an object, this is required e.g. to provide rollback
 * funtionality in the transaction layer. */
struct object* obj_copy(struct object *o);

#endif /* OBJECT_H */

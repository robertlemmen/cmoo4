#ifndef LOBJECT_H
#define LOBJECT_H

/* this implements a simple structure that ties together different versions
 * of a given object and the locks required to protect them */

#include "object.h"

struct lobject;

struct lobject* lobject_new(void);
void lobject_free(struct lobject *lo);

void lobject_set_object(struct lobject *lo, struct object *o);
struct object* lobject_get_object(struct lobject *lo);

#endif /* LOBJECT_H */

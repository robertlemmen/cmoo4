#ifndef PERSIST_H
#define PERSIST_H

#include "object.h"

struct persist;

struct persist* persist_new(void);
void persist_free(struct persist *p);

struct object* persist_get(struct persist *p, object_id oid);
void persist_put(struct persist *p, struct object *o);

#endif /* PERSIST_H */

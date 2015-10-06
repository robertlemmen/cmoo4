#include "check_object.h"

#include <stdlib.h>
#include <stdio.h>

#include "eval.h"
#include "object.h"

START_TEST(test_object_01) {
    printf("  test_object_01...\n");
    // check initial state of new object
    struct object *obj = obj_new();
    ck_assert(obj != NULL);
    ck_assert(obj_get_id(obj) == 0);
    ck_assert(obj_get_parent_count(obj) == 0);

    // set/get id
    obj_set_id(obj, 14);
    ck_assert(obj_get_id(obj) == 14);


    obj_add_parent(obj, 12);
    ck_assert(obj_get_parent_count(obj) == 1);
    obj_add_parent(obj, 45);
    obj_add_parent(obj, 23);
    ck_assert(obj_get_parent_count(obj) == 3);

    ck_assert(obj_get_parent(obj, 0) == 12);
    ck_assert(obj_get_parent(obj, 1) == 45);
    ck_assert(obj_get_parent(obj, 2) == 23);

    obj_remove_parent(obj, 45);
    ck_assert(obj_get_parent_count(obj) == 2);
    ck_assert(obj_get_parent(obj, 0) == 12);
    ck_assert(obj_get_parent(obj, 1) == 23);

    obj_remove_parent(obj, 145);
    ck_assert(obj_get_parent_count(obj) == 2);
    ck_assert(obj_get_parent(obj, 0) == 12);
    ck_assert(obj_get_parent(obj, 1) == 23);

    obj_remove_parent(obj, 12);
    obj_remove_parent(obj, 23);
    ck_assert(obj_get_parent_count(obj) == 0);

    opcode *cb;
    ck_assert(obj_get_code(obj, "test", &cb) == 0);
    ck_assert(cb == NULL);

    opcode cb2[] = {    OP_NOOP, 
                        OP_NOOP, 
                        OP_DEBUGI, 0x12, 0x34, 0x56, 0x78, 
                        OP_NOOP, 
                        OP_DEBUGI, 0xC0, 0x00, 0x00, 0x00, 
                   };
    obj_set_code(obj, "test2", cb2, 13);
    ck_assert(obj_get_code(obj, "test2", &cb) == 13);
    ck_assert(memcmp(cb, cb2, 13) == 0);

    ck_assert(obj_get_code(obj, "test", &cb) == 0);
    ck_assert(cb == NULL);

    ck_assert(val_type(obj_get_global(obj, "v1")) == TYPE_NIL);
    obj_set_global(obj, "v2", val_make_int(123));
    ck_assert(val_type(obj_get_global(obj, "v2")) == TYPE_INT);
    ck_assert(val_get_int(obj_get_global(obj, "v2")) == 123);
    ck_assert(val_type(obj_get_global(obj, "v1")) == TYPE_NIL);

    // XXX copy test case

    obj_free(obj);
}
END_TEST

TCase* make_object_checks(void) {
    TCase *tc_object;

    tc_object = tcase_create("Object");
    tcase_add_test(tc_object, test_object_01);

    return tc_object;
}

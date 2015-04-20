#include "check_types.h"

#include "types.h"

START_TEST(test_types_01) {
    val v;

    v = val_make_nil();
    ck_assert_int_eq(val_type(v), TYPE_NIL);

    v = val_make_bool(true);
    ck_assert_int_eq(val_type(v), TYPE_BOOL);
    ck_assert_int_eq(val_get_bool(v), true);
    v = val_make_bool(false);
    ck_assert_int_eq(val_type(v), TYPE_BOOL);
    ck_assert_int_eq(val_get_bool(v), false);
}
END_TEST

TCase* make_types_checks(void) {
    TCase *tc_types;

    tc_types = tcase_create("Types");
    tcase_add_test(tc_types, test_types_01);

    return tc_types;
}

#include "check_types.h"

#include <stdlib.h>
#include <stdio.h>

#include "types.h"

START_TEST(test_types_01) {
    printf("  test_types_01...\n");
    val v;

    v = val_make_nil();
    ck_assert(val_type(v) == TYPE_NIL);

    v = val_make_bool(true);
    ck_assert(val_type(v) == TYPE_BOOL);
    ck_assert(val_get_bool(v) == true);
    v = val_make_bool(false);
    ck_assert(val_type(v) == TYPE_BOOL);
    ck_assert(val_get_bool(v) == false);

    v = val_make_int(1234);
    ck_assert(val_type(v) == TYPE_INT);
    ck_assert(val_get_int(v) == 1234);
    v = val_make_int(-5678);
    ck_assert(val_type(v) == TYPE_INT);
    ck_assert(val_get_int(v) == -5678);
    v = val_make_int(0x77777777);
    ck_assert(val_type(v) == TYPE_INT);
    ck_assert(val_get_int(v) == 0x77777777);

    v = val_make_float(3.1415);
    ck_assert(val_type(v) == TYPE_FLOAT);
    ck_assert(abs(val_get_float(v) - 3.1415) < 0.00001);

    v = val_make_objref(1234567);
    ck_assert(val_type(v) == TYPE_OBJREF);
    ck_assert(val_get_objref(v) == 1234567);

}
END_TEST

START_TEST(test_types_02) {
    printf("  test_types_02...\n");
    val v;

    char *in1 = "test234";

    v = val_make_string(strlen(in1), in1);
    ck_assert(val_type(v) == TYPE_STRING);

    ck_assert(val_get_string_len(v) == 7);
    char *out = val_get_string_data(v);
    ck_assert(strncmp(out, in1, 7) == 0);

    val_dec_ref(v);
}
END_TEST

TCase* make_types_checks(void) {
    TCase *tc_types;

    tc_types = tcase_create("Types");
    tcase_add_test(tc_types, test_types_01);
    tcase_add_test(tc_types, test_types_02);

    return tc_types;
}

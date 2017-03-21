#include "check_cache.h"

#include <stdlib.h>
#include <stdio.h>

#include "cache.h"

START_TEST(test_cache_01) {
    printf("  test_cache_01...\n");

    struct cache *c = cache_new(10);

    struct lobject *lo1 = cache_get_object(c, 123);
    ck_assert_msg(lo1 == NULL, "failed to return NULL for absent item");

    struct lobject *lo2 = lobject_new();
    struct object *o2 = obj_new();
    obj_set_id(o2, 456);
    lobject_set_object(lo2, o2);
    cache_put_object(c, lo2);

    lo1 = cache_get_object(c, 123);
    ck_assert_msg(lo1 == NULL, "failed to return NULL for absent item after insert");
    struct lobject *lo3 = cache_get_object(c, 456);
    ck_assert_msg(lo3 == lo2, "failed to fetch object from cache");

    // XXX insert one that collides
    // XXX more stuff

    cache_free(c);
}
END_TEST

TCase* make_cache_checks(void) {
    TCase *tc_cache;

    tc_cache = tcase_create("Cache");
    tcase_add_test(tc_cache, test_cache_01);

    return tc_cache;
}

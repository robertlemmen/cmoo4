#include <check.h>
#include <stdlib.h>

#include "check_types.h"
#include "check_eval.h"
#include "check_object.h"
#include "check_cache.h"
#include "check_rwlock.h"

int main(int argc, char **argv) {
    Suite *s = suite_create("CMOO");

    suite_add_tcase(s, make_types_checks());
    suite_add_tcase(s, make_eval_checks());
    suite_add_tcase(s, make_object_checks());
    suite_add_tcase(s, make_cache_checks());
    suite_add_tcase(s, make_rwlock_checks());

    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);
    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#include "check_types.h"

#include <stdlib.h>
#include <stdio.h>

#include "eval.h"

struct eval_trace {
    int sum;
};

void eval_debug_callback(val v, void *a) {
    struct eval_trace *et = (struct eval_trace*)a;
    et->sum += val_get_int(v);
}

START_TEST(test_eval_01) {
    struct eval_ctx *ex = eval_new_ctx();
 
    struct eval_trace et;
    et.sum = 0;
    eval_set_dbg_handler(ex, &eval_debug_callback, &et);
  
    opcode code[] = {0x00, 0x00, 0x02, 0x01};

    eval_exec(ex, code);

    ck_assert(et.sum == 42);

    eval_free_ctx(ex); 
}
END_TEST

TCase* make_eval_checks(void) {
    TCase *tc_eval;

    tc_eval = tcase_create("Eval");
    tcase_add_test(tc_eval, test_eval_01);

    return tc_eval;
}

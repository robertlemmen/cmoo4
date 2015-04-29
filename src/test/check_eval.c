#include "check_types.h"

#include <stdlib.h>
#include <stdio.h>

#include "eval.h"

// XXX improve debug function to just create concatenated string, much better!
void eval_debug_callback(val v, void *a) {
    char *res = (char*)a;

    if (val_type(v) == TYPE_NIL) {
        strcat(res, "N");
    }
    else if (val_type(v) == TYPE_BOOL) {
        if (val_get_bool(v)) {
            strcat(res, "T");
        }
        else {
            strcat(res, "F");
        }
    }
    else if (val_type(v) == TYPE_INT) {
        char buffer[20];
        sprintf(buffer, "I%i", val_get_int(v));
        strcat(res, buffer);
    }
    else {
        ck_abort_msg("unexpected type in debug callback");
    }
}

START_TEST(test_eval_01) {
    struct eval_ctx *ex = eval_new_ctx();
 
    char trace[4096];
    trace[0] = '\0';
    eval_set_dbg_handler(ex, &eval_debug_callback, trace);
  
    opcode code[] = {   OP_NOOP, 
                        OP_NOOP, 
                        OP_DEBUGI, 0x12, 0x34, 0x56, 0x78, 
                        OP_NOOP, 
                        OP_DEBUGI, 0xC0, 0x00, 0x00, 0x00, 
                        OP_ARGS_LOCALS, 0x00, 0x03,
                        OP_DEBUGR, 0x00,
                        OP_LOAD_INT, 0x01, 0x00, 0x00, 0x00, 0x02,
                        OP_DEBUGR, 0x01,
                        OP_TYPE, 0x00, 0x01,
                        OP_DEBUGR, 0x00,
                        OP_TRUE, 0x00, 
                        OP_DEBUGR, 0x00,
                        OP_MOV, 0x02, 0x01,
                        OP_DEBUGR, 0x02,
                        OP_HALT};

    eval_exec(ex, code);

    printf("debug trace: %s\n", trace);
    ck_assert_msg(strcmp(trace, "I2018915346I192NI33554432I2TI33554432") == 0,
        "unexpected debug callback trace");

    eval_free_ctx(ex); 
}
END_TEST

START_TEST(test_eval_02) {
    struct eval_ctx *ex = eval_new_ctx();
 
    char trace[4096];
    trace[0] = '\0';
    eval_set_dbg_handler(ex, &eval_debug_callback, trace);
  
    opcode code[] = {   OP_NOOP, 
                        OP_ARGS_LOCALS, 0x00, 0x0A,
                        OP_TRUE, 0x01,
                        OP_LOGICAL_NOT, 0x00, 0x01,
                        OP_DEBUGR, 0x00,
                        OP_DEBUGR, 0x01,
                        OP_LOGICAL_OR, 0x02, 0x00, 0x01,
                        OP_DEBUGR, 0x02,
                        OP_LOGICAL_OR, 0x03, 0x01, 0x00,
                        OP_DEBUGR, 0x03,
                        OP_LOGICAL_OR, 0x04, 0x00, 0x00,
                        OP_DEBUGR, 0x04,
                        OP_LOGICAL_OR, 0x05, 0x01, 0x01,
                        OP_DEBUGR, 0x05,
                        OP_LOGICAL_AND, 0x06, 0x00, 0x01,
                        OP_DEBUGR, 0x06,
                        OP_LOGICAL_AND, 0x07, 0x01, 0x00,
                        OP_DEBUGR, 0x07,
                        OP_LOGICAL_AND, 0x08, 0x00, 0x00,
                        OP_DEBUGR, 0x08,
                        OP_LOGICAL_AND, 0x09, 0x01, 0x01,
                        OP_DEBUGR, 0x09,
                        OP_HALT};

    eval_exec(ex, code);

    printf("debug trace: %s\n", trace);
    ck_assert_msg(strcmp(trace, "FTTTFTFFFT") == 0,
        "unexpected debug callback trace");

    eval_free_ctx(ex); 
}
END_TEST

START_TEST(test_eval_03) {
    struct eval_ctx *ex = eval_new_ctx();
 
    char trace[4096];
    trace[0] = '\0';
    eval_set_dbg_handler(ex, &eval_debug_callback, trace);
  
    opcode code[] = {   OP_NOOP, 
                        OP_ARGS_LOCALS, 0x00, 0x08,
                        OP_LOAD_INT, 0x00, 0x02, 0x00, 0x00, 0x00,
                        OP_LOAD_INT, 0x01, 0x08, 0x00, 0x00, 0x00,
                        OP_ADD, 0x02, 0x00, 0x01,
                        OP_DEBUGR, 0x02,
                        OP_ADD, 0x03, 0x01, 0x00,
                        OP_DEBUGR, 0x03,
                        OP_SUB, 0x04, 0x00, 0x01,
                        OP_DEBUGR, 0x04,
                        OP_SUB, 0x05, 0x01, 0x00,
                        OP_DEBUGR, 0x05,
                        OP_MUL, 0x06, 0x00, 0x01,
                        OP_DEBUGR, 0x06,
                        OP_MUL, 0x07, 0x01, 0x00,
                        OP_DEBUGR, 0x07,
                        OP_HALT};

    eval_exec(ex, code);

    printf("debug trace: %s\n", trace);
    ck_assert_msg(strcmp(trace, "I10I10I-6I6I16I16") == 0,
        "unexpected debug callback trace");

    eval_free_ctx(ex); 
}
END_TEST

TCase* make_eval_checks(void) {
    TCase *tc_eval;

    tc_eval = tcase_create("Eval");
    tcase_add_test(tc_eval, test_eval_01);
    tcase_add_test(tc_eval, test_eval_02);
    tcase_add_test(tc_eval, test_eval_03);

    return tc_eval;
}

#include "check_eval.h"

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
    else if (val_type(v) == TYPE_FLOAT) {
        char buffer[20];
        sprintf(buffer, "f%.3f", val_get_float(v));
        strcat(res, buffer);
    }
    else if (val_type(v) == TYPE_STRING) {
        char buffer[val_get_string_len(v)+1];
        buffer[0] = 's';
        memcpy(&buffer[1], val_get_string_data(v), val_get_string_len(v));
        buffer[val_get_string_len(v)+1] = '\0';
        strcat(res, buffer);
    }
    else {
        ck_abort_msg("unexpected type in debug callback");
    }
}

START_TEST(test_eval_01) {
    printf("  test_eval_01...\n");
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
    printf("  test_eval_02...\n");
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
    printf("  test_eval_03...\n");
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

START_TEST(test_eval_04) {
    printf("  test_eval_04...\n");
    struct eval_ctx *ex = eval_new_ctx();
 
    char trace[4096];
    trace[0] = '\0';
    eval_set_dbg_handler(ex, &eval_debug_callback, trace);
  
    opcode code[] = {   OP_NOOP, 
                        OP_ARGS_LOCALS, 0x00, 0x0A,
                        OP_LOAD_INT, 0x00, 0x02, 0x00, 0x00, 0x00,
                        OP_LOAD_INT, 0x01, 0x08, 0x00, 0x00, 0x00,
                        OP_LOAD_INT, 0x02, 0x08, 0x00, 0x00, 0x00,
                        OP_EQ, 0x03, 0x00, 0x01,
                        OP_DEBUGR, 0x03,
                        OP_EQ, 0x04, 0x01, 0x02,
                        OP_DEBUGR, 0x04,
                        OP_EQ, 0x05, 0x01, 0x00,
                        OP_DEBUGR, 0x05,
                        OP_EQ, 0x06, 0x02, 0x01,
                        OP_DEBUGR, 0x06,
                        OP_EQ, 0x07, 0x03, 0x04,
                        OP_DEBUGR, 0x07,
                        OP_EQ, 0x08, 0x03, 0x05,
                        OP_DEBUGR, 0x08,
                        OP_EQ, 0x09, 0x01, 0x05,
                        OP_DEBUGR, 0x09,
                        OP_HALT};

    eval_exec(ex, code);

    printf("debug trace: %s\n", trace);
    ck_assert_msg(strcmp(trace, "FTFTFTF") == 0,
        "unexpected debug callback trace");

    eval_free_ctx(ex); 
}
END_TEST

START_TEST(test_eval_05) {
    printf("  test_eval_05...\n");
    struct eval_ctx *ex = eval_new_ctx();
 
    char trace[4096];
    trace[0] = '\0';
    eval_set_dbg_handler(ex, &eval_debug_callback, trace);
  
    opcode code[] = {   OP_NOOP, 
                        OP_ARGS_LOCALS, 0x00, 0x0B,
                        OP_LOAD_INT, 0x00, 0x01, 0x00, 0x00, 0x00,
                        OP_LOAD_INT, 0x01, 0x02, 0x00, 0x00, 0x00,
                        OP_LOAD_INT, 0x02, 0x02, 0x00, 0x00, 0x00,
                        OP_LE, 0x03, 0x00, 0x01,
                        OP_DEBUGR, 0x03,
                        OP_LE, 0x04, 0x01, 0x00,
                        OP_DEBUGR, 0x04,
                        OP_LE, 0x05, 0x01, 0x02,
                        OP_DEBUGR, 0x05,
                        OP_LE, 0x06, 0x02, 0x01,
                        OP_DEBUGR, 0x06,
                        OP_LT, 0x07, 0x00, 0x01,
                        OP_DEBUGR, 0x07,
                        OP_LT, 0x08, 0x01, 0x00,
                        OP_DEBUGR, 0x08,
                        OP_LT, 0x09, 0x01, 0x02,
                        OP_DEBUGR, 0x09,
                        OP_LT, 0x0A, 0x02, 0x01,
                        OP_DEBUGR, 0x0A,
                        OP_HALT};

    eval_exec(ex, code);

    printf("debug trace: %s\n", trace);
    ck_assert_msg(strcmp(trace, "TFTTTFFF") == 0,
        "unexpected debug callback trace");

    eval_free_ctx(ex); 
}
END_TEST

START_TEST(test_eval_06) {
    printf("  test_eval_06...\n");
    struct eval_ctx *ex = eval_new_ctx();

    char trace[4096];
    trace[0] = '\0';
    eval_set_dbg_handler(ex, &eval_debug_callback, trace);
  
    opcode code[] = {   OP_NOOP, 
                        OP_ARGS_LOCALS, 0x00, 0x04,
                        OP_LOAD_INT, 0x00, 0x06, 0x00, 0x00, 0x00,
                        OP_LOAD_INT, 0x01, 0x00, 0x00, 0x00, 0x00,
                        OP_LOAD_INT, 0x03, 0x01, 0x00, 0x00, 0x00,
                        OP_NOOP,
                        OP_NOOP,
                        OP_NOOP,
                        OP_NOOP,
                        OP_NOOP,
                        OP_NOOP,
                        OP_SUB, 0x00, 0x00, 0x03,
                        OP_DEBUGR, 0x00,
                        OP_EQ, 0x02, 0x00, 0x01,
                        OP_DEBUGR, 0x02,
                        OP_JUMP_IF, 0x02, 0x05, 0x00, 0x00, 0x00,
                        OP_JUMP, 0xE8, 0xFF, 0xFF, 0xFF,
                        OP_HALT};

    eval_exec(ex, code);

    printf("debug trace: %s\n", trace);
    ck_assert_msg(strcmp(trace, "I5FI4FI3FI2FI1FI0T") == 0,
        "unexpected debug callback trace");

    eval_free_ctx(ex); 
}
END_TEST

START_TEST(test_eval_07) {
    printf("  test_eval_07...\n");
    struct eval_ctx *ex = eval_new_ctx();

    char trace[4096];
    trace[0] = '\0';
    eval_set_dbg_handler(ex, &eval_debug_callback, trace);
  
    opcode code[] = {   OP_NOOP, 
                        OP_ARGS_LOCALS, 0x00, 0x03,
                        OP_LOAD_INT, 0x02, 0x01, 0x00, 0x00, 0x00,
                        OP_PUSH, 0x02,
                        OP_LOAD_INT, 0x02, 0x02, 0x00, 0x00, 0x00,
                        OP_PUSH, 0x02,
                        OP_LOAD_INT, 0x02, 0x03, 0x00, 0x00, 0x00,
                        OP_PUSH, 0x02,
                        OP_LOAD_INT, 0x02, 0x05, 0x00, 0x00, 0x00,
                        OP_PUSH, 0x02,
                        OP_NOOP,
                        OP_POP, 0x00,
                        OP_POP, 0x01,
                        OP_ADD, 0x02, 0x00, 0x01,
                        OP_DEBUGR, 0x02,
                        OP_PUSH, 0x02,
                        OP_POP, 0x00,
                        OP_POP, 0x01,
                        OP_ADD, 0x02, 0x00, 0x01,
                        OP_DEBUGR, 0x02,
                        OP_PUSH, 0x02,
                        OP_POP, 0x00,
                        OP_POP, 0x01,
                        OP_ADD, 0x02, 0x00, 0x01,
                        OP_DEBUGR, 0x02,
                        OP_HALT};

    eval_exec(ex, code);

    printf("debug trace: %s\n", trace);
    ck_assert_msg(strcmp(trace, "I8I10I11") == 0,
        "unexpected debug callback trace");

    eval_free_ctx(ex); 
}
END_TEST

START_TEST(test_eval_08) {
    printf("  test_eval_08...\n");
    struct eval_ctx *ex = eval_new_ctx();

    char trace[4096];
    trace[0] = '\0';
    eval_set_dbg_handler(ex, &eval_debug_callback, trace);
 
    float v1 = 1.23;
    float v2 = -345.67;
    float v3 = 0.023;
 
    opcode code[] = {   OP_NOOP, 
                        OP_ARGS_LOCALS, 0x00, 0x04,
                        OP_LOAD_FLOAT, 0x00, 0x00, 0x00, 0x00, 0x00,
                        OP_LOAD_FLOAT, 0x01, 0x00, 0x00, 0x00, 0x00,
                        OP_LOAD_FLOAT, 0x02, 0x00, 0x00, 0x00, 0x00,
                        OP_DEBUGR, 0x00,
                        OP_DEBUGR, 0x01,
                        OP_DEBUGR, 0x02,
                        OP_ADD, 0x03, 0x00, 0x01,
                        OP_DEBUGR, 0x03,
                        OP_ADD, 0x03, 0x00, 0x02,
                        OP_DEBUGR, 0x03,
                        OP_ADD, 0x03, 0x01, 0x02,
                        OP_DEBUGR, 0x03,
                        OP_SUB, 0x03, 0x00, 0x01,
                        OP_DEBUGR, 0x03,
                        OP_SUB, 0x03, 0x00, 0x02,
                        OP_DEBUGR, 0x03,
                        OP_SUB, 0x03, 0x01, 0x02,
                        OP_DEBUGR, 0x03,
                        OP_HALT};
    // XXX mul and div

    memcpy(&code[6], &v1, sizeof(float));
    memcpy(&code[12], &v2, sizeof(float));
    memcpy(&code[18], &v3, sizeof(float));

    eval_exec(ex, code);

    printf("debug trace: %s\n", trace);
    ck_assert_msg(strcmp(trace, "f1.230f-345.670f0.023f-344.440f1.253f-345.647f346.900f1.207f-345.693") == 0,
        "unexpected debug callback trace");

    eval_free_ctx(ex); 
}
END_TEST

// XXX test for float eq,lt,le

int scall_count = 0;
val sys_t0(void *ctx) {
    scall_count++;
}

val sys_t1(void *ctx, val arg) {
    scall_count += val_get_int(arg);
}

START_TEST(test_eval_09_syscall) {
    printf("  test_eval_09_syscall...\n");
    struct eval_ctx *ex = eval_new_ctx();
    struct syscall_table *st = syscall_table_new();
    syscall_table_add_a0(st, "sys_t0", sys_t0);
    syscall_table_add_a1(st, "sys_t1", sys_t1);
    eval_set_syscall_table(ex, st);
    scall_count = 0;

    opcode code[] = {   OP_NOOP, 
                        OP_ARGS_LOCALS, 0x00, 0x03,
                        OP_LOAD_STRING, 0x00, 0x06, 0x00, 's', 'y', 's', '_', 't', '0',
                        OP_LOAD_STRING, 0x01, 0x06, 0x00, 's', 'y', 's', '_', 't', '1',
                        OP_LOAD_INT, 0x02, 0x05, 0x00, 0x00, 0x00,
                        OP_PUSH, 0x00,
                        OP_SYSCALL, 0x00,
                        OP_PUSH, 0x01,
                        OP_PUSH, 0x02,
                        OP_SYSCALL, 0x01,
                        OP_HALT};

    ck_assert_msg(scall_count == 0, "syscall test pre-condition not met");
    eval_exec(ex, code);
    ck_assert_msg(scall_count == 6, "syscall not executed as expected");

    eval_free_ctx(ex);
    syscall_table_free(st);
}
END_TEST

START_TEST(test_eval_10_string) {
    printf("  test_eval_10_string...\n");
    struct eval_ctx *ex = eval_new_ctx();

    char trace[4096];
    trace[0] = '\0';
    eval_set_dbg_handler(ex, &eval_debug_callback, trace);

    opcode code[] = {   OP_NOOP, 
                        OP_ARGS_LOCALS, 0x00, 0x06,
                        OP_LOAD_STRING, 0x00, 0x04, 0x00, 't', 'e', 's', 't',
                        OP_LOAD_STRING, 0x01, 0x04, 0x00, 't', 'e', 's', 't',
                        OP_LOAD_STRING, 0x02, 0x05, 0x00, 'z', 'i', 'c', 'k', 'e',
                        OP_DEBUGR, 0x00,
                        OP_DEBUGR, 0x01,
                        OP_DEBUGR, 0x02,
                        OP_EQ, 0x03, 0x00, 0x01,
                        OP_DEBUGR, 0x03,
                        OP_EQ, 0x03, 0x01, 0x02,
                        OP_DEBUGR, 0x03,
                        OP_LOAD_STRING, 0x03, 0x00, 0x00, 
                        OP_LOAD_INT, 0x04, 0x00, 0x00, 0x00, 0x00,
                        OP_LENGTH, 0x05, 0x03,
                        OP_DEBUGR, 0x05,
                        OP_LENGTH, 0x05, 0x01,
                        OP_DEBUGR, 0x05,
                        OP_CONCAT, 0x03, 0x01, 0x02,
                        OP_LENGTH, 0x05, 0x03,
                        OP_DEBUGR, 0x05,
                        OP_DEBUGR, 0x03,
                        OP_HALT};

    eval_exec(ex, code);

    printf("debug trace: %s\n", trace);
    ck_assert_msg(strcmp(trace, "steststestszickeTFI0I4I9stestzicke") == 0,
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
    tcase_add_test(tc_eval, test_eval_04);
    tcase_add_test(tc_eval, test_eval_05);
    tcase_add_test(tc_eval, test_eval_06);
    tcase_add_test(tc_eval, test_eval_07);
    tcase_add_test(tc_eval, test_eval_08);
    tcase_add_test(tc_eval, test_eval_09_syscall);
    tcase_add_test(tc_eval, test_eval_10_string);

    return tc_eval;
}

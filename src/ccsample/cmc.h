#ifndef CMC_H
#define CMC_H

#define MODE_SLOT       0
#define MODE_COMPUNIT   1
#define MODE_OFF        2

// XXX fro now we need access to the ast directly, this is also in the deps of
// the makefile
#include "cc_ast.h"

struct cmc_ctx {
    int mode;
    ast_node *resp;
    char *error_msg;
    int error;
    YYNODESTATE ast_state;
};

struct cmc_ctx* cmc_parse(char *buffer, int mode);

#endif /* CMC_H */

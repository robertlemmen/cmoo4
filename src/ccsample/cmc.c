#include "cmc.h"

#include <stdlib.h>

#include "cc_par.h"
#include "cc_lex.h"
#include "cc_ast.h"

struct cmc_ctx* cmc_parse(char *buffer, int mode) {

    struct cmc_ctx *ctx = malloc(sizeof(struct cmc_ctx));
    yynodeinit(&ctx->ast_state);
    ctx->mode = mode;
    ctx->resp = NULL;
    ctx->error_msg = NULL;
    yyscan_t scanner;
    yylex_init(&scanner);
    yylex_init_extra(ctx, &scanner);
    yy_scan_string(buffer, scanner);
    ctx->error = yyparse(scanner);
    yylex_destroy(scanner);
    free(buffer);
    return ctx;
}

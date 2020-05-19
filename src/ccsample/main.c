#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "cmc.h"
// XXX fro now we need access to the ast directly, also in the deps in the
// makefile
#include "cc_ast.h"

size_t readfile(FILE *in, char **buffer) {
    size_t size = 0;
    size_t bufsize = 0;
    bool finished = 0;
    while (!finished) {
        if (bufsize < (size + 4096)) {
            bufsize += 4096 * 512;
            *buffer = realloc(*buffer, bufsize);
        }
        size_t n = fread(*buffer + size, 1, bufsize - size - 1, in);
        if (n < (bufsize - size - 1)) {
            finished = true;
        }
        size += n;
    }
    (*buffer)[size] = 0;
    return size;
}

int main(void) {
    char *buffer = NULL;
    readfile(stdin, &buffer);
    struct cmc_ctx *ctx = cmc_parse(buffer, MODE_COMPUNIT);

    if (!ctx->error) {
        printf("------------------\n");
        dump(ctx->resp, 0, false, false);
        printf("------------------\n");
    }
    else {
        fprintf(stderr, ctx->error_msg);
    }

    return 0;
}

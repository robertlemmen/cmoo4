#include <stdio.h>
#include <stdbool.h>

#include "cc_par.h"
#include "cc_lex.h"
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

    YYNODESTATE ast_state;
    yynodeinit(&ast_state);

    yyscan_t scanner;
    yylex_init(&scanner);
    yylex_init_extra(&ast_state, &scanner);
    yy_scan_string(buffer, scanner);
    yyparse(scanner);
    yylex_destroy(scanner);

    free(buffer);

    return 0;
}

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "./ast.h"
#include "./lexer.h"
#include "./parser.h"
#include "./runner.h"
#include "./strings.h"
#include "./symbols.h"
#include "./tokens.h"
#include "./tst.h"
#include "./typer.h"
#include "serene.h"

int main(int argc, char **argv) {
    struct serene_Arena strings_arena, ast_arena, type_arena, exec_arena;
    strings_arena = ast_arena = type_arena = exec_arena = (struct serene_Arena){
        .backing = serene_Libc_dyn(),
    };

    if (argc != 2) {
        printf("Please provide a filename!\n");
        return 1;
    }
    char *filename = argv[1];
    struct stat filestat = {0};
    if (stat(filename, &filestat) == -1) {
        printf("Could not access file '%s'!\n", filename);
        return 1;
    }
    int filedesc = open(filename, O_RDONLY);
    char *file =
        mmap(NULL, filestat.st_size, PROT_READ, MAP_PRIVATE, filedesc, 0);

    struct Intern intern = {0};
    struct Symbols symbols =
        populate_interner(serene_Arena_dyn(&strings_arena), &intern);

    struct Tokenvec tokens =
        lex(serene_Arena_dyn(&strings_arena), &intern, file);

    munmap(file, filestat.st_size);
    close(filedesc);

    printf("[\n");
    struct Tokenstream stream = Tokenvec_stream(&tokens);
    for (size_t i = 0; i < stream.len; i++) {
        printf("\t'%s'\n", stream.buf[i].spelling);
    }
    printf("]\n");

    struct Opdecls ops = {0};

    struct Ast ast = parse(
        serene_Arena_dyn(&ast_arena), ops, symbols, Tokenvec_stream(&tokens)
    );

    struct Tst tst = typecheck(serene_Arena_dyn(&type_arena), symbols, ast);

    run(serene_Arena_dyn(&exec_arena), symbols, ast);

    Tokenvec_deinit(&tokens);
    serene_Arena_deinit(&strings_arena);
    serene_Arena_deinit(&ast_arena);
    serene_Arena_deinit(&type_arena);
    serene_Arena_deinit(&exec_arena);
    printf("\n");
    return 0;
}

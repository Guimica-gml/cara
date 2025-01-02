#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "./arena.h"
#include "./strings.h"
#include "./symbols.h"
#include "./tokens.h"
#include "./lexer.h"
#include "./ast.h"
#include "./parser.h"
#include "./typer.h"
#include "./runner.h"

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Please provide a filename!\n");
        return 1;
    }
    char* filename = argv[1];
    struct stat filestat = {0};
    if (stat(filename, &filestat) == -1) {
        printf("Could not access file '%s'!\n", filename);
        return 1;
    }
    int filedesc = open(filename, O_RDONLY);
    char* file = mmap(
        NULL, filestat.st_size,
        PROT_READ, MAP_PRIVATE,
        filedesc, 0
    );

    struct Arena strings_arena = {0};
    struct Intern intern = {0};
    assert(arena_init(&strings_arena, 4096));
    struct Symbols symbols = populate_interner(&strings_arena, &intern);

    struct Tokenvec tokens = lex(&strings_arena, &intern, file);

    munmap(file, filestat.st_size);
    close(filedesc);
    
    printf("[\n");
    struct Tokenstream stream = Tokenvec_stream(&tokens);
    for (size_t i = 0; i < stream.len; i++) {
        printf("\t'%s'\n", stream.buf[i].spelling);
    }
    printf("]\n");

    struct Arena ast_arena = {0};
    assert(arena_init(&ast_arena, 4096) && "huh");
    struct Opdecls ops = {0};
    
    struct Ast ast = parse(&ast_arena, ops, symbols, Tokenvec_stream(&tokens));

    struct Arena type_arena = {0};
    assert(arena_init(&type_arena, 4096));
    typecheck(&type_arena, symbols, ast);
    arena_deinit(&type_arena);

    struct Arena exec_arena = {0};
    assert(arena_init(&exec_arena, 4096));
    run(&exec_arena, symbols, ast);
    arena_deinit(&exec_arena);
    
    arena_deinit(&ast_arena);
    Tokenvec_deinit(&tokens);
    arena_deinit(&strings_arena);
    return 0;
}

#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "./lexer.h"
#include "./arena.h"
#include "./parser.h"

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

    struct Tokens tokens = lex(file);
    
    printf("[\n");
    struct Tokenstream stream = Tokens_stream(&tokens);
    for (
        struct Stringview* s = Tokenstream_first_text(&stream);
        Tokenstream_drop(&stream);
        s = Tokenstream_first_text(&stream)
    ) {
        printf("\t'%.*s'\n", (int) s->len, s->str);
    }
    printf("]\n");

    struct Arena ast_arena = {0};
    assert(arena_init(&ast_arena, 4096) && "huh");

    struct Opdecls ops = {0};
    Opdecls_push(
        &ops,
        (struct Opdecl){
            .lbp = 0,
            .rbp = 0,
            .token =
            (struct Stringview){
                .str = ",",
                .len = 1,
            },
        } 
    );
    
    struct Ast ast = parse(&ast_arena, ops, Tokens_stream(&tokens));

    arena_deinit(&ast_arena);
    Tokens_deinit(&tokens);
    munmap(file, filestat.st_size);
    close(filedesc);
    return 0;
}

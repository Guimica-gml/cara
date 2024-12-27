#include <stdio.h>
#include <assert.h>

#include "./lexer.h"
#include "./lexer.c"
#include "./arena.h"
#include "./arena.c"

/*
#define LIST_NAME MyInts
#define LIST_TYPE int
#include "./segment_list.c"
*/

int main() {
    char str[] =
        "func main() {\n"
        "    print(\"Hello World!\");\n"
        "    return;\n"
        "}";
    
    struct Arena arena = {0};
    assert(arena_init(&arena, 4096) && "could not dedicate a page of memory!");
    struct Tokens tokens = lex(&arena, str);
    printf("[\n");
    for (size_t i = 0; i < tokens.len; i++) {
        printf("\t%s\n", lexer_tokenkind_name(*Tokens_at(&tokens, i)));
    }
    printf("]\n");
    arena_deinit(&arena);
    return 0;
}

#include <stdio.h>
#include <assert.h>

#include "./lexer.h"
#include "./arena.h"
#include "./parser.h"

int main() {
    char str[] =
        "func main() {\n"
        "    print(\"Hello World!\");\n"
        "    return;\n"
        "}";
    struct Tokens tokens = lex(str);
    
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

    struct Ast ast = parse(&ast_arena, Tokens_stream(&tokens));

    arena_deinit(&ast_arena);
    Tokens_deinit(&tokens);
    return 0;
}

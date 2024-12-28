#include <stdio.h>
#include <assert.h>

#include "./lexer.h"
#include "./lexer.c"
#include "./arena.h"
#include "./arena.c"

int main() {
    char str[] =
        "func main() {\n"
        "    print(\"Hello World!\");\n"
        "    return;\n"
        "}";
    struct Tokens tokens = lex(str);
    
    printf("[\n");
    for (size_t i = 0; i < tokens.len; i++) {
        printf("\t%s\n", lexer_tokenkind_name(*Tokens_at(&tokens, i)));
    }
    printf("]\n");
    
    Tokens_deinit(&tokens);
    return 0;
}

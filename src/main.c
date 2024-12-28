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
    struct Tokenstream tokens = lex(str);
    
    printf("[\n");
    for (size_t i = 0; i < tokens.strings.len; i++) {
        struct StringView *s = Tokenstrings_at(&tokens.strings, i);
        printf("\t'%.*s'\n", (int) s->len, s->str);
    }
    printf("]\n");
    
    Tokenstrings_deinit(&tokens.strings);
    Tokenkinds_deinit(&tokens.kinds);
    return 0;
}

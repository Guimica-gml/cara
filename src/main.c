#include <stdio.h>
#include <assert.h>

#include "./lexer.h"
//#include "./arena.h"

int main() {
    char str[] =
        "func main() {\n"
        "    print(\"Hello World!\");\n"
        "    return;\n"
        "}";
    struct Tokenstream tokens = lex(str);
    
    printf("[\n");
    for (size_t i = 0; i < tokens.strings.len; i++) {
        struct Stringview *s = Tokenstrings_at(&tokens.strings, i);
        printf("\t'%.*s'\n", (int) s->len, s->str);
    }
    printf("]\n");

    Tokenstream_deinit(&tokens);
    return 0;
}

#ifndef PARSER_C
#define PARSER_C

#include "./parser.h"

bool decls_function(struct Arena*, struct Tokenstream*, struct Function*);

struct Ast parse(struct Arena* arena, struct Tokenstream *toks) {
    struct Functions funcs = {0};
    while (true) {
        struct Function dest = {0};
        if (!decls_function(arena, toks, &dest)) break;
        assert(Functions_push(&funcs, arena, dest));
    }
}

bool decls_function(
    struct Arena *arena,
    struct Tokenstream* toks,
    struct Function* out
) {
    if (!Tokenstream_drop_text(toks, "func")) return false;
    struct StringView* name = Tokenstrings_first(toks->strings);
    if (!name) return false;
    binding_parenthesised(arena, toks);
    TODO
}

#endif

#include "./symbols.h"

struct Symbols
populate_interner(struct serene_Allocator alloc, struct Intern *intern) {
#define ins(s) Intern_insert(intern, alloc, s, sizeof(s) / sizeof(char) - 1)
    struct Symbols out = {0};

    out.s_main = ins("main");
    out.s_unit = ins("()");
    out.s_int = ins("int");
    out.s_bool = ins("bool");
    out.s_true = ins("true");
    out.s_false = ins("false");
    out.s_string = ins("string");
    out.s_star = ins("*");

    return out;
#undef ins
}

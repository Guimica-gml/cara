#ifndef SYMBOLS_H
#define SYMBOLS_H

#include "./arena.h"
#include "./strings.h"

struct Symbols {
    const char* s_unit;
    const char* s_int;
    const char* s_bool;
    const char* s_string;
    const char* s_star;
};

struct Symbols populate_interner(struct Arena* arena, struct Intern* intern) {
#define ins(s) Intern_insert(intern, arena, s, sizeof(s)/sizeof(char) - 1)
    struct Symbols out = {0};

    out.s_unit = ins("()");
    out.s_int = ins("int");
    out.s_bool = ins("bool");
    out.s_string = ins("string");
    out.s_star = ins("*");
    
    return out;
#undef ins
}

#endif

#ifndef SYMBOLS_H
#define SYMBOLS_H

#include "./arena.h"
#include "./strings.h"

struct Symbols {
    const char *s_unit;
    const char *s_int;
    const char *s_bool;
    const char *s_false;
    const char *s_true;
    const char *s_string;
    const char *s_star;
    const char *s_main;
};

struct Symbols populate_interner(struct Arena *, struct Intern *);

#endif

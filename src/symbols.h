#ifndef SYMBOLS_H
#define SYMBOLS_H

#include "./strings.h"
#include "serene.h"

struct Symbols {
    const char* s_unit;
    const char* s_int;
    const char* s_bool;
    const char* s_false;
    const char* s_true;
    const char* s_string;
    const char* s_star;
    const char* s_main;

    const char* s_badd;
    const char* s_bsub;
    const char* s_bmul;
    const char* s_bdiv;
    const char* s_bmod;
    const char* s_bneg;
    const char* s_band;
    const char* s_bor;
    const char* s_bxor;
    const char* s_bnot;
    const char* s_bshl;
    const char* s_bshr;
    const char* s_bcmpEQ;
    const char* s_bcmpNE;
    const char* s_bcmpGT;
    const char* s_bcmpLT;
    const char* s_bcmpGE;
    const char* s_bcmpLE;
    const char* s_syscall;
    const char* s_bptr_to_int;
    const char* s_bint_to_ptr;
};

struct Symbols populate_interner(struct Intern *);

#endif

#ifndef SYMBOLS_H
#define SYMBOLS_H

#include "./strings.h"
#include "serene.h"

struct Symbols {
    struct String s_unit;
    struct String s_int;
    struct String s_int8;
    struct String s_int16;
    struct String s_int32;
    struct String s_int64;
    struct String s_uint;
    struct String s_uint8;
    struct String s_uint16;
    struct String s_uint32;
    struct String s_uint64;
    struct String s_bool;
    struct String s_false;
    struct String s_true;
    struct String s_string;
    struct String s_star;
    struct String s_main;

    struct String s_badd;
    struct String s_bsub;
    struct String s_bmul;
    struct String s_bdiv;
    struct String s_bmod;
    struct String s_bneg;
    struct String s_band;
    struct String s_bor;
    struct String s_bxor;
    struct String s_bnot;
    struct String s_bshl;
    struct String s_bshr;
    struct String s_bcmpEQ;
    struct String s_bcmpNE;
    struct String s_bcmpGT;
    struct String s_bcmpLT;
    struct String s_bcmpGE;
    struct String s_bcmpLE;
    struct String s_syscall;
    struct String s_bptr_to_int;
    struct String s_bint_to_ptr;
};

struct Symbols populate_interner(struct Intern *);

#endif

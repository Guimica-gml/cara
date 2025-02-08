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

    out.s_badd = ins("__builtin_add");
    out.s_bsub = ins("__builtin_sub");
    out.s_bmul = ins("__builtin_mul");
    out.s_bdiv = ins("__builtin_div");
    out.s_bmod = ins("__builtin_mod");
    out.s_bneg = ins("__builtin_neg");
    out.s_band = ins("__builtin_and");
    out.s_bor = ins("__builtin_or");
    out.s_bxor = ins("__builtin_xor");
    out.s_bnot = ins("__builtin_not");
    out.s_bshl = ins("__builtin_shl");
    out.s_bshr = ins("__builtin_shr");
    out.s_bcmpEQ = ins("__builtin_cmp_eq");
    out.s_bcmpNE = ins("__builtin_cmp_ne");
    out.s_bcmpGT = ins("__builtin_cmp_gt");
    out.s_bcmpLT = ins("__builtin_cmp_lt");
    out.s_bcmpGE = ins("__builtin_cmp_ge");
    out.s_bcmpLE = ins("__builtin_cmp_le");
    out.s_syscall = ins("__builtin_syscall");

    return out;
#undef ins
}

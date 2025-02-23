#include "./symbols.h"

struct Symbols
populate_interner(struct Intern *intern) {
#define ins(s) Intern_insert(intern, (struct String){s, sizeof(s) / sizeof(char) - 1})
    struct Symbols out = {0};

    out.s_main = ins("main");
    out.s_unit = ins("()");
    out.s_int   = ins("int");
    out.s_int8  = ins("int8");
    out.s_int16 = ins("int16");
    out.s_int32 = ins("int32");
    out.s_int64 = ins("int64");
    out.s_uint   = ins("uint");
    out.s_uint8  = ins("uint8");
    out.s_uint16 = ins("uint16");
    out.s_uint32 = ins("uint32");
    out.s_uint64 = ins("uint64");
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
    out.s_bptr_to_int = ins("__builtin_ptr_to_int");
    out.s_bint_to_ptr = ins("__builtin_int_to_ptr");

    return out;
#undef ins
}

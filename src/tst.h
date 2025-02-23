#ifndef TST_H
#define TST_H

#include "strings.h"

struct tst_Binding;
struct tst_Expr;
struct tst_ExprsLL;
struct tst_Type;
struct tst_Function;
struct Tst;

enum tst_TypeTag {
    TTT_Unit,
    TTT_Int,
    TTT_Int8,
    TTT_Int16,
    TTT_Int32,
    TTT_Int64,
    TTT_Bool,
    TTT_String,
    TTT_Star,
    TTT_Func,
};
struct tst_TypeFunc;
struct tst_TypeStar;

struct tst_Type {
    enum tst_TypeTag tag;
    union {
        struct tst_TypeFunc* func;
        struct tst_TypeStar* star;
    };
};

struct tst_TypeFunc {
    struct tst_Type args;
    struct tst_Type ret;
};
struct tst_TypeStar {
    struct tst_TypeStar *next;
    struct tst_Type current;
};

enum tst_BindingTag {
    TBT_Empty,
    TBT_Name,
    TBT_Tuple,
};
struct tst_BindingTuple;

struct tst_Binding {
    enum tst_BindingTag tag;
    union {
        struct {
            struct String name;
            struct tst_Type type;
        } name;
        struct tst_BindingTuple *tuple;
    };
};

struct tst_BindingTuple {
    struct tst_BindingTuple* next;
    struct tst_Binding current;
};

enum tst_ExprTag {
    TET_Unit = 0,
    TET_If,
    TET_Loop,
    TET_Bareblock,
    TET_Call,
    TET_Recall,
    TET_NumberLit,
    TET_StringLit,
    TET_BoolLit,
    TET_Tuple,
    TET_Builtin,

    TST_Let,
    TST_Break,
    TST_Return,
    TST_Assign,
    TST_Const,
};
struct tst_ExprIf;
struct tst_ExprCall;
struct tst_ExprTuple;
struct tst_ExprLet;
struct tst_ExprAssign;
enum tst_ExprBuiltin {
    EB_badd,
    EB_bsub,
    EB_bmul,
    EB_bdiv,
    EB_bmod,
    EB_bneg,
    EB_band,
    EB_bor,
    EB_bxor,
    EB_bnot,
    EB_bshl,
    EB_bshr,
    EB_bcmpEQ,
    EB_bcmpNE,
    EB_bcmpGT,
    EB_bcmpLT,
    EB_bcmpGE,
    EB_bcmpLE,
    EB_syscall,
    EB_ptr_to_int,
    EB_int_to_ptr,
};

struct tst_Expr {
    enum tst_ExprTag tag;
    struct tst_Type type;
    union {
        struct tst_ExprIf *if_expr;
        struct tst_ExprCall* call;
        struct tst_ExprTuple* tuple;
        struct tst_Expr *loop;
        struct tst_ExprsLL* bareblock;
        enum tst_ExprBuiltin builtin;
        struct String lit;

        struct tst_ExprLet* let;
        struct tst_ExprAssign *assign;
        struct tst_Expr *break_stmt;
        struct tst_Expr *return_stmt;
        struct tst_Expr *const_stmt;
    };
};
struct tst_ExprsLL {
    struct tst_Expr current;
    struct tst_ExprsLL *next;
};

struct tst_ExprIf {
    struct tst_Expr cond;
    struct tst_Expr smash;
    struct tst_Expr pass;
};
struct tst_ExprCall {
    struct tst_Expr name;
    struct tst_Expr args;
};
struct tst_ExprTuple {
    struct tst_ExprTuple *next;
    struct tst_Expr current;
};
struct tst_ExprLet {
    struct tst_Binding bind;
    struct tst_Expr init;
};
struct tst_ExprAssign {
    struct String name;
    struct tst_Expr expr;
};

struct tst_Function {
    struct String name;
    struct tst_Type type;
    struct tst_Binding args;
    struct tst_Expr body;
};

struct Tst {
    struct tst_FunctionsLL {
        struct tst_Function current;
        struct tst_FunctionsLL *next;
    } *funcs;
};

#endif

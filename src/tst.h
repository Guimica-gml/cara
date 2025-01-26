#ifndef TST_H
#define TST_H

struct tst_Binding;
struct tst_Expr;
struct tst_ExprsLL;
struct tst_Type;
struct tst_Function;
struct Tst;

enum tst_TypeTag {
    TTT_Unit,
    TTT_Int,
    TTT_Bool,
    TTT_String,
    TTT_Star,
    TTT_Func,
    /* TTT_Call, */
    /* TTT_Comma, */
};
struct tst_Type {
    enum tst_TypeTag tag;
    union {
        struct {
            struct tst_Type *args;
            struct tst_Type *ret;
        } func;
        struct {
            struct tst_Type *lhs;
            struct tst_Type *rhs;
        } star;

        /* struct { */
        /*     struct tst_Type *name; */
        /*     struct tst_Type *args; */
        /* } call; */
        /* struct { */
        /*     struct tst_Type *lhs; */
        /*     struct tst_Type *rhs; */
        /* } comma; */
    };
};

enum tst_BindingTag {
    TBT_Empty,
    TBT_Name,
    TBT_Comma,
};
struct tst_Binding {
    enum tst_BindingTag tag;
    union {
        struct {
            const char *name;
            struct tst_Type type;
        } name;
        struct {
            struct tst_Binding *lhs;
            struct tst_Binding *rhs;
        } comma;
    };
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
    TET_Comma,

    TST_Let,
    TST_Break,
    TST_Return,
    TST_Assign,
    TST_Const,
};
struct tst_Expr {
    enum tst_ExprTag tag;
    struct tst_Type type;
    union {
        struct {
            struct tst_Expr *cond;
            struct tst_Expr *smash;
            struct tst_Expr *pass;
        } if_expr;
        struct {
            struct tst_Expr *name;
            struct tst_Expr *args;
        } call;
        struct {
            struct tst_Expr *lhs;
            struct tst_Expr *rhs;
        } comma;
        struct tst_Expr *loop;
        struct tst_ExprsLL *bareblock;
        const char *lit;

        struct {
            struct tst_Binding bind;
            struct tst_Expr *init;
        } let;
        struct {
            const char *name;
            struct tst_Expr *expr;
        } assign;
        struct tst_Expr *break_stmt;
        struct tst_Expr *return_stmt;
        struct tst_Expr *const_stmt;
    };
};
struct tst_ExprsLL {
    struct tst_Expr current;
    struct tst_ExprsLL *next;
};

struct tst_Function {
    const char *name;
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

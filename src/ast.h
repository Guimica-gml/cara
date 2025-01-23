#ifndef TYPES_H
#define TYPES_H

#include "./symbols.h"

struct Binding;
struct Expr;
struct ExprsLL;
struct Type;
struct Function;
struct Ast;

enum TypeTag {
    TT_Func,
    TT_Call,
    TT_Recall,
    TT_Comma,
    TT_Var,
};
struct Type {
    enum TypeTag tag;
    union {
        struct {
            struct Type *args;
            struct Type *ret;
        } func;
        struct {
            struct Type *name;
            struct Type *args;
        } call;
        struct {
            struct Type *lhs;
            struct Type *rhs;
        } comma;
        const char *recall;
        int var;
    };
};

struct Type Binding_to_type(struct Symbols, struct Binding);

struct Type Type_unit(struct Symbols);
struct Type Type_bool(struct Symbols);
struct Type Type_int(struct Symbols);
struct Type Type_string(struct Symbols);
struct Type Type_product(
    struct serene_Allocator, struct Symbols, struct Type, struct Type
);

enum BindingTag {
    BT_Empty,
    BT_Name,
};
struct Binding {
    enum BindingTag tag;
    union {
        struct {
            const char *name;
            struct Type annot;
        } name;
    };
};

enum ExprTag {
    ET_Unit = 0,
    ET_If,
    ET_Loop,
    ET_Bareblock,
    ET_Call,
    ET_Recall,
    ET_NumberLit,
    ET_StringLit,
    ET_BoolLit,
    ET_Comma,

    ST_Let,
    ST_Mut,
    ST_Break,
    ST_Return,
    ST_Assign,
    ST_Const,
};
struct Expr {
    enum ExprTag tag;
    struct Type type;
    union {
        struct {
            struct Expr *cond;
            struct Expr *smash;
            struct Expr *pass;
        } if_expr;
        struct {
            struct Expr *name;
            struct Expr *args;
        } call;
        struct {
            struct Expr *lhs;
            struct Expr *rhs;
        } comma;
        struct Expr *loop;
        struct ExprsLL *bareblock;
        const char *lit;

        struct {
            struct Binding bind;
            struct Expr *init;
        } let;
        struct {
            const char *name;
            struct Expr *expr;
        } assign;
        struct Expr *break_stmt;
        struct Expr *return_stmt;
        struct Expr *const_stmt;
    };
};
struct ExprsLL {
    struct Expr current;
    struct ExprsLL *next;
};

struct Function {
    const char *name;
    struct Type ret;
    struct Binding args;
    struct Expr body;
};

struct Ast {
    struct FunctionsLL {
        struct Function current;
        struct FunctionsLL *next;
    } *funcs;
};

#endif

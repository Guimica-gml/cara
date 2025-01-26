#ifndef AST_H
#define AST_H

#include "./symbols.h"
#include "./types.h"
#include "typereg.h"

struct Typesyms {
    const struct Type *t_unit;
    const struct Type *t_bool;
    const struct Type *t_int;
    const struct Type *t_string;
    const struct Type *t_star;
};

struct TypeIntern {
    struct serene_Allocator alloc;
    struct Typereg tree;
    struct Typesyms tsyms;
    struct Symbols syms;
};
struct TypeIntern TypeIntern_init(struct serene_Allocator, struct Symbols);
const struct Type *TypeIntern_intern(struct TypeIntern *, struct Type *);

const struct Type *Type_recall(struct TypeIntern *, const char *);
const struct Type *Type_func(struct TypeIntern *, const struct Type *, const struct Type *);
const struct Type *Type_comma(struct TypeIntern *, const struct Type *, const struct Type *);
const struct Type *Type_call(struct TypeIntern *, const struct Type *, const struct Type *);
const struct Type *Type_product(
    struct TypeIntern *, const struct Type *, const struct Type *
);

struct Binding;
struct Expr;
struct ExprsLL;
struct Function;
struct Ast;

enum BindingTag {
    BT_Empty,
    BT_Name,
    BT_Comma,
};
struct Binding {
    enum BindingTag tag;
    union {
        struct Type const *empty;
        struct {
            const char *name;
            struct Type const *annot;
        } name;
        struct {
            struct Binding *lhs;
            struct Binding *rhs;
        } comma;
    };
};

const struct Type *Binding_to_type(struct TypeIntern *, struct Binding);

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
    struct Type const *type;
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
    struct Type const *ret;
    struct Binding args;
    struct Expr body;
};

struct Ast {
    struct FunctionsLL {
        struct Function current;
        struct FunctionsLL *next;
    } *funcs;
};

void Ast_print(struct Ast *);

#endif

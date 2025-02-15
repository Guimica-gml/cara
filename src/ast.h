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
    int var_counter;
};
void TypeIntern_print(struct TypeIntern *);
struct TypeIntern TypeIntern_init(struct serene_Allocator, struct Symbols);
const struct Type *TypeIntern_intern(struct TypeIntern *, struct Type *);

const struct Type* Type_recall(struct TypeIntern *, const char *name);
const struct Type* Type_func(struct TypeIntern *, const struct Type *args, const struct Type *ret);
const struct Type* Type_tuple(struct TypeIntern*, const struct Type* lhs, const struct Type* rhs);
const struct Type* Type_tuple_extend(struct TypeIntern*, const struct Type* tail, const struct Type* head);
const struct Type* Type_call(struct TypeIntern*, const struct Type* name, const struct Type* args);
const struct Type* Type_new_typevar(struct TypeIntern*);

struct Binding;
struct Expr;
struct ExprsLL;
struct Function;
struct Ast;

enum BindingTag {
    BT_Empty,
    BT_Name,
    BT_Tuple,
};
struct BindingTuple;

struct Binding {
    enum BindingTag tag;
    union {
        const struct Type *empty;
        struct {
            const char *name;
            const struct Type *annot;
        } name;
        struct BindingTuple *tuple;
    };
};

struct BindingTuple {
    struct BindingTuple* next;
    struct Binding current;
};

const struct Type *Binding_to_type(struct TypeIntern *, struct Binding);

enum ExprTag {
    ET_If,
    ET_Loop,
    ET_Bareblock,
    ET_Call,
    ET_Recall,
    ET_NumberLit,
    ET_StringLit,
    ET_BoolLit,
    ET_Tuple,

    ST_Let,
    ST_Mut,
    ST_Break,
    ST_Return,
    ST_Assign,
    ST_Const,
};
struct ExprIf;
struct ExprCall;
struct ExprTuple {
    struct ExprsLL* last;
    struct ExprsLL* list;
};
struct ExprLet;
struct ExprAssign;

struct Expr {
    enum ExprTag tag;
    const struct Type* type;
    union {
        struct ExprIf *if_expr;
        struct ExprCall *call;
        struct ExprTuple tuple;
        struct Expr *loop;
        struct ExprsLL *bareblock;
        const char *lit;

        struct ExprLet *let;
        struct ExprAssign *assign;
        struct Expr *break_stmt;
        struct Expr *return_stmt;
        struct Expr *const_stmt;
    };
};

struct ExprsLL {
    struct ExprsLL* next;
    struct Expr current;
};

struct ExprIf {
    struct Expr cond;
    struct Expr smash;
    struct Expr pass;
};
struct ExprCall {
    struct Expr name;
    struct Expr args;
};
struct ExprLet {
    struct Binding bind;
    struct Expr init;
};
struct ExprAssign {
    const char *name;
    struct Expr expr;
};

struct Expr Expr_unit(struct TypeIntern*);
struct Expr Expr_call(struct serene_Allocator, struct TypeIntern*, struct Expr name, struct Expr args);
struct Expr Expr_if(struct serene_Allocator, struct Expr cond, struct Expr smash, struct Expr pass);
struct Expr Expr_loop(struct serene_Allocator, struct TypeIntern*, struct Expr body);
struct Expr Expr_let(struct serene_Allocator, struct TypeIntern*, struct Binding binding, struct Expr init);
struct Expr Expr_mut(struct serene_Allocator, struct TypeIntern*, struct Binding binding, struct Expr init);
struct Expr Expr_recall(struct TypeIntern*, const char* name);
struct Expr Expr_number(struct TypeIntern*, const char* lit);
struct Expr Expr_string(struct TypeIntern*, const char* lit);
struct Expr Expr_bool(struct TypeIntern*, const char* lit);
struct Expr Expr_assign(struct serene_Allocator, struct TypeIntern*, const char* name, struct Expr value);
struct Expr Expr_break(struct serene_Allocator, struct TypeIntern*, struct Expr body);
struct Expr Expr_return(struct serene_Allocator, struct TypeIntern*, struct Expr body);
struct Expr Expr_const(struct serene_Allocator, struct TypeIntern*, struct Expr body);
struct Expr Expr_tuple(struct serene_Allocator, struct TypeIntern *, struct Expr lhs, struct Expr rhs);
struct Expr Expr_tuple_extend(struct serene_Allocator, struct TypeIntern *, struct Expr tail, struct Expr head);

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

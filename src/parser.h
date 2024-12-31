#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include <assert.h>

#include "./lexer.h"
#include "./strings.h"
#include "./arena.h"

struct Binding;
struct Statement;
struct Expr;
struct Type;
struct Function;
struct Ast;

enum BindingTag {
    BT_Empty,
    BT_Name,
};
struct Binding {
    enum BindingTag tag;
    union {
        struct {
            struct Stringview name;
            struct Type* annot;
        } name;
    };
};

enum StatementTag {
    ST_Let,
    ST_Mut,
    ST_Break,
    ST_Return,
    ST_Assign,
    ST_Const,
};
struct Statement {
    enum StatementTag tag;
    union {
        struct {
            struct Binding bind;
            struct Expr* init;
        } let;
        struct {
            struct Binding bind;
            struct Expr* init;
        } mut;
        struct {
            struct Expr* expr;
        } break_stmt;
        struct {
            struct Expr* expr;
        } return_stmt;
        struct {
            struct Stringview name;
            struct Expr* expr;
        } assign;
        struct {
            struct Expr* expr;
        } const_stmt;
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
};
struct Expr {
    enum ExprTag tag;
    union {
        struct {
            struct Expr* cond;
            struct Expr* smash;
            struct Expr* pass;
        } if_expr;
        struct {
            struct Expr* block;
        } loop;
        struct {
            struct StatementsLL {
                struct Statement current;
                struct StatementsLL* next;
            }* stmts;
            struct Expr* tail;
        } bareblock;
        struct {
            struct Expr* name;
            struct Expr* args;
        } call;
        struct {
            struct Stringview name;
        } lit;
        struct {
            struct Expr *lhs;
            struct Expr* rhs;
        } comma;
    };
};

enum TypeTag {
    TT_Func,
    TT_Call,
    TT_Recall,
    TT_Comma,
};
struct Type {
    enum TypeTag tag;
    union {
        struct {
            struct Type* args;
            struct Type* ret;
        } func;
        struct {
            struct Type* name;
            struct Type* args;
        } call;
        struct {
            struct Stringview name;
        } recall;
        struct {
            struct Type *lhs;
            struct Type* rhs;
        } comma;
    };
};

struct Function {
    struct Stringview name;
    struct Type ret;
    struct Binding args;
    struct Expr body;
};

struct Ast {
    struct FunctionsLL {
        struct Function current;
        struct FunctionsLL* next;
    }* funcs;
};

struct Ast parse(struct Arena*, struct Opdecls, struct Tokenstream);

#endif

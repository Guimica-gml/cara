#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include <assert.h>
#include "./lexer.h"
#include "./arena.h"

enum ExprTag {};
struct Expr {
    enum ExprTag tag;
    union {} data;
};

enum TypeTag {};
struct Type {
    enum TypeTag tag;
    union {} data;
};

struct Function {
    struct StringView name;
    struct Type type;
    struct Expr body;
};

#define LIST_NAME Functions
#define LIST_TYPE struct Function
#include "./segment_list.c"

struct Ast {
    struct Functions funcs;
};

struct Ast parse(struct Arena*, struct Tokenstream *);

#endif

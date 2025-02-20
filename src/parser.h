#ifndef PARSER_H
#define PARSER_H

#include <assert.h>
#include <stdbool.h>

#include "./lexer.h"
#include "./strings.h"
#include "./tokens.h"
#include "serene.h"
#include "./ast.h"
#include "./symbols.h"
#include "mtree.h"
#include "preimport.h"

struct PTData {
    struct PPImports* imports;
    struct TypeIntern types;
    struct Ast ast;
};

void PTData_print(void*);

struct MTree* parse(
    struct serene_Trea*,
    struct Symbols*,
    struct MTree*
);

#endif

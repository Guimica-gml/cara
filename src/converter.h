#ifndef CONVERTER_H
#define CONVERTER_H

#include "./ast.h"
#include "./symbols.h"
#include "./tst.h"
#include "serene.h"

struct Tst convert_ast(struct serene_Allocator, struct TypeIntern *, struct Ast);

#endif

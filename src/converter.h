#ifndef CONVERTER_H
#define CONVERTER_H

#include "./tst.h"
#include "./ast.h"
#include "./symbols.h"
#include "serene.h"

struct Tst convert_ast(struct serene_Allocator, struct Symbols, struct Ast);

#endif

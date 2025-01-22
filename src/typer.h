#ifndef TYPER_H
#define TYPER_H

#include "serene.h"
#include "./ast.h"
#include "./symbols.h"
#include "./tst.h"

struct Tst typecheck(struct serene_Allocator, struct Symbols, struct Ast);

#endif

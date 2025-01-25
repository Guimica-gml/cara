#ifndef TYPER_H
#define TYPER_H

#include "./ast.h"
#include "./symbols.h"
#include "./tst.h"
#include "serene.h"

void typecheck(struct serene_Allocator, struct Symbols, struct Ast *);

#endif

#ifndef TYPER_H
#define TYPER_H

#include "./arena.h"
#include "./ast.h"
#include "./symbols.h"

void typecheck(struct Arena *, struct Symbols, struct Ast);

#endif

#ifndef TYPER_H
#define TYPER_H

#include "./strings.h"
#include "./arena.h"
#include "./parser.h"
#include "./types.h"

void typecheck(struct Arena*, struct Ast);

#endif

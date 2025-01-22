#ifndef RUNNER_H
#define RUNNER_H

#include "serene.h"
#include "./ast.h"
#include "./symbols.h"

void run(struct serene_Allocator, struct Symbols, struct Ast);

#endif

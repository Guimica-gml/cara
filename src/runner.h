#ifndef RUNNER_H
#define RUNNER_H

#include "./ast.h"
#include "./symbols.h"
#include "serene.h"

void run(struct serene_Allocator, struct Symbols, struct Ast);

#endif

#ifndef RUNNER_H
#define RUNNER_H

#include "./arena.h"
#include "./ast.h"
#include "./symbols.h"

void run(struct Arena*, struct Symbols, struct Ast);

#endif


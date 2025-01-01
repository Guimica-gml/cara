#ifndef PARSER_H
#define PARSER_H

#include <stdbool.h>
#include <assert.h>

#include "./strings.h"
#include "./arena.h"
#include "./tokens.h"
#include "./lexer.h"
// ↑ cuz of opdecls
#include "./ast.h"
#include "./symbols.h"

struct Ast parse(struct Arena*, struct Opdecls, struct Symbols, struct Tokenstream);

#endif

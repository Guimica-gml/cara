#ifndef PARSER_H
#define PARSER_H

#include <assert.h>
#include <stdbool.h>

#include "serene.h"
#include "./lexer.h"
#include "./strings.h"
#include "./tokens.h"
// ↑ cuz of opdecls
#include "./ast.h"
#include "./symbols.h"

struct Ast
parse(struct serene_Allocator, struct Opdecls, struct Symbols, struct Tokenstream);

#endif

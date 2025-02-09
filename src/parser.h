#ifndef PARSER_H
#define PARSER_H

#include <assert.h>
#include <stdbool.h>

#include "./lexer.h"
#include "./strings.h"
#include "./tokens.h"
#include "serene.h"
// ↑ cuz of opdecls
#include "./ast.h"
#include "./symbols.h"
#include "./tokenstream.h"

struct Ast parse(
    struct serene_Allocator, struct Opdecls, struct TypeIntern *,
    struct Tokenstream
);

#endif

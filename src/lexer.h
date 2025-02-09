#ifndef LEXER_H
#define LEXER_H

#include "./opdecl.h"
#include "./strings.h"
#include "./tokens.h"
#include "opdeclvec.h"
#include "serene.h"
#include <stdbool.h>

struct Lexer {
    const char* rest;
    struct Token token;
};

struct LexResult {
    struct Token token;
    size_t len;
};

struct LexResult Lexer_next(struct Lexer *);

#endif

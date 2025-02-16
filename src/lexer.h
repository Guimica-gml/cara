#ifndef LEXER_H
#define LEXER_H

#include "./opdecl.h"
#include "./strings.h"
#include "./tokens.h"
#include "opdeclvec.h"
#include "serene.h"
#include <stdbool.h>

struct Lexer {
    struct String rest;
    struct Token token;
};

struct Token Lexer_next(struct Lexer *);

#endif

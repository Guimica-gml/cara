#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>
#include "./tokens.h"
#include "./arena.h"

#define VEC_NAME Tokenvec
#define VEC_TYPE struct Token
#include "./vector.h"

struct Opdecl {
    // values < 0 used as absence
    int lbp;
    int rbp;
    const char* token;
};

#define VEC_NAME Opdecls
#define VEC_TYPE struct Opdecl
#include "./vector.h"

struct Tokenvec lex(struct Arena*, const char*);

struct Tokenstream Tokens_stream(struct Tokens*);

#endif

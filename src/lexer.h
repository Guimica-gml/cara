#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>
#include "./tokens.h"
#include "./arena.h"
#include "./strings.h"

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

struct Tokenvec lex(struct Arena*, struct Intern*, const char*);

struct Tokenstream Tokenvec_stream(struct Tokenvec*);

#endif

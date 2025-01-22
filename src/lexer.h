#ifndef LEXER_H
#define LEXER_H

#include "serene.h"
#include "./strings.h"
#include "./tokens.h"
#include <stdbool.h>

#define VEC_NAME Tokenvec
#define VEC_TYPE struct Token
#include "./vector.h"

struct Opdecl {
    // values < 0 used as absence
    int lbp;
    int rbp;
    const char *token;
};

#define VEC_NAME Opdecls
#define VEC_TYPE struct Opdecl
#include "./vector.h"

struct Tokenvec lex(struct serene_Allocator, struct Intern *, const char *);

struct Tokenstream Tokenvec_stream(struct Tokenvec *);

#endif

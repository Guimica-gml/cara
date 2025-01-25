#ifndef LEXER_H
#define LEXER_H

#include "./strings.h"
#include "./tokens.h"
#include "tokenvec.h"
#include "./opdecl.h"
#include "opdeclvec.h"
#include "serene.h"
#include <stdbool.h>

struct Tokenvec lex(struct serene_Allocator, struct Intern *, const char *);

struct Tokenstream Tokenvec_stream(struct Tokenvec *);

#endif

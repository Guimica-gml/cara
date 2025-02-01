#ifndef LEXER_H
#define LEXER_H

#include "./opdecl.h"
#include "./strings.h"
#include "./tokens.h"
#include "opdeclvec.h"
#include "serene.h"
#include "tokenvec.h"
#include <stdbool.h>

struct Tokenvec lex(struct serene_Allocator, struct Intern *, const char *);

struct Tokenstream Tokenvec_stream(struct Tokenvec *);

#endif

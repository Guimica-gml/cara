#ifndef TOKENSTREAM_H
#define TOKENSTREAM_H

#include "./lexer.h"

struct Tokenstream {
    /* struct Token *buf; */
    /* size_t len; */
    struct Lexer lexer;
    struct Token next;
};

bool Tokenstream_drop(struct Tokenstream *);
bool Tokenstream_drop_text(struct Tokenstream *, const char *);
bool Tokenstream_drop_kind(struct Tokenstream *, enum Tokenkind);
struct Token Tokenstream_peek(struct Tokenstream *);

#endif

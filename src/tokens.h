#ifndef TOKENS_H
#define TOKENS_H

#include <stdbool.h>
#include <stddef.h>

#include "strings.h"

enum Tokenkind {
    // used as placeholder primarily
    TK_EOF = 0,
    // Keywords
    TK_Func,
    TK_Return,
    TK_Loop,
    TK_Break,
    TK_If,
    TK_Else,
    TK_Let,
    TK_Mut,
    TK_Operator,
    TK_Type,
    TK_As,
    TK_Equals,
    // Immediates
    TK_OpenParen,
    TK_CloseParen,
    TK_OpenBrace,
    TK_CloseBrace,
    TK_OpenBracket,
    TK_CloseBracket,
    TK_Comma,
    TK_Semicolon,
    TK_Colon,
    // Literals
    TK_String,
    TK_Number,
    TK_Name,
    TK_Bool,
    // Comments...
    TK_Comment,
};

struct Token {
    enum Tokenkind kind;
    struct String spelling;
    int number;
};

struct Tokenstream {
    struct Token *buf;
    size_t len;
};

bool Tokenstream_drop(struct Tokenstream*);
bool Tokenstream_drop_text(struct Tokenstream*, struct String);
bool Tokenstream_drop_kind(struct Tokenstream*, enum Tokenkind);
struct Token Tokenstream_peek(struct Tokenstream*);

#endif

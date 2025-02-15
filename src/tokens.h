#ifndef TOKENS_H
#define TOKENS_H

#include <stdbool.h>
#include <stddef.h>

#include "strings.h"

enum Tokenkind {
    // used as placeholder primarily
    TK_EOF = 0,
    // Keywords
    TK_Func = 1,
    TK_Return = 2,
    TK_Loop = 3,
    TK_Break = 4,
    TK_If = 5,
    TK_Else = 6,
    TK_Let = 7,
    TK_Mut = 8,
    TK_Operator = 9,
    TK_Type = 10,
    TK_As = 11,
    TK_Equals = 12,
    // Immediates
    TK_OpenParen = 14,
    TK_CloseParen = 15,
    TK_OpenBrace = 16,
    TK_CloseBrace = 17,
    TK_OpenBracket = 18,
    TK_CloseBracket = 19,
    TK_DoubleColon = 20,
    TK_Comma = 21,
    TK_Semicolon = 22,
    TK_Colon = 23,
    // Literals
    TK_String = 25,
    TK_Number = 26,
    TK_Name = 27,
    TK_Bool = 28,
    // Comments...
    TK_Comment = 29,
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

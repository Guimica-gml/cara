#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>
#include "./arena.h"
#include "./arena.c"

enum Tokenkind {
    // Keywords
    TK_Func, TK_Return,
    TK_Loop, TK_Break,
    TK_If, TK_Else,
    TK_Let, TK_Mut,
    TK_Operator,
    TK_Type,
    TK_As,
    TK_Equals,
    // Immediates
    TK_OpenParen, TK_CloseParen,
    TK_OpenBrace, TK_CloseBrace,
    TK_OpenBracket, TK_CloseBracket,
    TK_Semicolon, TK_Colon,
    TK_DoubleColon,
    TK_Comma,
    // Literals
    TK_String,
    TK_Number,
    TK_Bool,
    TK_Name,
};

#define LIST_NAME Tokens
#define LIST_TYPE enum Tokenkind
#include "./segment_list.c"

struct Tokens lex(struct Arena*, char*);
char* lexer_tokenkind_name(enum Tokenkind);

#endif

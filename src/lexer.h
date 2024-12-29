#include <stdbool.h>

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
    TK_ImmediatesStart = 13,
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
    TK_LiteralsStart = 24,
    TK_String = 25,
    TK_Number = 26,
    TK_Name = 27,
    TK_Bool = 28,
};

#define VEC_NAME Tokenkinds
#define VEC_TYPE enum Tokenkind
#include "./vector.h"

#include "./strings.h"
#define VEC_NAME Tokenstrings
#define VEC_TYPE struct Stringview
#include "./vector.h"

struct Tokenstream {
    struct Tokenkinds kinds;
    struct Tokenstrings strings;
};

struct Tokenstream lex(const char*);
char* lexer_tokenkind_name(enum Tokenkind);
void Tokenstream_deinit(struct Tokenstream*);
void Tokenstream_drop(struct Tokenstream*);
bool Tokenstream_drop_text(struct Tokenstream*, char*);

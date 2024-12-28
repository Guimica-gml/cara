#ifndef LEXER_C
#define LEXER_C

#include <assert.h>
#include "./lexer.h"

struct Lexer_Result;
struct Lexer_SpellingEntry;

bool lexer_is_whitespace(char);
bool lexer_is_ascii_digit(char);
bool lexer_is_not_word_break(char);
bool lexer_is_not_zero(size_t);
bool lexer_starts_word_break(char*);
bool lexer_prefix_of(char*, char*);
bool lexer_const_true(char*);
size_t lexer_strlen(char*);
char* lexer_strip_comments(char*);
struct Lexer_Result lexer_once(char*);
struct Lexer_Result lexer_expect(
    const struct Lexer_SpellingEntry*,
    bool (*post)(char*),
    char*
);
struct Lexer_Result lexer_process(
    bool (*pre)(char*),
    bool (*intra)(char),
    bool (*post)(size_t),
    enum Tokenkind,
    char*
);
struct Lexer_Result lexer_immediates(char*);
struct Lexer_Result lexer_keywords(char*);
struct Lexer_Result lexer_bools(char*);
struct Lexer_Result lexer_string(char*);
struct Lexer_Result lexer_number(char*);
struct Lexer_Result lexer_name(char*);

struct Lexer_Result {
    enum { LexRes_Err = 0, LexRes_Ok } tag;
    enum Tokenkind kind;
    struct StringView spelling;
    char* rest;
};

struct Tokenstream lex(char* input) {
    struct Tokenstream toks = {0};
    for (
        struct Lexer_Result res = lexer_once(input);
        res.tag;
        res = lexer_once(input)
    ) {
        assert(Tokenstrings_push(&toks.strings, res.spelling));
        assert(Tokenkinds_push(&toks.kinds, res.kind));
        input = res.rest;
    }
    while (lexer_is_whitespace(*input)) input++;
    assert(*input == '\0' && "input should be consumed entirely!");
    return toks;
}

struct Lexer_Result lexer_once(char* input) {
    struct Lexer_Result out;
    input = lexer_strip_comments(input);
    if (out = lexer_immediates(input), out.tag) return out;
    if (out = lexer_keywords(input), out.tag) return out;
    if (out = lexer_bools(input), out.tag) return out;
    if (out = lexer_string(input), out.tag) return out;
    if (out = lexer_number(input), out.tag) return out;
    if (out = lexer_name(input), out.tag) return out;
    return (struct Lexer_Result) {0};
}

struct Lexer_SpellingEntry {
    char* str;
    enum Tokenkind kind;
};

struct Lexer_Result lexer_expect(
    const struct Lexer_SpellingEntry* table,
    bool (*postcondition)(char*),
    char* input
) {
    for (size_t i = 0; table[i].str; i++) {
        if (!lexer_prefix_of(input, table[i].str)) continue;
        size_t len = lexer_strlen(table[i].str);
        if (!postcondition(input + len)) continue;
        return (struct Lexer_Result) {
            .tag = LexRes_Ok,
            .kind = table[i].kind,
            .spelling = (struct StringView) {
                .str = input,
                .len = len,
            },
            .rest = input + len,
        };
    }
    return (struct Lexer_Result) {0};
}

const struct Lexer_SpellingEntry spelling_table[] = {
    { .str = "", .kind = TK_EOF },
    // keywords
    { .str = "func", .kind = TK_Func }, { .str = "return", .kind = TK_Return },
    { .str = "loop", .kind = TK_Loop }, { .str = "break", .kind = TK_Break },
    { .str = "if", .kind = TK_If }, { .str = "else", .kind = TK_Else },
    { .str = "let", .kind = TK_Let }, { .str = "mut", .kind = TK_Mut },
    { .str = "type", .kind = TK_Type }, { .str = "as", .kind = TK_As },
    { .str = "operator", .kind = TK_Operator }, { .str = "=", .kind = TK_Equals },
    // immediates
    { .str = NULL, .kind = TK_ImmediatesStart },
    { .str = "(",  .kind = TK_OpenParen },   { .str = ")", .kind = TK_CloseParen },
    { .str = "{",  .kind = TK_OpenBrace },   { .str = "}", .kind = TK_CloseBrace },
    { .str = "[",  .kind = TK_OpenBracket }, { .str = "]", .kind = TK_CloseBracket },
    // make sure '::' is before ':', so as to prioritize it
    { .str = "::", .kind = TK_DoubleColon }, { .str = ",", .kind = TK_Comma },
    { .str = ";",  .kind = TK_Semicolon },   { .str = ":", .kind = TK_Colon },
    // literals
    { .str = NULL, .kind = TK_LiteralsStart },
    { .str = "string", .kind = TK_String }, { .str = "number", .kind = TK_Number },
    { .str = "name", .kind = TK_Name }, { .str = "bool", .kind = TK_Bool },
    { .str = NULL, .kind = TK_EOF },
};

char* lexer_tokenkind_name(enum Tokenkind kind) {
    return spelling_table[kind].str;
}


struct Lexer_Result lexer_immediates(char* input) {
    return lexer_expect(spelling_table + TK_ImmediatesStart + 1, lexer_const_true, input);
}

struct Lexer_Result lexer_keywords(char* input) {
    return lexer_expect(spelling_table + 1, lexer_starts_word_break, input);
}

struct Lexer_Result lexer_bools(char* input) {
    const struct Lexer_SpellingEntry table[] = {
        { .str = "true", .kind = TK_Bool }, { .str = "false", .kind = TK_Bool },
        { .str = NULL, .kind = TK_EOF },
    };
    return lexer_expect(table, lexer_starts_word_break, input);
}

struct Lexer_Result lexer_process(
    bool (*pre)(char*),
    bool (*intra)(char),
    bool (*post)(size_t),
    enum Tokenkind kind,
    char* input
) {
    size_t len = 0;
    if (!pre(input)) return (struct Lexer_Result) {0};
    while (intra(input[len])) len++;
    if (!post(len)) return (struct Lexer_Result) {0};
    return (struct Lexer_Result) {
        .tag = LexRes_Ok,
        .kind = kind,
        .spelling = (struct StringView) {
            .str = input,
            .len = len,
        },
        .rest = input + len,
    };
}

struct Lexer_Result lexer_string(char* input) {
    size_t len = 0;
    bool escaped = true;
    bool loop = true;
    if (input[0] != '"') return (struct Lexer_Result) {0};
    while (loop) {
        switch (input[len]) {
        case '\\':
            escaped = !escaped;
            break;
        case '\n':
            loop = false;
            break;
        case '"':
            if (!escaped) loop = false;
            escaped = false;
            break;
        default:
            escaped = false;
            break;
        }
        len++;
    }
    return (struct Lexer_Result) {
        .tag = LexRes_Ok,
        .kind = TK_String,
        .spelling = (struct StringView) {
            .str = input,
            .len = len,
        },
        .rest = input + len,
    };
}

struct Lexer_Result lexer_number(char* input) {
    return lexer_process(
        lexer_const_true,
        lexer_is_ascii_digit,
        lexer_is_not_zero,
        TK_Number,
        input
    );
}

struct Lexer_Result lexer_name(char* input) {
    return lexer_process(
        lexer_const_true,
        lexer_is_not_word_break,
        lexer_is_not_zero,
        TK_Name,
        input
    );
}

char* lexer_strip_comments(char* input) {
    while (lexer_is_whitespace(*input)) input++;
    while (lexer_prefix_of(input, "//")) {
        while (*input != '\n') input++;
        while (lexer_is_whitespace(*input)) input++;
    }
    return input;
}

bool lexer_prefix_of(char* this, char* prefix) {
    while (*this && *prefix && *this == *prefix) this++, prefix++;
    return *prefix == '\0';
}

size_t lexer_strlen(char* str) {
    size_t i = 0;
    while (str[i]) i++;
    return i;
}

struct StringView StringView_from(char* str) {
    return (struct StringView) {
        .str = str,
        .len = lexer_strlen(str),
    };
}

bool lexer_is_whitespace(char c) {
    char _ws[] = { 0x09, 0x0A, 0x0C, 0x0D, 0x20, '\0' };
    char* ws = _ws;
    for (; *ws; ws++) { if (*ws == c) return true; }
    return false;
}

bool lexer_is_ascii_digit(char c) { return '0' <= c && c <= '9'; }

bool lexer_is_not_word_break(char input) {
    if (
        input == '\0'
        || lexer_is_ascii_digit(input)
        || input == '"'
        || lexer_is_whitespace(input)
    ) return false;
    for (int i = TK_ImmediatesStart + 1; spelling_table[i].str; i++) {
        if (lexer_prefix_of(&input, spelling_table[i].str)) return false;
    }
    return true;
}

bool lexer_starts_word_break(char* input) {
    return !lexer_is_not_word_break(*input);
}

bool lexer_is_not_zero(size_t n) { return n != 0; }

bool lexer_const_true(char* s) {
    return true;
    (void)s;
}

#endif

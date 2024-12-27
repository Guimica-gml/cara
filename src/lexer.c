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
    char* rest;
};

struct Tokens lex(struct Arena* arena, char* input) {
    struct Tokens toks = {0};
    for (
        struct Lexer_Result res = lexer_once(input);
        res.tag;
        res = lexer_once(input)
    ) {
        input = res.rest;
        assert(Tokens_push(&toks, arena, res.kind));
    }
    while (lexer_is_whitespace(*input)) input++;
    assert(*input == '\0' && "input should be consumed entirely!");
    return toks;
}

char* lexer_tokenkind_name(enum Tokenkind kind) {
#define ENTRY(TK) [TK_ ## TK] = #TK
    char* lookup[] = {
        ENTRY(Func), ENTRY(Return),
        ENTRY(Loop), ENTRY(Break),
        ENTRY(If), ENTRY(Else),
        ENTRY(Let), ENTRY(Mut),
        ENTRY(Type), ENTRY(As),
        ENTRY(Operator), ENTRY(Equals),
        ENTRY(OpenParen), ENTRY(CloseParen),
        ENTRY(OpenBrace), ENTRY(CloseBrace),
        ENTRY(OpenBracket), ENTRY(CloseBracket),
        ENTRY(Semicolon), ENTRY(Colon),
        ENTRY(DoubleColon), ENTRY(Comma),
        ENTRY(String), ENTRY(Number),
        ENTRY(Bool), ENTRY(Name)
    };
#undef ENTRY
    return lookup[kind];
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
        char* rest = input + lexer_strlen(table[i].str);
        if (!postcondition(rest)) continue;
        return (struct Lexer_Result) {
            .tag = LexRes_Ok,
            .kind = table[i].kind,
            .rest = rest
        };
    }
    return (struct Lexer_Result) {0};
}

const struct Lexer_SpellingEntry immediates_table[] = {
    { .str = "(",  .kind = TK_OpenParen },   { .str = ")", .kind = TK_CloseParen },
    { .str = "{",  .kind = TK_OpenBrace },   { .str = "}", .kind = TK_CloseBrace },
    { .str = "[",  .kind = TK_OpenBracket }, { .str = "]", .kind = TK_CloseBracket },
    // make sure '::' is before ':', so as to prioritize it
    { .str = "::", .kind = TK_DoubleColon }, { .str = ",", .kind = TK_Comma },
    { .str = ";",  .kind = TK_Semicolon },   { .str = ":", .kind = TK_Colon },
    // kind doesn't matter, str NULL as terminator
    { .str = NULL, .kind = TK_Colon },
};

struct Lexer_Result lexer_immediates(char* input) {
    return lexer_expect(immediates_table, lexer_const_true, input);
}

struct Lexer_Result lexer_keywords(char* input) {
    const struct Lexer_SpellingEntry table[] = {
        { .str = "func", .kind = TK_Func }, { .str = "return", .kind = TK_Return },
        { .str = "loop", .kind = TK_Loop }, { .str = "break", .kind = TK_Break },
        { .str = "if", .kind = TK_If }, { .str = "else", .kind = TK_Else },
        { .str = "let", .kind = TK_Let }, { .str = "mut", .kind = TK_Mut },
        { .str = "type", .kind = TK_Type }, { .str = "as", .kind = TK_As },
        { .str = "operator", .kind = TK_Operator }, { .str = "=", .kind = TK_Equals },
        // kind doesn't matter, str NULL as terminator
        { .str = NULL, .kind = TK_Colon },
    };
    return lexer_expect(table, lexer_starts_word_break, input);
}

struct Lexer_Result lexer_bools(char* input) {
    const struct Lexer_SpellingEntry table[] = {
        { .str = "true", .kind = TK_Bool }, { .str = "false", .kind = TK_Bool },
        // kind doesn't matter, str NULL as terminator
        { .str = NULL, .kind = TK_Colon },
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
    char* cursor = input;
    if (!pre(cursor)) return (struct Lexer_Result) {0};
    while (intra(*cursor)) cursor++;
    if (!post((size_t) (cursor - input))) return (struct Lexer_Result) {0};
    return (struct Lexer_Result) {
        .tag = LexRes_Ok,
        .kind = kind,
        .rest = cursor
    };
}

struct Lexer_Result lexer_string(char* input) {
    bool escaped = true;
    bool loop = true;
    if (*input != '"') return (struct Lexer_Result) {0};
    while (loop) {
        switch (*input) {
        case '\\':
            escaped = !escaped;
            break;
        case '\n':
            loop = false;
            break;
        case '"':
            if (!escaped) loop = false;
            __attribute__((fallthrough));
        default:
            escaped = false;
            break;
        }
        input++;
    }
    return (struct Lexer_Result) {
        .tag = LexRes_Ok,
        .kind = TK_String,
        .rest = input,
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
    for (int i = 0; immediates_table[i].str; i++) {
        if (lexer_prefix_of(&input, immediates_table[i].str)) return false;
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

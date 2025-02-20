#include "./lexer.h"
#include "./strings.h"
#include <assert.h>
#include <stdlib.h>

struct SpellingEntry;

static bool lexer_is_word_break(char);
static struct String lexer_strip_whitespace(struct String);
static bool lexer_immediates(struct Lexer*, struct String);
static bool lexer_comment(struct Lexer *, struct String);
static bool lexer_keywords(struct Lexer*, struct String);
static bool lexer_bools(struct Lexer*, struct String);
static bool lexer_string(struct Lexer*, struct String);
static bool lexer_number(struct Lexer*, struct String);
static bool lexer_name(struct Lexer*, struct String);

struct Token Lexer_next(struct Lexer* lexer) {
    if (lexer->rest.len == 0) return (struct Token){0};
    lexer->rest = lexer_strip_whitespace(lexer->rest);
    if (lexer_comment(lexer, lexer->rest) ||
        lexer_immediates(lexer, lexer->rest) ||
        lexer_keywords(lexer, lexer->rest) ||
        lexer_bools(lexer, lexer->rest) ||
        lexer_string(lexer, lexer->rest) ||
        lexer_number(lexer, lexer->rest) ||
        lexer_name(lexer, lexer->rest)) {
        return lexer->token;
    }
    return (struct Token) {0};
}

struct SpellingEntry {
    struct String str;
    enum Tokenkind kind;
};

const struct SpellingEntry immediates[] = {
#define mkString(s) {.str = s, .len = sizeof(s) - 1}
    {.str = mkString("("), .kind = TK_OpenParen},
    {.str = mkString(")"), .kind = TK_CloseParen},
    {.str = mkString("{"), .kind = TK_OpenBrace},
    {.str = mkString("}"), .kind = TK_CloseBrace},
    {.str = mkString("["), .kind = TK_OpenBracket},
    {.str = mkString("]"), .kind = TK_CloseBracket},
    {.str = mkString(","), .kind = TK_Comma},
    {.str = mkString(";"), .kind = TK_Semicolon},
    {.str = mkString(":"), .kind = TK_Colon},
    {.str = mkString("/"), .kind = TK_Slash},
    {.str = mkString("..."), .kind = TK_Ellipsis},
#undef mkString
};

static bool lexer_immediates(struct Lexer* lexer, struct String input) {
    if (lexer->rest.len == 0) return false;
    for (size_t i = 0; i < sizeof(immediates)/sizeof(immediates[0]); i++) {
        if (!strings_prefix_of(input, immediates[i].str)) continue;
        lexer->token.spelling = input;
        lexer->token.spelling.len = immediates[i].str.len;
        lexer->token.kind = immediates[i].kind;
        lexer->token.number = 0;
        lexer->rest = strings_drop(input, immediates[i].str.len);
        return true;
    }
    return false;
}

static bool lexer_starts_word_break(struct String input) {
    if (input.len == 0) return true;
    return lexer_is_word_break(input.str[0]);
}

static bool lexer_keywords(struct Lexer* lexer, struct String input) {
    const struct SpellingEntry table[] = {
#define mkString(s) {.str = s, .len = sizeof(s) - 1}
        {.str = mkString("func"), .kind = TK_Func},
        {.str = mkString("return"), .kind = TK_Return},
        {.str = mkString("loop"), .kind = TK_Loop},
        {.str = mkString("break"), .kind = TK_Break},
        {.str = mkString("if"), .kind = TK_If},
        {.str = mkString("else"), .kind = TK_Else},
        {.str = mkString("let"), .kind = TK_Let},
        {.str = mkString("mut"), .kind = TK_Mut},
        {.str = mkString("type"), .kind = TK_Type},
        {.str = mkString("as"), .kind = TK_As},
        {.str = mkString("operator"), .kind = TK_Operator},
        {.str = mkString("="), .kind = TK_Equals},
        {.str = mkString("import"), .kind = TK_Import},
#undef mkString
    };

    if (lexer->rest.len == 0) return false;
    for (size_t i = 0; i < sizeof(table)/sizeof(table[0]); i++) {
        if (!strings_prefix_of(input, table[i].str)) continue;
        struct String rest = strings_drop(input, table[i].str.len);
        if (!lexer_starts_word_break(rest)) return false;
        lexer->token.spelling = input;
        lexer->token.spelling.len = table[i].str.len;
        lexer->token.kind = table[i].kind;
        lexer->token.number = 0;
        lexer->rest = rest;
        return true;
    }
    return false;
}

static bool lexer_bools(struct Lexer* lexer, struct String input) {
    const struct SpellingEntry table[] = {
        {.str = {"true", 4}, .kind = TK_Bool},
        {.str = {"false", 5}, .kind = TK_Bool},
    };

    if (lexer->rest.len == 0) return false;
    for (size_t i = 0; i < sizeof(table)/sizeof(table[0]); i++) {
        if (!strings_prefix_of(input, table[i].str)) continue;
        struct String rest = strings_drop(input, table[i].str.len);
        if (!lexer_starts_word_break(rest)) return false;
        lexer->token.spelling = input;
        lexer->token.spelling.len = table[i].str.len;
        lexer->token.kind = table[i].kind;
        lexer->token.number = 0;
        lexer->rest = strings_drop(input, table[i].str.len);
        return true;
    }
    return false;
}

static bool lexer_string(struct Lexer *lexer, struct String in) {
    size_t len = 0;
    bool escaped = true;
    bool loop = true;
    if (in.str[0] != '"') return false;
    while (loop && len < in.len) {
        switch (in.str[len]) {
        case '\\':
            escaped = !escaped;
            break;
        case '\n':
            loop = false;
            break;
        case '"':
            if (!escaped)
                loop = false;
            escaped = false;
            break;
        default:
            escaped = false;
            break;
        }
        len++;
    }

    lexer->token.spelling = in;
    lexer->token.spelling.len = len;
    lexer->token.kind = TK_String;
    lexer->token.number = 0;
    lexer->rest = strings_drop(in, len);
    return true;
}

static bool lexer_number(struct Lexer* lexer, struct String in) {
    size_t len = 0;
    while (len < in.len && strings_ascii_digit(in.str[len])) len++;
    if (len == 0) return false;
    lexer->token.spelling = in;
    lexer->token.spelling.len = len;
    lexer->token.kind = TK_Number;
    lexer->token.number = atoi(lexer->token.spelling.str);
    lexer->rest = strings_drop(in, len);
    return true;
}

static bool lexer_name(struct Lexer* lexer, struct String in) {
    size_t len = 0;
    while (len < in.len && !lexer_is_word_break(in.str[len])) len++;
    if (len == 0) return false;
    lexer->token.spelling = in;
    lexer->token.spelling.len = len;
    lexer->token.kind = TK_Name;
    lexer->token.number = 0;
    lexer->rest = strings_drop(in, len);
    return true;
}

static struct String lexer_strip_whitespace(struct String in) {
    size_t len = 0;
    while (len < in.len && strings_ascii_whitespace(in.str[len]))
        len++;
    return strings_drop(in, len);
}

static bool lexer_comment(struct Lexer *lexer, struct String in) {
    size_t len = 0;
    if (!strings_prefix_of(in, (struct String){"//", 2})) return false;
    while (len < in.len && in.str[len] != '\n')
        len++;
    lexer->token.kind = TK_Comment;
    lexer->token.spelling = in;
    lexer->token.number = 0;
    lexer->rest = strings_drop(in, len);
    return true;
}

static bool lexer_is_word_break(char input) {
    if (input == '\0' || input == '"' || strings_ascii_digit(input) ||
        strings_ascii_whitespace(input))
        return true;
    for (unsigned int i = 0; i < sizeof(immediates)/sizeof(immediates[0]); i++) {
        if (strings_prefix_of((struct String){&input, 1}, immediates[i].str)) return true;
    }
    return false;
}

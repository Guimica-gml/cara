#include "./lexer.h"
#include "./strings.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

struct SpellingEntry;

static bool lexer_is_word_break(char);
static const char *lexer_strip_whitespace(const char *);
static bool lexer_expect(const struct SpellingEntry*, bool (*post)(const char*), struct Lexer*, const char*);
static bool lexer_immediates(struct Lexer*, const char*);
static bool lexer_comment(struct Lexer *, const char *);
static bool lexer_keywords(struct Lexer*, const char *);
static bool lexer_bools(struct Lexer*, const char *);
static bool lexer_string(struct Lexer*, const char *);
static bool lexer_number(struct Lexer*, const char *);
static bool lexer_name(struct Lexer*, const char *);

struct LexResult Lexer_next(struct Lexer* lexer) {
    lexer->rest = lexer_strip_whitespace(lexer->rest);
    size_t start = (size_t)lexer->rest;
    if (lexer_immediates(lexer, lexer->rest) || lexer_comment(lexer, lexer->rest) ||
        lexer_keywords(lexer, lexer->rest) || lexer_bools(lexer, lexer->rest) ||
        lexer_string(lexer, lexer->rest) || lexer_number(lexer, lexer->rest) ||
        lexer_name(lexer, lexer->rest)) {
        size_t len = (size_t)lexer->rest - start;
        return (struct LexResult) {
            .token = lexer->token,
            .len = len,
        };
    }
    return (struct LexResult) {0};
}

struct SpellingEntry {
    const char *str;
    enum Tokenkind kind;
};

static bool lexer_expect(
    const struct SpellingEntry* table,
    bool (*postcondition)(const char*),
    struct Lexer* lexer,
    const char* input
) {
    for (size_t i = 0; table[i].str; i++) {
        if (!strings_prefix_of(input, table[i].str)) continue;
        size_t len = strings_strlen(table[i].str);
        if (!postcondition(input + len)) continue;
        lexer->token.spelling = input;
        /* lexer->token.spelling = Intern_insert(lexer->intern, input, len); */
        lexer->token.kind = table[i].kind;
        lexer->token.number = 0;
        lexer->rest = input + len;
        return true;
    }
    return false;
}

const struct SpellingEntry spelling_table[] = {
    {.str = "", .kind = TK_EOF},
    // keywords
    {.str = "func", .kind = TK_Func},
    {.str = "return", .kind = TK_Return},
    {.str = "loop", .kind = TK_Loop},
    {.str = "break", .kind = TK_Break},
    {.str = "if", .kind = TK_If},
    {.str = "else", .kind = TK_Else},
    {.str = "let", .kind = TK_Let},
    {.str = "mut", .kind = TK_Mut},
    {.str = "type", .kind = TK_Type},
    {.str = "as", .kind = TK_As},
    {.str = "operator", .kind = TK_Operator},
    {.str = "=", .kind = TK_Equals},
    // immediates
    {.str = NULL, .kind = TK_ImmediatesStart},
    {.str = "(", .kind = TK_OpenParen},
    {.str = ")", .kind = TK_CloseParen},
    {.str = "{", .kind = TK_OpenBrace},
    {.str = "}", .kind = TK_CloseBrace},
    {.str = "[", .kind = TK_OpenBracket},
    {.str = "]", .kind = TK_CloseBracket},
    // make sure '::' is before ':', so as to prioritize it
    {.str = "::", .kind = TK_DoubleColon},
    {.str = ",", .kind = TK_Comma},
    {.str = ";", .kind = TK_Semicolon},
    {.str = ":", .kind = TK_Colon},
    // literals
    {.str = NULL, .kind = TK_LiteralsStart},
    {.str = "string", .kind = TK_String},
    {.str = "number", .kind = TK_Number},
    {.str = "name", .kind = TK_Name},
    {.str = "bool", .kind = TK_Bool},
    {.str = NULL, .kind = TK_EOF},
};

static bool lexer_const_true(const char *s) {
    return true;
    (void)s;
}
static bool lexer_immediates(struct Lexer* l, const char *i) {
    return lexer_expect(
        spelling_table + TK_ImmediatesStart + 1, lexer_const_true, l, i
    );
}

static bool lexer_starts_word_break(const char *input) {
    return lexer_is_word_break(*input);
}
static bool lexer_keywords(struct Lexer* l, const char *i) {
    return lexer_expect(spelling_table + 1, lexer_starts_word_break, l, i);
}

static bool lexer_bools(struct Lexer* l, const char *i) {
    const struct SpellingEntry table[] = {
        {.str = "true", .kind = TK_Bool},
        {.str = "false", .kind = TK_Bool},
        {.str = NULL, .kind = TK_EOF},
    };
    return lexer_expect(table, lexer_starts_word_break, l, i);
}

static bool lexer_string(struct Lexer *lexer, const char *in) {
    size_t len = 0;
    bool escaped = true;
    bool loop = true;
    if (in[0] != '"') return false;
    while (loop) {
        switch (in[len]) {
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

    /* char *string = serene_nalloc(lexer->intern->alloc, len, char); */
    /* assert(string && "OOM"); */
    /* size_t string_len = 0; */

    /* size_t i = 1; */
    /* while (true) { */
    /*     char ch = in[i]; */
    /*     if (ch == '"') { */
    /*         break; */
    /*     } else if (ch == '\n') { */
    /*         string[string_len++] = '\n'; */
    /*         break; */
    /*     } else if (ch == '\\') { */
    /*         i += 1; */
    /*         assert(i < len && "expected special character, got end of string literal"); */
    /*         ch = in[i]; */
    /*         assert(in[i] != '\n' && "expected special character, got end of string literal"); */
    /*         switch (ch) { */
    /*         case '\'': string[string_len++] = '\''; break; */
    /*         case '0': string[string_len++] = '\0'; break; */
    /*         case '"': string[string_len++] = '"'; break; */
    /*         case 'n': string[string_len++] = '\n'; break; */
    /*         case 't': string[string_len++] = '\t'; break; */
    /*         default: */
    /*             printf("unknown special character: \\%c\n", ch); */
    /*             assert(0 && "unknown special character"); */
    /*         } */
    /*     } else { */
    /*         string[string_len++] = ch; */
    /*     } */
    /*     i += 1; */
    /* } */

    /* lexer->token.spelling = Intern_insert(lexer->intern, string, string_len); */
    lexer->token.spelling = in;
    lexer->token.kind = TK_String;
    lexer->token.number = 0;
    lexer->rest = in + len;
    return true;
}

static bool lexer_number(struct Lexer* lexer, const char *in) {
    size_t len = 0;
    while (strings_ascii_digit(in[len])) len++;
    if (len == 0) return false;
    lexer->token.spelling = in;
    /* lexer->token.spelling = Intern_insert(lexer->intern, in, len); */
    lexer->token.kind = TK_Number;
    lexer->token.number = atoi(lexer->token.spelling);
    lexer->rest = in + len;
    return true;
}

static bool lexer_name(struct Lexer* lexer, const char *in) {
    size_t len = 0;
    while (!lexer_is_word_break(in[len])) len++;
    if (len == 0) return false;
    lexer->token.spelling = in;
    /* lexer->token.spelling = Intern_insert(lexer->intern, in, len); */
    lexer->token.kind = TK_Name;
    lexer->token.number = 0;
    lexer->rest = in + len;
    return true;
}

static const char *lexer_strip_whitespace(const char *in) {
    size_t len = 0;
    while (strings_ascii_whitespace(in[len]))
        len++;
    return in + len;
}

static bool lexer_comment(struct Lexer *lexer, const char *in) {
    size_t len = 0;
    if (strings_prefix_of(in, "//")) {
        while (in[len] != '\n')
            len++;
    } else {
        return false;
    }
    lexer->token.kind = TK_Comment;
    lexer->token.spelling = in;
    lexer->token.number = 0;
    lexer->rest = in + len;
    return true;
}

static bool lexer_is_word_break(char input) {
    if (input == '\0' || input == '"' || input == '/' || strings_ascii_digit(input) ||
        strings_ascii_whitespace(input))
        return true;
    for (int i = TK_ImmediatesStart + 1; spelling_table[i].str; i++) {
        if (strings_prefix_of(&input, spelling_table[i].str))
            return true;
    }
    return false;
}

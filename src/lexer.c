#include "./lexer.h"
#include "./strings.h"
#include <assert.h>

#define VECTOR_IMPLS
#define VEC_NAME Tokenvec
#define VEC_TYPE struct Token
#include "./vector.h"

#define VECTOR_IMPLS
#define VEC_NAME Opdecls
#define VEC_TYPE struct Opdecl
#include "./vector.h"

struct SpellingEntry;

struct Out {
    struct Token token;
    const char *rest;
};

struct Context {
    struct Intern *intern;
    struct serene_Allocator alloc;
};

static bool lexer_is_word_break(char);
static const char *lexer_strip_comments(const char *);
static bool
lexer_expect(const struct SpellingEntry *, bool (*post)(const char *), struct Context, struct Out *, const char *);
static bool lexer_immediates(struct Context, struct Out *, const char *);
static bool lexer_keywords(struct Context, struct Out *, const char *);
static bool lexer_bools(struct Context, struct Out *, const char *);
static bool lexer_string(struct Context, struct Out *, const char *);
static bool lexer_number(struct Context, struct Out *, const char *);
static bool lexer_name(struct Context, struct Out *, const char *);

struct Tokenvec
lex(struct serene_Allocator alloc, struct Intern *intern, const char *input) {
    struct Tokenvec toks = {0};
    struct Out res = {0};
    struct Context ctx = {
        .alloc = alloc,
        .intern = intern,
    };
    input = lexer_strip_comments(input);
    while (lexer_immediates(ctx, &res, input) ||
           lexer_keywords(ctx, &res, input) || lexer_bools(ctx, &res, input) ||
           lexer_string(ctx, &res, input) || lexer_number(ctx, &res, input) ||
           lexer_name(ctx, &res, input)) {
        assert(Tokenvec_push(&toks, res.token));
        input = lexer_strip_comments(res.rest);
    }
    assert(*input == '\0' && "input should be consumed entirely!");
    return toks;
}

struct SpellingEntry {
    const char *str;
    enum Tokenkind kind;
};

static bool lexer_expect(
    const struct SpellingEntry *table, bool (*postcondition)(const char *),
    struct Context ctx, struct Out *out, const char *input
) {
    for (size_t i = 0; table[i].str; i++) {
        if (!strings_prefix_of(input, table[i].str))
            continue;
        size_t len = strings_strlen(table[i].str);
        if (!postcondition(input + len))
            continue;
        out->token.spelling = Intern_insert(ctx.intern, ctx.alloc, input, len);
        out->token.kind = table[i].kind;
        out->rest = input + len;
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
static bool lexer_immediates(struct Context c, struct Out *o, const char *i) {
    return lexer_expect(
        spelling_table + TK_ImmediatesStart + 1, lexer_const_true, c, o, i
    );
}

static bool lexer_starts_word_break(const char *input) {
    return lexer_is_word_break(*input);
}
static bool lexer_keywords(struct Context c, struct Out *o, const char *i) {
    return lexer_expect(spelling_table + 1, lexer_starts_word_break, c, o, i);
}

static bool lexer_bools(struct Context c, struct Out *o, const char *i) {
    const struct SpellingEntry table[] = {
        {.str = "true", .kind = TK_Bool},
        {.str = "false", .kind = TK_Bool},
        {.str = NULL, .kind = TK_EOF},
    };
    return lexer_expect(table, lexer_starts_word_break, c, o, i);
}

static bool lexer_string(struct Context ctx, struct Out *out, const char *in) {
    size_t len = 0;
    bool escaped = true;
    bool loop = true;
    if (in[0] != '"')
        return false;
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
    out->token.spelling = Intern_insert(ctx.intern, ctx.alloc, in, len);
    out->token.kind = TK_String;
    out->rest = in + len;
    return true;
}

static bool lexer_number(struct Context ctx, struct Out *out, const char *in) {
    size_t len = 0;
    while (strings_ascii_digit(in[len]))
        len++;
    if (len == 0)
        return false;
    out->token.spelling = Intern_insert(ctx.intern, ctx.alloc, in, len);
    out->token.kind = TK_Number;
    out->rest = in + len;
    return true;
}

static bool lexer_name(struct Context ctx, struct Out *out, const char *in) {
    size_t len = 0;
    while (!lexer_is_word_break(in[len]))
        len++;
    if (len == 0)
        return false;
    out->token.spelling = Intern_insert(ctx.intern, ctx.alloc, in, len);
    out->token.kind = TK_Name;
    out->rest = in + len;
    return true;
}

static const char *lexer_strip_comments(const char *input) {
    while (strings_ascii_whitespace(*input))
        input++;
    while (strings_prefix_of(input, "//")) {
        while (*input != '\n')
            input++;
        while (strings_ascii_whitespace(*input))
            input++;
    }
    return input;
}

static bool lexer_is_word_break(char input) {
    if (input == '\0' || input == '"' || strings_ascii_digit(input) ||
        strings_ascii_whitespace(input))
        return true;
    for (int i = TK_ImmediatesStart + 1; spelling_table[i].str; i++) {
        if (strings_prefix_of(&input, spelling_table[i].str))
            return true;
    }
    return false;
}

struct Tokenstream Tokenvec_stream(struct Tokenvec *this) {
    return (struct Tokenstream){
        .buf = this->buf,
        .len = this->len,
    };
}

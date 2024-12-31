#include <assert.h>
#include "./lexer.h"

#define VECTOR_IMPLS
#define VEC_NAME Tokenkinds
#define VEC_TYPE enum Tokenkind
#include "./vector.h"

#define VECTOR_IMPLS
#define VEC_NAME Tokenstrings
#define VEC_TYPE struct Stringview
#include "./vector.h"

#define VECTOR_IMPLS
#define VEC_NAME Opdecls
#define VEC_TYPE struct Opdecl
#include "./vector.h"

struct Lexer_SpellingEntry;

static bool lexer_is_word_break(char);
static cstr lexer_strip_comments(cstr);
static bool lexer_expect(
    const struct Lexer_SpellingEntry*,
    bool (*post)(cstr),
    enum Tokenkind*,
    struct Stringview*,
    cstr*,
    cstr
);
static bool lexer_immediates(enum Tokenkind*, struct Stringview*, cstr*, cstr);
static bool lexer_keywords(enum Tokenkind*, struct Stringview*, cstr*, cstr);
static bool lexer_bools(enum Tokenkind*, struct Stringview*, cstr*, cstr);
static bool lexer_string(enum Tokenkind*, struct Stringview*, cstr*, cstr);
static bool lexer_number(enum Tokenkind*, struct Stringview*, cstr*, cstr);
static bool lexer_name(enum Tokenkind*, struct Stringview*, cstr*, cstr);

struct Tokens lex(cstr input) {
    struct Tokens toks = {0};
    enum Tokenkind kind;
    struct Stringview spelling;
    input = lexer_strip_comments(input);
    while (
        lexer_immediates (&kind, &spelling, &input, input)
        || lexer_keywords(&kind, &spelling, &input, input)
        || lexer_bools   (&kind, &spelling, &input, input)
        || lexer_string  (&kind, &spelling, &input, input)
        || lexer_number  (&kind, &spelling, &input, input)
        || lexer_name    (&kind, &spelling, &input, input)
    ) {
        assert(Tokenstrings_push(&toks.strings, spelling));
        assert(Tokenkinds_push(&toks.kinds, kind));
        input = lexer_strip_comments(input);
    }
    assert(*input == '\0' && "input should be consumed entirely!");
    return toks;
}

struct Lexer_SpellingEntry {
    char* str;
    enum Tokenkind kind;
};

static bool lexer_expect(
    const struct Lexer_SpellingEntry* table,
    bool (*postcondition)(cstr),
    enum Tokenkind* kind,
    struct Stringview* spelling,
    cstr* output,
    cstr input
) {
    for (size_t i = 0; table[i].str; i++) {
        if (!strings_prefix_of(input, table[i].str)) continue;
        size_t len = strings_strlen(table[i].str);
        if (!postcondition(input + len)) continue;
        *output = input + len;
        *kind = table[i].kind;
        *spelling = (struct Stringview) {
            .str = input,
            .len = len,
        };
        return true;
    }
    return false;
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

static bool lexer_const_true(cstr s) { return true; (void)s; }
static bool lexer_immediates(enum Tokenkind* k, struct Stringview* s, cstr* o, cstr i) {
    return lexer_expect(spelling_table + TK_ImmediatesStart + 1, lexer_const_true, k, s, o, i);
}

static bool lexer_starts_word_break(cstr input) { return lexer_is_word_break(*input); }
static bool lexer_keywords(enum Tokenkind* k, struct Stringview* s, cstr* o, cstr i) {
    return lexer_expect(spelling_table + 1, lexer_starts_word_break, k, s, o, i);
}

static bool lexer_bools(enum Tokenkind* k, struct Stringview* s, cstr* o, cstr i) {
    const struct Lexer_SpellingEntry table[] = {
        { .str = "true", .kind = TK_Bool }, { .str = "false", .kind = TK_Bool },
        { .str = NULL, .kind = TK_EOF },
    };
    return lexer_expect(table, lexer_starts_word_break, k, s, o, i);
}

static bool lexer_string(
    enum Tokenkind* kind,
    struct Stringview* spelling,
    cstr* output,
    cstr input
) {
    size_t len = 0;
    bool escaped = true;
    bool loop = true;
    if (input[0] != '"') return false;
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
    *output = input + len;
    *kind = TK_String;
    *spelling = (struct Stringview) {
        .str = input,
        .len = len,
    };
    return true;
}

static bool lexer_number(
    enum Tokenkind* kind,
    struct Stringview* spelling,
    cstr* out,
    cstr in
) {
    size_t len = 0;
    while (strings_ascii_digit(in[len])) len++;
    if (len == 0) return false;
    *out = in + len;
    *kind = TK_Number;
    *spelling = (struct Stringview) {
        .str = in,
        .len = len,
    };
    return true;
}

static bool lexer_name(
    enum Tokenkind* kind,
    struct Stringview* spelling,
    cstr* out,
    cstr in
) {
    size_t len = 0;
    while (!lexer_is_word_break(in[len])) len++;
    if (len == 0) return false;
    *out = in + len;
    *kind = TK_Name;
    *spelling = (struct Stringview) {
        .str = in,
        .len = len,
    };
    return true;
}

static cstr lexer_strip_comments(cstr input) {
    while (strings_ascii_whitespace(*input)) input++;
    while (strings_prefix_of(input, "//")) {
        while (*input != '\n') input++;
        while (strings_ascii_whitespace(*input)) input++;
    }
    return input;
}

static bool lexer_is_word_break(char input) {
    if (
        input == '\0'
        || input == '"'
        || strings_ascii_digit(input)
        || strings_ascii_whitespace(input)
    ) return true;
    for (int i = TK_ImmediatesStart + 1; spelling_table[i].str; i++) {
        if (strings_prefix_of(&input, spelling_table[i].str)) return true;
    }
    return false;
}

void Tokens_deinit(struct Tokens* this) {
    Tokenkinds_deinit(&this->kinds);
    Tokenstrings_deinit(&this->strings);
}

struct Tokenstream Tokens_stream(struct Tokens* this) {
    return (struct Tokenstream) {
        .kinds.buf = this->kinds.buf,
        .kinds.len = this->kinds.len,
        .strings.buf = this->strings.buf,
        .strings.len = this->strings.len,
    };
}

bool Tokenstream_eof(struct Tokenstream* this) {
    return this->kinds.len < 1;
}

bool Tokenstream_drop(struct Tokenstream* this) {
    if (Tokenstream_eof(this)) return false;
    this->kinds.buf++;
    this->kinds.len--;
    this->strings.buf++;
    this->strings.len--;
    return true;
}

bool Tokenstream_drop_text(struct Tokenstream* this, cstr str) {
    if (Tokenstream_eof(this)) return false;
    struct Stringview expect = Stringview_from(str);
    struct Stringview have = *this->strings.buf;
    if (!Stringview_equal(have, expect)) return false;
    Tokenstream_drop(this);
    return true;
}

bool Tokenstream_drop_kind(struct Tokenstream* this, enum Tokenkind expect) {
    if (Tokenstream_eof(this)) return false;
    enum Tokenkind have = *this->kinds.buf;
    if (have != expect) return false;
    Tokenstream_drop(this);
    return true;
}

struct Stringview* Tokenstream_first_text(struct Tokenstream* this) {
    if (this->strings.len < 1) return NULL;
    return this->strings.buf;
}

enum Tokenkind* Tokenstream_first_kind(struct Tokenstream* this) {
    return Tokenstream_at_kind(this, 0);
}

enum Tokenkind* Tokenstream_at_kind(struct Tokenstream* this, size_t idx) {
    if (this->kinds.len <= idx) return NULL;
    return &this->kinds.buf[idx];
}

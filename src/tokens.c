#include "./tokens.h"
#include "./strings.h"
#include "./tokenstream.h"
#include "./lexer.h"

bool Tokenstream_drop(struct Tokenstream* this) {
    if (this->next.kind == TK_EOF) return false;
    this->next = Lexer_next(&this->lexer);
    return true;
}

bool Tokenstream_drop_text(struct Tokenstream* this, const char* text) {
    if (this->next.kind == TK_EOF) return false;
    if (!strings_equal(this->next.spelling, text)) return false;
    this->next = Lexer_next(&this->lexer);
    return true;
}

bool Tokenstream_drop_kind(struct Tokenstream* this, enum Tokenkind kind) {
    if (this->next.kind != kind) return false;
    return Tokenstream_drop(this);
}

struct Token Tokenstream_peek(struct Tokenstream* this) {
    return this->next;
}

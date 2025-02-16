#include "./tokens.h"
#include "./strings.h"
#include <stdio.h>

bool Tokenstream_drop(struct Tokenstream* this) {
    if (this->len <= 0) {
        printf("Unexpected eof!\n");
        return false;
    }
    this->buf++;
    this->len--;
    return true;
}

bool Tokenstream_drop_text(struct Tokenstream* this, struct String text) {
    if (this->len <= 0) {
        printf("Unexpected eof!\n");
        return false;
    }
    if (!strings_equal(this->buf[0].spelling, text)) {
        printf("Unexpected token: '%s'!\n", this->buf[0].spelling.str);
        return false;
    }
    this->buf++;
    this->len--;
    return true;
}

bool Tokenstream_drop_kind(struct Tokenstream* this, enum Tokenkind kind) {
    if (this->len <= 0) {
        printf("Unexpected eof!\n");
        return false;
    }
    if (this->buf[0].kind != kind) {
        printf("Unexpected token: '%s'!\n", this->buf[0].spelling.str);
        return false;
    }
    this->buf++;
    this->len--;
    return true;
}

struct Token Tokenstream_peek(struct Tokenstream* this) {
    if (this->len <= 0) return (struct Token) {0};
    return this->buf[0];
}

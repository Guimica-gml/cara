#include "./tokens.h"
#include "./strings.h"

bool Tokenstream_drop(struct Tokenstream *this) {
    if (this->len == 0)
        return false;
    this->buf++;
    this->len--;
    return true;
}

bool Tokenstream_drop_text(struct Tokenstream *this, const char *text) {
    if (this->len == 0)
        return false;
    if (!strings_equal(this->buf[0].spelling, text))
        return false;
    this->buf++;
    this->len--;
    return true;
}

bool Tokenstream_drop_kind(struct Tokenstream *this, enum Tokenkind kind) {
    if (this->len == 0)
        return false;
    if (this->buf[0].kind != kind)
        return false;
    this->buf++;
    this->len--;
    return true;
}

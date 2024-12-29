#include "./strings.h"

struct Stringview Stringview_from(cstr s) {
    return (struct Stringview) {
        .str = s,
        .len = strings_strlen(s),
    };
}

bool Stringview_equal(struct Stringview this, struct Stringview other) {
    if (this.len != other.len) return false;
    for (size_t i = 0; i < this.len; i++) {
        if (this.str[i] != other.str[i]) return false;
    }
    return true;
}

bool strings_ascii_whitespace(char c) {
    char ws[5] = { 0x09, 0x0A, 0x0C, 0x0D, 0x20 };
    for (int i = 0; i < 5; i++) {
        if (ws[i] == c) return true;
    }
    return false;
}

bool strings_ascii_digit(char c) {
    return '0' <= c && c <= '9';
}

size_t strings_strlen(cstr str) {
    size_t i = 0;
    while (str[i]) i++;
    return i;
}

bool strings_prefix_of(cstr str, cstr prefix) {
    while (*str && *prefix && *str == *prefix) {
        str++;
        prefix++;
    }
    return *prefix == '\0';
}

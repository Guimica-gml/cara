#ifndef STRINGS_H
#define STRINGS_H

#include <stdbool.h>
#include <stddef.h>

typedef const char* cstr;

// a non-owning pointer to string of set length
// mainly used to index identifiers in input string
// without having to add 0-terminators
struct Stringview {
    cstr str;
    size_t len;
};

struct Stringview Stringview_from(cstr);
bool Stringview_equal(struct Stringview, struct Stringview);

// cstr helpers
bool strings_ascii_whitespace(char);
bool strings_ascii_digit(char);
size_t strings_strlen(cstr);
bool strings_prefix_of(cstr, cstr);

#endif

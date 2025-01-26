#include "./strings.h"
#include <assert.h>
#include <stdio.h>


bool strings_ascii_whitespace(char c) {
    char ws[5] = {0x09, 0x0A, 0x0C, 0x0D, 0x20};
    for (int i = 0; i < 5; i++) {
        if (ws[i] == c)
            return true;
    }
    return false;
}

bool strings_ascii_digit(char c) { return '0' <= c && c <= '9'; }

size_t strings_strlen(const char *str) {
    size_t i = 0;
    while (str[i])
        i++;
    return i;
}

bool strings_prefix_of(const char *str, const char *prefix) {
    while (*str && *prefix && *str == *prefix) {
        str++;
        prefix++;
    }
    return *prefix == '\0';
}

bool strings_equal(const char *this, const char *other) {
    while (*this && *other && *this == *other) {
        this ++;
        other++;
    }
    return *this == *other;
}

static bool strings_n_equal(const char *this, const char *other, size_t len) {
    size_t i = 0;
    while (i < len && this[i] && other[i] && this[i] == other[i])
        i++;
    return i == len;
}

const char *Intern_insert(
    struct Intern *this, struct serene_Allocator alloc, const char *s,
    size_t len
) {
    struct Ordstring *entry = Btrings_search(&this->tree, (struct Ordstring) {.str = s, .len = len});
    if (entry) return entry->str;

    char *new = serene_nalloc(alloc, 1 + len, char);
    assert(new && "OOM");
    for (size_t i = 0; i < len; i++) new[i] = s[i];
    new[len] = '\0';
    assert(Btrings_insert(&this->tree, alloc, (struct Ordstring) {.str = new, .len = len}));
    return new;
}

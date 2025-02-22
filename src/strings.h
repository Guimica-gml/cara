#ifndef STRINGS_H
#define STRINGS_H

#include "btrings.h"
#include "serene.h"
#include <stdbool.h>
#include <stddef.h>

struct String {
    const char* str;
    size_t len;
};

// cstr helpers
bool strings_ascii_whitespace(char);
bool strings_ascii_digit(char);
size_t cstrings_strlen(const char *);
bool strings_prefix_of(struct String, struct String prefix);
bool strings_equal(struct String, struct String);
struct String strings_drop(struct String, unsigned int num);
struct String strings_split_first(struct String*, char);

struct Intern {
    struct Btrings tree;
    struct serene_Trea alloc;
};

struct Intern Intern_init(struct serene_Trea alloc);
// copies the contents of the passed in string
// the returned string is owned by Intern
struct String Intern_insert(struct Intern *, struct String);

#endif

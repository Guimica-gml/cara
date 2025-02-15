#ifndef STRINGS_H
#define STRINGS_H

#include "btrings.h"
#include "serene.h"
#include <stdbool.h>
#include <stddef.h>

// cstr helpers
bool strings_ascii_whitespace(char);
bool strings_ascii_digit(char);
size_t strings_strlen(const char *);
bool strings_prefix_of(const char *, const char *);
bool strings_equal(const char *, const char *);

struct Intern {
    struct Btrings tree;
    struct serene_Trea alloc;
};

struct Intern Intern_init(struct serene_Trea alloc);
// copies the contents of the passed in string
// the returned string is owned by Intern
const char *Intern_insert(struct Intern *, const char *, size_t);

#endif

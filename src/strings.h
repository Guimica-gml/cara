#ifndef STRINGS_H
#define STRINGS_H

#include <stdbool.h>
#include <stddef.h>
#include "./arena.h"

// cstr helpers
bool strings_ascii_whitespace(char);
bool strings_ascii_digit(char);
size_t strings_strlen(const char*);
bool strings_prefix_of(const char*, const char*);
bool strings_equal(const char*, const char*);

struct Intern {
    struct HashesLL {
        struct {
            long int hash;
            const char* string;
        } current;
        struct HashesLL* next;
    }* hashes;
};

// copies the contents of the passed in string
// the returned string is owned by Intern
const char* Intern_insert(struct Intern*, struct Arena*, const char*, size_t);

#endif

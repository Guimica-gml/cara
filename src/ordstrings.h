#ifndef ORDSTRINGS_H
#define ORDSTRINGS_H

#include <stddef.h>

struct Ordstring {
    const char *str;
    size_t len;
};

int Ordstring_cmp(struct Ordstring, struct Ordstring);

#endif

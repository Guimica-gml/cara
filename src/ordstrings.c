#include "ordstrings.h"
#include <stdio.h>

int Ordstring_cmp(struct Ordstring a, struct Ordstring b) {
    if (a.len != b.len)
        return a.len - b.len;
    size_t min = a.len < b.len ? a.len : b.len;
    for (size_t i = 0; i < min; i++) {
        if (a.str[i] != b.str[i])
            return a.str[i] - b.str[i];
    }
    return 0;
}

void Ordstring_print(struct Ordstring s) { printf("%.*s", s.len, s.str); }

#ifndef ARENA_H
#define ARENA_H

#include <stddef.h>
#include <stdbool.h>

struct Arena {
    char* buf;
    size_t bump;
    size_t cap;
};

void* arena_alloc(struct Arena*, size_t);
bool arena_init(struct Arena*, size_t);
void arena_deinit(struct Arena*);

#endif

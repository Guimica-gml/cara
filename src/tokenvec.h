#ifndef tokenvec_H
#define tokenvec_H

#include "tokens.h"
#include <stdbool.h>
#include <stddef.h>

struct Tokenvec {
    size_t len;
    size_t cap;
    struct Token *buf;
};

struct Token *Tokenvec_at(struct Tokenvec *, size_t);
bool Tokenvec_init(struct Tokenvec *);
bool Tokenvec_grow(struct Tokenvec *);
bool Tokenvec_push(struct Tokenvec *, struct Token);
struct Token *Tokenvec_first(struct Tokenvec *);
struct Token *Tokenvec_last(struct Tokenvec *);
struct Token *Tokenvec_emplace(struct Tokenvec *);
void Tokenvec_deinit(struct Tokenvec *);
size_t Tokenvec_space(struct Tokenvec *);

#endif

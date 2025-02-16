#ifndef typereg_H
#define typereg_H

#include "types.h"
#include "serene.h"
#include <stdbool.h>

struct Typereg_impl;

struct Typereg {
    struct Typereg_impl *root;
};

bool Typereg_insert(struct Typereg *, struct serene_Allocator, struct Type*);
struct Type* *Typereg_search(struct Typereg *, struct Type*);
void Typereg_deinit(struct Typereg *, struct serene_Allocator);
void Typereg_print(struct Typereg *);

#endif


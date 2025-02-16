#ifndef btrings_H
#define btrings_H

#include "ordstrings.h"
#include "serene.h"
#include <stdbool.h>

struct Btrings_impl;

struct Btrings {
    struct Btrings_impl *root;
};

bool Btrings_insert(struct Btrings *, struct serene_Allocator, struct Ordstring);
struct Ordstring *Btrings_search(struct Btrings *, struct Ordstring);
void Btrings_deinit(struct Btrings *, struct serene_Allocator);
void Btrings_print(struct Btrings *);

#endif


#ifndef @filename@_H
#define @filename@_H

#include "@include@"
#include "serene.h"
#include <stdbool.h>

struct @treename@_impl;

struct @treename@ {
    struct @treename@_impl *root;
};

bool @treename@_insert(struct @treename@ *, struct serene_Allocator, @basetype@);
@basetype@ *@treename@_search(struct @treename@ *, @basetype@);
void @treename@_deinit(struct @treename@ *, struct serene_Allocator);
void @treename@_print(struct @treename@ *);

#endif

